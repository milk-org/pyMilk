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
