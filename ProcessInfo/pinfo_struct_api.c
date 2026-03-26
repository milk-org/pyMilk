#include <stdarg.h>
#include <string.h>

#include "pinfo_struct_api.h"
#include "ImageStreamIO/timeutils.h"
#include "ImageStreamIO/milkDebugTools.h"

#define FILEMODE 0666
#include <sys/mman.h> // mmap()
#include <sys/stat.h> // fstat()
#include <fcntl.h> // O_RDWR etc..
#include <dirent.h> // DIR

// High level processinfo function

errno_t processinfo_procdirname(char *procdname)
{
    int  procdirOK = 0;
    DIR *tmpdir;

    // first, we try the env variable if it exists
    char *MILK_PROC_DIR = getenv("MILK_PROC_DIR");
    if(MILK_PROC_DIR != NULL)
    {
        // printf(" [ MILK_PROC_DIR ] '%s'\n", MILK_PROC_DIR);

        {
            int slen = snprintf(procdname,
                                STRINGMAXLEN_FULLFILENAME,
                                "%s",
                                MILK_PROC_DIR);
            if(slen < 1)
            {
                PRINT_ERROR("snprintf wrote <1 char");
                abort(); // can't handle this error any other way
            }
            if(slen >= STRINGMAXLEN_FULLFILENAME)
            {
                PRINT_ERROR("snprintf string truncation");
                abort(); // can't handle this error any other way
            }
        }

        // does this direcory exist ?
        tmpdir = opendir(procdname);
        if(tmpdir)  // directory exits
        {
            procdirOK = 1;
            closedir(tmpdir);
        }
        else
        {
            printf(" [ WARNING ] '%s' does not exist\n", MILK_PROC_DIR);
        }
    }

    // second, we try SHAREDPROCDIR default
    if(procdirOK == 0)
    {
        tmpdir = opendir(SHAREDPROCDIR);
        if(tmpdir)  // directory exits
        {
            snprintf(procdname, STRINGMAXLEN_DIRNAME, "%s", SHAREDPROCDIR);
            procdirOK = 1;
            closedir(tmpdir);
        }
    }

    // if all above fails, set to /tmp
    if(procdirOK == 0)
    {
        tmpdir = opendir("/tmp");
        if(!tmpdir)
        {
            exit(EXIT_FAILURE);
        }
        else
        {
            snprintf(procdname, STRINGMAXLEN_DIRNAME, "/tmp");
            procdirOK = 1;
        }
    }

    return RETURN_SUCCESS;
}

int processinfo_compute_status(PROCESSINFO *processinfo)
{
    int processcompstatus = 1;

    if(processinfo->CTRLval == 5)
    {
        processcompstatus = 0;
    }

    return processcompstatus;
}

/**
 * @brief Initialize and register a process for MILK process management.
 *
 * This function performs the first-time setup of a process's shared memory
 * status structure.
 *
 * Logic flow:
 * 1.  Ensure the processinfo instance name is valid.
 * 2.  Call `processinfo_shm_create` to physically create and map the SHM segment.
 * 3.  Initialize metadata fields: loop status (INIT), source code location,
 *     description, and initial message.
 * 4.  Set default values for loop control: infinite loop, no timing measurement,
 *     and no real-time priority.
 */
PROCESSINFO *processinfo_setup(
    char         *
    pinfoname, // short name for the processinfo instance, avoid spaces, name should be human-readable
    const char *descriptionstring,
    const char *msgstring,
    const char *functionname,
    const char *filename,
    int         linenumber)
{
    DEBUG_TRACE_FSTART();

    //printf("DEBUG  %s [%d] %s\n", __FILE__, __LINE__, __FUNCTION__);
    //fflush(stdout);

    static PROCESSINFO *processinfo = NULL;
    static int processinfoActive = 0;

    if(processinfoActive == 0)
    {

        char pinfoname0[STRINGMAXLEN_PROCESSINFO_NAME];
        {
            int slen = snprintf(pinfoname0,
                                STRINGMAXLEN_PROCESSINFO_NAME,
                                "%s",
                                pinfoname);
            if(slen < 1)
            {
                PRINT_ERROR("snprintf wrote <1 char");
                abort();
            }
            if(slen >= STRINGMAXLEN_PROCESSINFO_NAME)
            {
                PRINT_ERROR("snprintf string truncation");
                abort();
            }
        }
        processinfo = processinfo_shm_create(pinfoname0, 0);
    }

    processinfo->loopstat = 0; // loop initialization
    strcpy(processinfo->source_FUNCTION, functionname);
    strcpy(processinfo->source_FILE, filename);
    processinfo->source_LINE = linenumber;
    strcpy(processinfo->description, descriptionstring);
    processinfo_WriteMessage(processinfo, msgstring);
    processinfoActive = 1;

    processinfo->loopcntMax = -1; // infinite loop

    processinfo->MeasureTiming = 0;  // default: do not measure timing
    processinfo->RT_priority   = -1; // default: do not assign RT priority

    DEBUG_TRACE_FEXIT();

    return processinfo;
}



