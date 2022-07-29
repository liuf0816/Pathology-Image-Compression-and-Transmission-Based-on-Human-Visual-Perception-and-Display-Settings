/*****************************************************************************/
// File: delivery.cpp [scope = CONTRIB/DSTO-CLIENT-SERVER-PORT]
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
   Implements the data delivery manager and its associated high
priority thread, together with the `transmitter' objects responsible for
delivering data-bin increments to a remote client over any of the protocols
implemented by the `kdu_server' application.
   Ported to SunOS/Linux by I. Ligertwood, as part of the "DSTO Ported Files"
described in the accompanying file, "License-to-use-DSTO-port.txt".  Refer
to that file for terms and conditions of use.
******************************************************************************/

#include <math.h>
#include "sysIF.h"
#include "server_local.h"
#include "kdu_messaging.h"


/* ========================================================================= */
/*                             kd_tcp_transmitter                            */
/* ========================================================================= */

/*****************************************************************************/
/*                  kd_tcp_transmitter::~kd_tcp_transmitter                  */
/*****************************************************************************/

kd_tcp_transmitter::~kd_tcp_transmitter()
{
  kd_queue_elt *elt;
  while ((elt=free_list) != NULL)
    {
      free_list = elt->next;
      delete elt;
    }
  while ((elt=acknowledged_head) != NULL)
    {
      acknowledged_head = elt->next;
      delete elt;
    }
  while ((elt=xmit_head) != NULL)
    {
      xmit_head = elt->next;
      delete elt;
    }
  while ((elt=holding_head) != NULL)
    {
      holding_head = elt->next;
      delete elt;
    }
}

/*****************************************************************************/
/*                 kd_tcp_transmitter::configure_flow_control                */
/*****************************************************************************/

void
  kd_tcp_transmitter::configure_flow_control(float max_rtt_target,
                                             float min_rtt_target)
{
  if (min_rtt_target > max_rtt_target)
    min_rtt_target = max_rtt_target;
  this->max_rtt_target = max_rtt_target;
  this->min_rtt_target = min_rtt_target;
  this->rtt_target = 0.5F*(max_rtt_target+min_rtt_target);

  estimated_network_delay = max_rtt_target;
  estimated_network_rate = max_bytes_per_second;
  if (estimated_network_rate > 5000)
    estimated_network_rate = 5000; // Don't start too fast.

  clock_t max_bunching = (clock_t)(SYS_CLOCKS_PER_SEC*max_rtt_target/2);
  if (max_bunching < bunching_ticks)
    bunching_ticks = max_bunching;
}

/*****************************************************************************/
/*              kd_tcp_transmitter::update_windowing_parameters              */
/*****************************************************************************/

