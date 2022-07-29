/*****************************************************************************/
// File: msvc_region_decompressor_local.h [scope = CORESYS/TRANSFORMS]
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
   Implements critical sample precision conversion and mapping functions
from `kdu_region_decompressor', using MMX/SSE/SSE2 intrinsics.  These can
be compiled under GCC or .NET and are compatible with both 32-bit and 64-bit
builds.  Note, however, that the SSE/SSE2 intrinsics require 16-byte
stack alignment, which might not be guaranteed by GCC builds if the
code is called from outside (e.g., from a Java Virtual Machine, which
has poorer stack alignment).  This is an ongoing problem with GCC, but
it does not affect stand-alone applications.
******************************************************************************/

#ifndef X86_REGION_DECOMPRESSOR_LOCAL_H
#define X86_REGION_DECOMPRESSOR_LOCAL_H

#include <emmintrin.h>

/*****************************************************************************/
/* INLINE                     simd_aligned_copy                              */
/*****************************************************************************/

inline bool
  simd_aligned_copy(kdu_sample16 *src, kdu_sample16 *dst, int num_samples)
  /* Takes advantage of the alignment properties of `kdu_line_buf' buffers,
     along with the fact that an additional 16 bytes can always be legally
     accessed for reading -- this is in addition to the smallest multiple
     of 16 bytes which covers the nominal buffer width. */
{
#ifdef KDU_NO_SSE
  return false;
#else // !KDU_NO_SSE
  if (kdu_mmx_level < 2)
    return false;

  __m128i *sp = (__m128i *) src;
  __m128i *dp = (__m128i *) dst;
  num_samples = (num_samples+7)>>3;
  for (int c=0; c < num_samples; c++)
    {
      __m128i val = _mm_load_si128(sp+c);
      _mm_store_si128(dp+c,val);
    }
  return true;
#endif // !KDU_NO_SSE
}

/*****************************************************************************/
/* INLINE                        simd_convert                                */
/*****************************************************************************/

inline bool
  simd_convert(kdu_sample16 *src, kdu_sample16 *dst, int num_samples,
               int upshift)
  /* Takes advantage of the alignment properties of `kdu_line_buf' buffers,
     along with the fact that an additional 16 bytes can always be legally
     accessed for reading -- this is in addition to the smallest multiple
     of 16 bytes which covers the nominal buffer width.  If `upshift' is
     negative, samples must be downshifted by -`upshift'. */
{
#ifdef KDU_NO_SSE
  return false;
#else // !KDU_NO_SSE
  if ((kdu_mmx_level < 2) || (num_samples < 16))
    return false; // Not worth special processing
  int lead=(-((_addr_to_kdu_int32(dst))>>1))&7; // Initial non-aligned samples
  for (; lead > 0; lead--, num_samples--, src++, dst++)
    dst->ival = (upshift>0)?(src->ival << upshift):(src->ival >> (-upshift));
  __m128i *sp = (__m128i *) src;
  __m128i *dp = (__m128i *) dst;
  num_samples = (num_samples+7)>>3;
  if (upshift <= 0)
    {
      __m128i shift = _mm_cvtsi32_si128(-upshift);
      for (int c=0; c < num_samples; c++)
        {
          __m128i val = _mm_loadu_si128(sp+c); // Unaligned 128-bit load
          val = _mm_sra_epi16(val,shift);
          _mm_store_si128(dp+c,val); // Aligned 128-bit store
        }
    }
  else
    {
      __m128i shift = _mm_cvtsi32_si128(upshift);
      for (int c=0; c < num_samples; c++)
        {
          __m128i val = _mm_loadu_si128(sp+c); // Unaligned 128-bit load
          val = _mm_sll_epi16(val,shift);
          _mm_store_si128(dp+c,val); // Aligned 128-bit store
        }
    }
  return true;
#endif // !KDU_NO_SSE
}

/*****************************************************************************/
/* INLINE                 simd_expand_x2_and_convert                         */
/*****************************************************************************/

