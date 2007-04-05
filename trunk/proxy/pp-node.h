/* pushpull.h
 * Scott Bronson
 * 30 Mar 2007
 * 
 * This file defines pushpull nodes.  A sequence of nodes
 * creates a pushpull pipeline.
 * 
 * The notable thing about a pushpull pipe is that data can
 * either be pushed by the source toward the destination, or
 * it can be pulled by the destination from the source.
 * Whether the pipe is pushing or pulling is chosen automatically
 * based on where the bottleneck is.
 * 
 * We use minibufs for all intermediate buffering.  This is
 * because data could stall at any stage.
 * 
 * 
 * NO BUFFERING TRAFFIC JAMS
 * 
 * When you have a long pushpull pipe,
 * there's a lot of buffer capacity in there.  When a downstream writer can't write,
 * we want the pipe to stop immediately -- we don't want all upstream buffers
 * to have to fill up before we halt the reader.  We could end up processing and
 * buffering hundreds of K of data before we discover that the writer has
 * actually disappeared and all that data needs to be thrown away.
 * 
 * Therefore, it's possible for downstream to consume the entire buffer and
 * still return an error saying "no more pushes until I pull."  That just means
 * that someone even further downstream has halted the pipe, and input should
 * stop even though there's still buffer capacity that could be filled.
 * 
 * 
 * FLUSHING FLAG
 * 
 * We need a way of pushing all in-progress data through to the end of the pipe right?
 * I might have a state machine that won't release a token until it sees an EOF.
 * Therefore, when upstream sends the last bytes, it includes a flag that tells
 * downstream that this is the last data in the entire connection so flush everything
 * because the very next call that arrives will be a close.
 * 
 * 
 * CLOSING
 * 
 * You can receive a close event at any time.  There's no need to recieve a packet
 * with the final flag set first.  This happens when one end resets without warning,
 * or the admin performs an aggressive shutdown.
 * 
 * A close event is now just a push_data or pull_data event with a NULL minibuf.
 * This allows the close event to come from either upstream or downstream, or even
 * somewhere in the middle (if the middle node fires the event both upstream and
 * downstream).  Close events may come at _any_ time.
 * 
 * When a close event arrives, all data currently in the pipe is abandoned.
 * A proper shutdown will first flush the rest of the data through the pipe so
 * that when the close event does execute, the pipe is totally empty.
 * 
 * 
 * BUFFER TYPES
 * 
 * There are two ways of moving data: 1. in big, complete chunks (think minibufs).
 * 2. In small, discrete units (think charp/len combos).  The minibufs are better
 * when handling massive, copying transforms, like ssl -> plaintext.  The small
 * units are better when doing discrete transforms (removing a header instance).
 * Neither one is always better than the other.
 * 
 * So, what buffering do we choose for pushpull pipes?  Both.  The problem with
 * using charps is that you are passing buffered data downstream...  the data
 * needs to have already been read and stored in memory somewhere for it to be
 * in a charp.  And that memory needs to be managed, because there's a good chance
 * that downstream will refuse to accept the entire buffer.  And then what?
 * 
 * So charps work well if inserted inbetween minibuf stages, but they don't stand
 * alone very well.  If we have multiple stages that need to do the discrete
 * transforms, we'll chain them via charps.  Most things, though, will just
 * pass around minibufs.
 * 
 * 
 * RETURN VALUES
 * 
 * Once a node is called with a buffer, it takes full responsibility to ensure
 * the buffer gets delivered to the next node downstream.  It need only be
 * responsible for one buffer at a time, though.  If it is given a buffer but
 * it can't push it further downstream, it returns PP_STALL.  When an upstream
 * node receives PP_STALL, it promises not to send it another node until
 * downstream asks.
 * 
 * Once a pipe is stalled, all pushing is stopped and data may only be pulled.
 * Data continues to be pulled until a PP_RESUME event gets passed upward.
 * Once PP_RESUME is passed, then push events may start again.
 * 
 * The sequence:
 * 
 * push push push STALL pull pull RESUME push STALL pull RESUME push etc.
 * 
 * 
// * WRONG WRONG WRONG:
// * 
// * You send information
// * downstream in as small chunks as you can (so you don't waste time copying
// * to reposition something that just has to be repositioned in the next stage
// * anyway), but you offer a stage the opportunity to assemble it into a big
// * buffer again before processing.
// * 
// * That is why all data is moved using charp/len combos (if the compiler is
// * good, it can just leave these in registers).  In addition, a flags parameter
// * will notify downstream whether more data will be arriving immediately.
// * 
// * So, stages that work best in small chunks will move data only when necessary,
// * and stages that require copying anyway (such as decoders) will buffer up the
// * small chunks until they have a good work unit, then transmit in one big go.
// * 
// * 
// * PARTIAL FLAG
// * 
// * This tells the receiver that this particular chunk of data is smaller than
// * expected and more data is probably going to arrive almost immediately.
// * For instance, if I wanted to rewrite a header, I'd send the first part of the
// * buffer downstream with the PARTIAL flag set, the new header out of a static
// * buffer, then rest of the upstream buffer.
// * 
// * This saves us from having to copy the buffer up or down in memory just
// * because we've added a header.  If the next stage also added a header, we
// * have just saved two rather large copies.
// * 
// * If the next stage is a gzip stage, though, there's no need to avoid unnecessary
// * copies.  gzip needs to copy the entire buffer anyway.  So, it buffers up what
// * it can and then processes all the data in one big chunk.  This should be much
// * more L2 cache friendly than trying to compress tons of little tiny chunks.
// * 
// * It's the PARTIAL flag that allows pushpull pipes to use either of the two
// * buffering strategies (see BUFFER TYPES above), whatever is the best for the
// * job.
 * 
 */

#ifndef PUSHPULL_H
#define PUSHPULL_H

#include "minibuf.h"


struct pushpull_node;

typedef int (*accept_pushed_data_proc)(pushpull_node *pp, minibuf *data, int flushing);
typedef int (*send_pulled_data_proc)(pushpull_node *pp, minibuf *data);
typedef void (*close_node_proc)(pushpull_node *pp);

typedef struct pushpull_node {
	struct pushpull_node *upstream;
	struct pushpull_node *downstream;
	process_pushed_data_proc process_pushed_data;
	send_pulled_data_proc send_pulled_data;
} pushpull_node;


// Only called by "subclasses" that create pushpull nodes with functionality.
void pushpull_init(pushpull_node *node, pushpull_node *upstream);

#endif /* PUSHPULL_H */


void pushpull_init(pushpull_node *node, pushpull_node *upstream, accept_pushed_data_proc push, send_pulled_data_proc pull)
{
	node->upstream = upstream;
	upstream->downstream = node;
	node->process_pushed_data = push;
	node->send_pulled_data = pull;
}
