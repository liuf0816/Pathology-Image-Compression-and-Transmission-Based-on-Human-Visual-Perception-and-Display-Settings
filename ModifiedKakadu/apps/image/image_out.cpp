/*****************************************************************************/
// File: image_out.cpp [scope = APPS/IMAGE-IO]
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
   Implements image file writing for a variety of different file formats:
currently BMP, PGM, PPM and RAW only.  Readily extendible to include other file
formats without affecting the rest of the system.
******************************************************************************/

// System includes
#include <string.h>
#include <math.h>
#include <assert.h>
// Core includes
#include "kdu_messaging.h"
#include "kdu_sample_processing.h"
// Image includes
#include "kdu_image.h"
#include "image_local.h"

/* ========================================================================= */
/*                             Internal Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* INLINE                       to_little_endian                             */
/*****************************************************************************/

static void
  to_little_endian(kdu_int32 * words, int num_words)
{
  kdu_int32 test = 1;
  kdu_byte *first_byte = (kdu_byte *) &test;
  if (*first_byte)
    return; // Machine uses little-endian architecture already.
  kdu_int32 tmp;
  for (; num_words--; words++)
    {
      tmp = *words;
      *words = ((tmp >> 24) & 0x000000FF) +
               ((tmp >> 8)  & 0x0000FF00) +
               ((tmp << 8)  & 0x00FF0000) +
               ((tmp << 24) & 0xFF000000);
    }
}

/*****************************************************************************/
/* INLINE                      from_little_endian                            */
/*****************************************************************************/

static inline void
  from_little_endian(kdu_int32 * words, int num_words)
{
  to_little_endian(words,num_words);
}

/*****************************************************************************/
/* STATIC                    convert_floats_to_bytes                         */
/*****************************************************************************/

static void
  convert_floats_to_bytes(kdu_sample32 *src, kdu_byte *dest, int num,
                          int precision, int orig_precision,
                          bool is_signed, int sample_gap)
/* If `orig_precision' differs from `precision' it will be less than
 `precision'.  In this case, the data samples are effectively divided
 by 2^{precision-orig_precision} as they are being written out.  If the
 samples have an originally unsigned representation, we add
 a correspondingly reduced offset to bring the samples to the unsigned
 output representation. */
{
  float scale16 = (float)(1<<16);
  kdu_int32 val;

  if ((precision >= 8) && (precision == orig_precision))
    {
      for (; num > 0; num--, src++, dest+=sample_gap)
        {
          val = (kdu_int32)(src->fval*scale16);
          val = (val+128)>>8; // Often faster than true rounding from floats.
          val += 128;
          if (val & ((-1)<<8))
            val = (val<0)?0:255;
          *dest = (kdu_byte) val;
        }
    }
  else
    { // Need to force zeros into one or more least significant bits.
      kdu_int32 downshift = 16-orig_precision;
      kdu_int32 upshift = 8-precision;
      kdu_int32 offset = 1<<(downshift-1);
      kdu_int32 post_offset = 128;
      if (precision != orig_precision)
        {
          assert((orig_precision < precision) && (precision <= 8));
          if (!is_signed)
            post_offset = (kdu_int32)(1<<(7+orig_precision-precision));
        }
      for (; num > 0; num--, src++, dest+=sample_gap)
        {
          val = (kdu_int32)(src->fval*scale16);
          val = (val+offset)>>downshift;
          val <<= upshift;
          val += post_offset;
          if (val & ((-1)<<8))
            val = (val<0)?0:(256-(1<<upshift));
          *dest = (kdu_byte) val;
        }
    }
}

/*****************************************************************************/
/* STATIC                   convert_fixpoint_to_bytes                        */
/*****************************************************************************/

static void
  convert_fixpoint_to_bytes(kdu_sample16 *src, kdu_byte *dest, int num,
                            int precision, int orig_precision,
                            bool is_signed, int sample_gap)
/* If `orig_precision' differs from `precision' it will be less than
 `precision'.  In this case, the data samples are effectively divided
 by 2^{precision-orig_precision} as they are being written out.  If the
 samples have an originally unsigned representation, we add
 a correspondingly reduced offset to bring the samples to the unsigned
 output representation. */
{
  kdu_int16 val;

  if ((precision >= 8) && (precision == orig_precision))
    {
      kdu_int16 downshift = KDU_FIX_POINT-8;
      if (downshift < 0)
        { kdu_error e; e << "Cannot use 16-bit representation with high "
          "bit-depth data"; }
      kdu_int16 offset = (1<<downshift)>>1;
      for (; num > 0; num--, src++, dest+=sample_gap)
        {
          val = src->ival;
          val = (val + offset) >> (KDU_FIX_POINT-8);
          val += 128;
          if (val & ((-1)<<8))
            val = (val<0)?0:255;
          *dest = (kdu_byte) val;
        }
    }
  else
    { // Need to force zeros into one or more least significant bits.
      kdu_int16 downshift = KDU_FIX_POINT-orig_precision;
      if (downshift < 0)
        { kdu_error e; e << "Cannot use 16-bit representation with high "
          "bit-depth data"; }
      kdu_int16 upshift = 8-precision;
      kdu_int16 offset = (1<<downshift)>>1;
      kdu_int16 post_offset = 128;
      if (precision != orig_precision)
        {
          assert((orig_precision < precision) && (precision <= 8));
          if (!is_signed)
            post_offset = (kdu_int16)(1<<(7+orig_precision-precision));
        }
      for (; num > 0; num--, src++, dest+=sample_gap)
        {
          val = src->ival;
          val = (val+offset)>>downshift;
          val <<= upshift;
          val += post_offset;
          if (val & ((-1)<<8))
            val = (val<0)?0:(256-(1<<upshift));
          *dest = (kdu_byte) val;
        }
    }
}

/*****************************************************************************/
/* STATIC                     convert_ints_to_bytes                          */
/*****************************************************************************/

static void
  convert_ints_to_bytes(kdu_sample32 *src, kdu_byte *dest, int num,
                        int precision, int orig_precision,
                        bool is_signed, int sample_gap)
/* If `precision' differs from `orig_precision', the `precision' is being
 forced.  This only affects the writing process if `is_signed' is false,
 in which case, the offset that we add to convert from the internal signed
 representation to the written unsigned representation is based on 
 `orig_precision' rather than `precision'. */
{
  kdu_int32 val;

  if ((precision >= 8) && (orig_precision == precision))
    {
      kdu_int32 downshift = precision-8;
      kdu_int32 offset = (1<<downshift)>>1;

      for (; num > 0; num--, src++, dest+=sample_gap)
        {
          val = src->ival;
          val = (val+offset)>>downshift;
          val += 128;
          if (val & ((-1)<<8))
            val = (val<0)?0:255;
          *dest = (kdu_byte) val;
        }
    }
  else
    {
      kdu_int32 upshift = 8-precision;
      kdu_int32 offset = 128;
      if ((precision != orig_precision) && !is_signed)
        {
          assert((precision > orig_precision) && (precision <= 8));
          offset = 1 << (orig_precision+upshift-1);
        }
      for (; num > 0; num--, src++, dest+=sample_gap)
        {
          val = src->ival;
          val <<= upshift;
          val += offset;
          if (val & ((-1)<<8))
            val = (val<0)?0:(256-(1<<upshift));
          *dest = (kdu_byte) val;
        }
    }
}

