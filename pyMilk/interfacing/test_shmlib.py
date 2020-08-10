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

                data_backw = np.random.randn(*s)

                shm_read.set_data(data_backw)
                dd2 = shm_write.get_data(check=False)
                assert np.all(
                        np.abs(dd2 - data_backw) < 1e-7), f"{s}, {sym}, {tri}"

                shm_read.close()
                shm_write.IMAGE.destroy()


def test_keyword():

    data = np.random.randn(50, 20)
    shm_write = SHM("pyMilk_autotest", data, nbkw=3)
    kww = {'yo': ('lo', 'un commentaire'), 'toto': 17, 'arthur': 3.1415}
    shm_write.set_keywords(kww)

    shm_read = SHM("pyMilk_autotest")
    kwr = shm_read.get_keywords()
    assert kwr == {'yo': 'lo', 'toto': 17, 'arthur': 3.1415}

    kww = {'yo': 2.718, 'roger': 'trois'}
    shm_read.set_keywords(kww)  # Should erase the comments
    kwr = shm_write.get_keywords()
    assert kwr == kww

    shm_write.update_keyword('roger', '12')
    shm_write.update_keyword('yo', 13, 'comment')
    assert shm_read.get_keywords(True)['roger'] == ('12', '')
    assert shm_read.get_keywords(True)['yo'] == (13, 'comment')

    shm_write.IMAGE.destroy()
