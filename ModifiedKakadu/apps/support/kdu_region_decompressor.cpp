/******************************************************************************/
// File: kdu_region_decompressor.cpp [scope = APPS/SUPPORT]
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
   Implements the incremental, region-based decompression services of the
"kdu_region_decompressor" object.  These services should prove useful
to many interactive applications which require JPEG2000 rendering capabilities.
*******************************************************************************/

#include <assert.h>
#include <string.h>
#include "kdu_arch.h"
#include "kdu_utils.h"
#include "kdu_compressed.h"
#include "kdu_sample_processing.h"
#include "region_decompressor_local.h"

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
     kdu_error _name("E(kdu_region_decompressor.cpp)",_id);
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("W(kdu_region_decompressor.cpp)",_id);
#  define KDU_TXT(_string) "<#>" // Special replacement pattern
#else // !KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("Error in Kakadu Region Decompressor:\n");
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("Warning in Kakadu Region Decompressor:\n");
#  define KDU_TXT(_string) _string
#endif // !KDU_CUSTOM_TEXT

#define KDU_ERROR_DEV(_name,_id) KDU_ERROR(_name,_id)
 // Use the above version for errors which are of interest only to developers
#define KDU_WARNING_DEV(_name,_id) KDU_WARNING(_name,_id)
 // Use the above version for warnings which are of interest only to developers

// Set things up for the inclusion of assembler optimized routines
// for specific architectures.  The reason for this is to exploit
// the availability of SIMD type instructions to greatly improve the
// efficiency with which sample values can be converted, truncated and mapped.
#if ((defined KDU_PENTIUM_MSVC) && (defined _WIN64))
# undef KDU_PENTIUM_MSVC
# ifndef KDU_X86_INTRINSICS
#   define KDU_X86_INTRINSICS // Use portable intrinsics instead
# endif
#endif // KDU_PENTIUM_MSVC && _WIN64

#ifndef KDU_NO_SSE
#  if defined KDU_X86_INTRINSICS
#    define KDU_SIMD_OPTIMIZATIONS
#    include "x86_region_decompressor_local.h" // Contains SIMD intrinsics
#  elif defined KDU_PENTIUM_MSVC
#    define KDU_SIMD_OPTIMIZATIONS
#    include "msvc_region_decompressor_local.h" // Contains asm commands in-line
#  endif
#endif // !KDU_NO_SSE


/* ========================================================================== */
/*                            Internal Functions                              */
/* ========================================================================== */

/******************************************************************************/
/* STATIC                    reduce_ratio_to_ints                             */
/******************************************************************************/

