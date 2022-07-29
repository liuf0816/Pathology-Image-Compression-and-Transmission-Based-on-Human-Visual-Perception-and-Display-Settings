/*****************************************************************************/
// File: kdu_serve.cpp [scope = APPS/SERVER]
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
   Implementation of the `kdu_serve' class, representing the platform
independent core services of the Kakadu server application.  A complete
server application requires network services and, in the case of the
"kdu_server" application, requires support for multi-threading.
******************************************************************************/

#include <math.h>
#include "kdu_params.h"
#include "kdu_utils.h"
#include "serve_local.h"
#include "jp2.h"

/* ========================================================================= */
/*                               Lookup Tables                               */
/* ========================================================================= */

#define KD_NUM_RELEVANCE_THRESHOLDS 20
static double kd_relevance_thresholds[KD_NUM_RELEVANCE_THRESHOLDS];
static int kd_log_relevance[KD_NUM_RELEVANCE_THRESHOLDS];
  // If kd_relevance_thresholds[i-1] >= relevance > kd_relevance_thresholds[i]
  //   assign log_relevance = kd_log_relevance[i]

class log_relevance_initializer {
    public: log_relevance_initializer();
  } _do_it;
log_relevance_initializer::log_relevance_initializer()
{
  double val = 1.0, gap = exp(log(0.5)/4.0);
  for (int i=0; i < KD_NUM_RELEVANCE_THRESHOLDS; i++)
    {
      kd_log_relevance[i] = (int)(256.0*(log(val)/log(2.0)));
      val *= gap;
      if (i == (KD_NUM_RELEVANCE_THRESHOLDS-1))
        val = -1.0; // Ensure everything is caught here.
      kd_relevance_thresholds[i] = val;
    }
}


/* ========================================================================= */
/*                             Internal Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* INLINE                     output_var_length                              */
/*****************************************************************************/

static inline void
  output_var_length(kds_chunk *chunk, int val)
  /* Encodes the supplied value using a byte-oriented extension code,
     storing the results in the supplied data chunk, augmenting its
     `num_bytes' field as appropriate. */
{
  int shift;
  kdu_byte byte;

  assert(val >= 0);
  for (shift=0; val >= (128<<shift); shift += 7);
  for (; shift >= 0; shift -= 7)
    {
      byte = (kdu_byte)(val>>shift) & 0x7F;
      if (shift > 0)
        byte |= 0x80;
      assert(chunk->num_bytes < chunk->max_bytes);
      chunk->data[chunk->num_bytes++] = byte;
    }
}

/*****************************************************************************/
/* INLINE                     simulate_var_length                            */
/*****************************************************************************/

static inline int
  simulate_var_length(int val)
{
  int result = 1;
  while (val >= 128)
    { val >>= 7; result++; }
  return result;
}

/*****************************************************************************/
/* STATIC                    set_precinct_dimensions                         */
/*****************************************************************************/

static void
  set_precinct_dimensions(kdu_params *cod,
                          bool is_component_specific, bool is_tile_specific)
  /* This function retrieves the code-block dimensions from the supplied
     COD parameter object and uses these, in conjunction with the current
     precinct dimensions, to determine suitable precinct dimensions for
     the interchange object.  The function may be called at the main header
     level, the tile level or the tile-component level, which allows it
     to ensure that the most suitable parameters are chosen, even if
     code-block dimensions vary from tile-component to tile-component. */
{
  // Determine the number of precinct specifications we need to write
  bool use_precincts=false;
  cod->get(Cuse_precincts,0,0,use_precincts,false);
  kdu_coords block_size, min_size;
  cod->get(Cblk,0,0,block_size.y);
  cod->get(Cblk,0,1,block_size.x);

  int r, num_specs = 1;
  kdu_coords precinct_size[17];
  if (use_precincts)
    {
      for (r=0; r < 17; r++)
        if (!(cod->get(Cprecincts,r,0,precinct_size[r].y,false,false) &&
              cod->get(Cprecincts,r,1,precinct_size[r].x,false,false)))
          break;
      num_specs = r;
      if (num_specs == 0)
        use_precincts = false;
      else
        for (; r < 17; r++)
          precinct_size[r] = precinct_size[num_specs-1];
    }
  int decomp_val=3;
  for (r=0;
       (r < 17) &&
       (cod->get(Cdecomp,r,0,decomp_val,true,false) || (r < num_specs));
       r++)
    {
      cod_params::get_max_decomp_levels(decomp_val,min_size.x,min_size.y);
      min_size.x = block_size.x << min_size.x;
      min_size.y = block_size.y << min_size.y;
      if (!use_precincts)
        precinct_size[r] = min_size;
      else
        {
          if (precinct_size[r].x > min_size.x)
            precinct_size[r].x = min_size.x;
          if (precinct_size[r].y > min_size.y)
            precinct_size[r].y = min_size.y;
        }
      cod->set(Cprecincts,r,0,precinct_size[r].y);
      cod->set(Cprecincts,r,1,precinct_size[r].x);
    }
  cod->set(Cuse_precincts,0,0,true);
}

/*****************************************************************************/
/* STATIC                           load_blocks                              */
/*****************************************************************************/

static void
  load_blocks(kdu_precinct dest, kdu_resolution src)
{
  int b, min_band;
  int num_bands = src.get_valid_band_indices(min_band);
  for (b=min_band; num_bands > 0; num_bands--, b++)
    {
      kdu_dims blk_indices;
      if (!dest.get_valid_blocks(b,blk_indices))
        continue;
      kdu_subband src_band = src.access_subband(b);
      kdu_coords idx;
      for (idx.y=0; idx.y < blk_indices.size.y; idx.y++)
        for (idx.x=0; idx.x < blk_indices.size.x; idx.x++)
          {
            kdu_block *src_block = src_band.open_block(idx+blk_indices.pos);
            kdu_block *dst_block = dest.open_block(b,idx+blk_indices.pos);
            assert((src_block->modes == dst_block->modes) &&
                   (src_block->size == dst_block->size));
            dst_block->missing_msbs = src_block->missing_msbs;
            if (dst_block->max_passes < src_block->num_passes)
              dst_block->set_max_passes(src_block->num_passes);
            dst_block->num_passes = src_block->num_passes;
            int num_bytes = 0;
            for (int z=0; z < src_block->num_passes; z++)
              {
                num_bytes +=
                  (dst_block->pass_lengths[z] = src_block->pass_lengths[z]);
                dst_block->pass_slopes[z] = src_block->pass_slopes[z];
              }
            if (dst_block->max_bytes < num_bytes)
              dst_block->set_max_bytes(num_bytes,false);
            memcpy(dst_block->byte_buffer,src_block->byte_buffer,
                   (size_t) num_bytes);
            dest.close_block(dst_block);
            src_band.close_block(src_block);
          }
    }
}

/*****************************************************************************/
/* STATIC               order_precinct_list_by_relevance                     */
/*****************************************************************************/

static void
  order_precinct_list_by_relevance(kd_active_precinct *start,
                                   kd_active_precinct * &list_head,
                                   kd_active_precinct * &list_tail)
  /* Processes the tail of a list of active precinct elements, starting from
     `start', filling in their `log_relevance' members and ordering them
     by relevance (most relevant to least relevant).  The head and tail
     of the complete list are managed by the `list_head' and `list_tail'
     variables, which are updated as required by the ordering operation. */
{
  if (start == NULL)
    return;

  // Start by chopping off the tail of the list we want to order
  list_tail = start->prev;
  if (list_tail == NULL)
    {
      assert(start == list_head);
      list_head = NULL;
    }
  else
    list_tail->next = NULL;
  start->prev = NULL;

  // Now work through the quantized log relevance values one by one
  kd_active_precinct *scan, *next;
  for (int i=0; (i < KD_NUM_RELEVANCE_THRESHOLDS) && (start != NULL); i++)
    {
      double threshold = kd_relevance_thresholds[i];
      int log_relevance = kd_log_relevance[i];
      for (scan=start; scan != NULL; scan=next)
        {
          next = scan->next;
          if (scan->relevance <= threshold)
            continue;
          scan->log_relevance = log_relevance;

          // Remove `scan' from the list headed by `start'
          if (scan->prev == NULL)
            {
              assert(scan == start);
              start = next;
            }
          else
            scan->prev->next = next;
          if (next != NULL)
            next->prev = scan->prev;

          // Append `scan' to the main list
          scan->prev = list_tail;
          scan->next = NULL;
          if (list_tail == NULL)
            list_head = list_tail = scan;
          else
            list_tail = list_tail->next = scan;
        }
    }
  assert(start == NULL);
}


/* ========================================================================= */
/*                               kd_chunk_server                             */
/* ========================================================================= */

/*****************************************************************************/
/*                      kd_chunk_server::~kd_chunk_server                    */
/*****************************************************************************/

kd_chunk_server::~kd_chunk_server()
{
  kd_chunk_group *grp;
  while ((grp=chunk_groups) != NULL)
    {
      chunk_groups = grp->next;
      free(grp);
    }
}

/*****************************************************************************/
/*                         kd_chunk_server::get_chunk                        */
/*****************************************************************************/

kds_chunk *
  kd_chunk_server::get_chunk()
{
  kds_chunk *chunk;
  if (free_chunks == NULL)
    {
      size_t header_size = sizeof(kds_chunk);
      header_size += ((8-header_size) & 7); // Round up to multiple of 8.
      size_t size_one = header_size + chunk_size;
      size_one += ((8-size_one) & 7); // Round up to multiple of 8.
      size_t num_in_grp = 1 + (16384 / size_one);
      size_t grp_head_size = sizeof(kd_chunk_group);
      grp_head_size += ((8-grp_head_size) & 7); // Round up to multiple of 8.
      kd_chunk_group *grp = (kd_chunk_group *)
        malloc(size_one * num_in_grp + grp_head_size);
      grp->next = chunk_groups;
      chunk_groups = grp;
      chunk = (kds_chunk *)(((kdu_byte *) grp) + grp_head_size);
      for (size_t n=0; n < num_in_grp; n++)
        {
          chunk->data = ((kdu_byte *) chunk) + header_size;
          chunk->max_bytes = chunk_size;
          chunk->next = free_chunks;
          free_chunks = chunk;
          chunk = (kds_chunk *)(((kdu_byte *) chunk) + size_one);
        }
    }
  chunk = free_chunks;
  free_chunks = chunk->next;
  chunk->prefix_bytes = chunk_prefix_bytes;
  chunk->num_bytes = chunk->prefix_bytes;
  chunk->max_bytes = chunk_size; // Value might have been changed by user
  chunk->abandoned = false;
  chunk->next = NULL;
  chunk->precincts = NULL;
  chunk->other_bins = NULL;
  return chunk;
}

/*****************************************************************************/
/*                       kd_chunk_server::release_chunk                      */
/*****************************************************************************/

void
  kd_chunk_server::release_chunk(kds_chunk *chunk)
{
  chunk->next = free_chunks;
  free_chunks = chunk;
  assert((chunk->precincts == NULL) && (chunk->other_bins == NULL));
     // The caller is responsible for releasing the above lists first.
}


/* ========================================================================= */
/*                               kd_chunk_output                             */
/* ========================================================================= */

/*****************************************************************************/
/*                          kd_chunk_output::flush_buf                       */
/*****************************************************************************/

void
  kd_chunk_output::flush_buf()
{
  kdu_byte *ptr = buffer;
  int xfer_bytes;

  if (skip_bytes > 0)
    {
      xfer_bytes = (int)(next_buf-ptr);
      if (xfer_bytes > skip_bytes)
        xfer_bytes = skip_bytes;
      skip_bytes -= xfer_bytes;
      ptr += xfer_bytes;
    }
  while (ptr < next_buf)
    {
      assert(chunk->num_bytes >= chunk->prefix_bytes);
      xfer_bytes = chunk->max_bytes - chunk->num_bytes;
      if (xfer_bytes == 0)
        {
          if (chunk->next != NULL)
            { chunk = chunk->next; continue; }
          break;
        }
      if (xfer_bytes > (next_buf-ptr))
        xfer_bytes = (int)(next_buf-ptr);
      memcpy(chunk->data+chunk->num_bytes,ptr,(size_t) xfer_bytes);
      ptr += xfer_bytes;
      chunk->num_bytes += xfer_bytes;
    }
  next_buf = buffer;
}

/* ========================================================================= */
/*                              kd_active_server                             */
/* ========================================================================= */

/*****************************************************************************/
/*                     kd_active_server::~kd_active_server                   */
/*****************************************************************************/

kd_active_server::~kd_active_server()
{
  kd_active_group *grp;
  while ((grp = groups) != NULL)
    {
      groups = grp->next;
      delete grp;
    }
}

/*****************************************************************************/
/*                       kd_active_server::get_precinct                      */
/*****************************************************************************/

kd_active_precinct *
  kd_active_server::get_precinct()
{
  kd_active_precinct *elt;
  if (free_list == NULL)
    {
      kd_active_group *grp = new kd_active_group;
      grp->next = groups;
      groups = grp;
      for (int n=0; n < 32; n++)
        {
          elt = grp->precincts + n;
          elt->next = free_list;
          free_list = elt;
        }
    }
  elt = free_list;
  free_list = elt->next;
  memset(elt,0,sizeof(kd_active_precinct));
  return elt;
}


/* ========================================================================= */
/*                               kds_id_encoder                              */
/* ========================================================================= */

/*****************************************************************************/
/*                          kds_id_encoder::encode_id                        */
/*****************************************************************************/

int
  kds_id_encoder::encode_id(kdu_byte *dest, int cls, int stream_id,
                            kdu_long bin_id, bool is_complete,
                            bool extended)
{
  assert((stream_id >= 0) && (bin_id >= 0));
  bool repeat_class =
    exploit_last_message && (last_class == cls) && (last_extended==extended);
  bool repeat_stream =
    exploit_last_message && (last_stream_id == stream_id);
  if (dest != NULL)
    {
      exploit_last_message = true;
      last_stream_id = stream_id;
      last_class = cls; last_extended = extended;
    }
  
  kdu_byte byte = (is_complete)?0x10:0;
  if (!repeat_stream)
    { repeat_class = false; byte |= 0x60; }
  else if (!repeat_class)
    byte |= 0x40;
  else
    byte |= 0x20;
  int bytes_out = 1;
  int bits_left = 4;

  // Write the in-class bin identifier.
  while ((bin_id >> bits_left) != 0)
    {
      bits_left += 7;
      bytes_out++;
    }
  if (dest != NULL)
    {
      bits_left -= 4;
      byte += (kdu_byte)((bin_id>>bits_left) & 0x0F);
      while (bits_left > 0)
        {
          byte |= 0x80;
          *(dest++) = byte;
          bits_left -= 7;
          byte = ((kdu_byte)(bin_id >> bits_left)) & 0x7F;
        }
      *(dest++) = byte;
    }

  int shift;

  if (!repeat_class)
    { // Write the class VBAS
      cls += cls + ((extended)?1:0);
      for (shift=0, bytes_out++; (cls>>shift) >= 128; shift+=7)
        bytes_out++;
      if (dest != NULL)
        {
          for (; shift >= 0; shift -= 7)
            {
              byte = (kdu_byte)(cls>>shift) & 0x7F;
              if (shift > 0)
                byte |= 0x80;
              *(dest++) = byte;
            }
        }
    }

  if (!repeat_stream)
    { // Write the stream-ID VBAS
      for (shift=0, bytes_out++; (stream_id>>shift) >= 128; shift+=7)
        bytes_out++;
      if (dest != NULL)
        {
          for (; shift >= 0; shift -= 7)
            {
              byte = (kdu_byte)(stream_id>>shift) & 0x7F;
              if (shift > 0)
                byte |= 0x80;
              *(dest++) = byte;
            }
        }
    }

  return bytes_out;
}


/* ========================================================================= */
/*                                  kd_meta                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                               kd_meta::init                               */
/*****************************************************************************/

int
  kd_meta::init(const kds_metagroup *metagroup, kdu_long bin_id,
                int bin_offset, int tree_depth)
{
  this->bin_id = bin_id;
  this->tree_depth = tree_depth;
  this->metagroup = metagroup;
  this->bin_offset = bin_offset;
  next_in_bin = prev_in_bin = phld = active_next = NULL;
  num_bytes = metagroup->length;
  dispatched_bytes = 0;
  fully_dispatched = false;
  in_scope = false;
  max_content_bytes = INT_MAX;
  sequence = INT_MAX;
  int length = num_bytes;
  if (metagroup->phld != NULL)
    {
      kd_meta *mnew, *tail = NULL;
      const kds_metagroup *scan;
      int off=0;
      for (scan=metagroup->phld; scan != NULL; scan=scan->next)
        {
          int phld_tree_depth = tree_depth;
          if (scan->num_box_types > 0)
            phld_tree_depth++;
          mnew = new kd_meta;
          length +=
            mnew->init(scan,metagroup->phld_bin_id,off,phld_tree_depth);
          off += mnew->num_bytes;
          mnew->prev_in_bin = tail;
          if (tail == NULL)
            phld = tail = mnew;
          else
            tail = tail->next_in_bin = mnew;
        }
    }
  return length;
}

/*****************************************************************************/
/*                    kd_meta::find_active_scope_and_sequence                */
/*****************************************************************************/

