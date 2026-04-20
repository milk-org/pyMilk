#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "pyFps.hpp"

namespace py = pybind11;
using namespace pybind11::literals;


int fps_value_to_key(pyFps             &cls,
                     const std::string &key,
                     const FPS_type     fps_type,
                     py::object         value)
{
    FPS_type switch_fps_type = fps_type == FPS_type::AUTO ? cls.keys(
                                   key) : fps_type;

    switch(switch_fps_type)
    {
    case FPS_type::ONOFF:
        return functionparameter_SetParamValue_ONOFF(cls, key.c_str(),
                py::bool_(value));
    case FPS_type::INT32:
    case FPS_type::UINT32:
    case FPS_type::INT64:
        return functionparameter_SetParamValue_INT64(cls, key.c_str(), py::int_(value));
    case FPS_type::UINT64:
        return functionparameter_SetParamValue_UINT64(cls, key.c_str(),
                py::int_(value));
    case FPS_type::FLOAT32:
        return functionparameter_SetParamValue_FLOAT32(cls, key.c_str(),
                py::float_(value));
    case FPS_type::FLOAT64:
        return functionparameter_SetParamValue_FLOAT64(cls, key.c_str(),
                py::float_(value));
    case FPS_type::STRING:
    case FPS_type::STREAMNAME:
    case FPS_type::DIRNAME:
    case FPS_type::EXECFILENAME:
    case FPS_type::FILENAME:
    case FPS_type::FITSFILENAME:
        return functionparameter_SetParamValue_STRING(
                   cls,
                   key.c_str(),
                   std::string(py::str(value)).c_str());
    case FPS_type::TIMESPEC:
        return functionparameter_SetParamValue_TIMESPEC(cls, key.c_str(),
                py::float_(value));
    default:
        return EXIT_FAILURE;
    }
}

py::object
fps_value_from_key(pyFps &cls, const std::string &key, const FPS_type fps_type)
{
    switch(fps_type)
    {
    case FPS_type::ONOFF:
        return py::bool_(functionparameter_GetParamValue_ONOFF(cls, key.c_str()));
    case FPS_type::INT32:
    case FPS_type::UINT32:
    case FPS_type::INT64:
        return py::int_(
                   functionparameter_GetParamValue_INT64(cls, key.c_str()));
    case FPS_type::UINT64:
        return py::int_(
                   functionparameter_GetParamValue_UINT64(cls, key.c_str()));
    case FPS_type::FLOAT32:
        return py::float_(
                   functionparameter_GetParamValue_FLOAT32(cls, key.c_str()));
    case FPS_type::FLOAT64:
        return py::float_(
                   functionparameter_GetParamValue_FLOAT64(cls, key.c_str()));
    case FPS_type::STRING:
    case FPS_type::STREAMNAME:
    case FPS_type::DIRNAME:
    case FPS_type::EXECFILENAME:
    case FPS_type::FILENAME:
    case FPS_type::FITSFILENAME:
        return py::str(functionparameter_GetParamPtr_STRING(cls, key.c_str()));
    case FPS_type::TIMESPEC:
        return py::float_(functionparameter_GetParamValue_TIMESPEC(cls, key.c_str()));
    default:
        return py::none();
    }
}

py::dict fps_to_dict(pyFps &cls)
{
    py::dict fps_dict;
    for(auto &key : cls.keys())
    {
        fps_dict[py::str(key.first)] =
            fps_value_from_key(cls, key.first, key.second);
    }
    return fps_dict;
}