static bool
  reduce_ratio_to_ints(kdu_long &num, kdu_long &den)
  /* Divides `num' and `den' by their common factors until both are reduced
     to values which can be represented as 32-bit signed integers, or else
     no further common factors can be found.  In the latter case the
     function returns false. */
{
  if ((num <= 0) || (den <= 0))
    return false;
  if ((num % den) == 0)
    { num = num / den;  den = 1; }

  kdu_long test_fac = 2;
  while ((num > 0x7FFFFFFF) || (den > 0x7FFFFFFF))
    {
      while (((num % test_fac) != 0) || ((den % test_fac) != 0))
        {
          test_fac++;
          if ((test_fac >= num) || (test_fac >= den))
            return false;
        }
      num = num / test_fac;
      den = den / test_fac;
    }
  return true;
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
  */
{
  kdu_long val, num, den;
  kdu_coords min = ref_comp_dims.pos;
  kdu_coords lim = min + ref_comp_dims.size;

  num = ref_comp_expand_numerator.x; den = ref_comp_expand_denominator.x;

  val = num*min.x;  val -= (num-1)>>1;  // Place source pixel in middle of its
  min.x = long_ceil_ratio(val,den);     // interpolated region of influence

  val = num*lim.x;  val -= (num-1)>>1;  // Place source pixel in middle of its
  lim.x = long_ceil_ratio(val,den);     // interpolated region of influence

  num = ref_comp_expand_numerator.y; den = ref_comp_expand_denominator.y;

  val = num*min.y;  val -= (num-1)>>1;  // Place source pixel in middle of its
  min.y = long_ceil_ratio(val,den);     // interpolated region of influence

  val = num*lim.y;  val -= (num-1)>>1;  // Place source pixel in middle of its
  lim.y = long_ceil_ratio(val,den);     // interpolated region of influence

  kdu_dims render_dims;
  render_dims.pos = min;
  render_dims.size = lim - min;
  return render_dims;
}

/******************************************************************************/
/* STATIC               find_conservative_ref_comp_dims                       */
/******************************************************************************/

static kdu_dims
  find_conservative_ref_comp_dims(kdu_dims render_dims,
                                  kdu_coords ref_comp_expand_numerator,
                                  kdu_coords ref_comp_expand_denominator)
  /* This function returns the location and dimension of the region on the
     reference image component which is required to render the region
     identified by `render_dims', given the rational expansion factors
     associated with rendering.  It is approximately an inverse of the
     `find_render_dims' function.  The function is used to efficiently
     assess whether or not some tiles lie outside the currently incomplete
     region when invoking the `start_tile_bank' function. */
{
  kdu_long val, num, den;
  kdu_coords min = render_dims.pos;
  kdu_coords lim = min + render_dims.size;

  num = ref_comp_expand_denominator.x;  den = ref_comp_expand_numerator.x;
  val = num*min.x;  min.x = long_floor_ratio(val,den) - 2; // Be generous
  val = num*lim.x;  lim.x = long_ceil_ratio(val,den) + 2; // Be generous

  num = ref_comp_expand_denominator.y;  den = ref_comp_expand_numerator.y;
  val = num*min.y;  min.y = long_floor_ratio(val,den) - 2; // Be generous
  val = num*lim.y;  lim.y = long_ceil_ratio(val,den) + 2; // Be generous

  kdu_dims ref_comp_dims;
  ref_comp_dims.pos = min;
  ref_comp_dims.size = lim - min;
  return ref_comp_dims;
}

/******************************************************************************/
/* STATIC                       reset_line_buf                                */
/******************************************************************************/

static void                    
  reset_line_buf(kdu_line_buf *buf)
{
  int num_samples = buf->get_width();
  if (buf->get_buf32() != NULL)
    {
      kdu_sample32 *sp = buf->get_buf32();
      while (num_samples--)
        (sp++)->ival = 0; // This function is never applied to lines whose
                          // samples are floating-point values.
    }
  else
    {
      kdu_sample16 *sp = buf->get_buf16();
      while (num_samples--)
        (sp++)->ival = 0;
    }
}

/*****************************************************************************/
/* STATIC             convert_samples_to_palette_indices                     */
/*****************************************************************************/

static void
  convert_samples_to_palette_indices(kdu_line_buf *line, int bit_depth,
                                     bool is_signed, int palette_bits,
                                     kdu_line_buf *indices,
                                     int dst_offset)
{
  int i, width=line->get_width();
  kdu_sample16 *dp = indices->get_buf16();
  assert((dp != NULL) && indices->is_absolute() &&
         (indices->get_width() >= (width+dst_offset)));
  dp += dst_offset;
  kdu_sample16 *sp16 = line->get_buf16();
  kdu_sample32 *sp32 = line->get_buf32();
  
  if (line->is_absolute())
    { // Convert absolute source integers to absolute palette indices
      if (sp16 != NULL)
        {
          kdu_int16 offset = (kdu_int16)((is_signed)?0:((1<<bit_depth)>>1));
          kdu_int16 val, mask = ((kdu_int16)(-1))<<palette_bits;
          for (i=0; i < width; i++)
            {
              val = sp16[i].ival + offset;
              if (val & mask)
                val = (val<0)?0:(~mask);
              dp[i].ival = val;
            }
        }
      else if (sp32 != NULL)
        {
          kdu_int32 offset = (is_signed)?0:((1<<bit_depth)>>1);
          kdu_int32 val, mask = ((kdu_int32)(-1))<<palette_bits;
          for (i=0; i < width; i++)
            {
              val = sp32[i].ival + offset;
              if (val & mask)
                val = (val<0)?0:(~mask);
              dp[i].ival = (kdu_int16) val;
            }
        }
      else
        assert(0);
    }
  else
    { // Convert fixed/floating point source samples to absolute palette indices
      if (sp16 != NULL)
        {
          kdu_int16 offset = (kdu_int16)((is_signed)?0:((1<<KDU_FIX_POINT)>>1));
          int downshift = KDU_FIX_POINT-palette_bits; assert(downshift > 0);
          offset += (kdu_int16)((1<<downshift)>>1);
          kdu_int16 val, mask = ((kdu_int16)(-1))<<palette_bits;
          for (i=0; i < width; i++)
            {
              val = (sp16[i].ival + offset) >> downshift;
              if (val & mask)
                val = (val<0)?0:(~mask);
              dp[i].ival = val;
            }
        }
      else if (sp32 != NULL)
        {
          float scale = (float)(1<<palette_bits);
          float offset = 0.5F + ((is_signed)?0.0F:(0.5F*scale));
          kdu_int32 val, mask = ((kdu_int32)(-1))<<palette_bits;
          for (i=0; i < width; i++)
            {
              val = (kdu_int32)((sp32[i].fval * scale) + offset);
              if (val & mask)
                val = (val<0)?0:(~mask);
              dp[i].ival = (kdu_int16) val;
            }
        }
      else
        assert(0);
    }
}

/******************************************************************************/
/* STATIC                      convert_and_copy                               */
/******************************************************************************/

static void
  convert_and_copy(kdu_line_buf *src, int bit_depth, kdu_line_buf *dst,
                   int dst_offset)
  /* This function converts raw decompressed samples in `src' to 16- or 32-bit
     fixed point output samples in `dst' of the form used to represent
     channel data in `kd_channel'.  It starts writing to the `dst' line after
     skipping over the first `dst_offset' samples.  Note that the 32-bit
     fixed point output type is a custom format used only internally by the
     `kdu_region_decompressor' object's implementation. */
{
  int i, width = src->get_width();
  kdu_sample16 *sp16 = src->get_buf16();
  kdu_sample32 *sp32 = src->get_buf32();
  kdu_sample16 *dp16 = dst->get_buf16();
  kdu_sample32 *dp32 = dst->get_buf32();
  assert((dst->get_width() >= (width+dst_offset)) && !dst->is_absolute());

  if (src->is_absolute())
    {
      int upshift = KDU_FIX_POINT - bit_depth;
      int downshift = -upshift;
      if (sp16 != NULL)
        { // Convert 16-bit absolute to 16-bit fixed point
          assert(dp16 != NULL); // There is no up-conversion to higher precision
          dp16 += dst_offset;
          if (upshift > 0)
            for (i=0; i < width; i++)
              dp16[i].ival = sp16[i].ival << upshift;
          else
            for (i=0; i < width; i++)
              dp16[i].ival = sp16[i].ival >> downshift;
        }
      else if ((sp32 != NULL) && (dp16 != NULL))
        { // Convert 32-bit absolute, to 16-bit fixed point
          dp16 += dst_offset;
          if (upshift > 0)
            for (i=0; i < width; i++)
              dp16[i].ival = (kdu_int16)(sp32[i].ival << upshift);
          else
            for (i=0; i < width; i++)
              dp16[i].ival = (kdu_int16)(sp32[i].ival >> downshift);
        }
      else if ((sp32 != NULL) && (dp32 != NULL))
        { // Convert 32-bit absolute to 32-bit fixed point
          upshift += 16;
          downshift = -upshift;
          dp32 += dst_offset;
          if (upshift > 0)
            for (i=0; i < width; i++)
              dp32[i].ival = sp32[i].ival << upshift;
          else
            for (i=0; i < width; i++)
              dp32[i].ival = sp32[i].ival >> downshift;
        }
      else
        assert(0);
    }
  else
    {
      if (sp16 != NULL)
        { // Convert 16-bit fixed point to 16-bit fixed point
          assert(dp16 != NULL); // There is no up-conversion to higher precision
          dp16 += dst_offset;
          for (i=0; i < width; i++)
            dp16[i] = sp16[i];
        }
      else if ((sp32 != NULL) && (dp16 != NULL))
        { // Convert 32-bit floating point to 16-bit fixed point
          dp16 += dst_offset;
          float fval, scale = (float)(1<<KDU_FIX_POINT);
          for (i=0; i < width; i++)
            {
              fval = sp32[i].fval * scale;
              dp16[i].ival = (fval>=0.0F)?
                ((kdu_int16)(fval+0.5F)):(-(kdu_int16)(-fval+0.5F));
            }
        }
      else if ((sp32 != NULL) && (dp32 != NULL))
        { // Convert 32-bit floating point to 32-bit fixed point
          dp32 += dst_offset;
          float fval, scale = (float)(1<<(KDU_FIX_POINT+16));
          for (i=0; i < width; i++)
            {
              fval = sp32[i].fval * scale;
              dp32[i].ival = (fval>=0.0F)?
                ((kdu_int32)(fval+0.5F)):(-(kdu_int32)(-fval+0.5F));
            }
        }
      else
        assert(0);
    }
}

/******************************************************************************/
/* STATIC                     interpolate_and_map                             */
/******************************************************************************/

static void
  interpolate_and_map(kdu_line_buf *src, int expand_counter,
                      int expand_numerator, int expand_denominator,
                      kdu_sample16 *lut, kdu_line_buf *dst,
                      int skip_cols, int num_cols)
  /* This function maps the elements of the `src' buffer through the supplied
     lookup table (the `src' samples are indices to the `lut' array) to
     produce 16-bit fixed point output samples, stored in the `dst' buffer.
     At the same time, the function interpolates the sources samples.  The
     interpolation ratio is `expand_numerator'/`expand_denominator' and the
     number of interpolated samples which are produced by the first source
     sample is given by `expand_counter'/`expand_denominator'.  It can happen
     that `expand_counter' is larger than `expand_numerator', in which case
     some extrapolation is involved at the left boundary.  Similarly, some
     extrapolation may be required at the right boundary if there are
     insufficient source samples.  If there are no source samples at all,
     they are extrapolated as 0, although this should not happen in a well
     constructed code-stream (it may happen if a tiled image is cropped
     down -- another reason to avoid tiling).
        Notionally, the line is interpolated as described above and then
     the first `skip_cols' output samples are discarded before writing the
     result into the `dst' buffer.  The number of output samples to be written
     to `dst' is given by `num_cols'. */
{
  kdu_sample16 val, *dp = dst->get_buf16();
  assert((dp != NULL) && !dst->is_absolute());
  int in_cols = src->get_width();
  if (in_cols == 0)
    { // No data available.  Synthesize 0's.
      for (; num_cols > 0; num_cols--)
        *(dp++) = lut[0];
      return;
    }

  // Create thresholds for bilinear interpolation if necessary
  int three_quarters_num, two_quarters_num, one_quarter_num;
  three_quarters_num = (3*expand_numerator) >> 2;
  two_quarters_num = expand_numerator >> 1;
  one_quarter_num = expand_numerator >> 2;

  // Adjust counters to deal with skipped samples
  expand_counter -= skip_cols * expand_denominator;
  int src_offset = 0;
  if (expand_denominator == 1)
    { // For integer expansion, `expand_counter' represents the remaining
      // replication interval for the current source pixel, expressed relative
      // to the first output location.  Each output location which lies in
      // this replication interval takes the value of the source pixel,
      // where output pixels are spaced at intervals of `expand_denominator'.
      // If `expand_counter' is non-positive, the current source pixel will
      // not be replicated into any output pixels, so we should advance the
      // source position until `expand_counter' > 0.
      while (expand_counter <= 0)
        { expand_counter += expand_numerator;  src_offset++;  in_cols--; }
    }
  else
    { // For rational expansion, `expand_counter' represents the remaining
      // interpolation interval for the current source pixel, expressed
      // relative to the first output location.  Each output location which
      // lies in this interpolation interval is obtained by interpolating
      // between the current source pixel and the previous source pixel.  To
      // find the first source pixel involved in interpolation, we advance
      // until `expand_counter' is > -`expand_numerator'.
      while (expand_counter <= -expand_numerator)
        { expand_counter += expand_numerator;  src_offset++;  in_cols--; }
    }
  if (in_cols <= 0)
    { src_offset += (in_cols-1); in_cols = 1; }

  // Now for the processing
  kdu_sample16 *sp = src->get_buf16();
  assert(sp != NULL);
  sp += src_offset;

  // Prepare the first sample
  val = lut[(sp++)->ival];  in_cols--;
  if (expand_denominator == 1)
    { // Treat the integer-expansion case specially
      for (; (num_cols>0) && (expand_counter>0); num_cols--, expand_counter--)
        *(dp++) = val; // First sample might be shorter
      int r, k = num_cols / expand_numerator;
      k = (k < in_cols)?k:in_cols;
      in_cols -= k;
      num_cols -= k*expand_numerator;
      if (expand_numerator == 1)
        for (; k > 0; k--)
          *(dp++) = lut[(sp++)->ival];
      else if (expand_numerator == 2)
        for (; k > 0; k--, dp+=2)
          {
            val = lut[(sp++)->ival];
            dp[0] = val;  dp[1] = val;
          }
      else
        for (; k > 0; k--)
          {
            val = lut[(sp++)->ival];
            for (r=expand_numerator; r--; )
              *(dp++) = val;
          }

      // Write any trailing values by reproduction
      for (sp-=((in_cols>0)?0:1); num_cols > 0; num_cols--)
        *(dp++) = lut[sp->ival];
    }
  else
    { // Generic slower implementation for rational expansion factors
      // In this case, we also do bi-linear interpolation, which
      // means that the initial value of the `expand_counter' is
      // assumed to have been selected such that the current value
      // of (`expand_counter'/`expand_numerator'-1/8) represents the
      // fraction of a pixel by which the current output sample lies
      // before `val2'.  Interpolation is between `val' and `val2'.
      kdu_sample16 val2=val;
      for (; num_cols > 0; num_cols--, expand_counter-=expand_denominator, dp++)
        {
          if ((expand_counter <= 0) && (in_cols > 0))
            {
              expand_counter += expand_numerator;
              assert(expand_counter > 0);
              val = val2;
              val2 = lut[(sp++)->ival];
              in_cols--;
            }
          if (expand_counter <= one_quarter_num)
            dp->ival = // We are at -1/8 to +1/8 of a pel w.r.t. val2
              val2.ival;
          else if (expand_counter <= two_quarters_num)
            dp->ival = // We are at -3/8 to -1/8 of a pel w.r.t. val2
              (val2.ival+((val.ival-val2.ival+2)>>2));
          else if (expand_counter < three_quarters_num)
            dp->ival = // We are at -5/8 to -3/8 of a pel w.r.t. val2
              ((val2.ival+val.ival)>>1);
          else
            dp->ival = // We are at -7/8 to -5/8 of a pel w.r.t. val2
              (val.ival+((val2.ival-val.ival+2)>>2));
        }
    }
}

/******************************************************************************/
/* STATIC                   interpolate_and_convert                           */
/******************************************************************************/

static void
  interpolate_and_convert(kdu_line_buf *src, int expand_counter,
                          int expand_numerator, int expand_denominator,
                          int bit_depth, kdu_line_buf *dst,
                          int skip_cols, int num_cols)
  /* Same as `interpolate_and_map' except that there is no lookup table and
     the source values may need to be converted, either to a 16-bit fixed point
     output data type, or to a custom 32-bit fixed point output data type. */
{
  int in_cols = src->get_width();
  kdu_sample16 *sp16 = src->get_buf16();
  kdu_sample32 *sp32 = src->get_buf32();
  kdu_sample16 *dp16 = dst->get_buf16();
  kdu_sample32 *dp32 = dst->get_buf32();
  
  assert(!dst->is_absolute());
  if (in_cols == 0)
    { // No data available.  Synthesize 0's.
      if (dp16 != NULL)
        for (; num_cols > 0; num_cols--)
          (dp16++)->ival = 0;
      else
        for (; num_cols > 0; num_cols--)
          (dp32++)->ival = 0;
      return;
    }

  // Create thresholds for bilinear interpolation if necessary
  int three_quarters_num, two_quarters_num, one_quarter_num;
  three_quarters_num = (3*expand_numerator) >> 2;
  two_quarters_num = expand_numerator >> 1;
  one_quarter_num = expand_numerator >> 2;

  // Adjust counters to deal with skipped samples
  expand_counter -= skip_cols * expand_denominator;
  int src_offset = 0;
  if (expand_denominator == 1)
    { // For integer expansion, `expand_counter' represents the remaining
      // replication interval for the current source pixel, expressed relative
      // to the first output location.  Each output location which lies in
      // this replication interval takes the value of the source pixel,
      // where output pixels are spaced at intervals of `expand_denominator'.
      // If `expand_counter' is non-positive, the current source pixel will
      // not be replicated into any output pixels, so we should advance the
      // source position until `expand_counter' > 0.
      while (expand_counter <= 0)
        { expand_counter += expand_numerator;  src_offset++;  in_cols--; }
    }
  else
    { // For rational expansion, `expand_counter' represents the remaining
      // interpolation interval for the current source pixel, expressed
      // relative to the first output location.  Each output location which
      // lies in this interpolation interval is obtained by interpolating
      // between the current source pixel and the previous source pixel.  To
      // find the first source pixel involved in interpolation, we advance
      // until `expand_counter' is > -`expand_numerator'.
      while (expand_counter <= -expand_numerator)
        { expand_counter += expand_numerator;  src_offset++;  in_cols--; }
    }
  if (in_cols <= 0)
    { src_offset += (in_cols-1); in_cols = 1; }

  // Now for the processing
  if (src->is_absolute())
    {
      int upshift = KDU_FIX_POINT - bit_depth;
      int downshift = -upshift;
      if (sp16 != NULL)
        { // Convert 16-bit absolute to 16-bit fixed point
          assert(dp16 != NULL); // There is no up-conversion to higher precision
          sp16 += src_offset;
          
          // Prepare the first sample
          kdu_int16 val = (sp16++)->ival;  in_cols--;
          val = (upshift > 0)?(val << upshift):(val >> downshift);
          if (expand_denominator == 1)
            { // Treat the integer-expansion case specially
              for (; (num_cols > 0) && (expand_counter > 0);
                   num_cols--, expand_counter--)
                (dp16++)->ival = val; // First sample might be shorter
              int r, k = num_cols / expand_numerator;
              k = (k < in_cols)?k:in_cols;
              in_cols -= k;
              num_cols -= k*expand_numerator;
              if (expand_numerator == 1)
                {
#ifdef KDU_SIMD_OPTIMIZATIONS
                  if (!simd_convert(sp16,dp16,k,upshift))
#endif // KDU_SIMD_OPTIMIZATIONS
                    {
                      if (upshift > 0)
                        for (; k > 0; k--)
                          (dp16++)->ival = ((sp16++)->ival << upshift);
                      else
                        for (; k > 0; k--)
                          (dp16++)->ival = ((sp16++)->ival >> downshift);
                    }
                }
              else if (expand_numerator == 2)
                {
#ifdef KDU_SIMD_OPTIMIZATIONS
                  if (!simd_expand_x2_and_convert(sp16,dp16,k,upshift))
#endif // KDU_SIMD_OPTIMIZATIONS
                    for (; k > 0; k--, dp16+=2)
                      {
                        val = (sp16++)->ival;
                        val = (upshift > 0)?(val << upshift):(val >> downshift);
                        dp16[0].ival = val;  dp16[1].ival = val;
                      }
                }
              else
                for (; k > 0; k--)
                  {
                    val = (sp16++)->ival;
                    val = (upshift > 0)?(val << upshift):(val >> downshift);
                    for (r=expand_numerator; r--; )
                      (dp16++)->ival = val;
                  }

              if (num_cols > 0)
                { // Write any trailing values by reproduction
                  sp16 += k; // Adjust pointers for case where conversion was
                  dp16 += k*expand_numerator; // performed by SIMD code above
                  sp16 -= (in_cols>0)?0:1;
                  val = sp16->ival;
                  val = (upshift > 0)?(val << upshift):(val >> downshift);
                  for (; num_cols > 0; num_cols--)
                    (dp16++)->ival = val;
                }
            }
          else
            { // Generic slower implementation for rational expansion factors
              // In this case, we also do bi-linear interpolation, which
              // means that the initial value of the `expand_counter' is
              // assumed to have been selected such that the current value
              // of (`expand_counter'/`expand_numerator'-1/8) represents the
              // fraction of a pixel by which the current output sample lies
              // before `val2'.  Interpolation is between `val' and `val2'.
              kdu_int16 val2=val;
              for (; num_cols > 0; num_cols--,
                   expand_counter-=expand_denominator, dp16++)
                {
                  if ((expand_counter <= 0) && (in_cols > 0))
                    {
                      expand_counter += expand_numerator;
                      assert(expand_counter > 0);
                      val = val2;
                      val2 = (sp16++)->ival;
                      val2 = (upshift>0)?(val2<<upshift):(val2>>downshift);
                      in_cols--;
                    }
                  if (expand_counter <= one_quarter_num)
                    dp16->ival = // We are at -1/8 to +1/8 of a pel w.r.t. val2
                      val2;
                  else if (expand_counter <= two_quarters_num)
                    dp16->ival = // We are at -3/8 to -1/8 of a pel w.r.t. val2
                      (val2+((val-val2+2)>>2));
                  else if (expand_counter < three_quarters_num)
                    dp16->ival = // We are at -5/8 to -3/8 of a pel w.r.t. val2
                      ((val2+val)>>1);
                  else
                    dp16->ival = // We are at -7/8 to -5/8 of a pel w.r.t. val2
                      (val+((val2-val+2)>>2));
                }
            }
        }
      else if ((sp32 != NULL) && (dp16 != NULL))
        { // Convert 32-bit absolute, to 16-bit fixed point
          sp32 += src_offset;

          // Prepare the first sample
          kdu_int32 val = (sp32++)->ival;  in_cols--;
          val = (upshift > 0)?(val << upshift):(val >> downshift);
          if (expand_denominator == 1)
            { // Treat the integer-expansion case specially
              for (; (num_cols > 0) && (expand_counter > 0);
                   num_cols--, expand_counter--)
                (dp16++)->ival = (kdu_int16) val; // First sample might be short
              int r, k = num_cols / expand_numerator;
              k = (k < in_cols)?k:in_cols;
              in_cols -= k;
              num_cols -= k*expand_numerator;
              if ((expand_numerator == 1) && (upshift > 0))
                for (; k > 0; k--)
                  (dp16++)->ival = (kdu_int16)((sp32++)->ival << upshift);
              else if (expand_numerator == 1)
                for (; k > 0; k--)
                  (dp16++)->ival = (kdu_int16)((sp32++)->ival >> downshift);
              else if (expand_numerator == 2)
                for (; k > 0; k--, dp16+=2)
                  {
                    val = (sp32++)->ival;
                    val = (upshift > 0)?(val << upshift):(val >> downshift);
                    dp16[0].ival = (kdu_int16) val;
                    dp16[1].ival = (kdu_int16) val;
                  }
              else
                for (; k > 0; k--)
                  {
                    val = (sp32++)->ival;
                    val = (upshift > 0)?(val << upshift):(val >> downshift);
                    for (r=expand_numerator; r--; )
                      (dp16++)->ival = (kdu_int16) val;
                  }

              // Write any trailing values by reproduction
              for (sp32-=((in_cols>0)?0:1); num_cols > 0; num_cols--)
                (dp16++)->ival = (kdu_int16)
                  ((upshift>0)?(sp32->ival<<upshift):(sp32->ival>>downshift));
            }
          else
            { // Generic slower implementation for rational expansion
              kdu_int32 val2=val;
              for (; num_cols > 0; num_cols--,
                   expand_counter-=expand_denominator, dp16++)
                {
                  if ((expand_counter <= 0) && (in_cols > 0))
                    {
                      expand_counter += expand_numerator;
                      assert(expand_counter > 0);
                      val = val2;
                      val2 = (sp32++)->ival;
                      val2 = (upshift>0)?(val2<<upshift):(val2>>downshift);
                      in_cols--;
                    }
                  if (expand_counter <= one_quarter_num)
                    dp16->ival = // We are at -1/8 to +1/8 of a pel w.r.t. val2
                      (kdu_int16) val2;
                  else if (expand_counter <= two_quarters_num)
                    dp16->ival = // We are at -3/8 to -1/8 of a pel w.r.t. val2
                      (kdu_int16)(val2+((val-val2+2)>>2));
                  else if (expand_counter < three_quarters_num)
                    dp16->ival = // We are at -5/8 to -3/8 of a pel w.r.t. val2
                      (kdu_int16)((val2+val)>>1);
                  else
                    dp16->ival = // We are at -7/8 to -5/8 of a pel w.r.t. val2
                      (kdu_int16)(val+((val2-val+2)>>2));
                }
            }
        }
      else if ((sp32 != NULL) && (dp32 != NULL))
        { // Convert 32-bit absolute to 32-bit fixed point
          assert((sp32 != NULL) && (dp32 != NULL));
          upshift += 16;
          downshift = -upshift;
          sp32 += src_offset;

          // Prepare the first sample
          kdu_int32 val = (sp32++)->ival;  in_cols--;
          val = (upshift > 0)?(val << upshift):(val >> downshift);
          if (expand_denominator == 1)
            { // Treat the integer-expansion case specially
              for (; (num_cols > 0) && (expand_counter > 0);
                   num_cols--, expand_counter--)
                (dp32++)->ival = val; // First sample might be shorter
              int r, k = num_cols / expand_numerator;
              k = (k < in_cols)?k:in_cols;
              in_cols -= k;
              num_cols -= k*expand_numerator;
              if ((expand_numerator == 1) && (upshift > 0))
                for (; k > 0; k--)
                  (dp32++)->ival = (sp32++)->ival << upshift;
              else if (expand_numerator == 1)
                for (; k > 0; k--)
                  (dp32++)->ival = (sp32++)->ival >> downshift;
              else if (expand_numerator == 2)
                for (; k > 0; k--, dp32+=2)
                  {
                    val = (sp32++)->ival;
                    val = (upshift > 0)?(val << upshift):(val >> downshift);
                    dp32[0].ival = val;  dp32[1].ival = val;
                  }
              else
                for (; k > 0; k--)
                  {
                    val = (sp32++)->ival;
                    val = (upshift > 0)?(val << upshift):(val >> downshift);
                    for (r=expand_numerator; r--; )
                      (dp32++)->ival = val;
                  }

              // Write any trailing values by reproduction
              for (sp32-=((in_cols>0)?0:1); num_cols > 0; num_cols--)
                (dp32++)->ival =
                  ((upshift>0)?(sp32->ival<<upshift):(sp32->ival>>downshift));
            }
          else
            { // Slower, more general implementation for rational expansion
              kdu_int32 val2=val;
              for (; num_cols > 0; num_cols--,
                   expand_counter-=expand_denominator, dp32++)
                {
                  if ((expand_counter <= 0) && (in_cols > 0))
                    {
                      expand_counter += expand_numerator;
                      assert(expand_counter > 0);
                      val = val2;
                      val2 = (sp32++)->ival;
                      val2 = (upshift>0)?(val2<<upshift):(val2>>downshift);
                      in_cols--;
                    }
                  if (expand_counter <= one_quarter_num)
                    dp32->ival = // We are at -1/8 to +1/8 of a pel w.r.t. val2
                      val2;
                  else if (expand_counter <= two_quarters_num)
                    dp32->ival = // We are at -3/8 to -1/8 of a pel w.r.t. val2
                      (val2+((val-val2+2)>>2));
                  else if (expand_counter < three_quarters_num)
                    dp32->ival = // We are at -5/8 to -3/8 of a pel w.r.t. val2
                      ((val2+val)>>1);
                  else
                    dp32->ival = // We are at -7/8 to -5/8 of a pel w.r.t. val2
                      (val+((val2-val+2)>>2));
                }
            }
        }
      else
        assert(0);
    }
  else
    {
      if (sp16 != NULL)
        { // Convert 16-bit fixed point to 16-bit fixed point
          assert(dp16 != NULL); // There is no up-conversion to higher precision
          sp16 += src_offset;

          // Prepare the first sample
          kdu_sample16 val = *(sp16++);  in_cols--;
          if (expand_denominator == 1)
            { // Treat the integer-expansion case specially
              for (; (num_cols > 0) && (expand_counter > 0);
                   num_cols--, expand_counter--)
                *(dp16++) = val; // First sample might be shorter
              int r, k = num_cols / expand_numerator;
              k = (k < in_cols)?k:in_cols;
              in_cols -= k;
              num_cols -= k*expand_numerator;
              if (expand_numerator == 1)
                {
#ifdef KDU_SIMD_OPTIMIZATIONS
                  if (!simd_convert(sp16,dp16,k,0))
#endif // KDU_SIMD_OPTIMIZATIONS
                    for (; k > 0; k--)
                      *(dp16++) = *(sp16++);
                }
              else if (expand_numerator == 2)
                {
#ifdef KDU_SIMD_OPTIMIZATIONS
                  if (!simd_expand_x2_and_convert(sp16,dp16,k,0))
#endif // KDU_SIMD_OPTIMIZATIONS
                    for (; k > 0; k--, dp16+=2)
                      {
                        val = *(sp16++);
                        dp16[0] = val;  dp16[1] = val;
                      }
                }
              else
                for (; k > 0; k--)
                  {
                    val = *(sp16++);
                    for (r=expand_numerator; r--; )
                      *(dp16++) = val;
                  }

              // Write any trailing values by reproduction
              if (num_cols > 0)
                { // Write any trailing values by reproduction
                  sp16 += k; // Adjust pointers for case where conversion was
                  dp16 += k*expand_numerator; // performed by SIMD code above
                  sp16 -= (in_cols>0)?0:1;
                  val = *sp16;
                  for (; num_cols > 0; num_cols--)
                    *(dp16++) = val;
                }
            }
          else
            { // Generic slower implementation for rational expansion factors
              kdu_sample16 val2=val;
              for (; num_cols > 0; num_cols--,
                   expand_counter-=expand_denominator, dp16++)
                {
                  if ((expand_counter <= 0) && (in_cols > 0))
                    {
                      expand_counter += expand_numerator;
                      assert(expand_counter > 0);
                      val = val2;
                      val2 = *(sp16++);
                      in_cols--;
                    }
                  if (expand_counter <= one_quarter_num)
                    dp16->ival = // We are at -1/8 to +1/8 of a pel w.r.t. val2
                      val2.ival;
                  else if (expand_counter <= two_quarters_num)
                    dp16->ival = // We are at -3/8 to -1/8 of a pel w.r.t. val2
                      (val2.ival+((val.ival-val2.ival+2)>>2));
                  else if (expand_counter < three_quarters_num)
                    dp16->ival = // We are at -5/8 to -3/8 of a pel w.r.t. val2
                      ((val2.ival+val.ival)>>1);
                  else
                    dp16->ival = // We are at -7/8 to -5/8 of a pel w.r.t. val2
                      (val.ival+((val2.ival-val.ival+2)>>2));
                }
            }
        }
      else if ((sp32 != NULL) && (dp16 != NULL))
        { // Convert 32-bit floating point to 16-bit fixed point
          sp32 += src_offset;
          float fval, scale = (float)(1<<KDU_FIX_POINT);

          // Prepare the first sample
          kdu_sample16 val;
          fval = (sp32++)->fval * scale;  in_cols--;
          val.ival =
            (fval>=0.0F)?((kdu_int16)(fval+0.5F)):(-(kdu_int16)(-fval+0.5F));
          if (expand_denominator == 1)
            { // Treat the integer-expansion case specially
              for (; (num_cols > 0) && (expand_counter > 0);
                   num_cols--, expand_counter--)
                *(dp16++) = val; // First sample might be shorter
              int r, k = num_cols / expand_numerator;
              k = (k < in_cols)?k:in_cols;
              in_cols -= k;
              num_cols -= k*expand_numerator;
              if (expand_numerator == 1)
                for (; k > 0; k--)
                  {
                    fval = (sp32++)->fval * scale;
                    (dp16++)->ival = (fval>=0.0F)?
                      ((kdu_int16)(fval+0.5F)):(-(kdu_int16)(-fval+0.5F));
                  }
              else if (expand_numerator == 2)
                for (; k > 0; k--, dp16+=2)
                  {
                    fval = (sp32++)->fval * scale;
                    val.ival = (fval>=0.0F)?
                      ((kdu_int16)(fval+0.5F)):(-(kdu_int16)(-fval+0.5F));
                    dp16[0] = val;  dp16[1] = val;
                  }
              else
                for (; k > 0; k--)
                  {
                    fval = (sp32++)->fval * scale;
                    val.ival = (fval>=0.0F)?
                      ((kdu_int16)(fval+0.5F)):(-(kdu_int16)(-fval+0.5F));
                    for (r=expand_numerator; r--; )
                      *(dp16++) = val;
                  }

              // Write any trailing values by reproduction
              for (sp32-=((in_cols>0)?0:1); num_cols > 0; num_cols--)
                {
                  fval = sp32->fval * scale;  in_cols--;
                  val.ival = (fval>=0.0F)?
                    ((kdu_int16)(fval+0.5F)):(-(kdu_int16)(-fval+0.5F));
                  *(dp16++) = val;
                }
            }
          else
            { // Slower more general implementation for rational expansion
              kdu_sample16 val2=val;
              for (; num_cols > 0; num_cols--,
                   expand_counter-=expand_denominator, dp16++)
                {
                  if ((expand_counter <= 0) && (in_cols > 0))
                    {
                      expand_counter += expand_numerator;
                      assert(expand_counter > 0);
                      fval = (sp32++)->fval * scale;
                      val = val2;
                      val2.ival = (fval>=0.0F)?
                        ((kdu_int16)(fval+0.5F)):(-(kdu_int16)(-fval+0.5F));
                      in_cols--;
                    }
                  if (expand_counter <= one_quarter_num)
                    dp16->ival = // We are at -1/8 to +1/8 of a pel w.r.t. val2
                      val2.ival;
                  else if (expand_counter <= two_quarters_num)
                    dp16->ival = // We are at -3/8 to -1/8 of a pel w.r.t. val2
                      (val2.ival+((val.ival-val2.ival+2)>>2));
                  else if (expand_counter < three_quarters_num)
                    dp16->ival = // We are at -5/8 to -3/8 of a pel w.r.t. val2
                      ((val2.ival+val.ival)>>1);
                  else
                    dp16->ival = // We are at -7/8 to -5/8 of a pel w.r.t. val2
                      (val.ival+((val2.ival-val.ival+2)>>2));
                }
            }
        }
      else if ((sp32 != NULL) && (dp32 != NULL))
        { // Convert 32-bit floating point to 32-bit fixed point
          sp32 += src_offset;
          float fval, scale = (float)(1<<(KDU_FIX_POINT+16));

          // Prepare the first sample
          kdu_sample32 val;
          fval = (sp32++)->fval * scale;  in_cols--;
          val.ival =
            (fval>=0.0F)?((kdu_int32)(fval+0.5F)):(-(kdu_int32)(-fval+0.5F));
          if (expand_denominator == 1)
            { // Treat the integer-expansion case specially
              for (; (num_cols > 0) && (expand_counter > 0);
                   num_cols--, expand_counter--)
                *(dp32++) = val; // First sample might be shorter
              int r, k = num_cols / expand_numerator;
              k = (k < in_cols)?k:in_cols;
              in_cols -= k;
              num_cols -= k*expand_numerator;
              if (expand_numerator == 1)
                for (; k > 0; k--)
                  {
                    fval = (sp32++)->fval * scale;
                    (dp32++)->ival = (fval>=0.0F)?
                      ((kdu_int32)(fval+0.5F)):(-(kdu_int32)(-fval+0.5F));
                  }
              else if (expand_numerator == 2)
                for (; k > 0; k--, dp32+=2)
                  {
                    fval = (sp32++)->fval * scale;
                    val.ival = (fval>=0.0F)?
                      ((kdu_int32)(fval+0.5F)):(-(kdu_int32)(-fval+0.5F));
                    dp32[0] = val;  dp32[1] = val;
                  }
              else
                for (; k > 0; k--)
                  {
                    fval = (sp32++)->fval * scale;
                    val.ival = (fval>=0.0F)?
                      ((kdu_int32)(fval+0.5F)):(-(kdu_int32)(-fval+0.5F));
                    for (r=expand_numerator; r--; )
                      *(dp32++) = val;
                  }

              // Write any trailing values by reproduction
              for (sp32-=((in_cols>0)?0:1); num_cols > 0; num_cols--)
                {
                  fval = sp32->fval * scale;  in_cols--;
                  val.ival = (fval>=0.0F)?
                    ((kdu_int32)(fval+0.5F)):(-(kdu_int32)(-fval+0.5F));
                  *(dp32++) = val;
                }
            }
          else
            { // Slower more general implementation for rational expansion
              kdu_sample32 val2=val;
              for (; num_cols > 0; num_cols--,
                   expand_counter-=expand_denominator, dp32++)
                {
                  if ((expand_counter <= 0) && (in_cols > 0))
                    {
                      expand_counter += expand_numerator;
                      assert(expand_counter > 0);
                      fval = (sp32++)->fval * scale;
                      val = val2;
                      val2.ival = (fval>=0.0F)?
                        ((kdu_int32)(fval+0.5F)):(-(kdu_int32)(-fval+0.5F));
                      in_cols--;
                    }
                  if (expand_counter <= one_quarter_num)
                    dp32->ival = // We are at -1/8 to +1/8 of a pel w.r.t. val2
                      val2.ival;
                  else if (expand_counter <= two_quarters_num)
                    dp32->ival = // We are at -3/8 to -1/8 of a pel w.r.t. val2
                      (val2.ival+((val.ival-val2.ival+2)>>2));
                  else if (expand_counter < three_quarters_num)
                    dp32->ival = // We are at -5/8 to -3/8 of a pel w.r.t. val2
                      ((val2.ival+val.ival)>>1);
                  else
                    dp32->ival = // We are at -7/8 to -5/8 of a pel w.r.t. val2
                      (val.ival+((val2.ival-val.ival+2)>>2));
                }
            }
        }
      else
        assert(0);
    }
}

/******************************************************************************/
/* STATIC                       interpolate_fix32                             */
/******************************************************************************/

static void
  interpolate_fix32(kdu_line_buf *src, int expand_counter,
                    int expand_numerator, int expand_denominator,
                    kdu_line_buf *dst, int skip_cols, int num_cols)
  /* Same as `interpolate_and_convert' except that no conversion is required.
     This function is used only if the source data already has a fixed-point
     32-bit representation.  It can have this kind of representation prior to
     interpolation only if the conversion from decompressed data has been
     applied previously while multi-tile data was being transferred to a
     single component line store for the entire tile-bank -- this is done
     by the `convert_and_copy' function, as invoked from
     `kdu_region_decompressor::process_generic'. */
{
  int in_cols = src->get_width();
  kdu_sample32 *sp32 = src->get_buf32();
  kdu_sample32 *dp32 = dst->get_buf32();
  assert((sp32 != NULL) && (dp32 != NULL) &&
         !(dst->is_absolute() || src->is_absolute()));
  if (in_cols == 0)
    { // No data available.  Synthesize 0's.
      for (; num_cols > 0; num_cols--)
        (dp32++)->ival = 0;
      return;
    }

  // Create thresholds for bilinear interpolation if necessary
  int three_quarters_num, two_quarters_num, one_quarter_num;
  three_quarters_num = (3*expand_numerator) >> 2;
  two_quarters_num = expand_numerator >> 1;
  one_quarter_num = expand_numerator >> 2;

  // Adjust counters to deal with skipped samples
  expand_counter -= skip_cols * expand_denominator;
  int src_offset = 0;
  if (expand_denominator == 1)
    { // For integer expansion, `expand_counter' represents the remaining
      // replication interval for the current source pixel, expressed relative
      // to the first output location.  Each output location which lies in
      // this replication interval takes the value of the source pixel,
      // where output pixels are spaced at intervals of `expand_denominator'.
      // If `expand_counter' is non-positive, the current source pixel will
      // not be replicated into any output pixels, so we should advance the
      // source position until `expand_counter' > 0.
      while (expand_counter <= 0)
        { expand_counter += expand_numerator;  src_offset++;  in_cols--; }
    }
  else
    { // For rational expansion, `expand_counter' represents the remaining
      // interpolation interval for the current source pixel, expressed
      // relative to the first output location.  Each output location which
      // lies in this interpolation interval is obtained by interpolating
      // between the current source pixel and the previous source pixel.  To
      // find the first source pixel involved in interpolation, we advance
      // until `expand_counter' is > -`expand_numerator'.
      while (expand_counter <= -expand_numerator)
        { expand_counter += expand_numerator;  src_offset++;  in_cols--; }
    }
  if (in_cols <= 0)
    { src_offset += (in_cols-1); in_cols = 1; }

  // Now for the processing
  sp32 += src_offset;

  // Prepare the first sample
  kdu_sample32 val = *(sp32++);  in_cols--;
  if (expand_denominator == 1)
    { // Treat the integer-expansion case specially
      for (; (num_cols>0) && (expand_counter>0); num_cols--, expand_counter--)
        *(dp32++) = val; // First sample might be shorter
      int r, k = num_cols / expand_numerator;
      k = (k < in_cols)?k:in_cols;
      in_cols -= k;
      num_cols -= k*expand_numerator;
      if (expand_numerator == 1)
        for (; k > 0; k--)
          *(dp32++) = *(sp32++);
      else if (expand_numerator == 2)
        for (; k > 0; k--, dp32+=2)
          {
            val = *(sp32++);
            dp32[0] = val;  dp32[1] = val;
          }
      else
        for (; k > 0; k--)
          {
            val = *(sp32++);
            for (r=expand_numerator; r--; )
              *(dp32++) = val;
          }

        // Write any trailing values by reproduction
        if (num_cols > 0)
          { // Write any trailing values by reproduction
            sp32 -= (in_cols>0)?0:1;
            val = *sp32;
            for (; num_cols > 0; num_cols--)
              *(dp32++) = val;
          }
    }
  else
    { // Generic slower implementation for rational expansion factors
      kdu_sample32 val2=val;
      for (; num_cols > 0; num_cols--,
           expand_counter-=expand_denominator, dp32++)
        {
          if ((expand_counter <= 0) && (in_cols > 0))
            {
              expand_counter += expand_numerator;
              assert(expand_counter > 0);
              val = val2;
              val2 = *(sp32++);
              in_cols--;
            }
          if (expand_counter <= one_quarter_num)
            dp32->ival = // We are at -1/8 to +1/8 of a pel w.r.t. val2
              val2.ival;
          else if (expand_counter <= two_quarters_num)
            dp32->ival = // We are at -3/8 to -1/8 of a pel w.r.t. val2
              (val2.ival+((val.ival-val2.ival+2)>>2));
          else if (expand_counter < three_quarters_num)
            dp32->ival = // We are at -5/8 to -3/8 of a pel w.r.t. val2
              ((val2.ival+val.ival)>>1);
          else
            dp32->ival = // We are at -7/8 to -5/8 of a pel w.r.t. val2
              (val.ival+((val2.ival-val.ival+2)>>2));
        }
    }
}

/******************************************************************************/
/* STATIC                    apply_white_stretch                              */
/******************************************************************************/

static void
  apply_white_stretch(kdu_line_buf *line, int num_cols,
                      kdu_uint16 stretch_residual)
{
  assert(num_cols <= line->get_width());
  if (line->get_buf16() != NULL)
    {
      kdu_sample16 *sp16 = line->get_buf16();
#ifdef KDU_SIMD_OPTIMIZATIONS
      if (!simd_apply_white_stretch(sp16,num_cols,stretch_residual))
#endif // KDU_SIMD_OPTIMIZATIONS
        {
          kdu_int32 val, stretch_factor=stretch_residual;
          kdu_int32 offset = -((-(stretch_residual<<(KDU_FIX_POINT-1))) >> 16);
          for (; num_cols > 0; num_cols--, sp16++)
            {
              val = sp16->ival;
              val += ((val*stretch_factor)>>16) + offset;
              sp16->ival = (kdu_int16) val;
            }
        }
    }
  else
    {
      kdu_sample32 *sp32 = line->get_buf32();
      kdu_int32 val, stretch_factor=stretch_residual;
      kdu_int32 offset = stretch_residual << (KDU_FIX_POINT-1);
      for (; num_cols > 0; num_cols--, sp32++)
        {
          val = sp32->ival;
          sp32->ival += (val>>16) * stretch_factor + offset;
        }
    }
}

/******************************************************************************/
/* STATIC                  interpolate_between_lines                          */
/******************************************************************************/

static void
  interpolate_between_lines(kdu_line_buf *src1, kdu_line_buf *src2,
                            kdu_line_buf *dst, int num_cols,
                            int expand_counter, int expand_numerator)
{
  // Create thresholds for bilinear interpolation
  int three_quarters_num, two_quarters_num, one_quarter_num;
  three_quarters_num = (3*expand_numerator) >> 2;
  two_quarters_num = expand_numerator >> 1;
  one_quarter_num = expand_numerator >> 2;

  if (dst->get_buf16() != NULL)
    {
      kdu_sample16 *sp1=src1->get_buf16();
      kdu_sample16 *sp2=src2->get_buf16();
      kdu_sample16 *dp=dst->get_buf16();
      assert((sp1 != NULL) && (sp2 != NULL) && (dp != NULL));
      if (expand_counter <= one_quarter_num)
        { // We are at -1/8 to +1/8 of a pel w.r.t. val2
          for (; num_cols > 0; num_cols--, sp2++, dp++)
            dp->ival = sp2->ival;
        }
      else if (expand_counter <= two_quarters_num)
        { // We are at -3/8 to -1/8 of a pel w.r.t. val2
          for (; num_cols > 0; num_cols--, sp1++, sp2++, dp++)
            dp->ival = (sp2->ival+((sp1->ival-sp2->ival+2)>>2));
        }
      else if (expand_counter < three_quarters_num)
        { // We are at -5/8 to -3/8 of a pel w.r.t. val2
          for (; num_cols > 0; num_cols--, sp1++, sp2++, dp++)
            dp->ival = ((sp2->ival+sp1->ival)>>1);
        }
      else
        { // We are at -7/8 to -5/8 of a pel w.r.t. val2
          for (; num_cols > 0; num_cols--, sp1++, sp2++, dp++)
            dp->ival = (sp1->ival+((sp2->ival-sp1->ival+2)>>2));
        }
    }
  else
    {
      kdu_sample32 *sp1=src1->get_buf32();
      kdu_sample32 *sp2=src2->get_buf32();
      kdu_sample32 *dp=dst->get_buf32();
      assert((sp1 != NULL) && (sp2 != NULL) && (dp != NULL));
      if (expand_counter <= one_quarter_num)
        { // We are at -1/8 to +1/8 of a pel w.r.t. val2
          for (; num_cols > 0; num_cols--, sp2++, dp++)
            dp->ival = sp2->ival;
        }
      else if (expand_counter <= two_quarters_num)
        { // We are at -3/8 to -1/8 of a pel w.r.t. val2
          for (; num_cols > 0; num_cols--, sp1++, sp2++, dp++)
            dp->ival = (sp2->ival+((sp1->ival-sp2->ival+2)>>2));
        }
      else if (expand_counter < three_quarters_num)
        { // We are at -5/8 to -3/8 of a pel w.r.t. val2
          for (; num_cols > 0; num_cols--, sp1++, sp2++, dp++)
            dp->ival = ((sp2->ival+sp1->ival)>>1);
        }
      else
        { // We are at -7/8 to -5/8 of a pel w.r.t. val2
          for (; num_cols > 0; num_cols--, sp1++, sp2++, dp++)
            dp->ival = (sp1->ival+((sp2->ival-sp1->ival+2)>>2));
        }
    }
}

/******************************************************************************/
/* STATIC                 transfer_fixed_point (bytes)                        */
/******************************************************************************/

static void
  transfer_fixed_point(kdu_line_buf *src, int num_samples, int gap,
                       kdu_byte *dest, int precision_bits, bool leave_signed)
  /* Transfers source samples from the supplied line buffer into the output
     byte buffer, spacing successive output samples apart by `gap' bytes.
     Only `num_samples' samples are transferred.  The source data has either a
     16-bit fixed point representation, with KDU_FIX_POINT fraction bits, or a
     32-bit fixed point representation, with KDU_FIX_POINT+16 fraction bits.
     `precision_bits' indicates the precision of the unsigned integers to be
     written into the `dest' bytes. */
{
  assert(num_samples <= src->get_width());
  if (src->get_buf16() != NULL)
    { // Transferring from 16-bit source buffers
      kdu_sample16 *sp = src->get_buf16();
      assert((sp != NULL) && !src->is_absolute());
      if (precision_bits <= 8)
        { // Normal case, in which we want to fit the data completely within
          // the 8-bit output samples
          int downshift = KDU_FIX_POINT-precision_bits;
          kdu_int16 val, offset, mask;
          offset = (1<<downshift)>>1; // Rounding offset for the downshift
          offset += (1<<KDU_FIX_POINT)>>1; // Convert from signed to unsigned
          mask = (kdu_int16)((-1) << precision_bits);
          if (leave_signed)
            { // Unusual case in which we want a signed result
              kdu_int16 post_offset = (kdu_int16)((1<<precision_bits)>>1);
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = (sp->ival+offset)>>downshift;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_byte)(val-post_offset);
                }
            }
          else
#ifdef KDU_SIMD_OPTIMIZATIONS
            if ((gap != 1) ||
                !simd_transfer_to_consecutive_bytes(sp,dest,num_samples,offset,
                                                    downshift,mask))
#endif // KDU_SIMD_OPTIMIZATIONS
            { // Typical case of conversion to 8-bit result
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = (sp->ival+offset)>>downshift;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_byte) val;
                }
            }
        }
      else
        { // Unusual case in which the output precision is insufficient.  In
          // this case, clipping should be based upon the 8-bit output
          // precision after conversion to a signed or unsigned representation
          // as appropriate.
          int upshift=0, downshift = KDU_FIX_POINT-precision_bits;
          if (downshift < 0)
            { upshift=-downshift; downshift=0; }
          kdu_int16 min, max, val, offset=(kdu_int16)((1<<downshift)>>1);
          if (leave_signed)
            {
              min = (kdu_int16)(-128>>upshift);
              max = (kdu_int16)(127>>upshift);
            }
          else
            {
              offset += (1<<KDU_FIX_POINT)>>1;
              min = 0;  max = (kdu_int16)(255>>upshift);
            }
          for (; num_samples > 0; num_samples--, sp++, dest+=gap)
            {
              val = (sp->ival+offset) >> downshift;
              if (val < min)
                val = min;
              else if (val > max)
                val = max;
              val <<= upshift;
              *dest = (kdu_byte) val;
            }
        }
    }
  else
    { // Transferring from 32-bit source buffers
      kdu_sample32 *sp = src->get_buf32();
      assert((sp != NULL) && !src->is_absolute());
      if (precision_bits <= 8)
        { // Normal case, in which we want to fit the data completely within
          // the 8-bit output samples
          int downshift = KDU_FIX_POINT+16-precision_bits;
          kdu_int32 val, offset, mask;
          offset = (1<<downshift)>>1; // Rounding offset for the downshift
          offset += (1<<(KDU_FIX_POINT+16))>>1;// Convert signed to unsigned
          mask = (kdu_int32)((-1) << precision_bits);
          if (leave_signed)
            {
              kdu_int32 post_offset = ((1<<precision_bits)>>1);
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = (sp->ival+offset)>>downshift;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_byte)(val-post_offset);
                }
            }
          else
            { // Typical case of conversion to unsigned result
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = (sp->ival+offset)>>downshift;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_byte) val;
                }
            }
        }
      else
        { // Unusual case in which the output precision is insufficient.  In
          // this case, clipping should be based upon the 8-bit output
          // precision after conversion to a signed or unsigned representation
          // as appropriate.
          int upshift=0, downshift = KDU_FIX_POINT+16-precision_bits;
          if (downshift < 0)
            { upshift=-downshift; downshift=0; }
          kdu_int32 min, max, val, offset=(kdu_int32)((1<<downshift)>>1);
          if (leave_signed)
            {
              min = (-128>>upshift);
              max = (127>>upshift);
            }
          else
            {
              offset += (1<<KDU_FIX_POINT)>>1;
              min = 0;  max = (255>>upshift);
            }
          for (; num_samples > 0; num_samples--, sp++, dest+=gap)
            {
              val = (sp->ival+offset) >> downshift;
              if (val < min)
                val = min;
              else if (val > max)
                val = max;
              val <<= upshift;
              *dest = (kdu_byte) val;
            }
        }
    }
}

