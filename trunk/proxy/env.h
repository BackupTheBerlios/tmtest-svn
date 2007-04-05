/* env.h
 * Scott Bronson
 * 10 Mar 2007
 * 
 * A data structure that keeps track of all the utilities neede by a thread.
 * 
 * Utilities:
 * - memory management
 * - instrumentation counters
 * - logging
 */

#include "counters.h"
//#include "io/poller.h"

struct minio_poller;
struct proxy_listener;


typedef struct thread_env {
	counter_table counters;
	struct minio_poller *poller;
	struct proxy_listener *listener;	// TODO: this must go away!
} thread_env;


void thread_env_init(thread_env *env, struct minio_poller *poller, struct proxy_listener *listener);
void thread_env_dispose(thread_env *env);

void* env_alloc(thread_env *env, size_t size);
void env_free(thread_env *env, void *mem);

