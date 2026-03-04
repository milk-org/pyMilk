#define _GNU_SOURCE // for sigabbrev_np from <string.h>

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "pinfo_exec.h"
#include "pinfo_struct.h"
#include "pinfo_struct_api.h"

#include "timeutils.h"

#include "ImageStreamIO/milkDebugTools.h"

// Globals for signal handling
static int32_t GLOBAL_signal_received = 0;
static int32_t GLOBAL_signal_received_signo = -1;

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
) {
  DEBUG_TRACE_FSTART("%p %d %d", (void*)image, triggermode, semindexrequested);

  // Legacy support: triggerstreamID is not used internally by the library anymore
  // but we can set it to -1 to indicate unused.
  processinfo->triggerstreamID = -1;
  processinfo->trigger_image = image;

  if(image != NULL) {
    processinfo->triggerstreaminode = image->md[0].inode;
    strncpy(processinfo->triggerstreamname,
            image->md[0].name,
            STRINGMAXLEN_IMAGE_NAME);
  } else {
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

  if(triggermode == PROCESSINFO_TRIGGERMODE_CNT0) {
    DEBUG_TRACEPOINT("trigger mode %d = cnt0", PROCESSINFO_TRIGGERMODE_CNT0);

    if(image == NULL) {
      PRINT_ERROR("missing trigger image");
      return RETURN_FAILURE;
    }
    // trigger on cnt0 increment
    processinfo->triggerstreamcnt = image->md[0].cnt0;
  }

  if(triggermode == PROCESSINFO_TRIGGERMODE_CNT1) {
    DEBUG_TRACEPOINT("trigger mode %d = cnt1", PROCESSINFO_TRIGGERMODE_CNT1);

    if(image == NULL) {
      PRINT_ERROR("missing trigger image");
      return RETURN_FAILURE;
    }
    // trigger on cnt1 increment
    processinfo->triggerstreamcnt = image->md[0].cnt1;
  }

  if(triggermode == PROCESSINFO_TRIGGERMODE_CNT2) {
    DEBUG_TRACEPOINT("trigger mode %d = cnt2", PROCESSINFO_TRIGGERMODE_CNT2);

    if(image == NULL) {
      PRINT_ERROR("missing trigger image");
      return RETURN_FAILURE;
    }
    // trigger on cnt0 < cnt2
    processinfo->triggerstreamcnt = image->md[0].cnt0;
  }

  if(triggermode == PROCESSINFO_TRIGGERMODE_IMMEDIATE) {
    DEBUG_TRACEPOINT("trigger mode %d = immediate",
                     PROCESSINFO_TRIGGERMODE_IMMEDIATE);
    // immmediate trigger
    processinfo->triggerstreamcnt = 0;
  }

  if(triggermode == PROCESSINFO_TRIGGERMODE_DELAY) {
    DEBUG_TRACEPOINT("trigger mode %d = time delay",
                     PROCESSINFO_TRIGGERMODE_DELAY);
    // time wait
    processinfo->triggerstreamcnt = 0;
  }

  // checking if semaphore trigger mode OK
  if(triggermode == PROCESSINFO_TRIGGERMODE_SEMAPHORE ||
      triggermode == PROCESSINFO_TRIGGERMODE_SEMAPHORE_PROP_TIMEOUTS) {
    DEBUG_TRACEPOINT("trigger mode %d = semaphore %d",
                     triggermode,
                     semindexrequested);
    if(semindexrequested < -1) {
      PRINT_ERROR("invalid semaphore index %d", semindexrequested);
      return RETURN_FAILURE;
    }
    if(image == NULL) {
      PRINT_ERROR("image not valid");
      return RETURN_FAILURE;
    }
    processinfo->triggersem =
      ImageStreamIO_getsemwaitindex(image, semindexrequested);
    if(processinfo->triggersem == -1) {
      // could not find available semaphore
      // fall back to CNT0 trigger mode
      processinfo->triggermode = PROCESSINFO_TRIGGERMODE_CNT0;
      processinfo->triggerstreamcnt = image->md[0].cnt0;
    } else {
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
errno_t processinfo_waitoninputstream(PROCESSINFO *processinfo) {
  IMAGE *image = processinfo->trigger_image;
  processinfo->triggermissedframe = 0;

  if(processinfo->triggermode == PROCESSINFO_TRIGGERMODE_IMMEDIATE) {
    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_RECEIVED;
    // return immediately
    return RETURN_SUCCESS;
  }

  if(processinfo->triggermode == PROCESSINFO_TRIGGERMODE_CNT0) {
    if(image == NULL) return RETURN_FAILURE;
    // use cnt0
    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_WAITING;

    while(image->md[0].cnt0 == processinfo->triggerstreamcnt) {
      // test if new frame exists
      usleep(5);
    }
    processinfo->triggermissedframe =
      image->md[0].cnt0 -
      processinfo->triggerstreamcnt - 1;
    // update trigger counter
    processinfo->triggerstreamcnt = image->md[0].cnt0;

    processinfo->triggermissedframe_cumul +=
      processinfo->triggermissedframe;

    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_RECEIVED;

    return RETURN_SUCCESS;
  }

  if(processinfo->triggermode == PROCESSINFO_TRIGGERMODE_CNT1) {
    if(image == NULL) return RETURN_FAILURE;
    // use cnt1
    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_WAITING;

    while(image->md[0].cnt1 == processinfo->triggerstreamcnt) {
      // test if new frame exists
      usleep(5);
    }
    processinfo->triggermissedframe =
      image->md[0].cnt1 -
      processinfo->triggerstreamcnt - 1;
    // update trigger counter
    processinfo->triggerstreamcnt = image->md[0].cnt1;

    processinfo->triggermissedframe_cumul +=
      processinfo->triggermissedframe;

    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_RECEIVED;

    return RETURN_SUCCESS;
  }

  if(processinfo->triggermode == PROCESSINFO_TRIGGERMODE_CNT2) {
    if(image == NULL) return RETURN_FAILURE;
    // use cnt2
    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_WAITING;

    while(image->md[0].cnt0 >= image->md[0].cnt2) {
      // wait until we are allowed to proceed
      usleep(5);
    }
    processinfo->triggermissedframe = 0;

    // update trigger counter
    processinfo->triggerstreamcnt = image->md[0].cnt0;

    processinfo->triggermissedframe_cumul +=
      processinfo->triggermissedframe;

    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_RECEIVED;

    return RETURN_SUCCESS;
  }

  if(processinfo->triggermode == PROCESSINFO_TRIGGERMODE_DELAY) {
    // return after fixed delay
    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_WAITING;

    // Note: nanosleep adds a few x10us of latency on most systems
    nanosleep(&processinfo->triggerdelay, NULL);
    processinfo->triggerstreamcnt++;

    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_RECEIVED;

    return RETURN_SUCCESS;
  }

  if(processinfo->triggermode == PROCESSINFO_TRIGGERMODE_SEMAPHORE ||
      processinfo->triggermode == PROCESSINFO_TRIGGERMODE_SEMAPHORE_PROP_TIMEOUTS) {
    if(image == NULL) return RETURN_FAILURE;

    int semr;
    int tmpstatus = PROCESSINFO_TRIGGERSTATUS_RECEIVED;

    DEBUG_TRACEPOINT("wait on semaphore");

    processinfo->triggerstatus = PROCESSINFO_TRIGGERSTATUS_WAITING;

    // get current time
    struct timespec ts;
    if(clock_gettime(CLOCK_MILK, &ts) == -1) {
      perror("clock_gettime");
      exit(EXIT_FAILURE);
    }

    // is semaphore at zero ?
    DEBUG_TRACEPOINT("test sem status");
    semr = 0;
    while(semr == 0) {
      // this should only run once, returning semr = -1 with errno = EAGAIN
      // otherwise, we're potentially missing frames
      DEBUG_TRACEPOINT("sem_trywait");
      semr = ImageStreamIO_semtrywait(image, processinfo->triggersem);
      if(semr == 0) {
        processinfo->triggermissedframe++;
      }
    }

    // expected state: NBmissedframe = 0, semr = -1, errno = EAGAIN
    // missed frame state: NBmissedframe>0, semr = -1, errno = EAGAIN
    DEBUG_TRACEPOINT("triggermissedframe = %d",
                     processinfo->triggermissedframe);
    if(processinfo->triggermissedframe == 0) {
      DEBUG_TRACEPOINT("timedwait");
      // add timeout
      ts.tv_sec += processinfo->triggertimeout.tv_sec;
      ts.tv_nsec += processinfo->triggertimeout.tv_nsec;
      while(ts.tv_nsec > 1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec++;
      }

      semr = ImageStreamIO_semtimedwait(image, processinfo->triggersem, &ts);
      if(semr == -1) {
        if(errno == ETIMEDOUT) {
          // timeout condition
          processinfo->trigggertimeoutcnt++;
          tmpstatus = PROCESSINFO_TRIGGERSTATUS_TIMEDOUT;
        }
      } else {
        tmpstatus = PROCESSINFO_TRIGGERSTATUS_RECEIVED;
      }
    }

    processinfo->triggermissedframe_cumul +=
      processinfo->triggermissedframe;

    processinfo->triggerstatus = tmpstatus;

    return RETURN_SUCCESS;
  }

  return RETURN_FAILURE;
}

/**
 * @brief Update output stream metadata and telemetry.
 *
 * This function updates the output image's shared memory metadata, including
 * write PID, timestamp, and propagates processing trace from input to output.
 *
 * @param[in,out] processinfo  Pointer to the PROCESSINFO structure.
 * @param[in,out] output_image Pointer to the output stream's IMAGE.
 * @param[in]     input_image  Pointer to the input stream's IMAGE.
 * @return RETURN_SUCCESS on success.
 */
errno_t processinfo_update_output_stream(
  PROCESSINFO *processinfo,
  IMAGE        *output_image,
  IMAGE        *input_image
) {
  if(output_image == NULL) {
    return RETURN_FAILURE;
  }

  if(output_image->md->shared == 1) {
    // Always update PID and timestamp, regardless of processinfo status
    struct timespec ts;
    if(clock_gettime(CLOCK_MILK, &ts) == -1) {
      perror("clock_gettime");
      exit(EXIT_FAILURE);
    }

    output_image->streamproctrace[0].procwrite_PID = getpid();
    output_image->streamproctrace[0].ts_streamupdate = ts;

    DEBUG_TRACEPOINT(" ");

    if(processinfo != NULL) {
      // If input_image is NULL, try to use processinfo->trigger_image
      if(input_image == NULL) {
        if(processinfo->trigger_image != NULL) {
          input_image = processinfo->trigger_image;
        }
      }

      if(input_image != NULL) {
        int sptisize = input_image->md[0].NBproctrace - 1;
        // Ensure we don't overflow output trace
        int sptosize = output_image->md[0].NBproctrace - 1;
        if(sptisize > sptosize) {
          sptisize = sptosize;
        }

        if(sptisize > 0) {
          // copy streamproctrace from input to output
          memcpy(&output_image->streamproctrace[1],
                 &input_image->streamproctrace[0],
                 sizeof(STREAM_PROC_TRACE) * sptisize);
        }
      }

      // write first streamproctrace entry
      DEBUG_TRACEPOINT("trigger info");
      output_image->streamproctrace[0].trigsemindex =
        processinfo->triggermode;

      output_image->streamproctrace[0].trigger_inode =
        processinfo->triggerstreaminode;

      output_image->streamproctrace[0].ts_procstart =
        processinfo->texecstart[processinfo->timerindex];

      output_image->streamproctrace[0].trigsemindex =
        processinfo->triggersem;

      output_image->streamproctrace[0].triggerstatus =
        processinfo->triggerstatus;

      if(input_image != NULL) {
        output_image->streamproctrace[0].cnt0 =
          input_image->md[0].cnt0;
      }
    }

    DEBUG_TRACEPOINT(" ");
  }

  ImageStreamIO_UpdateIm(output_image);

  return RETURN_SUCCESS;
}

int processinfo_exec_start(PROCESSINFO *processinfo) {
  DEBUG_TRACEPOINT(" ");
  if(processinfo->MeasureTiming == 1) {

    processinfo->timerindex++;
    if(processinfo->timerindex == PROCESSINFO_NBtimer) {
      processinfo->timerindex = 0;
      processinfo->timingbuffercnt++;
    }

    clock_gettime(CLOCK_MILK,
                  &processinfo->texecstart[processinfo->timerindex]);

    if(processinfo->dtiter_limit_enable != 0) {
      long dtiter;
      int  timerindexlast;

      if(processinfo->timerindex == 0) {
        timerindexlast = PROCESSINFO_NBtimer - 1;
      } else {
        timerindexlast = processinfo->timerindex - 1;
      }

      dtiter = processinfo->texecstart[processinfo->timerindex].tv_nsec -
               processinfo->texecstart[timerindexlast].tv_nsec;
      dtiter += 1000000000 *
                (processinfo->texecstart[processinfo->timerindex].tv_sec -
                 processinfo->texecstart[timerindexlast].tv_sec);

      if(dtiter > processinfo->dtiter_limit_value) {
        char msgstring[STRINGMAXLEN_PROCESSINFO_STATUSMSG];

        {
          int slen =
            snprintf(msgstring,
                     STRINGMAXLEN_PROCESSINFO_STATUSMSG,
                     "dtiter %4ld  %4d %6.1f us  > %6.1f us",
                     processinfo->dtiter_limit_cnt,
                     processinfo->timerindex,
                     0.001 * dtiter,
                     0.001 * processinfo->dtiter_limit_value);
          if(slen < 1) {
            PRINT_ERROR("snprintf wrote <1 char");
            abort(); // can't handle this error any other way
          }
          if(slen >= STRINGMAXLEN_PROCESSINFO_STATUSMSG) {
            PRINT_ERROR("snprintf string truncation");
            abort(); // can't handle this error any other way
          }
        }

        processinfo_WriteMessage(processinfo, msgstring);

        if(processinfo->dtiter_limit_enable ==
            2) { // pause process due to timing limit
          processinfo->CTRLval = 1;
          snprintf(msgstring, STRINGMAXLEN_PROCESSINFO_STATUSMSG, "dtiter lim -> paused");
          processinfo_WriteMessage(processinfo, msgstring);
        }
        processinfo->dtiter_limit_cnt++;
      }
    }
  }
  DEBUG_TRACEPOINT(" ");
  return 0;
}

/**
 * @brief Log an error and perform a clean exit.
 *
 * This function is used to handle fatal loop errors. it sets the loop
 * status to ERROR, writes the error message to SHM, and then calls
 * `processinfo_cleanExit` to detach.
 */
errno_t processinfo_error(PROCESSINFO *processinfo, char *errmsgstring) {
  processinfo->loopstat = 4; // ERROR // TODO ENUM
  processinfo_WriteMessage(processinfo, errmsgstring);
  processinfo_cleanExit(processinfo);
  return RETURN_SUCCESS;
}

/**
 * @brief Finalize process initialization and enter active loop state.
 *
 * This function should be called just before entering the main processing loop.
 * It resets the loop counter, sets status to ACTIVE, and attempts to set
 * the process's real-time scheduler priority if configured.
 */
errno_t processinfo_loopstart(PROCESSINFO *processinfo) {
  processinfo->loopcnt  = 0;
  processinfo->loopstat = 1;

  if(processinfo->RT_priority > -1) {
    struct sched_param schedpar;
    schedpar.sched_priority = processinfo->RT_priority;

    if (sched_setscheduler(0, SCHED_FIFO, &schedpar) != 0) {
      // perror("sched_setscheduler");
    }
  }

  return RETURN_SUCCESS;
}

/**
 * @brief Return loop status
 *
 * 0 if loop should exit
 * 1 otherwise
 *
 * @param processinfo
 * @return int loop status
 */
int processinfo_loopstep(PROCESSINFO *processinfo) {
  int loopstatus = 1;

  // TODO ENUM VALUES
  while(processinfo->CTRLval == 1) { // pause
    usleep(50);
  }
  if(processinfo->CTRLval == 2) { // single iteration
    processinfo->CTRLval = 1;
  }
  if(processinfo->CTRLval == 3) { // exit loop
    loopstatus = 0;
  }

  if (GLOBAL_signal_received && (GLOBAL_signal_received_signo == SIGINT || GLOBAL_signal_received_signo == SIGHUP)) {
    // Ctrl+C --> SIGINT
    // Term closed --> SIGHUP
    loopstatus = 0;
  }

  if(processinfo->loopcntMax != -1)
    if(processinfo->loopcnt >= processinfo->loopcntMax - 1) {
      loopstatus = 0;
    }

  return loopstatus;
}


int processinfo_exec_end(PROCESSINFO *processinfo) {
  int loopOK = 1;

  DEBUG_TRACEPOINT("End of execution loop, measure timing = %d",
                   processinfo->MeasureTiming);
  if(processinfo->MeasureTiming == 1) {
    clock_gettime(CLOCK_MILK,
                  &processinfo->texecend[processinfo->timerindex]);

    if(processinfo->dtexec_limit_enable != 0) {
      long dtexec;

      dtexec = processinfo->texecend[processinfo->timerindex].tv_nsec -
               processinfo->texecstart[processinfo->timerindex].tv_nsec;
      dtexec += 1000000000 *
                (processinfo->texecend[processinfo->timerindex].tv_sec -
                 processinfo->texecend[processinfo->timerindex].tv_sec);

      if(dtexec > processinfo->dtexec_limit_value) {
        char msgstring[STRINGMAXLEN_PROCESSINFO_STATUSMSG];

        {
          int slen =
            snprintf(msgstring,
                     STRINGMAXLEN_PROCESSINFO_STATUSMSG,
                     "dtexec %4ld  %4d %6.1f us  > %6.1f us",
                     processinfo->dtexec_limit_cnt,
                     processinfo->timerindex,
                     0.001 * dtexec,
                     0.001 * processinfo->dtexec_limit_value);
          if(slen < 1) {
            PRINT_ERROR("snprintf wrote <1 char");
            abort(); // can't handle this error any other way
          }
          if(slen >= STRINGMAXLEN_PROCESSINFO_STATUSMSG) {
            PRINT_ERROR("snprintf string truncation");
            abort(); // can't handle this error any other way
          }
        }
        processinfo_WriteMessage(processinfo, msgstring);

        if(processinfo->dtexec_limit_enable ==
            2) { // pause process due to timing limit
          processinfo->CTRLval = 1;
          {
            int slen = snprintf(msgstring,
                                STRINGMAXLEN_PROCESSINFO_STATUSMSG,
                                "dtexec lim -> paused");
            if(slen < 1) {
              PRINT_ERROR("snprintf wrote <1 char");
              abort(); // can't handle this error any other way
            }
            if(slen >= STRINGMAXLEN_PROCESSINFO_STATUSMSG) {
              PRINT_ERROR(
                "snprintf string "
                "truncation");
              abort(); // can't handle this error any other way
            }
          }

          processinfo_WriteMessage(processinfo, msgstring);
        }
        processinfo->dtexec_limit_cnt++;
      }
    }
  }
  DEBUG_TRACEPOINT("End of execution loop: check signals");
  loopOK = processinfo_ProcessSignals(processinfo);

  processinfo->loopcnt++;

  return loopOK; // returns 0 if signal stops loop
}


void processinfo_sig_handler(int signo) {
  // NOTE: in case multiple signals are received
  // SIGTERM prevails, then SIGINT, then the latest one
  // If we want to filter precedence for more signals, then make this a loop.
  if ((GLOBAL_signal_received && GLOBAL_signal_received_signo == SIGTERM) || signo == SIGTERM) {
    GLOBAL_signal_received = 1;
    GLOBAL_signal_received_signo = SIGTERM;
    return;
  }
  if ((GLOBAL_signal_received && GLOBAL_signal_received_signo == SIGINT) || signo == SIGINT) {
    GLOBAL_signal_received = 1;
    GLOBAL_signal_received_signo = SIGINT;
    return;
  }
  GLOBAL_signal_received = 1;
  GLOBAL_signal_received_signo = signo;
}


int processinfo_CatchSignals() {
  struct sigaction sigact;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigact.sa_handler = processinfo_sig_handler;

  if(sigaction(SIGTERM, &sigact, NULL) == -1) printf("\nCan't catch SIGTERM\n");
  if(sigaction(SIGINT, &sigact, NULL) == -1) printf("\nCan't catch SIGINT\n");
  if(sigaction(SIGABRT, &sigact, NULL) == -1) printf("\nCan't catch SIGABRT\n");
  if(sigaction(SIGBUS, &sigact, NULL) == -1) printf("\nCan't catch SIGBUS\n");
  if(sigaction(SIGSEGV, &sigact, NULL) == -1) printf("\nCan't catch SIGSEGV\n");
  if(sigaction(SIGHUP, &sigact, NULL) == -1) printf("\nCan't catch SIGHUP\n");
  if(sigaction(SIGPIPE, &sigact, NULL) == -1) printf("\nCan't catch SIGPIPE\n");

  return 0; // TODO change the return types to errno?
}

int processinfo_ProcessSignals(PROCESSINFO *processinfo) {
  int continue_exec = 1;

  if (GLOBAL_signal_received == 0) return continue_exec;

  switch (GLOBAL_signal_received_signo) {
  case SIGTERM:
  case SIGINT:
  case SIGABRT:
  case SIGBUS:
  case SIGSEGV:
  case SIGHUP:
  case SIGPIPE:
    continue_exec = 0;
    processinfo_SIGexit(processinfo, GLOBAL_signal_received_signo);
    break;
  default:
    break;
  }
  return continue_exec;
}


int processinfo_SIGexit(PROCESSINFO *processinfo, int signal_number) {
  char            timestring[200];
  struct timespec tstop;
  struct tm      *tstoptm;
  char            msgstring[STRINGMAXLEN_PROCESSINFO_STATUSMSG];

  clock_gettime(CLOCK_MILK, &tstop);
  tstoptm = gmtime(&tstop.tv_sec);

  snprintf(timestring,
           200,
           "%02d:%02d:%02d.%03d",
           tstoptm->tm_hour,
           tstoptm->tm_min,
           tstoptm->tm_sec,
           (int)(0.000001 * (tstop.tv_nsec)));
  processinfo->loopstat = 3; // clean exit

  int slen = snprintf(msgstring,
                      STRINGMAXLEN_PROCESSINFO_STATUSMSG,
                      "%s at %s",
                      sigabbrev_np(GLOBAL_signal_received_signo),
                      timestring);
  // TODO slen error macro to avoid all of this
  if(slen < 1) {
    PRINT_ERROR("snprintf wrote <1 char");
    abort(); // can't handle this error any other way
  }
  if(slen >= STRINGMAXLEN_PROCESSINFO_STATUSMSG) {
    PRINT_ERROR("snprintf string truncation");
    abort(); // can't handle this error any other way
  }

  processinfo_WriteMessage(processinfo, msgstring);

  return 0;
}

int processinfo_cleanExit(PROCESSINFO *processinfo) {
  // TODO loopstat emums
  if(processinfo->loopstat != 4) {
    struct timespec tstop;
    struct tm      *tstoptm;
    char            msgstring[STRINGMAXLEN_PROCESSINFO_STATUSMSG];

    clock_gettime(CLOCK_MILK, &tstop);
    tstoptm = gmtime(&tstop.tv_sec);

    if(processinfo->CTRLval == 3) { // loop exit from processinfo control
      snprintf(msgstring,
               STRINGMAXLEN_PROCESSINFO_STATUSMSG,
               "CTRLexit %02d:%02d:%02d.%03d",
               tstoptm->tm_hour,
               tstoptm->tm_min,
               tstoptm->tm_sec,
               (int)(0.000001 * (tstop.tv_nsec)));
      strncpy(processinfo->statusmsg,
              msgstring,
              STRINGMAXLEN_PROCESSINFO_STATUSMSG - 1);
    }

    if(processinfo->loopstat == 1) {
      snprintf(msgstring,
               STRINGMAXLEN_PROCESSINFO_STATUSMSG,
               "Loop exit %02d:%02d:%02d.%03d",
               tstoptm->tm_hour,
               tstoptm->tm_min,
               tstoptm->tm_sec,
               (int)(0.000001 * (tstop.tv_nsec)));
      strncpy(processinfo->statusmsg,
              msgstring,
              STRINGMAXLEN_PROCESSINFO_STATUSMSG - 1);
    }

    processinfo->loopstat = 3; // clean exit
  }

  // Remove processinfo shm file on clean exit
  processinfo_shm_delete(processinfo);


  return 0;
}
