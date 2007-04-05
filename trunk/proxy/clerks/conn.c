/* conn.c
 *
 * Copyright (C) 2006 Scott Bronson
 * This file is under the MIT license.
 * See the COPYING file for specifics.
 *
 * A connection represents a currently open single TCP connection that
 * passes through the tunnel.
 *
 * TODO: get rid of io_flags -- not needed anymore.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "io/socket.h"
#include "list.h"
#include "log.h"
#include "conn.h"
#include "tunnel.h"

#include <sys/socket.h>


/**
 * Size in bytes of a single receive/transmit buffer.
 *
 * This might seem a little small but it should keep our latency
 * down, reducing the amount of in-transit data.  If this utility
 * is used on different links, this number may have to be tweaked
 * to avoid unnecessary fragmentation.
 */

#define ENDPOINT_BUFSIZ 2000


    // If we couldn't complete a write, the data remains in the buffer
    // and bufcnt is nonzero.  For a healthy stream where all data is
    // consumed, bufcnt is always 0.

    // this is way larger than the maximum ethernet packet size but
    // not so big that a ton of connections will cause memory issues.


// ideally, a socket_buf would be a fifo.  The code to do this exists
// in rzh but I'm not going to take the time to clean it up now.  So,
// if a socket can't consume as much as is written, we will end up
// doing a LOT of memmoving.  This is probably a todo item...
// This is prototypoing the api before being moved into io/io_socketbuf
// or something...

// The data always lives in the *writer's* buffer.

struct socket_buf {
    int size;
    int cursor;
    char base[ENDPOINT_BUFSIZ];
};


struct endpoint {
    struct io_atom io;
    int io_flags;
    struct socket_addr addr;
    struct socket_buf buf;
    enum {
        EP_INIT,    // this endpoint hasn't been opened yet
        EP_OPEN,    // the endpoint is open and data is moving
        EP_CLOSING, // the remote is closed, this endpint must shut down
        EP_CLOSED,  // this endpoint has been shut down
    } state;
};

struct conn {
    struct endpoint local;
    struct endpoint remote;
    struct list_head list;
    struct tunnel *tunnel;
};



#define SHOW_DATA 0

/**
 * Read as much as possible from the given atom into the given
 * socket buffer.
 */

static int socket_buf_read(struct endpoint *ep, struct socket_buf *buf)
{
    size_t cnt;
    int err;

    err = io_read(&ep->io,
            buf->base + buf->cursor,
            buf->size - buf->cursor,
            &cnt);

    assert(cnt <= buf->size - buf->cursor);

    if(SHOW_DATA) {
        log_dbg("DATA received from %s:%d fd=%d len=%d err=%d <<<%.*s>>>",
                inet_ntoa(ep->addr.addr), ep->addr.port,
                ep->io.fd, cnt, err, cnt, buf->base+buf->cursor);
    } else {
        log_dbg("DATA received from %s:%d fd=%d len=%d err=%d",
                inet_ntoa(ep->addr.addr), ep->addr.port,
                ep->io.fd, cnt, err);
    }

    buf->cursor += cnt;
    return err;
}


/**
 * Write as much as possible from the given socket buffer into
 * the given atom
 */

static int socket_buf_write(struct endpoint *ep, struct socket_buf *buf)
{
    size_t cnt;
    int err;

    err = io_write(&ep->io, buf->base, buf->cursor, &cnt);
    assert(cnt <= buf->cursor);

    if(SHOW_DATA) {
       log_dbg("DATA written to %s:%d fd=%d, %d of %d bytes, err=%d <<<%.*s>>>",
                inet_ntoa(ep->addr.addr), ep->addr.port,
                ep->io.fd, cnt, buf->cursor, err, cnt, buf->base);
    } else {
        log_dbg("DATA written to %s:%d fd=%d, %d of %d bytes, err=%d",
                inet_ntoa(ep->addr.addr), ep->addr.port,
                ep->io.fd, cnt, buf->cursor, err);
    }

    if(cnt == buf->cursor) {
        buf->cursor = 0;
        return err;
    }

    if(cnt > 0) {
        assert(cnt <= buf->cursor);
        // some data was written, so we need to adjust the
        // rest of the data in the buffer.
        memmove(buf->base, buf->base+cnt, buf->cursor-cnt);
        buf->cursor -= cnt;
        return err;
    }

    return err;
}


/**
 * Tries to write as much of buffer as possible.  As opposed to
 * conn_perform_read(), does *not* try immediately try to read
 * to fill the newly exposed room.
 */

