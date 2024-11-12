'''
    Pure python class FPS

    Binds over the CacaoProcessTools pybind binding

    Main purpose is to allow pyroification.

    CacaoProcessTools needs to be compiled and installed:
    - Have all of milk installed
    - cd ${MILK_ROOT}
    - pip install . (with your userland pip)
    - Check the import works

    Alternatively:
    - Compile milk with -dbuild_python_module=ON
    - Add the installdir ${MILK_INSTALLDIR} to your LD_LIBRARY_PATH.

    First version:
        Requires milk more recent than Sep 27, 2023.
        Does not allow creation, only linking.
'''
from __future__ import annotations

import typing as typ

import os
import glob

_CORES = os.sched_getaffinity(0)
from CacaoProcessTools import fps as CPTFPS, FPS_type, FPS_flags

os.sched_setaffinity(0, _CORES)

if typ.TYPE_CHECKING:
    FPVal: typ.TypeAlias = str | bool | int | float


class FPSErrnoError(Exception):
    pass


class FPSDoesntExistError(Exception):
    pass


class SmartFPSInitError(Exception):
    pass


class FPS:

    @classmethod
    def create(cls, name: str, force_recreate: bool = False) -> cls:
        fps_filepath = os.environ['MILK_SHM_DIR'] + f'/{name}.fps.shm'
        if force_recreate and os.path.exists(fps_filepath):
            os.remove(fps_filepath)

        # FIXME we need an API bind. OR AT LEAST USE THE milk-session wrapper
        SHM_DIR = os.environ["MILK_SHM_DIR"]
        os.system(
                f'MILK_SHM_DIR={SHM_DIR} milk-exec "fpscreate 1000 {name} Comment"'
        )
        fps = cls(name)
        fps.add_param('Name', 'Name', FPS_type.STRING,
                      FPS_flags.DEFAULT_STATUS)  # 0x5 = VISIBLE | ACTIVE
        fps['Name'] = name

        return fps

    def __init__(self, name: str) -> None:
        self.name = name
        try:
            self.fps = CPTFPS(name)
        except RuntimeError as exc:
            raise FPSDoesntExistError from exc
        self.key_types: typ.Dict[str, int] = self.fps.keys

    def __str__(self) -> str:
        # FIXME append tmux status
        return f'{self.name} | CONF: {("N", "Y")[self.conf_isrunning()]} | RUN: {("N", "Y")[self.run_isrunning()]}'

    def _errno_raiser(self, retcode: int, info: str) -> None:
        if retcode != 0:
            raise FPSErrnoError(
                    f'FPS {self.name}: errno raise code {retcode} with info {info}.'
            )

    def add_param(self, key: str, comment: str, datatype: int,
                  flags: int = FPS_flags.DEFAULT_INPUT) -> None:
        self.fps.add_entry(key, comment, datatype, flags)
        self.key_types[key] = datatype

    def _destroy(self) -> None:
        # MAKING ME UNHAPPY
        self.fps.un

    def set_param(self, key: str, value: FPVal) -> None:
        if key in self.key_types:
            self.fps[key] = value
        else:
            raise ValueError(f'set_param: key {key} not in FPS {self.name}.')

    def get_param(self, key: str) -> FPVal:
        if key in self.key_types:
            return self.fps[key]
        else:
            raise ValueError(f'get_param: key {key} not in FPS {self.name}.')

    __setitem__ = set_param
    __getitem__ = get_param
    '''
    We don't have that in pyFPS!
    def tmux_isrunning(self) -> bool:
        return self.fps.TMUXrunning
    '''

    def conf_isrunning(self) -> bool:
        return self.fps.CONFrunning == 1

    def run_isrunning(self) -> bool:
        return self.fps.RUNrunning == 1

    def conf_start(self) -> None:
        self._errno_raiser(self.fps.CONFstart(), 'conf_start')

    def conf_stop(self) -> None:
        self._errno_raiser(self.fps.CONFstop(), 'conf_stop')

    def run_start(self) -> None:
        self._errno_raiser(self.fps.RUNstart(), 'run_start')

    def run_stop(self) -> None:
        self._errno_raiser(self.fps.RUNstop(), 'run_stop')

    def tmux_start(self) -> None:
        self._errno_raiser(self.fps.TMUXstart(), 'tmux_start')

    def tmux_stop(self) -> None:
        self._errno_raiser(self.fps.TMUXstop(), 'tmux_stop')

    def disconnect(self) -> None:
        self.fps.disconnect()

    def destroy(self) -> None:
        fps_filepath = os.environ['MILK_SHM_DIR'] + f'/{self.name}.fps.shm'
        os.remove(fps_filepath)


class FPSManager:

    def __init__(self, fps_name_glob: str = '*') -> None:
        self.fps_cache: typ.Dict[str, FPS] = {}
        self.fps_name_glob = fps_name_glob
        self.rescan_all()

    def find_fps(self, name: str) -> FPS:
        if (not name in self.fps_cache or self.fps_cache[name] is None):
            self.fps_cache[name] = FPS(name)  # throws a runtime error

        return self.fps_cache[name]

    def rescan_all(self, fps_name_glob: str | None = None):

        if fps_name_glob is None:
            fps_name_glob = self.fps_name_glob

        self.purge_cache()
        system_fps_files = glob.glob(os.environ['MILK_SHM_DIR'] +
                                     f'/{fps_name_glob}.fps.shm')
        for fps_file in system_fps_files:
            self.find_fps(os.path.basename(fps_file).split('.')[0])

    def purge_cache(self) -> None:
        for k, v in self.fps_cache.items():
            v.fps.disconnect()
        self.fps_cache = {}

    def set_param(self, fps: str, key: str, value: FPVal) -> None:
        self.find_fps(fps).set_param(key, value)

    def get_param(self, fps: str, key: str) -> FPVal:
        return self.find_fps(fps).get_param(key)

    def conf_start(self, fps: str) -> None:
        self.find_fps(fps).conf_start()

    def conf_stop(self, fps: str) -> None:
        self.find_fps(fps).conf_stop()

    def run_start(self, fps: str) -> None:
        self.find_fps(fps).run_start()

    def run_stop(self, fps: str) -> None:
        self.find_fps(fps).run_stop()

    def tmux_start(self, fps: str) -> None:
        self.find_fps(fps).tmux_start()

    def tmux_stop(self, fps: str) -> None:
        self.find_fps(fps).tmux_stop()
