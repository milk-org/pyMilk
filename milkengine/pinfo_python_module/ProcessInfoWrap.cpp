#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <string>
#include <cstring>

#include "ProcessInfo-engine/processinfo.h"
//#include "ProcessInfo-engine/pinfo_struct_api.h"
//#include "ProcessInfo-engine/pinfo_exec.h"

#include "pyProcessInfo.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

PYBIND11_MODULE(ProcessInfoWrap, m)
{
    m.doc() = "ProcessInfoWrap library module";

    py::class_<timespec>(m, "timespec", py::module_local())
        .def(py::init<time_t, long>())
        .def_readwrite("tv_sec", &timespec::tv_sec)
        .def_readwrite("tv_nsec", &timespec::tv_nsec);

    py::class_<pyProcessInfo>(m, "processinfo")
        .def(py::init<>(),
             R"pbdoc(Construct a empty Process Info object
)pbdoc")

        .def(py::init<char *, int>(),
             R"pbdoc(Construct a new Process Info object

Parameters:
    pname : name of the Process Info object (human-readable)
    CTRLval : control value to be externally written.
             - 0: run                     (default)
             - 1: pause
             - 2: increment single step (will go back to 1)
             - 3: exit loop
)pbdoc",
             py::arg("name"),
             py::arg("CTRLval"))

        .def("create",
             &pyProcessInfo::create,
             R"pbdoc(Create Process Info object in shared memory

Parameters:
    pname : name of the Process Info object (human-readable)
    CTRLval : control value to be externally written.
             - 0: run                     (default)
             - 1: pause
             - 2: increment single step (will go back to 1)
             - 3: exit loop
Return:
    ret : error code
)pbdoc",
             py::arg("name"),
             py::arg("CTRLval"))

        .def("link",
             &pyProcessInfo::link,
             R"pbdoc(Link an existing Process Info object in shared memory

Parameters:
    pname : name of the Process Info object (human-readable)
Return:
    ret : error code
)pbdoc",
             py::arg("name"))

        .def("close",
             py::overload_cast<const char*>(&pyProcessInfo::close),
             R"pbdoc(Close an existing Process Info object in shared memory

Parameters:
    pname : name of the Process Info object (human-readable)
Return:
    ret : error code
)pbdoc",
             py::arg("name")="")

        .def("sigexit",
             &pyProcessInfo::sigexit,
             R"pbdoc(Send a signal to a Process Info object

Parameters:
    sig : signal to send
)pbdoc",
             py::arg("sig"))

        .def("writeMessage",
             &pyProcessInfo::writeMessage,
             R"pbdoc(Write a message into a Process Info object

Parameters:
    message : message to write
Return:
    ret : error code
)pbdoc",
             py::arg("message"))

        .def("exec_start",
             &pyProcessInfo::exec_start,
             R"pbdoc(Define the start of the process (timing purpose)

Return:
    ret : error code
)pbdoc")

        .def("exec_end",
             &pyProcessInfo::exec_end,
             R"pbdoc(Define the end of the process (timing purpose)

Return:
    ret : error code
)pbdoc")

        .def_property(
            "name",
            [](pyProcessInfo &p) {
                return std::string(p->name);
            },
            [](pyProcessInfo &p, std::string name) {
                strncpy(p->name, name.c_str(), sizeof(p->name));
            },
            "Name of the Process Info object")

        .def_property(
            "source_FUNCTION",
            [](pyProcessInfo &p) {
                return std::string(p->source_FUNCTION);
            },
            [](pyProcessInfo &p, std::string source_FUNCTION) {
                strncpy(p->source_FUNCTION,
                        source_FUNCTION.c_str(),
                        sizeof(p->source_FUNCTION));
            })

        .def_property(
            "source_FILE",
            [](pyProcessInfo &p) {
                return std::string(p->source_FILE);
            },
            [](pyProcessInfo &p, std::string source_FILE) {
                strncpy(p->source_FILE,
                        source_FILE.c_str(),
                        sizeof(p->source_FILE));
            })
        .def_property(
            "source_LINE",
            [](pyProcessInfo &p) {
                return p->source_LINE;
            },
            [](pyProcessInfo &p, int source_LINE) {
                p->source_LINE = source_LINE;
            })
        .def_property(
            "PID",
            [](pyProcessInfo &p) {
                return p->PID;
            },
            [](pyProcessInfo &p, int PID) {
                p->PID = PID;
            })

        .def_property(
            "createtime",
            [](pyProcessInfo &p) {
                return p->createtime;
            },
            [](pyProcessInfo &p, timespec createtime) {
                p->createtime = createtime;
            })
        .def_property(
            "loopcnt",
            [](pyProcessInfo &p) {
                return p->loopcnt;
            },
            [](pyProcessInfo &p, int loopcnt) {
                p->loopcnt = loopcnt;
            })
        .def_property(
            "CTRLval",
            [](pyProcessInfo &p) {
                return p->CTRLval;
            },
            [](pyProcessInfo &p, int CTRLval) {
                p->CTRLval = CTRLval;
            })
        .def_property(
            "tmuxname",
            [](pyProcessInfo &p) {
                return std::string(p->tmuxname);
            },
            [](pyProcessInfo &p, std::string name) {
                strncpy(p->tmuxname, name.c_str(), sizeof(p->tmuxname));
            })
        .def_property(
            "loopstat",
            [](pyProcessInfo &p) {
                return p->loopstat;
            },
            [](pyProcessInfo &p, int loopstat) {
                p->loopstat = loopstat;
            })
        .def_property(
            "statusmsg",
            [](pyProcessInfo &p) {
                return std::string(p->statusmsg);
            },
            [](pyProcessInfo &p, std::string name) {
                strncpy(p->statusmsg, name.c_str(), sizeof(p->statusmsg));
            })
        .def_property(
            "statuscode",
            [](pyProcessInfo &p) {
                return p->statuscode;
            },
            [](pyProcessInfo &p, int statuscode) {
                p->statuscode = statuscode;
            })
        // .def_readwrite("logFile", &pyProcessInfo::m_pinfo::logFile)
        .def_property(
            "description",
            [](pyProcessInfo &p) {
                return std::string(p->description);
            },
            [](pyProcessInfo &p, std::string name) {
                strncpy(p->description, name.c_str(), sizeof(p->description));
            });
}
