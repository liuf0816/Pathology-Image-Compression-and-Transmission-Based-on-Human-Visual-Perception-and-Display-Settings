/*****************************************************************************/
// File: client_server.cpp [scope = APPS/CLIENT-SERVER]
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
  Implements objects which are common to both the client and server components
of a JPIP deployment, having no platform dependence.
******************************************************************************/
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "kdu_client_window.h"


/*****************************************************************************/
/* STATIC                    combine_sampled_ranges                          */
/*****************************************************************************/

static void
  combine_sampled_ranges(kdu_sampled_range &dst, kdu_sampled_range &src)
  /* Donates as many elements of the `src' range as possible to the `dst'
     range, leaving `dst' as large as possible.  In the process the `src'
     range could become empty, in which case `src.from' will be greater than
     `src.to'.  Each range is guaranteed to have the property that its
     `to' members is included in the range. */
{
  if (src.from > src.to)
    return;
  if ((src.context_type != 0) || (dst.context_type != 0))
    {
      if ((src.context_type != dst.context_type) ||
          (src.remapping_ids[0] != dst.remapping_ids[0]) ||
          (src.remapping_ids[1] != dst.remapping_ids[1]))
        return;
    }

  int step=dst.step;
  bool common_sampling =
    (src.step == step) && (((src.from-dst.from) % step) == 0);

  // Remove as much from the start of the `src' range as possible
  if (common_sampling)
    {
      if (src.from < dst.from)
        {
          if (src.to >= (dst.from-step))
            {
              dst.from = src.from;
              src.from = dst.to + step;
            }
        }
      else if (src.from <= dst.to)
        src.from = dst.to + step;
    }
  else
    { // Consider removing at most the first element of the `src' range
      if (src.from == (dst.from-step))
        dst.from -= step;
      else if (src.from == (dst.to+step))
        dst.to += step;
      if ((src.from >= dst.from) && (src.from <= dst.to) &&
          (((src.from - dst.from) % step) == 0))
        src.from += src.step;
    }
  if (src.from > src.to)
    return;

  // Remove as much from the end of the `src' range as possible
  if (common_sampling)
    {
      if (src.to > dst.to)
        {
          if (src.from <= (dst.to+step))
            {
              dst.to = src.to;
              src.to = dst.from - step;
            }
        }
      else if (src.to >= dst.from)
        src.to = dst.from - step;
    }
  else // Consider removing at most the last element of the `src' range
    {
      if (src.to == (dst.from-step))
        dst.from -= step;
      else if (src.to == (dst.to+step))
        dst.to += step;
      if ((src.to >= dst.from) && (src.to <= dst.to) &&
          (((src.to - dst.from) % step) == 0))
        src.to -= src.step;
    }
}


/* ========================================================================= */
/*                                kdu_range_set                              */
/* ========================================================================= */

/*****************************************************************************/
/*                       kdu_range_set::~kdu_range_set                       */
/*****************************************************************************/

kdu_range_set::~kdu_range_set()
{
  if (ranges != NULL)
    delete[] ranges;
}

/*****************************************************************************/
/*                          kdu_range_set::copy_from                         */
/*****************************************************************************/

void
  kdu_range_set::copy_from(kdu_range_set &src)
{
  init();
  if (src.num_ranges > max_ranges)
    {
      max_ranges = src.num_ranges;
      if (ranges != NULL)
        delete[] ranges;
      ranges = new kdu_sampled_range[max_ranges];
    }
  for (; num_ranges < src.num_ranges; num_ranges++)
    {
      ranges[num_ranges] = src.ranges[num_ranges];
      ranges[num_ranges].expansion = NULL;
    }
}

/*****************************************************************************/
/*                           kdu_range_set::contains                         */
/*****************************************************************************/

