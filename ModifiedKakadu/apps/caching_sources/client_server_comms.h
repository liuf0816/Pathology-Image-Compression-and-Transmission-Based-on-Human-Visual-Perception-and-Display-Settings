/*****************************************************************************/
// File: client_server_comms.h [scope = APPS/CLIENT-SERVER]
// Version: Kakadu, V6.1
// Author: David Taubman
// Last Revised: 28 November, 2008
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
  Common definitions used in the implementation of basic communication
services for both the client and the server.  This file contains
definitions which are platform specific.
******************************************************************************/

#ifndef CLIENT_SERVER_COMMS_H
#define CLIENT_SERVER_COMMS_H
#include <assert.h>
#include <time.h>
#include <winsock2.h>
#include <process.h>
#include <stdio.h>
#include <ctype.h>
#include "kdu_elementary.h"

// Classes Defined here:
class kd_message_block;
class kd_tcp_channel;


/* ========================================================================= */
/*                         Base JPIP syntax names                            */
/* ========================================================================= */

#define JPIP_FIELD_TARGET          "target"
#define JPIP_FIELD_SUB_TARGET      "subtarget"
#define JPIP_FIELD_TARGET_ID       "tid"

#define JPIP_FIELD_CHANNEL_ID      "cid"
#define JPIP_FIELD_CHANNEL_NEW     "cnew"
#define JPIP_FIELD_CHANNEL_CLOSE   "cclose"
#define JPIP_FIELD_REQUEST_ID      "qid"


#define JPIP_FIELD_FULL_SIZE       "fsiz"
#define JPIP_FIELD_REGION_SIZE     "rsiz"
#define JPIP_FIELD_REGION_OFFSET   "roff"
#define JPIP_FIELD_COMPONENTS      "comps"
#define JPIP_FIELD_CODESTREAMS     "stream"
#define JPIP_FIELD_CONTEXTS        "context"
#define JPIP_FIELD_ROI             "roi"
#define JPIP_FIELD_LAYERS          "layers"
#define JPIP_FIELD_SOURCE_RATE     "srate"

#define JPIP_FIELD_META_REQUEST    "metareq"

#define JPIP_FIELD_MAX_LENGTH      "len"
#define JPIP_FIELD_MAX_QUALITY     "quality"

#define JPIP_FIELD_ALIGN           "align"
#define JPIP_FIELD_WAIT            "wait"
#define JPIP_FIELD_TYPE            "type"
#define JPIP_FIELD_DELIVERY_RATE   "drate"

#define JPIP_FIELD_MODEL           "model"
#define JPIP_FIELD_TP_MODEL        "tpmodel"
#define JPIP_FIELD_NEED            "need"

#define JPIP_FIELD_CAPABILITIES    "cap"
#define JPIP_FIELD_PREFERENCES     "pref"

#define JPIP_FIELD_UPLOAD          "upload"
#define JPIP_FIELD_XPATH_BOX       "xpbox"
#define JPIP_FIELD_XPATH_BIN       "xpbin"
#define JPIP_FIELD_XPATH_QUERY     "xpquery"


/* ========================================================================= */
/*                     Return Data Termination Codes                         */
/* ========================================================================= */

#define JPIP_EOR_IMAGE_DONE             ((kdu_byte) 1)
#define JPIP_EOR_WINDOW_DONE            ((kdu_byte) 2)
#define JPIP_EOR_WINDOW_CHANGE          ((kdu_byte) 3)
#define JPIP_EOR_BYTE_LIMIT_REACHED     ((kdu_byte) 4)
#define JPIP_EOR_QUALITY_LIMIT_REACHED  ((kdu_byte) 5)
#define JPIP_EOR_SESSION_LIMIT_REACHED  ((kdu_byte) 6)
#define JPIP_EOR_RESPONSE_LIMIT_REACHED ((kdu_byte) 7)
#define JPIP_EOR_UNSPECIFIED_REASON     ((kdu_byte) 0xFF)



/* ========================================================================= */
/*                              Functions                                    */
/* ========================================================================= */