/*****************************************************************************/
/* STATIC                    convert_shorts_to_bytes                         */
/*****************************************************************************/

static void
  convert_shorts_to_bytes(kdu_sample16 *src, kdu_byte *dest, int num,
                          int precision, int orig_precision,
                          bool is_signed, int sample_gap)
/* If `precision' differs from `orig_precision', the `precision' is being
 forced.  This only affects the writing process if `is_signed' is false,
 in which case, the offset that we add to convert from the internal signed
 representation to the written unsigned representation is based on 
 `orig_precision' rather than `precision'. */
{
  kdu_int16 val;

  if ((precision >= 8) && (orig_precision == precision))
    {
      kdu_int16 downshift = precision-8;
      kdu_int16 offset = (1<<downshift)>>1;

      for (; num > 0; num--, src++, dest+=sample_gap)
        {
          val = src->ival;
          val = (val+offset)>>downshift;
          val += 128;
          if (val & ((-1)<<8))
            val = (val<0)?0:255;
          *dest = (kdu_byte) val;
        }
    }
  else
    {
      kdu_int16 upshift = 8-precision;
      kdu_int16 offset = 128;
      if ((precision != orig_precision) && !is_signed)
        {
          assert((precision > orig_precision) && (precision <= 8));
          offset = (kdu_int16)(1 << (orig_precision+upshift-1));
        }
      for (; num > 0; num--, src++, dest+=sample_gap)
        {
          val = src->ival;
          val <<= upshift;
          val += offset;
          if (val & ((-1)<<8))
            val = (val<0)?0:(256-(1<<upshift));
          *dest = (kdu_byte) val;
        }
    }
}

/*****************************************************************************/
/* STATIC                    convert_floats_to_words                         */
/*****************************************************************************/

static void
  convert_floats_to_words(kdu_sample32 *src, kdu_byte *dest, int num,
                          int precision, int orig_precision, bool is_signed,
                          int sample_bytes, bool littlendian,
                          int inter_sample_bytes=0)
/* If `orig_precision' differs from `precision' the precision is being forced
 to `precision' which must be greater than `orig_precision'. */
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  kdu_int32 val;
  float scale, min, max, fval, offset;

  if (orig_precision < 30)
    scale = (float)(1<<orig_precision);
  else
    scale = ((float)(1<<30)) * ((float)(1<<(orig_precision-30)));
  offset = (is_signed)?0.0F:0.5F;
  offset = offset * scale + 0.5F;
  min = -0.5F;  max = 0.5F;
  if (precision != orig_precision)
    {
      assert(precision > orig_precision);
      int rel_prec = precision-orig_precision;
      float rel_scale;
      if (rel_prec < 30)
        rel_scale = (float)(1<<rel_prec);
      else
        rel_scale = ((float)(1<<30)) * ((float)(1<<(rel_prec-30)));
      if (is_signed)
        { min = -0.5F*rel_scale;  max = 0.5F*rel_scale; }
      else
        max = rel_scale + min;
    }
  max -= 1.0F / scale;

  if (sample_bytes == 1)
    for (; num > 0; num--, src++, dest+=inter_sample_bytes)
      {
        fval = src->fval;
        fval = (fval>min)?fval:min;
        fval = (fval<max)?fval:max;
        val = (kdu_int32) floor(scale*fval + offset);
        dest[0] = (kdu_byte) val;
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            fval = src->fval;
            fval = (fval>min)?fval:min;
            fval = (fval<max)?fval:max;
            val = (kdu_int32) floor(scale*fval + offset);
            dest[0] = (kdu_byte)(val>>8);
            dest[1] = (kdu_byte) val;
          }
      else
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            fval = src->fval;
            fval = (fval>min)?fval:min;
            fval = (fval<max)?fval:max;
            val = (kdu_int32) floor(scale*fval + offset);
            dest[0] = (kdu_byte) val;
            dest[1] = (kdu_byte)(val>>8);
          }
    }
  else if (sample_bytes == 3)
    {
      if (!littlendian)
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            fval = src->fval;
            fval = (fval>min)?fval:min;
            fval = (fval<max)?fval:max;
            val = (kdu_int32) floor(scale*fval + offset);
            dest[0] = (kdu_byte)(val>>16);
            dest[1] = (kdu_byte)(val>>8);
            dest[2] = (kdu_byte) val;
          }
      else
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            fval = src->fval;
            fval = (fval>min)?fval:min;
            fval = (fval<max)?fval:max;
            val = (kdu_int32) floor(scale*fval + offset);
            dest[0] = (kdu_byte) val;
            dest[1] = (kdu_byte)(val>>8);
            dest[2] = (kdu_byte)(val>>16);
          }
    }
  else if (sample_bytes == 4)
    {
      if (!littlendian)
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            fval = src->fval;
            fval = (fval>min)?fval:min;
            fval = (fval<max)?fval:max;
            val = (kdu_int32) floor(scale*fval + offset);
            dest[0] = (kdu_byte)(val>>24);
            dest[1] = (kdu_byte)(val>>16);
            dest[2] = (kdu_byte)(val>>8);
            dest[3] = (kdu_byte) val;
          }
      else
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            fval = src->fval;
            fval = (fval>min)?fval:min;
            fval = (fval<max)?fval:max;
            val = (kdu_int32) floor(scale*fval + offset);
            dest[0] = (kdu_byte) val;
            dest[1] = (kdu_byte)(val>>8);
            dest[2] = (kdu_byte)(val>>16);
            dest[3] = (kdu_byte)(val>>24);
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/* STATIC                   convert_fixpoint_to_words                        */
/*****************************************************************************/

static void
  convert_fixpoint_to_words(kdu_sample16 *src, kdu_byte *dest, int num,
                            int precision, int orig_precision,
                            bool is_signed, int sample_bytes,
                            bool littlendian, int inter_sample_bytes=0)
/* If `orig_precision' differs from `precision' the precision is being forced
 to `precision' which must be greater than `orig_precision'. */
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  kdu_int32 val;
  kdu_int32 min, max;
  kdu_int32 downshift = KDU_FIX_POINT-orig_precision;
  if (downshift < 0)
    { kdu_error e; e << "Cannot use 16-bit representation with high "
      "bit-depth data"; }
  kdu_int32 offset = 1<<downshift;
  offset += (is_signed)?0:(1<<KDU_FIX_POINT);
  offset >>= 1;
  max = 1<<(KDU_FIX_POINT-1);  min = -max;
  if (precision != orig_precision)
    {
      assert(precision > orig_precision);
      if (is_signed)
        { max = 1<<(KDU_FIX_POINT-1+precision-orig_precision);  min = -max; }
      else
        max = (1<<(KDU_FIX_POINT+precision-orig_precision)) + min;
    }  
  max -= 1<<downshift;
  
  if (sample_bytes == 1)
    for (; num > 0; num--, src++, dest+=inter_sample_bytes)
      {
        val = src->ival;
        val = (val < min)?min:val;
        val = (val >= max)?max:val;
        val = (val+offset)>>downshift;
        dest[0] = (kdu_byte) val;
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val < min)?min:val;
            val = (val >= max)?max:val;
            val = (val+offset)>>downshift;
            dest[0] = (kdu_byte)(val>>8);
            dest[1] = (kdu_byte) val;
          }
      else
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val < min)?min:val;
            val = (val >= max)?max:val;
            val = (val+offset)>>downshift;
            dest[0] = (kdu_byte) val;
            dest[1] = (kdu_byte)(val>>8);
          }
    }
  else
    { kdu_error e; e << "Cannot use 16-bit fixed-point represetation for "
      "sample data processing, with high bit-depth decompressed data.  "
      "You may be receiving this error because you are trying to force "
      "a significant increase in the output file's sample bit-depth using "
      "the `-fprec' option to \"kdu_expand\".  If so, you should supply "
      "the `-precise' option as well, to increase the internal "
      "processing precision."; }
}

