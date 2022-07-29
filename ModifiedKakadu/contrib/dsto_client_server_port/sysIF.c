/*############################################################################
## (c) Copyright Commonwealth of Australia 1993 - 2005
## This work is copyright.  Apart from any use permitted under the Copyright
## Act 1968, no part may be reproduced by any process without prior written
## permission from the Commonwealth.
## Direct requests or inquiries in relation to reproduction to:
##              Chief Surveillance System Division
##              DSTO Electronics and Surveillance Research Laboratory
##              P.O. Box 1500, Salisbury, SA 5108
##              AUSTRALIA
## For details of the terms and conditions under which this work may be
## used by Kakadu Licensees, refer to the accompanying file,
## "License-to-use-DSTO-port.txt".
##
## Modified by Taubman, following suggestions from Scott Batchelor, for v6.1
############################################################################*/

/* Stubs / definitions to replace MSVC defs */

#include "sysIF.h"

/* may have debug macros around functions which we don't want defined here */
#ifdef sysWaitForEvent
#undef sysWaitForEvent
#endif
#ifdef sysCreateEvent
#undef sysCreateEvent
#endif
#ifdef sysSetEvent
#undef sysSetEvent
#endif
#ifdef sysResetEvent
#undef sysResetEvent
#endif
#ifdef sysEventSelect
#undef sysEventSelect
#endif
#ifdef sysDestroyEvent
#undef sysDestroyEvent
#endif
#ifdef sysDestroyMutex
#undef sysDestroyMutex
#endif
#ifdef sysKillThread
#undef sysKillThread
#endif
#ifdef sysDestroyThread
#undef sysDestroyThread
#endif

struct tms sysTimesBuffer;

static long sysTicksPerSec = 0;
static long sysTimeBase = 0;

/* get a system dependent value equivalent to CLK_TCK (or CLOCKS_PER_SEC) */
/* for use with clock() or times() calls.                                 */
long sysGetTicksPerSec(void)
{
  if (!sysTicksPerSec) sysTicksPerSec = sysconf(_SC_CLK_TCK);

  return sysTicksPerSec;
}

/* Get the time in milliseconds since the first call to sysClock */
/* This is similar to clock() but time is actual elapsed */
/* in milliseconds (not microseconds of CPU).            */
clock_t sysClock(void)
{
  if (!sysTicksPerSec) {
    sysTicksPerSec = sysconf(_SC_CLK_TCK);
    sysTimeBase = times(&sysTimesBuffer);
  }

  return (clock_t)((1000.0 * (times(&sysTimesBuffer) - sysTimeBase)) /
                   sysTicksPerSec);
}


