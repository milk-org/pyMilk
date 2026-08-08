#include <algorithm>
#include <string>
#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "transport_emit_milkudp.hpp"

MilkUDPEmit::MilkUDPEmit(const char *name, IMAGE_METADATA *md_request)
    : EmitTransport(name, md_request)
{
    // name is always IP:port, for unicast and multicast
    printf("MilkUDPEmit::ctor\n");
    fflush(stdout);

    std::string s(name);
    auto pos = s.find(':');
    std::string ipv4 = (pos != std::string::npos) ? s.substr(0, pos) : s;
    int port = std::stoi(s.substr(pos + 1));

    ImageStreamIO_createIm_gpu(&image_, ("udpemit_" + std::string(name)).c_str(),
                               md_request->naxis,
                               md_request->size,
                               md_request->datatype,
                               -1, 1,
                               IMAGE_NB_SEMAPHORE,
                               md_request->NBkw,
                               MATH_DATA,
                               0);
    md_ = image_.md;

    sockaddr_connect_ =
    {
        .sin_family = AF_INET,
        .sin_port = ::htons(port),
        .sin_addr = {.s_addr = ::inet_addr(ipv4.c_str())}
    };
    is_multicast_ = IN_MULTICAST(ntohl(sockaddr_connect_.sin_addr.s_addr));

    // Populate stable wire header fields once at construction.
    // Use topic as the ZMQ topic frame (frame 0) so subscribers filtering
    // on a specific stream name get a match; empty topic = match-all.
    stable_hdr_ =
    {
        .magic         = MILK_NETWORK_MAGIC,
        .version       = MILK_UDP_VERSION,
        .naxis         = md_->naxis,
        .datatype      = md_->datatype,
        .nelement      = md_->nelement,
        .imdatamemsize = md_->imdatamemsize,
        .NBkw          = md_->NBkw,
    };


    strncpy(stable_hdr_.name, md_->name, STRINGMAXLEN_IMAGE_NAME - 1);
    memcpy(stable_hdr_.size, md_->size, sizeof(md_->size));

    recreate_socket_();

    // UDP math
    size_t frame_size_total = md_->imdatamemsize + md_->NBkw * sizeof(
                                  IMAGE_KEYWORD); // excluding the MILK_WIRE_HEADER size, but we can got to almost 64k so we have headroom
    n_dgrams_ = frame_size_total / DATAGRAM_CHUNK_SIZE + (frame_size_total %
                DATAGRAM_CHUNK_SIZE != 0 ? 1 : 0);
    dgram_data_size_ = DATAGRAM_CHUNK_SIZE;
    dgram_data_size_last_ = frame_size_total % dgram_data_size_;
}

