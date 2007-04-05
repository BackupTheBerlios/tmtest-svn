/**
 * minio is a buffering layer on top of the IO Atom library.
 * 
 * minibuf-io uses minibuf buffering and the IO Atom library
 * to automate transferring data between sockets.  Most notably,
 * it automatically throttles data transfers.
 * 
 * Here is an overview of the minibuf-io layer.
 * 
 * Read Proc:  (called in response to a read event)
 *   Check upstream.  Is there still unhandled data?
 *   No: read incoming data into a new minibuf and pass it upstream.
 *     (if, during the read, we discover that the remote has closed,
 *     we'll call the close proc to dispose of everything)
 *   Yes: fill the existing buffer higher.
 *     Is there too much data in the existing buffer?
 *     No: continue as normal
 *     Yes: shut down read notifications so the writer can catch up
 *       to the reader.
 *       But: if we do this, how do read notifications get turned back on?
 *         A write notification will have to realize that we now have
 *         enough room to reasonably perform another read and re-enable reads.
 * 
 * Concepts:
 * 
 * 
 * ONE READ PER FD PER LOOP
 * 
 * Posix requires edge-triggered I/O to read each FD until exhaustion
 * (it returns an error, usually EAGAIN).  The problem is, syscalls are
 * expensive and only getting more expensive!  Syscall overhead was
 * around 100 cycles on PIIIs, closer to 500 cycles on P4s
 * (see contrib/syscall.c or http://lkml.org/lkml/2002/12/30/154).
 * 
 * Instead, after receiving bytes from an FD, we put the FD in a ready list
 * to be used the next time through the event loop.  If there was an event
 * on that FD, it's handled as ususal.  If not, and if it's in the ready
 * list, then we perform another read on it.  There is a new ready list
 * for each time through the event loop (actually, we just alternate lists,
 * like double-buffering).
 * 
 * The list operations are extremely cacheable and pipelineable (small,
 * inline, no branching) so they should be significantly faster than even
 * the fastest system call.
 * 
 * 
 * BUFFERING
 * 
 * The Nagle algorithm is good at coalescing small packets into bigger
 * ones but it absolutely destroys latency.  Therefore, since we're
 * explicitly buffering anyway, we turn it off.  We don't use the
 * TCP_CORK socket option, but we do provide a way of corking buffers.
 * Make sure to process as much data in each burst as possible, and
 * cork and uncork at any time (because minio corking is entirely
 * userspace, it's almost free).
 * 
 * Part of the reason I want to run with TCP_NODELAY is because if
 * you see a performance problem, the reason will be apparent in a
 * packet trace: tons of tiny packets.  Transparency is a good thing.
 * 
 * 
 * MORE ON BUFFERING
 * 
 * The reader doesn't buffer at all.  It's entirely up to the destination
 * where the data gets read.  This allows us to use a header-parsing
 * destination that reads into 28K minibufs, then switch to a data-transfer
 * destination that uses 2K minibufs, all without changing the reader.
 * 
 * 
 * EVENTS BEING WATCHED
 * 
 * I could just enable IO_READ|IO_WRITE but then we'd receive a ton
 * of useless write events every time we sent a buffer.  Can I leave
 * IO_READ always enabled and just turn on IO_WRITE when we have data
 * piling up?  Yes, because we should only get a few spurious reads
 * before the kernel's input buffer fills up and starts NAKing the
 * remote.  Is it worth it?
 * 
 * 
 * COALESCING
 * 
 * We definitely do coalesce writes (see BUFFERING, TCP_CORK and
 * TCP_NODELAY above).  It would be an obvious thing to coalesce
 * reads too.  For instance, if we read a partial buffer, write
 * doesn't consume it, why not read again into the remaining unused
 * space in the original read buffer?
 * 
 * Because our buffers are too damn big for this.  If we coalesce
 * reads, we would end up buffering 24K before slowing down the
 * source.  That would eat up great loads of memory and CPU for
 * no appreciable gain.  We need to keep buffer payloads as small
 * as possible.
 * 
 * In fact, this indicates to me that the writer needs to tell the
 * reader where to read!  Ye gads, that's the answer.  That also
 * allows us to use different sized buffers depending on stream
 * state.  Lovely, another big change.
 * 
 * 
 * LAZY DELETION
 * 
 * Even if we use list_for_each_safe, that means that we can only
 * delete the current list item.  When a close event arrives, there's
 * a good chance that we'll want to close and delete a bunch of other
 * list items as well.  There is no general-purpose way to allow you
 * to delete arbitrary items from a list while you're iterating over
 * it.
 * 
 * There is anothr problem with this: what happens if we delete a
 * connection, but there's an outstanding event on it?  Much easier
 * just to wait until all events have been handled, then delete all
 * connections.
 * 
 * Therefore, we're going to have a deadpool singly-linked list.
 * Connections that need to be closed are added to the deadpool
 * at any time.  Then, just before waiting again, minio runs through
 * the pool and purges each item.
 * 
 * 
 * DESTINATION CHAINS
 * 
 * We need to allow processing like this:
 * 
 *     minio_source -> unssl -> ungzip -> minio_minibuf -> service
 * 
 * If the service can't write the entire ungzipped buffer, it needs
 * to turn off read events until it can get things moving again.  But
 * how do we get things moving?  If we turn on read events again, the
 * reader will just discover that unssl already has a minibuf full of
 * data waiting to be processed.
 * 
 * First, minio_minibuf needs to tell minio_source to stop.  It then
 * registers a write event on itself and waits.  As soon as space has
 * been freed up, it calls ungzip to finish sending its data.
 * If ungzip can free up its output buffer, it calls on unssl, and
 * unssl calls minio_source.
 *
 * 
 * PUSH vs PULL
 * 
 * While processing data, sometimes we need to push (when the writer
 * is faster than the reader) and sometimes we need to pull (when the
 * reader is faster than the writer).  All minio streams start in the
 * push state, so data is pushed through the pipe when it becomes
 * available.
 * 
 * If a stage within the pipe gets blocked up (say the reader is faster
 * than the writer), then a call to push data (minio_push_proc) doesn't
 * consume all data supplied.  When this happens, all pushers are expected
 * to stop immediately and wait for a pull.  When the pull arrives, if it
 * consumes all data, then the pipe will go back to pushing.  As long as
 * either a push or a pull fails, though, the downstream stage is expected
 * to pull the next time a push or a pull won't fail.
 * 
 * In other words, we push push push.  When a push doesn't consume all the
 * data, the pusher turns off IO_READ events and waits.  If ever a puller can't
 * consume all the data, it turns on IO_WRITE events and, when it's possible
 * to write again, fires off some pull calls.  If the pulls drain the pipe
 * of all data, the first stage turns IO_READ events back on and adds itself
 * to the read_ready_list.
 */

