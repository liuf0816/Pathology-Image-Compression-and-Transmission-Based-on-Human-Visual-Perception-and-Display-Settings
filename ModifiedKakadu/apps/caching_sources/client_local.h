/*****************************************************************************/
// File: client_local.h [scope = APPS/CACHING_SOURCES]
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
  Private definitions used in the implementation of the "kdu_client" class.
******************************************************************************/

#ifndef CLIENT_LOCAL_H
#define CLIENT_LOCAL_H

#include "client_server_comms.h"
#include "kdu_client.h"
#include <windows.h>

// Defined here:
struct kd_window_request;
class kd_client_flow_control;
struct kd_client_model_manager;
class kd_client;

// Defined elsewhere:
class kd_tcp_channel;

// #define SIMULATE_REQUESTS // Uncomment this to simulate requests from
                 // a file named "simulated_requests.src" and log
                 // statistics in a file named "simulated_requests.log"

#ifdef SIMULATE_REQUESTS
  /***************************************************************************/
  struct kd_simulated_request {
      kdu_window window;
      time_t duration; // Next request comes this many ticks later unless
                       // window completes earlier.
      kd_simulated_request *next;
    };

  /***************************************************************************/
  class kd_request_simulator {
    public: // Member functions
      kd_request_simulator();
      ~kd_request_simulator();
      void allow_advance();
        /* Called when the most recent request is completely satisfied. */
      kdu_uint32 can_advance();
        /* If the current request can be advanced, the function returns 0.
           Otherwise, it returns the number of milliseconds left to wait before
           we will be allowed to advance, or INFINITE, if we must wait until
           `allow_advance' is explicitly called upon completion of the
           request. */
      bool advance(kdu_window &window);
        /* Advances the current request, setting the contents of `window' to
           reflect the new request.  The function returns false if there are
           no more requests left. */
      void log_received_data(int received_bytes, int extracted_bytes);
        /* This function is called whenever new data arrives from the server.
           `received_bytes' indicates the total number of bytes received,
           including all overhead, while `extracted_bytes' indicates the
           number of header or compressed data bytes extracted from the
           received bytes -- i.e., excluding the overhead. */
      void log_response_terminated(bool on_next_receive_data=false);
        /* Call this function when the terminal EOR message is received in
           response to a request.  It inserts a log record containing the
           negative of the total number of bytes received since the last
           call.  If `on_next_receive_data' is true, the effect of the
           function is delayed until after the next call to
           `log_receive_data'. */
    private: // Data
      kd_simulated_request *list, *current;
      time_t base_time;
      time_t advance_gate;
      FILE *log_file;
      int cumulative_received_bytes;
      int cumulative_extracted_bytes;
      bool response_termination_pending;
    };
#endif // SIMULATE_REQUESTS


/*****************************************************************************/
/*                             kd_window_request                             */
/*****************************************************************************/

struct kd_window_request {
  public: // Member functions
    void init()
      {
        window.init(); received_bytes = 0; byte_limit = 0;
        response_terminated = reply_received = window_completed = false;
        quality_limit_reached = byte_limit_reached = new_elements = false;
        next = NULL;
      }
  public: // Data
    kdu_window window;
    int received_bytes; // Chunk body bytes received in response to request
    int byte_limit; // If 0, there is no limit specified in the request
    bool new_elements; // True if this window is not a subset of previous one
    bool response_terminated; // True if final response message received
    bool window_completed; // True if all data for the window has been sent
    bool quality_limit_reached; // True if response terminated at quality limit
    bool byte_limit_reached; // True if response terminated at byte limit
    bool reply_received; // True if server has replied to the request
    kd_window_request *next; // Used to build linked lists
    kd_window_request *cleanup_next; // Used for robust cleanup of resources
  };

/*****************************************************************************/
/*                           kd_client_flow_control                          */
/*****************************************************************************/

