/*****************************************************************************/
// File: connection.cpp [scope = APPS/SERVER]
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
   Implements the session connection machinery, which talks HTTP with the
client in order to return the parameters needed to establish a persistent
connection -- it also brokers the persistent connection process itself
handing off the relevant sockets once connected.  This source file forms part
of the `kdu_server' application.
******************************************************************************/

#include <math.h>
#include "server_local.h"
#include "kdu_messaging.h"

/* ========================================================================= */
/*                             Internal Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                  process_delegate_response                         */
/*****************************************************************************/

static int
  process_delegate_response(const char *reply, kd_message_block &par_block,
                            const char *host_address, kdu_uint16 port)
  /* This function essentially copies the `reply' paragraph from a delegated
     host server into the supplied `par_block', returning the value of the
     status code received from the delegated host.  However, it also
     introduces a number of modifications.  Firstly, if the delegated host's
     reply did not include the "Connection: close" header, that header will
     be appended to the paragraph written to `par_block'.  Secondly, if
     the reply paragraph includes a "JPIP-cnew:" header, the new channel
     details are scanned and the `host_address' and `port' information is
     included in the list if not already in there.   If `reply' is not a
     valid HTTP reply paragraph, the function returns error code 503, which
     allows the delegation function to try another host. */
{
  int code = 503;
  const char *end, *cp;
  
  if (reply == NULL)
    return code;
  cp = strchr(reply,' ');
  if (cp == NULL)
    return code;
  while (*cp == ' ') cp++;
  if (sscanf(cp,"%d",&code) == 0)
    return code;

  par_block.restart();
  bool have_connection_close = false;
  while ((*reply != '\0') && (*reply != '\n'))
    {
      for (end=reply; (*end != '\n') && (*end != '\0'); end++);
      int line_length = (int)(end-reply);
      if (kd_has_caseless_prefix(reply,"JPIP-" JPIP_FIELD_CHANNEL_NEW ": "))
        {
          reply += strlen("JPIP-" JPIP_FIELD_CHANNEL_NEW ": ");
          while (*reply == ' ') reply++;
          par_block << "JPIP-" JPIP_FIELD_CHANNEL_NEW ": ";
          while (1)
            { // Scan through all the comma-delimited tokens
              cp = reply;
              while ((*cp != ',') && (*cp != ' ') &&
                     (*cp != '\0') && (*cp != '\n'))
                cp++;
              if (cp > reply)
                {
                  if (kd_has_caseless_prefix(reply,"host="))
                    host_address = NULL;
                  else if (kd_has_caseless_prefix(reply,"port="))
                    port = 0;
                  par_block.write_raw((kdu_byte *) reply,(int)(cp-reply));
                }
              if (*cp != ',')
                break;
              par_block << ",";
              reply = cp+1;
            }
          if (host_address != NULL)
            par_block << ",host=" << host_address;
          if (port != 0)
            par_block << ",port=" << port;
        }
      else
        {
          if (kd_has_caseless_prefix(reply,"Connection: "))
            {
              cp = strchr(reply,' ');
              while (*cp == ' ') cp++;
              if (kd_has_caseless_prefix(cp,"close"))
                have_connection_close = true;
            }
          par_block.write_raw((kdu_byte *) reply,line_length);
        }
      par_block << "\r\n";
      reply = (*end == '\0')?end:(end+1);
    }
  if (!have_connection_close)
    par_block << "Connection: close\r\n";
  par_block << "\r\n";
  return code;
}


/* ========================================================================= */
/*                              kd_request_fields                            */
/* ========================================================================= */

/*****************************************************************************/
/*                       kd_request_fields::write_query                      */
/*****************************************************************************/

void
  kd_request_fields::write_query(kd_message_block &block) const
{
  int n=0;

  if (target != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_TARGET "=" << target;
  if (sub_target != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_SUB_TARGET "=" << sub_target;
  if (target_id != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_TARGET_ID "=" << target_id;

  if (channel_id != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_CHANNEL_ID "=" << channel_id;
  if (channel_new != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_CHANNEL_NEW "=" << channel_new;
  if (channel_close != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_CHANNEL_CLOSE "=" << channel_close;
  if (request_id != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_REQUEST_ID "=" << request_id;

  if (full_size != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_FULL_SIZE "=" << full_size;
  if (region_size != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_REGION_SIZE "=" << region_size;
  if (region_offset != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_REGION_OFFSET "=" << region_offset;
  if (components != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_COMPONENTS "=" << components;
  if (codestreams != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_CODESTREAMS "=" << codestreams;
  if (contexts != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_CONTEXTS "=" << contexts;
  if (roi != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_ROI "=" << roi;
  if (layers != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_LAYERS "=" << layers;
  if (source_rate != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_SOURCE_RATE "=" << source_rate;

  if (meta_request != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_META_REQUEST "=" << meta_request;

  if (max_length != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_MAX_LENGTH "=" << max_length;
  if (max_quality != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_MAX_QUALITY "=" << max_quality;

  if (align != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_ALIGN "=" << align;
  if (type != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_TYPE "=" << type;
  if (delivery_rate != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_DELIVERY_RATE "=" << delivery_rate;

  if (model != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_MODEL "=" << model;
  if (tp_model != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_TP_MODEL "=" << tp_model;
  if (need != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_NEED "=" << need;

  if (capabilities != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_CAPABILITIES "=" << capabilities;
  if (preferences != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_PREFERENCES "=" << preferences;

  if (upload != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_UPLOAD "=" << upload;
  if (xpath_box != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_XPATH_BOX "=" << xpath_box;
  if (xpath_bin != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_XPATH_BIN "=" << xpath_bin;
  if (xpath_query != NULL)
    block << (((n++)==0)?"":"&")<<JPIP_FIELD_XPATH_QUERY "=" << xpath_query;

  if (unrecognized != NULL)
    block << (((n++)==0)?"":"&") << unrecognized;
}


/* ========================================================================= */
/*                                 kd_request                                */
/* ========================================================================= */

/*****************************************************************************/
/*                          kd_request::write_method                         */
/*****************************************************************************/

void
  kd_request::write_method(const char *string, int string_len)
{
  assert(string != NULL);
  resource = query = http_accept = NULL;
  set_cur_buf_len(string_len+1);
  memcpy(buf,string,string_len); buf[string_len]='\0';
  method = buf;
}

/*****************************************************************************/
/*                         kd_request::write_resource                        */
/*****************************************************************************/

void
  kd_request::write_resource(const char *string, int string_len)
{
  assert((string != NULL) && (method != NULL));
  int offset = cur_buf_len;
  set_cur_buf_len(offset+string_len+1);
  method = buf;
  memcpy(buf+offset,string,string_len); buf[offset+string_len]='\0';
  resource = buf+offset;
}

/*****************************************************************************/
/*                          kd_request::write_query                          */
/*****************************************************************************/

void
  kd_request::write_query(const char *string, int string_len)
{
  assert((string != NULL) && (method != NULL) && (resource != NULL));
  char *old_buf = buf;
  int offset = cur_buf_len;
  set_cur_buf_len(offset+string_len+1);
  method = buf;
  resource = buf + (resource-old_buf);
  if (http_accept != NULL)
    http_accept = buf + (http_accept-old_buf);
  memcpy(buf+offset,string,string_len); buf[offset+string_len]='\0';
  query = buf+offset;
}

/*****************************************************************************/
/*                       kd_request::write_http_accept                       */
/*****************************************************************************/

void
  kd_request::write_http_accept(const char *string, int string_len)
{
  assert((string != NULL) && (method != NULL) && (resource != NULL));
  char *old_buf = buf;
  int offset = cur_buf_len;
  set_cur_buf_len(offset+string_len+1);
  method = buf;
  resource = buf + (resource-old_buf);
  if (query != NULL)
    query = buf + (query-old_buf);
  memcpy(buf+offset,string,string_len); buf[offset+string_len]='\0';
  http_accept = buf+offset;
}

/*****************************************************************************/
/*                             kd_request::copy                              */
/*****************************************************************************/

void
  kd_request::copy(const kd_request *src)
{
  init();
  set_cur_buf_len(src->cur_buf_len);
  memcpy(buf,src->buf,cur_buf_len);
  if (src->method != NULL)
    method = buf + (src->method - src->buf);
  if (src->resource != NULL)
    resource = buf + (src->resource - src->buf);
  const char **src_ptr = (const char **) &(src->fields);
  const char **dst_ptr = (const char **) &fields;
  const char **lim_src_ptr = (const char **)(&(src->fields)+1);
  for (; src_ptr < lim_src_ptr; src_ptr++, dst_ptr++)
    { // Walk through all the request fields
      if (*src_ptr != NULL)
        {
          assert((*src_ptr >= src->buf) &&
                 (*src_ptr < (src->buf+src->cur_buf_len)));
          *dst_ptr = buf + (*src_ptr - src->buf);
        }
      else
        *dst_ptr = NULL;
    }
  close_connection = src->close_connection;
  preemptive = src->preemptive;
}


/* ========================================================================= */
/*                              kd_request_queue                             */
/* ========================================================================= */

/*****************************************************************************/
/*                     kd_request_queue::~kd_request_queue                   */
/*****************************************************************************/

kd_request_queue::~kd_request_queue()
{
  while ((tail=head) != NULL)
    {
      head=tail->next;
      delete tail;
    }
  while ((tail=free_list) != NULL)
    {
      free_list=tail->next;
      delete tail;
    }
  if (pending != NULL)
    delete pending;
}

/*****************************************************************************/
/*                           kd_request_queue::init                          */
/*****************************************************************************/

void
  kd_request_queue::init()
{
  while ((tail=head) != NULL)
    {
      head = tail->next;
      return_request(tail);
    }
  if (pending != NULL)
    return_request(pending);
  pending = NULL;
  pending_body_bytes = 0;
}

/*****************************************************************************/
/*                       kd_request_queue::read_request                      */
/*****************************************************************************/

bool
  kd_request_queue::read_request(bool blocking, kd_tcp_channel *channel)
{
  const char *cp, *text;
  
  if (pending_body_bytes == 0)
    {
      assert(pending == NULL);
      do {
          if ((text = channel->read_paragraph(blocking)) == NULL)
            return false;
          kd_msg_log.print(text,"<< ");
        } while (*text == '\n'); // Skip over blank lines

      pending = get_empty_request();

      if (kd_has_caseless_prefix(text,"GET "))
        {
          pending->write_method(text,3);
          text += 4;
        }
      else if (kd_has_caseless_prefix(text,"POST "))
        {
          pending->write_method(text,4);
          text += 5;
          if ((cp = kd_caseless_search(text,"\nContent-length:")) != NULL)
            {
              while (*cp == ' ') cp++;
              sscanf(cp,"%d",&pending_body_bytes);
              if ((cp = kd_caseless_search(text,"\nContent-type:")) != NULL)
                {
                  while (*cp == ' ') cp++;
                  if (!(kd_has_caseless_prefix(cp,
                                      "application/x-www-form-urlencoded") ||
                        kd_has_caseless_prefix(cp,"x-www-form-urlencoded")))
                    cp = NULL;
                }
            }
          if ((cp == NULL) || (pending_body_bytes < 0) ||
              (pending_body_bytes > 32000))
            { // Protocol not being followed; ignore query and kill connection
              pending_body_bytes = 0;
              pending->close_connection = true;        
            }
        }
      else
        { // Copy the method string only
          for (cp=text; (*cp != ' ') && (*cp != '\n') && (*cp != '\0'); cp++);
          pending->write_method(text,(int)(cp-text));
          pending->close_connection = true;
          complete_pending_request();
          return true; // Return true, even though it is an invalid request
        }

      // Find start of the resource string
      while (*text == ' ') text++;
      if (*text == '/')
        text++;
      else
        for (; (*text != ' ') && (*text != '\0') && (*text != '\n'); text++)
          if ((text[0] == '/') && (text[1] == '/'))
            { text += 2; break; }

      // Find end of the resource string
      cp = text;
      while ((*cp != '?') && (*cp != '\0') && (*cp != ' ') && (*cp != '\n'))
        cp++;
      pending->write_resource(text,(int)(cp-text));
      text = cp;

      // Look for query string
      if (pending_body_bytes == 0)
        {
          if (*text == '?')
            {
              text++; cp = text;
              while ((*cp != ' ') && (*cp != '\n') && (*cp != '\0'))
                cp++;
            }
          pending->write_query(text,(int)(cp-text));
          text = cp;
        }
      
      if (!pending->close_connection)
        { // See if we need to close the connection
          while (*text == ' ') text++;
          float version;
          if (kd_has_caseless_prefix(text,"HTTP/") &&
              (sscanf(text+5,"%f",&version) == 1) && (version < 1.1F))
            pending->close_connection = true;
          if ((cp=kd_caseless_search(text,"\nConnection:")) != NULL)
            {
              while (*cp == ' ') cp++;
              if (kd_has_caseless_prefix(cp,"CLOSE"))
                pending->close_connection = true;
            }
        }

      if ((cp=kd_caseless_search(text,"\nAccept:")) != NULL)
        {
          while (*cp == ' ') cp++;
          for (text=cp; (*cp != ' ') && (*cp != '\n') && (*cp != '\0'); cp++);
          pending->write_http_accept(text,(int)(cp-text));
        }
    }

  if (pending_body_bytes > 0)
    {
      int n;
      kdu_byte *body = channel->read_raw(blocking,pending_body_bytes);
      if (body == NULL)
        return false;

      text = (const char *) body;
      for (cp=text, n=0; n < pending_body_bytes; n++, cp++)
        if ((*cp == ' ') || (*cp == '\n') || (*cp == '\0'))
          break;
      pending->write_query(text,n);
      kd_msg_log.print(text,n,"<< ");
    }

  complete_pending_request();
  return true;
}

/*****************************************************************************/
/*                         kd_request_queue::push_copy                       */
/*****************************************************************************/

void
  kd_request_queue::push_copy(const kd_request *src)
{
  kd_request *req = get_empty_request();
  req->copy(src);
  if (tail == NULL)
    head = tail = req;
  else
    tail = tail->next = req;
}

/*****************************************************************************/
/*                      kd_request_queue::transfer_state                     */
/*****************************************************************************/

void
  kd_request_queue::transfer_state(kd_request_queue *src)
{
  assert(this->pending == NULL);
  if (src->head != NULL)
    {
      if (this->tail == NULL)
        this->head = src->head;
      else
        this->tail->next = src->head;
      if (src->tail != NULL)
        this->tail = src->tail;
      src->head = src->tail = NULL;
    }
  if (src->pending != NULL)
    {
      this->pending = src->pending;
      this->pending_body_bytes = src->pending_body_bytes;
      src->pending = NULL;
      src->pending_body_bytes = 0;
    }
}

/*****************************************************************************/
/*                       kd_request_queue::have_request                      */
/*****************************************************************************/

bool
  kd_request_queue::have_request(bool only_if_preemptive)
{
  kd_request *req;
  for (req=head; req != NULL; req=req->next)
    if (req->preemptive || !only_if_preemptive)
      break;
  return (req != NULL);
}

/*****************************************************************************/
/*                         kd_request_queue::pop_head                        */
/*****************************************************************************/

const kd_request *
  kd_request_queue::pop_head(bool *is_overridden)
{
  kd_request *req;
  if (is_overridden != NULL)
    {
      *is_overridden = false;
      if (head == NULL)
        return NULL;
      for (req=head->next; req != NULL; req=req->next)
        if (req->preemptive)
          {
            *is_overridden = true;
            break;
          }
    }
  req = head;
  if (req != NULL)
    {
      if ((head = req->next) == NULL)
        tail = NULL;
      return_request(req);
    }
  return req;
}

/*****************************************************************************/
/*                kd_request_queue::complete_pending_request                 */
/*****************************************************************************/

void
  kd_request_queue::complete_pending_request()
{
  assert(pending != NULL);
  if (tail == NULL)
    head = tail = pending;
  else
    tail = tail->next = pending;
  pending = NULL;
  pending_body_bytes = 0;

  if (tail->resource != NULL)
    hex_hex_decode((char *) tail->resource,tail->resource,tail->cur_buf_len-1);
  else
    return;

  if (tail->query != NULL)
    hex_hex_decode((char *) tail->query,tail->query,tail->cur_buf_len-1);

  tail->preemptive = true; // Until proven otherwise
  const char *next_scan, *scan = tail->query;
  for (bool scan_done=(scan==NULL); !scan_done; scan=next_scan)
    {
      scan_done = true;
      for (next_scan=scan; *next_scan != '\0'; next_scan++)
        if (*next_scan == '&')
          { // Skip over the field separator, replacing it with null terminator
            *((char *) next_scan) = '\0';
            next_scan++;
            scan_done = false;
            break;
          }

      if (kd_parse_request_field(scan,JPIP_FIELD_WAIT))
        {
          if (strcmp(scan,"yes")==0)
            tail->preemptive = false;
        }
      else if (kd_parse_request_field(scan,JPIP_FIELD_TARGET))
        tail->fields.target = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_SUB_TARGET))
        tail->fields.sub_target = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_TARGET_ID))
        tail->fields.target_id = scan;

      else if (kd_parse_request_field(scan,JPIP_FIELD_CHANNEL_ID))
        tail->fields.channel_id = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_CHANNEL_NEW))
        tail->fields.channel_new = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_CHANNEL_CLOSE))
        tail->fields.channel_close = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_REQUEST_ID))
        tail->fields.request_id = scan;

      else if (kd_parse_request_field(scan,JPIP_FIELD_FULL_SIZE))
        tail->fields.full_size = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_REGION_SIZE))
        tail->fields.region_size = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_REGION_OFFSET))
        tail->fields.region_offset = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_COMPONENTS))
        tail->fields.components = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_CODESTREAMS))
        tail->fields.codestreams = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_CONTEXTS))
        tail->fields.contexts = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_ROI))
        tail->fields.roi = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_LAYERS))
        tail->fields.layers = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_SOURCE_RATE))
        tail->fields.source_rate = scan;

      else if (kd_parse_request_field(scan,JPIP_FIELD_META_REQUEST))
        tail->fields.meta_request = scan;

      else if (kd_parse_request_field(scan,JPIP_FIELD_MAX_LENGTH))
        tail->fields.max_length = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_MAX_QUALITY))
        tail->fields.max_quality = scan;

      else if (kd_parse_request_field(scan,JPIP_FIELD_ALIGN))
        tail->fields.align = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_TYPE))
        tail->fields.type = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_DELIVERY_RATE))
        tail->fields.delivery_rate = scan;

      else if (kd_parse_request_field(scan,JPIP_FIELD_MODEL))
        tail->fields.model = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_TP_MODEL))
        tail->fields.tp_model = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_NEED))
        tail->fields.need = scan;

      else if (kd_parse_request_field(scan,JPIP_FIELD_CAPABILITIES))
        tail->fields.capabilities = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_PREFERENCES))
        tail->fields.preferences = scan;

      else if (kd_parse_request_field(scan,JPIP_FIELD_UPLOAD))
        tail->fields.upload = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_XPATH_BOX))
        tail->fields.xpath_box = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_XPATH_BIN))
        tail->fields.xpath_bin = scan;
      else if (kd_parse_request_field(scan,JPIP_FIELD_XPATH_QUERY))
        tail->fields.xpath_query = scan;

      else if (kd_parse_request_field(scan,"admin_key"))
        tail->fields.admin_key = scan;
      else if (kd_parse_request_field(scan,"admin_command"))
        tail->fields.admin_command = scan;
      else
        tail->fields.unrecognized = scan;
    }

  if (tail->fields.target == NULL)
    tail->fields.target = tail->resource;

  // Convert the file name to lower case so as to ensure correct matching
  char *cp;
  for (cp=(char *)(tail->fields.target); *cp != '\0'; cp++)
    *cp = (char) tolower(*cp);

  if ((tail->fields.type == NULL) && (tail->http_accept != NULL))
    { // Parse acceptable types from the HTTP "Accept:" header
      tail->fields.type = tail->http_accept;
      const char *sp=tail->http_accept;
      cp = (char *)(tail->fields.type);
      for (; *sp != '\0'; sp++)
        { // It is sufficient to remove spaces, since "type" request bodies
          // have otherwise the same syntax as the "Accept:" header.
          if ((*sp != ' ') && (*sp != '\r') && (*sp != '\n') && (*sp != '\t'))
            *(cp++) = *sp;
        }
      *cp = '\0';
    }
}


/* ========================================================================= */
/*                            kd_connection_thread                           */
/* ========================================================================= */

static unsigned int __stdcall
  kd_connection_thread_start_proc(void *param)
{
  kd_connection_thread *obj = (kd_connection_thread *) param;
  obj->thread_entry();
  return 0;
}

/*****************************************************************************/
/*                         kd_connection_thread::start                       */
/*****************************************************************************/

void
  kd_connection_thread::start()
{
  unsigned int thread_id;
  h_thread = (HANDLE)
    _beginthreadex(NULL,0,kd_connection_thread_start_proc,this,0,&thread_id);
}

/*****************************************************************************/
/*                     kd_connection_thread::thread_entry                    */
/*****************************************************************************/

void
  kd_connection_thread::thread_entry()
{
  kd_target_description target;

  while (1)
    {
      if (is_idle)
        {
          if (termination_requested)
            return;
          else
            {
              wait_event();
              continue;
            }
        }

      if (channel == NULL)
        {
          assert(new_socket != NULL);
          channel = new kd_tcp_channel;
          channel->open(new_socket);
        }

      // If we get here, we are being asked to service a new request
      do {
          bool close_connection = false;
          try {
              target.filename[0] = target.byte_range[0] = '\0';
              channel->start_timer(timeout_milliseconds,false,NULL);
              if (!request_queue.have_request(false))
                request_queue.read_request(true,channel);
              const kd_request *req = request_queue.pop_head(NULL);
              assert(req != NULL);

              if (req->close_connection)
                close_connection = true;

              send_par.restart();
              if ((req->resource == NULL) &&
                  kd_has_caseless_prefix(req->method,"JPHT"))
                { // Check if we are connecting an auxiliary channel
                  send_par << "JPHT/1.0 ";
                  if (source_manager->install_channel(req->method,true,
                                              channel,NULL,owner,send_par))
                    channel = NULL; // Channel successfully off loaded.
                  else
                    {
                      send_par << "\r\n\r\n";
                      kd_msg_log.print(send_par,"\t>> ");
                      delete channel; // Aux channel binary, so don't send text
                      channel = NULL;
                    }
                }
              else if (req->resource == NULL)
                { // Unacceptable method
                  send_par << "HTTP/1.1 405 Unsupported request method: \""
                           << req->method << "\"\r\n"
                              "Connection: close\r\n";
                  send_par << "Server: Kakadu JPIP Server "
                           << KDU_CORE_VERSION << "\r\n\r\n";
                  kd_msg_log.print(send_par,"\t>> ");
                  channel->write_block(true,send_par);
                  close_connection = true;
                }
              else if (req->fields.unrecognized != NULL)
                {
                  send_par << "HTTP/1.1 400 Unrecognized query field: \""
                           << req->fields.unrecognized << "\"\r\n"
                           << "Connection: close\r\n";
                  send_par << "Server: Kakadu JPIP Server "
                           << KDU_CORE_VERSION << "\r\n\r\n";
                  kd_msg_log.print(send_par,"\t>> ");
                  channel->write_block(true,send_par);
                  close_connection = true;
                }
              else if (req->fields.upload != NULL)
                {
                  send_par << "HTTP/1.1 501 Upload functionality not "
                              "implemented.\r\n"
                           << "Connection: close\r\n";
                  send_par << "Server: Kakadu JPIP Server "
                           << KDU_CORE_VERSION << "\r\n\r\n";
                  kd_msg_log.print(send_par,"\t>> ");
                  channel->write_block(true,send_par);
                  close_connection = true;
                }
              else if (((req->fields.channel_id != NULL) ||
                        (req->fields.channel_close != NULL)) &&
                       (strcmp(req->resource,KD_MY_RESOURCE_NAME) == 0))
                { // Client trying to connect to existing session
                  const char *cid = req->fields.channel_id;
                  if (cid == NULL)
                    cid = req->fields.channel_close;
                  send_par << "HTTP/1.1 ";
                  request_queue.replace_head(req);  req = NULL;
                  if (source_manager->install_channel(cid,false,channel,
                                              &request_queue,owner,send_par))
                    channel = NULL; // Channel successfully off loaded.
                  else
                    { // Failure: reason already written to `send_par'
                      req = request_queue.pop_head(NULL);
                      send_par << "\r\nConnection: close\r\n";
                      send_par << "Server: Kakadu JPIP Server "
                               << KDU_CORE_VERSION << "\r\n\r\n";
                      kd_msg_log.print(send_par,"\t>> ");
                      channel->write_block(true,send_par);
                      close_connection = true;
                    }
                }
              else if ((req->fields.admin_key != NULL) &&
                       (strcmp(req->fields.admin_key,"0") == 0) &&
                       (strcmp(req->resource,KD_MY_RESOURCE_NAME) == 0))
                { // Client requesting delivery of an admin key
                  send_body.restart();
                  if (source_manager->write_admin_id(send_body))
                    {
                      send_body << "\r\n";
                      send_par << "HTTP/1.1 200 OK\r\n";
                      int length = send_body.get_remaining_bytes();
                      send_par << "Cache-Control: no-cache\r\n";
                      send_par << "Content-Type: application/text\r\n";
                      send_par << "Content-Length: " << length << "\r\n";
                      kd_msg << "\n\t"
                                "Key generated for remote admin request.\n\n";
                    }
                  else
                    {
                      send_par <<
                        "HTTP/1.1 403 Remote administration not enabled.\r\n"
                        "Connection: close\r\n";
                      kd_msg << "\n\t"
                            "Admin key requested -- feature not enabled.\n\n";
                      close_connection = true; // Prevent easy re-try
                    }
                  send_par << "Server: Kakadu JPIP Server "
                           << KDU_CORE_VERSION << "\r\n\r\n";
                  kd_msg_log.print(send_par,"\t>> ");
                  kd_msg_log.print(send_body,"\t>> ",false);
                  channel->write_block(true,send_par);
                  channel->write_block(true,send_body);
                }
              else if ((req->fields.admin_key != NULL) &&
                       (strcmp(req->resource,KD_MY_RESOURCE_NAME) == 0))
                {
                  if (source_manager->compare_admin_id(req->fields.admin_key))
                    process_admin_request(req->fields.admin_command);
                  else
                    {
                      send_par << "HTTP/1.1 400 Wrong or missing admin "
                              "key as first query field in admin request.\r\n"
                              "Connection: close\r\n";
                      kd_msg << "\n\tWrong or missing admin key as "
                                "first query field in admin request.\n\n";
                      send_par << "Server: Kakadu JPIP Server "
                               << KDU_CORE_VERSION << "\r\n\r\n";
                      kd_msg_log.print(send_par,"\t>> ");
                      channel->write_block(true,send_par);
                      close_connection = true;
                    }
                }
              else if (req->fields.target != NULL)
                {
                  strncpy(target.filename,req->fields.target,255);
                  target.filename[255] = '\0';
                  if (req->fields.sub_target != NULL)
                    {
                      strncpy(target.byte_range,req->fields.sub_target,79);
                      target.byte_range[79] = '\0';
                    }
                  if (owner->restricted_access &&
                      ((target.filename[0] == '/') ||
                       (strchr(target.filename,':') != NULL) ||
                       (strstr(target.filename,"..") != NULL)))
                    {
                      send_par << "HTTP/1.1 403 Attempting to access "
                                  "privileged location on server.\r\n"
                                  "Server: Kakadu JPIP Server "
                               << KDU_CORE_VERSION << "\r\n\r\n";
                      kd_msg_log.print(send_par,"\t>> ");
                      channel->write_block(true,send_par);
                    }
                  else if (!target.parse_byte_range())
                    {
                      send_par << "HTTP/1.1 400 Illegal \""
                               << JPIP_FIELD_SUB_TARGET
                               << "\" value string.\r\n"
                                  "Server: Kakadu JPIP Server "
                               << KDU_CORE_VERSION << "\r\n\r\n";
                      kd_msg_log.print(send_par,"\t>> ");
                      channel->write_block(true,send_par);
                    }
                  else if (req->fields.channel_new == NULL)
                    process_stateless_request(target,req);
                  else if (process_new_session_request(target,req))
                    close_connection = true; // Request successfully
                       // delegated or else channel passed to source thread,
                       // in which case `close_connection' will have no effect.
                }
              else
                {
                  send_par << "HTTP/1.1 405 Unsupported request\r\n"
                              "Connection: close\r\n";
                  send_par << "Server: Kakadu JPIP Server "
                           << KDU_CORE_VERSION << "\r\n\r\n";
                  kd_msg_log.print(send_par,"\t>> ");
                  channel->write_block(true,send_par);
                  close_connection = true;
                }
            }
          catch (int)
            {
              close_connection = true;
            }

          if (close_connection)
            {
              if (channel != NULL)
                {
                  delete channel;
                  channel = NULL;
                }
            }
        } while (channel != NULL);
      request_queue.init();
      new_socket = INVALID_SOCKET; // Marks thread as idle in its list.
      owner->acquire_lock();
      is_idle = true;
      owner->idle_threads++;
      owner->dispatch_pending_active_channels();
      owner->release_lock();
      owner->signal_event(); // Owner might be waiting for idle threads
    }
}

