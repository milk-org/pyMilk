#ifndef TRANSPORT_EMIT_HPP
#define TRANSPORT_EMIT_HPP

#include <cstdio>
#include "ImageStreamIO/ImageStreamIO.h"

#include "transport_enums.hpp"

class EmitTransport
{
  public:
    EmitTransport(const char *name, InternalStorageEnum req);

    IMAGE_METADATA *md();
    void *ptr();

    virtual void print_type() = 0;

    virtual void move_and_publish_data() = 0;

  protected:
    void post_init();

    InternalStorageEnum req_;
    char name_[STRINGMAXLEN_IMAGE_NAME] = {};
    IMAGE_METADATA *md_ = nullptr;
    void* ptr_ = nullptr; // Code sending data should write it to this pointer
};

typedef struct
{
    EmitTransport *impl;
} CBaseEmitTransport;


class ImageStreamIOEmit: public EmitTransport
{
    public:
    ImageStreamIOEmit(const char* name, InternalStorageEnum req);
    ~ImageStreamIOEmit();
    void print_type() override;
    void move_and_publish_data() override;

    protected:
    void post_init();

  private:
    IMAGE image_ = {};
    void *h_mem_segment_ = nullptr;
    void *d_mem_segment_ = nullptr;

    int sem_trig_id_ = -1;
};

class Z_Transport : public EmitTransport
{
  public:
    Z_Transport(const char *name, InternalStorageEnum req): EmitTransport(name,
                req) {}
    void print_type() override;
    void move_and_publish_data() override;
};

#endif
