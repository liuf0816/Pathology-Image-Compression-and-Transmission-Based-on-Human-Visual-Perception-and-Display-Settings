/******************************************************************************/
// File: kdu_region_compositor.cpp [scope = APPS/SUPPORT]
// Version: Kakadu, V6.1
// Author: David Taubman
// Last Revised: 28 November, 2008
/******************************************************************************/
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
// The copyright owner is Unisearch Ltd, Australia (commercial arm of UNSW)
// Neither this copyright statement, nor the licensing details below
// may be removed from this file or dissociated from its contents.
/******************************************************************************/
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
/*******************************************************************************
Description:
  Implements the `kdu_region_compositor' object defined within
`kdu_region_compositor.h".
*******************************************************************************/

#include <assert.h>
#include <string.h>
#include <math.h>
#include "kdu_arch.h"
#include "kdu_utils.h"
#include "region_compositor_local.h"


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
     kdu_error _name("E(kdu_region_compositor.cpp)",_id);
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("W(kdu_region_compositor.cpp)",_id);
#  define KDU_TXT(_string) "<#>" // Special replacement pattern
#else // !KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("Error in Kakadu Region Compositor:\n");
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("Warning in Kakadu Region Compositor:\n");
#  define KDU_TXT(_string) _string
#endif // !KDU_CUSTOM_TEXT

#define KDU_ERROR_DEV(_name,_id) KDU_ERROR(_name,_id)
 // Use the above version for errors which are of interest only to developers
#define KDU_WARNING_DEV(_name,_id) KDU_WARNING(_name,_id)
 // Use the above version for warnings which are of interest only to developers


// Set things up for the inclusion of assembler optimized routines
// for specific architectures.  The reason for this is to exploit
// the availability of SIMD type instructions to greatly improve the
// efficiency with which layers may be composited (including alpha blending)
#if ((defined KDU_PENTIUM_MSVC) && (defined _WIN64))
# undef KDU_PENTIUM_MSVC
# ifndef KDU_X86_INTRINSICS
#   define KDU_X86_INTRINSICS // Use portable intrinsics instead
# endif
#endif // KDU_PENTIUM_MSVC && _WIN64

  static kdu_int32 kdrc_alpha_lut[256];
   /* The above LUT maps the 8-bit alpha value to a multiplier in the range
      0 to 2^14 (inclusive). */

#if defined KDU_X86_INTRINSICS
#  define KDU_SIMD_OPTIMIZATIONS
#  include "x86_region_compositor_local.h" // Contains SIMD intrinsics
#elif defined KDU_PENTIUM_MSVC
#  define KDU_SIMD_OPTIMIZATIONS
   static kdu_int16 kdrc_alpha_lut4[256*4]; // For quad-word parallel multiply
#  include "msvc_region_compositor_local.h" // Contains asm commands in-line
#endif

class kdrc_alpha_lut_init {
  public:
    kdrc_alpha_lut_init()
      {
        for (int n=0; n < 256; n++)
          {
            kdrc_alpha_lut[n] = (kdu_int32)((n + (n<<7) + (n<<15)) >> 9);
#         ifdef KDU_PENTIUM_MSVC
            kdrc_alpha_lut4[4*n] = kdrc_alpha_lut4[4*n+1] =
              kdrc_alpha_lut4[4*n+2] = kdrc_alpha_lut4[4*n+3] =
                (kdu_int16) kdrc_alpha_lut[n];
#         endif // KDU_PENTIUM_MSVC
          }
      }
  } init_alpha_lut;


/* ========================================================================== */
/*                             Internal Functions                             */
/* ========================================================================== */

/******************************************************************************/
/* STATIC                     adjust_geometry_flags                           */
/******************************************************************************/

static void
  adjust_geometry_flags(bool &transpose, bool &vflip, bool &hflip,
                        bool init_transpose, bool init_vflip, bool init_hflip)
  /* This function adjusts the `transpose', `vflip' and `hflip' values to
     include the effects of an initial change in geometry signalled by
     `init_transpose', `init_vflip' and `init_hflip'.  The interpretation
     of these geometric transformation flags is described in connection with
     `kdu_codestream::change_appearance'. */
{
  if (transpose)
    {
      if (init_vflip)
        hflip = !hflip;
      if (init_hflip)
        vflip = !vflip;
    }
  else
    {
      if (init_vflip)
        vflip = !vflip;
      if (init_hflip)
        hflip = !hflip;
    }
  if (init_transpose)
    transpose = !transpose;
}

/******************************************************************************/
/* STATIC                   initialize_buffer_surface                         */
/******************************************************************************/

static void
  initialize_buffer_surface(kdu_compositor_buf *buffer, kdu_dims region,
                            kdu_compositor_buf *old_buffer, kdu_dims old_region,
                            kdu_uint32 erase, bool can_skip_erase=false)
  /* This function is used by both `kdrc_layer' and `kdu_region_compositor'
     to initialize a buffer surface after it has been created, moved or
     resized.  If `old_buffer' is NULL, the new buffer surface is simply
     initialized to the `erase' value.  Otherwise, any region of the old
     buffer which intersects with the new buffer region is copied across and
     the remaining regions are initialized with the `erase' value.  It is
     important to realize that `buffer' and `old_buffer' may refer to exactly
     the same buffer, so that copying must be performed in the right order
     (top to bottom, bottom to top, left to right or right to left) so as to
     avoid overwriting data which has yet to be copied.  If `can_skip_erase'
     is true, only copy operations will be performed; erasure of samples which
     do not overlap with the old buffer region can be skipped. */
{
  int row_gap, old_row_gap;
  kdu_uint32 *buf=NULL, *old_buf=NULL;
  buf = buffer->get_buf(row_gap,false);
  if (old_buffer != NULL)
    old_buf = old_buffer->get_buf(old_row_gap,false);

  kdu_coords isect_size;
  kdu_coords isect_start; // Start of common region w.r.t. new surface
  kdu_coords isect_old_start; // Start of common region w.r.t. old surface
  kdu_coords isect_end; // End of common region  w.r.t. new surface
  kdu_coords isect_old_end; // End of common region w.r.t. old surface
  int m, n;
  kdu_uint32 *sp, *dp;

  kdu_dims isect;
  if ((old_buf == NULL) || ((isect = region & old_region).is_empty()))
    {
      if (can_skip_erase)
        return;
#ifdef KDU_SIMD_OPTIMIZATIONS
      if (!simd_erase_region(buf,region.size.y,region.size.x,row_gap,
                             erase))
#endif // KDU_SIMD_OPTIMIZATIONS
        {
          for (m=region.size.y; m > 0; m--, buf+=row_gap)
            for (dp=buf, n=region.size.x; n > 0; n--)
              *(dp++) = erase;
        }
      return;
    }
  isect_size = isect.size;
  isect_start = isect.pos - region.pos;
  isect_old_start = isect.pos - old_region.pos;
  isect_end = isect_start + isect_size;
  isect_old_end = isect_old_start + isect_size;
  old_buf += isect_old_start.y*old_row_gap+isect_old_start.x;

  if (isect_start.y < isect_old_start.y)
    { // erase -> copy -> erase from top to bottom
      if (can_skip_erase)
        buf += isect_start.y*row_gap;
      else
        {
          for (m=isect_start.y; m > 0; m--, buf+=row_gap)
            for (dp=buf, n=region.size.x; n > 0; n--)
              *(dp++) = erase;
        }
      for (m=isect_size.y; m > 0; m--, buf+=row_gap, old_buf+=old_row_gap)
        { // Common rows
          if (can_skip_erase)
            dp = buf + isect_start.x;
          else
            for (dp=buf, n=isect_start.x; n > 0; n--)
              *(dp++) = erase;
          for (sp=old_buf, n=isect_size.x; n > 0; n--)
            *(dp++) = *(sp++);
          if (!can_skip_erase)
            for (n=region.size.x-isect_end.x; n > 0; n--)
              *(dp++) = erase;
        }
      if (!can_skip_erase)
        {
          for (m=region.size.y-isect_end.y; m > 0; m--, buf+=row_gap)
            for (dp=buf, n=region.size.x; n > 0; n--)
              *(dp++) = erase;
        }
    }
  else
    { // erase -> copy -> erase from bottom to top
      buf += (region.size.y-1)*row_gap;
      old_buf += (isect_size.y-1)*old_row_gap;
      if (can_skip_erase)
        buf -= (region.size.y-isect_end.y)*row_gap;
      else
        {
          for (m=region.size.y-isect_end.y; m > 0; m--, buf-=row_gap)
            for (dp=buf, n=region.size.x; n > 0; n--)
              *(dp++) = erase;
        }
      for (m=isect_size.y; m > 0; m--, buf-=row_gap, old_buf-=old_row_gap)
        { // Common rows
          if (isect_start.x <= isect_old_start.x)
            { // erase -> copy -> erase from left to right
              if (can_skip_erase)
                dp = buf+isect_start.x;
              else
                for (dp=buf, n=isect_start.x; n > 0; n--)
                  *(dp++) = erase;
              for (sp=old_buf, n=isect_size.x; n > 0; n--)
                *(dp++) = *(sp++);
              if (!can_skip_erase)
                for (n=region.size.x-isect_end.x; n > 0; n--)
                  *(dp++) = erase;
            }
          else
            { // erase -> copy -> erase from right to left
              if (can_skip_erase)
                dp = buf + isect_end.x;
              else
                for (dp=buf+region.size.x,n=region.size.x-isect_end.x; n>0; n--)
                  *(--dp) = erase;
              for (sp=old_buf+isect_size.x, n=isect_size.x; n > 0; n--)
                *(--dp) = *(--sp);
              if (!can_skip_erase)
                for (n=isect_start.x; n > 0; n--)
                  *(--dp) = erase;
            }
        }
      if (!can_skip_erase)
        {
          for (m=isect_start.y; m > 0; m--, buf-=row_gap)
            for (dp=buf, n=region.size.x; n > 0; n--)
              *(dp++) = erase;
        }
    }
}

/******************************************************************************/
/* STATIC                      find_render_dims                               */
/******************************************************************************/

static kdu_dims
  find_render_dims(kdu_dims ref_comp_dims,
                   kdu_coords ref_comp_expand_numerator,
                   kdu_coords ref_comp_expand_denominator)
  /* This function returns the location and dimensions of the full image,
     as it appears on the rendering canvas, given the location and size of
     the reference image component (`ref_comp_dims') and the rational
     expansion factor.  It implements the coordinate transformations
     described in the comments appearing with `kdu_region_decompressor::start'.
     In fact it is identical to the function of the same name found within
     `kdu_region_decompressor'.  The reason for calling the function directly
     here, rather than via `kdu_region_decompressor::get_rendered_image_dims'
     is that we may need to know the dimensions of the rendered image after
     cropping.
  */
{
  kdu_long val, num, den;
  kdu_coords min = ref_comp_dims.pos;
  kdu_coords lim = min + ref_comp_dims.size;

  num = ref_comp_expand_numerator.x; den = ref_comp_expand_denominator.x;
  val = num*min.x;  val -= (num-1)>>1;  // Place source pixel in middle of its
                                        // interpolated region of influence
  min.x = long_ceil_ratio(val,den);
  val = num*lim.x;  val -= (num-1)>>1;  // Place source pixel in middle of its
                                        // interpolated region of influence
  lim.x = long_ceil_ratio(val,den);

  num = ref_comp_expand_numerator.y; den = ref_comp_expand_denominator.y;
  val = num*min.y;  val -= (num-1)>>1;  // Place source pixel in middle of its
                                        // interpolated region of influence
  min.y = long_ceil_ratio(val,den);
  val = num*lim.y;  val -= (num-1)>>1;  // Place source pixel in middle of its
                                        // interpolated region of influence
  lim.y = long_ceil_ratio(val,den);

  kdu_dims render_dims;
  render_dims.pos = min;
  render_dims.size = lim - min;
  return render_dims;
}


/* ========================================================================== */
/*                               kdrc_overlay                                 */
/* ========================================================================== */

/******************************************************************************/
/*                         kdrc_overlay::kdrc_overlay                         */
/******************************************************************************/

kdrc_overlay::kdrc_overlay(jpx_meta_manager meta_manager,
                           int codestream_idx)
{
  this->compositor = NULL;
  this->meta_manager = meta_manager;
  this->codestream_idx = codestream_idx;
  this->min_composited_size = 8; // Allow application to change this later
  expansion_numerator = expansion_denominator =
    buffer_region.size = kdu_coords(0,0);
  buffer = NULL;
  head = tail = last_painted_node = free_nodes = NULL;
  painting_param = 0;
}

/******************************************************************************/
/*                        kdrc_overlay::~kdrc_overlay                         */
/******************************************************************************/

kdrc_overlay::~kdrc_overlay()
{
  deactivate(); // Always safe to call this function
  assert(compositor == NULL);

  assert(head == NULL);
  while ((tail = free_nodes) != NULL)
    {
      free_nodes = tail->next;
      delete tail;
    }
}

/******************************************************************************/
/*                           kdrc_overlay::activate                           */
/******************************************************************************/

void
  kdrc_overlay::activate(kdu_region_compositor *compositor)
{
  this->compositor = compositor;
}

/******************************************************************************/
/*                          kdrc_overlay::deactivate                          */
/******************************************************************************/

void
  kdrc_overlay::deactivate()
{
  compositor = NULL;
  buffer = NULL;
  buffer_region.size = kdu_coords(0,0);
  while ((tail=head) != NULL)
    {
      head = tail->next;
      tail->next = free_nodes;
      free_nodes = tail;
    }
  last_painted_node = NULL;
}

/******************************************************************************/
/*                         kdrc_overlay::set_geometry                         */
/******************************************************************************/

void
  kdrc_overlay::set_geometry(kdu_coords image_offset, kdu_coords subsampling,
                             bool transpose, bool vflip, bool hflip,
                             kdu_coords expansion_numerator,
                             kdu_coords expansion_denominator,
                             kdu_coords compositing_offset)
{
  bool any_change = (image_offset != this->image_offset) ||
    (subsampling != this->subsampling) || (transpose != this->transpose) ||
    (vflip != this->vflip) || (hflip != this->hflip) ||
    (expansion_numerator != this->expansion_numerator) ||
    (expansion_denominator != this->expansion_denominator) ||
    (compositing_offset != this->compositing_offset);
  if (!any_change)
    return;
  this->image_offset = image_offset;
  this->subsampling = subsampling;
  this->transpose = transpose;
  this->vflip = vflip;
  this->hflip = hflip;
  this->expansion_numerator = expansion_numerator;
  this->expansion_denominator = expansion_denominator;
  this->compositing_offset = compositing_offset;

  kdu_long val;
  kdu_coords num = expansion_numerator;
  kdu_coords den = expansion_denominator;
  if (transpose)
    { num.transpose();  den.transpose(); } // Map to original geometry
  val = min_composited_size;  val *= subsampling.x;
  val = val * den.x;  val = val / num.x;
  min_size = (int) val;
  val = min_composited_size;  val *= subsampling.y;
  val = val * den.y;  val = val / num.y;
  if (min_size > (int) val)
    min_size = (int) val;

  while ((tail=head) != NULL)
    {
      head = tail->next;
      tail->next = free_nodes;
      free_nodes = tail;
    }
  last_painted_node = NULL;
  buffer_region.size = kdu_coords(0,0);
  buffer = NULL;
}

/******************************************************************************/
/*                     kdrc_overlay::set_buffer_surface                       */
/******************************************************************************/

bool
  kdrc_overlay::set_buffer_surface(kdu_compositor_buf *buffer,
                                   kdu_dims buffer_region,
                                   bool look_for_new_metadata)
{
  assert(compositor != NULL);
  assert((expansion_numerator.x > 0) &&
         (expansion_numerator.y > 0) &&
         (expansion_denominator.x > 0) &&
         (expansion_denominator.y > 0));
  kdu_dims valid_region = this->buffer_region;
  valid_region &= buffer_region;
  this->buffer = buffer;
  this->buffer_region = buffer_region;

  // See if we can reuse valid data calculated previously
  kdu_coords valid_min = valid_region.pos;
  kdu_coords valid_lim = valid_min + valid_region.size;
  kdu_coords new_min = buffer_region.pos;
  kdu_coords new_lim = new_min + buffer_region.size;
  bool reuse_existing = false;
  if ((valid_min.x == new_min.x) && (valid_lim.x == new_lim.x))
    { // Regions have shared vertical boundaries
      if ((valid_min.y < new_lim.y) && (valid_lim.y >= new_lim.y))
        { // Have a lower portion of the new region already
          new_lim.y = valid_min.y;
          reuse_existing = true;
        }
      else if ((valid_lim.y > new_min.y) && (valid_min.y <= new_min.y))
        { // Have an upper portion of the new region already
          new_min.y = valid_lim.y;
          reuse_existing = true;
        }
    }
  else if ((valid_min.y == new_min.y) && (valid_lim.y == new_lim.y))
    { // Regions have shared horizontal boundaries
      if ((valid_min.x < new_lim.x) && (valid_lim.x >= new_lim.x))
        { // Have a right hand portion of the new region already
          new_lim.x = valid_min.x;
          reuse_existing = true;
        }
      else if ((valid_lim.x > new_min.x) && (valid_min.x <= new_min.x))
        { // Have a left hand portion of the new region already
          new_min.x = valid_lim.x;
          reuse_existing = true;
        }
    }
  kdu_dims new_region;
  new_region.pos = new_min;
  new_region.size = new_lim - new_min;

  // Clean out metadata nodes which we are going to rebuild
  kdrc_roinode *nd;
  if (reuse_existing)
    { // Remove all metadata nodes which do not intersect with the new
      // buffer region, or which do intersect with the needed region
      kdrc_roinode *prev, *next;
      for (prev=NULL, nd=head; nd != NULL; prev=nd, nd = next)
        {
          next = nd->next;
          if (nd->bounding_box.intersects(buffer_region) &&
              !nd->bounding_box.intersects(new_region))
            continue; // Leave it there
          if (prev == NULL)
            head = next;
          else
            prev->next = next;
          if (nd == tail)
            {
              assert(next == NULL);
              tail = prev;
            }
          nd->next = free_nodes;
          free_nodes = nd;
          nd = prev; // So that `prev' does not change
        }
    }
  else
    { // Remove all existing elements
      while ((tail=head) != NULL)
        {
          head = tail->next;
          tail->next = free_nodes;
          free_nodes = tail;
        }
    }

  last_painted_node = NULL;
  assert(((head == NULL) && (tail == NULL)) ||
         ((tail != NULL) && (tail->next == NULL)));

  // Augment metadata list with new elements
  kdu_dims update_region = (look_for_new_metadata)?buffer_region:new_region;
  if (!update_region)
    return (head != NULL);
  map_from_compositing_grid(update_region);

  jpx_metanode mn;
  meta_manager.load_matches(1,&codestream_idx,0,NULL);
  while ((mn=meta_manager.enumerate_matches(mn,codestream_idx,-1,false,
                                            update_region,min_size)).exists())
    {
      kdu_dims bounding_box = mn.get_bounding_box();
      map_to_compositing_grid(bounding_box);
      if (!bounding_box.intersects(new_region))
        { // See if it already exists within the metadata list
          if (!bounding_box.intersects(buffer_region))
            continue;
          for (nd=head; nd != NULL; nd=nd->next)
            if (nd->node == mn)
              break;
          if (nd != NULL)
            continue;
        }
      if ((bounding_box.size.x < min_composited_size) &&
          (bounding_box.size.y < min_composited_size))
        continue;

      nd = free_nodes;
      if (nd != NULL)
        free_nodes = nd->next;
      else
        nd = new kdrc_roinode;
      nd->next = NULL;
      if (tail == NULL)
        head = tail = nd;
      else
        tail = tail->next = nd;
      nd->node = mn;
      nd->bounding_box = bounding_box;
      nd->painted = false;
    }
  return (head != NULL);
}

/******************************************************************************/
/*                       kdrc_overlay::update_config                          */
/******************************************************************************/

bool
  kdrc_overlay::update_config(int min_display_size, int new_painting_param)
{
  if (min_display_size < 1)
    min_display_size = 1;
  if ((new_painting_param == painting_param) &&
      (min_display_size == min_composited_size))
    return false;

  bool changed_painting_param = (painting_param != new_painting_param);
  painting_param = new_painting_param;

  bool scan_for_new_elements =
    (min_display_size < min_composited_size) && !buffer_region.is_empty();
  min_composited_size = min_display_size;
  if ((expansion_numerator.x == 0) || (expansion_numerator.y == 0) ||
      (expansion_denominator.x == 0) || (expansion_denominator.y == 0))
    return false; // Geometry not set yet

  kdu_long val;
  kdu_coords num = expansion_numerator;
  kdu_coords den = expansion_denominator;
  if (transpose)
    { num.transpose();  den.transpose(); } // Map to original geometry
  val = min_composited_size;  val *= subsampling.x;
  val = val * den.x;  val = val / num.x;
  min_size = (int) val;
  val = min_composited_size;  val *= subsampling.y;
  val = val * den.y;  val = val / num.y;
  if (min_size > (int) val)
    min_size = (int) val;

  last_painted_node = NULL;
  bool anything_removed = false;
  kdrc_roinode *scan, *prev, *next;
  for (prev=NULL, scan=head; scan != NULL; prev=scan, scan=next)
    {
      next = scan->next;
      if (changed_painting_param)
        scan->painted = false;
      if ((scan->bounding_box.size.x < min_composited_size) &&
          (scan->bounding_box.size.y < min_composited_size))
        { // Remove element
          anything_removed = true;
          if (prev == NULL)
            head = next;
          else
            prev->next = next;
          if (next == NULL)
            {
              assert(scan == tail);
              tail = prev;
            }
          scan->next = free_nodes;
          free_nodes = scan;
          scan = prev; // So `prev' won't change
        }
    }

  if (!scan_for_new_elements)
    {
      if (anything_removed)
        { // Repaint it all
          for (scan=head; scan != NULL; scan=scan->next)
            scan->painted = false;
          return true;
        }
      else
        return false;
    }

  // If we get here, we need to delete the list and start again with the new,
  // lower size bound
  while ((tail=head) != NULL)
    {
      head = tail->next;
      tail->next = free_nodes;
      free_nodes = tail;
    }

  kdu_dims mapped_region = buffer_region;
  map_from_compositing_grid(mapped_region);

  jpx_metanode mn;
  meta_manager.load_matches(1,&codestream_idx,0,NULL);
  while ((mn=meta_manager.enumerate_matches(mn,codestream_idx,-1,false,
                                            mapped_region,min_size)).exists())
    {
      kdu_dims bounding_box = mn.get_bounding_box();
      map_to_compositing_grid(bounding_box);
      if (!bounding_box.intersects(buffer_region))
        continue;
      if ((bounding_box.size.x < min_composited_size) &&
          (bounding_box.size.y < min_composited_size))
        continue;

      kdrc_roinode *nd = free_nodes;
      if (nd != NULL)
        free_nodes = nd->next;
      else
        nd = new kdrc_roinode;
      nd->next = NULL;
      if (tail == NULL)
        head = tail = nd;
      else
        tail = tail->next = nd;
      nd->node = mn;
      nd->bounding_box = bounding_box;
      nd->painted = false;
    }
  return false;
}

/******************************************************************************/
/*                          kdrc_overlay::process                             */
/******************************************************************************/

bool
  kdrc_overlay::process(kdu_dims &new_region)
{
  if ((buffer == NULL) || (last_painted_node == tail))
    return false;
  if (last_painted_node == NULL)
    last_painted_node = head;
  bool can_paint = false;
  do {
      can_paint = (!last_painted_node->painted) &&
        ((last_painted_node->bounding_box.size.x < buffer_region.size.x) ||
         (last_painted_node->bounding_box.size.y < buffer_region.size.y));
      if (last_painted_node->next == NULL)
        break;
      if (!can_paint)
        last_painted_node = last_painted_node->next;
    } while (!can_paint);
  if (!can_paint)
    return false;
  if (!compositor->custom_paint_overlay(buffer,buffer_region,
                                last_painted_node->bounding_box,codestream_idx,
                                last_painted_node->node,painting_param,
                                image_offset, subsampling,transpose,vflip,hflip,
                                expansion_numerator,expansion_denominator,
                                compositing_offset))
    compositor->paint_overlay(buffer,buffer_region,
                      last_painted_node->bounding_box,codestream_idx,
                      last_painted_node->node,painting_param,
                      image_offset,subsampling,transpose,vflip,hflip,
                      expansion_numerator,expansion_denominator,
                      compositing_offset);
  last_painted_node->painted = true;
  new_region = last_painted_node->bounding_box & buffer_region;
  return true;
}

/******************************************************************************/
/*                            kdrc_overlay::search                            */
/******************************************************************************/

