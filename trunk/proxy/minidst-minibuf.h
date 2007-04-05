/* minidst-minibuf.h
 * 
 * Defines the minibuf-driven minio destination.
 */

#include "minio.h"
#include "minibuf.h"


typedef struct {
	minio_destination minio;
	minio_source *write_atom;
	minibuf_pool *bufpool;
	minibuf *readbuf;
} minibuf_destination;


void minibuf_destination_init(minibuf_destination *dst, minio_source *write_atom, minibuf_pool *pool);