/*****************************************************************************/
/* EXTERN                        hex_hex_decode                              */
/*****************************************************************************/

extern void
  hex_hex_decode(char *result, const char *src, int len);
  /* Copies `src' to `result', writing at most `len'-1 characters, not
     including the null terminator, and expanding hex-hex encoded characters
     in the source URI. */

/*****************************************************************************/
/* INLINE                   kd_caseless_search                               */
/*****************************************************************************/

inline const char *
  kd_caseless_search(const char *string, const char *pattern)
  /* Searches for `pattern' within `string', returning a pointer to the
     character immediately after the pattern if successful, or NULL if
     unsuccessful.  All comparisons are case insensitive. */
{
  const char *mp;
  for (mp=pattern; *string != '\0'; string++)
    if (tolower(*string) == tolower(*mp))
      mp++;
    else if (*mp == '\0')
      break;
    else
      mp = pattern;
  return (*mp == '\0')?string:NULL;
}

/*****************************************************************************/
/* INLINE                  kd_has_caseless_prefix                            */
/*****************************************************************************/

inline bool
  kd_has_caseless_prefix(const char *string, const char *prefix)
  /* Returns true if `prefix' is a prefix of the supplied string; the
     comparison is case insensitive. */
{
  for (; (*string != '\0') && (*prefix != '\0'); string++, prefix++)
    if (tolower(*string) != tolower(*prefix))
      return false;
  return (*prefix == '\0');
}

/*****************************************************************************/
/* INLINE                  kd_parse_request_field                            */
/*****************************************************************************/

inline bool
  kd_parse_request_field(const char * &string, const char *field_name)
  /* If `string' commences with the `field_name' string, followed by '=', this
     function returns true, setting `string' to point immediately after
     the `=' sign.  Otherwise, the function returns false and leaves `string'
     unchanged. */
{
  const char *sp=field_name, *dp=string;
  for (; *sp != '\0'; sp++, dp++)
    if (*dp != *sp) return false;
  if (*dp != '=')
    return false;
  string = dp+1;
  return true;
}

/*****************************************************************************/
/* INLINE                   kd_parse_jpip_header                             */
/*****************************************************************************/

inline const char *
  kd_parse_jpip_header(const char *string, const char *field_name)
  /* Scans through the supplied `string' looking for a header which commences
     with the prefix "JPIP-", immediately preceded by a new-line character,
     and immediately followed by `field_name' and a ":".  If successful,
     the function returns a pointer to the first non-white-space character
     following the ":".  Otherwise, it returns NULL. */
{
  const char *fp, *sp=string;
  for (; *sp != '\0'; sp++)
    {
      if ((sp[0] != '\n') || (sp[1] != 'J') || (sp[2] != 'P') ||
          (sp[3] != 'I') || (sp[4] != 'P') || (sp[5] != '-'))
        continue;
      for (sp+=5, fp=field_name; *fp != '\0'; fp++, sp++)
        if (sp[1] != *fp)
          break;
      if (*fp != '\0')
        continue;
      if (sp[1] != ':')
        continue;
      for (sp+=2; (*sp == ' ') || (*sp == '\t'); sp++);
      return sp;
    }
  return NULL;
}


/* ========================================================================= */
/*                                Classes                                    */
/* ========================================================================= */

/*****************************************************************************/
/*                            kd_message_block                               */
/*****************************************************************************/

