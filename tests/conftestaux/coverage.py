import pytest
from pyMilk.ImageStreamIOWrap import _gcov_dump


@pytest.fixture(scope="session", autouse=True)
def ctfixt_coverage_gcov_dump_at_session_end():
    yield
    _gcov_dump()


'''
COVERAGE=ON python setup.py build_ext --inplace --build-temp ./build
cp build/pyMilk/*.so pyMilk/
sudo cp build/pyMilk/lib*.so /usr/local/milk/lib/ # Override milk libImageStreamIO.so... otherwise linking is confused.
coverage run
gcovr --verbose -r . --html-details -o gcov_html/c_coverage.html
firefox ./gcov_html/c_coverage.html
#rm -rf ./build
#rm pyMilk/*.so
'''