void
  kd_tcp_transmitter::update_windowing_parameters(float interval_time,
                                                  int interval_bytes,
                                                  float acknowledge_rtt,
                                                  int acknowledge_window)
{
  avg_1   = 0.9*avg_1   + 0.1;
  avg_t   = 0.9*avg_t   + 0.1*interval_time;
  avg_b   = 0.9*avg_b   + 0.1*interval_bytes;
  avg_bb  = 0.9*avg_bb  + (0.1*interval_bytes)*interval_bytes;
  avg_tb  = 0.9*avg_tb  + 0.1*interval_time*interval_bytes;
  double determinant = avg_1*avg_bb - avg_b*avg_b;
  if ((total_rtt_events > 2) && (determinant > 0.0))
    {
      double rate_denominator = avg_tb*avg_1 - avg_b*avg_t;
      double delay_numerator = avg_t*avg_bb - avg_tb*avg_b;

      if ((rate_denominator < (0.0000001*determinant)) ||
          (delay_numerator < 0.0))
        { // Can't believe the rate would be more than 1 Mbyte/s, while
          // negative rates or delays are clearly meaningless.  In such cases
          // we ignore the affine model below, and simply assume that RTT=0
          estimated_network_delay = 0.0F;
          if (avg_t < (0.0000001*avg_b))
            estimated_network_rate = 1000000.0F;
          else
            estimated_network_rate = (float)(avg_b / avg_t);
        }
      else
        { // Use affine estimator.  The following equation finds the
          // rate, R, and delay, D, which minimize the expected squared error
          // between interval time, T, and (W/R + D), where W is the
          // number of interval bytes.
          estimated_network_rate = (float)(determinant / rate_denominator);
          estimated_network_delay = (float)(delay_numerator / determinant);
        }

      if (estimated_network_rate <= 10.0F)
        { // Can't believe the rate is less than 10 bytes/second
          estimated_network_rate = 10.0F;
          estimated_network_delay = (float)
            (avg_t - estimated_network_rate*avg_b);
          if (estimated_network_delay < 0.0F)
            estimated_network_delay = 0.0F;
        }
      rtt_target = 2.0F*estimated_network_delay;
      if (rtt_target < min_rtt_target)
        rtt_target = min_rtt_target;
      else if (rtt_target > max_rtt_target)
        rtt_target = max_rtt_target;
    }

  // Adapt nominal window threshold with the aim of achieving the target RTT.
  if (acknowledge_rtt < rtt_target)
    { // Increase the transmission window to make RTT larger
      if (acknowledge_window > nominal_window_threshold)
        nominal_window_threshold = acknowledge_window + (max_chunk_bytes>>2);
    }
  else
    { // Decrease the transmission window to make RTT smaller
      int suggested_threshold = 1 +
        (int)(acknowledge_window * rtt_target / acknowledge_rtt);
      if (suggested_threshold < nominal_window_threshold)
        nominal_window_threshold = suggested_threshold;
    }

  // Add jitter to get the actual window threshold.
  window_threshold = nominal_window_threshold + window_jitter;
  window_jitter += max_chunk_bytes;
  if (window_jitter > (max_chunk_bytes<<1))
    window_jitter = 0;
}

/*****************************************************************************/
/*                   kd_tcp_transmitter::get_suggested_bytes                 */
/*****************************************************************************/

int
  kd_tcp_transmitter::get_suggested_bytes()
{
  float duration = 1.5F*estimated_network_delay;
  if (duration < 1.0F)
    duration = 1.0F;
  if (duration > 5.0F)
    duration = 5.0F;
  float rate_limit = 2000.0F*((total_rtt_events>2)?(total_rtt_events-2):1);
  if (rate_limit > max_bytes_per_second)
    rate_limit = max_bytes_per_second;
  float byte_rate = estimated_network_rate;
  if (byte_rate > rate_limit)
    byte_rate = rate_limit;

  int suggested_bytes = (int) (duration * byte_rate);
  if (suggested_bytes > 30000)
    suggested_bytes = 30000; // A reasonable threshold
  if (suggested_bytes < 100)
    suggested_bytes = 100;
  return suggested_bytes;
}

/*****************************************************************************/
/*                kd_tcp_transmitter::retrieve_completed_chunks              */
/*****************************************************************************/

kds_chunk *
  kd_tcp_transmitter::retrieve_completed_chunks()
{
  if (acknowledged_head == NULL)
    return NULL; // Quick, unguarded check.
  kds_chunk *result = NULL;
  acquire_lock();
  kd_queue_elt *qelt = acknowledged_head;
  if (qelt != NULL)
    {
      result = qelt->chunk;
      acknowledged_head = qelt->next;
      if (acknowledged_head == NULL)
        acknowledged_tail = NULL;
      qelt->next = free_list;
      free_list = qelt;
      result->abandoned = false;
    }
  release_lock();
  return result;
}

/*****************************************************************************/
/*                       kd_tcp_transmitter::push_chunk                      */
/*****************************************************************************/