int conn_perform_write(struct endpoint *reader, struct endpoint *writer)
{
    int err;

    if(writer->state == EP_INIT) {
        // we can't write because the remote connection isn't
        // open yet (we're waiting for conn_remote_is_ready())
        // So, no error, but no data written either.
        return 0;
    }

    err = socket_buf_write(writer, &writer->buf);
    if(err != 0) {
        if(err == EPIPE) {
            // remote has closed connection.
            log_note("Peer on %s:%d fd=%d closed connection.",
                    inet_ntoa(reader->addr.addr), reader->addr.port,
                    reader->io.fd);
        } else {
            log_err("Error resending data from %s:%d to %s:%d: %s",
                    inet_ntoa(reader->addr.addr), reader->addr.port,
                    inet_ntoa(writer->addr.addr), writer->addr.port,
                    strerror(errno));
        }
        writer->state = EP_CLOSING;
    }

    return err;
}


/**
 * Reads from target to exhaustion.  Tries to immediately write all
 * data that has been read.
 *
 * @returns 1 if either endpoint has closed the connection, 0 if not.
 */

void conn_perform_read(struct endpoint *reader, struct endpoint *writer)
{
    int cont = 0;
    int err;

    // we need to read to exhaustion.
    do {
        err = socket_buf_read(reader, &writer->buf);
        if(err != 0) {
            if(err == EPIPE) {
                // remote has closed connection.
                log_note("Peer on %s:%d fd=%d closed connection.",
                        inet_ntoa(reader->addr.addr), reader->addr.port,
                        reader->io.fd);
            } else {
                log_err("Error receiving data from %s:%d: %s",
                        inet_ntoa(reader->addr.addr), reader->addr.port,
                        strerror(errno));
            }
            reader->state = EP_CLOSING;
            return;
        }

        if(writer->buf.cursor == writer->buf.size) {
            // we need to read to exhaustion.  there was more
            // data waiting than we had space in the buffer.
            cont = 1;
        }

        err = conn_perform_write(reader, writer);
        if(err != 0) {
            return;
        }
    } while(cont);
}


/** Maintains ep->io_flags.
 */

static void conn_io_enable(struct endpoint *ep, int flags)
{
    ep->io_flags |= flags;
    if(ep->state == EP_OPEN) {
        io_enable(&ep->io, flags);
    }
}


/** Maintains ep->io_flags.  Never change an endpoint's iostate
 *  without calling these routines.
 */

static void conn_io_disable(struct endpoint *ep, int flags)
{
    ep->io_flags &= ~flags;
    if(ep->state == EP_OPEN) {
        io_disable(&ep->io, flags);
    }
}


/** Called whenever there is data to read on the local side
 * of a connection
 * @param flags The IO flags of the generated event
 * @param target The socket that the event occurred on.
 * @param verso The target's counterpart (if the target is a reader,
 *    verso is the associated writer).
 *
 * @returns 1 if any endpoint has closed, 0 if not.
 */

static int conn_perform_io(int flags, struct endpoint *reader, struct endpoint *writer)
{
    if(flags & IO_WRITE) {
        conn_perform_write(reader, writer);

        // We know that the socket's IO_WRITE flag is set.
        
        // if there's no more data to write in the buffer, then there's
        // no longer any need to be notified of write events.
        if(writer->buf.cursor == 0 && writer->io.fd != -1) {
            log_dbg("Disable writing on %d", writer->io.fd);
            conn_io_disable(writer, IO_WRITE);
        }

        // if there's room in the buffer, then we should ensure that
        // we get called for read events.
        if(writer->buf.cursor < writer->buf.size) {
            log_dbg("Enable reading on %d", reader->io.fd);
            conn_io_enable(reader, IO_READ);
            // we'll also try to read immediately in case there's data
            // sitting on the reader and we're using edge-triggered.
            flags |= IO_READ;
            // fall through...
        }
    }

    if(flags & IO_READ) {
        conn_perform_read(reader, writer);

        // We know that the socket's IO_READ flag is set.
       
        // if there's any data left in the buffer, we need to be
        // notified when we can write it out again.
        if(writer->buf.cursor > 0) {
            log_dbg("Enable writing on %d", writer->io.fd);
            conn_io_enable(writer, IO_WRITE);
        }

        // if the buffer is full then we need to disable reading
        // until some space frees up again.
        if(writer->buf.cursor == writer->buf.size) {
            log_dbg("Disable reading on %d", reader->io.fd);
            conn_io_disable(reader, IO_READ);
        }
    }

    if(flags & IO_EXCEPT) {
        log_err("Exception on %s:%d, closing.",
                inet_ntoa(reader->addr.addr), reader->addr.port);
        return 1;
    }

    return 0;
}


/**
 * Read from the local and write to the remote
 */

