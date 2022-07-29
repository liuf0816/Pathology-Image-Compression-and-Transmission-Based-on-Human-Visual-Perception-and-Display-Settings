/*****************************************************************************/
// File: server_local.h [scope = CONTRIB/DSTO-CLIENT-SERVER-PORT]
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
   Local header file for the "kdu_server" application.  Imported by
"kdu_server.cpp".
   Ported to SunOS/Linux by I. Ligertwood, as part of the "DSTO Ported Files"
described in the accompanying file, "License-to-use-DSTO-port.txt".  Refer
to that file for terms and conditions of use.
******************************************************************************/

#ifndef SERVER_LOCAL_H
#define SERVER_LOCAL_H

#include "sysIF.h"
#include "client_server_comms.h"
#include "kdu_serve.h"
#include "kdu_servex.h"
#include "kdu_security.h"

#define KD_MY_RESOURCE_NAME "jpip" // Resource name used by server for sessions
                                   // Note that the name must be lower-case.
#define KD_CHANNEL_ID_LEN 24 // Max length of any channel-ID assigned here

// Defined here:
class kd_message_lock;
class kd_message_sink;
class kd_message_log;

struct kd_request;
class kd_request_queue;

class kd_transmitter;
class kd_tcp_transmitter;
class kd_http_transmitter;
class kd_delivery_manager;

struct kd_target_description;

class kd_synchronized_serve_target;
class kd_source_thread;
struct kd_source;
struct kd_client_history;
class kd_source_manager;

class kd_connection_thread;
class kd_connection_server;

struct kd_delegation_host;
class kd_delegator;
extern kdu_message_formatter kd_msg; // Common text output device
extern kd_message_log kd_msg_log;


#define KD_DELEGATE_RESOLUTION_INTERVAL 20
       // Number of times we connect to known good hosts before we are prepared
       // to retry address resolution of a host which had a problem.

/*****************************************************************************/
/*                            kd_message_lock                                */
/*****************************************************************************/

class kd_message_lock {
  public: // Member functions
    kd_message_lock()
      { h_mutex = sysCreateMutex(); }
    ~kd_message_lock()
      { sysDestroyMutex(&h_mutex); }
    void acquire()
      { sysLockMutex(h_mutex); }
    void release()
      { sysReleaseMutex(h_mutex); }
  private: // Data
    HANDLE h_mutex;
  };

/*****************************************************************************/
/*                             kd_message_sink                               */
/*****************************************************************************/

  /* Objects of this class are the ultimate destinations for all text messages
     generated by the system.  When used to customize `kdu_error' and
     `kdu_warning', the object's `start_message' and `flush' functions
     employ the `kd_message_lock' object to protect the entire message
     against inter-thread conflicts. */
class kd_message_sink : public kdu_message {
  public: // Member functions
    kd_message_sink(FILE *Dest, kd_message_lock *message_lock,
                    bool Throw_end_of_message_exception)
      { this->dest = Dest; this->lock = message_lock;
        this->throw_eom_exception = Throw_end_of_message_exception; }
    void redirect(FILE *Dest)
      { this->dest = Dest; }
    void put_text(const char *string)
      { fprintf(dest,"%s",string); }
    void flush(bool end_of_message=false)
      {
        fflush(dest);
        if (end_of_message)
          {
            lock->release();
            if (throw_eom_exception)
              throw (int) 0;
          }
      }
    void start_message()
      { lock->acquire(); }
  private: // Data
    FILE *dest;
    kd_message_lock *lock;
    bool throw_eom_exception;
  };

/*****************************************************************************/
/*                               kd_message_log                              */
/*****************************************************************************/

  /* The purpose of this class is to provide a global logging facility for
     recording the ASCII text transactions flowing over TCP channels. */

class kd_message_log {
  public: // Member functions
    kd_message_log()
      { output = NULL; buf = NULL; buf_len = 0; }
    ~kd_message_log()
      { if (buf != NULL) delete[] buf; }
    void init(kdu_message *Output)
      { this->output = Output; }
    void print(const char *buffer, int num_chars, const char *prefix,
               bool leave_blank_line=true);
      /* Prints the first `num_chars' characters of `buffer' in lines, with
         each line prefixed by the supplied `prefix' string.  If
         `leave_blank_line' is true, a blank line is printed before the block
         of text; the blank line will not have  the `prefix' string
         prepended. */
    void print(const char *text, const char *prefix,
               bool leave_blank_line=true)
      { /* Same as above, but `text' is a null-terminated string. */
        if (output != NULL)
          print(text,(int) strlen(text),prefix,leave_blank_line);
      }
    void print(kd_message_block &block, const char *prefix,
               bool leave_blank_line=true)
      { /* Same as above, but prints the text currently held in the supplied
           message block. */
        if (output != NULL)
          print((char *) block.peek_block(),block.get_remaining_bytes(),prefix,
                leave_blank_line);
      }
  private: // Data
    kdu_message *output;
    char *buf;
    int buf_len; // Num chars which `buf' can hold.
  };

/*****************************************************************************/
/*                            kd_request_fields                              */
/*****************************************************************************/

  // This structure is embedded inside `kd_request'.  It contains pointers to
  // pre-parsed null-terminated strings, representing the bodies of each
  // possible request field.  If any pointer is NULL, the corresponding
  // request field was not encountered in the request.
struct kd_request_fields {
  public: // Functions
    void write_query(kd_message_block &block) const;
      /* Writes a JPIP query string containing the same information as this
         structure. */
  public: // Data
    const char *target;
    const char *sub_target;
    const char *target_id;
    const char *channel_id;

    const char *channel_new;
    const char *channel_close;
    const char *request_id;

    const char *full_size;
    const char *region_size;
    const char *region_offset;
    const char *components;
    const char *codestreams;
    const char *contexts;
    const char *roi;
    const char *layers;
    const char *source_rate;

    const char *meta_request;

    const char *max_length;
    const char *max_quality;

    const char *align;
    const char *type;
    const char *delivery_rate;

    const char *model;
    const char *tp_model;
    const char *need;

    const char *capabilities;
    const char *preferences;

    const char *upload;
    const char *xpath_box;
    const char *xpath_bin;
    const char *xpath_query;

    const char *admin_key;
    const char *admin_command;
    const char *unrecognized; // Holds full text of any unrecognized field
  };

/*****************************************************************************/
/*                                kd_request                                 */
/*****************************************************************************/

struct kd_request {
    kd_request()
      { max_buf_len=0; buf=NULL; init(); }
    ~kd_request()
      { if (buf != NULL) delete[] buf; }
    void init()
      {
        method=resource=query=http_accept=NULL;
        close_connection=preemptive=false;
        memset(&fields,0,sizeof(fields));
        cur_buf_len = 0;
        next=NULL;
      }
    void copy(const kd_request *src);
  public: // Data
    const char *method; // Always points to first token in the request
    const char *resource; // non-NULL if and only if method is GET or POST
    bool close_connection; // If connection to be closed after responding
    bool preemptive; // True unless we parsed a "wait=yes" request field
    kd_request_fields fields; // All parsed request fields
  private: // Helper functions
    void write_method(const char *string, int string_len);
      /* Write this immediately after calling `init'. */
    void write_resource(const char *string, int string_len);
      /* Write this immediately after the method string. */
    void write_query(const char *string, int string_len);
      /* Write this any time after resource. */
    void write_http_accept(const char *string, int string_len);
      /* Write this any time after resource. */
    void set_cur_buf_len(int len)
      {
        if (len > max_buf_len)
          {
            char *existing = buf;
            max_buf_len = len + 100;
            buf = new char[max_buf_len];
            if (cur_buf_len > 0) memcpy(buf,existing,cur_buf_len);
            if (existing != NULL) delete[] existing;
          }
        cur_buf_len = len;
      }
  private: // Data
    friend class kd_request_queue;
    const char *query; // Private, because we will break it apart later
    const char *http_accept; // NULL or points to body of HTTP "Accept:" header
    int cur_buf_len;
    int max_buf_len;
    char *buf;
    kd_request *next;
  };

/*****************************************************************************/
/*                             kd_request_queue                              */
/*****************************************************************************/

  /* Provides all the functionality needed to parse HTTP GET and HTTP POST
     requests, extract their query strings (either in the query component
     of the HTTP GET request line, or in the body of a POST requst), and
     manage a queue of such requests. */