bool
  kdu_range_set::contains(kdu_range_set &rhs)
{
  int n, k;
  kdu_sampled_range src, dst;
  for (n=0; n < rhs.num_ranges; n++)
    {
      src = rhs.ranges[n];
      if (src.context_type == KDU_JPIP_CONTEXT_TRANSLATED)
        continue; // No need to include translated ranges
      for (k=0; k < num_ranges; k++)
        {
          dst = ranges[k];
          if ((dst.context_type != 0) || (src.context_type != 0))
            { // Check compatibility of contexts
              if ((dst.context_type != src.context_type) ||
                  (dst.remapping_ids[0] != src.remapping_ids[0]) ||
                  (dst.remapping_ids[1] != src.remapping_ids[1]))
                continue;
            }
          int step = dst.step;
          bool common_sampling =
            (src.step == step) && (((src.from-dst.from) % step) == 0);

          // Remove as much from the start of the `src' range as possible
          bool wrap_around=false;
          if ((src.from >= dst.from) && (src.from <= dst.to))
            {
              int new_from=src.from;
              if (common_sampling)
                new_from = dst.to + step;
              else if (((src.from - dst.from) % step) == 0)
                new_from += src.step;
              if (new_from < src.from)
                wrap_around = true;
              src.from = new_from;
            }

          // Remove as much from the end of the `src' range as possible
          if ((src.to >= dst.from) && (src.to <= dst.to))
            {
              int new_to=src.to;
              if (common_sampling)
                new_to = dst.from - step;
              else if (((src.to - dst.from) % step) == 0)
                new_to -= src.step;
              if (new_to > src.to)
                wrap_around = true;
              src.to = new_to;
            }

          if (wrap_around || !src)
            break;
        }
      if (k == num_ranges)
        return false;
    }
  return true;
}

/*****************************************************************************/
/*                             kdu_range_set::add                            */
/*****************************************************************************/

void
  kdu_range_set::add(kdu_sampled_range range, bool allow_merging)
{
  int n;
  if ((range.step <= 0) || (range.from > range.to))
    return;
  range.expansion = NULL;

  // Adjust `to' to make sure it is included in the range -- simplifies merging
  int kval = (range.to - range.from) / range.step;
  range.to = range.from + kval * range.step;

  if (max_ranges < (num_ranges+1))
    {
      max_ranges += 10;
      kdu_sampled_range *new_ranges = new kdu_sampled_range[max_ranges];
      for (n=0; n < num_ranges; n++)
        new_ranges[n] = ranges[n];
      if (ranges != NULL)
        delete[] ranges;
      ranges = new_ranges;
    }

  if (allow_merging)
    { // Merge as much as possible of the new range into the existing list
      for (n=0; (n < num_ranges) && (range.from <= range.to); n++)
        combine_sampled_ranges(ranges[n],range);
    }

  // Add what remains of the new range into the list
  if (range.from <= range.to)
    ranges[num_ranges++] = range;

  if (!allow_merging)
    return;

  // Bubble sort the ranges
  bool done=false;
  while (!done)
    {
      done = true;
      for (n=1; n < num_ranges; n++)
        if (ranges[n-1].from > ranges[n].from)
          {
            range = ranges[n];
            ranges[n] = ranges[n-1];
            ranges[n-1] = range;
            done = false;
          }
    }

  // See if some more merging can be done (don't try to completely solve the
  // range merging problem, though).
  for (n=1; n < num_ranges; n++)
    if (ranges[n-1].to >= (ranges[n].from - ranges[n].step))
      {
        combine_sampled_ranges(ranges[n-1],ranges[n]);
        if (ranges[n].is_empty())
          {
            num_ranges--;
            for (int k=n; k < num_ranges; k++)
              ranges[k] = ranges[k+1];
            n--;
          }
      }
}

/*****************************************************************************/
/*                            kdu_range_set::expand                          */
/*****************************************************************************/

int
  kdu_range_set::expand(int *buf, int min_idx, int max_idx)
{
  int count = 0;
  for (int n=0; n < num_ranges; n++)
    {
      kdu_sampled_range *rg = ranges + n;
      int from = rg->from;
      if (from < min_idx)
        {
          from += (min_idx-from)/rg->step;
          if (from < min_idx)
            from += rg->step;
          assert(from >= min_idx);
        }
      for (int c=from; (c <= rg->to) && (c <= max_idx); c+=rg->step)
        {
          int i;
          for (i=0; i < count; i++)
            if (buf[i] == c)
              break;
          if (i == count)
            buf[count++] = c;
        }
    }
  return count;
}


