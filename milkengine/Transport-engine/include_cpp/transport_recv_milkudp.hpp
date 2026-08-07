#ifndef TRANSPORT_RECV_MILKUDP_HPP
#define TRANSPORT_RECV_MILKUDP_HPP

#include "transport_recv.hpp"
#include "tlib_zmq.h"

class MilkUDPRecv: public RecvTransport
{
  public:
    MilkUDPRecv(const char *name);
    ~MilkUDPRecv();
    void init_storage_target(InternalStorageEnum req) override;
    void print_type() override;
    SyncEnum sync_barrier() override;
    void move_new_data_to_requested() override;

  protected:
    void post_init();
    void deferred_init();

  private:
    // Receive a single datagram into dgram_buffer_. Returns SUCCESS, TIMEOUT
    // or FAILED (bad recv, short datagram, or bad magic/version). On SUCCESS
    // or FAILED-due-to-short-datagram, bytes_received holds the recvfrom()
    // return value.
    SyncEnum recv_datagram_(ssize_t &bytes_received);

    IMAGE image_ = {};
    bool needs_deferred_init_ = true; // Deferred memory init after the first packets are received

    int fds_recv_local_ = -1; // Receiving socket
    int fds_emit_remote_ = -1; // Connected sending socket

    // Network control flow
    bool needs_tcp_resync_ = true;
    bool needs_tcp_reconnect_ = true;

    MILK_WIRE_HEADER last_hdr_ = {0};

    // UDP datagram math (mirrors MilkUDPEmit), set up in deferred_init()
    int n_dgrams_ = 0; // Number of datagrams expected per frame
    size_t dgram_data_size_ = 0; // Bytes of data in each datagram except last
    size_t dgram_data_size_last_ = 0; // Bytes of data in last datagram

    void* cpu_ptr_ = nullptr;
    uint8_t tcp_flush_buffer_[4096];

    // Scratch buffer for a single received UDP datagram (header + payload)
    uint8_t *dgram_buffer_ = nullptr;

    NetErrorEnum wait_for_client_conn(); // This is the form of autorelink we want
};

#endif