bool
  kd_tcp_transmitter::push_chunk(kds_chunk *chunk, HANDLE h_event,
                                 const char *content_type)
{
  if (channel == NULL)
    throw 0;
  assert((source_event == NULL) && (chunk->prefix_bytes == 8));
  if (chunk->num_bytes >= (1<<16))
    { kdu_error e; e << "TCP communications require chunks to be "
      "less than 2^16 bytes in length."; }
  chunk->abandoned = true; // Reverts to false once acknowledgement received
  clock_t now = sysClock() - base_time;
  if (now >= disconnect_gate)
    return false;
  acquire_lock();
  if (chunk->max_bytes > max_chunk_bytes)
    max_chunk_bytes = chunk->max_bytes;
  if (((holding_bytes+window_bytes) > window_threshold) &&
      (holding_head != NULL))
    { // We will need to block.
      push_waiting = true;
      source_event = h_event;
      release_lock();
      clock_t wait_ticks = disconnect_gate - now;
      kdu_uint32 wait_miliseconds = (wait_ticks * 1000) / SYS_CLOCKS_PER_SEC;
      sysWaitForEvent(source_event, wait_miliseconds);
      acquire_lock();
      source_event = NULL;
      push_waiting = false;
    }

  bool push_succeeded = false;
  bool was_idle = (holding_head == NULL);
  if (((holding_bytes+window_bytes) <= window_threshold) ||
      (holding_head == NULL))
    {
      kd_queue_elt *qelt = free_list;
      if (qelt == NULL)
        qelt = new kd_queue_elt;
      else
        free_list = qelt->next;
      qelt->chunk = chunk;
      qelt->next = NULL;
      if (holding_tail == NULL)
        holding_head = holding_tail = qelt;
      else
        holding_tail = holding_tail->next = qelt;
      holding_bytes += chunk->num_bytes;
      if (was_idle)
        sysSetEvent(service_event); // Wake up service thread
      push_succeeded = true;
    }
  release_lock();
  return push_succeeded;
}

/*****************************************************************************/
/*                     kd_tcp_transmitter::service_queue                     */
/*****************************************************************************/

int
  kd_tcp_transmitter::service_queue()
{
  if (channel == NULL)
    return -1;

  kdu_byte *ack;
  kd_queue_elt *qp;
  clock_t now = sysClock() - base_time; // Get adjusted reference time.

  if (idle_start != 0)
    {
      idle_ticks += (now-idle_start);
      idle_start = 0;
    }
  acquire_lock(); // Lock access to the lists
  try { // So we can unlock the mutex in the event of an exception
      // First, process any pending acknowledgement messages
      while ((ack = channel->read_raw(false,8)) != NULL)
        {
          if (xmit_head == NULL)
            { kdu_error e; e << "Client sending acknowledgements for "
              "data chunks which have not yet been transmitted!"; }
          qp = xmit_head;
          xmit_head = qp->next;
          if (xmit_head == NULL)
            xmit_tail = NULL;
          if (acknowledged_tail == NULL)
            acknowledged_tail = acknowledged_head = qp;
          else
            acknowledged_tail = acknowledged_tail->next = qp;
          qp->next = NULL;
          total_acknowledged_bytes += qp->chunk->num_bytes;
          window_bytes -= qp->chunk->num_bytes;
          total_rtt_ticks += (now - qp->transmit_time);
          total_rtt_events++;
          float interval_time = (1.0F / SYS_CLOCKS_PER_SEC) *
            (float)(1 + now - qp->interval_start_time);
          float window_time = (1.0F / SYS_CLOCKS_PER_SEC) *
            (float)(1 + now - qp->transmit_time);
          update_windowing_parameters(interval_time,qp->interval_bytes,
                                      window_time,qp->window_bytes);
        }

      // See if we can wake up application to push in more data chunks
      if (push_waiting && ((window_bytes+holding_bytes) <= window_threshold))
        sysSetEvent(source_event); // Unblock `push_chunk'

      // Now see if we can deliver a data chunk immediately.
      if ((now >= delivery_gate) && (holding_head != NULL) &&
          (window_bytes <= window_threshold))
        { // Can transmit pending chunk; transmission window not yet full
          qp = holding_head;
          assert((qp->chunk->prefix_bytes == 8) &&
                 (qp->chunk->num_bytes < (1<<16)));
          qp->chunk->data[0] = (kdu_byte)(qp->chunk->num_bytes >> 8);
          qp->chunk->data[1] = (kdu_byte)(qp->chunk->num_bytes);
          qp->chunk->data[2] = qp->chunk->data[3] = qp->chunk->data[3] =
            qp->chunk->data[4] = qp->chunk->data[5] =
              qp->chunk->data[6] = qp->chunk->data[7] = 0;
          if (!channel->write_raw(false,qp->chunk->data,qp->chunk->num_bytes))
            { // Internal transmit queue is full.  We will be woken up once
              // space becomes available.  This should rarely if ever happen,
              // since we are carefully controlling the size of the transmit
              // window anyway.
              release_lock();
              return -1;
            }

          // Successful transmission; move from holding to transmitted queue
          holding_head = qp->next;
          if (holding_head == NULL)
            holding_tail = NULL;

          // Link into the transmission queue
          qp->next = NULL;
          if (xmit_tail == NULL)
            xmit_head = xmit_tail = qp;
          else
            xmit_tail = xmit_tail->next = qp;
          holding_bytes -= qp->chunk->num_bytes;
          window_bytes += qp->chunk->num_bytes;
          total_transmitted_bytes += qp->chunk->num_bytes;

          // Record statistics for use later in adaptive window control
          qp->transmit_time = now;
          qp->window_bytes = window_bytes;
          qp->interval_start_time = xmit_head->transmit_time;
          qp->interval_bytes = window_bytes +
            xmit_head->window_bytes - xmit_head->chunk->num_bytes;

          // Determine the next delivery gate.
          delivery_gate += 1 + (clock_t)
            (qp->chunk->num_bytes / max_bytes_per_second * SYS_CLOCKS_PER_SEC);
          if ((delivery_gate+bunching_ticks) < now)
            delivery_gate = now - bunching_ticks;
        }
    }
  catch(...)
    { // Return -1 after first releasing the mutex lock.
      if (source_event != NULL)
        sysSetEvent(source_event);
      channel->close();
      delete channel;
      channel = NULL;
      release_lock();
      return -1;
    }

  // Determine waiting parameters
  int return_value;
  if (window_bytes > window_threshold)
    return_value = -1; // Must wait for an acknowledgement event
  else if (holding_head == NULL)
    return_value = -1; // Wait for acknowledgement or a new push event
  else
    { // Must wait for delivery gate to be reached
      if (delivery_gate <= now)
        return_value = 0;
      else
        return_value = (int)
          (((float)(delivery_gate-now)) * (1000.0F/SYS_CLOCKS_PER_SEC));
    }
  if ((holding_head == NULL) && (xmit_head == NULL))
    idle_start = now;

  release_lock();
  return return_value;
}


