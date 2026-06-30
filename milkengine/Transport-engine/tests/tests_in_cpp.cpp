#include "test_compute_unit.hpp"


int main_shm_shm()
{
    InternalStorageEnum x = CPU_MEMORY;
    InternalStorageEnum y = CPU_MEMORY;
    ComputeUnit cunit = ComputeUnit(
                            "yolo_compute_shm_shm",
                            "shm::some_shm", x,
                            "shm::output_shm", y);

    double local_arr[1] = {0};
    for(int ii = 0; ii < 5000; ii++)
    {
        cunit.loop_once();
        if(x == -1)
        {
            //printf("Loop iter %d -- val %.2f\n", ii,
            //       ((double *)cunit.recv_tport_->ptr())[0]);
        }
        else
        {
            cudaSetDevice(0);
            cudaMemcpy(local_arr, cunit.recv_tport_->ptr(), 1 * sizeof(double),
                       cudaMemcpyDeviceToHost);
            //printf("Loop iter %d -- val %.2f\n", ii, local_arr[0]);
        }
        fflush(stdout);
    }
    printf("Done 0\n");
    return 0;
}

int main_shm_zmq()
{
    ComputeUnit cunit = ComputeUnit("yolo_compute_shm_udp",
                            "shm::output_shm", CPU_MEMORY,
                            "zmq::tcp://127.0.0.1:22345", CPU_MEMORY);

    double local_arr[1] = {0};
    for(int ii = 0; ii < 5000; ii++)
    {
        cunit.loop_once();
    }
    printf("Done 1\n");
    return 0;
}

int main_zmq_shm()
{
    ComputeUnit cunit = ComputeUnit("yolo_compute_shm_udp",
                            "zmq::tcp://127.0.0.1:22345", CPU_MEMORY,
                            "shm::output_shm_2", CPU_MEMORY);

    double local_arr[1] = {0};
    for(int ii = 0; ii < 5000; ii++)
    {
        cunit.loop_once();
        printf("Loop iter %d -- val %.2f\n", ii,
               ((double *)cunit.recv_tport_->ptr())[0]);
    }
    printf("Done 2\n");
    return 0;
}

int main(int argc, char* argv[])
{
    int x = atoi(argv[1]);
    if (x == 0)
        main_shm_shm();
    if (x == 1)
        main_shm_zmq();
    if (x == 2)
        main_zmq_shm();
}
