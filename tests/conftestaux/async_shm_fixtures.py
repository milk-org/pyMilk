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


@pytest.fixture
def threaded_shm_data_pingpong(request):
    yield from subfixture(request, _thread_func_pingpong)


@pytest.fixture
def threaded_shm_poster(request):
    yield from subfixture(request, _thread_func_looper)


@pytest.fixture
def threaded_shm_poster_recreator(request):
    yield from subfixture(request,
                          _thread_func_looper_with_forceful_recreation)


def subfixture(request, subprocess_callable):

    prefix, shape, dtype, delay = request.param

    shm_toprocess = SHM(prefix + '_toprocess', (shape, dtype))
    shm_fromprocess = SHM(prefix + '_fromprocess', (shape, dtype))
    exit_event = multiprocessing.Event()

    # Originally attempted with a threading.Thread, but that causes issues
    # because the semID issued is identical !
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
