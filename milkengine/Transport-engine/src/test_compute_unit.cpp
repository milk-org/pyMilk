#include <stdexcept>
#include <string.h>
#include "test_compute_unit.hpp"
#include "transport_recv_isio.hpp"
#include "transport_emit_isio.hpp"
#include "transport_recv_zmqsub.hpp"
#include "transport_emit_zmqpub.hpp"

#include "transport_enums.h"

// Parse "scheme:$$rest" and return a pointer past "::", or nullptr on mismatch.
static const char *match_scheme(const char *str, const char *scheme)
{
    size_t n = strlen(scheme);
    if(strncmp(str, scheme, n) == 0 && str[n] == ':' && str[n + 1] == ':')
    {
        return str + n + 2;
    }
    return nullptr;
}

ComputeUnit::ComputeUnit(
    const char *name,
    const char *recv_proto_string,
    InternalStorageEnum recv_memloc,
    const char *emit_proto_string,
    InternalStorageEnum emit_memloc
)
    : name_(name), recv_tport_(nullptr), emit_tport_(nullptr),
    emit_proto_string_(emit_proto_string), emit_memloc_(emit_memloc)
{
    // --- Recv transport factory ---
    const char *recv_name;
    if(recv_name = match_scheme(recv_proto_string, "shm"))
    {
        auto *t = new ImageStreamIORecv(recv_name);
        t->init_storage_target(recv_memloc);
        recv_tport_ = t;
    }
    else if ((recv_name = match_scheme(recv_proto_string, "zmq")))
    {
        recv_tport_ = new ZmqRecv(recv_name);
        // UDP transport resolves storage after inspecting stream metadata
    }
    else
    {
        throw std::invalid_argument("ComputeUnit: unknown recv protocol in: " +
                                    std::string(recv_proto_string));
    }
}

void ComputeUnit::emit_tport_deferred_init(
        IMAGE_METADATA *md_request)
{
    if(emit_tport_ != nullptr)
    {
        return;
    }

    // --- Emit transport factory ---
    const char *emit_name;
    if((emit_name = match_scheme(emit_proto_string_, "shm")))
    {
        emit_tport_ = new ImageStreamIOEmit(emit_name, md_request);
    } else if ((emit_name = match_scheme(emit_proto_string_, "zmq"))) {
        emit_tport_ = new ZmqEmit(emit_name, md_request);
    }
    else
    {
        throw std::invalid_argument("ComputeUnit: unknown emit protocol in: " +
                                    std::string(emit_proto_string_));
    }
    emit_tport_->init_storage_target(emit_memloc_);
    emit_tport_->_initialized = true;
}


void ComputeUnit::loop_once()
{
    recv_tport_->sync_barrier();
    recv_tport_->move_new_data_to_requested();

    if (emit_tport_ == nullptr) {
        IMAGE_METADATA md_req = *recv_tport_->md(); // by-value copy
        md_req.location = -1;
        emit_tport_deferred_init(&md_req);
    }


    auto recv_req = recv_tport_->req();
    auto emit_req = emit_tport_->req();
    auto dsize = recv_tport_->md()->imdatamemsize;


    if(recv_req == CPU_MEMORY && emit_req == CPU_MEMORY)
    {
        memcpy(emit_tport_->ptr(), recv_tport_->ptr(), dsize);
    }
    else if(recv_req >= 0 && emit_req == CPU_MEMORY)
    {
        cudaSetDevice(recv_tport_->md()->location);
        cudaMemcpy(emit_tport_->ptr(), recv_tport_->ptr(), dsize,
        cudaMemcpyDeviceToHost);
    }
    else if(recv_req == CPU_MEMORY && emit_req >= 0)
    {
        cudaSetDevice(emit_tport_->md()->location);
        cudaMemcpy(emit_tport_->ptr(), recv_tport_->ptr(), dsize,
        cudaMemcpyHostToDevice);
    }
    else if(recv_req == emit_req)
    {
        cudaSetDevice(recv_tport_->md()->location);
        cudaMemcpy(emit_tport_->ptr(), recv_tport_->ptr(), dsize,
        cudaMemcpyDeviceToDevice);
    }
    else
    {
        cudaMemcpyPeer(emit_tport_->ptr(), emit_tport_->md()->location,
        recv_tport_->ptr(), emit_tport_->md()->location,
        dsize);
    }

    emit_tport_->move_and_publish_data(&(recv_tport_->md()->atime));
}