static void conn_local_io_proc(io_atom *ioa, int flags)
{
    struct conn *conn = io_resolve_parent(ioa, struct conn, local.io);
    conn_perform_io(flags, &conn->local, &conn->remote);

    // check to see if remote has closed his end of the connection
    if(conn->local.state == EP_CLOSING || conn->remote.state == EP_CLOSING) {
        log_dbg("Local connection requested to close, so closing it.");
        conn_destroy(conn);
    }
}


/**
 * Read from the remote and write to the local
 */

static void conn_remote_io_proc(io_atom *ioa, int flags)
{
    struct conn *conn = io_resolve_parent(ioa, struct conn, remote.io);
    conn_perform_io(flags, &conn->remote, &conn->local);

    // check to see if remote has closed her end of the connection
    if(conn->local.state == EP_CLOSING || conn->remote.state == EP_CLOSING) {
        log_dbg("Remote connection requested to close, so closing it.");
        conn_destroy(conn);
    }
}


/**
 * Called when we are certain that the tunnel is up.
 * This initiates the remote connection and starts data moving.
 */

void conn_remote_is_ready(struct conn *conn)
{
    // open the outgoing socket
    tunnel_get_local_addr(conn->tunnel, &conn->remote.addr);
    io_connect(&conn->remote.io, conn_remote_io_proc,
            conn->remote.addr, conn->remote.io_flags);
    if(conn->remote.io.fd < 0) {
        // TODO: this should return an HTTP reply to the caller
        log_err("Could not connect to %s:%d: %s",
                inet_ntoa(conn->remote.addr.addr), conn->remote.addr.port,
                strerror(errno));
        conn_destroy(conn);
        return;
    }

    conn->remote.state = EP_OPEN;

    log_info("Opened remote connection to %s:%d on fd %d",
            inet_ntoa(conn->remote.addr.addr), conn->remote.addr.port,
            conn->remote.io.fd);

    if(conn->remote.buf.cursor > 0) {
        log_info("  %d bytes were queued up in the write buffer.",
                conn->remote.buf.cursor);
        conn_perform_write(&conn->local, &conn->remote);
    }
}


/**
 * Converts a pointer to a connection that we find in a list
 * to a pointer to the connection itself.
 */

struct conn* conn_from_list(struct list_head *list)
{
    return list_entry(list, struct conn, list);
}


struct conn* conn_create(struct tunnel *tunnel)
{
    struct conn *conn = calloc(1, sizeof(struct conn));
    if(conn) {
        conn->local.io.fd = -1;
        conn->local.io_flags = IO_READ;
        conn->remote.io.fd = -1;
        conn->remote.io_flags = IO_READ;

        conn->local.buf.cursor = 0;
        conn->local.buf.size = ENDPOINT_BUFSIZ;
        conn->local.state = EP_INIT;

        conn->remote.buf.cursor = 0;
        conn->remote.buf.size = ENDPOINT_BUFSIZ;
        conn->remote.state = EP_INIT;

        list_init(&conn->list);
        conn->tunnel = tunnel;

        log_dbg("Connection %08lX created.", (long)conn);
    }

    return conn;
}


void conn_accept(io_atom *ioa, struct tunnel *tunnel)
{
    int err;

    struct conn *conn = conn_create(tunnel);
    if(!conn) {
        return;
    }

    // accept the incoming socket
    io_accept(&conn->local.io, conn_local_io_proc,
            conn->local.io_flags, ioa, &conn->local.addr);
    if(conn->local.io.fd < 0) {
        // I have *no idea* why our listen socket gets asked to
        // connect when the tunnel's child closes.  It's weird.  TODO!
        log_err("Could not accept incoming socket: %s", strerror(errno));

        conn_destroy(conn);
        return;
    }
    conn->local.state = EP_OPEN;

    log_info("Opened local connection to %s:%d on fd %d",
            inet_ntoa(conn->local.addr.addr), conn->local.addr.port,
            conn->local.io.fd);

    err = tunnel_add_conn(tunnel, &conn->list);
    if(err) {
        // TODO: write the error to the client.
        // config_static_output(tunnel, msg)
        log_err("Conn couldn't be added: %s\n", tunnel_get_error_message());
        conn_destroy(conn);
        return;
    }
}


/**
 * Close all fds because we're about to fork but don't worry about
 * releasing memory because that all gets blown away on exec anyway.
 */

void conn_close(struct conn *conn)
{
    if(conn->local.io.fd >= 0) {
        io_close(&conn->local.io);
    }

    if(conn->remote.io.fd >= 0) {
        io_close(&conn->remote.io);
    }
}


void conn_destroy(struct conn *conn)
{
    conn_close(conn);
    if(!list_is_empty(&conn->list)) {
        tunnel_remove_conn(conn->tunnel, &conn->list);
    }

    memset(conn, 0, sizeof(struct conn));
    free(conn);

    log_dbg("Connection %08lX destroyed.", (long)conn);
}