/******************************************************************************/
/* STATIC                 transfer_fixed_point (words)                        */
/******************************************************************************/

static void
  transfer_fixed_point(kdu_line_buf *src, int num_samples, int gap,
                       kdu_uint16 *dest, int precision_bits,
                       bool leave_signed)
  /* Transfers source samples from the supplied line buffer into the output
     word buffer, spacing successive output samples apart by `gap' words.
     Only `num_samples' samples are transferred.  The source data has either a
     16-bit fixed point representation, with KDU_FIX_POINT fraction bits, or a
     32-bit fixed point representation, with KDU_FIX_POINT+16 fraction bits.
     `precision_bits' indicates the precision of the unsigned integers to be
     written into the `dest' words. */
{
  assert(num_samples <= src->get_width());
  if (src->get_buf16() != NULL)
    {
      kdu_sample16 *sp = src->get_buf16();
      assert((sp != NULL) && !src->is_absolute());
      int downshift = KDU_FIX_POINT-precision_bits;
      kdu_int16 val, offset, mask;
      if (downshift >= 0)
        {
          offset = (1<<downshift)>>1; // Rounding offset for the downshift
          offset += (1<<KDU_FIX_POINT)>>1; // Conversion from signed to unsigned
          mask = (kdu_int16)((-1) << precision_bits);
          if (leave_signed)
            {
              kdu_int16 post_offset = (kdu_int16)((1<<precision_bits)>>1);
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = (sp->ival+offset)>>downshift;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_uint16)(val-post_offset);
                }
            }
          else
            { // Typical case
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = (sp->ival+offset)>>downshift;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_uint16) val;
                }
            }
        }
      else if (precision_bits <= 16)
        { // Need to upshift, but result still fits in 16 bits
          int upshift = -downshift;
          mask = (kdu_int16)((-1) << KDU_FIX_POINT); // Apply the mask first
          offset = (1<<KDU_FIX_POINT)>>1; // Conversion from signed to unsigned
          if (leave_signed)
            {
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = sp->ival+offset;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_uint16)((val-offset) << upshift);
                }
            }
          else
            {
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = sp->ival+offset;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_uint16)(val << upshift);
                }
            }
        }
      else
        { // Unusual case in which the output precision is insufficient.  In
          // this case, clipping should be based upon the 16-bit output
          // precision after conversion to a signed or unsigned representation
          // as appropriate.
          int upshift = -downshift;
          kdu_int32 min, max, val, offset=0;
          if (leave_signed)
            {
              min = (-(1<<15)) >> upshift;
              max = ((1<<15)-1) >> upshift;
            }
          else
            {
              offset += (1<<KDU_FIX_POINT)>>1;
              min = 0;  max = ((1<<16)-1) >> upshift;
            }
          for (; num_samples > 0; num_samples--, sp++, dest+=gap)
            {
              val = sp->ival + offset;
              if (val < min)
                val = min;
              else if (val > max)
                val = max;
              val <<= upshift;
              *dest = (kdu_uint16) val;
            }
        }
    }
  else
    { // Transferring from 32-bit source buffers
      kdu_sample32 *sp = src->get_buf32();
      assert((sp != NULL) && !src->is_absolute());
      if (precision_bits <= 16)
        { // Normal case, in which we want to fit the data completely within
          // the 16-bit output samples
          int downshift = KDU_FIX_POINT+16-precision_bits;
          kdu_int32 val, offset, mask;
          offset = (1<<downshift)>>1; // Rounding offset for the downshift
          offset += (1<<(KDU_FIX_POINT+16))>>1;// Convert signed to unsigned
          mask = (kdu_int32)((-1) << precision_bits);
          if (leave_signed)
            {
              kdu_int32 post_offset = (1<<precision_bits)>>1;
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = (sp->ival+offset)>>downshift;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_uint16)(val-post_offset);
                }
            }
          else
            { // Typical case
              for (; num_samples > 0; num_samples--, sp++, dest+=gap)
                {
                  val = (sp->ival+offset)>>downshift;
                  if (val & mask)
                    val = (val < 0)?0:~mask;
                  *dest = (kdu_uint16) val;
                }
            }
        }
      else
        { // Unusual case in which the output precision is insufficient.  In
          // this case, clipping should be based upon the 16-bit output
          // precision after conversion to a signed or unsigned representation
          // as appropriate.
          int upshift=0, downshift = KDU_FIX_POINT+16-precision_bits;
          if (downshift < 0)
            { upshift=-downshift; downshift=0; }
          kdu_int32 min, max, val, offset=(kdu_int32)((1<<downshift)>>1);
          if (leave_signed)
            {
              min = (-(1<<15)) >> upshift;
              max = ((1<<15)-1) >> upshift;
            }
          else
            {
              offset += (1<<KDU_FIX_POINT)>>1;
              min = 0;  max = ((1<<16)-1) >> upshift;
            }
          for (; num_samples > 0; num_samples--, sp++, dest+=gap)
            {
              val = (sp->ival+offset) >> downshift;
              if (val < min)
                val = min;
              else if (val > max)
                val = max;
              val <<= upshift;
              *dest = (kdu_uint16) val;
            }
        }
    }
}


