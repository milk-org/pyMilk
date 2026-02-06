'''
pyMilk setup.py
'''

import os
import sys
import platform
import subprocess
from distutils.version import LooseVersion

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

import shlex
import pathlib


class CMakeExtension(Extension):

    def __init__(self, name, package, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir) + '/' + self.name
        self.package = package


class CMakeBuildExt(build_ext):

    def run(self):
        self.inplace = True
        try:
            import pybind11  # Will raise ModuleNotFoundError
            if pybind11.version_info < (2, 11):
                raise ModuleNotFoundError('pybind version nok')
            out = subprocess.check_output(['cmake', '--version'
                                           ])  # Will raise FileNotFoundError
        except:
            raise RuntimeError(
                    "CMake and pybind11 must be installed to build the following extensions: "
                    + ", ".join(e.name for e in self.extensions) +
                    "\n and pybind must be > 2.11 (pip install --upgrade pybind11)"
            )

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        self.announce("Preparing the build environment", level=3)

        extdir = os.path.abspath(
                os.path.dirname(self.get_ext_fullpath(ext.name)))

        build_temp_subdir = self.build_temp + '/' + ext.name

        if self.editable_mode:
            # When running `pip install -e .`
            # extdir = $HOME/src/pyMilk/
            # drop lib in $HOME/src/pyMilk/pyMilk
            lib_drop_in_directory = extdir + '/' + ext.package
        else:
            # self.build_temp is:    build/temp.linux-x86_64-cpython-310
            # build_temp_subdir is:  build/temp.linux-x86_64-cpython-310/ImageStreamIO
            # drop lib in:
            #       build/lib.linux-x86_64-cpython/pyMilk
            build_temp_path = pathlib.Path(os.path.abspath(self.build_temp))
            lib_drop_in_directory = str(build_temp_path.parent /
                                        build_temp_path.name.replace(
                                                'temp', 'lib', 1) /
                                        ext.package)

        os.makedirs(build_temp_subdir, exist_ok=True)

        cmake_args = [
                '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + lib_drop_in_directory,
                '-DPYTHON_EXECUTABLE=' + sys.executable,
        ]

        if os.environ.get('COVERAGE', None) == 'ON':
            cfg = 'Coverage'
        else:
            cfg = 'Debug' if self.debug else 'Release'
        build_args = ['--config', cfg]

        cmake_args += ['-DCMAKE_BUILD_TYPE=' + cfg]
        build_args += ['--', '-j%d' % os.cpu_count()]  #, 'VERBOSE=1']

        if "CUDA_ROOT" in os.environ:
            if os.path.isfile('{}/bin/gcc'.format(os.environ["CUDA_ROOT"])):
                cmake_args += [
                        '-DCMAKE_C_COMPILER={}/bin/gcc'.format(
                                os.environ["CUDA_ROOT"])
                ]
            if os.path.isfile('{}/bin/g++'.format(os.environ["CUDA_ROOT"])):
                cmake_args += [
                        '-DCMAKE_CXX_COMPILER={}/bin/g++'.format(
                                os.environ["CUDA_ROOT"])
                ]
            cmake_args += ['-DUSE_CUDA=ON']
        else:
            cmake_args += ['-DUSE_CUDA=OFF']

        cmake_args += ['-Dbuild_python_module=ON']

        self.announce("Configuring cmake project", level=3)
        command_a = f'cmake {ext.sourcedir} ' + ' '.join(cmake_args)
        command_b = f'cmake --build . ' + ' '.join(build_args)

        # Great way to locate a print-debug quickly in the pip build stack.
        '''
        raise ValueError(
                f'{ext} | {ext.name} | {ext.sourcedir} | command_a = "{command_a}"'
                f' | command _b = "{command_b}" | cmake cwd = {build_temp_subdir}'
        )
        '''

        subprocess.check_call(shlex.split(command_a), cwd=build_temp_subdir)
        subprocess.check_call(shlex.split(command_b), cwd=build_temp_subdir)


with open("README.md", 'r') as f:
    long_description = f.read()

#import sys
#raise ValueError(sys.argv)

setup(
        packages=['pyMilk'],  # same as name
        ext_modules=[
                CMakeExtension('CPTlocal', package='pyMilk'),
                CMakeExtension('ImageStreamIO', package='pyMilk'),
        ],
        cmdclass=dict(build_ext=CMakeBuildExt),
        long_description=long_description)
