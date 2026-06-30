#include <algorithm>
#include <string>
#include <cstring>

#include <zmq.h>

#include "transport_emit_zmqpub.hpp"

ZmqEmit::ZmqEmit(const char *name, IMAGE_METADATA *md_request)
    : EmitTransport(name, md_request)
{
    printf("ZmqEmit::ctor\n");
    fflush(stdout);

    std::string s(name);
    auto pos = s.find('#');
    std::string endpoint = (pos != std::string::npos) ? s.substr(0, pos) : s;
    std::string topic    = (pos != std::string::npos) ? s.substr(pos + 1) : "";

    // name_ probably contains slashes and colons...
    std::string safe_name(name_);
    safe_name.erase(std::remove_if(safe_name.begin(), safe_name.end(),
                                   [](char c){ return c == '/' || c == ':'; }),
                    safe_name.end());

    ImageStreamIO_createIm_gpu(&image_, ("zmqpub_" + safe_name).c_str(),
                               md_request->naxis,
                               md_request->size,
                               md_request->datatype,
                               -1, 1,
                               IMAGE_NB_SEMAPHORE,
                               md_request->NBkw,
                               MATH_DATA,
                               0);
    md_ = image_.md;

    milk_zmq_ctx_ = milk_zmq_pub_init(image_.array.raw, md_->imdatamemsize,
                                      endpoint.c_str(), topic.c_str());

    // Populate stable wire header fields once at construction.
    // Use topic as the ZMQ topic frame (frame 0) so subscribers filtering
    // on a specific stream name get a match; empty topic = match-all.
    milk_zmq_ctx_.wire_hdr.magic         = MILK_ZMQ_MAGIC;
    milk_zmq_ctx_.wire_hdr.version       = MILK_ZMQ_VERSION;
    strncpy(milk_zmq_ctx_.wire_hdr.name, topic.empty() ? md_->name : topic.c_str(),
            STRINGMAXLEN_IMAGE_NAME - 1);
    milk_zmq_ctx_.wire_hdr.naxis         = md_->naxis;
    memcpy(milk_zmq_ctx_.wire_hdr.size, md_->size, sizeof(md_->size));
    milk_zmq_ctx_.wire_hdr.datatype      = md_->datatype;
    milk_zmq_ctx_.wire_hdr.nelement      = md_->nelement;
    milk_zmq_ctx_.wire_hdr.imdatamemsize = md_->imdatamemsize;
    milk_zmq_ctx_.wire_hdr.NBkw          = md_->NBkw;
}


ZmqEmit::~ZmqEmit()
{
    milk_zmq_teardown(&milk_zmq_ctx_);
    if(image_.array.raw)
    {
        ImageStreamIO_closeIm(&image_);
    }
    if(d_mem_segment_)
    {
        cudaSetDevice(req_);
        cudaFree(d_mem_segment_);
    }
}
void ZmqEmit::init_storage_target(InternalStorageEnum req)
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


void ZmqEmit::print_type()
{
    printf("This is a ZMQ EMIT transport\n");
}

void ZmqEmit::move_and_publish_data(struct timespec *atime)
{
    // Assert new data has been set to the _ptr.
    // And actually this may cause an issue if _ptr is zerocopy because we SHOULD have notified of write earlier.
    image_.md->write = 1;

    if(req_ >= 0)
    {
        cudaSetDevice(req_);
        cudaMemcpy(ImageStreamIO_get_image_d_ptr(&image_), d_mem_segment_,
                   md_->imdatamemsize, cudaMemcpyDeviceToHost);
    }
    ImageStreamIO_UpdateIm_atime(&image_, atime);

    // Now for ZMQ wire send !
    if(!milk_zmq_ctx_.ptr)
    {
        return; // -1
    }

    /* Update per-frame fields only; stable fields set in ctor */
    milk_zmq_ctx_.wire_hdr.cnt0          = md_->cnt0;
    milk_zmq_ctx_.wire_hdr.cnt1          = md_->cnt1;
    milk_zmq_ctx_.wire_hdr.atime         = *atime;

    /* Frame 0: topic = image name from wire header */
    if(zmq_send(milk_zmq_ctx_.socket, milk_zmq_ctx_.wire_hdr.name,
                strlen(milk_zmq_ctx_.wire_hdr.name), ZMQ_SNDMORE) < 0)
    {
        printf("ZMQEmit early return 1\n"); fflush(stdout);
        return; // -1
    }

    /* Frame 1: wire header */
    if(zmq_send(milk_zmq_ctx_.socket, &milk_zmq_ctx_.wire_hdr, sizeof(milk_zmq_ctx_.wire_hdr),
                ZMQ_SNDMORE) < 0)
    {
        printf("ZMQEmit early return 2\n"); fflush(stdout);
        return; // -1
    }

    /* Frame 2: pixel data */
    if(zmq_send(milk_zmq_ctx_.socket, milk_zmq_ctx_.ptr, (size_t)milk_zmq_ctx_.data_size, 0) < 0)
    {
        printf("ZMQEmit early return 3\n"); fflush(stdout);
        return; // -1
    }

    return; // 0
}
