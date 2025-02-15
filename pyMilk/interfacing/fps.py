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
'''
from __future__ import annotations

import typing as typ

import os
import time
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

    _T_Subclass = typ.TypeVar('_T_Subclass',
                              bound='FPS')  # Any subclass of this class

    @classmethod
    def create(cls: type[_T_Subclass], name: str,
               force_recreate: bool = False) -> _T_Subclass:
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

        fps.post_create_set_defaults()

        return fps

    def post_create_set_defaults(self) -> None:
        '''
        Handle for subclassing
        '''
        return None

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

    def conf_start(self, timeoutsync: float | None = None) -> None:
        self._errno_raiser(self.fps.CONFstart(), 'conf_start')
        if not timeoutsync:
            return
        start = time.time()
        while (not self.conf_isrunning()) and (time.time() - start
                                               < timeoutsync):
            time.sleep(1e-4)

    def conf_stop(self, timeoutsync: float | None = None) -> None:
        self._errno_raiser(self.fps.CONFstop(), 'conf_stop')
        if not timeoutsync:
            return
        start = time.time()
        while self.conf_isrunning() and (time.time() - start < timeoutsync):
            time.sleep(1e-4)

    def run_start(self, timeoutsync: float | None = None) -> None:
        self._errno_raiser(self.fps.RUNstart(), 'run_start')
        if not timeoutsync:
            return
        start = time.time()
        while (not self.run_isrunning()) and (time.time() - start
                                              < timeoutsync):
            time.sleep(1e-4)

    def run_stop(self, timeoutsync: float | None = None) -> None:
        self._errno_raiser(self.fps.RUNstop(), 'run_stop')
        if not timeoutsync:
            return
        start = time.time()
        while (self.run_isrunning()) and (time.time() - start < timeoutsync):
            time.sleep(1e-4)

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

    def __init__(self, fps_name_glob: str = '*',
                 fps_keyword_glob: str = '*') -> None:
        self.fps_cache: typ.Dict[str, FPS] = {}
        self.fps_name_glob = fps_name_glob
        self.fps_keyword_glob = fps_keyword_glob
        self.rescan_all()

    def find_fps(self, name: str) -> FPS:
        if (not name in self.fps_cache or self.fps_cache[name] is None):
            self.fps_cache[name] = FPS(name)  # throws a runtime error

        return self.fps_cache[name]

    def rescan_all(self, fps_name_glob: str | None = None,
                   fps_keyword_glob: str | None = None):

        if fps_name_glob is None:
            fps_name_glob = self.fps_name_glob
        if fps_keyword_glob is None:
            fps_keyword_glob = self.fps_keyword_glob
        import fnmatch

        self.purge_cache()
        system_fps_files = glob.glob(os.environ['MILK_SHM_DIR'] +
                                     f'/{fps_name_glob}.fps.shm')
        for fps_file in system_fps_files:
            self.find_fps(os.path.basename(fps_file).split('.')[0])

        rm_keys = set()
        for name, fpsobj in self.fps_cache.items():
            md = fpsobj.fps.md()
            kw_str = md.kwarray.strip(':')
            if (len(kw_str) == 0 or not any(
                    (fnmatch.fnmatch(kw, fps_keyword_glob)
                     for kw in kw_str.split(':')))):
                rm_keys.add(name)

        for name in rm_keys:
            fps = self.fps_cache.pop(name)
            fps.disconnect()

    def purge_cache(self) -> None:
        for k, v in self.fps_cache.items():
            v.fps.disconnect()
        self.fps_cache = {}

    def set_param(self, fps: str, key: str, value: FPVal) -> None:
        self.find_fps(fps).set_param(key, value)

    def get_param(self, fps: str, key: str) -> FPVal:
        return self.find_fps(fps).get_param(key)

    def conf_start(self, fps: str, timeout: float | None = None) -> None:
        self.find_fps(fps).conf_start(timeout)

    def conf_stop(self, fps: str, timeout: float | None = None) -> None:
        self.find_fps(fps).conf_stop(timeout)

    def run_start(self, fps: str, timeout: float | None = None) -> None:
        self.find_fps(fps).run_start(timeout)

    def run_stop(self, fps: str, timeout: float | None = None) -> None:
        self.find_fps(fps).run_stop(timeout)

    def tmux_start(self, fps: str) -> None:
        self.find_fps(fps).tmux_start()

    def tmux_stop(self, fps: str) -> None:
        self.find_fps(fps).tmux_stop()


class SmartAttributesFPS(FPS):
    '''
    This is meant for FPS that get reused a lot, so that we get static typing

    E.g. define

    class SFPS(SmartAttributesFPS):
        gain: float

    in the class preamble

    and now you may do:

    f = SPFS()

    f.gain # return f['gain'] but correctly hinted to float
    f.gain = 1.0 # pyright no screaming
    f.gain = 'yolo' # pyright screaming,
    although this would completely work at runtime since 'yolo' is a valid FPVal


    Remains the question of the initializer...

    e.g. we would love to pass something like:
    gain: ('Averaging gain', FPS_type.FLOAT64, FPS_flags.DEFAULT_INPUT)

    Am I reinventing named tuples / named dicts?
    '''

    _DICT_METADATA: dict[str, tuple[str, FPS_type, FPS_flags]]

    _T_Subclass = typ.TypeVar(
            '_T_Subclass',
            bound='SmartAttributesFPS')  # Any subclass of this class

    def __new__(cls: type[_T_Subclass], name: str) -> _T_Subclass:
        '''
        We're doing all this work in __new__ so that subclasses can inherit the behavior of
        create once the runtime checks are a go.
        Hol' up.
        Probs we could just work it out in the init...
        but we'd have to delete the FPS created by __new__ in case of non-compliance and aborting
        the __init__... so better in __new__
        '''
        cls._cls_metadata_checks()
        return super(SmartAttributesFPS, cls).__new__(cls)

    @classmethod
    def smartfps_downcast(cls: type[_T_Subclass], fps: FPS) -> _T_Subclass:
        cls._cls_metadata_checks()
        # This actually casts...
        # fps.__class__ = cls
        # Let's prefer making a copy.
        return cls(fps.name)

    @classmethod
    def _cls_metadata_checks(cls) -> None:
        if not hasattr(cls, '_DICT_METADATA'):
            raise SmartFPSInitError(
                    f'Missing _DICT_METADATA to instantiate SmartAttributesFPS subclass {cls.__name__}'
            )
        if not cls._DICT_METADATA.keys() == cls._cls_find_annotations().keys():
            raise SmartFPSInitError(
                    f'__annotations__ and _DICT_METADATA differ to instantiate '
                    f'SmartAttributesFPS subclass {cls.__name__}')
        for param, value_tuple in cls._DICT_METADATA.items():
            if not len(value_tuple) == 3:
                raise SmartFPSInitError(
                        f'_DICT_METADATA tuple not len 3 for {param} to '
                        f'instantiate SmartAttributesFPS subclass {cls.__name__}'
                )
            comment, tipe, flag = value_tuple
            if not (isinstance(comment, str) and isinstance(tipe, FPS_type) and
                    isinstance(flag, FPS_flags)):
                raise SmartFPSInitError(
                        f'_DICT_METADATA tuple type not OK for {param} -'
                        f'- {value_tuple} to instantiate SmartAttributesFPS subclass {cls.__name__}'
                )

    @classmethod
    def _cls_find_annotations(cls: type[_T_Subclass]
                              ) -> typ.Iterable[str]:  # Really a dict_keys
        annotations = cls.__annotations__.copy()
        next_in_mro = cls.__mro__[
                1]  # This will break multiple inheritance if any
        if hasattr(next_in_mro, '_cls_find_annotations'):
            annotations.update(
                    next_in_mro._cls_find_annotations())  # type: ignore

        if '_DICT_METADATA' in annotations:
            del annotations['_DICT_METADATA']

        return annotations

    def _prop_fget(self, prop_name: str):

        def fget(self):
            return FPS.__getitem__(self, prop_name)

        return fget

    def _prop_fset(self, prop_name: str):

        def fset(self, value):
            FPS.__setitem__(self, prop_name, value)

        return fset

    def __init__(self, name: str) -> None:
        super().__init__(name)

        for key, (comment, tipe, flags) in self._DICT_METADATA.items():
            self.add_param(key, comment, tipe, flags)
        '''
        This is for Pyro4 exposes and preventing a __getattribute / __setattribute
        metaprogramming pain

        We wrap DICT_METADATA in properties.
        The underlying data storage is not in hidden attributes, but directly in the FPS
        SHM tree.

        There will come a problem with this whole thing for attributes with dots
        in their names.
        '''
        for key in self._DICT_METADATA:
            #setattr(self, '___' + key, getattr(self, key))
            setattr(self.__class__, key,
                    property(self._prop_fget(key), self._prop_fset(key)))
