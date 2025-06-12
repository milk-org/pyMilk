import os, sys
import nox


def hack_cacaoprocesstools_from_abspath():
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
def tests_editable_inner(session: nox.Session):
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
    session.run("./clean.sh", external=True)

    session.install("pytest", "pyright")
    session.install("-e", ".")
    session.install("pyright")

    hack_cacaoprocesstools_from_abspath()

    # Run tests from outside the project root
    project_dir = os.path.abspath(os.getcwd())
    with session.chdir("/tmp/nox"):
        session.run("pytest", "--pdb", project_dir)


@nox.session
def tests_non_editable_inner_and_outer(session: nox.Session):
    session.run("./clean.sh", external=True)

    session.install("pytest")
    session.install(".")

    hack_cacaoprocesstools_from_abspath()

    # Change cwd to something else...
    # Otherwise name collision with the source folder, rather than the install in the venv.
    project_dir = os.path.abspath(os.getcwd())
    with session.chdir("/tmp/nox"):
        session.run("pytest", project_dir)
