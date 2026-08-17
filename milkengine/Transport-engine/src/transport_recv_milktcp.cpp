#include <algorithm>
#include <string>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "ImageStreamIO/ImageStreamIO.h"
#include "transport_recv_milktcp.hpp"

MilkTCPRecv::MilkTCPRecv(const char *name)
    : RecvTransport(name)
{
    // name: port number
    int port = std::stoi(name_);

    constexpr int max_pending_tcp_conns = 5;
    const int flag = 1;

    // TODO errors
    fds_recv_local_ = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    // TODO errors
    ::setsockopt(fds_recv_local_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    ::setsockopt(fds_recv_local_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ::setsockopt(fds_recv_local_, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag));
    const auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(DEFAULT_TIMEOUT);
    const timeval tv{ .tv_sec = static_cast<time_t>(timeout_us.count() / 1'000'000),
                      .tv_usec = static_cast<suseconds_t>(timeout_us.count() % 1'000'000) };
    ::setsockopt(fds_recv_local_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in sock_server =
    {
        .sin_family      = AF_INET,
        .sin_port        = ::htons(port),
        .sin_addr = {.s_addr = ::htonl(INADDR_ANY)}
    };

    ::bind(fds_recv_local_, (sockaddr *)(&sock_server), sizeof(sock_server));
    ::listen(fds_recv_local_, max_pending_tcp_conns);
    printf("MilkTCPRecv ctor terminated and listening?\n"); fflush(stdout);
}

NetErrorEnum MilkTCPRecv::wait_for_client_conn()
{
    // Forceful release of the client socket
    if(fds_emit_remote_ != -1)
    {
        ::close(fds_emit_remote_);
        fds_emit_remote_ = -1;
    }
    sockaddr_in sock_emit_remote = {};
    socklen_t slen_client = sizeof(sock_emit_remote);
    fds_emit_remote_ = ::accept(fds_recv_local_, (sockaddr *)(&sock_emit_remote),
                                &slen_client);
    printf("MilkTCPRecv - accept returns %d %d\n", fds_emit_remote_, errno); fflush(stdout);
    if(fds_emit_remote_ == -1)
    {
        switch(errno)
        {
            case EWOULDBLOCK:
            // case EAGAIN:
            case EINTR:
                return NetErrorEnum::TIMEOUT;
            default:
                return NetErrorEnum::FATAL;
        }
    }
    return NetErrorEnum::SUCCESS;
}

void MilkTCPRecv::init_storage_target(InternalStorageEnum req)
{
    req_ = req; // Nothing we can do just yet! Must do deferred init instead.
}

MilkTCPRecv::~MilkTCPRecv()
{
    // Dealloc sockets, close connections
    if(fds_emit_remote_ != -1)
    {
        ::close(fds_emit_remote_);
        fds_emit_remote_ = -1;
    }
    if(fds_recv_local_ != -1)
    {
        ::close(fds_recv_local_);
        fds_recv_local_ = -1;
    }

    // Relay req_ image from the transport
    if(image_.used == 1)
    {
        ImageStreamIO_destroyIm(&image_);
    }

    if (req_ >= 0) { // GPU therefore separate CPU buffer
        free(cpu_ptr_);
    }
}

void MilkTCPRecv::print_type()
{
    printf("This is a MILK TCP RECV transport\n");
}

void MilkTCPRecv::deferred_init()
{ // This code is unchanged from other network receivers...
    printf("MilkTCPRecv - entering deferred init\n"); fflush(stdout);

    MILK_WIRE_HEADER hdr = last_hdr_;

    std::string safe_name(name_);
    safe_name.erase(std::remove_if(safe_name.begin(), safe_name.end(),
                                   [](char c)
    {
        return c == '/' || c == ':';
    }),
    safe_name.end());

    ImageStreamIO_createIm_gpu(&image_, ("tcprecv_" + safe_name).c_str(),
                               hdr.naxis, hdr.size, hdr.datatype,
                               req_, // CPU or GPU memory
                               1, // int shared
                               IMAGE_NB_SEMAPHORE, // int NBsem
                               hdr.NBkw,
                               MATH_DATA, // uint64_t imagetype
                               0 // uit32_t CBsize
                              );
    md_ = image_.md;
    //
    ptr_ = ImageStreamIO_get_image_d_ptr(&image_);
    if (req_ >= 0) {
        cpu_ptr_ = malloc(md_->imdatamemsize);
    } else {
        cpu_ptr_ = ptr_;
    }

    needs_deferred_init_ = false;
}

// TODO return should be a triggerstatus
// TODO Really we should just take a PROCESSINFO* as argument here.
SyncEnum MilkTCPRecv::sync_barrier()
{
    if(needs_tcp_reconnect_)
    {
        printf("MilkTCPRecv - entering sync_barrier -> wait_for_client_conn\n"); fflush(stdout);
        NetErrorEnum ret = wait_for_client_conn();
        printf("MilkTCPRecv - returning sync_barrier <- wait_for_client_conn returns %d\n", ret); fflush(stdout);
        // Here really we need to write a pinfo message just say "sync failure"
        // TODO in case of pixel streamers, we may need a "partial!"
        switch(ret)
        {
            case NetErrorEnum::SUCCESS:
                needs_tcp_reconnect_ = false;
                break;
            case NetErrorEnum::DISCONNECT:
            case NetErrorEnum::TIMEOUT:
                return SyncEnum::FAILED;
            case NetErrorEnum::FATAL:
    printf("MilkTCPRecv - recv initial header: %d bytes\n", ret); fflush(stdout);
                return SyncEnum::FATAL;
        }
    }

    int ret;
    // printf("MilkTCPRecv - awaiting header\n"); fflush(stdout);
    ret = ::recv(fds_emit_remote_, &last_hdr_, sizeof(MILK_WIRE_HEADER),
                     MSG_WAITALL);
    // printf("MilkTCPRecv - recv initial header: %d bytes\n", ret); fflush(stdout);
    if(ret < sizeof(MILK_WIRE_HEADER))
    {
        needs_tcp_reconnect_ = true; // We can process the errno upon next loop.
        return SyncEnum::FAILED;
    }
    else if(last_hdr_.magic != MILK_NETWORK_MAGIC)
    {
        printf("MilkTCPRecv - Invalid magic %x\n", last_hdr_.magic); fflush(stdout);
        // TCP flush
        while(::recv(fds_emit_remote_, tcp_flush_buffer_, sizeof(tcp_flush_buffer_),
                     MSG_DONTWAIT) > 0) {printf("MilkTCPRecv - forcefully flushing socket.\n");}
        return SyncEnum::FAILED;
    }

    // TODO check buffer sizes are still valid !!
    // Like if someone reconnected but the size is different.
    // In that case you may want to return a fatal.

    if(needs_deferred_init_)
    {
        deferred_init();
    }

    // Now receive the data - scatter-gather main data + keywords (possible in the future)
    if((ret = ::recv(fds_emit_remote_, cpu_ptr_, md_->imdatamemsize, MSG_WAITALL)) < md_->imdatamemsize) {
        return SyncEnum::TIMEOUT;
    }
    if((ret = ::recv(fds_emit_remote_, image_.kw, md_->NBkw * sizeof(IMAGE_KEYWORD), MSG_WAITALL)) < md_->NBkw * sizeof(IMAGE_KEYWORD)) {
        return SyncEnum::TIMEOUT;
    }

    return SyncEnum::SUCCESS;
}


void MilkTCPRecv::move_new_data_to_requested()
{
    // The data is in cpu_buf_, maybe we need a CUDA copy.
    if (req_ >= 0) {
        cudaSetDevice(req_);
        cudaMemcpy(ptr_, cpu_ptr_, md_->imdatamemsize, cudaMemcpyHostToDevice);
    }

    // TODO UpdateIm !
    ImageStreamIO_UpdateIm_atime(&image_, &last_hdr_.atime);
}