jpx_metanode
  kdrc_overlay::search(kdu_coords point, int &stream_idx)
{
  kdu_coords off;
  kdrc_roinode *scan;
  for (scan=head; scan != NULL; scan=scan->next)
    {
      off = point - scan->bounding_box.pos;
      if ((off.x >= 0) && (off.x < scan->bounding_box.size.x) &&
          (off.y >= 0) && (off.y < scan->bounding_box.size.y) &&
          ((scan->bounding_box.size.x < buffer_region.size.x) ||
           (scan->bounding_box.size.y < buffer_region.size.y)))
        {
          stream_idx = codestream_idx;
          return scan->node;
        }
    }
  return jpx_metanode(NULL);
}

/******************************************************************************/
/*                  kdrc_overlay::map_from_compositing_grid                   */
/******************************************************************************/

void
  kdrc_overlay::map_from_compositing_grid(kdu_dims &region)
{
  region.pos += compositing_offset;
  kdu_coords min = region.pos;
  kdu_coords lim = min + region.size;
  min.x = long_floor_ratio(((kdu_long) min.x)*expansion_denominator.x,
                           expansion_numerator.x);
  min.y = long_floor_ratio(((kdu_long) min.y)*expansion_denominator.y,
                           expansion_numerator.y);
  lim.x = long_ceil_ratio(((kdu_long) lim.x)*expansion_denominator.x,
                           expansion_numerator.x);
  lim.y = long_ceil_ratio(((kdu_long) lim.y)*expansion_denominator.y,
                           expansion_numerator.y);
  region.pos = min; region.size = lim - min;
  region.from_apparent(transpose,vflip,hflip);
  region.pos.x *= subsampling.x;
  region.pos.y *= subsampling.y;
  region.size.x *= subsampling.x;
  region.size.y *= subsampling.y;
  region.pos -= image_offset;
}

/******************************************************************************/
/*                   kdrc_overlay::map_to_compositing_grid                    */
/******************************************************************************/

void
  kdrc_overlay::map_to_compositing_grid(kdu_dims &region)
{
  region.pos += image_offset;
  kdu_coords min = region.pos, lim = min + region.size;
  min.x = ceil_ratio(min.x,subsampling.x);
  min.y = ceil_ratio(min.y,subsampling.y);
  lim.x = ceil_ratio(lim.x,subsampling.x);
  lim.y = ceil_ratio(lim.y,subsampling.y);
  region.pos = min; region.size = lim - min;
  region.to_apparent(transpose,vflip,hflip);
  
  min = region.pos;
  lim = min + region.size;
  min.x = long_floor_ratio(((kdu_long) min.x)*expansion_numerator.x,
                           expansion_denominator.x);
  min.y = long_floor_ratio(((kdu_long) min.y)*expansion_numerator.y,
                           expansion_denominator.y);
  lim.x = long_ceil_ratio(((kdu_long) lim.x)*expansion_numerator.x,
                           expansion_denominator.x);
  lim.y = long_ceil_ratio(((kdu_long) lim.y)*expansion_numerator.y,
                           expansion_denominator.y);
  region.pos = min;
  region.size = lim - min;
  region.pos -= compositing_offset;
}


/* ========================================================================== */
/*                              kdrc_codestream                               */
/* ========================================================================== */

/******************************************************************************/
/*                         kdrc_codestream::init (raw)                        */
/******************************************************************************/

void
  kdrc_codestream::init(kdu_compressed_source *source)
{
  assert(!ifc);
  ifc.create(source);
  if (persistent)
    {
      ifc.set_persistent();
      ifc.augment_cache_threshold(cache_threshold);
    }
  ifc.get_dims(-1,canvas_dims);

}

/******************************************************************************/
/*                       kdrc_codestream::init (JP2/JPX)                      */
/******************************************************************************/

void
  kdrc_codestream::init(jpx_codestream_source stream)
{
  assert(!ifc);
  stream.open_stream(&source_box);
  ifc.create(&source_box);
  if (persistent)
    {
      ifc.set_persistent();
      ifc.augment_cache_threshold(cache_threshold);
    }
  ifc.get_dims(-1,canvas_dims);
}

/******************************************************************************/
/*                         kdrc_codestream::init (MJ2)                        */
/******************************************************************************/

void
  kdrc_codestream::init(mj2_video_source *track, int frame_idx, int field_idx)
{
  assert(!ifc);
  track->seek_to_frame(frame_idx);
  track->open_stream(field_idx,&source_box);
  ifc.create(&source_box);
  if (persistent)
    {
      ifc.set_persistent();
      ifc.augment_cache_threshold(cache_threshold);
    }
  ifc.get_dims(-1,canvas_dims);
}

/******************************************************************************/
/*                         kdrc_codestream::restart                           */
/******************************************************************************/

bool
  kdrc_codestream::restart(mj2_video_source *track,
                           int frame_idx, int field_idx)
{
  assert(ifc.exists());
  source_box.close();
  track->seek_to_frame(frame_idx);
  track->open_stream(field_idx,&source_box);
  ifc.restart(&source_box);
  return true;
}

/******************************************************************************/
/*                           kdrc_codestream::attach                          */
/******************************************************************************/

void
  kdrc_codestream::attach(kdrc_stream *user)
{
  assert(user->codestream == NULL);
  user->next_codestream_user = head;
  user->prev_codestream_user = NULL;
  if (head != NULL)
    {
      assert(head->prev_codestream_user == NULL);
      head->prev_codestream_user = user;
    }
  head = user;
  user->codestream = this;

  // Scan the remaining users for this code-stream to make sure they are not
  // in the midst of processing.
  kdrc_stream *scan=head->next_codestream_user;
  for (; scan != NULL; scan=scan->next_codestream_user)
    scan->stop_processing();
  assert(!in_use);

  // Remove any appearance changes or input restrictions
  ifc.change_appearance(false,false,false);
  ifc.apply_input_restrictions(0,0,0,0,NULL,KDU_WANT_OUTPUT_COMPONENTS);
}

/******************************************************************************/
/*                           kdrc_codestream::detach                          */
/******************************************************************************/

void
  kdrc_codestream::detach(kdrc_stream *user)
{
  assert(user->codestream == this);
  if (user->prev_codestream_user == NULL)
    {
      assert(user == head);
      head = user->next_codestream_user;
      if (head != NULL)
        head->prev_codestream_user = NULL;
    }
  else
    {
      assert(user != head);
      user->prev_codestream_user->next_codestream_user =
        user->next_codestream_user;
    }
  if (user->next_codestream_user != NULL)
    user->next_codestream_user->prev_codestream_user =
      user->prev_codestream_user;
  user->codestream = NULL;
  user->prev_codestream_user = user->next_codestream_user = NULL;
  if (head == NULL)
    delete this;
}

/******************************************************************************/
/*                        kdrc_codestream::move_to_head                       */
/******************************************************************************/

void
  kdrc_codestream::move_to_head(kdrc_stream *user)
{
  assert(user->codestream == this);

  // First unlink from the list
  if (user->prev_codestream_user == NULL)
    {
      assert(user == head);
      head = user->next_codestream_user;
      if (head != NULL)
        head->prev_codestream_user = NULL;
    }
  else
    {
      assert(user != head);
      user->prev_codestream_user->next_codestream_user =
        user->next_codestream_user;
    }
  if (user->next_codestream_user != NULL)
    user->next_codestream_user->prev_codestream_user =
      user->prev_codestream_user;

  // Now link back in at head
  user->next_codestream_user = head;
  user->prev_codestream_user = NULL;
  if (head != NULL)
    {
      assert(head->prev_codestream_user == NULL);
      head->prev_codestream_user = user;
    }
  head = user;

  // Scan all users for this code-stream to make sure they are not
  // in the midst of processing.
  kdrc_stream *scan;
  for (scan=head; scan != NULL; scan=scan->next_codestream_user)
    scan->stop_processing();
  assert(!in_use);
}

/******************************************************************************/
/*                        kdrc_codestream::move_to_tail                       */
/******************************************************************************/

void
  kdrc_codestream::move_to_tail(kdrc_stream *user)
{
  assert(user->codestream == this);

  // First unlink from the list
  if (user->prev_codestream_user == NULL)
    {
      assert(user == head);
      head = user->next_codestream_user;
      if (head != NULL)
        head->prev_codestream_user = NULL;
    }
  else
    {
      assert(user != head);
      user->prev_codestream_user->next_codestream_user =
        user->next_codestream_user;
    }
  if (user->next_codestream_user != NULL)
    user->next_codestream_user->prev_codestream_user =
      user->prev_codestream_user;

  // Now link back in at tail
  kdrc_stream *prev=NULL, *scan;
  for (scan=head; scan != NULL; prev=scan, scan=scan->next_codestream_user);
  user->prev_codestream_user = prev;
  user->next_codestream_user = NULL;
  if (prev == NULL)
    head = user;
  else
    prev->next_codestream_user = user;

  // Scan all users for this code-stream to make sure they are not
  // in the midst of processing.
  for (scan=head; scan != NULL; scan=scan->next_codestream_user)
    scan->stop_processing();
  assert(!in_use);
}


/* ========================================================================== */
/*                                kdrc_stream                                 */
/* ========================================================================== */

/******************************************************************************/
/*                          kdrc_stream::kdrc_stream                          */
/******************************************************************************/

kdrc_stream::kdrc_stream(kdu_region_compositor *owner,
                         bool persistent, int cache_threshold,
                         kdu_thread_env *env, kdu_thread_queue *env_queue)
{
  this->owner = owner;
  this->persistent = persistent;
  this->cache_threshold = cache_threshold;
  this->env = env;
  this->env_queue = env_queue;
  mj2_track = NULL;
  mj2_frame_idx = mj2_field_idx = 0;
  alpha_only = false;
  alpha_is_premultiplied = false;

  overlay = NULL;
  max_display_layers = 1<<16;
  have_valid_scale = processing = false;
  buffer = NULL;
  int endian_test = 1;
  *((char *)(&endian_test)) = '\0';
  little_endian = (endian_test==0);
  is_complete = false;
  priority = KDRC_PRIORITY_MAX;
  layer_idx = codestream_idx = -1;
  layer = NULL;
  next = NULL;
  codestream = NULL;
  next_codestream_user = prev_codestream_user = NULL;
  active_component = single_component = reference_component = -1;
  component_access_mode = KDU_WANT_OUTPUT_COMPONENTS;
}

/******************************************************************************/
/*                          kdrc_stream::~kdrc_stream                         */
/******************************************************************************/

kdrc_stream::~kdrc_stream()
{
  stop_processing();
  if (codestream != NULL)
    {
      codestream->detach(this);
      codestream = NULL;
    }
  if (overlay != NULL)
    delete overlay;
}

/******************************************************************************/
/*                           kdrc_stream::init (raw)                          */
/******************************************************************************/

void
  kdrc_stream::init(kdu_compressed_source *source,
                    kdrc_stream *sibling)
{
  mj2_track = NULL;
  mj2_frame_idx = mj2_field_idx = 0;
  assert(overlay == NULL);

  alpha_only = false;
  alpha_is_premultiplied = false;
  codestream_idx = 0;
  layer_idx = 0;
  this->layer = NULL;
  assert(codestream == NULL);
  if (sibling != NULL)
    {
      sibling->codestream->attach(this);
      assert(codestream == sibling->codestream);
    }
  else
    {
      kdrc_codestream *cs = new kdrc_codestream(persistent,cache_threshold);
      try {
          cs->init(source);
        }
      catch (...) {
          delete cs;
          throw;
        }
      cs->attach(this);
      assert(codestream == cs);
    }
  single_component = -1;
  component_access_mode = KDU_WANT_OUTPUT_COMPONENTS;
  mapping.configure(codestream->ifc);
  reference_component = mapping.get_source_component(0);
  active_component = reference_component;
  max_discard_levels = codestream->ifc.get_min_dwt_levels();
  configure_subsampling();
  can_flip = codestream->ifc.can_flip(false);
  have_valid_scale = processing = false;
  invalidate_surface();
}

/******************************************************************************/
/*                           kdrc_stream::init (JPX)                          */
/******************************************************************************/

void
  kdrc_stream::init(jpx_codestream_source stream, jpx_layer_source layer,
                    jpx_source *jpx_src, bool alpha_only, kdrc_stream *sibling)
{
  mj2_track = NULL;
  mj2_frame_idx = mj2_field_idx = 0;
  assert(overlay == NULL);

  codestream_idx = stream.get_codestream_id();
  overlay = new kdrc_overlay(jpx_src->access_meta_manager(),codestream_idx);
  layer_idx = layer.get_layer_id();
  this->layer = NULL; // Changed by `kdrc_layer' object.
  this->alpha_only = alpha_only;
  this->alpha_is_premultiplied = false;
  assert(codestream == NULL);
  if (sibling != NULL)
    {
      sibling->codestream->attach(this);
      assert(codestream == sibling->codestream);
    }
  else
    {
      kdrc_codestream *cs = new kdrc_codestream(persistent,cache_threshold);
      try {
          cs->init(stream);
        }
      catch (...) {
          delete cs;
          throw;
        }
      cs->attach(this);
      assert(codestream == cs);
    }
  single_component = -1;
  component_access_mode = KDU_WANT_OUTPUT_COMPONENTS;
  jp2_channels channels = layer.access_channels();
  jp2_palette palette = stream.access_palette();
  jp2_dimensions dimensions = stream.access_dimensions();
  bool have_alpha = false;
  if (alpha_only)
    {
      mapping.clear();
      if (mapping.add_alpha_to_configuration(channels,codestream_idx,
                                             palette,dimensions,true))
        have_alpha = true;
      else if (mapping.add_alpha_to_configuration(channels,codestream_idx,
                                                  palette,dimensions,false))
        have_alpha = alpha_is_premultiplied = true;
      else
        { KDU_ERROR(e,0); e <<
            KDU_TXT("Complex opacity representation for compositing "
            "layer (index, starting from 0, equals ") << layer_idx <<
          KDU_TXT(") cannot be implemented without the "
          "inclusion of multiple distinct alpha blending channels.");
        }
    }
  else
    {
      // Hunt for a good colour space
      jp2_colour tmp_colour, colour;
      int which, tmp_prec, prec, lim_prec=128;
      do {
          prec = -10000;
          colour = jp2_colour(NULL);
          which = 0;
          while ((tmp_colour=layer.access_colour(which++)).exists())
            {
              tmp_prec = tmp_colour.get_precedence();
              if (tmp_prec >= lim_prec)
                continue;
              if (tmp_prec > prec)
                {
                  prec = tmp_prec;
                  colour = tmp_colour;
                }
            }
          if (!colour)
            { KDU_ERROR(e,1); e <<
                KDU_TXT("Unable to find any colour description which "
                "can be used by the present implementation to render "
                "compositing layer (index, starting from 0, equals ")
                << layer_idx <<
                KDU_TXT(") to sRGB."); }
          lim_prec = prec;
        } while (!mapping.configure(colour,channels,codestream_idx,
                                    palette,dimensions));
      if (mapping.add_alpha_to_configuration(channels,codestream_idx,
                                             palette,dimensions,true))
        have_alpha = true;
      else if (mapping.add_alpha_to_configuration(channels,codestream_idx,
                                                  palette,dimensions,false))
        have_alpha = alpha_is_premultiplied = true;
    }
  assert(mapping.num_channels ==
         (mapping.num_colour_channels + ((have_alpha)?1:0)));
  reference_component = mapping.get_source_component(0);
  active_component = reference_component;
  max_discard_levels = codestream->ifc.get_min_dwt_levels();
  configure_subsampling();
  can_flip = codestream->ifc.can_flip(false);
  have_valid_scale = processing = false;
  invalidate_surface();
}

/******************************************************************************/
/*                           kdrc_stream::init (MJ2)                          */
/******************************************************************************/

void
  kdrc_stream::init(mj2_video_source *track, int frame_idx, int field_idx,
                    kdrc_stream *sibling)
{
  mj2_track = track;;
  mj2_frame_idx = frame_idx;
  mj2_field_idx = field_idx;
  assert(overlay == NULL);
  alpha_only = false;
  alpha_is_premultiplied = false;

  if ((field_idx < 0) || (field_idx > 1) ||
      ((track->get_field_order() == KDU_FIELDS_NONE) && (field_idx != 0)))
    { KDU_ERROR_DEV(e,2); e <<
        KDU_TXT("Invalid field index passed to `kdrc_stream::init' "
        "when initializing the codestream management for a Motion JPEG2000 "
        "track.");
    }
  track->seek_to_frame(frame_idx);
  codestream_idx = track->get_stream_idx(field_idx);
  layer_idx = ((int) track->get_track_idx()) - 1;
  this->layer = NULL; // Changed by `kdrc_layer' object.

  assert(codestream == NULL);
  if (sibling != NULL)
    {
      sibling->codestream->attach(this);
      assert(codestream == sibling->codestream);
    }
  else
    {
      kdrc_codestream *cs = new kdrc_codestream(persistent,cache_threshold);
      try {
          cs->init(track,frame_idx,field_idx);
        }
      catch (...) {
          delete cs;
          throw;
        }
      cs->attach(this);
      assert(codestream == cs);
    }
  codestream->ifc.enable_restart(); // So we can restart
  single_component = -1;
  component_access_mode = KDU_WANT_OUTPUT_COMPONENTS;
  jp2_channels channels = track->access_channels();
  jp2_palette palette = track->access_palette();
  jp2_dimensions dimensions = track->access_dimensions();
  jp2_colour colour = track->access_colour();

  // Hunt for a good colour space
  if (!mapping.configure(colour,channels,0,palette,dimensions))
    { KDU_ERROR(e,3); e <<
        KDU_TXT("Unable to find any colour description which "
        "can be used by the present implementation to render MJ2 track "
        "(index, starting from 0, equals ") << layer_idx <<
        KDU_TXT(") to sRGB.");
    }

  if (track->get_graphics_mode() == MJ2_GRAPHICS_ALPHA)
    mapping.add_alpha_to_configuration(channels,0,palette,dimensions,true);
  else if ((track->get_graphics_mode() == MJ2_GRAPHICS_PREMULT_ALPHA) &&
           mapping.add_alpha_to_configuration(channels,0,palette,
                                              dimensions,false))
    alpha_is_premultiplied = true;
  reference_component = mapping.get_source_component(0);
  active_component = reference_component;
  max_discard_levels = codestream->ifc.get_min_dwt_levels();
  configure_subsampling();
  can_flip = codestream->ifc.can_flip(false);
  have_valid_scale = processing = false;
  invalidate_surface();
}

/******************************************************************************/
/*                         kdrc_stream::change_frame                          */
/******************************************************************************/

void
  kdrc_stream::change_frame(int frame_idx)
{
  if ((mj2_track == NULL) || (frame_idx == mj2_frame_idx))
    return;

  stop_processing();
  mj2_frame_idx = frame_idx;
  mj2_track->seek_to_frame(frame_idx);
  codestream_idx = mj2_track->get_stream_idx(mj2_field_idx);
  if ((codestream != NULL) &&
      !codestream->restart(mj2_track,frame_idx,mj2_field_idx))
    {
      codestream->detach(this);
      codestream = NULL;
    }
  if (codestream == NULL)
    {
      kdrc_codestream *cs = new kdrc_codestream(persistent,cache_threshold);
      try {
          cs->init(mj2_track,frame_idx,mj2_field_idx);
        }
      catch (...) {
          delete cs;
          throw;
        }
      cs->attach(this);
      assert(codestream == cs);
    }
  configure_subsampling();
  invalidate_surface();
}

/******************************************************************************/
/*                        kdrc_stream::set_error_level                        */
/******************************************************************************/

void
  kdrc_stream::set_error_level(int error_level)
{
  assert((codestream != NULL) && (codestream->ifc.exists()));
  switch (error_level) {
      case 0: codestream->ifc.set_fast(); break;
      case 1: codestream->ifc.set_fussy(); break;
      case 2: codestream->ifc.set_resilient(false); break;
      default: codestream->ifc.set_resilient(true);
    }
}

/******************************************************************************/
/*                            kdrc_stream::set_mode                           */
/******************************************************************************/

int
  kdrc_stream::set_mode(int single_idx, kdu_component_access_mode access_mode)
{
  if (single_idx < 0)
    access_mode = KDU_WANT_OUTPUT_COMPONENTS;
  if ((single_idx == single_component) &&
      (component_access_mode == access_mode))
    return single_component;

  // Scan all users for this code-stream to make sure they are not
  // in the midst of processing.
  kdrc_stream *scan;
  for (scan=codestream->head; scan != NULL; scan=scan->next_codestream_user)
    scan->stop_processing();

  single_component = single_idx;
  component_access_mode = access_mode;
  if (single_component >= 0)
    active_component = single_component;
  else
    active_component = reference_component;

  configure_subsampling(); // May change `single_component' if it is invalid

  have_valid_scale = false;
  buffer = NULL;
  invalidate_surface();
  return single_component;
}

/******************************************************************************/
/*                         kdrc_stream::set_thread_env                        */
/******************************************************************************/
  
void
  kdrc_stream::set_thread_env(kdu_thread_env *env, kdu_thread_queue *env_queue)
{
  if ((env != this->env) && processing)
    { kdu_error e; e << "Attempting to change the access thread "
      "associated with a `kdu_region_compositor' object, or "
      "move between multi-threaded and single-threaded access, "
      "while processing in the previous thread or environment is "
      "still going on."; }
  this->env = env;
  this->env_queue = env_queue;
}

/******************************************************************************/
/*                      kdrc_stream::configure_subsampling                    */
/******************************************************************************/

void
  kdrc_stream::configure_subsampling()
{
  int d, n, c;
  assert(active_component >= 0);
  if (max_discard_levels > 32)
    max_discard_levels = 32; // Just in case
  for (d=max_discard_levels; d >= 0; d--)
    {
      codestream->ifc.apply_input_restrictions(0,0,d,0,NULL,
                                               component_access_mode);
      if ((d == max_discard_levels) && (single_component >= 0))
        { // Check that `single_component' is valid
          int num_comps = codestream->ifc.get_num_components(true);
          if (num_comps <= single_component)
            single_component = active_component = num_comps-1;
        }
      kdu_coords subs, min_subs;
      codestream->ifc.get_subsampling(active_component,subs,true);
      min_subs = active_subsampling[d] = subs;
      if (single_component < 0)
        {
          for (n=0; n < mapping.num_channels; n++)
            {
              c = mapping.source_components[n];
              if (c != active_component)
                {
                  codestream->ifc.get_subsampling(c,subs,true);
                  if (subs.x < min_subs.x)
                    min_subs.x = subs.x;
                  if (subs.y < min_subs.y)
                    min_subs.y = subs.y;
                }
            }
        }
      min_subsampling[d] = min_subs;
    }
}

/******************************************************************************/
/*                       kdrc_stream::invalidate_surface                      */
/******************************************************************************/

void
  kdrc_stream::invalidate_surface()
{
  stop_processing();
  valid_region.pos = active_region.pos;
  valid_region.size = kdu_coords(0,0);
  region_in_process = incomplete_region = valid_region;
  is_complete = false;
  priority = KDRC_PRIORITY_MAX;
}

/******************************************************************************/
/*                      kdrc_stream::get_components_in_use                    */
/******************************************************************************/

int
  kdrc_stream::get_components_in_use(int *indices)
{
  int i, c, n=0;

  if (single_component >= 0)
    indices[n++] = single_component;
  else
    for (c=0; c < mapping.num_channels; c++)
      {
        int idx = mapping.source_components[c];
        for (i=0; i < n; i++)
          if (idx == indices[i])
            break;
        if (i == n)
          indices[n++] = idx;
      }
  for (i=n; i < 4; i++)
    indices[i] = -1;
  return n;
}

/******************************************************************************/
/*                           kdrc_stream::set_scale                           */
/******************************************************************************/