bool
  kd_meta::find_active_scope_and_sequence(kdu_window &window,
                                          kd_codestream_window *stream_windows,
                                          bool force_in_scope,
                                          bool force_headers_in_scope,
                                          int force_max_sequence,
                                          int *max_descendant_bytes)
{
  kd_codestream_window *wp;
  kd_meta *mscan;
  kdu_metareq *req;
  bool contents_in_scope = false; // Function return value
  int max_descendant_bytes_var=0; // Use if we need to start a new descendant
                                  // byte counter from this level in the tree.

  if (force_in_scope)
    {
      if (max_descendant_bytes != NULL)
        {
          max_content_bytes = *max_descendant_bytes;
          if (max_content_bytes < 0)
            {
              max_content_bytes = 0;
              force_in_scope = false;
              max_descendant_bytes = NULL;
            }
          else
            (*max_descendant_bytes) -= num_bytes;
        }
      else
        max_content_bytes = INT_MAX;
    }
  else
    max_content_bytes = 0;

  in_scope = force_in_scope;
  if (force_headers_in_scope && (metagroup->num_box_types > 0))
    in_scope = true;
  sequence = (in_scope)?force_max_sequence:KDU_INT32_MAX;

  // Start by setting the dynamic depth of each `kdu_metareq' entry
  if ((bin_id == 0) && (bin_offset == 0))
    { // Right at the start of the metadata scan, install initial configuration
      for (req=window.metareq; req != NULL; req=req->next)
        if (req->root_bin_id == 0)
          req->dynamic_depth = req->max_depth;
        else
          req->dynamic_depth = -1; // These elements don't apply yet
    }
  if (phld != NULL)
    { // Base dynamic depth on placeholder bin number, since it holds contents
      for (req=window.metareq; req != NULL; req=req->next)
        if (req->root_bin_id == phld->bin_id)
          req->dynamic_depth = tree_depth + req->max_depth;
    }

  // Find out if this group's scope matches any of the stream windows or
  // any of the originally requested codestream contexts; if
  // it is not a leaf, the match will be a possible match, rather than a
  // guaranteed one.
  kds_metascope *scope = metagroup->scope;
  assert(scope != NULL);
  bool have_scope_match = true; // True until proven otherwise
  bool have_stream_match = false;
  bool have_region_match = false;
  if (scope->flags & KDS_METASCOPE_HAS_IMAGE_SPECIFIC_DATA)
    {
      int cn, cne;
      kdu_sampled_range *rg, *rge;
      for (cn=0; ((rg=window.codestreams.access_range(cn)) != NULL) &&
                 !have_stream_match; cn++)
        if (scope->entities->test_codestream_range(*rg))
          have_stream_match = true;
      for (cn=0; ((rg=window.contexts.access_range(cn)) != NULL) &&
                 !have_stream_match; cn++)
        {
          if ((rg->context_type == KDU_JPIP_CONTEXT_JPXL) &&
              scope->entities->test_compositing_layer_range(*rg))
            have_stream_match = true;
          else if (rg->expansion != NULL)
            for (cne=0; ((rge=rg->expansion->access_range(cne)) != NULL) &&
                        !have_stream_match; cne++)
              have_stream_match = true;
        }
      if ((!have_stream_match) &&
          !(scope->flags & KDS_METASCOPE_HAS_GLOBAL_DATA))
        have_scope_match = false;
    }
  kdu_int32 region_adjusted_sequence = KDU_INT32_MAX;
  if (have_stream_match &&
      (scope->flags & KDS_METASCOPE_HAS_REGION_SPECIFIC_DATA))
    {
      for (wp=stream_windows; wp != NULL; wp=wp->next)
        {
          if ((scope->max_discard_levels >= wp->active_discard_levels) &&
              scope->region.intersects(wp->active_region))
            {
              have_region_match = true;
              // Introduce a simple heuristic to delay the sequencing of
              // spatially-sensitive metadata if the viewing resolution is
              // close to the resolution at which the data is supposed to be
              // visible.
              kdu_int32 adj_seq = 8 -
                3 * (scope->max_discard_levels - wp->active_discard_levels);
              adj_seq = scope->sequence + ((adj_seq<0)?0:adj_seq);
              if (adj_seq < region_adjusted_sequence)
                region_adjusted_sequence = adj_seq;
            }
        }
      if ((!have_region_match) &&
          !(scope->flags & (KDS_METASCOPE_HAS_IMAGE_WIDE_DATA |
                            KDS_METASCOPE_HAS_GLOBAL_DATA)))
        have_scope_match = false;
    }

  // Now see if this group or any of its sub-groups, have a possible match
  // with any metareq element.  We will need to know this later to determine
  // whether or not there is any value in examining matches with descendants.
  // It is also a simple test which can save us from having to examine all
  // metareq entries.
  bool have_possible_metareq_match = window.have_metareq_all ||
    (window.have_metareq_global &&
     (scope->flags & KDS_METASCOPE_HAS_GLOBAL_DATA)) ||
    (window.have_metareq_window && have_region_match) ||
    (window.have_metareq_stream && have_stream_match &&
     (scope->flags & KDS_METASCOPE_HAS_IMAGE_WIDE_DATA));

  // Now check to see if there are any actual metareq matches.
  if (have_possible_metareq_match && (metagroup->num_box_types > 0))
    for (req=window.metareq; req != NULL; req=req->next)
      {
        int b;

        if (req->dynamic_depth < tree_depth)
          continue; // Metareq entry does not apply so deep in the tree
        if (req->box_type != 0)
          {
            for (b=0; b < metagroup->num_box_types; b++)
              if (metagroup->box_types[b] == req->box_type)
                break;
            if (b == metagroup->num_box_types)
              continue; // No matching box types
          }
        if ((req->qualifier & KDU_MRQ_ALL) ||
            ((req->qualifier & KDU_MRQ_GLOBAL) &&
             (scope->flags & KDS_METASCOPE_HAS_GLOBAL_DATA)) ||
            ((req->qualifier & KDU_MRQ_STREAM) && have_stream_match &&
             (scope->flags & KDS_METASCOPE_HAS_IMAGE_WIDE_DATA)) ||
            ((req->qualifier & KDU_MRQ_WINDOW) && have_region_match))
          {
            in_scope = true;
            if (have_region_match)
              {
                if (region_adjusted_sequence < sequence)
                  sequence = region_adjusted_sequence;
              }
            else if (scope->sequence < sequence)
              sequence = scope->sequence;
            if (req->priority && (sequence > 0))
              sequence = 0;
            if (req->byte_limit > max_descendant_bytes_var)
              max_descendant_bytes_var = req->byte_limit;
            if (req->byte_limit > max_content_bytes)
              max_content_bytes = req->byte_limit;
            if (req->recurse)
              force_headers_in_scope = true;
          }
      }

  if (max_descendant_bytes_var > 0)
    { // See if we need to set up a new descendant bytes counter
      if (max_descendant_bytes != NULL)
        {
          if (*max_descendant_bytes < max_descendant_bytes_var)
            {
              *max_descendant_bytes = 0;
              max_descendant_bytes = &max_descendant_bytes_var;
            }
        }
      else
        max_descendant_bytes = &max_descendant_bytes_var;
    }

  // Now check for direct matches with the view window
  if ((scope->flags & KDS_METASCOPE_LEAF) && // Matches count only at leaves
      have_scope_match &&
      ((window.metareq == NULL) ||
       (scope->flags & KDS_METASCOPE_MANDATORY) ||
       ((scope->flags & KDS_METASCOPE_IMAGE_MANDATORY) &&
        !window.metadata_only)))
    {
      in_scope = true;
      max_content_bytes = INT_MAX;
      max_descendant_bytes = NULL; // Include all of descendant contents
      if (have_region_match)
        {
          if (region_adjusted_sequence < sequence)
            sequence = region_adjusted_sequence;
        }
      else if (scope->sequence < sequence)
        sequence = scope->sequence;
    }

  if (max_content_bytes > 0)
    contents_in_scope = true;

  if ((phld != NULL) &&
      (in_scope || have_scope_match || have_possible_metareq_match))
    { // Examine the descendants, allowing them to affect our own scope
      bool non_initial_contents_in_scope = false;
      for (mscan=phld; mscan != NULL; mscan=mscan->next_in_bin)
        {
          if (mscan->metagroup->num_box_types == 0)
            {
              assert((mscan->phld == NULL) && (mscan->next_in_bin == NULL) &&
                     (mscan==phld)); // Must be the contents of this box
              mscan->in_scope = in_scope;
              mscan->sequence = sequence;
              mscan->max_content_bytes = max_content_bytes;
            }
          else
            {
              bool sub_contents_in_scope =
                mscan->find_active_scope_and_sequence(window,stream_windows,
                                     in_scope && (max_content_bytes > 0),
                                     force_headers_in_scope,sequence,
                                     max_descendant_bytes);
              if (sub_contents_in_scope)
                {
                  contents_in_scope = true;
                  if (mscan != phld)
                    non_initial_contents_in_scope = true;
                }
            }
        }
      if (phld->in_scope)
        {
          in_scope = true;
          max_content_bytes = INT_MAX;
          if (phld->sequence < sequence)
            sequence = phld->sequence;
        }
      if ((metagroup->last_box_type == jp2_association_4cc) &&
          non_initial_contents_in_scope)
        { // Force inclusion of entire first sub-box of an asoc box
          phld->force_active_scope_and_sequence(sequence);
        }
    }

  if (phld != NULL)
    { // Reset all dynamic depth values installed by this function
      for (req=window.metareq; req != NULL; req=req->next)
        if (req->root_bin_id == phld->bin_id)
          req->dynamic_depth = -1;
    }

  // Finally, make scope & sequence adjustments based on sibling metagroups.
  if (in_scope)
    { // Walk backwards
      for (mscan=prev_in_bin; mscan != NULL; mscan=mscan->prev_in_bin)
        {
          if (mscan->in_scope && (mscan->sequence <= sequence))
            {
              if (mscan->max_content_bytes == INT_MAX)
                break; // All previous elements in scope with lower sequence
            }
          else
            {
              mscan->sequence = sequence;
              mscan->in_scope = true;
            }
          mscan->max_content_bytes = INT_MAX;
        }
    }

  if ((prev_in_bin != NULL) && prev_in_bin->in_scope &&
      (prev_in_bin->metagroup->scope->flags &
       KDS_METASCOPE_INCLUDE_NEXT_SIBLING))
    { // Need to at least send the placeholder box to allow the client to
      // receive a complete data-bin and hence determine whether or not
      // there are any further boxes of interest at the top level of the
      // current data-bin.
      if (!in_scope)
        {
          in_scope = true;
          max_content_bytes = 0;
          sequence = prev_in_bin->sequence;
        }
      else if (prev_in_bin->sequence < sequence)
        sequence = prev_in_bin->sequence; // Sequence box headers at same time
                 // as prev group, but might end up sequencing box contents for
                 // this group later, or not at all.
    }

  return contents_in_scope;
}

/*****************************************************************************/
/*                   kd_meta::force_active_scope_and_sequence                */
/*****************************************************************************/

void
  kd_meta::force_active_scope_and_sequence(kdu_int32 max_sequence)
{
  in_scope = true;
  if (sequence > max_sequence)
    sequence = max_sequence;
  max_content_bytes = INT_MAX;
  for (kd_meta *mscan=phld; mscan != NULL; mscan=mscan->next_in_bin)
    mscan->force_active_scope_and_sequence(max_sequence);
}

/*****************************************************************************/
/*                        kd_meta::include_active_groups                     */
/*****************************************************************************/

void
  kd_meta::include_active_groups(kd_meta *start)
{
  active_length = 0;
  if (!in_scope)
    return;
  kd_meta *scan, *prev=start;
  for (scan=prev->active_next; scan != NULL; prev=scan, scan=scan->active_next)
    {
      assert(scan->in_scope);
      if (scan->sequence > sequence)
        break;
    }
  active_next = scan;
  if (prev != this)
    prev->active_next = this;

  assert((max_content_bytes >= 0) &&
         (metagroup->length >= metagroup->last_box_header_prefix));
  active_length = metagroup->length;
  if (max_content_bytes < (active_length - metagroup->last_box_header_prefix))
    active_length = max_content_bytes + metagroup->last_box_header_prefix;

  if (phld != NULL)
    for (prev=this, scan=phld; (scan != NULL) && scan->in_scope;
         prev=scan, scan=scan->next_in_bin)
      scan->include_active_groups(prev);
}


/* ========================================================================= */
/*                           kd_codestream_window                            */
/* ========================================================================= */

/*****************************************************************************/
/*               kd_codestream_window::kd_codestream_window                  */
/*****************************************************************************/

kd_codestream_window::kd_codestream_window()
{
  stream = NULL;
  stream_idx = -1;
  active_discard_levels = 0;
  max_codestream_components = num_codestream_components = 0;
  codestream_components = NULL;
  max_context_components = num_context_components = 0;
  context_components = NULL;
  max_scratch_components = 0;
  scratch_components = NULL;
  next = prev = NULL;
  context_type = 0;
  context_idx = -1;
  member_idx = -1;
  remapping_ids[0] = remapping_ids[1] = -1;
}

/*****************************************************************************/
/*                    kd_codestream_window::initialize                       */
/*****************************************************************************/

