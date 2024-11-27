import pytest

from pyMilk.interfacing import fps


def test_smart_fps_instantiation():

    class A(fps.SmartAttributesFPS):
        pass

    with pytest.raises(fps.SmartFPSInitError):
        A('uniq_name_A')  # no _DICT_METADATA

    class B(fps.SmartAttributesFPS):
        gain: float

    with pytest.raises(fps.SmartFPSInitError):
        B('uniq_name_B')  # No _DICT_METADATA

    class C(fps.SmartAttributesFPS):
        gain: float
        _DICT_METADATA = {}

    with pytest.raises(fps.SmartFPSInitError):
        C('uniq_name_C')  # 'gain' not in _DICT_METADATA

    class D(fps.SmartAttributesFPS):
        _DICT_METADATA = {}

    assert isinstance(D.create('uniq_name_D'), D)

    class E(fps.SmartAttributesFPS):
        gain: float
        _DICT_METADATA = {'yolo': ()}

    with pytest.raises(fps.SmartFPSInitError):
        E('uniq_name_E')  # Unrelated key in _DICT_METADATA
    with pytest.raises(fps.SmartFPSInitError):
        E.create('uniq_name_E')  # _DICT_METADATA tuple-value noncompliant

    class F(fps.SmartAttributesFPS):
        gain: float
        _DICT_METADATA = {'gain': (1, 2, 3)}

    with pytest.raises(fps.SmartFPSInitError):
        F('uniq_name_F')  # _DICT_METADATA tuple-value noncompliant
    with pytest.raises(fps.SmartFPSInitError):
        F.create('uniq_name_F')  # _DICT_METADATA tuple-value noncompliant

    class G(fps.SmartAttributesFPS):
        gain: float
        _DICT_METADATA = {
                'gain': ('toto', fps.FPS_type.FLOAT32,
                         fps.FPS_flags.DEFAULT_INPUT)
        }

    with pytest.raises(fps.FPSDoesntExistError):
        G('uniq_name_G')  # _DICT_METADATA tuple-value noncompliant
    g = G.create('uniq_name_G')
    assert isinstance(g, G)
    assert isinstance(g.gain, float)

    g.gain = 3.1415
    assert g.gain == pytest.approx(3.1415)


@pytest.fixture
def fixt_dumb_fps():
    with pytest.raises(fps.FPSDoesntExistError):
        a = fps.FPS('uniq_fps')

    a = fps.FPS.create('uniq_fps')

    yield a

    a.destroy()


@pytest.fixture
def fixt_smart_fps_properties():

    class A(fps.SmartAttributesFPS):
        i4: int
        i8: int
        u4: int
        u8: int
        f4: float
        f8: float
        s: str

        _DICT_METADATA = {
                'i4': ('i4', fps.FPS_type.INT32, fps.FPS_flags.DEFAULT_INPUT),
                'i8': ('i8', fps.FPS_type.INT64, fps.FPS_flags.DEFAULT_INPUT),
                'u4': ('u4', fps.FPS_type.UINT32, fps.FPS_flags.DEFAULT_INPUT),
                'u8': ('u8', fps.FPS_type.UINT64, fps.FPS_flags.DEFAULT_INPUT),
                'f4': ('f4', fps.FPS_type.FLOAT32, fps.FPS_flags.DEFAULT_INPUT
                       ),
                'f8': ('f8', fps.FPS_type.FLOAT64, fps.FPS_flags.DEFAULT_INPUT
                       ),
                's': ('s', fps.FPS_type.STRING, fps.FPS_flags.DEFAULT_INPUT)
        }

    with pytest.raises(fps.FPSDoesntExistError):
        a = A('uniq_a')

    a = A.create('uniq_a')

    yield a

    a.destroy()


def test_md(fixt_dumb_fps):
    fps = fixt_dumb_fps
    md = fps.fps.md()
    for tag in md.__dir__():
        if not tag.startswith('_'):
            print(tag, getattr(md, tag))

    assert True


def test_fixture_works(fixt_smart_fps_properties):
    fps = fixt_smart_fps_properties
    assert True


def test_fixture_works_twice(fixt_smart_fps_properties):
    assert True
