from __future__ import annotations
import typing as typ

import pytest

_pmp = pytest.mark.parametrize

import os

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


def emitter_class(transport_type: str):
    return {
            'shm': TW.ImageStreamIOEmit,
            'zmq': TW.ZmqEmit,
            'tcp': TW.MilkTcpEmit,
            'udp': TW.MilkUdpEmit
    }[transport_type]


def receiver_class(transport_type: str):
    return {
            'shm': TW.ImageStreamIORecv,
            'zmq': TW.ZmqRecv,
            'tcp': TW.MilkTcpRecv,
            'udp': TW.MilkUdpRecv
    }[transport_type]


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


# I need the multiprocessed to be fixturized, maybe
# I need to encapsulate dakine in a termination event but also a "constructor terminated" event, which will allow avoiding shitty race conditions in the asserts
# when stopping/restarting stuff

# Implement unicast and multicast UDP.


def multiprocessed_recvtransport_to_shm(recv_endpoint: str, mem_target: int,
                                        event_termination: Event,
                                        event_ready: Event | None = None):

    s = SHM('mproc_recv_relay', symcode=0)  # assert exists

    transport_type, transport_address = recv_endpoint.split('::')
    Klass = receiver_class(transport_type)
    tport_recv = Klass(transport_address)
    tport_recv.init_storage_target(mem_target)

    count = 0
    while True:
        if event_termination.is_set():
            event_termination.clear()
            break
        ret = tport_recv.sync_barrier()
        print(f'sync_barrier returns {ret}')
        # Wait after the first / receive timeout to confirm ready.
        if event_ready:
            event_ready.set()

        if ret != TW.SyncEnum.SUCCESS:
            continue

        tport_recv.move_new_data_to_requested()
        print("pushing data out... 1")

        s.set_data(tport_recv.view())  # assert size/dtype are OK
        count += 1

    print(f'Done receiving {count}.')

    del tport_recv


def multiprocessed_shm_to_emittransport(emit_endpoint: str, mem_target: int,
                                        event_termination: Event,
                                        event_ready: Event | None = None):
    s = SHM('mproc_emit_relay', symcode=0)

    transport_type, transport_address = emit_endpoint.split('::')
    Klass = emitter_class(transport_type)
    tport_emit = Klass(transport_address, s.IMAGE.md)
    tport_emit.init_storage_target(mem_target)

    if event_ready:
        event_ready.set()

    count = 0
    while True:
        if event_termination.is_set():
            event_termination.clear()
            break

        arr = s.get_data(True, copy=False, timeout=0.05,
                         return_none_on_timeout=True)
        if arr is None:
            continue
        print("Mproc'd emitter: received real data !")
        tport_emit.write(arr)
        tport_emit.move_and_publish_data(0.0)  # atime
        count += 1

    print(f'Done sending {count}.')

    del tport_emit


def _recv_get_data(tport_recv):
    ret = tport_recv.sync_barrier()
    if ret == TW.SyncEnum.SUCCESS:
        tport_recv.move_new_data_to_requested()
        return tport_recv.copy()
    elif ret == TW.SyncEnum.TIMEOUT:
        return None

    raise ValueError('Error on sync_barrier other than TIMEOUT')


MILK_UDP_DEFAULT_MCAST_GROUP = "239.72.55.1"


@_pmp(
        ('emit_addr', 'recv_addr'),
        [  #
                ('shm::x', 'shm::x'),
                ('tcp::127.0.0.1:12345', 'tcp::12345'),
                ('zmq::ipc:///tmp/ipcptr', 'zmq::ipc:///tmp/ipcptr'),
                ('zmq::tcp://127.0.0.1:12346', 'zmq::tcp://127.0.0.1:12346'),
                ('udp::127.0.0.1:12346', 'udp::12346'),
                (f'udp::{MILK_UDP_DEFAULT_MCAST_GROUP}:12346', 'udp::12346'),
        ])
