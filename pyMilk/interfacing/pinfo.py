from __future__ import annotations

import typing as typ
'''
Trying a new approach here that respects the CPP code more.
I'll define a type stub, and then refer to the bare nanobind import

Technically, we should carry all the docstrings from the nanobind cpp here, since this
is what static analyzers will parse.

TODO: would be much nice if we had a binding of the enums for CTRLval, etc.
'''

if not typ.TYPE_CHECKING:
    from . import glib_loader_fix
    from ..ProcessInfoWrap import processinfo as ProcessInfo
else:

    class timespec:
        sec: int
        nsec: int

    class ProcessInfo:

        @typ.overload
        def __init__(self) -> None:
            ...

        @typ.overload
        def __init__(self, name: str, CTRLval: int) -> None:
            ...

        def __init__(self, name: str = '', CTRLval: int = -1) -> None:
            ...

        def create(self, name: str, CTRLval: int) -> int:
            ...

        def link(self, name: str) -> int:
            ...

        def close(self, name: str = '') -> int:
            ...

        def sigexit(self, sig: int) -> int:
            ...

        def writeMessage(self, message: str) -> int:
            ...

        def exec_start(self) -> int:
            ...

        def exec_end(self) -> int:
            ...

        @property
        def name(self) -> str:
            ...

        @name.setter
        def name(self, value: str) -> None:
            ...

        @property
        def source_function(self) -> str:
            ...

        @source_function.setter
        def source_function(self, value: str) -> None:
            ...

        @property
        def source_FILE(self) -> str:
            ...

        @source_FILE.setter
        def source_FILE(self, value: str) -> None:
            ...

        @property
        def source_LINE(self) -> int:
            ...

        @source_LINE.setter
        def source_LINE(self, value: int) -> None:
            ...

        @property
        def PID(self) -> int:
            ...

        @PID.setter
        def PID(self, value: int) -> None:
            ...

        # TODO: this could just be a pass-by-timestamp.
        @property
        def create_time(self) -> timespec:
            ...

        @create_time.setter
        def create_time(self, value: timespec) -> None:
            ...

        @property
        def loopcnt(self) -> int:
            ...

        @loopcnt.setter
        def loopcnt(self, value: int) -> None:
            ...

        @property
        def CTRLval(self) -> int:
            ...

        @CTRLval.setter
        def CTRLval(self, value: int) -> None:
            ...

        @property
        def tmuxname(self) -> str:
            ...

        @tmuxname.setter
        def tmuxname(self, value: str) -> None:
            ...

        @property
        def loopstat(self) -> int:
            ...

        @loopstat.setter
        def loopstat(self, value: int) -> None:
            ...

        @property
        def statusmsg(self) -> str:
            ...

        @statusmsg.setter
        def statusmsg(self, value: str) -> None:
            ...

        @property
        def statuscode(self) -> int:
            ...

        @statuscode.setter
        def statuscode(self, value: int) -> None:
            ...

        @property
        def description(self) -> str:
            ...

        @description.setter
        def description(self, value: str) -> None:
            ...