/* ========================================================================= */
/*                                 kdu_window                                */
/* ========================================================================= */

/*****************************************************************************/
/*                           kdu_window::kdu_window                          */
/*****************************************************************************/

kdu_window::kdu_window()
{
  expansions = last_used_expansion = NULL;
  metareq = free_metareqs = NULL;
  have_metareq_all = have_metareq_global =
    have_metareq_stream = have_metareq_window = false;
  init();
}

/*****************************************************************************/
/*                          kdu_window::~kdu_window                          */
/*****************************************************************************/

kdu_window::~kdu_window()
{
  init_metareq();
  assert(metareq == NULL);
  while ((metareq=free_metareqs) != NULL)
    {
      free_metareqs = metareq->next;
      delete metareq;
    }
  while ((last_used_expansion=expansions) != NULL)
    {
      expansions = last_used_expansion->next;
      delete last_used_expansion;
    }
}

/*****************************************************************************/
/*                              kdu_window::init                             */
/*****************************************************************************/

void
  kdu_window::init()
{
  resolution = region.pos = region.size = kdu_coords(0,0);
  max_layers = 0; round_direction = -1;
  components.init(); codestreams.init(); contexts.init();
  last_used_expansion = NULL;
  init_metareq();
}

/*****************************************************************************/
/*                           kdu_window::copy_from                           */
/*****************************************************************************/

void
  kdu_window::copy_from(kdu_window &src, bool copy_expansions)
{
  region = src.region;
  resolution = src.resolution;
  round_direction = src.round_direction;
  max_layers = src.max_layers;
  components.copy_from(src.components);
  codestreams.copy_from(src.codestreams);
  contexts.copy_from(src.contexts);
  if (copy_expansions)
    {
      int n, num_ranges = src.contexts.get_num_ranges();
      assert(num_ranges == contexts.get_num_ranges());
      for (n=0; n < num_ranges; n++)
        {
          kdu_range_set *src_set = src.contexts.access_range(n)->expansion;
          if (src_set != NULL)
            {
              kdu_range_set *dst_set = create_context_expansion(n);
              dst_set->copy_from(*src_set);
            }
        }
    }

  init_metareq();
  metadata_only = src.metadata_only;
  for (kdu_metareq *req=src.metareq; req != NULL; req=req->next)
    add_metareq(req->box_type,req->qualifier,req->priority,
                req->byte_limit,req->recurse,req->root_bin_id,req->max_depth);
}

/*****************************************************************************/
/*                             kdu_window::equals                            */
/*****************************************************************************/

bool
  kdu_window::equals(kdu_window &rhs)
{
  if ((rhs.max_layers != max_layers) ||
      (rhs.resolution != resolution) ||
      (rhs.round_direction != round_direction) ||
      (rhs.region != region) ||
      (rhs.components != components) ||
      (rhs.codestreams != codestreams) ||
      (rhs.contexts != contexts) ||
      (rhs.metadata_only != metadata_only))
    return false;
  
  kdu_metareq *rq1, *rq2;
  for (rq1=metareq; rq1 != NULL; rq1=rq1->next)
    {
      for (rq2=rhs.metareq; rq2 != NULL; rq2=rq2->next)
        if (*rq2 == *rq1)
          break;
      if (rq2 == NULL)
        return false;
    }
  for (rq1=rhs.metareq; rq1 != NULL; rq1=rq1->next)
    {
      for (rq2=metareq; rq2 != NULL; rq2=rq2->next)
        if (*rq2 == *rq1)
          break;
      if (rq2 == NULL)
        return false;
    }

  return true;
}

/*****************************************************************************/
/*                            kdu_window::contains                           */
/*****************************************************************************/

