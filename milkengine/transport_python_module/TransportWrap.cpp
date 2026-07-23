#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include "transport_emit_isio.hpp"
#include "transport_emit_milktcp.hpp"
#include "transport_emit_zmqpub.hpp"
#include "transport_recv_isio.hpp"
#include "transport_recv_milktcp.hpp"
#include "transport_recv_zmqsub.hpp"
#include "test_compute_unit.hpp"

#include "transport_wrap_utils.hpp"

namespace nb = nanobind;
using namespace nanobind::literals;

NB_MODULE(TransportWrap, m)
{
    m.doc() = "TransportWrap library module";

    nb::class_<timespec>(m, "timespec")
    .def(nb::init<time_t, long>())
    .def_rw("tv_sec", &timespec::tv_sec)
    .def_rw("tv_nsec", &timespec::tv_nsec);

    nb::enum_<InternalStorageEnum>(m, "InternalStorageEnum")
    .value("UNINITIALIZED", InternalStorageEnum::UNINITIALIZED)
    .value("CPU_MEMORY",   InternalStorageEnum::CPU_MEMORY)
    .value("GPU0_MEMORY",  InternalStorageEnum::GPU0_MEMORY)
    .value("GPU1_MEMORY",  InternalStorageEnum::GPU1_MEMORY)
    .value("GPU2_MEMORY",  InternalStorageEnum::GPU2_MEMORY)
    .value("GPU3_MEMORY",  InternalStorageEnum::GPU3_MEMORY)
    .value("GPU4_MEMORY",  InternalStorageEnum::GPU4_MEMORY)
    .value("GPU5_MEMORY",  InternalStorageEnum::GPU5_MEMORY)
    .value("GPU6_MEMORY",  InternalStorageEnum::GPU6_MEMORY)
    .value("GPU7_MEMORY",  InternalStorageEnum::GPU7_MEMORY);

    nb::enum_<TransportTypeEmitEnum>(m, "TransportTypeEmitEnum")
    .value("EMITTYPE_ISIO",   TransportTypeEmitEnum::EMITTYPE_ISIO)
    .value("EMITTYPE_ZMQPUB", TransportTypeEmitEnum::EMITTYPE_ZMQPUB);

    nb::enum_<TransportTypeRecvEnum>(m, "TransportTypeRecvEnum")
    .value("RECVTYPE_A",      TransportTypeRecvEnum::RECVTYPE_A)
    .value("RECVTYPE_ISIO",   TransportTypeRecvEnum::RECVTYPE_ISIO)
    .value("RECVTYPE_UDP",    TransportTypeRecvEnum::RECVTYPE_UDP)
    .value("RECVTYPE_ZMQSUB", TransportTypeRecvEnum::RECVTYPE_ZMQSUB);

    nb::enum_<SyncEnum>(m, "SyncEnum")
    .value("SUCCESS", SyncEnum::SUCCESS)
    .value("FATAL", SyncEnum::FATAL)
    .value("FAILED", SyncEnum::FAILED)
    .value("TIMEOUT", SyncEnum::TIMEOUT);


    // -----------------------------------------------------------------------
    // Abstract base: EmitTransport
    // -----------------------------------------------------------------------
    nb::class_<EmitTransport>(m, "EmitTransport")
    .def("md",
         &EmitTransport::md,
         nb::rv_policy::reference,
         R"pbdoc(Return a reference to the stream metadata.

Note: ImageStreamIOWrap must be imported before calling this method so that
the IMAGE_METADATA type is registered with nanobind.)pbdoc")
    .def("req",
         &EmitTransport::req,
         R"pbdoc(Return the requested internal storage location.)pbdoc")
    .def("init_storage_target",
         &EmitTransport::init_storage_target,
         nb::arg("req"),
         R"pbdoc(Initialize the storage target for the emitter.

Parameters:
    req  [in]: requested internal storage location
)pbdoc")
        .def("print_type", &EmitTransport::print_type,
             R"pbdoc(Print the transport type to stdout.)pbdoc")
    // TODO: ptr() returns a raw void* pointing to the internal data buffer.
    // TODO      In the future this should be exposed as a numpy/cupy array view
    // TODO      (similar to ImageStreamIOWrap's view_img), respecting the location
    // TODO      field of the metadata (CPU vs GPU memory).
    .def("move_and_publish_data",
         [](EmitTransport & self, double atime_sec)
    {
        // Convert double (fractional seconds since epoch) to timespec
        time_t   tv_sec  = static_cast<time_t>(atime_sec);
        long     tv_nsec = static_cast<long>((atime_sec - tv_sec) * 1e9);
        timespec atime   = {tv_sec, tv_nsec};
        self.move_and_publish_data(&atime);
    },
    nb::arg("atime"),
    R"pbdoc(Copy buffered data to the target storage and publish it.

Parameters:
    atime  [in]: acquisition timestamp as a float (fractional seconds since
                 the Unix epoch, e.g. from time.time())
)pbdoc")
        .def_rw("_initialized", &EmitTransport::_initialized,
                R"pbdoc(True once init_storage_target has succeeded.)pbdoc")

    .def("write", [](const EmitTransport & emit, nb::ndarray<nb::f_contig> b) -> void
    {
        write_to_ptr(emit.ptr(), emit.req(), *emit.md(), b);
    }, R"pbdoc(
          Write into emit transport
          Parameters:
            buffer [in]:  buffer to put into memory image stream
          )pbdoc",
         nb::arg("buffer"));

    // -----------------------------------------------------------------------
    // Abstract base: RecvTransport
    // -----------------------------------------------------------------------
    nb::class_<RecvTransport>(m, "RecvTransport")
    .def("md",
         &RecvTransport::md,
         nb::rv_policy::reference,
         R"pbdoc(Return a reference to the stream metadata.

Note: ImageStreamIOWrap must be imported before calling this method so that
the IMAGE_METADATA type is registered with nanobind.)pbdoc")
    .def("req",
         &RecvTransport::req,
         R"pbdoc(Return the requested internal storage location.)pbdoc")
    .def("init_storage_target",
         &RecvTransport::init_storage_target,
         nb::arg("req"),
         R"pbdoc(Initialize the storage target for the receiver.

Parameters:
    req  [in]: requested internal storage location
)pbdoc")
    .def("print_type", &RecvTransport::print_type,
         R"pbdoc(Print the transport type to stdout.)pbdoc")
    .def("sync_barrier",
         &RecvTransport::sync_barrier,
         R"pbdoc(Block until a new frame is available from the transport.)pbdoc")
    .def("move_new_data_to_requested",
         &RecvTransport::move_new_data_to_requested,
         R"pbdoc(Move the newly received data to the requested storage location.)pbdoc")

    .def("copy", [](const RecvTransport & recv) -> nb::object
    {
        if(recv.ptr() == nullptr)
            throw std::runtime_error("RecvTransport ptr not initialized");
        ImageStreamIODataType dt(recv.md()->datatype);
        switch(dt.datatype)
        {
            case ImageStreamIODataType::DataType::UINT8:
                return copy_from_ptr<uint8_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::INT8:
                return copy_from_ptr<int8_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::UINT16:
                return copy_from_ptr<uint16_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::INT16:
                return copy_from_ptr<int16_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::UINT32:
                return copy_from_ptr<uint32_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::INT32:
                return copy_from_ptr<int32_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::UINT64:
                return copy_from_ptr<uint64_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::INT64:
                return copy_from_ptr<int64_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::FLOAT:
                return copy_from_ptr<float>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::DOUBLE:
                return copy_from_ptr<double>(recv.ptr(), recv.req(), *recv.md());
            // case ImageStreamIODataType::DataType::COMPLEX_FLOAT: return ;
            // case ImageStreamIODataType::DataType::COMPLEX_DOUBLE: return ;
            default:
                throw std::runtime_error("Not implemented");
        }
    })

    .def("view", [](const RecvTransport & recv) -> nb::object
    {
        if(recv.ptr() == nullptr)
            throw std::runtime_error("RecvTransport ptr not initialized");
        ImageStreamIODataType dt(recv.md()->datatype);
        switch(dt.datatype)
        {
            case ImageStreamIODataType::DataType::UINT8:
                return view_from_ptr<uint8_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::INT8:
                return view_from_ptr<int8_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::UINT16:
                return view_from_ptr<uint16_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::INT16:
                return view_from_ptr<int16_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::UINT32:
                return view_from_ptr<uint32_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::INT32:
                return view_from_ptr<int32_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::UINT64:
                return view_from_ptr<uint64_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::INT64:
                return view_from_ptr<int64_t>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::FLOAT:
                return view_from_ptr<float>(recv.ptr(), recv.req(), *recv.md());
            case ImageStreamIODataType::DataType::DOUBLE:
                return view_from_ptr<double>(recv.ptr(), recv.req(), *recv.md());
            // case ImageStreamIODataType::DataType::COMPLEX_FLOAT: return ;
            // case ImageStreamIODataType::DataType::COMPLEX_DOUBLE: return ;
            default:
                throw std::runtime_error("Not implemented");
        }
    });

    // -----------------------------------------------------------------------
    // ImageStreamIOEmit
    // -----------------------------------------------------------------------
    nb::class_<ImageStreamIOEmit, EmitTransport>(m, "ImageStreamIOEmit")
    .def(nb::init<const char *, IMAGE_METADATA *>(),
         R"pbdoc(Create an ImageStreamIO shared-memory emitter.

Parameters:
    name        [in]: name of the ImageStreamIO shared memory stream
    md_request  [in]: metadata template describing shape, datatype, etc.
)pbdoc",
             nb::arg("name"),
             nb::arg("md_request"));

    // -----------------------------------------------------------------------
    // ZmqEmit
    // -----------------------------------------------------------------------
    nb::class_<ZmqEmit, EmitTransport>(m, "ZmqEmit")
        .def(nb::init<const char *, IMAGE_METADATA *>(),
             R"pbdoc(Create a ZMQ publisher emitter.

Parameters:
    name        [in]: name / connection string of the ZMQ stream
    md_request  [in]: metadata template describing shape, datatype, etc.
)pbdoc",
             nb::arg("name"),
             nb::arg("md_request"));

    // -----------------------------------------------------------------------
    // ImageStreamIORecv
    // -----------------------------------------------------------------------
    nb::class_<ImageStreamIORecv, RecvTransport>(m, "ImageStreamIORecv")
        .def(nb::init<const char *>(),
             R"pbdoc(Create an ImageStreamIO shared-memory receiver.

Parameters:
    name  [in]: name of the ImageStreamIO shared memory stream to receive from
)pbdoc",
             nb::arg("name"));

    // -----------------------------------------------------------------------
    // ZmqRecv
    // -----------------------------------------------------------------------
    nb::class_<ZmqRecv, RecvTransport>(m, "ZmqRecv")
        .def(nb::init<const char *>(),
             R"pbdoc(Create a ZMQ subscriber receiver.

Parameters:
    name  [in]: name / connection string of the ZMQ stream to receive from
)pbdoc",
             nb::arg("name"));

    // -----------------------------------------------------------------------
    // MilkTCPEmit
    // -----------------------------------------------------------------------
    nb::class_<MilkTCPEmit, EmitTransport>(m, "MilkTcpEmit")
        .def(nb::init<const char *, IMAGE_METADATA *>(),
             R"pbdoc(Create a MILK TCP emitter (client side).

Parameters:
    name        [in]: connection string in the form "ipv4:port" (e.g. "127.0.0.1:8888")
    md_request  [in]: metadata template describing shape, datatype, etc.
)pbdoc",
             nb::arg("name"),
             nb::arg("md_request"));

    // -----------------------------------------------------------------------
    // MilkTCPRecv
    // -----------------------------------------------------------------------
    nb::class_<MilkTCPRecv, RecvTransport>(m, "MilkTcpRecv")
        .def(nb::init<const char *>(),
             R"pbdoc(Create a MILK TCP receiver (server side).

Parameters:
    name  [in]: port number as a string (e.g. "8888"); binds to INADDR_ANY
)pbdoc",
             nb::arg("name"));

    // -----------------------------------------------------------------------
    // ComputeUnit
    // -----------------------------------------------------------------------
    nb::class_<ComputeUnit>(m, "ComputeUnit")
        .def(nb::init<const char *, const char *, InternalStorageEnum,
                      const char *, InternalStorageEnum>(),
             R"pbdoc(Create a ComputeUnit wiring a receive and an emit transport.

The recv transport is constructed and initialized immediately.
The emit transport is constructed lazily on the first call to loop_once()
(deferred init), using the metadata from the first received frame.

Protocol strings use the scheme::name convention:
    "shm::stream_name"   -> ImageStreamIO shared memory
    "zmq::endpoint"      -> ZMQ pub/sub

Parameters:
    name              [in]: human-readable name for this unit
    recv_proto_string [in]: recv protocol string (e.g. "shm::input")
    recv_memloc       [in]: requested storage location for received data
    emit_proto_string [in]: emit protocol string (e.g. "shm::output")
    emit_memloc       [in]: requested storage location for emitted data
)pbdoc",
             nb::arg("name"),
             nb::arg("recv_proto_string"),
             nb::arg("recv_memloc"),
             nb::arg("emit_proto_string"),
             nb::arg("emit_memloc"))

        .def("loop_once",
             &ComputeUnit::loop_once,
             R"pbdoc(Run one recv/emit cycle.

Blocks on sync_barrier(), moves received data to the requested location,
performs the deferred emit-transport init on the first call, then copies
data from recv to emit and publishes it.)pbdoc")

    .def_ro("name", &ComputeUnit::name_,
            R"pbdoc(Human-readable name of this ComputeUnit.)pbdoc")

    .def_prop_ro("recv_tport",
                 [](ComputeUnit & self) -> RecvTransport *
    {
        return self.recv_tport_;
    },
    nb::rv_policy::reference,
    R"pbdoc(The active receive transport, or None if not yet initialized.)pbdoc")

    .def_prop_ro("emit_tport",
                 [](ComputeUnit & self) -> EmitTransport *
    {
        return self.emit_tport_;
    },
    nb::rv_policy::reference,
    R"pbdoc(The active emit transport, or None before the first loop_once().)pbdoc");
}
