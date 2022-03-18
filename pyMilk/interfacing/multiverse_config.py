'''
    multiverse_config.py

    This file shall define the various versions of milk on the system (two for now, v1.01.01 and v1.01.03) and
    their respective MILK_SHM_DIR variables.

    This file will perform the imports from the .so libraries.

    And define the default.

    Amend paths for your system but don't commit !
'''
'''
    Multiverse rules:

        Explicit name opening: locate the version matching the MILK_SHM_DIR, try open in here.
        Ambiguous opening: try opening in all successive MILK_SHM_DIR

        Explicit creation: locate the version matching the MILK_SHM_DIR, create in there.
        Ambiguous creation: Create in the first environment.
'''

import os, sys
MILK_SHM_DIR_SYSTEM = os.environ['MILK_SHM_DIR']

# Import ISIOWrap for 1.01.03
try:
    try:  # First shot
        from ImageStreamIOWrap import Image, Image_kw
    except:  # Second shot - maybe you forgot the default path ?
        sys.path.append("/usr/local/python")
        from ImageStreamIOWrap import Image, Image_kw
except:
    print("pyMilk.interfacing.isio_shmlib:")
    print("WARNING: did not find ImageStreamIOWrap. Compile or path issues ?")

# Import ISIOWrap for 1.01.01
try:
    try:  # First shot
        from ImageStreamIOWrap_backport import Image as Image_old, Image_kw as Image_kw_old
    except:  # Second shot - maybe you forgot the default path ?
        sys.path.append("/usr/local/python")
        from ImageStreamIOWrap_backport import Image as Image_old, Image_kw as Image_kw_old
except:
    print("pyMilk.interfacing.isio_shmlib:")
    print("WARNING: did not find ImageStreamIOWrap_backport. Compile or path issues ?"
          )

MILK_ENVIRONMENTS = [  # In priority order !
        {
                'SHM_DIR': '/tmp',
                'Image': Image_old,
                'Image_kw': Image_kw_old
        },
        {
                'SHM_DIR': '/milk/shm',
                'Image': Image,
                'Image_kw': Image_kw
        },
]