int processinfo_WriteMessage(
    PROCESSINFO *processinfo,
    const char  *msgstring
)
{
    struct timespec tnow;
    struct tm      *tnowtm;
    clock_gettime(CLOCK_MILK, &tnow);
    tnowtm = gmtime(&tnow.tv_sec);

    snprintf(processinfo->statusmsg,
             STRINGMAXLEN_PROCESSINFO_STATUSMSG,
             "%02d:%02d:%02d.%03d %s",
             tnowtm->tm_hour,
             tnowtm->tm_min,
             tnowtm->tm_sec,
             (int)(0.000001 * (tnow.tv_nsec)),
             msgstring);

    if(processinfo->PID == 0) // not initialized
    {
        strcpy(processinfo->statusmsg, msgstring);
    }

// TODO should this be compile-time ?? Why not always enabled and then toggle on with e.g. an env. variable???
#ifdef PROCESSINFO_LOGFILE
    if(processinfo->logFile != NULL)
    {
        fprintf(processinfo->logFile,
                "%02d:%02d:%02d.%09ld %06d %s\n",
                tnowtm->tm_hour,
                tnowtm->tm_min,
                tnowtm->tm_sec,
                tnow.tv_nsec,
                (int) processinfo->PID,
                msgstring);

        fflush(processinfo->logFile);
    }
#endif

    return 0;
}



int processinfo_WriteMessage_fmt(
    PROCESSINFO *processinfo,
    const char *format,
    ...
)
{
    va_list args;
    char msg[STRINGMAXLEN_PROCESSINFO_STATUSMSG];

    va_start(args, format);
    vsnprintf(msg, STRINGMAXLEN_PROCESSINFO_STATUSMSG, format, args);
    va_end(args);

    processinfo_WriteMessage(processinfo, msg);

    return 0;
}

