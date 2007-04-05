/* mempool.c
 * Scott Bronson
 * 19 Mar 2007
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "mempool.h"

// I very carefully kept instrumentation counters out of this
// code.  It's up to the mempool client to increment the counters.


/// mempool_elements are stored in a linked list of mempool_arrays.

typedef struct mempool_array {
	struct mempool_array *next;	///< if the pool needed to be grown
	int num_elements;			///< number of elements in this particular pool
} mempool_array;


/** Grows the pool to allocate more mempool_elements, then returns
 * the next free element.
 * 
 * @param pool: a pool created with mempool_create().
 * @returns: the element to use.  It's impossible for this routine
 * to return null.  If it truly can't allocate any more elements,
 * it exits the entire program.
 * 
 * If the pool is out of elements, it will automaitcally allocate
 * more.  It will create the same number of elements as you
 * originally created with mempool_create().
 */

void* mempool_alloc_array(mempool *pool)
{
	mempool_element *buf;
	
	assert(pool);
	assert(!pool->first_free);	// ensure mempool_alloc has already failed.
	
	// By default we'll grow by the same number of
	// elements first allocated.
	mempool_grow(pool, pool->arrays->num_elements);
	buf = pool->first_free;
	if(!buf) {
		die("Out of elements!");
	}
	
	pool->first_free = buf->next;
	pool->free_elements -= 1;
	
	return buf;
}


/** Returns the given size with enough padding to ensure the given alignment.
 * 
 * i.e., if size is 11 and padding is 4, returns 12.
 * If size is 9 and padding is 8, returns 16.
 */

static int get_padded_size(int size, int padding)
{
	// make sure we have enough room to store our free lists.
	if(size < sizeof(mempool_element)) {
		size = sizeof(mempool_element);
	}
	
	return ((size + padding - 1) / padding) * padding;
}


/** Creates a pool of elements.
 * 
 * @param head The memory that contains the pool head.
 * @param num_elements The number of buffers to create
 * @param element_size The maximum size in bytes for each buffer's payload.
 * @param alloc A routine to be used to allocate the pool's memory.
 * 
 * Use mempool_alloc() and mempool_return() to check elements
 * out of the pool for use and then return them when you're done.
 * Use mempool_dispose() to dispose of this pool and all
 * pools added to it using mempool_grow().
 */

mempool* mempool_create(mempool *head,
		int num_elements, int element_size, int alignment,
		memory_alloc_proc alloc, memory_dealloc_proc free, void *alloc_arg)
{
	head->first_free = NULL;
	head->element_size = get_padded_size(element_size, alignment);
	head->total_elements = 0;
	head->free_elements = 0;
	head->alignment = alignment;
	
	head->alloc = alloc;
	head->free = free;
	head->alloc_arg = alloc_arg;
	
	head->arrays = NULL;
	
	assert(num_elements > 0);
	assert(element_size > 0);
	
	if(!mempool_grow(head, num_elements)) {
		return NULL;
	}
	
	return head;
}


/** Adds more elements to an already-allocated pool.
 * 
 * @param head: the head pool for the array.  You must pass the first
 * 	pool allocated for the array; any pool in the array will not do.
 * 
 * TODO: how do we ensure that the appropriate counter gets incremented?
 */

mempool* mempool_grow(mempool *pool, int num_elements)
{
	mempool_element **tochange;
	mempool_array *array;
	int i, size, array_size;
	unsigned char *ptr;
	
	assert(pool);
	assert(num_elements > 0);

	array_size = get_padded_size(sizeof(mempool_array), pool->alignment);
	size = array_size + num_elements * pool->element_size;
	array = (*pool->alloc)(size, pool->alloc_arg);
	if(!pool) {
		return NULL;
	}
	
	array->num_elements = num_elements;
	
	// Link this array into the pool's list of arrays
	array->next = pool->arrays;
	pool->arrays = array;

	// we add the elements from the new pool on the end
	// of the current list of free elements...

	// Scan to the end of the list.
	tochange = &pool->first_free;
	while(*tochange) {
		tochange = &(*tochange)->next;
	}

	ptr = (unsigned char*)array + array_size;
	for(i=0; i<num_elements; i++) {
		*tochange = (mempool_element*)ptr;
		tochange = &((mempool_element*)ptr)->next;
		ptr += pool->element_size;
	}
	*tochange = NULL;
	assert(ptr == (unsigned char*)array + size);
	
	pool->total_elements += num_elements;
	pool->free_elements += num_elements;
	
	return pool;
}


/** Disposes of all element pools in this array.
 * 
 * @param pool: The pool to dispose.
 * 
 * You must be certain of course that all mininum_elements have been returned
 * to the free list using element_free() before deleting the pool.
 * (we don't check for this now but we may in the future)
 */

void mempool_dispose(mempool *pool)
{
	mempool_array *array, *next;
	
	// Make sure that all elements have been freed
	assert(pool->free_elements == pool->total_elements);

	// free all the arrays in the pool
	array = pool->arrays;
	while(array) {
		next = array->next;
		(*pool->free)(array, pool->alloc_arg);
		array = next;
	}
	
	INVALIDATE_PTR(&pool->arrays);
	INVALIDATE_PTR(&pool->first_free);
}


