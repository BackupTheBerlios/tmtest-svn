#include <stdlib.h>
#include <string.h>
#include "env.h"


void thread_env_init(thread_env *env, struct minio_poller *poller, struct proxy_listener *listener)
{
	init_counter_table(&env->counters);
	env->poller = poller;
	env->listener = listener;
}


void thread_env_dispose(thread_env *env)
{
}


void* env_alloc(thread_env *env, size_t size)
{
	counter_inc(&env->counters, env_blocks_allocated);
	counter_add(&env->counters, env_bytes_allocated, size);
	return malloc(size);
}


void env_free(thread_env *env, void *mem)
{
	counter_inc(&env->counters, env_blocks_freed);
	free(mem);
}

