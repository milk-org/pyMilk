#ifndef _PINFO_STRUCT_API
#define _PINFO_STRUCT_API

#include "pinfo_struct.h"

PROCESSINFO *processinfo_setup(
  char       *pinfoname, // short name for the processinfo instance, avoid spaces, name should be human-readable
  const char *descriptionstring,
  const char *msgstring,
  const char *functionname,
  const char *filename,
  int         linenumber);

errno_t processinfo_procdirname(char *procdname);

int processinfo_compute_status(PROCESSINFO *processinfo); // TODO no idea what this does.

int processinfo_WriteMessage(PROCESSINFO *processinfo, const char *msgstring);
int processinfo_WriteMessage_fmt(PROCESSINFO *processinfo, const char *format, ...);

PROCESSINFO *processinfo_shm_create(const char *pname, int CTRLval);
PROCESSINFO *processinfo_shm_link(const char *pname, int *fd);
int processinfo_shm_close(PROCESSINFO *pinfo, int fd);
int processinfo_shm_delete(PROCESSINFO* pinfo);


#endif // #ifndef _PINFO_STRUCT_API
