/**
 * @file    processinfo.h
 *
 *
 *
 */

#ifndef _PINFO_STRUCT_H
#define _PINFO_STRUCT_H

#include <stdint.h>
#include <sched.h> // for cpu_set_t

#include "ImageStreamIO/ImageStruct.h"

typedef long imageID; // TODO

#define SHAREDPROCDIR SHAREDMEMDIR

#define STRINGMAXLEN_PROCESSINFO_NAME        80
#define STRINGMAXLEN_PROCESSINFO_SRCFUNC     200
#define STRINGMAXLEN_PROCESSINFO_SRCFILE     200
#define STRINGMAXLEN_PROCESSINFO_TMUXNAME    100
#define STRINGMAXLEN_PROCESSINFO_STATUSMSG   200
#define STRINGMAXLEN_PROCESSINFO_LOGFILENAME 250
#define STRINGMAXLEN_PROCESSINFO_DESCRIPTION 200

// timing info for real-time loop processes
#define PROCESSINFO_NBtimer 100


#define PROCESSINFO_CTRLVAL_RUN   0
#define PROCESSINFO_CTRLVAL_PAUSE 1
#define PROCESSINFO_CTRLVAL_INCR  2
#define PROCESSINFO_CTRLVAL_EXIT  3

#define PROCESSINFO_LOOPSTAT_INIT    0
#define PROCESSINFO_LOOPSTAT_ACTIVE  1
#define PROCESSINFO_LOOPSTAT_PAUSE   2
#define PROCESSINFO_LOOPSTAT_STOP    3
#define PROCESSINFO_LOOPSTAT_ERROR   4
#define PROCESSINFO_LOOPSTAT_SPIN    5
#define PROCESSINFO_LOOPSTAT_CRASHED 6

#define PROCESSINFOLISTSIZE 50000

// uncomment to enable LOGFILE for debugging
//#define PROCESSINFO_LOGFILE

// TODO this is global caching, I don't want caching at this stage.
//
// This structure maintains a list of active processes
// It is used to quickly build (without scanning directory) an array of
// PROCESSINFO
//
/*
typedef struct
{
    pid_t PIDarray[PROCESSINFOLISTSIZE];
    int32_t   active[PROCESSINFOLISTSIZE];
    char  pnamearray[PROCESSINFOLISTSIZE][STRINGMAXLEN_PROCESSINFO_NAME]; // short name
    double createtime[PROCESSINFOLISTSIZE];

} PROCESSINFOLIST;


extern PROCESSINFOLIST *pinfolist;
extern pid_t CLIPID;
//*/

/**
 *
 * This structure hold process information and hooks required for basic
 * monitoring and control Unlike the larger DATA structure above, it is meant to
 * be stored in shared memory for fast access by other processes
 *
 *
 * File name:  /tmp/proc.PID.shm
 *
 */
typedef struct
{
    char name[STRINGMAXLEN_PROCESSINFO_NAME]; /// process name (human-readable)

    char source_FUNCTION
    [STRINGMAXLEN_PROCESSINFO_SRCFUNC];             /// source code function
    char source_FILE[STRINGMAXLEN_PROCESSINFO_SRCFILE]; /// source code file
    int32_t  source_LINE;                                   /// source code line

    pid_t PID; /// process ID; file name is /tmp/proc.PID.shm

    struct timespec createtime; // time at which pinfo was created

    int64_t loopcnt; // counter, useful for loop processes to monitor activity
    int64_t loopcntMax; // exit loop if loopcnt = loopcntMax. Set to -1 for infinite loop
    int32_t CTRLval; // control value to be externally written.
    // 0: run                     (default)
    // 1: pause
    // 2: increment single step (will go back to 1)
    // 3: exit loop

    char tmuxname[STRINGMAXLEN_PROCESSINFO_TMUXNAME];
    // name of tmux session in which process is running, or
    // "NULL"

    int32_t loopstat;
    // 0: INIT       Initialization before loop
    // 1: ACTIVE     in loop
    // 2: PAUSED     loop paused (do not iterate)
    // 3: STOPPED    terminated (clean exit following user request to stop process)
    // 4: ERROR      process could not run, typically used when loop can't start, e.g. missing input
    // 5: SPINNING   do not compute (loop iterates, but does not compute. output stream(s) will still be posted/incremented)
    // 6: CRASHED    pid has gone away without proper exit sequence. Will attempt to generate exit log file (using atexit) to identify crash location

    char statusmsg[STRINGMAXLEN_PROCESSINFO_STATUSMSG]; // status message
    int32_t  statuscode;                                    // status code

#ifdef PROCESSINFO_LOGFILE
    FILE *logFile;
    char  logfilename[STRINGMAXLEN_PROCESSINFO_LOGFILENAME];
#endif

    // OPTIONAL INPUT STREAM SETUP
    // Used to specify which stream will trigger the computation and track trigger state
    // Enables use of function processinfo_waitoninputstream()
    // Enables streamproctrace entry
    // Must be inialized by processinfo_waitoninputstream_init()
    int     triggermode;     // see TRIGGERMODE codes
    imageID triggerstreamID; // -1 if not initialized
    ino_t   triggerstreaminode;
    char    triggerstreamname[STRINGMAXLEN_IMAGE_NAME];
    int32_t triggersem; // semaphore index
    uint64_t triggerstreamcnt; // previous value of trigger counter, updates on trigger
    struct timespec triggerdelay;   // for PROCESSINFO_TRIGGERMODE_DELAY
    struct timespec triggertimeout; // how long to wait until trigger ?
    uint64_t        trigggertimeoutcnt;
    int32_t triggermissedframe; // have we missed any frame, if yes how many ?
    //  0  : no missed frame, loop has been waiting for semaphore to be posted
    //  1  : no missed frame, but semaphore was already posted and at 1 when triggering
    //  2+ : frame(s) missed
    uint64_t triggermissedframe_cumul; // cumulative missed frames
    int32_t    triggerstatus;            // see TRIGGERSTATUS codes

    // Pointer to trigger image (process-local, not valid in SHM for other processes)
    // Used by libprocessinfo to wait on stream without depending on global data
    IMAGE *trigger_image;

    int32_t RT_priority; // -1 if unused. 0-99 for higher priority
    cpu_set_t CPUmask;

    // OPTIONAL TIMING MEASUREMENT
    // Used to measure how long loop process takes to complete task
    // Provides means to stop/pause loop process if timing constraints exceeded
    //
    int32_t MeasureTiming; // 1 if timing is measured, 0 otherwise

    // the last PROCESSINFO_NBtimer times are stored in a circular buffer, from
    // which timing stats are derived
    int32_t timerindex;      // last written index in circular buffer
    int32_t timingbuffercnt; // increments every cycle of the circular buffer
    struct timespec texecstart[PROCESSINFO_NBtimer]; // task starts
    struct timespec texecend[PROCESSINFO_NBtimer];   // task ends

    int64_t dtmedian_iter_ns; // median time offset between iterations [nanosec]
    int64_t dtmedian_exec_ns; // median compute/busy time [nanosec]

    // If enabled=1, pause process if dtiter larger than limit
    int32_t  dtiter_limit_enable;
    int64_t dtiter_limit_value;
    int64_t dtiter_limit_cnt;

    // If enabled=1, pause process if dtexec larger than limit
    int32_t  dtexec_limit_enable;
    int64_t dtexec_limit_value;
    int64_t dtexec_limit_cnt;

    char description[STRINGMAXLEN_PROCESSINFO_DESCRIPTION];

} PROCESSINFO;

#endif // #ifndef _PINFO_STRUCT_H
