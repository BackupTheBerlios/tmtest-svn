/**
 * minidst-minibuf.c
 * Scott Bronson
 * March 2007
 * 
 * This file offers a destination for a minio source that writes to another file
 * descriptor.  If the write descriptor refuses the data, the minidst buffers it
 * in a minibuf and turns off read events in the minio source.
 * 
 * In other words, this file implements one half of a full-duplex proxy connection.
 */

#include <values.h>
#include <string.h>
#include <stdio.h>

#include "minidst-minibuf.h"


static int mb_get_buffer(minio_poller *mpoll, struct minio_source *src, char **buf)
{
	minibuf_destination *dst = io_resolve_parent(src->destination, minibuf_destination, minio);
	assert(!dst->readbuf);	// it is an error to have readbuf occupied here.
	
	dst->readbuf = minibuf_acquire(dst->bufpool);
	*buf = dst->readbuf->data;
	return dst->bufpool->bufsiz;
}


static void mb_push_data(minio_poller *mpoll, struct minio_source *src, int rlen)
{
	minibuf_destination *dst = io_resolve_parent(src->destination, minibuf_destination, minio);
	size_t wlen;
	int err;
	
	if(rlen) {
		err = io_write(&mpoll->poller, &dst->write_atom->io, dst->readbuf->data, rlen, &wlen);
		assert(err ? !wlen : 1);
		assert(wlen ? !err : 1);
		
		printf("wrote %d chars of %d chars to %d\n", (int)wlen, (int)rlen, dst->write_atom->io.fd);
		
		if(rlen < wlen || err == EAGAIN || err == EWOULDBLOCK) {
			// TODO TODO
			// The remote didn't accept all our data.  We need to turn off
			// read events until the write events can catch up.
			assert(!"Need to add write throttling!");
			// return so that we don't return the minibuf to the free pool.
			return;
		}
	} else {
		// Apparently the reader didn't have anything to read after all.
		// We'll simply return the minibuf we allocated back to the free pool.
	}
	
	minibuf_return(dst->bufpool, dst->readbuf);
	dst->readbuf = NULL;
}


static void mb_close(minio_poller *mpoll, minio_destination *mdst)
{
	assert(!"Need to add the ability to close!");
}


/**
 * To help with chicken-and-egg problem, write_atom is not dereferenced until
 * data starts moving.  Therefore, you can pass the address of an atom that
 * hasn't been initialized yet just so long as it ends up fully initialized
 * by the time the consume_data routine is called.
 */

void minibuf_destination_init(minibuf_destination *dst, minio_source *write_atom, minibuf_pool *pool)
{
	minio_destination_init(&dst->minio, mb_get_buffer, mb_push_data, mb_close);
	dst->write_atom = write_atom;
	dst->readbuf = NULL;
	dst->bufpool = pool;
}








///////////////////////////////////////////////////////////////////////////////////////////////////////
///                                                                                                 ///
///                                       UNIT TESTS                                                ///
///                                                                                                 ///
///////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef NDEBUG

#include "unit-tests.h"
#include "io/pollers/mock.h"


// This sets up a straight, two-way proxy and hits it with some bench tests.
/*
void fullduplex_proxy_bench_test(io_poller poller, int clifd, int servfd)
{
	
	// This illustrates a straight proxy workload...
	// We connect the clifd->servfd and servfd->clifd via xfer minibufs.
	
	minio_source client;
	minibuf_destination csd_rec, *cliserv_dst=&csd_rec;
	minio_source server;
	minibuf_destination scd_rec, *servcli_dst=&scd_rec;
	
	// set up the client -> server pipe
	minibuf_destination_init(cliserv_dst, &server, xfer_minibuf);
	minio_source_init(poller, clifd, cliserv_src, cliserv_dst);
	// and the server -> client pipe
	minibuf_destination_init(servcli_dst, &client, xfer_minibuf);
	minio_source_init(poller, servfd, servcli_src, servcli_dst);

	// There, now 
}
*/



