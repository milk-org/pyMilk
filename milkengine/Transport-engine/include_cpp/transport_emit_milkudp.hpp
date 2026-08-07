#ifndef TRANSPORT_EMIT_MILKUDP_HPP
#define TRANSPORT_EMIT_MILKUDP_HPP

#include "transport_emit.hpp"
#include "tlib_zmq.h" // MILK_WIRE_HEADER
#include <netinet/in.h> // sockaddr_in

class MilkUDPEmit: public EmitTransport
{
  public:
    MilkUDPEmit(const char *name, IMAGE_METADATA *md_request);
    ~MilkUDPEmit();
    void init_storage_target(InternalStorageEnum req) override;
    void print_type() override;
    void move_and_publish_data(struct timespec *atime) override;

    //protected:

  private:

    IMAGE image_ = {};

    void *d_mem_segment_ = nullptr;

    int fds_emit_local_ = -1; // Receiving socket
    //bool needs_tcp_reconnect_ = true;
    struct sockaddr_in sockaddr_connect_ = {0};

    // UDP housekeeping
    MILK_WIRE_HEADER stable_hdr_ = {0};

    int n_dgrams_; // Number of datagams to send
    size_t dgram_data_size_; // Bytes of data to put in each datagram except last
    size_t dgram_data_size_last_; // Bytes of data to put in last datagram

    void recreate_socket_();
};

#endif // #ifndef TRANSPORT_EMIT_ISIO_HPP