/*****************************************************************************/
/*               kd_connection_thread::process_admin_request                 */
/*****************************************************************************/

void
  kd_connection_thread::process_admin_request(const char *command)
{
  send_body.restart();
  assert(command != NULL);

  // Extract commands
  bool shutdown = false;
  const char *next;
  char command_buf[80];
  for (; command != NULL; command=next)
    {
      next = command;
      while ((*next != '\0') && (*next != ','))
        next++;
      memset(command_buf,0,80);
      if ((next-command) < 80)
        strncpy(command_buf,command,next-command);
      if (*next == ',')
        next++;
      else
        next = NULL;
      if (strcmp(command_buf,"shutdown") == 0)
        shutdown = true;
      else if (strcmp(command_buf,"stats") == 0)
        source_manager->write_stats(send_body);
      else if (strncmp(command_buf,"history",strlen("history")) == 0)
        {
          int max_records = INT_MAX;
          char *cp = command_buf + strlen("history");
          if (*cp == '=')
            sscanf(cp+1,"%d",&max_records);
          if (max_records > 0)
            source_manager->write_history(send_body,max_records);
        }
    }

  // Send reply
  send_par.restart();
  send_par << "HTTP/1.1 200 OK\r\n";
  int length = send_body.get_remaining_bytes();
  send_par << "Cache-Control: no-cache\r\n";
  if (length != 0)
    {
      send_par << "Content-Type: application/text\r\n";
      send_par << "Content-Length: " << length << "\r\n";
    }
  send_par << "\r\n";
  kd_msg_log.print(send_par,"\t>> ");
  kd_msg_log.print(send_body,"\t>> ",false);
  channel->write_block(true,send_par);
  channel->write_block(true,send_body);
  if (shutdown)
    source_manager->request_shutdown();
}

