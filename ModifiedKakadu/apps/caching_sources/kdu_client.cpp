/*****************************************************************************/
// File: kdu_client.cpp [scope = APPS/CACHING_SOURCES]
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
  Implements a compressed data source, derived from "kdu_cache" which
interacts with a server using the protocol proposed by UNSW to the JPIP
(JPEG2000 Interactive Imaging Protocol) working group of ISO/IEC JTC1/SC29/WG1
******************************************************************************/

#include <assert.h>
#include "client_local.h"
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
     kdu_error _name("E(kdu_client.cpp)",_id);
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("W(kdu_client.cpp)",_id);
#  define KDU_TXT(_string) "<#>" // Special replacement pattern
#else // !KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("Error in Kakadu Client:\n");
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("Warning in Kakadu Client:\n");
#  define KDU_TXT(_string) _string
#endif // !KDU_CUSTOM_TEXT

#define KDU_ERROR_DEV(_name,_id) KDU_ERROR(_name,_id)
 // Use the above version for errors which are of interest only to developers
#define KDU_WARNING_DEV(_name,_id) KDU_WARNING(_name,_id)
 // Use the above version for warnings which are of interest only to developers


#ifdef SIMULATE_REQUESTS
  /* ======================================================================= */
  /*                          kd_request_simulator                           */
  /* ======================================================================= */

  /***************************************************************************/
  /*               kd_request_simulator::kd_request_simulator                */
  /***************************************************************************/

  kd_request_simulator::kd_request_simulator()
  {
    FILE *fp = fopen("simulated_requests.src","r");
    if (fp == NULL)
      { kdu_error e;
        e << "Unable to open \"simulated_requests.src\" file.  You may "
             "wish to recompile the client with the \"SIMULATE_REQUESTS\" "
             "macro disabled.";
      }
    list = current = NULL;
    char line[256];
    kd_simulated_request *req, *tail=NULL;
    while (fgets(line,255,fp) != NULL)
      {
        char *qp = line;
        while ((*qp==' ') || (*qp=='\r') || (*qp=='\n') || (*qp=='\t'))
          qp++;
        if (*qp == '\0')
          continue;
        req = new kd_simulated_request;
        req->duration = 0;
        do {
            if (*qp == '&')
              qp++;
            bool have_size = false;
            if ((qp[0] == 'R') && (qp[1] == '='))
              {
                int val1, val2;
                if ((sscanf(qp+2,"%d,%d",&val1,&val2) != 2) ||
                    (val1 < 0) || (val2 < 0))
                  { kdu_error e; e << "Malformed resolution (R) field in "
                    "\"simulated_requests.src\" line:\n\n" << line; }
                req->window.resolution.x = val1;
                req->window.resolution.y = val2;
                if (!have_size)
                  window.region.size = window.resolution;
              }
            else if ((qp[0] == 'O') && (qp[1] == '='))
              {
                int val1, val2;
                if ((sscanf(qp+2,"%d,%d",&val1,&val2) != 2) ||
                    (val1 < 0) || (val2 < 0))
                  { kdu_error e; e << "Malformed offset (O) field in "
                    "\"simulated_requests.src\" line:\n\n" << line; }
                req->window.region.pos.x = val1;
                req->window.region.pos.y = val2;
              }
            else if ((qp[0] == 'S') && (qp[1] == '='))
              {
                int val1, val2;
                if ((sscanf(qp+2,"%d,%d",&val1,&val2) != 2) ||
                    (val1 < 0) || (val2 < 0))
                  { kdu_error e; e << "Malformed size (S) field in "
                    "\"simulated_requests.src\" line:\n\n" << line; }
                req->window.region.size.x = val1;
                req->window.region.size.y = val2;
                have_size = true;
              }
            else if ((qp[0] == 'C') && (qp[1] == '='))
              {
                int val;
                req->window.num_components = 0;
                for (qp+=2; (*qp >= '0') && (*qp <= '9'); )
                  {
                    if ((sscanf(qp,"%d",&val) != 1) || (val < 0))
                      { kdu_error e; e << "Malformed components (C) field in "
                        "\"simulated_requests.src\" line:\n\n" << line; }
                    req->window.set_num_components(
                                req->window.num_components+1);
                    req->window.components[req->window.num_components-1] = val;
                    while ((*qp >= '0') && (*qp <= '9'))
                      qp++;
                    if (*qp == ',')
                      qp++;
                  }
              }
            else if ((qp[0] == 'T') && (qp[1] == '='))
              {
                float val;
                if ((sscanf(qp+2,"%f",&val) != 1) || (val <= 0.0F))
                  { kdu_error e; e << "Malformed time (T) field in "
                    "\"simulated_requests.src\" line:\n\n" << line; }
                req->duration = (time_t)(0.5F+CLOCKS_PER_SEC*val);
                if (req->duration == 0)
                  req->duration = 1;
              }
            else
              { kdu_error e; e << "Unrecognized field found in \"simulated_"
                "requests.src\" file; problem encountered at:\n\n" << qp; }
            while ((*qp != '\0') && (*qp != '&'))
              qp++;
          } while (*qp == '&');
        
        if (tail == NULL)
          tail = list = req;
        else
          tail = tail->next = req;
        tail->next = NULL;
      }

    fclose(fp);
    if (list == NULL)
      { kdu_error e; e << "The \"simulated_requests.src\" file is empty!"; }
    advance_gate = 0;

    log_file = fopen("simulated_requests.log","w");
    if (log_file == NULL)
      { kdu_error e;
        e << "Unable to open \"simulated_requests.log\" for writing!"; }
    cumulative_received_bytes = cumulative_extracted_bytes = 0;
    response_termination_pending = false;
  }

  /***************************************************************************/
  /*               kd_request_simulator::~kd_request_simulator               */
  /***************************************************************************/

  kd_request_simulator::~kd_request_simulator()
  {
    while  ((current=list) != NULL)
      {
        list = current->next;
        delete current;
      }
    if (log_file != NULL)
      fclose(log_file);
  }

  /***************************************************************************/
  /*                   kd_request_simulator::can_advance                     */
  /***************************************************************************/

  kdu_uint32
    kd_request_simulator::can_advance()
  {
    if (current == NULL)
      return 0; // Have not started yet; can always advance
    if (advance_gate == 0)
      return INFINITE; // Cannot advance until `allow_advance' is called
    time_t now = clock() - base_time;
    if (now >= advance_gate)
      return 0;
    return (kdu_uint32) ((1000*(advance_gate-now)) / CLOCKS_PER_SEC);
  }

  /***************************************************************************/
  /*                  kd_request_simulator::allow_advance                    */
  /***************************************************************************/

  void
    kd_request_simulator::allow_advance()
  {
    advance_gate = clock() - base_time;
    if (advance_gate == 0)
      advance_gate = 1;
  }

  /***************************************************************************/
  /*                     kd_request_simulator::advance                       */
  /***************************************************************************/

  bool
    kd_request_simulator::advance(kdu_window &window)
  {
    if ((current != NULL) && (current->next == NULL))
      return false;
    if (current == NULL)
      {
        current = list;
        base_time = clock();
      }
    else
      current = current->next;
    window.copy_from(current->window);
    if (current->duration == 0)
      advance_gate = 0;
    else
      {
        time_t now = clock() - base_time;
        if ((advance_gate == 0) || (advance_gate >= now))
          advance_gate = now;
        advance_gate += current->duration;
      }
    return true;
  }

  /***************************************************************************/
  /*                kd_request_simulator::log_received_data                  */
  /***************************************************************************/

  void
    kd_request_simulator::log_received_data(int received_bytes,
                                            int extracted_bytes)
  {
    cumulative_received_bytes += received_bytes;
    cumulative_extracted_bytes += extracted_bytes;
    time_t now = clock() - base_time;
    float fnow = ((float) now) / CLOCKS_PER_SEC;
    fprintf(log_file,"%f %d %d\n",fnow,received_bytes,extracted_bytes);
    fflush(log_file);
    if (response_termination_pending)
      log_response_terminated(false);
  }

  /***************************************************************************/
  /*             kd_request_simulator::log_response_terminated               */
  /***************************************************************************/

  void
    kd_request_simulator::log_response_terminated(bool delay)
  {
    if (delay)
      { response_termination_pending = true; return; }
    response_termination_pending = false;
    time_t now = clock() - base_time;
    float fnow = ((float) now) / CLOCKS_PER_SEC;
    fprintf(log_file,"%f %d %d\n",fnow,
            -cumulative_received_bytes,-cumulative_extracted_bytes);
    fflush(log_file);
    cumulative_received_bytes = cumulative_extracted_bytes = 0;
  }

#endif // SIMULATE_REQUESTS


/* ========================================================================= */
/*                             Internal Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                        compare_targets                             */
/*****************************************************************************/

static bool
  compare_targets(const char *str1, const char *str2)
  /* Returns true if and only if `str1' and `str2' are URL's which refer
     to the same target file. */
{
  const char *qp, *next_qp, *query, *tgt1=str1, *tgt2=str2;
  int len1=(int) strlen(tgt1), len2=(int) strlen(tgt2);
  const char *sub_tgt1=NULL, *sub_tgt2=NULL;
  int sub_len1=0, sub_len2=0;

  query = strchr(str1,'?');
  if (query != NULL)
    {
      len1 = (int)(query-str1);
      for (qp=query+1; *qp != '\0'; qp=next_qp)
        {
          next_qp = strchr(qp,'&');
          if (next_qp == NULL)
            next_qp = qp + strlen(qp);
          if (kd_parse_request_field(qp,JPIP_FIELD_TARGET))
            { tgt1 = qp; len1 = (int)(next_qp-qp); }
          else if (kd_parse_request_field(qp,JPIP_FIELD_SUB_TARGET))
            { sub_tgt1 = qp; sub_len1 = (int)(next_qp-qp); }
        }
    }
      
  query = strchr(str2,'?');
  if (query != NULL)
    {
      len2 = (int)(query-str2);
      for (qp=query+1; *qp != '\0'; qp=next_qp)
        {
          next_qp = strchr(qp,'&');
          if (next_qp == NULL)
            next_qp = qp + strlen(qp);
          if (kd_parse_request_field(qp,JPIP_FIELD_TARGET))
            { tgt2 = qp; len2 = (int)(next_qp-qp); }
          else if (kd_parse_request_field(qp,JPIP_FIELD_SUB_TARGET))
            { sub_tgt2 = qp; sub_len2 = (int)(next_qp-qp); }
        }
    }

  if ((len1 != len2) || (strncmp(tgt1,tgt2,len1) != 0))
    return false;
  if ((sub_tgt1 != NULL) && (sub_tgt2 != NULL))
    {
      if ((sub_len1 != sub_len2) || (strncmp(sub_tgt1,sub_tgt2,sub_len1) != 0))
        return false;
    }
  else if ((sub_tgt1 != NULL) || (sub_tgt2 != NULL))
    return false;

  return true;
}

/*****************************************************************************/
/* STATIC                       thread_start_proc                            */
/*****************************************************************************/

static unsigned int __stdcall
  thread_start_proc(void *param)
{
  kd_client *client = (kd_client *) param;
  try {
      client->network_manager();
    }
  catch (int exc)
    { // Catch terminal errors from `kdu_error' and elsewhere.
      exc = exc;
      client->need_cclose = false;
    }
  client->network_closing();
  return 0;
}

/* ========================================================================= */
/*                           kd_client_flow_control                          */
/* ========================================================================= */

/*****************************************************************************/
/*                  kd_client_flow_control::response_complete                */
/*****************************************************************************/

#define ONE_SECOND CLOCKS_PER_SEC
#define TEN_SECONDS (10*ONE_SECOND)

void
  kd_client_flow_control::response_complete(int received_bytes, bool pause)
{
  time_t now = clock();
  if ((reply_received_time != 0) &&
      ((received_bytes-requested_bytes) < (requested_bytes>>1)) &&
      (received_bytes > (requested_bytes>>1)))
    {
      int adjustment = 0; // 1 for up, 0 for none; -1 for down.
      time_t tdat = now - reply_received_time;
      if (tdat > TEN_SECONDS)
        adjustment = -1; // Adjust byte limit down
      else if (response_completion_time != 0)
        {
          time_t tgap = reply_received_time - response_completion_time;
          if ((tgap+tdat) < ONE_SECOND)
            adjustment = +1;
          else
            {
              float gap_ratio = ((float) tgap) / ((float)(tgap + tdat));
              float target_ratio = ((float)(tdat+tgap)) / TEN_SECONDS;
              if (gap_ratio > target_ratio)
                adjustment = +1; // Adjust byte limit up
              else
                adjustment = -1; // Adjust byte limit down
            }
        }
      if (adjustment > 0)
        {
          byte_limit += (byte_limit >> 2);
          if (byte_limit > (requested_bytes<<1))
            byte_limit = requested_bytes<<1; // Avoid exceeding server adjusted
                                             // limit by too much.
        }
      else if (adjustment < 0)
        {
          byte_limit -= (byte_limit >> 2);
          if (byte_limit < 2000)
            byte_limit = 2000;
        }
    }
  response_completion_time = (pause)?0:now;
  reply_received_time = 0;
}



/* ========================================================================= */
/*                                 kd_client                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                             kd_client::kd_client                          */
/*****************************************************************************/

kd_client::kd_client(kdu_client *owner)
{
  this->owner = owner;

  original_request[0] = original_server[0] = channel_transport[0] = '\0';
  resource[0] = query[0] = '\0';
  is_stateless = true;
  server[0] = immediate_server[0] = target_id[0] = channel_id[0] = '\0';
  request_port = return_port = immediate_port = 80;
  is_interactive = true;
  need_cclose = false;

  cache_path[0] = cache_dir[0] = '\0';
  cache_file = NULL;
  cache_store_len = 0;
  cache_store_buf = NULL;
  client_model_managers = NULL;

  pending_name_resolution = false;
  request_head = request_tail = request_free = request_waiting = NULL;
  request_cleanup = NULL;
  thread_closed = true;
  
  wsa_event = CreateEvent(NULL,FALSE,FALSE,NULL);
  h_thread = 0;
  thread_closure_requested = false;
  
  request_channel = NULL;
  return_channel = NULL;
  uses_two_channels = false;
  force_immediate_server_validation = false;
  check_channel_alive = false;

  immediate_address_resolved = false;
  using_proxy = false;
  is_persistent = true;

  request_delivery_gate = 0;
  http_reply_ref = NULL;

  metareq_buf_len = 0;
  metareq_buf = NULL;
  max_scratch_ints = 0;
  scratch_ints = NULL;

  last_msg_class_id = 0;
  last_msg_stream_id = 0;
}

