/**
 * proxy.h
 * Scott Bronson
 * 30 Mar 2007
 * 
 * This file handles connecting two minio-minibuf streams together
 * into a full-duplex proxy.
 */

#include "minidst-minibuf.h"



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

