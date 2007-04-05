/* minibuf.h
 * Scott Bronson
 * 10 Mar 2007
 */

#include <sys/types.h>
#include "mempool.h"
#include "env.h"


// TODO: should we ensure that minibufs are a multiple of the page size?
// Probably.  That means we need to subtract out the minibuf overhead and
// then say a 2048K minibuf actually stores 2032 bytes, but 2 minibufs
// fit exactly into a kernel page.  But...  Is there any guarantee malloc
// will return blocks aligned to pages?  Probably not...



/** Minibufs are used all throughout this proxy to buffer data.
 * They allow us to preserve data between event loops, yet we
 * don't need to allocate a per-connection static buffer.
 * 
 * They are super fast to acquire and return.  All minibufs
 * live in a mempool.
 * 
 * The C99 spec (n1124.pdf) §6.7.2.1:18 claims that "t1.d[0] = 4.2;" might be
 * undefined behavior.  That's retarded.  Is "*(t1.d+0)" valid?  What about
 * "(void*)&t1.d"?  The spec doesn't say.  WTF?  How is this supposed to be
 * useful??  gcc does the right thing in all cases (and has for many years)
 * so I'll use C99's "flexible array members" but this might need to change
 * if we ever use a nonconformant compiler.
 */

typedef struct minibuf {
	struct minibuf *next;	///< TODO: I think the new architecture makes this field totally unnecessary...  when will we ever queue up minibufs anymore?
	int datalen;	///< number of bytes in this minibuf that contain valid data
	char data[];	///< payload size is given by minibuf_pool::bufsiz.  See warning in ::minibuf on flexible array members.
} minibuf;


typedef struct minibuf_pool {
	int bufsiz;					///< maximum minibuf::datalen for all buffers in this pool
	mempool mem;				///< the pool itself
	counter_table *counters;	///< the instrumentation counters to update
} minibuf_pool;



static inline minibuf *minibuf_acquire(minibuf_pool *pool)
{
	minibuf *buf = mempool_alloc(&pool->mem);
	counter_inc(pool->counters, minibufs_acquired);
	
	// Initialize the minibuf
	buf->next = NULL;
	buf->datalen = 0;

	return buf;
}


static inline void minibuf_return(minibuf_pool *pool, minibuf *buf)
{
	mempool_return(&pool->mem, buf);
	counter_inc(pool->counters, minibufs_returned);
}


void minibuf_pool_create(thread_env *env, minibuf_pool *head, int num_bufs, int payload_size);
void minibuf_pool_grow(minibuf_pool *pool, int nbufs);
void minibuf_pool_dispose(minibuf_pool *pool);