kdu_long
  kd_codestream_window::initialize(kd_stream *stream, kdu_coords resolution,
                                   kdu_dims region, int round_direction,
                                   kdu_range_set &component_ranges,
                                   int num_context_components,
                                   const int *context_component_indices)
{
  this->stream = stream;
  this->stream_idx = stream->stream_id;
  active_discard_levels = 0;
  active_region = stream->image_dims;
  this->num_context_components = 0;
  this->component_ranges.init();
  if ((resolution.x < 1) || (resolution.y < 1) ||
      (region.size.x < 1) || (region.size.y < 1))
    {
      active_tiles.size = kdu_coords(0,0);
      active_region.size = kdu_coords(0,0); // so we send only headers
      return 0;
    }

  // Start by figuring out the number of discard levels
  kdu_coords min = stream->image_dims.pos;
  kdu_coords size = stream->image_dims.size;
  kdu_coords lim = min + size;
  kdu_dims active_res; active_res.pos = min; active_res.size = size;
  kdu_long target_area = ((kdu_long) resolution.x) * ((kdu_long) resolution.y);
  kdu_long best_area_diff = 0;
  int d = 0;
  bool done = false;
  while (!done)
    {
      if (round_direction < 0)
        { // Round down
          if ((size.x <= resolution.x) && (size.y <= resolution.y))
            {
              active_discard_levels = d;
              active_res.size = size; active_res.pos = min;
              done = true;
            }
        }
      else if (round_direction > 0)
        { // Round up
          if ((size.x >= resolution.x) && (size.y >= resolution.y))
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
  min = region.pos;
  lim = min + region.size;
  active_region.pos.x = (int)
    ((((kdu_long) min.x) * ((kdu_long) active_res.size.x)) /
     ((kdu_long) resolution.x));
  active_region.pos.y = (int)
    ((((kdu_long) min.y) * ((kdu_long) active_res.size.y)) /
     ((kdu_long) resolution.y));
  active_region.size.x = 1 + (int)
    ((((kdu_long)(lim.x-1)) * ((kdu_long) active_res.size.x)) /
     ((kdu_long) resolution.x)) - active_region.pos.x;
  active_region.size.y = 1 + (int)
    ((((kdu_long)(lim.y-1)) * ((kdu_long) active_res.size.y)) /
     ((kdu_long) resolution.y)) - active_region.pos.y;
  active_region.pos += active_res.pos;
  active_region &= active_res;

  if ((active_region.size.x < 1) || (active_region.size.y < 1))
    { /* v6.0 fix */
      active_tiles.size = kdu_coords(0,0);
      active_region.size = kdu_coords(0,0); // so we send only headers
      return 0;
    }

  // Save the dimensions required later to calculate active area
  size = active_region.size;

  // Now adjust the image region up onto the full image canvas
  active_region.pos.x <<= active_discard_levels;
  active_region.pos.y <<= active_discard_levels;
  active_region.size.x <<= active_discard_levels;
  active_region.size.y <<= active_discard_levels;
  active_region &= stream->image_dims;

  // Calculate the set of relevant tiles
  min = active_region.pos - stream->tile_partition.pos;
  lim = min + active_region.size;
  min.x = floor_ratio(min.x,stream->tile_partition.size.x);
  min.y = floor_ratio(min.y,stream->tile_partition.size.y);
  lim.x = ceil_ratio(lim.x,stream->tile_partition.size.x);
  lim.y = ceil_ratio(lim.y,stream->tile_partition.size.y);
  active_tiles.pos = min;
  active_tiles.size = lim - min;
  if (!active_region)
    active_tiles.size = kdu_coords(0,0);

  // Install the image components which belong to the window.
  if (component_ranges.is_empty())
    this->component_ranges.add(0,stream->num_components-1);
  else
    this->component_ranges.copy_from(component_ranges);


  if (stream->num_components > max_codestream_components)
    {
      max_codestream_components = stream->num_components;
      if (codestream_components != NULL)
        delete[] codestream_components;
      codestream_components = new int[max_codestream_components];
    }
  num_codestream_components =
    this->component_ranges.expand(codestream_components,0,
                                  stream->num_components-1);
  int n, c;
  expand_ycc = false;
  if (num_codestream_components > 3)
    {
      bool ycc_usage[3]={false,false,false};
      for (n=0; n < num_codestream_components; n++)
        {
          c = codestream_components[n];
          if (c < 3)
            expand_ycc = ycc_usage[c] = true;
        }
      if (expand_ycc)
        for (c=0; c < 3; c++)
          if (!ycc_usage[c])
            codestream_components[num_codestream_components++] = c;
    }
  assert(num_codestream_components <= stream->num_components);

  this->num_context_components = num_context_components;
  if (num_context_components > this->max_context_components)
    {
      this->max_context_components += num_context_components;
      if (this->context_components != NULL)
        delete[] this->context_components;
      this->context_components = new int[this->max_context_components];
    }
  for (n=0; n < num_context_components; n++)
    this->context_components[n] = context_component_indices[n];

  // Finally, compute the area of this stream window, making some effort to
  // take into account overlap with other stream windows
  kdu_long area, context_component_area=0;
  for (n=0; n < num_context_components; n++)
    {
      kd_codestream_window *win;
      int idx = this->context_components[n];
      if (idx >= stream->num_output_components)
        continue;
      kdu_long overlap_area, max_overlap_area=0;
      kdu_long region_area=active_region.area();
      for (win=prev; win != NULL; win=win->prev)
        {
          if ((win->stream_idx != stream_idx) ||
              (win->active_discard_levels != active_discard_levels))
            continue;
          for (c=0; c < win->num_context_components; c++)
            if (win->context_components[c] == idx)
              break;
          if (c == win->num_context_components)
            continue;
          overlap_area = (win->active_region & active_region).area();
          if (overlap_area > max_overlap_area)
            max_overlap_area = overlap_area;
        }
      area = (kdu_long)(size.x / stream->output_component_subs[idx].x + 64) *
             (kdu_long)(size.y / stream->output_component_subs[idx].y + 64);
      area -= (area * max_overlap_area) / region_area;
      context_component_area += area;
    }

  if (component_ranges.is_empty() && (context_component_area > 0))
    return context_component_area;

  kdu_long codestream_component_area=0;
  for (n=0; n < num_codestream_components; n++)
    {
      int idx = this->codestream_components[n];
      kd_codestream_window *win;
      kdu_long overlap_area, max_overlap_area=0;
      kdu_long region_area=active_region.area();
      for (win=prev; win != NULL; win=win->prev)
        {
          if ((win->stream_idx != stream_idx) ||
              (win->active_discard_levels != active_discard_levels))
            continue;
          for (c=0; c < win->num_codestream_components; c++)
            if (win->codestream_components[c] == idx)
              break;
          if (c == win->num_codestream_components)
            continue;
          overlap_area = (win->active_region & active_region).area();
          if (overlap_area > max_overlap_area)
            max_overlap_area = overlap_area;
        }
      area = (kdu_long)(size.x / stream->component_subs[idx].x + 64) *
             (kdu_long)(size.y / stream->component_subs[idx].y + 64);
      area -= (area * max_overlap_area) / region_area;
      codestream_component_area += area;
    }

  if ((context_component_area == 0) ||
      (codestream_component_area < context_component_area))
    return codestream_component_area;
  else
    return context_component_area;
}


/* ========================================================================= */
/*                                kd_stream                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                           kd_stream::kd_stream                            */
/*****************************************************************************/

kd_stream::kd_stream(kd_serve *owner)
{
  this->owner = owner;
  stream_id = -1;
  layer_log_slopes = NULL;
  max_layer_slopes = 0;
  target = NULL;
  num_components = num_output_components = total_tiles = 0;
  component_subs = output_component_subs = NULL;
  tiles = NULL;
  main_header_bytes = dispatched_header_bytes = completed_tiles = 0;
  header_fully_dispatched = is_active = is_locked = false;
  active_tiles = NULL;
  next = NULL;
}

/*****************************************************************************/
/*                           kd_stream::~kd_stream                           */
/*****************************************************************************/

kd_stream::~kd_stream()
{
  if (!interchange)
    return;
  if (is_locked)
    {
      target->release_codestreams(1,&stream_id,owner);
      is_locked = false;
    }

  if (tiles != NULL)
    {
      kdu_coords idx;
      kd_tile *tp=tiles;

      for (idx.y=0; idx.y < tile_indices.size.y; idx.y++)
        for (idx.x=0; idx.x < tile_indices.size.x; idx.x++, tp++)
          {
            int c, r, b, nblks;
            kd_precinct_block *pblk;
            
            if (tp->comps != NULL)
              {
                for (c=0; c < num_components; c++)
                  {
                    kd_tile_comp *tc = tp->comps + c;
                    if (tc->res != NULL)
                      {
                        for (r=0; r < tc->num_resolutions; r++)
                          {
                            kd_resolution *rp = tc->res + r;
                            if (rp->pblocks != NULL)
                              {
                                nblks = rp->num_pblocks.x*rp->num_pblocks.y;
                                for (b=0; b < nblks; b++)
                                  if (rp->pblocks[b] != NULL)
                                    {
                                      rp->pblocks[b]->next = rp->free_pblocks;
                                      rp->free_pblocks = rp->pblocks[b];
                                    }
                                  delete[] rp->pblocks;
                              }
                            while ((pblk=rp->free_pblocks) != NULL)
                              {
                                rp->free_pblocks = pblk->next;
                                free(pblk);
                              }
                          }
                        delete[] tc->res;
                      }
                  }
                delete[] tp->comps;
              }
            if (tp->interchange.exists())
              tp->interchange.close();
          }
      delete[] tiles;
    }
  interchange.destroy();
  target->detach_from_codestream(stream_id,owner);
  if (layer_log_slopes != NULL)
    delete[] layer_log_slopes;
  if (component_subs != NULL)
    delete[] component_subs;
  if (output_component_subs != NULL)
    delete[] output_component_subs;
}

/*****************************************************************************/
/*                           kd_stream::initialize                           */
/*****************************************************************************/

void
  kd_stream::initialize(int stream_id, kdu_codestream codestream,
                        kdu_serve_target *target, bool ignore_relevance_info)
{
  int c;

  this->stream_id = stream_id;
  this->target = target;
  source = codestream;
  target->lock_codestreams(1,&stream_id,owner);
  is_locked = true;
  source.get_dims(-1,image_dims);
  source.get_tile_partition(tile_partition);
  num_components = source.get_num_components(false);
  num_output_components = source.get_num_components(true);
  component_subs = new kdu_coords[num_components];
  for (c=0; c < num_components; c++)
    source.get_subsampling(c,component_subs[c]);
  output_component_subs = new kdu_coords[num_output_components];
  for (c=0; c < num_output_components; c++)
    source.get_subsampling(c,output_component_subs[c]);
  source.get_valid_tiles(tile_indices);
  total_tiles = (int) tile_indices.area();
  tiles = new kd_tile[total_tiles];

  // Start building the interchange object.  We will transfer tile headers
  // and build the structures for each tile only on demand.
  siz_params *src_params = source.access_siz();
  interchange.create(src_params);
  siz_params *ichg_params = interchange.access_siz();
  ichg_params->copy_from(src_params,-1,-1); // Copy main header parameters
  kdu_params *ichg_cod = ichg_params->access_cluster(COD_params);
  set_precinct_dimensions(ichg_cod,false,false);
  kdu_params *ichg_poc = ichg_params->access_cluster(POC_params);
  ichg_poc->delete_unparsed_attribute(Porder); // Remove global POC
  for (c=0; c < num_components; c++)
    {
      kdu_params *comp_cod = ichg_cod->access_unique(-1,c);
      if (comp_cod != NULL)
        set_precinct_dimensions(comp_cod,true,false);
    }
  ichg_params->finalize_all(-1,false);
  main_header_bytes = 2 + ichg_params->generate_marker_segments(NULL,-1,0);

  // Examine code-stream comments, looking to parse relevance info
  max_layer_slopes = 0;
  layer_log_slopes = NULL;
  if (!ignore_relevance_info)
    {
      const char *layer_info_header =
        "Kdu-Layer-Info: log_2{Delta-D(MSE)/[2^16*Delta-L(bytes)]}";
      size_t layer_info_len = strlen(layer_info_header);
      const char *com_text;
      kdu_codestream_comment com;
      while ((com_text = (com=source.get_comment(com)).get_text()) != NULL)
        {
          if (strncmp(com_text,layer_info_header,layer_info_len) != 0)
            continue;
          if (max_layer_slopes != 0)
            continue; // Two separate records of the layer information.
          com_text = strchr(com_text,'\n');
          double val;
          int num_layers=0, max_layers=0;
          while ((com_text != NULL) &&
                 (sscanf(com_text+1,"%lf",&val) == 1))
            {
              if (num_layers == max_layers)
                {
                  max_layers += 10;
                  int *log_slopes = new int[max_layers];
                  if (layer_log_slopes != NULL)
                    {
                      for (c=0; c < num_layers; c++)
                        log_slopes[c] = layer_log_slopes[c];
                      delete[] layer_log_slopes;
                    }
                  layer_log_slopes = log_slopes;
                }
              layer_log_slopes[num_layers] = (int)
                floor(val*256.0+0.5);
              if ((num_layers > 0) &&
                  (layer_log_slopes[num_layers] >=
                   layer_log_slopes[num_layers-1]))
                layer_log_slopes[num_layers] =
                  layer_log_slopes[num_layers-1]-1;
              num_layers++;
              com_text = strchr(com_text+1,'\n');
            }
          max_layer_slopes = num_layers;
        }
    }

  target->release_codestreams(1,&stream_id,owner);
  is_locked = false;
  active_tiles = NULL;
  completed_tiles = 0;
  dispatched_header_bytes = 0;
  header_fully_dispatched = false;
  is_active = false;
}

/*****************************************************************************/
/*                            kd_stream::init_tile                           */
/*****************************************************************************/

void
  kd_stream::init_tile(kd_tile *tp)
{
  if (tp->comps != NULL)
    return;
  tp->stream = this;
  kdu_coords rel_idx = tp->t_idx - tile_indices.pos;
  tp->tnum = rel_idx.x + (rel_idx.y * tile_indices.size.x);
  bool was_locked = is_locked;
  if (!is_locked)
    {
      target->lock_codestreams(1,&stream_id,owner);
      is_locked = true;
    }
  tp->source = source.open_tile(tp->t_idx);
  tp->source.close(); // Ensures all tile header parameters are loaded

  // Copy parameters, fixing up precinct dimensions and eliminating any
  // POC marker segments.
  kdu_params *src_params = source.access_siz();
  kdu_params *ichg_params = interchange.access_siz();
  ichg_params->copy_from(src_params,tp->tnum,tp->tnum); // Copy tile params
  kdu_params *ichg_cod = ichg_params->access_cluster(COD_params);
  kdu_params *ichg_poc = ichg_params->access_cluster(POC_params);
  kdu_params *tile_poc = ichg_poc->access_unique(tp->tnum,-1);
  if (tile_poc != NULL)
    tile_poc->delete_unparsed_attribute(Porder);
  int c;
  for (c=-1; c < num_components; c++)
    {
      kdu_params *tcomp_cod = ichg_cod->access_unique(tp->tnum,c);
      if (tcomp_cod != NULL)
        set_precinct_dimensions(tcomp_cod,(c >= 0),true);
    }
  ichg_params->finalize_all(tp->tnum,false);
  tp->header_bytes=ichg_params->generate_marker_segments(NULL,tp->tnum,0);
      
  // Now complete the structure.
  tp->interchange = interchange.open_tile(tp->t_idx);
  tp->uses_ycc = tp->interchange.get_ycc();
  tp->num_layers = tp->interchange.get_num_layers();
  if (tp->num_layers >= 0xFFFF)
    tp->num_layers = 0xFFFE; // Reserve 0xFFFF for special signalling
  tp->comps = new kd_tile_comp[num_components];
  tp->total_precincts = 0;
  for (c=0; c < num_components; c++)
    {
      kd_tile_comp *tc = tp->comps + c;
      tc->c_idx = c;
      tc->tile = tp;
      tc->interchange = tp->interchange.access_component(c);
      tc->num_resolutions = tc->interchange.get_num_resolutions();
      tc->res = new kd_resolution[tc->num_resolutions];
      int r;
      for (r=0; r < tc->num_resolutions; r++)
        {
          kd_resolution *rp = tc->res + r;
          rp->r_idx = r;
          rp->tile_comp = tc;
          rp->interchange = tc->interchange.access_resolution(r);
          rp->interchange.get_valid_precincts(rp->precinct_indices);
          tp->total_precincts += (int) rp->precinct_indices.area();
          rp->log_pblock_size = 4;
          rp->pblock_size = (1 << rp->log_pblock_size);
          rp->pblock_bytes = sizeof(kd_precinct_block) +
            sizeof(kd_precinct)*rp->pblock_size*rp->pblock_size;
          rp->num_pblocks.x =
            1 + ((rp->precinct_indices.size.x-1)>>rp->log_pblock_size);
          rp->num_pblocks.y =
            1 + ((rp->precinct_indices.size.y-1)>>rp->log_pblock_size);
          int nblks = rp->num_pblocks.x*rp->num_pblocks.y;
          rp->pblocks = new kd_precinct_block *[nblks];
          memset(rp->pblocks,0,sizeof(kd_precinct_block *)*nblks);
        }
    }
  tp->interchange.close();
  if (!was_locked)
    {
      target->release_codestreams(1,&stream_id,owner);
      is_locked = false;
    }
}

/*****************************************************************************/
/*                            kd_stream::init_tile                           */
/*****************************************************************************/

void
  kd_stream::init_tile(kd_tile *tp, int tnum)
{
  kdu_coords idx;
  idx.y = tnum / tile_indices.size.x;
  idx.x = tnum - idx.y*tile_indices.size.x;
  tp->t_idx = idx + tile_indices.pos;
  init_tile(tp);
  assert(tp->tnum == tnum);
}

/*****************************************************************************/
/*                      kd_stream::include_active_tiles                      */
/*****************************************************************************/

int
  kd_stream::include_active_tiles(kd_codestream_window *stream_windows)
{
  assert(is_active && (active_tiles == NULL));
  int max_layers = 0;
  kd_codestream_window *win;
  for (win=stream_windows; win != NULL; win=win->next)
    {
      if (win->stream != this)
        continue;

      kd_tile *tp;
      kdu_coords t_idx, mod_t_idx;
      for (t_idx.y=0; t_idx.y < win->active_tiles.size.y; t_idx.y++)
        for (t_idx.x=0; t_idx.x < win->active_tiles.size.x; t_idx.x++)
          {
            mod_t_idx = t_idx + win->active_tiles.pos - tile_indices.pos;
            tp = tiles + mod_t_idx.x + mod_t_idx.y*tile_indices.size.x;
            if (tp->comps == NULL)
              {
                tp->t_idx = t_idx + win->active_tiles.pos;
                init_tile(tp);
              }
            assert(!tp->interchange.exists());
            if (!tp->is_active)
              { // Link it into the head of the active tiles list
                tp->is_active = true;
                tp->active_next = active_tiles;
                active_tiles = tp;
              }
            if (tp->num_layers > max_layers)
              max_layers = tp->num_layers;
          }
    }
  return max_layers;
}

/*****************************************************************************/
/*                    kd_stream::include_active_precincts                    */
/*****************************************************************************/

void
  kd_stream::include_active_precincts(kd_active_precinct * &old_list,
                                      kd_active_precinct * &new_head,
                                      kd_active_precinct * &new_tail,
                                      kd_codestream_window *stream_windows,
                                      kd_active_server *active_server,
                                      int *dummy_layer_log_slopes,
                                      bool ignore_relevance_info)
{
  assert(is_active);
  kd_tile *tp;
  try { // In case an exception occurs while accessing the codestream
      kd_codestream_window *win;
      kdu_coords t_idx, mod_t_idx;
      int max_layers, max_resolutions, discard_levels, r;
      kd_tile_comp *tc;
      kd_resolution *rp;
      kdu_dims p_region;
      kdu_coords p_idx, mod_p_idx;
      kd_active_precinct *active;
      kd_precinct *prec;

      for (win=stream_windows; win != NULL; win=win->next)
        {
          if (win->stream != this)
            continue;
          close_active_tiles(); // So we can change geometry
          interchange.apply_input_restrictions(0,0,0,0,&win->active_region,
                                               KDU_WANT_OUTPUT_COMPONENTS);

          // Start by opening tiles and finding max discard levels
          max_resolutions = 1;
          for (t_idx.y=0; t_idx.y < win->active_tiles.size.y; t_idx.y++)
            for (t_idx.x=0; t_idx.x < win->active_tiles.size.x; t_idx.x++)
              {
                mod_t_idx = t_idx + win->active_tiles.pos - tile_indices.pos;
                tp = tiles + mod_t_idx.x + mod_t_idx.y*tile_indices.size.x;
                assert(tp->is_active && (tp->comps != NULL));
                assert(!(tp->source.exists() || tp->interchange.exists()));
                tp->interchange = interchange.open_tile(tp->t_idx);
                int cn, comp_idx;
                int num_scan_comps = win->num_codestream_components;
                int *scan_comps = win->codestream_components;
                if (win->num_context_components > 0)
                  {
                    int nsi, nso, nbi, nbo;
                    tp->interchange.set_components_of_interest(
                                                win->num_context_components,
                                                win->context_components);
                    tp->interchange.get_mct_block_info(0,0,nsi,nso,nbi,nbo);
                    num_scan_comps = nsi;
                    scan_comps = win->get_scratch_components(num_scan_comps);
                    tp->interchange.get_mct_block_info(0,0,nsi,nso,nbi,nbo,
                                            NULL,NULL,NULL,NULL,scan_comps);
                  }
                for (cn=0; cn < num_scan_comps; cn++)
                  {
                    comp_idx = scan_comps[cn];
                    assert((comp_idx >= 0) && (comp_idx < num_components));
                    if (((comp_idx < 3) && win->expand_ycc && tp->uses_ycc) ||
                        win->component_ranges.test(comp_idx))
                      {
                        tc = tp->comps + comp_idx;
                        if (tc->num_resolutions > max_resolutions)
                          max_resolutions = tc->num_resolutions;
                      }
                  }
              }

          // Now build the precinct lists
          discard_levels = max_resolutions - 1;
          bool first_level = true;
          do { // Work from low to high resolution, across all tiles
              kd_active_precinct *res_head = NULL;
              for (t_idx.y=0; t_idx.y < win->active_tiles.size.y; t_idx.y++)
                for (t_idx.x=0; t_idx.x<win->active_tiles.size.x; t_idx.x++)
                  {
                    mod_t_idx = t_idx+win->active_tiles.pos-tile_indices.pos;
                    tp = tiles + mod_t_idx.x + mod_t_idx.y*tile_indices.size.x;
                    assert(tp->interchange.exists());
                    max_layers = tp->num_layers;

                    int cn, comp_idx;
                    int num_scan_comps = win->num_codestream_components;
                    int *scan_comps = win->codestream_components;
                    if (win->num_context_components > 0)
                      {
                        int nsi, nso, nbi, nbo;
                        tp->interchange.get_mct_block_info(0,0,
                                                           nsi,nso,nbi,nbo);
                        num_scan_comps = nsi;
                        scan_comps=win->get_scratch_components(num_scan_comps);
                        tp->interchange.get_mct_block_info(0,0,nsi,nso,nbi,nbo,
                                            NULL,NULL,NULL,NULL,scan_comps);
                      }
                    for (cn=0; cn < num_scan_comps; cn++)
                      {
                        comp_idx = scan_comps[cn];
                        assert((comp_idx >= 0) && (comp_idx < num_components));
                        if (!(((comp_idx < 3) &&
                               win->expand_ycc && tp->uses_ycc) ||
                              win->component_ranges.test(comp_idx)))
                          continue;
                        tc = tp->comps + comp_idx;
                        assert(!tc->source);
                        if (first_level)
                          tc->interchange =
                            tp->interchange.access_component(comp_idx);
                        r = (tc->num_resolutions-1) - discard_levels;
                        if (first_level)
                          {
                            assert(r <= 0);
                            if (r < 0)
                              r = 0; // Include first resolution no matter what
                          }
                        else if (r <= 0)
                          continue; // First resolution included previously

                        double component_relevance = 1.0;
                        if (!ignore_relevance_info)
                          component_relevance =
                            tp->interchange.find_component_gain_info(comp_idx,
                                                                     true) /
                            tp->interchange.find_component_gain_info(comp_idx,
                                                                     false);
                        rp = tc->res + r;
                        rp->interchange = tc->interchange.access_resolution(r);
                        assert(!rp->source);
                        rp->interchange.get_valid_precincts(p_region);
                        for (p_idx.y=0; p_idx.y < p_region.size.y; p_idx.y++)
                          for (p_idx.x=0; p_idx.x < p_region.size.x; p_idx.x++)
                            {
                              mod_p_idx = p_idx + p_region.pos; // Absolute
                              prec = rp->access_precinct(mod_p_idx);
                              double relevance = component_relevance *
                                rp->interchange.get_precinct_relevance(
                                                                    mod_p_idx);
                              if ((active=prec->active) == NULL)
                                { // Create new active precinct element.
                                  prec->active =
                                    active = active_server->get_precinct();
                                  active->cache = prec;
                                  active->p_idx = mod_p_idx;
                                  active->current_bytes =
                                    active->prev_bytes = prec->cached_bytes;
                                  active->current_packets =
                                    active->next_layer = active->prev_packets =
                                      prec->cached_packets;
                                  active->max_packets = max_layers;
                                  active->res = NULL; // Signal need to link
                                }
                              else if (active->res == NULL)
                                { // Unlink existing precinct from old list
                                  if (active->prev == NULL)
                                    {
                                      assert(active == old_list);
                                      old_list = active->next;
                                    }
                                  else
                                    active->prev->next = active->next;
                                  if (active->next != NULL)
                                    active->next->prev = active->prev;
                                }
                              else if (relevance > active->relevance)
                                { // Have to explicitly set log-relevance
                                  int i;
                                  for (i=0; i<KD_NUM_RELEVANCE_THRESHOLDS; i++)
                                    if (relevance > kd_relevance_thresholds[i])
                                      {
                                        assert(kd_log_relevance[i] >=
                                               active->log_relevance);
                                        active->log_relevance =
                                          kd_log_relevance[i];
                                        active->relevance = relevance;
                                        break;
                                      }
                                }

                              assert(active->p_idx == mod_p_idx);
                              if (active->res == NULL)
                                { // Link into new list
                                  active->res = rp;
                                  active->stream_id = stream_id;
                                  active->relevance = relevance;
                                  active->log_relevance = 0; // Temporary
                                  if (dummy_layer_log_slopes != NULL)
                                    active->layer_log_slopes =
                                      dummy_layer_log_slopes;
                                  else
                                    {
                                      assert(layer_log_slopes != NULL);
                                      active->layer_log_slopes =
                                        layer_log_slopes;
                                    }
                                  active->prev = new_tail;
                                  active->next = NULL;
                                  if (new_tail == NULL)
                                    new_head = new_tail = active;
                                  else
                                    new_tail = new_tail->next = active;
                                  if (res_head == NULL)
                                    res_head = active;
                                }
                              else
                                assert(active->res == rp); // Already linked
                            }
                      }
                  }

              // At this point, we have included all active precincts for the
              // current resolution.  Let's see if we should order them based
              // on the log-relevance info.
              if ((res_head != NULL) && !ignore_relevance_info)
                order_precinct_list_by_relevance(res_head,new_head,new_tail);

              // Advance to the next resolution
              first_level = false;
              discard_levels--;
            } while (discard_levels >= win->active_discard_levels);
        }
    }
  catch (int val)
    {
      close_active_tiles();
      interchange.apply_input_restrictions(0,0,0,0,NULL,
                                           KDU_WANT_OUTPUT_COMPONENTS);
      throw val;
    }
  
  // Release resources
  close_active_tiles();
  interchange.apply_input_restrictions(0,0,0,0,NULL,
                                       KDU_WANT_OUTPUT_COMPONENTS);
}


/* ========================================================================= */
/*                                 kd_serve                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                            kd_serve::kd_serve                             */
/*****************************************************************************/

kd_serve::kd_serve(kdu_serve *owner)
{
  this->owner = owner;
  chunk_server = NULL;
  active_server = NULL;
  target = NULL;
  dummy_layer_slopes = NULL;
  streams = NULL;
  metatree = NULL;
  extra_data_head = extra_data_tail = NULL;
  num_active_codestreams = 0;
  stream_windows = last_stream_window = free_stream_windows = NULL;
  active_precincts = precinct_ptr = NULL;
  active_metagroups = meta_ptr = NULL;
}

/*****************************************************************************/
/*                            kd_serve::~kd_serve                            */
/*****************************************************************************/

kd_serve::~kd_serve()
{
  if (chunk_server != NULL)
    delete chunk_server;
  if (active_server != NULL)
    delete active_server;

  while ((last_stream_window=stream_windows) != NULL)
    {
      stream_windows = last_stream_window->next;
      delete last_stream_window;
    }

  while ((last_stream_window=free_stream_windows) != NULL)
    {
      free_stream_windows = last_stream_window->next;
      delete last_stream_window;
    }

  kd_stream *sp;
  while ((sp=streams) != NULL)
    {
      streams = sp->next;
      delete sp;
    }

  kd_meta *mp;
  while ((mp=metatree) != NULL)
    {
      metatree = mp->next_in_bin;
      delete mp; // Recursively deletes the `phld'-linked sub-trees.
    }

  if (dummy_layer_slopes != NULL)
    delete[] dummy_layer_slopes;
}

/*****************************************************************************/
/*                            kd_serve::initialize                           */
/*****************************************************************************/

void
  kd_serve::initialize(kdu_serve_target *target, int max_chunk_size,
                       bool ignore_relevance_info)
{
  chunk_server = new kd_chunk_server(max_chunk_size);
  active_server = new kd_active_server();
  this->target = target;
  this->ignore_relevance_info = ignore_relevance_info;
  this->streams = NULL;
  dummy_layer_slopes = NULL;
  max_dummy_layer_slopes = 0;

  // Configure an initial window; may be changed before we access any data
  current_window.init();
  int num_stream_ranges=0;
  int *stream_ranges = target->get_codestream_ranges(num_stream_ranges);
  if (num_stream_ranges != 0)
    current_window.codestreams.add(stream_ranges[0]);
  current_window.components.init();
  current_window.resolution = kdu_coords(0,0); // Headers only
  current_window.region.pos = current_window.region.size = kdu_coords(0,0);
  window_changed = true;
  max_active_layers = 0;
  completed_layers = 0;
  active_precincts = precinct_ptr = NULL;
  active_metagroups = meta_ptr = NULL;
  active_threshold = next_active_threshold = INT_MIN;
  scan_first_layer = true;
  active_lists_invalid = true;
  extra_data_head = extra_data_tail = NULL;
  extra_data_bytes = 0;

  // Now we need to build the meta-data tree
  const kds_metagroup *mgroup=target->get_metatree();
  assert(mgroup != NULL); // Even for raw streams, need one group with 0 length
  kd_meta *mnew, *mtail=NULL;
  metatree=NULL;
  total_meta_bytes = 0;
  int bin_off=0;
  for (; mgroup != NULL; mgroup=mgroup->next)
    {
      mnew = new kd_meta;
      total_meta_bytes += mnew->init(mgroup,0,bin_off,0);
      bin_off += mnew->num_bytes;
      mnew->prev_in_bin = mtail;
      if (mtail == NULL)
        metatree = mtail = mnew;
      else
        mtail = mtail->next_in_bin = mnew;
    }
}

/*****************************************************************************/
/*                      kd_serve::process_window_changes                     */
/*****************************************************************************/

void
  kd_serve::process_window_changes()
{
  if (!window_changed)
    return;

  // Save a copy of the window, as requested, prior to translation
  last_translated_window.copy_from(current_window);

  // Start by making any active code-streams inactive and recycling all
  // codestream windows
  kd_stream *str;
  for (str=streams; str != NULL; str=str->next)
    {
      kd_tile *tp;
      while ((tp=str->active_tiles) != NULL)
        {
          str->active_tiles = tp->active_next;
          tp->active_next = NULL;
          tp->is_active = false;
          if (tp->interchange.exists())
            tp->interchange.close();
        }
      str->is_active = false;
    }
  while ((last_stream_window=stream_windows) != NULL)
    {
      stream_windows = last_stream_window->next;
      last_stream_window->next = free_stream_windows;
      free_stream_windows = last_stream_window;
      last_stream_window->stream = NULL;
      last_stream_window->stream_idx = -1;
    }
  num_active_codestreams = 0;

  // Ensure consistency amongst window dimensions
  if ((current_window.resolution.x <= 0) || (current_window.resolution.y <= 0))
    current_window.resolution = kdu_coords(0,0);
  if ((current_window.region.size.x<=0) || (current_window.region.size.y<=0))
    current_window.region.size = current_window.region.pos = kdu_coords(0,0);

  // Make copies of the requested codestreams and codestream contexts so that
  // we can limit them when building the actual window to be served.
  context_set.copy_from(current_window.contexts);
  current_window.contexts.init();
  stream_set.copy_from(current_window.codestreams);
  current_window.codestreams.init();

  // Build an initial set of codestream windows based on the codestreams
  // which are explicitly requested
  kd_codestream_window *win;
  int c, csn, range_idx, num_stream_ranges;
  const int *stream_ranges = target->get_codestream_ranges(num_stream_ranges);
  if (stream_set.is_empty() && context_set.is_empty())
    stream_set.add(stream_ranges[0]);
  kdu_sampled_range csrg, rg;
  kdu_long total_area=0;
  kdu_long active_area, max_active_area=0;
  for (csn=0; !(csrg=stream_set.get_range(csn)).is_empty(); csn++)
    {
      for (range_idx=0; range_idx < num_stream_ranges; range_idx++)
        {
          int first_id = stream_ranges[2*range_idx];
          int last_id = stream_ranges[2*range_idx+1];
          if ((first_id > csrg.to) || (last_id < csrg.from))
            continue; // No intersection
          rg = csrg;
          if (first_id > rg.from)
            {
              c = 1 + (first_id-rg.from-1) / rg.step;
              rg.from += c * rg.step;
              assert(rg.from >= first_id);
            }
          if (last_id < rg.to)
            rg.to = last_id;
          if (rg.is_empty())
            continue;
          for (c=rg.from; c <= rg.to; c += rg.step)
            { // Augment active codestreams array
              str = add_active_codestream(c);
              if (str == NULL)
                continue; // Cannot deliver this codestream
              if (!str->is_active)
                { // Adding codestream for the first time
                  str->is_active = true;
                  current_window.codestreams.add(c);
                  win = add_stream_window();
                  total_area +=
                    win->initialize(str,current_window.resolution,
                                    current_window.region,
                                    current_window.round_direction,
                                    current_window.components);
                  active_area = win->active_region.area();
                  if (active_area > max_active_area)
                    max_active_area = active_area;
                }
              if (num_active_codestreams == KD_MAX_ACTIVE_CODESTREAMS)
                break;
            }
        }
    }

  // Now process any requested codestream contexts
  range_idx = 0; // Here it stands for the current context range
  for (csn=0; !(csrg=context_set.get_range(csn)).is_empty(); csn++)
    {
      // Copy all contexts in the range, for which an expansion exists
      rg = csrg;
      rg.to = rg.from - 1;
      for (c=csrg.from; c <= csrg.to; c+=csrg.step)
        {
          if (target->get_num_context_members(rg.context_type,c,
                                              rg.remapping_ids) <= 0)
            break;
          rg.to = c;
        }
      if (rg.is_empty())
        continue; // Try the next context range

      // Add this context range to the current window, applying its expansion
      current_window.contexts.add(rg,false); // Do not allow merging
      kdu_sampled_range *rg_ref =
        current_window.contexts.access_range(range_idx);
      kdu_range_set *expansion =
        current_window.create_context_expansion(range_idx);
      if (expansion != rg_ref->expansion)
        assert(0);
      kdu_sampled_range aux_range;
      aux_range.context_type = KDU_JPIP_CONTEXT_TRANSLATED;
      aux_range.remapping_ids[0] = range_idx;
      aux_range.remapping_ids[1] = 0;
      for (c=rg.from; c <= rg.to; c+=rg.step, aux_range.remapping_ids[1]++)
        {
          int num_members = target->get_num_context_members(rg.context_type,c,
                                                            rg.remapping_ids);
          int stream_idx, member_idx;
          for (member_idx=0; member_idx < num_members; member_idx++)
            {
              stream_idx = target->get_context_codestream(rg.context_type,c,
                                                          rg.remapping_ids,
                                                          member_idx);
              expansion->add(stream_idx);
              
              // Now see if we should add a codestream window
              kdu_coords res = current_window.resolution;
              kdu_dims reg = current_window.region;
              if (!target->perform_context_remapping(rg.context_type,c,
                                                     rg.remapping_ids,
                                                     member_idx,res,reg))
                continue; // Cannot translate the view window
              str = add_active_codestream(stream_idx);
              if (str == NULL)
                continue; // Cannot deliver this codestream
              aux_range.from = aux_range.to = stream_idx;
              current_window.codestreams.add(aux_range);
              if (str->is_active && !reg)
                continue; // This window has zero size and codestream already
                          // included; no need to create the stream window
              str->is_active = true;

              win = add_stream_window();
              int num_comps=0;
              const int *comps =
                target->get_context_components(rg.context_type,c,
                                               rg.remapping_ids,member_idx,
                                               num_comps);
              total_area +=
                win->initialize(str,res,reg,
                                current_window.round_direction,
                                current_window.components,num_comps,comps);
              active_area = win->active_region.area();
              if (active_area > max_active_area)
                max_active_area = active_area;
              win->context_type = rg.context_type;
              win->context_idx = c;
              win->member_idx = member_idx;
              win->remapping_ids[0] = rg.remapping_ids[0];
              win->remapping_ids[1] = rg.remapping_ids[1];
            }
        }
      range_idx++;
    }

  // Now go back through the codestream windows, adjusting the size of the
  // view window until the complexity limits are satisfied

  while ((total_area > KD_MAX_WINDOW_AREA) &&
         (max_active_area > (1<<14)) &&
         (current_window.region.size.x > 128) &&
         (current_window.region.size.y > 128))
    {
      double scale = KD_MAX_WINDOW_AREA / (double) total_area;
      double min_scale = ((double)(1<<14)) / (double) max_active_area;
      if (scale < min_scale)
        scale = min_scale;
      if (scale >= 1.0)
        break;

      double scale_x = sqrt(scale);
      double min_scale_x = 128.0 / current_window.region.size.x;
      if (scale_x < min_scale_x)
        scale_x = min_scale_x;
      if (scale_x > 1.0)
        scale_x = 1.0;
      double scale_y = scale / scale_x;
      double min_scale_y = 128.0 / current_window.region.size.y;
      if (scale_y < min_scale_y)
        scale_y = min_scale_y;
      if (scale_y > 1.0)
        scale_y = 1.0;
      double centre_x =
        current_window.region.pos.x + 0.5*current_window.region.size.x;
      double centre_y =
        current_window.region.pos.y + 0.5*current_window.region.size.y;
      if ((scale_x == 1.0) && (scale_y == 1.0))
        break; // Just in case
      if (scale_x < 1.0)
        {
          current_window.region.pos.x = (int)
            (centre_x - 0.5*scale_x*current_window.region.size.x);
          current_window.region.size.x = (int)
            (scale_x*current_window.region.size.x) - 1;
        }
      if (scale_y < 1.0)
        {
          current_window.region.pos.y = (int)
            (centre_y - 0.5*scale_y*current_window.region.size.y);
          current_window.region.size.y = (int)
            (scale_y*current_window.region.size.y) - 1;
        }
      total_area = max_active_area = 0;
      for (win=stream_windows; win != NULL; win=win->next)
        {
          if (win->context_type == 0)
            {
              total_area +=
                win->initialize(win->stream,current_window.resolution,
                                current_window.region,
                                current_window.round_direction,
                                current_window.components);
              active_area = win->active_region.area();
              if (active_area > max_active_area)
                max_active_area = active_area;
            }
          else
            {
              kdu_coords res = current_window.resolution;
              kdu_dims reg = current_window.region;
              target->perform_context_remapping(win->context_type,
                                                win->context_idx,
                                                win->remapping_ids,
                                                win->member_idx,
                                                res,reg);
              int num_comps=0;
              const int *comps =
                target->get_context_components(win->context_type,
                                               win->context_idx,
                                               win->remapping_ids,
                                               win->member_idx,num_comps);
              total_area +=
                win->initialize(win->stream,res,reg,
                                current_window.round_direction,
                                current_window.components,num_comps,comps);
              active_area = win->active_region.area();
              if (active_area > max_active_area)
                max_active_area = active_area;
            }
        }
    }

  // Adjust status flags
  max_active_layers = 0; // Will adjust this in `generate_active_lists'
  window_changed = false;
  active_lists_invalid = true;
}

/*****************************************************************************/
/*                       kd_serve::generate_active_lists                     */
/*****************************************************************************/

void
  kd_serve::generate_active_lists()
{
  if (!active_lists_invalid)
    return;
  if (window_changed)
    process_window_changes();

  // Temporarily move old active precinct list, setting the `active->res'
  // fields all to NULL to signal the fact that the elements will need to be
  // linked back into a new active list or else recycled.
  kd_active_precinct *active, *old_list = active_precincts;
  active_precincts = NULL;
  for (active=old_list; active != NULL; active=active->next)
    active->res = NULL;

  assert(max_active_layers == 0);
  if (!current_window.metadata_only)
    {      
      kd_stream *str;

      // Start by finding active tiles and also `max_active_layers'
      int max_layers;
      for (str=streams; str != NULL; str=str->next)
        {
          if (!str->is_active)
            continue;
          max_layers = str->include_active_tiles(stream_windows);
          if (max_layers > max_active_layers)
            max_active_layers = max_layers;
        }
      if (max_active_layers > 0)
        {
          if (current_window.max_layers > 0)
            {
              if (current_window.max_layers < max_active_layers)
                max_active_layers = current_window.max_layers;
              current_window.max_layers = max_active_layers;
            }
        }

      // Augmenting the layer log-slope info as appropriate, and find the
      // maximum layer log slope threshold over all active streams.
      bool use_dummy_layer_slopes = ignore_relevance_info;
      for (str=streams; (str!=NULL) && !use_dummy_layer_slopes; str=str->next)
        {
          if (!str->is_active)
            continue;
          if ((str->layer_log_slopes == NULL) || (str->max_layer_slopes <= 0))
            use_dummy_layer_slopes = true;
          else if (max_active_layers >= str->max_layer_slopes)
            {
              int i, *new_slopes = new int[max_active_layers+1];
              for (i=0; i < str->max_layer_slopes; i++)
                new_slopes[i] = str->layer_log_slopes[i];
              int val = new_slopes[i-1] - (1<<15);
              str->max_layer_slopes = max_active_layers+1;
              delete[] str->layer_log_slopes;
              str->layer_log_slopes = new_slopes;
              for (; i < str->max_layer_slopes; i++, val -= (1<<15))
                str->layer_log_slopes[i] = val;
            }
        }

      if (use_dummy_layer_slopes)
        {
          if (max_active_layers >= max_dummy_layer_slopes)
            {
              if (dummy_layer_slopes != NULL)
                delete[] dummy_layer_slopes;
              max_dummy_layer_slopes = max_active_layers+1;
              dummy_layer_slopes = new int[max_dummy_layer_slopes];
              int i, val = 0;
              for (i=0; i < max_dummy_layer_slopes; i++, val -= (1<<15))
                dummy_layer_slopes[i] = val;
            }
        }

      // Create new active list, moving old elements to new list where
      // appropriate
      kd_active_precinct *active_tail = NULL;

      for (str=streams; str != NULL; str=str->next)
        if (str->is_active)
          str->include_active_precincts(old_list,active_precincts,active_tail,
                           stream_windows,active_server,
                           ((use_dummy_layer_slopes)?dummy_layer_slopes:NULL),
                           ignore_relevance_info);
    }

  // Discard any remaining elements from the old active list, closing any
  // open precinct interfaces and moving the state information into the
  // much smaller `kds_precinct' structure.
  for (active=old_list; active != NULL; active=active->next)
    {
      assert(active->res == NULL);
      assert(active->cache->active == active);
      active->cache->cached_bytes = active->current_bytes;
      active->cache->cached_packets = active->current_packets;
      if (active->interchange.exists())
        active->interchange.close();
      active->cache->active = NULL;
    }
  active_server->release_precincts(old_list);

  // Process the metadata now; this is a recursive process
  kd_meta *mscan, *mstart;
  for (mscan=metatree; mscan != NULL; mscan=mscan->next_in_bin)
    mscan->find_active_scope_and_sequence(current_window,stream_windows);

  active_metagroups = mstart = NULL;
  for (mscan=metatree; (mscan != NULL) && mscan->in_scope;
       mstart=mscan, mscan=mscan->next_in_bin)
    {
      if (mstart == NULL)
        { active_metagroups = mstart = mscan; mscan->active_next = NULL; }
      mscan->include_active_groups(mstart); // Recursive function
    }

  // Initialize the packet simulation state variables
  active_lists_invalid = false;
  precinct_ptr = active_precincts;
  meta_ptr = active_metagroups;
  next_active_threshold = INT_MIN;
  active_threshold = INT_MAX; // Will be ignored, since `scan_first_layer' true
  scan_first_layer = true;
  completed_layers = 0;
}

/*****************************************************************************/
/*                    kd_serve::simulate_packet_generation                   */
/*****************************************************************************/

int
  kd_serve::simulate_packet_generation(int suggested_body_bytes,
                                       int max_body_bytes, bool align)
{
  if (suggested_body_bytes > max_body_bytes)
    suggested_body_bytes = max_body_bytes;
  kd_stream *str=NULL;
  kd_tile *tp;
  kd_tile_comp *tc;
  kd_resolution *rp;
  bool is_significant;
  int cum_packets, cum_bytes, increment, thresh;
  int simulated_bytes = 0;
  bool streams_locked = false;
  while ((simulated_bytes < suggested_body_bytes) &&
         (active_threshold != INT_MIN))
    {
      kd_active_precinct *scan = precinct_ptr;
      while (scan != NULL)
        {
          // First see if we need to determine the number of cached packets
          // from the number of cached bytes.
          if (scan->current_packets == 0xFFFF)
            { // Special value means number of packets not currently known
              assert((scan->current_bytes > 0) && !scan->interchange);
              rp=scan->res; tc=rp->tile_comp; tp=tc->tile; str=tp->stream;
              scan->interchange=rp->interchange.open_precinct(scan->p_idx);
              if (!scan->interchange.check_loaded())
                { // Do, unless precinct has no samples; rare but possible
                  if (!streams_locked)
                    {
                      target->lock_codestreams(num_active_codestreams,
                                               active_codestream_indices,this);
                      streams_locked = true;
                    }
                  assert(tp->is_active);
                  if (!tp->source)
                    tp->source = str->source.open_tile(tp->t_idx);
                  if (!tc->source)
                    tc->source = tp->source.access_component(tc->c_idx);
                  if (!rp->source)
                    rp->source = tc->source.access_resolution(rp->r_idx);
                  load_blocks(scan->interchange,rp->source);
                }
              scan->current_packets = 0;
              for (cum_packets=1; cum_packets <= tp->num_layers; cum_packets++)
                { 
                  cum_bytes = 0;
                  scan->interchange.size_packets(cum_packets,cum_bytes,
                                                 is_significant);
                  if (cum_bytes <= scan->current_bytes)
                    {
                      scan->current_packets = (kdu_uint16) cum_packets;
                      scan->current_packet_bytes = (kdu_uint16) cum_bytes;
                    }
                  if (cum_bytes >= scan->current_bytes)
                    break;
                }
              scan->prev_packets=scan->next_layer = scan->current_packets;
              scan->prev_packet_bytes = scan->current_packet_bytes;
              if ((cum_bytes > scan->current_bytes) &&
                  (scan->current_packets < scan->max_packets))
                scan->interchange.restart(); // Start simulating from scratch
              else if (scan->current_packets == scan->max_packets)
                {
                  assert(tp->completed_precincts < tp->total_precincts);
                  tp->completed_precincts++;
                  if ((tp->completed_precincts == tp->total_precincts) &&
                      tp->header_fully_dispatched)
                    {
                      assert(str->completed_tiles < str->total_tiles);
                      str->completed_tiles++;
                    }
                }
            }

          assert(scan->next_layer >= scan->current_packets);
          if ((scan->current_packets == scan->max_packets) ||
              (max_active_layers <= (int) scan->next_layer))
            { // All done with this precinct
              scan = scan->next; continue;
            }

          // Adjust next active threshold as appropriate
          thresh = scan->layer_log_slopes[scan->next_layer+1]
                 + scan->log_relevance + 1;
          if (thresh > next_active_threshold)
            next_active_threshold = thresh;

          // Now check to see if the present precinct is a candidate for
          // including more data.
          if (scan_first_layer)
            {
              if (scan->next_layer != 0)
                { scan = scan->next; continue; }
            }
          else if (active_threshold >
                   (scan->layer_log_slopes[scan->next_layer] +
                    scan->log_relevance))
            { scan = scan->next; continue; }

          // Perhaps the precinct still needs to be loaded
          if (!scan->interchange)
            {
              rp=scan->res; tc=rp->tile_comp; tp=tc->tile; str=tp->stream;
              scan->interchange = rp->interchange.open_precinct(scan->p_idx);
              if (!scan->interchange.check_loaded())
                { // Do unless precinct has no samples; rare but possible
                  if (!streams_locked)
                    {
                      target->lock_codestreams(num_active_codestreams,
                                               active_codestream_indices,this);
                      streams_locked = true;
                    }
                  if (!tp->source)
                    tp->source = str->source.open_tile(tp->t_idx);
                  if (!tc->source)
                    tc->source = tp->source.access_component(tc->c_idx);
                  if (!rp->source)
                    rp->source = tc->source.access_resolution(rp->r_idx);
                  load_blocks(scan->interchange,rp->source);
                }
              scan->prev_packet_bytes = scan->current_packet_bytes = 0;
              if (scan->prev_packets > 0)
                { // Find out num bytes corresponding to the packets
                  // whose bytes are fully cached.
                  cum_bytes = 0; cum_packets = scan->prev_packets;
                  scan->interchange.size_packets(cum_packets,cum_bytes,
                                                 is_significant);
                  scan->prev_packet_bytes =
                    scan->current_packet_bytes = (kdu_uint16) cum_bytes;
                  if (scan->current_bytes < scan->current_packet_bytes)
                    scan->prev_bytes = scan->current_bytes =
                      scan->current_packet_bytes;
                }
            }
          
          // Now we are finally ready to do some simulation to see if this
          // packet can contribute an increment.
          assert(scan->next_layer < scan->max_packets);
          cum_bytes = 0; cum_packets = scan->next_layer+1;
          scan->interchange.size_packets(cum_packets,cum_bytes,is_significant);
          assert(cum_packets == (scan->next_layer+1));
          if ((cum_packets < (int) scan->max_packets) &&
              ((cum_bytes < (int) (scan->prev_bytes+8)) || !is_significant))
            { // Must generate at least 8 bytes, except when completing the
              // precinct.  We will increase `next_layer', but not
              // `current_packets'.
              scan->next_layer++;
              continue; // Continue examining precinct in the next layer
            }
          increment = (cum_bytes-scan->current_bytes);
          simulated_bytes += increment;
          if (simulated_bytes >= suggested_body_bytes)
            { // Need to decide how much of packet to send and finish here
              if ((simulated_bytes > max_body_bytes) &&
                  (increment > (max_body_bytes>>1)) &&
                  ((!align) || (cum_packets <= (scan->prev_packets+1))))
                { // Include part of packet here.
                  cum_bytes -= (simulated_bytes-max_body_bytes);
                  scan->current_bytes = (kdu_uint16) cum_bytes;
                  simulated_bytes = max_body_bytes;
                }
              else if ((align && (simulated_bytes == increment)) ||
                       ((simulated_bytes <= max_body_bytes) &&
                        ((simulated_bytes == increment) ||
                         (increment < (suggested_body_bytes >> 2)))))
                { // Include whole packet here
                  scan->next_layer =
                    scan->current_packets = (kdu_uint16) cum_packets;
                  scan->current_bytes =
                    scan->current_packet_bytes = (kdu_uint16) cum_bytes;
                }
              break;
            }
          else
            { // Include whole packet comfortably
              scan->next_layer =
                scan->current_packets = (kdu_uint16) cum_packets;
              scan->current_bytes =
                scan->current_packet_bytes = (kdu_uint16) cum_bytes;
            }
        }

      precinct_ptr = scan;

      if (precinct_ptr == NULL)
        { // Start processing from scratch
          precinct_ptr = active_precincts;
          active_threshold = next_active_threshold;
          next_active_threshold = INT_MIN;
          scan_first_layer = false;
        }
    }

  if (streams_locked)
    { // Close any open source tiles and unlock the codestreams
      for (str=streams; str != NULL; str=str->next)
        for (tp=str->active_tiles; tp != NULL; tp=tp->active_next)
          if (tp->source.exists())
            tp->source.close();
      target->release_codestreams(num_active_codestreams,
                                  active_codestream_indices,this);

      // Mark any now-defunct source code-stream interfaces as closed.
      kd_active_precinct *scan;
      for (scan=active_precincts; scan != NULL; scan=scan->next)
        {
          rp = scan->res;
          if (rp->source.exists())
            {
              rp->source = kdu_resolution(NULL);
              tc = rp->tile_comp;
              tc->source = kdu_tile_comp(NULL);
              assert(!tc->tile->source); // Should have been closed above.
            }
        }
    }

  return simulated_bytes;
}

/*****************************************************************************/
/*                      kd_serve::generate_header_increment                  */
/*****************************************************************************/

kds_chunk *
  kd_serve::generate_header_increment(kds_chunk *chunk,
                                      kds_id_encoder *id_encoder,
                                      kd_stream *str, int tnum, int skip_bytes,
                                      int increment, bool is_final,
                                      bool decouple_chunks)
{
  assert((chunk != NULL) && (chunk->next == NULL));

  out.open(chunk,skip_bytes);

  kds_chunk *tail=chunk;
  int restore_max_bytes=0;
  do {
      int max_id_length;
      if (tnum < 0)
        max_id_length = id_encoder->encode_id(NULL,KDU_MAIN_HEADER_DATABIN,
                                              str->stream_id,0,true);
      else
        max_id_length = id_encoder->encode_id(NULL,KDU_TILE_HEADER_DATABIN,
                                              str->stream_id,tnum,true);
      int xfer_bytes = tail->max_bytes - tail->num_bytes;
      xfer_bytes -= max_id_length;
      xfer_bytes -= simulate_var_length(skip_bytes);
      xfer_bytes -= simulate_var_length(increment);
      if (xfer_bytes >= increment)
        xfer_bytes = increment;
      else if (xfer_bytes < 8)
        { // Avoid sending small increments just to fit the chunk
          if (tail->num_bytes == tail->prefix_bytes)
            {
              kdu_error e; e << "You must use larger chunks or smaller chunk "
              "prefixes for `kdu_serve::generate_increments'.";
            }
          tail->max_bytes = tail->num_bytes;
          tail = tail->next = chunk_server->get_chunk();
          if (decouple_chunks)
            id_encoder->decouple();
          continue;
        }
      if (tnum < 0)
        tail->num_bytes +=
          id_encoder->encode_id(tail->data+tail->num_bytes,
                                KDU_MAIN_HEADER_DATABIN,str->stream_id,0,
                                (xfer_bytes == increment) && is_final);
      else
        tail->num_bytes +=
          id_encoder->encode_id(tail->data+tail->num_bytes,
                                KDU_TILE_HEADER_DATABIN,str->stream_id,tnum,
                                (xfer_bytes == increment) && is_final);
      output_var_length(tail,skip_bytes);
      output_var_length(tail,xfer_bytes);
      restore_max_bytes = tail->max_bytes;
      assert(tail->max_bytes >= (tail->num_bytes + xfer_bytes));
      tail->max_bytes = tail->num_bytes + xfer_bytes;
      kd_active_bin *ttmp = active_server->get_bin();
      ttmp->next = tail->other_bins;
      tail->other_bins = ttmp;
      if (tnum < 0)
        ttmp->stream = str;
      else
        ttmp->tile = str->tiles+tnum;
      ttmp->prev_bytes = skip_bytes;

      increment -= xfer_bytes;
      skip_bytes += xfer_bytes;
      if (increment > 0)
        {
          tail = tail->next = chunk_server->get_chunk();
          if (decouple_chunks)
            id_encoder->decouple();
        }
    } while ((increment > 0) || (restore_max_bytes==0));
 
  kdu_params *ichg_params = str->interchange.access_siz();
  if (tnum < 0)
    out.put(KDU_SOC);
  ichg_params->generate_marker_segments(&out,tnum,0);
  out.close();
  tail->max_bytes = restore_max_bytes;
  assert(tail->max_bytes >= tail->num_bytes);
  return tail;
}

/*****************************************************************************/
/*                      kd_serve::generate_meta_increment                    */
/*****************************************************************************/

kds_chunk *
  kd_serve::generate_meta_increment(kd_meta *meta,
                                    kds_id_encoder *id_encoder,
                                    int range_length, kds_chunk *chunk,
                                    bool decouple_chunks)
{
  if (meta->metagroup->is_rubber_length)
    { kdu_error e; e << "The present implementation of `kdu_serve' cannot "
      "dispatch meta data-bins whose length cannot be determined up front "
      "(e.g., rubber length JP2 box in the original file).";
    }
  int range_start = meta->dispatched_bytes;
  meta->dispatched_bytes += range_length;
  assert(meta->dispatched_bytes <= meta->num_bytes);
  bool is_final = false;
  if (meta->dispatched_bytes == meta->num_bytes)
    {
      meta->fully_dispatched = true;
      is_final = meta->metagroup->is_last_in_bin;
    }
  dispatched_meta_bytes += range_length; // Global state
  if ((range_length == 0) && !is_final)
    return chunk; // Meta-data group not at end of its data-bin.
  bool generated_something = false;
  do {
      int id_length =
        id_encoder->encode_id(NULL,KDU_META_DATABIN,0,meta->bin_id,true);
      int xfer_bytes = chunk->max_bytes - chunk->num_bytes;
      xfer_bytes -= id_length;
      xfer_bytes -= simulate_var_length(range_start+meta->bin_offset);
      xfer_bytes -= simulate_var_length(range_length);
      if (xfer_bytes >= range_length)
        xfer_bytes = range_length; // Can put whole thing in message
      else
        { // Can't fit entire range into current message
          if (is_final && ((range_length - xfer_bytes) < 8))
            xfer_bytes = range_length - 8; // Avoid holes of < 8 bytes
          if ((xfer_bytes < 8) ||
              ((xfer_bytes < (chunk->max_bytes>>1)) &&
               (range_length <= (chunk->max_bytes - chunk->prefix_bytes))))
            { // Start packing data at the next chunk
              if (chunk->num_bytes == chunk->prefix_bytes)
                { kdu_error e;
                  e << "You must use larger chunks or smaller chunk "
                       "prefixes for `kdu_serve::generate_increments' "
                       "to be able to create a legal set of data chunks.";
                }
              chunk->max_bytes = chunk->num_bytes; // Freeze chunk
              chunk = chunk->next = chunk_server->get_chunk();
              if (decouple_chunks)
                id_encoder->decouple();
              continue;
            }
        }

      generated_something = true;
      kd_active_bin *btmp = active_server->get_bin();
      btmp->next = chunk->other_bins;
      chunk->other_bins = btmp;
      btmp->meta = meta;
      btmp->prev_bytes = range_start;
      chunk->num_bytes +=
        id_encoder->encode_id(chunk->data + chunk->num_bytes,
                              KDU_META_DATABIN,0,meta->bin_id,
                              is_final && (range_length==xfer_bytes));


      output_var_length(chunk,range_start+meta->bin_offset);
      output_var_length(chunk,xfer_bytes);
      assert(chunk->max_bytes >= (chunk->num_bytes+xfer_bytes));
      if (target->read_metagroup(meta->metagroup,chunk->data+chunk->num_bytes,
                                 range_start,xfer_bytes) != xfer_bytes)
        { kdu_error e; e << "`kdu_serve_target' derived object failed to "
          "return all requested bytes for a group of meta-data boxes.  This "
          "advanced feature (intended to support proxy servers with "
          "partial data) is not supported by the current \"kdu_serve\" "
          "object's implementation.  Alternatively, it may be that the "
          "file has been corrupted or truncated.";
        }
      range_start += xfer_bytes;
      range_length -= xfer_bytes;
      chunk->num_bytes += xfer_bytes;
    } while ((range_length > 0) || !generated_something);
  return chunk;
}

/*****************************************************************************/
/*                        kd_serve::generate_increments                      */
/*****************************************************************************/

kds_chunk *
  kd_serve::generate_increments(int suggested_body_bytes,
                                int &max_body_bytes, bool align,
                                bool use_extended_message_headers,
                                bool decouple_chunks,
                                kds_id_encoder *id_encoder)
{
  if (max_body_bytes <= 0)
    return chunk_server->get_chunk();

  bool main_headers_needed=false;
  kd_stream *str;
  for (str=streams; str != NULL; str=str->next)
    if (str->is_active)
      {
        str->open_active_tiles();
        if (!str->header_fully_dispatched)
          main_headers_needed = true;
      }
  if (current_window.metadata_only)
    main_headers_needed = false;

  if ((completed_layers == max_active_layers) && (extra_data_bytes == 0) &&
      (meta_ptr == NULL) && !main_headers_needed)
    return chunk_server->get_chunk();

  if (suggested_body_bytes > max_body_bytes)
    suggested_body_bytes = max_body_bytes;
  if (suggested_body_bytes < 1)
    suggested_body_bytes = 1; // Make sure something is generated (if possible)

  int simulated_bytes_left = suggested_body_bytes;

  // Count the cost of sending extra data
  suggested_body_bytes -= extra_data_bytes;

  // Count the cost of sending relevant metadata
  kd_meta *mscan;
  for (mscan=meta_ptr;
       (mscan != NULL) && (simulated_bytes_left > 0);
       mscan=mscan->active_next)
    if (mscan->sequence > completed_layers)
      break;
    else if (mscan->in_scope &&
             (mscan->active_length > mscan->dispatched_bytes))
      simulated_bytes_left -= mscan->active_length - mscan->dispatched_bytes;

  if (!current_window.metadata_only)
    {
      // Count the cost of sending all required headers
      for (str=streams; (str != NULL) && (simulated_bytes_left > 0); str=str->next)
        {
          if (!str->is_active)
            continue;
          simulated_bytes_left -=
            str->main_header_bytes - str->dispatched_header_bytes;
          kd_tile *tp;
          for (tp=str->active_tiles; tp != NULL; tp=tp->active_next)
            simulated_bytes_left -=
            tp->header_bytes - tp->dispatched_header_bytes;
        }

      // Simulate precinct packet generation
      simulated_bytes_left -=
        simulate_packet_generation(simulated_bytes_left,
            (max_body_bytes-suggested_body_bytes)+simulated_bytes_left,align);
    }

  // Now we are ready to generate data chunks.  Start by moving extra
  // data chunks onto the output list.
  kds_chunk *chunk_head, *chunk_tail;
  if (id_encoder == NULL)
    id_encoder = &default_id_encoder;
  chunk_head = chunk_tail = NULL;
  while ((suggested_body_bytes > 0) && (extra_data_head != NULL))
    {
      int chunk_bytes = extra_data_head->num_bytes
                      - extra_data_head->prefix_bytes;
      if (chunk_tail == NULL)
        chunk_head = chunk_tail = extra_data_head; // Must send >= 1 chunk
      else
        {
          if (chunk_bytes > suggested_body_bytes)
            break;
          chunk_tail = chunk_tail->next = extra_data_head;
        }
      suggested_body_bytes -= chunk_bytes;
      max_body_bytes -= chunk_bytes;
      extra_data_bytes -= chunk_bytes;
      assert(extra_data_bytes >= 0);
      extra_data_head = extra_data_head->next;
      if (extra_data_head == NULL)
        extra_data_tail = NULL;
      chunk_tail->next = NULL;
    }

  // Now include any metadata which has higher priority than the imagery
  if (decouple_chunks)
    id_encoder->decouple();
  if (chunk_tail == NULL)
    chunk_head = chunk_tail = chunk_server->get_chunk();
  for (; (suggested_body_bytes > 0) && (meta_ptr != NULL) &&
         (meta_ptr->sequence <= completed_layers);
       meta_ptr=meta_ptr->active_next)
    {
      assert(meta_ptr->in_scope);
      if (meta_ptr->fully_dispatched)
        continue;
      assert(meta_ptr->active_length <= meta_ptr->num_bytes);
      int increment = meta_ptr->active_length - meta_ptr->dispatched_bytes;
      if ((increment < 0) ||
          ((increment==0) && (meta_ptr->active_length < meta_ptr->num_bytes)))
        continue; // Nothing to send
      bool incomplete = false;
      if (increment > suggested_body_bytes)
        {
          increment = suggested_body_bytes;
          incomplete = true;
        }
      chunk_tail = generate_meta_increment(meta_ptr,id_encoder,
                                           increment,chunk_tail,
                                           decouple_chunks);
      suggested_body_bytes -= increment;
      max_body_bytes -= increment;
      if (incomplete)
        break;
    }

  // Now include any required header increments.
  if (!current_window.metadata_only)
    {
      for (str=streams; str != NULL; str=str->next)
        {
          if (!str->is_active)
            continue;
          if ((suggested_body_bytes > 0) && !str->header_fully_dispatched)
            {
              int existing_header_bytes = str->dispatched_header_bytes;
              int increment = str->main_header_bytes - existing_header_bytes;
              if (increment > suggested_body_bytes)
                increment = suggested_body_bytes;
              str->dispatched_header_bytes += increment;
              if (str->dispatched_header_bytes == str->main_header_bytes)
                str->header_fully_dispatched = true;
              chunk_tail = 
                generate_header_increment(chunk_tail,id_encoder,str,-1,
                                          existing_header_bytes,increment,
                                          str->header_fully_dispatched,
                                          decouple_chunks);
              suggested_body_bytes -= increment;
              max_body_bytes -= increment;
            }

          kd_tile *tp;
          for (tp=str->active_tiles; tp != NULL; tp=tp->active_next)
            {
              if (suggested_body_bytes <= 0)
                break;
              if (tp->header_fully_dispatched)
                continue;
              int existing_header_bytes = tp->dispatched_header_bytes;
              int increment = tp->header_bytes - existing_header_bytes;
              if (increment > suggested_body_bytes)
                increment = suggested_body_bytes;
              tp->dispatched_header_bytes += increment;
              if (tp->dispatched_header_bytes == tp->header_bytes)
                {
                  tp->header_fully_dispatched = true;
                  if (tp->completed_precincts == tp->total_precincts)
                    {
                      str->completed_tiles++;
                      assert(str->completed_tiles <= str->total_tiles);
                    }
                }
              chunk_tail =
                generate_header_increment(chunk_tail,id_encoder,str,tp->tnum,
                                          existing_header_bytes,increment,
                                          tp->header_fully_dispatched,
                                          decouple_chunks);
              suggested_body_bytes -= increment;
              max_body_bytes -= increment;
            }
        }
    }

  // Now process the precinct increments determined during the simulation
  // phase above, reconciling the `prev*' and `current*' members of each
  // active precinct as we do so.
  kd_active_precinct *scan;
  int cum_packets, cum_bytes;
  completed_layers = max_active_layers; // Recompute `completed_layers' here
  for (scan=active_precincts; scan != NULL; scan=scan->next)
    {
      assert(!current_window.metadata_only);

      // Adjust `completed_layers' downward as necessary
      if ((scan->next_layer != scan->max_packets) &&
          (completed_layers > (int) scan->next_layer))
        completed_layers = (int) scan->next_layer;

      // See if there is any contribution here.
      if (scan->current_bytes == scan->prev_bytes)
        continue;
      assert((scan->current_bytes > scan->prev_bytes) &&
             (scan->prev_bytes >= scan->prev_packet_bytes) &&
             (scan->current_bytes >= scan->current_packet_bytes) &&
             (scan->current_packets >= scan->prev_packets) &&
             scan->interchange.exists());

      // Assemble messages
      int range_start = scan->prev_bytes;
      int range_length = ((int) scan->current_bytes) - range_start;
      suggested_body_bytes -= range_length;
      max_body_bytes -= range_length;
      assert(max_body_bytes >= 0);
      kdu_long unique_id = scan->interchange.get_unique_id();
      kds_chunk *chunk_start = NULL; // Will point to first chunk containing
                                     // data for this message
      int restore_max_bytes = chunk_tail->max_bytes;
      while (range_length > 0)
        { // Looping may be required to break large ranges into smaller
          // messages for packing into data chunks.
          bool is_final = (scan->current_packets == scan->max_packets);
          int xfer_bytes = chunk_tail->max_bytes - chunk_tail->num_bytes;
          int id_length =
            id_encoder->encode_id(NULL,KDU_PRECINCT_DATABIN,scan->stream_id,
                                  unique_id,is_final,
                                  use_extended_message_headers);
          xfer_bytes -= id_length;
          xfer_bytes -= simulate_var_length(range_start);
          xfer_bytes -= simulate_var_length(range_length);
          if (use_extended_message_headers)
            xfer_bytes -= simulate_var_length(scan->current_packets);
          if (xfer_bytes >= range_length)
            xfer_bytes = range_length; // Can put whole thing in message
          else
            { // Can't fit entire range into current message
              if ((xfer_bytes < 8) ||
                  ((xfer_bytes < (chunk_tail->max_bytes>>1)) &&
                   (range_length <=
                    (chunk_tail->max_bytes-chunk_tail->prefix_bytes))))
                { // Start packing data at the next chunk
                  if (chunk_tail->num_bytes == chunk_tail->prefix_bytes)
                    { kdu_error e;
                      e << "You must use larger chunks or smaller chunk "
                        "prefixes for `kdu_serve::generate_increments' "
                        "to be able to create a legal set of data chunks.";
                    }
                  chunk_tail->max_bytes = chunk_tail->num_bytes;
                  chunk_tail = chunk_tail->next = chunk_server->get_chunk();
                  if (decouple_chunks)
                    id_encoder->decouple();
                  continue;
                }
            }
          if (chunk_start == NULL)
            chunk_start = chunk_tail; // So we know where to start writing the
                                      // actual data.
          is_final = is_final && (xfer_bytes == range_length);
          chunk_tail->num_bytes +=
            id_encoder->encode_id(chunk_tail->data + chunk_tail->num_bytes,
                                  KDU_PRECINCT_DATABIN,scan->stream_id,
                                  unique_id,is_final,
                                  use_extended_message_headers);
          output_var_length(chunk_tail,range_start);
          output_var_length(chunk_tail,xfer_bytes);
          if (use_extended_message_headers)
            output_var_length(chunk_tail,(xfer_bytes==range_length)?
                              (scan->current_packets):(scan->prev_packets));
          assert(chunk_tail->max_bytes >= (chunk_tail->num_bytes+xfer_bytes));

          // Add element to member precinct list of current buffer
          kd_active_precinct *ptmp = active_server->get_precinct();
          ptmp->cache = scan->cache;
          ptmp->next = chunk_tail->precincts;
          chunk_tail->precincts = ptmp;
          ptmp->prev_bytes = (kdu_uint16) range_start;
          ptmp->prev_packets = scan->prev_packets;
          ptmp->prev_packet_bytes = scan->prev_packet_bytes;
          ptmp->max_packets = scan->max_packets;

          // Set (maybe temporarily) the chunk byte limit
          restore_max_bytes = chunk_tail->max_bytes;
          chunk_tail->max_bytes = chunk_tail->num_bytes + xfer_bytes;

          // Update data range
          range_length -= xfer_bytes;
          range_start += xfer_bytes;
          if (range_length > 0)
            {
              chunk_tail = chunk_tail->next = chunk_server->get_chunk();
              if (decouple_chunks)
                id_encoder->decouple();
            }
        }

      // Write precinct packet data to the data chunks which it spans.
      out.open(chunk_start,(scan->prev_bytes - scan->prev_packet_bytes));
      cum_packets = 0; cum_bytes = scan->current_bytes;
      scan->interchange.get_packets(scan->prev_packets,0,
                                    cum_packets,cum_bytes,&out);
      if (out.close() != chunk_tail)
        assert(0);
      assert(chunk_tail->max_bytes == chunk_tail->num_bytes);
      chunk_tail->max_bytes = restore_max_bytes;

      // Update precinct state on the basis that the data is actually written.
      if (scan->current_bytes == scan->current_packet_bytes)
        { // Full packet transfer
          assert((cum_packets == (int) scan->current_packets) &&
                 (cum_bytes == (int) scan->current_bytes));
        }
      else
        { // Partial packet transfer
          scan->interchange.restart(); // Partial transfers require restart
        }
      scan->prev_bytes = scan->current_bytes;
      scan->prev_packets = scan->current_packets;
      scan->prev_packet_bytes = scan->current_packet_bytes;
      if (scan->current_packets == scan->max_packets)
        {
          kd_tile *tp = scan->res->tile_comp->tile;
          tp->completed_precincts++;
          assert(tp->completed_precincts <= tp->total_precincts);
          if ((tp->completed_precincts == tp->total_precincts) &&
              tp->header_fully_dispatched)
            {
              tp->stream->completed_tiles++;
              assert(tp->stream->completed_tiles <= tp->stream->total_tiles);
            }
        }
    }

  // Finally include any metadata which has priority at least as high
  // as the number of completed quality layers
  for (; (suggested_body_bytes > 0) && (meta_ptr != NULL) &&
         ((meta_ptr->sequence <= completed_layers) ||
          (completed_layers == max_active_layers));
       meta_ptr=meta_ptr->active_next)
    {
      assert(meta_ptr->in_scope);
      if (meta_ptr->fully_dispatched)
        continue;
      assert(meta_ptr->active_length <= meta_ptr->num_bytes);
      int increment = meta_ptr->active_length - meta_ptr->dispatched_bytes;
      if ((increment < 0) ||
          ((increment==0) && (meta_ptr->active_length < meta_ptr->num_bytes)))
        continue; // Nothing to send
      if (increment > suggested_body_bytes)
        increment = suggested_body_bytes;
      chunk_tail = generate_meta_increment(meta_ptr,id_encoder,
                                           increment,chunk_tail,
                                           decouple_chunks);
      suggested_body_bytes -= increment;
      max_body_bytes -= increment;
      if (meta_ptr->dispatched_bytes < meta_ptr->num_bytes)
        break; // Already reached the suggested byte limit
    }
  return chunk_head;
}

/*****************************************************************************/
/*                      kd_serve::add_active_codestream                      */
/*****************************************************************************/

kd_stream *
  kd_serve::add_active_codestream(int stream_id)
{
  kd_stream *str;
  for (str=streams; str != NULL; str=str->next)
    if (str->stream_id == stream_id)
      break;
  if (str != NULL)
    {
      if (str->is_active)
        return str; // Do not need to increment `num_active_codestreams'
      if (num_active_codestreams == KD_MAX_ACTIVE_CODESTREAMS)
        return NULL;
      active_codestream_indices[num_active_codestreams++] = stream_id;
      return str;
    }

  // If we get here, we need to create the codestream from scratch
  if (num_active_codestreams == KD_MAX_ACTIVE_CODESTREAMS)
    return NULL;
  str = new kd_stream(this);
  kdu_codestream codestream =
    target->attach_to_codestream(stream_id,this);
  if (!codestream)
    { // Codestream index is legal, but stream will be shipped as meta-data
      delete str;
      return NULL;
    }
  str->next = streams;
  streams = str;
  str->initialize(stream_id,codestream,target,ignore_relevance_info);
  str->is_active = false; // Just to make it clear that we intend this
  active_codestream_indices[num_active_codestreams++] = stream_id;
  return str;
}

/*****************************************************************************/
/*                        kd_serve::add_stream_window                        */
/*****************************************************************************/

kd_codestream_window *
  kd_serve::add_stream_window()
{
  kd_codestream_window *result = free_stream_windows;
  if (result == NULL)
    result = new kd_codestream_window();
  else
    free_stream_windows = result->next;
  result->next = NULL;
  result->prev = last_stream_window;
  if (last_stream_window == NULL)
    last_stream_window = stream_windows = result;
  else
    last_stream_window = last_stream_window->next = result;
  result->context_type = 0;
  result->context_idx = -1;
  result->member_idx = -1;
  result->remapping_ids[0] = result->remapping_ids[1] = -1;
  return result;
}

/*****************************************************************************/
/*                        kd_serve::adjust_cache_model                       */
/*****************************************************************************/

void
  kd_serve::adjust_cache_model(int cls, int stream_min, int stream_max,
                               kdu_long bin_id, int available_bytes,
                               int available_packets, bool truncate)
{
  if (window_changed &&
      ((!last_translated_window.codestreams.contains(
                                  current_window.codestreams)) ||
       (!last_translated_window.contexts.contains(
                                  current_window.contexts))))
    { // Set of codestreams of interest may have changed.  This, in turn,
      // may change the model set (JPIP `mset').
      process_window_changes();
      if (active_lists_invalid)
        generate_active_lists();
    }
  if (bin_id < 0)
    { // Additive wildcard model update must be confined to the active window
      if (window_changed)
        process_window_changes();
      if (active_lists_invalid)
        generate_active_lists();
    }

  if (cls == KDU_PRECINCT_DATABIN)
    {
      if (bin_id < 0)
        { // Scan through all the precincts in the active list
          assert(!active_lists_invalid);
          kd_active_precinct *pscan;
          for (pscan=active_precincts; pscan != NULL; pscan=pscan->next)
            {
              if ((pscan->stream_id < stream_min) ||
                  (pscan->stream_id > stream_max))
                continue;
              if (truncate)
                truncate_precinct_cache(pscan->cache,available_bytes,
                                        available_packets,
                                        pscan->res->tile_comp->tile);
              else
                augment_precinct_cache(pscan->cache,available_bytes,
                                       available_packets,
                                       pscan->res->tile_comp->tile);
            }
        }
      else
        { // Search for the precinct
          for (kd_stream *str=streams; str != NULL; str=str->next)
            { // Scan the set of modeled code-streams
              if ((str->stream_id < stream_min) ||
                  (str->stream_id > stream_max))
                continue;
              kdu_long id = bin_id;
              kdu_long tmp = id / str->total_tiles;
              int tnum = (int)(id - (tmp * str->total_tiles));
              id = tmp;
              kd_tile *tp = str->tiles + tnum;
              if (tp->comps == NULL)
                str->init_tile(tp,tnum);
              tmp = id / str->num_components;
              int c = (int)(id - (tmp * str->num_components));
              id = tmp;
              kd_tile_comp *tc = tp->comps + c;
              for (int r=0; r < tc->num_resolutions; r++)
                {
                  kd_resolution *rp = tc->res + r;
                  kdu_long res_precincts = rp->precinct_indices.area();
                  if (res_precincts > id)
                    {
                      kdu_coords idx;
                      idx.y = (int)(id / rp->precinct_indices.size.x);
                      idx.x = (int)
                        (id - ((kdu_long) idx.y)*rp->precinct_indices.size.x);
                      kd_precinct *prec =
                        rp->access_precinct(idx+rp->precinct_indices.pos,
                                            !truncate);
                      if (!truncate)
                        augment_precinct_cache(prec,available_bytes,
                                               available_packets,tp);
                      else if (prec != NULL)
                        truncate_precinct_cache(prec,available_bytes,
                                                available_packets,tp);
                      break;
                    }
                  id -= res_precincts;
                }
            }
        }
    }
  else if ((cls == KDU_MAIN_HEADER_DATABIN) && (bin_id <= 0))
    {
      for (kd_stream *str=streams; str != NULL; str=str->next)
        { // Scan the set of modeled code-streams
          if ((str->stream_id < stream_min) ||
              (str->stream_id > stream_max))
            continue;
          if ((bin_id < 0) && !str->is_active)
            continue;
          if (truncate)
            truncate_stream_header_cache(str,available_bytes);
          else
            augment_stream_header_cache(str,available_bytes);
        }
    }
  else if (cls == KDU_TILE_HEADER_DATABIN)
    {
      for (kd_stream *str=streams; str != NULL; str=str->next)
        { // Scan the set of modeled code-streams
          if ((str->stream_id < stream_min) ||
              (str->stream_id > stream_max))
            continue;
          kd_tile *tp;
          if ((bin_id >= 0) && (bin_id < str->tile_indices.area()))
            {
              int tnum = (int) bin_id;
              tp = str->tiles + tnum;
              if (tp->comps == NULL)
                str->init_tile(tp,tnum);
              if (truncate)
                truncate_tile_header_cache(tp,available_bytes);
              else
                augment_tile_header_cache(tp,available_bytes);
            }
          else if ((bin_id < 0) && str->is_active)
            { // Scan through all active tiles
              assert(!active_lists_invalid);
              for (tp=str->active_tiles; tp != NULL; tp=tp->active_next)
                if (truncate)
                  truncate_tile_header_cache(tp,available_bytes);
                else
                  augment_tile_header_cache(tp,available_bytes);
            }
        }
    }
  else if (cls == KDU_META_DATABIN)
    { // Look through the list of meta data-bins
      kd_meta *mscan;
      for (mscan=active_metagroups; mscan != NULL; mscan=mscan->active_next)
        if ((mscan->bin_id == bin_id) || ((bin_id < 0) && mscan->in_scope))
          {
            if (truncate)
              truncate_meta_cache(mscan,available_bytes);
            else
              augment_meta_cache(mscan,available_bytes);
          }
    }
}

/*****************************************************************************/
/*                        kd_serve::adjust_cache_model                       */
/*****************************************************************************/

void
  kd_serve::adjust_cache_model(int stream_min, int stream_max,
                               int tmin, int tmax,
                               int cmin, int cmax, int rmin, int rmax,
                               int pmin, int pmax, int available_bytes,
                               int available_packets, bool truncate)
{
  if (window_changed &&
      ((!last_translated_window.codestreams.contains(
                                  current_window.codestreams)) ||
       (!last_translated_window.contexts.contains(
                                  current_window.contexts))))
    { // Set of codestreams of interest may have changed.  This, in turn,
      // may change the model set (JPIP `mset').
      process_window_changes();
      if (active_lists_invalid)
        generate_active_lists();
    }

  bool scoped =
    ((tmin < 0) || (tmin < tmax) || (cmin < 0) || (cmin < cmax) ||
     (rmin < 0) || (rmin < rmax) || (pmin < 0) || (pmin < pmax));
  if (scoped)
    { // Function needs to operate within the scope of the current window
      if (window_changed)
        process_window_changes();
      if (active_lists_invalid)
        generate_active_lists();
    }

  for (kd_stream *str=streams; str != NULL; str=str->next)
    {
      if ((str->stream_id < stream_min) || (str->stream_id > stream_max))
        continue;
      if (scoped && !str->is_active)
        continue;

      kd_tile *tp = NULL;
      kd_tile_comp *tc;
      kd_resolution *rp;
      kdu_dims t_range, p_range;
      kdu_coords t_idx, mod_t_idx, p_idx, mod_p_idx;

      if (tmin >= 0)
        {
          t_range.pos.y = tmin / str->tile_indices.size.x;
          t_range.pos.x = tmin - t_range.pos.y*str->tile_indices.size.x;
          t_range.size.y = tmax / str->tile_indices.size.x;
          t_range.size.x = tmax - t_range.size.y*str->tile_indices.size.x;
          t_range.size.y += 1 - t_range.pos.y;
          t_range.size.x += 1 - t_range.pos.x;
          t_range.pos += str->tile_indices.pos;
          t_range &= str->tile_indices;
        }

      if (pmin < 0)
        { // Cheapest option is to scan through all active precincts, looking
          // for those which match the conditions.
          assert(!active_lists_invalid);
          kd_active_precinct *pscan;
          for (pscan=active_precincts; pscan != NULL; pscan=pscan->next)
            {
              if (pscan->stream_id != str->stream_id)
                continue;
              rp = pscan->res;
              if ((rmin >= 0) && ((rp->r_idx < rmin) || (rp->r_idx > rmax)))
                continue;
              tc = rp->tile_comp;
              if ((cmin >= 0) && ((tc->c_idx < cmin) || (tc->c_idx > cmax)))
                continue;
              tp = tc->tile;
              if ((tmin >= 0) &&
                  ((tp->t_idx.x < t_range.pos.x) ||
                   (tp->t_idx.x >= (t_range.pos.x+t_range.size.x)) ||
                   (tp->t_idx.y < t_range.pos.y) ||
                   (tp->t_idx.y >= (t_range.pos.y+t_range.size.y))))
                continue;
              if (!truncate)
                augment_precinct_cache(pscan->cache,available_bytes,
                                       available_packets,tp);
              else
                truncate_precinct_cache(pscan->cache,available_bytes,
                                        available_packets,tp);
            }
          continue;
        }

      // If we get here, we are choosing to scan the coordinate ranges rather
      // than the active precincts
      if (tmin < 0)
        tp = str->active_tiles; // Scan active tile list
      do {
          if (tmin >= 0)
            { // Get tile from index, `t_idx' within `t_range'
              if (t_idx.x >= t_range.size.x)
                { // Advance to next row of tiles
                  t_idx.y++; t_idx.x=0;
                  if (t_idx.y >= t_range.size.y)
                    break; // All done
                }
              mod_t_idx = t_idx + t_range.pos - str->tile_indices.pos;
              int tnum = mod_t_idx.x + mod_t_idx.y * str->tile_indices.size.x;
              tp = str->tiles + tnum;
              t_idx.x++;
              if (scoped && !tp->is_active)
                continue;
              if (tp->comps == NULL)
                str->init_tile(tp,tnum);
            }
          int c, min_comp=cmin, max_comp=cmax;
          if (cmin < 0)
            { min_comp = 0; max_comp = str->num_components-1; }
          for (c=min_comp; c <= max_comp; c++)
            {
              tc = tp->comps + c;
              int r, min_res=rmin, max_res=rmax;
              if (rmin < 0)
                { min_res = 0; max_res = tc->num_resolutions-1; }
              for (r=min_res; r <= max_res; r++)
                {
                  rp = tc->res + r;
                  assert(pmin >= 0);
                  p_range.pos.y = pmin / rp->precinct_indices.size.x;
                  p_range.pos.x =
                    pmin - p_range.pos.y*rp->precinct_indices.size.x;
                  p_range.size.y = pmax / rp->precinct_indices.size.x;
                  p_range.size.x =
                    pmax - p_range.size.y*rp->precinct_indices.size.y;
                  p_range.size.y += 1 - p_range.pos.y;
                  p_range.size.x += 1 - p_range.pos.x;
                  p_range.pos += rp->precinct_indices.pos;
                  p_range &= rp->precinct_indices;
                  for (p_idx.y=0; p_idx.y < p_range.size.y; p_idx.y++)
                    for (p_idx.x=0; p_idx.x < p_range.size.x; p_idx.x++)
                      {
                        mod_p_idx = p_idx + p_range.pos;
                        kd_precinct *prec =
                          rp->access_precinct(mod_p_idx,!truncate);
                        if (!truncate)
                          augment_precinct_cache(prec,available_bytes,
                                                 available_packets,tp);
                        else if (prec != NULL)
                          truncate_precinct_cache(prec,available_bytes,
                                                  available_packets,tp);
                      }
                }
            }
          if (tmin < 0) // Scanning the active tile list
            tp = tp->active_next;
        } while (tp != NULL); // If not scanning active list, terminating
                           // condition corresponds to `break' statement above.
    }
}

/*****************************************************************************/
/*                     kd_serve::augment_precinct_cache                      */
/*****************************************************************************/

void
  kd_serve::augment_precinct_cache(kd_precinct *precinct, int available_bytes,
                                   int available_packets, kd_tile *tp)
{
  int max_packets = tp->num_layers;
  if ((available_packets < 0) && (available_bytes == 0))
    available_bytes = -1;
  bool is_final = (available_bytes < 0);
  if (is_final || (available_bytes > 0xFFFF))
    available_bytes = 0xFFFF;
  if (available_packets >= max_packets)
    {
      available_packets = max_packets;
      is_final = true;
    }
  kd_active_precinct *active = precinct->active;
  if (active == NULL)
    {
      if ((available_bytes > (int) precinct->cached_bytes) &&
          (precinct->cached_packets != (kdu_uint16) max_packets))
        {
          precinct->cached_bytes = (kdu_uint16) available_bytes;
          precinct->cached_packets = 0xFFFF; // Indeterminate
          if (is_final)
            precinct->cached_packets = (kdu_uint16) max_packets;
        }
      else if ((available_bytes == 0) &&
               (available_packets > (int) precinct->cached_packets))
        precinct->cached_packets = (kdu_uint16) available_packets;
      else
        return; // No effect on this precinct
    }
  else
    { // Precinct has an expanded `active' representation
      if ((available_bytes > (int) active->current_bytes) &&
          (active->current_packets != active->max_packets))
        {
          active->current_bytes =
            active->prev_bytes = (kdu_uint16) available_bytes;
          if (is_final)
            active->current_packets = active->next_layer =
              active->prev_packets = active->max_packets;
          else if (!active->interchange.exists())
            active->current_packets = active->next_layer =
              active->prev_packets = 0xFFFF;
          else
            {
              bool is_significant;
              int cum_bytes, cum_packets;
              for (cum_packets=active->current_packets+1;
                   cum_packets < max_packets; cum_packets++)
                {
                  cum_bytes = 0;
                  active->interchange.size_packets(cum_packets,cum_bytes,
                                                   is_significant);
                  if (cum_bytes <= available_bytes)
                    {
                      active->prev_packets =
                        active->current_packets = (kdu_uint16) cum_packets;
                      active->prev_packet_bytes =
                        active->current_packet_bytes = (kdu_uint16) cum_bytes;
                    }
                  if (cum_bytes >= available_bytes)
                    break;
                }
              if (active->current_packets > active->next_layer)
                active->next_layer = active->current_packets;
              if ((cum_bytes > available_bytes) &&
                  (active->current_packets < active->max_packets))
                active->interchange.restart();
              else if (active->current_packets == active->max_packets)
                is_final = true;
            }
        }
      else if ((available_bytes == 0) &&
               (available_packets > (int) active->current_packets))
        { // Adjust cache contents on available packets
          active->current_packets =
            active->prev_packets = (kdu_uint16) available_packets;
          if (active->interchange.exists())
            { // Figure out the number of bytes
              bool is_significant;
              int cum_bytes=0;
              int cum_packets=active->current_packets;
              active->interchange.size_packets(cum_packets,cum_bytes,
                                               is_significant);
              if (cum_bytes > (int) active->current_packet_bytes)
                active->current_bytes = active->prev_packet_bytes = cum_bytes;
              if (cum_bytes > (int) active->current_bytes)
                active->current_bytes = (kdu_uint16) cum_bytes;
            }
          if (active->current_packets > active->next_layer)
            active->next_layer = active->current_packets;
        }
      else
        return; // No effect on this precinct
    }

  if (is_final)
    {
      assert(tp->completed_precincts < tp->total_precincts);
      tp->completed_precincts++;
      if (tp->header_fully_dispatched &&
          (tp->completed_precincts == tp->total_precincts))
        {
          assert(tp->stream->completed_tiles < tp->stream->total_tiles);
          tp->stream->completed_tiles++;
        }
    }
}

/*****************************************************************************/
/*                     kd_serve::truncate_precinct_cache                     */
/*****************************************************************************/

void
  kd_serve::truncate_precinct_cache(kd_precinct *precinct,
                                    int available_bytes,
                                    int available_packets, kd_tile *tp)
{
  int max_packets = tp->num_layers;
  if (available_bytes < 0)
    available_bytes = 0;
  if (available_packets < 0)
    available_packets = 0;
  if (available_bytes != 0)
    available_packets = max_packets;
  else if (available_packets > 0)
    available_bytes = 65535; // So that the byte limit will have no effect
  bool was_complete = false; // If precinct was complete, and is no longer so
  kd_active_precinct *active = precinct->active;
  if (active == NULL)
    {
      if ((((int) precinct->cached_packets ) == max_packets) &&
          (precinct->cached_bytes > 0))
        was_complete = true;
      if (available_bytes == 0)
        precinct->cached_bytes = precinct->cached_packets = 0;
      else if ((available_bytes < (int) precinct->cached_bytes) ||
               ((available_packets == 0) && (precinct->cached_bytes == 0) &&
                (precinct->cached_packets > 0)))
        { // Set byte limit and require packet count to be recomputed
          precinct->cached_bytes = (kdu_uint16) available_bytes;
          precinct->cached_packets = 0xFFFF;
        }
      else if ((available_packets <= (int) precinct->cached_packets) &&
               (available_packets < max_packets))
        {
          precinct->cached_packets = (kdu_uint16) available_packets;
          precinct->cached_bytes = 0; // Unknown.
        }
      else
        return; // No effect on this precinct
    }
  else
    { // Precinct has an expanded `active' representation
      if ((((int) active->current_packets ) == max_packets) &&
          (active->current_bytes > 0))
        was_complete = true;
      if (available_bytes == 0)
        {
          active->current_bytes = active->prev_bytes = active->next_layer =
            active->current_packets = active->prev_packets =
              active->current_packet_bytes = active->prev_packet_bytes = 0;
          if (active->interchange.exists())
            active->interchange.restart();
        }
      else if ((available_bytes < (int) active->current_bytes) ||
               ((available_packets == 0) && (active->current_bytes == 0) &&
                (active->current_packets > 0)))
        {
          active->current_bytes =
            active->prev_bytes = (kdu_uint16) available_bytes;
          if (!active->interchange.exists())
            active->current_packets = active->next_layer =
              active->prev_packets = 0xFFFF;
          else
            {
              active->interchange.restart();
              bool is_significant;
              int cum_bytes, cum_packets;
              for (cum_packets=0; cum_packets < max_packets; cum_packets++)
                {
                  cum_bytes = 0;
                  active->interchange.size_packets(cum_packets,cum_bytes,
                                                   is_significant);
                  if (cum_bytes <= available_bytes)
                    {
                      active->prev_packets = active->next_layer =
                        active->current_packets = (kdu_uint16) cum_packets;
                      active->prev_packet_bytes =
                        active->current_packet_bytes = (kdu_uint16) cum_bytes;
                    }
                  if (cum_bytes >= available_bytes)
                    break;
                }
              if (cum_bytes > available_bytes)
                active->interchange.restart();
            }
        }
      else if ((available_packets <= (int) active->current_packets) &&
               (available_packets < max_packets))
        { // Adjust cache contents on available packets
          active->current_packets = active->next_layer =
            active->prev_packets = (kdu_uint16) available_packets;
          active->current_bytes = active->prev_bytes = 0;
          if (active->interchange.exists())
            { // Figure out the number of bytes
              bool is_significant;
              int cum_bytes=0;
              int cum_packets=active->current_packets;
              active->interchange.size_packets(cum_packets,cum_bytes,
                                               is_significant);
              active->current_packet_bytes = active->prev_packet_bytes =
                active->current_bytes = active->prev_bytes =
                  (kdu_uint16) cum_bytes;
            }
        }
      else
        return; // No effect on this precinct

      // Restart simulator from scratch is the safest option here.
      precinct_ptr = active_precincts;
      scan_first_layer = true;
      active_threshold = INT_MAX; // Ignored, since `scan_first_layer' true
      next_active_threshold = INT_MIN;

      // Be more careful adjusting `completed_layers' so as
      // to encourage appropriate interleaving of meta-data
      if (active->current_packets == 0xFFFF)
        completed_layers = 0;
      else if (completed_layers > (int) active->current_packets)
        completed_layers = (int) active->current_packets;
    }

  if (was_complete)
    {
      assert(tp->completed_precincts > 0);
      if (tp->header_fully_dispatched &&
          (tp->completed_precincts == tp->total_precincts))
        {
          assert(tp->stream->completed_tiles > 0);
          tp->stream->completed_tiles--;
        }
      tp->completed_precincts--;
    }
}

/*****************************************************************************/
/*                   kd_serve::augment_stream_header_cache                    */
/*****************************************************************************/

void
  kd_serve::augment_stream_header_cache(kd_stream *str, int available_bytes)
{
  if ((available_bytes < 0) || (available_bytes > str->main_header_bytes))
    available_bytes = str->main_header_bytes;
  if ((!str->header_fully_dispatched) &&
      (available_bytes >= str->dispatched_header_bytes))
    {
      str->dispatched_header_bytes = available_bytes;
      if (str->dispatched_header_bytes == str->main_header_bytes)
        str->header_fully_dispatched = true;
    }
}

/*****************************************************************************/
/*                   kd_serve::augment_tile_header_cache                     */
/*****************************************************************************/

void
  kd_serve::augment_tile_header_cache(kd_tile *tp, int available_bytes)
{
  if (available_bytes < 0)
    available_bytes = tp->header_bytes;
  if ((!tp->header_fully_dispatched) &&
      (available_bytes >= tp->dispatched_header_bytes))
    {
      tp->dispatched_header_bytes = available_bytes;
      if (tp->dispatched_header_bytes == tp->header_bytes)
        {
          tp->header_fully_dispatched = true;
          if (tp->completed_precincts == tp->total_precincts)
            {
              assert(tp->stream->completed_tiles < tp->stream->total_tiles);
              tp->stream->completed_tiles++;
            }
        }
    }
}

/*****************************************************************************/
/*                  kd_serve::truncate_stream_header_cache                   */
/*****************************************************************************/

void
  kd_serve::truncate_stream_header_cache(kd_stream *str, int available_bytes)
{
  if (available_bytes < 0)
    available_bytes = 0;
  if (available_bytes < str->dispatched_header_bytes)
    {
      str->dispatched_header_bytes = available_bytes;
      str->header_fully_dispatched = false;
      completed_layers = 0; // Increment processing must start from scratch
    }
}

/*****************************************************************************/
/*                   kd_serve::truncate_tile_header_cache                    */
/*****************************************************************************/

void
  kd_serve::truncate_tile_header_cache(kd_tile *tp, int available_bytes)
{
  if (available_bytes < 0)
    available_bytes = 0;
  if (available_bytes < tp->dispatched_header_bytes)
    {
      tp->dispatched_header_bytes = available_bytes;
      if (tp->header_fully_dispatched)
        {
          tp->header_fully_dispatched = false;
          if (tp->completed_precincts == tp->total_precincts)
            {
              assert(tp->stream->completed_tiles > 0);
              tp->stream->completed_tiles--;
            }
        }
      completed_layers = 0; // Increment processing must start from scratch
    }
}

/*****************************************************************************/
/*                      kd_serve::augment_meta_cache                         */
/*****************************************************************************/

void
  kd_serve::augment_meta_cache(kd_meta *meta, int available_bytes)
{
  if (meta->fully_dispatched)
    return;
  if (available_bytes < 0)
    available_bytes = meta->num_bytes;
  else
    {
      available_bytes -= meta->bin_offset;
      if (available_bytes > meta->num_bytes)
        available_bytes = meta->num_bytes;
    }
  int increment = available_bytes - meta->dispatched_bytes;
  if (increment >= 0)
    {
      meta->dispatched_bytes = available_bytes;
      dispatched_meta_bytes += increment;
      if (meta->dispatched_bytes == meta->num_bytes)
        meta->fully_dispatched = true;
    }
}

/*****************************************************************************/
/*                       kd_serve::truncate_meta_cache                       */
/*****************************************************************************/

void
  kd_serve::truncate_meta_cache(kd_meta *meta, int available_bytes)
{
  if (available_bytes < 0)
    available_bytes = 0;
  else
    {
      available_bytes -= meta->bin_offset;
      if (available_bytes < 0)
        available_bytes = 0;
    }
  int decrement = meta->dispatched_bytes - available_bytes;
  if (decrement >= 0)
    {
      meta->dispatched_bytes = available_bytes;
      dispatched_meta_bytes -= decrement;
      if (meta->fully_dispatched || (decrement > 0))
        {
          meta->fully_dispatched = false;
          meta_ptr = active_metagroups; // Safest to start again from scratch
        }
    }
}


/* ========================================================================= */
/*                                 kdu_serve                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                           kdu_serve::initialize                           */
/*****************************************************************************/

void
  kdu_serve::initialize(kdu_serve_target *target, int max_chunk_size,
                        int chunk_prefix_bytes, bool ignore_relevance_info)
{
  if (state != NULL)
    { kdu_error e; e << "Attempting to initialize a \"kdu_serve\" object "
      "which has already been initialized and has not yet been destroyed."; }
  state = new kd_serve(this);
  state->initialize(target,max_chunk_size,ignore_relevance_info);
  state->chunk_server->set_chunk_prefix_bytes(chunk_prefix_bytes);
}

/*****************************************************************************/
/*                             kdu_serve::destroy                            */
/*****************************************************************************/

void
  kdu_serve::destroy()
{
  if (state != NULL)
    delete state;
  state = NULL;
}

/*****************************************************************************/
/*                           kdu_serve::set_window                           */
/*****************************************************************************/

void
  kdu_serve::set_window(kdu_window &window)
{
  assert(state != NULL);
  if ((!state->window_changed) &&
      (state->current_window.equals(window) ||
       state->last_translated_window.equals(window)))
    return; // No changes need to be processed
  state->window_changed = true;
  state->active_lists_invalid = true;
  state->current_window.copy_from(window);
}

/*****************************************************************************/
/*                           kdu_serve::get_window                           */
/*****************************************************************************/

bool
  kdu_serve::get_window(kdu_window &window)
{
  assert(state != NULL);
  window.copy_from(state->current_window,true);
  if (state->active_lists_invalid ||
      (state->completed_layers < state->max_active_layers) ||
      (state->extra_data_bytes > 0) ||
      (state->meta_ptr != NULL))
    return true;

  if (window.metadata_only)
    return false;

  kd_stream *str;
  for (str=state->streams; str != NULL; str=str->next)
    if (str->is_active && !str->header_fully_dispatched)
      break;
  return (str != NULL);
}

/*****************************************************************************/
/*                        kdu_serve::augment_cache_model                     */
/*****************************************************************************/

void
  kdu_serve::augment_cache_model(int cls, int stream_min, int stream_max,
                                 kdu_long bin_id, int available_bytes,
                                 int available_packets)
{
  assert(state != NULL);
  state->adjust_cache_model(cls,stream_min,stream_max,bin_id,
                            available_bytes,available_packets,false);
}

/*****************************************************************************/
/*                       kdu_serve::truncate_cache_model                     */
/*****************************************************************************/

void
  kdu_serve::truncate_cache_model(int cls, int stream_min, int stream_max,
                                  kdu_long bin_id, int available_bytes,
                                  int available_packets)
{
  assert(state != NULL);
  state->adjust_cache_model(cls,stream_min,stream_max,bin_id,
                            available_bytes,available_packets,true);
}

/*****************************************************************************/
/*                       kdu_serve::augment_cache_model                      */
/*****************************************************************************/

void
  kdu_serve::augment_cache_model(int stream_min, int stream_max,
                                 int tmin, int tmax, int cmin, int cmax,
                                 int rmin, int rmax, int pmin, int pmax,
                                 int available_bytes, int available_packets)
{
  assert(state != NULL);
  state->adjust_cache_model(stream_min,stream_max,tmin,tmax,cmin,cmax,
                            rmin,rmax,pmin,pmax,available_bytes,
                            available_packets,false);
}

/*****************************************************************************/
/*                       kdu_serve::truncate_cache_model                     */
/*****************************************************************************/

void
  kdu_serve::truncate_cache_model(int stream_min, int stream_max,
                                  int tmin, int tmax, int cmin, int cmax,
                                  int rmin, int rmax, int pmin, int pmax,
                                  int available_bytes, int available_packets)
{
  assert(state != NULL);
  state->adjust_cache_model(stream_min,stream_max,tmin,tmax,cmin,cmax,
                            rmin,rmax,pmin,pmax,available_bytes,
                            available_packets,true);
}

/*****************************************************************************/
/*                          kdu_serve::get_image_done                        */
/*****************************************************************************/

bool
  kdu_serve::get_image_done()
{
  if (state == NULL)
    return false;
  if (state->dispatched_meta_bytes < state->total_meta_bytes)
    return false;
  if (state->extra_data_bytes != 0)
    return false;
  kd_stream *str;
  for (str=state->streams; str != NULL; str=str->next)
    if (!(str->header_fully_dispatched &&
          (str->completed_tiles == str->total_tiles)))
      break;
  return (str == NULL);
}

/*****************************************************************************/
/*                          kdu_serve::push_extra_data                       */
/*****************************************************************************/

int
  kdu_serve::push_extra_data(kdu_byte *data, int num_bytes,
                             kds_chunk *chunk_list)
{
  kds_chunk *scan;
  if (chunk_list != NULL)
    for (scan=chunk_list; scan->next != NULL; scan=scan->next);
  else
    scan = state->extra_data_tail;
  if ((data != NULL) && (num_bytes != 0))
    {
      if (scan == NULL)
        scan = state->extra_data_head = state->chunk_server->get_chunk();
      else if (scan->max_bytes < (scan->num_bytes + num_bytes))
        scan = scan->next = state->chunk_server->get_chunk();
      if (chunk_list == NULL)
        {
          state->extra_data_tail = scan;
          state->extra_data_bytes += num_bytes;
        }
      if (scan->max_bytes < (scan->num_bytes + num_bytes))
        { kdu_error e; e << "Attempting to push too much data in a single "
          "call to `kdu_serve::push_extra_data'.  You should be more "
          "careful to push the data incrementally."; }
      memcpy(scan->data+scan->num_bytes,data,(size_t) num_bytes);
      scan->num_bytes += num_bytes;
    }
  return ((scan==NULL)?0:(scan->max_bytes-scan->num_bytes));
}

/*****************************************************************************/
/*                        kdu_serve::generate_increments                     */
/*****************************************************************************/

kds_chunk *
  kdu_serve::generate_increments(int recommended_body_bytes,
                                 int &max_body_bytes, bool align,
                                 bool use_extended_message_headers,
                                 bool decouple_chunks,
                                 kds_id_encoder *id_encoder)
{
  assert(state != NULL);
  if (state->window_changed)
    state->process_window_changes();
  if (state->active_lists_invalid)
    state->generate_active_lists();
  return state->generate_increments(recommended_body_bytes,max_body_bytes,
                                    align,use_extended_message_headers,
                                    decouple_chunks,id_encoder);
}

/*****************************************************************************/
/*                          kdu_serve::release_chunks                        */
/*****************************************************************************/

void
  kdu_serve::release_chunks(kds_chunk *chunks, bool check_abandoned)
{
  assert(state != NULL);

  kds_chunk *chk;
  while ((chk=chunks) != NULL)
    {
      chunks = chk->next;
      if (check_abandoned && chk->abandoned)
        {
          kd_active_bin *scan;
          for (scan=chk->other_bins;  scan != NULL; scan=scan->next)
            if (scan->stream != NULL)
              {
                assert(scan->prev_bytes <= scan->stream->main_header_bytes);
                if (scan->stream->header_fully_dispatched ||
                    (scan->prev_bytes < scan->stream->dispatched_header_bytes))
                  {
                    scan->stream->dispatched_header_bytes = scan->prev_bytes;
                    scan->stream->header_fully_dispatched = false;
                    state->completed_layers = 0; // Force a rethink
                  }
              }
            else if (scan->tile != NULL)
              {
                assert(scan->prev_bytes <= scan->tile->header_bytes);
                if (scan->tile->header_fully_dispatched ||
                    (scan->prev_bytes < scan->tile->dispatched_header_bytes))
                  {
                    scan->tile->dispatched_header_bytes = scan->prev_bytes;
                    state->completed_layers = 0;
                  }
                if (scan->tile->header_fully_dispatched &&
                    (scan->tile->completed_precincts ==
                     scan->tile->total_precincts))
                  {
                    assert(scan->tile->stream->completed_tiles > 0);
                    scan->tile->stream->completed_tiles--;
                  }
                scan->tile->header_fully_dispatched = false;
              }
            else if (scan->meta != NULL)
              {
                assert(scan->prev_bytes <= scan->meta->num_bytes);
                int delta = scan->meta->dispatched_bytes - scan->prev_bytes;
                if ((delta > 0) || scan->meta->fully_dispatched)
                  {
                    scan->meta->dispatched_bytes -= delta;
                    scan->meta->fully_dispatched = false;
                    state->dispatched_meta_bytes -= delta;
                  }
                state->meta_ptr = state->active_metagroups;
            }

          kd_active_precinct *pscan;
          for (pscan=chk->precincts; pscan != NULL; pscan=pscan->next)
            {
              kd_precinct *prec = pscan->cache;
              kd_active_precinct *active = prec->active;
              if (active == NULL)
                { // State is stored in the permanent cache entry.
                  if (prec->cached_bytes > pscan->prev_bytes)
                    prec->cached_bytes = pscan->prev_bytes;
                  if (prec->cached_packets > pscan->prev_packets)
                    {
                      if (prec->cached_packets == pscan->max_packets)
                        { // Reduce num completed precincts
                          kd_tile *tp = pscan->res->tile_comp->tile;
                          assert(tp->completed_precincts > 0);
                          if ((tp->completed_precincts==tp->total_precincts) &&
                              tp->header_fully_dispatched)
                            {
                              assert(tp->stream->completed_tiles > 0);
                              tp->stream->completed_tiles--;
                            }
                          tp->completed_precincts--;
                        }
                      prec->cached_packets = pscan->prev_packets;
                    }
                }
              else
                { // State is stored in the active precinct structure
                  assert((active->current_bytes==active->prev_bytes) &&
                         (active->current_packets==active->prev_packets));
                  if (active->current_bytes > pscan->prev_bytes)
                    active->current_bytes =
                      active->prev_bytes = pscan->prev_bytes;
                  if (active->current_packets > pscan->prev_packets)
                    {
                      if (active->current_packets == active->max_packets)
                        { // Reduce num completed precincts
                          kd_tile *tp = pscan->res->tile_comp->tile;
                          assert(tp->completed_precincts > 0);
                          if ((tp->completed_precincts==tp->total_precincts) &&
                              tp->header_fully_dispatched)
                            {
                              assert(tp->stream->completed_tiles > 0);
                              tp->stream->completed_tiles--;
                            }
                          tp->completed_precincts--;
                        }
                      active->current_packets = active->next_layer =
                        active->prev_packets = pscan->prev_packets;
                      active->prev_packet_bytes =
                        active->current_packet_bytes =
                          pscan->prev_packet_bytes;
                      if (active->interchange.exists())
                        active->interchange.restart();

                      // Safest thing when number of packets is reduced is to
                      // restart the simulation counters
                      state->precinct_ptr = state->active_precincts;
                      state->scan_first_layer = true;
                      state->active_threshold = INT_MAX;
                      state->next_active_threshold = INT_MIN;

                      // Be more careful adjusting `completed_layers' so as
                      // to encourage appropriate interleaving of meta-data
                      if (state->completed_layers>(int)active->current_packets)
                        state->completed_layers = (int)active->current_packets;
                    }
                }
            }
        }
      state->active_server->release_precincts(chk->precincts);
      chk->precincts = NULL;
      state->active_server->release_bins(chk->other_bins);
      chk->other_bins = NULL;
      state->chunk_server->release_chunk(chk);
    }
}
