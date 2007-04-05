/* pp-znode.h
 * Scott Bronson
 * 30 Mar 2007
 *
 * This implements a compression node and a decompression node.
 * 
 * Why, even though deflate is supposed to be the wire protocol,
 * it's recommended to use gzip:
 *    http://www.gzip.org/zlib/zlib_faq.html#faq38
 * 
 * TODO: we only process one minibuf at a time.  Apparently the
 * decompression routines are MUCH faster if you can do 128K or
 * 256K at once.
 */

#ifndef PPZNODE_H
#define PPZNODE_H

#include "pp-node.h"
#include "zlib.h"



void ppz_deflate_init(pp_copy_node *node, pushpull_node *upstream);
{
	pushpull_init(&node->ppnode, upstream, push_data, pull_data);
}


#endif /*PPZNODE_H_*/
