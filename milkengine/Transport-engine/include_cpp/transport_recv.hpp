#ifndef TRANSPORT_RECV_HPP
#define TRANSPORT_RECV_HPP

//#include <cstdio>
#include <cstring>
#include "ImageStreamIO/ImageStreamIO.h"

#include "transport_enums.h"

class RecvTransport
{
  public:
    RecvTransport(const char *name) {strcpy(name_, name);};

    // Public interface - read-only
    IMAGE_METADATA *md() const { return md_; }
    void *ptr() const { return ptr_; }
    InternalStorageEnum req() const { return req_; }

    // Pure virtual
    virtual void init_storage_target(InternalStorageEnum req) = 0;
    virtual void print_type() = 0;
    virtual SyncEnum sync_barrier() = 0;
    virtual void move_new_data_to_requested() = 0;


  protected:

    InternalStorageEnum req_ = UNINITIALIZED;
    char name_[STRINGMAXLEN_IMAGE_NAME] = {};
    IMAGE_METADATA *md_ = nullptr;
    void* ptr_ = nullptr; // Code getting data should get it from this pointer
};

typedef struct
{
    RecvTransport *impl;
} CBaseRecvTransport;

#endif