class kd_request_queue {
  public: // Member functions
    kd_request_queue()
      { head=tail=pending=free_list = NULL; init(); }
    ~kd_request_queue();
    void init();
      /* Sets to the uninitialized state, except that any allocated
         `kd_request' structures are moved to the free list, rather than
         being destroyed. */
    bool read_request(bool blocking, kd_tcp_channel *channel);
      /* This function plays a similar role to
         `kd_tcp_channel::read_paragraph', which it uses.  The function reads
         an initial request paragraph, checking to see if it corresponds to
         an HTTP GET or POST.  For GET paragraphs, the function creates a
         complete new `kd_request' structure immediately.  For POST
         requests, the function also reads the entity body to recover the
         query string, if any.
            If `blocking' is true, the function waits until a complete request
         can be parsed, returning true once it is done.  Otherwise, the
         function does not block the caller; if insufficient data is available
         to completely parse the request, the function returns false, but may
         be called again any number of times until the operation has been
         completed.  As with `kd_tcp_channel::read_paragraph' (which it uses)
         error conditions (e.g. failure of the TCP channel) will result in
         an integer valued exception being thrown.
            Upon success (i.e., whenever the function returns true), a
         new `kd_request' entry is always created and appended to the queue.
         If the request did not correspond to a recognized HTTP request,
         the request's `resource' string is set to NULL and all members of
         the `fields' structure will be NULL.  Otherwise, `resource' points
         to the original resource string (possibly hex-hex decoded) and
         the members of the `fields' structure are each set either to NULL
         (if the request field was not found) or to point to a pre-parsed
         null-terminated string, which has been hex-hex decoded if necessary.
            Note that the `kd_request_fields.type' member may be derived
         either from an HTTP "Accept:" header line, or from a JPIP "type="
         request field in the query string.  The `kd_request_fields.target'
         member may be derived either from the resource string (after
         skipping over protocol prefixes) or from a JPIP "target=" request
         field in the query string.
            If the request suggests that the TCP connection should be closed
         after the response is complete, the `kd_request::close_connection'
         member is set to true.  This is done if the request contained
         a "Connection: close" header or an HTTP version less than HTTP/1.1. */
    void push_copy(const kd_request *src);
      /* Copies the `src' object to form an entry which is pushed onto the
         queue. */
    bool have_request(bool only_if_preemptive);
      /* Returns true if the queue contains one or more requests, and either
         1) at least one of these requests has `preemptive'=true, or 2) the
         `only_if_preemptive' argument is false. */
    const kd_request *pop_head(bool *is_overridden);
      /* Returns the least recent (head) request in the queue.  If
         `is_overridden' is non-NULL, the value of *`is_overridden' is set to
         false unless the request is overridden by a more recent request in
         the queue for which `kd_request::preemptive' is true.  In the latter
         case, *`is_overridden' is set to true.  Returns NULL only if the
         queue is empty. */
    void replace_head(const kd_request *hd)
      { /* You can use this function to return the request previously fetched
           using `pop_head' back to the head of the request queue.  You
           can only return the most recently poppoed head. */
        kd_request *tmp = free_list;
        assert(hd == tmp);
        free_list = tmp->next;
        tmp->next = head;
        head = tmp;
        if (tail == NULL)
          tail = tmp;
      }
    void transfer_state(kd_request_queue *src);
      /* This function appends all fully parsed requests, plus the state of
         any partially parsed request to the current object, as though the
         requsts had been read into this object instead.  The function leaves
         `src' in the initialized state (as though `src->init' had just been
         called).  The function MUST NOT BE USED unless you are sure that the
         current object contains no partially parsed requests of its own,
         since at most one request can be in a partially parsed state. */
  private: // Helper functions
    kd_request *get_empty_request()
      {
        kd_request *req = free_list;
        if (req == NULL) req = new kd_request;
        free_list = req->next;
        req->init();
        return req;
      }
    void return_request(kd_request *req)
      {
        req->next = free_list;
        free_list = req;
      }
    void complete_pending_request();
      /* Called when `pending' points to a completed request.  Starts by
         hex-hex decoding the resource and query strings.  Then scans through
         the query string (and possibly the `resource' and `http_accept'
         strings) in order to complete all the various members of the
         `kd_request::fields' structure.  Also determines whether or not the
         JPIP "wait=" request field is present, setting the `preemptive'
         member accordingly.  Finally appends the pending request to the end
         of the queue and resets `pending' to NULL. */
  private: // Data
    kd_request *head, *tail;
    kd_request *free_list;
    int pending_body_bytes;
    kd_request *pending; // Partially created request
  };

/*****************************************************************************/
/*                               kd_transmitter                              */
/*****************************************************************************/

  /* Abstract base class representing a generic asynchronous agent for
     transmitting return data. */

class kd_transmitter {
  public: // Member functions
    kd_transmitter()
      { /* Basic initialization only.  You must call `configure' afterwards. */
        channel = closed_channel = NULL;
        max_bytes_per_second = 0.0F;
        h_mutex = sysCreateMutex();
        source_event = service_event = NULL;
        push_waiting = false;
        base_time = sysClock();
        delivery_gate = 1; // Never let this equal 0.
        disconnect_gate = idle_start = idle_ticks = 0;
        bunching_ticks = (clock_t)(SYS_CLOCKS_PER_SEC*0.5F);
        total_transmitted_bytes = 0;
      }
    virtual void configure(kd_tcp_channel *Channel,
                           HANDLE Service_event,
                           float Max_connection_seconds,
                           float Max_bytes_per_second)
      { /* The `channel' supplied here is used to send all outgoing data
           chunks, and to receive the corresponding acknowledgement chunks.
           The channel is being donated by another part of the application
           which has already established the connection.  The caller should
           not explicitly close or destroy this channel, but this will happen
           automatically when the present object is destroyed.
             The `service_event' object is supplied by the network management
           thread which waits upon this event to know when it needs to call the
           `service_queue' function.
             The `max_connection_seconds' argument determines the point at
           which the server will disconnect from the client, allowing the
           connection resources to be handed to another client.
             The `max_bytes_per_second' argument determines the maximum average
           rate at which data will be sent out to the client.  The actual rate
           may be significantly lower, but establishing a maximum rate can be
           useful controlling the volume of external network traffic. */
        assert(closed_channel == NULL);
        this->channel = Channel; this->service_event = Service_event;
        channel->change_event(Service_event);
        this->max_bytes_per_second = Max_bytes_per_second;
        if (Max_connection_seconds > 0.0F)
          disconnect_gate = (clock_t)(SYS_CLOCKS_PER_SEC*Max_connection_seconds);
      }
    bool is_open() { return (channel != NULL); }
      /* Returns true if and only if the object is associated with an open
         TCP channel. */
    virtual ~kd_transmitter()
      {
        if (h_mutex != NULL)
          sysDestroyMutex(&h_mutex);
        if (channel != NULL)
          delete channel;
        if (closed_channel != NULL)
          delete closed_channel;
      }
    virtual int get_suggested_bytes()=0;
      /* Returns a suggested number of bytes for the application to prepare
         for transmission and push in via `push_chunk' before checking again
         to see if the client has sent any new requests which might change
         the decision as to what information should be transmitted next.
         The implementation of this  function depends upon the communication
         protocol being used, and how much information it gives the server
         regarding the network conditions. */
    virtual bool push_chunk(kds_chunk *chunk, HANDLE h_event,
                            const char *content_type)=0;
      /* This function attempts to enter a new data chunk buffer into a
         temporary holding queue, from which it will be taken into the
         transmission queue as slots become available.  If the holding queue
         is full, the function blocks the caller until one or more chunks
         have been moved onto the transmission queue, or the `h_event'
         object becomes signalled; this pre-supposes the existence of a
         separate network service thread, which invokes the `service_queue'
         function.
            The `content_type' argument may be NULL if the type of data
         contained in the chunk is unknown or need not be specified (e.g.,
         if it is not required by the transport protocol).  Otherwise, it
         must point to a constant string which holds the name of the MIME
         media type or any valid token which may appear within the body of
         an HTTP "Content-type:" header.
            `h_event' must be the handle to a valid event object, which is
         used only in the event that the caller must be blocked to wait for
         a slot to open up on the temporary holding queue.  The caller may
         arrange for this object to become signalled in the event that some
         other important event occurs -- e.g., another socket may become
         receivable.
            The function returns true, unless the conditions required to
         complete the chunk pushing operation have not yet occurred.  This
         may happen if the connection time limit has been reached, or if the
         caller would block (or does block) but `h_event' is signalled by
         another condition, established by the caller.  Expiration of the
         connection time limit may be checked by calling `get_time_left'.
            The function may throw an exception if the delivery manager has
         failed on this transmitter before, which generally means that the
         connection has been closed from the other end.
      */
    virtual kds_chunk *retrieve_completed_chunks()=0;
      /* This function returns NULL unless a previously pushed chunk is no
         longer required for servicing the connection.  Since transmission is
         asynchronous with calls to `push_chunk', the caller must not recycle
         any pushed storage until it gets the go-ahead from this function.
         The internal machinery may need to wait for client acknowledgement
         messages, or it may consider a chunk to have completed as soon as
         it has been pushed into a TCP queue, for example.
            Applications should normally call this function repeatedly until
         it returns NULL, immediately after or prior to a successful call to
         `push_chunk'.  It is safe to call the function at any time, as
         often as desired. */
    virtual int service_queue()=0;
      /* This function is to be executed from a special network service thread,
         which might be responsible for servicing any or all of the client
         connections.  The function receives any outstanding acknowledgement
         messages from the client (if the protocol requires explicit
         acknowledgement). It transmits any outstanding outgoing
         data chunks (i.e., those for which the delivery gate has been
         reached). In this process, the function may find that it can unblock
         a thread waiting inside the `push_chunk' function.
            Once the function has finished all oustanding servicing operations,
         it returns the number of miliseconds before the next service
         is required.  It also sets up the `service_event' object supplied in
         the constructor to be signalled when any of the following occur:
         1) a message arrives from the client; 2) new data becomes
         available and can be sent immediately.  The function returns a
         negative quantity if further service will not be required until the
         `service_event' object becomes signalled. */
  public: // Functions for monitoring transmission statistics and times.
    kdu_uint32 get_time_left()
      { /* Returns the amount of time left before the client connection
           should be terminated (based on the `max_connection_seconds' argument
           supplied to the constructor).  Returns 0 if the connection has
           already expired.  Otherwise, the return value indicates the number
           of milliseconds before the client must be forcibly disconnected. */
        clock_t now = sysClock() - base_time;
        if (disconnect_gate < now) return 0;
        return (int)(((float)(disconnect_gate-now))*(1000.0F/SYS_CLOCKS_PER_SEC));
      }
    int get_total_transmitted_bytes() { return total_transmitted_bytes; }
    float get_connected_seconds()
      { clock_t ticks = sysClock() - base_time;
        return ((float) ticks) / SYS_CLOCKS_PER_SEC; }
    float get_active_seconds()
      { clock_t ticks = sysClock() - base_time;
        if (idle_start != 0) ticks = idle_start;
        return ((float)(ticks - idle_ticks)) / SYS_CLOCKS_PER_SEC; }
  protected: // Helper functions
    void acquire_lock() { sysLockMutex(h_mutex); }
    void release_lock() { sysReleaseMutex(h_mutex); }
  protected: // Connection members
    kd_tcp_channel *channel; // Null if closed
    kd_tcp_channel *closed_channel; // Temporarily stores channel after closing
    float max_bytes_per_second;
  protected: // Synchronization
    HANDLE h_mutex; // Protects access to the various queues
    HANDLE source_event; // Used to wake up the source thread
    HANDLE service_event; // Supplied by network manager
    bool push_waiting; // True when `push_chunk' blocked.  Guarded by mutex.
  protected: // Timing information
    clock_t base_time; // Object creation time; see below.
    clock_t disconnect_gate; // Based on `max_connection_seconds'.
    clock_t delivery_gate; // Scheduled time to deliver next data chunk.
  protected: // Generic statistics
    int total_transmitted_bytes;
    clock_t idle_start;
    clock_t idle_ticks;
    clock_t bunching_ticks; // Amount delivery gate can get behind current time
  };