kdu_dims
  kdrc_stream::set_scale(kdu_dims full_source_dims, kdu_dims full_target_dims,
                         kdu_coords source_sampling,
                         kdu_coords source_denominator,
                         bool transpose, bool vflip, bool hflip, float scale,
                         int &invalid_scale_code)
{
  kdu_dims result;

  invalid_scale_code = 0;
  if (mj2_track != NULL)
    full_source_dims.pos = full_source_dims.size = kdu_coords(0,0);

  bool no_change = have_valid_scale &&
    (this->vflip == vflip) && (this->hflip == hflip) &&
    (this->transpose == transpose) && (this->scale == scale) &&
    (this->full_source_dims == full_source_dims) &&
    (this->full_target_dims == full_target_dims) &&
    (this->source_sampling == source_sampling) &&
    (this->source_denominator == source_denominator);
  have_valid_scale = false;
  this->vflip = vflip;
  this->hflip = hflip;
  this->transpose = transpose;
  this->scale = scale;
  this->full_source_dims = full_source_dims;
  this->full_target_dims = full_target_dims;
  this->source_sampling = source_sampling;
  this->source_denominator = source_denominator;

  if ((vflip || hflip) && !can_flip)
    { // Required flipping is not supported
      invalid_scale_code |= KDU_COMPOSITOR_CANNOT_FLIP;
      return result;
    }

  // Scan all users for this code-stream to make sure they are not
  // in the midst of processing.
  kdrc_stream *scan;
  for (scan=codestream->head; scan != NULL; scan=scan->next_codestream_user)
    scan->stop_processing();

  // Figure out the expansion factors and the number of discard levels
  double scale_x, scale_y;
  double tol_x=0.0, tol_y=0.0; // Tolerance in X and Y scaling
  if ((source_sampling.x == 0) || (source_sampling.y == 0))
    { // Composit at natural scale of the principle reference component
      scale_x = scale / (double)(active_subsampling[0].x);
      scale_y = scale / (double)(active_subsampling[0].y);

      // Find reasonable scaling tolerances
      tol_x = (2.0/codestream->canvas_dims.size.x) *
        (double)(active_subsampling[0].x);
      tol_y = (2.0/codestream->canvas_dims.size.y) *
        (double)(active_subsampling[0].y);
    }
  else
    { // Find horizontal and vertical scale factors relative to active
      // component dimensions.

      // First convert `full_source_dims' to canvas coordinates and fill in
      // defaults for any empty regions
      bool synthesized_full_source_dims = false;
      if (!full_source_dims)
        {
          full_source_dims.pos = kdu_coords(0,0);
          full_source_dims.size.x =
            long_ceil_ratio(((kdu_long) codestream->canvas_dims.size.x) *
                            source_sampling.x,source_denominator.x);
          full_source_dims.size.y =
            long_ceil_ratio(((kdu_long) codestream->canvas_dims.size.y) *
                            source_sampling.y,source_denominator.y);
          synthesized_full_source_dims = true;
        }
      if (!full_target_dims)
        {
          full_target_dims.pos = kdu_coords(0,0);
          full_target_dims.size = full_source_dims.size;
        }

      // Next convert `full_source_dims' to dimensions on the codestream canvas
      if (synthesized_full_source_dims)
        full_source_dims = codestream->canvas_dims;
      else
        {
          kdu_coords min = full_source_dims.pos;
          kdu_coords lim = min + full_source_dims.size;
          min.x = long_ceil_ratio(((kdu_long) min.x)*source_denominator.x,
                                  source_sampling.x);
          min.y = long_ceil_ratio(((kdu_long) min.y)*source_denominator.y,
                                  source_sampling.y);
          lim.x = long_ceil_ratio(((kdu_long) lim.x)*source_denominator.x,
                                  source_sampling.x);
          lim.y = long_ceil_ratio(((kdu_long) lim.y)*source_denominator.y,
                                  source_sampling.y);
          full_source_dims.pos = min; full_source_dims.size = lim-min;
          full_source_dims.pos +=
            codestream->canvas_dims.pos; // Convert to canvas origin
        }

      // Now find the scale factors
      scale_x = full_target_dims.size.x / (double) full_source_dims.size.x;
      scale_y = full_target_dims.size.y / (double) full_source_dims.size.y;
      scale_x *= scale;
      scale_y *= scale;

      // Find reasonable scaling tolerances
      if ((full_source_dims.size.x / active_subsampling[0].x) <
          full_target_dims.size.x)
        tol_x = (2.0/full_source_dims.size.x)*(double)(active_subsampling[0].x);
      else
        tol_x = 2.0 / full_target_dims.size.x;

      if ((full_source_dims.size.y / active_subsampling[0].y) <
          full_target_dims.size.y)
        tol_y = (2.0/full_source_dims.size.y)*(double)(active_subsampling[0].y);
      else
        tol_y = 2.0 / full_target_dims.size.y;
    }
  if (tol_x > 0.1)
    tol_x = 0.1;
  if (tol_y > 0.1)
    tol_y = 0.1;

  discard_levels = 0;
  while (((scale_x*min_subsampling[discard_levels].x) < (1.0-tol_x)) ||
         ((scale_y*min_subsampling[discard_levels].y) < (1.0-tol_y)))
    {
      if (discard_levels >= max_discard_levels)
        { // Scale is too small
          invalid_scale_code |= KDU_COMPOSITOR_SCALE_TOO_SMALL;
          return result;
        }
      discard_levels++;
    }

  // Make minor adjustments to `scale_x' and `scale_y' to as to encourage
  // the selection of integer scaling factors for the active component
  double min_scale_x = scale_x * min_subsampling[discard_levels].x;
  double min_scale_y = scale_y * min_subsampling[discard_levels].y;
  scale_x *= active_subsampling[discard_levels].x;
  scale_y *= active_subsampling[discard_levels].y;
  double tx = floor(0.5 + scale_x);
  double ty = floor(0.5 + scale_y);
  if (fabs(tx-scale_x) < tol_x)
    { min_scale_x *= tx / scale_x;   scale_x = tx; }
  if (fabs(ty-scale_y) < tol_y)
    { min_scale_y *= ty / scale_y;   scale_y = ty; }
  if (min_scale_x < 1.0)
    { scale_x *= 1.0 / min_scale_x;   min_scale_x = 1.0; }
  if (min_scale_y < 1.0)
    { scale_y *= 1.0 / min_scale_y;   min_scale_y = 1.0; }

  // Convert scale factors into fractions
  expansion_denominator.x = 1;
  expansion_numerator.x = (int) ceil(scale_x); // Try integer expansion first
  if (((double) expansion_numerator.x) >
      ((1.0+tol_x) * scale_x * expansion_denominator.x))
    { // Try rational expansion with full_source_dims as the denominator
      expansion_denominator.x = full_source_dims.size.x;
      if (source_sampling.x == 0)
        expansion_denominator.x = codestream->canvas_dims.size.x;
      assert(expansion_denominator.x > 0);
      while ((expansion_denominator.x > 1) &&
             ((scale_x * expansion_denominator.x) > (double)(1<<30)))
        expansion_denominator.x >>= 1;
      expansion_numerator.x = (int) ceil(scale_x * expansion_denominator.x);
    }

  expansion_denominator.y = 1;
  expansion_numerator.y = (int) ceil(scale_y); // Try integer expansion first
  if (((double) expansion_numerator.y) >
      ((1.0+tol_y) * scale_y * expansion_denominator.y))
    { // Try rational expansion with full_source_dims as the denominator
      expansion_denominator.y = full_source_dims.size.y;
      if (source_sampling.y == 0)
        expansion_denominator.y = codestream->canvas_dims.size.y;
      assert(expansion_denominator.y > 0);
      while ((expansion_denominator.y > 1) &&
             ((scale_y * expansion_denominator.y) > (double)(1<<30)))
        expansion_denominator.y >>= 1;
      expansion_numerator.y = (int) ceil(scale_y * expansion_denominator.y);
    }

  if ((source_sampling.x != 0) && (source_sampling.y != 0))
    { // Find the amount by which the original compositing target region is
      // actually expanded and find the expanded target region on the
      // compositing grid.
      scale_x = scale*(expansion_numerator.x/(scale_x*expansion_denominator.x));
      scale_y = scale*(expansion_numerator.y/(scale_y*expansion_denominator.y));
      result.pos.x = (int) floor(0.5 + full_target_dims.pos.x * scale_x);
      result.pos.y = (int) floor(0.5 + full_target_dims.pos.y * scale_y);
      codestream->ifc.apply_input_restrictions(0,0,discard_levels,0,
                                       &full_source_dims,component_access_mode);
    }
  else
    { // No cropping or offsets
      codestream->ifc.apply_input_restrictions(0,0,discard_levels,0,NULL,
                                               component_access_mode);
    }

  // Apply orientation adjustments and find rendered image dimensions
  codestream->ifc.change_appearance(transpose,vflip,hflip);
  if (transpose)
    {
      expansion_numerator.transpose();
      expansion_denominator.transpose();
    }
  kdu_dims active_comp_dims;
  codestream->ifc.get_dims(active_component,active_comp_dims,true);
  rendering_dims = find_render_dims(active_comp_dims,
                                    expansion_numerator,expansion_denominator);
  result.size = rendering_dims.size;
  if (transpose)
    result.size.transpose(); // Flip dimensions back to original orientation,
          // since this is the one for which `result.pos' was computed
  result.to_apparent(transpose,vflip,hflip);

  // Calculate the scaled and rotated composition region and finish up
  compositing_offset = rendering_dims.pos - result.pos;
  if (!no_change)
    {
      buffer = NULL;
      invalidate_surface();
    }
  have_valid_scale = true;

  // Pass on geometry info to the overlay manager, if appropriate
  if (overlay != NULL)
    overlay->set_geometry(codestream->canvas_dims.pos,
                          active_subsampling[discard_levels],
                          transpose,vflip,hflip,expansion_numerator,
                          expansion_denominator,compositing_offset);

  return result;
}

/******************************************************************************/
/*                      kdrc_stream::find_optimal_scale                       */
/******************************************************************************/

float
  kdrc_stream::find_optimal_scale(float anchor_scale, float min_scale,
                                  float max_scale, kdu_dims full_source_dims,
                                  kdu_dims full_target_dims,
                                  kdu_coords source_sampling,
                                  kdu_coords source_denominator)
{
  if (mj2_track != NULL)
    full_source_dims.pos = full_source_dims.size = kdu_coords(0,0);
  if (anchor_scale < min_scale)
    anchor_scale = min_scale;
  if (anchor_scale > max_scale)
    anchor_scale = max_scale;

  double scale_x, scale_y;
  if ((source_sampling.x == 0) || (source_sampling.y == 0))
    { // Composit at natural scale of the principle reference component
      scale_x = anchor_scale / (double)(active_subsampling[0].x);
      scale_y = anchor_scale / (double)(active_subsampling[0].y);
    }
  else
    { // Find horizontal and vertical scale factors relative to reference
      // component dimensions.

      // First convert `full_source_dims' to canvas coordinates and fill in
      // defaults for any empty regions
      bool synthesized_full_source_dims = false;
      if (!full_source_dims)
        {
          full_source_dims.pos = kdu_coords(0,0);
          full_source_dims.size.x =
            long_ceil_ratio(((kdu_long) codestream->canvas_dims.size.x) *
                            source_sampling.x,source_denominator.x);
          full_source_dims.size.y =
            long_ceil_ratio(((kdu_long) codestream->canvas_dims.size.y) *
                            source_sampling.y,source_denominator.y);
          synthesized_full_source_dims = true;
        }
      if (!full_target_dims)
        {
          full_target_dims.pos = kdu_coords(0,0);
          full_target_dims.size = full_source_dims.size;
        }

      // Next convert `full_source_dims' to dimensions on the codestream canvas
      if (synthesized_full_source_dims)
        full_source_dims = codestream->canvas_dims;
      else
        {
          kdu_coords min = full_source_dims.pos;
          kdu_coords lim = min + full_source_dims.size;
          min.x = long_ceil_ratio(((kdu_long) min.x)*source_denominator.x,
                                  source_sampling.x);
          min.y = long_ceil_ratio(((kdu_long) min.y)*source_denominator.y,
                                  source_sampling.y);
          lim.x = long_ceil_ratio(((kdu_long) lim.x)*source_denominator.x,
                                  source_sampling.x);
          lim.y = long_ceil_ratio(((kdu_long) lim.y)*source_denominator.y,
                                  source_sampling.y);
          full_source_dims.pos = min; full_source_dims.size = lim-min;
          full_source_dims.pos +=
            codestream->canvas_dims.pos; // Convert to canvas origin
        }

      // Now find the scale factors
      scale_x = full_target_dims.size.x / (double) full_source_dims.size.x;
      scale_y = full_target_dims.size.y / (double) full_source_dims.size.y;
      scale_x *= anchor_scale;
      scale_y *= anchor_scale;
    }

  // At this point `scale_x' and `scale_y' hold the amount by which the canvas
  // dimensions must be scaled in order to achieve the `anchor_scale' factor.

  double fact, best_fact=-1.0;
  double max_fact = max_scale / anchor_scale;
  double min_fact = min_scale / anchor_scale;

  // Pass through all the possible discard levels
  int d = 0; // Simulated number of discard levels
  do {
     double min_x = scale_x*min_subsampling[d].x;
     double min_y = scale_y*min_subsampling[d].y;
     double active_x = scale_x*active_subsampling[d].x;
     double active_y = scale_y*active_subsampling[d].y;
     double active_xy =
       (active_x < active_y)?active_x:active_y; // Optimize for min scale
     double inv_xy = 1.0 / active_xy;

     // Find smallest value of `fact' such that active_xy*fact is an integer and
     // min_x*fact and min_y*fact are both >= 1.0.
     fact = 1.0 / ((min_x < min_y)?min_x:min_y);
     fact = ceil(fact*active_xy) * inv_xy;
     if ((((fact-inv_xy)*min_x) >= 1.0) && (((fact-inv_xy)*min_y) >= 1.0))
       fact -= inv_xy; // Just a precaution, in case numerical errors conspire
                       // against us in the above calculation.
     if ((d == 0) && (fact < 1.0))
       { // A larger value for `fact' may be more appropriate
         fact += inv_xy * floor((1.0-fact)*active_xy);
         if ((fact < min_fact) ||
             (((fact+inv_xy) < (1.0 / fact)) && ((fact+inv_xy) <= max_fact)))
           fact += inv_xy;
       }
     if ((fact >= min_fact) && (fact <= max_fact))
       { // This factor is a candidate.  Pick the one which is closest to 1.0,
         // meaning that the scale will be closest to `anchor_scale'.
         if (best_fact < 0.0)
           best_fact = fact;
         else
           {
             double best_ratio = (best_fact < 1.0)?(1.0/best_fact):best_fact;
             double ratio = (fact < 1.0)?(1.0/fact):fact;
             if (ratio < best_ratio)
               best_fact = fact; // Picks the factor which is closest to 1.0 in
                                 // in the geometric sense.
           }
       }
     d++;
    } while ((d <= max_discard_levels) && (fact >= min_fact));
  if (best_fact < 0.0)
    best_fact = fact; // In case even `max_scale' was too small

  return (float)(best_fact * anchor_scale);
}

/******************************************************************************/
/*                 kdrc_stream::get_component_scale_factors                   */
/******************************************************************************/

void
  kdrc_stream::get_component_scale_factors(kdu_dims full_source_dims,
                                           kdu_dims full_target_dims,
                                           kdu_coords source_sampling,
                                           kdu_coords source_denominator,
                                           double &scale_x, double &scale_y)
{
  if (mj2_track != NULL)
    full_source_dims.pos = full_source_dims.size = kdu_coords(0,0);
  kdu_coords ref_subs = active_subsampling[0];

  if ((source_sampling.x == 0) || (source_sampling.y == 0))
    { // Composit at natural scale of the principle reference component
      scale_x = scale_y = 1.0;
    }
  else
    { // Find horizontal and vertical scale factors relative to reference
      // component dimensions.

      // First convert `full_source_dims' to canvas coordinates and fill in
      // defaults for any empty regions
      if (!full_source_dims)
        {
          full_source_dims.pos = kdu_coords(0,0);
          full_source_dims.size.x =
            long_ceil_ratio(((kdu_long) codestream->canvas_dims.size.x) *
                            source_sampling.x,source_denominator.x);
          full_source_dims.size.y =
            long_ceil_ratio(((kdu_long) codestream->canvas_dims.size.y) *
                            source_sampling.y,source_denominator.y);
        }
      if (!full_target_dims)
        {
          full_target_dims.pos = kdu_coords(0,0);
          full_target_dims.size = full_source_dims.size;
        }
      kdu_coords min = full_source_dims.pos;
      kdu_coords lim = min + full_source_dims.size;
      min.x = long_ceil_ratio(((kdu_long) min.x)*source_denominator.x,
                              source_sampling.x);
      min.y = long_ceil_ratio(((kdu_long) min.y)*source_denominator.y,
                              source_sampling.y);
      lim.x = long_ceil_ratio(((kdu_long) lim.x)*source_denominator.x,
                              source_sampling.x);
      lim.y = long_ceil_ratio(((kdu_long) lim.y)*source_denominator.y,
                              source_sampling.y);
      full_source_dims.pos = min; full_source_dims.size = lim-min;
      full_source_dims.pos +=
        codestream->canvas_dims.pos; // Convert to canvas origin

      // Now find the scale factors
      scale_x = full_target_dims.size.x / (double) full_source_dims.size.x;
      scale_y = full_target_dims.size.y / (double) full_source_dims.size.y;
      scale_x *= ref_subs.x;
      scale_y *= ref_subs.y;
    }
}

/******************************************************************************/
/*                    kdrc_stream::find_composited_region                     */
/******************************************************************************/

kdu_dims
  kdrc_stream::find_composited_region(bool apply_cropping)
{
  if (!have_valid_scale)
    return kdu_dims();

  kdu_dims result;
  if (apply_cropping)
    result = rendering_dims;
  else
    {
      kdu_coords subsampling = active_subsampling[discard_levels];
      kdu_coords min = codestream->canvas_dims.pos;
      kdu_coords lim = min + codestream->canvas_dims.size;
      min.x = ceil_ratio(min.x,subsampling.x);
      min.y = ceil_ratio(min.y,subsampling.y);
      lim.x = ceil_ratio(lim.x,subsampling.x);
      lim.y = ceil_ratio(lim.y,subsampling.y);
      result.pos = min;
      result.size = lim - min;
      if (transpose)
        result.transpose(); // Convert to orientation used by expansion factors
      result = find_render_dims(result,expansion_numerator,
                                expansion_denominator);
      if (transpose)
        result.transpose(); // Convert back to original geometric alignment
      result.to_apparent(transpose,vflip,hflip); // Apply geometric changes
    }

  result.pos -= compositing_offset;
  return result;
}

/******************************************************************************/
/*                       kdrc_stream::get_packet_stats                        */
/******************************************************************************/

void
  kdrc_stream::get_packet_stats(kdu_dims region, kdu_long &precinct_samples,
                                kdu_long &packet_samples,
                                kdu_long &max_packet_samples)
{
  if ((!have_valid_scale) || !region)
    return;

  kdrc_stream *scan;
  for (scan=codestream->head; scan != NULL; scan=scan->next_codestream_user)
    scan->stop_processing();

  region.pos += compositing_offset; // Adjust to rendering grid
  region &= rendering_dims;
  
  // Map `region' to the reference component's coordinate system
  kdu_long num, den;
  kdu_coords min = region.pos;
  kdu_coords lim = min + region.size;

  num = expansion_denominator.x;
  den = expansion_numerator.x;
  min.x = long_ceil_ratio(num*min.x,den);
  lim.x = long_floor_ratio(num*lim.x,den);
  if (min.x >= lim.x)
    {
      min.x = long_floor_ratio(num*min.x,den);
      lim.x = long_ceil_ratio(num*lim.x,den);
    }

  num = expansion_denominator.y;
  den = expansion_numerator.y;
  min.y = long_ceil_ratio(num*min.y,den);
  lim.y = long_floor_ratio(num*lim.y,den);
  if (min.y >= lim.y)
    {
      min.y = long_floor_ratio(num*min.y,den);
      lim.y = long_ceil_ratio(num*lim.y,den);
    }

  kdu_dims ref_region;
  ref_region.pos = min;
  ref_region.size = lim - min;

  // Convert to a region on the high resolution codestream canvas
  int ref_idx = (single_component >= 0)?single_component:reference_component;
  kdu_dims canvas_region;
  codestream->ifc.change_appearance(transpose,vflip,hflip); // Just to be sure
  codestream->ifc.apply_input_restrictions(0,0,discard_levels,0,NULL,
                                           component_access_mode);
  codestream->ifc.map_region(ref_idx,ref_region,canvas_region,true);
  
  // Restrict to the region of interest, as well as the components of interest
  if (single_component < 0)
    codestream->ifc.apply_input_restrictions(mapping.num_channels,
                          mapping.source_components,discard_levels,0,
                          &canvas_region,KDU_WANT_OUTPUT_COMPONENTS);
  else
    codestream->ifc.apply_input_restrictions(single_component,1,
                                             discard_levels,0,&canvas_region,
                                             component_access_mode);

  // Scan through all the relevant precincts
  kdu_dims valid_tiles;  codestream->ifc.get_valid_tiles(valid_tiles);
  kdu_coords t_idx;
  for (t_idx.y=0; t_idx.y < valid_tiles.size.y; t_idx.y++)
    for (t_idx.x=0; t_idx.x < valid_tiles.size.x; t_idx.x++)
      {
        kdu_tile tile = codestream->ifc.open_tile(t_idx+valid_tiles.pos);
        int tile_layers = tile.get_num_layers();
        int c, num_tile_comps=tile.get_num_components();
        for (c=0; c < num_tile_comps; c++)
          {
            kdu_tile_comp tc = tile.access_component(c);
            if (!tc)
              continue;
            int r, num_resolutions=tc.get_num_resolutions();
            for (r=0; r < num_resolutions; r++)
              {
                kdu_resolution res = tc.access_resolution(r);
                kdu_dims valid_precs; res.get_valid_precincts(valid_precs);
                kdu_coords p_idx, abs_p_idx;
                kdu_long p_samples;
                int p_packets;
                for (p_idx.y=0; p_idx.y < valid_precs.size.y; p_idx.y++)
                  for (p_idx.x=0; p_idx.x < valid_precs.size.x; p_idx.x++)
                    {
                      abs_p_idx = p_idx + valid_precs.pos;
                      p_samples = res.get_precinct_samples(abs_p_idx);
                      p_packets = res.get_precinct_packets(abs_p_idx);
                      assert(p_packets <= tile_layers);
                      precinct_samples += p_samples;
                      packet_samples += (p_samples * p_packets);
                      max_packet_samples += (p_samples * tile_layers);
                    }
              }
          }
        tile.close();
      }
}

/******************************************************************************/
/*                      kdrc_stream::set_buffer_surface                       */
/******************************************************************************/

void
  kdrc_stream::set_buffer_surface(kdu_compositor_buf *buffer,
                                  kdu_dims buffer_region,
                                  bool start_from_scratch)
{
  assert(have_valid_scale);
  this->buffer = buffer;
  buffer_region.pos += compositing_offset; // Adjust to rendering grid
  this->buffer_origin = buffer_region.pos;
  buffer_region &= rendering_dims;
  bool no_change = (buffer_region == this->active_region);
  this->active_region = buffer_region;

  if (start_from_scratch)
    {
      stop_processing();
      valid_region.pos = active_region.pos;
      valid_region.size = kdu_coords(0,0);
      region_in_process = incomplete_region = valid_region;
      is_complete = false;
      priority = KDRC_PRIORITY_MAX;
      return;
    }

  if (no_change)
    return;

  // Intersect with current region in process
  if (processing)
    {
      region_in_process &= active_region;
      incomplete_region &= active_region;
      if (!incomplete_region)
        stop_processing();
    }

  // Intersect with valid region
  valid_region &= active_region;
  update_completion_status();
}

/******************************************************************************/
/*                          kdrc_stream::map_region                           */
/******************************************************************************/

kdu_dims
  kdrc_stream::map_region(kdu_dims region)
{
  if (!have_valid_scale)
    return kdu_dims();
  region.pos += compositing_offset; // Adjust to rendering grid
  region &= rendering_dims;
  if (!region)
    return kdu_dims();

  // Adjust region down, based on expansion factors
  kdu_long num, den;
  kdu_coords min = region.pos;
  kdu_coords lim = min + region.size;

  num = expansion_denominator.x;  den = expansion_numerator.x;
  min.x = long_floor_ratio(num*min.x,den);
  lim.x = long_ceil_ratio(num*lim.x,den);

  num = expansion_denominator.y;  den = expansion_numerator.y;
  min.y = long_floor_ratio(num*min.y,den);
  lim.y = long_ceil_ratio(num*lim.y,den);

  // Account for rotation/flipping
  region.pos = min;
  region.size = lim - min;
  region.from_apparent(transpose,vflip,hflip);
  min = region.pos;
  lim = min + region.size;

  // Adjust region up, based on discard levels and ref component subsampling
  min.x *= active_subsampling[discard_levels].x;
  min.y *= active_subsampling[discard_levels].y;
  lim.x *= active_subsampling[discard_levels].x;
  lim.y *= active_subsampling[discard_levels].y;

  // Put region back together and subtract the image origin
  region.pos = min - codestream->canvas_dims.pos;
  region.size = lim - min;
  if (region.pos.x < 0)
    region.pos.x = 0; // Correct for possible side-effects of rounding
  if (region.pos.y < 0)
    region.pos.y = 0; // Correct for possible side-effects of rounding
  return region;
}

