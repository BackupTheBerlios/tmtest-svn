/* read-fd.h
 * Scott Bronson
 * 30 Dec 2004
 *
 * This allows you to feed an re2c scanner directly from a
 * Unix file descriptor.
 */


#include "read.h"


scanstate* readfd_attach(scanstate *ss, int fd);


// convenience functions:
scanstate* readfd_open(const char *path, size_t bufsiz);
void readfd_close(scanstate *ss);

