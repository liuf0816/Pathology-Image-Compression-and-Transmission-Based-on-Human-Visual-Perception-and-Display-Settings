/*****************************************************************************/
// File: client_server_comms.cpp [scope = CONTRIB/DSTO-CLIENT-SERVER-PORT]
// Version: Kakadu, V6.0
// Author: David Taubman
// Last Revised: 12 August, 2007
/*****************************************************************************/
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
// The copyright owner is Unisearch Ltd, Australia (commercial arm of UNSW)
// Neither this copyright statement, nor the licensing details below
// may be removed from this file or dissociated from its contents.
/*****************************************************************************/
// Licensee: University of Arizona
// License number: 00307
// The licensee has been granted a UNIVERSITY LIBRARY license to the
// contents of this source file.  A brief summary of this license appears
// below.  This summary is not to be relied upon in preference to the full
// text of the license agreement, accepted at purchase of the license.
// 1. The License is for University libraries which already own a copy of
//    the book, "JPEG2000: Image compression fundamentals, standards and
//    practice," (Taubman and Marcellin) published by Kluwer Academic
//    Publishers.
// 2. The Licensee has the right to distribute copies of the Kakadu software
//    to currently enrolled students and employed staff members of the
//    University, subject to their agreement not to further distribute the
//    software or make it available to unlicensed parties.
// 3. Subject to Clause 2, the enrolled students and employed staff members
//    of the University have the right to install and use the Kakadu software
//    and to develop Applications for their own use, in their capacity as
//    students or staff members of the University.  This right continues
//    only for the duration of enrollment or employment of the students or
//    staff members, as appropriate.
// 4. The enrolled students and employed staff members of the University have the
//    right to Deploy Applications built using the Kakadu software, provided
//    that such Deployment does not result in any direct or indirect financial
//    return to the students and staff members, the Licensee or any other
//    Third Party which further supplies or otherwise uses such Applications.
// 5. The Licensee, its students and staff members have the right to distribute
//    Reusable Code (including source code and dynamically or statically linked
//    libraries) to a Third Party, provided the Third Party possesses a license
//    to use the Kakadu software, and provided such distribution does not
//    result in any direct or indirect financial return to the Licensee,
//    students or staff members.  This right continues only for the
//    duration of enrollment or employment of the students or staff members,
//    as appropriate.
/******************************************************************************
Description:
  Implements objects which are common to both "kdu_client2.cpp" and
"kdu_serve2.cpp", but are platform dependent.
   Ported to SunOS/Linux by I. Ligertwood, as part of the "DSTO Ported Files"
described in the accompanying file, "License-to-use-DSTO-port.txt".  Refer
to that file for terms and conditions of use.
******************************************************************************/

#include <assert.h>
#include "sysIF.h"
#include "client_server_comms.h"
#include "kdu_messaging.h"

/* Note Carefully:
      If you want to be able to use the "kdu_text_extractor" tool to
   extract text from calls to `kdu_error' and `kdu_warning' so that it
   can be separately registered (possibly in a variety of different
   languages), you should carefully preserve the form of the definitions
   below, starting from #ifdef KDU_CUSTOM_TEXT and extending to the
   definitions of KDU_WARNING_DEV and KDU_ERROR_DEV.  All of these
   definitions are expected by the current, reasonably inflexible
   implementation of "kdu_text_extractor".
      The only things you should change when these definitions are ported to
   different source files are the strings found inside the `kdu_error'
   and `kdu_warning' constructors.  These strings may be arbitrarily
   defined, as far as "kdu_text_extractor" is concerned, except that they
   must not occupy more than one line of text.
*/
#ifdef KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("E(client_server_comms.cpp)",_id);
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("W(client_server_comms.cpp)",_id);
#  define KDU_TXT(_string) "<#>" // Special replacement pattern
#else // !KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("Error in Client/Server Comms:\n");
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("Warning in Client/Server Comms:\n");
#  define KDU_TXT(_string) _string
#endif // !KDU_CUSTOM_TEXT

#define KDU_ERROR_DEV(_name,_id) KDU_ERROR(_name,_id)
 // Use the above version for errors which are of interest only to developers
#define KDU_WARNING_DEV(_name,_id) KDU_WARNING(_name,_id)
 // Use the above version for warnings which are of interest only to developers


/* ========================================================================= */
/*                             External Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* EXTERN                        hex_hex_decode                              */
/*****************************************************************************/

