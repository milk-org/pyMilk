/**
 * @file processtools_trigger.c
 *
 *
 */

#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "processinfo_internal.h"
#include "processinfo.h"
#include "processtools_trigger.h"
#include "ImageStreamIO/ImageStreamIO.h"

#ifndef CLOCK_MILK
#define CLOCK_MILK CLOCK_REALTIME
#endif

/** @brief Set up input wait stream
 *
 * Specify stream on which the loop process will be triggering, and
 * what is the trigger mode.
 *
 * The actual trigger mode may be different from the requested trigger mode.
 *
 * The standard option should be tiggermode = PROCESSINFO_TRIGGERMODE_SEMAPHORE
 * and semindex = -1, which will automatically find a suitable semaphore
 *
 */
errno_t processinfo_waitoninputstream_init(
    PROCESSINFO *processinfo,
    IMAGE        *image,
    int          triggermode,
    int          semindexrequested
)
{
    DEBUG_TRACE_FSTART("%p %d %d", (void*)image, triggermode, semindexrequested);

    // Legacy support: triggerstreamID is not used internally by the library anymore
    // but we can set it to -1 to indicate unused.
    processinfo->triggerstreamID = -1;
    //processinfo->trigger_image = image;

    if(image != NULL)
    {
        processinfo->triggerstreaminode = image->md[0].inode;
        strncpy(processinfo->triggerstreamname,
                image->md[0].name,
                STRINGMAXLEN_IMAGE_NAME);
    }
    else
    {
        // convention : stream name single space : inactive
        DEBUG_TRACEPOINT("Setting trigger stream name to single space");
        processinfo->triggerstreaminode = 0;
        strcpy(processinfo->triggerstreamname, " ");
    }

    processinfo->triggermissedframe_cumul = 0;
    processinfo->trigggertimeoutcnt       = 0;
    processinfo->triggerstatus            = 0;

    // Default timeout: 2 seconds
    processinfo->triggertimeout.tv_sec  = 2;
    processinfo->triggertimeout.tv_nsec = 0;

    // Set requested triggermode
    processinfo->triggermode = triggermode;

    // valid modes

    if(triggermode == PROCESSINFO_TRIGGERMODE_CNT0)
    {
        DEBUG_TRACEPOINT("trigger mode %d = cnt0", PROCESSINFO_TRIGGERMODE_CNT0);

        if(image == NULL)
        {
            PRINT_ERROR("missing trigger image");
            return RETURN_FAILURE;
        }
        // trigger on cnt0 increment
        processinfo->triggerstreamcnt = image->md[0].cnt0;
    }

    if(triggermode == PROCESSINFO_TRIGGERMODE_CNT1)
    {
        DEBUG_TRACEPOINT("trigger mode %d = cnt1", PROCESSINFO_TRIGGERMODE_CNT1);

        if(image == NULL)
        {
            PRINT_ERROR("missing trigger image");
            return RETURN_FAILURE;
        }
        // trigger on cnt1 increment
        processinfo->triggerstreamcnt = image->md[0].cnt1;
    }

    if(triggermode == PROCESSINFO_TRIGGERMODE_CNT2)
    {
        DEBUG_TRACEPOINT("trigger mode %d = cnt2", PROCESSINFO_TRIGGERMODE_CNT2);

        if(image == NULL)
        {
            PRINT_ERROR("missing trigger image");
            return RETURN_FAILURE;
        }
        // trigger on cnt0 < cnt2
        processinfo->triggerstreamcnt = image->md[0].cnt0;
    }

    if(triggermode == PROCESSINFO_TRIGGERMODE_IMMEDIATE)
    {
        DEBUG_TRACEPOINT("trigger mode %d = immediate",
                         PROCESSINFO_TRIGGERMODE_IMMEDIATE);
        // immmediate trigger
        processinfo->triggerstreamcnt = 0;
    }

    if(triggermode == PROCESSINFO_TRIGGERMODE_DELAY)
    {
        DEBUG_TRACEPOINT("trigger mode %d = time delay",
                         PROCESSINFO_TRIGGERMODE_DELAY);
        // time wait
        processinfo->triggerstreamcnt = 0;
    }

    // checking if semaphore trigger mode OK
    if(triggermode == PROCESSINFO_TRIGGERMODE_SEMAPHORE ||
       triggermode == PROCESSINFO_TRIGGERMODE_SEMAPHORE_PROP_TIMEOUTS)
    {
        DEBUG_TRACEPOINT("trigger mode %d = semaphore %d",
                         triggermode,
                         semindexrequested);
        if(semindexrequested < -1)
        {
            PRINT_ERROR("invalid semaphore index %d", semindexrequested);
            return RETURN_FAILURE;
        }
        if(image == NULL)
        {
            PRINT_ERROR("image not valid");
            return RETURN_FAILURE;
        }
        processinfo->triggersem =
            ImageStreamIO_getsemwaitindex(image, semindexrequested);
        if(processinfo->triggersem == -1)
        {
            // could not find available semaphore
            // fall back to CNT0 trigger mode
            processinfo->triggermode = PROCESSINFO_TRIGGERMODE_CNT0;
            processinfo->triggerstreamcnt = image->md[0].cnt0;
        }
        else
        {
            // register PID to stream
            image->semReadPID[processinfo->triggersem] = getpid();
        }
    }

    DEBUG_TRACE_FEXIT();
    return RETURN_SUCCESS;
}


/** @brief Wait on a stream
 *
 */
// DISABLED IN BACKPORT -- relies on data global struct inextricably.
