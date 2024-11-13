import pytest

import os, glob


# ConfTest.py FIXTture == ctfixt_
@pytest.fixture(scope='session', autouse=True)
def ctfixt_change_MILK_SHM_DIR(request):

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


@pytest.fixture(scope='session', autouse=True)
def ctfixt_change_pwd(tmpdir_factory, request):

    # This would be better used with the --basetemp feature...

    basetemp = None
    if 'PYTEST_TMPDIR' in os.environ:
        basetemp = os.environ['PYTEST_TMPDIR']
    if basetemp:
        tmpdir_factory.config.option.basetemp = basetemp

    #from datetime import datetime
    #time_str = datetime.strftime(datetime.now(), '%Y%m%dT%H%M%S')

    # if there is no basetemp, this just straight up goes to a tree in /tmp! Perfect
    folder = tmpdir_factory.mktemp('pytest_dont_use_for_anything')
    orig_cwd = os.getcwd()

    os.chdir(folder)

    yield None

    os.chdir(orig_cwd)
