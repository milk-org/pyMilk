#include <stdio.h>

#include "transport_c_bind.hpp"
#include "transport_enums.hpp"

#include <cuda.h>

int main() {
    printf("Hello, world!\n");

    CBaseRecvTransport a = tr_recv_init(TYPE_A, "A_thing", GPU_MEMORY);

    CBaseRecvTransport img_to_cpu = tr_recv_init(TYPE_ISIO, "some_shm", CPU_MEMORY);
    CBaseRecvTransport img_to_gpu = tr_recv_init(TYPE_ISIO, "some_shm", GPU_MEMORY);

    tr_recv_print_type(a);
    tr_recv_print_type(img_to_cpu);
    tr_recv_print_type(img_to_gpu);

    void* ptr1 = tr_recv_ptr_(img_to_cpu);
    void* ptr2 = tr_recv_ptr_(img_to_gpu);

    for (int kk = 0; kk < 100; ++kk) {
        tr_recv_sync_barrier(img_to_cpu);
        tr_recv_move_data_to_requested(img_to_cpu);
        printf("Values [from CPU]: %.0f %.2f\n", ((double*)ptr1)[0], ((double*)ptr1)[1]);
    }

    for (int kk = 0; kk < 100; ++kk) {
        tr_recv_sync_barrier(img_to_gpu);
        tr_recv_move_data_to_requested(img_to_gpu);
        // this should most definitely segfault
        double local_arr[2] = {0, 0};
        cudaSetDevice(0);
        cudaMemcpy(local_arr, ptr2, 2*sizeof(double), cudaMemcpyDeviceToHost);
        printf("Values [from GPU]: %.0f %.2f\n", local_arr[0], local_arr[1]);
    }
    fflush(stdout);

    tr_recv_close(&img_to_cpu);
    tr_recv_close(&img_to_gpu);
}