class kd_message_block {
  public: // Member functions
    kd_message_block()
      {
        block_bytes = 0; block = next_unread = next_unwritten = NULL;
        mode_hex = leave_white = false; text = NULL; text_max = 0;
      }
    ~kd_message_block()
      {
        if (block != NULL) delete[] block;
        if (text != NULL) delete[] text;
      }
    void restart()
      /* Initializes both read and write pointers to the beginning of the
         block.  It is safe to call this function any time. */
      {
        next_unread = next_unwritten = block; mode_hex = false;
      }
    void backup()
      /* Initializes the read pointer to the beginning of the block, without
         altering the write pointer.  This allows a block of data to be read
         again from scratch, but the behaviour may be unpredictable if
         reading and writing have been interleaved (write some data, read
         some data, write some more data, etc.), since in that case some
         memory may have been recycled.  The function is primarily of use
         if some data may have been written on an outgoing connection which
         is subsequently found to have been dropped. */
      {
        next_unread = block;
      }
    int get_remaining_bytes() { return (int)(next_unwritten-next_unread); }
      /* Returns the difference between the write pointer and the read
         pointer, representing the number of bytes which have been written
         to the block, but not yet read, since the last `restart' call. */
    kdu_byte *peek_block() { return next_unread; }
      /* Returns a pointer to the unread portion of the internal message
         block.  The number of valid bytes is indicated by the
         `get_remaining_bytes' member function. */
    bool keep_whitespace(bool yes=true)
      { /* By default, all superfluous white space is removed in calls to
           `read_line' or `read_paragraph'.  Use this function to alter the
           whitespace keeping mode; the function returns the previous mode. */
        bool result = leave_white; leave_white = yes; return result;
      }
    const char *read_line(char delim='\n');
      /* Returns the next line of text stored in the block, where lines
         are delimited by the indicated character.
            The returned character string has all superfluous white space
         characters (' ', '\t', '\r', '\n') removed.  This includes all leading
         white space, all trailing white space and all but one space
         character between words.  '\t' or '\r' characters between words
         are converted to ' ' characters.
            The returned pointer refers to an internal resource, which may not
         be valid after subsequent use of the object's member functions. 
            If there are no more lines left, the function returns NULL.
      */
    const char *read_paragraph(char delim='\n');
      /* Same as `read_line', but returns an entire paragraph, delimited by
         two consecutive instances of the `delim' character, separated at
         most by white space, one instance of the `delim' character which
         is encountered before any non-whitespace character, or the end
         of the message block.  Unlike `read_line', this function returns
         a non-NULL pointer even if there is no text to read -- in this case,
         the returned string will be empty. */
    kdu_byte *read_raw(int num_bytes);
      /* Returns a pointer to an internal buffer, holding the next `num_bytes'
         bytes from the input.  The function returns NULL if the
         requested number of bytes is not available.  You can always check
         availability by calling `get_remaining_bytes'. */
    void write_raw(kdu_byte buf[], int num_bytes);
      /* Writes the supplied bytes to the internal memory block. */
    int backspace(int num_chars)
      { /* Causes the most recent `num_chars' characters in the block to be
           erased, returning the number of characters left. */
        assert((next_unwritten-next_unread) >= num_chars);
        next_unwritten -= num_chars;
        return (int)(next_unwritten-next_unread);
      }
    bool set_hex_mode(bool new_mode)
      { bool old_mode = mode_hex; mode_hex = new_mode; return old_mode; }
      /* Hex mode affects the way integers are written as text by the
         overloaded "<<" operator. */
    void write_text(const char *string)
      { write_raw((kdu_byte *) string,(int) strlen(string)); }
      /* Writes the supplied text string to the internal memory block.  The
         supplied null-terminated string is written as-is, without the
         null-terminator. */
    kd_message_block &operator<<(const char *string)
      { write_text(string); return *this; }
    kd_message_block &operator<<(char ch)
      { char text[2]; text[0]=ch; text[1]='\0';
        write_text(text); return *this; }
    kd_message_block &operator<<(int val)
      { char text[80];
        sprintf(text,(mode_hex)?"%x":"%d",val);
        write_text(text); return *this; }
    kd_message_block &operator<<(unsigned int val)
      { char text[80];
        sprintf(text,(mode_hex)?"%x":"%u",val);
        write_text(text); return *this; }
    kd_message_block &operator<<(long val)
      { return (*this)<<(int) val; }
    kd_message_block &operator<<(unsigned long val)
      { return (*this)<<(unsigned int) val; }
    kd_message_block &operator<<(short int val)
      { return (*this)<<(int) val; }
    kd_message_block &operator<<(unsigned short int val)
      { return (*this)<<(unsigned int) val; }
    kd_message_block &operator<<(float val)
      { char text[80]; sprintf(text,"%f",val);
        write_text(text); return *this; }
    kd_message_block &operator<<(double val)
      { char text[80]; sprintf(text,"%f",val);
        write_text(text); return *this; }
  private: // Data
    int block_bytes;
    kdu_byte *block;
    kdu_byte *next_unread; // Points to next location to read from`block'.
    kdu_byte *next_unwritten; // Points to next location to write into `block'.
    char *text; // Buffer used for accumulating parsed ASCII text
    int text_max; // Max chars allowed in `text', before reallocation needed
    bool mode_hex;
    bool leave_white;
  };