/* ========================================================================== */
/*                           kdu_channel_mapping                             */
/* ========================================================================== */

/******************************************************************************/
/*                 kdu_channel_mapping::kdu_channel_mapping                   */
/******************************************************************************/

kdu_channel_mapping::kdu_channel_mapping()
{
  num_channels = 0;
  source_components = NULL; palette = NULL;
  default_rendering_precision = NULL;
  default_rendering_signed = NULL;
  clear();
}

/******************************************************************************/
/*                        kdu_channel_mapping::clear                          */
/******************************************************************************/

void
  kdu_channel_mapping::clear()
{
  if (palette != NULL)
    {
      for (int c=0; c < num_channels; c++)
        if (palette[c] != NULL)
          delete[] palette[c];
      delete[] palette;
    }
  palette = NULL;
  if (source_components != NULL)
    delete[] source_components;
  source_components = NULL;
  if (default_rendering_precision != NULL)
    delete[] default_rendering_precision;
  default_rendering_precision = NULL;
  if (default_rendering_signed != NULL)
    delete[] default_rendering_signed;
  default_rendering_signed = NULL;
  num_channels = num_colour_channels = 0; palette_bits = 0;
  colour_converter.clear();
}

/******************************************************************************/
/*                   kdu_channel_mapping::set_num_channels                    */
/******************************************************************************/