bool
  kdu_window::contains(kdu_window &rhs)
{
  if (max_layers != 0)
    {
      if ((rhs.max_layers == 0) || (max_layers < rhs.max_layers))
        return false;
    }
  
  if (metadata_only && !rhs.metadata_only)
    return false;

  if (!components.is_empty())
    { // Otherwise, all components are included in current window
      if (rhs.components.is_empty() || !components.contains(rhs.components))
        return false;
    }

  if (rhs.codestreams.is_empty() && !codestreams.is_empty())
    return false; // Conservative assumption that the default codestream
                  // is not contained inside any specific range of
                  // codestreams.
  if (!codestreams.contains(rhs.codestreams))
    return false;

  if (!contexts.contains(rhs.contexts))
    return false;
  
  if ((rhs.resolution.x > resolution.x) || (rhs.resolution.y > resolution.y) ||
      (rhs.round_direction > round_direction))
    return false;

  kdu_coords min = region.pos;
  kdu_coords max = min + region.size - kdu_coords(1,1);
  kdu_coords rhs_min = rhs.region.pos;
  kdu_coords rhs_max = rhs_min + rhs.region.size - kdu_coords(1,1);
  if (((double) min.x)*((double) rhs.resolution.x) >
      ((double) rhs_min.x)*((double) resolution.x))
    return false;
  if (((double) min.y)*((double) rhs.resolution.y) >
      ((double) rhs_min.y)*((double) resolution.y))
    return false;
  if (((double) max.x)*((double) rhs.resolution.x) <
      ((double) rhs_max.x)*((double) resolution.x))
    return false;
  if (((double) max.y)*((double) rhs.resolution.y) <
      ((double) rhs_max.y)*((double) resolution.y))
    return false;

  if ((metareq == NULL) && (rhs.metareq == NULL))
    return true;
  if ((metareq == NULL) || (rhs.metareq == NULL))
    return false;
  kdu_metareq *rq1, *rq2;
  for (rq1=rhs.metareq; rq1 != NULL; rq1=rq1->next)
    {
      for (rq2=metareq; rq2 != NULL; rq2=rq2->next)
        if (*rq2 == *rq1)
          break;
      if (rq2 == NULL)
        return false;
    }

  return true;
}

/*****************************************************************************/
/*                  kdu_window::create_context_expansion                     */
/*****************************************************************************/

kdu_range_set *
  kdu_window::create_context_expansion(int which)
{
  kdu_sampled_range *range = contexts.access_range(which);
  if (range == NULL)
    return NULL;
  if (range->expansion != NULL)
    return range->expansion;

  if (last_used_expansion != NULL)
    { // Check to see if `contexts' has been reset since we last updated the
      // `last_used_expansion' member
      int n=0;
      kdu_sampled_range *rscan;
      while ((rscan = contexts.access_range(n++)) != NULL)
        if (rscan->expansion != NULL)
          break;
      if (rscan == NULL)
        last_used_expansion = NULL; // Not using any expansions after all
    }

  if (expansions == NULL)
    expansions = new kdu_range_set;
  else if ((last_used_expansion != NULL) &&
           (last_used_expansion->next == NULL))
    last_used_expansion->next = new kdu_range_set;
  if (last_used_expansion == NULL)
    last_used_expansion = expansions;
  else
    last_used_expansion = last_used_expansion->next;
  range->expansion = last_used_expansion;
  range->expansion->init();
  return range->expansion;
}

/*****************************************************************************/
/*                        kdu_window::parse_context                          */
/*****************************************************************************/