/*****************************************************************************/
/*                            kd_http_transmitter                            */
/*****************************************************************************/

class kd_http_transmitter : public kd_transmitter {
  public: // Member functions
    kd_http_transmitter()
      {
        free_list = holding_head = holding_tail = NULL;
        completed_head = completed_tail = NULL;
        holding_bytes = max_holding_bytes = 0;
        waiting_for_chunk = have_requests = need_chunk_trailer = false;
        close_requested = false;
      }
    virtual ~kd_http_transmitter();
    virtual void configure(kd_tcp_channel *Channel, HANDLE Service_event,
                           float Max_connection_seconds,
                           float Max_bytes_per_second)
      { // Override, to adjust the `max_holding_bytes' value.
        kd_transmitter::configure(Channel,Service_event,
                                  Max_connection_seconds,Max_bytes_per_second);
        max_holding_bytes = (int)(Max_bytes_per_second*1.0F);
        if ((max_holding_bytes <= 0) || (max_holding_bytes > 16000))
          max_holding_bytes = 16000;
      }
    void transfer_request_state(kd_request_queue *transfer_queue)
      { requests.transfer_state(transfer_queue); }
    kd_tcp_channel *close(kd_request_queue *queue, HANDLE h_event);
      /* The purpose of this function is to release the TCP channel, without
         actually destroying the transmitter object.  The transmitter can
         later be re-opened with a new TCP channel if desired, which will
         cause processing to resume as if nothing had happened.
           The channel cannot actually be released until all outstanding
         data chunks have been delivered, or the channel expiry deadline is
         reached, both of which could take some time.  The caller is blocked
         during this time, or until `h_event' becomes otherwise signalled.
         In the latter case, the function returns NULL, meaning that the
         function must be called again to complete the operation.  Otherwise,
         the function returns a pointer to the TCP channel which it is
         releasing, and transfers all fully and/or partially parsed requests
         to the supplied `queue' object; thereafter, the `is_open' function
         will return false. */
    void reopen(kd_tcp_channel *channel, kd_request_queue *transfer_queue);
      /* It is an error to call this function unless `is_open' returns false.
         It allows transmission services to be restored over a new channel,
         after a previous successful call to `close'.  See that function for
         an explanation. */
    virtual int get_suggested_bytes();
    virtual bool push_chunk(kds_chunk *chunk, HANDLE h_event,
                            const char *content_type);
      /* Implements the pure virtual function, `kd_transmitter::push_chunk'. */
    virtual kds_chunk *retrieve_completed_chunks();
      /* Implements `kd_transmitter::retrieve_completed_chunks'. */
    virtual int service_queue();
      /* Implements `kd_transmitter::service_queue'. */
    bool push_replies(kd_message_block &block, HANDLE h_event,
                      bool wait_for_chunk);
      /* Similar to `push_chunk', but pushes HTTP reply text stored in
         `block'.  As with `push_chunk', this function may block the caller
         until the temporary holding queue has sufficient space to accommodate
         the new data.  The function will return false, if the caller was
         unblocked (by `h_event' being signalled) before the conditions
         required for a successful push occurred.
            The caller should NOT insert HTTP transfer-encoding or
         content-type headers into its replies.  These are introduced
         automatically by the network service thread immediately before
         deliverying a reply paragraph which is followed by one or more
         data chunks.
            If `wait_for_chunk' is true, the service thread may not deliver
         the reply text until a data chunk is pushed in via `push_chunk'.
         This should always be the case for replies which will have a body.
         If `wait_for_chunk' is false, the service thread may deliver the reply
         text as soon as it likes.  If more reply text is pushed in before
         the service thread has a chance to deliver the current text, the
         text messages are concatenated and delivered together.
            Note carefully that HTTP reply paragraphs are expected to be
         terminated by a blank line, where each line terminates with a
         CRLF pair. */
    void terminate_chunked_data();
      /* Call this function if you know that the chunks most recently pushed
         into `push_chunk' constitute a complete response and that no further
         chunks will be pushed in without first sending an appropriate reply
         using `push_replies'.  Chunked data responses will be terminated
         automatically when new reply text is received and passed into
         `push_replies', but this may not happen until the client issues
         a new request, so if the current response is not being pre-empted
         by a new request, you should call this function when you are done. */
    bool retrieve_requests(kd_request_queue *queue, HANDLE h_event);
      /* Retrieves requests which have been received on the underlying TCP
         channel by the network delivery manager, appending them to the
         supplied queue.  This call never blocks the caller; however, if
         request information does later become available, the supplied
         `h_event' object will be signalled, which may be used to wake up the
         caller from an alternate sleep condition.  The function returns true
         if one or more new requests are appended to the `queue' and false
         otherwise. */

  private: // Definitions

