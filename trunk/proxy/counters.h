/* counters.h
 * Scott Bronson
 * 10 Mar 2007
 * 
 * Defines the data structures used to keep track of the proxy's
 * instruction counters.
 * 
 * Right now all instruction counters are monotonically incrementing.
 * They either go up by 1 (number of connections) or by some value
 * (number of bytes transferred).
 * 
 * Beware the wrap!!  They are only 4 bytes on 32-bit systems so
 * rapidly increasing counters may very well wrap around.
 */


/** Symbolic identifiers for each instruction counter
 */

typedef enum {
#define CTR(name,title,description) ctr_##name,
#include "counters.inc"
#undef CTR
} counter_index;


/** A table that contains a slot for each instruction counter.
 */

typedef struct {
	long int counters[ctr_end];
} counter_table;


/** Adds an arbitrary value to an instruction counter
 */

#define counter_add(env, name, amt) ((env)->counters[ctr_##name] += (amt))


/** Increments an instruction counter by 1.
 */

#define counter_inc(env, name) counter_add(env, name, 1)


/** Initializes a new counter table to all 0s.
 */
#define init_counter_table(t) memset(t,0,sizeof(counter_table))
