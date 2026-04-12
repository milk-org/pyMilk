import pytest

import numpy as np

from pyMilk.interfacing.shm import SHM

from ..conftestaux.async_shm_fixtures import (threaded_shm_data_pingpong,
                                              threaded_shm_poster,
                                              threaded_shm_poster_recreator)
'''
CAUTION

All tests in this file are inherently race-conditionny

Some simple things, such as e.g. resizing your shell while the tests (with
the underlying multiprocessed fixture) are running may very well cause them to
fail.
'''


@pytest.mark.parametrize('threaded_shm_data_pingpong',
                         [('x', (2, 3, 4), np.float32, 0.01)], indirect=True)
def test_shm_threaded_pingpong(threaded_shm_data_pingpong):
    #thread = threaded_shm_data_pingpong

    s_to_p, s_from_p, process = threaded_shm_data_pingpong

    dat = np.zeros((2, 3, 4), np.float32)
    import time
    time.sleep(1.0)

    for _ in range(23):
        s_to_p.set_data(dat)
        #print(f'M: getting {_=}')
        dat2 = s_from_p.get_data(True, checkSemAndFlush=False)
        assert np.all(dat2 == 1)


@pytest.mark.parametrize('threaded_shm_poster',
                         [('x', (2, 3, 4), np.float32, 0.01)], indirect=True)
def test_shm_threaded_poster(threaded_shm_poster):
    s_to_p, s_from_p, process = threaded_shm_poster

    for ii in range(25):
        dat = s_from_p.get_data(True)
        assert np.all(dat == ii)


@pytest.mark.parametrize('threaded_shm_poster',
                         [('x', (2, 3, 4), np.float32, 0.01)], indirect=True)
def test_shm_threaded_poster_2(threaded_shm_poster):
    s_to_p, s_from_p, process = threaded_shm_poster

    for ii in range(25):
        dat = s_from_p.get_data(True, timeout=None)
        assert np.all(dat == ii)


@pytest.mark.parametrize('threaded_shm_poster_recreator',
                         [('x', (2, 3, 4), np.float32, 0.01)], indirect=True)
def test_shm_threaded_poster_overwriter(threaded_shm_poster_recreator):
    s_to_p, s_from_p, process = threaded_shm_poster_recreator

    for ii in range(25):
        s_from_p.non_block_wait_semaphore(0.001)
        dat = s_from_p.get_data(False)
        if ii == 0:
            refval = dat.flat[0]
        assert np.all(dat == ii + refval)


@pytest.mark.parametrize('threaded_shm_poster_recreator',
                         [('x', (2, 3, 4), np.float32, 0.01)], indirect=True)
def test_shm_threaded_poster_overwriter_2(threaded_shm_poster_recreator):
    s_to_p, s_from_p, process = threaded_shm_poster_recreator

    for ii in range(25):
        s_from_p.non_block_wait_until_recreation(0.001)
        dat = s_from_p.get_data(False)
        if ii == 0:
            refval = dat.flat[0]
        assert np.all(dat == ii + refval)


@pytest.mark.parametrize('threaded_shm_poster',
                         [('x', (2, 3, 4), np.float32, 0.01)], indirect=True)
def test_multi_recv_data(threaded_shm_poster):
    _, s_from_p, _ = threaded_shm_poster

    dat = s_from_p.multi_recv_data(100, False)

    assert isinstance(dat, list)
    assert len(dat) == 100
    assert all([d.shape == s_from_p.shape for d in dat])

    sliced = np.asarray([d[0, 0, 0] for d in dat])
    assert np.all(sliced - sliced[0] == np.arange(100))


@pytest.mark.parametrize('threaded_shm_poster',
                         [('x', (2, 3, 4), np.float32, 0.01)], indirect=True)
@pytest.mark.parametrize('do_counter', (True, False))
def test_multi_recv_data_2(threaded_shm_poster, do_counter):
    _, s_from_p, _ = threaded_shm_poster

    dat = s_from_p.multi_recv_data(100, True, monitorCount=do_counter)

    assert isinstance(dat, np.ndarray)
    assert dat.shape == (100, 2, 3, 4)

    sliced = dat[:, 0, 0, 0]
    assert np.all(sliced - sliced[0] == np.arange(100))
