import pytest

import os, glob


# ConfTest.py FIXTture == ctfixt_
@pytest.fixture(scope='session')
def fixture_change_MILK_SHM_DIR(request):

    MILK_SHM_DIR_SPOOF = '/tmp/milk_shm_dir_pytest'
    os.makedirs(MILK_SHM_DIR_SPOOF)

    # There was something more aggressive to reload the env at runtime, wasn't there?
    os.environ['MILK_SHM_DIR'] = MILK_SHM_DIR_SPOOF

    yield MILK_SHM_DIR_SPOOF

    # Fixture teardown here:

    files = glob.glob(MILK_SHM_DIR_SPOOF + '/*')
    for file in files:
        os.remove(file)
    os.removedirs(MILK_SHM_DIR_SPOOF)