/* ========================================================================= */
/*                            kd_http_transmitter                            */
/* ========================================================================= */

/*****************************************************************************/
/*                 kd_http_transmitter::~kd_http_transmitter                 */
/*****************************************************************************/

kd_http_transmitter::~kd_http_transmitter()
{
  kd_queue_elt *elt;
  while ((elt=free_list) != NULL)
    {
      free_list = elt->next;
      delete elt;
    }
  while ((elt=holding_head) != NULL)
    {
      holding_head = elt->next;
      delete elt;
    }
  while ((elt=completed_head) != NULL)
    {
      completed_head = elt->next;
      delete elt;
    }
}

/*****************************************************************************/
/*                  kd_http_transmitter::get_suggested_bytes                 */
/*****************************************************************************/

int
  kd_http_transmitter::get_suggested_bytes()
{
  int suggested_bytes = (int)(max_bytes_per_second * 2.0F);
  if (suggested_bytes > 30000)
    suggested_bytes = 30000; // A reasonable threshold
  return suggested_bytes;
}

/*****************************************************************************/
/*               kd_http_transmitter::retrieve_completed_chunks              */
/*****************************************************************************/

kds_chunk *
  kd_http_transmitter::retrieve_completed_chunks()
{
  if (completed_head == NULL)
    return NULL; // Quick, unguarded check.
  kds_chunk *result = NULL;
  acquire_lock();
  while (completed_head != NULL)
    {
      kd_queue_elt *qelt = completed_head;
      result = qelt->chunk;
      completed_head = qelt->next;
      if (completed_head == NULL)
        completed_tail = NULL;
      qelt->next = free_list;
      free_list = qelt;
      if (result != NULL)
        {
          result->abandoned = false;
          break;
        }
    }
  release_lock();
  return result;
}

/*****************************************************************************/
/*                   kd_http_transmitter::retrieve_requests                  */
/*****************************************************************************/

bool
  kd_http_transmitter::retrieve_requests(kd_request_queue *queue,
                                         HANDLE h_event)
{
  if (channel == NULL)
    throw 0; // Connection has failed.
  assert((source_event == NULL) || (source_event == h_event));
  source_event = h_event;
  if (!have_requests)
    return false; // quick, unguarded check.
  bool result = false;
  acquire_lock();
  if (have_requests)
    {
      have_requests = false;
      result = true;
      const kd_request *req;
      while ((req=requests.pop_head(NULL)) != NULL)
        queue->push_copy(req);
    }
  release_lock();
  return result;
}