/*****************************************************************************/
/*               kd_connection_thread::process_stateless_request             */
/*****************************************************************************/

void
  kd_connection_thread::process_stateless_request(kd_target_description &tgt,
                                                  const kd_request *req)
{
  char channel_id[KD_CHANNEL_ID_LEN+1];

  send_par.restart();
  send_par << "HTTP/1.1 ";
  request_queue.replace_head(req);  req = NULL;
  if (source_manager->create_source_thread(tgt,NULL,channel_id,send_par) &&
      source_manager->install_channel(channel_id,false,channel,
                                      &request_queue,owner,send_par))
    channel = NULL; // Channel successfully off loaded.
  else
    { req = request_queue.pop_head(NULL); assert(req != NULL); }
  if (channel != NULL)
    {
      send_par << "\r\nServer: Kakadu JPIP Server "
               << KDU_CORE_VERSION << "\r\n\r\n";
      kd_msg_log.print(send_par,"\t>> ");
      channel->write_block(true,send_par);
    }

  // Write some info for the log file.
  kd_msg.start_message();
  if (channel == NULL)
    {
      kd_msg << "\n\tStateless request accepted\n";
      kd_msg << "\t\tInternal ID: " << channel_id << "\n";
    }
  else
    {
      kd_msg << "\n\t\tStateless request refused\n";
      kd_msg << "\t\t\tRequested target: \"" << tgt.filename << "\"";
      if (tgt.byte_range[0] != '\0')
        kd_msg << " (" << tgt.byte_range << ")";
      kd_msg << "\n";
    }
  kd_msg.flush(true);
}