extern void
  hex_hex_decode(char *result, const char *src, int len)
{
  for (; (len > 0) && (*src != '\0'); len--, result++, src++)
    {
      int hex1, hex2;
      if ((*src == '%') &&
          (isdigit(hex1=toupper(src[1])) ||
           ((hex1 >= (int) 'A') && (hex1 <= (int) 'F'))) &&
          (isdigit(hex2=toupper(src[2])) ||
           ((hex2 >= (int) 'A') && (hex2 <= (int) 'F'))))
        {
          int decoded = 0;
          if ((hex1 >= (int) 'A') && (hex1 <= (int) 'F'))
            decoded += (hex1 - (int) 'A') + 10;
          else
            decoded += (hex1 - (int) '0');
          decoded <<= 4;
          if ((hex2 >= (int) 'A') && (hex2 <= (int) 'F'))
            decoded += (hex2 - (int) 'A') + 10;
          else
            decoded += (hex2 - (int) '0');

          *result = (char) decoded;
          src += 2;
        }
      else
        *result = *src;
    }
  *result = '\0';
}


/* ========================================================================= */
/*                              kd_message_block                             */
/* ========================================================================= */

/*****************************************************************************/
/*                         kd_message_block::read_line                       */
/*****************************************************************************/

const char *
  kd_message_block::read_line(char delim)
{
  if (text == NULL)
    {
      text_max = 100;
      text = new char[text_max+1];
    }
  int text_len = 0;
  bool skip_white = !leave_white;
  bool line_start = true;
  while (next_unread < next_unwritten)
    {
      if (text_len == text_max)
        {
          text_max <<= 1;
          char *new_text = new char[text_max+1];
          memcpy(new_text,text,(size_t) text_len);
          delete[] text;
          text = new_text;
        }
      char ch = *(next_unread++);
      if ((ch == '\0') || (ch == delim))
        {
          if (skip_white && !line_start)
            { // Back over last white space character
              assert(text_len > 0);
              text_len--;
            }
          text[text_len++] = ch;
          break;
        }
      else if ((ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n'))
        {
          if (!skip_white)
            {
              if (ch == '\n')
                text[text_len++] = ch;
              else
                text[text_len++] = ' ';
            }
          skip_white = !leave_white;
        }
      else
        {
          line_start = skip_white = false;
          text[text_len++] = ch;
        }
    }
  text[text_len] = '\0';
  if (text_len == 0)
    return NULL;
  return text;
}

/*****************************************************************************/
/*                      kd_message_block::read_paragraph                     */
/*****************************************************************************/

const char *
  kd_message_block::read_paragraph(char delim)
{
  if (text == NULL)
    {
      text_max = 100;
      text = new char[text_max+1];
    }
  int text_len = 0;
  bool skip_white = !leave_white;
  bool line_start = true;
  while (next_unread < next_unwritten)
    {
      if (text_len == text_max)
        {
          text_max <<= 1;
          char *new_text = new char[text_max+1];
          memcpy(new_text,text,(size_t) text_len);
          delete[] text;
          text = new_text;
        }
      char ch = *(next_unread++);
      if ((ch == '\0') || (ch == delim))
        {
          if (skip_white && !line_start)
            { // Back over last white space character
              assert(text_len > 0);
              text_len--;
            }
          text[text_len++] = ch;
          skip_white = !leave_white;
          line_start = true;
          if ((ch == '\0') || (text_len == 1) || (text[text_len-2] == delim))
            break;
        }
      else if ((ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n'))
        {
          if (!skip_white)
            {
              if (ch == '\n')
                text[text_len++] = ch;
              else
                text[text_len++] = ' ';
            }
          skip_white = !leave_white;
        }
      else
        {
          line_start = skip_white = false;
          text[text_len++] = ch;
        }
    }
  text[text_len] = '\0';
  return text;
}

/*****************************************************************************/
/*                         kd_message_block::read_raw                        */
/*****************************************************************************/

kdu_byte *
  kd_message_block::read_raw(int num_bytes)
{
  if (block == NULL)
    return NULL;
  kdu_byte *result = next_unread;
  next_unread += num_bytes;
  if (next_unread > next_unwritten)
    return NULL;
  return result;
}

/*****************************************************************************/
/*                        kd_message_block::write_raw                        */
/*****************************************************************************/

void
  kd_message_block::write_raw(kdu_byte buf[], int num_bytes)
{
  if (num_bytes <= 0)
    return;
  if (block == NULL)
    {
      block_bytes = 160;
      block = new kdu_byte[block_bytes];
      next_unread = next_unwritten = block;
    }
  if ((next_unwritten - next_unread) < (next_unread - block))
    { // Reclaim wasted space before continuing.
      kdu_byte *scan = block;
      while (next_unread < next_unwritten)
        *(scan++) = *(next_unread++);
      next_unread = block;
      next_unwritten = scan;
    }
  int need_bytes = (int)(next_unwritten-block) + num_bytes;
  if (need_bytes > block_bytes)
    {
      block_bytes += need_bytes;
      kdu_byte *new_block = new kdu_byte[block_bytes];
      memcpy(new_block,block,(size_t)(next_unwritten-block));
      next_unwritten += new_block-block;
      next_unread += new_block-block;
      delete[] block;
      block = new_block;
    }
  memcpy(next_unwritten,buf,(size_t) num_bytes);
  next_unwritten += num_bytes;
}


/* ========================================================================= */
/*                               kd_tcp_channel                              */
/* ========================================================================= */

/*****************************************************************************/
/*                       kd_tcp_channel::kd_tcp_channel                      */
/*****************************************************************************/

kd_tcp_channel::kd_tcp_channel()
{
  sock = INVALID_SOCKET;
  event = NULL;
  internal_event = NULL;
  event_flags = 0;
  closure_request_flag = NULL;
  suppress_errors = false;
  dummy_closure_request_flag = false;
  tbuf_bytes = tbuf_used = 0;
  text = NULL;
  text_len = text_max = 0;
  raw_complete = text_complete = skip_white = false;
  raw = NULL;
  raw_len = raw_max = 0;
  block_len = 0;
  partial_bytes_sent = 0;
  timeout_milliseconds = 0;
}

/*****************************************************************************/
/*                      kd_tcp_channel::~kd_tcp_channel                      */
/*****************************************************************************/

kd_tcp_channel::~kd_tcp_channel()
{
  if (raw != NULL)
    delete[] raw;
  if (text != NULL)
    delete[] text;
  close();
  if (internal_event != NULL)
    sysDestroyEvent(&internal_event);
}

/*****************************************************************************/
/*                        kd_tcp_channel::wait_event                         */
/*****************************************************************************/

bool
  kd_tcp_channel::wait_event()
{
  if (timeout_milliseconds <= 0)
    return (sysWaitForEvent(event,INFINITE) == SYS_WAIT_EVENT_0);
  clock_t elapsed = sysClock();
  if (sysWaitForEvent(event,timeout_milliseconds) != SYS_WAIT_EVENT_0)
    {
      timeout_milliseconds = 1;
      return false;
    }
  elapsed = sysClock() - elapsed;
  timeout_milliseconds -= (int)elapsed;
  if (timeout_milliseconds <= 0)
    timeout_milliseconds = 1;
  return true;
}

/*****************************************************************************/
/*                           kd_tcp_channel::open                            */
/*****************************************************************************/

bool
  kd_tcp_channel::open(SOCKADDR_IN &address, HANDLE h_event,
                       bool *closure_request_flag)
{
  int fileOpts;

  close(); // Just in case.
  if (h_event == NULL)
    {
      if (internal_event == NULL)
        internal_event = sysCreateEvent(FALSE); // Auto-reset
      h_event = internal_event;
    }
  this->event = h_event;
  if (closure_request_flag == NULL)
    closure_request_flag = &(this->dummy_closure_request_flag);
  this->closure_request_flag = closure_request_flag;

  sock = socket(AF_INET,SOCK_STREAM,0);
  if (sock == INVALID_SOCKET)
    {
      close();
      return false;
    }

  /* set socket to non-blocking  and connect */
  if (((fileOpts = fcntl(sock, F_GETFL)) < 0) ||
      ((fcntl(sock, F_SETFL, fileOpts | O_NONBLOCK)) < 0)) return false;

  /* Try disabling nagel (changes the way packets are sent) to see if faster */
  fileOpts = 1; /* for disabling nagle algorithm */
  setsockopt(sock,                          /* socket affected */
             IPPROTO_TCP,             /* set option at TCP level */
             TCP_NODELAY,             /* name of option */
             (char *) &fileOpts,      /* the cast is historical cruft */
             sizeof(int));            /* length of option value */

  if ((connect(sock,(SOCKADDR *) &address,sizeof(address)) == -1) &&
      (sysGetLastError() == ECONNREFUSED)) {
    if (suppress_errors || *closure_request_flag) throw 0;
    KDU_ERROR(e,0); e << 
    KDU_TXT("Socket open attempt failed.");
    return false;
  }

  if (sysEventSelect(sock, event, SYS_CONNECT) != 0)
    {
      close();
      return false;
    }

  /* wait until the connection is ready */
  if ((!wait_event()) || *closure_request_flag) {
    if (suppress_errors || *closure_request_flag) throw 0;
    KDU_ERROR(e,0); e <<
    KDU_TXT("Socket open attempt timed out.");
    return false;
  }
  sysEventSelect(sock, event, 0); // Cancel event selection.
  event_flags = 0;
  timeout_milliseconds = 0;
  return true;
}

/*****************************************************************************/
/*                           kd_tcp_channel::open                            */
/*****************************************************************************/

bool
  kd_tcp_channel::open(SOCKET connected_socket, HANDLE h_event,
                       bool *closure_request_flag)
{
  int fileOpts;

  close(); // Just in case.

  if (h_event == NULL)
    {
      if (internal_event == NULL)
        internal_event = sysCreateEvent(FALSE); // Auto-reset
      h_event = internal_event;
    }
  this->event = h_event;
  if (closure_request_flag == NULL)
    this->closure_request_flag = &(this->dummy_closure_request_flag);
  else
    this->closure_request_flag = closure_request_flag;
  sock = connected_socket;
  if (sock == INVALID_SOCKET)
    { close(); return false; }

  if (((fileOpts = fcntl(sock, F_GETFL)) < 0) ||
      ((fcntl(sock, F_SETFL, fileOpts | O_NONBLOCK)) < 0)) return false;

  timeout_milliseconds = 0;
  return true;
}

/*****************************************************************************/
/*                           kd_tcp_channel::close                           */
/*****************************************************************************/

void
  kd_tcp_channel::close()
{
  if (sock != INVALID_SOCKET)
    {
      //sysEventSelect(sock,NULL,0); // Cancel any event selection.
      sysEventSelect(sock,event,0); // Cancel any event selection.
      shutdown(sock,SD_BOTH);
      closesocket(sock);
      sock = INVALID_SOCKET;
    }
  event = NULL;
  event_flags = 0;
  closure_request_flag = NULL;
  suppress_errors = false;
  tbuf_bytes = tbuf_used = 0;
  text_len = 0;
  raw_len = 0;
  text_complete = true; // So next text read will start from scratch
  raw_complete = true; // So next raw read will start from scratch
  block_len = 0;
  partial_bytes_sent = 0;
}

/*****************************************************************************/
/*                     kd_tcp_channel::get_peer_address                      */
/*****************************************************************************/

void
  kd_tcp_channel::get_peer_address(SOCKADDR_IN &address)
{
  socklen_t length = sizeof(SOCKADDR_IN);
  memset(&address,0,(size_t) length);
  if (sock != INVALID_SOCKET)
    getpeername(sock,(SOCKADDR *) &address,&length);
  else
    address.sin_addr.s_addr = INADDR_NONE;
}

/*****************************************************************************/
/*                    kd_tcp_channel::get_local_address                      */
/*****************************************************************************/

void
  kd_tcp_channel::get_local_address(SOCKADDR_IN &address)
{
  socklen_t length = sizeof(SOCKADDR_IN);
  memset(&address,0,(size_t) length);
  if (sock != INVALID_SOCKET)
    getsockname(sock,(SOCKADDR *) &address,&length);
  else
    address.sin_addr.s_addr = INADDR_NONE;
}

/*****************************************************************************/
/*                       kd_tcp_channel::change_event                        */
/*****************************************************************************/

void
  kd_tcp_channel::change_event(HANDLE h_event)
{
  if (sock == INVALID_SOCKET)
    return;
  event_flags = 0;
  sysEventSelect(sock,event,0);
  if (h_event == NULL)
    {
      if (internal_event == NULL)
        internal_event = sysCreateEvent(FALSE); // Auto-reset
      h_event = internal_event;
    }
  this->event = h_event;
}

/*****************************************************************************/
/*                        kd_tcp_channel::read_line                          */
/*****************************************************************************/

const char *
  kd_tcp_channel::read_line(bool blocking, bool accumulate, char delim)
{
  if (sock == INVALID_SOCKET)
    throw -1;
  if (text_complete && !accumulate)
    text_len = 0;
  text_complete = false;
  line_start = true;
  skip_white = true; // Skip over leading white space
  while (!text_complete)
    {
      // Start by using whatever data is available in `tbuf'.
      while ((tbuf_used < tbuf_bytes) && !text_complete)
        {
          if (text_len == text_max)
            { // Grow the text buffer
              text_max = 2*text_max + 10;
              char *new_text = new char[text_max+1];
              if (text != NULL)
                {
                  memcpy(new_text,text,(size_t) text_len);
                  delete[] text;
                }
              text = new_text;
            }
          char ch = tbuf[tbuf_used++];
          if ((ch == '\0') || (ch == delim))
            {
              if (skip_white && !line_start)
                { // Back over last white space character
                  assert(text_len > 0);
                  text_len--;
                }
              text[text_len++] = ch;
              text_complete = true;
            }
          else if ((ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n'))
            {
              if (!skip_white)
                {
                  if (ch == '\n')
                    text[text_len++] = ch;
                  else
                    text[text_len++] = ' ';
                }
              skip_white = true;
            }
          else
            {
              line_start = skip_white = false;
              text[text_len++] = ch;
            }
        }

      if (!text_complete)
        { // Load more data from the TCP channel
          assert(tbuf_used == tbuf_bytes);
          tbuf_used = 0;
          tbuf_bytes = recv(sock,(char *) tbuf,256,0);
          if (tbuf_bytes == 0)
            { // Socket has been closed gracefully from the other end
              close();
              throw -1;
            }
          if (tbuf_bytes == SOCKET_ERROR)
            { // Socket error or socket would block
              tbuf_bytes = 0;
              if (sysGetLastError() != EWOULDBLOCK)
                { // Socket error; maybe non-graceful disconnect from other end
                  close();
                  throw -1;
                }
              if (!blocking)
                {
                  event_flags |= (SYS_READ|SYS_CLOSE);
                  sysEventSelect(sock,event,event_flags);
                  return NULL; // Caller will have to come back later.
                }
              sysEventSelect(sock,event,SYS_READ|SYS_CLOSE);
              bool wait_succeded;
              if ((!(wait_succeded=wait_event())) || *closure_request_flag)
                {
                  if (suppress_errors || *closure_request_flag)
                    throw 0;
                  KDU_ERROR(e,1); e <<
                    KDU_TXT("Timeout reached while waiting for data on TCP "
                            "socket.");
                }
            //sysEventSelect(sock,NULL,event_flags); // Restore event selection
            //sysEventSelect(sock,event,event_flags); // Restore event selection
            sysEventSelect(sock,event,0); // Restore event selection
            }
        }
    }
  assert(text_complete);
  text[text_len] = '\0'; // Allocation always leaves enough room for terminator
  if (event_flags & SYS_READ)
    { // We can now remove read from event selection conditions
      event_flags &= ~SYS_READ;
      if (event_flags == SYS_CLOSE)
        event_flags = 0; // Don't keep close by itself
      sysEventSelect(sock,event,event_flags);
    }
  return text;
}

/*****************************************************************************/
/*                     kd_tcp_channel::read_paragraph                        */
/*****************************************************************************/

const char *
  kd_tcp_channel::read_paragraph(bool blocking, char delim)
{
  if (text_complete)
    text_len = 0;
  text_complete = false;
  do {
      if (read_line(blocking,true,delim) == NULL)
        return NULL;
    } while ((text_len >= 2) && (text[text_len-1] != '\0') &&
             (text[text_len-2] != delim));
  return text;
}

/*****************************************************************************/
/*                      kd_tcp_channel::test_readable                        */
/*****************************************************************************/

bool
  kd_tcp_channel::test_readable(bool blocking)
{
  if (sock == INVALID_SOCKET)
    throw -1;

  while (1)
    {
      char tbuf[1];
      int tnum = recv(sock,tbuf,1,MSG_PEEK);
      if (tnum == 1)
        return true;
      if (tnum == 0)
        { // Socket has been closed gracefully from the other end
          close();
          throw -1;
        }
      if (tnum == SOCKET_ERROR)
        { // Socket error or socket would block
          if (errno != EWOULDBLOCK)
            { // Socket error; maybe non-graceful disconnect from other end
              close();
              throw -1;
            }
          if (!blocking)
            {
              event_flags |= (SYS_READ|SYS_CLOSE);
              sysEventSelect(sock,event,event_flags);
              return false; // Caller will have to come back later.
            }
          sysEventSelect(sock,event,SYS_READ|SYS_CLOSE);
          bool wait_succeeded;
          if ((!(wait_succeeded=wait_event())) || *closure_request_flag)
            { // Caller will figure out what has gone wrong.
              if (suppress_errors || *closure_request_flag)
                throw 0;
              KDU_ERROR(e,2); e <<
                KDU_TXT("Timeout reached while waiting for data on TCP "
                        "socket.");
            }
          //sysEventSelect(sock,NULL,event_flags); // Restore event selection.
          //sysEventSelect(sock,event,event_flags); // Restore event selection.
          sysEventSelect(sock,event,0); // Restore event selection.
        }
    }
  return false;
}

/*****************************************************************************/
/*                         kd_tcp_channel::read_raw                          */
/*****************************************************************************/

kdu_byte *
  kd_tcp_channel::read_raw(bool blocking, int num_bytes)
{
  if (sock == INVALID_SOCKET)
    throw -1;
  if (raw_complete)
    raw_len = 0;
  raw_complete = false;
  if ((raw_max < num_bytes) || (raw == NULL))
    {
      raw_max = (num_bytes > 0)?num_bytes:1;
      kdu_byte *new_raw = new kdu_byte[raw_max];
      if (raw != NULL)
        delete[] raw;
      raw = new_raw;
    }

  while (raw_len < num_bytes)
    {
      // Start by using whatever data is available in `tbuf'.
      int xfer_bytes = tbuf_bytes - tbuf_used;
      if (xfer_bytes > (num_bytes-raw_len))
        xfer_bytes = num_bytes - raw_len;
      if (xfer_bytes > 0)
        {
          memcpy(raw+raw_len,tbuf+tbuf_used,(size_t) xfer_bytes);
          tbuf_used += xfer_bytes;
          raw_len += xfer_bytes;
        }
      if (raw_len < num_bytes)
        { // Load more data from the TCP channel
          assert(tbuf_used == tbuf_bytes);
          tbuf_used = 0;
          tbuf_bytes = recv(sock,(char *) tbuf,256,0);
          if (tbuf_bytes == 0)
            { // Socket has been closed gracefully from the other end
              close();
              throw -1;
            }
          if (tbuf_bytes == SOCKET_ERROR)
            { // Socket error or socket would block
              tbuf_bytes = 0;
              if (errno != EWOULDBLOCK)
                { // Socket error; maybe non-graceful disconnect from other end
                  close();
                  throw -1;
                }
              if (!blocking)
                {
                  event_flags |= (SYS_READ|SYS_CLOSE);
                  sysEventSelect(sock,event,event_flags);
                  return NULL; // Caller will have to come back later.
                }
              sysEventSelect(sock,event,SYS_READ|SYS_CLOSE);
              bool wait_succeeded;
              if ((!(wait_succeeded=wait_event())) || *closure_request_flag)
                { // Caller will figure out what has gone wrong.
                  if (suppress_errors || *closure_request_flag)
                    throw 0;
                  KDU_ERROR(e,3); e <<
                    KDU_TXT("Timeout reached while waiting for data on TCP "
                    "socket.");
                }
            //sysEventSelect(sock,NULL,event_flags); // Restore event selection
            //sysEventSelect(sock,event,event_flags); // Restore event selection
            sysEventSelect(sock,event,0); // Restore event selection
            }
        }
    }
  raw_complete = true;
  if (event_flags & SYS_READ)
    { // We can now remove read from event selection conditions
      event_flags &= ~SYS_READ;
      if (event_flags == SYS_CLOSE)
        event_flags = 0; // Don't keep close by itself
      sysEventSelect(sock,event,event_flags);
    }
  return raw;
}

/*****************************************************************************/
/*                        kd_tcp_channel::read_block                         */
/*****************************************************************************/

bool
  kd_tcp_channel::read_block(bool blocking, int num_bytes,
                             kd_message_block &block)
{
  if (sock == INVALID_SOCKET)
    throw -1;
  while (block_len < num_bytes)
    {
      // Start by using whatever data is available in `tbuf'.
      int xfer_bytes = tbuf_bytes - tbuf_used;
      if (xfer_bytes > (num_bytes-block_len))
        xfer_bytes = num_bytes - block_len;
      if (xfer_bytes > 0)
        {
          block.write_raw(tbuf+tbuf_used,xfer_bytes);
          tbuf_used += xfer_bytes;
          block_len += xfer_bytes;
        }
      if (block_len < num_bytes)
        { // Load more data from the TCP channel
          assert(tbuf_used == tbuf_bytes);
          tbuf_used = 0;
          tbuf_bytes = recv(sock,(char *) tbuf,256,0);
          if (tbuf_bytes == 0)
            { // Socket has been closed gracefully from the other end
              close();
              throw -1;
            }
          if (tbuf_bytes == SOCKET_ERROR)
            { // Socket error or socket would block
              tbuf_bytes = 0;
              if (errno != EWOULDBLOCK)
                { // Socket error; maybe non-graceful disconnect from other end
                  close();
                  throw -1;
                }
              if (!blocking)
                {
                  event_flags |= (SYS_READ|SYS_CLOSE);
                  sysEventSelect(sock,event,event_flags);
                  return false; // Caller will have to come back later.
                }
              sysEventSelect(sock,event,SYS_READ|SYS_CLOSE);
              bool wait_succeeded;
              if ((!(wait_succeeded=wait_event())) || *closure_request_flag)
                { // Caller will figure out what has gone wrong.
                  if (suppress_errors || *closure_request_flag)
                    throw 0;
                  KDU_ERROR(e,4); e <<
                    KDU_TXT("Timeout reached while waiting for data on TCP "
                    "socket.");
                }
            //sysEventSelect(sock,NULL,event_flags); // Restore event selection
            //sysEventSelect(sock,event,event_flags); // Restore event selection
            sysEventSelect(sock,event,0); // Restore event selection
            }
        }
    }
  block_len = 0;
  if (event_flags & SYS_READ)
    { // We can now remove read from event selection conditions
      event_flags &= ~SYS_READ;
      if (event_flags == SYS_CLOSE)
        event_flags = 0; // Don't keep close by itself
      //sysEventSelect(sock,NULL,event_flags);
      sysEventSelect(sock,event,event_flags);
    }
  return true;
}

/*****************************************************************************/
/*                         kd_tcp_channel::write_raw                         */
/*****************************************************************************/

bool
  kd_tcp_channel::write_raw(bool blocking, kdu_byte buf[], int num_bytes)
{
  if (sock == INVALID_SOCKET)
    throw -1;
  num_bytes -= partial_bytes_sent;
  buf += partial_bytes_sent;
  if (num_bytes <= 0)
    return true;
  while (num_bytes > 0)
    {
      int xfer_bytes = send(sock,(char *) buf,num_bytes,0);
      if (xfer_bytes == 0)
        { // Connection closed gracefully from other end.
          close();
          throw -1;
        }
      else if (xfer_bytes == SOCKET_ERROR)
        { // Socket error or socket would block
          if (sysGetLastError() != EWOULDBLOCK)
            { // Socket error; maybe non-graceful disconnect from other end
              close();
              throw -1;
            }
          if (!blocking)
            {
              event_flags |= (SYS_WRITE|SYS_CLOSE);
              sysEventSelect(sock,event,event_flags);
              return false; // Caller will have to come back later.
            }
          sysEventSelect(sock,event,SYS_WRITE|SYS_CLOSE);
          bool wait_succeeded;
          if ((!(wait_succeeded=wait_event())) || *closure_request_flag)
            { // Caller will figure out what has gone wrong.
              if (suppress_errors || *closure_request_flag)
                throw 0;
              KDU_ERROR(e,5); e <<
                KDU_TXT("Timeout reached while waiting for data on TCP "
                "socket.");
            }
          //sysEventSelect(sock,NULL,event_flags); // Restore event selection.
          //sysEventSelect(sock,event,event_flags); // Restore event selection.
          sysEventSelect(sock,event,0); // Restore event selection.
        }
      else
        {
          assert(xfer_bytes > 0);
          num_bytes -= xfer_bytes;
          partial_bytes_sent += xfer_bytes;
          buf += xfer_bytes;
        }
    }
  partial_bytes_sent = 0;
  if (event_flags & SYS_WRITE)
    { // We can now remove write from event selection conditions
      event_flags &= ~SYS_WRITE;
      if (event_flags == SYS_CLOSE)
        event_flags = 0; // Don't keep close by itself
      //sysEventSelect(sock,NULL,event_flags);
      sysEventSelect(sock,event,event_flags);
    }
  return true;
}
