#ifndef TRANSPORT_C_BIND_HPP
#define TRANSPORT_C_BIND_HPP

#include "transport_c_bind.hpp"
#include "transport_recv.hpp"
#include "transport_emit.hpp"

#include <stdexcept>

CBaseRecvTransport tr_recv_init(TransportTypeEnum t, const char *name,
                              InternalStorageEnum req)
{
    auto req_cpp = static_cast<InternalStorageEnum>(req);

    switch(t)
    {
        case TYPE_A:
            return CBaseRecvTransport{new A_Transport(name, req_cpp)};
            break;
        case TYPE_ISIO:
            return CBaseRecvTransport{new ImageStreamIORecv(name, req_cpp)};
            break;
        default: // This just an error case.
            throw std::runtime_error("tr_recv_init - invalid enum " + std::to_string(t));
    }
}

void tr_recv_close(CBaseRecvTransport *tport) {
    delete tport->impl;
    tport->impl = nullptr;
}

void tr_recv_print_type(CBaseRecvTransport tport)
{
    return tport.impl->print_type();
}

IMAGE_METADATA* tr_recv_md_(CBaseRecvTransport tport) {
    return tport.impl->md();
}

void* tr_recv_ptr_(CBaseRecvTransport tport) {
    return tport.impl->ptr();
}

void tr_recv_sync_barrier(CBaseRecvTransport tport) {
    return tport.impl->sync_barrier();
}
void tr_recv_move_data_to_requested(CBaseRecvTransport tport){
    return tport.impl->move_new_data_to_requested();
}

// Emit API
// ctor / dtor
CBaseEmitTransport tr_emit_init(TransportTypeEnum t, const char* name, InternalStorageEnum req) {
    auto req_cpp = static_cast<InternalStorageEnum>(req);
    return CBaseEmitTransport{new Z_Transport(name, req_cpp)};
}
void tr_emit_close(CBaseEmitTransport *tport) {

}

// Attributes
IMAGE_METADATA* tr_emit_md_(CBaseEmitTransport tport) {
    return nullptr;
}
void* tr_emit_ptr_(CBaseEmitTransport tport) {
    return nullptr;
}

// Methods
void tr_emit_print_type(CBaseEmitTransport tport) {

}
void tr_emit_sync_barrier(CBaseEmitTransport tport) {

}
void tr_emit_move_data_to_requested(CBaseEmitTransport tport) {

}

#endif
