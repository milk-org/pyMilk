#include <stdio.h>

#include "transport_c_bind.h"
#include "transport_enums.h"

#include <cuda.h>

#include <errno.h>

extern cudaError_t check(cudaError_t result, char const *const func, const char *const file, int const line);
#define checkCudaErrors(val) check((val), #val, __FILE__, __LINE__)

int main() {
    printf("Hello, world!\n");

    CBaseRecvTransport img_to_cpu = tr_recv_init(RECVTYPE_ISIO, "some_shm");
    tr_recv_init_storage_target(img_to_cpu, CPU_MEMORY);

    CBaseRecvTransport img_to_gpu = tr_recv_init(RECVTYPE_ISIO, "some_shm");
    tr_recv_init_storage_target(img_to_gpu, GPU0_MEMORY);

    tr_recv_print_type(img_to_cpu);
    tr_recv_print_type(img_to_gpu);

    void* ptr1 = tr_recv_ptr_(img_to_cpu);
    void* ptr2 = tr_recv_ptr_(img_to_gpu);

    for (int kk = 0; kk < 20; ++kk) {
        tr_recv_sync_barrier(img_to_cpu);
        tr_recv_move_data_to_requested(img_to_cpu);
        printf("Values [from CPU]: %.0f %.2f\n", ((double*)ptr1)[0], ((double*)ptr1)[1]);
    }

    for (int kk = 0; kk < 20; ++kk) {
        tr_recv_sync_barrier(img_to_gpu);
        tr_recv_move_data_to_requested(img_to_gpu);
        // this should most definitely segfault
        double local_arr[2] = {0, 0};
        if(checkCudaErrors(cudaSetDevice(0))) break;
        cudaMemcpy(local_arr, ptr2, 2*sizeof(double), cudaMemcpyDeviceToHost);
        printf("Values [from GPU]: %.0f %.2f\n", local_arr[0], local_arr[1]);
    }
    fflush(stdout);

    tr_recv_close(&img_to_cpu);
    tr_recv_close(&img_to_gpu);
}