/*****************************************************************************/
/*                      kd_http_transmitter::push_chunk                      */
/*****************************************************************************/

bool
  kd_http_transmitter::push_chunk(kds_chunk *chunk, HANDLE h_event,
                                  const char *content_type)
{
  if (channel == NULL)
    throw 0; // Connection has failed.
  assert(((source_event == NULL) || (source_event == h_event)) &&
         (chunk->prefix_bytes == 0));
  chunk->abandoned = true; // Becomes false once chunk actually transmitted
  clock_t now = sysClock() - base_time;
  if (now >= disconnect_gate)
    return false;
  acquire_lock();
  if ((holding_bytes > max_holding_bytes) && !waiting_for_chunk)
    { // We will need to block.
      push_waiting = true;
      source_event = h_event;
      release_lock();
      clock_t wait_ticks = disconnect_gate - now;
      kdu_uint32 wait_miliseconds = (wait_ticks * 1000) / SYS_CLOCKS_PER_SEC;
      sysWaitForEvent(source_event, wait_miliseconds);
      acquire_lock();
      source_event = NULL;
      push_waiting = false;
    }

  bool push_succeeded = false;
  bool was_idle = ((holding_head == NULL) ||
                   ((holding_head == holding_tail) && waiting_for_chunk));
  if ((holding_bytes <= max_holding_bytes) || waiting_for_chunk)
    {
      kd_queue_elt *qelt = holding_tail;
      if ((qelt == NULL) || (qelt->chunk != NULL) || qelt->started_bytes)
        { // Need a new queue element.
          assert(!waiting_for_chunk);
          qelt = get_new_holding_element();
        }
      waiting_for_chunk = false;
      qelt->chunk = chunk;
      qelt->content_type = content_type;
      qelt->chunk_bytes = chunk->num_bytes;
      need_chunk_trailer = true;
      holding_bytes += qelt->chunk_bytes;
      if (was_idle)
        sysSetEvent(service_event); // Wake up service thread
      push_succeeded = true;
    }
  release_lock();
  return push_succeeded;
}

/*****************************************************************************/
/*                     kd_http_transmitter::push_replies                     */
/*****************************************************************************/

bool
  kd_http_transmitter::push_replies(kd_message_block &block, HANDLE h_event,
                                    bool wait_for_chunk)
{
  if (channel == NULL)
    throw 0; // Connection has failed.
  assert((source_event == NULL) || (source_event == h_event));
  clock_t now = sysClock() - base_time;
  if (now >= disconnect_gate)
    return false;
  acquire_lock();
  if (holding_bytes > max_holding_bytes)
    { // We will need to block.
      push_waiting = true;
      source_event = h_event;
      release_lock();
      clock_t wait_ticks = disconnect_gate - now;
      kdu_uint32 wait_miliseconds = (wait_ticks * 1000) / SYS_CLOCKS_PER_SEC;
      sysWaitForEvent(source_event, wait_miliseconds);
      acquire_lock();
      source_event = NULL;
      push_waiting = false;
    }

  bool push_succeeded = false;
  bool was_idle = (holding_head == NULL);
  assert(!waiting_for_chunk);
  if (holding_bytes <= max_holding_bytes)
    {
      kd_queue_elt *qelt = holding_tail;
      if ((qelt == NULL) || (qelt->chunk != NULL) || qelt->started_bytes)
        qelt = get_new_holding_element();
      waiting_for_chunk = wait_for_chunk;
      if (need_chunk_trailer)
        {
          assert(qelt->reply_bytes == 0);
          need_chunk_trailer = false;
          qelt->reply << "0\r\n\r\n"; // Write the chunk trailer.
        }
      qelt->reply.write_raw(block.peek_block(),block.get_remaining_bytes());
      int new_reply_bytes = qelt->reply.get_remaining_bytes();
      holding_bytes += new_reply_bytes - qelt->reply_bytes;
      qelt->reply_bytes = new_reply_bytes;
      if (was_idle && !wait_for_chunk) // Don't service until complete
        sysSetEvent(service_event); // Wake up service thread
      push_succeeded = true;
    }
  release_lock();
  return push_succeeded;
}