    struct kd_queue_elt {
        kd_queue_elt *next;
        kd_message_block reply; // HTTP reply text; empty if non-initial chunk.
        int reply_bytes; // Num bytes in `reply' before any adjustments
        kds_chunk *chunk; // May be NULL if the reply has no body.
        int chunk_bytes; // Num bytes in `chunk' still to be sent.
        int started_bytes; // Sum of adjusted reply bytes and chunk bytes
                         // Becomes non-zero only once transmission has started
        const char *content_type; // Content-type string for HTTP replies; NULL
                                  // if unknown or irrelevant
      };

  private: // Helper functions
    kd_queue_elt *get_new_holding_element()
      {
        kd_queue_elt *qelt = free_list;
        if (qelt == NULL)
          qelt = new kd_queue_elt;
        else
          free_list = qelt->next;
        qelt->reply.restart();
        qelt->chunk = NULL;
        qelt->content_type = NULL;
        qelt->next = NULL;
        qelt->started_bytes = qelt->reply_bytes = qelt->chunk_bytes = 0;
        if (holding_tail == NULL)
          holding_head = holding_tail = qelt;
        else
          holding_tail = holding_tail->next = qelt;
        return qelt;
      }
  private: // Data
    kd_queue_elt *free_list; // List of unused `kd_queue_elt' structures.
    kd_queue_elt *holding_head; // Queue of msgs waiting for transmission
    kd_queue_elt *holding_tail; // Tail of above queue.
    kd_queue_elt *completed_head; // Queue of msgs already transmitted
    kd_queue_elt *completed_tail; // Tail of above queue
    int holding_bytes; // Total bytes in holding queue
    int max_holding_bytes; // Max bytes allowed in holding queue.
    bool waiting_for_chunk; // If true, cannot deliver last holding elt yet.
    bool need_chunk_trailer; // If no trailer included in text since last chunk
    kd_request_queue requests; // Collects requests sent by client.
    bool have_requests; // True if something in `requests'.
    bool close_requested; // If service thread is asked to close channel
  };

/*****************************************************************************/
/*                             kd_tcp_transmitter                            */
/*****************************************************************************/

class kd_tcp_transmitter : public kd_transmitter {
  public: // Member functions
    kd_tcp_transmitter()
      { // Basic initialization here.
        window_jitter = 0;
        avg_1 = avg_t = avg_b = avg_bb = avg_tb = 0.0;
        max_chunk_bytes = 1;
        window_threshold = 1; // Wait for round trip before growing window
        nominal_window_threshold = 1;
        max_rtt_target = 1.0F; min_rtt_target = 0.0F; rtt_target = 0.5F;
        free_list = xmit_head = xmit_tail = holding_head = holding_tail = NULL;
        acknowledged_head = acknowledged_tail = NULL;
        window_bytes = holding_bytes = 0;
        total_rtt_events = total_acknowledged_bytes = 0;
        total_rtt_ticks = 0;
      }
    virtual ~kd_tcp_transmitter();
    void configure_flow_control(float max_rtt_target_seconds,
                                float min_rtt_target_seconds);
      /* The delivery algorithm transmits data at the maximum allowed
         rate until the number of outstanding transmitted bytes (bytes
         which have been transmitted, but not yet acknowledged) reaches an
         adaptive window size.  The window size is adjusted whenever
         an acknowledgement message is received, so as to keep the total
         round-trip time approximately equal to some target value, RTT_target.
         The actual value of RTT_target is also slowly adapted, within the
         range given by `min_rtt_target_seconds' and `max_rtt_target_seconds',
         so as to find the smallest RTT which is consistent with efficient
         utilization of the network's capacity.  The idea is that
         RTT_target should be large enough to ensure that we are not held
         up by the intrinsic round-trip delay (i.e., the part of the RTT
         which is not dependent upon the transmission rate).  Of course,
         if the network is capable of transmitting data at rates in excess
         of `max_bytes_per_second', the algorithm will never be able to
         increase the number of outstanding bytes in the queue beyond a
         certain point. */
    virtual int get_suggested_bytes();
    virtual bool push_chunk(kds_chunk *chunk, HANDLE h_event,
                            const char *content_type);
      /* Implements the pure virtual function, `kd_transmitter::push_chunk'. */
    virtual kds_chunk *retrieve_completed_chunks();
      /* Implements `kd_transmitter::retrieve_completed_chunks'. */
    virtual int service_queue();
      /* Implements `kd_transmitter::service_queue'. */
  public: // Statistics functions
    int get_total_acknowledged_bytes() { return total_acknowledged_bytes; }
    int get_total_rtt_events() { return total_rtt_events; }
    float get_average_rtt()
      {
        if (total_rtt_events == 0)
          return get_connected_seconds();
        else
          return ((float) total_rtt_ticks) / (total_rtt_events*SYS_CLOCKS_PER_SEC);
      }
  private: // Helper functions
    void update_windowing_parameters(float interval_time, int interval_bytes,
                                float acknowledge_rtt, int acknowledge_window);
      /* This function implements the adaptive algorithms which adjust the
         window threshold in order to achieve the target RTT, and also adjust
         the target RTT itself.  The first two arguments identify the time
         from the point at which the current acknowledgement interval started
         until the present record is acknowledged, and the number of bytes
         pushed into the queue since the acknowledgement window started, up to
         and including the point at which the current record was pushed into
         the queue.  The second two arguments indicate the round-trip-time
         for the current record and the number of outstanding bytes in the
         transmission queue (window) immediately after the current record
         was transmitted. */

  private: // Definitions
    struct kd_queue_elt {
        kd_queue_elt *next;
        kds_chunk *chunk;
        clock_t transmit_time;
        int window_bytes;
        clock_t interval_start_time;
        int interval_bytes;
      };
      /* Notes: `window_bytes' is the number of unacknowledged transmitted
         bytes at the point when this record was sent.  This may be used for
         `interval_bytes' as well, in which case `transmit_time' will be used
         for `interval_start_time'.  However, the timed acknowledgement
         interval used for estimating network conditions is usually based
         upon the the window size and transmission time of the first element
         in the transmission queue. */

  private: // Resources
    kd_queue_elt *free_list; // List of unused `kd_queue_elt' structures.
    kd_queue_elt *xmit_head; // Queue of msgs xmitted but not acknowledged.
    kd_queue_elt *xmit_tail; // Tail of above queue.
    kd_queue_elt *holding_head; // First chunk in temporary holding queue.
    kd_queue_elt *holding_tail; // Last chunk in temporary holding queue.
    kd_queue_elt *acknowledged_head; // First and last chunk in temporary
    kd_queue_elt *acknowledged_tail; // list of acknowledged chunks.
    int window_bytes; // Total number of bytes in the transmitted data queue
    int holding_bytes; // Total number of bytes in temporary holding queue
  private: // Current transmission parameters
    float max_rtt_target, min_rtt_target;
    float rtt_target;
    int window_threshold; // Can't xmit until `window_bytes'<`window_threshold'
  private: // State information used to drive adaptation algorithms
    int max_chunk_bytes;
    double avg_1; // Sum of the local averaging weights
    double avg_t; // Local average of the acknowledgement interval time
    double avg_b; // Local avg of the acknowledgement interval bytes
    double avg_bb; // Local avg of squared interval bytes
    double avg_tb; // Local avg correlation between interval bytes and time.
    float estimated_network_delay; // Measured in seconds
    float estimated_network_rate; // Measured in bytes per second
    int nominal_window_threshold; // `window_threshold' minus `window_jitter'
    int window_jitter;  // Jitter added to force variation in window size to
                        // get useful network statistics.
  private: // Statistics
    int total_acknowledged_bytes;
    clock_t total_rtt_ticks; // Cumulative transmit to acknowledge times
    int total_rtt_events; // Increments whenever an acknowledgement comes in
  };
  /* Notes:
        The list of transmitted data chunks headed by `xmit_head' is ordered
     by transmission time, with the least recently transmitted chunk
     appearing at the head of the queue.
        The temporary holding queue has the same order.  Chunks are pushed
     onto the end of the queue and moved from the start of the queue onto the
     end of the transmission queue.
        Once a chunk has been acknowledged, it is moved onto the acknowledged
     list from which it will be removed by subsequent calls to
     `retrieve_acknowledged'.
        The `base_time' member holds the return value of `clock()' at the
     time when the object was created.  This is the time when the server
     began servicing a particular client.  The time is subtracted from
     the return value of `clock()' before it is used to estimate elapsed
     times.  The purpose of this is to avoid wrap-around problems in the
     numeric representation associated with the clock_t data type.  These
     problems could manifest themselves after the server process has been
     running for several days.  Subtracting `base_time' from all the times
     essentially makes the clock references relative to the start of the
     client service, as opposed to the server process.  In this event,
     wrap-around problems would not be expected unless a single client were
     served continuously for a period of several days -- an event unlikely
     to be tolerated by a practical server application.
        The `delivery_gate' member is set whenever a chunk is transmitted
     to the client, to identify the point at which the next chunk may be
     transmitted. */

