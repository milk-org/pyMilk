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
from CacaoProcessTools import fps as CPTFPS

os.sched_setaffinity(0, _CORES)

FPVal: typ.TypeAlias = typ.Union[str, bool, int, float]


class FPS:

    def __init__(self, name: str) -> None:
        self.name = name
        self.fps = CPTFPS(name)
        self.key_types: typ.Dict[str, int] = self.fps.keys

    def __str__(self) -> str:
        return f'{self.name} | CONF: {("N", "Y")[self.conf_isrunning()]} | RUN: {("N", "Y")[self.run_isrunning()]}'

    def _errno_raiser(self, retcode: int, info: str) -> None:
        if retcode != 0:
            raise RuntimeError(
                    f'FPS {self.name}: errno raise code {retcode} with info {info}.'
            )

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
