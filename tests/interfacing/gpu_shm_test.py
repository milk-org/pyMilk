import pytest

import os
import time

import numpy as np

from pyMilk.interfacing.shm import SHM, check_SHM_name
from pyMilk import errors

from ..conftestaux.async_shm_fixtures import serve_external_shm

from pyMilk.interfacing.shm import IMAGESTREAMIO_HAVE_CUDA
if not IMAGESTREAMIO_HAVE_CUDA:
    pytest.skip("Skipping GPU tests.", allow_module_level=True)

import cupy as cp


@pytest.mark.parametrize('data', [
        ((1, ), np.int8),
        ((1, 1), np.int16),
        ((1, 1, 1), np.int32),
        ((1, 2, 3), np.int64),
        ((2, 1, 3), np.uint8),
        ((2, 3, 1), np.uint16),
        ((10, ), np.uint32),
        ((10, 20), np.uint64),
        ((10, 20, 30), np.float32),
])
def test_create_good_data_tuple(data):
    shape, dtype = data
    s = SHM('a_name', data, symcode=0, autoSqueeze=False, location=0)

    got_data = s.get_data()
    assert s.shape == s.shape_c
    assert s.shape == shape

    assert isinstance(got_data, np.ndarray)
    assert got_data.dtype == s.nptype
    assert got_data.dtype == dtype

    s.destroy()


def test_double_open():
    data = ((10, 20), np.uint64)
    s1 = SHM('a_name', data, symcode=0, autoSqueeze=False, location=0)
    s2 = SHM('a_name')  # pass

    s1.get_data()
    s2.get_data()

    s2.close()
    s1.destroy()


def test_double_open_with_data_sanity():
    data = np.random.randn(10, 20)
    s1 = SHM('a_name', data, autoSqueeze=False, location=0)
    s2 = SHM('a_name')  # pass

    np.testing.assert_almost_equal(s1.get_data(), data)
    np.testing.assert_almost_equal(s2.get_data(), data)
    data = np.random.randn(10, 20)
    s2.set_data(data)
    np.testing.assert_almost_equal(s1.get_data(), data)
    np.testing.assert_almost_equal(s2.get_data(), data)

    s2.close()
    s1.destroy()


# def serve_external_shm(name: str, shape: tuple[int, ...], nptype: np.typing.DTypeLike, location: int):


@pytest.mark.parametrize('serve_external_shm', [('gpu_test',
                                                 (100, 200), np.float64, 0)],
                         indirect=True)
def test_external_process_serves_shm(serve_external_shm: SHM):
    # Nothing to test, the fixture already opened the local SHM and will close it.
    assert np.all(serve_external_shm.get_data() == 0.0)


@pytest.mark.parametrize('serve_external_shm', [('gpu_test',
                                                 (100, 200), np.float64, 0)],
                         indirect=True)
@pytest.mark.parametrize('overwrite_location', [-1, 0])
def test_external_process_serves_shm_and_make_zombie(serve_external_shm: SHM,
                                                     overwrite_location: int):
    # Nothing to test, the fixture already opened the local SHM and will close it.
    assert np.all(serve_external_shm.get_data() == 0.0)
    z_data = serve_external_shm.get_data()

    new_handle = SHM(serve_external_shm.FNAME)

    overwrite = SHM(serve_external_shm.FNAME, z_data - 1,
                    location=overwrite_location)

    # We purposefully _disable_ autorelink_if_need
    # Otherwise we'd just automatically relink to overwritecpu.
    # But here we're testing that the zombie is staying alive

    assert np.all(serve_external_shm.get_data(autorelink_if_need=False) == 0.0)
    assert np.all(overwrite.get_data() == -1.0)

    new_handle.set_data(z_data + 1, autorelink_if_need=False)
    assert np.all(
            serve_external_shm.get_data(autorelink_if_need=False) == +1.0)
    assert np.all(overwrite.get_data() == -1.0)


def test_cp_array_to_cpu_shm():
    x_np = np.random.randn(123, 45).astype(np.float32)
    x_cp = cp.array(-x_np)

    s_cpu = SHM('x', x_np)

    s_cpu.set_data(x_cp)

    assert np.all(s_cpu.get_data() == -x_np)

    s_cpu.destroy()


def test_gpu_shm_from_np_cp():
    x_np = np.random.randn(123, 45).astype(np.float32)
    x_cp = cp.array(-x_np)

    s_gpu = SHM('x', x_cp, location=0)
    assert type(s_gpu.get_data(copy=False)) == cp.ndarray
    assert np.all(s_gpu.get_data(copy=True) == -x_np)
    assert np.all(s_gpu.get_data(copy=False) == x_cp)
    s_gpu.destroy()

    s_gpu = SHM('x', x_np, location=0)
    assert np.all(s_gpu.get_data(copy=True) == x_np)
    assert np.all(s_gpu.get_data(copy=False) == -x_cp)

    s_gpu.destroy()
