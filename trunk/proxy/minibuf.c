/* minibuf.c
 * Scott Bronson
 * 10 Mar 2007
 */

#include "minibuf.h"


static void* minibuf_alloc(size_t size, void *arg)
{
	thread_env *env = arg;
	
	counter_inc(&env->counters, minibuf_arrays_allocated);
	return env_alloc(env, size);
}


static void minibuf_free(void *block, void *arg)
{
	thread_env *env = arg;
	
	counter_inc(&env->counters, minibuf_arrays_freed);
	env_free(env, block);
}


void minibuf_pool_create(thread_env *env, minibuf_pool *head, int num_bufs, int payload_size)
{
	counter_inc(&env->counters, minibuf_pools_created);

	head->bufsiz = payload_size;
	head->counters = &env->counters;
	
	if(!mempool_create(&head->mem, num_bufs,
			sizeof(minibuf) + payload_size,
			sizeof(void*), minibuf_alloc, minibuf_free, env)) {
		die("Could not allocate memory for minibuf_pool_create");
	}
}


void minibuf_pool_grow(minibuf_pool *pool, int nbufs)
{
	counter_inc(pool->counters, minibuf_pool_grows);
	mempool_grow(&pool->mem, nbufs);
}


void minibuf_pool_dispose(minibuf_pool *pool)
{
	counter_inc(pool->counters, minibufs_returned);
	mempool_dispose(&pool->mem);
}
