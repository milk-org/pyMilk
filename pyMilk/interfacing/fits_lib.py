"""
    Fits manipulation routines

    with symcode and 3D axis management
    As in shmlib, convention is to perform decode on read and encode on write

    TODO: write tests
"""

import astropy.io.fits as pf
from typing import Union, Iterable

from pyMilk.interfacing.isio_shmlib import SHM
from pyMilk.util import img_shapes

import numpy as np


def multi_read(file_path: str, symcode: int = 0,
               tri_dim: int = img_shapes.Which3DState.LAST2LAST):
    """
        Handle reading of tuples in fits files
    """
    hdu_list = pf.open(file_path, mmap=False)
    if len(hdu_list) == 1:
        data = hdu_list[0].data.copy()
        n_dim = len(data.shape)
        if n_dim == 2:
            data = img_shapes.image_decode(data, symcode)
        elif n_dim == 3:
            data = img_shapes.full_cube_decode(data, symcode, tri_dim)
    else:
        data = [hh.data.copy() for hh in hdu_list]
        data = tuple(data)

    # Cleanup
    hdu_list.close()
    for hh in hdu_list:
        del hh.data

    return data


def multi_write(
        file_path: str,
        data: Union[Iterable[np.ndarray], np.ndarray],
        symcode: int = 0,
        tri_dim: int = img_shapes.Which3DState.LAST2LAST,
):
    """
        Handle saving of tuples in fits files
        TODO: symcode and 3Dord handlers
    """
    if isinstance(data, np.ndarray):
        n_dim = len(data.shape)
        if n_dim == 2:
            data = img_shapes.image_encode(data, symcode)
        elif n_dim == 3:
            data = img_shapes.full_cube_encode(data, symcode, tri_dim)
        data = (data, )

    headers = [pf.ImageHDU(m) for m in data]
    headers[0] = pf.PrimaryHDU(data[0])
    hdu_list = pf.HDUList(headers)
    hdu_list.writeto(file_path, overwrite=True)


def read_fits_load_shm(file_path: str, shm_name: str, symcode: int = 0,
                       tri_dim: int = img_shapes.Which3DState.LAST2LAST,
                       create_shm: bool = False):
    """
        Grab a fits file, load it into the given SHM
        Only a single ndarray per fit - no tuples

        Will fail on size mismatches
    """
    data = multi_read(file_path, symcode, tri_dim)

    if create_shm:
        shm = SHM(shm_name, data, symcode=symcode, triDim=tri_dim, location=0,
                  shared=1)
    else:
        shm = SHM(shm_name)
    shm.set_data(data)

    return shm


def shm_to_fits(shm_obj_or_name: Union[SHM, str], file_path: str):
    """
        Get a shm name or object, get the data, write into a fits file
        TODO: symcode and 3Dord handlers
    """
    if isinstance(shm_obj_or_name, str):
        shm = SHM(shm_obj_or_name)

    shm_obj_or_name.save_as_fits()