/*****************************************************************************/
/*             kd_connection_thread::process_new_session_request             */
/*****************************************************************************/

bool
  kd_connection_thread::process_new_session_request(kd_target_description &tgt,
                                                    const kd_request *req)
{
  char channel_id[KD_CHANNEL_ID_LEN+1];
  const char *delegated_host =
    delegator->delegate(req,channel,send_par,send_body);
  if (delegated_host != NULL)
    {
      kd_msg.start_message();
      kd_msg << "\n\t\tDelegated session request to host, \""
             << delegated_host << "\"\n";
      kd_msg << "\t\t\tRequested target: \"" << tgt.filename << "\"";
      if (tgt.byte_range[0] != '\0')
        kd_msg << " (" << tgt.byte_range << ")";
      kd_msg << "\n";
      kd_msg << "\t\t\tRequested channel type: \""
             << req->fields.channel_new << "\"\n";
      kd_msg.flush(true);
      return true;
    }

  send_par.restart();
  send_par << "HTTP/1.1 ";
  const char *cnew = req->fields.channel_new;
  request_queue.replace_head(req); req = NULL;
  if (source_manager->create_source_thread(tgt,cnew,channel_id,send_par) &&
      source_manager->install_channel(channel_id,false,channel,
                                      &request_queue,owner,send_par))
    channel = NULL; // Channel successfully off loaded.
  else
    { req = request_queue.pop_head(NULL); assert(req != NULL); }
  if (channel != NULL)
    {
      send_par << "\r\nServer: Kakadu JPIP Server "
               << KDU_CORE_VERSION << "\r\n\r\n";
      kd_msg_log.print(send_par,"\t>> ");
      channel->write_block(true,send_par);
    }

  // Write some info for the log file.
  kd_msg.start_message();
  if (channel == NULL)
    {
      kd_msg << "\n\tNew channel request accepted locally\n";
      kd_msg << "\t\tAssigned channel ID: " << channel_id << "\n";
    }
  else
    {
      kd_msg << "\n\t\tNew channel request refused\n";
      kd_msg << "\t\t\tRequested target: \"" << tgt.filename << "\"";
      if (tgt.byte_range[0] != '\0')
        kd_msg << " (" << tgt.byte_range << ")";
      kd_msg << "\n";
      kd_msg << "\t\t\tRequested channel type: \""
             << req->fields.channel_new << "\"\n";
    }
  kd_msg.flush(true);
  
  return (channel == NULL);
}