/*****************************************************************************/
/*                         kd_http_transmitter::close                        */
/*****************************************************************************/

kd_tcp_channel *
  kd_http_transmitter::close(kd_request_queue *queue, HANDLE h_event)
{
  assert(!waiting_for_chunk);
  close_requested = true;
  assert((source_event == NULL) || (source_event == h_event));
  acquire_lock();
  if (channel != NULL)
    {
      clock_t now = sysClock() - base_time;
      if (now < disconnect_gate)
        {
          source_event = h_event;
          if (holding_head == NULL)
            sysSetEvent(service_event); // Wake up service thread
          release_lock();
          clock_t wait_ticks = disconnect_gate - now;
          kdu_uint32 wait_miliseconds = (wait_ticks * 1000) / SYS_CLOCKS_PER_SEC;
          sysWaitForEvent(source_event, wait_miliseconds);
          acquire_lock();
          source_event = NULL;
          now = sysClock() - base_time;
        }
      if ((now >= disconnect_gate) && (channel != NULL))
        {
          closed_channel = channel;
          closed_channel->change_event(NULL);
          channel = NULL;
        }
    }
  release_lock();
  kd_tcp_channel *result = closed_channel;
  if (result != NULL)
    {
      assert(channel == NULL);
      if (holding_head != NULL)
        throw 0; // Channel disconnected unexpectedly.
      if (queue == NULL)
        requests.init();
      else
        queue->transfer_state(&requests);
      closed_channel = NULL;
    }
  return result;
}

/*****************************************************************************/
/*                         kd_http_transmitter::reopen                       */
/*****************************************************************************/

void
  kd_http_transmitter::reopen(kd_tcp_channel *Channel,
                              kd_request_queue *transfer_queue)
{
  assert(this->channel == NULL);
  if (this->closed_channel != NULL)
    throw 0;
  this->requests.init();
  if (transfer_queue != NULL)
    this->requests.transfer_state(transfer_queue);
  close_requested = false;
  this->channel = Channel;
  channel->change_event(service_event);
}

/*****************************************************************************/
/*               kd_http_transmitter::terminate_chunked_data                 */
/*****************************************************************************/

void
  kd_http_transmitter::terminate_chunked_data()
{
  if (!need_chunk_trailer)
    return;
  acquire_lock();
  bool was_idle = (holding_head == NULL);
  kd_queue_elt *qelt = holding_tail;
  assert(!waiting_for_chunk);
  assert((qelt == NULL) || (qelt->chunk != NULL));
  qelt = get_new_holding_element();
  qelt->reply << "0\r\n\r\n"; // Write the chunk trailer.
  need_chunk_trailer = false;
  qelt->reply_bytes = qelt->reply.get_remaining_bytes();
  holding_bytes += qelt->reply_bytes;
  if (was_idle)
    sysSetEvent(service_event); // Wake up service thread
  release_lock();
}

/*****************************************************************************/
/*                     kd_http_transmitter::service_queue                    */
/*****************************************************************************/