/*****************************************************************************/
/*                              kd_tcp_channel                               */
/*****************************************************************************/

class kd_tcp_channel {
  public: // Member functions
    kd_tcp_channel();
      /* Does not actually create a socket.
         All the real work done in `open'. */
    virtual ~kd_tcp_channel(); // Can be invoked from abstract base
      /* Note: if at all possible, you should try to call `close' from the
         same thread which called `open' before destroying this object. */
    bool operator!()
      { return !is_open(); }
    virtual bool is_open()
      { return (sock != INVALID_SOCKET); }
      /* Returns true if a previous call to `open' has succeeded, and the
         channel has not yet been closed.  This function does not test
         the TCP channel to see if the other party has closed the connection;
         however, if a previous read or write operation failed due to
         premature termination of the connection, the present function will
         return false. */
    bool open(SOCKADDR_IN &address, HANDLE h_event=NULL,
              bool *close_request_flag=NULL);
      /* Creates a socket and establishes a TCP connection to the indicated
         address.  The function blocks the caller until connection is
         complete, or the controlling application requests immediate closure
         of the client.  If connection fails, the function returns false,
         but if the application requests closure, an exception is thrown.
            If the channel is successfully opened, the `h_event' object is
         installed for use in receiving notification of network events and
         of closure requests by the application.  Blocking calls use `h_event'
         signalling to unblock the caller.  If at that point the boolean
         variable at *`close_request_flag' is found to be true, the
         relevant read or write function will terminate immediately, returning
         the appropriate failure code.  The `close_request_flag' argument
         may be NULL, if this feature is not of interest.
            If `h_event' is NULL, all I/O transactions will be blocking and
         the corresponding member functions must have their `blocking'
         argument set to true. */
    bool open(SOCKET connected_socket, HANDLE h_event=NULL,
              bool *close_request_flag=NULL);
      /* Same as the first form of the open function, except that the
         TCP socket has already been opened and is passed in to the
         function for immediate use.  For consistency with the
         first form, this function has a boolean return value, but it
         will always return true, unless you pass in an invalid socket
         handle. */
    void close();
      /* Closes any open TCP connection and releases the socket, but does
         not destroy other internal resources which might be used if the
         object is re-opened.  It is safe to call this function at any time,
         even if no TCP connection is open. */
    void get_peer_address(SOCKADDR_IN &address);
      /* Returns with `address' filled out to reflect the IP address and port
         number of the peer socket to which the current channel is connected.
         Returns with `address.sin_addr.S_un.S_addr' equal to INADDR_NONE if
         the channel is not associated with an open socket. */
    void get_local_address(SOCKADDR_IN &address);
      /* Returns with `address' filled out to reflect the IP address and port
         number of the local socket to which the current channel is connected.
         Returns with `address.sin_addr.S_un.S_addr' equal to INADDR_NONE if
         the channel is not associated with an open socket. */
    void change_event(HANDLE h_event);
      /* Changes the `event' object which will be used for socket blocking
         operations and which will be signalled when a non-blocking condition
         becomes true after the non-blocking call returns.  This function
         is useful when an open TCP channel is transferred from one object
         to another, possibly in a different thread.
            Note that this function will clear any existing wait conditions,
         so you should only use it when you know that there are no outstanding
         non-blocking calls. */
    void start_timer(int milliseconds, bool suppress_errors,
                     bool *close_request_flag)
      {
        this->timeout_milliseconds = milliseconds;
        this->suppress_errors = suppress_errors;
        if (close_request_flag == NULL)
          this->closure_request_flag = &(this->dummy_closure_request_flag);
        else
          this->closure_request_flag = close_request_flag;
      }
      /* After calling this function, the cumulative time spent blocking on
         all subsequent socket conditions (read or write) is limited to the
         supplied number of milliseconds.  If this limit is exceeded, a
         timeout occurs, resulting in an error message being delivered through
         `kdu_error' (if `suppress_errors' is false and `close_request_flag'
         is NULL or points to a variable which holds true) or an integer
         valued exception being thrown.  An argument of 0 disables timeouts.
            The timeout value is set to 0 after a successful call to `open',
         although a non-zero timeout value does apply while an open call is
         in progress.
            Note that the `suppress_errors' and `close_request_flag'
         arguments have a persistent effect on the internal error behaviour.
         In particular, `close_request_flag' overwrites the value installed
         by `open' and `suppress_errors' changes the default error
         behaviour.  The default error behaviour set by `close' and upon
         construction is to not suppress errors.  The function thus provides
         a useful means for manipulating error handling behaviour, even
         if you do not want timeouts. */
    const char *read_line(bool blocking, bool accumulate=false,
                          char delim='\n');
      /* Accumulates text characters from the TCP channel until either the
         `delim' character or the null character, `\0', is encountered.  If
         successful, the function returns a null-terminated character string
         which terminates with the `delim' character if it was found.
            The returned character string has all superfluous white space
         characters (' ', '\t', '\r', '\n') removed.  This includes all leading
         white space, all trailing white space and all but one space
         character between words.  '\t' or '\r' characters between words
         are converted to ' ' characters.
            If `blocking' is false, the function will not block.  If
         sufficient data is not yet available to complete the request, the
         function returns NULL, registering the `event' object passed into
         `open' to be signalled once more data is available (or an error
         has occurred); the caller will normally wait upon the `event' object,
         re-invoking the present function when signalled.
            If `accumulate' is false, this function automatically discards
         any text which was returned by a previous successful call to
         `read_line' or `read_paragraph', before commencing the new call.
         Otherwise, the function appends the new line of text to previously
         accumulated text, returning the multi-line response once a new
         complete line has become available, or NULL if a non-blocking
         request is incomplete.
            If an unexpected error or a timeout condition occurs, an
         appropriate error message is generated through `kdu_error', which
         should end up throwing an exception.  If the application requests
         closure, an integer exception with the value 0 will be generated.
         If the connection is closed, an integer exception with the value -1
         will be thrown.
    */
    const char *read_paragraph(bool blocking, char delim='\n');
      /* Essentially calls `read_line' until two consecutive occurrences of
         the `delim' character are encountered, or else one occurrence of
         the `delim' character occurs immediately, before any
         non-whitespace character is encountered, or '\0' is encountered.
         In both cases, the returned string is null-terminated.  The
         returned string will conclude with all recovered delimiters.
            Non-blocking calls will return NULL, until the complete
         request has been satisfied, or an exception is thrown. */
    kdu_byte *read_raw(bool blocking, int num_bytes);
      /* Reads the indicated number of bytes from the TCP channel, returning
         a pointer to an internal buffer which holds the requested data.  If
         `blocking' is true, the function blocks the caller until the request
         can be satisfied.  Otherwise, the function returns immediately,
         without blocking.  Non-blocking calls return NULL if the request
         could not be completed immediately, inviting the caller to invoke
         the function again later, after first waiting for the `event' object
         passed into `open' to become signalled.
            If an unexpected error or a timeout condition occurs, an
         appropriate error message is generated through `kdu_error', which
         should end up throwing an exception.  If the application requests
         closure, an integer exception with the value 0 will be generated.
         If the connection is closed, an integer exception with the value -1
         will be thrown. */
    bool test_readable(bool blocking);
      /* This function does not actually read anything, but it checks to
         see if there is anything available for reading (it might be as
         little as one byte).  If the channel currently has no new
         data and `blocking' is true, the function blocks the caller until
         something does become available or the `event' object passed into
         `open' (or `change_event') becomes otherwise signalled.  The
         function may throw an exception or generate an error message
         through `kdu_error', under the same conditions as the other
         read functions.  As with those functions, a non-blocking call
         will arrange for the `event' object to become signalled once
         the necessary conditions occur for a blocking call to this
         function to return immediately. */
    bool read_block(bool blocking, int num_bytes, kd_message_block &block);
      /* Equivalent to concatenating `read_raw' with `block.write_raw', but
         generally more efficient.  Returns false only if `blocking' is true
         and the call could not complete immediately, in which case the
         caller should invoke the function again after waiting for the
         `event' object passed into `open' to become signalled.  After a
         non-blocking call which did not complete, the next call to this
         function should supply exacly the same `block' and `num_bytes'
         arguments, even though some bytes may have been written to
         `block' in the last call. */
    bool write_raw(bool blocking, kdu_byte *buf, int num_bytes);
      /* Writes `num_bytes' from the supplied buffer to the TCP channel.
            If an unexpected error condition occurs, an appropriate error
         message is generated through `kdu_error', which should end up
         throwing an exception.  If the application requests closure, an
         integer exception with the value 0 will be generated.  If the
         connection is closed, an integer exception with the value -1 will
         be thrown.
            If `blocking' is true, the function blocks until the message can
         be delivered in full to the TCP send buffer, or an exception is
         thrown.  In this case, the function invariably returns true.
         Otherwise, if `blocking' is false, the function returns immediately,
         after sending as much of the message as possible.  If the message
         was not completely sent, the function returns false and registers the
         `event' object passed into `open' to be signalled once the TCP
         send buffer can accept more data; the caller will normally wait
         upon the `event' object, re-invoking the present function once it
         becomes signalled.  Between such non-blocking calls, you should be
         careful to always supply exactly the same `buf' and `num_bytes'
         arguments. */
    bool write_block(bool blocking, kd_message_block &block)
      { int num_bytes = block.get_remaining_bytes();
        return write_raw(blocking,block.peek_block(),num_bytes); }
      /* Writes the entire contents of `block', starting from the position
         of its current read pointer, but otherwise identical to
         `write_raw'. */
  private: // Helper functions
    bool wait_event();
      /* Waits for the `event' object to be signalled or for a timeout to
         expire.  Returns false in the event of a timeout, true otherwise. */
  private: // Data
    SOCKET sock;
    HANDLE event; // Signals network activity or application closure requests
    HANDLE internal_event; // Created only if `open' supplied no event
    kdu_uint32 event_flags; // Network event bits passed to WSAEventSelect
    bool *closure_request_flag; // Points to master application closure flag
    bool suppress_errors; // Cause I/O failures to generate eceptions not errors
    bool dummy_closure_request_flag;
    int timeout_milliseconds;
    kdu_byte tbuf[256]; // Temporary holding buffer for reading
    int tbuf_bytes; // Total read bytes currently stored in `tbuf'
    int tbuf_used; // Number of initial bytes from `tbuf' already used
    char *text; // Buffer used for accumulating parsed ASCII text
    int text_len; // Number of characters accumulated in `text'
    int text_max; // Max chars allowed in `text', before reallocation needed
    bool text_complete; // True if `text' was returned exactly as is in a
              // previous successful call to `read_text' or `read_paragraph'.
    bool skip_white; // True if white space is to be skipped
    bool line_start; // True if we have not yet found a non-white character
    kdu_byte *raw; // Buffer used for accumulating raw binary data
    int raw_len; // Number of bytes accumulated in `raw'
    int raw_max; // Max bytes allowed in `raw', before reallocation needed
    bool raw_complete; // True if `raw' was returned exactly as is in a
              // previous successful call to `read_raw'.
    int block_len;  // Used by `read_block'.
    int partial_bytes_sent; // Total bytes sent after incomplete non-blocking
                            // call to `write_raw'.
  };

#endif // CLIENT_SERVER_COMMS_H