def test_emit_survives_recv_respawn(emit_addr, recv_addr):
    # Same as test_emit_survives_recv_respawn, except the recv transport is
    # created/destroyed and driven directly in the main testing thread,
    # instead of being dispatched to a multiprocessed instance.
    arr = np.random.randn(60, 61).astype(np.float32)
    shm_emit = SHM('mproc_emit_relay', arr, symcode=0)

    event_emit = Event()

    def mkemit_subprocessed():
        event_spool = Event()
        p = multiprocessing.Process(
                target=multiprocessed_shm_to_emittransport,
                args=(emit_addr, -1, event_emit, event_spool))
        p.start()
        assert event_spool.wait(timeout=5.0)  # assert fails on timeout

        return p

    def mkrecv_mainthread():
        transport_type, transport_address = recv_addr.split('::')
        Klass = receiver_class(transport_type)
        tport_recv = Klass(transport_address)
        tport_recv.init_storage_target(-1)
        return tport_recv

    te = mkemit_subprocessed()

    for kk in range(5):
        tport_recv = mkrecv_mainthread()
        # tport_recv needs a semflush !
        while tport_recv.sync_barrier() == TW.SyncEnum.SUCCESS:
            # Effectively a semflush - but also stale connections in the TCP variant.
            # while tport_recv.sync_barrier() != TW.SyncEnum.TIMEOUT will NOT work for the TCP variant.
            pass

        assert te.is_alive()

        # Send 100 sync'd frames
        for ll in range(100):
            arr = np.random.randn(60, 61).astype(np.float32)
            arr[0, 0] = ll + kk * 1000
            shm_emit.set_data(arr)
            print(f'Posted new data ({ll}) (sample={arr[0,0]}, {arr[0,1]}, {arr[0,2]})'
                  )
            arr_returned = _recv_get_data(tport_recv)
            print(f'Tport received {None if arr_returned is None else arr_returned[0,0]}'
                  )

            if ll > 1:  # TCP initialization quirks and timeout values
                assert arr_returned is not None
                np.testing.assert_equal(arr_returned, arr)

        print('destroying recv transport')
        del tport_recv

        # Send frames into oblivion
        for ll in range(10):
            arr = np.random.randn(60, 61).astype(np.float32)
            shm_emit.set_data(arr)
            assert shm_emit.get_data(True, timeout=0.01,
                                     return_none_on_timeout=True) is None

    event_emit.set()
    te.join()

    shm_emit.destroy()
    try:
        os.remove('/tmp/ipcptr')
    except FileNotFoundError:
        pass


@_pmp(
        ('emit_addr', 'recv_addr'),
        [  #
                ('shm::x', 'shm::x'),
                ('tcp::127.0.0.1:12345', 'tcp::12345'),
                ('zmq::ipc:///tmp/ipcptr', 'zmq::ipc:///tmp/ipcptr'),
                ('zmq::tcp://127.0.0.1:12346', 'zmq::tcp://127.0.0.1:12346'),
                ('udp::127.0.0.1:12346', 'udp::12346'),
                (f'udp::{MILK_UDP_DEFAULT_MCAST_GROUP}:12346', 'udp::12346'),
        ])
def test_recv_survives_emit_respawn(emit_addr, recv_addr):
    arr = np.random.randn(60, 61).astype(np.float32)
    shm_recv = SHM('mproc_recv_relay', arr * 0, symcode=0)

    event_recv = Event()

    def mkemit_mainthread():
        transport_type, transport_address = emit_addr.split('::')
        Klass = emitter_class(transport_type)
        tport_emit = Klass(transport_address, shm_recv.IMAGE.md)
        tport_emit.init_storage_target(-1)

        return tport_emit

    def mkrecv_subprocessed():
        event_spool = Event()
        p = multiprocessing.Process(
                target=multiprocessed_recvtransport_to_shm,
                args=(recv_addr, -1, event_recv, event_spool))
        p.start()
        assert event_spool.wait(timeout=5.0)  # assert fails on timeout
        return p

    for kk in range(5):
        te = mkemit_mainthread()

        # Needs posteriority of recv instantiation for some transports.
        if kk == 0:
            #_ensure_shm_exists('x', timeout=1.0)
            tr = mkrecv_subprocessed()
            shm_recv._checkGrabSemaphore(
            )  # Also hides a race condition in the restart
            shm_recv.IMAGE.semflush(shm_recv.semID)

        assert tr.is_alive()

        # Send 100 sync'd frames
        for ll in range(100):
            arr = np.random.randn(60, 61).astype(np.float32)
            arr[0, 0] = ll + kk * 1000
            te.write(arr)
            te.move_and_publish_data(0.0)

            print(f'Posted new data ({ll})')
            # timeout on get_data must be longer than
            # static constexpr std::chrono::milliseconds DEFAULT_TIMEOUT{100};
            # that is passed to all receivers
            arr_returned = shm_recv.get_data(True, timeout=0.5,
                                             return_none_on_timeout=True,
                                             checkSemAndFlush=False)
            print(f'received {None if arr_returned is None else arr[0,0]}...')

            if ll > 0:  # TCP initialization quirks and timeout values
                assert arr_returned is not None
                np.testing.assert_equal(arr_returned, arr)

        print('killing emit')
        del te

    event_recv.set()
    tr.join()

    shm_recv.destroy()
    try:
        os.remove('/tmp/ipcptr')
    except FileNotFoundError:
        pass
    try:
        x = SHM('x')
        x.destroy()
    except:
        pass
