/* pp-copy.h
 * Scott Bronson
 * 31 Mar 2007
 * 
 * This file implements a do-nothing pushpull node.
 * It's like pp-null.h, except that we actually allocate
 * a new minibuf and copy data from the source to the
 * destination.  This is the model that you can base
 * transforming nodes on, such as ssl or gzip.
 * 
 * This is the worst-case scenario, written as a test case.
 */

#ifndef PPCOPY_H_
#define PPCOPY_H_

#include <assert.h>
#include "pp-node.h"


typedef struct pp_copy_node {
	pushpull_node ppnode;
	minibuf_pool *pool;
	minibuf buf;	///< When pushing, contains the buffer that we're preparing for downstream.
	int offset;
	int mincopy;
	int maxcopy;
} ppcopy_node;


void ppcopy_init(ppcopy_node *node);


#endif /*PPCOPY_H_*/


/** called by upstream
 * 
 * - Data arrives from upstream.
 * TOPLOOP:
 *   - Do we already have a pending downstream packet?
 *     - No?  Allocate a minibuf
 *   - copy as much as we can from the upstream packet to the downstream.
 *   - Is it big enough to send downstream?
 *     - No?  Assert that the upstream packet was entirely used, return the minibuf
 *       - return no error.
 *   - Send the packet and forget it, it's downstream's problem now.
 *   - Did we get a STALL error?
 *     - Yes?  Store any data we didn't send and return STALL.
 *   - Is all upstream data used up?
 *     - Yes?  return no error.
 *   - Go back to TOPLOOP
 */

static int accept_pushed_data(ppnull_node *node, minibuf *ubuf, int flushing)
{
	minibuf *dbuf;
	int len;

	if(unlikely(!ubuf)) {
		// We're closing!  Propagate the event downstream.  TODO: deallocate!
		return (*node->ppnode.downstream.accept_pushed_data)(&node->ppnode.downstream, NULL);
	}
	

	// node->buf 
	dbuf = node->buf;
	if(!dbuf) {
		dbuf = minibuf_acquire(node->ppnode.pool);
	}

	for(;;) {
		len = node->maxcopy;
		if(len > ubuf->datalen - node->offset) {
			len = ubuf->datalen - node->offset;
		}

		memcpy(dbuf->data, ubuf->data + node->offset, len);
		dbuf->datalen = len;
		node->offset += len;
		assert(node->offset <= ubuf->datalen);

		if(len < node->mincopy && !FLUSHING) {
			// we don't have enough data to send downstream so we'll hang onto the buffer
			// and tell upstream that there's no error so it will send more data
			// (unless we're flushing in which case we send the data downstream no matter what)
			node->buf = ubuf;
			return 0;
		}

		err = (*node->ppnode.downstream.accept_pushed_data)(&node->ppnode.downstream, dbuf);
		dbuf = NULL; // downstream has just taken responsibility for dbuf, we're done with it.

		if(err == PP_STALL) {
			// except that downstream has stalled so tell upstream not to send any more data.
			node->buf = ubuf;
			return PP_STALL;
		}

		if(node->offset == ubuf->datalen) {
			// Hey, we processed all the data!  Return without error.
			node->buf = NULL;
			node->offset = 0;
			return 0;
		}

		// We sent a packet downstream but there's still more upstream data that we need to process.
		dbuf = minibuf_acquire(node->ppnode.pool);
	}
}


/** called by downstream
 * 
 * - Request by downstream for another packet
 * - 
 */
static int send_pulled_data(ppnull_node *node, minibuf **dbufp)
{
	if(unlikely(!dbufp)) {
		// We're closing.  TODO: deallocate!
		return (*node->ppnode.upstream.send_pulled_data)(&node->ppnode.upstream, NULL);
	}
	
	// Start the loop with the buffer that we may have used last time
	ubuf = node->buf;
	if(!dbuf) {
		dbuf = minibuf_acquire(node->ppnode.pool);
	}
	
	 

	int val = (*node->ppnode.upstream.send_pulled_data)(&node->ppnode.upstream, buf);
	
	return val;
}



int ppcopy_init(pp_copy_node *node, pushpull_node *upstream, int minbuf, int maxbuf)
{
	if(minbuf < 0 || minbuf > pool->bufsiz || maxbuf < minbuf || maxbuf > pool->bufsiz) {
		return EINVAL;
	}
	
	pushpull_init(&node->ppnode, upstream, push_data, pull_data, pool);
	
	node->buf = NULL;
	node->offset = 0; 
	node->mincopy = minbuf;
	node->maxcopy = maxbuf;
	
	return 0;
}
