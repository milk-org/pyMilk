'''
pyMilk setup.py
'''

from setuptools import setup

with open("README.md", 'r') as f:
    long_description = f.read()

setup(
        name='pyMilk',
        version='0.0.1',
        description='Python bindings and utilities to use with MILK and CACAO',
        long_description=long_description,
        author='Vincent Deo',
        author_email='vdeo@naoj.org',
        url="http://www.github.com/milk-org/pyMilk",
        packages=['pyMilk'],  #same as name
        install_requires=['docopt', 'pyqtgraph'],
        scripts=['pyMilk/visual/shmPlot.py', 'pyMilk/visual/shmImshow.py'])