inline bool
  simd_expand_x2_and_convert(kdu_sample16 *src, kdu_sample16 *dst,
                             int num_samples, int upshift)
{
#ifdef KDU_NO_SSE
  return false;
#else // !KDU_NO_SSE
  if ((kdu_mmx_level < 2) || (num_samples < 16))
    return false; // Not worth special processing
  int lead=(-((_addr_to_kdu_int32(dst))>>1))&7; // Initial non-aligned samples
  if (lead & 1)
    return false; // Need to write 2 aligned output words at a time
  for (; lead > 0; lead-=2, num_samples--, src++, dst+=2)
    {
      kdu_int16 val = src->ival;
      val = (upshift > 0)?(val << upshift):(val >> (-upshift));
      dst[0].ival = val;  dst[1].ival = val;
    }
  __m128i *sp = (__m128i *) src;
  __m128i *dp = (__m128i *) dst;
  num_samples = (num_samples+7)>>3;
  if (upshift <= 0)
    {
      __m128i shift = _mm_cvtsi32_si128(-upshift);
      for (int c=0; c < num_samples; c++)
        {
          __m128i val = _mm_loadu_si128(sp+c); // Unaligned 128-bit load
          val = _mm_sra_epi16(val,shift);
          __m128i low = _mm_unpacklo_epi16(val,val);
          _mm_store_si128(dp+2*c,low); // Aligned 128-bit store
          __m128i high = _mm_unpackhi_epi16(val,val);
          _mm_store_si128(dp+2*c+1,high); // Aligned 128-bit store
        }
    }
  else
    {
      __m128i shift = _mm_cvtsi32_si128(upshift);
      for (int c=0; c < num_samples; c++)
        {
          __m128i val = _mm_loadu_si128(sp+c); // Unaligned 128-bit load
          val = _mm_sll_epi16(val,shift);
          __m128i low = _mm_unpacklo_epi16(val,val);
          _mm_store_si128(dp+2*c,low); // Aligned 128-bit store
          __m128i high = _mm_unpackhi_epi16(val,val);
          _mm_store_si128(dp+2*c+1,high); // Aligned 128-bit store
        }
    }
  return true;
#endif // !KDU_NO_SSE
}

/*****************************************************************************/
/* INLINE                   simd_apply_white_stretch                         */
/*****************************************************************************/

inline bool
  simd_apply_white_stretch(kdu_sample16 *src, int num_samples,
                           int stretch_residual)
  /* Takes advantage of the alignment properties of `kdu_line_buf' buffers,
     to efficiently implement the white stretching algorithm for 16-bit
     channel buffers. */
{
#ifdef KDU_NO_SSE
  return false;
#else // !KDU_NO_SSE
  if ((kdu_mmx_level < 2) || (num_samples < 1))
    return false; // Not worth special processing
  kdu_int32 stretch_offset = -((-(stretch_residual<<(KDU_FIX_POINT-1))) >> 16);
  num_samples = (num_samples+7)>>3;
  if (stretch_residual <= 0x7FFF)
    { // Use full multiplication-based approach
      __m128i factor = _mm_set1_epi16((kdu_int16) stretch_residual);
      __m128i offset = _mm_set1_epi16((kdu_int16) stretch_offset);
      __m128i *sp = (__m128i *) src;
      for (int c=0; c < num_samples; c++)
        {
          __m128i val = sp[c];
          __m128i residual = _mm_mulhi_epi16(val,factor);
          val = _mm_add_epi16(val,offset);
          sp[c] = _mm_add_epi16(val,residual);
        }
    }
  else
    { // Large stretch residual -- can only happen with 1-bit original data
      int diff=(1<<16)-((int) stretch_residual), downshift = 1;
      while ((diff & 0x8000) == 0)
        { diff <<= 1; downshift++; }
      __m128i shift = _mm_cvtsi32_si128(downshift);
      __m128i offset = _mm_set1_epi16((kdu_int16) stretch_offset);
      __m128i *sp = (__m128i *) src;
      for (int c=0; c < num_samples; c++)
        {
          __m128i val = sp[c];
          __m128i twice_val = _mm_add_epi16(val,val);
          __m128i shifted_val = _mm_sra_epi16(val,shift);
          val = _mm_sub_epi16(twice_val,shifted_val);
          sp[c] = _mm_add_epi16(val,offset);
        }
    }
  return true;
#endif // !KDU_NO_SSE
}

/*****************************************************************************/
/* INLINE             simd_transfer_to_consecutive_bytes                     */
/*****************************************************************************/

