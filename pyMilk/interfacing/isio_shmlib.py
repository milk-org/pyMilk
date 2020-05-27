'''
isio_shmlib.py

Author: V. Deo
Date: April 13, 2020

Offers a python binding to MILK streams.

Is based on the ImageStreamIOWrap (ISIOW) C wrapper to python
which must be compiled and accessible in the PYTHONPATH
(default /usr/local/python is included automatically in this file)

    Your code
        ^
        |
isio_shmlib.py SHM class     (python) <- You are here
        ^
        |
ImageStreamIOWrap.cpp     (pyBind)
        ^
        |
ImagestreamIO.c           (C)
        ^
        |
Shared memory


For previous xaosim.shmlib users:

    Class interface and documentation is plagiarized
    on xaosim's shmlib and scexao_shmlib.
    With a few advantages provided by using ImageStreamIO directly:
    - Evolves naturally with ImageStreamIO updates
    - Natural integration with MILK's streamCTRL
    - Semaphores, metadata and keywords handled by the C.

    Note: Class methods which only affected internal flags in shmlib
          were removed if unnecessary

          Methods deleted:
              create
              increment_counter


          New methods are added a the end of the file, that rely on
          the ImageStreamIOWrap interface and may very well not be
          back-portable to xaosim.

          Methods added:


Credit for the ideas, templates, docstrings, and xaosim.shmlib:
    F. Martinache (www.github.com/fmartinache/xaosim)

Credit for ImageStreamIO C library: O. Guyon

Credit for ImageStreamIOWrap pybind interface: A. Sevin
'''

try:
    try:  # First shot
        from ImageStreamIOWrap import Image, Image_kw
    except:  # Second shot - maybe you forgot the default path ?
        import sys
        sys.path.append('/usr/local/python')
        from ImageStreamIOWrap import Image, Image_kw
except:
    print('pyMilk.interfacing.isio_shmlib:')
    print('WARNING: did not find ImageStreamIOWrap. Compile or path issues ?')

from typing import Union, Tuple, List
import numpy as np
import time

from pyMilk.util.symcode import symcode_encode, symcode_decode

import os