/******************************************************************************/
/*                      kdrc_stream::inverse_map_region                       */
/******************************************************************************/

kdu_dims
  kdrc_stream::inverse_map_region(kdu_dims region)
{
  if (!have_valid_scale)
    return kdu_dims();
  
  // Put back image offset
  kdu_coords min = region.pos + codestream->canvas_dims.pos;
  kdu_coords lim = min + region.size;
  
  // Adjust region down, based on discard levels and ref component sampling
  min.x = long_ceil_ratio(min.x,active_subsampling[discard_levels].x);
  min.y = long_ceil_ratio(min.y,active_subsampling[discard_levels].y);
  lim.x = long_ceil_ratio(lim.x,active_subsampling[discard_levels].x);
  lim.y = long_ceil_ratio(lim.y,active_subsampling[discard_levels].y);
  
  // Account for rotation/flipping
  region.pos = min;
  region.size = lim - min;
  region.to_apparent(transpose,vflip,hflip);
  min = region.pos;
  lim = min + region.size;

  // Adjust region up, based on expansion factors
  kdu_long num, den;
  
  den = expansion_denominator.x;  num = expansion_numerator.x;
  min.x = long_floor_ratio(num*min.x,den);
  lim.x = long_ceil_ratio(num*lim.x,den);
  
  den = expansion_denominator.y;  num = expansion_numerator.y;
  min.y = long_floor_ratio(num*min.y,den);
  lim.y = long_ceil_ratio(num*lim.y,den);
  
  // Subtract composing offset and convert back to region
  region.pos = min - compositing_offset;
  region.size = lim - min;
  return region;
}

/******************************************************************************/
/*                           kdrc_stream::process                             */
/******************************************************************************/

bool
  kdrc_stream::process(int suggested_increment, kdu_dims &new_region,
                       int &invalid_scale_code)
{
  invalid_scale_code = 0;
  assert(buffer != NULL);
  if ((!processing) && !active_region.is_empty())
    {
      // Decide on region to process
      if (valid_region.area() < (active_region.area()>>2))
        { // Start from scratch
          valid_region.pos = active_region.pos;
          valid_region.size = kdu_coords(0,0);
          region_in_process = active_region;
        }
      else
        { /* The new region to be processed should share a boundary with
             `valid_reg' so that the two regions can be added together to
             form a new rectangular valid region.  Of the four possibilities,
             pick the one which leads to the largest region being processed. */
          kdu_coords valid_min = valid_region.pos;
          kdu_coords valid_lim = valid_min + valid_region.size;
          kdu_coords active_min = active_region.pos;
          kdu_coords active_lim = active_min + active_region.size;
          int needed_left = valid_min.x - active_min.x;
          int needed_right = active_lim.x - valid_lim.x;
          int needed_above = valid_min.y - active_min.y;
          int needed_below = active_lim.y - valid_lim.y;
          assert((needed_left >= 0) && (needed_right >= 0) &&
                 (needed_above >= 0) && (needed_below >= 0));
          kdu_dims region_left = valid_region;
          region_left.pos.x = active_min.x; region_left.size.x = needed_left;
          kdu_dims region_right = valid_region;
          region_right.pos.x = valid_lim.x; region_right.size.x = needed_right;
          kdu_dims region_above = valid_region;
          region_above.pos.y = active_min.y; region_above.size.y = needed_above;
          kdu_dims region_below = valid_region;
          region_below.pos.y = valid_lim.y; region_below.size.y = needed_below;
          region_in_process = region_left;
          if ((region_in_process.area() < region_right.area()) ||
              !region_in_process)
            region_in_process = region_right;
          if ((region_in_process.area() < region_above.area()) ||
              !region_in_process)
            region_in_process = region_above;
          if ((region_in_process.area() < region_below.area()) ||
              !region_in_process)
            region_in_process = region_below;
        }

      incomplete_region = region_in_process;
      if (!region_in_process.is_empty())
        { // Start the decompressor
          assert(!codestream->in_use);
          decompressor.set_white_stretch(8);
          if (!decompressor.start(codestream->ifc,
                                  ((single_component<0)?(&mapping):NULL),
                                  single_component,discard_levels,
                                  max_display_layers,region_in_process,
                                  expansion_numerator,expansion_denominator,
                                  false,component_access_mode,true,
                                  env,env_queue))
            throw (int) 0; // Something went wrong
          else
            processing = codestream->in_use = true;
        }
    }

  new_region.size = kdu_coords(0,0);
  if (processing)
    { // Process some more data
      int row_gap;
      kdu_uint32 *buf32=buffer->get_buf(row_gap,false);
      bool process_result;
      if (alpha_only && (single_component < 0))
        { // Generating a separate alpha channel here
          kdu_byte *buf8 = ((kdu_byte *) buf32) + ((little_endian)?3:0);
          process_result =
            decompressor.process(&buf8,false,4,buffer_origin,row_gap,
                                 suggested_increment,0,incomplete_region,
                                 new_region);
        }
      else if ((single_component < 0) && (layer != NULL) &&
               layer->have_alpha_channel &&
               (mapping.num_colour_channels == mapping.num_channels))
        { // There is an alpha channel, but it is not rendered from this
          // codestream.  This means that we cannot run the risk of overwriting
          // alpha information in the call to `kdu_region_decompressor::process'
          kdu_byte *bufs[3], *base8=(kdu_byte *) buf32;
          if (little_endian)
            { bufs[0]=base8+2; bufs[1]=base8+1; bufs[2]=base8; }
          else
            { bufs[0]=base8+1; bufs[1]=base8+2; bufs[2]=base8+3; }
          process_result =
            decompressor.process(bufs,true,4,buffer_origin,
                                 row_gap,suggested_increment,0,
                                 incomplete_region,new_region);
        }
      else
        { // Typical case, where all data is generated at once.  If there is
          // no alpha channel, it will automatically be set to 0xFF in the
          // following call to the most efficient version of the `process'
          // function.
          process_result =
            decompressor.process((kdu_int32 *) buf32,buffer_origin,row_gap,
                                 suggested_increment,0,incomplete_region,
                                 new_region);
        }
      if ((!process_result) || !incomplete_region)
        {
          processing = codestream->in_use = false;
          if (!decompressor.finish())
            { // Code-stream failure; must destroy
              throw (int) 0;
            }
          else if ((max_discard_levels =
                    codestream->ifc.get_min_dwt_levels()) < discard_levels)
            {
              discard_levels = max_discard_levels;
              have_valid_scale = false;
              invalid_scale_code |= KDU_COMPOSITOR_SCALE_TOO_SMALL;
              invalidate_surface();
              return false;
            }
          else if ((hflip || vflip) &&
                   !(can_flip = codestream->ifc.can_flip(false)))
            {
              hflip = vflip = have_valid_scale = false;
              invalid_scale_code |= KDU_COMPOSITOR_CANNOT_FLIP;
              invalidate_surface();
              return false;
            }
          else
            { // Combine newly completed region with the existing valid region
              valid_region &= active_region; // Just to be sure
              region_in_process &= active_region; // Just to be sure
              if (valid_region.is_empty())
                valid_region = region_in_process;
              else
                {
                  kdu_coords a_min = valid_region.pos;
                  kdu_coords a_lim = a_min + valid_region.size;
                  kdu_coords b_min = region_in_process.pos;
                  kdu_coords b_lim = b_min + region_in_process.size;
                  if ((a_min.x == b_min.x) && (a_lim.x == b_lim.x))
                    { // Regions have same horizontal edge profile
                      if ((a_min.y <= b_lim.y) && (a_lim.y >= b_min.y))
                        { // Union of regions is another rectangle
                          a_min.y = (a_min.y < b_min.y)?a_min.y:b_min.y;
                          a_lim.y = (a_lim.y > b_lim.y)?a_lim.y:b_lim.y;
                        }
                    }
                  else if ((a_min.y == b_min.y) && (a_lim.y == b_lim.y))
                    { // Regions have same vertical edge profile
                      if ((a_min.x <= b_lim.x) && (a_lim.x >= b_min.x))
                        { // Union of regions is another rectangle
                          a_min.x = (a_min.x < b_min.x)?a_min.x:b_min.x;
                          a_lim.x = (a_lim.x > b_lim.x)?a_lim.x:b_lim.x;
                        }
                    }
                  valid_region.pos = a_min;
                  valid_region.size = a_lim - a_min;
                }
            }
        }
      new_region.pos -= compositing_offset; // Convert to compositing grid
    }

  update_completion_status();
  return true;
}

/******************************************************************************/
/*                        kdrc_stream::adjust_refresh                         */
/******************************************************************************/

void
  kdrc_stream::adjust_refresh(kdu_dims region, kdrc_refresh *refresh)
{
  assert(have_valid_scale);
  kdu_dims active = active_region; active.pos -= compositing_offset;
  kdu_dims valid = valid_region; valid.pos -= compositing_offset;
  valid &= region;
  kdu_dims intersection = region & active;
  if (intersection.is_empty())
    refresh->add_region(region);
  else
    {
      int left = intersection.pos.x - region.pos.x;
      int right = (region.pos.x+region.size.x)
                - (intersection.pos.x+intersection.size.x);
      int top = intersection.pos.y - region.pos.y;
      int bottom = (region.pos.y+region.size.y)
                 - (intersection.pos.y+intersection.size.y);
      assert((left >= 0) && (right >= 0) && (top >= 0) && (bottom >= 0));
      kdu_dims new_region;
      if (top > 0)
        {
          new_region = region;
          new_region.size.y = top;
          refresh->add_region(new_region);
        }
      if (bottom > 0)
        {
          new_region = region;
          new_region.pos.y = intersection.pos.y+intersection.size.y;
          new_region.size.y = bottom;
          refresh->add_region(new_region);
        }
      if (left > 0)
        {
          new_region = intersection;
          new_region.pos.x = region.pos.x;
          new_region.size.x = left;
          refresh->add_region(new_region);
        }
      if (right > 0)
        {
          new_region = intersection;
          new_region.pos.x = intersection.pos.x+intersection.size.x;
          new_region.size.x = right;
          refresh->add_region(new_region);
        }
      if (!valid.is_empty())
        refresh->add_region(valid); // This will not be refreshed
    }
}


/* ========================================================================== */
/*                                kdrc_layer                                  */
/* ========================================================================== */

/******************************************************************************/
/*                          kdrc_layer::kdrc_layer                            */
/******************************************************************************/

kdrc_layer::kdrc_layer(kdu_region_compositor *owner)
{
  this->owner = owner;
  for (int s=0; s < 2; s++)
    {
      streams[s] = NULL;
      stream_sampling[s] = stream_denominator[s] = kdu_coords(1,1);
    }
  mj2_track = NULL;
  mj2_frame_idx = mj2_field_handling = 0;
  mj2_pending_frame_change = false;
  init_transpose = init_vflip = init_hflip = false;

  overlay = NULL;
  have_alpha_channel = alpha_is_premultiplied = have_overlay_info = false;
  overlay_buffer_size = buffer_size = kdu_coords(0,0);
  full_source_dims.pos = full_source_dims.size = buffer_size;
  layer_region = full_target_dims = full_source_dims;
  have_valid_scale = false;
  repaint_entire_overlay = false;
  buffer = compositing_buffer = overlay_buffer = NULL;
  layer_idx = -1;
  next = prev = NULL;
}

/******************************************************************************/
/*                         kdrc_layer::~kdrc_layer                            */
/******************************************************************************/

kdrc_layer::~kdrc_layer()
{
  if (overlay != NULL)
    overlay->deactivate();
  for (int s=0; s < 2; s++)
    if (streams[s] != NULL)
      owner->remove_stream(streams[s],true);
  if (buffer != NULL)
    owner->internal_delete_buffer(buffer);
  if (overlay_buffer != NULL)
    owner->internal_delete_buffer(overlay_buffer);
}

/******************************************************************************/
/*                             kdrc_layer::init (JPX)                         */
/******************************************************************************/

void
  kdrc_layer::init(jpx_layer_source layer, kdu_dims full_source_dims,
                   kdu_dims full_target_dims, bool init_transpose,
                   bool init_vflip, bool init_hflip)
{
  assert((streams[0] == NULL) && (streams[1] == NULL));

  this->jpx_layer = layer;
  this->mj2_track = NULL;
  this->mj2_frame_idx = 0;
  this->mj2_field_handling = 0;
  this->mj2_pending_frame_change = false;
  this->full_source_dims = full_source_dims;
  this->full_target_dims = full_target_dims;
  this->layer_idx = layer.get_layer_id();
  this->init_transpose = init_transpose;
  this->init_vflip = init_vflip;
  this->init_hflip = init_hflip;
  this->have_valid_scale = false;

  if (!layer.have_stream_headers())
    return; // Need to finish initialization in a later call to `init'

  jp2_channels channels = layer.access_channels();
  int c, comp_idx, lut_idx, stream_idx;
  channels.get_colour_mapping(0,comp_idx,lut_idx,stream_idx);
  streams[0] = owner->add_active_stream(stream_idx,layer_idx,false);
  if (streams[0] == NULL)
    { KDU_ERROR(e,4); e <<
        KDU_TXT("Unable to create compositing layer "
        "(index, starting from 0, equals ") << layer_idx <<
        KDU_TXT("), since its primary codestream cannot be opened.");
    }
  streams[0]->layer = this;
  int main_stream_idx = stream_idx;
  int aux_stream_idx = -1;

  // Now go looking for alpha information
  have_alpha_channel = false;
  alpha_is_premultiplied = false;
  if (streams[0]->get_num_channels() > streams[0]->get_num_colours())
    {
      have_alpha_channel = true;
      alpha_is_premultiplied = streams[0]->is_alpha_premultiplied();
    }
  else
    { // See if alpha is in a different code-stream
      int aux_comp_idx=-1, aux_lut_idx=-1;
      for (c=0; c < channels.get_num_colours(); c++)
        {
          if ((!channels.get_opacity_mapping(c,comp_idx,lut_idx,stream_idx)) ||
              (stream_idx == main_stream_idx))
            { // Unable to find or use alpha information
              aux_stream_idx = -1;
              break;
            }
          else if (c == 0)
            {
              aux_stream_idx = stream_idx;
              aux_comp_idx = comp_idx;
              aux_lut_idx = lut_idx;
            }
          else if ((stream_idx != aux_stream_idx) ||
                   (comp_idx != aux_comp_idx) || (lut_idx != aux_lut_idx))
            { KDU_WARNING(w,0); w <<
                KDU_TXT("Unable to render compositing layer "
                "(index, starting from 0, equals ") << layer_idx <<
                KDU_TXT(") with alpha blending, since there are multiple "
                "distinct alpha channels for a single set of colour channels.");
             aux_stream_idx = -1;
             break;
            }
        }
      if (aux_stream_idx >= 0)
        {
          streams[1] = owner->add_active_stream(aux_stream_idx,layer_idx,true);
          if (streams[1] == NULL)
            {
              aux_stream_idx = -1;
              KDU_WARNING(w,1); w <<
                KDU_TXT("Unable to render compositing layer "
                "(index, starting from 0, equals ") << layer_idx <<
                KDU_TXT(") with alpha blending, since the codestream "
                "containing the alpha data cannot be opened.");
            }
          else
            {
              streams[1]->layer = this;
              have_alpha_channel = true;
              alpha_is_premultiplied = streams[1]->is_alpha_premultiplied();
            }
        }
    }

  // Now find the sampling factors
  kdu_coords align, sampling, denominator;
  c = 0;
  while ((stream_idx=layer.get_codestream_registration(c++,align,sampling,
                                                       denominator)) >= 0)
    if (stream_idx == main_stream_idx)
      {
        stream_sampling[0] = sampling;
        stream_denominator[0] = denominator;
      }
    else if (stream_idx == aux_stream_idx)
      {
        stream_sampling[1] = sampling;
        stream_denominator[1] = denominator;
      }
}

/******************************************************************************/
/*                             kdrc_layer::init (MJ2)                         */
/******************************************************************************/

void
  kdrc_layer::init(mj2_video_source *track, int frame_idx,
                   int field_handling, kdu_dims full_target_dims,
                   bool transpose, bool vflip, bool hflip)
{
  assert((streams[0] == NULL) && (streams[1] == NULL));

  this->jpx_layer = jpx_layer_source(NULL);
  this->mj2_track = track;
  this->mj2_frame_idx = frame_idx;
  this->mj2_field_handling = field_handling;
  this->mj2_pending_frame_change = false;
  this->full_source_dims = kdu_dims();
  this->full_target_dims = full_target_dims;
  this->layer_idx = (int)(track->get_track_idx() - 1);
  this->init_transpose = init_transpose;
  this->init_vflip = init_vflip;
  this->init_hflip = init_hflip;
  this->have_valid_scale = false;

  int field_idx = field_handling & 1;
  if ((frame_idx < 0) || (frame_idx >= track->get_num_frames()))
    { KDU_ERROR_DEV(e,5); e <<
        KDU_TXT("Unable to create compositing layer for MJ2 track ")
        << layer_idx+1 <<
        KDU_TXT(": requested frame index is out of range.");
    }
  if ((field_idx == 1) && (track->get_field_order() == KDU_FIELDS_NONE))
    { KDU_ERROR_DEV(e,6); e <<
        KDU_TXT("Unable to create compositing layer for MJ2 track ")
        << layer_idx+1 <<
        KDU_TXT(": requested field does not exist (source is "
        "progressive, not interlaced).");
    }

  track->seek_to_frame(frame_idx);
  if (!track->can_open_stream(field_idx))
    return; // Need to finish initialization in a later call to `init'
  int stream_idx = track->get_stream_idx(field_idx);
  assert(stream_idx >= 0);

  streams[0] = owner->add_active_stream(stream_idx,layer_idx,false);
  if (streams[0] == NULL)
    { KDU_ERROR(e,7); e <<
        KDU_TXT("Unable to create compositing layer for MJ2 track ")
        << layer_idx+1 <<
        KDU_TXT(": codestream cannot be opened.");
    }
  streams[0]->layer = this;

  // Now go looking for alpha information
  have_alpha_channel = alpha_is_premultiplied = false;
  if (streams[0]->get_num_channels() > streams[0]->get_num_colours())
    {
      have_alpha_channel = true;
      alpha_is_premultiplied = streams[0]->is_alpha_premultiplied();
    }

  // Now set the sampling factors
  stream_sampling[0] = stream_denominator[0] = kdu_coords(1,1);
}

/******************************************************************************/
/*                         kdrc_layer::change_frame                           */
/******************************************************************************/

bool
  kdrc_layer::change_frame(int frame_idx, bool all_or_nothing)
{
  if (streams[0] == NULL)
    {
      if (mj2_track != NULL)
        mj2_frame_idx = frame_idx;
      reinit();
      return (streams[0] != NULL);
    }
  if (mj2_track == NULL)
    return false;
  if ((frame_idx == mj2_frame_idx) && !mj2_pending_frame_change)
    return true;

  if ((frame_idx < 0) || (frame_idx >= mj2_track->get_num_frames()))
    { KDU_ERROR_DEV(e,8); e <<
        KDU_TXT("Requested frame index for MJ2 track ")
        << layer_idx+1 <<
        KDU_TXT(" is out of range.");
    }

  mj2_frame_idx = frame_idx;
  mj2_pending_frame_change = true;

  int s;
  for (s=0; s < 2; s++)
    if (streams[s] != NULL)
      {
        int fld_idx=s, frm_idx=mj2_frame_idx;
        if (mj2_field_handling & 1)
          fld_idx = 1-s;
        if ((fld_idx==0) && (mj2_field_handling == 3))
          frm_idx++;
        mj2_track->seek_to_frame(frm_idx);
        if (!mj2_track->can_open_stream(fld_idx))
          return false;
        if (!all_or_nothing)
          streams[s]->change_frame(frm_idx);
      }

  if (all_or_nothing)
    { // Make the frame changes here, now we know they can succeed
      for (s=0; s < 2; s++)
        if (streams[s] != NULL)
          {
            int fld_idx=s, frm_idx=mj2_frame_idx;
            if (mj2_field_handling & 1)
              fld_idx = 1-s;
            if ((fld_idx==0) && (mj2_field_handling == 3))
              frm_idx++;
            mj2_track->seek_to_frame(frm_idx);
            streams[s]->change_frame(frm_idx);
          }
    }

  mj2_pending_frame_change = false;
  return true;
}

/******************************************************************************/
/*                           kdrc_layer::activate                             */
/******************************************************************************/

void
  kdrc_layer::activate(kdu_dims full_source_dims, kdu_dims full_target_dims,
                       bool init_transpose, bool init_vflip, bool init_hflip,
                       int frame_idx, int field_handling)
{
  int s;

  if (streams[0] == NULL)
    {
      if (jpx_layer.exists())
        init(jpx_layer,full_source_dims,full_target_dims,
             init_transpose,init_vflip,init_hflip);
      else
        init(mj2_track,frame_idx,field_handling,full_target_dims,
             init_transpose,init_vflip,init_hflip);
      return;
    }
  assert(overlay == NULL);
  for (s=0; s < 2; s++)
    if (streams[s] != NULL)
      {
        if (!streams[s]->is_active)
          {
            bool alpha_only = (s==1) && jpx_layer.exists();
            kdrc_stream *tmp =
              owner->add_active_stream(streams[s]->codestream_idx,
                                       streams[s]->layer_idx,alpha_only);
            if (tmp != streams[s])
              assert(0);
          }
        streams[s]->set_mode(-1,KDU_WANT_OUTPUT_COMPONENTS);
                                   // In case we were in single-component mode
        streams[s]->layer = this;
      }
  repaint_entire_overlay = false;
  if (full_target_dims != this->full_target_dims)
    have_valid_scale = false;
  this->full_target_dims = full_target_dims;
  this->init_transpose = init_transpose;
  this->init_vflip = init_vflip;
  this->init_hflip = init_hflip;
  if (jpx_layer.exists())
    {
      if (full_source_dims != this->full_source_dims)
        have_valid_scale = false;
      this->full_source_dims = full_source_dims;
    }
  else if ((field_handling != mj2_field_handling) ||
           (((frame_idx != mj2_frame_idx) || mj2_pending_frame_change) &&
            !change_frame(frame_idx,true)))
    { // Delete streams and re-initialize
      for (s=0; s < 2; s++)
        {
          if (streams[s] != NULL)
            owner->remove_stream(streams[s],true);
          streams[s] = NULL;
        }
      reinit();
    }
}

/******************************************************************************/
/*                          kdrc_layer::deactivate                            */
/******************************************************************************/

void
  kdrc_layer::deactivate()
{
  for (int s=0; s < 2; s++)
    if (streams[s] != NULL)
      owner->remove_stream(streams[s],false);
  if (overlay != NULL)
    {
      overlay->deactivate();
      overlay = NULL;
    }
  repaint_entire_overlay = false;
  have_overlay_info = false;
  if (overlay_buffer != NULL)
    {
      owner->internal_delete_buffer(overlay_buffer);
      overlay_buffer = NULL;
    }
}

/******************************************************************************/
/*                          kdrc_layer::set_scale                             */
/******************************************************************************/

kdu_dims
  kdrc_layer::set_scale(bool transpose, bool vflip, bool hflip, float scale,
                        int &invalid_scale_code)
{
  invalid_scale_code = 0;
  if (streams[0] == NULL)
    return kdu_dims(); // Not successfully initialized yet
  if (mj2_pending_frame_change)
    change_frame();

  adjust_geometry_flags(transpose,vflip,hflip,
                        init_transpose,init_vflip,init_hflip);
  bool no_change = have_valid_scale &&
    (this->vflip == vflip) && (this->hflip == hflip) &&
    (this->transpose == transpose) && (this->scale == scale);
  have_valid_scale = false;
  this->vflip = vflip;
  this->hflip = hflip;
  this->transpose = transpose;
  this->scale = scale;

  for (int s=0; s < 2; s++)
    if (streams[s] != NULL)
      {
        kdu_dims stream_region =
          streams[s]->set_scale(full_source_dims,full_target_dims,
                                stream_sampling[s],stream_denominator[s],
                                transpose,vflip,hflip,scale,invalid_scale_code);
        if (s == 0)
          layer_region = stream_region;
        else
          layer_region &= stream_region;
        if (!layer_region)
          return kdu_dims(); // Scale too small or cannot flip, depending on
                             // `invalid_scale_code'
      }

  have_valid_scale = true;

  compositing_buffer = NULL;
  if (no_change)
    return layer_region; // No need to invalidate the layer buffer

  // Invalidate the layer buffer(s)
  if (buffer != NULL)
    {
      owner->internal_delete_buffer(buffer);
      buffer = NULL;
    }
  if (overlay_buffer != NULL)
    {
      owner->internal_delete_buffer(overlay_buffer);
      overlay_buffer = NULL;
      repaint_entire_overlay = false;
    }
  return layer_region;
}