inline bool
  simd_transfer_to_consecutive_bytes(kdu_sample16 *src, kdu_byte *dst,
                                     int num_samples, kdu_int16 offset,
                                     int downshift, kdu_int16 mask)
{
  if ((kdu_mmx_level < 2) || (num_samples < 16))
    return false;

  kdu_int32 vec_offset = offset;  vec_offset += vec_offset << 16;
  kdu_int32 vec_max = (kdu_int32)(~mask);  vec_max += vec_max << 16;
  int c, num_blocks = num_samples >> 4; // Number of whole 16-sample blocks
  __m128i *sp = (__m128i *) src;
  __m128i *dp = (__m128i *) dst;
  __m128i voff = _mm_cvtsi32_si128(vec_offset);
  voff = _mm_shuffle_epi32(voff,0);
  __m128i vmax = _mm_cvtsi32_si128(vec_max);
  vmax = _mm_shuffle_epi32(vmax,0);
  __m128i vmin = _mm_setzero_si128();
  __m128i shift = _mm_cvtsi32_si128(downshift);
  for (c=0; c < num_blocks; c++)
    {
      __m128i low = _mm_load_si128(sp+2*c);
      low = _mm_add_epi16(low,voff); // Add the offset
      low = _mm_sra_epi16(low,shift); // Apply the downshift
      low = _mm_max_epi16(low,vmin); // Clip to min value of 0
      low = _mm_min_epi16(low,vmax); // Clip to max value of `mask'
      __m128i high = _mm_load_si128(sp+2*c+1);
      high = _mm_add_epi16(high,voff); // Add the offset
      high = _mm_sra_epi16(high,shift); // Apply the downshift
      high = _mm_max_epi16(high,vmin); // Clip to min value of 0
      high = _mm_min_epi16(high,vmax); // Clip to max value of `mask'
      __m128i packed = _mm_packus_epi16(low,high);
      _mm_storeu_si128(dp+c,packed);
    }
  for (c=num_blocks<<4; c < num_samples; c++)
    {
      kdu_int16 val = (src[c].ival+offset)>>downshift;
      if (val & mask)
        val = (val < 0)?0:~mask;
      dst[c] = (kdu_byte) val;
    }
  return true;
}

/*****************************************************************************/
/* INLINE                  simd_interleaved_transfer                         */
/*****************************************************************************/