/* Create a new event object */
HANDLE sysCreateEvent(BOOL manualReset)
{
  tsysEvent          *newEvent;
  pthread_mutexattr_t mxAttr;
  int                 status;

  if (!(newEvent = (tsysEvent *) calloc(1,sizeof(tsysEvent)))) OUT_OF_MEM;

  /* set mutex to recursive to avoid deadlock within thread */
  pthread_mutexattr_init(&mxAttr);
  pthread_mutexattr_settype(&mxAttr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutexattr_setpshared(&mxAttr, PTHREAD_PROCESS_PRIVATE);
  if ((status = pthread_mutex_init(&newEvent->mutex, &mxAttr))) {
    fprintf(stderr, "\nError: "
            "Unable to init mutex: %s\n", strerror(status));
    pthread_mutexattr_destroy(&mxAttr);
    free(newEvent);
    return NULL;
  }
  pthread_mutexattr_destroy(&mxAttr);

  for (unsigned int pollDataIndex(0); pollDataIndex < MAX_EVENT_SOCKETS;
       pollDataIndex++)
    newEvent->pollData[pollDataIndex].fd = INVALID_SOCKET;

  newEvent->manualReset = manualReset;

  return (HANDLE)newEvent;
}


/* Release the memory associatted with an event - note no checks are made as */
/* to whether the event is being used by another thread.                     */
BOOL sysDestroyEvent(HANDLE *hEvent)
{
  tsysEvent *event = (tsysEvent *)*hEvent;

  if (!hEvent || !*hEvent) return FALSE;

  pthread_mutex_destroy(&(event->mutex));

  free(*hEvent);
  *hEvent = NULL;

  return TRUE;
}

/* Set an events state to signalled */
BOOL sysSetEvent(HANDLE hEvent)
{
  tsysEvent *event = (tsysEvent *)hEvent;

  if (!hEvent) return FALSE;

  if (pthread_mutex_lock(&event->mutex)) return FALSE;
  event->userSignal = TRUE;
  pthread_mutex_unlock(&event->mutex);

  return TRUE;
}

/* Clear a signalled event - only required for "manualReset" events */
BOOL sysResetEvent(HANDLE hEvent)
{
  tsysEvent *event = (tsysEvent *)hEvent;

  if (!hEvent) return FALSE;

  if (pthread_mutex_lock(&event->mutex)) return FALSE;
  event->userSignal = FALSE;
  
  for (unsigned int pollDataIndex(0); pollDataIndex < MAX_EVENT_SOCKETS;
       pollDataIndex++)
    event->pollData[pollDataIndex].revents = 0;
  
  pthread_mutex_unlock(&event->mutex);

  return TRUE;
}


/* this is not the same as WSAEventSelect  - the socket is related to */
/* the event, not vice versa and at most 2 sockets may be watched.    */
int sysEventSelect(SOCKET sID, HANDLE hEventObject, long lNetworkEvents)
{
  tsysEvent *event = (tsysEvent *)hEventObject;
  int     slotNo = -1;

  if (!event) return SOCKET_ERROR;

  if (pthread_mutex_lock(&event->mutex)) return SOCKET_ERROR;
  
  /* see if the socket is already being watched */
  for (unsigned int pollDataIndex(0); pollDataIndex < MAX_EVENT_SOCKETS; 
       ++pollDataIndex)
    {
      if (event->pollData[pollDataIndex].fd == sID) 
        {
          slotNo = pollDataIndex;
          break;
        } 
    }

  if (slotNo == -1) {
    /* neither of the tracked sockets */

    /* may be simply resetting a socket */
    if (!lNetworkEvents) {
      pthread_mutex_unlock(&event->mutex);
      return 0;
    }

    /* find an unused slot */
    for (unsigned int pollDataIndex(0); pollDataIndex < MAX_EVENT_SOCKETS; 
         ++pollDataIndex)
      {
        if (event->pollData[pollDataIndex].fd == INVALID_SOCKET) 
          {
            slotNo = pollDataIndex;
            break;
          }
      }
    if (slotNo == -1) // No slots left.
      { // No slots left
        fprintf(stderr,
                "kdu_server error: Maximum number of sockets (%d) exceeded.  "
                "Rebuild kdu_server with a larger value for MAX_EVENT_SOCKETS "
                "in \"sysIF.h\".\n",MAX_EVENT_SOCKETS);
        pthread_mutex_unlock(&event->mutex);
        return SOCKET_ERROR;
      }
  }

  /* have a slot to work with */
  if (!event->manualReset) event->pollData[slotNo].revents = 0;
  event->pollData[slotNo].events = lNetworkEvents;
  /* may be resetting an existing socket */
  event->pollData[slotNo].fd = lNetworkEvents ? sID : INVALID_SOCKET;
  pthread_mutex_unlock(&event->mutex);

  return 0;
}


/* Suspent the current threads execution until a specified event is signalled */
DWORD sysWaitForEvent(HANDLE hEvent, int milliTimeout)
{
  tsysEvent *event = (tsysEvent *)hEvent;
  int        status = 0;
  int        milliTicks = 0;
  int        startSlot = -1;
  int        slotCount = 0;
  struct timespec sleepTime = {0};

/* Note: "poll" has a minimum wait time of 1ms so the wait loop below
 *       is actually in ~2ms steps.
 */

  if (!event) return SYS_WAIT_EVENT_0;

  /* may be waiting on either a socket event OR a user event */

  /* quick check for a user signal */
  if (event->userSignal) {
    if (!event->manualReset) event->userSignal = FALSE;
    return SYS_WAIT_EVENT_0;
  }

  /* check for sockets */
  /* note: checked in reverse order */
  for (int pollDataIndex(MAX_EVENT_SOCKETS-1); pollDataIndex >= 0; 
       --pollDataIndex)
    {
      if (event->pollData[pollDataIndex].fd != INVALID_SOCKET) 
        {
          startSlot = pollDataIndex;
          slotCount++;
          if (!event->manualReset)
            event->pollData[pollDataIndex].revents = 0;
        }
    }
  
  do {
    if (slotCount) {
      
      if (pthread_mutex_lock(&event->mutex))
        return SYS_WAIT_FAILED;
      status = poll(&event->pollData[startSlot], slotCount, 1);

      if (status > 0) {
     
        /* NB: invalid socket gives return status of 1 !! */

        /* check for invalid (closed) sockets */
        for (unsigned int pollDataIndex(0); pollDataIndex < MAX_EVENT_SOCKETS; 
             ++pollDataIndex)
          {
            if ((event->pollData[pollDataIndex].fd != INVALID_SOCKET) &&
                (event->pollData[pollDataIndex].revents & SYS_INVALID)) 
              {
                /* Treat as a HUP */
                event->pollData[pollDataIndex].revents = ~SYS_INVALID;
                event->pollData[pollDataIndex].revents |= SYS_CLOSE;
              }
          }

        /* may have got a HUP when we weren't looking for one - error. */
        for (unsigned int pollDataIndex(0); pollDataIndex < MAX_EVENT_SOCKETS; 
             ++pollDataIndex)
          {
            if ((event->pollData[pollDataIndex].revents & SYS_CLOSE &
                 ~event->pollData[pollDataIndex].events))
              {
                pthread_mutex_unlock(&event->mutex);
                return SYS_WAIT_FAILED;
              }
          }
        
        if (!event->manualReset) 
          {
            for (unsigned int pollDataIndex(0); pollDataIndex < MAX_EVENT_SOCKETS; 
                 ++pollDataIndex)
              {
                event->pollData[pollDataIndex].events &= 
                ~event->pollData[pollDataIndex].revents;
              }
            event->userSignal = FALSE;
          }
        pthread_mutex_unlock(&event->mutex);

        return SYS_WAIT_EVENT_0;
      } else if (status < 0) {
        /* error from poll */
        pthread_mutex_unlock(&event->mutex);
        return SYS_WAIT_FAILED;
      }

      /* status == 0 ie poll timed out */
      pthread_mutex_unlock(&event->mutex);

      /* have a little nap to give other threads a look in */
      sleepTime.tv_nsec = 1000000;
      nanosleep(&sleepTime, NULL);
    } else {
      /* manual event only */
      sleepTime.tv_nsec = 2000000;
      nanosleep(&sleepTime, NULL);
    }

    if (event->userSignal) {
      if (!event->manualReset) event->userSignal = FALSE;

      return SYS_WAIT_EVENT_0;
    }
/* See Note and poll/nanosleeps above re increment of 2ms */
  } while (milliTimeout < 0 || (milliTicks += 2) <= milliTimeout);

  return SYS_WAIT_TIMEOUT;
}

/* Set and clear an event to alow other threads waiting on it to continue.
 * This is a depricated function, MSDN has this to say about its version:
 *   This function is unreliable and should not be used.
 *   It exists mainly for backward compatibility.
 *   A thread waiting on a synchronization object can be momentarily removed
 *   from the wait state by a kernel-mode APC, and then returned to the wait
 *   state after the APC is complete. If the call to PulseEvent occurs during
 *   the time when the thread has been removed from the wait state, the thread
 *   will not be released because PulseEvent releases only those threads that
 *   are waiting at the moment it is called. Therefore, PulseEvent is
 *   unreliable and should not be used by new applications.
 * And...
 *   Note that for a thread using the multiple-object wait functions to wait for
 *   all specified objects to be signaled, PulseEvent can set the event object's
 *   state to signaled and reset it to nonsignaled without causing the wait
 *   function to return. This happens if not all of the specified objects are
 *   simultaneously signaled
 */
int sysPulseEvent(HANDLE hEvent)
{
  tsysEvent       *event = (tsysEvent *)hEvent;
  struct timespec  sleepTime = {0, SYS_PULSE_USECS * 1000};

  /* behavior here varies between manual and auto reset event types beacuse */
  /* an auto-reset event will be cleared by each thread waiting on it.      */

  /* This is a bit tricky because we don't really know how many threads   */
  /* are actually waiting on the event, so we just give them a predefined */
  /* amount of time and hope that we get them all.                        */

  do {
    sysSetEvent(hEvent);
    nanosleep(&sleepTime, NULL);
  } while (!event->userSignal); /* other thread(s) may have auto-reset it */
  sysResetEvent(hEvent);

  return 0;
}


int closesocket(SOCKET s)
{
  return close(s);
}


/* Create a new mutual exclusion lock */
HANDLE sysCreateMutex(void)
{
  pthread_mutexattr_t  mxAttr;
  pthread_mutex_t     *newMutex;

  newMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
  pthread_mutexattr_init(&mxAttr);
  /* set mutex to recursive to avoid deadlock within thread */
  pthread_mutexattr_settype(&mxAttr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutexattr_setpshared(&mxAttr, PTHREAD_PROCESS_PRIVATE);
  pthread_mutex_init(newMutex, &mxAttr);
  pthread_mutexattr_destroy(&mxAttr);

  return (HANDLE)newMutex;
}

/* release the memory associated with a mutual exclusion lock */
/* Equivalent to the windows function CloseHandle when applied to a mutex */
/* Note: No check of whether the mutex is locked is performed */
void sysDestroyMutex(HANDLE *mutex)
{
  if (*mutex) {
    pthread_mutex_destroy((pthread_mutex_t *)*mutex);
    free(*mutex);
    *mutex = NULL;
  }
  return;
}


/* start a new execution thread and return an unsignalled event handle for it */
HANDLE sysStartThread(void *(*start_routine)(void*), void *arg,
                      BOOL raisePriority)
{
  struct sched_param schedParams;
  tsysThread        *newThread;

  newThread = (tsysThread *) calloc(1, sizeof(tsysThread));

  pthread_attr_init(&(newThread->threadAttrs));
  pthread_attr_setschedpolicy(&(newThread->threadAttrs), SCHED_OTHER);
  if (raisePriority) {
    /* changing the priority of a thread is only allowed under SunOS so */
    /* returned error values in the following are ignored */
    pthread_attr_getschedparam(&(newThread->threadAttrs), &schedParams);
    schedParams.sched_priority++;
    pthread_attr_setschedparam(&(newThread->threadAttrs), &schedParams);
  }

  pthread_create(&(newThread->threadID), &(newThread->threadAttrs),
                 start_routine, arg);

  return (HANDLE) newThread;
}


/* kill an existing thread and flag its closure */
void sysKillThread(HANDLE hThread)
{
  tsysThread *thread = (tsysThread *) hThread;

  /* notify other interested threads of its closure */
  sysSetEvent(hThread);

  pthread_kill(thread->threadID, SIGTERM);

  return;
}

/* kill a thread and free its handle */
void sysDestroyThread(HANDLE *hThread)
{
  tsysThread *thread = (tsysThread *) *hThread;

  sysKillThread(*hThread);

  pthread_attr_destroy(&(thread->threadAttrs));
  sysDestroyEvent(hThread);
}

/* wait for a child thread to complete */
void sysWaitForThreadEnd(HANDLE hThread)
{
  tsysThread *thread = (tsysThread *) hThread;

  pthread_join(thread->threadID, NULL);
}