void
  kdu_channel_mapping::set_num_channels(int num)
{
  assert(num >= 0);
  if (num_channels >= num)
    {
      num_channels = num;
      return;
    }
  
  int *old_comps = source_components;
  int *old_precs = default_rendering_precision;
  bool *old_signed = default_rendering_signed;
  source_components = new int[num];
  default_rendering_precision = new int[num];
  default_rendering_signed = new bool[num];
  int c = 0;
  if (old_comps != NULL)
    {
      for (c=0; (c < num_channels) && (c < num); c++)
        {
          source_components[c] = old_comps[c];
          default_rendering_precision[c] = old_precs[c];
          default_rendering_signed[c] = old_signed[c];
        }
      delete[] old_comps;
      delete[] old_precs;
      delete[] old_signed;
    }
  for (; c < num; c++)
    {
      source_components[c] = -1;
      default_rendering_precision[c] = 8;
      default_rendering_signed[c] = false;
    }

  kdu_sample16 **old_palette = palette;
  palette = new kdu_sample16 *[num];
  c = 0;
  if (old_palette != NULL)
    {
      for (c=0; (c < num_channels) && (c < num); c++)
        palette[c] = old_palette[c];
      for (int d=c; d < num_channels; d++)
        if (old_palette[d] != NULL)
          delete[] old_palette[d];
      delete[] old_palette;
    }
  for (; c < num; c++)
    palette[c] = NULL;

  num_channels = num;
}

/******************************************************************************/
/*                    kdu_channel_mapping::configure (raw)                    */
/******************************************************************************/

bool
  kdu_channel_mapping::configure(kdu_codestream codestream)
{
  clear();
  set_num_channels((codestream.get_num_components(true) >= 3)?3:1);
  kdu_coords ref_subs; codestream.get_subsampling(0,ref_subs,true);
  int c;
  for (c=0; c < num_channels; c++)
    {
      source_components[c] = c;
      default_rendering_precision[c] = codestream.get_bit_depth(c,true);
      default_rendering_signed[c] = codestream.get_signed(c,true);
      kdu_coords subs; codestream.get_subsampling(c,subs,true);
      if (subs != ref_subs)
        break;
    }
  if (c < num_channels)
    num_channels = 1;
  num_colour_channels = num_channels;
  return true;
}

/******************************************************************************/
/*                    kdu_channel_mapping::configure (JPX)                    */
/******************************************************************************/

bool
  kdu_channel_mapping::configure(jp2_colour colr, jp2_channels chnl,
                                 int codestream_idx, jp2_palette plt,
                                 jp2_dimensions codestream_dimensions)
{
  clear();
  if (!colour_converter.init(colr))
    return false;
  set_num_channels(chnl.get_num_colours());
  num_colour_channels = num_channels;
  if (num_channels <= 0)
    { KDU_ERROR(e,0); e <<
        KDU_TXT("JP2 object supplied to "
        "`kdu_channel_mapping::configure' has no colour channels!");
    }

  int c;
  for (c=0; c < num_channels; c++)
    {
      int lut_idx, stream;
      chnl.get_colour_mapping(c,source_components[c],lut_idx,stream);
      if (stream != codestream_idx)
        {
          clear();
          return false;
        }
      if (lut_idx >= 0)
        { // Set up palette lookup table
          int i, num_entries = plt.get_num_entries();
          assert(num_entries <= 1024);
          palette_bits = 1;
          while ((1<<palette_bits) < num_entries)
            palette_bits++;
          assert(palette[c] == NULL);
          palette[c] = new kdu_sample16[(int)(1<<palette_bits)];
          plt.get_lut(lut_idx,palette[c]);
          for (i=num_entries; i < (1<<palette_bits); i++)
            (palette[c])[i] = (palette[c])[num_entries-1];
          default_rendering_precision[c] = plt.get_bit_depth(lut_idx);
          default_rendering_signed[c] = plt.get_signed(lut_idx);
        }
      else
        {
          default_rendering_precision[c] =
            codestream_dimensions.get_bit_depth(source_components[c]);
          default_rendering_signed[c] =
            codestream_dimensions.get_signed(source_components[c]);
        }
    }
  return true;  
}

/******************************************************************************/
/*                    kdu_channel_mapping::configure (jp2)                    */
/******************************************************************************/

bool
  kdu_channel_mapping::configure(jp2_source *jp2_in, bool ignore_alpha)
{
  jp2_channels chnl = jp2_in->access_channels();
  jp2_palette plt = jp2_in->access_palette();
  jp2_colour colr = jp2_in->access_colour();
  jp2_dimensions dims = jp2_in->access_dimensions();
  if (!configure(colr,chnl,0,plt,dims))
    { KDU_ERROR(e,1); e <<
        KDU_TXT("Cannot perform colour conversion from the colour "
        "description embedded in a JP2 (or JP2-compatible) data source, to the "
        "sRGB colour space.  This should not happen with truly JP2-compatible "
        "descriptions.");
    }
  if (!ignore_alpha)
    add_alpha_to_configuration(chnl,0,plt,dims);
  return true;
}

/******************************************************************************/
/*              kdu_channel_mapping::add_alpha_to_configuration               */
/******************************************************************************/

bool
  kdu_channel_mapping::add_alpha_to_configuration(jp2_channels chnl,
                                 int codestream_idx, jp2_palette plt,
                                 jp2_dimensions codestream_dimensions,
                                 bool ignore_premultiplied_alpha)
{
  int scan_channels = chnl.get_num_colours();
  set_num_channels(num_colour_channels);
  int c, alpha_comp_idx=-1, alpha_lut_idx=-1;
  for (c=0; c < scan_channels; c++)
    {
      int lut_idx, tmp_idx, stream;
      if (chnl.get_opacity_mapping(c,tmp_idx,lut_idx,stream) &&
          (stream == codestream_idx))
        { // See if we can find a consistent alpha channel
          if (c == 0)
            { alpha_comp_idx = tmp_idx; alpha_lut_idx = lut_idx; }
          else if ((alpha_comp_idx != tmp_idx) && (alpha_lut_idx != lut_idx))
            alpha_comp_idx = alpha_lut_idx = -1; // Channels use different alpha
        }
      else
        alpha_comp_idx = alpha_lut_idx = -1; // Not all channels have alpha
    }

  if ((alpha_comp_idx < 0) && !ignore_premultiplied_alpha)
    for (c=0; c < scan_channels; c++)
      {
        int lut_idx, tmp_idx, stream;
        if (chnl.get_premult_mapping(c,tmp_idx,lut_idx,stream) &&
            (stream == codestream_idx))
          { // See if we can find a consistent alpha channel
            if (c == 0)
              { alpha_comp_idx = tmp_idx; alpha_lut_idx = lut_idx; }
            else if ((alpha_comp_idx != tmp_idx) && (alpha_lut_idx != lut_idx))
              alpha_comp_idx=alpha_lut_idx=-1; // Channels use different alpha
          }
        else
          alpha_comp_idx = alpha_lut_idx = -1; // Not all channels have alpha
      }

  if (alpha_comp_idx < 0)
    return false;

  set_num_channels(num_colour_channels+1);
  c = num_colour_channels; // Index of entry for the alpha channel
  source_components[c] = alpha_comp_idx;
  if (alpha_lut_idx >= 0)
    {
      int i, num_entries = plt.get_num_entries();
      assert(num_entries <= 1024);
      palette_bits = 1;
      while ((1<<palette_bits) < num_entries)
        palette_bits++;
      assert(palette[c] == NULL);
      palette[c] = new kdu_sample16[(int)(1<<palette_bits)];
      plt.get_lut(alpha_lut_idx,palette[c]);
      for (i=num_entries; i < (1<<palette_bits); i++)
        (palette[c])[i] = (palette[c])[num_entries-1];
      default_rendering_precision[c] = plt.get_bit_depth(alpha_lut_idx);
      default_rendering_signed[c] = plt.get_signed(alpha_lut_idx);
    }
  else
    {
      default_rendering_precision[c] =
        codestream_dimensions.get_bit_depth(alpha_comp_idx);
      default_rendering_signed[c] =
        codestream_dimensions.get_signed(alpha_comp_idx);
    }
  return true;
}


/* ========================================================================== */
/*                         kdu_region_decompressor                            */
/* ========================================================================== */

/******************************************************************************/
/*             kdu_region_decompressor::kdu_region_decompressor               */
/******************************************************************************/

kdu_region_decompressor::kdu_region_decompressor()
{
  precise = false;
  fastest = false;
  white_stretch_precision = 0;
  env=NULL;
  env_queue = NULL;
  next_queue_bank_idx = 0;
  tile_banks = new kd_tile_bank[2];
  current_bank = background_bank = NULL;
  codestream_failure = false;
  discard_levels = 0;
  max_channels = num_channels = num_colour_channels = 0;
  channels = NULL;
  colour_converter = NULL;
  pre_convert_colour = post_convert_colour = false;
  max_components = num_components = 0;
  components = NULL;
  component_indices = NULL;
  max_channel_bufs = num_channel_bufs = 0;
  channel_bufs = NULL;
}

/******************************************************************************/
/*             kdu_region_decompressor::~kdu_region_decompressor              */
/******************************************************************************/

kdu_region_decompressor::~kdu_region_decompressor()
{
  codestream_failure = true; // Force premature termination, if required
  finish();
  if (components != NULL)
    delete[] components;
  if (component_indices != NULL)
    delete[] component_indices;
  if (channels != NULL)
    delete[] channels;
  if (channel_bufs != NULL)
    delete[] channel_bufs;
  if (tile_banks != NULL)
    delete[] tile_banks;
}

/******************************************************************************/
/*                 kdu_region_decompressor::set_num_channels                  */
/******************************************************************************/

void
  kdu_region_decompressor::set_num_channels(int num)
{
  if (num > max_channels)
    {
      max_channels = num;
      if (channels != NULL)
        delete[] channels;
      channels = new kd_channel[max_channels];
    }
  num_channels = num_colour_channels = num;
  for (int c=0; c < num_channels; c++)
    {
      channels[c].source = NULL;
      channels[c].lut = NULL;
      channels[c].native_precision = 0;
      channels[c].native_signed = false;
    }
}

/******************************************************************************/
/*                   kdu_region_decompressor::add_component                   */
/******************************************************************************/

kd_component *
  kdu_region_decompressor::add_component(int comp_idx)
{
  int n;
  for (n=0; n < num_components; n++)
    if (component_indices[n] == comp_idx)
      return components + n;
  if (num_components == max_components)
    { // Allocate a new array
      max_components += 1+num_components; // Grow fast if we have a lot of them

      kd_component *old_comps = components;
      components = new kd_component[max_components];
      for (n=0; n < num_components; n++)
        components[n] = old_comps[n];
      if (old_comps != NULL)
        {
          delete[] old_comps;
          for (int k=0; k < num_channels; k++)
            if (channels[k].source != NULL)
              { // Fix up pointers
                int off = (int)(channels[k].source-old_comps);
                assert((off >= 0) && (off < num_components));
                channels[k].source = components + off;
              }
        }

      int *old_indices = component_indices;
      component_indices = new int[max_components];
      for (n=0; n < num_components; n++)
        component_indices[n] = old_indices[n];
      if (old_indices != NULL)
        delete[] old_indices;
    }

  n = num_components++;
  component_indices[n] = comp_idx;
  components[n].rel_comp_idx = n;
  components[n].palette_bits = 0;
  components[n].using_shorts = false;
  return components + n;
}

/******************************************************************************/
/*              kdu_region_decompressor::get_rendered_image_dims              */
/******************************************************************************/

kdu_dims
  kdu_region_decompressor::get_rendered_image_dims(kdu_codestream codestream,
                           kdu_channel_mapping *mapping, int single_component,
                           int discard_levels, kdu_coords expand_numerator,
                           kdu_coords expand_denominator,
                           kdu_component_access_mode access_mode)
{
  if (this->codestream.exists())
    { KDU_ERROR_DEV(e,2); e <<
        KDU_TXT("The `kdu_region_decompressor::get_rendered_image_dims' "
        "function should not be called with a `codestream' argument between "
        "calls to `kdu_region_decompressor::start' and "
        "`kdu_region_decompressor::finish'."); }
  int ref_idx;
  if (mapping == NULL)
    ref_idx = single_component;
  else
    ref_idx = mapping->source_components[0];

  kdu_dims ref_dims;
  codestream.apply_input_restrictions(0,0,discard_levels,0,NULL,access_mode);
  codestream.get_dims(ref_idx,ref_dims);
  return find_render_dims(ref_dims,expand_numerator,expand_denominator);
}

/******************************************************************************/
/*                      kdu_region_decompressor::start                        */
/******************************************************************************/

