'''
pyMilk setup.py
'''

import os
import re
import sys
import platform
import subprocess

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):

    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):

    def run(self):
        try:
            out = subprocess.check_output(['cmake', '--version'])
        except OSError:
            raise RuntimeError(
                    "CMake must be installed to build the following extensions: "
                    + ", ".join(e.name for e in self.extensions))

        if platform.system() == "Windows":
            raise RuntimeError("Windows is not supported")

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):

        extdir = os.path.abspath(
                os.path.dirname(self.get_ext_fullpath(ext.name)))
        cmake_args = [
                '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + extdir,
                '-DPYTHON_EXECUTABLE=' + sys.executable,
                '-DUSE_CUDA=ON',
        ]

        cfg = 'Debug' if self.debug else 'Release'
        build_args = ['--config', cfg]

        if platform.system() == "Windows":
            cmake_args += [
                    '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{}={}'.format(
                            cfg.upper(), extdir)
            ]
            if sys.maxsize > 2**32:
                cmake_args += ['-A', 'x64']
            build_args += ['--', '/m']
        else:
            cmake_args += ['-DCMAKE_BUILD_TYPE=' + cfg]
            build_args += ['--', '-j2']

        env = os.environ.copy()
        env['CXXFLAGS'] = '{} -DVERSION_INFO=\\"{}\\"'.format(
                env.get('CXXFLAGS', ''), self.distribution.get_version())
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)

        cmake_full_line = [
                'cmake', f'-S{ext.sourcedir}', f'-B{self.build_temp}'
        ] + cmake_args
        subprocess.check_call(cmake_full_line, env=env)
        cmake_full_line = ['cmake', '--build', self.build_temp] + build_args
        subprocess.check_call(cmake_full_line, env=env)
        # INFO: if debugging, do not allocate stdin, stdout, stderr in the subprocess calls.
        # through the pip toolchains, it results in OSErrors bad file descriptor.


with open("README.md", 'r') as f:
    long_description = f.read()

setup(
        name='pyMilk',
        version='1.01',
        description='Python bindings and utilities to use with MILK and CACAO',
        long_description=long_description,
        author='Vincent Deo',
        author_email='vdeo@naoj.org',
        url="http://www.github.com/milk-org/pyMilk",
        packages=['pyMilk'],  # same as name
        install_requires=['docopt', 'pyqtgraph', 'pybind11', 'numpy'],
        setup_requires=['pybind11>=2.11'],
        ext_modules=[CMakeExtension('ImageStreamIO')],
        cmdclass=dict(build_ext=CMakeBuild),
        scripts=[
                'pyMilk/visual/shmPlot.py',
                'pyMilk/visual/shmImshow.py',
                'pyMilk/scripts/zmq_send.py',
                'pyMilk/scripts/zmq_recv.py',
                'pyMilk/scripts/updatekw',
                'pyMilk/scripts/creashmim',
                'pyMilk/visual/shmTermshow.py',
        ])
