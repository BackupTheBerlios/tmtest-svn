/* mempool.h
 * Scott Bronson
 * 19 Mar 2007
 */

// to import the likely macro
#include "global.h"

typedef void* (*memory_alloc_proc)(size_t size, void *arg);
typedef void (*memory_dealloc_proc)(void *mem, void *arg);


typedef struct mempool_element {
	struct mempool_element *next;
} mempool_element;


struct mempool_array;


/** A mempool contains any number of fixed-size objects available
 * for allocation.
 * 
 * A mempool contains one or more mempool_arrays.
 * A mempool_array contains one or more mempool_elements.
 * All free mempool_elements are maintained in a singly-linked list.
 * 
 * You can add arrays to increase the number of elements in the
 * pool.
 * 
 * During normal usage, the linked list of free
 * pools gets hopelessly tangled, making it very hard to
 * free a pool.  It's probably possible but it's easier
 * just to delete the whole thing and start over.
 */

typedef struct mempool {
	mempool_element *first_free;	///< the first free element in the pool
	int element_size;				///< size of each element (adjusted for alignment, i.e. a 12 byte element aligned to an 8 byte boundary would have an element_size of 16)
	int total_elements;				///< Number of elements in the pool (sum of mempool_array::num_elements for each group in the pool)
	int free_elements;				///< the number of free memory blocks in the pool right now.
	int alignment;					///< The byte boundary we should align the mempool_elements to.  Should be 1 for characters, 4 for ints, 8 for pointers, 16 in rare circumstances.  If in doubt, use 8.
	
	memory_alloc_proc alloc; 				///< how to fetch more memory to store the memory pools
	memory_dealloc_proc free; 				///< how to free the memory allocated for the pools.
	void *alloc_arg; 				///< argument to pass to the alloc proc

	struct mempool_array *arrays;	///< The storage arrays.
} mempool;


void *mempool_alloc_array(mempool *pool);


/** Procures a memory block from the pool for use.
 * 
 * When you're done, return it to the pool using mempool_return().
 * Use mempool_pool_create() to allocate a memory pool.
 * 
 * If another mempool_array needs to be allocated, calls through
 * to mempool_alloc_array().
 */

static inline void* mempool_alloc(mempool *pool)
{
	mempool_element *elem = pool->first_free;
	if(likely(elem)) {
		pool->first_free = elem->next;
		pool->free_elements -= 1;
		return elem;
	}

	// We're out of elements in this pool!  Allocate more.
	return mempool_alloc_array(pool);
}


static inline void mempool_return(mempool *pool, void *inelem)
{
	mempool_element *elem = inelem;
	elem->next = pool->first_free;
	pool->first_free = elem;
	pool->free_elements += 1;
}


mempool* mempool_create(mempool *pool,
			int nbufs, int bufsiz, int alignment,
			memory_alloc_proc alloc, memory_dealloc_proc free, void *alloc_arg);

/// Call this to grow a pool before you actually run out
/// (i.e. number of free buffers goes below a low-water mark)
mempool* mempool_grow(mempool *pool, int nbufs);

/// Gets rid of the pool and all arrays backed by it.
/// It's up to you to ensure that all elements have been
/// returned before calling this.
void mempool_dispose(mempool *pool);
