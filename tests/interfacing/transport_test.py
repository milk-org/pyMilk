from __future__ import annotations
import typing as typ

import pytest

from pyMilk.interfacing.transport import CopybackComputeUnit
from pyMilk.interfacing.shm import SHM
import pyMilk.TransportWrap as TW

import multiprocessing
if typ.TYPE_CHECKING:
    Event: typ.TypeAlias = multiprocessing.Event  # type: ignore
else:
    from multiprocessing import Event

from ..conftestaux.gpu_configure import SINGLE_GPU_TESTING, MULTI_GPU_TESTING

import time
import numpy as np


def _ensure_shm_exists(name: str, timeout: float) -> bool:
    t = time.time()
    while time.time() - t < timeout:
        try:
            s = SHM(name)
            return True
        except FileNotFoundError:
            pass

    return False


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


def multiprocessed_recvtransport_to_shm(recv_endpoint: str, mem_target: int,
                                        event: Event):

    s = SHM('mproc_recv_relay', symcode=0)  # assert exists

    transport_type, transport_address = recv_endpoint.split('::')
    Klass = {
            'shm': TW.ImageStreamIORecv,
            'zmq': TW.ZmqRecv,
            'tcp': TW.MilkTcpRecv,
    }[transport_type]
    tport_recv = Klass(transport_address)
    tport_recv.init_storage_target(mem_target)

    count = 0
    while True:
        if event.is_set():
            event.clear()
            break
        ret = tport_recv.sync_barrier()
        print(f'sync_barrier returns {ret}')
        if ret != TW.SyncEnum.SUCCESS:
            continue

        tport_recv.move_new_data_to_requested()
        print("pushing data out... 1")

        s.set_data(tport_recv.view())  # assert size/dtype are OK
        count += 1

    print(f'Done receiving {count}.')

    del tport_recv


def multiprocessed_shm_to_emittransport(emit_endpoint: str, mem_target: int,
                                        event: Event):
    s = SHM('mproc_emit_relay', symcode=0)

    transport_type, transport_address = emit_endpoint.split('::')
    Klass = {
            'shm': TW.ImageStreamIOEmit,
            'zmq': TW.ZmqEmit,
            'tcp': TW.MilkTcpEmit,
    }[transport_type]
    tport_emit = Klass(transport_address, s.IMAGE.md)
    tport_emit.init_storage_target(mem_target)

    count = 0
    while True:
        if event.is_set():
            event.clear()
            break

        arr = s.get_data(True, copy=False, timeout=0.05,
                         return_none_on_timeout=True)
        if arr is None:
            continue
        print('Emit: received real data !')
        tport_emit.write(arr)
        tport_emit.move_and_publish_data(0.0)  # atime
        count += 1

    print(f'Done sending {count}.')

    del tport_emit


pmp = pytest.mark.parametrize


@pmp(('emit_addr', 'recv_addr'), [('shm::x', 'shm::x'),
                                  ('tcp::127.0.0.1:12345', 'tcp::12345')])
def test_emit_survives_recv_respawn(emit_addr, recv_addr):
    arr = np.random.randn(60, 61).astype(np.float32)
    shm_emit = SHM('mproc_emit_relay', arr)
    shm_recv = SHM('mproc_recv_relay', arr * 0)

    event_emit = Event()
    event_recv = Event()

    def mkemit():
        return multiprocessing.Process(
                target=multiprocessed_shm_to_emittransport,
                args=(emit_addr, -1, event_emit))

    def mkrecv():
        return multiprocessing.Process(
                target=multiprocessed_recvtransport_to_shm,
                args=(recv_addr, -1, event_recv))

    te = mkemit()
    te.start()
    import time
    time.sleep(0.1)  # We need to make sure, in the SHM case,
    # that "emit" has created the SHM, since we can't spawn a recv on a non-existent SHM
    # Or could we??

    for kk in range(5):
        tr = mkrecv()
        tr.start()
        time.sleep(2)  # Hides a race conditions in the restart...

        assert te.is_alive()

        shm_recv._checkGrabSemaphore(
        )  # Also hides a race condition in the restart
        shm_recv.IMAGE.semflush(shm_recv.semID)

        # Send 100 sync'd frames
        for ll in range(100):
            arr = np.random.randn(60, 61).astype(np.float32)
            shm_emit.set_data(arr)
            print(f'Posted new data ({ll}).')
            arr_returned = shm_recv.get_data(True, timeout=0.3,
                                             return_none_on_timeout=True,
                                             checkSemAndFlush=False)

            if ll > 1:  # TCP initialization quirks and timeout values
                assert arr_returned is not None
                np.testing.assert_equal(arr_returned, arr)

        print('event_recv set')
        #event_recv.set()
        tr.kill(
        )  # calling event_recv.set() induces a full timeout cycle and it's annoying. But this probably bypasses the destructor for shared resources
        tr.join()

        print('recv joined')

        # Send frames into oblivion
        for ll in range(10):
            arr = np.random.randn(60, 61).astype(np.float32)
            shm_emit.set_data(arr)
            assert shm_emit.get_data(True, timeout=0.01,
                                     return_none_on_timeout=True) is None

    event_emit.set()
    te.join()

    shm_emit.destroy()
    shm_recv.destroy()


@pmp(('emit_addr', 'recv_addr'), [('shm::x', 'shm::x'),
                                  ('tcp::127.0.0.1:12345', 'tcp::12345')])
def test_transport_recv_survives_emit_respawn(emit_addr, recv_addr):
    arr = np.random.randn(60, 61).astype(np.float32)
    shm_emit = SHM('mproc_emit_relay', arr)
    shm_recv = SHM('mproc_recv_relay', arr * 0)

    event_emit = Event()
    event_recv = Event()

    def mkemit():
        return multiprocessing.Process(
                target=multiprocessed_shm_to_emittransport,
                args=(emit_addr, -1, event_emit))

    def mkrecv():
        return multiprocessing.Process(
                target=multiprocessed_recvtransport_to_shm,
                args=(recv_addr, -1, event_recv))

    for kk in range(5):
        te = mkemit()
        # TODO the emit transport authoritatively re-creates the SHM.
        # TODO so either -- it shouldn't.
        # TODO or we should systematically have autorelink capability.
        # TODO This causes failure in the SHM case.
        te.start()

        if kk == 0:
            _ensure_shm_exists('x', timeout=3.0)
            tr = mkrecv()
            tr.start()
            shm_recv._checkGrabSemaphore(
            )  # Also hides a race condition in the restart
            shm_recv.IMAGE.semflush(shm_recv.semID)

        time.sleep(2)

        assert tr.is_alive()

        # Send 100 sync'd frames
        for ll in range(100):
            arr = np.random.randn(60, 61).astype(np.float32)
            shm_emit.set_data(arr)
            print(f'Posted new data ({ll})')
            arr_returned = shm_recv.get_data(True, timeout=0.3,
                                             return_none_on_timeout=True,
                                             checkSemAndFlush=False)

            if ll > 1:  # TCP initialization quirks and timeout values
                assert arr_returned is not None
                np.testing.assert_equal(arr_returned, arr)

        print('event_recv set')
        #event_recv.set()
        te.kill(
        )  # calling event_recv.set() induces a full timeout cycle and it's annoying. But this probably bypasses the destructor for shared resources
        te.join()

        print('emit joined')

    event_recv.set()
    tr.join()

    shm_emit.destroy()
    shm_recv.destroy()
