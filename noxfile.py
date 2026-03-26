import os, sys
import nox

nox.options.error_on_missing_interpreters = False
'''
I am investigating nox as an option to run the (some) tests with multiple "installation modes"
In particular, with the dependency on libImageStreamIO.so and the building of the python
module with pybind, and combining between editable and non-editable installs,
comprehending _what_ exactly happens is tricky.

Nox should allow making virgin python environments, installing one way or another, and
running the test suite.

The functions below test for editable/non-editable install
and whether the tests are run from pyMilk dir or externally. This matters due to PYTHONPATH resolution.
'''

#def clean_all_possible_stale_things():


def hack_cacaoprocesstools_from_abspath():
    '''
    Symlink the installed CacaoProcessTools cpython extension.
    This is necessary because CPT is not (yet) packaged as part of pyMilk / part of this
    repo's build system. It's fully external.
    '''
    os.makedirs('/tmp/nox/', exist_ok=True)
    for py_ver in range(7, 15):
        try:
            os.remove(
                    f'/tmp/nox/CacaoProcessTools.cpython-3{py_ver}-x86_64-linux-gnu.so'
            )
        except FileNotFoundError:
            ...
        try:
            os.symlink(
                    f'{os.environ["MILK_INSTALLDIR"]}/python/CacaoProcessTools.cpython-3{py_ver}-x86_64-linux-gnu.so',
                    f'/tmp/nox/CacaoProcessTools.cpython-3{py_ver}-x86_64-linux-gnu.so'
            )
        except FileExistsError:
            ...


@nox.session
def tests_not_installed_inner(session: nox.Session):
    '''
    NOT INSTALLED, ie no pip install; test from inside the project root.
    INNER testing, ie we run pytest while PWD is $PYMILK_ROOT (often $HOME/src/pyMilk/)
    ACTUALLY THAT CANNOT WORK, due to uncompiled ImageStreamIO / CacaoProcessTools which gets compiled on install.
    '''
    import CacaoProcessTools
    ...


@nox.session
def tests_editable_inner(session: nox.Session):
    '''
    EDITABLE installation, ie `pip install -e .`; test from inside the project root.
    INNER testing, ie we run pytest while PWD is $PYMILK_ROOT (often $HOME/src/pyMilk/)
    '''
    #session.run("./clean.sh", external=True)

    hack_cacaoprocesstools_from_abspath()

    session.install("pytest", "pyright")
    session.install("-e", ".")
    session.install("pyright")

    # Run tests from inside the project root
    # Pass the env for CacaoProcessTools
    session.run("pytest", env={'PYTHONPATH': '/tmp/nox'})


@nox.session
def tests_editable_outer(session: nox.Session):
    '''
    EDITABLE installation, ie `pip install -e .`; test from outside the project root.
    OUTER testing, ie we run pytest while PWD is NOT $PYMILK_ROOT; here's its /tmp/nox
    '''
    #session.run("./clean.sh", external=True)

    hack_cacaoprocesstools_from_abspath()

    session.install("pytest", "pyright")
    session.install("-e", ".")
    session.install("pyright")

    # Run tests from outside the project root
    project_dir = os.path.abspath(os.getcwd())
    os.makedirs("/tmp/nox_runtime", exist_ok=True)
    with session.chdir("/tmp/nox_runtime"):
        session.run("pytest", project_dir, env={'PYTHONPATH': '/tmp/nox'})


@nox.session
def tests_non_editable_inner_and_outer(session: nox.Session):
    '''
    NON-EDITABLE installation, ie `pip install .`; run from both inside and outside the project root.
    IN and OUT testing
    '''
    #session.run("./clean.sh", external=True)

    session.install("pytest")
    session.install(".")

    hack_cacaoprocesstools_from_abspath()

    # Change cwd to something else...
    # Otherwise name collision with the source folder, rather than the install in the venv.
    project_dir = os.path.abspath(os.getcwd())
    os.makedirs("/tmp/nox_runtime", exist_ok=True)
    with session.chdir("/tmp/nox_runtime"):
        session.run("pytest", project_dir, env={'PYTHONPATH': '/tmp/nox'})