/*****************************************************************************/
/*                            kd_client::~kd_client                          */
/*****************************************************************************/

kd_client::~kd_client()
{
  request_closure();
  if (h_thread != 0)
    CloseHandle(h_thread);

  if (owner->keep_transport_open && (request_channel != NULL))
    {
      request_channel->start_timer(0,false,NULL);
      owner->saved_request_channel = request_channel;
      owner->timeout_milliseconds = 0;
      request_channel = NULL;
    }
  if (request_channel != NULL)
    delete request_channel;
  if (return_channel != NULL)
    delete return_channel;

  WSACloseEvent(wsa_event);

  if (owner->saved_request_channel == NULL)
    WSACleanup(); // Otherwise, keep WSA services alive to preserve channel

  kd_window_request *req;
  while ((req=request_cleanup) != NULL)
    {
      request_cleanup = req->cleanup_next;
      delete req;
    }
  if (cache_file != NULL)
    fclose(cache_file);
  if (cache_store_buf != NULL)
    delete[] cache_store_buf;

  kd_client_model_manager *mgr;
  while ((mgr=client_model_managers) != NULL)
    {
      client_model_managers = mgr->next;
      delete mgr;
    }
  if (metareq_buf != NULL)
    delete[] metareq_buf;
  if (scratch_ints != NULL)
    delete[] scratch_ints;
}

/*****************************************************************************/
/*                              kd_client::start                             */
/*****************************************************************************/

void
  kd_client::start()
{
  assert(h_thread == 0);
  if (owner->saved_request_channel != NULL)
    {
      request_channel = owner->saved_request_channel;
      request_channel->change_event(wsa_event);
      request_channel->start_timer(0,false,&thread_closure_requested);
      owner->saved_request_channel = NULL;
    }
  else
    {
      WORD wsa_version = MAKEWORD(2,2);
      WSADATA wsa_data;
      WSAStartup(wsa_version,&wsa_data);
    }

  if (request_channel == NULL)
    request_channel = new kd_tcp_channel;
  thread_closed = false;
  unsigned int thread_id;
  h_thread = (HANDLE)
    _beginthreadex(NULL,0,thread_start_proc,this,0,&thread_id);
  SetThreadPriority(h_thread,THREAD_PRIORITY_ABOVE_NORMAL);
}

/*****************************************************************************/
/*                         kd_client::network_closing                        */
/*****************************************************************************/

void
  kd_client::network_closing()
{
  if (thread_closed)
    return;
  bool keep_transport_open = owner->keep_transport_open;
  int timeout_milliseconds = owner->timeout_milliseconds;
  if (!(owner->idle_state && thread_closure_requested))
    keep_transport_open = false;
  WSAResetEvent(wsa_event); // Just in case
  if (need_cclose && (request_channel != NULL) && request_channel->is_open())
    { // Send a final `cclose' message to the server to gracefully terminate
      // the channel's resources
      try { // We have no other protection at this point
          query_block.restart();
          query_block << JPIP_FIELD_CHANNEL_CLOSE "=" << channel_id
                      << "&" JPIP_FIELD_MAX_LENGTH "=0";
          int query_bytes = query_block.get_remaining_bytes();
          send_block.restart();
          bool using_post = false;
          if ((query_bytes+strlen(resource)) < 200)
            {
              send_block << "GET ";
              if (using_proxy)
                {
                  send_block << "http://" << server;
                  if (request_port != 80)
                    send_block << ":" << request_port;
                }
              send_block << "/" << resource << "?";
              send_block.write_raw(query_block.read_raw(query_bytes),
                                   query_bytes);
              send_block << " HTTP/1.1\r\n";
            }
          else
            { // Use Post request
              using_post = true;
              send_block << "POST ";
              if (using_proxy)
                {
                  send_block << "http://" << server;
                  if (request_port != 80)
                    send_block << ":" << request_port;
                }
              send_block << "/" << resource << " HTTP/1.1\r\n";
              send_block << "Content-type: "
                            "application/x-www-form-urlencoded\r\n";
              send_block << "Content-length: " << query_bytes << "\r\n";
            }
          send_block << "Host: " << server;
          if (request_port != 80)
            send_block << ":" << request_port;
          send_block << "\r\n";
          if (!keep_transport_open)
            send_block << "Connection: close\r\n";
          send_block << "Cache-Control: no-cache\r\n";
          send_block << "\r\n";
          if (using_post)
            { // Write the POST request's entity body
              send_block.write_raw(query_block.read_raw(query_bytes),
                                   query_bytes);
            }

          if (!keep_transport_open)
            { // Just send the terminal request, but don't wait for a reply,
              // since we are closing the TCP channel anyway.
              request_channel->write_block(false,send_block);
            }
          else
            { // Send the terminal request, but wait a while to see if there
              // is a reply, discarding the reply text and any body data.  If
              // we have to wait too long, though, we will kill the channel.
              int reply_code, reply_length;
              const char *text, *header;
              
              request_channel->start_timer(timeout_milliseconds,true,NULL);
              if (!request_channel->write_block(true,send_block))
                throw 0;
              if ((text = request_channel->read_paragraph(true)) == NULL)
                throw 0;
              if (((header = strchr(text,' ')) == NULL) ||
                  (sscanf(header+1,"%d",&reply_code) == 0) ||
                  (reply_code >= 400))
                throw 0;
              if ((header=kd_caseless_search(text,"\nConnection:")) != NULL)
                {
                  while (*header == ' ') header++;
                  if (strncmp(header,"close",5) == 0)
                    throw 0;
                }
              if ((header =
                   kd_caseless_search(text,"\nContent-length:")) != NULL)
                { // Discard body with fixed content length
                  while (*header == ' ') header++;
                  if (!sscanf(header,"%d",&reply_length))
                    throw 0;
                  while (reply_length > 0)
                    {
                      int xfer_bytes = (reply_length < 128)?reply_length:128;
                      if (request_channel->read_raw(true,xfer_bytes) == NULL)
                        throw 0;
                    }
                }
              else if (((header =
                         kd_caseless_search(text,"\nTransfer-encoding:"))
                         != NULL))
                {
                  while (*header == ' ') header++;
                  int chunk_len = 1;
                  if (kd_has_caseless_prefix(header,"chunked"))
                    do { // discard chunked body
                        if ((text=request_channel->read_line(true))==NULL)
                          throw 0;
                        if ((*text == '\0') || (*text == '\n'))
                          continue;
                        if ((sscanf(text,"%x",&chunk_len)==0) || (chunk_len<0))
                          throw 0;
                        if ((chunk_len > 0) &&
                            request_channel->read_raw(true,chunk_len) == NULL)
                          throw 0;
                      } while (chunk_len > 0);
                }
            }
        }
      catch (int)
        {
          keep_transport_open = false;
        }
    }
  if (request_channel != NULL)
    {
      if (!(keep_transport_open && request_channel->is_open()))
        {
          request_channel->close();
          delete request_channel;
          request_channel = NULL;
        }
      else if (request_channel->is_open())
        {
          request_channel->change_event(NULL);
          request_channel->start_timer(0,false,NULL);
        }
    }
  if (return_channel != NULL)
    {
      return_channel->close();
      delete return_channel;
      return_channel = NULL;
    }

  if ((cache_path[0] != '\0') && (cache_file == NULL) &&
      (owner->received_bytes > 0))
    { // If `cache_file' is not NULL, we had a crash before closing the file
      signal_status("Saving cache contents to disk.");
      try {
          if ((!save_cache_contents()) && (!thread_closure_requested))
            { KDU_ERROR(e,0); e <<
                KDU_TXT("Unable to write to cache file")
                << ", \"" << cache_path << "\".";
            }
        } catch (int) {};
    }

  owner->acquire_lock();
  owner->idle_state = true;
  thread_closed = true;
  owner->release_lock();
  signal_status("Network connection closed."); // Wakes up app if necessary
}

/*****************************************************************************/
/*                         kd_client::request_closure                        */
/*****************************************************************************/

void
  kd_client::request_closure()
{
  if (!thread_closed)
    {
      thread_closure_requested = true;
      WSASetEvent(wsa_event); // Try to wake up thread, asking it to die
      owner->acquire_lock();
      if (pending_name_resolution)
        TerminateThread(h_thread,0);
      owner->release_lock();
    }
  WaitForSingleObject(h_thread,INFINITE);
  network_closing(); // Does nothing if termination was graceful, calling
                     // `network_closing' from the network management thread.
}

/*****************************************************************************/
/*                    kd_client::get_empty_window_request                    */
/*****************************************************************************/

kd_window_request *
  kd_client::get_empty_window_request()
{
  kd_window_request *result = request_free;
  if (result == NULL)
    {
      result = new kd_window_request;
      result->cleanup_next = request_cleanup;
      request_cleanup = result;
    }
  else
    request_free = result->next;
  result->init();
  return result;
}

/*****************************************************************************/
/*                     kd_client::return_window_request                      */
/*****************************************************************************/

void
  kd_client::return_window_request(kd_window_request *req)
{
  req->next = request_free;
  request_free = req;
}

/*****************************************************************************/
/*                        kd_client::resolve_hostname                        */
/*****************************************************************************/

kdu_uint32
  kd_client::resolve_hostname(const char *hostname)
{
  kdu_uint32 address = inet_addr(hostname);
  if (address == INADDR_NONE)
    { // Try to convert the host name to an IP address
      signal_status("Resolving host name ...");
      owner->acquire_lock();
      pending_name_resolution = true;
      owner->release_lock();
      if (thread_closure_requested)
        throw 0; // Caught in thread start up procedure
      HOSTENT *hostent = gethostbyname(hostname);
        // Be careful not to do anything before this point which pre-supposes
        // that the thread will not be explicitly deleted.  The only way I
        // know of to terminate the above function while it is in progress
        // is to explicitly kill the thread.  This is what happens if the
        // `kdu_client::close' function is called while
        // `pending_name_resolution' is true.
      owner->acquire_lock();
      pending_name_resolution = false;
      owner->release_lock();
      if (hostent != NULL)
        {
          memcpy(&address,hostent->h_addr_list[0],4);
          signal_status("Host name resolved.");
        }
    }
  if (address == INADDR_NONE)
    { KDU_ERROR(e,1); e <<
        KDU_TXT("Unable to resolve host address")
        << ", \"" << hostname << "\".";
    }
  return address;
}

/*****************************************************************************/
/*                     kd_client::extract_window_from_query                  */
/*****************************************************************************/