/*****************************************************************************/
/* STATIC                     convert_ints_to_words                          */
/*****************************************************************************/

static void
  convert_ints_to_words(kdu_sample32 *src, kdu_byte *dest, int num,
                        int precision, int orig_precision, bool is_signed,
                        int sample_bytes, bool littlendian,
                        int inter_sample_bytes=0)
/* If `orig_precision' differs from `precision' the precision is being forced
 to `precision' which must be greater than `orig_precision'. */
{
  kdu_int32 val, min, max, offset;

  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  offset = (is_signed)?0:(1<<(orig_precision-1));
  min = -(1<<(orig_precision-1));
  if (precision != orig_precision)
    {
      assert(precision > orig_precision);
      if (is_signed)
        min = -(1<<(precision-1));
    }
  max = (1<<precision)+min-1;
  
  if (sample_bytes == 1)
    for (; num > 0; num--, src++, dest+=inter_sample_bytes)
      {
        val = src->ival;
        val = (val>min)?val:min;
        val = (val<max)?val:max;
        val += offset;
        dest[0] = (kdu_byte) val;
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val>min)?val:min;
            val = (val<max)?val:max;
            val += offset;
            dest[0] = (kdu_byte)(val>>8);
            dest[1] = (kdu_byte) val;
          }
      else
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val>min)?val:min;
            val = (val<max)?val:max;
            val += offset;
            dest[0] = (kdu_byte) val;
            dest[1] = (kdu_byte)(val>>8);
          }
    }
  else if (sample_bytes == 3)
    {
      if (!littlendian)
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val>min)?val:min;
            val = (val<max)?val:max;
            val += offset;
            dest[0] = (kdu_byte)(val>>16);
            dest[1] = (kdu_byte)(val>>8);
            dest[2] = (kdu_byte) val;
          }
      else
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val>min)?val:min;
            val = (val<max)?val:max;
            val += offset;
            dest[0] = (kdu_byte) val;
            dest[1] = (kdu_byte)(val>>8);
            dest[2] = (kdu_byte)(val>>16);
          }
    }
  else if (sample_bytes == 4)
    {
      if (!littlendian)
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val>min)?val:min;
            val = (val<max)?val:max;
            val += offset;
            dest[0] = (kdu_byte)(val>>24);
            dest[1] = (kdu_byte)(val>>16);
            dest[2] = (kdu_byte)(val>>8);
            dest[3] = (kdu_byte) val;
          }
      else
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val>min)?val:min;
            val = (val<max)?val:max;
            val += offset;
            dest[0] = (kdu_byte) val;
            dest[1] = (kdu_byte)(val>>8);
            dest[2] = (kdu_byte)(val>>16);
            dest[3] = (kdu_byte)(val>>24);
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/* STATIC                    convert_shorts_to_words                         */
/*****************************************************************************/

static void
  convert_shorts_to_words(kdu_sample16 *src, kdu_byte *dest, int num,
                          int precision, int orig_precision, bool is_signed,
                          int sample_bytes, bool littlendian,
                          int inter_sample_bytes=0)
/* If `orig_precision' differs from `precision' the precision is being forced
 to `precision' which must be greater than `orig_precision'. */
{
  kdu_int32 val, min, max, offset;
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  
  offset = (is_signed)?0:(1<<(orig_precision-1));
  min = -(1<<(orig_precision-1));
  if (precision != orig_precision)
    {
      assert(precision > orig_precision);
      if (is_signed)
        min = -(1<<(precision-1));
    }
  max = (1<<precision)+min-1;

  if (sample_bytes == 1)
    for (; num > 0; num--, src++, dest+=inter_sample_bytes)
      {
        val = src->ival;
        val = (val>min)?val:min;
        val = (val<max)?val:max;
        val += offset;
        dest[0] = (kdu_byte) val;
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val>min)?val:min;
            val = (val<max)?val:max;
            val += offset;
            dest[0] = (kdu_byte)(val>>8);
            dest[1] = (kdu_byte) val;
          }
      else
        for (; num > 0; num--, src++, dest+=inter_sample_bytes)
          {
            val = src->ival;
            val = (val>min)?val:min;
            val = (val<max)?val:max;
            val += offset;
            dest[0] = (kdu_byte) val;
            dest[1] = (kdu_byte)(val>>8);
          }
    }
  else
    { kdu_error e; e << "Cannot use 16-bit representation with high "
      "bit-depth data"; }
}


/* ========================================================================= */
/*                                kdu_image_out                              */
/* ========================================================================= */

/*****************************************************************************/
/*                        kdu_image_out::kdu_image_out                       */
/*****************************************************************************/

kdu_image_out::kdu_image_out(const char *fname, kdu_image_dims &dims,
                             int &next_comp_idx, bool &vflip)
{
  const char *suffix;

  out = NULL;
  vflip = false;
  if ((suffix = strrchr(fname,'.')) != NULL)
    {
      if ((strcmp(suffix+1,"pgm") == 0) || (strcmp(suffix+1,"PGM") == 0))
        out = new pgm_out(fname,dims,next_comp_idx);
      else if ((strcmp(suffix+1,"ppm") == 0) || (strcmp(suffix+1,"PPM") == 0))
        out = new ppm_out(fname,dims,next_comp_idx);
      else if ((strcmp(suffix+1,"bmp") == 0) || (strcmp(suffix+1,"BMP") == 0))
        { vflip = true; out = new bmp_out(fname,dims,next_comp_idx); }
      else if ((strcmp(suffix+1,"raw") == 0) || (strcmp(suffix+1,"RAW") == 0))
        out = new raw_out(fname,dims,next_comp_idx,false);
      else if ((strcmp(suffix+1,"rawl") == 0) || (strcmp(suffix+1,"RAWL")==0))
        out = new raw_out(fname,dims,next_comp_idx,true);
      else if ((strcmp(suffix+1,"tif")==0) || (strcmp(suffix+1,"TIF")==0) ||
               (strcmp(suffix+1,"tiff")==0) || (strcmp(suffix+1,"TIFF")==0))
        out = new tif_out(fname,dims,next_comp_idx);
    }
  if (out == NULL)
    { kdu_error e; e << "Image file, \"" << fname << ", does not have a "
      "recognized suffix.  Valid suffices are currently: "
      "\"bmp\", \"pgm\", \"ppm\", \"tif\", \"tiff\", \"raw\" and \"rawl\".  "
      "Upper or lower case may be used, but must be used consistently.";
    }
}


/* ========================================================================= */
/*                                  pgm_out                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                              pgm_out::pgm_out                             */
/*****************************************************************************/