PROCESSINFO *processinfo_shm_create(
    const char *pname,
    int CTRLval
)
{
    DEBUG_TRACE_FSTART();

    size_t       sharedsize = 0; // shared memory size in bytes
    int          SM_fd;          // shared memory file descriptor
    PROCESSINFO *pinfo = NULL;

    static int LogFileCreated = 0;
    // toggles to 1 when created. To avoid re-creating file on same process

    sharedsize = sizeof(PROCESSINFO);

    char  SM_fname[STRINGMAXLEN_FULLFILENAME];
    pid_t PID = getpid();

    // TODO I've shut down caching from the call to:
    // long pindex = processinfo_shm_list_create();
    // TODO and dependent initialization

    DEBUG_TRACEPOINT("getting procdname");
    char procdname[STRINGMAXLEN_DIRNAME];
    processinfo_procdirname(procdname);


    WRITE_FULLFILENAME(SM_fname,
                       "%s/proc.%s.%06d.shm",
                       procdname,
                       pname,
                       (int) PID);

    DEBUG_TRACEPOINT("SM_fname = %s", SM_fname);

    umask(0);
    SM_fd = open(SM_fname, O_RDWR | O_CREAT | O_TRUNC, (mode_t) FILEMODE);
    if(SM_fd == -1)
    {
        perror("Error opening file for writing");
        exit(0);
    }

    int result;
    result = lseek(SM_fd, sharedsize - 1, SEEK_SET);
    if(result == -1)
    {
        close(SM_fd);
        fprintf(stderr, "Error calling lseek() to 'stretch' the file");
        exit(0);
    }

    result = write(SM_fd, "", 1);
    if(result != 1)
    {
        close(SM_fd);
        perror("Error writing last byte of the file");
        exit(0);
    }

    pinfo = (PROCESSINFO *)
            mmap(0, sharedsize, PROT_READ | PROT_WRITE, MAP_SHARED, SM_fd, 0);
    if(pinfo == MAP_FAILED)
    {
        close(SM_fd);
        perror("Error mmapping the file");
        exit(0);
    }

    DEBUG_TRACEPOINT("created processinfo entry at %s\n", SM_fname);
    DEBUG_TRACEPOINT("shared memory space = %ld bytes\n", sharedsize);

    clock_gettime(CLOCK_MILK, &pinfo->createtime);

    strcpy(pinfo->name, pname);

    int tmuxnamestrlen = 100;
    char  tmuxname[tmuxnamestrlen];
    FILE *fpout;
    int   notmux = 0;

    fpout = popen("tmuxsessionname", "r");
    if(fpout == NULL)
    {
        // printf("WARNING: cannot run command \"tmuxsessionname\"\n");
        notmux = 1;
    }
    else
    {
        if(fgets(tmuxname, tmuxnamestrlen, fpout) == NULL)
        {
            //printf("WARNING: fgets error\n");
            notmux = 1;
        }
        pclose(fpout);
    }
    // remove line feed
    if(strlen(tmuxname) > 0)
    {
        //  printf("tmux name : %s\n", tmuxname);
        //  printf("len: %d\n", (int) strlen(tmuxname));
        fflush(stdout);

        if(tmuxname[strlen(tmuxname) - 1] == '\n')
        {
            tmuxname[strlen(tmuxname) - 1] = '\0';
        }
        else
        {
            // printf("tmux name empty\n");
        }
    }
    else
    {
        notmux = 1;
    }

    if(notmux == 1)
    {
        snprintf(tmuxname, tmuxnamestrlen, " ");
    }

    // force last char to be term, just in case
    tmuxname[99] = '\0';

    DEBUG_TRACEPOINT("tmux name : %s\n", tmuxname);

    strncpy(pinfo->tmuxname, tmuxname, tmuxnamestrlen - 1);

    // set control value (default 0)
    // 1 : pause
    // 2 : increment single step (will go back to 1)
    // 3 : exit loop
    pinfo->CTRLval = CTRLval;

    pinfo->MeasureTiming = 1;

    // initialize timer indexes and counters
    pinfo->timerindex      = 0;
    pinfo->timingbuffercnt = 0;

    // disable timer limit feature
    pinfo->dtiter_limit_enable = 0;
    pinfo->dtexec_limit_enable = 0;

    // data.pinfo = pinfo; // REMOVED
    pinfo->PID = PID;

    // create logfile
    //char logfilename[300];
    struct timespec tnow;

    clock_gettime(CLOCK_MILK, &tnow);

#ifdef PROCESSINFO_LOGFILE
    {
        int slen = snprintf(pinfo->logfilename,
                            STRINGMAXLEN_PROCESSINFO_LOGFILENAME,
                            "%s/proc.%s.%06d.%09ld.logfile",
                            procdname,
                            pinfo->name,
                            (int) pinfo->PID,
                            tnow.tv_sec);
        if(slen < 1)
        {
            PRINT_ERROR("snprintf wrote <1 char");
            abort();
        }
        if(slen >= STRINGMAXLEN_PROCESSINFO_LOGFILENAME)
        {
            PRINT_ERROR("snprintf string truncation");
            abort();
        }
    }

    if(LogFileCreated == 0)
    {
        pinfo->logFile = fopen(pinfo->logfilename, "w");
        LogFileCreated = 1;
    }

    int msgstrlen = 300;
    char msgstring[msgstrlen];
    snprintf(msgstring, msgstrlen, "LOG START %s", pinfo->logfilename);
    processinfo_WriteMessage(pinfo, msgstring);
#endif

    DEBUG_TRACE_FEXIT();

    return pinfo;
}

PROCESSINFO *processinfo_shm_link(const char *pname, int *fd)
{
    size_t sharedsize = 0; // shared memory size in bytes
    int    SM_fd;          // shared memory file descriptor

    SM_fd = open(pname, O_RDWR);
    if(SM_fd == -1)
    {
        return (PROCESSINFO *) MAP_FAILED;
    }

    struct stat SM_stat;
    if (fstat(SM_fd, &SM_stat) == -1) {
        close(SM_fd);
        return (PROCESSINFO *) MAP_FAILED;
    }
    sharedsize = SM_stat.st_size;

    PROCESSINFO *pinfo = (PROCESSINFO *)
                             mmap(0, sharedsize, PROT_READ | PROT_WRITE, MAP_SHARED, SM_fd, 0);

    if(pinfo == MAP_FAILED)
    {
        close(SM_fd);
        return (PROCESSINFO *) MAP_FAILED;
    }

    *fd = SM_fd;

    return pinfo;
}

int processinfo_shm_close(PROCESSINFO *pinfo, int fd)
{
    if(munmap(pinfo, sizeof(PROCESSINFO)) == -1)
    {
        perror("Error un-mmapping the file");
    }
    close(fd);

    return 0;
}

int processinfo_shm_delete(PROCESSINFO* pinfo) {
    // TODO this actually expects the pinfo to still be mmap but does NOT munmap.
    // TODO the file will disappear but the memory will remain until this process exits.
    char procdname[STRINGMAXLEN_DIRNAME];
    processinfo_procdirname(procdname);

    char SM_fname[STRINGMAXLEN_FULLFILENAME];
    WRITE_FULLFILENAME(SM_fname,
        "%s/proc.%s.%06d.shm",
        procdname,
        pinfo->name,
        pinfo->PID);

    remove(SM_fname);
}