/******************************************************************************/
/*                      kdrc_layer::find_optimal_scale                        */
/******************************************************************************/

float
  kdrc_layer::find_optimal_scale(float anchor_scale, float min_scale,
                                 float max_scale)
{
  if (streams[0] == NULL)
    return anchor_scale;
  else
    return streams[0]->find_optimal_scale(anchor_scale,min_scale,max_scale,
                                          full_source_dims,full_target_dims,
                                          stream_sampling[0],
                                          stream_denominator[0]);
}

/******************************************************************************/
/*                  kdrc_layer::get_component_scale_factors                   */
/******************************************************************************/

void
  kdrc_layer::get_component_scale_factors(kdrc_stream *stream,
                                          double &scale_x, double &scale_y)
{
  int s;
  for (s=0; s < 2; s++)
    if (stream == streams[s])
      {
        stream->get_component_scale_factors(full_source_dims,full_target_dims,
                                            stream_sampling[s],
                                            stream_denominator[s],
                                            scale_x,scale_y);
        break;
      }
  assert(s != 2);
}

/******************************************************************************/
/*                      kdrc_layer::set_buffer_surface                        */
/******************************************************************************/

void
  kdrc_layer::set_buffer_surface(kdu_dims whole_region, kdu_dims visible_region,
                                 kdu_compositor_buf *compositing_buffer,
                                 bool can_skip_surface_init)
{
  if (compositing_buffer != NULL)
    can_skip_surface_init = false;

  // Install the compositing buffer, regardless of whether we are initialized.
  // This allows us to erase the compositing buffer if we are the bottom
  // layer in a composition, whenever `update_composition' is called.
  this->compositing_buffer = compositing_buffer;
  this->compositing_buffer_region = whole_region;

  if (streams[0] == NULL)
    return; // Not yet initialized

  assert(have_valid_scale);

  bool read_access_required =
    (compositing_buffer != NULL) || ((overlay != NULL) && have_overlay_info);

  // First, set up the regular buffer
  kdu_compositor_buf *old_buffer = buffer;
  kdu_dims old_region = buffer_region;
  buffer_region = visible_region & layer_region;
  kdu_coords new_size = buffer_region.size;
  bool start_from_scratch = (old_buffer == NULL) ||
    ((old_region != buffer_region) && !old_buffer->is_read_access_allowed());
  if ((buffer == NULL) || (new_size.x > buffer_size.x) ||
      (new_size.y > buffer_size.y))
    buffer = owner->internal_allocate_buffer(new_size,buffer_size,
                                             read_access_required);
  else if (!buffer->set_read_accessibility(read_access_required))
    start_from_scratch = true;
  if (start_from_scratch)
    initialize_buffer_surface(buffer,buffer_region,NULL,kdu_dims(),
                              ((have_alpha_channel)?0x00FFFFFF:0xFFFFFFFF),
                              can_skip_surface_init);
  else if ((buffer != old_buffer) || (buffer_region != old_region))
    initialize_buffer_surface(buffer,buffer_region,old_buffer,old_region,
                              ((have_alpha_channel)?0x00FFFFFF:0xFFFFFFFF),
                              can_skip_surface_init);
  if ((old_buffer != NULL) && (old_buffer != buffer))
    owner->internal_delete_buffer(old_buffer);

  // Next, see if we need to create an overlay buffer
  if ((overlay != NULL) && (!have_overlay_info) &&
      overlay->set_buffer_surface(NULL,buffer_region,false))
    {
      have_overlay_info = true; // Activate overlays
      if (compositing_buffer == NULL)
        { // Must be the only layer; donate current buffer to owner for a
          // compositing buffer and allocate a new regular buffer
          assert(!read_access_required);
          assert((next == NULL) && (prev == NULL) &&
                 (whole_region == visible_region) && (overlay_buffer == NULL));
          owner->donate_compositing_buffer(buffer,buffer_region,buffer_size);
          this->compositing_buffer = compositing_buffer = buffer;
          buffer = owner->internal_allocate_buffer(new_size,buffer_size,true);
          if (!compositing_buffer->set_read_accessibility(true))
            { // Write-only buffer became read-write.  Initialize from scratch
              initialize_buffer_surface(compositing_buffer,buffer_region,
                                        NULL,kdu_dims(),0xFFFFFFFF);
              start_from_scratch = true;
            }
          initialize_buffer_surface(buffer,buffer_region,compositing_buffer,
                                    buffer_region,0xFFFFFFFF);
        }
    }

  // Set up the overlay buffer
  if ((overlay != NULL) && have_overlay_info)
    {
      old_buffer = overlay_buffer;
      if ((overlay_buffer == NULL) || (new_size.x > overlay_buffer_size.x) ||
          (new_size.y > overlay_buffer_size.y))
        overlay_buffer =
          owner->internal_allocate_buffer(new_size,overlay_buffer_size,true);
      initialize_buffer_surface(overlay_buffer,buffer_region,
                                old_buffer,old_region,0x00FFFFFF);
      if ((old_buffer != NULL) && (old_buffer != overlay_buffer))
        owner->internal_delete_buffer(old_buffer);
      overlay->set_buffer_surface(overlay_buffer,buffer_region,false);
      kdu_dims painted_region;
      while (overlay->process(painted_region)); // Paint it all now
    }
  
  // Set the stream buffer surfaces
  for (int s=0; s < 2; s++)
    if (streams[s] != NULL)
      streams[s]->set_buffer_surface(buffer,buffer_region,
                                     start_from_scratch);
}

/******************************************************************************/
/*                      kdrc_layer::take_layer_buffer                         */
/******************************************************************************/

kdu_compositor_buf *
  kdrc_layer::take_layer_buffer(bool can_skip_surface_init)
{
  kdu_compositor_buf *result = buffer;
  if (result == NULL)
    return NULL;
  assert(compositing_buffer == NULL);
  buffer =
    owner->internal_allocate_buffer(buffer_region.size,buffer_size,false);
  assert(!have_alpha_channel);
  if (!can_skip_surface_init)
    initialize_buffer_surface(buffer,buffer_region,NULL,kdu_dims(),0xFFFFFFFF);
  for (int s=0; s < 2; s++)
    if (streams[s] != NULL)
      streams[s]->set_buffer_surface(buffer,buffer_region,true);
  return result;
}

/******************************************************************************/
/*                     kdrc_layer::measure_visible_area                       */
/******************************************************************************/

kdu_long
  kdrc_layer::measure_visible_area(kdu_dims region,
                                   bool assume_visible_through_alpha)
{
  region &= layer_region;
  kdu_long result = region.area();
  kdrc_layer *scan;
  for (scan=this->next; (scan != NULL) && (result > 0); scan=scan->next)
    {
      if (scan->have_alpha_channel && assume_visible_through_alpha)
        continue;
      result -= scan->measure_visible_area(region,assume_visible_through_alpha);
    }
  if (result < 0)
    result = 0; // Should not be possible
  return result;
}

/******************************************************************************/
/*                   kdrc_layer::get_visible_packet_stats                     */
/******************************************************************************/

void
  kdrc_layer::get_visible_packet_stats(kdrc_stream *stream, kdu_dims region,
                                       kdu_long &precinct_samples,
                                       kdu_long &packet_samples,
                                       kdu_long &max_packet_samples)
{
  kdrc_layer *scan = next;
  while ((scan != NULL) && scan->have_alpha_channel)
    scan = scan->next;

  if (scan == NULL)
    stream->get_packet_stats(region,precinct_samples,packet_samples,
                             max_packet_samples);
  else
    { // Remove the portion of `region' which is covered by `scan'
      kdu_dims cover_dims = scan->layer_region;
      if (!cover_dims.intersects(region))
        scan->get_visible_packet_stats(stream,region,precinct_samples,
                                       packet_samples,max_packet_samples);
      else
        {
          kdu_coords min = region.pos;
          kdu_coords lim = min + region.size;
          kdu_coords cover_min = cover_dims.pos;
          kdu_coords cover_lim = cover_min + cover_dims.size;
          kdu_dims visible_region;
          if (cover_min.x > min.x)
            { // Entire left part of region is visible
              assert(cover_min.x < lim.x);
              visible_region.pos.x = min.x;
              visible_region.size.x = cover_min.x - min.x;
              visible_region.pos.y = min.y;
              visible_region.size.y = lim.y - min.y;
              scan->get_visible_packet_stats(stream,visible_region,
                                             precinct_samples,packet_samples,
                                             max_packet_samples);
            }
          else
            cover_min.x = min.x;
          if (cover_lim.x < lim.x)
            { // Entire right part of region is visible
              assert(cover_lim.x > min.x);
              visible_region.pos.x = cover_lim.x;
              visible_region.size.x = lim.x - cover_lim.x;
              visible_region.pos.y = min.y;
              visible_region.size.y = lim.y - min.y;
              scan->get_visible_packet_stats(stream,visible_region,
                                             precinct_samples,packet_samples,
                                             max_packet_samples);
            }
          else
            cover_lim.x = lim.x;
          if (cover_min.x < cover_lim.x)
            { // Central region fully is partially or fully covered
              visible_region.pos.x = cover_min.x;
              visible_region.size.x = cover_lim.x - cover_min.x;
              if (cover_min.y > min.y)
                { // Upper left part of region is visible
                  assert(cover_min.y < lim.y);
                  visible_region.pos.y = min.y;
                  visible_region.size.y = cover_min.y - min.y;
                  scan->get_visible_packet_stats(stream,visible_region,
                                               precinct_samples,packet_samples,
                                               max_packet_samples);
                }
              if (cover_lim.y < lim.y)
                { // Lower left part of region is visible
                  assert(cover_lim.y > min.y);
                  visible_region.pos.y = cover_lim.y;
                  visible_region.size.y = lim.y - cover_lim.y;
                  scan->get_visible_packet_stats(stream,visible_region,
                                               precinct_samples,packet_samples,
                                               max_packet_samples);
                }
            }
        }
    }
}

/******************************************************************************/
/*                      kdrc_layer::configure_overlay                         */
/******************************************************************************/

void
  kdrc_layer::configure_overlay(bool enable, int min_display_size,
                                int painting_param)
{
  if ((streams[0] == NULL) || !jpx_layer)
    return;

  if (!enable)
    { // Turn off overlays
      if (have_overlay_info)
        { // Need next call to `process_overlay' to completely repaint screen
          // For this reason, we do not set `have_overlay_info' to false yet
          repaint_entire_overlay = true; // So next call to `process'
        }
      if (overlay != NULL)
        {
          overlay->deactivate();
          overlay = NULL;
        }
      if (overlay_buffer != NULL)
        {
          owner->internal_delete_buffer(overlay_buffer);
          overlay_buffer = NULL;
        }
      return;
    }

  assert(enable);
  if (overlay == NULL)
    { // Enabling for the first time
      overlay = streams[0]->get_overlay();
      if (overlay != NULL)
        overlay->activate(owner);
      update_overlay(false,true);
    }
  if (overlay->update_config(min_display_size,painting_param))
    {
      if (overlay_buffer != NULL)
        initialize_buffer_surface(overlay_buffer,buffer_region,
                                  NULL,kdu_dims(),0x00FFFFFF);
      repaint_entire_overlay = true;
    }
}

/******************************************************************************/
/*                        kdrc_layer::update_overlay                          */
/******************************************************************************/

void
  kdrc_layer::update_overlay(bool start_from_scratch, bool delay_painting)
{
  if ((streams[0] == NULL) || (overlay == NULL) || !have_valid_scale)
    return;

  if (start_from_scratch)
    {
      overlay->deactivate();
      overlay->activate(owner);
      if (overlay_buffer != NULL)
        initialize_buffer_surface(overlay_buffer,buffer_region,
                                  NULL,kdu_dims(),0x00FFFFFF);
    }

  if (overlay->set_buffer_surface(overlay_buffer,buffer_region,true))
    {
      if (!have_overlay_info)
        { // Need to create an overlay buffer
          have_overlay_info = true; // Activate overlays
          if (compositing_buffer == NULL)
            { // Must be the only layer; donate current buffer to owner for a
              // compositing buffer and allocate a new regular buffer
              assert((next == NULL) && (prev == NULL) &&
                     (overlay_buffer == NULL));
              owner->donate_compositing_buffer(buffer,buffer_region,
                                               buffer_size);
              compositing_buffer = buffer;
              buffer = owner->internal_allocate_buffer(buffer_region.size,
                                                       buffer_size,true);
              bool start_stream_from_scratch = false;
              if (!compositing_buffer->set_read_accessibility(true))
                { // Write-only buffer became read-write.
                  initialize_buffer_surface(compositing_buffer,buffer_region,
                                            NULL,kdu_dims(),0xFFFFFFFF);
                  start_stream_from_scratch = true;
                }
              initialize_buffer_surface(buffer,buffer_region,
                                        compositing_buffer,buffer_region,
                                        0xFFFFFFFF);
              for (int s=0; s < 2; s++)
                if (streams[s] != NULL)
                  streams[s]->set_buffer_surface(buffer,buffer_region,
                                                 start_stream_from_scratch);
            }

          if (overlay_buffer == NULL)
            {
              overlay_buffer =
                owner->internal_allocate_buffer(buffer_region.size,
                                                overlay_buffer_size,true);
              initialize_buffer_surface(overlay_buffer,buffer_region,
                                        NULL,kdu_dims(),0x00FFFFFF);
              overlay->set_buffer_surface(overlay_buffer,buffer_region,false);
            }
        }
      if (!delay_painting)
        {
          kdu_dims paint_region;
          while (overlay->process(paint_region));
        }
    }
  if (have_overlay_info && start_from_scratch && delay_painting)
    repaint_entire_overlay = true;
}

/******************************************************************************/
/*                        kdrc_layer::process_overlay                         */
/******************************************************************************/

bool
  kdrc_layer::process_overlay(kdu_dims &new_region)
{
  if (overlay == NULL)
    {
      bool just_disabled = have_overlay_info && repaint_entire_overlay;
        // Special case means that we just disabled overlays.
      have_overlay_info = repaint_entire_overlay = false;
      if (just_disabled)
        { // Repaint everything
          kdu_compositor_buf *old_buffer = buffer;
          new_region = buffer_region;
          if ((compositing_buffer != NULL) &&
              owner->retract_compositing_buffer(buffer_size))
            { // We no longer need a separate compositing buffer; swap current
              // compositing buffer into current buffer position so application
              // sees no change
              buffer = compositing_buffer;
              compositing_buffer = NULL;
              assert(!have_alpha_channel);
              initialize_buffer_surface(buffer,buffer_region,old_buffer,
                                        buffer_region,0xFFFFFFFF);
              buffer->set_read_accessibility(false);
              for (int s=0; s < 2; s++)
                if (streams[s] != NULL)
                  streams[s]->set_buffer_surface(buffer,buffer_region,false);
              owner->internal_delete_buffer(old_buffer);
            }
          return true;
        }
    }
  if (!have_overlay_info)
    return false;
  if (repaint_entire_overlay)
    {
      while (overlay->process(new_region)); // Paint it all, then signal whole
      new_region = buffer_region;           // overlay surface needs compositing
      repaint_entire_overlay = false;
      return true;
    }
  return overlay->process(new_region);
}

/******************************************************************************/
/*                          kdrc_layer::map_region                            */
/******************************************************************************/

bool
  kdrc_layer::map_region(int &codestream_idx, kdu_dims &region)
{
  if ((!have_valid_scale) || (streams[0] == NULL))
    return false;
  kdu_dims result = streams[0]->map_region(region);
  if (result.is_empty())
    return false;
  region = result;
  codestream_idx = streams[0]->codestream_idx;
  return true;
}

/******************************************************************************/
/*                         kdrc_layer::search_overlay                         */
/******************************************************************************/

jpx_metanode
  kdrc_layer::search_overlay(kdu_coords point, int &codestream_idx,
                             bool &is_opaque)
{
  is_opaque = false;
  if (overlay == NULL)
    return jpx_metanode(NULL);
  kdu_coords off = point - buffer_region.pos;
  if ((off.x < 0) || (off.x >= buffer_region.size.x) ||
      (off.y < 0) || (off.y >= buffer_region.size.y))
    return jpx_metanode(NULL);
  is_opaque = !have_alpha_channel;
  return overlay->search(point,codestream_idx);
}

/******************************************************************************/
/*                       kdrc_layer::update_composition                       */
/******************************************************************************/

void
  kdrc_layer::update_composition(kdu_dims region,
                                 kdu_uint32 compositing_background)
{
  kdu_uint32 *spp, *dpp;

  if (compositing_buffer == NULL)
    return; // Nothing to do
  if (this == owner->active_layers)
    { // First layer; erase the region first before painting
      int compositing_row_gap=0;
      dpp = compositing_buffer->get_buf(compositing_row_gap,false);
      dpp += (region.pos.x-compositing_buffer_region.pos.x)
        + compositing_row_gap*(region.pos.y-compositing_buffer_region.pos.y);
#   ifdef KDU_SIMD_OPTIMIZATIONS
      if (!simd_erase_region(dpp,region.size.y,region.size.x,
                             compositing_row_gap,compositing_background))
#   endif // KDU_SIMD_OPTIMIZATIONS
        {
          int m, n;
          kdu_uint32 *dp;
          for (m=region.size.y; m > 0; m--, dpp+=compositing_row_gap)
            for (dp=dpp, n=region.size.x; n > 0; n--)
              *(dp++) = compositing_background;
        }
    }

  region &= buffer_region;
  region &= compositing_buffer_region;
  if ((streams[0] == NULL) || !region)
    return;

  int row_gap, compositing_row_gap;
  assert(have_valid_scale && (buffer != NULL));
  spp = buffer->get_buf(row_gap,true);
  spp += (region.pos.x-buffer_region.pos.x)
    + row_gap*(region.pos.y-buffer_region.pos.y);

  dpp = compositing_buffer->get_buf(compositing_row_gap,true);
  dpp += (region.pos.x-compositing_buffer_region.pos.x)
    + compositing_row_gap*(region.pos.y-compositing_buffer_region.pos.y);
  if (!have_alpha_channel)
    { // Just copy the region
#   ifdef KDU_SIMD_OPTIMIZATIONS
      if (!simd_copy_region(dpp,spp,region.size.y,region.size.x,
                            compositing_row_gap,row_gap))
#   endif // KDU_SIMD_OPTIMIZATIONS
        {
          int m, n;
          kdu_uint32 *sp, *dp;
          for (m=region.size.y; m > 0; m--,
               spp+=row_gap, dpp+=compositing_row_gap)
            for (sp=spp, dp=dpp, n=region.size.x; n > 0; n--)
              *(dp++) = *(sp++);
        }
    }
  else if (!alpha_is_premultiplied)
    { // Alpha blend the region
#   ifdef KDU_SIMD_OPTIMIZATIONS
      if (!simd_blend_region(dpp,spp,region.size.y,region.size.x,
                             compositing_row_gap,row_gap))
#   endif // KDU_SIMD_OPTIMIZATIONS
        {
          int m, n;
          kdu_uint32 *sp, *dp;
          kdu_uint32 src, tgt;
          kdu_int32 aval, red, green, blue, diff, alpha;
          for (m=region.size.y; m > 0; m--,
               spp+=row_gap, dpp+=compositing_row_gap)
            for (sp=spp, dp=dpp, n=region.size.x; n > 0; n--)
              {
                src = *(sp++); tgt = *dp;
                alpha = kdrc_alpha_lut[src>>24];
                aval =  (tgt>>24) & 0x0FF;
                red   = (tgt>>16) & 0x0FF;
                green = (tgt>>8) & 0x0FF;
                blue  = tgt & 0x0FF;
                diff = 255                 - aval;   aval  += (diff*alpha)>>14;
                diff = ((src>>16) & 0x0FF) - red;    red   += (diff*alpha)>>14;
                diff = ((src>> 8) & 0x0FF) - green;  green += (diff*alpha)>>14;
                diff = (src & 0x0FF)       - blue;   blue  += (diff*alpha)>>14;
                if (aval & 0xFFFFFF00)
                  aval = (aval<0)?0:255;
                if (red & 0xFFFFFF00)
                  red = (red<0)?0:255;
                if (green & 0xFFFFFF00)
                  green = (green<0)?0:255;
                if (blue & 0xFFFFFF00)
                  blue = (blue<0)?0:255;
                *(dp++) = (kdu_uint32)((aval<<24)+(red<<16)+(green<<8)+blue);
              }
        }
    }
  else
    { // Alpha blend the region, using premultiplied alpha
#   ifdef KDU_SIMD_OPTIMIZATIONS
      if (!simd_blend_premult_region(dpp,spp,region.size.y,region.size.x,
                                     compositing_row_gap,row_gap))
#   endif // KDU_SIMD_OPTIMIZATIONS
        {
          int m, n;
          kdu_uint32 *sp, *dp;
          kdu_uint32 src, tgt;
          kdu_int32 aval, red, green, blue, tmp, alpha;
          for (m=region.size.y; m > 0; m--,
               spp+=row_gap, dpp+=compositing_row_gap)
            for (sp=spp, dp=dpp, n=region.size.x; n > 0; n--)
              {
                src = *(sp++); tgt = *dp;
                alpha = kdrc_alpha_lut[src>>24];
                aval  = (src>>24) & 0x0FF;
                red   = (src>>16) & 0x0FF;
                green = (src>>8) & 0x0FF;
                blue  = src & 0x0FF;
                tmp = ((tgt>>24) & 0x0FF);  aval  += tmp-((tmp*alpha)>>14);
                tmp = ((tgt>>16) & 0x0FF);  red   += tmp-((tmp*alpha)>>14);
                tmp = ((tgt>> 8) & 0x0FF);  green += tmp-((tmp*alpha)>>14);
                tmp = (tgt & 0x0FF)      ;  blue  += tmp-((tmp*alpha)>>14);
                if (aval & 0xFFFFFF00)
                  aval = (aval<0)?0:255;
                if (red & 0xFFFFFF00)
                  red = (red<0)?0:255;
                if (green & 0xFFFFFF00)
                  green = (green<0)?0:255;
                if (blue & 0xFFFFFF00)
                  blue = (blue<0)?0:255;
                *(dp++) = (kdu_uint32)((aval<<24)+(red<<16)+(green<<8)+blue);
              }
        }
    }

  if (overlay_buffer == NULL)
    return;

  // Alpha blend the overlay information
  int overlay_row_gap;
  spp = overlay_buffer->get_buf(overlay_row_gap,true);
  spp += (region.pos.x-buffer_region.pos.x)
    + overlay_row_gap*(region.pos.y-buffer_region.pos.y);
  dpp = compositing_buffer->get_buf(compositing_row_gap,true);
  dpp += (region.pos.x-compositing_buffer_region.pos.x)
    + compositing_row_gap*(region.pos.y-compositing_buffer_region.pos.y);
#   ifdef KDU_SIMD_OPTIMIZATIONS
  if (!simd_blend_region(dpp,spp,region.size.y,region.size.x,
                         compositing_row_gap,overlay_row_gap))
#   endif // KDU_SIMD_OPTIMIZATIONS
    {
      int m, n;
      kdu_uint32 *sp, *dp;
      kdu_uint32 src, tgt;
      kdu_int32 aval, red, green, blue, diff, alpha;
      for (m=region.size.y; m > 0; m--,
           spp+=overlay_row_gap, dpp+=compositing_row_gap)
        for (sp=spp, dp=dpp, n=region.size.x; n > 0; n--)
          {
            src = *(sp++); tgt = *dp;
            alpha = kdrc_alpha_lut[src>>24];
            aval =  (tgt>>24) & 0x0FF;
            red   = (tgt>>16) & 0x0FF;
            green = (tgt>>8) & 0x0FF;
            blue  = tgt & 0x0FF;
            diff = 255                 - aval;   aval  += (diff * alpha) >> 14;
            diff = ((src>>16) & 0x0FF) - red;    red   += (diff * alpha) >> 14;
            diff = ((src>> 8) & 0x0FF) - green;  green += (diff * alpha) >> 14;
            diff = (src & 0x0FF)       - blue;   blue  += (diff * alpha) >> 14;
            if (aval & 0xFFFFFF00)
              aval = (aval<0)?0:255;
            if (red & 0xFFFFFF00)
              red = (red<0)?0:255;
            if (green & 0xFFFFFF00)
              green = (green<0)?0:255;
            if (blue & 0xFFFFFF00)
              blue = (blue<0)?0:255;
            *(dp++) = (kdu_uint32)((aval<<24) + (red<<16) + (green<<8) + blue);
          }
    }
}


/* ========================================================================== */
/*                              kdrc_refresh                                  */
/* ========================================================================== */

/******************************************************************************/
/*                         kdrc_refresh::add_region                           */
/******************************************************************************/

