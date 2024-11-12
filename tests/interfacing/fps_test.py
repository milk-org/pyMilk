import pytest

from pyMilk.interfacing import fps


def test_smart_fps_instantiation(fixture_change_MILK_SHM_DIR):

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