class kd_client_flow_control {
  public: // Member functions
    kd_client_flow_control() { enable(false); }
    bool enable(bool set_to_enabled=true)
      /* The object must be enabled before `get_request_byte_limit' will
         return anything other than 0.  All other calls have no effect
         either unless the object is enabled.  You may disable an enabled
         object by setting the `set_to_enable' flag to false.  The function
         returns the previous enabled state. */
      {
        bool result = enabled; enabled = set_to_enabled;
        response_completion_time = reply_received_time = 0;
        requested_bytes = 0; byte_limit = 2000;
        return result;
      }
    void reply_received(int requested_bytes)
      {
        this->requested_bytes = requested_bytes;
        reply_received_time = clock();
      }
      /* Call this when you receive the reply text for a non-empty response.
         The response will follow.  The function allows the gap between the
         end of a previous response and the start of a new response to be
         timed.  The `request_limit' argument indicates the number of bytes
         to which the request was limited (this may have been overridden by
         the server). */
    void response_complete(int received_bytes, bool pause);
      /* Call this when you receive the completed response corresponding to
         the reply for which `reply_received' was called.  The `received_bytes'
         argument indicates the total number of bytes which were received.
         The `pause' argument should be true only if there are not currently
         any queued requests in the pipeline. */
    int get_request_byte_limit()
      {
        return (enabled)?byte_limit:0;
      }
      /* Returns a suggested byte limit for the next request.  An adaptive
         algorithm uses timing statistics derived from the above two 
         functions to come up with a reasonable limit.  The function
         returns 0, meaning that there should be no limit, if the object
         is not currently enabled.  Otherwise, it uses the following
         algorithm:
             Two quantities are derived from the reply and response
         completion events signalled by the above functions.  These are:
         1) Tgap = the time between the end of a non-paused response and
         the receipt of the next reply; and
         2) Tdat = the time between the receipt of the reply text and the
         end of the response data belonging to that reply.  The object
         adaptively adjusts its suggested request byte limit so that
                   Tgap / (Tdat+Tgap) \approx (Tdat+Tgap) / 10 seconds.
           * If Tgap / (Tdat+Tgap) > (Tdat+Tgap)/10, the request size is
         increased, unless the request size was already so large that the
         server pre-empted the last part of the request, reducing the
         response length substantially below the suggested byte limit.
         Evidently, the algorithm will not push Tdat+Tgap beyond 10 seconds.
           * If Tgap / (Tdat+Tgap) < (Tdat+Tgap)/10, the request size is
         decreased, unless Tdat+Tgap is already less than 1 second, in
         which case it will be left as is.  Also, the request byte limit
         will not be decreased below 2 kBytes. */
  private: // Data
    bool enabled;
    time_t response_completion_time; // 0 if paused after last response
    time_t reply_received_time; // 0 if no reply received yet
    int requested_bytes; // Set when reply is received.
    int byte_limit;
  };

/*****************************************************************************/
/*                          kd_client_model_manager                          */
/*****************************************************************************/

  // This class manages objects which are used to determine and signal
  // cache model information for a single code-stream.
struct kd_client_model_manager {
  public: // Member functions
    kd_client_model_manager()
      { codestream_id=-1; next = NULL; }
    ~kd_client_model_manager()
      { if (codestream.exists()) codestream.destroy(); }
  public: // Data
    kdu_long codestream_id;
    kdu_cache aux_cache;
    kdu_codestream codestream;
    kd_client_model_manager *next; // For linked list of code-streams
  };

/*****************************************************************************/
/*                                 kd_client                                 */
/*****************************************************************************/

  // This class manages all network communications.
