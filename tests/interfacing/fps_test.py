import pytest
import os

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


def test_downcasting(fixt_smart_fps_properties):
    a = fixt_smart_fps_properties
    a.i4 = 12345

    f = fps.FPS(a.name)
    with pytest.raises(AttributeError):
        _ = f.i4

    assert f['i4'] == 12345

    f_as_a = type(a).smartfps_downcast(f)
    assert f_as_a.i4 == 12345
    f_as_a.s = "yolo"
    assert f['s'] == 'yolo'


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


def test_smart_fps_property_types(fixt_smart_fps_properties):
    fps = fixt_smart_fps_properties

    # Test integer properties
    fps.i4 = 42
    assert fps.i4 == 42
    fps.i8 = -9223372036854775807  # Test large int64
    assert fps.i8 == -9223372036854775807

    # Test unsigned integers
    fps.u4 = 4294967295  # Max uint32
    assert fps.u4 == 4294967295
    fps.u8 = 18446744073709551615  # Max uint64
    assert fps.u8 == 18446744073709551615

    # Test floats
    fps.f4 = 3.14159
    assert fps.f4 == pytest.approx(3.14159)
    fps.f8 = 2.718281828459045
    assert fps.f8 == pytest.approx(2.718281828459045)

    # Test string
    fps.s = "Hello FPS!"
    assert fps.s == "Hello FPS!"


def test_smart_fps_type_validation(fixt_smart_fps_properties):
    fps = fixt_smart_fps_properties

    # Test invalid type assignments
    with pytest.raises(ValueError):
        fps.i4 = "not integer-able"
    with pytest.raises(ValueError):
        fps.f4 = "not float-able"

    fps.s = 42
    assert fps.s == '42'  # 'Dis JS or what?


def test_smart_fps_concurrent_access():
    """Test that multiple SmartAttributesFPS instances can coexist"""

    class TestFPS(fps.SmartAttributesFPS):
        value: float
        _DICT_METADATA = {
                'value': ('val', fps.FPS_type.FLOAT32,
                          fps.FPS_flags.DEFAULT_INPUT)
        }

    fps1 = TestFPS.create('fps_test_1')
    fps2 = TestFPS.create('fps_test_2')

    fps1.value = 1.0
    fps2.value = 2.0

    assert fps1.value == pytest.approx(1.0)
    assert fps2.value == pytest.approx(2.0)

    fps1.destroy()
    fps2.destroy()

    # Verify FPS was properly destroyed
    with pytest.raises(fps.FPSDoesntExistError):
        TestFPS('fps_test_1')
    with pytest.raises(fps.FPSDoesntExistError):
        TestFPS('fps_test_2')


def test_fps_filesystem_operations():
    """Test that FPS create/destroy operations are reflected in the filesystem"""

    fps_name = 'test_fps_filesystem_ops'
    fps_path = os.environ[
            'MILK_SHM_DIR'] + f"/{fps_name}.fps.shm"  # Standard FPS file location

    # Ensure FPS doesn't exist before test
    if os.path.exists(fps_path):
        raise RuntimeError(f"FPS file {fps_path} already exists")

    # Test creation
    test_fps = fps.FPS.create(fps_name)
    assert os.path.exists(fps_path), "FPS file was not created"

    # Test destruction
    test_fps.destroy()
    assert not os.path.exists(fps_path), "FPS file was not removed"

    # Verify FPS was properly destroyed
    with pytest.raises(fps.FPSDoesntExistError):
        fps.FPS(fps_name)
