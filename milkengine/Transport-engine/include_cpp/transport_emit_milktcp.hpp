#ifndef TRANSPORT_EMIT_MILKTCP_HPP
#define TRANSPORT_EMIT_MILKTCP_HPP

#include "transport_emit.hpp"
#include "tlib_zmq.h" // MILK_WIRE_HEADER
#include <netinet/in.h> // sockaddr_in

class MilkTCPEmit: public EmitTransport
{
  public:
    MilkTCPEmit(const char *name, IMAGE_METADATA* md_request);
    ~MilkTCPEmit();
    void init_storage_target(InternalStorageEnum req) override;
    void print_type() override;
    void move_and_publish_data(struct timespec* atime) override;

  //protected:

  private:

    IMAGE image_ = {};

    void *d_mem_segment_ = nullptr;

    int fds_emit_local_ = -1; // Receiving socket
    bool needs_tcp_reconnect_ = true;
    struct sockaddr_in sockaddr_connect_ = {0};

    MILK_WIRE_HEADER stable_hdr_ = {0};

    void recreate_socket_();
};

#endif // #ifndef TRANSPORT_EMIT_ISIO_HPP