void
  kd_client::extract_window_from_query(kdu_window &window)
{
  const char *scan;
  const char *qp = query;

  window.init();
  bool have_size = false;
  while (*qp != '\0')
    {
      const char *field_start = qp;
      if (kd_parse_request_field(qp,JPIP_FIELD_FULL_SIZE))
        {
          int val1, val2;
          if ((sscanf(qp,"%d,%d",&val1,&val2) != 2) ||
              (val1 <= 0) || (val2 <= 0))
            { KDU_ERROR(e,2); e <<
                KDU_TXT("Malformed")
                << "\"" JPIP_FIELD_FULL_SIZE "\" " <<
                KDU_TXT("field in query component of "
                "requested URL; query string is:\n\n") << query;
            }
          window.resolution.x = val1;
          window.resolution.y = val2;
          if (!have_size)
            window.region.size = window.resolution;
          qp = strchr(qp,','); assert(qp != NULL);
          for (qp++; *qp != '\0'; qp++)
            if ((*qp == '&') || (*qp == ','))
              break;
          window.round_direction = -1; // Default is round-down
          if (kd_has_caseless_prefix(qp,",round-up"))
            window.round_direction = 1;
          else if (kd_has_caseless_prefix(qp,",closest"))
            window.round_direction = 0;
          else if (kd_has_caseless_prefix(qp,",round-down"))
            window.round_direction = -1;
          else if ((*qp != '\0') && (*qp != '&'))
            { KDU_ERROR(e,3); e <<
                KDU_TXT("Malformed")
                << "\"" JPIP_FIELD_FULL_SIZE "\" " <<
                KDU_TXT("field in query component of "
                "requested URL; query string is:\n\n") << query;
            }
        }
      else if (kd_parse_request_field(qp,JPIP_FIELD_REGION_OFFSET))
        {
          int val1, val2;
          if ((sscanf(qp,"%d,%d",&val1,&val2) != 2) ||
              (val1 < 0) || (val2 < 0))
            { KDU_ERROR(e,4); e <<
                KDU_TXT("Malformed")
                << "\"" JPIP_FIELD_REGION_OFFSET "\" " <<
                KDU_TXT("field in query component of "
                "requested URL; query string is:\n\n") << query;
            }
          window.region.pos.x = val1;
          window.region.pos.y = val2;
        }
      else if (kd_parse_request_field(qp,JPIP_FIELD_REGION_SIZE))
        {
          int val1, val2;
          if ((sscanf(qp,"%d,%d",&val1,&val2) != 2) ||
              (val1 <= 0) || (val2 <= 0))
            { KDU_ERROR(e,5); e <<
                KDU_TXT("Malformed")
                << "\"" JPIP_FIELD_REGION_SIZE "\" " <<
                KDU_TXT("field in query component of "
                "requested URL; query string is:\n\n") << query;
            }
          window.region.size.x = val1;
          window.region.size.y = val2;
          have_size = true;
        }
      else if (kd_parse_request_field(qp,JPIP_FIELD_COMPONENTS))
        {
          char *end_cp;
          int from, to;
          window.components.init();
          for (scan=qp; (*scan != '&') && (*scan != '\0'); )
            {
              while (*scan == ',')
                scan++;
              from = to = strtol(scan,&end_cp,10);
              if (end_cp > scan)
                {
                  scan = end_cp;
                  if (*scan == '-')
                    {
                      scan++;
                      to = strtol(scan,&end_cp,10);
                      if (end_cp == scan)
                        to = INT_MAX;
                      scan = end_cp;
                    }
                }
              else
                scan--; // To force an error
              if (((*scan != ',') && (*scan != '&') && (*scan != '\0')) ||
                  (from < 0) || (from > to))
                { KDU_ERROR(e,6); e <<
                    KDU_TXT("Malformed")
                    << "\"" JPIP_FIELD_COMPONENTS "\" " <<
                    KDU_TXT("field in query component of requested URL; "
                    "query string is:\n\n") << query;
                }
              window.components.add(from,to);
            }
        }
      else if (kd_parse_request_field(qp,JPIP_FIELD_CODESTREAMS))
        {
          char *end_cp;
          kdu_sampled_range range;
          window.codestreams.init();
          for (scan=qp; (*scan != '&') && (*scan != '\0'); )
            {
              while (*scan == ',')
                scan++;
              range.step = 1;
              range.from = range.to = strtol(scan,&end_cp,10);
              if (end_cp > scan)
                {
                  scan = end_cp;
                  if (*scan == '-')
                    {
                      scan++;
                      range.to = strtol(scan,&end_cp,10);
                      if (end_cp == scan)
                        range.to = INT_MAX;
                      scan = end_cp;
                    }
                  if (*scan == ':')
                    {
                      scan++;
                      range.step = strtol(scan,&end_cp,10);
                      if (end_cp > scan)
                        scan = end_cp;
                      else
                        scan--; // To force an error
                    }
                }
              else
                scan--; // To force an error
              if (((*scan != ',') && (*scan != '&') && (*scan != '\0')) ||
                  (range.from < 0) || (range.from > range.to) ||
                  (range.step < 1))
                { KDU_ERROR(e,7); e <<
                    KDU_TXT("Malformed")
                    << "\"" JPIP_FIELD_COMPONENTS "\" " <<
                    KDU_TXT("field in query component of requested URL; "
                    "query string is:\n\n") << query;
                }
              window.codestreams.add(range);
            }
        }
      else if (kd_parse_request_field(qp,JPIP_FIELD_CONTEXTS))
        {
          for (scan=qp; (*scan != '&') && (*scan != '\0'); )
            {
              scan = window.parse_context(scan);
              if ((*scan != ',') && (*scan != '&') && (*scan != '\0'))
                { KDU_ERROR(e,8); e <<
                    KDU_TXT("Malformed")
                    << "\"" JPIP_FIELD_CONTEXTS "\" " <<
                    KDU_TXT("field in query component of requested URL; "
                    "query string is:\n\n") << query;
                }
              while (*scan == ',')
                scan++;
            }
        }
      else if (kd_parse_request_field(qp,JPIP_FIELD_LAYERS))
        {
          int val;
          if ((sscanf(qp,"%d",&val) != 1) || (val < 0))
            { KDU_ERROR(e,9); e <<
                KDU_TXT("Malformed")
                << "\"" JPIP_FIELD_LAYERS "\" " <<
                KDU_TXT("field in query component of "
                "requested URL; query string is:\n\n") << query;
            }
          window.max_layers = val;
        }
      else if (kd_parse_request_field(qp,JPIP_FIELD_META_REQUEST))
        {
          if (*qp == '\0')
            { KDU_ERROR(e,10); e <<
                KDU_TXT("Malformed")
                << "\"" JPIP_FIELD_META_REQUEST "\" " <<
                KDU_TXT("field in query component of requested URL.  At "
                "least one descriptor must appear in the body of the request "
                "field.  Query string is:\n\n") << query;
            }
          for (scan=qp; (*qp != '\0') && (*qp != '&'); qp++);
          if ((qp-scan) >= metareq_buf_len)
            {
              metareq_buf_len += (int)(qp-scan)+1;
              if (metareq_buf != NULL)
                delete[] metareq_buf;
              metareq_buf = new char[metareq_buf_len];
            }
          strncpy(metareq_buf,scan,qp-scan);
          metareq_buf[qp-scan] = '\0';
          hex_hex_decode(metareq_buf,metareq_buf,metareq_buf_len-1);
          const char *failure = window.parse_metareq(metareq_buf);
          if (failure != NULL)
            { KDU_ERROR(e,11); e <<
                KDU_TXT("Malformed")
                << "\"" JPIP_FIELD_META_REQUEST "\" " <<
                KDU_TXT("field in query component of requested URL.  "
                "Problem encountered at:\n\n\t") << failure <<
                KDU_TXT("\n\nComplete query string is:\n\n\t") << query;
            }
        }

      char *cp, *dp;
      for (cp=(char *) qp; *cp != '\0'; cp++)
        if (*cp == '&')
          { cp++; break; }
      if (qp != field_start)
        { // Remove the most recently parsed field.
          if ((*cp == '\0') &&
              (field_start > query) && (field_start[-1] == '&'))
            field_start--;
          for (dp=(char *) field_start; *cp != '\0'; cp++, dp++)
            *dp = *cp;
          *dp = '\0';
          qp = field_start;
        }
      else
        qp = cp;
    }
}

/*****************************************************************************/
/*                     kd_client::connect_request_channel                    */
/*****************************************************************************/

void
  kd_client::connect_request_channel()
{
  signal_status("Forming main request connection...");

  if (force_immediate_server_validation)
    {
      assert(!using_proxy);
      if (strcmp(server,immediate_server) != 0)
        immediate_address_resolved = false;
      force_immediate_server_validation = false;
    }
  if (!using_proxy)
    {
      strcpy(immediate_server,server);
      immediate_port = request_port;
    }
  if (!immediate_address_resolved)
    {
      memset(&immediate_address,0,sizeof(immediate_address));
      immediate_address.sin_addr.S_un.S_addr =
        resolve_hostname(immediate_server);
      immediate_address.sin_family = AF_INET;
      immediate_address_resolved = true;
    }
  immediate_address.sin_port = htons(immediate_port);

  if (request_channel->is_open())
    {
      SOCKADDR_IN existing_address;
      request_channel->get_peer_address(existing_address);
      if ((existing_address.sin_addr.S_un.S_addr ==
           immediate_address.sin_addr.S_un.S_addr) &&
          (existing_address.sin_port == immediate_address.sin_port))
        return;
      request_channel->close();
    }

  if (!request_channel->open(immediate_address,wsa_event,
                             &thread_closure_requested))
    { KDU_ERROR(e,12); e <<
        KDU_TXT("Unable to complete main request connection to server.");
    }
  is_persistent = true; // Assume new channel persistent until proven otherwise
  check_channel_alive = true; // Force validation of channel in first request

  signal_status("Connected");
}

/*****************************************************************************/
/*                     kd_client::connect_return_channel                     */
/*****************************************************************************/

void
  kd_client::connect_return_channel()
{
  assert(uses_two_channels);
  if (return_channel == NULL)
    return_channel = new kd_tcp_channel;
  signal_status("Forming auxiliary return data connection...");

  assert(!using_proxy);
  if (strcmp(server,immediate_server) != 0)
    {
      immediate_address_resolved = false;
      strcpy(immediate_server,server);
    }
  immediate_port = request_port;

  if (!immediate_address_resolved)
    {
      memset(&immediate_address,0,sizeof(immediate_address));
      immediate_address.sin_addr.S_un.S_addr =
        resolve_hostname(immediate_server);
      immediate_address.sin_port = htons(immediate_port);
      immediate_address.sin_family = AF_INET;
      immediate_address_resolved = true;
    }

  SOCKADDR_IN return_address = immediate_address;
  return_address.sin_port = htons(return_port);
  assert(!return_channel->is_open());
  if (!return_channel->open(return_address,wsa_event,
                            &thread_closure_requested))
    { KDU_ERROR(e,13); e <<
        KDU_TXT("Unable to complete auxiliary return connection to server.");
    }

  send_block.restart();
  send_block << channel_id << "\r\n\r\n";
  return_channel->write_block(true,send_block);
  send_block.restart();

  signal_status("Connected");
}

/*****************************************************************************/
/*                        kd_client::deliver_new_request                     */
/*****************************************************************************/

void
  kd_client::deliver_new_request()
{
  owner->acquire_lock();
  if (request_waiting == NULL)
    { owner->release_lock(); return; }
  kd_window_request *request = request_waiting;
  request_waiting = NULL;
  owner->release_lock();

  request->byte_limit = flow_control.get_request_byte_limit();
  
  query_block.restart();
  bool first_field = true;
  if (query[0] != '\0')
    {
      query_block << query;
      first_field = false;
    }
  if ((request->window.resolution.x > 0) &&
      (request->window.resolution.y > 0))
    {
      query_block << ((first_field)?"":"&") << JPIP_FIELD_FULL_SIZE "="
                  << request->window.resolution.x
                  << "," << request->window.resolution.y;
      if (request->window.round_direction > 0)
        query_block << ",round-up";
      else if (request->window.round_direction == 0)
        query_block << ",closest";
      first_field = false;
      query_block << "&" JPIP_FIELD_REGION_OFFSET "="
                  << request->window.region.pos.x
                  << "," << request->window.region.pos.y;
      query_block << "&" JPIP_FIELD_REGION_SIZE "="
                  << request->window.region.size.x
                  << "," << request->window.region.size.y;
    }

  if (!request->window.components.is_empty())
    {
      query_block << ((first_field)?"":"&") << JPIP_FIELD_COMPONENTS "=";
      first_field = false;
      int c=0;
      kdu_sampled_range *rg;
      for (; (rg=request->window.components.access_range(c)) != NULL; c++)
        {
          if (c > 0)
            query_block << ",";
          query_block << rg->from;
          if (rg->to == INT_MAX)
            query_block << "-";
          else if (rg->to > rg->from)
            query_block << "-" << rg->to;
        }
    }

  if (request->window.codestreams.is_empty() &&
      request->window.contexts.is_empty())
    request->window.codestreams.add(0); // Explicitly include the default

  if (!request->window.codestreams.is_empty())
    {
      int c;
      query_block << ((first_field)?"":"&") << JPIP_FIELD_CODESTREAMS "=";
      first_field = false;
      kdu_sampled_range *rg;
      for (c=0; (rg=request->window.codestreams.access_range(c)) != NULL; c++)
        {
          if (rg->context_type == KDU_JPIP_CONTEXT_TRANSLATED)
            continue;
          if (c > 0)
            query_block << ",";
          query_block << rg->from;
          if (rg->to > (INT_MAX-rg->step))
            query_block << "-";
          else if (rg->to > rg->from)
            query_block << "-" << rg->to;
          if (rg->step != 1)
            query_block << ":" << rg->step;
        }
    }

  if (!request->window.contexts.is_empty())
    {
      query_block << ((first_field)?"":"&") << JPIP_FIELD_CONTEXTS "=";
      first_field = false;
      int c=0;
      kdu_sampled_range *rg;
      for (; (rg=request->window.contexts.access_range(c)) != NULL; c++)
        {
          if (c > 0)
            query_block << ",";
          if ((rg->context_type != KDU_JPIP_CONTEXT_JPXL) &&
              (rg->context_type != KDU_JPIP_CONTEXT_MJ2T))
            continue;
          if (rg->context_type == KDU_JPIP_CONTEXT_JPXL)
            query_block << "jpxl";
          else if (rg->context_type == KDU_JPIP_CONTEXT_MJ2T)
            query_block << "mj2t";
          else
            assert(0);
          query_block << "<" << rg->from;
          if (rg->to > rg->from)
            query_block << "-" << rg->to;
          if ((rg->step > 1) && (rg->to > rg->from))
            query_block << ":" << rg->step;
          if ((rg->context_type == KDU_JPIP_CONTEXT_MJ2T) &&
              (rg->remapping_ids[1] == 0))
            query_block << "+now";
          query_block << ">";
          if (rg->context_type == KDU_JPIP_CONTEXT_JPXL)
            {
              if ((rg->remapping_ids[0] >= 0) && (rg->remapping_ids[1] >= 0))
                query_block << "[s" << rg->remapping_ids[0]
                            << "i" << rg->remapping_ids[1] << "]";
            }
          else if (rg->context_type == KDU_JPIP_CONTEXT_MJ2T)
            {
              if (rg->remapping_ids[0] == 0)
                query_block << "[track]";
              else if (rg->remapping_ids[0] == 1)
                query_block << "[movie]";
            }
        }
    }

  if (request->window.max_layers > 0)
    {
      query_block << ((first_field)?"":"&") << JPIP_FIELD_LAYERS "="
                  << request->window.max_layers;
      first_field = false;
    }
  if (request->window.metareq != NULL)
    {
      kdu_metareq *req;
      query_block << ((first_field)?"":"&") << JPIP_FIELD_META_REQUEST "=";
      first_field = false;
      for (req=request->window.metareq; req != NULL; req=req->next)
        {
          query_block << "[";
          if (req->box_type == 0)
            query_block << "*";
          else
            for (int s=24; s >= 0; s-=8)
              {
                char ch = (char)(req->box_type >> s);
                if (ch == ' ')
                  query_block << '_';
                else if (!(isalnum(ch) || (ch == '-')))
                  { // Hex-hex encode it
                    query_block << "%";
                    for (int d=4; d >= 0; d-=4)
                      {
                        char dig = (ch >> d) & 0x0F;
                        dig += ((dig <= 9)?'0':('a'-10));
                        query_block << dig;
                      }
                  }
                else
                  query_block << ch;
              }
          if (req->recurse)
            query_block << ":r";
          else if (req->byte_limit < INT_MAX)
            query_block << ":" << req->byte_limit;
          if ((req->qualifier != KDU_MRQ_DEFAULT) &&
              (req->qualifier & KDU_MRQ_ANY))
            {
              query_block << "/";
              if (req->qualifier & KDU_MRQ_WINDOW)
                query_block << "w";
              if (req->qualifier & KDU_MRQ_STREAM)
                query_block << "s";
              if (req->qualifier & KDU_MRQ_GLOBAL)
                query_block << "g";
              if (req->qualifier & KDU_MRQ_ALL)
                query_block << "a";
            }
          if (req->priority)
            query_block << "!";
          query_block << "]";
          if (req->root_bin_id != 0)
            {
              query_block << "R";
              kdu_long id = req->root_bin_id;
              if (id < 0) id = 0;
              do {
                  int digit = (int)(id % 10);
                  id = id / 10;
                  query_block << (char)(digit+'0');
                } while (id > 0);
            }
          if (req->max_depth < INT_MAX)
            query_block << "D" << req->max_depth;
          if (req->next != NULL)
            query_block << ",";
        }
      if (request->window.metadata_only)
        query_block << "!!";
    }
  if (request->byte_limit > 0)
    {
      query_block << ((first_field)?"":"&") << JPIP_FIELD_MAX_LENGTH "="
                  << request->byte_limit;
      first_field = false;
    }
  if (target_id[0] == '\0')
    { // Ask for target-id
      query_block << ((first_field)?"":"&") << JPIP_FIELD_TARGET_ID "=0";
      first_field = false;
    }
  if (channel_id[0] != '\0')
    {
      query_block << ((first_field)?"":"&") << JPIP_FIELD_CHANNEL_ID "="
                  << channel_id;
      first_field = false;
    }
  else
    { // Need to specify target-id & type and ask for new channel if required
      query_block << ((first_field)?"":"&") << JPIP_FIELD_TYPE "="
                  << "jpp-stream";
      first_field = false;
      if ((target_id[0] != '\0') && (strcmp(target_id,"0") != 0))
        query_block << "&" JPIP_FIELD_TARGET_ID "=" << target_id;
      if (is_stateless && (channel_transport[0] != '\0'))
        {
          query_block << "&" JPIP_FIELD_CHANNEL_NEW "=" << channel_transport;
          channel_transport[0] = '\0'; // In case server does'nt assign channel
        }
    }

  if (is_stateless || request->new_elements)
    signal_model_corrections(request->window,query_block);

  // query_block << "&model=Hm"; // Just for testing.

  int query_bytes = query_block.get_remaining_bytes();
  send_block.restart();
  bool using_post = false;
  if ((query_bytes+strlen(resource)) < 200)
    {
      send_block << "GET ";
      if (using_proxy)
        {
          send_block << "http://" << server;
          if (request_port != 80)
            send_block << ":" << request_port;
        }
      send_block << "/" << resource << "?";
      send_block.write_raw(query_block.read_raw(query_bytes),query_bytes);
      send_block << " HTTP/1.1\r\n";
    }
  else
    { // Use Post request
      using_post = true;
      send_block << "POST ";
      if (using_proxy)
        {
          send_block << "http://" << server;
          if (request_port != 80)
            send_block << ":" << request_port;
        }
      send_block << "/" << resource << " HTTP/1.1\r\n";
      send_block << "Content-type: application/x-www-form-urlencoded\r\n";
      send_block << "Content-length: " << query_bytes << "\r\n";
    }
  send_block << "Host: " << server;
  if (request_port != 80)
    send_block << ":" << request_port;
  send_block << "\r\n";
  if (!is_interactive)
    send_block << "Connection: close\r\n";
  if (channel_transport[0] != '\0')
    send_block << "Cache-Control: no-cache\r\n";
  send_block << "\r\n";
  if (using_post)
    { // Write the POST request's entity body
      send_block.write_raw(query_block.read_raw(query_bytes),query_bytes);
    }

  // Now try to send the block of data.  We allow ourselves the luxury of
  // re-opening a connection which has failed exactly once, hence the loop
  // below.
  bool succeeded=false, attempted_channel_open = false;
  while (!(succeeded || attempted_channel_open))
    {
      try {
          if (force_immediate_server_validation ||
              !request_channel->is_open())
            {
              attempted_channel_open = true;
              connect_request_channel();
            }
          send_block.backup();
          request_channel->write_block(true,send_block);
          if (check_channel_alive)
            {
              request_channel->test_readable(true);
              check_channel_alive = false;
            }
          succeeded = true;
        }
      catch (int exc)
        { // Catch exceptions in channel opening or channel writing
          if (!thread_closure_requested)
            {
              assert(!request_channel->is_open());
              if (!attempted_channel_open)
                continue;
            }
          throw exc;
        }
    }
  
  owner->acquire_lock();
  if (request_tail == NULL)
    request_head = request_tail = request;
  else
    request_tail = request_tail->next = request;

  process_completed_requests();
  signal_status("Interactive transfer ...");
  owner->release_lock();
}