/* ========================================================================= */
/*                            kd_connection_server                           */
/* ========================================================================= */

/*****************************************************************************/
/*                 kd_connection_server::~kd_connection_server               */
/*****************************************************************************/

kd_connection_server::~kd_connection_server()
{
  acquire_lock();
  max_threads = 0;
  release_lock();
  bool done = false;
  while (!done)
    {
      acquire_lock();
      done = (idle_threads == num_threads);
      release_lock();
      if (!done)
        wait_event();
    }
  kd_connection_thread *thrd;
  while ((thrd=threads) != NULL)
    {
      threads = thrd->next;
      delete thrd;
    }
  CloseHandle(h_mutex);
  CloseHandle(h_event);

  while ((pending_active_tail=pending_active_head) != NULL)
    {
      pending_active_head = pending_active_tail->next;
      if (pending_active_tail->channel != NULL)
        delete pending_active_tail->channel;
      delete pending_active_tail;
    }
}

/*****************************************************************************/
/*                  kd_connection_server::push_active_channel                */
/*****************************************************************************/

void
  kd_connection_server::push_active_channel(kd_tcp_channel *channel,
                                            kd_request_queue &queue)
{
  if (channel == NULL)
    return;

  if (!channel->is_open())
    {
      queue.init();
      delete channel;
      return;
    }

  channel->change_event(NULL); // Just in case
  channel->start_timer(0,false,NULL); // Also just in case
  kd_pending_active_channel *pending = new kd_pending_active_channel;
  pending->channel = channel;
  pending->request_queue.transfer_state(&queue);
  pending->next = NULL;
  acquire_lock();
  if (pending_active_tail == NULL)
    pending_active_head = pending_active_tail = pending;
  else
    pending_active_tail = pending_active_tail->next = pending;
  dispatch_pending_active_channels();
  release_lock();
}

