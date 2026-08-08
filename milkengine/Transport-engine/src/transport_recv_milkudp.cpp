#include <algorithm>
#include <string>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "ImageStreamIO/ImageStreamIO.h"
#include "transport_recv_milkudp.hpp"

MilkUDPRecv::MilkUDPRecv(const char *name)
    : RecvTransport(name)
{
    // TODO what is name ??
    // name can be a single port if unicast receiver
    // name can be mcast group + port if mcast receiver

    int port = std::stoi(name_);

    const int flag = 1;

    // TODO errors
    fds_recv_local_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // TODO errors
    ::setsockopt(fds_recv_local_, SOL_SOCKET, SO_NO_CHECK, &flag, sizeof(flag));
    ::setsockopt(fds_recv_local_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ::setsockopt(fds_recv_local_, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag));
    timeval tv{ .tv_sec = 2, .tv_usec = 0 }; // Timeout 2 seconds
    ::setsockopt(fds_recv_local_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#ifdef SO_ATTACH_REUSEPORT_CBPF
    setsockopt(fds_recv_local_, SOL_SOCKET, SO_ATTACH_REUSEPORT_CBPF, &flag,
               sizeof(flag));
#endif

    // TODO UDP MULTICAST GROUP

    sockaddr_in sock_server =
    {
        .sin_family      = AF_INET,
        .sin_port        = ::htons(port),
        .sin_addr = {.s_addr = ::htonl(INADDR_ANY)} // TODO Multicast will need an adress.
    };

    ::bind(fds_recv_local_, (sockaddr *)(&sock_server), sizeof(sock_server));

    // Always join the default multicast group so multicast senders can
    // reach this receiver without any additional configuration.
    join_multicast_group_(::inet_addr(MILK_UDP_DEFAULT_MCAST_GROUP));

    dgram_buffer_ = (uint8_t *) malloc(sizeof(MILK_WIRE_HEADER) + DATAGRAM_CHUNK_SIZE);

    printf("MilkUDPRecv ctor terminated and socket bound\n");
    fflush(stdout);
}

void MilkUDPRecv::init_storage_target(InternalStorageEnum req)
{
    req_ = req; // Nothing we can do just yet! Must do deferred init instead.
}

MilkUDPRecv::~MilkUDPRecv()
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

    if(req_ >= 0)    // GPU therefore separate CPU buffer
    {
        free(cpu_ptr_);
    }

    free(dgram_buffer_);
    dgram_buffer_ = nullptr;
}

void MilkUDPRecv::print_type()
{
    printf("This is a MILK UDP RECV transport\n");
}

void MilkUDPRecv::join_multicast_group_(in_addr_t group_addr)
{
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = group_addr;
    mreq.imr_interface.s_addr = ::htonl(INADDR_ANY);

    if(::setsockopt(fds_recv_local_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
                    sizeof(mreq)) < 0)
    {
        printf("MilkUDPRecv - failed to join multicast group (%s)\n",
               strerror(errno));
        fflush(stdout);
    }
}

void MilkUDPRecv::deferred_init()
{
    // This code is unchanged from other network receivers...
    printf("MilkUDPRecv - entering deferred init\n");
    fflush(stdout);

    MILK_WIRE_HEADER hdr = last_hdr_;

    std::string safe_name(name_);
    safe_name.erase(std::remove_if(safe_name.begin(), safe_name.end(),
                                   [](char c)
    {
        return c == '/' || c == ':';
    }),
    safe_name.end());

    ImageStreamIO_createIm_gpu(&image_, ("udprecv_" + safe_name).c_str(),
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
    if(req_ >= 0)
    {
        cpu_ptr_ = malloc(md_->imdatamemsize);
    }
    else
    {
        cpu_ptr_ = ptr_;
    }

    // UDP datagram math (mirrors MilkUDPEmit)
    size_t frame_size_total = hdr.imdatamemsize + hdr.NBkw * sizeof(
                                  IMAGE_KEYWORD); // excluding the MILK_WIRE_HEADER size, but we can got to almost 64k so we have headroom
    n_dgrams_ = frame_size_total / DATAGRAM_CHUNK_SIZE + (frame_size_total %
                DATAGRAM_CHUNK_SIZE != 0 ? 1 : 0);
    dgram_data_size_ = DATAGRAM_CHUNK_SIZE;
    dgram_data_size_last_ = frame_size_total % dgram_data_size_;

    needs_deferred_init_ = false;
}

SyncEnum MilkUDPRecv::recv_datagram_(ssize_t &bytes_received)
{
    MILK_WIRE_HEADER *hdr = reinterpret_cast<MILK_WIRE_HEADER *>(dgram_buffer_);

    bytes_received = ::recvfrom(fds_recv_local_, dgram_buffer_,
                                sizeof(MILK_WIRE_HEADER) + DATAGRAM_CHUNK_SIZE, 0,
                                nullptr, nullptr);
    if(bytes_received < 0)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return SyncEnum::TIMEOUT;
        }
        printf("MilkUDPRecv - recvfrom failure (%s)\n", strerror(errno));
        fflush(stdout);
        return SyncEnum::FAILED;
    }

    if(static_cast<size_t>(bytes_received) < sizeof(MILK_WIRE_HEADER))
    {
        printf("MilkUDPRecv - short datagram (%zd bytes)\n", bytes_received);
        fflush(stdout);
        return SyncEnum::FAILED;
    }

    if(hdr->magic != MILK_NETWORK_MAGIC || hdr->version != MILK_UDP_VERSION)
    {
        printf("MilkUDPRecv - bad magic/version on received datagram\n");
        fflush(stdout);
        return SyncEnum::FAILED;
    }

    return SyncEnum::SUCCESS;
}

