#ifndef TRANSPORT_RECV_ZMQSUB_HPP
#define TRANSPORT_RECV_ZMQSUB_HPP

#include "transport_recv.hpp"
#include "tlib_zmq.h"

class ZmqRecv: public RecvTransport
{
  public:
    ZmqRecv(const char *name);
    ~ZmqRecv();
    void init_storage_target(InternalStorageEnum req) override;
    void print_type() override;
    void sync_barrier() override;
    void move_new_data_to_requested() override;

  protected:
    void post_init();
    void deferred_init();

  private:
    IMAGE image_ = {};
    bool needs_deferred_init_ = true;

    MILK_ZMQ_CONTEXT milk_zmq_ctx_;
};

#endif
