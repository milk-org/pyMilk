#ifndef TRANSPORT_EMIT_ISIO_HPP
#define TRANSPORT_EMIT_ISIO_HPP

#include "transport_emit.hpp"

class ImageStreamIOEmit: public EmitTransport
{
  public:
    ImageStreamIOEmit(const char *name, IMAGE_METADATA* md_request);
    ~ImageStreamIOEmit();
    void init_storage_target(InternalStorageEnum req) override;
    void print_type() override;
    void move_and_publish_data(struct timespec* atime) override;

  //protected:

  private:
    IMAGE image_ = {};
    void *h_mem_segment_ = nullptr;
    void *d_mem_segment_ = nullptr;

    int sem_trig_id_ = -1;
};

#endif // #ifndef TRANSPORT_EMIT_ISIO_HPP
