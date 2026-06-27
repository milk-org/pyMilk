#include <cstdio>
#include <stdexcept>
#include <string.h>

#include "transport_recv.hpp"

RecvTransport::RecvTransport(const char* name, InternalStorageEnum req) : req_(req) {
    strcpy(name_, name);
}
void RecvTransport::post_init() {}

IMAGE_METADATA* RecvTransport::md() {
    return md_;
}
void* RecvTransport::ptr() {
    return ptr_;
}

ImageStreamIORecv::ImageStreamIORecv(const char* name, InternalStorageEnum req)
    : RecvTransport(name, req)
{
    post_init();
}

void ImageStreamIORecv::post_init() {

    printf("ImageStreamIORecv::post_init\n"); fflush(stdout);

    ImageStreamIO_openIm(&image_, name_); // ERRCHECK
    md_ = image_.md;

    // Initialize buffers for data copy if needed
    // todo HAVE_CUDA and ignore GPU_MEMORY completely.
    // CUDA includes are carried from ImageStreamIO. So is -DHAVE_CUDA actually.
    if (req_ == CPU_MEMORY && md_->location >= 0) {
        // Image is GPU, request is to CPU
        cudaSetDevice(md_->location);
        cudaMallocHost(&h_mem_segment_, md_->imdatamemsize);
        ptr_ = h_mem_segment_;
    } else if (req_ == GPU_MEMORY && md_->location == -1) {
        // Image is CPU, request is to GPU
        // TODO req_ should reflect WHICH GPU
        cudaSetDevice(0);
        cudaMalloc(&d_mem_segment_, md_->imdatamemsize);
        ptr_ = d_mem_segment_;
    } else {
        ptr_ = ImageStreamIO_get_image_d_ptr(&image_);
    }

    // Initialize synchro over semaphore
    // TODO fallback
    sem_trig_id_ = ImageStreamIO_getsemwaitindex(&image_, -1);
}

ImageStreamIORecv::~ImageStreamIORecv() {
    if (image_.array.raw)
        ImageStreamIO_closeIm(&image_);
    if (h_mem_segment_)
        cudaFreeHost(h_mem_segment_);
    if (d_mem_segment_)
        cudaFree(d_mem_segment_);
}

void ImageStreamIORecv::print_type()
{
    printf("This is an ImageStreamIO RECV transport\n");
}

// TODO return should be a triggerstatus
// TODO And really we should just take a PROCESSINFO* as argument here.
void ImageStreamIORecv::sync_barrier() {
    // get current time
    struct timespec ts;
    clock_gettime(CLOCK_ISIO, &ts);
    ts.tv_sec += 1;
    while(ImageStreamIO_semtimedwait(&image_, sem_trig_id_, &ts) != 0) {}
}


/*
STREAM NAME

shm://some_shm
udp://
tcp://
dpdk://
*/

void ImageStreamIORecv::move_new_data_to_requested() {
    if (req_ == CPU_MEMORY && md_->location >= 0) {
        cudaSetDevice(md_->location);
        cudaMemcpy(h_mem_segment_, ImageStreamIO_get_image_d_ptr(&image_), md_->imdatamemsize, cudaMemcpyDeviceToHost);
    }
    if (req_ == GPU_MEMORY && md_->location == -1) {
         // TODO which GPU
        cudaSetDevice(md_->location);
        cudaMemcpy(d_mem_segment_, ImageStreamIO_get_image_d_ptr(&image_), md_->imdatamemsize, cudaMemcpyHostToDevice);
    }
    // TODO 2 GPUs but different GPUs.
    // Otherwise a no-op.
}

void A_Transport::print_type()
{
    printf("A type transport\n");
}

void A_Transport::sync_barrier()
{
    throw std::runtime_error("A_Transport::sync_barrier not implemented");
}

void A_Transport::move_new_data_to_requested()
{
    throw std::runtime_error("A_Transport::move_new_data_to_requested not implemented");
}