PYBIND11_MODULE(FpsWrap, m)
{
    m.doc() = "FpsWrap library module";

    py::enum_<FPS_status>(m, "FPS_status")
        .value("CONF", FPS_status::CONF)
        .value("RUN", FPS_status::RUN)
        .value("CMDCONF", FPS_status::CMDCONF)
        .value("CMDRUN", FPS_status::CMDRUN)
        .value("RUNLOOP", FPS_status::RUNLOOP)
        .value("CHECKOK", FPS_status::CHECKOK)
        .value("TMUXCONF", FPS_status::TMUXCONF)
        .value("TMUXRUN", FPS_status::TMUXRUN)
        .value("TMUXCTRL", FPS_status::TMUXCTRL)
        .export_values();

    py::enum_<FPS_type>(m, "FPS_type")
        .value("AUTO", FPS_type::AUTO)
        .value("UNDEF", FPS_type::UNDEF)
        .value("INT32", FPS_type::INT32)
        .value("UINT32", FPS_type::UINT32)
        .value("INT64", FPS_type::INT64)
        .value("UINT64", FPS_type::UINT64)
        .value("FLOAT32", FPS_type::FLOAT32)
        .value("FLOAT64", FPS_type::FLOAT64)
        .value("PID", FPS_type::PID)
        .value("TIMESPEC", FPS_type::TIMESPEC)
        .value("FILENAME", FPS_type::FILENAME)
        .value("FITSFILENAME", FPS_type::FITSFILENAME)
        .value("EXECFILENAME", FPS_type::EXECFILENAME)
        .value("DIRNAME", FPS_type::DIRNAME)
        .value("STREAMNAME", FPS_type::STREAMNAME)
        .value("STRING", FPS_type::STRING)
        .value("ONOFF", FPS_type::ONOFF)
        .value("PROCESS", FPS_type::PROCESS)
        .value("FPSNAME", FPS_type::FPSNAME)
        .export_values();

    py::enum_<FPS_flags>(m, "FPS_flags")
	.value("DEFAULT_INPUT", FPS_flags::DEFAULT_INPUT)
	.value("DEFAULT_OUTPUT", FPS_flags::DEFAULT_OUTPUT)
	.value("DEFAULT_INPUT_STREAM", FPS_flags::DEFAULT_INPUT_STREAM)
	.value("DEFAULT_OUTPUT_STREAM", FPS_flags::DEFAULT_OUTPUT_STREAM)
    .value("DEFAULT_STATUS", FPS_flags::DEFAULT_STATUS)
	.export_values();


    py::class_<timespec>(m, "timespec")
        .def(py::init<time_t, long>())
        .def_readwrite("tv_sec", &timespec::tv_sec)
        .def_readwrite("tv_nsec", &timespec::tv_nsec);

    py::class_<pyFps>(m, "fps")
        // read-only constructor
        .def(py::init<std::string>(),
             R"pbdoc(Read / connect to existing shared memory FPS
Parameters:
    name     [in]:  the name of the shared memory file to connect
)pbdoc",
             py::arg("name"))

        // read/write constructor
        .def(py::init<std::string, bool, int>(),
             R"pbdoc(Read / connect to existing shared memory FPS
Parameters:
    name     [in]:  the name of the shared memory file to connect
    create   [in]:  flag for creating of shared memory identifier
    NBparamMAX   [in]:  Max number of parameters
)pbdoc",
             py::arg("name"),
             py::arg("create"),
             py::arg("NBparamMAX") = FUNCTION_PARAMETER_NBPARAM_DEFAULT)

        .def("asdict", &fps_to_dict)

        .def("__getitem__",
             [](pyFps &cls, const std::string &key) {
                 return fps_value_from_key(cls, key, cls.keys(key));
             })

        .def("__setitem__",
             [](pyFps &cls, const std::string &key, py::object value) {
                 return fps_value_to_key(cls, key, cls.keys(key), value);
             })

        .def("md", &pyFps::md, py::return_value_policy::reference)

        .def("add_entry",
             &pyFps::add_entry,
             R"pbdoc(Add parameter to database with default settings

If entry already exists, do not modify it

Parameters:
    name     [in]:  the name of the shared memory file to connect
    comment  [in]:  description
    fptype   [in]:  datatype
)pbdoc",
             py::arg("entry_name"),
             py::arg("entry_desc"),
             py::arg("fptype"))

	    .def("add_entry",
             &pyFps::add_entry_w_flags,
             R"pbdoc(Add parameter to database with custom flags

If entry already exists, do not modify it

Parameters:
    name     [in]:  the name of the shared memory file to connect
    comment  [in]:  description
    fptype   [in]:  datatype
    fpflag   [in]:  fps flags
)pbdoc",
             py::arg("entry_name"),
             py::arg("entry_desc"),
             py::arg("fptype"),
	         py::arg("fpflag"))

        .def("get_status", &pyFps::get_status)
        .def("set_status", &pyFps::set_status)
        .def("disconnect", &pyFps::disconnect)

        .def("get_levelKeys", &pyFps::get_levelKeys)

        .def_property_readonly("keys",
                               [](pyFps &cls) {
                                   return cls.keys();
                               })
        .def("signal_update",
             &pyFps::signal_update,
             R"pbdoc(Send update signal to FPS
)pbdoc")
        .def("CONFstart",
             &pyFps::CONFstart,
             R"pbdoc(FPS start CONF process

Requires setup performed by milk-fpsinit, which performs the following setup
- creates the FPS shared memory
- create up tmux sessions
- create function fpsrunstart, fpsrunstop, fpsconfstart and fpsconfstop

Return:
    ret      [out]: error code
)pbdoc")

        .def("CONFstop",
             &pyFps::CONFstop,
             R"pbdoc(FPS stop CONF process

Return:
    ret      [out]: error code
)pbdoc")

        .def("FPCONFexit",
             &pyFps::FPCONFexit,
             R"pbdoc(FPS exit FPCONF process

      Return:
          ret      [out]: error code
      )pbdoc")

    //   .def("FPCONFsetup", &pyFps::FPCONFsetup,
    //        R"pbdoc(FPS setup FPCONF process

    //   Return:
    //       ret      [out]: error code
    //   )pbdoc")

    .def("FPCONFloopstep",
         &pyFps::FPCONFloopstep,
         R"pbdoc(FPS loop step FPCONF process

      Return:
          ret      [out]: error code
      )pbdoc")

    .def("RUNstart",
         &pyFps::RUNstart,
         R"pbdoc(FPS start RUN process

Requires setup performed by milk-fpsinit, which performs the following setup
- creates the FPS shared memory
- create up tmux sessions
- create function fpsrunstart, fpsrunstop, fpsconfstart and fpsconfstop

Return:
    ret      [out]: error code
)pbdoc")

        .def("RUNstop",
             &pyFps::RUNstop,
             R"pbdoc(FPS stop RUN process

 Run pre-set function fpsrunstop in tmux ctrl window

Return:
    ret      [out]: error code
)pbdoc")

        .def("RUNexit",
             &pyFps::RUNexit,
             R"pbdoc(FPS exit RUN process

Return:
    ret      [out]: error code
)pbdoc")
        // Test if CONF process is running

        .def_property_readonly(
            "CONFrunning",
            [](pyFps &cls) {
                pid_t pid = cls->md->confpid;
                if ((getpgid(pid) >= 0) && (pid > 0))
                {
                    return 1;
                }
                else // PID not active
                {
                    if (cls->md->status &
                        FUNCTION_PARAMETER_STRUCT_STATUS_CMDCONF)
                    {
                        // not clean exit
                        return -1;
                    }
                    else
                    {
                        return 0;
                    }
                }
            },
            R"pbdoc(Test if CONF process is running

Return:
    ret      [out]: CONF process is running or not
)pbdoc")

        .def_property_readonly(
            "RUNrunning",
            [](pyFps &cls) {
                pid_t pid = cls->md->runpid;
                if ((getpgid(pid) >= 0) && (pid > 0))
                {
                    return 1;
                }
                else // PID not active
                {
                    if (cls->md->status &
                        FUNCTION_PARAMETER_STRUCT_STATUS_CMDRUN)
                    {
                        // not clean exit
                        return -1;
                    }
                    else
                    {
                        return 0;
                    }
                }
            },
            R"pbdoc(Test if RUN process is running

Return:
    ret      [out]: RUN process is running or not
)pbdoc")

        ;
    py::class_<FUNCTION_PARAMETER_STRUCT_MD>(m, "FPS_md")
        // .def(py::init([]() {
        //     return std::unique_ptr<FUNCTION_PARAMETER_STRUCT_MD>(new
        //     FUNCTION_PARAMETER_STRUCT_MD());
        // }))
        .def_readonly("name", &FUNCTION_PARAMETER_STRUCT_MD::name)
        .def_readonly("description", &FUNCTION_PARAMETER_STRUCT_MD::description)
        .def_readonly("kwarray", &FUNCTION_PARAMETER_STRUCT_MD::keywordarray)
        .def_readonly("workdir", &FUNCTION_PARAMETER_STRUCT_MD::workdir)
        .def_readonly("datadir", &FUNCTION_PARAMETER_STRUCT_MD::datadir)
        .def_readonly("confdir", &FUNCTION_PARAMETER_STRUCT_MD::confdir)
        .def_readonly("sourcefname", &FUNCTION_PARAMETER_STRUCT_MD::sourcefname)
        .def_readonly("sourceline", &FUNCTION_PARAMETER_STRUCT_MD::sourceline)
        .def_readonly("pname", &FUNCTION_PARAMETER_STRUCT_MD::pname)
        .def_readonly("callprogname",
                      &FUNCTION_PARAMETER_STRUCT_MD::callprogname)
        //   .def_readonly("nameindexW",
        //  &FUNCTION_PARAMETER_STRUCT_MD::nameindexW)
        .def_readonly("NBnameindex", &FUNCTION_PARAMETER_STRUCT_MD::NBnameindex)
        .def_readonly("confpid", &FUNCTION_PARAMETER_STRUCT_MD::confpid)
        .def_readonly("confpidstarttime",
                      &FUNCTION_PARAMETER_STRUCT_MD::confpidstarttime)
        .def_readonly("runpid", &FUNCTION_PARAMETER_STRUCT_MD::runpid)
        .def_readonly("runpidstarttime",
                      &FUNCTION_PARAMETER_STRUCT_MD::runpidstarttime)
        .def_readonly("signal", &FUNCTION_PARAMETER_STRUCT_MD::signal)
        .def_readonly("confwaitus", &FUNCTION_PARAMETER_STRUCT_MD::confwaitus)
        .def_readonly("status", &FUNCTION_PARAMETER_STRUCT_MD::status)
        .def_readonly("NBparamMAX", &FUNCTION_PARAMETER_STRUCT_MD::NBparamMAX)
        //   .def_readonly("message", &FUNCTION_PARAMETER_STRUCT_MD::message)
        .def_readonly("msgpindex", &FUNCTION_PARAMETER_STRUCT_MD::msgpindex)
        .def_readonly("msgcode", &FUNCTION_PARAMETER_STRUCT_MD::msgcode)
        .def_readonly("msgcnt", &FUNCTION_PARAMETER_STRUCT_MD::msgcnt)
        .def_readonly("conferrcnt", &FUNCTION_PARAMETER_STRUCT_MD::conferrcnt);
}
