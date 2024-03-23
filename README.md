# pyMilk

pyMilk means to provide a user-side interaction channel between your python scripts and the shared memory structures used in MILK.
It may also become our preferred repository for a variety of python modules relating to and interacting with the core features of MILK and CACAO (processInfo, FPS).

For interfacing with the MILK streams, we use a [pybind](https://github.com/pybind/pybind11) linking deployed with the C library [ImageStreamIO](https://github.com/milk-org/ImageStreamIO) (ISIO).

ISIO, and the python wrapper, are included here as a submodule.
Credit due to **Arnaud Sevin** for ImageStreamIOWrap.cpp and various upgrades to ISIO.

## Dependencies

There are to ways to go: you already have a MILK install on the target computer, or not.
If you do have an existing MILK install, we're assuming you want to interact with streams from other sources than pyMilk.
The corresponding section guarantees compatibility.

### No existing MILK install

Clone the repo with ISIO as submodule
```
git clone https://github.com/milk-org/pyMilk --recurse-submodules
```

Install pyMilk **in "developer mode"** (`-e` flag; CMake is misbehaved with `pip install .`) install that does not copy stuff in your python install. The source code actually used is the one in the pyMilk folder. If you intend to change python sources, that's way more convenient.

```
cd pyMilk
pip install -e .
```

### You already have MILK somewhere

This is just a suggestion - you may do otherwise.

```
git clone https://github.com/milk-org/pyMilk --recurse-submodules
```

Check the version of ISIO in `$MILK_ROOT/src/ImageStreamIO/ImageStruct.h` and checkout the corresponding pyMilk tag.
If you have the latest ISIO version, you can remain on the master branch head. E.g.:
```
cd pyMilk
git checkout v0.0.01
git submodule update
```
and install
```
pip install -e .
```

NOTE: to avoid source duplication, or if doing active development, you may simply not init the submodule and replace it with a symlink to your existing `$MILK_ROOT/src/ImageStreamIO`.



## Usage

At this early step, we chose to replicate the interface of the purely-python [xaosim.shmlib](https://github.com/fmartinache/xaosim) stream binding.
This way, we provide a one-on-one template for collaborators who are already using the `xaosim` solution for connecting with streams.
All credit to **Frantz Martinache** for defining these interfaces, and for lots of code/documentation copy-pasted into `pyMilk.interfacing.isio_shmlib`.

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

**If your conda-based qt installation is misbehaved:**

This easily happens even in mint-condition anaconda installs... For the viewer `shmImshow.py` we want pyqtgraph 0.12... which requires Qt 5.12+, but conda is (01/2022) locked to Qt 5.9.

No other choice found but to break the `conda` dependency tree:
```
conda remove --force qt pyqt qtconsole qtpy
```
And then use `pip` to work around the issue:
```
pip install pyqt5
```
Which install (Py)Qt 5.15+. There may of course be numerous other ways to go about this (`apt` Qt, etc etc).

### NotImplemented.s

At this point, we're not able to support all the features provided by `xaosim`, and therefore a lot of functions are *here* but are *empty*.
This includes everything related to keyword writing. Keyword reading is supported but unclean.

This will evolve with the feedback of our endeared collaborators regarding what is expected from ISIO(W).

### Contribution

Contributors are welcome to add python contributions, or if they feel up to it, to dig into ISIOW.
Useful python contributions to the interface will be ported deeper into the C for speed and reliability.


### Data ordering

Data ordering is **column-major** by default upon read-write.
A **symcode** parameter enables to switch through all transposed-flipped representations of rectangular images.
The default is 4 as to provide retro-compatibility. We lack, at this point, the adequate compatibility with the fits files.

We hope to be properly dealing with data-ordering at the scale of MILK at some point in the future.