/*****************************************************************************/
/*                            kd_delivery_manager                            */
/*****************************************************************************/

// Note: This object exists to manage the network service thread.
class kd_delivery_manager {
  public: // Member functions
    kd_delivery_manager(float Max_connection_seconds,
                        float Max_rtt_target_seconds,
                        float Min_rtt_target_seconds,
                        float Max_bytes_per_second)
      {
        this->max_connection_seconds = Max_connection_seconds;
        this->max_rtt_target_seconds = Max_rtt_target_seconds;
        this->min_rtt_target_seconds = Min_rtt_target_seconds;
        this->max_bytes_per_second = Max_bytes_per_second;
        transmitter_list = NULL; num_transmitters = 0;
        h_mutex = sysCreateMutex();
        h_event = sysCreateEvent(TRUE);
        h_thread = NULL; thread_closure_requested = false;
      }
    ~kd_delivery_manager();
      /* You must not call this function until all clients have been
         removed.  The function should not be called from within the
         network service thread itself. */
    void start();
      /* Starts the thread running. */
    void thread_entry(); // Main thread entry point function for this object
    kd_tcp_transmitter *
      get_tcp_transmitter(kd_tcp_channel *channel);
      /* Creates a new `kd_tcp_transmitter' object and adds it to the list
         of transmitters whose channels are being serviced by the delivery
         management thread.  The caller should not explicitly delete the
         returned object, but should instead call `release_transmitter'
         when done.  The `channel' argument refers to an object which the
         caller is donating to the returned TCP transmitter object.  It
         will be closed and destroyed automatically when that object is
         passed to `release_tcp_transmitter'. */
    kd_http_transmitter *
      get_http_transmitter(kd_tcp_channel *channel,
                           kd_request_queue *transfer_queue);
      /* Same as `get_tcp_transmitter', but creates an object for use with
         protocols which deliver the image data in HTTP response messages.
         A key difference from the TCP transmitter is that this object also
         takes care of collecting HTTP requests and delivering the HTTP
         response headers.  Since the `channel' may have been previously
         in use elsewhere, there may be some partially transmitted requests.
         These are pulled off the `transfer_queue' object, if non-NULL,
         using `kd_request_queue::transfer_state'. */
    void release_transmitter(kd_transmitter *transmitter);
      /* Used to recycle resources associated with the transmitter created
         by `get_tcp_transmitter' or `get_http_transmitter'. */
  private: // Helper functions
    void acquire_lock() { sysLockMutex(h_mutex); }
    void release_lock() { sysReleaseMutex(h_mutex); }
  private: // Definitions
      struct kd_transmitter_list {
          kd_transmitter *transmitter;
          kd_transmitter_list *next;
        };
  private: // Data
    kd_transmitter_list *transmitter_list;
    int num_transmitters;
    float max_connection_seconds;
    float max_rtt_target_seconds;
    float min_rtt_target_seconds;
    float max_bytes_per_second;
    HANDLE h_mutex; // Protects access to the delivery list.
    HANDLE h_event; // Local event passed to `kd_deliver::serice_queue'
    HANDLE h_thread;
    bool thread_closure_requested;
  };

/*****************************************************************************/
/*                           kd_target_description                           */
/*****************************************************************************/

struct kd_target_description {
  public: // Member functions
    kd_target_description()
      { filename[0]=byte_range[0]='\0'; range_start=0; range_lim=KDU_LONG_MAX;}
    bool parse_byte_range();
      /* Converts the `byte_range' string to numeric values writing them
         to the `range_start' and `range_lim' members.  If the `byte_range'
         string is empty, the ranges are from 0 to KDU_LONG_MAX.  If an
         error occurs, the function returns false. */
  public: // Data
    char filename[256];
    char byte_range[80]; // Holds the
    kdu_long range_start;
    kdu_long range_lim;
  };

/*****************************************************************************/
/* ENUM                       kd_jpip_channel_type                           */
/*****************************************************************************/

enum kd_jpip_channel_type {
    KD_CHANNEL_STATELESS=0,
    KD_CHANNEL_HTTP, // Channel using HTTP transport
    KD_CHANNEL_HTTP_TCP // Channel using HTTP-TCP transport
  };

/*****************************************************************************/
/*                        kd_synchronized_serve_target                       */
/*****************************************************************************/

class kd_synchronized_serve_target : public kdu_servex {
  public: // Member functions
    kd_synchronized_serve_target()
      {
        h_mutex = NULL;
        h_event = sysCreateEvent(TRUE);
      }
    void install_mutex(HANDLE H_mutex)
      { this->h_mutex = H_mutex; }
    void acquire_lock() { sysLockMutex(h_mutex); }
    void release_lock() { sysReleaseMutex(h_mutex); }
    void wait_event()   { sysWaitForEvent(h_event,INFINITE); }
    void pulse_event()  { sysPulseEvent(h_event); }
  private: // Data
    HANDLE h_mutex;
    HANDLE h_event;
  };

/*****************************************************************************/
/*                                kd_source                                  */
/*****************************************************************************/

struct kd_source {
  public: // Member functions
    kd_source()
      {
        threads = NULL; manager = NULL; prev = next = NULL;
        target_id[0] = '\0'; source_id = 0; failed = false;
        h_mutex = sysCreateMutex();
        serve_target.install_mutex(h_mutex);
      }
    ~kd_source()
      { // Note: you may need to call `unlink' first.
        sysDestroyMutex(&h_mutex);
        assert(manager == NULL);
        serve_target.close();
      }
    void unlink();
      /* Unlinks the source from any source manager's queue, decrementing
         the manager's source count.  Does nothing unless there is a source
         manager.  Note that this function DOES NOT ACQUIRE the source
         manager's mutex.  This must be done by the caller. */
    bool open(kd_target_description &target, kd_message_block &explanation,
              int phld_threshold, int per_client_cache,
              const char *cache_directory);
      /* Called after construction to open the source file, read any file
         format preamble, and create the `codestream' object.  If an
         error occurs, an explanatory HTML status line (starting from the
         error code) is written to `explanation', without any new-line
         character, and the function returns false.  Otherwise, the function
         returns true without writing any explanation.  The implementation
         of this function must be careful to catch exceptions which may be
         generated during the file opening process. */
    bool generate_target_id();
      /* Attempts to generate the target-id string from the filename and
         byte range.  Returns false, leaving `target_id' as an empty string
         if this cannot be achieved.  Otherwise, generates a unique target
         ID string which is exactly 32 characters long. */
    void acquire_lock() { sysLockMutex(h_mutex); }
    void release_lock() { sysReleaseMutex(h_mutex); }
  private: // Data
    HANDLE h_mutex; // Used by source threads to access code-stream.
  public: // Data
    kd_target_description target;
    char target_id[33]; // 128-bit quantity expressed as an ASCII hex string
    kdu_uint32 source_id; // For rapid identification
    kd_synchronized_serve_target serve_target;
    kd_source_thread *threads; // Doubly linked list of sources using me
    kd_source_manager *manager; // My manager
    kd_source *prev, *next; // To build doubly linked list inside manager
    bool failed;
  };

/*****************************************************************************/
/*                              kd_source_thread                             */
/*****************************************************************************/