class SHM:
    '''
        Main interfacing class
    '''

    #############################################################
    # CORE COMPATIBILITY WITH xaosim.shmlib
    #############################################################

    def __init__(self, fname: str, data: np.ndarray = None, nbkw: int = 0,
                 shared: bool = True, location: int = -1,
                 verbose: bool = False, packed=False, autoSqueeze: bool = True,
                 symcode: int = 4) -> None:
        '''
        Constructor for a SHM (shared memory) object.

        Parameters:
        ----------
        fname: name of the shm file
                 the resulting name will be $MILK_SHM_DIR/<fname>.im.shm
        data: a numpy array (1, 2 or 3D of data)
                alternatively, a tuple ((int, ...), dtype) is accepted
                    which provides shape and datatype (string or np type)
        nbkw: # of keywords to be appended to the data structure (optional)
        shared: True if the memory is shared among users
        location: -1 for CPU RAM, >= 0 provides the # of the GPU.
        autoSqueeze: Remove singleton dimensions between C-side and user side
                     Otherwise, we take the necessary steps to squeeze / pad the singleton dimensions.
                 Warning: [data not None] assumes [autoSqueeze=False].
                     If you're creating a SHM, it's assumed you know what you're passing.

        Depending on whether the file already exists, and/or some new
        data is provided, the file will be created or overwritten.


        CHANGES from xaosim:
            arguments "packed" and "verbose" are unused.
            (this is handled as a compilation flag in ImageStreamIO)

            arguments shared and location added.
        '''

        self.IMAGE = Image()
        self.FNAME = _checkSHMName(fname)

        self.semID = None  # type: int
        self.location = location
        self.symcode = symcode  # Handle image symetries; 0-7, see pyMilk.util.symcode

        # Image opening for reading
        if data is None:
            if not self._checkExists():
                raise FileNotFoundError(
                        f'Requested SHM {fname} does not exist')
            # _checkExists already performed the self.IMAGE.open()

            self._checkGrabSemaphore()

        # Image creation or re-write
        else:
            if not isinstance(data, np.ndarray):
                # data is (Shape, type) tuple
                data = np.empty(data[0], dtype=data[1])

            if not self._checkExists():
                print(f"{self.FNAME}.im.shm will be created")
            else:
                print(f"{self.FNAME}.im.shm will be overwritten")
                # _checkExist opened the image, we can destroy.
                self.IMAGE.destroy()

            self.IMAGE.create(self.FNAME, data, location=location,
                              shared=shared, NBkw=nbkw)

        dataTmp = self.IMAGE.copy()
        self.nptype = dataTmp.dtype
        self.shape_c = dataTmp.shape

        # Array slices for read/write (autoSqueeze argument)
        self.readSlice = np.s_[...]
        self.writeSlice = np.s_[...]
        if autoSqueeze:
            self.shape = np.squeeze(dataTmp).shape
            self.readSlice = tuple((slice(None, None, None), 0)[n == 1]
                                   for n in self.shape_c)
            self.writeSlice = tuple((slice(None, None, None), None)[n == 1]
                                    for n in self.shape_c)
        else:
            self.shape = self.shape_c

        if len(self.shape) >= 2 and self.symcode >= 4:  # We need a transpose
            self.shape = (self.shape[1], self.shape[0], *self.shape[2:])

    def rename_img(self, newname: str) -> None:
        raise NotImplementedError()

    def close(self) -> None:
        '''
        Clean close of a SHM data structure link
        '''
        self.IMAGE.close()

    def read_meta_data(self, verbose: bool = True) -> None:
        if verbose:
            self.print_meta_data()

    def create_keyword_list(self) -> None:
        raise NotImplementedError()

    def read_keywords(self) -> None:
        raise NotImplementedError()

    def write_keywords(self) -> None:
        raise NotImplementedError()

    def read_keyword(self, ii: int) -> None:
        raise NotImplementedError()

    def update_keyword(self, ii: int, name: str, value: Union[int, str, float],
                       comment: str) -> None:
        raise NotImplementedError()

    def write_keyword(self, ii: int) -> None:
        raise NotImplementedError()

    def print_meta_data(self) -> None:
        print(self.IMAGE.md)

    def select_dtype(self) -> None:
        raise NotImplementedError()

    def select_atype(self) -> None:
        raise NotImplementedError()

    def get_counter(self) -> int:
        return self.IMAGE.md.cnt0

    def get_data(
            self,
            check: bool = False,
            reform: bool = True,
            sleepT: float = 0.001,
            timeout: float = 5.,
            copy: bool = True,
            checkSemAndFlush: bool = True,
    ) -> np.ndarray:
        '''
        Reads and returns the data part of the SHM file
        Parameters:
        ----------
        - check: boolean (integer supported); if not False, waits image update
        - copy: boolean, if False returns a np.array pointing to the shm, not a copy.

        CHANGES from xaosim:
            reform, timeout not implemented
            For reform: use the symcode constructor argument (see pyMilk.util.symcode for more info)
            sleepT not useful as we use semaphore waiting now.
            TBC upon user testing.
        '''
        if check:
            if checkSemAndFlush: # For irregular operations - we want to bypass this in multi_recv_data
                # Check, flush, and wait the semaphore
                self._checkGrabSemaphore()
                self.IMAGE.semflush(self.semID)
            if timeout is None or timeout <= 0:
                self.IMAGE.semwait(self.semID)
            else:
                err = self.IMAGE.semtimedwait(self.semID, timeout)
                if err != 0:
                    print('Warning - isio_shmlib.SHM.get_data has timed out and returned old data.'
                          )

        if self.location >= 0:
            if copy:
                return symcode_encode(self.IMAGE.copy()[self.readSlice],
                                      self.symcode)
            else:
                raise AssertionError('copy=False not allowed on GPU.')
        else:
            # This syntax is only allowed on CPU
            return symcode_encode(
                    np.array(self.IMAGE, copy=copy)[self.readSlice],
                    self.symcode)

    def set_data(self, data: np.ndarray, check_dt: bool = False) -> None:
        '''
        Upload new data to the SHM file.
        Parameters:
        ----------
        - data: the array to upload to SHM
        - check_dt: boolean (default: false) recasts data
        '''
        if check_dt:
            data = data.astype(self.nptype)

        # Handling very specific cases
        # SHM is actually a scalar, autosqueezed to 0 dimensions.
        if len(self.shape) == 0:
            data = np.array(data)  # A scalar array with () shape
        self.IMAGE.write(symcode_decode(data[self.writeSlice], self.symcode))

    def save_as_fits(self, fitsname: str) -> int:
        '''
        Convenient sometimes, to be able to export the data as a fits file.

        Parameters:
        ----------
        - fitsname: a filename (overwrite=True)
        '''
        try:
            import astropy.io.fits as pf  # type: ignore
            pf.writeto(fitsname, self.get_data(), overwrite=True)
        except:
            import pyfits as pf  # type: ignore
            pf.writeto(fitsname, self.get_data(), clobber=True)
        return 0

    #############################################################
    # EXTENSION proposed by xaosim.scexao_shmlib
    #############################################################

    def get_expt(self) -> float:
        '''
        Returns exposure time (sec)
        '''
        return self.keywords['tint'].value

    def get_fps(self) -> float:
        '''
        Returns framerate (Hz)
        '''
        return self.keywords['fps'].value

    def get_ndr(self) -> int:
        '''
        Returns NDR status
        '''
        return self.keywords['NDR'].value

    def get_crop(self) -> Tuple[int, int, int, int]:
        '''
        Return image crop boundaries
        '''
        return (self.keywords['x0'].value, self.keywords['x1'].value,
                self.keywords['y0'].value, self.keywords['y1'].value)

    #############################################################
    # ADDITIONAL FEATURES - NOT SUPPORTED BY xaosim shm STRUCTURE
    #############################################################

    # A handy property to access the self.IMAGE keywords from top-level
    keywords = property(lambda x: x.IMAGE.kw, None)

    def _checkExists(self) -> bool:
        '''
        Check if the requeste SHM file is already allocated
        This is called by the constructor when built for writing.

        Returns True if file exists.
        '''
        retcode = self.IMAGE.open(self.FNAME)
        return retcode == 0

    def _checkGrabSemaphore(self) -> None:
        '''
        Called by constructor in read mode
        Called by synchronous receive functions

        Sets, and returns, semaphore ID
        '''
        if self.semID is None:
            self.semID = self.IMAGE.getsemwaitindex(0)

    def multi_recv_data(self, n: int, outputFormat: int = 0,
                        monitorCount: bool = False
                        ) -> Union[List[np.ndarray], np.ndarray]:
        '''
        Synchronous read of n successive images in a stream.

        Parameters:
        ----------
        - n: number of frames to read and return
        - outputFormat: flag to indicate what is desired
                        0 for List[np.ndarray]
                        1 for aggregated np.ndarray
        - monitorSem: Monitor and report the counter states when ready to receive - WIP
        '''
        # Prep output
        if outputFormat == 0:
            OUT = []  # type: Union[np.ndarray, List[np.ndarray]]
        else:
            OUT = np.zeros((n, *self.shape), dtype=self.nptype)

        if monitorCount:
            countValues = np.zeros((2, n), dtype=np.uint64)

        # Check and flush the semaphore
        self._checkGrabSemaphore()
        self.IMAGE.semflush(self.semID)

        for k in range(n):
            if monitorCount:
                countValues[0, k] = self.IMAGE.md.cnt0

            if outputFormat == 0:
                OUT.append(self.get_data(check=True, checkSemAndFlush = False))
            else:
                OUT[k] = self.get_data(check=True, copy=self.location >= 0, checkSemAndFlush = False)
            if monitorCount:
                countValues[1, k] = self.IMAGE.md.cnt0

        if monitorCount:
            x = countValues[:, 1:] - countValues[:, :-1]
            p1, p2 = np.sum(x < 1, axis=1), np.sum(x > 1, axis=1)
            y = countValues[1] - countValues[0]
            p3, p4 = np.sum(y < 1), np.sum(y > 1)
            print(f'Irregularities:')
            print(f'{p1[0]} < 1, {p1[1]} > 1 deltas in pre.')
            print(f'{p2[0]} < 1, {p2[1]} > 1 deltas in post.')
            print(f'{p3} < 1, {p4} > 1 pre/post diff.')

        return OUT


