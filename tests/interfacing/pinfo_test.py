import pytest

import os

from pyMilk.interfacing.pinfo import ProcessInfo


@pytest.fixture
def pinfo():
    p = ProcessInfo()
    p.create("pytest_pinfo", 0)

    yield p

    # The destructor tries to be smart
    # But I can't control when the destructor is invoked
    # And if it's invoked after the MILK_SHM_DIR spoof fixture teardown, this all goes wrong.


def test_fixt(pinfo: ProcessInfo):
    assert True


def test_fixt_reused(pinfo: ProcessInfo):
    assert True