const char *
  kdu_window::parse_context(const char *scan)
{
  kdu_sampled_range range;
  if ((scan[0] == 'j') && (scan[1] == 'p') &&
      (scan[2] == 'x') && (scan[3] == 'l'))
    range.context_type = KDU_JPIP_CONTEXT_JPXL;
  else if ((scan[0] == 'm') && (scan[1] == 'j') &&
           (scan[2] == '2') && (scan[3] == 't'))
    range.context_type = KDU_JPIP_CONTEXT_MJ2T;
  else
    { // Skip unrecognized expression
      while (isalnum(*scan) || (*scan == '<') || (*scan == '>') ||
             (*scan == '[') || (*scan == ']') || (*scan == '_') ||
             (*scan == '-') || (*scan == '+') || (*scan == ':') ||
             (*scan == '/') || (*scan == '.'))
        scan++;
      return scan;
    }
  if (scan[4] != '<')
    return scan;
  const char *open_range=scan;
  scan += 5;

  char *end_cp;
  range.from = range.to = strtol(scan,&end_cp,10);
  if (end_cp == scan)
    return scan-1;
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
      if (end_cp == scan)
        return scan-1;
      scan = end_cp;
    }

  assert((range.remapping_ids[0] < 0) && (range.remapping_ids[1] < 0));

  if ((range.context_type == KDU_JPIP_CONTEXT_MJ2T) &&
      (scan[0]=='+') && (scan[1]=='n') && (scan[2]=='o') && (scan[3]=='w'))
    {
      range.remapping_ids[1] = 0;
      scan += 4;
    }
  if (*scan != '>')
    return open_range;
  scan++;

  if (*scan == '[')
    {
      open_range = scan;
      scan++;
      if (range.context_type == KDU_JPIP_CONTEXT_JPXL)
        {
          if (*scan != 's')
            return open_range;
          scan++;
          range.remapping_ids[0] = strtol(scan,&end_cp,10);
          if (end_cp == scan)
            return open_range;
          scan = end_cp;
          if (*scan != 'i')
            return scan;
          scan++;
          range.remapping_ids[1] = strtol(scan,&end_cp,10);
          if (end_cp == scan)
            return open_range;
          scan = end_cp;
        }
      else if (range.context_type == KDU_JPIP_CONTEXT_MJ2T)
        {
          if ((scan[0]=='t') && (scan[1]=='r') && (scan[2]=='a') &&
              (scan[3]=='c') && (scan[4]=='k'))
            {
              range.remapping_ids[0] = 0;
              scan += 5;
            }
          else if ((scan[0]=='m') && (scan[1]=='o') && (scan[2]=='v') &&
                   (scan[3]=='i') && (scan[4]=='e'))
            {
              range.remapping_ids[0] = 1;
              scan += 5;
            }
          else
            return open_range;
        }
      if (*scan != ']')
        return open_range;
      scan++;
    }
  
  contexts.add(range,false);

  if (*scan != '=')
    return scan;
  scan++;

  // From this point on, we are parsing a context expansion
  int which=contexts.get_num_ranges() - 1;
  assert(which >= 0);
  kdu_range_set *set = create_context_expansion(which);

  range = kdu_sampled_range(); // Reinitialize `range'
  while (((range.from=strtol(scan,&end_cp,10)) >= 0) && (end_cp > scan))
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
      else
        range.to = range.from;
      if (*scan == ':')
        {
          scan++;
          range.step = strtol(scan,&end_cp,10);
          if (end_cp == scan)
            return scan-1;
          scan = end_cp;
        }
      else
        range.step = 1;
      set->add(range);
      if (*scan == ',')
        scan++;
    }

  return scan;
}

/*****************************************************************************/
/*                         kdu_window::add_metareq                           */
/*****************************************************************************/

void
  kdu_window::add_metareq(kdu_uint32 box_type, int qualifier,
                          bool priority, int byte_limit, bool recurse,
                          kdu_long root_bin_id, int max_depth)
{
  if ((byte_limit < 0) || recurse)
    byte_limit = 0;
  if (root_bin_id < 0)
    root_bin_id = 0;
  if (max_depth < 0)
    max_depth = 0;

  kdu_metareq *req = free_metareqs;
  if (req == NULL)
    req = new kdu_metareq;
  else
    free_metareqs = req->next;
  req->next = metareq;
  metareq = req;
  req->box_type = box_type;
  req->qualifier = qualifier;
  req->priority = priority;
  req->byte_limit = byte_limit;
  req->recurse = recurse;
  req->root_bin_id = root_bin_id;
  req->max_depth = max_depth;
  if (qualifier & KDU_MRQ_ALL)
    have_metareq_all = true;
  if (qualifier & KDU_MRQ_GLOBAL)
    have_metareq_global = true;
  if (qualifier & KDU_MRQ_STREAM)
    have_metareq_stream = true;
  if (qualifier & KDU_MRQ_WINDOW)
    have_metareq_window = true;
}

