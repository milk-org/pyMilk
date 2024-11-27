"""
Management of image symcodes

Symcodes are the eight possibilities
regarding how an image is represented and corresponds to other
images.
They represent the 8 isometries making a square invariant.

Assume your image is:
  _
 |_
 |

It becomes, through symcode transforms:

  0     1    2    3     4     5      6     7
  _     _                          _ _    _ _
 |_     _|  |_   _|    _|_|  |_|_   | |  | |
 |       |  |_   _|

Inverses:
  0     1    2    3     4     6      5     7
  Symcodes are self-inverted EXCEPT 5 and 6.

How you actually want your data stored depends on
your memory structure & writing order, programming language, etc.

Mainly, there are two conventions:
"Image"
First index axis is x and should be horizontal, left-to-right
Second index is y and vertical, bottom-to-top.

This is good for ovelaying xy plots on an image

"Matrix"
First axis is row and is represented vertical, top-to-bottom/
Hence it becomes "-y" on display
Second axis is column and left-to-right, hence "+x"

This is good for not ovethinking indexing in your code.
"""
from __future__ import annotations

import numpy as np
import numpy.typing as npt
import typing as typ


def image_encode(im: npt.NDArray, s: int) -> npt.NDArray:
    # Cases 4 to 7: x and y must be swapped
    if s > 3 and s <= 7:
        return image_encode(im.T, s - 4)

    if s == 0:
        return im
    elif s == 1:
        return im[::-1, :]
    elif s == 2:
        return im[:, ::-1]
    elif s == 3:
        return im[::-1, ::-1]
    else:
        raise ValueError("symcode s must be 0-7")


def image_decode(x: npt.NDArray, s: int) -> npt.NDArray:
    return image_encode(x, [0, 1, 2, 3, 4, 6, 5, 7][s])


def cube_front_image_encode(cube: npt.NDArray, s: int) -> npt.NDArray:
    """
    Encode image cube - dim 0 is the cube axis
    """
    if s > 3:
        return cube_front_image_encode(np.swapaxes(cube, 1, 2), s - 4)

    if s == 0:
        return cube
    elif s == 1:
        return cube[:, ::-1, :]
    elif s == 2:
        return cube[:, :, ::-1]
    elif s == 3:
        return cube[:, ::-1, ::-1]
    else:
        raise ValueError("symcode s must be 0-7")


def cube_front_image_decode(cube: npt.NDArray, s: int) -> npt.NDArray:
    return cube_front_image_encode(cube, [0, 1, 2, 3, 4, 6, 5, 7][s])


def cube_back_image_encode(cube: npt.NDArray, s: int) -> npt.NDArray:
    """
    Encode image cube - dim 2 is the cube axis
    """
    if s > 3:
        return cube_back_image_encode(np.swapaxes(cube, 0, 1), s - 4)

    if s == 0:
        return cube
    elif s == 1:
        return cube[::-1, :, :]
    elif s == 2:
        return cube[:, ::-1, :]
    elif s == 3:
        return cube[::-1, ::-1, :]
    else:
        raise ValueError("symcode s must be 0-7")


def cube_back_image_decode(cube: npt.NDArray, s: int) -> npt.NDArray:
    return cube_back_image_encode(cube, [0, 1, 2, 3, 4, 6, 5, 7][s])


"""
Squeezing and unsqueezing with numpy slices
"""

if typ.TYPE_CHECKING:
    from types import EllipsisType
    T_ReadSlice = EllipsisType | tuple[slice | typ.Literal[0], ...]
    T_WriteSlice = EllipsisType | tuple[slice | None, ...]
    T_RW_Slices = tuple[T_ReadSlice, T_WriteSlice]


def gen_squeeze_unsqueeze_slices(shape_orig: tuple[int, ...],
                                 autoSqueeze: bool) -> T_RW_Slices:
    """
        I do NOT bother to check if shape_dest actually describes a squeezing of shape_orig
    """

    if not autoSqueeze:
        return np.s_[...], np.s_[...]

    read_slice = tuple((slice(None) if n != 1 else 0 for n in shape_orig))
    write_slice = tuple((slice(None) if n != 1 else None for n in shape_orig))
    return read_slice, write_slice  # type: ignore -- inference on read_slice is incorrect.


"""
3D swaps

The "Stack axis" is the one that defines the time, etc. or any other counting property
It's usually at the end in fits.
Python likes it at the beginning

There's are two states: stack-first or stack-last
Conversion is a simple axis roll.

I define the roll statuses - what to expect and what to return regarding the stack axis position
"""


class Which3DRoll:
    ROLL_FORW = 0
    ROLL_BACK = 1


class Which3DState:
    LAST2LAST = 0
    LAST2FRONT = 1
    FRONT2FRONT = 2
    FRONT2LAST = 3


def cube_roll_forw(cube: np.ndarray) -> np.ndarray:
    return np.moveaxis(cube, 2, 0)


def cube_roll_back(cube: np.ndarray) -> np.ndarray:
    return np.moveaxis(cube, 0, 2)


def full_cube_encode(cube: np.ndarray, s: int, tri_dim: int) -> np.ndarray:
    if tri_dim in [Which3DState.FRONT2FRONT, Which3DState.FRONT2LAST]:
        cube = cube_front_image_encode(cube, s)
    else:
        cube = cube_back_image_encode(cube, s)

    if tri_dim == Which3DState.LAST2FRONT:
        cube = cube_roll_forw(cube)
    elif tri_dim == Which3DState.FRONT2LAST:
        cube = cube_roll_back(cube)

    return cube


def full_cube_decode(cube: np.ndarray, s: int, tri_dim: int) -> np.ndarray:
    if tri_dim == Which3DState.LAST2FRONT:
        cube = cube_roll_back(cube)
    elif tri_dim == Which3DState.FRONT2LAST:
        cube = cube_roll_forw(cube)

    if tri_dim in [Which3DState.FRONT2LAST, Which3DState.FRONT2FRONT]:
        cube = cube_front_image_decode(cube, s)
    else:
        cube = cube_back_image_decode(cube, s)

    return cube
