import multiprocessing

multiprocessing.set_start_method('fork')

import time
import numpy as np
import pytest

from pyMilk.interfacing.shm import SHM

multiprocessing.Event


def _thread_func_pingpong(shm_toprocess: SHM, shm_fromprocess: SHM,
                          delay: float, evt) -> None:
    shm_toprocess._checkGrabSemaphore()
    shm_toprocess.IMAGE.semflush(shm_toprocess.semID)

    shm_fromprocess._checkGrabSemaphore()
    shm_fromprocess.IMAGE.semflush(shm_fromprocess.semID)

    break_all = False

    while True:
        while not shm_toprocess.check_sem_timedwait(0.001):
            if evt.is_set():
                break_all = True
                break
        if evt.is_set() or break_all:
            break

        # Repost the semaphores.
        dat = shm_toprocess.get_data(
                False)  # check_sem_timedwait clicked the semaphore.
        #print('P: giggity')
        time.sleep(delay)
        shm_fromprocess.set_data(dat + 1)


def _thread_func_looper(shm_toprocess: SHM, shm_fromprocess: SHM, delay: float,
                        evt) -> None:
    dat = shm_fromprocess.get_data()
    ii = -1
    while True:
        ii += 1
        if evt.is_set():
            break
        dat[:] = ii
        time.sleep(delay)
        shm_fromprocess.set_data(dat)


def _thread_func_looper_with_forceful_recreation(shm_toprocess: SHM,
                                                 shm_fromprocess: SHM,
                                                 delay: float, evt) -> None:
    dat = shm_fromprocess.get_data()
    ii = -1
    while True:
        ii += 1
        if evt.is_set():
            break

        dat[:] = ii
        time.sleep(delay)
        shm_fromprocess.close()
        shm_fromprocess = SHM(
                shm_fromprocess.FNAME,
                dat)  # I count that this induces a set_data and sempost


def _thread_func_create_kill_recreate(shm_toprocess: SHM, shm_fromprocess: SHM,
                                      delay: float, evt) -> None:
    time.sleep(0.1)  # be sure we got time to instantiate in main process
    shm_toprocess.repost()
    print(f'orig -- {shm_toprocess.IMAGE.md.inode}')
    time.sleep(0.1)
    shm_toprocess = SHM(shm_toprocess.FNAME,
                        (shm_toprocess.shape, shm_toprocess.nptype))
    shm_fromprocess.repost()
    print(f'recreated -- {shm_toprocess.IMAGE.md.inode}')
    time.sleep(0.1)
    shm_toprocess.repost()

    time.sleep(0.1)
    shm_toprocess = SHM(shm_toprocess.FNAME, ((5, 2, 3), np.int32))
    shm_fromprocess.repost()
    print(f'recreated -- {shm_toprocess.IMAGE.md.inode}')
    time.sleep(0.1)
    shm_toprocess.repost()


@pytest.fixture
def threaded_shm_data_pingpong(request):
    yield from subfixture(request, _thread_func_pingpong)


@pytest.fixture
def threaded_shm_data_pingpong_gpu(request):
    yield from subfixture(request, _thread_func_pingpong, location=0)


@pytest.fixture
def threaded_shm_poster(request):
    yield from subfixture(request, _thread_func_looper)


@pytest.fixture
def threaded_shm_poster_recreator(request):
    yield from subfixture(request,
                          _thread_func_looper_with_forceful_recreation)


@pytest.fixture
def threaded_shm_create_kill_recreate(request):
    yield from subfixture(request, _thread_func_create_kill_recreate)


def subfixture(request, subprocess_callable, location=-1):

    prefix, shape, dtype, delay = request.param

    shm_toprocess = SHM(prefix + '_toprocess', (shape, dtype),
                        location=location)
    shm_fromprocess = SHM(prefix + '_fromprocess', (shape, dtype),
                          location=location)
    exit_event = multiprocessing.Event()

    # Originally attempted with a threading.Thread, but that causes issues
    # because the semID issued is identical !
    # Also completely nuts to copy the struct, would be much better to pass name, shape, dtype !!
    subprocess = multiprocessing.Process(
            target=subprocess_callable, args=(shm_toprocess, shm_fromprocess,
                                              delay, exit_event), daemon=False)
    subprocess.start()

    shm_toprocess._checkGrabSemaphore()
    shm_fromprocess._checkGrabSemaphore()
    shm_toprocess.IMAGE.semflush(shm_toprocess.semID)
    shm_fromprocess.IMAGE.semflush(shm_fromprocess.semID)
    yield shm_toprocess, shm_fromprocess, subprocess

    exit_event.set()
    subprocess.join(timeout=1.0)

    assert not subprocess.is_alive()

    shm_toprocess.destroy()
    shm_fromprocess.destroy()
