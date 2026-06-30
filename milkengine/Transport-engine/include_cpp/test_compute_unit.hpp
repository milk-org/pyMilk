#ifndef TEST_COMPUTE_UNIT_HPP
#define TEST_COMPUTE_UNIT_HPP

#include "transport_enums.h"
#include "transport_recv.hpp"
#include "transport_emit.hpp"

class ComputeUnit
{
  public:
    ComputeUnit(
        const char *name,
        const char *recv_proto_string,
        InternalStorageEnum recv_memloc,
        const char *emit_proto_string,
        InternalStorageEnum emit_memloc
    );

    void loop_once();
    void emit_tport_deferred_init(const char *emit_proto_string,
                                  IMAGE_METADATA *md_request,
                                  InternalStorageEnum emit_memloc);

    const char *name_;
    RecvTransport *recv_tport_ = nullptr;
    EmitTransport *emit_tport_ = nullptr;

  protected:
    const char* emit_proto_string_;
    InternalStorageEnum emit_memloc_;
};

#endif // ifndef TRANSPORT_COMPUTE_UNIT_HPP
