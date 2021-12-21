import os
import glob

from .isio_shmlib import SHM, check_SHM_name
from pyMilk.util import img_shapes

import numpy as np

from typing import Tuple

MILK_SHM_DIR = os.environ['MILK_SHM_DIR']


def creashmim(
        name: str,
        shape: Tuple[int],
        data_type: type = np.float32,
        nb_kw: int = 50,
        symcode: int = 0,  # Should it be 4 ??
        tri_dim: int = img_shapes.Which3DState.LAST2LAST,
        delete_existing: bool = False,
        attempt_reuse: bool = True) -> SHM:
    '''
        See isio_shmlib.py

        Note attempt_reuse intervenes before delete_existing
    '''

    shm_handle = None

    if attempt_reuse:
        try:
            # Errors upon non-existence
            shm_handle = SHM(name, symcode=symcode, triDim=tri_dim)
            # Errors upon wrong shape or size
            shm_handle.set_data(np.zeros(shape, dtype=data_type))
            # Errors upon lack of keyword space
            if shm_handle.IMAGE.md.NBkw < nb_kw:
                raise ValueError(
                        "Existing SHM overwrite due to not enough kw space.")
            # Success in reopening !
            return shm_handle
        except:
            print(f"creashmim {name}: attempt_reuse failed.")

    name_no_ext = check_SHM_name(name)

    if delete_existing:
        try:
            os.remove(f"{MILK_SHM_DIR}/{name_no_ext}.im.shm")
        except FileNotFoundError:
            pass
        try:
            for f in glob.glob(
                    f"/dev/shm/sem..{MILK_SHM_DIR.replace('/', '.')}.{name_no_ext}_sem*"
            ):
                os.remove(f)
        except FileNotFoundError:
            pass

    shm_handle = SHM(name, (shape, data_type), nbkw=nb_kw, symcode=symcode,
                     triDim=tri_dim)

    return shm_handle