#include <netinet/tcp.h>
#include <values.h>

// TODO: remove me!!
#include <stdio.h>

#include "list.h"
#include "io/poller.h"
#include "env.h"
#include "minio.h"


void minio_destination_init(minio_destination *dst, minio_get_buffer_proc get_buffer, minio_push_proc consume, minio_close_proc close)
{
	dst->get_buffer = get_buffer;
	dst->consume = consume;
	dst->close = close;
}


void minio_read(io_poller *in_poller, io_atom *in_atom)
{
	minio_poller *mpoll = io_resolve_parent(in_poller, minio_poller, poller);
	minio_source *msrc = io_resolve_parent(in_atom, minio_source, io);

	char *buf;
	int bufsiz;
	size_t len;
	int err;

	// take ourselves off the old read ready list.
	if(msrc->read_ready_list.next) {
		list_remove(&msrc->read_ready_list);
	}
	
	// find out where the destination wants us to write data
	bufsiz = (*msrc->destination->get_buffer)(mpoll, msrc, &buf);
	if(!bufsiz) {
		// If there's no room to read, the read event should
		// already be turned off!
		assert(!"Destination returned 0-length buffer, should not be this careless!");
	}
	// we always do a single read each time through the event loop.
	err = io_read(&mpoll->poller, &msrc->io, buf, bufsiz, &len);
	
	// Need to always call the write routine.  That way, if the 
	// if the write routine allocated a buffer, it's still freed
	// even if the read returns an error.
	(*msrc->destination->consume)(mpoll, msrc, len);

	// shortcut the common case
	if(len) {
		assert(err == 0);
		// add ourselves to the new read ready list to ensure that
		// we'll be called the next time through the loop.
		list_add(&mpoll->read_ready_list, &msrc->read_ready_list);
		return;
	}
	
	assert(len == 0);
	switch(err) {
	case 0:
		// the shortcut above took care of htis.
		assert(!"No error and no bytes read?!");
		break;
	
	case EAGAIN:
#if EAGAIN != EWOULDBLOCK
	case EWOULDBLOCK:		// stupid C...  why is a duplicate symbol an error?
#endif
		assert(len == 0);
		// No data read means no need to requeue the socket!
		break;
		
	case EPIPE:
	case ECONNRESET:
		// remote has closed connection, close immediately.
		assert(len == 0);
		assert("!TODO TODO: we need to close here");
		break;
	
	default:
		// an unknown error!!  close immediately.
		assert("!TODO TODO: we need to close here");
	}
}


