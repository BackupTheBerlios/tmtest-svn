/* counters.h
 * Scott Bronson
 * 10 Mar 2007
 * 
 * Declares the counters used in this application.
 * 
 * Each thread has its own couter table.  Periodically a master
 * thread will read the tables of each thread to assemble one
 * global table.  Right now they always need to be added.
 * 
 * 
 */


// we might support invariants later...  hopefully periodic checks
// will highlight if the counters ever get bogus values.  Invariants
// should be listed next to the counters that they're associated with.
#ifndef INVARIANT
#define INVARIANT(v1,op,v2)
#endif

CTR(env_blocks_allocated,		"Env Blocks Allocted",
		"The number of blocks allocated for a thread_env")
CTR(env_bytes_allocated,		"Env Bytes Allocated",
		"The number of bytes allocated for a thread_env")
CTR(env_blocks_freed,			"Env Blocks Freed",
		"The number of blocks freed for a thread_env")

INVARIANT(env_blocks_allocated, >=, env_blocks_freed)


CTR(minibufs_acquired, 			"Minibufs Acquired",
		"The number of times a minibuf has been allocated from a pool.")
CTR(minibufs_returned, 			"Minibufs Returned",
		"The number of times a minibuf has been returned to its pool.")
CTR(minibuf_pools_created,		"Minibuf Pools Created",
		"Number of times a minibuf pool is created.")
CTR(minibuf_pools_disposed,		"Minibuf Pools Disposed",
		"Number of times a pool is destroyed.")
CTR(minibuf_pool_grows,			"Minibuf Pool Grows",
		"Number of times a minibuf pool has been grown.")
CTR(minibuf_arrays_allocated,	"Minibuf Arrays Allocated",
		"Number of times a new minibuf array has needed to be allocated (uses dynamic mem).")
CTR(minibuf_arrays_freed,		"Minibuf Arrays Freed",
		"Number of times a minibuf array has been freed.")

INVARIANT(minibufs_acquired, >=, minibufs_returned)
INVARIANT(minibuf_pools_created, >=, minibuf_pools_disposed)
INVARIANT(minibuf_arrays_allocated, >=, minibuf_arrays_freed)
INVARIANT(minibuf_arrays_allocated, >=, minibuf_pools_allocated)

CTR(end, "Last Counter", "Sentinel that marks the end of the counters.")