pgm_out::pgm_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx)
{
  comp_idx = next_comp_idx++;
  if (comp_idx >= dims.get_num_components())
    { kdu_error e; e << "Output image files require more image components "
      "(or mapped colour channels) than are available!"; }
  rows = dims.get_height(comp_idx);
  cols = dims.get_width(comp_idx);
  precision = orig_precision = dims.get_bit_depth(comp_idx);
  int forced_prec = dims.get_forced_precision(comp_idx);
  if (forced_prec != 0)
    {
      if ((forced_prec >= precision) && (forced_prec <= 8))
        precision = forced_prec;
      else
        { kdu_error w; w << "Forced precision supplied via `-fprec' argument "
          "cannot be adopted by the PGM file writer unless it is at least "
          "as large as the nominal precision (from Sprecision or Mprecision) "
          "and no larger than 8 bits.  Ignoring the forced precision, for "
          "component " << comp_idx << ".";
        }
    }
  orig_signed = dims.get_signed(comp_idx);
  if (orig_signed)
    { kdu_warning w;
      w << "Signed sample values will be written to the PGM file as unsigned "
           "8-bit quantities, centered about 128.";
    }
  if ((out = fopen(fname,"wb")) == NULL)
    { kdu_error e;
      e << "Unable to open output image file, \"" << fname <<"\"."; }
  fprintf(out,"P5\n%d %d\n255\n",cols,rows);
  incomplete_lines = free_lines = NULL;
  num_unwritten_rows = rows;
  initial_non_empty_tiles = 0; // Don't know yet.
}

/*****************************************************************************/
/*                              pgm_out::~pgm_out                            */
/*****************************************************************************/

pgm_out::~pgm_out()
{
  if ((num_unwritten_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image component "
        << comp_idx << " were completed!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  fclose(out);
}

/*****************************************************************************/
/*                                pgm_out::put                               */
/*****************************************************************************/

void
  pgm_out::put(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  assert(comp_idx == this->comp_idx);
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(line.get_width() == 0);
      return;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to open a new line buffer.
      assert(x_tnum == 0); // Must supply samples from left to right.
      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(cols,1);
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
    }
  assert((scan->width-scan->accessed_samples) >= line.get_width());

  if (line.get_buf32() != NULL)
    {
      if (line.is_absolute())
        convert_ints_to_bytes(line.get_buf32(),
                              scan->buf+scan->accessed_samples,
                              line.get_width(),precision,orig_precision,
                              orig_signed,1);
      else
        convert_floats_to_bytes(line.get_buf32(),
                                scan->buf+scan->accessed_samples,
                                line.get_width(),precision,orig_precision,
                                orig_signed,1);
    }
  else
    {
      if (line.is_absolute())
        convert_shorts_to_bytes(line.get_buf16(),
                                scan->buf+scan->accessed_samples,
                                line.get_width(),precision,orig_precision,
                                orig_signed,1);
      else
        convert_fixpoint_to_bytes(line.get_buf16(),
                                  scan->buf+scan->accessed_samples,
                                  line.get_width(),precision,orig_precision,
                                  orig_signed,1);
    }

  scan->next_x_tnum++;
  scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == scan->width)
    { // Write completed line and send it to the free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      if (num_unwritten_rows == 0)
        { kdu_error e; e << "Attempting to write too many lines to image "
          "file for component " << comp_idx << "."; }
      if (fwrite(scan->buf,1,(size_t) scan->width,out) != (size_t) scan->width)
        { kdu_error e; e << "Unable to write to image file for component "
          << comp_idx
          << ". File may be write protected, or disk may be full."; }
      num_unwritten_rows--;
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }
}


/* ========================================================================= */
/*                                  ppm_out                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                              ppm_out::ppm_out                             */
/*****************************************************************************/

ppm_out::ppm_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx)
{
  int n;

  first_comp_idx = next_comp_idx;
  if ((first_comp_idx+2) >= dims.get_num_components())
    { kdu_error e; e << "Output image files require more image components "
      "(or mapped colour channels) than are available!"; }
  rows = dims.get_height(first_comp_idx);
  cols = dims.get_width(first_comp_idx);
  orig_signed = dims.get_signed(first_comp_idx);
  for (n=0; n < 3; n++, next_comp_idx++)
    {
      if ((rows != dims.get_height(next_comp_idx)) ||
          (cols != dims.get_width(next_comp_idx)) ||
          (orig_signed != dims.get_signed(next_comp_idx)))
        { kdu_error e; e << "Can only write a PPM file with 3 image "
          "components, all having the same dimensions and the same "
          "signed/unsigned characteristics."; }
      precision[n] = orig_precision[n] = dims.get_bit_depth(next_comp_idx);
      int forced_prec = dims.get_forced_precision(next_comp_idx);
      if (forced_prec != 0)
        {
          if ((forced_prec >= precision[n]) && (forced_prec <= 8))
            precision[n] = forced_prec;
          else
            { kdu_error w; w << "Forced precision supplied via `-fprec' "
              "argument cannot be adopted by the PPM file writer unless it "
              "is at least as large as the nominal precision (from Sprecision "
              "or Mprecision) and no larger than 8 bits.  Ignoring the "
              "forced precision, for component " << next_comp_idx << ".";
            }
        }
    }
  if (orig_signed)
    { kdu_warning w;
      w << "Signed sample values will be written to the "
           "PPM file as unsigned 8-bit quantities, centered about 128.";
    }
  if ((out = fopen(fname,"wb")) == NULL)
    { kdu_error e;
      e << "Unable to open output image file, \"" << fname <<"\"."; }
  fprintf(out,"P6\n%d %d\n255\n",cols,rows);

  incomplete_lines = NULL;
  free_lines = NULL;
  num_unwritten_rows = rows;
  initial_non_empty_tiles = 0; // Don't know yet.
}

/*****************************************************************************/
/*                              ppm_out::~ppm_out                            */
/*****************************************************************************/

ppm_out::~ppm_out()
{
  if ((num_unwritten_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image components "
        << first_comp_idx << " through " << first_comp_idx+2
        << " were completed!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  fclose(out);
}

/*****************************************************************************/
/*                                ppm_out::put                               */
/*****************************************************************************/

void
  ppm_out::put(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  int idx = comp_idx - this->first_comp_idx;
  assert((idx >= 0) && (idx <= 2));
  x_tnum = x_tnum*3+idx; // Works so long as components written in order.
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(line.get_width() == 0);
      return;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to open a new line buffer
      assert(x_tnum == 0); // Must consume in very specific order.
      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(cols,3);
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
    }

  assert((scan->width-scan->accessed_samples) >= line.get_width());

  if (line.get_buf32() != NULL)
    {
      if (line.is_absolute())
        convert_ints_to_bytes(line.get_buf32(),
                              scan->buf+3*scan->accessed_samples+idx,
                              line.get_width(),precision[idx],
                              orig_precision[idx],orig_signed,3);
      else
        convert_floats_to_bytes(line.get_buf32(),
                                scan->buf+3*scan->accessed_samples+idx,
                                line.get_width(),precision[idx],
                                orig_precision[idx],orig_signed,3);
    }
  else
    {
      if (line.is_absolute())
        convert_shorts_to_bytes(line.get_buf16(),
                                scan->buf+3*scan->accessed_samples+idx,
                                line.get_width(),precision[idx],
                                orig_precision[idx],orig_signed,3);
      else
        convert_fixpoint_to_bytes(line.get_buf16(),
                                  scan->buf+3*scan->accessed_samples+idx,
                                  line.get_width(),precision[idx],
                                  orig_precision[idx],orig_signed,3);
    }

  scan->next_x_tnum++;
  if (idx == 2)
    scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == scan->width)
    { // Write completed line and send it to the free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      if (num_unwritten_rows == 0)
        { kdu_error e; e << "Attempting to write too many lines to image "
          "file for components " << first_comp_idx << " through "
          << first_comp_idx+2 << "."; }
      if (fwrite(scan->buf,1,(size_t)(scan->width*3),out) !=
          (size_t)(scan->width*3))
        { kdu_error e; e << "Unable to write to image file for components "
          << first_comp_idx << " through " << first_comp_idx+2
          << ". File may be write protected, or disk may be full."; }
      num_unwritten_rows--;
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }
}


