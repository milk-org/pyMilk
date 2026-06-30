#ifndef TRANSPORT_EMIT_ZMQPUB_HPP
#define TRANSPORT_EMIT_ZMQPUB_HPP

#include "transport_emit.hpp"
#include "tlib_zmq.h"

class ZmqEmit: public EmitTransport
{
  public:
    ZmqEmit(const char *name, IMAGE_METADATA* md_request);
    ~ZmqEmit();
    void init_storage_target(InternalStorageEnum req) override;
    void print_type() override;
    void move_and_publish_data(struct timespec* atime) override;

  //protected:

  private:
    IMAGE image_ = {};

    MILK_ZMQ_CONTEXT milk_zmq_ctx_;

    void *d_mem_segment_ = nullptr;
};

#endif // #ifndef TRANSPORT_EMIT_ISIO_HPP