/*****************************************************************************/
/*                       kd_client::process_server_reply                     */
/*****************************************************************************/

bool
  kd_client::process_server_reply(const char *reply)
{
  const char *header, *cp = strchr(reply,' ');
  int code;
  float version;
  if (!(kd_has_caseless_prefix(reply,"HTTP/") &&
        sscanf(reply+5,"%f",&version)))
    { KDU_ERROR(e,14); e <<
        KDU_TXT("Server reply to client window request does not "
        "appear to contain an HTTP version number as the first token.  "
        "Complete server response is:\n\n") << reply;
    }
  is_persistent = is_interactive; // Otherwise request used "Connection: close"
  if (version < 1.1)
    is_persistent = false;
  else if ((header = kd_caseless_search(reply,"\nConnection:")) != NULL)
    {
      while (*header == ' ') header++;
      if (kd_has_caseless_prefix(header,"close"))
        is_persistent = false;
    }
  if ((cp == NULL) || (sscanf(cp+1,"%d",&code) == 0))
    { KDU_ERROR(e,15); e <<
        KDU_TXT("Server reply to client window request "
        "does not appear to contain a status code as the second token.  "
        "Complete server response is:\n\n") << reply;
    }
  if (code >= 400)
    { KDU_ERROR(e,16); e <<
        KDU_TXT("Server could not process client window request.  "
        "Complete server response is:\n\n") << reply;
    }
  if ((code >= 100) && (code < 200))
    return false; // Ignore 100-series responses

  owner->acquire_lock();
  kd_window_request *scan;
  for (scan=request_head; scan != NULL; scan=scan->next)
    if (!scan->reply_received)
      break;
  if (scan == NULL)
    {
      owner->release_lock();
      KDU_ERROR(e,17); e <<
        KDU_TXT("Server reply received on connection channel does "
        "not match any outstanding request!  Suspect server is not fluent in "
        "the JPIP dialect.");
    }

  // Process window-specific JPIP response headers
  int val1, val2;
  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_MAX_LENGTH)) != NULL)
    {
      if ((sscanf(header,"%d",&val1) != 1) || (val1 < 0))
        {
          owner->release_lock();
          KDU_ERROR(e,18); e <<
            KDU_TXT("Incorrectly formatted")
            << " \"JPIP-" JPIP_FIELD_MAX_LENGTH ":\" " <<
            KDU_TXT("header in server's reply to window request.  "
            "Expected a non-negative byte limit parameter.  Complete server "
            "reply paragraph was:\n\n")
            << reply;
        }
      scan->byte_limit = val1;
    }
  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_FULL_SIZE)) != NULL)
    {
      if ((sscanf(header,"%d,%d",&val1,&val2) != 2) ||
          (val1 <= 0) || (val2 <= 0))
        {
          owner->release_lock();
          KDU_ERROR(e,19); e <<
            KDU_TXT("Incorrectly formatted")
            << " \"JPIP-" JPIP_FIELD_FULL_SIZE ":\" " <<
            KDU_TXT("header in server's reply to window request.  "
            "Expected positive horizontal and vertical dimensions, separated "
            "only by a comma.  Complete server reply paragraph was:\n\n")
            << reply;
        }
      scan->window.resolution.x = val1;
      scan->window.resolution.y = val2;
    }
  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_REGION_OFFSET)) != NULL)
    {
      if ((sscanf(header,"%d,%d",&val1,&val2) != 2) ||
          (val1 < 0) || (val2 < 0))
        {
          owner->release_lock();
          KDU_ERROR(e,20); e <<
            KDU_TXT("Incorrectly formatted")
            << " \"JPIP-" JPIP_FIELD_REGION_OFFSET ":\" " <<
            KDU_TXT("header in server's reply to window request.  "
            "Expected non-negative horizontal and vertical offsets from the "
            "upper left hand corner of the requested image resolution.  "
            "Complete server reply paragraph was:\n\n")
            << reply;
        }
      scan->window.region.pos.x = val1;
      scan->window.region.pos.y = val2;
    }
  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_REGION_SIZE)) != NULL)
    {
      if ((sscanf(header,"%d,%d",&val1,&val2) != 2) ||
          (val1 < 0) || (val2 < 0))
        { 
          owner->release_lock();
          KDU_ERROR(e,21); e <<
            KDU_TXT("Incorrectly formatted")
            << " \"JPIP-" JPIP_FIELD_REGION_SIZE ":\" " <<
            KDU_TXT("header in server's reply to window request.  "
            "Expected non-negative horizontal and vertical dimensions for the "
            "region of interest within the requested image resolution.  "
            "Complete server reply paragraph was:\n\n")
            << reply;
        }
      scan->window.region.size.x = val1;
      scan->window.region.size.y = val2;
    }
  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_COMPONENTS)) != NULL)
    {
      char *end_cp;
      int from, to;
      scan->window.components.init();
      while ((*header != '\0') && (*header != '\n') && (*header != ' '))
        {
          while (*header == ',')
            header++;
          from = to = strtol(header,&end_cp,10);
          if (end_cp > header)
            {
              header = end_cp;
              if (*header == '-')
                {
                  header++;
                  to = strtol(header,&end_cp,10);
                  if (end_cp == header)
                    to = INT_MAX;
                  header = end_cp;
                }
            }
          else
            header-=3; // Force an error
          if (((*header != ',') && (*header != ' ') && (*header != '\0') &&
               (*header != '\n')) || (from < 0) || (from > to))
            { owner->release_lock();
              KDU_ERROR(e,22); e <<
                KDU_TXT("Incorrectly formatted")
                << " \"JPIP-" JPIP_FIELD_COMPONENTS ":\" " <<
                KDU_TXT("header in server's reply to window request.  "
                "Complete server reply paragraph was:\n\n")
                << reply;
            }
          scan->window.components.add(from,to);
        }
    }
  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_CODESTREAMS)) != NULL)
    {
      char *end_cp;
      kdu_sampled_range range;
      scan->window.codestreams.init();
      while ((*header != '\0') && (*header != '\n') && (*header != ' '))
        {
          if (*header == ',')
            header++;

          // Check for a translation identifier
          range.context_type = 0;
          if (*header == '<')
            {
              range.context_type = KDU_JPIP_CONTEXT_TRANSLATED;
              header++;
              range.remapping_ids[0] = strtol(header,&end_cp,10);
              if ((end_cp == header) || (range.remapping_ids[0] < 0) ||
                  (*end_cp != ':'))
                { owner->release_lock();
                  KDU_ERROR(e,23); e <<
                    KDU_TXT("Illegal translation identifier in")
                    << " \"JPIP-" JPIP_FIELD_CODESTREAMS ":\" " <<
                    KDU_TXT("header in server's reply "
                    "to window request.  Complete server reply paragraph "
                    "was:\n\n") << reply;
                }
              header = end_cp+1;
              range.remapping_ids[1] = strtol(header,&end_cp,10);
              if ((end_cp == header) || (range.remapping_ids[1] < 0) ||
                  (*end_cp != '>'))
                { owner->release_lock();
                  KDU_ERROR(e,24); e <<
                    KDU_TXT("Illegal translation identifier in")
                    << " \"JPIP-" JPIP_FIELD_CODESTREAMS ":\" " <<
                    KDU_TXT("header in server's reply "
                    "to window request.  Complete server reply paragraph "
                    "was:\n\n") << reply;
                }
              header = end_cp+1;
            }

          // Parse the rest of the list element into a sampled range
          range.step = 1;
          range.from = range.to = strtol(header,&end_cp,10);
          if (end_cp > header)
            {
              header = end_cp;
              if (*header == '-')
                {
                  header++;
                  range.to = strtol(header,&end_cp,10);
                  if (end_cp == header)
                    range.to = INT_MAX;
                  header = end_cp;
                }
              if (*header == ':')
                {
                  range.step = strtol(header+1,&end_cp,10);
                  if (end_cp > (header+1))
                    header = end_cp;
                }
            }
          else
            header-=3; // Force an error
          if (((*header != ',') && (*header != ' ') && (*header != '\0') &&
               (*header != '\n')) || (range.from < 0) ||
               (range.from > range.to) || (range.step < 1))
            { owner->release_lock();
              KDU_ERROR(e,25); e <<
                KDU_TXT("Illegal value or range in")
                << " \"JPIP-" JPIP_FIELD_CODESTREAMS ":\" " <<
                KDU_TXT("header in server's reply to window request.  "
                "Complete server reply paragraph was:\n\n")
                << reply;
            }
          scan->window.codestreams.add(range);
        }
    }

  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_CONTEXTS)) != NULL)
    {
      scan->window.contexts.init();
      while ((*header != '\0') && (*header != '\n'))
        {
          while ((*header == ';') || (*header == ' '))
            header++;
          cp = scan->window.parse_context(header);
          if ((*cp != ';') && (*cp != '\n') && (*cp != ' ') && (*cp != '\0'))
            {
              owner->release_lock();
              KDU_ERROR(e,26); e <<
                KDU_TXT("Incorrectly formatted")
                << " \"JPIP-" JPIP_FIELD_CONTEXTS ":\" " <<
                KDU_TXT("header in server's reply to window request.  "
                "Complete server reply paragraph was:\n\n")
                << reply;
            }
          header = cp;
        }
    }

  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_LAYERS)) != NULL)
    {
      if ((sscanf(header,"%d",&val1) != 1) || (val1 < 0))
        { 
          owner->release_lock();
          KDU_ERROR(e,27); e <<
            KDU_TXT("Incorrectly formatted")
            << " \"JPIP-" JPIP_FIELD_LAYERS ":\" " <<
            KDU_TXT("header in server's reply to window request.  "
            "Expected non-negative maximum number of quality layers.  "
            "Complete server reply paragraph was:\n\n")
            << reply;
        }
      scan->window.max_layers = val1;
    }

  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_META_REQUEST)) != NULL)
    {
      scan->window.init_metareq();
      for (cp=header; (*cp != '\0') && (*cp != ' ') && (*cp != '\n'); cp++);
      if ((cp-header) >= metareq_buf_len)
        {
          metareq_buf_len += (int)(cp-header)+1;
          if (metareq_buf != NULL)
            delete[] metareq_buf;
          metareq_buf = new char[metareq_buf_len];
        }
      strncpy(metareq_buf,header,cp-header);
      metareq_buf[cp-header] = '\0';
      hex_hex_decode(metareq_buf,metareq_buf,metareq_buf_len-1);
      cp = scan->window.parse_metareq(metareq_buf);
      if (cp != NULL)
        {
          owner->release_lock();
          KDU_ERROR(e,28); e <<
            KDU_TXT("Incorrectly formatted")
            << " \"JPIP-" JPIP_FIELD_META_REQUEST ":\" " <<
            KDU_TXT("header in server's reply to window request.  Error "
            "encountered at:\n\n\t" << cp << "\n\nComplete server reply "
            "paragraph was:\n\n") << reply;
        }
    }

  scan->reply_received = true;

  // Check for availability of a new target-id
  bool need_to_open_local_cache = false;
  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_TARGET_ID)) != NULL)
    {
      int length = (int) strcspn(header," \n");
      if (length < 256)
        {
          char new_id[256];
          strncpy(new_id,header,length);
          new_id[length] = '\0';
          if (new_id[0] == '0')
            { // Check for numerical 0, converting to a single 0.
              for (cp=new_id; *cp != '\0'; cp++)
                if (*cp != '0')
                  break;
              if (*cp == '\0')
                new_id[1] = '\0';
            }
          if (target_id[0] == '\0')
            {
              strcpy(target_id,new_id);
              if ((cache_dir[0] != '\0') && (cache_path[0] == '\0') &&
                  (target_id[0] != '\0') && (strcmp(target_id,"0") != 0))
                need_to_open_local_cache = true;
            }
          else if (strcmp(target_id,new_id) != 0)
            { // Target ID has changed while in the middle of browsing
              owner->release_lock();
              KDU_ERROR(e,29); e <<
                KDU_TXT("Server appears to have issued a new unique target "
                "identifier, while we were in the middle of browsing the "
                "image.  Most likely, the image has been modified on the "
                "server and you should re-open it.");
            }
        }
    }
  else if (target_id[0] == '\0')
    { // Must have specified `tid=0' in request
      owner->release_lock();
      KDU_ERROR(e,30); e <<
        KDU_TXT("Server has responded with a successful status code, but has "
        "not included a")
        << " \"JPIP-" JPIP_FIELD_TARGET_ID ":\" " <<
        KDU_TXT("response header, even though we requested the "
        "target-id with a")
        << "\"" JPIP_FIELD_TARGET_ID "=0\" " <<
        KDU_TXT("request field.  Complete server reply "
        "paragraph was:\n\n") << reply;
    }

  // Check for establishment of a new channel
  if ((header = kd_parse_jpip_header(reply,JPIP_FIELD_CHANNEL_NEW)) != NULL)
    {
      if ((channel_id[0] != '\0') || !is_stateless)
        {
          owner->release_lock();
          KDU_ERROR(e,31); e <<
            KDU_TXT("Server appears to have issued a new channel ID where "
            "none was requested!!");
        }
      int length = (int) strcspn(header," \n");
      length = (length > 254)?254:length;
      char channel_params[256];
      channel_params[0] = ',';
      strncpy(channel_params+1,header,length);
      channel_params[length+1] = '\0';
      if ((header = kd_caseless_search(channel_params,",cid=")) != NULL)
        {
          length = (int) strcspn(header,",");
          length = (length > 255)?255:length;
          strncpy(channel_id,header,length);
          channel_id[length] = '\0';
        }
      if (channel_id[0] == '\0')
        { owner->release_lock();
          KDU_ERROR(e,32); e <<
            KDU_TXT("Server has failed to include a new channel-id in the "
            "set of channel parameters returned via the")
            << " \"JPIP-" JPIP_FIELD_CHANNEL_NEW ":\" " <<
            KDU_TXT("header in server's reply.  "
            "Expected a list of non-negative image component indices.  "
            "Complete server reply paragraph was:\n\n") << reply;
        }
      if ((header = kd_caseless_search(channel_params,",transport=")) != NULL)
        {
          length = (int) strcspn(header,",");
          length = (length > 79)?79:length;
          strncpy(channel_transport,header,length);
          channel_transport[length] = '\0';
          uses_two_channels =
            kd_has_caseless_prefix(channel_transport,"http-tcp");
          if (uses_two_channels)
            {
              return_port = request_port;
              using_proxy = false;
              flow_control.enable(false); // Disable client-based flow control
              if ((kd_caseless_search(reply,"\nContent-length:") != NULL) ||
                  (kd_caseless_search(reply,"\nTransfer-encoding:") != NULL))
                { owner->release_lock();
                  KDU_ERROR(e,33); e <<
                    KDU_TXT("Server has allocated an \"http-tcp\" transported "
                    "channel, but appears to be trying to send an entity "
                    "body in the same response.  This is illegal.  The "
                    "response data in this case MUST be sent on the new "
                    "TCP connection to be formed as part of the "
                    "\"http-tcp\" transport.  "
                    "Complete server reply paragraph was:\n\n")
                    << reply;
                }
            }
        }
      if ((header = kd_caseless_search(channel_params,",host=")) != NULL)
        {
          length = (int) strcspn(header,",");
          length = (length > 255)?255:length;
          strncpy(server,header,length);
          server[length] = '\0';
        }
      if ((header = kd_caseless_search(channel_params,",port=")) != NULL)
        {
          if (sscanf(header,"%d",&val1) != 0)
            request_port = return_port = (kdu_uint16) val1;
        }
      if ((header = kd_caseless_search(channel_params,",auxport=")) != NULL)
        {
          if (sscanf(header,"%d",&val1) != 0)
            return_port = (kdu_uint16) val1;
        }
      if ((header = kd_caseless_search(channel_params,",path=")) != NULL)
        {
          length = (int) strcspn(header,",");
          length = (length > 255)?255:length;
          strncpy(resource,header,length);
          resource[length] = '\0';
        }
      if (!using_proxy)
        force_immediate_server_validation = true;
      query[0] = '\0'; // Delete all target details from original query string
      is_stateless = false; // We now have a channel and hence a session
      need_cclose = true; // Remember to be a nice citizen and gracefully close
                          // the channel when we are done.
    }

  // See if we need to create model managers for code-streams covered by the
  // server's response.  This will be true if the communication is ongoing and
  // stateless.
  if (is_stateless)
    {
      kdu_sampled_range cs_range;
      int cs_range_num, cs_idx;
      for (cs_range_num=0;
           !(cs_range =
             scan->window.codestreams.get_range(cs_range_num)).is_empty();
           cs_range_num++)
        {
          if (cs_range.from < 0)
            continue; // Cannot provide model info for unknown codestreams.
          for (cs_idx=cs_range.from; cs_idx <= cs_range.to;
               cs_idx+=cs_range.step)
             {
               bool main_header_complete;
               owner->get_databin_length(KDU_MAIN_HEADER_DATABIN,cs_idx,0,
                                         &main_header_complete);
               if (main_header_complete)
                 add_client_model_manager(cs_idx);
             }
        }
    }

  // Now for some final processing.
  if (!uses_two_channels)
    {
      assert(http_reply_ref == NULL);
      http_reply_ref = scan;
    }

  process_completed_requests();
  if ((request_head == request_tail) && (request_head != NULL) &&
      request_head->reply_received && request_head->response_terminated &&
      (request_waiting == NULL))
    {
      owner->idle_state = true;
      signal_status("Connection idle.");
    }
  owner->release_lock();

  if (need_to_open_local_cache)
    {
      strcpy(cache_path,cache_dir);
      strcat(cache_path,"/");
      strcat(cache_path,target_id);
      strcat(cache_path,".kjc");
      read_cache_contents();
    }

  if (uses_two_channels &&
      ((return_channel == NULL) || !return_channel->is_open()))
    connect_return_channel();
  return true;
}