void
  kdrc_refresh::add_region(kdu_dims region)
{
  kdrc_refresh_elt *scan, *prev, *next;
  for (prev=NULL, scan=list; scan != NULL; prev=scan, scan=next)
    {
      next = scan->next;
      kdu_dims intersection = scan->region & region;
      if (!intersection)
        continue;
      if (intersection == region)
        return; // region is entirely contained in existing element
      if (intersection == scan->region)
        { // Existing element is entirely contained in new region
          if (prev == NULL)
            list = next;
          else
            prev->next = next;
          scan->next = free_elts;
          free_elts = scan;
          scan = prev; // So `prev' does not change
          continue;
        }

      // See if we should shrink either the new region or the existing region
      if ((intersection.pos.x == region.pos.x) && 
          (intersection.size.x == region.size.x))
        { // See if we can reduce the height of the new region
          int int_min = intersection.pos.y;
          int int_lim = int_min + intersection.size.y;
          int reg_min = region.pos.y;
          int reg_lim = reg_min + region.size.y;
          if (int_min == reg_min)
            {
              region.pos.y = int_lim;
              region.size.y = reg_lim - int_lim;
            }
          else if (int_lim == reg_lim)
            region.size.y = int_min - reg_min;
        }
      else if ((intersection.pos.y == region.pos.y) &&
               (intersection.size.y == region.size.y))
        { // See if we can reduce the width of the new region
          int int_min = intersection.pos.x;
          int int_lim = int_min + intersection.size.x;
          int reg_min = region.pos.x;
          int reg_lim = reg_min + region.size.x;
          if (int_min == reg_min)
            {
              region.pos.x = int_lim;
              region.size.x = reg_lim - int_lim;
            }
          else if (int_lim == reg_lim)
            region.size.x = int_min - reg_min;
        }
      else if ((intersection.pos.x == scan->region.pos.x) &&
               (intersection.size.x == scan->region.size.x))
        { // See if we can reduce the height of the existing region
          int int_min = intersection.pos.y;
          int int_lim = int_min + intersection.size.y;
          int reg_min = scan->region.pos.y;
          int reg_lim = reg_min + scan->region.size.y;
          if (int_min == reg_min)
            {
              scan->region.pos.y = int_lim;
              scan->region.size.y = reg_lim - int_lim;
            }
          else if (int_lim == reg_lim)
            scan->region.size.y = int_min - reg_min;
        }
      else if ((intersection.pos.y == scan->region.pos.y) &&
               (intersection.size.y == scan->region.size.y))
        { // See if we can reduce the width of the existing region
          int int_min = intersection.pos.x;
          int int_lim = int_min + intersection.size.x;
          int reg_min = scan->region.pos.x;
          int reg_lim = reg_min + scan->region.size.x;
          if (int_min == reg_min)
            {
              scan->region.pos.x = int_lim;
              scan->region.size.x = reg_lim - int_lim;
            }
          else if (int_lim == reg_lim)
            scan->region.size.x = int_min - reg_min;
        }
    }

  kdrc_refresh_elt *elt = free_elts;
  if (elt == NULL)
    elt = new kdrc_refresh_elt;
  else
    free_elts = elt->next;
  elt->next = list;
  list = elt;
  elt->region = region;

}

/******************************************************************************/
/*                           kdrc_refresh::reset                              */
/******************************************************************************/

void
  kdrc_refresh::reset()
{
  kdrc_refresh_elt *tmp;
  while ((tmp=list) != NULL)
    {
      list = tmp->next;
      tmp->next = free_elts;
      free_elts = tmp;
    }
}

/******************************************************************************/
/*                      kdrc_refresh::adjust (region)                         */
/******************************************************************************/

void
  kdrc_refresh::adjust(kdu_dims buffer_region)
{
  kdrc_refresh_elt *scan, *prev, *next;
  for (prev=NULL, scan=list; scan != NULL; prev=scan, scan=next)
    {
      next = scan->next;
      scan->region &= buffer_region;
      if (!scan->region)
        {
          if (prev == NULL)
            list = next;
          else
            prev->next = next;
          scan->next = free_elts;
          free_elts = scan;
          scan = prev; // So `prev' does not change
        }
    }
}

/******************************************************************************/
/*                      kdrc_refresh::adjust (stream)                         */
/******************************************************************************/

void
  kdrc_refresh::adjust(kdrc_stream *stream)
{
  kdrc_refresh_elt *scan, *old_list = list;
  list = NULL;
  while ((scan=old_list) != NULL)
    {
      old_list = scan->next;
      stream->adjust_refresh(scan->region,this);
      scan->next = free_elts;
      free_elts = scan;
    }
}


/* ========================================================================== */
/*                          kdu_region_compositor                             */
/* ========================================================================== */

/******************************************************************************/
/*            kdu_region_compositor::kdu_region_compositor (blank)            */
/******************************************************************************/

kdu_region_compositor::kdu_region_compositor(kdu_thread_env *env,
                                             kdu_thread_queue *env_queue)
{
  init(env,env_queue);
}

/******************************************************************************/
/*            kdu_region_compositor::kdu_region_compositor (raw)              */
/******************************************************************************/

kdu_region_compositor::kdu_region_compositor(kdu_compressed_source *source,
                                             int persistent_cache_threshold)
{
  init(NULL,NULL);
  create(source,persistent_cache_threshold);
}

/******************************************************************************/
/*            kdu_region_compositor::kdu_region_compositor (JPX)              */
/******************************************************************************/

kdu_region_compositor::kdu_region_compositor(jpx_source *source,
                                             int persistent_cache_threshold)
{
  init(NULL,NULL);
  create(source,persistent_cache_threshold);
}

/******************************************************************************/
/*            kdu_region_compositor::kdu_region_compositor (MJ2)              */
/******************************************************************************/

kdu_region_compositor::kdu_region_compositor(mj2_source *source,
                                             int persistent_cache_threshold)
{
  init(NULL,NULL);
  create(source,persistent_cache_threshold);
}

/******************************************************************************/
/*                       kdu_region_compositor::init                          */
/******************************************************************************/

void
  kdu_region_compositor::init(kdu_thread_env *env, kdu_thread_queue *env_queue)
{
  this->raw_src = NULL;
  this->jpx_src = NULL;
  this->mj2_src = NULL;
  this->persistent_codestreams = true;
  this->codestream_cache_threshold = 256000; 
  this->error_level = 0;
  composition_buffer = NULL;
  buffer_region = kdu_dims();
  buffer_size = kdu_coords();
  buffer_background = 0xFFFFFFFF;
  processing_complete = true;
  have_valid_scale = false;
  max_quality_layers = 1<<16;
  vflip = hflip = transpose = false;
  composition_invalid = true;
  scale = 1.0F;
  invalid_scale_code = 0;
  queue_head = queue_tail = queue_free = NULL;
  active_layers = last_active_layer = inactive_layers = NULL;
  active_streams = inactive_streams = NULL;
  enable_overlays = false;
  can_skip_surface_initialization = false;
  initialize_surfaces_on_next_refresh = false;
  overlay_painting_param = 0;
  overlay_min_display_size = 1;
  refresh_mgr = new kdrc_refresh();
  if ((env != NULL) && !env->exists())
    env = NULL;
  this->env = env;
  this->env_queue = (env==NULL)?NULL:env_queue;
}

/******************************************************************************/
/*                      kdu_region_compositor::create (raw)                   */
/******************************************************************************/

void
  kdu_region_compositor::create(kdu_compressed_source *source,
                                int persistent_cache_threshold)
{
  if ((raw_src != NULL) || (jpx_src != NULL) || (mj2_src != NULL))
    { KDU_ERROR_DEV(e,0x04040501); e <<
      KDU_TXT("Attempting to invoke `kdu_region_compositor::create' on an "
              "object which has already been created.");
    }

  this->raw_src = source;
  this->persistent_codestreams = (persistent_cache_threshold >= 0);
  this->codestream_cache_threshold = persistent_cache_threshold;
}

/******************************************************************************/
/*                    kdu_region_compositor::create (JPX/JP2)                 */
/******************************************************************************/

void
  kdu_region_compositor::create(jpx_source *source,
                                int persistent_cache_threshold)
{
  if ((raw_src != NULL) || (jpx_src != NULL) || (mj2_src != NULL))
    { KDU_ERROR_DEV(e,0x04040502); e <<
      KDU_TXT("Attempting to invoke `kdu_region_compositor::create' on an "
              "object which has already been created.");
    }

  this->jpx_src = source;
  this->persistent_codestreams = (persistent_cache_threshold >= 0);
  this->codestream_cache_threshold = persistent_cache_threshold;
}

/******************************************************************************/
/*                     kdu_region_compositor::create (MJ2)                    */
/******************************************************************************/

void
  kdu_region_compositor::create(mj2_source *source,
                                int persistent_cache_threshold)
{
  if ((raw_src != NULL) || (jpx_src != NULL) || (mj2_src != NULL))
    { KDU_ERROR_DEV(e,0x04040503); e <<
      KDU_TXT("Attempting to invoke `kdu_region_compositor::create' on an "
              "object which has already been created.");
    }

  this->mj2_src = source;
  this->persistent_codestreams = (persistent_cache_threshold >= 0);
  this->codestream_cache_threshold = persistent_cache_threshold;
}

/******************************************************************************/
/*                    kdu_region_compositor::pre_destroy                      */
/******************************************************************************/

void
  kdu_region_compositor::pre_destroy()
{
  remove_compositing_layer(-1,true);
  while (active_streams != NULL)
    remove_stream(active_streams,true);
  while (inactive_streams != NULL)
    remove_stream(inactive_streams,true);
  if (composition_buffer != NULL)
    { internal_delete_buffer(composition_buffer); composition_buffer = NULL; }
  if (refresh_mgr != NULL)
    { delete refresh_mgr; refresh_mgr = NULL; }
  flush_composition_queue();
  while ((queue_tail=queue_free) != NULL)
    {
      queue_free = queue_tail->next;
      delete queue_tail;
    }
}

/******************************************************************************/
/*                  kdu_region_compositor::set_error_level                    */
/******************************************************************************/

void
  kdu_region_compositor::set_error_level(int error_level)
{
  this->error_level = error_level;
  kdrc_stream *scan;
  for (scan=active_streams; scan != NULL; scan=scan->next)
    scan->set_error_level(error_level);
}

/******************************************************************************/
/*               kdu_region_compositor::add_compositing_layer                 */
/******************************************************************************/

bool
  kdu_region_compositor::add_compositing_layer(int layer_idx,
                                               kdu_dims full_source_dims,
                                               kdu_dims full_target_dims,
                                               bool transpose, bool vflip,
                                               bool hflip, int frame_idx,
                                               int field_handling)
{
  full_target_dims.from_apparent(transpose,vflip,hflip);
          // Convert to geometry prior to any flipping/transposition

  fixed_composition_dims = default_composition_dims = kdu_dims();
  if (raw_src != NULL)
    {
      if (layer_idx != 0)
        { KDU_ERROR_DEV(e,9); e <<
            KDU_TXT("Invalid compositing layer index supplied to "
            "`kdu_region_compositor::add_compositing_layer'.");
        }
      if (active_streams != NULL)
        remove_stream(active_streams,false);
      assert(active_streams == NULL);
      try {
          add_active_stream(0,0,false);
          active_streams->set_mode(-1,KDU_WANT_OUTPUT_COMPONENTS);
        }
      catch (...) {
          if (active_streams != NULL)
            remove_stream(active_streams,true);
          throw;
        }
      composition_invalid = true;
      return true;
    }

  // If we get here, we have a JP2/JPX/MJ2 source.  First check that the
  // requested object can be opened.
  if (jpx_src != NULL)
    { // Try to open the requested JPX compositing layer
      int available_layers;
      bool all_found = jpx_src->count_compositing_layers(available_layers);
      jpx_layer_source layer = jpx_src->access_layer(layer_idx);
      if (!layer)
        {
          if ((layer_idx >= 0) &&
              ((layer_idx < available_layers) || !all_found))
            return false; // Check back again later once more data is in cache
          KDU_ERROR_DEV(e,10); e <<
            KDU_TXT("Attempting to add a non-existent compositing "
            "layer via `kdu_region_compositor::add_compositing_layer'.");
        }
    }
  else
    { // Try to open the requested MJ2 track
      assert(mj2_src != NULL);
      int track_type = mj2_src->get_track_type((kdu_uint32)(layer_idx+1));
      if (track_type == MJ2_TRACK_MAY_EXIST)
        return false; // Check back again later once more data is in the cache
      if (track_type != MJ2_TRACK_IS_VIDEO)
        { KDU_ERROR_DEV(e,11); e <<
            KDU_TXT("Attempting to add a non-existent or non-video "
            "Motion JPEG2000 track via "
            "`kdu_region_compositor::add_compositing_layer'.");
        }
    }
  
  // Next, remove any active streams which are not attached to layers.
  while ((active_streams != NULL) && (active_layers == NULL))
    remove_stream(active_streams,false); // May remove permanently anyway if
        // it was created just to serve the needs of single component rendering
  single_component_box.close(); // In case one was used

  // Look for the layer on the active list (we need to move it to the end)
  kdrc_layer *scan, *prev;
  for (prev=NULL, scan=active_layers; scan != NULL; prev=scan, scan=scan->next)
    if (scan->layer_idx == layer_idx)
      { // Found it; just remove it from the list for now
        if (prev == NULL)
          {
            assert(scan == active_layers);
            active_layers = scan->next;
          }
        else
          prev->next = scan->next;
        if (scan->next == NULL)
          {
            assert(scan == last_active_layer);
            last_active_layer = prev;
          }
        else
          {
            assert(scan != last_active_layer);
            scan->next->prev = scan->prev;
          }
        scan->next = scan->prev = NULL;
        break;
      }

  if (scan == NULL)
    { // Look for the layer on the inactive list
      for (prev=NULL, scan=inactive_layers; scan != NULL;
           prev=scan, scan=scan->next)
        if (scan->layer_idx == layer_idx)
          { // Found it; remove from list
            if (prev == NULL)
              inactive_layers = scan->next;
            else
              prev->next = scan->next;
            scan->next = scan->prev = NULL;
            break;
          }
    }

  bool need_layer_init=false;
  if (scan == NULL)
    { // Create the layer from scratch
      scan = new kdrc_layer(this);
      need_layer_init = true;
    }

  // Append to the end of the active layers list and complete the configuration
  if (last_active_layer == NULL)
    {
      assert(active_layers == NULL);
      active_layers = last_active_layer = scan;
    }
  else
    {
      scan->prev = last_active_layer;
      last_active_layer->next = scan;
      last_active_layer = scan;
    }

  composition_invalid = true;
  try {
      if (need_layer_init)
        {
          if (jpx_src != NULL)
            {
              jpx_layer_source layer = jpx_src->access_layer(layer_idx);
              scan->init(layer,full_source_dims,full_target_dims,
                         transpose,vflip,hflip);
            }
          else
            {
              mj2_video_source *track =
                mj2_src->access_video_track((kdu_uint32)(layer_idx+1));
              scan->init(track,frame_idx,field_handling,full_target_dims,
                         transpose,vflip,hflip);
            }
        }
      else
        {
          scan->activate(full_source_dims,full_target_dims,
                         transpose,vflip,hflip,frame_idx,field_handling);
        }
    }
  catch (...) {
      remove_compositing_layer(layer_idx,true);
      throw;
    }
  return true;
}

/******************************************************************************/
/*           kdu_region_compositor::change_compositing_layer_frame            */
/******************************************************************************/

bool
  kdu_region_compositor::change_compositing_layer_frame(int layer_idx,
                                                        int new_frame_idx)
{
  if (mj2_src == NULL)
    return (new_frame_idx == 0);
  kdrc_layer *scan;
  for (scan=active_layers; scan != NULL; scan=scan->next)
    if (scan->layer_idx == layer_idx)
      break;
  if (scan == NULL)
    { KDU_ERROR_DEV(e,12); e <<
        KDU_TXT("The `layer_idx' value supplied to "
        "`kdu_region_compositor::change_compositing_layer_frame', does not "
        "correspond to any currently active layer.");
    }
  return scan->change_frame(new_frame_idx,false);
}

/******************************************************************************/
/*                     kdu_region_compositor::set_frame                       */
/******************************************************************************/

void
  kdu_region_compositor::set_frame(jpx_frame_expander *expander)
{
  jpx_composition composition;
  if ((jpx_src == NULL) || !(composition = jpx_src->access_composition()))
    { KDU_ERROR_DEV(e,13); e <<
        KDU_TXT("Invoking `kdu_region_compositor::set_frame' on an "
        "object whose data source does not offer any composition or "
        "animation instructions!");
    }

  fixed_composition_dims = kdu_dims(); // Empty until proven otherwise
  default_composition_dims.pos = kdu_coords(0,0);
  composition.get_global_info(default_composition_dims.size);
  
  // Remove all current layers non-permanently
  remove_compositing_layer(-1,false);
  while (active_streams != NULL)
    remove_stream(active_streams,false);
  single_component_box.close();

  int m, num_frame_members = expander->get_num_members();
  for (m=0; m < num_frame_members; m++)
    {
      int layer_idx, instruction_idx;
      kdu_dims source_dims, target_dims;
      bool covers_composition;
      expander->get_member(m,instruction_idx,layer_idx,covers_composition,
                           source_dims,target_dims);
      jpx_layer_source layer = jpx_src->access_layer(layer_idx,false);
      if (!layer)
        { KDU_ERROR_DEV(e,14); e <<
            KDU_TXT("Attempting to invoke `set_frame' with a "
            "`jpx_frame_expander' object whose `construct' function did not "
            "return true when you invoked it.");
        }

      if (target_dims != default_composition_dims)
        fixed_composition_dims = default_composition_dims;

      // Now add the layer to the tail of the compositing list
      kdrc_layer *scan, *prev;
          
      // Look on the inactive list first
      for (prev=NULL, scan=inactive_layers; scan != NULL;
           prev=scan, scan=scan->next)
        if (scan->layer_idx == layer_idx)
          { // Found it; just remove it from the list for now
            if (prev == NULL)
              inactive_layers = scan->next;
            else
              prev->next = scan->next;
            scan->next = scan->prev = NULL;
            break;
          }
          
      if (scan != NULL)
        {
          scan->next = active_layers;
          if (active_layers == NULL)
            {
              assert(last_active_layer == NULL);
              last_active_layer = active_layers = scan;
            }
          else
            {
              assert(last_active_layer != NULL);
              active_layers->prev = scan;
              active_layers = scan;
            }
          scan->activate(source_dims,target_dims,false,false,false,0,0);
        }
      else
        { // Create layer from scratch
          scan = new kdrc_layer(this);
          scan->next = active_layers;
          if (active_layers == NULL)
            {
              assert(last_active_layer == NULL);
              last_active_layer = active_layers = scan;
            }
          else
            {
              assert(last_active_layer != NULL);
              active_layers->prev = scan;
              active_layers = scan;
            }
          try {
              scan->init(layer,source_dims,target_dims,false,false,false);
            }
          catch (...) {
              remove_compositing_layer(layer_idx,true);
              throw;
            }
        }
      composition_invalid = true;
    }
}

/******************************************************************************/
/*              kdu_region_compositor::remove_compositing_layer               */
/******************************************************************************/

void
  kdu_region_compositor::remove_compositing_layer(int layer_idx,
                                                  bool permanent)
{
  // Move all matching layers to the inactive list first
  kdrc_layer *prev, *scan, *next;
  for (prev=NULL, scan=active_layers; scan != NULL; prev=scan, scan=next)
    {
      next = scan->next;
      if ((scan->layer_idx == layer_idx) || (layer_idx < 0))
        { // Move to inactive list
          composition_invalid = true;
          scan->deactivate();
          if (prev == NULL)
            {
              assert(scan == active_layers);
              active_layers = next;
            }
          else
            prev->next = next;
          if (next == NULL)
            {
              assert(scan == last_active_layer);
              last_active_layer = prev;
            }
          else
            {
              assert(scan != last_active_layer);
              next->prev = prev;
            }

          scan->prev = NULL;
          scan->next = inactive_layers;
          inactive_layers = scan;
          scan = prev; // so that `prev' does not change
        }
    }
  if (!permanent)
    return;

  // Delete all matching layers from the inactive list
  for (prev=NULL, scan=inactive_layers; scan != NULL; prev=scan, scan=next)
    {
      next = scan->next;
      if ((scan->layer_idx == layer_idx) || (layer_idx < 0))
        { // Move to inactive list
          if (prev == NULL)
            inactive_layers = next;
          else
            prev->next = next;
          delete scan;
          scan = prev; // so that `prev' does not change
        }
    }
}

/******************************************************************************/
/*               kdu_region_compositor::cull_inactive_layers                  */
/******************************************************************************/

void
  kdu_region_compositor::cull_inactive_layers(int max_inactive)
{
  kdrc_layer *scan, *prev;
  for (prev=NULL, scan=inactive_layers;
       (scan != NULL) && (max_inactive > 0);
       prev=scan, scan=scan->next, max_inactive--);
  while (scan != NULL)
    {
      if (prev == NULL)
        inactive_layers = scan->next;
      else
        prev->next = scan->next;
      delete scan;
      scan = (prev==NULL)?inactive_layers:prev->next;
    }
}

/******************************************************************************/
/*               kdu_region_compositor::set_single_component                  */
/******************************************************************************/

int
  kdu_region_compositor::set_single_component(
                                    int stream_idx, int component_idx,
                                    kdu_component_access_mode access_mode)
{
  fixed_composition_dims = default_composition_dims = kdu_dims();
  composition_invalid = true;
  while (active_layers != NULL)
    remove_compositing_layer(active_layers->layer_idx,false);
  assert(last_active_layer == NULL);
  while (active_streams != NULL)
    remove_stream(active_streams,false); // May remove permanently anyway if
        // it was created just to serve the needs of single component rendering

  // Check for valid codestream index
  if (raw_src != NULL)
    {
      if (stream_idx != 0)
        { KDU_ERROR_DEV(e,15); e <<
            KDU_TXT("Invalid codestream index passed to "
            "`kdu_region_compositor::set_single_component'.");
        }
    }
  else if (jpx_src != NULL)
    {
      int available_codestreams;
      bool all_found = jpx_src->count_codestreams(available_codestreams);
      jpx_codestream_source stream = jpx_src->access_codestream(stream_idx);
      if (!stream)
        {
          if ((stream_idx >= 0) &&
              ((stream_idx < available_codestreams) || !all_found))
            return -1;
          KDU_ERROR_DEV(e,16); e <<
            KDU_TXT("Invalid codestream index passed to "
            "`kdu_region_compositor::set_single_component'.");
        }
    }
  else
    {
      assert(mj2_src != NULL);
      kdu_uint32 track_idx=0;
      int frame_idx=0, field_idx=0;
      bool can_translate =
        mj2_src->find_stream(stream_idx,track_idx,frame_idx,field_idx);
      if (!can_translate)
        return -1;
      if (track_idx == 0)
        { KDU_ERROR_DEV(e,17); e <<
            KDU_TXT("Invalid codestream index passed to "
            "`kdu_region_compositor::set_single_component.");
        }
      mj2_video_source *track = mj2_src->access_video_track(track_idx);
      if (track == NULL)
        return -1; // Track is not yet ready to be opened
      track->seek_to_frame(frame_idx);
      if (!track->can_open_stream(field_idx))
        return -1; // Codestream main header not yet available
    }

  // Create the code-stream
  if (add_active_stream(stream_idx,-1,false) == NULL)
    { KDU_ERROR(e,18); e <<
        KDU_TXT("Unable to complete call to "
        "`kdu_region_compositor::set_single_component', since the requested "
        "codestream (index, starting from 0, equals ")
        << stream_idx <<
        KDU_TXT(") cannot be opened.");
    }
  assert(active_streams->codestream_idx == stream_idx);
  return active_streams->set_mode(component_idx,access_mode);
}

/******************************************************************************/
/*                      kdu_region_compositor::set_scale                      */
/******************************************************************************/

void
  kdu_region_compositor::set_scale(bool transpose, bool vflip,
                                   bool hflip, float scale)
{
  invalid_scale_code = 0;
  bool no_change = have_valid_scale &&
      (transpose == this->transpose) && (vflip == this->vflip) &&
      (hflip == this->hflip) && (scale == this->scale);
  if ((!composition_invalid) && no_change)
    return; // No change
  this->transpose = transpose;
  this->vflip = vflip;
  this->hflip = hflip;
  this->scale = scale;
  have_valid_scale = true;
  composition_invalid = true;
  if (!no_change)
    {
      buffer_region.size = kdu_coords(0,0);
      refresh_mgr->reset();
    }
}

/******************************************************************************/
/*                  kdu_region_compositor::set_buffer_surface                 */
/******************************************************************************/

