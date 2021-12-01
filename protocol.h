#pragma once

#define MAX PKT 1024 /* determines packet size in bytes */

typedef enum {false, true} boolean; /* boolean type */

/* sequence or ack numbers
 * a small integer used to number the frames so that we can tell them apart.
 * These sequence numbers run from 0 up to and including MAX SEQ,
 * which is defined in each protocol needing it.
 */
typedef unsigned int seq nr;

/* packet definition
 * A packet is the unit of information exchanged between the network layer
 * and the data link layer on the same machine, or between network layer peers.
 * In our model it always contains MAX_PKT bytes,
 * but more realistically it would be of variable length.
 */
typedef struct {unsigned char data[MAX PKT];} packet;

typedef enum {data, ack, nak} frame kind; /* frame kind definition */

/* A more realistic implementation would use a variable-length info field, 
 * omitting it altogether for control frames.
 */
typedef struct { /* frames are transported in this layer */
frame kind kind; /* what kind of frame is it? data or control only*/
seq nr seq; /* sequence number */
seq nr ack; /* acknowledgement number */
packet info; /* a single network layer packet */
} frame;

/* Wait for an event to happen.
 * return its type in event. 
 * sits in a tight loop waiting for something to happen.
 */
void wait_for_event(event type *event);

/* Fetch a packet from the network layer for transmission on the channel. */
void from_network_layer(packet *p);

/* Deliver information from an inbound frame to the network layer. */
void to_network_layer(packet *p);

/* Go get an inbound frame from the physical layer and copy it to r. */
void from_physical_layer(frame *r);

/* Pass the frame to the physical layer for transmission. */
void to_physical_layer(frame *s);

/* Start the clock running and enable the timeout event. 
 * It is explicitly permitted to call start timer while the timer is running. * such a call simply resets the clock to cause the next timeout after a full * timer interval has elapsed (unless it is reset or turned off).
*/
void start_timer(seq nr k);

/* Stop the clock and disable the timeout event. */
void stop_timer(seq nr k);

/* Start an auxiliary timer used to generate acknowledgements under certain    * conditions, and enable the ack timeout event.
 */
void start_ack_timer(void);

/* Stop the auxiliary timer and disable the ack timeout event. */
void stop_ack_timer(void);

/* Allow the network layer to cause a network layer ready event. */
void enable_network_layer(void);

/* Forbid the network layer from causing a network layer ready event. */
void disable_network_layer(void);


/* Frame sequence numbers are always in the range 0 to MAX SEQ (inclusive),
 * where MAX SEQ is different for the different protocols. It is frequently
 * necessary to advance a sequence number by 1 circularly. 
 * (i.e., MAX SEQ is followed by 0).
 * The macro inc performs this incrementing.
*/

/* Macro inc is expanded in-line: increment k circularly. */
#define inc(k) if (k < MAX SEQ) k = k + 1; else k = 0