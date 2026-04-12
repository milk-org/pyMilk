import pytest

import numpy as np

from pyMilk.interfacing.shm import SHM
from pyMilk.interfacing.fps import FPS
'''
These printing tests are not very useful in their testability, but can be inspected quickly
by running this file with pytest -s
'''


@pytest.fixture()
def shm_fixt():
    s = SHM('x', np.random.randn(10, 15))
    yield s
    s.destroy()


def test_print_shm(shm_fixt: SHM):
    shm_fixt.print_meta_data()


def test_str_shm(shm_fixt: SHM):
    print(shm_fixt)


def test_repr_shm(shm_fixt: SHM):
    print(repr(shm_fixt))