def _checkSHMName(fname):
    '''
    Check if the name provided in the constructor is
    - a string in ISIO style
    - a full path + filename + in.shm in xaosim style

    Parameters:
    - fname: the string to check

    Returns:
    The cleaned, ISIO formatted file name (no extension)

    Raises:
    Errors if it's a path-like name that
    1 / does not fit in $MILK_SHM_DIR
    2 / does not end with .im.shm
    '''
    if not '/' in fname:  # filename only
        if fname.endswith('.im.shm'):
            return fname[:-7]
        else:
            return fname

    # It has slashes
    pre, end = os.path.split(fname)
    milk_shm_dir = os.environ['MILK_SHM_DIR']

    # Third condition is for future use if we allow subdirs
    if not pre.startswith(milk_shm_dir) or (len(pre) > len(milk_shm_dir) and
                                            pre[len(milk_shm_dir)] != '/'):
        raise ValueError(
                f'Only files in MILK_SHR_DIR ({milk_shm_dir}) are supported')

    # Do we need to make folders ? We can't... maybe some day
    if '/' in pre[len(milk_shm_dir):]:
        raise ValueError(
                f'Only files at the root of MILK_SHR_DIR ({milk_shm_dir}) are supported'
        )

    # OK we're good, just filter the extension
    if end.endswith('.im.shm'):
        return end[:-7]
    else:
        return end
