#include "transport_emit_isio.hpp"

ImageStreamIOEmit::ImageStreamIOEmit(const char *name,
                                     IMAGE_METADATA *md_request)
    : EmitTransport(name, md_request)
{
    printf("ImageStreamIOEmit::ctor\n");
    fflush(stdout);

    ImageStreamIO_createIm_gpu(&image_, name,
                               md_request->naxis,
                               md_request->size,
                               md_request->datatype,
                               md_request->location, 1,
                               IMAGE_NB_SEMAPHORE,
                               md_request->NBkw,
                               MATH_DATA,
                               0);

    md_ = image_.md;
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
        cudaSetDevice(req_);
        cudaFree(d_mem_segment_);
    }
}
void ImageStreamIOEmit::init_storage_target(InternalStorageEnum req)
{
    // Initialize buffers for data copy if needed
    // todo HAVE_CUDA and ignore GPU_MEMORY completely.
    // todo CUDA includes are carried from ImageStreamIO. So is -DHAVE_CUDA actually.

    req_ = req;

    if(req_ == CPU_MEMORY && md_->location >= 0)
    {
        // Image is GPU, request is to CPU
        cudaSetDevice(md_->location);
        cudaMallocHost(&h_mem_segment_, md_->imdatamemsize);
        ptr_ = h_mem_segment_;
    }
    else if(req_ >= 0 && md_->location != req_)
    {
        // Request is GPU - IMAGE is on CPU or another GPU
        cudaSetDevice(req_); // TODO check the GPU exists ??
        cudaMalloc(&d_mem_segment_, md_->imdatamemsize);
        ptr_ = d_mem_segment_;

        // Enable peer-access between the two GPUs
        if(md_->location >= 0)
        {
            int can_access_peer = 0;
            cudaDeviceCanAccessPeer(&can_access_peer, req_, md_->location);
            if(can_access_peer)
            {
                cudaSetDevice(req_);
                cudaDeviceEnablePeerAccess(md_->location, 0);
                cudaSetDevice(md_->location);
                cudaDeviceEnablePeerAccess(req_, 0);
            }
        }
    }
    else
    {
        // Cases: CPU-CPU, same GPU
        ptr_ = ImageStreamIO_get_image_d_ptr(&image_);
    }
}


void ImageStreamIOEmit::print_type()
{
    printf("This is an ImageStreamIO EMIT transport\n");
}

void ImageStreamIOEmit::move_and_publish_data(struct timespec* atime)
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
    else if(req_ >= 0 && md_->location != req_)
    {
        if(md_->location == -1)
        {
            cudaSetDevice(req_);
            cudaMemcpy(ImageStreamIO_get_image_d_ptr(&image_), d_mem_segment_,
                       md_->imdatamemsize, cudaMemcpyDeviceToHost);
        }
        else
        {
            cudaMemcpyPeer(
                ImageStreamIO_get_image_d_ptr(&image_), md_->location,
                d_mem_segment_, req_,
                md_->imdatamemsize);
        }
    } else {
        // No-op - ptr_ is set to the right place.
    }

    // And update the Image... now atime things become complicated!
    // There'll be a lot of MD to pass around.
    // Possibly the EmitTransport should reference the RecvTransport
    ImageStreamIO_UpdateIm_atime(&image_, atime);
}