/*****************************************************************************/
/*                    kd_client::process_completed_requests                  */
/*****************************************************************************/

void
  kd_client::process_completed_requests()
{
  kd_window_request *scan;
  while (((scan = request_head) != request_tail) &&
         scan->reply_received && scan->response_terminated)
    {
      if (scan == http_reply_ref)
        http_reply_ref = NULL;
      request_head = scan->next;
      return_window_request(scan);
    }

  if ((scan != NULL) && (scan == request_tail) && scan->reply_received &&
      (!scan->window_completed) && (request_waiting == NULL) &&
      (scan->byte_limit_reached || (scan->byte_limit > 0)))
    {
      if (is_stateless || force_immediate_server_validation || !is_persistent)
        { // Avoid automatic request pipelining
          if (!scan->response_terminated)
            return;
        }
      else
        { // Pipeline new request after active request reaches 1/3 way point
          if ((!scan->response_terminated) &&
              ((3*scan->received_bytes) < scan->byte_limit))
            return;
        }
      request_waiting = get_empty_window_request();
      request_waiting->window.copy_from(scan->window);
    }
}

/*****************************************************************************/
/*                        kd_client::finish_http_body                        */
/*****************************************************************************/

void
  kd_client::finish_http_body()
{
  if (recv_block.get_remaining_bytes() != 0)
    { KDU_ERROR(e,34); e <<
        KDU_TXT("HTTP response body terminated before sufficient "
        "compressed data was received to correctly parse all server "
        "messages!");
    }
  if (http_reply_ref != NULL)
    {
      http_reply_ref->response_terminated = true;
      owner->acquire_lock();
      process_completed_requests();
      http_reply_ref = NULL;
      owner->release_lock();
    }
  if (!is_persistent)
    request_channel->close();
}

/*****************************************************************************/
/*                         kd_client::network_manager                        */
/*****************************************************************************/

