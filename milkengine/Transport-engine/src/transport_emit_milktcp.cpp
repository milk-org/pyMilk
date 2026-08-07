#include <algorithm>
#include <string>
#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "transport_emit_milktcp.hpp"

MilkTCPEmit::MilkTCPEmit(const char *name, IMAGE_METADATA *md_request)
    : EmitTransport(name, md_request)
{
    printf("MilkTCPEmit::ctor\n");
    fflush(stdout);

    std::string s(name);
    auto pos = s.find(':');
    std::string ipv4 = (pos != std::string::npos) ? s.substr(0, pos) : s;
    int port = std::stoi(s.substr(pos + 1));

    ImageStreamIO_createIm_gpu(&image_, ("tcpemit_" + std::string(name)).c_str(),
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

    // Populate stable wire header fields once at construction.
    // Use topic as the ZMQ topic frame (frame 0) so subscribers filtering
    // on a specific stream name get a match; empty topic = match-all.
    stable_hdr_ =
    {
        .magic         = MILK_NETWORK_MAGIC,
        .version       = MILK_TCP_VERSION,
        .naxis         = md_->naxis,
        .datatype      = md_->datatype,
        .nelement      = md_->nelement,
        .imdatamemsize = md_->imdatamemsize,
        .NBkw          = md_->NBkw,
    };
    strncpy(stable_hdr_.name, md_->name, STRINGMAXLEN_IMAGE_NAME - 1);
    memcpy(stable_hdr_.size, md_->size, sizeof(md_->size));
}

void MilkTCPEmit::recreate_socket_()
{
    if(fds_emit_local_ != -1)
    {
        ::close(fds_emit_local_);
    }

    // Socket ops - emitting socket
    const int flag = 1;
    fds_emit_local_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // TODO return fatal
    ::setsockopt(fds_emit_local_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    //const int timeout_ms = 300; // fail sends after 2s of unacknowledged data
    //::setsockopt(fds_emit_local_, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    // Socket ops - addr struct to connect to

    //int idle = 10;
    int intvl = 2;
    int cnt = 1;

    //setsockopt(fds_emit_local_, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(fds_emit_local_, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    setsockopt(fds_emit_local_, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
}


MilkTCPEmit::~MilkTCPEmit()
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
void MilkTCPEmit::init_storage_target(InternalStorageEnum req)
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

    // If the zmqpub clone image is on GPU (which is kinda stupid when there's no DMA capability anyway),
    // we're gonna need an additional ZMQ host-side buffer
}


void MilkTCPEmit::print_type()
{
    printf("This is a MILKTCP EMIT transport\n");
}

void MilkTCPEmit::move_and_publish_data(struct timespec *atime)
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

    if(needs_tcp_reconnect_)
    {
        printf("MilkTCPEmit -- attempting (re)connect\n"); fflush(stdout);
        recreate_socket_(); // Connect should be in there ! And return should reflect it.
        int ret = connect(fds_emit_local_, (struct sockaddr *) &sockaddr_connect_,
                          sizeof(sockaddr_connect_));
        if(ret < 0)
        {
            printf("MilkTCPEmit -- failed connect %d %d ret\n", ret, errno);
            fflush(stdout);
            return;
        }
        printf("MilkTCPEmit -- successful connect\n");
        fflush(stdout);
        needs_tcp_reconnect_ = false;
    }

    // Configure wire header
    MILK_WIRE_HEADER hdr = stable_hdr_;
    hdr.cnt0 = md_->cnt0;
    hdr.cnt1 = md_->cnt1;
    hdr.atime = *atime;

    // Catch a disconnect by calling an recv // -1 + EWOULDBLOCK if CONN is alive
    // -1 + ECONNRESET when conn just died
    // 0 when conn is not established
    uint8_t c;
    int rc = recv(fds_emit_local_, &c, 1, MSG_DONTWAIT);
    if(!(rc == -1 && errno == EWOULDBLOCK))
    {
        needs_tcp_reconnect_ = true;
    }

    if(::send(fds_emit_local_, &hdr, sizeof(hdr), MSG_MORE) < sizeof(hdr))
    {
        printf("MilkTCPEmit -- send failure A\n");
        fflush(stdout);
        needs_tcp_reconnect_ = true;
        return;
    }
    // Send from the image, which is always CPU here
    if(::send(fds_emit_local_, image_ptr, md_->imdatamemsize,
              md_->NBkw > 0 ? MSG_MORE : 0) < md_->imdatamemsize)
    {
        printf("MilkTCPEmit -- send failure B\n");
        fflush(stdout);
        needs_tcp_reconnect_ = true;
        return;
    }
    if(md_->NBkw > 0)
    {
        if(::send(fds_emit_local_, image_.kw, md_->NBkw * sizeof(IMAGE_KEYWORD),
                  0) < md_->NBkw * sizeof(IMAGE_KEYWORD))
        {
            printf("MilkTCPEmit -- send failure C\n");
            fflush(stdout);
            needs_tcp_reconnect_ = true;
            return;
        }
    }
}