class kd_source_thread : private kdu_serve {
  public: // Member function
    kd_source_thread(kd_source *source, kd_source_manager *manager,
                     kd_delivery_manager *delivery_manager,
                     const char *channel_id, kd_jpip_channel_type channel_type,
                     int listen_port, int max_chunk_bytes,
                     bool ignore_relevance_info, int completion_timeout);
    ~kd_source_thread();
      /* Must not be called from within the thread itself. */
    void start();
      /* Starts the thread running.  If this function fails, the `closing'
         function will be called immediately from the present thread. */
    void closing(); // Called from thread start proc right before thread exit
    void thread_entry(); // Main thread entry point function for this object
    bool install_channel(bool auxiliary, kd_tcp_channel *channel,
                         kd_request_queue *transfer_queue,
                         kd_connection_server *cserver);
      /* Does the handoff for `kd_source_manager::install_channel'. */
  private:
    void http_tcp_service_loop();
    void http_service_loop();
    void wait_event(kdu_uint32 timeout_milliseconds=INFINITE)
      { sysWaitForEvent(h_event,timeout_milliseconds); }
    void signal_event() { sysSetEvent(h_event); }
    void reset_event() { sysResetEvent(h_event); }
    bool process_window_requests(kdu_window &window, int &max_bytes,
                               bool &need_target_id, bool &align,
                               bool &extended_headers, bool &jpip_cclose,
                               int &num_preempted_requests, int &roi_index,
                               bool *close_after_response=NULL,
                               bool *release_channel=NULL);
      /* Pops requests one by one from the `pending_requests' queue,
         using them to configure the supplied `window' and set the
         values associated with the last 6 arguments.  The first 5 of these
         last 6 argument values are not modified unless the function
         encounters a valid request, in which event it returns true.
            The `extended_headers' argument is treated a little differently
         to the others.  If the request includes a "type" request field
         which identifies the request type as "jpp-stream", with an optional
         parameter of "ptype=ext", the value of `extended_headers' is set to
         true.  If the request includes a "type" request field which
         identifies the request type as "jpp-stream" but without the optional
         parameter expression, the value of `extended_headers' is set to false.
         Otherwise, the value of `extended_headers' is left unchanged.  Thus
         `extended_headers' really references a state variable which retains
         its state between requests, until a request includes an appropriate
         "type" field.
            The `max_bytes' argument is set to reflect the maximum number of
         bytes which should be sent in response to the request.  The
         `need_target_id' flag is set to true if the client's request
         identifies a target ID which differs from that actually being used;
         otherwise it is set to false.  The `align' flag is set if the
         response data should serve to align all referenced data-bins to
         natural boundaries; otherwise it is set to false.
            If multiple requests are present, the function processes all
         requests up until the most recent request whose
         `kd_request::preemptive' flag is true.  If no request has this flag
         set, the function processes requests only the first valid request,
         leaving the rest on the queue.  Replies to all but the last request
         processed here are written to the private `reply_block' member (it
         is reset upon entry to this function).  If a request is received for
         a different target or JPIP channel, the function also appends
         appropriate error replies to the `reply_block' object, in the
         appropriate order.  If a request contains bad parameters, the
         function again appends the appropriate HTTP error replies to the
         `reply_block' object.
            The `roi_index' argument is used to return the index of a
         custom target-defined ROI which is being used to determine the
         region of interest -- see `kdu_serve_target::find_roi' for more
         information.  A negative value is returned if the request includes
         an "roi" request field whose identifier could not be matched.
            If `close_after_response' is non-NULL, the function sets
         *`close_after_response' to true if the last processed request
         indicates that the connection should be closed once the request
         has been handled (see `kd_request::close_connection').
            If `release_channel' is non-NULL, the function sets
         *`release_channel' to true if a request is encountered which
         does not belong to the current source.  In this case, the
         request is placed back on the head of the request queue.  A non-NULL
         `release_channel' argument should be supplied only where the
         caller intends is prepared to release the channel back to the
         connection manager for re-assignment, without actually closing the
         current connection.
            The function returns false if `window' does not contain a valid
         new request window.  This can happen if there were no requests with
         the correct channel ID or target, or if the last request
         processed contained syntax errors, causing earlier requests to be
         pre-empted, but not contributing a valid window itself.  If the
         function returns true, the response to the last processed request
         has still to be written to the `reply_block' member.
            Upon return, the `num_preempted_requests' argument is augmented
         by the number of requests for which return data will not be
         generated.  This number of terminators may need to be sent to the
         client on the separate return/acknowledge channel, for protocols
         which have such a channel.  Note carefully, that the argument is
         treated as a counter; it is not reset anywhere in this function.
      */
    bool process_cache_specifiers(const char *string,
                                  int default_codestream_id,
                                  bool need=false);
      /* Used by `process_window_request' to parse the bodies of the JPIP
         request fields "need=" and "model=".  If `need' is true, we
         are processing a "need=" request field, which means that all
         cache specifiers must be applied subtractively to the cache model,
         rather than additively.  The `default_codestream_id' value is used
         to provide code-stream scope for cache specifiers which appear
         outside the context of any code-stream qualifiers.  If a parsing
         error occurs, the function returns false. */
    void generate_window_reply(kdu_window &requested, kdu_window &actual,
                               int requested_max_bytes, int actual_max_bytes,
                               bool need_target_id, int roi_index,
                               bool close_after_response=false);
      /* Generates a reply in response to a requested window change which
         we are using to generate new compressed data.  The reply indicates
         any differences between the requested window and the actual window
         which is being serviced.  The reply is not actually transmitted to
         the remote client, but is instead appended to the `reply_block'
         member for later delivery by the caller.  The `need_target_id'
         argument has the value obtained from the `process_window_request'
         function.
            Note that upon successful return, the component and code-stream
         identifiers in the `requested' object will be arranged in strictly
         increasing order.
            The `roi_index' argument is obtained from the most recent call
         to `process_window_requests'.  It is used to generate a "JPIP-roi:"
         header if necessary.  If 0, no "JPIP-roi:" header is required.  If
         negative, an unrecognized ROI must be signalled.  If positive,
         a recognized ROI must be signalled, along with the full region
         and resolution associated with the ROI.
            If `close_after_response' is true, a "connection: close" header
         is included with the reply text. */
  private: // Data
    HANDLE h_thread;
    HANDLE h_event; // For network communications only
    int max_chunk_bytes;
    bool ignore_relevance_info;
    int prefix_bytes; // Set inside `thread_entry'
    int completion_timeout;
    kd_source *source; // NULL if the thread has died
    kd_source_manager *manager; // Used for entering on dead threads list
    kd_delivery_manager *delivery_manager;
    kd_tcp_transmitter *tcp_transmitter;
    kd_http_transmitter *http_transmitter;
    kdu_byte *extra_data_buf; // Has `max_chunk_bytes' elements.
    kd_message_block reply_block;
      // For use with the `kdu_serve::push_extra_data' function.
    bool jpip_cnew_header_required; // If required in next JPIP response
    bool destruction_scheduled; // True if I'm the one setting `source->failed'
    bool waiting_for_primary_channel;
    bool waiting_for_auxiliary_channel;
    kd_tcp_channel *request_channel;
    kd_tcp_channel *primary_channel;
    kd_tcp_channel *auxiliary_channel;
    kd_request_queue primary_holding_queue;// Passes queue from `install_channel'
    kd_connection_server *cserver; // Place to return any spent channels
    kd_request_queue pending_requests;
  private: // Statistics
    int num_connections; // Number of connection re-establishment events
    int total_received_requests;
    int total_serviced_requests;
    int num_serviced_with_byte_limits; // Requests with byte limits (jpip-h)
    int cumulative_byte_limit; // Sum of limits for above (jpip-h only)
    float idle_start;
  public: // Links and identification
    int listen_port; // For writing "JPIP-cnew" headers.
    kd_jpip_channel_type jpip_channel_type;
    bool is_stateless; // If `jpip_channel_type'=KD_CHANNEL_STATELESS
    char channel_id[KD_CHANNEL_ID_LEN+1]; // For stateful sessions only
    kd_source_thread *next, *prev; // source threads list is doubly linked
  };

/*****************************************************************************/
/*                              kd_client_history                            */
/*****************************************************************************/

struct kd_client_history {
  public: // generic data
    enum kd_jpip_channel_type jpip_channel_type;
    kd_target_description target;
    int transmitted_bytes;
    int num_requests;
    int num_connections; // Number of channel re-connection operations
    float connected_seconds;
    kd_client_history *next, *prev;
  public: // "jpip-ht" specific data
    int rtt_events;
    float average_rtt;
    float approximate_rate;
  public: // "jpip-h" specific data
    float average_serviced_byte_limit; // Average byte limit specified in
                                 // requests which were not pre-empted
    float average_service_bytes; // Average bytes actually served for requests
                                 // which were not pre-empted
  };

/*****************************************************************************/
/*                              kd_source_manager                            */
/*****************************************************************************/