#ifndef NDEBUG

#include "mutest/mu_assert.h"
#include <stdlib.h>


int total_allocations;
int total_bytes_allocated;
int total_frees;


static void* mock_pool_alloc(size_t size, void *arg)
{
	void *mem;

	// element_tests is just a random value to check that arg is passed.
	AssertPtrEqual(arg, mock_pool_alloc);
	mem = malloc(size);
	AssertPtrNonNull(mem);
	
	total_allocations += 1;
	total_bytes_allocated += size;
	return mem;
}


static void mock_pool_free(void *ptr, void *arg)
{
	AssertPtrEqual(arg, mock_pool_alloc);
	total_frees += 1;
	free(ptr);
}


typedef struct {
	int total_elements;		///< total number of buffers counted in the pool
	int free_elements;		///< number of free buffers counted in the pool
	int narrays;		///< number of arrays in the pool
} check_pool_vals;


static void check_pool(mempool *pool, check_pool_vals *vals)
{
	mempool_element *buf;
	mempool_array *array;

	memset(vals, 0, sizeof(check_pool_vals));

	// first collect data on the arrays
	for(array=pool->arrays; array; array=array->next) {
		vals->narrays += 1;
		vals->total_elements += array->num_elements;
	}
	
	// Total number of elements in the pool needs to match
	AssertEqual(vals->total_elements, pool->total_elements);
	
	// then collect data on the elements
	for(buf=pool->first_free; buf; buf=buf->next) {
		vals->free_elements += 1;
	}
	
	// claimed number of free buffers needs to match the counted number
	AssertEqual(vals->free_elements, pool->free_elements);
}


static void test_mempool_grow()
{
	void *buf1, *buf2, *buf3;

	mempool ppool, *pool = &ppool;
	check_pool_vals vvals, *vals = &vvals;
		
	mutest_start("mempool_grow", "Tests creating a 1-element pool and growing it twice.")
	{
		total_allocations = 0;
		total_frees = 0;
		
		// set the alloc_arg to a random value so we can check it later
		mempool_create(pool, 1, 256, 8, mock_pool_alloc, mock_pool_free, mock_pool_alloc);
		
		check_pool(pool, vals);
		AssertEqual(vals->total_elements, 1);
		AssertEqual(vals->free_elements, 1);
		AssertEqual(vals->narrays, 1);
		AssertEqual(total_allocations, 1);
		
		// Since we allocate more bufs only when the free elements
		// list bottoms out, this fetch won't trigger another allocation.
		buf1 = mempool_alloc(pool);
		AssertPtr(buf1);
		AssertZero(pool->free_elements);
		AssertPtrNull(pool->first_free);

		mempool_return(pool, buf1);
		AssertEqual(pool->first_free, buf1);	// 
		AssertEqual(pool->free_elements, 1);

		
		
		// good, a create / fetch / free cycle just worked.
		// now let's test automatic allocation.

		// first, grab the free element
		buf1 = mempool_alloc(pool);
		AssertPtr(buf1);
		
		// Then grab another element to force allocation of a new array.
		// Automatically-allocated pools have identical
		// characteristics to the first pool created.
		buf2 = mempool_alloc(pool);
		AssertPtr(buf2);
		AssertPtrNotEqual(buf1, buf2);
		AssertEqual(pool->free_elements, 0);
		AssertPtrNull(pool->first_free);
		AssertEqual(pool->total_elements, 2);
		AssertEqual(pool->free_elements, 0);

		check_pool(pool, vals);
		AssertEqual(vals->total_elements, 2);
		AssertEqual(vals->free_elements, 0);
		AssertEqual(vals->narrays, 2);
		AssertEqual(total_allocations, 2);

		// And create a third array to test a few corner cases.
		buf3 = mempool_alloc(pool);
		AssertPtr(buf3);
		AssertPtrNotEqual(buf1, buf3);
		AssertPtrNotEqual(buf2, buf3);
		AssertEqual(pool->free_elements, 0);
		AssertPtrNull(pool->first_free);
		AssertEqual(pool->total_elements, 3);
		AssertEqual(pool->free_elements, 0);

		check_pool(pool, vals);
		AssertEqual(vals->total_elements, 3);
		AssertEqual(vals->free_elements, 0);
		AssertEqual(vals->narrays, 3);
		AssertEqual(total_allocations, 3);

		// we'll return the bufs to the pool out of order
		mempool_return(pool, buf1);
		AssertEqual(pool->first_free, buf1);
		AssertEqual(pool->free_elements, 1);
		
		mempool_return(pool, buf3);
		AssertEqual(pool->first_free, buf3);
		AssertEqual(pool->free_elements, 2);

		mempool_return(pool, buf2);
		AssertEqual(pool->first_free, buf2);
		AssertEqual(pool->free_elements, 3);
		
		// We're done!  Dispose of the pool
		mempool_dispose(pool);
		AssertEqual(total_allocations, 3);
		AssertEqual(total_frees, 3);
	}
}


void mempool_tests()
{
	test_mempool_grow();
	
	/*
	test_allocation(1, 1, 1024);
	test_allocation(2, 12, 1024);
	test_allocation(37, 5, 256);
	test_allocation(8, 4, 1);
	*/
}

#endif
