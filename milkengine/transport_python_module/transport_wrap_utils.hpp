#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/vector.h>

#include "transport_enums.h"
#include "ImageStreamIO/ImageStreamIO.h"

namespace nb = nanobind;

/*
copy_from_ptr and view_from_ptr NEED to live in the hpp file
so that templated versions are available for any translation unit
#include-ing this file.
*/

template <typename T>
nb::object copy_from_ptr(void *ptr, InternalStorageEnum req,
                         const IMAGE_METADATA &md)
{
    if(ImageStreamIO_typesize(md.datatype) != sizeof(T))
    {
        throw std::runtime_error("IMAGE is not compatible with output format");
    }

    size_t nelement = md.nelement;
    T *data = new T[nelement];

    if(req == -1)
    {
        memcpy(data, ptr, nelement * sizeof(T));
    }
    else
    {
#ifdef HAVE_CUDA
        cudaSetDevice(req);
        cudaMemcpy(data, ptr, nelement * sizeof(T),
                   cudaMemcpyDeviceToHost);
#else
        delete[] data;
        throw std::runtime_error(
            "unsupported location, CACAO needs to be compiled with -DUSE_CUDA=ON");
#endif
    }

    nb::capsule owner(data, [](void *p) noexcept
    {
        delete[](T *)p;
    });

    std::vector<size_t> shape(md.naxis);
    for(int8_t axis = 0; axis < md.naxis; ++axis)
    {
        shape[axis] = md.size[axis];
    }

    // Always return a CPU copy
    return nb::cast(nb::ndarray<nb::numpy, T>(
                        data, md.naxis, shape.data(), owner, nullptr, nb::dtype<T>(),
                        nb::device::cpu::value, 0, 'F'));
}

template <typename T>
nb::object view_from_ptr(void *ptr, InternalStorageEnum req,
                         const IMAGE_METADATA &md)
{
    if(ImageStreamIO_typesize(md.datatype) != sizeof(T))
    {
        throw std::runtime_error("IMAGE is not compatible with output format");
    }

    std::vector<size_t> shape(md.naxis);
    for(int8_t axis = 0; axis < md.naxis; ++axis)
    {
        shape[axis] = md.size[axis];
    }

    // No-op capsule: shared memory is externally managed
    nb::capsule owner((void *)ptr, [](void *) noexcept {});

    // Return a CPU or a GPU view!
    if(req == -1)
    {
        return nb::cast(nb::ndarray<nb::numpy, T>(
                            (T *)ptr, md.naxis, shape.data(), owner, nullptr,
                            nb::dtype<T>(), nb::device::cpu::value, 0, 'F'));
    }
    else
    {
#ifdef HAVE_CUDA
        return nb::cast(nb::ndarray<nb::cupy, T>(
                            (T *)ptr, md.naxis, shape.data(), owner, nullptr,
                            nb::dtype<T>(), nb::device::cuda::value, 0, 'F'));
#else
        throw std::runtime_error("location >= 0 in view but HAVE_CUDA is not set.");
#endif
    }
}

void write_to_ptr(void* ptr, InternalStorageEnum req, IMAGE_METADATA &md, nb::ndarray<nb::f_contig> b);


struct ImageStreamIODataType {
  enum DataType : uint8_t {
    UINT8 = _DATATYPE_UINT8,
    INT8 = _DATATYPE_INT8,
    UINT16 = _DATATYPE_UINT16,
    INT16 = _DATATYPE_INT16,
    UINT32 = _DATATYPE_UINT32,
    INT32 = _DATATYPE_INT32,
    UINT64 = _DATATYPE_UINT64,
    INT64 = _DATATYPE_INT64,
    FLOAT = _DATATYPE_FLOAT,
    DOUBLE = _DATATYPE_DOUBLE,
    COMPLEX_FLOAT = _DATATYPE_COMPLEX_FLOAT,
    COMPLEX_DOUBLE = _DATATYPE_COMPLEX_DOUBLE,
    HALF = _DATATYPE_HALF
  };
  static const std::vector<uint8_t> Size;

  DataType datatype;
  uint8_t asize;

  ImageStreamIODataType() : datatype(FLOAT), asize(Size[FLOAT]){};

  ImageStreamIODataType(uint8_t datatype)
      : datatype(static_cast<ImageStreamIODataType::DataType>(datatype)),
        asize(Size[datatype]){};

  operator uint8_t() const { return datatype; }
};

// Helper: map ndarray dtype to ImageStreamIO datatype
template <typename... Args>
uint8_t NdarrayDtypeToImageStreamIODataType(const nb::ndarray<Args...> &arr) {
  auto dt = arr.dtype();
  if (dt == nb::dtype<uint8_t>()) return _DATATYPE_UINT8;
  if (dt == nb::dtype<int8_t>()) return _DATATYPE_INT8;
  if (dt == nb::dtype<uint16_t>()) return _DATATYPE_UINT16;
  if (dt == nb::dtype<int16_t>()) return _DATATYPE_INT16;
  if (dt == nb::dtype<uint32_t>()) return _DATATYPE_UINT32;
  if (dt == nb::dtype<int32_t>()) return _DATATYPE_INT32;
  if (dt == nb::dtype<uint64_t>()) return _DATATYPE_UINT64;
  if (dt == nb::dtype<int64_t>()) return _DATATYPE_INT64;
  if (dt == nb::dtype<float>()) return _DATATYPE_FLOAT;
  if (dt == nb::dtype<double>()) return _DATATYPE_DOUBLE;
  throw std::runtime_error(
      "NdarrayDtypeToImageStreamIODataType -- Not implemented datatype");
}