bool
  kdu_region_decompressor::start(kdu_codestream codestream,
                                 kdu_channel_mapping *mapping,
                                 int single_component, int discard_levels,
                                 int max_layers, kdu_dims region,
                                 kdu_coords expand_numerator,
                                 kdu_coords expand_denominator, bool precise,
                                 kdu_component_access_mode access_mode,
                                 bool fastest, kdu_thread_env *env,
                                 kdu_thread_queue *env_queue)
{
  int c;

  if ((tile_banks[0].num_tiles > 0) || (tile_banks[1].num_tiles > 0))
    { KDU_ERROR_DEV(e,0x10010701); e <<
        KDU_TXT("Attempting to call `kdu_region_decompressor::start' without "
        "first invoking the `kdu_region_decompressor::finish' to finish a "
        "previously installed region.");
    }
  this->precise = precise;
  this->fastest = fastest;
  this->env = env;
  this->env_queue = env_queue;
  this->next_queue_bank_idx = 0;
  this->codestream = codestream;
  codestream_failure = false;
  this->discard_levels = discard_levels;
  num_components = 0;
  post_convert_colour = pre_convert_colour = false;

  colour_converter = NULL;
  full_render_dims.pos = full_render_dims.size = kdu_coords(0,0);

  try { // Protect the following code
      // Set up components and channels.
      int total_palette_entries=0;
      if (mapping == NULL)
        {
          set_num_channels(1);
          channels[0].source = add_component(single_component);
          channels[0].lut = NULL;
        }
      else
        {
          if (mapping->num_channels < 1)
            { KDU_ERROR(e,3); e <<
                KDU_TXT("The `kdu_channel_mapping' object supplied to "
                "`kdu_region_decompressor::start' does not define any channels "
                "at all.");
            }
          set_num_channels(mapping->num_channels);
          num_colour_channels = mapping->num_colour_channels;
          if (num_colour_channels > num_channels)
            { KDU_ERROR(e,4); e <<
                KDU_TXT("The `kdu_channel_mapping' object supplied to "
                "`kdu_region_decompressor::start' specifies more colour "
                "channels than the total number of channels.");
            }
          colour_converter = mapping->get_colour_converter();
          if (!(colour_converter->exists() &&
                colour_converter->is_non_trivial()))
            colour_converter = NULL;
          pre_convert_colour = (colour_converter != NULL);
               // We will change this to false and set `post_convert_colour' to
               // true later on if we find that the channels have different
               // vertical sampling properties.
          for (c=0; c < mapping->num_channels; c++)
            {
              channels[c].source = add_component(mapping->source_components[c]);
              channels[c].lut =
                (mapping->palette_bits <= 0)?NULL:mapping->palette[c];
              if (channels[c].lut != NULL)
                channels[c].source->palette_bits = mapping->palette_bits;
              total_palette_entries += (1<<channels[c].source->palette_bits);
              channels[c].native_precision =
                mapping->default_rendering_precision[c];
              channels[c].native_signed =
                mapping->default_rendering_signed[c];
              channels[c].ref_line1 = NULL;
              channels[c].ref_line2 = NULL;
              channels[c].cur_line = NULL;
            }
        }

      // Configure component sampling and data representation information.
      if ((expand_denominator.x < 1) || (expand_denominator.y < 1))
        { KDU_ERROR_DEV(e,5); e <<
            KDU_TXT("Invalid expansion ratio supplied to "
            "`kdu_region_decompressor::start'.  The numerator and denominator "
            "terms expressed by the `expand_numerator' and "
            "`expand_denominator' arguments must be strictly positive.");
        }
      if ((expand_numerator.x < expand_denominator.x) ||
          (expand_numerator.y < expand_denominator.y))
        { KDU_ERROR_DEV(e,6); e <<
            KDU_TXT("Invalid expansion factors supplied to "
            "`kdu_region_decompressor::start'.  The ratio between "
            "`expand_numerator' and `expand_denominator' must be no less "
            "than 1.");
        }

      codestream.apply_input_restrictions(num_components,component_indices,
                                          discard_levels,max_layers,NULL,
                                          access_mode);
      kd_component *ref_comp = channels[0].source;
      kdu_coords ref_subs, this_subs, max_subs;
      codestream.get_subsampling(ref_comp->rel_comp_idx,ref_subs,true);

      max_subs = ref_subs;
      for (c=0; c < num_components; c++)
        {
          kd_component *comp = components + c;
          comp->bit_depth = codestream.get_bit_depth(comp->rel_comp_idx,true);
          comp->is_signed = codestream.get_signed(comp->rel_comp_idx,true);
          comp->num_line_users = 0; // Adjust when scanning the channels below
          codestream.get_subsampling(comp->rel_comp_idx,this_subs,true);
          if (this_subs.x > max_subs.x)
            max_subs.x = this_subs.x;
          if (this_subs.y > max_subs.y)
            max_subs.y = this_subs.y;
          
          kdu_long num_x, num_y, den_x, den_y;
          num_x = ((kdu_long) expand_numerator.x ) * ((kdu_long) this_subs.x);
          den_x = ((kdu_long) expand_denominator.x ) * ((kdu_long) ref_subs.x);
          num_y = ((kdu_long) expand_numerator.y) * ((kdu_long) this_subs.y);
          den_y = ((kdu_long) expand_denominator.y) * ((kdu_long) ref_subs.y);
          if (!(reduce_ratio_to_ints(num_x,den_x) &&
                reduce_ratio_to_ints(num_y,den_y)))
            { KDU_ERROR_DEV(e,7); e <<
                KDU_TXT("Unable to represent all component "
                "expansion factors as rational numbers whose numerator and "
                "denominator can both be expressed as 32-bit signed "
                "integers.  This is a very unusual condition.");
            }
          if ((num_x < den_x) || (num_y < den_y))
            { KDU_ERROR_DEV(e,8); e <<
                KDU_TXT("Invalid expansion factors supplied to "
                "`kdu_region_decompressor::start'.  The `expand_numerator' "
                "and `expand_denominator' coefficients must be chosen for the "
                "reference image component, such that all image components "
                "involved in the rendering process are guaranteed to have an "
                "expansion factor which is no less than 1.");
            }
          if ((num_x % den_x) == 0)
            { num_x = num_x / den_x;  den_x = 1; }
          if ((num_y % den_y) == 0)
            { num_y = num_y / den_y;  den_y = 1; }
          comp->expansion_numerator.x = (int) num_x;
          comp->expansion_denominator.x = (int) den_x;
          comp->expansion_numerator.y = (int) num_y;
          comp->expansion_denominator.y = (int) den_y;
          comp->expansion_counter = kdu_coords(0,0); // Fill in true value later
          codestream.get_registration(comp->rel_comp_idx,
                                      comp->expansion_denominator,
                                      comp->registration_offset,true);
          if (pre_convert_colour && (c > 0) &&
              ((comp->expansion_numerator.y !=
                components[0].expansion_numerator.y) ||
               (comp->expansion_denominator.y !=
                components[0].expansion_denominator.y) ||
               (comp->registration_offset.y !=
                components[0].registration_offset.y)))
            { // We will have to perform colour conversion once for each
              // separate output line.
              post_convert_colour = true;
              pre_convert_colour = false;
            }
        }

      // Check if we still need to initialize the native channel precisions
      for (c=0; c < num_channels; c++)
        if (channels[c].native_precision == 0)
          {
            channels[c].native_precision = channels[c].source->bit_depth;
            channels[c].native_signed = channels[c].source->is_signed;
          }

      // Set `kd_component::num_line_users' field and channel stretching factors
      for (c=0; c < num_channels; c++)
        {
          channels[c].stretch_residual = 0;
          if (channels[c].lut != NULL)
            continue; // Can't reliably stretch palette outputs, since
                      // we can't be sure of the palette output precision here.
          channels[c].source->num_line_users++;
          int B = (white_stretch_precision>16)?16:white_stretch_precision;
          int P = channels[c].source->bit_depth;
          if (B > P)
            { /* Evaluate:  2^16 * [(1-2^{-B}) / (1-2^{-P}) - 1]
                          = 2^16 * (2^{-P} - 2^{-B}) / (1-2^{-P})
                          = 2^16 * (2^{B-P} - 1) / (2^B - 2^{B-P}} */
              int num_val = ((1<<(B-P)) - 1) << 16;
              int den_val = (1<<B) - (1<<(B-P));
              channels[c].stretch_residual = (kdu_uint16)(num_val/den_val);
            }
        }

      // Apply level/layer restrictions and find `full_render_dims'
      codestream.apply_input_restrictions(num_components,component_indices,
                                          discard_levels,max_layers,NULL,
                                          access_mode);
      codestream.get_dims(ref_comp->rel_comp_idx,ref_comp->dims,true);
      full_render_dims = find_render_dims(ref_comp->dims,
                                          ref_comp->expansion_numerator,
                                          ref_comp->expansion_denominator);
      if ((full_render_dims & region) != region)
        { KDU_ERROR_DEV(e,9); e <<
            KDU_TXT("The `region' passed into "
            "`kdu_region_decompressor::start' does not lie fully within the "
            "region occupied by the full image on the rendering canvas.  The "
            "error is probably connected with a misunderstanding of the "
            "way in which codestream dimensions are mapped to rendering "
            "coordinates through the rational upsampling process offered by "
            "the `kdu_region_decompressor' object.  It is best to use "
            "`kdu_region_decompressor::get_rendered_image_dims' to find the "
            "full image dimensions on the rendering canvas.");
        }

      // Find an appropriate region (a little large) on the code-stream canvas
      kdu_long num, den;
      kdu_coords min = region.pos;
      kdu_coords lim = min + region.size;

      num = ref_comp->expansion_denominator.x;
      den = ref_comp->expansion_numerator.x;
      min.x = long_floor_ratio(num*min.x,den) - ((2*max_subs.x) / ref_subs.x);
      lim.x = long_ceil_ratio(num*lim.x,den)  + ((2*max_subs.x) / ref_subs.x);
 
      num = ref_comp->expansion_denominator.y;
      den = ref_comp->expansion_numerator.y;
      min.y = long_floor_ratio(num*min.y,den) - ((2*max_subs.y) / ref_subs.y);
      lim.y = long_ceil_ratio(num*lim.y,den)  + ((2*max_subs.y) / ref_subs.y);

      kdu_dims ref_region;
      ref_region.pos = min; ref_region.size = lim-min;
      kdu_dims canvas_region;
      codestream.map_region(ref_comp->rel_comp_idx,ref_region,
                            canvas_region,true);
      codestream.apply_input_restrictions(num_components,component_indices,
                                          discard_levels,max_layers,
                                          &canvas_region,access_mode);

      // Prepare for processing tiles within the region.
      codestream.get_valid_tiles(valid_tiles);
      next_tile_idx = valid_tiles.pos;
    }
  catch (int exc) {
      if (env != NULL)
        env->handle_exception(exc);
      finish();
      return false;
    }
  return true;
}

/******************************************************************************/
/*                 kdu_region_decompressor::start_tile_bank                   */
/******************************************************************************/

bool
  kdu_region_decompressor::start_tile_bank(kd_tile_bank *bank,
                                           kdu_long suggested_tile_area,
                                           kdu_dims incomplete_region)
{
  assert(bank->num_tiles == 0); // Make sure it is not currently open
  bank->env_queue = NULL;
  bank->queue_bank_idx = 0;
  bank->freshly_created = true;

  // Start by finding out how many tiles will be in the bank, and the region
  // they occupy on the canvas
  kd_component *ref_comp = channels[0].source;
  if (suggested_tile_area < 1)
    suggested_tile_area = 1;
  suggested_tile_area += suggested_tile_area >> 1;
  kdu_dims required_canvas_dims =
    find_conservative_ref_comp_dims(incomplete_region,
                                    ref_comp->expansion_numerator,
                                    ref_comp->expansion_denominator);
  int num_tiles=0;
  kdu_coords idx;
  while (((next_tile_idx.y-valid_tiles.pos.y) < valid_tiles.size.y) &&
         ((next_tile_idx.x-valid_tiles.pos.x) < valid_tiles.size.x) &&
         (suggested_tile_area > 0))
    {
      idx = next_tile_idx;   next_tile_idx.x++;
      kdu_dims dims;
      codestream.get_tile_dims(idx,ref_comp->rel_comp_idx,dims,true);
      if (!dims.intersects(required_canvas_dims))
        { // Discard tile immediately
          kdu_tile tt=codestream.open_tile(idx,env);
          if (tt.exists()) tt.close(env);
          continue;
        }
      if (num_tiles == 0)
        {
          bank->dims = dims;
          bank->first_tile_idx = idx;
        }
      else
        bank->dims.size.x += dims.size.x;
      suggested_tile_area -= dims.area();
      num_tiles++;
    }

  if ((next_tile_idx.x-valid_tiles.pos.x) == valid_tiles.size.x)
    { // Advance `next_tile_idx' to the start of the next row of tiles
      next_tile_idx.x = valid_tiles.pos.x;
      next_tile_idx.y++;
    }

  if (num_tiles == 0)
    return true; // Nothing to do; must have discarded some tiles

  // Allocate the necessary storage in the `kd_tile_bank' object
  if (num_tiles > bank->max_tiles)
    {
      if (bank->tiles != NULL) delete[] bank->tiles;
      if (bank->engines != NULL) delete[] bank->engines;
      bank->max_tiles = num_tiles;
      bank->tiles = new kdu_tile[bank->max_tiles];
      bank->engines = new kdu_multi_synthesis[bank->max_tiles];
    }
  bank->num_tiles = num_tiles; // Only now is it completely safe to record
                      // `num_tiles' in `bank' -- in case of thrown exceptions

  // Now open all the tiles, checking whether or not an error will occur
  int tnum;
  for (idx=bank->first_tile_idx, tnum=0; tnum < num_tiles; tnum++, idx.x++)
    bank->tiles[tnum] = codestream.open_tile(idx,env);
  if ((codestream.get_min_dwt_levels() < discard_levels) ||
      !codestream.can_flip(true))
    {
      for (tnum=0; tnum < num_tiles; tnum++)
        bank->tiles[tnum].close(env);
      bank->num_tiles = 0;
      return false;
    }
  
  // Create processing queue (if required) and all tile processing engines
  if (env != NULL)
    bank->env_queue =
      env->add_queue(NULL,env_queue,"`kdu_region_decompressor' tile bank",
                     (bank->queue_bank_idx=next_queue_bank_idx++));
  int processing_stripe_height = 1;
  bool double_buffering = false;
  if ((env != NULL) && (bank->dims.size.y >= 64))
    { // Assign a heuristic double buffering height
      double_buffering = true;
      processing_stripe_height = 32;
    }
  for (tnum=0; tnum < num_tiles; tnum++)
    bank->engines[tnum].create(codestream,bank->tiles[tnum],precise,false,
                               fastest,processing_stripe_height,
                               env,bank->env_queue,double_buffering);
  return true;
}

/******************************************************************************/
/*                  kdu_region_decompressor::close_tile_bank                  */
/******************************************************************************/

void
  kdu_region_decompressor::close_tile_bank(kd_tile_bank *bank)
{
  if (bank->num_tiles == 0)
    return;

  int tnum;
  if ((env != NULL) && (bank->env_queue != NULL))
    env->terminate(bank->env_queue,false);
  bank->env_queue = NULL;
  for (tnum=0; tnum < bank->num_tiles; tnum++)
    if ((!codestream_failure) && bank->tiles[tnum].exists())
      {
        try {
            bank->tiles[tnum].close(env);
          }
      catch (int exc) { // `kdu_error' can throw exception from deep inside core
          codestream_failure = true;
          if (env != NULL)
            env->handle_exception(exc);
        }
      }
  for (tnum=0; tnum < bank->num_tiles; tnum++)
    if (bank->engines[tnum].exists())
      bank->engines[tnum].destroy();
  bank->num_tiles = 0;
}

/******************************************************************************/
/*             kdu_region_decompressor::make_tile_bank_current                */
/******************************************************************************/

