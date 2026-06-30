#include "transport_recv_isio.hpp"

ImageStreamIORecv::ImageStreamIORecv(const char *name)
    : RecvTransport(name)
{

    printf("ImageStreamIORecv::ctor\n");
    fflush(stdout);

    ImageStreamIO_openIm(&image_, name_); // ERRCHECK
    md_ = image_.md;

    // Initialize synchro over semaphore
    // TODO fallback
    sem_trig_id_ = ImageStreamIO_getsemwaitindex(&image_, -1);
}

void ImageStreamIORecv::init_storage_target(InternalStorageEnum req)
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

ImageStreamIORecv::~ImageStreamIORecv()
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

void ImageStreamIORecv::print_type()
{
    printf("This is an ImageStreamIO RECV transport\n");
}

// TODO return should be a triggerstatus
// TODO And really we should just take a PROCESSINFO* as argument here.
void ImageStreamIORecv::sync_barrier()
{
    // get current time
    struct timespec ts;
    clock_gettime(CLOCK_ISIO, &ts);
    ts.tv_sec += 1;
    // TODO and should notify of the timeout !!
    // TODO or, all transports should be timeout-capable
    while(ImageStreamIO_semtimedwait(&image_, sem_trig_id_, &ts) != 0) {}
}


void ImageStreamIORecv::move_new_data_to_requested()
{
    if(req_ == CPU_MEMORY && md_->location >= 0)
    {
        // Image is GPU, request is to CPU
        cudaSetDevice(md_->location);
        cudaMemcpy(h_mem_segment_, ImageStreamIO_get_image_d_ptr(&image_),
                   md_->imdatamemsize, cudaMemcpyDeviceToHost);
    }
    else if(req_ >= 0 && md_->location != req_)
    {
        if(md_->location == -1)
        {
            cudaSetDevice(req_);
            cudaMemcpy(d_mem_segment_, ImageStreamIO_get_image_d_ptr(&image_),
                       md_->imdatamemsize, cudaMemcpyHostToDevice);
        }
        else
        {
            cudaMemcpyPeer(d_mem_segment_, req_,
                           ImageStreamIO_get_image_d_ptr(&image_), md_->location,
                           md_->imdatamemsize);
        }

    }
    else
    {
        // No-op - ptr_ is set to the right place.
    }
}