void
  kd_client::network_manager()
{
  using_proxy = (strcmp(immediate_server,server) != 0 ) ||
                (immediate_port != request_port);
  immediate_address_resolved = false;
  uses_two_channels = false;
  is_stateless = true; // Always start in the stateless mode
  if (is_interactive)
    flow_control.enable(); // May disable this later, if using http-tcp channel
  else
    channel_transport[0] = '\0'; // No sessions/channels unless interactive
  if (strcmp(channel_transport,"http-tcp") == 0)
    {
      using_proxy = false; // No point in using proxy for HTTP/TCP
      strcpy(channel_transport,"http-tcp,http"); // Allow server to choose
    }
  else if ((channel_transport[0] != '\0') &&
           (strcmp(channel_transport,"http") != 0))
    { KDU_ERROR(e,35); e <<
        KDU_TXT("Unrecognized channel transport type")
        << ", \"" << channel_transport << "\n";
    }

  // Place initial window request on the queue
  kd_window_request *req = get_empty_window_request();
  if (!is_interactive)
    extract_window_from_query(req->window);
  else
    req->window.init();
  req->new_elements = true;
  owner->acquire_lock();
  if (request_waiting == NULL)
    request_waiting = req;
  else
    {
      assert(is_interactive);
      return_window_request(req);
    }
  owner->release_lock();

  this->check_channel_alive = true; // Instructs `deliver_new_request' to wait
                                    // until at least one byte is returned

  // Now run the main loop.
  kdu_byte ack_buf[8];
  int i, chunk_length = 0;
  bool chunked_transfer=false;
  bool in_http_body = false; // Not relevant for the "http-tcp" transport
  int total_http_body_bytes = 0; // Not relevant for the "http-tcp" transport
  int extracted_bytes, received_bytes = 0;
  recv_block.restart(); // Use to accumulate response body data
  while (!thread_closure_requested)
    {
      kdu_byte *raw = NULL;
      DWORD wait_milliseconds = 5000; // Occasionally network events don't fire
                                      // so limit the maximum wait to 5 seconds
#ifdef SIMULATE_REQUESTS
      wait_milliseconds = simulated_requests.can_advance();
      if (wait_milliseconds == 0)
        {
          owner->acquire_lock();
          if (request_waiting == NULL)
            request_waiting = get_empty_window_request();
          owner->release_lock();
          if (!request_simulator.advance(request_waiting->window))
            {
              request_simulator.log_response_terminated();
              return; // All requests done.  Terminate network connection now.
            }
          request_waiting->new_elements = true;
          wait_milliseconds = INFINITE;
        }
#endif // SIMULATE_REQUESTS
      if ((request_waiting != NULL) &&
          ((is_persistent && !is_stateless) ||
           (((request_tail==NULL) || request_tail->response_terminated) &&
            !in_http_body)))
        { // Pipeline requests only if connection is persistent and involves
          // a connected session.
          clock_t current_time = clock();
          if ((current_time >= request_delivery_gate) ||
              ((request_tail != NULL) && request_tail->reply_received))
            { // Send request and wait > 1 seconds before considering another
              // unless we have already received replies to all outstanding
              // requests.
              deliver_new_request();
              request_delivery_gate = current_time + CLOCKS_PER_SEC;
            }
          else
            { // Wait for event to be set or request delivery gate
              wait_milliseconds = 1 +
                (1000 * (kdu_uint32)(request_delivery_gate - current_time)) /
                CLOCKS_PER_SEC;
              if (wait_milliseconds < 100)
                wait_milliseconds = 100; // No harm in extra 100 milliseconds
            }
        }

      const char *text = NULL;
      if ((!in_http_body) && (request_channel != NULL) &&
          request_channel->is_open() &&
          ((text = request_channel->read_paragraph(false)) != NULL))
        { // Server has answered the most recent pending request
          received_bytes += (int) strlen(text);
          if ((*text == '\0') || (*text == '\n'))
            continue;

          if (!process_server_reply(text))
            continue; // Ignore response

          if (!uses_two_channels)
            {
              const char *header;
              assert(chunk_length == 0);
              bool have_jpp_stream = false;
              if ((header =
                   kd_caseless_search(text,"\nContent-type:")) != NULL)
                {
                  while (*header == ' ') header++;
                  if (kd_has_caseless_prefix(header,"image/jpp-stream"))
                    {
                      header += strlen("image/jpp-stream");
                      if ((*header == ' ') || (*header == '\n') ||
                          (*header == ';')) // Ignore any parameter values
                        have_jpp_stream = true;
                    }
                  if (!have_jpp_stream)
                    { KDU_ERROR(e,36); e <<
                        KDU_TXT("Server response has an unacceptable "
                        "content type.  Complete server response is:\n\n")
                        << text;
                    }
                }
              if (((header =
                    kd_caseless_search(text,"\nContent-length:"))
                    != NULL) && have_jpp_stream)
                {
                  while (*header == ' ') header++;
                  if ((!sscanf(header,"%d",&chunk_length)) || (chunk_length<0))
                    { KDU_ERROR(e,37); e <<
                        KDU_TXT("Malformed \"Content-length\" header "
                        "in HTTP response message.  Complete server response "
                        "is:\n\n") << text;
                    }
                  chunked_transfer = false;
                }
              else if (((header =
                         kd_caseless_search(text,"\nTransfer-encoding:"))
                         != NULL) && have_jpp_stream)
                {
                  while (*header == ' ') header++;
                  if (kd_has_caseless_prefix(header,"chunked"))
                    chunked_transfer = true;
                }
              if ((chunk_length > 0) || chunked_transfer)
                {
                  in_http_body = true;
                  total_http_body_bytes = chunk_length;
                  flow_control.reply_received(http_reply_ref->byte_limit);
                }
              else
                { // This reply has no response data and hence no body
                  finish_http_body();
                  if (!is_interactive)
                    {
                      signal_status("Non-interactive service complete.");
                      return;
                    }
                }
            }
          else if (!is_persistent)
            request_channel->close(); // So server can test non-persistence
                                      // for both http and http-tcp transports
        }
      else if (in_http_body && (chunk_length == 0) &&
               ((text = request_channel->read_line(false)) != NULL))
        {
          received_bytes += (int) strlen(text);
          if ((*text == '\0') || (*text == '\n'))
            continue;
          assert(chunked_transfer);
          if ((sscanf(text,"%x",&chunk_length) == 0) || (chunk_length < 0))
            { KDU_ERROR(e,38);  e <<
                KDU_TXT("Expected non-negative hex-encoded chunk length on "
                "line:\n\n") << text;
            }
          if (chunk_length == 0)
            { // end of chunked body
              chunked_transfer = in_http_body = false;
              finish_http_body();
              bool paused = (request_tail == NULL) ||
                request_tail->response_terminated;
              if ((is_stateless || !is_persistent) &&
                  (request_waiting != NULL))
                paused = false;
              flow_control.response_complete(total_http_body_bytes,paused);
              if (!is_interactive)
                {
                  signal_status("Non-interactive service complete.");
                  return;
                }
            }
          total_http_body_bytes += chunk_length;
        }
      else if (in_http_body && (chunk_length > 0) &&
               request_channel->read_block(false,chunk_length,recv_block))
        {
          received_bytes += chunk_length;
          if (chunk_length == 586)
            chunk_length = chunk_length;
          extracted_bytes = process_return_data(recv_block);
#ifdef SIMULATE_REQUESTS
          request_simulator.log_received_data(received_bytes,extracted_bytes);
#endif // SIMULATE_REQUESTS
          owner->received_bytes += received_bytes; received_bytes = 0;
          chunk_length = 0;
          if (!chunked_transfer)
            {
              in_http_body = false;
              finish_http_body();
              bool paused = (request_tail == NULL) ||
                request_tail->response_terminated;
              if ((is_stateless || !is_persistent) &&
                  (request_waiting != NULL))
                paused = false;
              flow_control.response_complete(total_http_body_bytes,paused);
              if (!is_interactive)
                {
                  signal_status("Non-interactive service complete.");
                  return;
                }
            }
        }
      else if ((chunk_length == 0) && uses_two_channels &&
               (return_channel != NULL) &&
               ((raw = return_channel->read_raw(false,8)) != NULL))
        {
          chunk_length = (int) raw[0];
          chunk_length = (chunk_length << 8) + (int) raw[1];
          if (chunk_length < 8)
            { KDU_ERROR(e,39); e <<
                KDU_TXT("Illegal chunk length found in server return data "
                "sent on the auxiliary TCP channel.  Chunk lengths "
                "must include the length of the 8-byte chunk preamble, which "
                "contains the chunk length value itself.  This means that "
                "the length may not be less than 8.  Got a "
                "value of ") << chunk_length << ".";
            }
          for (i=0; i < 8; i++)
            ack_buf[i] = raw[i];
          raw = NULL;
        }
      else if ((chunk_length != 0) && uses_two_channels &&
               (return_channel != NULL) &&
               ((raw=return_channel->read_raw(false,chunk_length-8)) != NULL))
        { // Return 8-byte acknowledgement identifier and process chunk body
          assert(chunk_length >= 8);
          received_bytes += chunk_length;
          recv_block.write_raw(raw,chunk_length-8);
          return_channel->write_raw(true,ack_buf,8);
          extracted_bytes = process_return_data(recv_block);
#ifdef SIMULATE_REQUESTS
          request_simulator.log_received_data(received_bytes,extracted_bytes);
#endif // SIMULATE_REQUESTS
          owner->received_bytes += received_bytes; received_bytes = 0;
          raw = NULL;
          chunk_length = 0;
        }
      else if (uses_two_channels && !return_channel->is_open())
        {
          signal_status("Server disconnected.");
          request_channel->close(); // Abnormal condition
          return;
        }
      else if (is_persistent && !request_channel->is_open())
        {
          signal_status("Server disconnected.");
          return; // Channel terminated without "Connection: close" header
        }
      else if ((!thread_closure_requested) &&
               (request_channel->is_open() || (request_waiting == NULL)))
        { // Wait for event to be set.
          DWORD wait_result =
            WSAWaitForMultipleEvents(1,&wsa_event,FALSE,
                                     wait_milliseconds,FALSE);
          if ((wait_result != WSA_WAIT_EVENT_0) &&
              (wait_result != WSA_WAIT_TIMEOUT))
            return;
        }
    }
}

/*****************************************************************************/
/*                       kd_client::process_return_data                      */
/*****************************************************************************/

int
  kd_client::process_return_data(kd_message_block &block)
{
  owner->acquire_lock();
  kd_window_request *scan = request_head;
  bool image_done = false;
  kdu_byte *data_start = block.peek_block();
  kdu_byte *data = data_start;
  int data_bytes = block.get_remaining_bytes();
  int extracted_bytes = 0;
  bool channel_closed_by_server = false;
  while (data_bytes > 0)
    {
      // Get the request to which the next message belongs
      for (; scan != NULL; scan=scan->next)
        if (!scan->response_terminated)
          break;
      if (scan == NULL)
        {
          owner->release_lock();
          KDU_ERROR(e,40); e <<
            KDU_TXT("Received response data from server without any "
            "pending requests.  Server is probably not fluent in this JPIP "
            "dialect.");
        }

      // Process the next message, updating `scan->received_bytes'.
      kdu_byte byte = *(data++); data_bytes--;
      int class_id=0, eor_reason_code=-1;
      int range_from=0, range_length=0, aux_val=0;
      kdu_long bin_id = 0;
      kdu_long stream_id = 0;
      bool is_final = ((byte & 0x10) != 0);
      if (byte == 0)
        { // EOR message;
          if (data_bytes == 0)
            { // Can't process any further right now.
              owner->release_lock();
              return extracted_bytes;
            }
          eor_reason_code = *(data++); data_bytes--;
        }
      else
        { // Extract class-id, bin-id, stream-id and range-offset
          switch ((byte & 0x7F) >> 5) {
            case 0:
              {
                owner->release_lock();
                KDU_ERROR(e,41); e <<
                  KDU_TXT("Illegal message header encountered "
                  "in response message sent by server.");
              }
            case 1:
              class_id = last_msg_class_id; stream_id = last_msg_stream_id;
              break;
            case 2:
              class_id = -1; stream_id = last_msg_stream_id;
              break;
            case 3:
              class_id = -1; stream_id = -1;
              break;
            }
          bin_id = (kdu_long)(byte & 0x0F);
          while (byte & 0x80)
            {
              if (data_bytes == 0)
                { // Can't process any further right now.
                  owner->release_lock();
                  return extracted_bytes;
                }
              byte = *(data++); data_bytes--;
              bin_id = (bin_id << 7) | (kdu_long)(byte & 0x7F);
            }

          if (class_id < 0)
            { // Read class code
              class_id = 0;
              do {
                  if (data_bytes == 0)
                    { // Can't process any further right now.
                      owner->release_lock();
                      return extracted_bytes;
                    }
                  byte = *(data++); data_bytes--;
                  class_id = (class_id << 7) | (int)(byte & 0x7F);
                } while (byte & 0x80);
            }

          if (stream_id < 0)
            { // Read codestream ID
              stream_id = 0;
              do {
                  if (data_bytes == 0)
                    { // Can't process any further right now.
                      owner->release_lock();
                      return extracted_bytes;
                    }
                  byte = *(data++); data_bytes--;
                  stream_id = (stream_id << 7) | (int)(byte & 0x7F);
                } while (byte & 0x80);
            }

          do {
              if (data_bytes == 0)
                { // Can't process any further right now.
                  owner->release_lock();
                  return extracted_bytes;
                }
              byte = *(data++); data_bytes--;
              range_from = (range_from << 7) | (int)(byte & 0x7F);
            } while (byte & 0x80);
        }

      // Read range length
      do {
          if (data_bytes == 0)
            { // Can't process any further right now.
              owner->release_lock();
              return extracted_bytes;
            }
          byte = *(data++); data_bytes--;
          range_length = (range_length << 7) | (int)(byte & 0x7F);
        } while (byte & 0x80);

      if (class_id & 1)
        { // Discard auxiliary VBAS for extended class code
          do {
              if (data_bytes == 0)
                { // Can't process any further right now.
                  owner->release_lock();
                  return extracted_bytes;
                }
              byte = *(data++); data_bytes--;
              aux_val = (aux_val << 7) | (int)(byte & 0x7F);
            } while (byte & 0x80);
        }

      if ((range_from < 0) || (range_length < 0) ||
          (bin_id < 0) || (stream_id < 0) ||
          (((class_id>>1) == KDU_MAIN_HEADER_DATABIN) && (bin_id != 0)))
        { owner->release_lock();
          KDU_ERROR(e,42); e <<
            KDU_TXT("Received a JPIP stream message containing an "
            "illegal header or one which contains a ridiculously large "
            "parameter.");
        }

      if (data_bytes < range_length)
        { // Can't process any further right now.
          owner->release_lock();
          return extracted_bytes;
        }

      // At this point, we have a complete message.
      scan->received_bytes += range_length;
      if (eor_reason_code >= 0)
        { // End of response message
          data += range_length;
          data_bytes -= range_length; // Skip over any message body for now
          scan->response_terminated = true;
          if (eor_reason_code == JPIP_EOR_IMAGE_DONE)
            {
              image_done = true;
              scan->window_completed = true;
            }
          else if (eor_reason_code == JPIP_EOR_WINDOW_DONE)
            scan->window_completed = true;
          else if (eor_reason_code == JPIP_EOR_BYTE_LIMIT_REACHED)
            scan->byte_limit_reached = true;
          else if (eor_reason_code == JPIP_EOR_QUALITY_LIMIT_REACHED)
            scan->quality_limit_reached = true;
          else if (eor_reason_code == JPIP_EOR_SESSION_LIMIT_REACHED)
            {
              need_cclose = false;
              channel_closed_by_server = true;
            }
#ifdef SIMULATE_REQUESTS
          if (scan->window_completed ||
              ((scan->next != NULL) && scan->next->new_elements) ||
              ((scan->next == NULL) &&
               (request_waiting != NULL) && request_waiting->new_elements))
            request_simulator.log_response_terminated(true);
#endif // SIMULATE_REQUESTS
        }
      else
        { // Regular data-bin class
          last_msg_class_id = class_id;
          last_msg_stream_id = stream_id;
          int cls = class_id >> 1;
          owner->add_to_databin(cls,stream_id,bin_id,data,range_from,
                                range_length,is_final);
          data += range_length;
          data_bytes -= range_length;
          extracted_bytes += range_length;
        }
      block.read_raw((int)(data-data_start)); // Advance over completed bytes.
      data_start = data;
    }

  process_completed_requests();
  if ((request_head == request_tail) && (request_head != NULL) &&
      request_head->reply_received && request_head->response_terminated &&
      (request_waiting == NULL))
    {
      owner->idle_state = true;
      if (image_done)
        signal_status("Image Completely Downloaded");
      else if (!channel_closed_by_server)
        signal_status("Connection idle.");
      else
        {
          signal_status("Channel Closed by Server (Session Limit Reached)");
          is_persistent = true;
          request_channel->close(); // Force termination of network manager
        }
#ifdef SIMULATE_REQUESTS
      request_simulator.allow_advance();
#endif
    }
  else if (owner->notifier != NULL)
    { // Notify application that cache state has changed.
      owner->notifier->notify();
    }

  owner->release_lock();
  return extracted_bytes;
}

/*****************************************************************************/
/*                    kd_client::write_cache_descriptor                      */
/*****************************************************************************/

void
  kd_client::write_cache_descriptor(int cs_idx, bool &cs_started,
                                    const char *bin_class, kdu_long bin_id,
                                    int available_bytes,
                                    bool is_complete,
                                    kd_message_block &block)
{
  assert((cs_idx >= 0) && (available_bytes >= 0));
  if (!cs_started)
    {
      cs_started = true;
      block << "[" << cs_idx << "],";
    }

  char buf[20];
  char *start = buf+20;
  kdu_long tmp;
  *(--start) = '\0';
  if (bin_id >= 0)
    {
      while (start > buf)
        {
          tmp = bin_id / 10;
          *(--start) = (char)('0' + (int)(bin_id - tmp*10));
          bin_id = tmp;
          if (bin_id == 0)
            break;
        }
      assert(bin_id == 0);
    }
  block << bin_class << start;
  if (!is_complete)
    block << ":" << available_bytes;
  block << ",";
}

/*****************************************************************************/
/*                   kd_client::signal_model_corrections                     */
/*****************************************************************************/

