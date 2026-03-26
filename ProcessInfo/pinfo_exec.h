#ifndef _PINFO_EXEC_H
#define _PINFO_EXEC_H

#include "pinfo_struct.h"

// input stream triggering modes

// trigger immediately
#define PROCESSINFO_TRIGGERMODE_IMMEDIATE 0
// trigger when cnt0 increments
#define PROCESSINFO_TRIGGERMODE_CNT0 1
// trigger when cnt1 increments
#define PROCESSINFO_TRIGGERMODE_CNT1 2
// trigger when semaphore is posted
#define PROCESSINFO_TRIGGERMODE_SEMAPHORE 3
// trigger after a time delay
#define PROCESSINFO_TRIGGERMODE_DELAY 4
// trigger when semaphore is posted AND propagate the timeout (i.e. enter the execution anyway)
#define PROCESSINFO_TRIGGERMODE_SEMAPHORE_PROP_TIMEOUTS 5
// trigger when cnt0 < cnt2 (demand-driven / flow control)
#define PROCESSINFO_TRIGGERMODE_CNT2 6

// trigger is currently waiting for input
#define PROCESSINFO_TRIGGERSTATUS_WAITING 1
// trigger has been received and we're executing the loop
#define PROCESSINFO_TRIGGERSTATUS_RECEIVED 2
// trigger has not been received but we've skipped out of the wait into the execution of the loop
#define PROCESSINFO_TRIGGERSTATUS_TIMEDOUT 3



errno_t processinfo_waitoninputstream_init(PROCESSINFO *processinfo,
        IMAGE        *image,
        int          triggermode,
        int          semindexrequested);

errno_t processinfo_waitoninputstream(PROCESSINFO *processinfo);

#define PROCINFO_TRIGGER_DELAYUS(delayus)                                      \
    do                                                                         \
    {                                                                          \
        processinfo_waitoninputstream_init(processinfo,                        \
                                           NULL,                               \
                                           PROCESSINFO_TRIGGERMODE_DELAY,      \
                                           -1);                                \
        processinfo->triggerdelay.tv_sec  = 0;                                 \
        processinfo->triggerdelay.tv_nsec = (long) ((delayus) * 1000);         \
        while (processinfo->triggerdelay.tv_nsec > 1000000000)                 \
        {                                                                      \
            processinfo->triggerdelay.tv_nsec -= 1000000000;                   \
            processinfo->triggerdelay.tv_sec += 1;                             \
        }                                                                      \
    } while (0)

errno_t processinfo_update_output_stream(PROCESSINFO *processinfo,
        IMAGE        *output_image,
        IMAGE        *input_image);

int processinfo_exec_start(PROCESSINFO *processinfo);
int processinfo_exec_end(PROCESSINFO *processinfo);

// These probably don't need to be part of the public API.
void processinfo_sig_handler(int signo);
int processinfo_CatchSignals();
int processinfo_ProcessSignals(PROCESSINFO *processinfo);
int processinfo_SIGexit(PROCESSINFO *processinfo, int SignalNumber);

// TODO why are JUST THESE TWO having an errno_t return???
errno_t processinfo_error(PROCESSINFO *processinfo, char *errmsgstring);
errno_t processinfo_loopstart(PROCESSINFO *processinfo);
int processinfo_loopstep(PROCESSINFO *processinfo);
int processinfo_cleanExit(PROCESSINFO *processinfo);

#endif // #ifndef _PINFO_EXEC_H