/*****************************************************************************/
/*                    kd_connection_server::service_request                  */
/*****************************************************************************/

void
  kd_connection_server::service_request(SOCKET connected)
{
  kd_connection_thread *handler = NULL;

  wait_for_resources();
  acquire_lock();
  if (idle_threads == 0)
    { // Create a new thread
      assert(num_threads < max_threads);
      handler = new kd_connection_thread(this,source_manager,delegator);
      num_threads++;
      idle_threads++;
      handler->next = threads;
      threads = handler;
      handler->start();
    }
  else
    {
      for (handler=threads; handler != NULL; handler=handler->next)
        if (handler->is_idle)
          break;
      assert(handler != NULL);
    }
  handler->service_request(connected,timeout_milliseconds);
  idle_threads--;
  release_lock();
}

/*****************************************************************************/
/*          kd_connection_server::dispatch_pending_active_channels           */
/*****************************************************************************/

void
  kd_connection_server::dispatch_pending_active_channels()
{
  kd_pending_active_channel *pending;
  while (((pending = pending_active_head) != NULL) && (idle_threads > 0))
    {
      assert(pending->channel != NULL);
      kd_connection_thread *handler;
      for (handler=threads; handler != NULL; handler=handler->next)
        if (handler->is_idle)
          break;
      assert(handler != NULL);
      handler->service_request(pending->channel,pending->request_queue,
                               timeout_milliseconds);
      idle_threads--;
      pending->channel = NULL;
      if ((pending_active_head = pending->next) == NULL)
        pending_active_tail = NULL;
      delete pending;
    }
}


