#include "list.h"
#include "io/poller.h"


typedef struct minio_poller {
	io_poller poller;
	list_head read_ready_list;		///< The list of minio_atoms that need to be read from the next time through the event loop.  Add atoms here.
	list_head write_ready_list;
} minio_poller;


struct minio_source;
struct minio_destination;

// routines implemented by minio destinations:
typedef int (*minio_get_buffer_proc)(minio_poller *mpoll, struct minio_source *msrc, char **buf);
typedef void (*minio_push_proc)(minio_poller *mpoll, struct minio_source *msrc, int len);
typedef void (*minio_close_proc)(minio_poller *mpoll, struct minio_destination *mdst);


typedef struct minio_destination {
	minio_get_buffer_proc get_buffer;
	minio_push_proc consume;
	minio_close_proc close;
} minio_destination;


/** A source for minio data.
 * 
 * Sources are ultra-stupid, and therefore never need customization.
 * All they do is read data into the destination's buffer, throttle,
 * and call the destination's close routine when closed.  All the
 * intelligence and customization goes into the destination.
 */

typedef struct minio_source {
	io_atom io;
	int curflags;
	minio_destination *destination;
	list_head read_ready_list;		///< node for remembering whether we need to run an extra read on this guy
	list_head write_ready_list;		///< node for remembering whether we need to run an extra write on this guy
} minio_source;




void minio_poller_init(minio_poller *mpoll, io_poller_type type);
void minio_poller_dispose(minio_poller *mpoll);

void minio_loop_once(minio_poller *mpoll);

// routines used only by minio destinations...
void minio_destination_init(minio_destination *dst, minio_get_buffer_proc get_buffer, minio_push_proc consume, minio_close_proc close);


// routines used by minio clients...
void minio_source_init(minio_poller *poller, minio_source *src, minio_destination *dst);


// Routines used only by unit test programs...
void minio_read(io_poller *in_poller, io_atom *in_atom);

