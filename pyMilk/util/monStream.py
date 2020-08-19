from pyMilk.interfacing.isio_shmlib import SHM
import os
import sys
import time
import numpy as np


name = sys.argv[1]
shm = SHM(name)
shm.get_data(1)

N = 10000
timing = np.zeros(N, dtype=np.float32)
outSHM = SHM(name + '_timers', timing)

# s = time.time()
s = shm.IMAGE.md.cnt0
while True:
    for k in range(N):
        shm.IMAGE.semwait(shm.semID)
        # t = time.time()
        t = shm.IMAGE.md.cnt0
        timing[k] = (t - s) #*1e6
        s = t
    outSHM.set_data(timing)