// No, this is too goddamn nondeterministic.  Probably will never be
// useful.  (I wrote the mock poller so we wouldn't have to put up with
// this kind of indeterminism when testing).
/*
void minidst_pipe_test_prepare()
{
	thread_env env_rec, *env = &env_rec;
	minio_poller mpoll_rec, *mpoll=&mpoll_rec;		// poller
	minibuf_pool minibuf_xfer_rec, *xfer_minibuf = &minibuf_xfer_rec;
	int pinput[2], poutput[2];

	thread_env_init(env);
	minio_poller_init(mpoll);
	minibuf_pool_create(env, xfer_minibuf, 8, 20);

	if(pipe(pinput) == -1) {
    	Fail("pipe(pinput) failed: %s\n", strerror(errno));
    }
	if(pipe(poutput) == -1) {
    	Fail("pipe(poutput) failed: %s\n", strerror(errno));
    }

	halfduplex_bench_test(env, xfer_minibuf,
			pinput[0], poutput[1], pinput[1], poutput[0]);
	
	close(pinput[0]);
	close(pinput[1]);
	close(poutput[0]);
	close(poutput[1]);
	
	minibuf_pool_dispose(xfer_minibuf);
	minio_poller_dispose(mpoll);
	thread_env_dispose(env);
}
*/


static const mock_connection listener = {
	"listener", mock_socket, "127.0.0.1:6543"
};

static const mock_connection incoming = {
	"incoming", mock_socket, "127.0.0.1:30000"	// originating address of the incoming connection
};

static const mock_connection outgoing = {
	"outgoing", mock_socket, "127.0.0.1:49153"	// originating address of the outgoing connection
};


static const mock_event_queue half_duplex_events = {
	MAX_EVENTS_PER_SET, {
	{
		// client a requests a connection
		{ EVENT, mock_listen, &listener, "127.0.0.1:6543" },
	},
	{
		{ EVENT, mock_event_read, &listener },
		{ EVENT, mock_accept, &incoming, "127.0.0.1:6543" },	// destination address of the incoming connection
		{ EVENT, mock_connect, &outgoing, "127.0.0.1:45000" },	// destination address of the outgoing connection
	},
	{
		{ EVENT, mock_event_read, &incoming },
		{ EVENT, mock_read, &incoming, MOCK_DATA("hi\n") },
		{ EVENT, mock_write, &outgoing, MOCK_DATA("hi\n") },
	},
	{
		// Ensure that, even though there are no more events,
		// the application still drains the entire read buffer.
		{ EVENT, mock_read, &incoming, MOCK_ERROR(EAGAIN) },
	},
	{
		{ EVENT, mock_event_read, &incoming },
		{ EVENT, mock_read, &incoming, MOCK_DATA("ha\n") },
		{ EVENT, mock_write, &outgoing, MOCK_DATA("ha\n") },
	},
	{
		{ EVENT, mock_event_read, &incoming },
		{ EVENT, mock_read, &incoming, MOCK_DATA("") },
		{ EVENT, mock_close, &incoming },
		{ EVENT, mock_close, &outgoing },
	},
	{
		{ EVENT, mock_finished }		// null terminator
	}
}};


struct proxy_conn {
	minio_source source;
	minibuf_destination destination;
	
	// this is unfortunate...
	minio_source dstatom;
};

struct proxy_listener {
	io_atom atom;				// listens for incoming connections
	socket_addr service;		// connect all incoming connections to here.
	minibuf_pool *minibufs;		// minibufs for the proxied connections to use.
	thread_env *env;			// TODO: can this be removed?
	
	// this definitely doesn't belong here:
	struct proxy_conn conn;
};

#define DEFAULT_PROXY_PORT 9999


