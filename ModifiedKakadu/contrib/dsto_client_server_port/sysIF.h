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

#ifndef __SYS_IF_H_
#define __SYS_IF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/times.h>

#define MAX_EVENT_SOCKETS 50 // Limits the number of supported clients
  
extern int errno;
extern struct tms sysTimesBuffer;

typedef unsigned long DWORD;
typedef DWORD BOOL;
typedef void *HANDLE;

#ifndef FALSE
#define FALSE (0)
#define TRUE  (1 == 1)
#endif

#define OUT_OF_MEM \
  do { \
    fprintf(stderr, "Out of memory at %s:%d", __FILE__,__LINE__); \
    abort(); \
  } while (0)


#define INFTIM             (-1)
#define INFINITE           INFTIM

#define SYS_WAIT_EVENT_0   (0l)
#define SYS_WAIT_TIMEOUT   (258l)
#define SYS_WAIT_FAILED    ((DWORD)0xFFFFFFFF)

//#define SD_RECEIVE 0x00
//#define SD_SEND    0x01
#define SD_BOTH    0x02

typedef struct hostent HOSTENT;

#define SYS_READ    POLLIN
#define SYS_WRITE   POLLOUT
#define SYS_CONNECT POLLOUT
#define SYS_CLOSE   POLLHUP
#define SYS_INVALID POLLNVAL

/* event may be triggered by at most `MAX_EVENT_SOCKETS sockets */
typedef struct tsysEvent {
  pthread_mutex_t     mutex; /* This MUST be first so events and mutexes */
                             /* can be used interchangeably.             */
  struct pollfd       pollData[MAX_EVENT_SOCKETS];
  BOOL                manualReset;
  BOOL                userSignal;
} tsysEvent;


/* struct for managing a thread */
typedef struct {
  tsysEvent      closeFlag; /* event so parent thread can wait for completion */
  pthread_t      threadID;
  pthread_attr_t threadAttrs;
} tsysThread;


typedef HANDLE WSAEVENT;

typedef int SOCKET;

typedef char IOCTL_TYPE;
typedef void SOCKOPT_TYPE;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;

#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

#ifndef INADDR_NONE
#define INADDR_NONE (0xFFFFFFFF)
#endif


int closesocket(SOCKET s);
#define ioctlsocket ioctl

#define DEBUG(...) \
  (fprintf(stderr, "DEBUG[%d@%s:%d] ", (int)pthread_self(),__FILE__,__LINE__), \
   fprintf(stderr, __VA_ARGS__), \
   fputc('\n', stderr))

#define sysGetLastError() errno

/* get a system dependent value equivalent to CLK_TCK (or CLOCKS_PER_SEC) */
/* for use with clock() or times() calls.                                 */
long sysGetTicksPerSec(void);

#define SYS_CLOCKS_PER_SEC 1000

/* Get the time in milliseconds since the first call to sysClock */
/* This is similar to clock() but time is actual elapsed */
/* in milliseconds (not microseconds of CPU).            */
clock_t sysClock(void);

/* under unix the clock() call returns cpu time only in CLOCKS_PER_SEC not  */
/* actual elapsed time. Convert calls to the times func which gives a total */
//#define sysClock() times(&sysTimesBuffer)
/* and just in case ... */
/* find usage of calls to clock in external headers: */
//#define clock() clock_is_nonportable_and_should_not_be_used()
/* or when happy: */
#define clock() sysClock()

/* Note the prototypes here are intentionally different to the windows versions
 * to force compiler errors where code has not been ported.
 */

/* Create a new event object */
HANDLE sysCreateEvent(BOOL manualReset);

/* Set an events state to signalled */
BOOL   sysSetEvent(HANDLE hEvent);

/* Clear a signalled event - only required for "manualReset" events */
BOOL   sysResetEvent(HANDLE hEvent);

/* release the memory associatted with an event - note no checks are made as */
/* to whether the event is being used by another thread.                     */
BOOL   sysDestroyEvent(HANDLE *hEvent);