/* ========================================================================= */
/*                                kd_delegator                               */
/* ========================================================================= */

/*****************************************************************************/
/*                           kd_delegator::add_host                          */
/*****************************************************************************/

void
  kd_delegator::add_host(const char *hostname)
{
  kd_delegation_host *elt = new kd_delegation_host;
  elt->hostname = new char[strlen(hostname)+1];
  strcpy(elt->hostname,hostname);
  elt->load_share = 4;
  char *suffix = strrchr(elt->hostname,'*');
  if (suffix != NULL)
    {
      if ((sscanf(suffix+1,"%d",&(elt->load_share)) != 1) ||
          (elt->load_share < 1))
        {
          delete elt;
          kdu_error e;
          e << "Invalid delegate spec, \"" << hostname << "\", given "
               "on command line.  The `*' character must be followed by a "
               "positive integer load share value.";
        }
      *suffix = '\0';
    }
  elt->port = 80;
  suffix = strrchr(elt->hostname,':');
  if (suffix != NULL)
    {
      int port_val = 0;
      if (sscanf(suffix+1,"%d",&port_val) != 1)
        {
          delete elt;
          kdu_error e;
          e << "Illegal port number suffix found in "
               "delegated host address, \"" << hostname << "\".";
        }
      *suffix = '\0';
      elt->port = (kdu_uint16) port_val;
    }
  elt->hostname_with_port = new char[strlen(elt->hostname)+16]; // Plenty
  sprintf(elt->hostname_with_port,"%s:%d",elt->hostname,elt->port);
  acquire_lock();
  elt->next = head;
  elt->prev = NULL;
  if (head != NULL)
    head = head->prev = elt;
  else
    head = tail = elt;
  num_delegation_hosts++;
  release_lock();
}

/*****************************************************************************/
/*                           kd_delegator::delegate                          */
/*****************************************************************************/