/* ========================================================================= */
/*                                  raw_out                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                              raw_out::raw_out                             */
/*****************************************************************************/

raw_out::raw_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
                 bool littlendian)
{
  comp_idx = next_comp_idx++;
  if (comp_idx >= dims.get_num_components())
    { kdu_error e; e << "Output image files require more image components "
      "(or mapped colour channels) than are available!"; }
  rows = dims.get_height(comp_idx);
  cols = dims.get_width(comp_idx);
  precision = orig_precision = dims.get_bit_depth(comp_idx);
  int forced_prec = dims.get_forced_precision(next_comp_idx);
  if (forced_prec != 0)
    {
      if ((forced_prec >= precision) && (forced_prec <= 32))
        precision = forced_prec;
      else
        { kdu_error w; w << "Forced precision supplied via `-fprec' "
          "argument cannot be adopted by the RAW file writer unless it "
          "is at least as large as the nominal precision (from Sprecision "
          "or Mprecision) and no larger than 32 bits.  Ignoring the "
          "forced precision, for component " << next_comp_idx << ".";
        }
    }
  is_signed = dims.get_signed(comp_idx);
  sample_bytes = (precision+7)>>3;
  incomplete_lines = free_lines = NULL;
  num_unwritten_rows = rows;  
  if ((out = fopen(fname,"wb")) == NULL)
    { kdu_error e;
      e << "Unable to open output image file, \"" << fname <<"\".";}
  initial_non_empty_tiles = 0; // Don't know yet.
  this->littlendian = littlendian;
}

/*****************************************************************************/
/*                              raw_out::~raw_out                            */
/*****************************************************************************/

raw_out::~raw_out()
{
  if ((num_unwritten_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image component "
        << comp_idx << " were produced!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  fclose(out);
}

/*****************************************************************************/
/*                                raw_out::put                               */
/*****************************************************************************/

void
  raw_out::put(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  assert(comp_idx == this->comp_idx);
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(line.get_width() == 0);
      return;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to open a new line buffer.
      assert(x_tnum == 0); // Must supply samples from left to right.
      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(cols,sample_bytes);
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
    }
  assert((scan->width-scan->accessed_samples) >= line.get_width());

  if (line.get_buf32() != NULL)
    {
      if (line.is_absolute())
        convert_ints_to_words(line.get_buf32(),
                              scan->buf+sample_bytes*scan->accessed_samples,
                              line.get_width(),
                              precision,orig_precision,is_signed,
                              sample_bytes,littlendian);
      else
        convert_floats_to_words(line.get_buf32(),
                                scan->buf+sample_bytes*scan->accessed_samples,
                                line.get_width(),
                                precision,orig_precision,is_signed,
                                sample_bytes,littlendian);
    }
  else
    {
      if (line.is_absolute())
        convert_shorts_to_words(line.get_buf16(),
                                scan->buf+sample_bytes*scan->accessed_samples,
                                line.get_width(),
                                precision,orig_precision,is_signed,
                                sample_bytes,littlendian);
      else
        convert_fixpoint_to_words(line.get_buf16(),
                                 scan->buf+sample_bytes*scan->accessed_samples,
                                 line.get_width(),
                                 precision,orig_precision,is_signed,
                                 sample_bytes,littlendian);
    }

  scan->next_x_tnum++;
  scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == scan->width)
    { // Write completed line and send it to the free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      if (num_unwritten_rows == 0)
        { kdu_error e; e << "Attempting to write too many lines to image "
          "file for component " << comp_idx << "."; }
      if (fwrite(scan->buf,1,(size_t)(scan->width*scan->sample_bytes),out) !=
          (size_t)(scan->width*scan->sample_bytes))
        { kdu_error e; e << "Unable to write to image file for component "
          << comp_idx
          << ". File may be write protected, or disk may be full."; }
      num_unwritten_rows--;
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }
}


/* ========================================================================= */
/*                                  bmp_out                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                              bmp_out::bmp_out                             */
/*****************************************************************************/

bmp_out::bmp_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx)
{
  int n;

  first_comp_idx = next_comp_idx;
  num_components = dims.get_num_components() - first_comp_idx;
  if (num_components <= 0)
    { kdu_error e; e << "Output image files require more image components "
      "(or mapped colour channels) than are available!"; }
  if (num_components >= 3)
    num_components = 3;
  else
    num_components = 1;
  rows = dims.get_height(first_comp_idx);  
  cols = dims.get_width(first_comp_idx);
  orig_signed = dims.get_signed(first_comp_idx);
  for (n=0; n < num_components; n++, next_comp_idx++)
    {
      if ((rows != dims.get_height(next_comp_idx)) ||
          (cols != dims.get_width(next_comp_idx)) ||
          (orig_signed != dims.get_signed(next_comp_idx)))
        { assert(n > 0); num_components = 1; break; }
      precision[n] = orig_precision[n] = dims.get_bit_depth(next_comp_idx);
      int forced_prec = dims.get_forced_precision(next_comp_idx);
      if (forced_prec != 0)
        {
          if ((forced_prec >= precision[n]) && (forced_prec <= 8))
            precision[n] = forced_prec;
          else
            { kdu_error w; w << "Forced precision supplied via `-fprec' "
              "argument cannot be adopted by the BMP file writer unless it "
              "is at least as large as the nominal precision (from Sprecision "
              "or Mprecision) and no larger than 8 bits.  Ignoring the "
              "forced precision, for component " << next_comp_idx << ".";
            }
        }      
    }
  next_comp_idx = first_comp_idx + num_components;
  if (orig_signed)
    { kdu_warning w;
      w << "Signed sample values will be written to the "
           "BMP file as unsigned 8-bit quantities, centered about 128.";
    }

  kdu_byte magic[14];
  bmp_header header;
  int header_bytes = 14+sizeof(header);
  assert(header_bytes == 54);
  if (num_components == 1)
    header_bytes += 1024; // Need colour LUT.
  int line_bytes = num_components * cols;
  alignment_bytes = (4-line_bytes) & 3;
  line_bytes += alignment_bytes;
  int file_bytes = line_bytes*rows + header_bytes;
  magic[0] = 'B'; magic[1] = 'M';
  magic[2] = (kdu_byte) file_bytes;
  magic[3] = (kdu_byte)(file_bytes>>8);
  magic[4] = (kdu_byte)(file_bytes>>16);
  magic[5] = (kdu_byte)(file_bytes>>24);
  magic[6] = magic[7] = magic[8] = magic[9] = 0;
  magic[10] = (kdu_byte) header_bytes;
  magic[11] = (kdu_byte)(header_bytes>>8);
  magic[12] = (kdu_byte)(header_bytes>>16);
  magic[13] = (kdu_byte)(header_bytes>>24);
  header.size = 40;
  header.width = cols;
  header.height = rows;
  header.planes_bits = 1; // Set `planes'=1 (mandatory)
  header.planes_bits |= ((num_components==1)?8:24) << 16; // Set bits per pel.
  header.compression = 0;
  header.image_size = 0;

  bool res_units_known;
  double xppm, yppm;
  if (dims.get_resolution(first_comp_idx,res_units_known,xppm,yppm) &&
      (res_units_known || (xppm != yppm)))
    { // Record display resolution in BMP header
      if (!res_units_known)
        { // Choose a reasonable scale factor -- 72 dpi
          double scale = (72.0*1000.0/25.4) / xppm;
          xppm *= scale;  yppm *= scale;
        }
      header.xpels_per_metre = (kdu_int32)(xppm+0.5);
      header.ypels_per_metre = (kdu_int32)(yppm+0.5);
    }
  else
    header.xpels_per_metre = header.ypels_per_metre = 0;

  header.num_colours_used = header.num_colours_important = 0;
  to_little_endian((kdu_int32 *) &header,10);
  if ((out = fopen(fname,"wb")) == NULL)
    { kdu_error e; e << "Unable to open output image file, \"" << fname <<"\".";}
  fwrite(magic,1,14,out);
  fwrite(&header,1,40,out);
  if (num_components == 1)
    for (n=0; n < 256; n++)
      { fputc(n,out); fputc(n,out); fputc(n,out); fputc(0,out); }
  incomplete_lines = NULL;
  free_lines = NULL;
  num_unwritten_rows = rows;
  initial_non_empty_tiles = 0; // Don't know yet.
}