bool
  kd_client::signal_model_corrections(kdu_window &ref_window,
                                      kd_message_block &block)
{
  int start_chars = block.get_remaining_bytes(); // To backtrack if required.
  block << "&model=";
  int test_chars = block.get_remaining_bytes();

  // Scan through all relevant code-streams and code-stream contexts
  bool cs_started, is_complete;
  int available_bytes;
  kdu_long bin_id;
  int cs_idx, cs_range_num, ctxt_idx, ctxt_range_num;
  int member_idx, num_members;
  kdu_sampled_range *rg=NULL;
  kdu_client_translator *translator = owner->translator;
  if (translator != NULL)
    translator->update();
  cs_range_num = ctxt_range_num = ctxt_idx = member_idx = num_members = 0;
  while (1)
    {
      // Find next code-stream or code-stream context
      kdu_coords res = ref_window.resolution;
      kdu_dims reg = ref_window.region;
      int num_context_comps = 0;
      const int *context_comps = NULL;
      int num_cs_comps = 0;
      int *cs_comps = NULL;
      if (translator != NULL)
        { // Get codestream and window by translating codestream context
          member_idx++;
          if (member_idx >= num_members)
            { // Load next context
              if ((rg == NULL) || ((ctxt_idx += rg->step) > rg->to))
                { // Advance to next range
                  rg = ref_window.contexts.access_range(ctxt_range_num++);
                  if (rg == NULL)
                    { translator = NULL; continue; }
                  else
                    ctxt_idx = rg->from;
                }
              member_idx = 0;
              num_members =
                translator->get_num_context_members(rg->context_type,ctxt_idx,
                                                    rg->remapping_ids);
              if (num_members == 0)
                continue;
            }
          cs_idx =
            translator->get_context_codestream(rg->context_type,ctxt_idx,
                                               rg->remapping_ids,member_idx);
          if (!translator->perform_context_remapping(rg->context_type,ctxt_idx,
                                                     rg->remapping_ids,
                                                     member_idx,res,reg))
            continue; // Cannot translate view window
          context_comps =
            translator->get_context_components(rg->context_type,ctxt_idx,
                                               rg->remapping_ids,member_idx,
                                               num_context_comps);
        }
      else
        { // Get codestream directly, without translation
          if ((rg == NULL) || ((cs_idx += rg->step) > rg->from))
            { // Advance to next range
              rg = ref_window.codestreams.access_range(cs_range_num++);
              if (rg == NULL)
                break; // Break out of main loop; all code-streams checked
              if (rg->context_type == KDU_JPIP_CONTEXT_TRANSLATED)
                { rg = NULL; continue; }
              cs_idx = rg->from;
            }
        }
          
      if (cs_idx < 0)
        continue;
      cs_started = false;

      // Check main code-stream header first
      available_bytes =
        owner->get_databin_length(KDU_MAIN_HEADER_DATABIN,
                                  cs_idx,0,&is_complete);
      if (((available_bytes > 0) || is_complete) &&
          (is_stateless ||
           owner->mark_databin(KDU_MAIN_HEADER_DATABIN,cs_idx,0,false)))
        write_cache_descriptor(cs_idx,cs_started,
                               "Hm",-1,available_bytes,is_complete,block);
      kd_client_model_manager *mgr;
      for (mgr=client_model_managers; mgr != NULL; mgr=mgr->next)
        if (mgr->codestream_id >= cs_idx)
          break;
      if ((mgr == NULL) || (mgr->codestream_id != cs_idx) || !is_complete)
        continue; // No model info available for this code-stream, or else
                  // no point looking for precinct and tile headers, since
                  // main header is not available.

      kdu_codestream aux_stream = mgr->codestream;
      if (!aux_stream)
        continue; // Can't open this code-stream yet.
      if ((res.x < 1) || (res.y < 1) || (reg.size.x < 1) || (reg.size.y < 1))
        continue; // No relevant tile headers or precincts

      // Find the set of available components
      int total_cs_comps = aux_stream.get_num_components();
      bool expand_ycc = false;
      if (context_comps == NULL)
        { // Expand the codestream component ranges to simplify matters
          cs_comps = get_scratch_ints(total_cs_comps);
          if ((num_cs_comps =
               ref_window.components.expand(cs_comps,0,total_cs_comps-1)) == 0)
            for (num_cs_comps=0; num_cs_comps < total_cs_comps; num_cs_comps++)
              cs_comps[num_cs_comps] = num_cs_comps; // Includes all components
          if (total_cs_comps >= 3)
            { // See if we need to include any extra components to allow for
              // inverting a Part-1 decorrelating transform
              int i;
              bool ycc_usage[3]={false,false,false};
              for (i=0; i < num_cs_comps; i++)
                if (cs_comps[i] < 3)
                  expand_ycc = ycc_usage[cs_comps[i]] = true;
              if (expand_ycc)
                for (i=0; i < 3; i++)
                  if (!ycc_usage[i])
                    cs_comps[num_cs_comps++] = i;
            }
        }

      // Get global codestream parameters
      kdu_dims image_dims, total_tiles;
      aux_stream.apply_input_restrictions(0,0,0,0,NULL,
                                          KDU_WANT_OUTPUT_COMPONENTS);
      aux_stream.get_dims(-1,image_dims);
      aux_stream.get_valid_tiles(total_tiles);

      // Mimic the server's window adjustments.
      // Start by figuring out the number of discard levels
      int round_direction = ref_window.round_direction;
      kdu_coords min = image_dims.pos;
      kdu_coords size = image_dims.size;
      kdu_coords lim = min + size;
      kdu_dims active_res; active_res.pos = min; active_res.size = size;
      kdu_long target_area = ((kdu_long) res.x) * ((kdu_long) res.y);
      kdu_long best_area_diff = 0;
      int active_discard_levels=0, d=0;
      bool done = false;
      while (!done)
        {
          if (round_direction < 0)
            { // Round down
              if ((size.x <= res.x) && (size.y <= res.y))
                {
                  active_discard_levels = d;
                  active_res.size = size; active_res.pos = min;
                  done = true;
                }
            }
          else if (round_direction > 0)
            { // Round up
              if ((size.x >= res.x) && (size.y >= res.y))
                {
                  active_discard_levels = d;
                  active_res.size = size; active_res.pos = min;
                }
              else
                done = true;
            }
          else
            { // Round to closest in area
              kdu_long area = ((kdu_long) size.x) * ((kdu_long) size.y);
              kdu_long area_diff =
                (area < target_area)?(target_area-area):(area-target_area);
              if ((d == 0) || (area_diff < best_area_diff))
                {
                  active_discard_levels = d;
                  active_res.size = size; active_res.pos = min;
                  best_area_diff = area_diff;
                }
              if (area <= target_area)
                done = true; // The area can only keep on getting smaller
            }
          min.x = (min.x+1)>>1;   min.y = (min.y+1)>>1;
          lim.x = (lim.x+1)>>1;   lim.y = (lim.y+1)>>1;
          size = lim - min;
          d++;
        }

      // Now scale the image region to match the selected image resolution
      kdu_dims active_region;
      min = reg.pos;
      lim = min + reg.size;
      active_region.pos.x = (int)
        ((((kdu_long) min.x)*((kdu_long)active_res.size.x))/((kdu_long)res.x));
      active_region.pos.y = (int)
        ((((kdu_long) min.y)*((kdu_long)active_res.size.y))/((kdu_long)res.y));
      active_region.size.x = 1 + (int)
        ((((kdu_long)(lim.x-1)) *
          ((kdu_long) active_res.size.x)) / ((kdu_long) res.x))
        - active_region.pos.x;
      active_region.size.y = 1 + (int)
        ((((kdu_long)(lim.y-1)) *
          ((kdu_long) active_res.size.y)) / ((kdu_long) res.y))
        - active_region.pos.y;
      active_region.pos += active_res.pos;
      active_region &= active_res;

      // Now adjust the active region up onto the full codestream canvas
      active_region.pos.x <<= active_discard_levels;
      active_region.pos.y <<= active_discard_levels;
      active_region.size.x <<= active_discard_levels;
      active_region.size.y <<= active_discard_levels;
      active_region &= image_dims;

      // Now scan through the tiles
      kdu_dims active_tiles, active_precincts;
      aux_stream.apply_input_restrictions(0,0,0,0,&active_region,
                                          KDU_WANT_OUTPUT_COMPONENTS);
      aux_stream.get_valid_tiles(active_tiles);
      kdu_coords t_idx, abs_t_idx, p_idx;
      kdu_tile tile;
      kdu_tile_comp tcomp;
      kdu_resolution rs;
      int tnum, r, num_resolutions;
      for (t_idx.y=0; t_idx.y < active_tiles.size.y; t_idx.y++)
        for (t_idx.x=0; t_idx.x < active_tiles.size.x; t_idx.x++)
          {
            abs_t_idx = t_idx + active_tiles.pos;
            tnum = abs_t_idx.x + abs_t_idx.y * total_tiles.size.x;
            available_bytes =
              owner->get_databin_length(KDU_TILE_HEADER_DATABIN,cs_idx,
                                        tnum,&is_complete);
            if (((available_bytes > 0) || is_complete) &&
                (is_stateless ||
                 owner->mark_databin(KDU_TILE_HEADER_DATABIN,cs_idx,
                                     tnum,false)))
              write_cache_descriptor(cs_idx,cs_started,"H",tnum,
                                     available_bytes,is_complete,block);
            if (!is_complete)
              continue;
            tile = aux_stream.open_tile(abs_t_idx);
            bool have_ycc = tile.get_ycc() && expand_ycc;
            if (context_comps != NULL)
              { // Convert context components into codestream components for
                // this tile.
                int nsi, nso, nbi, nbo;
                tile.set_components_of_interest(num_context_comps,
                                                context_comps);
                tile.get_mct_block_info(0,0,nsi,nso,nbi,nbo);
                num_cs_comps = nsi;
                cs_comps = get_scratch_ints(num_cs_comps);
                tile.get_mct_block_info(0,0,nsi,nso,nbi,nbo,
                                        NULL,NULL,NULL,NULL,cs_comps);
              }

            for (int nc=0; nc < num_cs_comps; nc++)
              {
                int c_idx = cs_comps[nc];
                if (((c_idx >= 3) || !have_ycc) &&
                    !(ref_window.components.is_empty() ||
                      ref_window.components.test(c_idx)))
                  continue; // Component is excluded

                tcomp = tile.access_component(c_idx);
                num_resolutions = tcomp.get_num_resolutions();
                num_resolutions -= active_discard_levels;
                if (num_resolutions < 1)
                  num_resolutions = 1;
                for (r=0; r < num_resolutions; r++)
                  {
                    rs = tcomp.access_resolution(r);
                    rs.get_valid_precincts(active_precincts);
                    for (p_idx.y=0; p_idx.y < active_precincts.size.y;
                         p_idx.y++)
                      for (p_idx.x=0; p_idx.x < active_precincts.size.x;
                           p_idx.x++)
                        {
                          bin_id =
                            rs.get_precinct_id(p_idx+active_precincts.pos);
                          available_bytes =
                            owner->get_databin_length(KDU_PRECINCT_DATABIN,
                                                   cs_idx,bin_id,&is_complete);
                          if (((available_bytes > 0) || is_complete) &&
                              (is_stateless ||
                               owner->mark_databin(KDU_PRECINCT_DATABIN,
                                                   cs_idx,bin_id,false)))
                            write_cache_descriptor(cs_idx,cs_started,
                                                   "P",bin_id,available_bytes,
                                                   is_complete,block);
                        }
                  }
              }
            tile.close();
          }
    }

  // Finally, signal whatever meta-data bins we already have
  cs_started = true; // Prevent inclusion of additional code-stream qualifiers
  bin_id = owner->get_next_lru_databin(KDU_META_DATABIN,0,-1,!is_stateless);
  while (bin_id >= 0)
    {
      available_bytes =
        owner->get_databin_length(KDU_META_DATABIN,0,bin_id,&is_complete);
      if (((available_bytes > 0) || is_complete) &&
          (is_stateless ||
           owner->mark_databin(KDU_META_DATABIN,0,bin_id,false)))
        write_cache_descriptor(0,cs_started,"M",bin_id,available_bytes,
                               is_complete,block);
      bin_id = owner->get_next_lru_databin(KDU_META_DATABIN,0,bin_id,
                                           !is_stateless);
    }

  // Now we are done writing all model information
  if (block.get_remaining_bytes() == test_chars)
    {
      block.backspace(block.get_remaining_bytes()-start_chars);
      return false;
    }
  else
    block.backspace(1); // Backspace over the trailing comma.
  return true;
}

/*****************************************************************************/
/*                    kd_client::add_client_model_manager                    */
/*****************************************************************************/

kd_client_model_manager *
  kd_client::add_client_model_manager(kdu_long codestream_id)
{
  kd_client_model_manager *mgr, *prev_mgr;
  for (prev_mgr=NULL, mgr=client_model_managers;
       mgr != NULL;
       prev_mgr=mgr, mgr=mgr->next)
    if (mgr->codestream_id >= codestream_id)
      break;
  if ((mgr != NULL) && (mgr->codestream_id == codestream_id))
    return mgr; // Already exists
  kd_client_model_manager *new_mgr = new kd_client_model_manager;
  new_mgr->next = mgr;
  if (prev_mgr == NULL)
    client_model_managers = new_mgr;
  else
    prev_mgr->next = new_mgr;
  new_mgr->codestream_id = codestream_id;
  new_mgr->aux_cache.attach_to(owner);
  new_mgr->aux_cache.set_read_scope(KDU_MAIN_HEADER_DATABIN,codestream_id,0);
  new_mgr->codestream.create(&new_mgr->aux_cache);
  new_mgr->codestream.set_persistent();
  return new_mgr;
}

/*****************************************************************************/
/*                      kd_client::save_cache_contents                       */
/*****************************************************************************/