class kd_source_manager {
  public: // Member functions
    kd_source_manager(kd_delivery_manager *delivery_manager, int listen_port,
                      int max_sources, int max_threads, int max_chunk_bytes,
                      bool ignore_relevance_info, int phld_threshold,
                      int per_client_cache, int max_history_records,
                      int completion_timeout, const char *password,
                      HANDLE shutdown_event, const char *cache_directory);
      /* If `password' is NULL, no external administration access will be
         allowed.  The `shutdown_event' will be signalled if
         a service thread processes an administration shutdown message. */
    ~kd_source_manager();
    void request_shutdown()
      { shutdown_requested = true; sysSetEvent(shutdown_event); }
    bool check_shutdown_requested() { return shutdown_requested; }
    void delete_dead_threads()
      {
        kd_source_thread *tmp;
        acquire_lock();
        while ((tmp=dead_threads) != NULL)
          {
            dead_threads = tmp->next;
            delete tmp;
          }
        release_lock();
      }
    bool
      create_source_thread(kd_target_description &target,
                           const char *jpip_channel_type,
                           char channel_id[],
                           kd_message_block &explanation);
      /* This key function attempts to locate an open source for the
         indicated image target.  If one cannot be found, a new source is
         created.  Before successful return, the function creates
         and attaches a dedicated service thread, which will wait for the
         required persistent connection sockets to be supplied by
         asynchronous calls to `install_channel', and then proceed to handle
         window requests.  The first channel ID string associated with the
         newly created source thread is returned via the `channel_id' buffer
         which must be at least KD_CHANNEL_ID_LEN+1 characters in length
         (KD_CHANNEL_ID_LEN characters, plus one for the null terminator).
            Note that the `target' object must already have had its
         `kd_target_description::parse_byte_range' function successfully
         invoked.
            As might be expected, various error conditions might occur.  If
         the maximum thread count or the maximum source count is reached, or
         the image does not exist, or the file exists but cannot be opened as
         a valid JPEG2000 file, the function returns false.  In this case,
         a text explanation is written to the `explanation' object.  The text
         explanation is a valid HTTP status line starting with the status
         code (not 200) and finishing with the human readable explanatory
         message; no new-line character should be used, and the message
         should be no longer than a couple of hundred characters at most.
         If the function returns true, no text message is written to
         `explanation'.
            If `jpip_channel_type' is NULL, the created source thread operates
         in the stateless mode, deliberately not keeping track of the
         information which has been sent to the client between requests.
         Apart from this, the source thread created in this case behaves in
         exactly the same way as if `jpip_channel_type' were "http".  The only
         two channel type strings currently recognized are "http" and
         "http-tcp", but `jpip_channel_type' may contain a list containing
         either or both of these.
            Note that although this function creates resources, there is no
         guarantee that a client will actually use them.  For this reason, the
         newly created service thread should time its socket wait functions,
         terminating and cleaning up the resources if a timeout is
         exceeded before all required communication channels have been
         connected. */
    bool install_channel(const char *channel_id, bool auxiliary,
                         kd_tcp_channel *channel,
                         kd_request_queue *transfer_queue,
                         kd_connection_server *cserver,
                         kd_message_block &explanation);
      /* Used by connection threads to hand a socket and associated channel
         management machinery to a source thread.  If `auxiliary' is true,
         the new channel corresponds to the second (auxiliary) channel
         required by the relevant communication protocol.  Otherwise, it
         represents the main communication channel for requests and replies.
         The auxiliary channel (if any) is used for return/acknowledge
         messages.
            The `transfer_queue' argment supplies fully or partially parsed
         requests which have already been recovered from the channel.  For
         primary channels, the request on the head of the transfer queue is
         the one which was used to generate this call.  The `transfer_queue'
         argument should be NULL if `auxiliary' is true.
            If an active thread with the indicated `channel_id' cannot be
         found, the function returns false, in which case it is the caller's
         responsibility to respond to the client and/or close the channel.  If
         the function returns false, it must write the reason to the
         `explanation' object; the reason must commence with an HTTP error
         code, followed by a text description and a single CRLF pair.  This
         is used by the caller to construct an appropriate error response
         to the client. */
    bool write_admin_id(kd_message_block &block);
      /* Advances the admin id and writes it as a 32 character hexadecimal
         string to `block'.  Returns false if remote administration is
         not allowed. */
    bool compare_admin_id(const char *string);
      /* Advances the admin id and compares it with `string'.  Returns
         true if the comparison is successful. */
    bool advance_admin_id(kdu_byte buf[16]);
      /* Advances the admin id and writes it to the supplied buffer.  Returns
         false if remote administration is not supported. */
    void write_stats(kd_message_block &block);
      /* Writes statistical information to the supplied message block, for
         reporting in response to a remote administration call. */
    void write_history(kd_message_block &block, int max_records);
      /* Writes client connection history to the supplied message block,
         for reporting in response to a remote administration call. */
    kd_client_history *get_new_history_entry();
      /* Used by `kd_source_thread::closing' to get a new history record
         to fill with information about the connection which is closing.
         The caller must have called `acquire_lock' before using this
         function.  Returns NULL if the history feature is not enabled. */
    void acquire_lock() { sysLockMutex(h_mutex); }
    void release_lock() { sysReleaseMutex(h_mutex); }
    void wait_event()   { sysWaitForEvent(h_event,INFINITE); }
    void signal_event() { sysSetEvent(h_event); }
  private: // Data
    HANDLE h_mutex;
    HANDLE h_event;
    HANDLE shutdown_event; // Supplied in constructor
    int listen_port;
    int max_chunk_bytes;
    bool ignore_relevance_info;
    int phld_threshold;
    int per_client_cache;
    int completion_timeout; // Time allowed for client to call back.
    bool shutdown_requested;
    const char *cache_directory; // NULL if ".cache" files go in same directory
                                 // as source image files.
  public: // Links
    kd_delivery_manager *delivery_manager;
    int max_sources;
    int num_sources;
    kd_source *sources; // Doubly linked list
    int max_threads;
    int num_threads;
    kd_source_thread *dead_threads; // Singly linked list
  public: // Statistics
    int total_clients;
    kdu_long total_transmitted_bytes; // Updated when client connection closes
    int completed_connections; // Num clients successfully connected & closed
    int terminated_connections; // Num terminated before connecting channels
    int num_history_records;
    int max_history_records;
    kd_client_history *history_head, *history_tail;
    kdu_byte admin_id[16];
    kdu_rijndael admin_cipher;
  };

/*****************************************************************************/
/*                           kd_connection_thread                            */
/*****************************************************************************/

class kd_connection_thread {
  public: // Member functions
    kd_connection_thread(kd_connection_server *Owner,
                         kd_source_manager *Source_manager,
                         kd_delegator *Delegator)
      {
        h_event = sysCreateEvent(FALSE); // Auto-reset
        h_thread = NULL;
        new_socket = INVALID_SOCKET;
        is_idle = true;
        timeout_milliseconds = 0;
        termination_requested = false;
        this->owner = Owner;
        this->source_manager = Source_manager;
        this->delegator = Delegator;
        channel = NULL;
        next = NULL;
      }
    ~kd_connection_thread()
      { // Must not be called from within the thread itself.
        termination_requested = true;
        signal_event(); // Wake up entry-point function if necessary
        if (h_thread != NULL)
          {
            sysWaitForThreadEnd(h_thread);
            sysDestroyThread(&h_thread);
          }
        if (channel != NULL)
          delete channel;
        sysDestroyEvent(&h_event);
      }
    void start(); // Starts the thread running.
    void thread_entry(); // Main thread entry-point function to this object
    void service_request(SOCKET connected, int Timeout_milliseconds)
      { /* Wakes up the main loop inside the `thread_entry' function to do the
           work of `kd_connection_server::service_request'. */
        assert((new_socket == INVALID_SOCKET) && is_idle && (channel == NULL));
        this->timeout_milliseconds = Timeout_milliseconds;
        this->request_queue.init();
        new_socket = connected;
        is_idle = false;
        signal_event();
      }
    void service_request(kd_tcp_channel *connected,
                         kd_request_queue &existing_queue,
                         int Timeout_milliseconds)
      { /* Same as the other version of this overloaded function, except that
           a `kd_tcp_channel' object which already exists is being donated
           for re-use by the present object, and the contents of the
           `existing_queue' of partially and/or fully parsed requests
           are to be transferred to `request_queue' as part of the initial
           state to be presented to the connection establishment logic. */
        assert((new_socket == INVALID_SOCKET) && is_idle && (channel == NULL));
        this->timeout_milliseconds = Timeout_milliseconds;
        this->channel = connected;
        this->request_queue.init();
        this->request_queue.transfer_state(&existing_queue);
        is_idle = false;
        signal_event();
      }
    void wait_event() { sysWaitForEvent(h_event,INFINITE); }
    void signal_event() { sysSetEvent(h_event); }
  private: // Helper functions
    void process_stateless_request(kd_target_description &target,
                                   const kd_request *request);
      /* Used to satisfy calls which involve no session.  If successful, the
         channel is passed to the source thread and the `channel' member
         is set to NULL.  Otherwise, an error message is delivered to the
         client and the function returns with `channel' still active. */
    bool process_new_session_request(kd_target_description &target,
                                     const kd_request *request);
      /* Attempts to create a new session for the indicated image target,
         using the channel transport type in `request->fields.channel_new'
         (one of "http" or "http-tcp").  If delegation hosts are available,
         this function first attempts to delegate the session request.  If
         the request is handled locally, the channel is passed to the source
         thread which will manage the session and the `channel' member is set
         to NULL.  Otherwise, either the delegated host's response or an
         error message is delivered to the client and the function returns
         with `channel' still active.  Returns true if the request was
         delegated or the channel was passed on to a local source thread. */
    void process_admin_request(const char *command);
      /* Scans the `command' string to determine how to process
         the remote administration request. */
  private: // Data
    friend class kd_connection_server;
    kd_delegator *delegator;
    HANDLE h_event; // For receiving a new work item, and to manage HTTP timing
    SOCKET new_socket; // Used by `service_request' to pass request to thread
    kd_tcp_channel *channel;
    int timeout_milliseconds; // For each HTTP transaction
    kd_request_queue request_queue;
    kd_message_block send_par; // For assembling HTTP response paragraphs
    kd_message_block send_body; // For assembling HTTP entity bodies
    bool is_idle; // True if loop is waiting to service a new request
  public: // Links
    kd_source_manager *source_manager;
    kd_connection_server *owner;
    HANDLE h_thread;
    bool termination_requested;
    kd_connection_thread *next;
  };

