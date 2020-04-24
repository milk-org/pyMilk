'''
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
'''

import numpy as np


def symcode_encode(im: np.ndarray, s: int) -> np.ndarray:
    # Cases 4 to 7: x and y must be swapped
    if s > 3:
        return symcode_encode(im.T, s - 4)

    if s == 0:
        return im
    elif s == 1:
        return im[::-1, :]
    elif s == 2:
        return im[:, ::-1]
    elif s == 3:
        return im[::-1, ::-1]
    else:
        raise ValueError('symcode s must be 0-7')


def symcode_decode(x: np.ndarray, s: int) -> np.ndarray:
    return symcode_encode(x, [0, 1, 2, 3, 4, 6, 5, 7][s])
