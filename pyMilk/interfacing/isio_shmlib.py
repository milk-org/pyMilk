"""
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
"""

try:
    try:  # First shot
        from ImageStreamIOWrap import Image, Image_kw
    except:  # Second shot - maybe you forgot the default path ?
        import sys

        sys.path.append("/usr/local/python")
        from ImageStreamIOWrap import Image, Image_kw
except:
    print("pyMilk.interfacing.isio_shmlib:")
    print("WARNING: did not find ImageStreamIOWrap. Compile or path issues ?")

from typing import Union, Tuple, List, Dict
import numpy as np
import time

from pyMilk.util import img_shapes

import os


class SHM:
    """
        Main interfacing class
    """

    #############################################################
    # CORE COMPATIBILITY WITH xaosim.shmlib
    #############################################################

    def __init__(
            self,
            fname: str,
            data: np.ndarray = None,
            nbkw: int = 0,
            shared: bool = True,
            location: int = -1,
            verbose: bool = False,
            packed=False,
            autoSqueeze: bool = True,
            symcode: int = 4,
            triDim: int = img_shapes.Which3DState.LAST2LAST,
    ) -> None:
        """
        Constructor for a SHM (shared memory) object.

        Parameters:
        ----------
        fname: name of the shm file
                 the resulting name will be $MILK_SHM_DIR/<fname>.im.shm
        data: a numpy array (1, 2 or 3D of data)
                alternatively, a tuple ((int, ...), dtype) is accepted
                    which provides shape and datatype (string or np type)

            WARNING: if data is None, we're reading
                     the userside shape will be found from the C-side shape, the symcode, and the triDim
                     BUT
                     if data isnt None and we're creating - data.shape should be the python shape -- not the c-shape
                     in that case, the C-shape must be found from data.shape

        nbkw: # of keywords to be appended to the data structure (optional)
        shared: True if the memory is shared among users
        location: -1 for CPU RAM, >= 0 provides the # of the GPU.

        autoSqueeze: Remove singleton dimensions between C-side and user side
                     Otherwise, we take the necessary steps to squeeze / pad the singleton dimensions.
                 Warning: [data not None] assumes [autoSqueeze=False].
                     If you're creating a SHM, it's assumed you know what you're passing.
        symcode: symmetry to apply to the data - see pyMilk.util.symcode
        triDim: what to do with tri-dimensional arrays. See util.symcode.


        NOTE:
            The order of operations on read is
                squeeze -> 3D swap -> symcode
            And on write
                symcode -> 3D swap -> unsqueeze

        Depending on whether the file already exists, and/or some new
        data is provided, the file will be created or overwritten.


        CHANGES from xaosim:
            arguments "packed" and "verbose" are unused.
            (this is handled as a compilation flag in ImageStreamIO)

            arguments shared and location added.
        """

        self.IMAGE = Image()
        self.FNAME = _checkSHMName(fname)

        self.semID = None  # type: int
        self.location = location
        self.symcode = (
                symcode  # Handle image symetries; 0-7, see pyMilk.util.img_shapes
        )
        self.triDimState = triDim

        # Image opening for reading
        isCreating = data is not None
        if not isCreating:
            if not self._checkExists():
                raise FileNotFoundError(
                        f"Requested SHM {fname} does not exist")
            # _checkExists already performed the self.IMAGE.open()

            self._checkGrabSemaphore()
            data = self.IMAGE.copy()  # Data is C-side shaped

            self._init_internals_read(data, autoSqueeze)
        else:
            if not isinstance(data, np.ndarray):
                # data is (Shape, type) tuple
                data = np.empty(data[0],
                                dtype=data[1])  # Data is Py-side shaped

            data_c = self._init_internals_creation(data)

            if not self._checkExists():
                print(f"{self.FNAME}.im.shm will be created")
            else:
                print(f"{self.FNAME}.im.shm will be overwritten")
                # _checkExist opened the image, we can destroy.
                self.IMAGE.destroy()
                # Sleep 1/10th of a second - convenience to let streamCTRL re-scan (20 Hz) and detect the destruction
                # before re-creating with the same name - but not the same ID.
                time.sleep(0.1)

            self.IMAGE.create(self.FNAME, data_c, location=location,
                              shared=shared, NBkw=nbkw)

    def _init_internals_read(self, data: np.ndarray, autoSqueeze: bool):
        """
            Aux function for initializing
            data is C-side shape

            Must assign
                self.nptype
                self.nDim
                self.shape_c
                self.shape
        """

        self.nptype = data.dtype
        self.shape_c = data.shape

        # Autosqueeze
        if autoSqueeze:
            self.shape = np.squeeze(data).shape
        else:
            self.shape = self.shape_c

        self.readSlice, self.writeSlice = img_shapes.gen_squeeze_unsqueeze_slices(
                self.shape_c, autoSqueeze)

        # Quick ref
        self.nDim = len(self.shape)

        # 3D ordering
        if self.nDim == 2:
            self.shape = img_shapes.image_decode(data[self.readSlice],
                                                 self.symcode).shape
        elif self.nDim == 3:
            self.shape = img_shapes.full_cube_decode(data[self.readSlice],
                                                     self.symcode,
                                                     self.triDimState).shape

    def _init_internals_creation(self, data):
        """
            Aux function for initializing
            data is python-side shape

            Must assign
                self.nptype
                self.nDim
                self.shape_c
                self.shape
        """

        self.nptype = data.dtype
        self.shape = data.shape
        self.nDim = len(self.shape)

        # Autosqueeze
        if 1 in self.shape:
            print("Warning: ignoring autoSqueeze parameter when creating SHM."
                  "Remove the singleton dimensions yourself.")

        self.readSlice, self.writeSlice = img_shapes.gen_squeeze_unsqueeze_slices(
                self.shape, False)  # Trivial slices - no squeezing anyway

        if self.nDim <= 1:
            data_c = data  # Starting point
        elif self.nDim == 2:
            data_c = img_shapes.image_encode(data, self.symcode)
        elif self.nDim == 3:
            data_c = img_shapes.full_cube_encode(data, self.symcode,
                                                 self.triDimState)

        self.shape_c = data_c.shape
        return data_c

    def rename_img(self, newname: str) -> None:
        raise NotImplementedError()

    def close(self) -> None:
        """
        Clean close of a SHM data structure link
        """
        self.IMAGE.close()

    def read_meta_data(self, verbose: bool = True) -> None:
        '''
            Only for xaosim retro-compatibility
        '''
        if verbose:
            self.print_meta_data()

    def create_keyword_list(self) -> None:
        '''
            Deprecated.
        '''
        raise DeprecationWarning(
                'This function is not used in pyMilk. Use get_keyords/set_keywords.'
        )

    def read_keywords(self) -> None:
        '''
            Deprecated.
        '''
        raise DeprecationWarning(
                'This function is not used in pyMilk. Use get_keyords/set_keywords.'
        )

    def write_keywords(self) -> None:
        '''
            Deprecated.
        '''
        raise DeprecationWarning(
                'This function is not used in pyMilk. Use get_keyords/set_keywords.'
        )

    def read_keyword(self, ii: int) -> None:
        '''
            Deprecated.
        '''
        raise DeprecationWarning(
                'This function is not used in pyMilk. Use get_keyords/set_keywords.'
        )

    def write_keyword(self, ii: int) -> None:
        '''
            Deprecated.
        '''
        raise DeprecationWarning(
                'This function is not used in pyMilk. Use get_keyords/set_keywords.'
        )

    def update_keyword(self, name: str, value: Union[int, str, float],
                       comment: str = None) -> None:
        '''
        Mind the signature change from xaosim: ii (kw index) is not used.
        '''
        kws = self.IMAGE.get_kws_list()
        kws_names = [kw.name for kw in kws]
        if name not in kws_names:
            raise AssertionError('Updating a keyword that does not exist yet.')
        idx = kws_names.index(name)
        if comment is None:  # Keeping prev comment, updating value only
            kws[idx] = Image_kw(name, value, kws[idx].comment)
        else:
            kws[idx] = Image_kw(name, value, comment)
        self.IMAGE.set_kws_list(kws)

    def set_keywords(self, kw_dict: Dict[str, None]):
        '''
        Sets a keyword dictionnary into the SHM
        This is a tentatively non-destructive write

        kw_dict: {key: value}, {key: (value,)} and {key: (value, comment)} are all accepted
        '''
        kws = self.IMAGE.get_kws_list()
        kws_names = [kw.name for kw in kws]

        for name in kw_dict:
            if name in kws_names:
                idx = kws_names.index(name)  # Current location
            else:
                idx = -1  # Append

            if not isinstance(kw_dict[name], tuple):
                v = kw_dict[name]
                c = ''
            elif len(kw_dict[name]) == 1:
                v = kw_dict[name][0]
                c = ''
            else:
                v = kw_dict[name][0]
                c = kw_dict[name][1]

            if idx >= 0:
                kws[idx] = Image_kw(name, v, c)
            else:
                kws.append(Image_kw(name, v, c))

        self.IMAGE.set_kws_list(kws)

    def reset_keywords(self, kw_dict: Dict[str, None]):
        '''
        Sets a keyword dictionnary into the SHM
        This is a complete write - it erases pre-existing keywords

        kw_dict: {key: value}, {key: (value,)} and {key: (value, comment)} are all accepted
        '''
        kws = {}
        for name in kw_dict:
            if not isinstance(kw_dict[name], tuple):
                v = kw_dict[name]
                c = ''
            elif len(kw_dict[name]) == 1:
                v = kw_dict[name][0]
                c = ''
            else:
                v = kw_dict[name][0]
                c = kw_dict[name][1]
            kws[name] = Image_kw(name, v, c)

        self.IMAGE.set_kws(kws)

    def get_keywords(self, comments=False):
        '''
        Return the keyword dictionary from the SHM

        Will return {name: value} if comments is False
        Will return {name: (value, comments)} if comments is True
        '''

        kws = self.IMAGE.get_kws()
        kws_ret = {}
        for name in kws:
            if comments:
                kws_ret[name] = (kws[name].value, kws[name].comment)
            else:
                kws_ret[name] = kws[name].value

        return kws_ret

    def print_meta_data(self) -> None:
        print(self.IMAGE.md)

    def select_dtype(self) -> None:
        raise NotImplementedError()

    def select_atype(self) -> None:
        raise NotImplementedError()

    def get_counter(self) -> int:
        return self.IMAGE.md.cnt0

    def non_block_wait_semaphore(self, sleeptime=0.1):
        self._checkGrabSemaphore()
        self.IMAGE.semflush(self.semID)
        ret = -1
        while ret < 0:
            time.sleep(sleeptime)
            # ret is -1 is semaphore is alive and not posted
            ret = self.IMAGE.semtrywait(self.semID)

        # ret will be 1 (sem destroyed) or 0 (sem posted)
        return ret

    def get_data(
            self,
            check: bool = False,
            reform: bool = True,
            sleepT: float = 0.001,
            timeout: float = 5.0,
            copy: bool = True,
            checkSemAndFlush: bool = True,
    ) -> np.ndarray:
        """
        Reads and returns the data part of the SHM file
        Parameters:
        ----------
        - check: boolean (integer supported); if not False, waits image update
        - copy: boolean, if False returns a np.array pointing to the shm, not a copy.
        - checkSemAndFlush: flush the semaphore to 0, to guarantee SHM
                            was written after this function was called

        CHANGES from xaosim:
            reform, sleepT not implemented
            - reform: use the symcode, autoSqueeze, and triDim constructor arguments (see pyMilk.util.img_shapes for more info)
            - sleepT not useful as we use semaphore waiting now.
        """
        if check:
            if (
                    checkSemAndFlush
            ):  # For irregular operations - we want to bypass this in multi_recv_data
                # Check, flush, and wait the semaphore
                self._checkGrabSemaphore()
                self.IMAGE.semflush(self.semID)
            if timeout is None or timeout <= 0:
                self.IMAGE.semwait(self.semID)
            else:
                err = self.IMAGE.semtimedwait(self.semID, timeout)
                if err != 0:
                    print("Warning - isio_shmlib.SHM.get_data has timed out and returned old data."
                          )

        if self.location >= 0:
            if copy:
                if self.nDim == 2:
                    return img_shapes.image_decode(
                            self.IMAGE.copy()[self.readSlice], self.symcode)
                elif self.nDim == 3:
                    return img_shapes.full_cube_decode(
                            self.IMAGE.copy()[self.readSlice],
                            self.symcode,
                            self.triDimState,
                    )
                else:
                    return self.IMAGE.copy()[self.readSlice]
            else:
                raise AssertionError("copy=False not allowed on GPU.")
        else:
            # This syntax is only allowed on CPU - segfaults if loc > 0
            if self.nDim == 2:
                return img_shapes.image_decode(
                        np.array(self.IMAGE, copy=copy)[self.readSlice],
                        self.symcode)
            elif self.nDim == 3:
                return img_shapes.full_cube_decode(
                        np.array(self.IMAGE, copy=copy)[self.readSlice],
                        self.symcode,
                        self.triDimState,
                )
            else:
                return np.array(self.IMAGE, copy=copy)[self.readSlice]

    def set_data(self, data: np.ndarray, check_dt: bool = False) -> None:
        """
        Upload new data to the SHM file.
        Parameters:
        ----------
        - data: the array to upload to SHM
        - check_dt: boolean (default: false) recasts data
        """
        if check_dt:
            data = data.astype(self.nptype)

        # Handling very specific cases
        # SHM is actually a scalar, autosqueezed to 0 dimensions.
        if self.nDim == 0:
            data = np.array(data)  # A scalar array with () shape

        if self.nDim == 2:
            self.IMAGE.write(
                    img_shapes.image_encode(data,
                                            self.symcode)[self.writeSlice])
        elif self.nDim == 3:
            self.IMAGE.write(
                    img_shapes.full_cube_encode(
                            data, self.symcode,
                            self.triDimState)[self.writeSlice])
        else:
            self.IMAGE.write(data[self.writeSlice])

    def save_as_fits(self, fitsname: str) -> int:
        """
        Convenient sometimes, to be able to export the data as a fits file.

        Parameters:
        ----------
        - fitsname: a filename (overwrite=True)
        """
        from pyMilk.interfacing import fits_lib
        fits_lib.multi_write(fitsname, self.get_data(), symcode=self.symcode,
                             tri_dim=self.triDimState)
        return 0

    #############################################################
    # EXTENSION proposed by xaosim.scexao_shmlib
    #############################################################

    def get_expt(self) -> float:
        """
        Returns exposure time (sec)
        """
        try:
            return self.get_keywords()["tint"]
        except:
            return self.get_keywords()["EXPTIME"]

    def get_fps(self) -> float:
        """
        Returns framerate (Hz)
        """
        try:
            return self.get_keywords()["fps"]
        except:
            return self.get_keywords()["FRATE"]

    def get_ndr(self) -> int:
        """
        Returns NDR status
        """
        return self.get_keywords()["NDR"]

    def get_crop(self) -> Tuple[int, int, int, int]:
        """
        Return image crop boundaries
        """
        x0x1y0y1 = [None, None, None, None]
        old_key = ['x0', 'x1', 'y0', 'y1']
        new_key = ['CROP_OR1', 'CROP_EN1', 'CROP_OR2', 'CROP_EN2']
        for k in range(4):
            try:
                x0x1y0y1[k] = self.get_keywords()[old_key[k]]
            except:
                x0x1y0y1[k] = self.get_keywords()[new_key[k]]
        return np.asarray(x0x1y0y1)

    #############################################################
    # ADDITIONAL FEATURES - NOT SUPPORTED BY xaosim shm STRUCTURE
    #############################################################

    # A handy property to access the self.IMAGE keywords from top-level
    keywords = property(lambda x: x.IMAGE.kw, None)

    def _checkExists(self) -> bool:
        """
        Check if the requeste SHM file is already allocated
        This is called by the constructor when built for writing.

        Returns True if file exists.
        """
        retcode = self.IMAGE.open(self.FNAME)
        return retcode == 0

    def _checkGrabSemaphore(self) -> None:
        """
        Called by constructor in read mode
        Called by synchronous receive functions

        Sets, and returns, semaphore ID
        """
        if self.semID is None:
            self.semID = self.IMAGE.getsemwaitindex(0)

    def multi_recv_data(self, n: int, outputFormat: int = 0,
                        monitorCount: bool = False
                        ) -> Union[List[np.ndarray], np.ndarray]:
        """
        Synchronous read of n successive images in a stream.

        Parameters:
        ----------
        - n: number of frames to read and return
        - outputFormat: flag to indicate what is desired
                        0 for List[np.ndarray]
                        1 for aggregated np.ndarray
        - monitorSem: Monitor and report the counter states when ready to receive - WIP
        """
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
                OUT.append(self.get_data(check=True, checkSemAndFlush=False))
            else:
                OUT[k] = self.get_data(check=True, copy=self.location >= 0,
                                       checkSemAndFlush=False)
            if monitorCount:
                countValues[1, k] = self.IMAGE.md.cnt0

        if monitorCount:
            x = countValues[:, 1:] - countValues[:, :-1]
            p1, p2 = np.sum(x < 1, axis=1), np.sum(x > 1, axis=1)
            y = countValues[1] - countValues[0]
            p3, p4 = np.sum(y < 1), np.sum(y > 1)
            print(f"Irregularities:")
            print(f"{p1[0]} < 1, {p1[1]} > 1 deltas in pre.")
            print(f"{p2[0]} < 1, {p2[1]} > 1 deltas in post.")
            print(f"{p3} < 1, {p4} > 1 pre/post diff.")

        return OUT


def _checkSHMName(fname):
    """
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
    """
    if not "/" in fname:  # filename only
        if fname.endswith(".im.shm"):
            return fname[:-7]
        else:
            return fname

    # It has slashes
    pre, end = os.path.split(fname)
    milk_shm_dir = os.environ["MILK_SHM_DIR"]

    # Third condition is for future use if we allow subdirs
    if not pre.startswith(milk_shm_dir) or (len(pre) > len(milk_shm_dir) and
                                            pre[len(milk_shm_dir)] != "/"):
        raise ValueError(
                f"Only files in MILK_SHR_DIR ({milk_shm_dir}) are supported")

    # Do we need to make folders ? We can't... maybe some day
    if "/" in pre[len(milk_shm_dir):]:
        raise ValueError(
                f"Only files at the root of MILK_SHR_DIR ({milk_shm_dir}) are supported"
        )

    # OK we're good, just filter the extension
    if end.endswith(".im.shm"):
        return end[:-7]
    else:
        return end