int
  kd_http_transmitter::service_queue()
{
  if (channel == NULL)
    return -1;
  clock_t now = sysClock() - base_time; // Get adjusted reference time.

  if (idle_start != 0)
    {
      idle_ticks += (now-idle_start);
      idle_start = 0;
    }
  acquire_lock(); // Lock access to the lists
  bool need_source_wakeup = false;
  try { // So we can unlock the mutex in the event of an exception
      
      if (close_requested && (holding_head == NULL))
        throw 0; // Forces channel closure

      // See if we can receive something
      if (requests.read_request(false,channel))
        {
          if (!have_requests)
            {
              have_requests = true;
              if (source_event != NULL)
                need_source_wakeup = true;
            }
        }
  
      // See if we can transmit something.
      kd_queue_elt *qp = holding_head;
      if ((qp != NULL) && (now >= delivery_gate) &&
          ((!waiting_for_chunk) || (qp != holding_tail)))
        {
          if (!qp->started_bytes)
            { // First time working with this queue element
              if (qp->reply_bytes && (qp->chunk_bytes > 0))
                { // Add in the transfer-coding and content-type headers
                  qp->reply.backspace(2);
                  qp->reply << "Transfer-Encoding: chunked\r\n";
                  if (qp->content_type != NULL)
                    qp->reply << "Content-Type: "
                              << qp->content_type << "\r\n\r\n";
                }
              if (qp->chunk_bytes > 0)
                { // Write the chunk header
                  assert(qp->chunk->prefix_bytes == 0);
                  qp->reply.set_hex_mode(true);
                  qp->reply << qp->chunk_bytes << "\r\n";
                  qp->reply.set_hex_mode(false);
                }
              int new_reply_bytes = qp->reply.get_remaining_bytes();
              assert(new_reply_bytes >= qp->reply_bytes);
              holding_bytes += new_reply_bytes - qp->reply_bytes;
              qp->reply_bytes = new_reply_bytes;
              qp->started_bytes = qp->reply_bytes + qp->chunk_bytes;
              kd_msg_log.print(qp->reply,"\t>> ");
            }
          do {
            if (qp->reply_bytes > 0)
              {
                if (!channel->write_block(false,qp->reply))
                  { // Internal TCP send queue is full.  We will be woken up
                    // once space becomes available.
                    if (need_source_wakeup)
                      sysSetEvent(source_event);
                    release_lock();
                    return -1;
                  }
                qp->reply.restart();
                holding_bytes -= qp->reply_bytes;
                qp->reply_bytes = 0;
              }
            if (qp->chunk_bytes > 0)
              { // Use chunked transfer encoding for the data
                if (!channel->write_raw(false,qp->chunk->data,qp->chunk_bytes))
                  { // Internal TCP send queue is full.  We will be woken up
                    // once space becomes available.
                    if (need_source_wakeup)
                      sysSetEvent(source_event);
                    release_lock();
                    return -1;
                  }
                holding_bytes -= qp->chunk_bytes;
                qp->chunk_bytes = 0;
                qp->reply << "\r\n"; // Terminate chunk with CRLF.
                qp->reply_bytes = qp->reply.get_remaining_bytes();
                qp->started_bytes += qp->reply_bytes;
                holding_bytes += qp->reply_bytes;
              }
            } while (qp->reply_bytes > 0);

          // Successful transmission; move from holding to completed queue
          holding_head = qp->next;
          if (holding_head == NULL)
            holding_tail = NULL;
          qp->next = NULL;
          if (completed_tail == NULL)
            completed_head = completed_tail = qp;
          else
            completed_tail = completed_tail->next = qp;

          // Adjust statistics
          total_transmitted_bytes += qp->started_bytes;

          // Determine the next delivery gate.
          delivery_gate += 1 + (clock_t)
            (qp->started_bytes / max_bytes_per_second * SYS_CLOCKS_PER_SEC);
          if ((delivery_gate+bunching_ticks) < now)
            delivery_gate = now - bunching_ticks;

          // See if we need to wake up a blocked application thread
          if (push_waiting && (holding_bytes <= max_holding_bytes))
            need_source_wakeup = true;
          if (close_requested && (holding_head == NULL))
            throw 0; // Forces channel closure
        }
      if (need_source_wakeup)
        sysSetEvent(source_event);
    }
  catch(...)
    { // Close the transmitter, at least for now, releasing the mutex lock.
      if (source_event != NULL)
        sysSetEvent(source_event);
      closed_channel = channel;
      channel = NULL;
      closed_channel->change_event(NULL);
      release_lock();
      return -1;
    }

  int return_value;
  if ((holding_head == NULL) ||
      ((holding_head == holding_tail) && waiting_for_chunk))
    return_value = -1; // Wait for new request or a new push event
  else
    { // Must wait for delivery gate to be reached
      if (delivery_gate <= now)
        return_value = 0;
      else
        return_value = (int)
          (((float)(delivery_gate-now)) * (1000.0F/SYS_CLOCKS_PER_SEC));
    }
  if (holding_head == NULL)
    idle_start = now;
  release_lock();
  return return_value;
}


/* ========================================================================= */
/*                            kd_delivery_manager                            */
/* ========================================================================= */

extern "C" {
  static void *kd_delivery_manager_start_proc(void *param)
  {
    kd_delivery_manager *obj = (kd_delivery_manager *) param;
    obj->thread_entry();
    return NULL;
  }
}

/*****************************************************************************/
/*                 kd_delivery_manager::~kd_delivery_manager                 */
/*****************************************************************************/

