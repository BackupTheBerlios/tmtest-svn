/* pp-null.h
 * 
 * Declares a no-op pushpull node.
 *
 * This node just moves data from its source to its destination.
 * This is an example of how to set up a search node.  We could watch for data,
 * and, with reasonable limitations, we can change it in-place.
 * 
 * It's only for testing -- there's no reason to use this node in real life.
 */

#ifndef PPNULL_H
#define PPNULL_H

#include "pp-node.h"


typedef struct ppnull_node {
	pushpull_node ppnode;
} ppnull_node;


void ppnull_init(ppnull_node *node);

#endif /*PPNULL_H*/


// called by upstream
static int accept_pushed_data(ppnull_node *node, minibuf *buf)
{
	if(buf) {
		// look at the data in buf, possibly change it in-place
	} else {
		// We're closing!  TODO: deallocate!
	}
	
	return (node->ppnode.downstream.accept_pushed_data)(&node->ppnode.downstream, buf);
}


// called by downstream
static int send_pulled_data(ppnull_node *node, minibuf **buf)
{
	int val = (node->ppnode.upstream.send_pulled_data)(&node->ppnode.upstream, buf);

	if(bufp) {
		// look at the data in buf
	} else {
		// We're closing.  TODO: deallocate!
	}
	
	return val;
}


void ppnull_init(ppnull_node *node, pushpull_node *upstream);
{
	pushpull_init(&node->ppnode, upstream, accept_pushed_data, send_pulled_data);
}