static void proxy_accept_proc(io_poller *inpoll, io_atom *inatom)
{
	struct proxy_listener *listener = io_resolve_parent(inatom, struct proxy_listener, atom);
	minio_poller *poller = io_resolve_parent(inpoll, minio_poller, poller);
	struct proxy_conn *conn = &listener->conn;
	socket_addr remote;
	int err;

	minibuf_destination_init(&conn->destination, &conn->dstatom, listener->minibufs);

	
	err = io_accept(&poller->poller, &conn->source.io, NULL, NULL, IO_READ, inatom, &remote);
	if(err) {
		fprintf(stderr, "%s while accepting connection from %s:%d\n",
				 strerror(err), inet_ntoa(remote.addr), remote.port);
		return;
	}
	minio_source_init(poller, &conn->source, &conn->destination.minio);

	printf("proxy: Connection opened from %s port %d, given fd %d\n",
		inet_ntoa(remote.addr), remote.port, conn->source.io.fd);

	
	
	err = io_connect(&poller->poller, &conn->dstatom.io, NULL, NULL, listener->service, IO_READ);
	if(err) {
		fprintf(stderr, "%s while connecting to %s:%d\n",
				 strerror(err), inet_ntoa(remote.addr), remote.port);
		assert(!"TODO: handle this");
		return;
	}
	// if this were full-duplex, it would point back to the source atom.
	minio_source_init(poller, &conn->dstatom, NULL);
	
	printf("proxy: Connection from %s:%d linked to %s:%d (fd %d -> fd %d)\n",
			inet_ntoa(remote.addr), remote.port, 
			inet_ntoa(listener->service.addr), listener->service.port, 
			conn->source.io.fd, conn->dstatom.io.fd);
}


static void create_proxy_listener(struct minio_poller *poller,
		struct proxy_listener *listener,
		const char *listen, const char *service, minibuf_pool *minibufs)
{
	socket_addr sock = { { htonl(INADDR_ANY) }, DEFAULT_PROXY_PORT };
	const char *errstr;
	int err;
	
	listener->minibufs = minibufs;
	
	// first parse service's address
	errstr = io_parse_address(service, &listener->service);
	if(errstr) {
		fprintf(stderr, errstr, service);
		exit(1);
	}
	
	// then parse listener's address
	errstr = io_parse_address(listen, &sock);
	if(errstr) {
		fprintf(stderr, errstr, listen);
		exit(1);
	}

	err = io_listen(&poller->poller, &listener->atom, proxy_accept_proc, sock);
	if(err) {
		fprintf(stderr, "io_listen on %s:%d failed: %s\n", 
				inet_ntoa(sock.addr), sock.port, strerror(err));
		assert(!"TODO: handle this");
		return;
	}
	
	printf("Opened listening socket on %s:%d, fd=%d\n",
		inet_ntoa(sock.addr), sock.port, listener->atom.fd );
	
	return;
}


static void halfduplex_bench_test(thread_env *env, minibuf_pool *minibufs)
{
	io_mock_set_events(&env->poller->poller, &half_duplex_events);
	create_proxy_listener(env->poller, env->listener, "127.0.0.1:6543", "127.0.0.1:45000", minibufs);
	
	for(;;) {
		minio_loop_once(env->poller);
	}
	
	// This will never get reached -- the mock test will exit early.
}


void minidst_test_prepare()
{
	thread_env env_rec, *env = &env_rec;
	minio_poller mpoll_rec, *mpoll=&mpoll_rec;		// poller
	minibuf_pool minibuf_xfer_rec, *xfer_minibuf = &minibuf_xfer_rec;

	struct proxy_listener listener;
	
	minio_poller_init(mpoll, IO_POLLER_MOCK);
	thread_env_init(env, mpoll, &listener);
	minibuf_pool_create(env, xfer_minibuf, 8, 20);

	halfduplex_bench_test(env, xfer_minibuf);

	minibuf_pool_dispose(xfer_minibuf);
	minio_poller_dispose(mpoll);
	thread_env_dispose(env);
}


void minidst_minibuf_tests()
{
	minidst_test_prepare();
}

#endif