/*****************************************************************************/
/*                        kdu_window::parse_metareq                          */
/*****************************************************************************/

const char *
  kdu_window::parse_metareq(const char *string)
{
  while (*string != '\0')
    {
      while ((*string == ',') || (*string == ';'))
        string++;
      if (*string != '[')
        return string;

      const char *end_prop, *prop=string+1;
      for (string=prop+1; *string != ']'; string++)
        if (*string == '\0')
          return prop;
      end_prop = string;
      string++;

      // Parse global properties for the descriptor
      kdu_metareq req;
      req.root_bin_id = 0;
      req.max_depth = INT_MAX;
      if (*string == 'R')
        {
          for (string++; (*string >= '0') && (*string <= '9'); string++)
            req.root_bin_id = (req.root_bin_id * 10) + (int)(*string - '0');
          if (string[-1] == 'R')
            return string;
        }
      if (*string == 'D')
        {
          req.max_depth = 0;
          for (string++; (*string >= '0') && (*string <= '9'); string++)
            req.max_depth = (req.max_depth * 10) + (int)(*string - '0');
          if (string[-1] == 'D')
            return string;
        }

      if ((string[0] == '!') && (string[1] == '!') && (string[2] == '\0'))
        {
          string += 2;
          metadata_only = true;
        }
      else if ((*string != ',') && (*string != ';') && (*string != '\0'))
        return string;
      
      // Now cycle through all the box properties in the descriptor
      while (prop < end_prop)
        {
          while ((*prop == ',') || (*prop == ';'))
            prop++;
          req.box_type = 0;
          if (*prop == '*')
            prop++;
          else
            {
              if ((end_prop - prop) < 4)
                return prop; // Not enough room for a valid box-type code
              for (int s=24; s >= 0; s-=8)
                {
                  char ch = *(prop++);
                  if (ch == '_')
                    ch = ' ';
                  req.box_type |= ((kdu_uint32) ch) << s;
                }
            }
          req.recurse = false;
          req.byte_limit = INT_MAX;
          if (*prop == ':')
            {
              req.byte_limit = 0;
              for (prop++; (*prop >= '0') && (*prop <= '9'); prop++)
                req.byte_limit = (req.byte_limit * 10) + (int)(*prop - '0');
              if (prop[-1] == ':')
                {
                  if (*prop == 'r')
                    { prop++; req.recurse = true; }
                  else
                    return prop;
                }
            }
          req.qualifier = KDU_MRQ_DEFAULT;
          if (*prop == '/')
            {
              prop++;
              req.qualifier = 0;
              while ((*prop == 'w') || (*prop == 's') ||
                     (*prop == 'g') || (*prop == 'a'))
                {
                  if (*prop == 'w')
                    { prop++; req.qualifier |= KDU_MRQ_WINDOW; }
                  else if (*prop == 's')
                    { prop++; req.qualifier |= KDU_MRQ_STREAM; }
                  else if (*prop == 'g')
                    { prop++; req.qualifier |= KDU_MRQ_GLOBAL; }
                  else if (*prop == 'a')
                    { prop++; req.qualifier |= KDU_MRQ_ALL; }
                }
              if (req.qualifier == 0)
                return prop-1;
            }
          req.priority = false;
          if (*prop == '!')
            { prop++; req.priority = true; }

          add_metareq(req.box_type,req.qualifier,req.priority,
                      req.byte_limit,req.recurse,req.root_bin_id,
                      req.max_depth);
        }
    }
  return NULL;
}
