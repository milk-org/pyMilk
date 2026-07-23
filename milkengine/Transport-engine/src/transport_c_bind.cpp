#include "transport_c_bind.h"

#include "transport_recv.hpp"

#include "transport_recv_isio.hpp" // TODO transport_recv_all exporter that has all symbols?
#include "transport_recv_zmqsub.hpp"

#include "transport_emit.hpp"
#include "transport_emit_isio.hpp"
#include "transport_emit_zmqpub.hpp"

#include <stdexcept>

CBaseRecvTransport tr_recv_init(TransportTypeRecvEnum t, const char *name)
{

    // TODO should I separate a "protocol" string from a "name" string?
    // TODO this is how I built the ComputeUnit class in test_compute_unit.cpp
    switch(t)
    {
        case RECVTYPE_ISIO:
            return CBaseRecvTransport{new ImageStreamIORecv(name)};
            break;
        case RECVTYPE_ZMQSUB:
            return CBaseRecvTransport{new ZmqRecv(name)};
            break;
        default: // This just an error case.
            throw std::runtime_error("tr_recv_init - invalid enum " + std::to_string(t));
    }
}

void tr_recv_init_storage_target(CBaseRecvTransport tport,
                                 InternalStorageEnum req)
{
    auto req_cpp = static_cast<InternalStorageEnum>(req);

    tport.impl->init_storage_target(req);
}

void tr_recv_close(CBaseRecvTransport *tport)
{
    delete tport->impl;
    tport->impl = nullptr;
}

void tr_recv_print_type(CBaseRecvTransport tport)
{
    return tport.impl->print_type();
}

IMAGE_METADATA *tr_recv_md_(CBaseRecvTransport tport)
{
    return tport.impl->md();
}

void *tr_recv_ptr_(CBaseRecvTransport tport)
{
    return tport.impl->ptr();
}

int tr_recv_sync_barrier(CBaseRecvTransport tport)
{
    return static_cast<int>(tport.impl->sync_barrier());
}
void tr_recv_move_data_to_requested(CBaseRecvTransport tport)
{
    return tport.impl->move_new_data_to_requested();
}

// Emit API
// TODO: by string with proto:: prefix

// ctor / dtor
CBaseEmitTransport tr_emit_init(TransportTypeEmitEnum t, const char *name,
                                IMAGE_METADATA *md_request,
                                InternalStorageEnum req)
{
    switch(t)
    {
        case EMITTYPE_ISIO:
            return CBaseEmitTransport{new ImageStreamIOEmit(name, md_request)};
            break;
        case EMITTYPE_ZMQPUB:
            return CBaseEmitTransport{new ZmqEmit(name, md_request)};
            break;
        default: // This just an error case.
            throw std::runtime_error("tr_emit_init - invalid enum " + std::to_string(t));
    }
}

void tr_emit_init_storage_target(CBaseEmitTransport tport,
                                 InternalStorageEnum req)
{
    auto req_cpp = static_cast<InternalStorageEnum>(req);

    tport.impl->init_storage_target(req);
}

void tr_emit_close(CBaseEmitTransport *tport)
{
    delete tport->impl;
    tport->impl = nullptr;
}

// Attributes
IMAGE_METADATA *tr_emit_md_(CBaseEmitTransport tport)
{
    return tport.impl->md();
}
void *tr_emit_ptr_(CBaseEmitTransport tport)
{
    return tport.impl->ptr();
}

// Methods
void tr_emit_print_type(CBaseEmitTransport tport)
{
    return tport.impl->print_type();
}

void tr_emit_move_data_to_requested(CBaseEmitTransport tport,
                                    struct timespec *atime)
{
    return tport.impl->move_and_publish_data(atime);
}