void
  kdu_region_compositor::set_buffer_surface(kdu_dims region,
                                            kdu_int32 bckgnd)
{
  kdu_uint32 background = (kdu_uint32) bckgnd;
  if ((buffer_region != region) || (background != buffer_background))
    flush_composition_queue();
  
  bool start_from_scratch =
    (composition_buffer != NULL) && (background != buffer_background);
  buffer_background = background;

  if (composition_invalid)
    {
      buffer_region = region;
      return; // Defer further processing until we update the configuration
    }

  kdu_dims old_region = buffer_region;
  if (composition_buffer != NULL)
    {
      kdu_compositor_buf *old_buffer = composition_buffer;
      buffer_region = region & total_composition_dims;
      kdu_coords new_size = buffer_region.size;
      if ((new_size.x > buffer_size.x) || (new_size.y > buffer_size.y))
        composition_buffer =
          internal_allocate_buffer(new_size,buffer_size,true);
      if (start_from_scratch)
        initialize_buffer_surface(composition_buffer,buffer_region,
                                  NULL,kdu_dims(),buffer_background);
      else
        initialize_buffer_surface(composition_buffer,buffer_region,
                                  old_buffer,old_region,buffer_background);
      if (old_buffer != composition_buffer)
        internal_delete_buffer(old_buffer);
    }
  else
    buffer_region = region & total_composition_dims;
  if (active_layers == NULL)
    {
      assert((active_streams != NULL) && (active_streams->next == NULL) &&
             (composition_buffer != NULL));
      active_streams->set_buffer_surface(composition_buffer,buffer_region,
                                         false);
      processing_complete = false;
    }
  else
    set_layer_buffer_surfaces();

  // Adjust the `refresh_mgr' object to include any new regions.
  kdu_dims common_region = old_region & buffer_region;
  if (start_from_scratch || !common_region)
    {
      refresh_mgr->reset();
      refresh_mgr->add_region(buffer_region);
    }
  else
    { // Adjust what is still to be refreshed, and add new elements based on
      // the portion of the buffer which is new
      refresh_mgr->adjust(buffer_region);
      kdu_coords common_min = common_region.pos;
      kdu_coords common_lim = common_min + common_region.size;
      kdu_coords new_min = buffer_region.pos;
      kdu_coords new_lim = new_min + buffer_region.size;
      int needed_left = common_min.x - new_min.x;
      int needed_right = new_lim.x - common_lim.x;
      int needed_above = common_min.y - new_min.y;
      int needed_below = new_lim.y - common_lim.y;
      assert((needed_left >= 0) && (needed_right >= 0) &&
             (needed_above >= 0) && (needed_below >= 0));
      kdu_dims region_left = common_region;
      region_left.pos.x = new_min.x; region_left.size.x = needed_left;
      kdu_dims region_right = common_region;
      region_right.pos.x = common_lim.x; region_right.size.x = needed_right;
      kdu_dims region_above = buffer_region;
      region_above.pos.y = new_min.y; region_above.size.y = needed_above;
      kdu_dims region_below = buffer_region;
      region_below.pos.y = common_lim.y; region_below.size.y = needed_below;
      if (!region_left.is_empty())
        refresh_mgr->add_region(region_left);
      if (!region_right.is_empty())
        refresh_mgr->add_region(region_right);
      if (!region_above.is_empty())
        refresh_mgr->add_region(region_above);
      if (!region_below.is_empty())
        refresh_mgr->add_region(region_below);
    }

  kdrc_stream *sp;
  for (sp=active_streams; sp != NULL; sp=sp->next)
    refresh_mgr->adjust(sp);
}

/******************************************************************************/
/*                  kdu_region_compositor::find_optimal_scale                 */
/******************************************************************************/

float
  kdu_region_compositor::find_optimal_scale(kdu_dims region,
                                            float anchor_scale,
                                            float min_scale, float max_scale,
                                            int *compositing_layer_idx,
                                            int *codestream_idx,
                                            int *component_idx)
{
  kdrc_stream *best_stream=NULL;
  kdrc_layer *scan, *best_layer=NULL;
  kdu_long layer_area, best_layer_area=0;
  if (active_layers != NULL)
    {
      if (region.is_empty() || !have_valid_scale)
        best_layer = last_active_layer;
      else
        for (scan=last_active_layer; scan != NULL; scan=scan->prev)
          {
            layer_area = scan->measure_visible_area(region,false);
            if ((best_layer == NULL) || (layer_area > best_layer_area))
              { best_layer_area = layer_area;  best_layer = scan; }
          }
      if (best_layer != NULL)
        {
          best_stream = best_layer->get_stream(0);
          if (best_stream == NULL)
            best_layer = NULL;
        }
    }
  else
    best_stream = active_streams;
  
  float result;
  if (best_layer != NULL)
    result = best_layer->find_optimal_scale(anchor_scale,min_scale,max_scale);
  else if (best_stream != NULL)
    result = best_stream->find_optimal_scale(anchor_scale,min_scale,max_scale);
  else
    {
      if (anchor_scale < min_scale)
        result = min_scale;
      else if (anchor_scale > max_scale)
        result = max_scale;
      else
        result = anchor_scale;
    }

  if (compositing_layer_idx != NULL)
    *compositing_layer_idx = (best_layer==NULL)?(-1):(best_layer->layer_idx);
  if (best_stream == NULL)
    {
      if (codestream_idx != NULL)
        *codestream_idx = -1;
      if (component_idx != NULL)
        *component_idx = -1;
    }
  else
    {
      if (codestream_idx != NULL)
        *codestream_idx = best_stream->codestream_idx;
      if (component_idx != NULL)
        *component_idx = best_stream->get_primary_component_idx();
    }

  return result;
}

/******************************************************************************/
/*                      kdu_region_compositor::map_region                     */
/******************************************************************************/

bool
  kdu_region_compositor::map_region(int &codestream_idx, kdu_dims &region)
{
  if (active_layers == NULL)
    {
      kdu_dims result;
      if ((active_streams != NULL) &&
          !(result = active_streams->map_region(region)).is_empty())
        {
          region = result;
          codestream_idx = active_streams->codestream_idx;
          return true;
        }
    }
  else
    { // Invoke on layers in order, so that the top-most matching layer is used
      kdrc_layer *scan;
      for (scan=last_active_layer; scan != NULL; scan=scan->prev)
        if (scan->map_region(codestream_idx,region))
          return true;
    }
  return false;
}

/******************************************************************************/
/*                 kdu_region_compositor::inverse_map_region                  */
/******************************************************************************/

kdu_dims
  kdu_region_compositor::inverse_map_region(kdu_dims region,
                                            int codestream_idx,
                                            int layer_idx)
{
  if (codestream_idx < 0)
    { // Special case of a region which is associated with the renderered
      // result, apart from scale and geometry adjustments
      region.to_apparent(transpose,vflip,hflip);
      kdu_coords lim = region.pos + region.size;
      region.pos.x = (int) floor(((double) region.pos.x) * scale);
      region.pos.y = (int) floor(((double) region.pos.y) * scale);
      region.size.x = ((int) ceil(((double) lim.x) * scale)) - region.pos.x;
      region.size.y = ((int) ceil(((double) lim.y) * scale)) - region.pos.y;
      return region;
    }
  
  kdrc_stream *stream = NULL;
  if (active_layers != NULL)
    {
      kdrc_layer *scan;
      for (scan=last_active_layer; scan != NULL; scan=scan->prev)
        {
          if ((layer_idx >= 0) && (layer_idx != scan->layer_idx))
            continue;
          if (((stream = scan->get_stream(0)) != NULL) &&
              (stream->codestream_idx == codestream_idx))
            return stream->inverse_map_region(region);
        }
      for (scan=last_active_layer; scan != NULL; scan=scan->prev)
        {
          if ((layer_idx >= 0) && (layer_idx != scan->layer_idx))
            continue;
          if (((stream = scan->get_stream(1)) != NULL) &&
              (stream->codestream_idx == codestream_idx))
            return stream->inverse_map_region(region);
        }
    }
  else if ((active_streams != NULL) && (layer_idx <= 0))
    {
      for (stream=active_streams; stream != NULL; stream=stream->next)
        if (stream->codestream_idx == codestream_idx)
          return stream->inverse_map_region(region);
    }
  return kdu_dims(); // Cannot find a match  
}

/******************************************************************************/
/*                      kdu_region_compositor::find_point                     */
/******************************************************************************/

bool
  kdu_region_compositor::find_point(kdu_coords point, int &layer_idx,
                                    int &codestream_idx)
{
  layer_idx = codestream_idx = -1;
  if (active_layers != NULL)
    {
      kdrc_layer *scan;
      for (scan=last_active_layer; scan != NULL; scan=scan->prev)
        {
          kdrc_stream *stream = scan->get_stream(0);
          if (stream == NULL)
            continue;
          kdu_dims region = stream->find_composited_region(true);
          if ((point.x >= region.pos.x) && (point.y >= region.pos.y) &&
              (point.x < (region.pos.x+region.size.x)) &&
              (point.y < (region.pos.y+region.size.y)))
            {
              codestream_idx = stream->codestream_idx;
              layer_idx = scan->layer_idx;
              return true;
            }
        }
    }
  else if (active_streams != NULL)
    {
      codestream_idx = active_streams->codestream_idx;
      return true;
    }
  return false;
}

/******************************************************************************/
/*                 kdu_region_compositor::find_layer_region                   */
/******************************************************************************/

kdu_dims
  kdu_region_compositor::find_layer_region(int layer_idx, int instance,
                                           bool apply_cropping)
{
  if (!have_valid_scale)
    return kdu_dims();
  kdrc_layer *scan;
  for (scan=last_active_layer; scan != NULL; scan=scan->prev)
    if (scan->layer_idx == layer_idx)
      {
        if (instance == 0)
          break;
        instance--;
      }
  if (scan == NULL)
    return kdu_dims();
  kdrc_stream *stream;
  kdu_dims result, region;
  for (int w=0; (stream=scan->get_stream(w)) != NULL; w++)
    {
      region = stream->find_composited_region(apply_cropping);
      if (w == 0)
        result = region;
      else
        result &= region;
    }
  return result;
}

/******************************************************************************/
/*               kdu_region_compositor::find_codestream_region                */
/******************************************************************************/

kdu_dims
  kdu_region_compositor::find_codestream_region(int stream_idx, int instance,
                                                bool apply_cropping)
{
  if (!have_valid_scale)
    return kdu_dims();
  kdrc_layer *scan;
  kdrc_stream *stream;
  for (scan=last_active_layer; scan != NULL; scan=scan->prev)
    {
      for (int w=0; (stream=scan->get_stream(w)) != NULL; w++)
        {
          if (stream->codestream_idx == stream_idx)
            {
              if (instance == 0)
                return stream->find_composited_region(apply_cropping);
              instance--;
            }
        }
    }
  return kdu_dims();
}

/******************************************************************************/
/*                      kdu_region_compositor::refresh                        */
/******************************************************************************/

bool
  kdu_region_compositor::refresh()
{
  if (!persistent_codestreams)
    return true;
  processing_complete = false;
  kdrc_stream *scan;
  for (scan=active_streams; scan != NULL; scan=scan->next)
    scan->invalidate_surface();
  for (scan=inactive_streams; scan != NULL; scan=scan->next)
    scan->invalidate_surface();
  bool have_newly_initialized_layers = false;
  kdrc_layer *lscan;
  for (lscan=active_layers; lscan != NULL; lscan=lscan->next)
    if (lscan->get_stream(0) == NULL)
      {
        if (lscan->reinit())
          have_newly_initialized_layers = true;
      }
    else
      {
        lscan->change_frame(); // In case a frame change is pending
        lscan->update_overlay(false,false);
      }

  if (!composition_invalid)
    {
      if (have_newly_initialized_layers)
        {
          composition_invalid = true;
          return false;
        }
      refresh_mgr->reset();
      refresh_mgr->add_region(buffer_region);
      kdrc_stream *sp;
      for (sp=active_streams; sp != NULL; sp=sp->next)
        refresh_mgr->adjust(sp);
    }
  if (initialize_surfaces_on_next_refresh)
    {
      initialize_surfaces_on_next_refresh = false;
      if (!(composition_invalid || !buffer_region))
        { // Otherwise initialization will take place when buffer is allocated
          if (composition_buffer != NULL)
            initialize_buffer_surface(composition_buffer,buffer_region,
                                      NULL,kdu_dims(),buffer_background);
          else if ((active_layers != NULL) && (active_layers->next == NULL))
            initialize_buffer_surface(active_layers->get_layer_buffer(),
                                      buffer_region,NULL,kdu_dims(),0xFFFFFFFF);
        }
    }
  return true;
}

/******************************************************************************/
/*                   kdu_region_compositor::update_overlays                   */
/******************************************************************************/

void
  kdu_region_compositor::update_overlays(bool start_from_scratch)
{
  kdrc_layer *scan;
  for (scan=active_layers; scan != NULL; scan=scan->next)
    {
      scan->update_overlay(start_from_scratch,true);
      if (scan->have_overlay_info)
        processing_complete = false;
    }
}

/******************************************************************************/
/*                 kdu_region_compositor::configure_overlays                  */
/******************************************************************************/

void
  kdu_region_compositor::configure_overlays(bool enable, int min_display_size,
                                            int painting_param)
{
  if ((enable == this->enable_overlays) &&
      (painting_param == this->overlay_painting_param) &&
      (min_display_size == this->overlay_min_display_size))
    return;
  this->enable_overlays = enable;
  this->overlay_painting_param = painting_param;
  this->overlay_min_display_size = min_display_size;
  if (!composition_invalid)
    { // Otherwise, the ensuing steps will be performed in `update_composition'
      kdrc_layer *scan;
      for (scan=active_layers; scan != NULL; scan=scan->next)
        {
          scan->configure_overlay(enable,min_display_size,painting_param);
          if (scan->have_overlay_info)
            processing_complete = false;
        }
    }
}

/******************************************************************************/
/*                    kdu_region_compositor::search_overlays                  */
/******************************************************************************/

jpx_metanode
  kdu_region_compositor::search_overlays(kdu_coords point, int &codestream_idx)
{
  jpx_metanode result;
  if ((active_layers == NULL) || !enable_overlays)
    return result; // Returns empty interface
  kdrc_layer *scan;
  for (scan=last_active_layer; scan != NULL; scan=scan->prev)
    {
      bool is_opaque;
      result = scan->search_overlay(point,codestream_idx,is_opaque);
      if (result.exists() || is_opaque)
        break;
    }
  return result;
}

/******************************************************************************/
/*                   kdu_region_compositor::halt_processing                   */
/******************************************************************************/

void
  kdu_region_compositor::halt_processing()
{
  kdrc_stream *scan;
  for (scan=active_streams; scan != NULL; scan=scan->next)
    scan->stop_processing();
}

/******************************************************************************/
/*              kdu_region_compositor::get_total_composition_dims             */
/******************************************************************************/

bool
  kdu_region_compositor::get_total_composition_dims(kdu_dims &dims)
{
  if (composition_invalid && !update_composition())
    return false;
  dims = total_composition_dims;
  return true;
}

/******************************************************************************/
/*               kdu_region_compositor::get_composition_buffer                */
/******************************************************************************/

kdu_compositor_buf *
  kdu_region_compositor::get_composition_buffer(kdu_dims &region)
{
  if (composition_invalid && !update_composition())
    return NULL;
  region = buffer_region;
  if (queue_head != NULL)
    return queue_head->composition_buffer;

  if (composition_buffer != NULL)
    return composition_buffer;
  assert((active_layers != NULL) && (active_layers->next == NULL) &&
         (active_layers == last_active_layer));
  return active_layers->get_layer_buffer();
}

/******************************************************************************/
/*              kdu_region_compositor::push_composition_buffer                */
/******************************************************************************/

bool
  kdu_region_compositor::push_composition_buffer(kdu_long stamp, int id)
{
  if (!is_processing_complete())
    return false;
  kdrc_queue *elt = queue_free;
  if (elt != NULL)
    queue_free = elt->next;
  else
    elt = new kdrc_queue;

  if (queue_tail == NULL)
    queue_head = queue_tail = elt;
  else
    queue_tail = queue_tail->next = elt;
  elt->next = NULL;
  elt->id = id;
  elt->stamp = stamp;
  if (composition_buffer != NULL)
    {
      elt->composition_buffer = composition_buffer;
      composition_buffer =
        internal_allocate_buffer(buffer_region.size,buffer_size,true);
      initialize_buffer_surface(composition_buffer,buffer_region,
                                NULL,kdu_dims(),buffer_background);
      kdrc_layer *lp = active_layers;
      if (lp == NULL)
        active_streams->set_buffer_surface(composition_buffer,buffer_region,
                                           true);
      else
        for (; lp != NULL; lp=lp->next)
          lp->change_compositing_buffer(composition_buffer);
    }
  else
    {
      assert((active_layers != NULL) && (active_layers->next == NULL) &&
             (active_layers == last_active_layer));
      elt->composition_buffer =
        active_layers->take_layer_buffer(can_skip_surface_initialization);
    }

  processing_complete = false;
  refresh_mgr->reset();
  refresh_mgr->add_region(buffer_region);
  kdrc_stream *sp;
  for (sp=active_streams; sp != NULL; sp=sp->next)
    refresh_mgr->adjust(sp);

  return true;
}

/******************************************************************************/
/*              kdu_region_compositor::pop_composition_buffer                */
/******************************************************************************/

bool
  kdu_region_compositor::pop_composition_buffer()
{
  kdrc_queue *elt = queue_head;
  if (elt == NULL)
    return false;
  internal_delete_buffer(elt->composition_buffer);
  elt->composition_buffer = NULL;
  if ((queue_head = elt->next) == NULL)
    queue_tail = NULL;
  elt->next = queue_free;
  queue_free = elt;
  return true;
}

/******************************************************************************/
/*             kdu_region_compositor::inspect_composition_queue               */
/******************************************************************************/

bool
  kdu_region_compositor::inspect_composition_queue(int elt_idx,
                                                   kdu_long *stamp, int *id)
{
  if (elt_idx < 0)
    return false;
  kdrc_queue *elt = queue_head;
  while ((elt_idx > 0) && (elt != NULL))
    {
      elt_idx--;
      elt = elt->next;
    }
  if (elt == NULL)
    return false;
  if (id != NULL)
    *id = elt->id;
  if (stamp != NULL)
    *stamp = elt->stamp;
  return true;
}

/******************************************************************************/
/*              kdu_region_compositor::flush_composition_queue                */
/******************************************************************************/

void
  kdu_region_compositor::flush_composition_queue()
{
  while ((queue_tail=queue_head) != NULL)
    {
      queue_head = queue_tail->next;
      queue_tail->next = queue_free;
      queue_free = queue_tail;
      if (queue_free->composition_buffer != NULL)
        {
          internal_delete_buffer(queue_free->composition_buffer);
          queue_free->composition_buffer = NULL;
        }
    }
}

/******************************************************************************/
/*                    kdu_region_compositor::set_thread_env                   */
/******************************************************************************/

kdu_thread_env *
  kdu_region_compositor::set_thread_env(kdu_thread_env *env,
                                        kdu_thread_queue *env_queue)
{
  if ((env != NULL) && !env->exists())
    env = NULL;
  if (env == NULL)
    env_queue = NULL;
  if ((env != this->env) || (env_queue != this->env_queue))
    {
      kdrc_stream *scan;
      for (scan=active_streams; scan != NULL; scan=scan->next)
        scan->set_thread_env(env,env_queue);
      for (scan=inactive_streams; scan != NULL; scan=scan->next)
        scan->set_thread_env(env,env_queue);
    }
  kdu_thread_env *old_env = this->env;
  this->env = env;
  this->env_queue = env_queue;
  return old_env;
}

/******************************************************************************/
/*                       kdu_region_compositor::process                       */
/******************************************************************************/

bool
  kdu_region_compositor::process(int suggested_increment, kdu_dims &new_region)
{
  invalid_scale_code = 0;
  if (composition_invalid && !update_composition())
    return false;
  if (processing_complete)
    return false;
  
  kdrc_layer *lp;

  processing_complete = true; // Until proven otherwise

  if (refresh_mgr->pop_region(new_region))
    processing_complete = false;
  else
    {
      kdrc_stream *scan, *target=NULL;
      for (scan=active_streams; scan != NULL; scan=scan->next)
        if (scan->can_process_now() &&
            ((target == NULL) || (scan->priority > target->priority)))
          target = scan;
      if (target != NULL)
        {
          assert(!target->is_complete);
          processing_complete = false;
          if (!target->process(suggested_increment,new_region,
                               invalid_scale_code))
            { // Scale has been found to be invalid
              assert(invalid_scale_code != 0);
              have_valid_scale = false;
              composition_invalid = true;
              return false;
            }
        }
      else
        { // Scan through the active layers for overlays that need processing
          for (lp=active_layers; lp != NULL; lp=lp->next)
            if (lp->have_overlay_info && lp->process_overlay(new_region))
              {
                processing_complete = false;
                break;
              }
        }
    }

  if ((!processing_complete) && (composition_buffer != NULL) &&
      (active_layers != NULL) && !new_region.is_empty())
    { // Update composition
      for (lp=active_layers; lp != NULL; lp=lp->next)
        lp->update_composition(new_region,buffer_background);
    }
  return true;
}

/******************************************************************************/
/*               kdu_region_compositor::set_max_quality_layers                */
/******************************************************************************/

void
  kdu_region_compositor::set_max_quality_layers(int max_quality_layers)
{
  this->max_quality_layers = max_quality_layers;
  kdrc_stream *scan;
  for (scan=active_streams; scan != NULL; scan=scan->next)
    scan->set_max_quality_layers(max_quality_layers);
}

/******************************************************************************/
/*          kdu_region_compositor::get_max_available_quality_layers           */
/******************************************************************************/

int
  kdu_region_compositor::get_max_available_quality_layers()
{
  int val, result = 0;
  kdrc_stream *scan;
  for (scan=active_streams; scan != NULL; scan=scan->next)
    {
      val = scan->codestream->ifc.get_max_tile_layers();
      if (val > result)
        result = val;
    }
  return result;
}

/******************************************************************************/
/*                 kdu_region_compositor::get_next_codestream                 */
/******************************************************************************/

kdrc_stream *
  kdu_region_compositor::get_next_codestream(kdrc_stream *last_stream_ref,
                                             bool only_active_codestreams,
                                             bool no_duplicates)
{
  kdrc_stream *scan = last_stream_ref;
  bool try_inactive =
    ((scan == NULL) || (scan->is_active)) && !only_active_codestreams;
  if (scan == NULL)
    scan = active_streams;
  else
    scan = scan->next;
  if ((scan == NULL) && try_inactive)
    { scan = inactive_streams; try_inactive = false; }
  if (scan == NULL)
    return NULL;
  while (no_duplicates && (scan->codestream->head != scan))
    {
      scan = scan->next;
      if ((scan == NULL) && try_inactive)
        { scan = inactive_streams; try_inactive = false; }
      if (scan == NULL)
        return NULL;
    }
  return scan;    
}

/******************************************************************************/
/*             kdu_region_compositor::get_next_visible_codestream             */
/******************************************************************************/

kdrc_stream *
  kdu_region_compositor::get_next_visible_codestream(
                                             kdrc_stream *last_stream_ref,
                                             kdu_dims region,
                                             bool include_alpha)
{
  kdrc_stream *scan = last_stream_ref;
  if (scan == NULL)
    scan = active_streams;
  else if (scan->is_active)
    scan = scan->next;
  else
    return NULL;
  for (; scan != NULL; scan=scan->next)
    {
      if (!scan->is_scale_valid())
        continue;
      if (scan->layer == NULL)
        break;
      if ((!include_alpha) && (scan != scan->layer->get_stream(0)))
        continue; // Not a primary codestream
      if (scan->layer->measure_visible_area(region,true) > 0)
        break;
    }
  return scan;
}

/******************************************************************************/
/*                  kdu_region_compositor::access_codestream                  */
/******************************************************************************/

kdu_codestream
  kdu_region_compositor::access_codestream(kdrc_stream *stream_ref)
{
  if (stream_ref == NULL)
    return kdu_codestream();
  kdrc_stream *scan = stream_ref->codestream->head;
  for (; scan != NULL; scan=scan->next_codestream_user)
    scan->stop_processing();
  return stream_ref->codestream->ifc;
}

/******************************************************************************/
/*                 kdu_region_compositor::get_codestream_info                 */
/******************************************************************************/