static void minio_write(io_poller *in_poller, io_atom *in_atom)
{
	assert(!"minio_write");
	
	/*
	minio_poller *mpoll = io_resolve_parent(in_poller, minio_poller, poller);
	minio_source *msrc = io_resolve_parent(in_atom, minio_source, io);

	TODO: reach back and turn on read events.  Can we pull data?
	*/
}


static int set_nodelay(int sd)
{
    int flag = 1;
    return setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
}


/** This creates one entire leg of a communication stream.
 *  
 * @param dst: the destination to send the data to.
 * 
 * To change an atom's destination, use minio_atom_change_destination().
 * 
 * Atom need not be initialized.  You still need to call io_add on the
 * atom if you haven't already.
 * 
 * All atoms are initialized IO_READ by default.  If you prepare and add
 * the atom before calling minio_source_init (say, by calling
 * io_accept()), make sure IO_READ is turned on and IO_WRITE is off.
 */

void minio_source_init(minio_poller *poller, minio_source *src, minio_destination *dst)
{
	int err;
	
	if(!io_is_mock(&poller->poller)) {
		err = set_nodelay(src->io.fd);
		if(err) {
			// TODO: need to turn this into a logging message
			assert(!"set_nodelay returned an error?!");
		}
	}
	
	io_atom_init(&src->io, src->io.fd, minio_read, minio_write);
	src->curflags = IO_READ;
	src->destination = dst;
	list_reset(&src->read_ready_list);
	list_reset(&src->write_ready_list);
}



void minio_poller_init(minio_poller *mpoll, io_poller_type type)
{
	io_poller_init(&mpoll->poller, type);
	list_init(&mpoll->read_ready_list);
	list_init(&mpoll->write_ready_list);
}


void minio_poller_dispose(minio_poller *mpoll)
{
	io_poller_dispose(&mpoll->poller);
	assert(list_is_empty(&mpoll->read_ready_list));
	assert(list_is_empty(&mpoll->write_ready_list));
}


void minio_loop_once(minio_poller *mpoll)
{
	list_head temp_read_list;
	list_head temp_write_list;
	list_head *cur, *tmp;
	

	if(io_wait(&mpoll->poller, MAXINT) < 0) {
		perror("io_wait"); // TODO!!!
	}

	// move the ready lists onto private list heads so that the
	// sources can add themselves to the empty lists while dispatching.
	list_init(&temp_read_list);
	list_move(&mpoll->read_ready_list, &temp_read_list);
	list_init(&temp_write_list);
	list_move(&mpoll->write_ready_list, &temp_write_list);
	
	// Dispatching will remove a bunch of atoms from the temp_lists and add them to
	// the new ready list (happens when we get back-to-back events).
	io_dispatch(&mpoll->poller);
	
	// Now settle up with all remaining readied atoms.  Need to use _safe because
	// the source will remove themselves from the old ready list when finished.
	list_for_each_safe(cur, tmp, &temp_read_list) {
		minio_source *atom = list_entry(cur, minio_source, read_ready_list);
		minio_read(&mpoll->poller, &atom->io);
	}
	list_for_each_safe(cur, tmp, &temp_write_list) {
		minio_source *atom = list_entry(cur, minio_source, write_ready_list);
		minio_write(&mpoll->poller, &atom->io);
	}
	
	// We're finished dispatching, the running list should be empty.
	assert(list_is_empty(&temp_read_list));
	assert(list_is_empty(&temp_write_list));
}