void MilkUDPEmit::recreate_socket_()
{
    if(fds_emit_local_ != -1)
    {
        ::close(fds_emit_local_);
    }

    // Socket ops - emitting socket
    const int flag = 1;
    fds_emit_local_ = ::socket(AF_INET, SOCK_DGRAM,
                               IPPROTO_UDP); // TODO return fatal
    ::setsockopt(fds_emit_local_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ::setsockopt(fds_emit_local_, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag));
#ifdef SO_ATTACH_REUSEPORT_CBPF
    setsockopt(fds_emit_local_, SOL_SOCKET, SO_ATTACH_REUSEPORT_CBPF, &flag,
               sizeof(flag));
#endif

    if(is_multicast_)
    {
        // Cap how many router hops a multicast datagram may travel.
        int ttl = MILK_UDP_DEFAULT_MCAST_TTL;
        ::setsockopt(fds_emit_local_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    }
}


MilkUDPEmit::~MilkUDPEmit()
{
    if(image_.array.raw)
    {
        ImageStreamIO_closeIm(&image_);
    }
    if(d_mem_segment_)
    {
        cudaSetDevice(req_);
        cudaFree(d_mem_segment_);
    }
    if(fds_emit_local_ != -1)
    {
        ::close(fds_emit_local_);
        fds_emit_local_ = -1;
    }
}
void MilkUDPEmit::init_storage_target(InternalStorageEnum req)
{
    // Initialize buffers for data copy if needed
    // todo HAVE_CUDA and ignore GPU_MEMORY completely.
    // todo CUDA includes are carried from ImageStreamIO. So is -DHAVE_CUDA actually.

    req_ = req;

    if(req_ >= 0)
    {
        cudaSetDevice(req_);
        cudaMalloc(&d_mem_segment_, md_->imdatamemsize);
        ptr_ = d_mem_segment_;
    }
    else
    {
        ptr_ = ImageStreamIO_get_image_d_ptr(&image_);
    }
}


void MilkUDPEmit::print_type()
{
    printf("This is a MILKUDP EMIT transport\n");
}

void MilkUDPEmit::move_and_publish_data(struct timespec *atime)
{
    // Assert new data has been set to the _ptr.
    // And actually this may cause an issue if _ptr is zerocopy because we SHOULD have notified of write earlier.
    image_.md->write = 1;

    void *image_ptr = ImageStreamIO_get_image_d_ptr(&image_);

    if(req_ >= 0)
    {
        cudaSetDevice(req_);
        cudaMemcpy(image_ptr, d_mem_segment_,
                   md_->imdatamemsize, cudaMemcpyDeviceToHost);
    }
    ImageStreamIO_UpdateIm_atime(&image_, atime);

    // Configure wire header
    MILK_WIRE_HEADER hdr = stable_hdr_;
    hdr.cnt0 = md_->cnt0;
    hdr.cnt1 = md_->cnt1;
    hdr.atime = *atime;

    size_t bytes_remaining_data = md_->imdatamemsize;
    char *ptr_next_data = (char *)image_.array.raw;
    size_t bytes_remaining_keywords = md_->NBkw * sizeof(IMAGE_KEYWORD);
    char *ptr_next_keywords = (char *)image_.kw;

    struct iovec iov[3];
    struct msghdr msg = {
        .msg_name = &sockaddr_connect_,
        .msg_namelen = sizeof(sockaddr_connect_),
        .msg_iov = iov,
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0
    };
    size_t bytes_left_this_dgram = dgram_data_size_;

    int iov_send;

    for(int dgram = 0; dgram < n_dgrams_; ++dgram)
    {
        iov_send = 2;
        stable_hdr_.cnt_udp = dgram;
        bytes_left_this_dgram = dgram < n_dgrams_ - 1 ? dgram_data_size_ : dgram_data_size_last_;

        // Use iov vectors to do DGRAM_HDR + WIRE_HEADER + DATA
        iov[0].iov_base = &hdr;
        iov[0].iov_len = sizeof(MILK_WIRE_HEADER);
        iov[1].iov_len = 0; // marker
        if(bytes_remaining_data > 0)
        {
            iov[1].iov_base = ptr_next_data;
            iov[1].iov_len = std::min(bytes_remaining_data, bytes_left_this_dgram);
            bytes_remaining_data -= iov[1].iov_len;
            ptr_next_data += iov[1].iov_len;
            bytes_left_this_dgram -= iov[1].iov_len;
        }
        if(bytes_remaining_data == 0 && bytes_left_this_dgram > 0) {
            iov_send = iov[1].iov_len > 0 ? 3 : 2; // Junction datagram of data and keywords.
            int k = iov_send - 1;
            iov[k].iov_base = ptr_next_keywords;
            iov[k].iov_len = std::min(bytes_remaining_keywords, bytes_left_this_dgram);
            bytes_remaining_keywords -= iov[k].iov_len;
            ptr_next_keywords += iov[k].iov_len;
            bytes_left_this_dgram -= iov[k].iov_len;
        }

        size_t expected_bytes = 0;
        for(int i = 0; i < iov_send; ++i)
        {
            expected_bytes += iov[i].iov_len;
        }

        msg.msg_iovlen = iov_send;

        ssize_t sent_bytes = ::sendmsg(fds_emit_local_, &msg, 0);
        if(sent_bytes < 0 || static_cast<size_t>(sent_bytes) < expected_bytes)
        {
            printf("MilkUDPEmit -- send failure (A) at datagram %d\n", dgram);
            fflush(stdout);
            return;
        }
    }
}