/*****************************************************************************/
/*                           kd_connection_server                            */
/*****************************************************************************/

  /* This class manages the connection establishment process, which is
     performed over HTTP.  It manages a pool of threads which are used
     to perform the connection establishment signalling.  When all threads
     are occupied, the main socket listening loop must wait, but the presence
     of multiple threads allows any single client to take some time connecting
     (possibly over a slow link), without preventing the server from
     connecting other clients.  TCP connections which have been used to
     service a JPIP session may be returned to the connection server if
     the client did not close them when it closed the JPIP session. */
class kd_connection_server {
  public: // Member functions
    kd_connection_server(kd_source_manager *Source_manager,
                         bool Restricted_access,
                         int Max_threads, kd_delegator *Delegator,
                         int Completion_timeout_milliseconds)
      {
        this->source_manager = Source_manager;
        this->restricted_access = Restricted_access;
        threads = NULL; num_threads = idle_threads = 0;
        this->max_threads = Max_threads;
        this->delegator = Delegator;
        this->timeout_milliseconds = Completion_timeout_milliseconds;
        pending_active_head = pending_active_tail = NULL;
        h_mutex = sysCreateMutex();
        h_event = sysCreateEvent(FALSE); // Auto-reset
      }
    ~kd_connection_server();
      /* Sets `max_threads' to 0 and waits for all threads to exit from their
         thread start up procedure; then deletes the thread objects. */
    void wait_for_resources()
      { // Waits until at least one thread is free
        bool available = false;
        while (!available)
          {
            acquire_lock();
            dispatch_pending_active_channels();
            available = (num_threads < max_threads) || (idle_threads > 0);
            release_lock();
            if (!available) wait_event();
          }
      }
    void service_request(SOCKET connected);
      /* Creates a new thread or pulls one off the free list and asks
         it to negotiate with the HTTP client over the `connected'
         socket.  `timeout_milliseconds' holds the maximum total number of
         milliseconds between the point at which an HTTP transaction (e.g.,
         read request, or send response) commences and the time when it
         concludes.  If the completion timeout (see constructor) is exceeded,
         the connection is closed on the delinquent client -- it may be
         involved in a denial of service  attack.  This function may be
         called only by the main socket listening loop. */
    void push_active_channel(kd_tcp_channel *channel, kd_request_queue &queue);
      /* This function is typically called from within a
         `kd_source_thread' object, to relinquish an active transport
         channel which it can no longer use.  This may happen because an
         HTTP request is received which refers to a different source, or
         because a JPIP session is closed without closing the TCP channel.
         The function transfers the `channel' object and the contents of
         the `queue' object onto an internal list of active channels which
         are waiting to be assigned to connection threads.  They are
         assigned at the earliest convenience (possibly also within the
         present function).  Note that ownership of the `channel' object
         is transferred to the present object.
            If the supplied `channel' is already closed, the function
         deletes it and returns after emptying the `queue' object. */
  private: // Data structures
      struct kd_pending_active_channel {
          kd_tcp_channel *channel;
          kd_request_queue request_queue;
          kd_pending_active_channel *next;
        };
  private: // Helper functions
    void dispatch_pending_active_channels();
      /* This function tries to assign active channels which were supplied
         via `push_active_channel' to idle connection threads.  It should
         be called whenever a new thread becomes available, or at other times,
         so as to ensure that the pending channels do not go unserviced
         indefinitely.  Calls to this function MUST BE PROTECTED.
         Specifically, the caller must be sure to already have acquired the
         mutex lock. */
    void acquire_lock() { sysLockMutex(h_mutex); }
    void release_lock() { sysReleaseMutex(h_mutex); }
    void wait_event()   { sysWaitForEvent(h_event,INFINITE); }
    void signal_event() { sysSetEvent(h_event); }
  private: // Data
    friend class kd_connection_thread;
    kd_source_manager *source_manager;
    kd_delegator *delegator;
    bool restricted_access;
    int max_threads; // Max threads allowed
    int num_threads; // Num threads actually created
    int idle_threads; // Num threads waiting to be used
    int timeout_milliseconds; // Supplied to connection thread.
    kd_connection_thread *threads; // List of all available threads
    kd_pending_active_channel *pending_active_head;
    kd_pending_active_channel *pending_active_tail;
    HANDLE h_mutex; // For accessing above members
    HANDLE h_event; // Pulsed whenever a thread becomes free
  };

/*****************************************************************************/
/*                             kd_delegation_host                            */
/*****************************************************************************/

struct kd_delegation_host {
  public: // Member functions
    kd_delegation_host()
      {
        hostname = hostname_with_port = NULL;
        resolution_counter = 1; lookup_in_progress = false;
        prev = next = NULL; load_counter = load_share = 0;
      }
    ~kd_delegation_host()
      {
        if (hostname != NULL) delete[] hostname;
        if (hostname_with_port != NULL) delete[] hostname_with_port;
      }
  public: // Data
    char *hostname_with_port;
    char *hostname;
    kdu_uint16 port;
    SOCKADDR_IN address;
    int resolution_counter; // 0 if address already translated
    bool lookup_in_progress; // True if address resolution is in progress
    int load_share; // Relative number of clients we try to send to this host.
    int load_counter; // Counts down from `load_share'.
    kd_delegation_host *prev, *next;
  };

/*****************************************************************************/
/*                                kd_delegator                               */
/*****************************************************************************/

class kd_delegator {
  public: // Member functions
    kd_delegator()
      {
        head = tail = NULL;
        num_delegation_hosts = 0;
        h_mutex = sysCreateMutex();
      }
    ~kd_delegator()
      {
        while ((tail=head) != NULL)
          { head=tail->next; delete tail; }
        sysDestroyMutex(&h_mutex);
      }
    void add_host(const char *hostname);
    const char *delegate(const kd_request *request,
                         kd_tcp_channel *response_channel,
                         kd_message_block &par_block,
                         kd_message_block &body_block);
      /* If the request can be delegated to another server, the function
         returns the name of the delegated server, after forwarding the
         delegated server's HTTP response to the `response_channel', adding
         "Connection: close" to the header if necessary.  The `par_block'
         and `body_block' objects are provided to facilitate construction of
         request and response paragraphs.  If the delegated server encounters
         an error which is likely to apply across all possible servers (e.g.,
         the requested file might exist but have an invalid format), the
         function behaves in the same way as normal, passing the error along
         to the client via the `response_channel'.  Otherwise, the function
         returns NULL, writing nothing to the `response_channel', leaving the
         caller to handle the request locally. */
  private: // Member functions
    void acquire_lock() { sysLockMutex(h_mutex); }
    void release_lock() { sysReleaseMutex(h_mutex); }
  private: // Data
    HANDLE h_mutex;
    int num_delegation_hosts;
    kd_delegation_host *head, *tail; // The most likely candidate is always
                                     // at the head of the list.
  };


#endif // SERVER_LOCAL_H
