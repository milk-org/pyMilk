#include <string.h>
#include <zmq.h>
#include "tlib_zmq.h"

// TODO probably I don't want this file to exist and I just want to embed it in my C++ classes.

/* ---------------------------------------------------------------------------
 * Wire protocol
 *
 * Each published message is 3 ZMQ frames:
 *   [0] topic       : image name as a null-terminated string (used by
 *                     subscribers to filter; subscribe to "" to receive all)
 *   [1] wire header : MILK_ZMQ_WIRE_HEADER  (fixed-size, host byte-order for
 *                     now - sender and receiver are assumed same endianness)
 *   [2] pixel data  : raw bytes, imdatamemsize bytes
 *
 * The receiver reconstructs / updates an IMAGE from frames [1]+[2].
 * ---------------------------------------------------------------------------*/

/* ---- init / teardown ----------------------------------------------------- */

MILK_ZMQ_CONTEXT milk_zmq_pub_init(void *ptr, uint64_t data_size,
                                   const char *endpoint,
                                   const char *topic)
{
    MILK_ZMQ_CONTEXT ctx;
    memset(&ctx, 0, sizeof(ctx));

    ctx.zmq_ctx   = zmq_ctx_new();
    ctx.socket    = zmq_socket(ctx.zmq_ctx, ZMQ_PUB);
    int rc = zmq_bind(ctx.socket, endpoint);
    printf("zmq_bind(%s) -> %d (errno=%d)\n", endpoint, rc, errno); fflush(stdout);

    strncpy(ctx.endpoint, endpoint, sizeof(ctx.endpoint) - 1);
    ctx.ptr       = ptr;
    ctx.data_size = data_size;
    ctx.is_pub    = 1;

    return ctx;
}

MILK_ZMQ_CONTEXT milk_zmq_sub_init(void *ptr, uint64_t data_size,
                                   const char *endpoint, const char *topic)
{
    MILK_ZMQ_CONTEXT ctx = {0};

    ctx.zmq_ctx   = zmq_ctx_new();
    ctx.socket    = zmq_socket(ctx.zmq_ctx, ZMQ_SUB);
    int rc = zmq_connect(ctx.socket, endpoint);
    printf("zmq_connect(%s) -> %d (errno=%d)\n", endpoint, rc, errno); fflush(stdout);

    /* Subscribe to a specific image name, or "" for everything */
    zmq_setsockopt(ctx.socket, ZMQ_SUBSCRIBE, topic, strlen(topic));

    strncpy(ctx.endpoint, endpoint, sizeof(ctx.endpoint) - 1);
    ctx.ptr       = ptr;
    ctx.data_size = data_size;
    ctx.is_pub    = 0;

    return ctx;
}

void milk_zmq_teardown(MILK_ZMQ_CONTEXT *ctx)
{
    if(!ctx)
    {
        return;
    }
    if(ctx->socket)
    {
        zmq_close(ctx->socket);
    }
    if(ctx->zmq_ctx)
    {
        zmq_ctx_destroy(ctx->zmq_ctx);
    }
    ctx->socket  = NULL;
    ctx->zmq_ctx = NULL;
}

/* ---- send ---------------------------------------------------------------- */

/*
 * Snapshot the metadata and send one complete frame.
 * Caller must hold any write lock on the image if needed.
 * Returns 0 on success, -1 on zmq error.
 */
int milk_zmq_send(MILK_ZMQ_CONTEXT *ctx)
{
    if(!ctx->ptr)
    {
        return -1;
    }

    /* Stamp mandatory protocol fields; caller is responsible for the rest */
    ctx->wire_hdr.magic         = MILK_NETWORK_MAGIC;
    ctx->wire_hdr.version       = MILK_ZMQ_VERSION;
    ctx->wire_hdr.imdatamemsize = ctx->data_size;

    /* Frame 0: topic = image name from wire header */
    if(zmq_send(ctx->socket, ctx->wire_hdr.name,
                strlen(ctx->wire_hdr.name), ZMQ_SNDMORE) < 0)
    {
        return -1;
    }

    /* Frame 1: wire header */
    if(zmq_send(ctx->socket, &ctx->wire_hdr, sizeof(ctx->wire_hdr),
                ZMQ_SNDMORE) < 0)
    {
        return -1;
    }

    /* Frame 2: pixel data */
    if(zmq_send(ctx->socket, ctx->ptr, (size_t)ctx->data_size, 0) < 0)
    {
        return -1;
    }

    return 0;
}

/* ---- recv ---------------------------------------------------------------- */

/*
 * Receive one message and write pixel data into ctx->ptr.
 * ctx->last_hdr is updated on every successful receive so the caller can
 * inspect shape/type/timestamp without holding an IMAGE pointer.
 *
 * Returns  0 on success
 *         -1 on zmq error
 *         -2 on protocol mismatch (bad magic / version)
 *         -3 on size mismatch between wire and local image
 */
int milk_zmq_recv(MILK_ZMQ_CONTEXT *ctx)
{
    zmq_msg_t msg_topic, msg_hdr, msg_data;
    zmq_msg_init(&msg_topic);
    zmq_msg_init(&msg_hdr);
    zmq_msg_init(&msg_data);

    int rc = 0;

    /* Frame 0: topic (image name) */
    if(zmq_msg_recv(&msg_topic, ctx->socket, 0) < 0)
    {
        rc = -1;
        goto cleanup;
    }

    /* Frame 1: wire header */
    if(zmq_msg_recv(&msg_hdr, ctx->socket, 0) < 0)
    {
        rc = -1;
        goto cleanup;
    }
    if(zmq_msg_size(&msg_hdr) != sizeof(MILK_WIRE_HEADER))
    {
        rc = -2;
        goto cleanup;
    }

    {
        MILK_WIRE_HEADER *hdr = (MILK_WIRE_HEADER *)zmq_msg_data(&msg_hdr);

        if(hdr->magic != MILK_NETWORK_MAGIC || hdr->version != MILK_ZMQ_VERSION)
        {
            rc = -2;
            goto cleanup;
        }

        /* Frame 2: pixel data */
        if(zmq_msg_recv(&msg_data, ctx->socket, 0) < 0)
        {
            rc = -1;
            goto cleanup;
        }

        if(zmq_msg_size(&msg_data) != (size_t)hdr->imdatamemsize)
        {
            rc = -3;
            goto cleanup;
        }

        /* Stash last header so caller can inspect metadata */
        ctx->last_hdr = *hdr;

        /* Copy pixel data into the caller-supplied buffer */
        if(ctx->ptr)
        {
            if(ctx->data_size < hdr->imdatamemsize)
            {
                rc = -3;
                goto cleanup;
            }
            memcpy(ctx->ptr, zmq_msg_data(&msg_data), (size_t)hdr->imdatamemsize);
        }
    }

cleanup:
    zmq_msg_close(&msg_topic);
    zmq_msg_close(&msg_hdr);
    zmq_msg_close(&msg_data);
    return rc;
}