void
  kdu_region_decompressor::make_tile_bank_current(kd_tile_bank *bank)
{
  assert(bank->num_tiles > 0);
  current_bank = bank;

  // Establish rendering dimensions for the current tile bank.  The mapping
  // between rendering dimensions and actual code-stream dimensions is
  // invariably based upon the first channel as the reference component.
  kd_component *ref_comp = channels[0].source;
  render_dims = find_render_dims(bank->dims,
                                 ref_comp->expansion_numerator,
                                 ref_comp->expansion_denominator);
  assert((full_render_dims & render_dims) == render_dims);

  // Fill in tile-specific component fields and pre-allocate component lines
  int c, w;
  aux_allocator.restart();
  for (c=0; c < num_components; c++)
    {
      kd_component *comp = components + c;
      codestream.get_tile_dims(bank->first_tile_idx,
                               comp->rel_comp_idx,comp->dims,true);
      if (bank->num_tiles > 1)
        {
          kdu_coords last_tile_idx = bank->first_tile_idx;
          last_tile_idx.x += bank->num_tiles-1;
          kdu_dims last_tile_dims;
          codestream.get_tile_dims(last_tile_idx,
                                   comp->rel_comp_idx,last_tile_dims,true);
          assert((last_tile_dims.pos.y == comp->dims.pos.y) &&
                 (last_tile_dims.size.y == comp->dims.size.y));
          comp->dims.size.x =
            last_tile_dims.pos.x+last_tile_dims.size.x-comp->dims.pos.x;
        }
      comp->new_line_samples = 0;
      comp->needed_line_samples = comp->dims.size.x;
      bool using_line_store;
      if (comp->dims.size.y <= 0)
        { // Need to store the all-zero sequence separately
          using_line_store = true;
          comp->using_shorts = !precise;
        }
      else if (comp->num_line_users == 0)
        { // Transfer lines directly from decompressor to palette `indices'
          using_line_store = false;
          comp->using_shorts = true; // Should not matter what this is
        }
      else if (bank->num_tiles == 1)
        { // Only one tile; `line' is obtained directly from decompressor
          using_line_store = false;
          comp->using_shorts =
            !bank->engines[0].is_line_precise(comp->rel_comp_idx);
        }
      else
        { // Multiple tiles; copy tile engine outputs to a single `line_store'.
          // In this case, we should use shorts if any tile uses shorts or if
          // there is any colour conversion, because we don't do up-conversion
          // from short to long representations and non-trivial colour
          // conversion is always performed using short precision.
          using_line_store = true;
          int t=0;
          if (colour_converter == NULL)
            for (; t < bank->num_tiles; t++)
              if (!bank->engines[t].is_line_precise(comp->rel_comp_idx))
                break;
          comp->using_shorts = (t < bank->num_tiles);
        }
      comp->line = NULL;
      comp->line_store.destroy(); // Works even if never allocated
      comp->indices.destroy(); // Works even if never allocated
      if (using_line_store)
        {
          comp->line_store.pre_create(&aux_allocator,comp->dims.size.x,
                                      false,comp->using_shorts);
          comp->line = &(comp->line_store);
        }
      if (comp->palette_bits > 0)
        comp->indices.pre_create(&aux_allocator,comp->dims.size.x,true,true);

      comp->can_use_directly = (comp->num_line_users == 1) &&
        (comp->expansion_numerator.x==1) &&
        (comp->expansion_denominator.x==1) &&
        (using_line_store ||
         (comp->using_shorts &&
          !bank->engines[0].is_line_absolute(comp->rel_comp_idx)));
            // The above conditions are almost sufficient to guarantee that we
            // can use the `line' buffer as a channel buffer directly, without
            // any copying or conversions.  We still need to verify that the
            // line is big enough and its start point corresponds to that
            // of the channel, but this is easiest to do in `process_generic'.

      kdu_coords offset = comp->registration_offset;
      kdu_coords ref_offset = ref_comp->registration_offset;

      kdu_long val, num, den, half;
      num = comp->expansion_numerator.x;  den = comp->expansion_denominator.x;
      if (den == 1)
        { // Integer expansion factor; expand pixels as blocks for speed and
          // sharpness.  In this case, `expansion_counter' holds the size of
          // the first output block produced by the first input pixel.
          half = (num-1) >> 1;
        }
      else
        { // Rational expansion factor; in this case `expansion_counter'
          // should be set so that `expansion_counter'/`expansion_numerator'-1/8
          // represents the fraction of an input pixel separation by which the
          // first output pixel precedes the first input pixel.
          half = num - ((num+4) >> 3);
        }
      val = num * comp->dims.pos.x + num - half;
      val -= den * render_dims.pos.x;
      val += (offset.x - ref_offset.x);
      comp->expansion_counter.x = (int) val;

      num = comp->expansion_numerator.y;  den = comp->expansion_denominator.y;
      if (den == 1)
        { // Integer expansion factor; expand pixels as blocks for speed and
          // sharpness.  In this case, `expansion_counter' holds the size of
          // the first output block produced by the first input pixel.
          half = (num-1) >> 1;
        }
      else
        {
          half = num - ((num+4) >> 3);
        }
      val = num * comp->dims.pos.y + num - half;
      val -= den * render_dims.pos.y;
      val += (offset.y - ref_offset.y);
      comp->expansion_counter.y = (int) val;
    }

  // Pre-create channel line buffers.
  for (c=0; c < num_channels; c++)
    {
      kd_channel *chan = channels + c;
      if (chan->lut != NULL)
        chan->using_shorts = true;
      else if (colour_converter != NULL)
        chan->using_shorts = true; // Non-trivial conversion; use only shorts
      else
        chan->using_shorts = chan->source->using_shorts;
      for (w=0; w < 3; w++)
        {
          channels[c].lines[w].destroy(); // Works even if never created
          chan->lines[w].pre_create(&aux_allocator,render_dims.size.x,false,
                                    chan->using_shorts);
        }
    }

  // Perform final resource allocation
  aux_allocator.finalize();
  for (c=0; c < num_components; c++)
    {
      if (components[c].palette_bits > 0)
        components[c].indices.create();
      if (components[c].line != NULL)
        components[c].line_store.create();
    }
  for (c=0; c < num_channels; c++)
    {
      for (w=0; w < 3; w++)
        channels[c].lines[w].create();
      channels[c].ref_line1 = NULL;
      channels[c].ref_line2 = NULL;
      channels[c].cur_line = NULL;
    }
}

/******************************************************************************/
/*                       kdu_region_decompressor::finish                      */
/******************************************************************************/

bool
  kdu_region_decompressor::finish()
{
  bool success;
  int b, c, w;

  // Make sure all tile banks are closed and their queues terminated
  if (current_bank != NULL)
    { // Make sure we close this one first, in case there are two open banks
      close_tile_bank(current_bank);
    }
  if (tile_banks != NULL)
    for (b=0; b < 2; b++)
      close_tile_bank(tile_banks+b);
  current_bank = background_bank = NULL;

  success = !codestream_failure;
  codestream_failure = false;
  env = NULL;
  env_queue = NULL;

  // Reset any resource pointers which may no longer be valid
  for (c=0; c < num_components; c++)
    {
      components[c].line = NULL;
      components[c].line_store.destroy();
      components[c].indices.destroy();
    }
  for (c=0; c < num_channels; c++)
    {
      for (w=0; w < 3; w++)
        channels[c].lines[w].destroy();
      channels[c].ref_line1 = NULL;
      channels[c].ref_line2 = NULL;
      channels[c].cur_line = NULL;
      channels[c].lut = NULL;
    }
  codestream = kdu_codestream(); // Invalidate the internal pointer for safety
  aux_allocator.restart(); // Get ready to use the allocation object again.
  full_render_dims.pos = full_render_dims.size = kdu_coords(0,0);
  num_components = num_channels = 0;
  return success;
}

/******************************************************************************/
/*                  kdu_region_decompressor::process (bytes)                  */
/******************************************************************************/

bool
  kdu_region_decompressor::process(kdu_byte **chan_bufs,
                                   bool expand_monochrome,
                                   int pixel_gap, kdu_coords buffer_origin,
                                   int row_gap, int suggested_increment,
                                   int max_region_pixels,
                                   kdu_dims &incomplete_region,
                                   kdu_dims &new_region, int precision_bits,
                                   bool measure_row_gap_in_pixels)
{
  if (num_colour_channels != 1)
    expand_monochrome = false;
  num_channel_bufs = num_channels + ((expand_monochrome)?2:0);
  if (num_channel_bufs > max_channel_bufs)
    {
      max_channel_bufs = num_channel_bufs;
      if (channel_bufs != NULL)
        delete[] channel_bufs;
      channel_bufs = new kdu_byte *[max_channel_bufs];
    }
  for (int c=0; c < num_channel_bufs; c++)
    channel_bufs[c] = chan_bufs[c];
  if (measure_row_gap_in_pixels)
    row_gap *= pixel_gap;
  return process_generic(1,pixel_gap,buffer_origin,row_gap,suggested_increment,
                         max_region_pixels,incomplete_region,new_region,
                         precision_bits);
}

/******************************************************************************/
/*                  kdu_region_decompressor::process (words)                  */
/******************************************************************************/

bool
  kdu_region_decompressor::process(kdu_uint16 **chan_bufs,
                                   bool expand_monochrome,
                                   int pixel_gap, kdu_coords buffer_origin,
                                   int row_gap, int suggested_increment,
                                   int max_region_pixels,
                                   kdu_dims &incomplete_region,
                                   kdu_dims &new_region, int precision_bits,
                                   bool measure_row_gap_in_pixels)
{
  if (num_colour_channels != 1)
    expand_monochrome = false;
  num_channel_bufs = num_channels + ((expand_monochrome)?2:0);
  if (num_channel_bufs > max_channel_bufs)
    {
      max_channel_bufs = num_channel_bufs;
      if (channel_bufs != NULL)
        delete[] channel_bufs;
      channel_bufs = new kdu_byte *[max_channel_bufs];
    }
  for (int c=0; c < num_channel_bufs; c++)
    channel_bufs[c] = (kdu_byte *) chan_bufs[c];
  if (measure_row_gap_in_pixels)
    row_gap *= pixel_gap;
  return process_generic(2,pixel_gap,buffer_origin,row_gap,suggested_increment,
                         max_region_pixels,incomplete_region,new_region,
                         precision_bits);
}

/******************************************************************************/
/*                 kdu_region_decompressor::process (packed)                  */
/******************************************************************************/

bool
  kdu_region_decompressor::process(kdu_int32 *buffer,
                                   kdu_coords buffer_origin,
                                   int row_gap, int suggested_increment,
                                   int max_region_pixels,
                                   kdu_dims &incomplete_region,
                                   kdu_dims &new_region)
{
  if (num_colour_channels == 2)
    { KDU_ERROR_DEV(e,0x12060600); e <<
        KDU_TXT("The convenient, packed 32-bit integer version of "
        "`kdu_region_decompressor::process' may not be used if the number "
        "of colour channels equals 2.");
    }

  num_channel_bufs = num_colour_channels + 1;
  if (num_colour_channels == 1)
    num_channel_bufs += 2;

  if (max_channel_bufs < num_channel_bufs)
    {
      max_channel_bufs = num_channel_bufs;
      if (channel_bufs != NULL)
        delete[] channel_bufs;
      channel_bufs = new kdu_byte *[max_channel_bufs];
    }
  kdu_byte *buf = (kdu_byte *) buffer;
  int c, test_endian = 1;
  if (((kdu_byte *) &test_endian)[0] == 0)
    { // Big-endian byte order
      channel_bufs[0] = buf+1;
      channel_bufs[1] = buf+2;
      channel_bufs[2] = buf+3;
      for (c=3; c < num_colour_channels; c++)
        channel_bufs[c] = NULL;
      channel_bufs[c++] = buf+0;
    }
  else
    { // Little-endian byte order
      channel_bufs[0] = buf+2;
      channel_bufs[1] = buf+1;
      channel_bufs[2] = buf+0;
      for (c=3; c < num_colour_channels; c++)
        channel_bufs[c] = NULL;
      channel_bufs[c++] = buf+3;
    }
  assert(c <= num_channel_bufs);
  for (; c < num_channel_bufs; c++)
    channel_bufs[c] = NULL;

  bool result =
    process_generic(1,4,buffer_origin,row_gap*4,suggested_increment,
                    max_region_pixels,incomplete_region,new_region,8,
                    (num_channels==num_colour_channels));
  return result;
}

/******************************************************************************/
/*            kdu_region_decompressor::process (interleaved bytes)            */
/******************************************************************************/

bool
  kdu_region_decompressor::process(kdu_byte *buffer,
                                   int *chan_offsets,
                                   int pixel_gap,
                                   kdu_coords buffer_origin,
                                   int row_gap,
                                   int suggested_increment,
                                   int max_region_pixels,
                                   kdu_dims &incomplete_region,
                                   kdu_dims &new_region,
                                   int precision_bits,
                                   bool measure_row_gap_in_pixels)
{
  num_channel_bufs = num_channels;
  if (num_channel_bufs > max_channel_bufs)
    {
      max_channel_bufs = num_channel_bufs;
      if (channel_bufs != NULL)
        delete[] channel_bufs;
      channel_bufs = new kdu_byte *[max_channel_bufs];
    }
  for (int c = 0; c < num_channel_bufs; c++)
    channel_bufs[c] = buffer + chan_offsets[c];
  if (measure_row_gap_in_pixels)
    row_gap *= pixel_gap;
  bool result;
  result = process_generic(1,pixel_gap,buffer_origin,row_gap,suggested_increment,
                           max_region_pixels,incomplete_region,new_region,
                           precision_bits);
  return result;
}

/******************************************************************************/
/*            kdu_region_decompressor::process (interleaved words)            */
/******************************************************************************/

bool 
  kdu_region_decompressor::process(kdu_uint16 *buffer,
                                   int *chan_offsets,
                                   int pixel_gap,
                                   kdu_coords buffer_origin,
                                   int row_gap,
                                   int suggested_increment,
                                   int max_region_pixels,
                                   kdu_dims &incomplete_region,
                                   kdu_dims &new_region,
                                   int precision_bits,
                                   bool measure_row_gap_in_pixels)
{
  num_channel_bufs = num_channels;
  if (num_channel_bufs > max_channel_bufs)
    {
      max_channel_bufs = num_channel_bufs;
      if (channel_bufs != NULL)
        delete[] channel_bufs;
      channel_bufs = new kdu_byte *[max_channel_bufs];
    }
  for (int c = 0; c < num_channel_bufs; c++)
    channel_bufs[c] = (kdu_byte *)(buffer + chan_offsets[c]);
  if (measure_row_gap_in_pixels)
    row_gap *= pixel_gap;
  return process_generic(2,pixel_gap,buffer_origin,row_gap,suggested_increment,
                         max_region_pixels,incomplete_region,new_region,
                         precision_bits);
}

/******************************************************************************/
/*                  kdu_region_decompressor::process_generic                  */
/******************************************************************************/

