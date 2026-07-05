from __future__ import annotations

from . import glib_loader_fix

from pyMilk.ImageStreamIOWrap import Image_md
from pyMilk.TransportWrap import ComputeUnit, InternalStorageEnum, TransportTypeEmitEnum, TransportTypeRecvEnum


class CopybackComputeUnit:

    def __init__(self, name: str, recv_proto_string: str,
                 recv_memloc: InternalStorageEnum, emit_proto_string: str,
                 emit_memloc: str):
        self._internal = ComputeUnit(name, recv_proto_string, recv_memloc,
                                     emit_proto_string, emit_memloc)

    def loop(self, n: int = -1):
        '''
        Loop until interrupted
        '''
        if n >= 0:
            for _ in range(n):
                self._internal.loop_once()
        if n == -1:
            while True:
                self._internal.loop_once()