/*****************************************************************************/
/*                              bmp_out::~bmp_out                            */
/*****************************************************************************/

bmp_out::~bmp_out()
{
  if ((num_unwritten_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image components "
        << first_comp_idx << " through "
        << first_comp_idx+num_components-1
        << " were completed!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  fclose(out);
}

/*****************************************************************************/
/*                                bmp_out::put                               */
/*****************************************************************************/

void
  bmp_out::put(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  int idx = comp_idx - this->first_comp_idx;
  assert((idx >= 0) && (idx < num_components));
  x_tnum = x_tnum*num_components+idx;
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(line.get_width() == 0);
      return;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to open a new line buffer
      assert(x_tnum == 0); // Must generate in very specific order.
      if ((scan = free_lines) == NULL)
        {
          scan = new image_line_buf(cols+3,num_components);
          for (int k=0; k < alignment_bytes; k++)
            scan->buf[num_components*cols+k] = 0;
        }
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
    }

  assert((cols-scan->accessed_samples) >= line.get_width());
  int comp_offset = (num_components==3)?(2-idx):0;

  if (line.get_buf32() != NULL)
    {
      if (line.is_absolute())
        convert_ints_to_bytes(line.get_buf32(),
                scan->buf+num_components*scan->accessed_samples+comp_offset,
                line.get_width(),precision[idx],orig_precision[idx],
                orig_signed,num_components);
      else
        convert_floats_to_bytes(line.get_buf32(),
                scan->buf+num_components*scan->accessed_samples+comp_offset,
                line.get_width(),precision[idx],orig_precision[idx],
                orig_signed,num_components);
    }
  else
    {
      if (line.is_absolute())
        convert_shorts_to_bytes(line.get_buf16(),
                scan->buf+num_components*scan->accessed_samples+comp_offset,
                line.get_width(),precision[idx],orig_precision[idx],
                orig_signed,num_components);
      else
        convert_fixpoint_to_bytes(line.get_buf16(),
                scan->buf+num_components*scan->accessed_samples+comp_offset,
                line.get_width(),precision[idx],orig_precision[idx],
                orig_signed,num_components);
    }

  scan->next_x_tnum++;
  if (idx == (num_components-1))
    scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == cols)
    { // Write completed line and send it to the free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      if (num_unwritten_rows == 0)
        { kdu_error e; e << "Attempting to write too many lines to image "
          "file for components " << first_comp_idx << " through "
          << first_comp_idx+num_components-1 << "."; }
      if (fwrite(scan->buf,1,(size_t)(cols*num_components+alignment_bytes),
                 out) != (size_t)(cols*num_components+alignment_bytes))
        { kdu_error e; e << "Unable to write to image file for components "
          << first_comp_idx << " through " << first_comp_idx+num_components-1
          << ". File may be write protected, or disk may be full."; }
      num_unwritten_rows--;
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }
}


/* ========================================================================= */
/*                                  tif_out                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                              tif_out::tif_out                             */
/*****************************************************************************/

