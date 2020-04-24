# pyMilk

pyMilk means to provide a user-side interaction mean to connect your python scripts to the shared memory structures used in Milk.  
It may also become our preferred repository for a variety of python modules relating to and interacting with the core features of MILK and CACAO.

For interfacing with the MILK streams, we use a [pybind](https://github.com/pybind/pybind11) linking deployed with the C library [ImageStreamIO](https://github.com/milk-org/ImageStreamIO) (ISIO).  
This binding, ImageStreamIOWrap (ISIOW), is not included here, but is embedded in the ISIO repository.

## Dependencies

To operate pyMilk using the ISIOW binding, you need to obtain and compile this binding.
This is simply done by compiling ISIO with an additional flag `-Dpython_build=ON`.

If you have the new [milk-package](https://github.com/milk-org/ImageStreamIO), you can just add this flag in the cmake line.

You'll need pybind to configure/compile:
```bash
pip install pybind11
```

The only extra step beyond the milk-package README is to add the ISIOW so file in the PYTHONPATH:
```bash
export PYTHONPATH=$MILK_INSTALLDIR/python:$PYTHONPATH
```

All credit to *A. Sevin* for ImageStreamIOWrap.cpp.

## Installation

A setup.py file is included for convenience and replicability.  
At the repository root, run:

```bash
pip install -e .
```
The `-e` flag will perform a symlink install rather than a copy install. This way, you can develop in the repository and have your changes reflected at once.

## Usage

At this early step, we chose to replicate the interface of the purely-python [xaosim.shmlib](https://github.com/fmartinache/xaosim) stream binding.  
This way, we provide a one-on-one template for collaborators who are already using the `xaosim` solution for connecting with streams.  
All credit to *F. Martinache* for defining these interfaces, and for lots of code/documentation copy-pasted into `pyMilk.interfacing.isio_shmlib`.

For those who have pre-existing scripts:

```python
from xaosim.shmlib import shm [as XXX]
# or
from xaosim.scexao_shmlib import shm [as XXX]
```
should simply be replaced with

```python
from pyMilk.interfacing.isio_shmlib import SHM [as shm] [as XXX]
```


Here is a quick example:

```python
from pyMilk.interfacing.isio_shmlib import SHM

# Reading a shm file
shm = SHM('shm_name')
data = shm.get_data(check=False) # Just read
data = shm.get_data(check=True) # Wait for semaphore udate, then read

# Writing in an existing shm stream
shm.set_data(np.random.rand(*shm.shape).astype(shm.nptype))

# Reading a RT stream
ocam = SHM('ocam2d')
output = ocam.multi_recv_data(10000,
            outputFormat = 1, # Puts everything in a 3D np.array
            monitorCount = True # Prints a synthesis of counters, ie likely missed frames
         )
output.shape # 10000 x 120 x 120
output.dtype # np.uint16

# Creating a brand new stream (e.g. 30x30 int16 images)
shm_wr = SHM('shm_name', data, # 30x30 int16 np.array
             location=-1, # CPU
             shared=True, # Shared
            )
# Or with shape and type
shm_wr = SHM('shm_name', ((30,30), np.int16), ...)
```

## Technical notes

### NotImplementeds

At this point, we're not able to support all the features provided by `xaosim`, and therefore a lot of functions are *here* but are *empty*.  
This includes everything related to keyword writing. Keyword reading is supported but unclean.

This will evolve with the feedback of our endeared collaborators regarding what is expected from ISIO(W).


### Data ordering

Data ordering is column-major by default upon read-write.  
A symcode parameter enable to switch through all transposed-flipped representations of rectangular images.  
The default is 4 as to provide retro-compatibility. We lack, at this point, the adequate compatibility with the fits files.

We hope to be properly dealing with data-ordering at the scale of MILK at some point in the future.

