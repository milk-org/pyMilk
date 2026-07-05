from __future__ import annotations

import pytest

from pyMilk.interfacing.transport import CopybackComputeUnit
from pyMilk.interfacing.shm import SHM
import pyMilk.TransportWrap as TW

import numpy as np


def test_copyback():
    arr = np.random.randn(50, 30)
    s = SHM('input', arr)
    c = CopybackComputeUnit('compute_unit', 'shm::input',
                            TW.InternalStorageEnum.CPU_MEMORY, 'shm::output',
                            TW.InternalStorageEnum.CPU_MEMORY)
    c.loop(1)

    s2 = SHM('output')

    arr2 = s2.get_data()

    np.testing.assert_equal(arr, arr2)

    s2.destroy()
    s.destroy()


import time

ADDR = 'tcp://127.0.0.1:12345'
#ADDR = 'udp://127.0.0.1:12345'
'''
epgm _may_ work but not onto oneself, I need another machine to relay.
But deploying on my second computer, epgm not built in...
'''


def mproc_func(arr, mem_target: int = -1):
    s = SHM('input', arr, symcode=0)

    tport_emit = TW.ZmqEmit(ADDR, s.IMAGE.md)
    tport_emit.init_storage_target(mem_target)

    for _ in range(100):
        tport_emit.write(arr)
        tport_emit.move_and_publish_data(0.0)
        time.sleep(.02)
    print('Done sending 100.')

    del tport_emit


pmp = pytest.mark.parametrize


@pmp('recv_mem_target', (-1, 0))
@pmp('emit_mem_target', (-1, 0))
def test_zmq_transports(recv_mem_target: int, emit_mem_target: int):

    import multiprocessing
    arr = np.random.randn(50, 30)
    t = multiprocessing.Process(target=mproc_func, args=(arr, emit_mem_target))
    t.start()

    time.sleep(0.5)

    tport_recv = TW.ZmqRecv(ADDR)
    tport_recv.init_storage_target(recv_mem_target)

    for _ in range(5):
        tport_recv.sync_barrier()
        tport_recv.move_new_data_to_requested()

    arr2 = tport_recv.copy()  # Always CPU
    np.testing.assert_equal(arr, arr2)

    t.join()

    del tport_recv
