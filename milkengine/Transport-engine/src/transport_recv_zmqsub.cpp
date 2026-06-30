#include <algorithm>
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

    milk_zmq_ctx_ = milk_zmq_sub_init(nullptr, 0, endpoint.c_str(), topic.c_str());
}

void ZmqRecv::init_storage_target(InternalStorageEnum req)
{
    req_ = req; // Nothing we can do just yet! Must do deferred init instead.
}

ZmqRecv::~ZmqRecv()
{
    // Dealloc zmq, dealloc GPU buffer
    milk_zmq_teardown(&milk_zmq_ctx_);
    if(d_mem_segment_)
    {
        cudaSetDevice(req_);
        cudaFree(d_mem_segment_);
        d_mem_segment_ = nullptr;
    }
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
    MILK_ZMQ_WIRE_HEADER hdr = milk_zmq_ctx_.last_hdr;

    // This is always a CPU image
    std::string safe_name(name_);
    safe_name.erase(std::remove_if(safe_name.begin(), safe_name.end(),
                                   [](char c){ return c == '/' || c == ':'; }),
                    safe_name.end());

    ImageStreamIO_createIm_gpu(&image_, ("zmqsub_" + safe_name).c_str(),
                               hdr.naxis, hdr.size, hdr.datatype,
                               -1, // CPU memory
                               1, // int shared
                               IMAGE_NB_SEMAPHORE, // int NBsem
                               hdr.NBkw,
                               MATH_DATA, // uint64_t imagetype
                               0 // uit32_t CBsize
                              );
    md_ = image_.md;

    milk_zmq_ctx_.ptr = image_.array.raw; // always CPU
    milk_zmq_ctx_.data_size = hdr.imdatamemsize;

    if(req_ >= 0)
    {
        cudaSetDevice(req_);
        cudaMalloc(&d_mem_segment_, hdr.imdatamemsize);
        ptr_ = d_mem_segment_;
    } else {
        ptr_ = ImageStreamIO_get_image_d_ptr(&image_);
    }

    needs_deferred_init_ = false;
}

// TODO return should be a triggerstatus
// TODO Really we should just take a PROCESSINFO* as argument here.
void ZmqRecv::sync_barrier()
{
    // Wait for message, perform deferred init
    zmq_msg_t msg_topic, msg_hdr, msg_data;
    zmq_msg_init(&msg_topic);
    zmq_msg_init(&msg_hdr);
    zmq_msg_init(&msg_data);

    int rc = 0;

    /* Frame 0: topic (image name) */
    if(zmq_msg_recv(&msg_topic, milk_zmq_ctx_.socket, 0) < 0)
    {
        rc = -1;
        goto cleanup;
    }

    /* Frame 1: wire header */
    if(zmq_msg_recv(&msg_hdr, milk_zmq_ctx_.socket, 0) < 0)
    {
        rc = -1;
        goto cleanup;
    }
    if(zmq_msg_size(&msg_hdr) != sizeof(MILK_ZMQ_WIRE_HEADER))
    {
        rc = -2;
        goto cleanup;
    }

    {
        MILK_ZMQ_WIRE_HEADER *hdr = (MILK_ZMQ_WIRE_HEADER *)zmq_msg_data(&msg_hdr);

        if(hdr->magic != MILK_ZMQ_MAGIC || hdr->version != MILK_ZMQ_VERSION)
        {
            rc = -2;
            goto cleanup;
        }

        /* Frame 2: pixel data */
        if(zmq_msg_recv(&msg_data, milk_zmq_ctx_.socket, 0) < 0)
        {
            rc = -1;
            goto cleanup;
        }

        if(zmq_msg_size(&msg_data) != (size_t)hdr->imdatamemsize)
        {
            rc = -3;
            goto cleanup;
        }

        /* Stash last header so caller can inspect metadata */
        milk_zmq_ctx_.last_hdr = *hdr;

        if(needs_deferred_init_)
        {
            deferred_init();
        }

        /* Copy pixel data into the caller-supplied buffer */
        if(milk_zmq_ctx_.ptr)
        {
            if(milk_zmq_ctx_.data_size < hdr->imdatamemsize)
            {
                rc = -3;
                goto cleanup;
            }
            memcpy(milk_zmq_ctx_.ptr, zmq_msg_data(&msg_data), (size_t)hdr->imdatamemsize);
        }
    }

cleanup:
    zmq_msg_close(&msg_topic);
    zmq_msg_close(&msg_hdr);
    zmq_msg_close(&msg_data);
    return;
}


void ZmqRecv::move_new_data_to_requested()
{
}