class kd_client {
  public: // Member functions
    kd_client(kdu_client *owner);
      /* Creates the synchronization objects, but does not start the
         network management thread; this is done inside `start'. */
    ~kd_client();
      /* Asks the network thread to terminate, waits for it to do so, and then
         destroys all synchronization objects. */
    void start();
      /* Call this function to start the network management. */
    void network_manager();
      /* Called from the network management thread's entry-point function. */
    void network_closing();
      /* Called from within the network management thread, when it is about
         to terminate.  This gives the object an opportunity to clean up
         resources which should be released from the same thread in which
         they were allocated. */
    void request_closure();
      /* Called from another thread to request closure of the network
         management thread.  This function blocks the caller until closure
         is complete, at which point the object may be destroyed if
         desired. */
    void signal_status(const char *message)
      { // Combines status message change with application notification
        owner->acquire_lock();
        owner->status = message;
        if ((owner->notifier != NULL) && !thread_closure_requested)
          owner->notifier->notify();
        owner->release_lock();
      }
    kd_window_request *get_empty_window_request();
      /* Allocates a new window request structure, recycling from the free
         list if possible. */
    void return_window_request(kd_window_request *req);
      /* Return client requeset record to the recycling list.  There is no
         need to call this function if the object is being destroyed due to an
         error condition, since an internal list of all requests is maintained
         for cleanup purposes. */
    void window_posted() { SetEvent(wsa_event); }
      /* Called after `kdu_client::post_window' completes. */
  private: // Helper functions
    kdu_uint32 resolve_hostname(const char *hostname);
      /* If `hostname' identifies an IP address directly, the relevant 32-bit
         IP address is returned here.  Otherwise, the function attempts
         to resolve the host name to a valid IP address, which is subsequently
         returned.  If address resolution fails, the function generates an
         appropriate error message through `kdu_error', which is expected to
         cause an exception to be thrown and caught in the thread start up
         procedure. */
    void extract_window_from_query(kdu_window &window);
      /* Examines the `query' string, looking for window parameters, removing
         them from the string and updating the `window' object's members
         accordingly.  This is useful when constucting an initial request
         window from the query string supplied in a non-interactive URL. */
    void connect_request_channel();
      /* This function is called immediately before delivering a request
         if the request channel is found not to be connected currently, or
         if the immediate TCP connection to the server or an HTTP proxy
         may need to be closed and re-opened due to a change in address or
         port number (if `force_immediate_server_validation' is true).
         Connection establishment happens when the first connection attempt
         is made, but may happen subsequently, if an HTTP/1.0 proxy requires
         reconnection on each successive request, or if a new channel command
         results in a server response which identifies a different server.
            If a connection cannot be established, the function will terminate
         through `kdu_error', which will generally throw an exception, so
         you must be prepared to invoke the function from a context in which
         exceptions can safely be thrown.  In practice, the function is
         always executed from within `deliver_new_request'. */
    void connect_return_channel();
      /* Similar to `connect_request_channel', but this function is invoked
         from within `process_server_reply' if a reply message indicates that
         a new channel request was successful and the response data will
         appear on a separate return channel which must be connected. */
    void deliver_new_request();
      /* Called when a waiting request can be processed.  It is possible that
         once `acquire_lock' has been called, `request_waiting' turns out to
         be NULL.  In this event, the function should release the lock and
         do nothing. */
    bool process_server_reply(const char *text);
      /* Processes the reply text received from the server in response to a
         window request.  Each request must be followed by one reply, which
         may modify any or all of the request parameters, in accordance
         with the server's actual capabilities.  The reply, however, may
         arrive before or after the server begins (or even finishes) sending
         response data messages for the request.  The function returns false
         if the response message can be completely ignored (100-series
         messages). */
    void finish_http_body();
      /* This function is called only where the "http" transport is being used,
         when the body of an HTTP response is complete, even if the response
         contains only headers and no entity body.  The principle reason for
         this function is that "http" transport does not require each
         request/reply pair to be matched by an EOR message in a
         reply body.  Instead, termination of the response body is considered
         to be equivalent to the server sending an EOR message with the
         reason code "response pre-empted", unless it has already sent an
         EOR message. */
    int process_return_data(kd_message_block &block);
      /* Absorbs as much as possible of the return data contained in the
         supplied message block, returning when done.  In many cases, the
         function will return with `block' completely empty, but in some
         circumstances HTTP proxies might repackage original server chunks
         in such a way that the chunk boundaries fall part way through
         a server message.  In that case, the message will be left behind
         for later processing.
            Returns the total number of bytes which were actually extracted
         and written into the cache. */
    void process_completed_requests();
      /* Remove all completed requests from the head of the pending request
         list, leaving at least one request (pending or otherwise) on the
         list to service `kdu_client::get_window_in_progress' requests.  This
         function must be called from within a protected section (i.e., after
         acquiring the mutex with `owner->acquire_lock()'.
            If the last request has had its response terminated, but the
         window has not yet been completely serviced, or if the last
         request had a byte limit, which has been partially serviced
         already, the present function checks to see if a new window request
         from the user is pending; if not, a new request is automatically
         posted for the same window.  The reason for doing this when a
         previous request has been only partially serviced is to minimize
         the impact of communication latencies, when the client must take
         charge of flow control. */
    bool save_cache_contents();
      /* Saves the contents of the cache to the file whose name appears in
         the `cache_path' member.  Returns false if the cache file
         cannot be opened.  This function should not be called unless
         `cache_path' points to the name of a cache file. */
    bool read_cache_contents();
      /* Attempts to open the cache file whose name is stored in
         `cache_path' and read its contents into the cache.  Returns
         true if a file was found and read.  The file must have both
         the correct "target-id" string and the correct file name. */
    kd_client_model_manager *
      add_client_model_manager(kdu_long codestream_id);
      /* Called whenever a code-stream main header data-bin becomes complete
         in the cache, this function adds a new manager for the server's
         cache model for that code-stream.  In stateful sessions, it is not
         necessary to create a new client model manager for a code-stream
         whose main header was completed during the current session, since
         the server has a properly synchronized cache model for such
         code-streams.  Returns a pointer to the relevant model manager. */
    bool signal_model_corrections(kdu_window &window, kd_message_block &block);
      /* Called when generating a request message to be sent to the server.
         The purpose of this function is to identify elements from the
         client's cache which the server is not expected to know about and
         signal them using a `model' request field.  For stateless requests,
         the server cannot be expected to know anything about the client's
         cache.  For stateful sessions, the server cannot be expected to
         know about any information which the client received in a
         previous session, stored in a `.kjc' file.
            The function is best called from within a try/catch construct to
         catch any exceptions generated from ill-constructed code-stream
         headers.
            Returns true if any cache model information was written.  Returns
         false otherwise.  This information may be used to assist in
         determining whether or not the request/response pair is cacheable
         by intermediate HTTP proxies. */
    void write_cache_descriptor(int cs_idx, bool &cs_started,
                                const char *bin_class, kdu_long bin_id,
                                int available_bytes, bool is_complete,
                                kd_message_block &block);
      /* Utility function used by `signal_model_corrections' to write a
         cache descriptor for a single code-stream.  If `cs_started' is false,
         the code-stream qualifier (`cs_idx') is written first, followed by
         the descriptor itself.  In this case, `cs_started' is set to true
         so that the code-stream qualifier will not need to be written multiple
         times.  If `bin_id' is negative, no data-bin ID is actually written;
         this is appropriate if the `bin_class' string is "Hm". */
    int *get_scratch_ints(int len)
      {
        if (len > max_scratch_ints)
          {
            if (scratch_ints != NULL) delete[] scratch_ints;
            scratch_ints = new int[max_scratch_ints+=len];
          }
        return scratch_ints;
      }
       
#ifdef SIMULATE_REQUESTS
  private:
    kd_request_simulator request_simulator;
#endif // SIMULATE_REQUESTS

