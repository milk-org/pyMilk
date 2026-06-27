#include <stdexcept>
#include <string.h>

#include "transport_emit.hpp"

EmitTransport::EmitTransport(const char *name,
                             InternalStorageEnum req) : req_(req)
{
    strcpy(name_, name);
}
// void EmitTransport::post_init() {}

IMAGE_METADATA *EmitTransport::md()
{
    return md_;
}
void *EmitTransport::ptr()
{
    return ptr_;
}


ImageStreamIOEmit::ImageStreamIOEmit(const char *name, InternalStorageEnum req)
    : EmitTransport(name, req)
{
    post_init();
}

void ImageStreamIOEmit::post_init()
{
    printf("ImageStreamIOEmit::post_init\n");
    fflush(stdout);

    // So this is where we definitely need an IMAGE_ID request...
    ImageStreamIO_openIm(&image_, name_); // ERRCHECK
    md_ = image_.md;

    if(req_ == CPU_MEMORY && md_->location >= 0)
    {
        // Image is GPU, request is to CPU
        cudaSetDevice(md_->location);
        cudaMallocHost(&h_mem_segment_, md_->imdatamemsize);
        ptr_ = h_mem_segment_;
    }
    else if(req_ == GPU_MEMORY && md_->location == -1)
    {
        // Image is CPU, request is to GPU
        // TODO req_ should reflect WHICH GPU
        cudaSetDevice(0);
        cudaMalloc(&d_mem_segment_, md_->imdatamemsize);
        ptr_ = d_mem_segment_;
    }
    else
    {
        ptr_ = ImageStreamIO_get_image_d_ptr(&image_);
    }
}

ImageStreamIOEmit::~ImageStreamIOEmit()
{
    if(image_.array.raw)
    {
        ImageStreamIO_closeIm(&image_);
    }
    if(h_mem_segment_)
    {
        cudaFreeHost(h_mem_segment_);
    }
    if(d_mem_segment_)
    {
        cudaFree(d_mem_segment_);
    }
}

void ImageStreamIOEmit::print_type()
{
    printf("This is an ImageStreamIO RECV transport\n");
}

void ImageStreamIOEmit::move_and_publish_data()
{
    // Assert new data has been set to the _ptr.
    // And actually this may cause an issue if _ptr is zerocopy because we SHOULD have notified of write earlier.
    image_.md->write = 1;
    if(req_ == CPU_MEMORY && md_->location >= 0)
    {
        cudaSetDevice(md_->location);
        cudaMemcpy(ImageStreamIO_get_image_d_ptr(&image_), h_mem_segment_,
                   md_->imdatamemsize, cudaMemcpyHostToDevice);
    }
    if(req_ == GPU_MEMORY && md_->location == -1)
    {
        cudaSetDevice(0); // TODO which GPU
        cudaMemcpy(ImageStreamIO_get_image_d_ptr(&image_), d_mem_segment_,
                   md_->imdatamemsize, cudaMemcpyDeviceToHost);
    }
    // Otherwise a no-op.

    // And update the Image... now atime things become complicated!
    // There'll be a lot of MD to pass around.
    // Possibly the EmitTransport should reference the RecvTransport
    ImageStreamIO_UpdateIm(&image_);
}

void Z_Transport::print_type()
{
    printf("A type transport\n");
}

void Z_Transport::move_and_publish_data()
{
    throw std::runtime_error("A_Transport::move_new_data_to_requested not implemented");
}
