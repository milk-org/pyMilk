#ifndef TRANSPORT_RECV_ISIO_HPP
#define TRANSPORT_RECV_ISIO_HPP

#include "transport_recv.hpp"

class ImageStreamIORecv: public RecvTransport
{
  public:
  ImageStreamIORecv(const char* name);
    ~ImageStreamIORecv();
    void init_storage_target(InternalStorageEnum req) override;
    void print_type() override;
    SyncEnum sync_barrier() override;
    void move_new_data_to_requested() override;

  protected:
    void post_init();

  private:
    IMAGE image_ = {};
    void *h_mem_segment_ = nullptr;
    void *d_mem_segment_ = nullptr;

    int sem_trig_id_ = -1;
};

#endif
