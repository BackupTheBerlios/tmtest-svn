// TODO: integrate with our logging so that stdio can go away!
// TODO: turn panic and die into real functions.  this is a hack.

#include <stdio.h>
// need stdlib for exit(3)
#include <stdlib.h>



#if 1
/// Specifies that an expression is likeley to be true.
/// For instance, if(likely(err==0)) { ... }
/// This really helps with branch prediction.
/// Presence of likely/unlikely indicates that you're inside an inner loop.
/// Even if the answer is bone-dead obvious, don't use these in non-performance-critical code.
#define likely(x)       __builtin_expect(!!(x), 1)
/// Specifies that an expression is unlikeley to be true.
/// For instance, if(unlikely(err) { ... }
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif


// Tries to ensure a crash if we ever use this pointer again.
#define INVALIDATE_PTR(p) (*(p) = (void*)-1)

#define panic(x) fprintf(stderr, "PANIC: %s\n", x)
#define die(x) do { panic(x); exit(222); } while(0)