tif_out::tif_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx)
{
  // Initialize state information in case we have to cleanup prematurely
  orig_precision = NULL;
  is_signed = NULL;
  incomplete_lines = NULL;
  free_lines = NULL;
  num_unwritten_rows = 0;
  initial_non_empty_tiles = 0; // Don't know yet.

  // Find max image components
  first_comp_idx = next_comp_idx;
  num_components = dims.get_num_components() - first_comp_idx;
  if (num_components <= 0)
    { kdu_error e; e << "Output image files require more image components "
      "(or mapped colour channels) than are available!"; }

  // Find the colour space and alpha properties of the file to be written
  int num_colours = 1;
  bool have_premultiplied_alpha = false;
  kdu_uint16 photometric_type = KDU_TIFF_PhotometricInterp_BLACKISZERO;
  kdu_uint16 inkset=0, numberofinks=0;
  if (next_comp_idx > 0)
    num_components = 1; // Just write one component in each non-initial file
  else
    {
      bool have_unassociated_alpha=false;
      int colour_space_confidence=0;
      jp2_colour_space colour_space=JP2_sLUM_SPACE;
      num_colours =
        dims.get_colour_info(have_premultiplied_alpha,have_unassociated_alpha,
                             colour_space_confidence,colour_space);
      if ((num_colours > num_components) || (num_colours == 2))
        num_colours = num_components = 1; // Write one monochrome component
      else if (colour_space_confidence <= 0)
        {
          if (num_colours == 3)
            photometric_type = KDU_TIFF_PhotometricInterp_RGB;
          else if (num_colours != 1)
            num_colours = num_components = 1;
        }
      else if (colour_space == JP2_sLUM_SPACE)
        {
          assert(num_colours == 1);
          photometric_type = KDU_TIFF_PhotometricInterp_BLACKISZERO;
        }
      else if (colour_space == JP2_sRGB_SPACE)
        {
          assert(num_colours == 3);
          photometric_type = KDU_TIFF_PhotometricInterp_RGB;
        }
      else if (colour_space == JP2_CMYK_SPACE)
        {
          assert(num_colours == 4);
          photometric_type = KDU_TIFF_PhotometricInterp_SEPARATED;
          inkset = KDU_TIFF_InkSet_CMYK;
          numberofinks = 4;
        }
      else if (colour_space == JP2_bilevel1_SPACE)
        {
          assert(num_colours == 1);
          photometric_type = KDU_TIFF_PhotometricInterp_WHITEISZERO;
        }
      else if (colour_space == JP2_bilevel2_SPACE)
        {
          assert(num_colours == 1);
          photometric_type = KDU_TIFF_PhotometricInterp_BLACKISZERO;
        }
      else if (num_colours == 3)
        {
          photometric_type = KDU_TIFF_PhotometricInterp_RGB;
          kdu_warning w; w << "Trying to save uncommon 3-colour space to "
            "TIFF file (JP2 colour space identifier is "
            << (int) colour_space << ").  "
            "The current TIFF writer module will record this as an "
            "RGB space, possibly eroneously.";
        }
      else if (num_colours > 3)
        {
          photometric_type = KDU_TIFF_PhotometricInterp_SEPARATED;
          inkset = KDU_TIFF_InkSet_NotCMYK;
          numberofinks = (kdu_uint16) num_colours;
          kdu_warning w; w << "Trying to save non-CMYK colour space with "
            "more than 3 colour channels to TIFF file (JP2 colour space "
            "identifier is " << (int) colour_space << ").  "
            "The current TIFF writer module will record this as a "
            "separated colour space, but cannot determine TIFF ink names.";
        }
      else
        {
          assert(num_colours == 1);
          photometric_type = KDU_TIFF_PhotometricInterp_BLACKISZERO;
          kdu_warning w; w << "Unrecognized monochromatic colour space "
            "will be recorded in TIFF file as having the BLACK-IS-ZERO "
            "photometric type.";
        }
      if (num_colours >= num_components)
        have_premultiplied_alpha = false; // Alpha has been discarded
      if (have_unassociated_alpha)
        { kdu_warning w; w << "Alpha channel cannot be identified in a TIFF "
          "file since it is of the unassociated (i.e., not premultiplied) "
          "type, and these are not supported by TIFF.  You can save this "
          "to a separate output file."; }
      num_components = num_colours + ((have_premultiplied_alpha)?1:0);
    }

  rows = dims.get_height(first_comp_idx);  
  cols = dims.get_width(first_comp_idx);

  // Find component dimensions and other info.
  is_signed = new bool[num_components];
  orig_precision = new int[num_components];
  precision = 0; // Just for now
  int n;
  for (n=0; n < num_components; n++, next_comp_idx++)
    {
      is_signed[n] = dims.get_signed(next_comp_idx);
      int comp_prec = orig_precision[n] = dims.get_bit_depth(next_comp_idx);
      int forced_prec = dims.get_forced_precision(next_comp_idx);
      if (forced_prec != 0)
        {
          if ((forced_prec >= comp_prec) && (forced_prec <= 32))
            comp_prec = forced_prec;
          else
            { kdu_error w; w << "Forced precision supplied via `-fprec' "
              "argument cannot be adopted by the TIFF file writer unless it "
              "is at least as large as the nominal precision (from Sprecision "
              "or Mprecision) and no larger than 32 bits.  Ignoring the "
              "forced precision, for component " << next_comp_idx << ".";
            }
        }
      if (n == 0)
        precision = comp_prec;
      if ((rows != dims.get_height(next_comp_idx)) ||
          (cols != dims.get_width(next_comp_idx)) ||
          (comp_prec != precision))
        {
          assert(n > 0);
          num_colours=num_components=1;
          have_premultiplied_alpha=false;
          photometric_type = KDU_TIFF_PhotometricInterp_BLACKISZERO;
          break;
        }
    }
  next_comp_idx = first_comp_idx + num_components;

  // Find the sample, pixel and line dimensions
  if (precision <= 8)
    sample_bytes = 1;
  else if (precision <= 16)
    sample_bytes = 2;
  else if (precision <= 32)
    sample_bytes = 4;
  else
    { kdu_error e;
      e << "Cannot write output with sample precision in excess of 32 bits "
           "per sample."; }
  pixel_bytes = sample_bytes * num_components;
  row_bytes = pixel_bytes * cols;
  scanline_width = num_components * precision * cols;
  scanline_width = (scanline_width+7)>>3;

  // Find the resolution info
  bool res_units_known=false;
  double xppm, yppm;
  if (!dims.get_resolution(first_comp_idx,res_units_known,xppm,yppm))
    { xppm = 1.0; yppm = 1.0; }
  kdu_uint16 resolution_unit = KDU_TIFF_ResolutionUnit_CM;
  if (!res_units_known)
    { // Choose a reasonable scale factor -- 72 dpi
      double scale = (72.0*1000.0/25.4) / xppm;
      xppm *= scale;  yppm *= scale;
      resolution_unit = KDU_TIFF_ResolutionUnit_NONE;
    }
  float xpels_per_cm = (float)(xppm*0.01);
  float ypels_per_cm = (float)(yppm*0.01);

  // See if we can find the JP2Geo box
  jp2_input_box geo_box;
  jpx_meta_manager meta_manager = dims.get_meta_manager();
  if (meta_manager.exists())
    {
      kdu_byte uuid[16];
      kdu_byte geojp2_uuid[16] = {0xB1,0x4B,0xF8,0xBD,0x08,0x3D,0x4B,0x43,
                                  0xA5,0xAE,0x8C,0xD7,0xD5,0xA6,0xCE,0x03};
      jpx_metanode scn, mn = meta_manager.access_root();
      for (int cnt=0; (scn=mn.get_descendant(cnt)).exists(); cnt++)
        if (scn.get_uuid(uuid) && (memcmp(uuid,geojp2_uuid,16)==0))
          { // Found GeoJP2 box
            jp2_family_src *jsrc;
            jp2_locator loc = scn.get_existing(jsrc);
            geo_box.open(jsrc,loc);
            geo_box.seek(16); // Seek over the UUID code
            break;
          }
    }

  // Create TIFF directory entries
  kdu_tiffdir tiffdir;
  tiffdir.init(tiffdir.is_native_littlendian());
       // Create a TIFF file which uses native byte order.  Everything should
       // work correctly if you choose to force the byte order to be one of
       // little-endian or big-endian, since the `pre_pack_littlendian' member
       // is set (near the end of this function) in such a way as to ensure
       // compliant byte ordering for the written scanline samples.

  tiffdir.write_tag(KDU_TIFFTAG_ImageWidth32,(kdu_uint32) cols);
  tiffdir.write_tag(KDU_TIFFTAG_ImageHeight32,(kdu_uint32) rows);
  tiffdir.write_tag(KDU_TIFFTAG_SamplesPerPixel,(kdu_uint16) num_components);
  tiffdir.write_tag(KDU_TIFFTAG_PhotometricInterp,photometric_type);
  tiffdir.write_tag(KDU_TIFFTAG_PlanarConfig,KDU_TIFF_PlanarConfig_CONTIG);
  tiffdir.write_tag(KDU_TIFFTAG_Compression,KDU_TIFF_Compression_NONE);
  tiffdir.write_tag(KDU_TIFFTAG_ResolutionUnit,resolution_unit);
  tiffdir.write_tag(KDU_TIFFTAG_XResolution,xpels_per_cm);
  tiffdir.write_tag(KDU_TIFFTAG_YResolution,ypels_per_cm);
  if (have_premultiplied_alpha)
    tiffdir.write_tag(KDU_TIFFTAG_ExtraSamples,(kdu_uint16) 1);
  for (n=0; n < num_components; n++)
    {
      tiffdir.write_tag(KDU_TIFFTAG_BitsPerSample,(kdu_uint16) precision);
      tiffdir.write_tag(KDU_TIFFTAG_SampleFormat,
                        (is_signed[n])?KDU_TIFF_SampleFormat_SIGNED:
                                       KDU_TIFF_SampleFormat_UNSIGNED);
    }
  if (geo_box.exists())
    {
      kdu_tiffdir geotiff;
      if (geotiff.opendir(&geo_box))
        { // Copy GeoTIFF tags across; we will do this in a type-agnostic way
          kdu_uint32 tag_type;
          if ((tag_type=geotiff.open_tag(((kdu_uint32) 33550)<<16)) != 0)
            tiffdir.copy_tag(geotiff,tag_type);
          if ((tag_type=geotiff.open_tag(((kdu_uint32) 33922)<<16)) != 0)
            tiffdir.copy_tag(geotiff,tag_type);
          if ((tag_type=geotiff.open_tag(((kdu_uint32) 34264)<<16)) != 0)
            tiffdir.copy_tag(geotiff,tag_type);
          if ((tag_type=geotiff.open_tag(((kdu_uint32) 34735)<<16)) != 0)
            tiffdir.copy_tag(geotiff,tag_type);
          if ((tag_type=geotiff.open_tag(((kdu_uint32) 34736)<<16)) != 0)
            tiffdir.copy_tag(geotiff,tag_type);
          if ((tag_type=geotiff.open_tag(((kdu_uint32) 34737)<<16)) != 0)
            tiffdir.copy_tag(geotiff,tag_type);
        }
      geotiff.close();
      geo_box.close();
    }

  // Write the image strip properties -- we will write everything in one strip
  // right after the TIFF directory
  tiffdir.write_tag(KDU_TIFFTAG_RowsPerStrip32,(kdu_uint32) rows);
  tiffdir.write_tag(KDU_TIFFTAG_StripByteCounts32,
                    (kdu_uint32)(scanline_width*rows));
  kdu_uint32 image_pos = tiffdir.get_dirlength()+8; // This will change
  tiffdir.write_tag(KDU_TIFFTAG_StripOffsets32,image_pos);
  image_pos = tiffdir.get_dirlength()+8; // Length will have changed
  tiffdir.create_tag(KDU_TIFFTAG_StripOffsets32); // Reset this tag
  tiffdir.write_tag(KDU_TIFFTAG_StripOffsets32,image_pos); // Write it again
  assert(image_pos == (tiffdir.get_dirlength()+8)); // Length should be stable

  // Open TIFF file and write everything except the scan lines
  if (!out.open(fname,false,true))
    { kdu_error e;
      e << "Unable to open output image file, \"" << fname << "\"."; }
  tiffdir.write_header(&out,8);
  if (!tiffdir.writedir(&out,8))
    { kdu_error e; e << "Attempt to write TIFF directory failed.  Output "
      "device might be full."; }
  if ((precision == 16) || (precision == 32))
    pre_pack_littlendian = tiffdir.is_littlendian();
  else
    pre_pack_littlendian = tiffdir.is_native_littlendian();
  num_unwritten_rows = rows;
}