// TODO return should be a triggerstatus
// TODO Really we should just take a PROCESSINFO* as argument here.
SyncEnum MilkUDPRecv::sync_barrier()
{
    MILK_WIRE_HEADER *hdr = reinterpret_cast<MILK_WIRE_HEADER *>(dgram_buffer_);
    ssize_t ret = 0;

    // Drop everything until we see the start of a new frame (datagram #0).
    SyncEnum status;
    do
    {
        status = recv_datagram_(ret);
        if(status != SyncEnum::SUCCESS)
        {
            return status;
        }
    }
    while(hdr->cnt_udp != 0);

    last_hdr_ = *hdr;

    if(needs_deferred_init_)
    {
        deferred_init();
    }

    // Reassemble the full frame: data goes to cpu_ptr_, keywords to image_.kw.
    size_t bytes_remaining_data = md_->imdatamemsize;
    char *ptr_next_data = (char *)cpu_ptr_;
    size_t bytes_remaining_keywords = md_->NBkw * sizeof(IMAGE_KEYWORD);
    char *ptr_next_keywords = (char *)image_.kw;

    for(int dgram = 0; dgram < n_dgrams_; ++dgram)
    {
        if(dgram > 0)
        {
            status = recv_datagram_(ret);
            if(status != SyncEnum::SUCCESS)
            {
                return status;
            }
            last_hdr_ = *hdr;
        }

        if(hdr->cnt_udp != dgram)
        {
            printf("MilkUDPRecv - datagram sequence break: expected %d, got %llu\n",
                   dgram, (unsigned long long)hdr->cnt_udp);
            fflush(stdout);
            return SyncEnum::FAILED;
        }

        size_t bytes_left_this_dgram = static_cast<size_t>(ret) - sizeof(
                                            MILK_WIRE_HEADER);
        char *payload = (char *)dgram_buffer_ + sizeof(MILK_WIRE_HEADER);

        if(bytes_remaining_data > 0)
        {
            size_t n = std::min(bytes_remaining_data, bytes_left_this_dgram);
            memcpy(ptr_next_data, payload, n);
            ptr_next_data += n;
            payload += n;
            bytes_remaining_data -= n;
            bytes_left_this_dgram -= n;
        }
        if(bytes_remaining_data == 0 && bytes_left_this_dgram > 0)
        {
            size_t n = std::min(bytes_remaining_keywords, bytes_left_this_dgram);
            memcpy(ptr_next_keywords, payload, n);
            ptr_next_keywords += n;
            bytes_remaining_keywords -= n;
            bytes_left_this_dgram -= n;
        }
    }

    return SyncEnum::SUCCESS;
}


void MilkUDPRecv::move_new_data_to_requested()
{
    // The data is in cpu_buf_, maybe we need a CUDA copy.
    if(req_ >= 0)
    {
        cudaSetDevice(req_);
        cudaMemcpy(ptr_, cpu_ptr_, md_->imdatamemsize, cudaMemcpyHostToDevice);
    }

    // TODO UpdateIm !
    ImageStreamIO_UpdateIm_atime(&image_, &last_hdr_.atime);
}