bool
  kd_client::save_cache_contents()
{
  cache_file = fopen(cache_path,"wb");
  if (cache_file == NULL)
    return false;
  fprintf(cache_file,"kjc/1.0\n");
  fprintf(cache_file,"%s\n",original_request);
  fprintf(cache_file,"%s\n",original_server);

  bool is_complete;
  int cls, length, id_bits, cs_bits, i;
  kdu_long cs_id, bin_id;
  kdu_byte *hd, header[24];

  cs_id = owner->get_next_codestream(-1);
  while (cs_id >= 0)
    {
      for (cls=0; cls < KDU_NUM_DATABIN_CLASSES; cls++)
        {
          bin_id = owner->get_next_lru_databin(cls,cs_id,-1,false);
          while (bin_id >= 0)
            {
              length =
                owner->get_databin_length(cls,cs_id,bin_id,&is_complete);
              if ((length > 0) || is_complete)
                { // Save data-bin
                  if (length > cache_store_len)
                    {
                      if (cache_store_buf != NULL) delete[] cache_store_buf;
                      cache_store_len += length+256;
                      cache_store_buf = new kdu_byte[cache_store_len];
                    }
                  length =
                    owner->get_databin_prefix(cls,cs_id,bin_id,
                                              cache_store_buf,length);
                  hd = header;
                  if (is_complete)
                    *(hd++) = (kdu_byte)(2*cls+1);
                  else
                    *(hd++) = (kdu_byte)(2*cls);
                  for (cs_bits=0; (cs_id>>cs_bits) > 0; cs_bits+=8);
                  for (id_bits=0; (bin_id>>id_bits) > 0; id_bits+=8);
                  *(hd++) = (kdu_byte)((cs_bits<<1) | (id_bits>>3));
                  for (i=cs_bits-8; i >= 0; i-=8)
                    *(hd++) = (kdu_byte)(cs_id>>i);
                  for (i=id_bits-8; i >= 0; i-=8)
                    *(hd++) = (kdu_byte)(bin_id>>i);
                  for (i=24; i >= 0; i-=8)
                    *(hd++) = (kdu_byte)(length>>i);
                  fwrite(header,1,(size_t)(hd-header),cache_file);
                  fwrite(cache_store_buf,1,(size_t) length,cache_file);
                }
              bin_id = owner->get_next_lru_databin(cls,cs_id,bin_id,false);
            }
        }
      cs_id = owner->get_next_codestream(cs_id);
    }
  fclose(cache_file); cache_file = NULL;
  return true;
}

/*****************************************************************************/
/*                      kd_client::read_cache_contents                       */
/*****************************************************************************/

bool
  kd_client::read_cache_contents()
{
  cache_file = fopen(cache_path,"rb");
  if (cache_file == NULL)
    return false;
  if (cache_store_len < 300)
    {
      cache_store_len = 300;
      if (cache_store_buf != NULL)
        delete[] cache_store_buf;
      cache_store_buf = new kdu_byte[cache_store_len];
    }

  char *cp, *char_buf = (char *) cache_store_buf;
  *char_buf = '\0';
  fgets(char_buf,80,cache_file); // Read version info
  if (strcmp(char_buf,"kjc/1.0\n") != 0)
    return false;
  fgets(char_buf,256,cache_file); // Read original request
  cp = strchr(char_buf,'\n');
  if (cp == NULL)
    return false;
  *cp = '\0';
  if (!compare_targets(char_buf,original_request))
    return false;
  fgets(char_buf,256,cache_file); // Read original server name

  kdu_long bin_id, cs_id;
  int i, id_bytes, cs_bytes, length;
  kdu_byte *sp;
  while (fread(cache_store_buf,1,2,cache_file) == 2)
    {
      cs_bytes = (cache_store_buf[1] >> 4) & 0x0F;
      id_bytes = cache_store_buf[1] & 0x0F;
      if (fread(cache_store_buf+2,1,(size_t)(cs_bytes+id_bytes+4),
                cache_file) != (size_t)(cs_bytes+id_bytes+4))
        break;
      for (cs_id=0, sp=cache_store_buf+2, i=0; i < cs_bytes; i++)
        cs_id = (cs_id << 8) + *(sp++);
      for (bin_id=0, i=0; i < id_bytes; i++)
        bin_id = (bin_id << 8) + *(sp++);
      for (length=0, i=0; i < 4; i++)
        length = (length << 8) + *(sp++);
      bool is_complete = (cache_store_buf[0] & 1)?true:false;
      int cls = (cache_store_buf[0] >> 1);
      if (length > cache_store_len)
        {
          cache_store_len += length + 256;
          delete[] cache_store_buf;
          cache_store_buf = new kdu_byte[cache_store_len];
        }
      if (fread(cache_store_buf,1,(size_t)length,cache_file) != (size_t)length)
        break;
      if ((cls >= 0) && (cls < KDU_NUM_DATABIN_CLASSES))
        owner->add_to_databin(cls,cs_id,bin_id,cache_store_buf,0,length,
                              is_complete,false,true);
      if ((cls == KDU_MAIN_HEADER_DATABIN) && is_complete)
        add_client_model_manager(cs_id);
    }
  fclose(cache_file); cache_file = NULL;
  if (owner->notifier != NULL)
    owner->notifier->notify();
  return true;
}


/* ========================================================================= */
/*                                  kdu_client                               */
/* ========================================================================= */

/*****************************************************************************/
/*                            kdu_client::kdu_client                         */
/*****************************************************************************/

kdu_client::kdu_client()
{
  active_state = false;
  one_time_request = false;
  idle_state = false;
  status = "<not in use>";
  notifier = NULL;
  state = NULL;
  translator = NULL;
  h_mutex = CreateMutex(NULL,FALSE,NULL);
  received_bytes = 0;
  timeout_milliseconds = 0;
  keep_transport_open = false;
  saved_request_channel = NULL;
}

/*****************************************************************************/
/*                           kdu_client::~kdu_client                         */
/*****************************************************************************/

kdu_client::~kdu_client()
{
  disconnect(false);
  close();
  CloseHandle(h_mutex);
}

/*****************************************************************************/
/*                          kdu_client::acquire_lock                         */
/*****************************************************************************/

void
  kdu_client::acquire_lock()
{
  WaitForSingleObject(h_mutex,INFINITE);
}

/*****************************************************************************/
/*                          kdu_client::release_lock                         */
/*****************************************************************************/

void
  kdu_client::release_lock()
{
  ReleaseMutex(h_mutex);
}

/*****************************************************************************/
/*                             kdu_client::close                             */
/*****************************************************************************/

bool
  kdu_client::close()
{
  active_state = idle_state = one_time_request = keep_transport_open = false;
  timeout_milliseconds = 0;
  if (state != NULL)
    {
      delete state;
      state = NULL;
    }
  kdu_cache::close();
  active_state = false;
  idle_state = false;
  status = "<not in use>";
  notifier = NULL;
  received_bytes = 0;
  return true;
}

/*****************************************************************************/
/*                           kdu_client::disconnect                          */
/*****************************************************************************/

void
  kdu_client::disconnect(bool keep_transport_open,
                         int timeout_milliseconds)
{
  if (!is_alive())
    {
      if ((saved_request_channel != NULL) && !keep_transport_open)
        {
          delete saved_request_channel;
          saved_request_channel = NULL;
          this->timeout_milliseconds = 0;
          WSACleanup();
        }
      return;
    }
  this->timeout_milliseconds = timeout_milliseconds;
  this->keep_transport_open = keep_transport_open;
  delete state; // Gracefully terminates network thread first.
  state = NULL;
}

/*****************************************************************************/
/*                            kdu_client::is_alive                           */
/*****************************************************************************/

bool
  kdu_client::is_alive()
{
  if (state == NULL)
    return false;
  if (state->thread_closed)
    {
      delete state;
      state = NULL;
      return false;
    }
  return true;
}

/*****************************************************************************/
/*                             kdu_client::connect                           */
/*****************************************************************************/

void
  kdu_client::connect(const char *server, const char *proxy,
                      const char *request, const char *channel_transport,
                      const char *cache_dir)
{
  close(); // Just in case
  if ((proxy == NULL) || (*proxy == '\0'))
    proxy = server;
  if ((server == NULL) || (*server == '\0'))
    { KDU_ERROR_DEV(e,43); e <<
        KDU_TXT("You must supply a server name in the "
        "call to `kdu_client::connect'.");
    }
  if (strlen(server) > 255)
    { KDU_ERROR_DEV(e,44); e <<
        KDU_TXT("Server host name supplied to `kdu_client::connect' "
        "may not exceed 255 characters in length!  Offending name is")
        << " \"" << server << "\".";
    }
  if (strlen(proxy) > 255)
    { KDU_ERROR_DEV(e,45); e <<
        KDU_TXT("JPIP immediate (proxy) host name may not exceed "
        "255 characters in length!  Offending name is")
        << " \"" << proxy << "\".";
    }
  if (strlen(request) > 255)
    { KDU_ERROR_DEV(e,46); e <<
        KDU_TXT("Name of JPIP object requested in call to "
        "`kdu_client::connect' may not exceed 255 characters in length!  "
        "Offending name is")
        << " \"" << request << "\".";
    }
  if (channel_transport == NULL)
    channel_transport = "";
  if (strlen(channel_transport) > 79)
    { KDU_ERROR_DEV(e,47); e <<
        KDU_TXT("JPIP channel transport type string may not exceed "
        "79 characters in length!  Offending name is")
        << " \"" << channel_transport << "\".";
    }
  if (cache_dir == NULL)
    cache_dir = "";
  if (strlen(cache_dir) > 199)
    { KDU_ERROR_DEV(e,48); e <<
        KDU_TXT("JPIP cache directory name may not exceed "
        "199 characters in length!  Offending name is")
        << " \"" << cache_dir << "\".";
    }

  assert(state == NULL);
  state = new kd_client(this);
  strcpy(state->original_server,server);
  strcpy(state->server,server);
  strcpy(state->immediate_server,proxy);
  strcpy(state->cache_dir,cache_dir);
  strcpy(state->original_request,request);
  strcpy(state->resource,request);
  strcpy(state->channel_transport,channel_transport);

  // Separate out the port numbers
  char *cp;
  int port;
  if (((cp = strrchr(state->server,':')) != NULL) &&
      (cp > state->server) && (sscanf(cp+1,"%d",&port) == 1))
    {
      if ((port <= 0) || (port >= (1<<16)))
        { KDU_ERROR(e,49); e <<
            KDU_TXT("Illegal port number found in server address suffix")
            << ", \"" << state->server << "\", " <<
            KDU_TXT("in call to `kdu_client::connect'.");
        }
      state->request_port = (kdu_uint16) port;
      *cp = '\0';
    }
  if (((cp = strrchr(state->immediate_server,':')) != NULL) &&
      (cp > state->immediate_server) && (sscanf(cp+1,"%d",&port) == 1))
    {
      if ((port <= 0) || (port >= (1<<16)))
        { KDU_ERROR(e,50); e <<
            KDU_TXT("Illegal port number found in suffix of the "
            "immediate (proxy) server address")
            << ", \"" << state->immediate_server << "\".";
        }
      state->immediate_port = (kdu_uint16) port;
      *cp = '\0';
    }

  // Replace backslashes with forward slashes
  for (cp=state->server; *cp != '\0'; cp++)
    if (*cp == '\\')
      *cp = '/';
  for (cp=state->immediate_server; *cp != '\0'; cp++)
    if (*cp == '\\')
      *cp = '/';
  for (cp=state->resource; *cp != '\0'; cp++)
    if (*cp == '\\')
      *cp = '/';
  for (cp=state->cache_dir; *cp != '\0'; cp++)
    if (*cp == '\\')
      *cp = '/';
  if ((cp > state->cache_dir) && (cp[-1] == '/'))
    cp[-1] = '\0';

  // Change session type to lower case and look for "none" key word
  for (cp=state->channel_transport; *cp != '\0'; cp++)
    *cp = (char) tolower(*cp);
  if (strcmp(state->channel_transport,"none") == 0)
    state->channel_transport[0] = '\0';

  // Separate query component from resource string
  if ((cp = strchr(state->resource,'?')) != NULL)
    {
      *cp = '\0';
      strcpy(state->query,cp+1);
    }

  // Check for interactivity
  state->is_interactive = true;
  one_time_request = false;
  const char *qp = state->query;
  while (*qp != '\0')
    {
      if (!(kd_parse_request_field(qp,JPIP_FIELD_TARGET) ||
            kd_parse_request_field(qp,JPIP_FIELD_SUB_TARGET)))
        break;
      while ((*qp != '\0') && (*qp != '&'))
        qp++;
      if (*qp == '&')
        qp++;
    }
  if (*qp != '\0')
    {
      state->is_interactive = false;
      state->channel_transport[0] = '\0';
      one_time_request = true;
    }

  // Start the network management thread.
  active_state = true;
  idle_state = false;
  timeout_milliseconds = 0;
  keep_transport_open = false;
  state->start();
}

/*****************************************************************************/
/*                          kdu_client::post_window                          */
/*****************************************************************************/

bool
  kdu_client::post_window(kdu_window *window)
{
#ifdef SIMULATE_REQUESTS
  return false; // Window requests generated from the simulation file instead
#endif // SIMULATE_REQUESTS

  if (!is_alive())
    return false; // Safest just to do nothing
  bool window_accepted = true;
  acquire_lock();
  if (!state->is_interactive)
    {
      release_lock();
      return false;
    }
  if (state->request_waiting == NULL)
    state->request_waiting = state->get_empty_window_request();
  state->request_waiting->window.copy_from(*window);
  kd_window_request *recent = state->request_tail;
  if ((recent != NULL) && // recent->window_completed -- seemed unnecessary
      (recent->window.equals(*window) ||
       (idle_state && recent->window_completed &&
        recent->window.contains(*window))))
    { // New request has no effect
      state->return_window_request(state->request_waiting);
      state->request_waiting = NULL;
      window_accepted = false;
    }
  else
    {
      state->request_waiting->new_elements =
        ((recent == NULL) || !recent->window.contains(*window));
      state->window_posted();
      idle_state = false;
    }
  release_lock();
  return window_accepted;
}

/*****************************************************************************/
/*                    kdu_client::get_window_in_progress                     */
/*****************************************************************************/

bool
  kdu_client::get_window_in_progress(kdu_window *window)
{
  is_alive(); // Destroys `state' if network thread terminated.
  acquire_lock();
  if ((state == NULL) || (state->request_head == NULL))
    {
      release_lock();
      if (window != NULL)
        window->init();
      return false;
    }
  if (window != NULL)
    window->copy_from(state->request_head->window,true);
  bool is_current = (state->request_tail == state->request_head) &&
                    (state->request_waiting == NULL);
  release_lock();
  return is_current;
}


/* ========================================================================= */
/*                            kdu_client_translator                          */
/* ========================================================================= */

/*****************************************************************************/
/*                kdu_client_translator::kdu_client_translator               */
/*****************************************************************************/

kdu_client_translator::kdu_client_translator()
{
  return; // Need this function for foreign language bindings
}