kd_delivery_manager::~kd_delivery_manager()
{
  if (h_thread != NULL)
    { // Close down the thread.
      thread_closure_requested = true;
      sysSetEvent(h_event);
      sysWaitForThreadEnd(h_thread);
    }
  assert((num_transmitters == 0) && (transmitter_list == NULL));
  sysDestroyEvent(&h_event);
  sysDestroyMutex(&h_mutex);
  sysDestroyThread(&h_thread);
}

/*****************************************************************************/
/*                         kd_delivery_manager::start                        */
/*****************************************************************************/

void
  kd_delivery_manager::start()
{
  h_thread = sysStartThread(kd_delivery_manager_start_proc, this, TRUE);
}

/*****************************************************************************/
/*                    kd_delivery_manager::thread_entry                      */
/*****************************************************************************/

void
  kd_delivery_manager::thread_entry()
{
  do {
      int wait_miliseconds = -1;
      kd_transmitter_list *scan;

      sysResetEvent(h_event);
      acquire_lock();
      for (scan=transmitter_list; scan != NULL; scan=scan->next)
        if (scan->transmitter->is_open())
          {
            int miliseconds = scan->transmitter->service_queue();
            if ((miliseconds >= 0) &&
                ((wait_miliseconds < 0) || (miliseconds < wait_miliseconds)))
              wait_miliseconds = miliseconds;
          }
      release_lock();
      if (wait_miliseconds < 0)
        { // Wait until service event is signalled
          sysWaitForEvent(h_event, INFINITE);
        }
      else if (wait_miliseconds > 0)
        {
          //sysWaitForEvent(h_event, (wait_miliseconds+10));
          sysWaitForEvent(h_event, wait_miliseconds);
        }
    } while (!thread_closure_requested);
}

/*****************************************************************************/
/*                 kd_delivery_manager::get_tcp_transmitter                  */
/*****************************************************************************/

kd_tcp_transmitter *
  kd_delivery_manager::get_tcp_transmitter(kd_tcp_channel *channel)
{
  kd_transmitter_list *tmp;
  tmp = new kd_transmitter_list;
  kd_tcp_transmitter *transmitter = new kd_tcp_transmitter;
  tmp->transmitter = transmitter;
  transmitter->configure(channel,h_event,max_connection_seconds,
                         max_bytes_per_second);
  transmitter->configure_flow_control(max_rtt_target_seconds,
                                      min_rtt_target_seconds);
  acquire_lock();
  tmp->next = transmitter_list;
  transmitter_list = tmp;
  num_transmitters++;
  sysSetEvent(h_event); // Wake up delivery thread.
  release_lock();
  return transmitter;
}

/*****************************************************************************/
/*                kd_delivery_manager::get_http_transmitter                  */
/*****************************************************************************/

kd_http_transmitter *
  kd_delivery_manager::get_http_transmitter(kd_tcp_channel *channel,
                                            kd_request_queue *transfer_queue)
{
  kd_transmitter_list *tmp;
  tmp = new kd_transmitter_list;
  kd_http_transmitter *transmitter = new kd_http_transmitter;
  tmp->transmitter = transmitter;
  transmitter->configure(channel,h_event,max_connection_seconds,
                         max_bytes_per_second);
  if (transfer_queue != NULL)
    transmitter->transfer_request_state(transfer_queue);
  acquire_lock();
  tmp->next = transmitter_list;
  transmitter_list = tmp;
  num_transmitters++;
  sysSetEvent(h_event); // Wake up delivery thread.
  release_lock();
  return transmitter;
}

/*****************************************************************************/
/*                 kd_delivery_manager::release_transmitter                  */
/*****************************************************************************/

void
  kd_delivery_manager::release_transmitter(kd_transmitter *obj)
{
  kd_transmitter_list *scan, *prev;

  acquire_lock();
  for (prev=NULL, scan=transmitter_list;
       scan != NULL;
       prev=scan, scan=scan->next)
    if (scan->transmitter == obj)
      break;
  if (scan != NULL)
    {
      if (prev == NULL)
        transmitter_list = scan->next;
      else
        prev->next = scan->next;
      delete scan;
      num_transmitters--;
    }
  else
    assert(0);
  release_lock();
  delete obj;
}