bool
  kdu_region_decompressor::process_generic(int sample_bytes, int pixel_gap,
                                           kdu_coords buffer_origin,
                                           int row_gap, int suggested_increment,
                                           int max_region_pixels,
                                           kdu_dims &incomplete_region,
                                           kdu_dims &new_region,
                                           int precision_bits, bool fill_alpha)
{
  new_region.size = kdu_coords(0,0); // In case we decompress nothing
  if (codestream_failure || !incomplete_region)
    return false;

  try { // Protect, in case a fatal error is generated by the decompressor
      kd_component *ref_comp = channels[0].source;
      int suggested_ref_comp_samples = suggested_increment;
      if ((suggested_increment <= 0) && (row_gap == 0))
        { // Need to consider `max_region_pixels' argument
          kdu_long num = max_region_pixels;
          num *= ref_comp->expansion_denominator.x;
          num *= ref_comp->expansion_denominator.y;
          kdu_long den = ref_comp->expansion_numerator.x;
          den *= ref_comp->expansion_numerator.y;
          suggested_ref_comp_samples = 1 + (int)(num / den);
        }
      if ((current_bank == NULL) && (background_bank != NULL))
        {
          make_tile_bank_current(background_bank);
          background_bank = NULL;
        }
      if (current_bank == NULL)
        {
          kd_tile_bank *new_bank = tile_banks + 0;
          if (!start_tile_bank(new_bank,suggested_ref_comp_samples,
                               incomplete_region))
            return false; // Trying to discard too many resolution levels or
                          // else trying to flip an unflippable image
          if (new_bank->num_tiles == 0)
            { // Opened at least one tile, but all such tiles
              // were found to lie outside the `conservative_ref_comp_region'
              // and so were closed again immediately.
              if ((next_tile_idx.x == valid_tiles.pos.x) &&
                  (next_tile_idx.y >= (valid_tiles.pos.y+valid_tiles.size.y)))
                { // No more tiles can lie within the `incomplete_region'
                  incomplete_region.pos.y += incomplete_region.size.y;
                  incomplete_region.size.y = 0;
                  return false;
                }
              return true;
            }
          make_tile_bank_current(new_bank);
        }
      if ((env != NULL) && (background_bank == NULL) &&
          (next_tile_idx.y < (valid_tiles.pos.y+valid_tiles.size.y)))
        {
          background_bank = tile_banks + ((current_bank==tile_banks)?1:0);
          if (!start_tile_bank(background_bank,suggested_ref_comp_samples,
                               incomplete_region))
            return false; // Trying to discard too many resolution levels or
                          // else trying to flip an unflippable image
          if (background_bank->num_tiles == 0)
            { // Opened at least one tile, but all such tiles were found to
              // lie outside the `conservative_ref_comp_region' and so
              // were closed again immediately. No problem; just don't do any
              // background processing at present.
              background_bank = NULL;
            }
        }

      bool last_bank_in_row =
        ((render_dims.pos.x+render_dims.size.x) >=
         (incomplete_region.pos.x+incomplete_region.size.x));
      bool first_bank_in_new_row = current_bank->freshly_created &&
        (render_dims.pos.x <= incomplete_region.pos.x);
      if ((last_bank_in_row || first_bank_in_new_row) &&
          (render_dims.pos.y > incomplete_region.pos.y))
        { // Incomplete region must have shrunk horizontally and the
          // application has no way of knowing that we have already rendered
          // some initial lines of the new, narrower incomplete region.
          int y = render_dims.pos.y - incomplete_region.pos.y;
          incomplete_region.size.y -= y;
          incomplete_region.pos.y += y;
        }
      kdu_dims incomplete_bank_region = render_dims & incomplete_region;
      if (!incomplete_bank_region)
        { // No intersection between current tile bank and incomplete region.
          close_tile_bank(current_bank);
          current_bank = NULL;
                // When we consider multiple concurrent banks later on, we will
                // need to modify the code here.
          return true; // Let the caller get back to us for more tile processing
        }
      current_bank->freshly_created = false; // We are about to decompress data

      // Determine an appropriate number of lines to process before returning.
      // Note that some or all of these lines might not intersect with the
      // incomplete region.
      new_region = incomplete_bank_region;
      new_region.size.y = 0;
      int new_lines =
        1 + (suggested_ref_comp_samples / current_bank->dims.size.x);
      new_lines = (int)
        ((new_lines * (kdu_long) ref_comp->expansion_numerator.y) /
         ((kdu_long) ref_comp->expansion_denominator.y));
      if (new_lines > incomplete_bank_region.size.y)
        new_lines = incomplete_bank_region.size.y;
      if ((row_gap == 0) && ((new_lines*new_region.size.x) > max_region_pixels))
        new_lines = max_region_pixels / new_region.size.x;
      if (new_lines <= 0)
        { KDU_ERROR_DEV(e,13); e <<
            KDU_TXT("Channel buffers supplied to "
            "`kdr_region_decompressor::process' are too small to "
            "accommodate even a single line of the new region "
            "to be decompressed.  You should be careful to ensure that the "
            "buffers are at least as large as the width indicated by the "
            "`incomplete_region' argument passed to the `process' "
            "function.  Also, be sure to identify the buffer sizes "
            "correctly through the `max_region_pixels' argument supplied "
            "to that function.");
        }

      // Align buffers at start of new region and set `row_gap' if necessary
      int c;
      if (row_gap == 0)
        row_gap = new_region.size.x * pixel_gap;
      else
        {
          int buf_offset = sample_bytes *
            ((new_region.pos.y - buffer_origin.y) * row_gap +
             (new_region.pos.x - buffer_origin.x) * pixel_gap);
          for (c=0; c < num_channel_bufs; c++)
            if (channel_bufs[c] != NULL)
              channel_bufs[c] += buf_offset;
        }
      row_gap *= sample_bytes;

      int num_active_channels = num_channel_bufs - ((fill_alpha)?1:0);
#ifdef KDU_SIMD_OPTIMIZATIONS
      // Set up fast processing for interleaved buffers under simple, yet
      // common conditions
      kdu_byte *fast_base = NULL; // Non-NULL if fast processing possible
      kdu_byte fast_mask[4] = {0,0,0,0}; // FF if byte is to be written
      kdu_byte fast_source[4] = {0,0,0,0}; // Idx of src channel to use
      if ((pixel_gap == 4) && (sample_bytes == 1) && (num_channel_bufs <= 4) &&
          (precision_bits > 0) && (precision_bits <= 8))
        {
          fast_base = (kdu_byte *)
            _kdu_long_to_addr(_addr_to_kdu_long(channel_bufs[0]) &
                              ~((kdu_long) 3));
          for (c=0; c < num_channel_bufs; c++)
            {
              if ((c < num_channels) && !channels[c].using_shorts)
                break; // Fast processing only if source samples 16 bits wide
              int dst_idx = (int)(channel_bufs[c] - fast_base);
              if ((dst_idx < 0) || (dst_idx > 3))
                break; // Fast processing only for aligned & interleaved bufs
              int src_idx = c;
              if ((num_active_channels > num_channels) && (c != 0))
                {
                  assert(num_active_channels == (num_channels+2));
                  src_idx -= ((c==1)?1:2);
                }
              if (fill_alpha && (c >= num_active_channels))
                src_idx = 0; // Any valid source; we won't use it.
              fast_mask[dst_idx] = (kdu_byte) 0xFF;
              fast_source[dst_idx] = (kdu_byte) src_idx;
            }
          if (c < num_channel_bufs)
            fast_base = NULL; // Cannot use fast processing
        }
#endif // KDU_SIMD_OPTIMIZATIONS

      // Determine and process new region.
      int skip_cols = new_region.pos.x - render_dims.pos.x;
      int num_cols = new_region.size.x;
      for (; new_lines > 0; new_lines--)
        {
          // Decompress new image component lines as necessary.
          bool anything_needed = false;
          for (c=0; c < num_components; c++)
            {
              kd_component *comp = components + c;
              if (comp->dims.size.y <= 0)
                { // No more lines available.
                  if (comp->needed_line_samples > 0)
                    { // Tile has no lines of this component at all.  Set to 0
                      assert(comp->line == &(comp->line_store));
                      reset_line_buf(&(comp->line_store));
                      comp->new_line_samples = comp->needed_line_samples;
                      comp->needed_line_samples = 0;
                    }
                }
              else if (comp->expansion_counter.y <= 0)
                {
                  comp->expansion_counter.y += comp->expansion_numerator.y;
                  comp->needed_line_samples = comp->dims.size.x;
                }
              if (comp->needed_line_samples > 0)
                anything_needed = true;
            }
          if (anything_needed)
            {
              int tnum;
              for (tnum=0; tnum < current_bank->num_tiles; tnum++)
                {
                  kdu_multi_synthesis *engine = current_bank->engines + tnum;
                  for (c=0; c < num_components; c++)
                    {
                      kd_component *comp = components + c;
                      if (comp->needed_line_samples <= comp->new_line_samples)
                        continue;
                      kdu_line_buf *line =
                        engine->get_line(comp->rel_comp_idx,env);
                      if ((line == NULL) || (line->get_width() <= 0))
                        continue;
                      if (comp->palette_bits > 0)
                        convert_samples_to_palette_indices(line,
                                         comp->bit_depth,comp->is_signed,
                                         comp->palette_bits,&(comp->indices),
                                         comp->new_line_samples);
                      if (comp->line == &(comp->line_store))
                        convert_and_copy(line,comp->bit_depth,
                                         comp->line,comp->new_line_samples);
                      else if (comp->num_line_users > 0)
                        comp->line = line;
                      comp->new_line_samples += line->get_width();
                    }
                }
              for (c=0; c < num_components; c++)
                {
                  kd_component *comp = components + c;
                  if (comp->needed_line_samples > 0)
                    {
                      assert(comp->new_line_samples==comp->needed_line_samples);
                      comp->needed_line_samples = 0;
                      comp->dims.size.y--;
                      comp->dims.pos.y++;
                    }
                }
            }

          // Perform horizontal interpolation/mapping operations for channel
          // lines whose component lines were recently created
          bool anything_new = false;
          for (c=0; c < num_channels; c++)
            {
              kd_channel *channel = channels + c;
              kd_component *comp = channel->source;
              if (comp->new_line_samples == 0)
                continue;
              anything_new = true;
              if (comp->can_use_directly &&
                  (skip_cols==0) && (num_cols <= comp->dims.size.x))
                channel->ref_line2 = comp->line;
              else
                {
                  channel->ref_line1 = channel->ref_line2;
                  channel->ref_line2 = channel->lines;
                  if (channel->ref_line2 == channel->ref_line1)
                    channel->ref_line2++;
                  if (channel->ref_line1 == NULL)
                    channel->ref_line1 = channel->ref_line2;
                  if (channel->lut != NULL)
                    interpolate_and_map(&(comp->indices),
                                        comp->expansion_counter.x,
                                        comp->expansion_numerator.x,
                                        comp->expansion_denominator.x,
                                        channel->lut,channel->ref_line2,
                                        skip_cols,num_cols);
                  else if ((comp->line == &(comp->line_store)) &&
                           !comp->using_shorts)
                    interpolate_fix32(comp->line,
                                      comp->expansion_counter.x,
                                      comp->expansion_numerator.x,
                                      comp->expansion_denominator.x,
                                      channel->ref_line2,skip_cols,num_cols);
                  else if ((comp->expansion_numerator.x == 1) &&
                           (comp->expansion_denominator.x == 1) &&
                           (skip_cols==0) && (num_cols <= comp->dims.size.x))
                    convert_and_copy(comp->line,comp->bit_depth,
                                     channel->ref_line2,0);
                  else
                    interpolate_and_convert(comp->line,
                                            comp->expansion_counter.x,
                                            comp->expansion_numerator.x,
                                            comp->expansion_denominator.x,
                                            comp->bit_depth,channel->ref_line2,
                                            skip_cols,num_cols);
                }
              if (channel->stretch_residual > 0)
                apply_white_stretch(channel->ref_line2,num_cols,
                                    channel->stretch_residual);
            }

          if (pre_convert_colour && anything_new)
            { // Here we perform colour conversion once for each new batch
              // of source channel lines, using the results for each vertically
              // interpolated output line.  We can do this whenever the
              // channels all have identical vertical interpolation properties.
              if (num_colour_channels < 3)
                colour_converter->convert_lum(*(channels[0].ref_line2),
                                              num_cols);
              else if (num_colour_channels == 3)
                colour_converter->convert_rgb(*(channels[0].ref_line2),
                                              *(channels[1].ref_line2),
                                              *(channels[2].ref_line2),
                                              num_cols);
              else
                colour_converter->convert_rgb4(*(channels[0].ref_line2),
                                               *(channels[1].ref_line2),
                                               *(channels[2].ref_line2),
                                               *(channels[3].ref_line2),
                                               num_cols);
            }

          // Now determine whether or not we can proceed to generate any
          // output lines yet
          bool can_proceed = true;
          for (c=0; c < num_components; c++)
            {
              kd_component *comp = components + c;
              comp->new_line_samples = 0;
              if ((comp->expansion_counter.y <= 0) && (comp->dims.size.y > 0))
                can_proceed = false; // This could happen if some lines have an
                                // unusually large vertical registration offset
            }
          if (!can_proceed)
            continue;

          // Now see if we can output a new set of channel lines
          if (render_dims.pos.y == incomplete_bank_region.pos.y)
            { // This line has non-empty intersection with the incomplete region
              for (c=0; c < num_channels; c++)
                {
                  kd_channel *channel = channels + c;
                  kd_component *comp = channel->source;
                  if (comp->expansion_denominator.y == 1)
                    { // Just use `ref_line2' or a copy of it
                      if (!post_convert_colour)
                        { // No need to make a copy of the line for colour
                          // conversion to work on.
                          channel->cur_line = channel->ref_line2;
                        }
                      else if ((comp->expansion_counter.y <= 1) &&
                               (comp->dims.size.y > 0))
                        { // In this case, we can be sure that this line will
                          // not be used again; a new one is about to be
                          // produced.  Therefore, we don't have to worry about
                          // colour conversion overwriting it.
                          channel->cur_line = channel->ref_line2;
                        }
                      else
                        {
                          channel->cur_line = &(channel->lines[2]);
                          kdu_sample16 *dst16 = channel->cur_line->get_buf16();
                          kdu_sample16 *src16 = channel->ref_line2->get_buf16();
                          if (dst16 != NULL)
                            {
#ifdef KDU_SIMD_OPTIMIZATIONS
                              if (!simd_aligned_copy(src16,dst16,num_cols))
#endif // KDU_SIMD_OPTIMIZATIONS
                                memcpy(dst16,src16,(size_t)(num_cols*2));
                            }
                          else
                            memcpy(channel->cur_line->get_buf32(),
                                   channel->ref_line2->get_buf32(),
                                   (size_t)(num_cols*4));
                        }
                    }
                  else
                    { // `cur_line' is generated by interpolation
                      channel->cur_line = &(channel->lines[2]);
                      interpolate_between_lines(channel->ref_line1,
                                                channel->ref_line2,
                                                channel->cur_line,num_cols,
                                                comp->expansion_counter.y,
                                                comp->expansion_numerator.y);
                    }
                }
              if (post_convert_colour)
                { // In this case, we apply `colour_converter' once for each
                  // vertical output line.  We need to do this if the channels
                  // have different vertical interpolation properties.
                  if (num_colour_channels < 3)
                    colour_converter->convert_lum(*(channels[0].cur_line),
                                                  num_cols);
                  else if (num_colour_channels == 3)
                    colour_converter->convert_rgb(*(channels[0].cur_line),
                                                  *(channels[1].cur_line),
                                                  *(channels[2].cur_line),
                                                  num_cols);
                  else
                    colour_converter->convert_rgb4(*(channels[0].cur_line),
                                                   *(channels[1].cur_line),
                                                   *(channels[2].cur_line),
                                                   *(channels[3].cur_line),
                                                   num_cols);
                }

#ifdef KDU_SIMD_OPTIMIZATIONS
              if ((fast_base != NULL) &&
                  simd_interleaved_transfer(fast_base,fast_mask,num_cols,
                                            num_channel_bufs,precision_bits,
                                            channels[fast_source[0]].cur_line,
                                            channels[fast_source[1]].cur_line,
                                            channels[fast_source[2]].cur_line,
                                            channels[fast_source[3]].cur_line,
                                            fill_alpha))
                fast_base += row_gap;
              else
#endif // KDU_SIMD_OPTIMIZATIONS
              for (c=0; c < num_channel_bufs; c++)
                {
                  if (channel_bufs[c] == NULL)
                    continue;
                  kd_channel *chan = channels + c;

                  if ((num_active_channels > num_channels) && (c != 0))
                    {
                      assert(num_active_channels == (num_channels+2));
                      chan -= ((c==1)?1:2);
                    }
                  if ((c >= num_active_channels) && fill_alpha)
                    {
                      assert(sample_bytes == 1);
                      int n;
                      kdu_byte *dp = channel_bufs[c];
                      for (n=num_cols; n > 0; n--, dp+=pixel_gap)
                        *dp = 0xFF;
                    }
                  else if (precision_bits == 0)
                    { // Derive the precision information from `chan'
                      if (sample_bytes == 1)
                        transfer_fixed_point(chan->cur_line,num_cols,
                                    pixel_gap,channel_bufs[c],
                                    chan->native_precision,chan->native_signed);
                      else if (sample_bytes == 2)
                        transfer_fixed_point(chan->cur_line,num_cols,
                                    pixel_gap,(kdu_uint16 *)(channel_bufs[c]),
                                    chan->native_precision,chan->native_signed);
                      else
                        assert(0);
                    }
                  else
                    { // Typical case; write all samples as unsigned data
                      // with the same precision
                      if (sample_bytes == 1)
                        transfer_fixed_point(chan->cur_line,num_cols,
                                    pixel_gap,channel_bufs[c],
                                    precision_bits,false);
                      else if (sample_bytes == 2)
                        transfer_fixed_point(chan->cur_line,num_cols,
                                    pixel_gap,(kdu_uint16 *)(channel_bufs[c]),
                                    precision_bits,false);
                      else
                        assert(0);
                    }
                }

              for (c=0; c < num_channel_bufs; c++)
                if (channel_bufs[c] != NULL)
                  channel_bufs[c] += row_gap;

              incomplete_bank_region.pos.y++;
              incomplete_bank_region.size.y--;
              new_region.size.y++; // Transferred data region grows by one row.
              if (last_bank_in_row)
                {
                  int y = (render_dims.pos.y+1) - incomplete_region.pos.y;
                  assert(y > 0);
                  incomplete_region.pos.y += y;
                  incomplete_region.size.y -= y;
                }
            }

          for (c=0; c < num_components; c++)
            components[c].expansion_counter.y -=
              components[c].expansion_denominator.y;

          render_dims.pos.y++;
          render_dims.size.y--;
        }
      if (!incomplete_bank_region)
        { // Done all the processing we need for this tile.
          close_tile_bank(current_bank);
          current_bank = NULL;
          return true;
        }
    }
  catch (int exc)
    {
      codestream_failure = true;
      if (env != NULL)
        env->handle_exception(exc);
      return false;
    }
  return true;
}
