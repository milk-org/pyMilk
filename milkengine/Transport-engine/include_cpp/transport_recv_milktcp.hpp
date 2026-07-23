#ifndef TRANSPORT_RECV_MILKTCP_HPP
#define TRANSPORT_RECV_MILKTCP_HPP

#include "transport_recv.hpp"
#include "tlib_zmq.h"

class MilkTCPRecv: public RecvTransport
{
  public:
    MilkTCPRecv(const char *name);
    ~MilkTCPRecv();
    void init_storage_target(InternalStorageEnum req) override;
    void print_type() override;
    SyncEnum sync_barrier() override;
    void move_new_data_to_requested() override;

  protected:
    void post_init();
    void deferred_init();

  private:
    IMAGE image_ = {};
    bool needs_deferred_init_ = true; // Deferred memory init after the first packets are received

    int fds_recv_local_ = -1; // Receiving socket
    int fds_emit_remote_ = -1; // Connected sending socket

    // Network control flow
    bool needs_tcp_resync_ = true;
    bool needs_tcp_reconnect_ = true;

    MILK_WIRE_HEADER last_hdr_ = {0};

    void* cpu_ptr_ = nullptr;
    uint8_t tcp_flush_buffer_[4096];

    NetErrorEnum wait_for_client_conn(); // This is the form of autorelink we want
};

#endif
