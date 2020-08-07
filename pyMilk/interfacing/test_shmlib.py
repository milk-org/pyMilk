import numpy as np

from .isio_shmlib import SHM


def test_data_conservation():

    all_shapes = [(3, ), (2, 3), (2, 3, 4), (3, 4, 5), (5, 6, 7)]

    for s in all_shapes:
        for sym in range(8):
            for tri in range(4):
                data = np.random.randn(*s)

                shm_write = SHM("pyMilk_autotest", data, symcode=sym,
                                triDim=tri)
                shm_read = SHM("pyMilk_autotest", symcode=sym, triDim=tri)

                dd = shm_read.get_data()
                assert (shm_write.shape == shm_read.shape
                        ), f"{shm_write.shape}, {shm_read.shape}, {sym}, {tri}"
                assert (shm_write.shape_c == shm_read.shape_c
                        ), f"{shm_write.shape_c}, {shm_read.shape_c}"
                assert np.all(np.abs(dd - data) < 1e-7), f"{s}, {sym}, {tri}"

                shm_read.close()
                shm_write.IMAGE.destroy()
