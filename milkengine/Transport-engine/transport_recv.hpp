#ifndef TRANSPORT_RECV_HPP
#define TRANSPORT_RECV_HPP

#include <cstdio>
#include "ImageStreamIO/ImageStreamIO.h"

#include "transport_enums.hpp"

class RecvTransport
{
  public:
    RecvTransport(const char *name, InternalStorageEnum req);

    IMAGE_METADATA *md();
    void *ptr();

    virtual void print_type() = 0;

    virtual void sync_barrier() = 0;
    virtual void move_new_data_to_requested() = 0;


    //virtual void begin_frame(int synchro_type);

    // So, what should it do?
    /*
    Provide metadata, data, to a ISIO-like structure - potentially this can be void data

    It should be able to be initialized with a "transport request" --> or that can be done in post against an IMGID

    Synchronization primitives
    - compatible with
        errno_t processinfo_waitoninputstream_init(...)
        errno_t processinfo_waitoninputstream(...)
            which, of note, do pass the IMAGE

    - should be compatible with some or all of the PROCESSINFO_TRIGGERMODE
        -> cnt0, cnt1, cnt2
        -> immediate, delay
        -> semaphore (with or without timeouts)

    - should be aware of a transport_destination_request
    GPU_PINNED_MEM
    GPU_MEM
    CPU_MEM
        - allocate buffers (init) and perform copy or zero-copy into that destination request (at loop time)
        - yield a pointer to that area
    */
  protected:
    void post_init();

    InternalStorageEnum req_;
    char name_[STRINGMAXLEN_IMAGE_NAME] = {};
    IMAGE_METADATA *md_ = nullptr;
    void* ptr_ = nullptr; // Code getting data should get it from this pointer
};


class ImageStreamIORecv: public RecvTransport
{
  public:
  ImageStreamIORecv(const char* name, InternalStorageEnum req);
    ~ImageStreamIORecv();
    void print_type() override;
    void sync_barrier() override;
    void move_new_data_to_requested() override;

  protected:
    void post_init();

  private:
    IMAGE image_ = {};
    void *h_mem_segment_ = nullptr;
    void *d_mem_segment_ = nullptr;

    int sem_trig_id_ = -1;
};




typedef struct
{
    RecvTransport *impl;
} CBaseRecvTransport;

class A_Transport : public RecvTransport
{
  public:
    A_Transport(const char *name, InternalStorageEnum req): RecvTransport(name,
                req) {}
    void print_type() override;
    void sync_barrier() override;
    void move_new_data_to_requested() override;
};

#endif
