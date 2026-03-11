import pytest
from pyMilk.ImageStreamIOWrap import _gcov_dump


@pytest.fixture(scope="session", autouse=True)
def ctfixt_coverage_gcov_dump_at_session_end():
    yield
    _gcov_dump()
