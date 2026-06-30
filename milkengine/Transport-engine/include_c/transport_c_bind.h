#ifndef TRANSPORT_C_BIND_H
#define TRANSPORT_C_BIND_H

#include "transport_enums.h"
#include "ImageStreamIO/ImageStruct.h"

#ifdef __cplusplus
#include "transport_recv.hpp"
#include "transport_emit.hpp"

extern "C" {
#endif

#ifndef __cplusplus
// This could store C-accessible things, or directly alias a void*
typedef struct
{
    void *impl;
} CBaseRecvTransport;

typedef struct
{
    void *impl;
} CBaseEmitTransport;
#endif

// Receive API
// ctor / dtor
CBaseRecvTransport tr_recv_init(TransportTypeRecvEnum t, const char* name);
void tr_recv_init_storage_target(CBaseRecvTransport tport, InternalStorageEnum req);
void tr_recv_close(CBaseRecvTransport *tport);

// Attributes
IMAGE_METADATA* tr_recv_md_(CBaseRecvTransport tport);
void* tr_recv_ptr_(CBaseRecvTransport tport);

// Methods
void tr_recv_print_type(CBaseRecvTransport tport);
void tr_recv_sync_barrier(CBaseRecvTransport tport);
void tr_recv_move_data_to_requested(CBaseRecvTransport tport);

// Emit API
// ctor / dtor
CBaseEmitTransport tr_emit_init(TransportTypeEmitEnum t, const char* name, IMAGE_METADATA md_req, InternalStorageEnum req);
void tr_emit_close(CBaseEmitTransport *tport);

// Attributes
IMAGE_METADATA* tr_emit_md_(CBaseEmitTransport tport);
void* tr_emit_ptr_(CBaseEmitTransport tport);

// Methods
void tr_emit_print_type(CBaseEmitTransport tport);
void tr_emit_sync_barrier(CBaseEmitTransport tport);
void tr_emit_move_data_to_requested(CBaseEmitTransport tport);

#ifdef __cplusplus
}
#endif


#endif