/*****************************************************************************/
/*                             tif_out::~tif_out                             */
/*****************************************************************************/

tif_out::~tif_out()
{
  if ((num_unwritten_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image components "
        << first_comp_idx << " through "
        << first_comp_idx+num_components-1
        << " were completed!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  if (orig_precision != NULL)
    delete[] orig_precision;
  if (is_signed != NULL)
    delete[] is_signed;
  out.close();
}

/*****************************************************************************/
/*                                tif_out::put                               */
/*****************************************************************************/

void
  tif_out::put(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  int width = line.get_width();
  int idx = comp_idx - this->first_comp_idx;
  assert((idx >= 0) && (idx < num_components));
  x_tnum = x_tnum*num_components+idx;
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(width == 0);
      return;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to open a new line buffer
      assert(x_tnum == 0); // Must generate in very specific order.
      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(cols+4,pixel_bytes);
                  // Big enough for padding and expanding bits to bytes
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
    }

  assert((cols-scan->accessed_samples) >= width);

  // Extract data from `line' buffer, performing conversions as required
  kdu_byte *dst = scan->buf +
    pixel_bytes*scan->accessed_samples + sample_bytes*idx;
  if (line.get_buf32() != NULL)
    {
      if (line.is_absolute())
        convert_ints_to_words(line.get_buf32(),dst,width,precision,
                              orig_precision[idx],is_signed[idx],
                              sample_bytes,pre_pack_littlendian,
                              pixel_bytes);
      else
        convert_floats_to_words(line.get_buf32(),dst,width,precision,
                                orig_precision[idx],is_signed[idx],
                                sample_bytes,pre_pack_littlendian,
                                pixel_bytes);
    }
  else
    {
      if (line.is_absolute())
        convert_shorts_to_words(line.get_buf16(),dst,width,precision,
                                orig_precision[idx],is_signed[idx],
                                sample_bytes,pre_pack_littlendian,
                                pixel_bytes);
      else
        convert_fixpoint_to_words(line.get_buf16(),dst,width,precision,
                                  orig_precision[idx],is_signed[idx],
                                  sample_bytes,pre_pack_littlendian,
                                  pixel_bytes);
    }

  // Finished writing to line-tile; now see if we can write a TIFF scan-line
  scan->next_x_tnum++;
  if (idx == (num_components-1))
    scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == cols)
    { // Write completed line and send it to the free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      if (num_unwritten_rows == 0)
        { kdu_error e; e << "Attempting to write too many lines to image "
          "file for components " << first_comp_idx << " through "
          << first_comp_idx+num_components-1 << "."; }
      
      if ((precision != 8) && (precision != 16) && (precision != 32))
        perform_buffer_pack(scan->buf);
      out.write(scan->buf,scanline_width);

      num_unwritten_rows--;
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }
}

/*****************************************************************************/
/*                        tif_out::perform_buffer_pack                       */
/*****************************************************************************/

void
  tif_out::perform_buffer_pack(kdu_byte *dst)
{
  if (sample_bytes == 1)
    {
      kdu_byte *src = dst;
      assert(precision < 8);
      kdu_byte out_val=0; // Used to accumulate packed output bytes
      kdu_byte in_val;
      int n, shift, bits_needed=8;
      for (n=row_bytes; n > 0; n--, src++)
        {
          in_val = *src;
          if (bits_needed > precision)
            {
              out_val = (out_val<<precision) | in_val;
              bits_needed -= precision;
              continue; // `bits_needed' is still > 0
            }
          in_val = *src; // Need to borrow `bits_needed' bits from `in_val'
          shift = precision-bits_needed;
          *(dst++) = (out_val<<bits_needed) | (in_val>>shift);
          out_val = in_val;
          bits_needed += 8 - precision;
        }
      if (bits_needed < 8)
        *(dst++) = (out_val << bits_needed); // Last byte is padded with 0's
    }
  else if (sample_bytes == 2)
    {
      assert((precision > 8) && (precision < 16));
      kdu_uint16 *src = (kdu_uint16 *) dst;
      kdu_uint16 val=0, next_val;
      int n, shift=-8;
      for (n=scanline_width; n > 0; n--, dst++, shift-=8)
        {
          if (shift < 0)
            {
              val <<= -shift;  next_val = *(src++);  shift += precision;
              *dst = (kdu_byte)(val | (next_val>>shift));
              val = next_val;
            }
          else
            *dst = (kdu_byte)(val>>shift);
        }
    }
  else if (sample_bytes == 4)
    {
      assert((precision > 16) && (precision < 32));
      kdu_uint32 *src = (kdu_uint32 *) dst;
      kdu_uint32 val=0, next_val;
      int n, shift=-8;
      for (n=scanline_width; n > 0; n--, dst++, shift-=8)
        {
          if (shift < 0)
            {
              val <<= -shift;  next_val = *(src++);  shift += precision;
              *dst = (kdu_byte)(val | (next_val>>shift));
              val = next_val;
            }
          else
            *dst = (kdu_byte)(val>>shift);
        }
    }
  else
    assert(0);
}
