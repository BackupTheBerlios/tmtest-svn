// minidst-test.c
// Scott Bronson
// 22 Mar 2006

#include "minidst-test.h"

// This file contains a destination that allows me to write unit
// tests that ensure I/O is properly handled.  Mostly this ensures
// that read and write events are turned off/on properly.

static int minio_test_get_buffer(minio_poller *mpoll, minio_source *in_dst, char **buf)
{
	minibuf_destination *dst = io_resolve_parent(in_dst, minibuf_destination, minio);
	assert(!dst->readbuf);	// it is an error to have readbuf occupied here.
	
	dst->readbuf = minibuf_fetch(POOL);
	*buf = dst->readbuf->data;
	return POOL->bufsiz;
}


static void minio_test_consume_data(minio_poller *mpoll, minio_source *in_dst, char *buf, int len)
{

}


static void minio_test_close(minio_poller *mpoll, minio_destination *mdst)
{
}


void minio_test_destination_init(minio_destination *dst)
{
	minio_destination_init(dst, minio_test_get_buffer,
			minio_test_consume_data, minio_test_close);
}

#ifndef NDEBUG

#include <unistd.h>


/*
void fullduplex_proxy_bench_test(minio_poller poller, int clifd, int servfd)
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


// This sets up a straight, two-way proxy and hits it with some bench tests.

void halfduplex_proxy_bench_test(minio_poller poller, int clifd)
{
	
	// This illustrates a straight proxy workload...
	// We connect the clifd->servfd and servfd->clifd via xfer minibufs.
	
	minio_source client;
	minibuf_destination csd_rec, *cliserv_dst=&csd_rec;

	if(pipe(pfd) == -1) {
    	Fail("pipe() failed: %s\n", strerror(errno));
    }
	
	// set up the client -> server pipe
	minibuf_destination_init(cliserv_dst);
	minio_source_init(poller, clifd, cliserv_src, cliserv_dst);
	

	minio_read(&poller->poller, &client->io);
}

void minidst_test_prepare()
{
	thread_env env_rec, *env = &env_rec;
	minio_poller mpoll_rec, *mpoll=&mpoll_rec;		// poller
	minibuf_pool minibuf_xfer_rec, *xfer_minibuf = &minibuf_xfer_rec;
	
	thread_env_init(env);
	minio_poller_init(mpoll);
	minibuf_pool_create(env, xfer_minibuf, 8, 20);

	straight_proxy_bench_test(env, xfer_minibuf, 1, 2);
	
	minibuf_pool_dispose(xfer_minibuf);
	minio_poller_dispose(mpoll);
	thread_env_dispose(env);
}


void minidst_test_tests()
{
	minidst_test_prepare();
}


#endif