inline bool
  simd_interleaved_transfer(kdu_byte *dst_base, kdu_byte channel_mask[],
                            int num_pixels, int num_channels, int prec_bits,
                            kdu_line_buf *src_line0, kdu_line_buf *src_line1,
                            kdu_line_buf *src_line2, kdu_line_buf *src_line3,
                            bool fill_alpha)
  /* This function performs the transfer of processed short integers to
     output channel bytes all at once, assuming interleaved output channels.
     Notionally, there are 4 output channels, whose first samples are at
     locations `dst_base' + k, for k=0,1,2,3.  The corresponding source
     line buffers are identified by `src_line0' through `src_line3'.  The
     actual number of real output channels is identified by `num_channels';
     these are the ones with location `dst_base'+k where `channel_mask'[k]
     is 0xFF.  All other locations have `channel_mask'[k]=0 -- nothing will
     be written to those locations, but the corresponding `src_line'
     argument is still usable (although not unique).  This allows for
     simple generic implementations.  If `fill_alpha' is true, the last
     channel is simply to be filled with 0xFF; it's mask is irrelevant
     and there is no need to read from the corresponding source line. */
{
#ifdef KDU_NO_SSE
  return false;
#else // !KDU_NO_SSE
  assert(num_channels <= 4);
  int cm_val = ((int *) channel_mask)[0]; // Load all 4 mask bytes into 1 dword
  if ((kdu_mmx_level < 2) || ((cm_val & 0x00FFFFFF) != 0x00FFFFFF))
    return false; // Present implementation assumes first 3 channels are used
  cm_val >>= 24;  // Keep just the last source mask byte
  int downshift = KDU_FIX_POINT-prec_bits;
  kdu_int16 offset = (1<<downshift)>>1; // Rounding offset for the downshift
  offset += (1<<KDU_FIX_POINT)>>1; // Convert from signed to unsigned
  kdu_int16 mask = (kdu_int16)((-1) << prec_bits);
  kdu_int32 vec_offset = offset;  vec_offset += vec_offset << 16;
  kdu_int32 vec_max = (kdu_int32)(~mask);  vec_max += vec_max << 16;

  kdu_sample16 *src0 = src_line0->get_buf16();
  kdu_sample16 *src1 = src_line1->get_buf16();
  kdu_sample16 *src2 = src_line2->get_buf16();
  kdu_sample16 *src3 = src_line3->get_buf16();

  int c, num_blocks = num_pixels>>3;
  __m128i *dp  = (__m128i *) dst_base;
  __m128i *sp0 = (__m128i *) src0;
  __m128i *sp1 = (__m128i *) src1;
  __m128i *sp2 = (__m128i *) src2;
  __m128i shift = _mm_cvtsi32_si128(downshift);
  __m128i voff = _mm_cvtsi32_si128(vec_offset);
  voff = _mm_shuffle_epi32(voff,0); // Create 8 replicas of offset in `voff'
  __m128i vmax = _mm_cvtsi32_si128(vec_max);
  vmax = _mm_shuffle_epi32(vmax,0); // Create 8 replicas of max value in `vmax'
  if (fill_alpha)
    { // Three source channels available; overwrite alpha channel with FF
      __m128i vmin = _mm_setzero_si128();
      __m128i mask = _mm_setzero_si128(); // Avoid compiler warnings
      mask = _mm_cmpeq_epi16(mask,mask); // Set all bits to 1
      mask = _mm_slli_epi16(mask,8); // Leaves 0xFF in MSB of each 16-bit word
      for (c=0; c < num_blocks; c++)
        {
          __m128i val0 = _mm_load_si128(sp0+c);
          val0 = _mm_add_epi16(val0,voff); // Add the offset
          val0 = _mm_sra_epi16(val0,shift); // Apply the downshift
          val0 = _mm_max_epi16(val0,vmin); // Clip to min value of 0
          val0 = _mm_min_epi16(val0,vmax); // Clip to max value of `mask'
          __m128i val1 = _mm_load_si128(sp1+c);
          val1 = _mm_add_epi16(val1,voff); // Add the offset
          val1 = _mm_sra_epi16(val1,shift); // Apply the downshift
          val1 = _mm_max_epi16(val1,vmin); // Clip to min value of 0
          val1 = _mm_min_epi16(val1,vmax); // Clip to max value of `mask'
          val1 = _mm_slli_epi16(val1,8);
          val1 = val0 = _mm_or_si128(val0,val1); // Interleave val0 and val1
          __m128i val2 = _mm_load_si128(sp2+c);
          val2 = _mm_add_epi16(val2,voff); // Add the offset
          val2 = _mm_sra_epi16(val2,shift); // Apply the downshift
          val2 = _mm_max_epi16(val2,vmin); // Clip to min value of 0
          val2 = _mm_min_epi16(val2,vmax); // Clip to max value of `mask'
          val2 = _mm_or_si128(val2,mask); // Fill alpha channel with FF

          val0 = _mm_unpacklo_epi16(val0,val2);
          _mm_storeu_si128(dp+2*c,val0); // Write low pixel*4

          val1 = _mm_unpackhi_epi16(val1,val2);
          _mm_storeu_si128(dp+2*c+1,val1); // Write high pixel*4
        }
    }
  else if (cm_val == 0)
    { // Three source channels available; leave the alpha channel unchanged
      __m128i mask = _mm_setzero_si128(); // Avoid compiler warnings
      mask = _mm_cmpeq_epi16(mask,mask); // Set all bits to 1
      mask = _mm_slli_epi32(mask,24); // Leaves 0xFF in MSB of each 32-bit word
      for (c=0; c < num_blocks; c++)
        {
          __m128i multival = _mm_setzero_si128();
          __m128i val0 = _mm_load_si128(sp0+c);
          val0 = _mm_add_epi16(val0,voff); // Add the offset
          val0 = _mm_sra_epi16(val0,shift); // Apply the downshift
          val0 = _mm_max_epi16(val0,multival); // Clip to min value of 0
          val0 = _mm_min_epi16(val0,vmax); // Clip to max value of `mask'
          __m128i val1 = _mm_load_si128(sp1+c);
          val1 = _mm_add_epi16(val1,voff); // Add the offset
          val1 = _mm_sra_epi16(val1,shift); // Apply the downshift
          val1 = _mm_max_epi16(val1,multival); // Clip to min value of 0
          val1 = _mm_min_epi16(val1,vmax); // Clip to max value of `mask'
          val1 = _mm_slli_epi16(val1,8);
          val1 = val0 = _mm_or_si128(val0,val1); // Interleave val0 and val1
          __m128i val2 = _mm_load_si128(sp2+c);
          val2 = _mm_add_epi16(val2,voff); // Add the offset
          val2 = _mm_sra_epi16(val2,shift); // Apply the downshift
          val2 = _mm_max_epi16(val2,multival); // Clip to min value of 0
          val2 = _mm_min_epi16(val2,vmax); // Clip to max value of `mask'

          multival = _mm_loadu_si128(dp+2*c); // Load existing low pixel*4
          multival = _mm_and_si128(multival,mask); // Mask off all but alpha
          val0 = _mm_unpacklo_epi16(val0,val2);
          val0 = _mm_or_si128(val0,multival); // Blend existing alpha into val0
          _mm_storeu_si128(dp+2*c,val0); // Write new low pixel*4

          multival = _mm_loadu_si128(dp+2*c+1); // Load existing high pixel*4
          multival = _mm_and_si128(multival,mask); // Mask off all but alpha
          val1 = _mm_unpackhi_epi16(val1,val2);
          val1 = _mm_or_si128(val1,multival); // Blend existing alpha into val0
          _mm_storeu_si128(dp+2*c+1,val1); // Write new high pixel*4
        }
    }
  else
    { // Four source channels available
      __m128i vmin = _mm_setzero_si128();
      __m128i *sp3 = (__m128i *) src3;
      for (c=0; c < num_blocks; c++)
        {
          __m128i val0 = _mm_load_si128(sp0+c);
          val0 = _mm_add_epi16(val0,voff); // Add the offset
          val0 = _mm_sra_epi16(val0,shift); // Apply the downshift
          val0 = _mm_max_epi16(val0,vmin); // Clip to min value of 0
          val0 = _mm_min_epi16(val0,vmax); // Clip to max value of `mask'
          __m128i val1 = _mm_load_si128(sp1+c);
          val1 = _mm_add_epi16(val1,voff); // Add the offset
          val1 = _mm_sra_epi16(val1,shift); // Apply the downshift
          val1 = _mm_max_epi16(val1,vmin); // Clip to min value of 0
          val1 = _mm_min_epi16(val1,vmax); // Clip to max value of `mask'
          val1 = _mm_slli_epi16(val1,8);
          val1 = val0 = _mm_or_si128(val0,val1); // Interleave val0 and val1
          __m128i val2 = _mm_load_si128(sp2+c);
          val2 = _mm_add_epi16(val2,voff); // Add the offset
          val2 = _mm_sra_epi16(val2,shift); // Apply the downshift
          val2 = _mm_max_epi16(val2,vmin); // Clip to min value of 0
          val2 = _mm_min_epi16(val2,vmax); // Clip to max value of `mask'
          __m128i val3 = _mm_load_si128(sp3+c);
          val3 = _mm_add_epi16(val3,voff); // Add the offset
          val3 = _mm_sra_epi16(val3,shift); // Apply the downshift
          val3 = _mm_max_epi16(val3,vmin); // Clip to min value of 0
          val3 = _mm_min_epi16(val3,vmax); // Clip to max value of `mask'
          val3 = _mm_slli_epi16(val3,8);
          val2 = _mm_or_si128(val2,val3);

          val0 = _mm_unpacklo_epi16(val0,val2);
          _mm_storeu_si128(dp+2*c,val0); // Write low pixel*4

          val1 = _mm_unpackhi_epi16(val1,val2);
          _mm_storeu_si128(dp+2*c+1,val1); // Write high pixel*4
        }
    }

  for (c=num_blocks<<3; c < num_pixels; c++)
    { // Need to generate trailing samples one-by-one
      kdu_int16 val;

      val = (src0[c].ival+offset) >> downshift;
      if (val & mask)
        val = (val < 0)?0:~mask;
      dst_base[4*c+0] = (kdu_byte) val;

      val = (src1[c].ival+offset) >> downshift;
      if (val & mask)
        val = (val < 0)?0:~mask;
      dst_base[4*c+1] = (kdu_byte) val;

      val = (src2[c].ival+offset) >> downshift;
      if (val & mask)
        val = (val < 0)?0:~mask;
      dst_base[4*c+2] = (kdu_byte) val;
      
      if (fill_alpha)
        dst_base[4*c+3] = 0xFF;
      else if (cm_val)
        {
          val = (src3[c].ival+offset) >> downshift;
          if (val & mask)
            val = (val < 0)?0:~mask;
          dst_base[4*c+3] = (kdu_byte) val;
        }
    }

  return true;
#endif // !KDU_NO_SSE
}

#endif // X86_REGION_DECOMPRESSOR_LOCAL_H
