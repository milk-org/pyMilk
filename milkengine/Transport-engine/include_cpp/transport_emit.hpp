#ifndef TRANSPORT_EMIT_HPP
#define TRANSPORT_EMIT_HPP

#include <cstring>

#include "ImageStreamIO/ImageStreamIO.h"

#include "transport_enums.h"

class EmitTransport
{
  public:
    EmitTransport(const char *name, IMAGE_METADATA* md_request) {strcpy(name_, name);};

    // Public interface - read-only
    IMAGE_METADATA *md() const { return md_; }
    void *ptr() const { return ptr_; }
    InternalStorageEnum req() const { return req_; }

    // Pure virtual
    virtual void init_storage_target(InternalStorageEnum req) = 0;
    virtual void print_type() = 0;
    virtual void move_and_publish_data(struct timespec* atime) = 0;

    bool _initialized = false;

    protected:

    char name_[STRINGMAXLEN_IMAGE_NAME] = {};
    InternalStorageEnum req_ = UNINITIALIZED;
    IMAGE_METADATA *md_ = nullptr;
    void *ptr_ = nullptr; // Code sending data should write it to this pointer
};

typedef struct
{
    EmitTransport *impl;
} CBaseEmitTransport;

#endif
