#ifndef TLIB_ZMQ_H
#define TLIB_ZMQ_H

#include <stdint.h>
#include <time.h>
#include "ImageStreamIO/ImageStruct.h"  /* STRINGMAXLEN_IMAGE_NAME */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Wire protocol constants
 * ---------------------------------------------------------------------------*/
#define MILK_NETWORK_MAGIC    0x4D494C4B   /* "MILK" */
#define MILK_ZMQ_VERSION  1
#define MILK_TCP_VERSION  1
#define MILK_UDP_VERSION  1

// Lower bound on allowable payload per UDP datagram
constexpr size_t DATAGRAM_CHUNK_SIZE = 62 * 1024;

// Multicast defaults
constexpr const char *MILK_UDP_DEFAULT_MCAST_GROUP = "239.72.55.1"; // org-local scope (RFC 2365)
constexpr int         MILK_UDP_DEFAULT_MCAST_TTL    = 1;            // cap router hops (local subnet only)

/* ---------------------------------------------------------------------------
 * MILK_WIRE_HEADER
 *
 * Sent as ZMQ frame 1 in every published message.
 * Contains the minimal IMAGE_METADATA fields needed by the receiver to:
 *   - verify compatibility
 *   - create or validate a local IMAGE
 *   - synchronize frame counters and timestamps
 * ---------------------------------------------------------------------------*/
typedef struct __attribute__((aligned(8)))
{
    uint32_t magic;                         /* MILK_ZMQ_MAGIC                 */
    uint16_t version;                       /* MILK_ZMQ_VERSION               */
    char     name[STRINGMAXLEN_IMAGE_NAME]; /* stream name                    */
    uint8_t  naxis;                         /* 1, 2 or 3                      */
    uint32_t size[3];                       /* size along each axis           */
    uint8_t  datatype;                      /* _DATATYPE_* code               */
    uint64_t nelement;                      /* total number of pixels         */
    uint64_t imdatamemsize;                 /* payload size in bytes          */
    uint64_t cnt0;                          /* frame counter from sender      */
    uint64_t cnt1;                          /* frame counter from sender      */
    uint64_t cnt_udp;                       /* datagram counter for UDP       */
    uint64_t NBkw;                          /* number of keywords from sender */
    struct timespec atime;                  /* acq timestamp from sender      */
}
MILK_WIRE_HEADER;

enum NetErrorEnum
{
    SUCCESS = 0,
    TIMEOUT = 1,
    DISCONNECT = 2,
    FATAL = -1
};

/* ---------------------------------------------------------------------------
 * MILK_ZMQ_CONTEXT
 *
 * Owns one libzmq context + one socket (PUB or SUB).
 * Holds a pointer to the local IMAGE that data flows to/from.
 * ---------------------------------------------------------------------------*/
typedef struct
{
    void     *zmq_ctx;                   /**< zmq_ctx_new() handle                  */
    void     *socket;                    /**< ZMQ_PUB or ZMQ_SUB socket             */
    char      endpoint[256];             /**< e.g. "tcp://192.168.1.1:5555"         */

    void     *ptr;                       /**< pixel data buffer (read on send, write on recv) */
    uint64_t  data_size;                 /**< size of ptr buffer in bytes           */
    int       is_pub;                    /**< 1 = publisher, 0 = subscriber         */

    MILK_WIRE_HEADER
    wire_hdr;       /**< header to send (pub); caller fills before milk_zmq_send() */
    MILK_WIRE_HEADER
    last_hdr;       /**< last wire header received (sub only)  */
} MILK_ZMQ_CONTEXT;

/* ---------------------------------------------------------------------------
 * API
 * ---------------------------------------------------------------------------*/

/**
 * Create a publisher bound to endpoint.
 * ptr/data_size point to the pixel buffer to read from on each milk_zmq_send().
 * Caller must also fill ctx.wire_hdr before the first send.
 */
MILK_ZMQ_CONTEXT milk_zmq_pub_init(void *ptr, uint64_t data_size,
                                   const char *endpoint,
                                   const char *topic);

/**
 * Create a subscriber connected to endpoint.
 * topic is the image name to filter on; pass "" to receive all streams.
 * ptr/data_size is the destination buffer; must be >= the sender's imdatamemsize.
 */
MILK_ZMQ_CONTEXT milk_zmq_sub_init(void *ptr, uint64_t data_size,
                                   const char *endpoint, const char *topic);

/** Close socket and destroy zmq context. */
void milk_zmq_teardown(MILK_ZMQ_CONTEXT *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TLIB_ZMQ_H */