bool
  kdu_region_compositor::get_codestream_info(kdrc_stream *stream,
                                             int &codestream_idx,
                                             int &compositing_layer_idx,
                                             int *components_in_use,
                                             int *principle_component_idx,
                                             float *principle_component_scale_x,
                                             float *principle_component_scale_y)
{
  assert(stream != NULL);
  codestream_idx = stream->codestream_idx;
  compositing_layer_idx = stream->layer_idx;
  if (components_in_use != NULL)
    stream->get_components_in_use(components_in_use);
  if (principle_component_idx != NULL)
    *principle_component_idx = stream->get_primary_component_idx();
  if ((principle_component_scale_x != NULL) ||
      (principle_component_scale_y != NULL))
    {
      double scale_x=1.0, scale_y=1.0;
      if (stream->layer != NULL)
        stream->layer->get_component_scale_factors(stream,scale_x,scale_y);
      *principle_component_scale_x = (float) scale_x;
      *principle_component_scale_y = (float) scale_y;
    }
  return stream->is_active;
}

/******************************************************************************/
/*               kdu_region_compositor::get_codestream_packets                */
/******************************************************************************/

bool
  kdu_region_compositor::get_codestream_packets(kdrc_stream *stream,
                             kdu_dims region,
                             kdu_long &visible_precinct_samples,
                             kdu_long &visible_packet_samples,
                             kdu_long &max_visible_packet_samples)
{
  visible_precinct_samples = 0;
  visible_packet_samples = 0;
  max_visible_packet_samples = 0;
  if ((stream == NULL) || (!stream->is_scale_valid()) || !have_valid_scale)
   return false;
  region = stream->find_composited_region(true) & region;
  if (!region)
    return false;

  if (stream->layer == NULL)
    stream->get_packet_stats(region,visible_precinct_samples,
                             visible_packet_samples,
                             max_visible_packet_samples);
  else
    stream->layer->get_visible_packet_stats(stream,region,
                                            visible_precinct_samples,
                                            visible_packet_samples,
                                            max_visible_packet_samples);
  return (visible_precinct_samples != 0);
}

/******************************************************************************/
/*              kdu_region_compositor::set_layer_buffer_surfaces              */
/******************************************************************************/

void
  kdu_region_compositor::set_layer_buffer_surfaces()
{
  kdu_dims visible_region = buffer_region;
  kdu_coords visible_min = visible_region.pos;
  kdu_coords visible_lim = visible_min + visible_region.size;
  kdu_dims opaque_region;
  for (kdrc_layer *scan=last_active_layer; scan != NULL; scan=scan->prev)
    {
      scan->set_buffer_surface(buffer_region,visible_region,
                               composition_buffer,
                               can_skip_surface_initialization);
      processing_complete = false;
      if (scan->have_alpha_channel)
        continue; // No effect on opaque region
      scan->get_buffer_region(opaque_region);
      kdu_coords opaque_min = opaque_region.pos;
      kdu_coords opaque_lim = opaque_min + opaque_region.size;
      if ((opaque_min.x==visible_min.x) && (opaque_lim.x==visible_lim.x))
        { // May be able to reduce height of visible region
          if ((opaque_min.y < visible_lim.y) &&
              (opaque_lim.y >= visible_lim.y))
            visible_lim.y = opaque_min.y;
          else if ((opaque_lim.y > visible_min.y) &&
                   (opaque_min.y <= visible_min.y))
            visible_min.y = opaque_lim.y;
          if (visible_min.y < buffer_region.pos.y)
            visible_min.y = buffer_region.pos.y;
          if (visible_lim.y < visible_min.y)
            visible_lim.y = visible_min.y;
          visible_region.pos.y = visible_min.y;
          visible_region.size.y = visible_lim.y - visible_min.y;
        }
      else if ((opaque_min.y==visible_min.y) && (opaque_lim.y==visible_lim.y))
        { // May be able to reduce width of visible region
          if ((opaque_min.x < visible_lim.x) &&
              (opaque_lim.x >= visible_lim.x))
            visible_lim.x = opaque_min.x;
          else if ((opaque_lim.x > visible_min.x) &&
                   (opaque_min.x <= visible_min.x))
            visible_min.x = opaque_lim.x;
          if (visible_min.x < buffer_region.pos.x)
            visible_min.x = buffer_region.pos.x;
          if (visible_lim.x < visible_min.x)
            visible_lim.x = visible_min.x;
          visible_region.pos.x = visible_min.x;
          visible_region.size.x = visible_lim.x - visible_min.x;
        }
    }
}

/******************************************************************************/
/*                  kdu_region_compositor::update_composition                 */
/******************************************************************************/

bool
  kdu_region_compositor::update_composition()
{
  invalid_scale_code = 0;
  if (!have_valid_scale)
    return false;
  if ((active_streams == NULL) && (active_layers == NULL))
    return false;

  // Start by setting the scale of every layer and hence finding the total
  // composition size
  bool need_separate_composition_buffer = false;
  kdu_dims layer_dims;
  if (active_layers == NULL)
    { // Processing single layer or raw codestream
      assert(active_streams->next == NULL);
      need_separate_composition_buffer = true;
      total_composition_dims = layer_dims =
        active_streams->set_scale(kdu_dims(),kdu_dims(),kdu_coords(0,0),
                                  kdu_coords(1,1),transpose,vflip,hflip,scale,
                                  invalid_scale_code);
      if (!total_composition_dims)
        return (have_valid_scale=false);
    }
  else
    {
      kdrc_layer *scan;
      kdu_coords min, lim, layer_min, layer_lim;
      for (scan=active_layers; scan != NULL; scan=scan->next)
        {
          if ((scan != active_layers) || scan->have_alpha_channel ||
              scan->have_overlay_info)
            need_separate_composition_buffer = true;
          if (scan->get_stream(0) == NULL)
            continue; // Layer is not initialized yet
          layer_dims = scan->set_scale(transpose,vflip,hflip,scale,
                                       invalid_scale_code);
          if (!layer_dims)
            return (have_valid_scale=false);
          if (min == lim)
            { // First layer
              min = layer_dims.pos;
              lim = min + layer_dims.size;
            }
          else
            { // Enlarge region as necessary
              layer_min = layer_dims.pos;
              layer_lim = layer_min + layer_dims.size;
              if (layer_min.x < min.x)
                min.x = layer_min.x;
              if (layer_min.y < min.y)
                min.y = layer_min.y;
              if (layer_lim.x > lim.x)
                lim.x = layer_lim.x;
              if (layer_lim.y > lim.y)
                lim.y = layer_lim.y;
            }
        }
      total_composition_dims.pos = min;
      total_composition_dims.size = lim - min;
      if (total_composition_dims.is_empty() ||
          !fixed_composition_dims.is_empty())
        {
          need_separate_composition_buffer = true;
          if (fixed_composition_dims.is_empty())
            {
              min = default_composition_dims.pos;
              lim = default_composition_dims.size-min;
            }
          else
            {
              min = fixed_composition_dims.pos;
              lim = fixed_composition_dims.size-min;
            }
          min.x = (int) floor(scale * (double) min.x);
          min.y = (int) floor(scale * (double) min.y);
          lim.x = (int) ceil(scale * (double) lim.x);
          lim.y = (int) ceil(scale * (double) lim.y);
          kdu_dims adj_dims; adj_dims.pos = min; adj_dims.size = lim-min;
          adj_dims.to_apparent(transpose,vflip,hflip);
          total_composition_dims = adj_dims;
        }
    }

  // Next, find the compositing buffer region and create a separate
  // compositing buffer if necessary
  kdu_dims old_region = buffer_region;
  buffer_region &= total_composition_dims;
  if (buffer_region != old_region)
    flush_composition_queue(); // Some change has affected the buffer region
  if (need_separate_composition_buffer)
    {
      kdu_coords new_size = buffer_region.size;
      if ((composition_buffer == NULL) ||
          (new_size.x > buffer_size.x) || (new_size.y > buffer_size.y))
        {
          if (composition_buffer != NULL)
            {
              internal_delete_buffer(composition_buffer);
              composition_buffer = NULL;
            }
          composition_buffer =
            internal_allocate_buffer(new_size,buffer_size,true);
        }
      initialize_buffer_surface(composition_buffer,buffer_region,
                                NULL,kdu_dims(),buffer_background);
    }
  else
    { // Can re-use first layer's compositing buffer
      if (composition_buffer != NULL)
        {
          internal_delete_buffer(composition_buffer);
          composition_buffer = NULL;
          buffer_size = kdu_coords(0,0);
        }
    }

  // Finally, set the buffer surface for all layers and perform whatever
  // recomposition is possible
  if (active_layers == NULL)
    active_streams->set_buffer_surface(composition_buffer,buffer_region,true);
  else
    {
      set_layer_buffer_surfaces();
      kdrc_layer *scan;
      for (scan=active_layers; scan != NULL; scan=scan->next)
        scan->configure_overlay(enable_overlays,overlay_min_display_size,
                                overlay_painting_param);
    }

  refresh_mgr->reset();
  refresh_mgr->add_region(buffer_region);
  kdrc_stream *sp;
  for (sp=active_streams; sp != NULL; sp=sp->next)
    refresh_mgr->adjust(sp);

  processing_complete = false; // Force at least one call to `process'
  composition_invalid = false;
  return true;
}

/******************************************************************************/
/*              kdu_region_compositor::internal_allocate_buffer               */
/******************************************************************************/

kdu_compositor_buf *
  kdu_region_compositor::internal_allocate_buffer(kdu_coords min_size,
                                                  kdu_coords &actual_size,
                                                  bool read_access_required)
{
  kdu_compositor_buf *container =
    allocate_buffer(min_size,actual_size,read_access_required);
  if (container == NULL)
    {
      int num_pels = min_size.x*min_size.y;
      kdu_uint32 *buf = new kdu_uint32[num_pels];
      if (buf == NULL)
        { KDU_ERROR(e,19); e <<
          KDU_TXT("Trying to allocate an image buffer surface which is "
          "too large for the available memory.  Problem is likely due to a "
          "failure to recognize that the `kdu_region_compressor' object is "
          "designed principally for incremental processing of smaller "
          "regions.  Requested buffer is for an image region of ")
          << min_size.x <<
          KDU_TXT(" by ") << min_size.y <<
          KDU_TXT(" pixels.");
        }
      actual_size = min_size;
      container = new kdu_compositor_buf;
      container->init(buf,actual_size.x);
      container->set_read_accessibility(read_access_required);
      container->internal = true;
    }
  container->accessible_region.size = actual_size;
  return container;
}

/******************************************************************************/
/*               kdu_region_compositor::internal_delete_buffer                */
/******************************************************************************/

void
  kdu_region_compositor::internal_delete_buffer(kdu_compositor_buf *buffer)
{
  if (buffer->internal)
    delete buffer;
  else
    delete_buffer(buffer);
}

/******************************************************************************/
/*                    kdu_region_compositor::paint_overlay                    */
/******************************************************************************/

void
  kdu_region_compositor::paint_overlay(kdu_compositor_buf *buffer,
                                  kdu_dims buffer_region,
                                  kdu_dims bounding_region, int codestream_idx,
                                  jpx_metanode node, int painting_param,
                                  kdu_coords image_offset,
                                  kdu_coords subsampling, bool transpose,
                                  bool vflip, bool hflip,
                                  kdu_coords expansion_numerator,
                                  kdu_coords expansion_denominator,
                                  kdu_coords compositing_offset)
{
  int num_regions = node.get_num_regions();
  const jpx_roi *regions = node.get_regions();
  for (; num_regions > 0; num_regions--, regions++)
    {
      kdu_dims mapped_region = regions->region;
      mapped_region.pos += image_offset;
      kdu_coords min = mapped_region.pos, lim = min + mapped_region.size;
      min.x = ceil_ratio(min.x,subsampling.x);
      min.y = ceil_ratio(min.y,subsampling.y);
      lim.x = ceil_ratio(lim.x,subsampling.x);
      lim.y = ceil_ratio(lim.y,subsampling.y);
      mapped_region.pos = min; mapped_region.size = lim - min;
      mapped_region.to_apparent(transpose,vflip,hflip);

      min = mapped_region.pos;
      lim = min + mapped_region.size;
      min.x = long_floor_ratio(((kdu_long) min.x)*expansion_numerator.x,
                               expansion_denominator.x);
      min.y = long_floor_ratio(((kdu_long) min.y)*expansion_numerator.y,
                               expansion_denominator.y);
      lim.x = long_ceil_ratio(((kdu_long) lim.x)*expansion_numerator.x,
                              expansion_denominator.x);
      lim.y = long_ceil_ratio(((kdu_long) lim.y)*expansion_numerator.y,
                              expansion_denominator.y);
      mapped_region.pos = min;
      mapped_region.size = lim - min;
      if (!mapped_region)
        continue; // Region is too small to be painted

      mapped_region.pos -= compositing_offset;

      int paint_alpha = painting_param & 0xFF;
      if (paint_alpha & 0x80)
        paint_alpha -= 256; // 2's complement conversion
      paint_alpha = (paint_alpha > 2)?2:paint_alpha;
      paint_alpha = (paint_alpha < -2)?-2:paint_alpha;
      paint_alpha = 96 - paint_alpha*32;
      int border_alpha = paint_alpha+48;
      kdu_uint32 paint_colour, border_colour;

      if (bounding_region.area() >= 64)
        {
          switch ((painting_param >> 8) % 6) {
            case 0: paint_colour = 0x800000; break;
            case 1: paint_colour = 0x606000; break;
            case 2: paint_colour = 0x008000; break;
            case 3: paint_colour = 0x006060; break;
            case 4: paint_colour = 0x000080; break;
            case 5: paint_colour = 0x600060; break;
            };
        }
      else
        {
          switch ((painting_param >> 8) & 1) {
            case 0: paint_colour = 0xFFFFFF; break;
            case 1: paint_colour = 0x000000; break;
            };
        }

      border_colour = paint_colour;
      paint_colour |= (kdu_uint32)(paint_alpha<<24);
      border_colour |= (kdu_uint32)(border_alpha<<24);

      kdu_dims paint_region = mapped_region;
      paint_region.pos -= buffer_region.pos; // Make relative to buffer surface
      int first_region_row = (paint_region.pos.y < 0)?(-paint_region.pos.y):0;
      int first_region_col = (paint_region.pos.x < 0)?(-paint_region.pos.x):0;
      int first_buffer_row = paint_region.pos.y + first_region_row;
      int first_buffer_col = paint_region.pos.x + first_region_col;
      int num_rows = paint_region.size.y - first_region_row;
      if (num_rows > (buffer_region.size.y - first_buffer_row))
        num_rows = buffer_region.size.y - first_buffer_row;
      int num_cols = paint_region.size.x - first_region_col;
      if (num_cols > (buffer_region.size.x - first_buffer_col))
        num_cols = buffer_region.size.x - first_buffer_col;
      if ((num_rows <= 0) || (num_rows <= 0))
        continue;
      
      int m, buffer_row_gap;
      kdu_uint32 *dpp = buffer->get_buf(buffer_row_gap,false);
      dpp += first_buffer_col + first_buffer_row*buffer_row_gap;
      if (!regions->is_elliptical)
        { // Paint a simple rectangular region, with a border
          kdu_uint32 *dp, *right_lim = dpp + num_cols;
          kdu_uint32 *left_border_lim = dpp+(3-first_region_col);
          kdu_uint32 *centre_lim = dpp+(paint_region.size.x-3-first_region_col);
          if (left_border_lim > right_lim)
            left_border_lim = right_lim;
          if (centre_lim > right_lim)
            centre_lim = right_lim;
          for (m=first_region_row; num_rows > 0; num_rows--, m++,
               dpp+=buffer_row_gap, left_border_lim+=buffer_row_gap,
               centre_lim+=buffer_row_gap, right_lim+=buffer_row_gap)
            if ((m < 3) || (m >= (paint_region.size.y-3)))
              { // Painting a top or bottom border row
                for (dp=dpp; dp < right_lim; dp++)
                  *dp = border_colour;
              }
            else
              { // Painting interior rows
                for (dp=dpp; dp < left_border_lim; dp++)
                  *dp = border_colour;
                for (; dp < centre_lim; dp++)
                  *dp = paint_colour;
                for (; dp < right_lim; dp++)
                  *dp = border_colour;
              }
        }
      else
        { // Paint an elliptical region, with a border (more tricky)
          // Find half-height and half-width for the full ellipse with border
          // and the centre of the ellipse, excluding the border
          double h_full = 0.5 * (paint_region.size.y-1);
          double h_centre = 0.5 * (paint_region.size.y-7);
          double w_full = 0.5 * (paint_region.size.x-1);
          double w_centre = 0.5 * (paint_region.size.x-7);
          assert((h_centre > 0.0) && (w_centre > 0.0));
          double inv_h_full_sq = 1.0 / (h_full * h_full);
          double inv_h_centre_sq = 1.0 / (h_centre * h_centre);
          double y = first_region_row - h_full;
          for (m=first_region_row; num_rows > 0;
               num_rows--, m++, y+=1.0, dpp+=buffer_row_gap)
            {
              double x_range_full = w_full * sqrt(1.0-y*y*inv_h_full_sq);
              double x_range_centre = 1.0 - y*y*inv_h_centre_sq;
              int c1 = ((int) floor(w_full-x_range_full+0.5))
                     -first_region_col;
              int c2 = ((int) floor(w_full+x_range_full+0.5))
                     -first_region_col + 1;
              if (c1 < 0) c1 = 0;
              if (c2 > num_cols) c2 = num_cols;
              kdu_uint32 *dp = dpp + c1;
              kdu_uint32 *right_lim = dpp + c2;
              if (x_range_centre <= 0.0)
                { // Painting border only
                  for (; dp < right_lim; dp++)
                    *dp = border_colour;
                }
              else
                { // Painting border and central region
                  x_range_centre = w_centre * sqrt(x_range_centre);
                  c1 = ((int) floor(w_centre-x_range_centre+0.5))
                     -first_region_col+3;
                  c2 = ((int) floor(w_centre+x_range_centre+0.5))
                     -first_region_col+4;
                  kdu_uint32 *left_border_lim = dpp + c1;
                  kdu_uint32 *centre_lim = dpp + c2;
                  if (left_border_lim > right_lim)
                    left_border_lim = right_lim;
                  if (centre_lim > right_lim)
                    centre_lim = right_lim;
                  for (; dp < left_border_lim; dp++)
                    *dp = border_colour;
                  for (; dp < centre_lim; dp++)
                    *dp = paint_colour;
                  for (; dp < right_lim; dp++)
                    *dp = border_colour;
                }
            }
        }
    }
}

/******************************************************************************/
/*              kdu_region_compositor::donate_compositing_buffer              */
/******************************************************************************/

void
  kdu_region_compositor::donate_compositing_buffer(kdu_compositor_buf *buffer,
                                                   kdu_dims buffer_region,
                                                   kdu_coords buffer_size)
{
  assert(this->composition_buffer == NULL);
  assert(this->buffer_region == buffer_region);
  assert((active_layers != NULL) && active_layers->have_overlay_info);
  this->composition_buffer  = buffer;
  this->buffer_size = buffer_size;
}

/******************************************************************************/
/*             kdu_region_compositor::retract_compositing_buffer              */
/******************************************************************************/

bool
  kdu_region_compositor::retract_compositing_buffer(kdu_coords &buffer_size)
{
  assert(active_layers != NULL);
  if ((composition_buffer == NULL) || (active_layers->next != NULL) ||
      active_layers->have_alpha_channel || active_layers->have_overlay_info ||
      !fixed_composition_dims.is_empty())
    return false;
  composition_buffer = NULL;
  buffer_size = this->buffer_size;
  return true;
}

/******************************************************************************/
/*                   kdu_region_compositor::add_active_stream                 */
/******************************************************************************/

kdrc_stream *
  kdu_region_compositor::add_active_stream(int codestream_idx, int layer_idx,
                                           bool alpha_only)
{
  bool init_failed = false;

  if (raw_src != NULL)
    layer_idx = 0; // Open raw source as if it had a single layer, 0.

  // Look for sibling on active list first
  kdrc_stream *sibling;
  for (sibling=active_streams; sibling != NULL; sibling=sibling->next)
    if ((sibling->codestream_idx==codestream_idx) && (sibling->layer_idx >= 0))
      break;

  // Search on inactive list
  kdrc_stream *scan, *prev;
  for (prev=NULL, scan=inactive_streams; scan!=NULL; prev=scan, scan=scan->next)
    if (scan->codestream_idx == codestream_idx)
      {
        if ((sibling == NULL) && (scan->layer_idx >= 0))
          sibling = scan;
        if ((layer_idx < 0) || (scan->layer_idx == layer_idx))
          break;
      }
  if (scan != NULL)
    { // Move from inactive to active list
      if (prev == NULL)
        inactive_streams = scan->next;
      else
        prev->next = scan->next;
      scan->next = NULL;
      scan->next = active_streams;
      active_streams = scan;
      scan->is_active = true;
      if (scan->codestream == NULL)
        { KDU_ERROR_DEV(e,20); e <<
            KDU_TXT("Attempting to open a codestream which has "
            "already been found to contain an error.");
        }
      scan->codestream->move_to_head(scan);
    }
  else
    { // Create a new one
      
      // First check to see if we should remove a temporary raw code-stream
      for (prev=NULL, scan=inactive_streams;
           scan != NULL; prev=scan, scan=scan->next)
        if ((scan->layer_idx < 0) && (scan->codestream_idx == codestream_idx))
          break;
      if (scan != NULL)
        { // Remove this one; if we need it again, we can use the one we
          // are just about to create, which also has the correct layer bindings
          if (prev == NULL)
            inactive_streams = scan->next;
          else
            prev->next = scan->next;
          delete scan;
          scan = NULL;
        }

      scan = new kdrc_stream(this,persistent_codestreams,
                             codestream_cache_threshold,env,env_queue);
      scan->next = active_streams;
      active_streams = scan;
      scan->is_active = true;
      if ((layer_idx < 0) && (mj2_src == NULL))
        { // Create an isolated code-stream
          assert(raw_src == NULL); // Raw sources use `layer_idx'=0
          jpx_codestream_source stream =
            jpx_src->access_codestream(codestream_idx);
          stream.open_stream(&single_component_box);
          try {
              scan->init(&single_component_box,sibling);
            } catch (...) { init_failed = true; }
          scan->codestream_idx = codestream_idx;
          scan->layer_idx = layer_idx;
        }
      else
        {
          if (raw_src != NULL)
            {
              scan->init(raw_src,sibling);
              scan->codestream_idx = codestream_idx;
              scan->layer_idx = layer_idx;
            }
          else if (jpx_src != NULL)
            {
              jpx_codestream_source stream =
                jpx_src->access_codestream(codestream_idx);
              jpx_layer_source layer =
                jpx_src->access_layer(layer_idx);
              try {
                  scan->init(stream,layer,jpx_src,alpha_only,sibling);
                } catch (...) { init_failed = true; }
            }
          else if (mj2_src != NULL)
            {
              kdu_uint32 track_idx=0;
              int frame_idx=0, field_idx=0;
              mj2_src->find_stream(codestream_idx,track_idx,frame_idx,
                                   field_idx);
              mj2_video_source *track = mj2_src->access_video_track(track_idx);
              assert(track != NULL);
              try {
                  scan->init(track,frame_idx,field_idx,sibling);
                } catch (...) { init_failed = true; }
            }
          else
            assert(0);
        }
    }

  // Set parameters
  if (init_failed)
    {
      remove_stream(scan,true);
      single_component_box.close();
      return NULL;
    }

  scan->set_error_level(error_level);
  scan->set_max_quality_layers(max_quality_layers);

  return scan;
}

/******************************************************************************/
/*                     kdu_region_compositor::remove_stream                   */
/******************************************************************************/

void
  kdu_region_compositor::remove_stream(kdrc_stream *stream, bool permanent)
{
  bool close_single_component_box=false;
  stream->layer = NULL;
  if (stream->layer_idx < 0)
    {
      close_single_component_box = true;
      permanent = true; // Created only temporarily for single component request
    }

  // Move from active to inactive list, if necessary
  kdrc_stream *scan, *prev;
  for (prev=NULL, scan=active_streams; scan != NULL; prev=scan, scan=scan->next)
    if (scan == stream)
      {
        if (prev == NULL)
          active_streams = scan->next;
        else
          prev->next = scan->next;
        scan->next = inactive_streams;
        inactive_streams = scan;
        scan->is_active = false;
        if (scan->codestream != NULL)
          scan->codestream->move_to_tail(scan);
        break;
      }

  if (!permanent)
    return;

  // Remove from inactive list
  for (prev=NULL, scan=inactive_streams; scan!=NULL; prev=scan, scan=scan->next)
    if (scan == stream)
      {
        if (prev == NULL)
          inactive_streams = scan->next;
        else
          prev->next = scan->next;
        delete scan;
        break;
      }
  assert(scan != NULL);
  if (close_single_component_box)
    {
      assert((jpx_src != NULL) && single_component_box.exists());
      single_component_box.close();
    }
}
