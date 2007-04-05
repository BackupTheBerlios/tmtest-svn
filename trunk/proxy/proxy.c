/**
 * proxy.c
 * Scott Bronson
 * 30 Mar 2007
 * 
 * This file handles connecting two minio-minibuf streams together
 * into a full-duplex proxy.
 */

#include "minidst-minibuf.h"


// This function just returns the same service address every time it's called.

void proxy_static_service()
{
	// first parse service's address
	errstr = io_parse_address(service, &listener->service);
	if(errstr) {
		fprintf(stderr, errstr, service);
		return -1;
	}
	
	if(listener->service.port == -1) {
		return -2;
	}	
}
	

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


int proxy_create_listener(struct minio_poller *poller, struct proxy_listener *listener,
		const char *listen, const char *service, minibuf_pool *minibufs)
{
	socket_addr sock = { { htonl(INADDR_ANY) }, -1 };
	const char *errstr;
	int err;
	
	listener->minibufs = minibufs;
	
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


int proxy_create_static_responder()



///////////////////////////////////////////////////////////////////////////////////////////////////////
///                                                                                                 ///
///                                       UNIT TESTS                                                ///
///                                                                                                 ///
///////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef NDEBUG

#include "unit-tests.h"
#include "io/pollers/mock.h"


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


void proxy_test_halfduplex()
{
	thread_env env_rec, *env = &env_rec;
	minio_poller mpoll_rec, *mpoll=&mpoll_rec;		// poller
	minibuf_pool minibuf_xfer_rec, *xfer_minibuf = &minibuf_xfer_rec;

	struct proxy_listener listener;
	
	minio_poller_init(mpoll, IO_POLLER_MOCK);
	thread_env_init(env, mpoll, &listener);
	minibuf_pool_create(env, xfer_minibuf, 8, 20);

	io_mock_set_events(&env->poller->poller, &half_duplex_events);
	proxy_create_listener(env->poller, env->listener, "127.0.0.1:6543", "127.0.0.1:45000", minibufs);
	
	for(;;) {
		// TODO: somehow we need to make this exit.
		minio_loop_once(env->poller);
	}

	minibuf_pool_dispose(xfer_minibuf);
	minio_poller_dispose(mpoll);
	thread_env_dispose(env);
}


void minidst_minibuf_tests()
{
	proxy_test_halfduplex();
}

#endif