  // --------------------------------------------------------------------------
  public: // Inter-linking
    kdu_client *owner;
  // --------------------------------------------------------------------------
  public: // Host and resource name details
    char original_request[256]; // As passed in by `kdu_client::connect'
    char original_server[256]; // As passed in by `kdu_client::connect'
    char channel_transport[80]; // See `kdu_client::connect'

    char resource[256]; // Name of resource to be used in requests
    char query[256]; // Minimal query string to be used in requests
    bool is_stateless; // True if all communication is stateless

    char server[256]; // Name or IP addr of server to be used for next request
    kdu_uint16 request_port; // Port to be used with above server for requests
    kdu_uint16 return_port; // For transports with 2 connections
    char immediate_server[256]; // Same as `server' unless talking via proxy
    kdu_uint16 immediate_port; // Same as `request_port' unless going via proxy
    char target_id[256]; // target-id string for verifying cache re-usability
    char channel_id[256]; // channel-id string for sessions
    bool is_interactive; // If false, only one request will ever be generated
    bool need_cclose; // If `network_manager' returns normally with this flag
      // asserted and the `request_channel' still open, a special request
      // with the `cclose' request field should be sent to gracefully terminate
      // the channel's resources on the server.
  // --------------------------------------------------------------------------
  public: // Cache file management
    char cache_dir[200]; // Base directory for `cache_path'
    char cache_path[350]; // Path name for cache files (".kic" files)
    FILE *cache_file;
    int cache_store_len; // Size of the `cache_store_buf' array
    kdu_byte *cache_store_buf; // Temporary buffer for cache file transactions
    kd_client_model_manager *client_model_managers; // List of active managers
  // --------------------------------------------------------------------------
  public: // Status information available to "kdu_client"
    bool pending_name_resolution; // True during server name resolution only
    kd_window_request *request_head; // Head of list of outstanding requests
    kd_window_request *request_tail; // Tail of list of outstanding requests
    kd_window_request *request_free; // List of recycled request structures
    kd_window_request *request_waiting; // NULL unless request waiting to sent
    kd_window_request *request_cleanup; // For robust cleanup of request lists
    bool thread_closed; // True if the network thread has exited.
  // --------------------------------------------------------------------------
  private: // Inter-thread communication
    WSAEVENT wsa_event; // Used to notify the completion of network events.
    HANDLE h_thread;
    bool thread_closure_requested; // Used in conjunction with `wsa_event'
  // --------------------------------------------------------------------------
  private: // Connections
    kd_tcp_channel *request_channel; // For all transports
    kd_tcp_channel *return_channel; // For transports with two connections
    bool uses_two_channels; // If two connection required by transport
    bool force_immediate_server_validation; // If immediate server connection
            // for next request may have a different IP address or port number
    bool check_channel_alive; // Forces `deliver_new_request' to wait until
                              // at least one byte comes back from server
  // --------------------------------------------------------------------------
  private: // Temporary resources
    SOCKADDR_IN immediate_address; // Address of `immediate_server'
    bool immediate_address_resolved; // True if `address' is valid
    bool using_proxy; // True if `immediate_address' is that of a proxy server
    bool is_persistent; // If HTTP's TCP connection persists to next request
    kd_message_block query_block;
    kd_message_block send_block;
    kd_message_block recv_block;
    int metareq_buf_len; // Length of `metareq_buf' array
    char *metareq_buf; // Used to pre-parse "JPIP-metareq:" header bodies
    int max_scratch_ints;
    int *scratch_ints;
  // --------------------------------------------------------------------------
  private: // Transport loop state
    clock_t request_delivery_gate; // Min time to deliver waiting request
    kd_window_request *http_reply_ref; // Points to request to which current
                // http reply applies.  Set to NULL inside `finish_http_body'.
    kd_client_flow_control flow_control;
  private: // Message decoding state
    int last_msg_class_id;
    kdu_long last_msg_stream_id;
  };

#endif // CLIENT_LOCAL_H
