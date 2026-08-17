#include <algorithm>
#include <chrono>
#include <string>
#include <cstring>

#include <zmq.h>

#include "ImageStreamIO/ImageStreamIO.h"
#include "transport_recv_zmqsub.hpp"

ZmqRecv::ZmqRecv(const char *name)
    : RecvTransport(name)
{
    // name: either <endpoint> and topic = ""
    // or <endpoint>#<topic>
    std::string s(name);
    auto pos = s.find('#');
    std::string endpoint = (pos != std::string::npos) ? s.substr(0, pos) : s;
    std::string topic    = (pos != std::string::npos) ? s.substr(pos + 1) : "";

    milk_zmq_ctx_ = milk_zmq_sub_init(nullptr, 0, endpoint.c_str(), topic.c_str(),
                                      std::chrono::duration_cast<std::chrono::milliseconds>(DEFAULT_TIMEOUT).count());
}

void ZmqRecv::init_storage_target(InternalStorageEnum req)
{
    req_ = req; // Nothing we can do just yet! Must do deferred init instead.
}

ZmqRecv::~ZmqRecv()
{
    // Dealloc zmq, dealloc GPU buffer
    milk_zmq_teardown(&milk_zmq_ctx_);
    // Relay CPU image from the transport
    if(image_.used == 1)
    {
        ImageStreamIO_destroyIm(&image_);
    }
}

void ZmqRecv::print_type()
{
    printf("This is an ZMQ SUB RECV transport\n");
}

void ZmqRecv::deferred_init()
{
    MILK_WIRE_HEADER hdr = milk_zmq_ctx_.last_hdr;

    // This is always a CPU image
    std::string safe_name(name_);
    safe_name.erase(std::remove_if(safe_name.begin(), safe_name.end(),
                                   [](char c)
    {
        return c == '/' || c == ':';
    }),
    safe_name.end());

    ImageStreamIO_createIm_gpu(&image_, ("zmqsub_" + safe_name).c_str(),
                               hdr.naxis, hdr.size, hdr.datatype,
                               req_, // CPU or GPU memory
                               1, // int shared
                               IMAGE_NB_SEMAPHORE, // int NBsem
                               hdr.NBkw,
                               MATH_DATA, // uint64_t imagetype
                               0 // uit32_t CBsize
                              );
    md_ = image_.md;

    milk_zmq_ctx_.ptr = ImageStreamIO_get_image_d_ptr(&image_);
    milk_zmq_ctx_.data_size = hdr.imdatamemsize;

    ptr_ = milk_zmq_ctx_.ptr;

    needs_deferred_init_ = false;
}

// TODO return should be a triggerstatus
// TODO Really we should just take a PROCESSINFO* as argument here.
SyncEnum ZmqRecv::sync_barrier()
{
    MILK_WIRE_HEADER *hdr;
    SyncEnum ret = SyncEnum::SUCCESS;

    // Wait for message, perform deferred init
    zmq_msg_t msg_topic, msg_hdr, msg_data;
    zmq_msg_init(&msg_topic);
    zmq_msg_init(&msg_hdr);
    zmq_msg_init(&msg_data);

    /* Frame 0: topic (image name) */
    if(zmq_msg_recv(&msg_topic, milk_zmq_ctx_.socket, 0) < 0)
    {
        ret = zmq_errno() == EAGAIN ? SyncEnum::TIMEOUT : SyncEnum::FAILED;
        goto cleanup;
    }

    /* Frame 1: wire header */
    if(zmq_msg_recv(&msg_hdr, milk_zmq_ctx_.socket, 0) < 0)
    {
        ret = zmq_errno() == EAGAIN ? SyncEnum::TIMEOUT : SyncEnum::FAILED;
        goto cleanup;
    }
    if(zmq_msg_size(&msg_hdr) != sizeof(MILK_WIRE_HEADER))
    {
        ret = zmq_errno() == EAGAIN ? SyncEnum::TIMEOUT : SyncEnum::FAILED;
        goto cleanup;
    }

    hdr = (MILK_WIRE_HEADER *)zmq_msg_data(&msg_hdr);

    if(hdr->magic != MILK_NETWORK_MAGIC || hdr->version != MILK_ZMQ_VERSION)
    {
        ret = SyncEnum::FAILED;
        goto cleanup;
    }

    /* Frame 2: pixel data */
    if(zmq_msg_recv(&msg_data, milk_zmq_ctx_.socket, 0) < 0)
    {
        ret = zmq_errno() == EAGAIN ? SyncEnum::TIMEOUT : SyncEnum::FAILED;
        goto cleanup;
    }

    if(zmq_msg_size(&msg_data) != (size_t)hdr->imdatamemsize)
    {
        ret = zmq_errno() == EAGAIN ? SyncEnum::TIMEOUT : SyncEnum::FAILED;
        goto cleanup;
    }

    /* Stash last header so caller can inspect metadata */
    milk_zmq_ctx_.last_hdr = *hdr;

    if(needs_deferred_init_)
    {
        deferred_init();
    }

    /* Copy pixel data into the caller-supplied buffer */
    // TODO must also update the receiving image metadata !
    // TODO put the receiving image directly on the correct target

    if(ptr_)
    {
        md_->write = 1; // Notify image write
        if(milk_zmq_ctx_.data_size < hdr->imdatamemsize)
        {
            ret = SyncEnum::FAILED;
            goto cleanup;
        }

        if(req_ == CPU_MEMORY)
        {
            memcpy(ptr_, zmq_msg_data(&msg_data), (size_t)hdr->imdatamemsize);
        }
        else
        {
            cudaSetDevice(req_);
            cudaMemcpy(ptr_, zmq_msg_data(&msg_data), (size_t)hdr->imdatamemsize,
                       cudaMemcpyHostToDevice);
        }
        ImageStreamIO_UpdateIm_atime(&image_, &hdr->atime);
    }

cleanup:
    zmq_msg_close(&msg_topic);
    zmq_msg_close(&msg_hdr);
    zmq_msg_close(&msg_data);
    return ret;
}


void ZmqRecv::move_new_data_to_requested()
{
    // The data is in the correct location already from sync_barrier, nothing extra to do.
}