/* Suspent the current threads execution until a specified event is signalled */
DWORD  sysWaitForEvent(HANDLE hEvent, int milliTimeout);

/* specify the socket events of interest prior to call of sysWaitForEvent */
/* Note: This is not the same as MSs WSAEventSelect - the sockets are */
/* related to the event, not vice versa.                              */
int    sysEventSelect(SOCKET s, HANDLE hEventObject, long lNetworkEvents);

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
 *   Note that for a thread using the multiple-object wait functions to wait for *   all specified objects to be signaled, PulseEvent can set the event object's *   state to signaled and reset it to nonsignaled without causing the wait
 *   function to return. This happens if not all of the specified objects are
 *   simultaneously signaled
 */
int sysPulseEvent(HANDLE hEvent);

/* Define the wait time in microseconds between set and reset of above */
/* Note: Actual time may be multiplied by the number of waiting threads */
#define SYS_PULSE_USECS 2000


/* Create a new mutual exclusion lock */
HANDLE sysCreateMutex(void);

#if 0
/* some deadlock detection debugging macros */
#define pthread_mutex_lock(mx) \
  (DEBUG("Lock    %X", (int)mx), pthread_mutex_lock(mx))
#define pthread_mutex_unlock(mx) \
  (DEBUG("Release %X", (int)mx), pthread_mutex_unlock(mx))
#endif

/* Acquire a mutex when it becomes available */
/* Replaces windows "WaitForSingleObject" */
#define sysLockMutex(mx) pthread_mutex_lock((pthread_mutex_t *)mx)

/* decrease the usage count and/or release a mutex */
/* Equivalent to windows version */
#define sysReleaseMutex(mx) pthread_mutex_unlock((pthread_mutex_t *)mx)

/* release the memory associated with a mutual exclusion lock */ 
/* Equivalent to the windows function CloseHandle when applied to a mutex */
/* Note: No check of whether the mutex is locked is performed */
void sysDestroyMutex(HANDLE *mutex);

/* start a new execution thread and return an unsignalled event handle for it */
HANDLE sysStartThread(void *(*start_routine)(void*), void *arg,
                      BOOL raisePriority);

/* wait for a child thread to complete */
void sysWaitForThreadEnd(HANDLE hThread);

/* kill an existing thread and flag its closure */
void sysKillThread(HANDLE hThread);

/* kill a thread and free its handle */
void sysDestroyThread(HANDLE *hThread);


/* some aliases for windows */
#define SetEvent sysSetEvent


/* some general debug macros for sys calls */

/* #define sysWaitForEvent(e,t) \
 (DEBUG("%X clock:%d Wait %dms",(int)e,(int)sysClock(),t),sysWaitForEvent(e,t))
*/
/* #define sysCreateEvent(m) \
  (DEBUG("Create event Manual: %d", m), sysCreateEvent(m))
*/
/*#define sysSetEvent(e) \
  (DEBUG("%X Setting", (int)e), sysSetEvent(e))
*/
/*#define sysResetEvent(e) \
  (DEBUG("%X Reset", (int)e), sysResetEvent(e))
*/
/*#define sysEventSelect(s,e,f) \
  (DEBUG("%X Select sock %d 0x%X", (int)e,s,f), sysEventSelect(s,e,f))
*/
/*#define sysDestroyEvent(pe) \
  (DEBUG("%X destroyed", (int)*pe), sysDestroyEvent(pe))
*/
/*#define sysDestroyMutex(pm) \
  (DEBUG("%X destroyed", (int)*pm), sysDestroyMutex(pm))
*/
/* #define sysKillThread(t) \
  (DEBUG("%X kill thread", (int)t), sysKillThread(t))
*/
/*#define sysDestroyThread(t) \
  (DEBUG("%X destroy", t), sysDestroyThread(t))
*/


#ifdef __cplusplus
}
#endif

#endif /* __SYS_IF_H_ */
