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


def hack_cacaoprocesstools_from_abspath():
    '''
    Symlink the installed CacaoProcessTools cpython extension.
    This is necessary because CPT is not (yet) packaged as part of pyMilk / part of this
    repo's build system. It's fully external.
    '''
    os.makedirs('/tmp/nox/', exist_ok=True)
    try:
        os.symlink(
                '/home/vdeo/mambaforge/lib/python3.10/site-packages/CacaoProcessTools.cpython-310-x86_64-linux-gnu.so',
                '/tmp/nox/CacaoProcessTools.cpython-310-x86_64-linux-gnu.so')
    except FileExistsError:
        ...

    sys.path.append('/tmp/nox/')
    import CacaoProcessTools


@nox.session
def tests_not_installed_inner(session: nox.Session):
    '''
    NOT INSTALLED, ie no pip install; test from inside the project root.
    INNER testing, ie we run pytest while PWD is $PYMILK_ROOT (often $HOME/src/pyMilk/)
    ACTUALLY THAT CANNOT WORK, due to uncompiled ImageStreamIO / CacaoProcessTools which gets compiled on install.
    '''
    ...


@nox.session
def tests_editable_inner(session: nox.Session):
    '''
    EDITABLE installation, ie `pip install -e .`; test from inside the project root.
    INNER testing, ie we run pytest while PWD is $PYMILK_ROOT (often $HOME/src/pyMilk/)
    '''
    session.run("./clean.sh", external=True)

    session.install("pytest", "pyright")
    session.install("-e", ".")
    session.install("pyright")

    # Run tests from inside the project root
    session.run("pytest")

    # What about the CacaoProcessTools dependency????
    # We basically need all of milk for that to work...


@nox.session
def tests_editable_outer(session: nox.Session):
    '''
    EDITABLE installation, ie `pip install -e .`; test from outside the project root.
    OUTER testing, ie we run pytest while PWD is NOT $PYMILK_ROOT; here's its /tmp/nox
    '''
    session.run("./clean.sh", external=True)

    session.install("pytest", "pyright")
    session.install("-e", ".")
    session.install("pyright")

    hack_cacaoprocesstools_from_abspath()

    # Run tests from outside the project root
    project_dir = os.path.abspath(os.getcwd())
    with session.chdir("/tmp/nox"):  #TODO maybe
        session.run("pytest", "--pdb", project_dir)


@nox.session
def tests_non_editable_inner_and_outer(session: nox.Session):
    '''
    NON-EDITABLE installation, ie `pip install .`; run from both inside and outside the project root.
    IN and OUT testing
    '''
    session.run("./clean.sh", external=True)

    session.install("pytest")
    session.install(".")

    hack_cacaoprocesstools_from_abspath()

    # Change cwd to something else...
    # Otherwise name collision with the source folder, rather than the install in the venv.
    project_dir = os.path.abspath(os.getcwd())
    with session.chdir("/tmp/nox"):
        session.run("pytest", project_dir)