const char *
  kd_delegator::delegate(const kd_request *req,
                         kd_tcp_channel *response_channel,
                         kd_message_block &par_block,
                         kd_message_block &body_block)
{
  assert(req->resource != NULL);

  // Find the source to use
  int num_connections_attempted = 0;
  int num_resolutions_attempted = 0;
  kd_delegation_host *host = NULL;
  kd_tcp_channel channel;
  while (1)
    {
      acquire_lock();
      if ((num_resolutions_attempted >= num_delegation_hosts) ||
          (num_connections_attempted > num_delegation_hosts))
        {
          release_lock();
          return NULL;
        }
      for (host = head; host != NULL; host=host->next)
        if ((!host->lookup_in_progress) && (host->load_counter > 0))
          break;
      if (host == NULL)
        { // All load counters down to 0; let's increment them all
          for (host=head; host != NULL; host=host->next)
            if (!host->lookup_in_progress)
              host->load_counter = host->load_share;
          for (host=head; host != NULL; host=host->next)
            if (!host->lookup_in_progress)
              break;
        }
      if (host == NULL)
        { // Waiting for another thread to finish resolving host address
          release_lock();
          Sleep(1000);
          continue;
        }

      // If we get here, we have a host to try.  First, move it to the tail
      // of the service list so that it will be the last host which we or
      // anybody else retries, if something goes wrong.
      if (host->prev == NULL)
        {
          assert(host == head);
          head = host->next;
        }
      else
        host->prev->next = host->next;
      if (host->next == NULL)
        {
          assert(host == tail);
          tail = host->prev;
        }
      else
        host->next->prev = host->prev;
      host->prev = tail;
      if (tail == NULL)
        {
          assert(head == NULL);
          head = tail = host;
        }
      else
        tail = tail->next = host;
      host->next = NULL;

      // Now see if we need to resolve the address
      if (host->resolution_counter > 1)
        { // The host has failed before.  Do our best to avoid it for a while.
          host->resolution_counter--;
          release_lock();
          continue;
        }
      if (host->resolution_counter == 1)
        { // Attempt to resolve host name
          host->lookup_in_progress = true;
          release_lock();
          
          int address_len = sizeof(host->address);
          memset(&(host->address),0,(size_t) address_len);
          host->address.sin_addr.S_un.S_addr = inet_addr(host->hostname);
          if (host->address.sin_addr.S_un.S_addr == INADDR_NONE)
            { // Try to convert name to IP address
              HOSTENT *hostent = gethostbyname(host->hostname);
              if (hostent != NULL)
                memcpy(&(host->address.sin_addr),hostent->h_addr_list[0],4);
            }
          acquire_lock();
          if (host->address.sin_addr.S_un.S_addr == INADDR_NONE)
            { // Cannot resolve host address
              num_resolutions_attempted++;
              host->resolution_counter = KD_DELEGATE_RESOLUTION_INTERVAL;
              host->lookup_in_progress = false;
              release_lock();
              continue;
            }
          host->address.sin_port = htons(host->port);
          host->address.sin_family = AF_INET;
          host->resolution_counter = 0;
          host->load_counter = host->load_share;
          host->lookup_in_progress = false;
        }

      // Now we are ready to use the host
      num_connections_attempted++;
      if (host->load_counter > 0) // May have changed in another thread
        host->load_counter--;

      // Copy address before releasing lock, in case it is changed elsewhere.
      SOCKADDR_IN address = host->address;
      char host_address[64];
      strcpy(host_address,inet_ntoa(address.sin_addr));

      release_lock();

      // Now try connecting to the host address
      try {
          channel.start_timer(3000,false,NULL);
                               // Allow 3 seconds for delegate connect
          channel.open(address);
        } catch(...) { }
      if (!channel.is_open())
        {
          acquire_lock();
          host->resolution_counter = KD_DELEGATE_RESOLUTION_INTERVAL;
          host->load_counter = 0;
          release_lock();
          continue;
        }
  
      // Now perform the relevant transactions.
      int code = 503; // Try back later by default
      try {
          par_block.restart();
          body_block.restart();
          req->fields.write_query(body_block);
          int body_length = body_block.get_remaining_bytes();
          par_block << "POST " << req->resource << " HTTP/1.1\r\n";
          par_block << "Host: " << host->hostname << ":"
                    << host->port << "\r\n";
          par_block << "Content-Length: " << body_length << "\r\n";
          par_block << "Content-Type: x-www-form-urlencoded\r\n";
          par_block << "Connection: close\r\n\r\n";
          channel.write_block(true,par_block);
          channel.write_block(true,body_block);

          const char *reply, *header;
        
          par_block.restart();
          if ((reply = channel.read_paragraph(true)) != NULL)
            code = process_delegate_response(reply,par_block,
                                             host_address,host->port);
          if (code == 503)
            { // This delegation host is too busy
              channel.close();
              continue;
            }

          // Forward the response incrementally
          bool more_chunks = false;
          int content_length = 0;
          if ((header =
               kd_caseless_search(reply,"\nContent-Length:")) != NULL)
            {
              while (*header == ' ') header++;
              sscanf(header,"%d",&content_length);
            }
          else if ((header =
                    kd_caseless_search(reply,"\nTransfer-encoding:")) != NULL)
            {
              while (*header == ' ') header++;
              if (kd_has_caseless_prefix(header,"chunked"))
                more_chunks = true;
            }

          response_channel->write_block(true,par_block);

          while (more_chunks || (content_length > 0))
            {
              body_block.restart();
              if (content_length <= 0)
                {
                  reply = channel.read_line(true);
                  if ((*reply == '\0') || (*reply == '\n'))
                    continue;
                  if ((sscanf(reply,"%x",&content_length) == 0) ||
                      (content_length <= 0))
                    more_chunks = false;
                  body_block.set_hex_mode(true);
                  body_block << content_length << "\r\n";
                  body_block.set_hex_mode(false);
                }
              if (content_length > 0)
                {
                  channel.read_block(true,content_length,body_block);
                  content_length = 0;
                }
              else
                body_block << "\r\n"; // End of chunked transfer
              response_channel->write_block(true,body_block);
            }
          channel.close();
          return host->hostname_with_port;
        }
      catch (...) {}
      // If we get here, we have failed to complete the transaction.
      channel.close();
      break;
    }
  return NULL;
}
