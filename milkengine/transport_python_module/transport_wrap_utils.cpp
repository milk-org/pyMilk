#include "transport_wrap_utils.hpp"
#include "transport_enums.h"

const std::vector<uint8_t> ImageStreamIODataType::Size(
{
    0, SIZEOF_DATATYPE_UINT8, SIZEOF_DATATYPE_INT8, SIZEOF_DATATYPE_UINT16,
    SIZEOF_DATATYPE_INT16, SIZEOF_DATATYPE_UINT32, SIZEOF_DATATYPE_INT32,
    SIZEOF_DATATYPE_UINT64, SIZEOF_DATATYPE_INT64, SIZEOF_DATATYPE_FLOAT,
    SIZEOF_DATATYPE_DOUBLE, SIZEOF_DATATYPE_COMPLEX_FLOAT,
    SIZEOF_DATATYPE_COMPLEX_DOUBLE, SIZEOF_DATATYPE_HALF});

void write_to_ptr(void *ptr, InternalStorageEnum req, IMAGE_METADATA &md,
                  nb::ndarray<nb::f_contig> b)
{
    if(ptr == nullptr)
    {
        throw std::runtime_error("image not initialized");
    }

    uint8_t datatype = NdarrayDtypeToImageStreamIODataType(b);
    if(md.datatype != datatype)
    {
        throw std::invalid_argument("incompatible type");
    }
    if((size_t)b.ndim() != (size_t)md.naxis)
    {
        throw std::invalid_argument("incompatible number of axis");
    }
    for(size_t i = 0; i < b.ndim(); ++i)
    {
        if(b.shape(i) != md.size[i])
        {
            throw std::invalid_argument("incompatible shape");
        }
    }

    uint64_t size = md.nelement * ImageStreamIO_typesize(datatype);
    md.write = 1;

    if(b.device_type() == nb::device::cpu::value)    // np array
    {
        if(req == -1)    // np array -> CPU SHM
        {
            memcpy(ptr, b.data(), size);
        }
        else     // np array -> GPU SHM
        {
#ifdef HAVE_CUDA
            cudaSetDevice(req);
            cudaMemcpy(ptr, b.data(), size, cudaMemcpyHostToDevice);

#else
            throw std::runtime_error("unsupported location, needs -DUSE_CUDA=ON");
#endif
        }
    }
    else if(b.device_type() == nb::device::cuda::value)       // cp array
    {
#ifdef HAVE_CUDA
        if(req == -1)     // cp array -> CPU SHM
        {
            cudaMemcpy(ptr, b.data(), size, cudaMemcpyDeviceToHost);
        }
        else      // cp array -> GPU SHM | Handles both same device and peer access.
        {
            cudaMemcpyPeer(ptr, md.location, b.data(), b.device_id(), size);
        }

#else
        throw std::runtime_error("CUDA array passed but needs -DUSE_CUDA=ON");
#endif
    }
    else
    {
        throw std::runtime_error("unsupported device type");
    }
}
