/*****************************************************************************/
// File: image_in.cpp [scope = APPS/IMAGE-IO]
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
   Implements image file reading for a variety of different file formats:
currently BMP, PGM, PPM, TIFF and RAW only.  Readily extendible to include
other file formats without affecting the rest of the system.
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
// IO includes
#include "kdu_file_io.h"
#include "image_local.h"


/* ========================================================================= */
/*                             Internal Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                       to_little_endian                             */
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
/* INLINE                    eat_white_and_comments                          */
/*****************************************************************************/

static inline void
  eat_white_and_comments(FILE *in)
{
  int ch;
  bool in_comment;

  in_comment = false;
  while ((ch = getc(in)) != EOF)
    if (ch == '#')
      in_comment = true;
    else if (ch == '\n')
      in_comment = false;
    else if ((!in_comment) && (ch != ' ') && (ch != '\t') && (ch != '\r'))
      {
        ungetc(ch,in);
        return;
      }
}

/*****************************************************************************/
/* STATIC                    convert_words_to_floats                         */
/*****************************************************************************/

static void
  convert_words_to_floats(kdu_byte *src, kdu_sample32 *dest, int num,
                          int precision, bool is_signed, int sample_bytes,
                          bool littlendian, int inter_sample_bytes=0)
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  float scale;
  if (precision < 30)
    scale = (float)(1<<precision);
  else
    scale = ((float)(1<<30)) * ((float)(1<<(precision-30)));
  scale = 1.0F / scale;
  
  kdu_int32 centre = 1<<(precision-1);
  kdu_int32 offset = (is_signed)?centre:0;
  kdu_int32 mask = ~((-1)<<precision);
  kdu_int32 val;

  if (sample_bytes == 1)
    for (; num > 0; num--, dest++, src+=inter_sample_bytes)
      {
        val = src[0];
        val += offset;  val &= mask;  val -= centre;
        dest->fval = ((float) val) * scale;
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
    }
  else if (sample_bytes == 3)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1]; val = (val<<8) + src[2];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[2]; val = (val<<8) + src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
    }
  else if (sample_bytes == 4)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val = (val<<8) + src[2]; val = (val<<8) + src[3];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[3]; val = (val<<8) + src[2];
            val = (val<<8) + src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/* STATIC                   convert_words_to_fixpoint                        */
/*****************************************************************************/

static void
  convert_words_to_fixpoint(kdu_byte *src, kdu_sample16 *dest, int num,
                            int precision, bool is_signed, int sample_bytes,
                            bool littlendian, int inter_sample_bytes=0)
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  kdu_int32 upshift = KDU_FIX_POINT-precision;
  if (upshift < 0)
    { kdu_error e; e << "Cannot use 16-bit representation with high "
      "bit-depth data"; }
  kdu_int32 centre = 1<<(precision-1);
  kdu_int32 offset = (is_signed)?centre:0;
  kdu_int32 mask = ~((-1)<<precision);
  kdu_int32 val;

  if (sample_bytes == 1)
    for (; num > 0; num--, dest++, src+=inter_sample_bytes)
      {
        val = src[0];
        val += offset;  val &= mask;  val -= centre;
        dest->ival = (kdu_int16)(val<<upshift);
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = (kdu_int16)(val<<upshift);
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = (kdu_int16)(val<<upshift);
          }
    }
  else
    { kdu_error e; e << "Cannot use 16-bit representation with high "
      "bit-depth data"; }
}

/*****************************************************************************/
/* STATIC                     convert_words_to_ints                          */
/*****************************************************************************/

static void
  convert_words_to_ints(kdu_byte *src, kdu_sample32 *dest, int num,
                        int precision, bool is_signed, int sample_bytes,
                        bool littlendian, int inter_sample_bytes=0)
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  kdu_int32 centre = 1<<(precision-1);
  kdu_int32 offset = (is_signed)?centre:0;
  kdu_int32 mask = ~((-1)<<precision);
  kdu_int32 val;

  if (sample_bytes == 1)
    for (; num > 0; num--, dest++, src+=inter_sample_bytes)
      {
        val = *src;
        val += offset;  val &= mask;  val -= centre;
        dest->ival = val;
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
    }
  else if (sample_bytes == 3)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1]; val = (val<<8) + src[2];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[2]; val = (val<<8) + src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
    }
  else if (sample_bytes == 4)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val = (val<<8) + src[2]; val = (val<<8) + src[3];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[3]; val = (val<<8) + src[2];
            val = (val<<8) + src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/* STATIC                   convert_words_to_shorts                          */
/*****************************************************************************/

static void
  convert_words_to_shorts(kdu_byte *src, kdu_sample16 *dest, int num,
                          int precision, bool is_signed, int sample_bytes,
                          bool littlendian, int inter_sample_bytes=0)
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  kdu_int32 centre = 1<<(precision-1);
  kdu_int32 offset = (is_signed)?centre:0;
  kdu_int32 mask = ~((-1)<<precision);
  kdu_int32 val;

  if (sample_bytes == 1)
    for (; num > 0; num--, dest++, src+=inter_sample_bytes)
      {
        val = src[0];
        val += offset;  val &= mask;  val -= centre;
        dest->ival = (kdu_int16) val;
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = (kdu_int16) val;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = (kdu_int16) val;
          }
    }
  else
    { kdu_error e; e << "Cannot use 16-bit representation with high "
      "bit-depth data"; }
}

/*****************************************************************************/
/* STATIC                   convert_floats_to_ints                           */
/*****************************************************************************/

static void
  convert_floats_to_ints(kdu_byte *src, kdu_sample32 *dest,  int num,
                         int precision, bool is_signed,
                         double minval, double maxval, int sample_bytes,
                         bool littlendian, int inter_sample_bytes)
{
  int test = 1;
  bool native_littlendian = (((kdu_byte *) &test)[0] != 0);

  double scale, offset=0.0;
  double limmin=-0.75, limmax=0.75;
  if (is_signed)
    scale = 0.5 / (((maxval+minval) > 0.0)?maxval:(-minval));
  else
    {
      scale = 1.0 / maxval;
      offset = -0.5;
    }
  scale *= (double)((1<<precision)-1);
  offset *= (double)(1<<precision);
  limmin *= (double)(1<<precision);
  limmax *= (double)(1<<precision);

  if (sample_bytes == 4)
    { // Transfer floats to ints
      union {
          float fbuf_val;
          kdu_byte fbuf[4];
        };
      if (littlendian == native_littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[0]=src[0]; fbuf[1]=src[1]; fbuf[2]=src[2]; fbuf[3]=src[3];
            double fval = fbuf_val * scale + offset;
            fval = (fval > limmin)?fval:limmin;
            fval = (fval < limmax)?fval:limmax;
            dest->ival = (kdu_int32) fval;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[3]=src[0]; fbuf[2]=src[1]; fbuf[1]=src[2]; fbuf[0]=src[3];
            double fval = fbuf_val * scale + offset;
            fval = (fval > limmin)?fval:limmin;
            fval = (fval < limmax)?fval:limmax;
            dest->ival = (kdu_int32) fval;
          }
    }
  else if (sample_bytes == 8)
    { // Transfer doubles to floats, with some scaling
      union {
          double fbuf_val;
          kdu_byte fbuf[8];
        };
      if (littlendian == native_littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[0]=src[0]; fbuf[1]=src[1]; fbuf[2]=src[2]; fbuf[3]=src[3];
            fbuf[4]=src[4]; fbuf[5]=src[5]; fbuf[6]=src[6]; fbuf[7]=src[7];
            double fval = fbuf_val * scale + offset;
            fval = (fval > limmin)?fval:limmin;
            fval = (fval < limmax)?fval:limmax;
            dest->ival = (kdu_int32) fval;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[7]=src[0]; fbuf[6]=src[1]; fbuf[5]=src[2]; fbuf[4]=src[3];
            fbuf[3]=src[4]; fbuf[2]=src[5]; fbuf[1]=src[6]; fbuf[0]=src[7];
            double fval = fbuf_val * scale + offset;
            fval = (fval > limmin)?fval:limmin;
            fval = (fval < limmax)?fval:limmax;
            dest->ival = (kdu_int32) fval;
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/* STATIC                  convert_floats_to_floats                          */
/*****************************************************************************/

static void
  convert_floats_to_floats(kdu_byte *src, kdu_sample32 *dest,  int num,
                           int precision, bool is_signed,
                           double minval, double maxval, int sample_bytes,
                           bool littlendian, int inter_sample_bytes)
{
  int test = 1;
  bool native_littlendian = (((kdu_byte *) &test)[0] != 0);

  double scale, offset=0.0;
  if (is_signed)
    scale = 0.5 / (((maxval+minval) > 0.0)?maxval:(-minval));
  else
    {
      scale = 1.0 / maxval;
      offset = -0.5;
    }

  if (sample_bytes == 4)
    { // Transfer floats to floats, with some scaling
      union {
          float fbuf_val;
          kdu_byte fbuf[4];
        };
      if (littlendian == native_littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[0]=src[0]; fbuf[1]=src[1]; fbuf[2]=src[2]; fbuf[3]=src[3];
            dest->fval = (float)(fbuf_val * scale + offset);
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[3]=src[0]; fbuf[2]=src[1]; fbuf[1]=src[2]; fbuf[0]=src[3];
            dest->fval = (float)(fbuf_val * scale + offset);
          }
    }
  else if (sample_bytes == 8)
    { // Transfer doubles to floats, with some scaling
      union {
          double fbuf_val;
          kdu_byte fbuf[8];
        };
      if (littlendian == native_littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[0]=src[0]; fbuf[1]=src[1]; fbuf[2]=src[2]; fbuf[3]=src[3];
            fbuf[4]=src[4]; fbuf[5]=src[5]; fbuf[6]=src[6]; fbuf[7]=src[7];
            dest->fval = (float)(fbuf_val * scale + offset);
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[7]=src[0]; fbuf[6]=src[1]; fbuf[5]=src[2]; fbuf[4]=src[3];
            fbuf[3]=src[4]; fbuf[2]=src[5]; fbuf[1]=src[6]; fbuf[0]=src[7];
            dest->fval = (float)(fbuf_val * scale + offset);
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/* STATIC                    force_sample_precision                          */
/*****************************************************************************/

static void
  force_sample_precision(kdu_line_buf &line, int forced_prec, int initial_prec,
                         bool is_signed)
  /* If working with an absolute representation, this function truncates the
     samples to the range -2^{P-1} to 2^{P-1}-1 where P is the value of
     `forced_prec'.  If working with a fixed-point or floating point
     representation, the function scales the data by
     2^{initial_prec-forced_prec}, clipping upscaled values.
     If `is_signed' is false, the original sample values are first offset by
     a value equivalent to 2^{initial_prec-1} - 2^{forced_prec-1}. */
{
  assert(initial_prec > 0);
  if (initial_prec == forced_prec)
    return;
  int n = line.get_width();
  if (line.get_buf32() != NULL)
    {
      kdu_sample32 *sp = line.get_buf32();
      if (line.is_absolute())
        {
          if (forced_prec >= initial_prec)
            { // We only have to introduce the offset
              if (is_signed)
                return;
              kdu_int32 offset = ((1<<initial_prec)>>1)-((1<<forced_prec)>>1);
              for (; n > 0; n--, sp++)
                sp->ival += offset;
            }
          else
            {
              kdu_int32 offset = 0;
              if (!is_signed)
                offset = ((1<<initial_prec)>>1)-((1<<forced_prec)>>1);
              kdu_int32 min_val = -(1<<(forced_prec-1));
              kdu_int32 max_val = -min_val-1;
              min_val -= offset;  max_val -= offset;
              for (; n > 0; n--, sp++)
                {
                  int val = sp->ival;
                  if (val < min_val)
                    val = min_val;
                  else if (val > max_val)
                    val = max_val;
                  sp->ival = val + offset;
                }
            }
        }
      else
        {
          float scale = ((float)(1<<initial_prec)) / ((float)(1<<forced_prec));
          float offset = 0.0F;
          if (!is_signed)
            offset = 0.5F*scale - 0.5F;
          float max_val = 0.5F - 1.0F / (float)(1<<forced_prec);
          for (; n > 0; n--, sp++)
            {
              float val = sp->fval * scale + offset;
              if (val < -0.5F)
                sp->fval = -0.5F;
              else if (val > max_val)
                sp->fval = max_val;
              else
                sp->fval = val;
            }
        }
    }
  else
    {
      kdu_sample16 *sp = line.get_buf16();
      if (line.is_absolute())
        {
          if (forced_prec >= initial_prec)
            { // We only have to introduce the offset
              if (is_signed)
                return;
              kdu_int16 offset = (kdu_int16)
                (((1<<initial_prec)>>1)-((1<<forced_prec)>>1));
              for (; n > 0; n--, sp++)
                sp->ival += offset;
            }
          else
            {
              kdu_int16 offset = 0;
              if (!is_signed)
                offset = (kdu_int16)
                  (((1<<initial_prec)>>1)-((1<<forced_prec)>>1));
              kdu_int16 min_val = (kdu_int16) -(1<<(forced_prec-1));
              kdu_int16 max_val = -min_val-1;
              min_val -= offset;  max_val -= offset;
              for (; n > 0; n--, sp++)
                {
                  kdu_int16 val = sp->ival;
                  if (val < min_val)
                    val = min_val;
                  else if (val > max_val)
                    val = max_val;
                  sp->ival = val + offset;
                }
            }
        }
      else if (forced_prec < initial_prec)
        {
          int upshift = initial_prec - forced_prec;
          kdu_int16 min_val=0, max_val=0;
          kdu_int16 offset = 0;
          if (upshift < KDU_FIX_POINT)
            {
              min_val = (kdu_int16) -(1<<(KDU_FIX_POINT-upshift-1));
              max_val = -min_val;
              if (initial_prec <= KDU_FIX_POINT)
                max_val -= (1<<(KDU_FIX_POINT-initial_prec));
              if (!is_signed)
                offset = (kdu_int16)((1<<(KDU_FIX_POINT-1)) -
                                     (1<<(KDU_FIX_POINT-upshift-1)));
              min_val -= offset;  max_val -= offset;
            }
          for (; n > 0; n--, sp++)
            {
              kdu_int16 val = sp->ival;
              if (val < min_val)
                val = min_val;
              else if (val > max_val)
                val = max_val;
              sp->ival = (val + offset) << upshift;
            }
        }
      else
        {
          int downshift = forced_prec - initial_prec;
          kdu_int32 offset = (kdu_int32)((1<<downshift)>>1);
          if (!is_signed)
            offset += ((1<<(KDU_FIX_POINT-1)) -
                       (1<<(KDU_FIX_POINT+downshift-1)));
          for (; n > 0; n--, sp++)
            {
              int val = sp->ival;
              sp->ival = (kdu_int16)((val + offset) >> downshift);
            }
        }
    }
}

/*****************************************************************************/
/* STATIC                         invert_line                                */
/*****************************************************************************/

static void
  invert_line(kdu_line_buf &line)
  /* Inverts each sample value in the line.  For absolute representations,
     `precision' is required to do this. */
{
  int n = line.get_width();
  if (line.get_buf32() != NULL)
    {
      kdu_sample32 *sp = line.get_buf32();
      if (line.is_absolute())
        for (; n > 0; n--, sp++)
          sp->ival = -sp->ival;
      else
        for (; n > 0; n--, sp++)
          sp->fval = -sp->fval;
    }
  else
    {
      kdu_sample16 *sp = line.get_buf16();
      for (; n > 0; n--, sp++)
        sp->ival = -sp->ival;
    }
}


/* ========================================================================= */
/*                                kdu_image_in                               */
/* ========================================================================= */

/*****************************************************************************/
/*                         kdu_image_in::kdu_image_in                        */
/*****************************************************************************/

kdu_image_in::kdu_image_in(const char *fname, kdu_image_dims &dims,
                           int &next_comp_idx, bool &vflip,
                           kdu_rgb8_palette *palette, kdu_long skip_bytes)
{
  const char *suffix;

  in = NULL;
  vflip = false; // Allows derived constructors to ignore the argument.
  if ((suffix = strrchr(fname,'.')) != NULL)
    {
      if ((strcmp(suffix+1,"pbm") == 0) || (strcmp(suffix+1,"PBM") == 0))
        in = new pbm_in(fname,dims,next_comp_idx,palette,skip_bytes);
      else if ((strcmp(suffix+1,"pgm") == 0) || (strcmp(suffix+1,"PGM") == 0))
        in = new pgm_in(fname,dims,next_comp_idx,skip_bytes);
      else if ((strcmp(suffix+1,"ppm") == 0) || (strcmp(suffix+1,"PPM") == 0))
        in = new ppm_in(fname,dims,next_comp_idx,skip_bytes);
      else if ((strcmp(suffix+1,"bmp") == 0) || (strcmp(suffix+1,"BMP") == 0))
        in = new bmp_in(fname,dims,next_comp_idx,vflip,palette,skip_bytes);
      else if ((strcmp(suffix+1,"raw") == 0) || (strcmp(suffix+1,"RAW") == 0))
        in = new raw_in(fname,dims,next_comp_idx,skip_bytes,false);
      else if ((strcmp(suffix+1,"rawl") == 0) || (strcmp(suffix+1,"RAWL")==0))
        in = new raw_in(fname,dims,next_comp_idx,skip_bytes,true);
      else if ((strcmp(suffix+1,"tif")==0) || (strcmp(suffix+1,"TIF")==0) ||
               (strcmp(suffix+1,"tiff")==0) || (strcmp(suffix+1,"TIFF")==0))
        in = new tif_in(fname,dims,next_comp_idx,palette,skip_bytes);
    }
  if (in == NULL)
    { kdu_error e; e << "Image file, \"" << fname << ", does not have a "
      "recognized suffix.  Valid suffices are currently: "
      "\"tif\", \"tiff\", \"bmp\", \"pgm\", \"ppm\", \"raw\" and \"rawl\".  "
      "Upper or lower case may be used, but must be used consistently.";
    }
}

/* ========================================================================= */
/*                                   pbm_in                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                               pbm_in::pbm_in                              */
/*****************************************************************************/

pbm_in::pbm_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
               kdu_rgb8_palette *palette, kdu_long skip_bytes)
{
  char magic[3];

  if ((in = fopen(fname,"rb")) == NULL)
    { kdu_error e;
      e << "Unable to open input image file, \"" << fname <<"\".";}
  if (skip_bytes > 0)
    kdu_fseek(in,skip_bytes);
  magic[0] = magic[1] = magic[2] = '\0';
  fread(magic,1,2,in);
  if (strcmp(magic,"P4") != 0)
    { kdu_error e; e << "PBM image file must start with the magic string, "
      "\"P4\"!"; }
  bool failed = false;
  eat_white_and_comments(in);
  if (fscanf(in,"%d",&cols) != 1)
    failed = true;
  eat_white_and_comments(in);
  if (fscanf(in,"%d",&rows) != 1)
    failed = true;
  if (failed)
    {kdu_error e; e << "Image file \"" << fname << "\" does not appear to "
     "have a valid PBM header."; }
  int ch;
  while ((ch = fgetc(in)) != EOF)
    if ((ch == '\n') || (ch == ' '))
      break;
  comp_idx = next_comp_idx++;
  dims.add_component(rows,cols,1,false,comp_idx);
  if ((forced_prec = dims.get_forced_precision(comp_idx)) > 30)
    { kdu_warning w; w << "Attempting to force the precision of component "
      << comp_idx
      << " beyond 30 bits; we will force to 30 bits only.";
      forced_prec = 30;
    }
  if (forced_prec != 0)
    dims.set_bit_depth(comp_idx,forced_prec);
  if ((palette != NULL) && !palette->exists())
    {
      palette->input_bits = 1;
      palette->output_bits = 8;
      palette->source_component = comp_idx;
      palette->blue[0] = palette->green[0] = palette->red[0] = 0;
      palette->blue[1] = palette->green[1] = palette->red[1] = 255;
      // Note that we will be flipping the bits so that a 0 really does
      // represent black, rather than white -- this is more efficient for
      // coding facsimile type images where the background is white.
    }
  incomplete_lines = free_lines = NULL;
  num_unread_rows = rows;
  initial_non_empty_tiles = 0; // Don't know yet.
}

/*****************************************************************************/
/*                               pbm_in::~pbm_in                             */
/*****************************************************************************/

pbm_in::~pbm_in()
{
  if ((num_unread_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image component "
        << comp_idx << " were consumed!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  fclose(in);
}

/*****************************************************************************/
/*                                 pbm_in::get                               */
/*****************************************************************************/

bool
  pbm_in::get(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  assert(comp_idx == this->comp_idx);
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(line.get_width() == 0);
      return true;
    }

  image_line_buf *scan, *prev=NULL;
  kdu_byte *sp;
  int n;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to read a new image line.
      assert(x_tnum == 0); // Must consume line from left to right.
      if (num_unread_rows == 0)
        return false;
      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(cols+7,1);
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      n = (cols+7) >> 3;
      if (fread(scan->buf,1,(size_t) n,in) != (size_t) n)
        { kdu_error e; e << "Image file for component " << comp_idx
          << " terminated prematurely!"; }
      // Expand the packed representation into whole bytes, flipping and
      // storing each binary digit in the LSB of a single byte.  The reason
      // for flipping is that PBM files represent white using a 0 and
      // black using a 1, but the more natural and also more efficient
      // representation for coding unsigned data with 1-bit precision in
      // JPEG2000 is the opposite.
      sp = scan->buf + n;
      kdu_byte val, *dp = scan->buf + (n<<3);
      for (; n > 0; n--)
        {
          val = *(--sp); val = ~val;
          *(--dp) = (val&1); val >>= 1;    *(--dp) = (val&1); val >>= 1;
          *(--dp) = (val&1); val >>= 1;    *(--dp) = (val&1); val >>= 1;
          *(--dp) = (val&1); val >>= 1;    *(--dp) = (val&1); val >>= 1;
          *(--dp) = (val&1); val >>= 1;    *(--dp) = (val&1);
        }
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
      num_unread_rows--;
    }
  assert((cols-scan->accessed_samples) >= line.get_width());

  sp = scan->buf+scan->accessed_samples;
  n = line.get_width();

  if (line.get_buf32() != NULL)
    {
      kdu_sample32 *dp = line.get_buf32();
      if (line.is_absolute())
        { // 32-bit absolute integers
          for (; n > 0; n--, sp++, dp++)
            dp->ival = ((kdu_int32)(*sp)) - 1;
        }
      else
        { // true 32-bit floats
          for (; n > 0; n--, sp++, dp++)
            dp->fval = (((float)(*sp)) / 2.0F) - 0.5F;
        }
    }
  else
    {
      kdu_sample16 *dp = line.get_buf16();
      if (line.is_absolute())
        { // 16-bit absolute integers
          for (; n > 0; n--, sp++, dp++)
            dp->ival = ((kdu_int16)(*sp)) - 1;
        }
      else
        { // 16-bit normalized representation.
          for (; n > 0; n--, sp++, dp++)
            dp->ival = (((kdu_int16)(*sp)) - 1) << (KDU_FIX_POINT-1);
        }
    }
  if (forced_prec != 0)
    force_sample_precision(line,forced_prec,1,false);

  scan->next_x_tnum++;
  scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == cols)
    { // Send empty line to free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }

  return true;
}


/* ========================================================================= */
/*                                   pgm_in                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                               pgm_in::pgm_in                              */
/*****************************************************************************/

pgm_in::pgm_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
               kdu_long skip_bytes)
{
  char magic[3];
  int max_val; // We don't actually use this.

  if ((in = fopen(fname,"rb")) == NULL)
    { kdu_error e;
      e << "Unable to open input image file, \"" << fname <<"\"."; }
  if (skip_bytes > 0)
    kdu_fseek(in,skip_bytes);
  magic[0] = magic[1] = magic[2] = '\0';
  fread(magic,1,2,in);
  if (strcmp(magic,"P5") != 0)
    { kdu_error e; e << "PGM image file must start with the magic string, "
      "\"P5\"!"; }
  bool failed = false;
  eat_white_and_comments(in);
  if (fscanf(in,"%d",&cols) != 1)
    failed = true;
  eat_white_and_comments(in);
  if (fscanf(in,"%d",&rows) != 1)
    failed = true;
  eat_white_and_comments(in);
  if (fscanf(in,"%d",&max_val) != 1)
    failed = true;
  if (failed)
    {kdu_error e; e << "Image file \"" << fname << "\" does not appear to "
     "have a valid PGM header."; }
  int ch;
  while ((ch = fgetc(in)) != EOF)
    if ((ch == '\n') || (ch == ' '))
      break;
  comp_idx = next_comp_idx++;
  dims.add_component(rows,cols,8,false,comp_idx);
  if ((forced_prec = dims.get_forced_precision(comp_idx)) > 30)
    { kdu_warning w; w << "Attempting to force the precision of component "
                       << comp_idx
                       << " beyond 30 bits; we will force to 30 bits only.";
      forced_prec = 30;
    }
  if (forced_prec != 0)
    dims.set_bit_depth(comp_idx,forced_prec);
  incomplete_lines = free_lines = NULL;
  num_unread_rows = rows;
  initial_non_empty_tiles = 0; // Don't know yet.
}

/*****************************************************************************/
/*                               pgm_in::~pgm_in                             */
/*****************************************************************************/

pgm_in::~pgm_in()
{
  if ((num_unread_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image component "
        << comp_idx << " were consumed!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  fclose(in);
}

/*****************************************************************************/
/*                                 pgm_in::get                               */
/*****************************************************************************/

bool
  pgm_in::get(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  assert(comp_idx == this->comp_idx);
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(line.get_width() == 0);
      return true;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to read a new image line.
      assert(x_tnum == 0); // Must consume line from left to right.
      if (num_unread_rows == 0)
        return false;
      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(cols,1);
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      if (fread(scan->buf,1,(size_t) scan->width,in) != (size_t) scan->width)
        { kdu_error e; e << "Image file for component " << comp_idx
          << " terminated prematurely!"; }
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
      num_unread_rows--;
    }
  assert((scan->width-scan->accessed_samples) >= line.get_width());

  kdu_byte *sp = scan->buf+scan->accessed_samples;
  int n=line.get_width();

  if (line.get_buf32() != NULL)
    {
      kdu_sample32 *dp = line.get_buf32();
      if (line.is_absolute())
        { // 32-bit absolute integers
          for (; n > 0; n--, sp++, dp++)
            dp->ival = ((kdu_int32)(*sp)) - 128;
        }
      else
        { // true 32-bit floats
          for (; n > 0; n--, sp++, dp++)
            dp->fval = (((float)(*sp)) / 256.0F) - 0.5F;
        }
    }
  else
    {
      kdu_sample16 *dp = line.get_buf16();
      if (line.is_absolute())
        { // 16-bit absolute integers
          for (; n > 0; n--, sp++, dp++)
            dp->ival = ((kdu_int16)(*sp)) - 128;
        }
      else
        { // 16-bit normalized representation.
          for (; n > 0; n--, sp++, dp++)
            dp->ival = (((kdu_int16)(*sp)) - 128) << (KDU_FIX_POINT-8);
        }
    }
  if (forced_prec != 0)
    force_sample_precision(line,forced_prec,8,false);        

  scan->next_x_tnum++;
  scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == scan->width)
    { // Send empty line to free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }

  return true;
}


/* ========================================================================= */
/*                                   ppm_in                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                               ppm_in::ppm_in                              */
/*****************************************************************************/

ppm_in::ppm_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
               kdu_long skip_bytes)
{
  char magic[3];
  int max_val; // We don't actually use this.
  int n;

  if ((in = fopen(fname,"rb")) == NULL)
    { kdu_error e;
      e << "Unable to open input image file, \"" << fname <<"\"."; }
  if (skip_bytes > 0)
    kdu_fseek(in,skip_bytes);
  magic[0] = magic[1] = magic[2] = '\0';
  fread(magic,1,2,in);
  if (strcmp(magic,"P6") != 0)
    { kdu_error e; e << "PPM image file must start with the magic string, "
      "\"P6\"!"; }
  bool failed = false;
  eat_white_and_comments(in);
  if (fscanf(in,"%d",&cols) != 1)
    failed = true;
  eat_white_and_comments(in);
  if (fscanf(in,"%d",&rows) != 1)
    failed = true;
  eat_white_and_comments(in);
  if (fscanf(in,"%d",&max_val) != 1)
    failed = true;
  if (failed)
    {kdu_error e; e << "Image file \"" << fname << "\" does not appear to "
     "have a valid PPM header."; }
  int ch;
  while ((ch = fgetc(in)) != EOF)
    if ((ch == '\n') || (ch == ' '))
      break;
  first_comp_idx = next_comp_idx;
  for (n=0; n < 3; n++)
    {
      dims.add_component(rows,cols,8,false,next_comp_idx);
      if ((forced_prec[n] = dims.get_forced_precision(next_comp_idx)) > 30)
        { kdu_warning w; w << "Attempting to force the precision of component "
          << next_comp_idx
          << " beyond 30 bits; we will force to 30 bits only.";
          forced_prec[n] = 30;
        }
      if (forced_prec[n] != 0)
        dims.set_bit_depth(next_comp_idx,forced_prec[n]);
      next_comp_idx++;
    }
  incomplete_lines = NULL;
  free_lines = NULL;
  num_unread_rows = rows;
  initial_non_empty_tiles = 0; // Don't know yet.
}

/*****************************************************************************/
/*                               ppm_in::~ppm_in                             */
/*****************************************************************************/

ppm_in::~ppm_in()
{
  if ((num_unread_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image components "
        << first_comp_idx << " through " << first_comp_idx+2
        << " were consumed!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  fclose(in);
}

/*****************************************************************************/
/*                                 ppm_in::get                               */
/*****************************************************************************/

bool
  ppm_in::get(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  int idx = comp_idx - this->first_comp_idx;
  assert((idx >= 0) && (idx <= 2));
  x_tnum = x_tnum*3+idx; // Works so long as components read in order.
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(line.get_width() == 0);
      return true;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to read a new image line.
      assert(x_tnum == 0); // Must consume in very specific order.
      if (num_unread_rows == 0)
        return false;
      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(cols,3);
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      if (fread(scan->buf,1,(size_t)(scan->width*3),in) !=
          (size_t)(scan->width*3))
        { kdu_error e; e << "Image file for components " << first_comp_idx
          << " through " << first_comp_idx+2 << " terminated prematurely!"; }
      num_unread_rows--;
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
    }

  assert((scan->width-scan->accessed_samples) >= line.get_width());

  kdu_byte *sp = scan->buf+3*scan->accessed_samples+idx;
  int n=line.get_width();
  if (line.get_buf32() != NULL)
    {
      kdu_sample32 *dp = line.get_buf32();
      if (line.is_absolute())
        { // 32-bit absolute integers
          for (; n > 0; n--, sp+=3, dp++)
            dp->ival = ((kdu_int32)(*sp)) - 128;
        }
      else
        { // true 32-bit floats
          for (; n > 0; n--, sp+=3, dp++)
            dp->fval = (((float)(*sp)) / 256.0F) - 0.5F;
        }
    }
  else
    {
      kdu_sample16 *dp = line.get_buf16();
      if (line.is_absolute())
        { // 16-bit absolute integers
          for (; n > 0; n--, sp+=3, dp++)
            dp->ival = ((kdu_int16)(*sp)) - 128;
        }
      else
        { // 16-bit normalized representation.
          for (; n > 0; n--, sp+=3, dp++)
            dp->ival = (((kdu_int16)(*sp)) - 128) << (KDU_FIX_POINT-8);
        }
    }
  if (forced_prec[idx] != 0)
    force_sample_precision(line,forced_prec[idx],8,false);        

  scan->next_x_tnum++;
  if (idx == 2)
    scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == scan->width)
    { // Send empty line to free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }

  return true;
}


/* ========================================================================= */
/*                                   raw_in                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                               raw_in::raw_in                              */
/*****************************************************************************/

raw_in::raw_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
               kdu_long skip_bytes, bool littlendian)
{
  if ((in = fopen(fname,"rb")) == NULL)
    { kdu_error e;
      e << "Unable to open input image file, \"" << fname <<"\".";}
  if (skip_bytes > 0)
    kdu_fseek(in,skip_bytes);
  comp_idx = next_comp_idx++;
  while (comp_idx >= dims.get_num_components())
    dims.append_component();
  rows = dims.get_height(comp_idx);
  cols = dims.get_width(comp_idx);
  precision = dims.get_bit_depth(comp_idx);
  is_signed = dims.get_signed(comp_idx);
  if ((rows<=0) || (cols<=0) || (precision<=0))
    { kdu_error e;
      e << "To use the raw image input file format, you must explicitly "
           "supply image component dimensions, image sample bit-depth and "
           "signed/unsigned information, using \"Sdims\", in conjunction "
           "with \"Sprecision\" and \"Ssigned\" or \"Mprecision\" and "
           "\"Msigned\", as appropriate.";
    }
  if (precision > 32)
    { kdu_error e; e << "Current implementation does not support "
      "image sample bit-depths in excess of 32 bits!"; }
  if ((forced_prec = dims.get_forced_precision(comp_idx)) > 30)
    { kdu_warning w; w << "Attempting to force the precision of component "
      << comp_idx
      << " beyond 30 bits; we will force to 30 bits only.";
      forced_prec = 30;
    }
  if (forced_prec != 0)
    dims.set_bit_depth(comp_idx,forced_prec);
  
  num_unread_rows = rows;
  sample_bytes = (precision+7)>>3;
  incomplete_lines = free_lines = NULL;
  initial_non_empty_tiles = 0; // Don't know yet.
  this->littlendian = littlendian;
}

/*****************************************************************************/
/*                               raw_in::~raw_in                             */
/*****************************************************************************/

raw_in::~raw_in()
{
  if ((num_unread_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image component "
        << comp_idx << " were consumed!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  fclose(in);
}

/*****************************************************************************/
/*                                 raw_in::get                               */
/*****************************************************************************/

bool
  raw_in::get(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  assert(comp_idx == this->comp_idx);
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(line.get_width() == 0);
      return true;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to read a new image line.
      assert(x_tnum == 0); // Must consume line from left to right.
      if (num_unread_rows == 0)
        return false;
      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(cols,sample_bytes);
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      if (fread(scan->buf,1,(size_t)(scan->width*scan->sample_bytes),in) !=
          (size_t)(scan->width*scan->sample_bytes))
        { kdu_error e; e << "Image file for component " << comp_idx
          << " terminated prematurely!"; }


      num_unread_rows--;
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
    }
  assert((scan->width-scan->accessed_samples) >= line.get_width());

  if (line.get_buf32() != NULL)
    {
      if (line.is_absolute())
        convert_words_to_ints(scan->buf+sample_bytes*scan->accessed_samples,
                              line.get_buf32(),line.get_width(),
                              precision,is_signed,sample_bytes,
                              littlendian);
      else
        convert_words_to_floats(scan->buf+sample_bytes*scan->accessed_samples,
                                line.get_buf32(),line.get_width(),
                                precision,is_signed,sample_bytes,
                                littlendian);
    }
  else
    {
      if (line.is_absolute())
        convert_words_to_shorts(scan->buf+sample_bytes*scan->accessed_samples,
                                line.get_buf16(),line.get_width(),
                                precision,is_signed,sample_bytes,
                                littlendian);
      else
        convert_words_to_fixpoint(scan->buf +
                                  sample_bytes*scan->accessed_samples,
                                  line.get_buf16(),line.get_width(),
                                  precision,is_signed,sample_bytes,
                                  littlendian);
    }
  if (forced_prec != 0)
    force_sample_precision(line,forced_prec,precision,is_signed);        

  scan->next_x_tnum++;
  scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == scan->width)
    { // Send empty line to free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }

  return true;
}


/* ========================================================================= */
/*                                   bmp_in                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                               bmp_in::bmp_in                              */
/*****************************************************************************/

bmp_in::bmp_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
               bool &vflip, kdu_rgb8_palette *palette,
               kdu_long skip_bytes)
{
  int n;

  if ((in = fopen(fname,"rb")) == NULL)
    { kdu_error e;
      e << "Unable to open input image file, \"" << fname <<"\"."; }
  if (skip_bytes > 0)
    kdu_fseek(in,skip_bytes);

  kdu_byte magic[14];
  bmp_header header;
  fread(magic,1,14,in);
  if ((magic[0] != 'B') || (magic[1] != 'M') || (fread(&header,1,40,in) != 40))
    { kdu_error e; e << "BMP image file must start with the magic string, "
      "\"BM\", and continue with a header whose total size is at least 54 "
      "bytes."; }
  from_little_endian((kdu_int32 *) &header,10);
  if (header.compression != 0)
    { kdu_error e; e << "BMP image file contains a compressed "
      "representation.  Processing of BMP compression types is certainly "
      "not within the scope of this JPEG2000-centric demonstration "
      "application.  Try loading your file into an image editing application "
      "and saving it again in an uncompressed format."; }
  cols = header.width;
  rows = header.height;
  if (rows < 0)
    rows = -rows;
  else
    vflip = true;

  if ((header.xpels_per_metre > 0) && (header.ypels_per_metre > 0))
    dims.set_resolution(cols,rows,true,(double) header.xpels_per_metre,
                        (double) header.ypels_per_metre);

  bytes = nibbles = bits = expand_palette = false;
  precision = 8;
  int bit_count = (header.planes_bits>>16);
  if (bit_count == 32)
    num_components = 4;
  else if (bit_count == 24)
    num_components = 3;
  else if (bit_count == 8)
    { num_components = 1; bytes = true; }
  else if (bit_count == 4)
    { num_components = 1; nibbles = true; precision = 4; }
  else if (bit_count == 1)
    { num_components = 1; bits = true; precision = 1; }
  else
    { kdu_error e;
      e << "We currently support only 1-, 4-, 8-, 24- and 32-bit BMP files."; }
  int palette_entries_used = header.num_colours_used;
  if (num_components != 1)
    palette_entries_used = 0;
  else if (header.num_colours_used == 0)
    palette_entries_used = (1<<precision);
  int header_size = 54 + 4*palette_entries_used;

  int offset = magic[13];
  offset <<= 8; offset += magic[12];
  offset <<= 8; offset += magic[11];
  offset <<= 8; offset += magic[10];
  if (offset < header_size)
    { kdu_error e; e << "Invalid sample data offset field specified in BMP "
      "file header!"; }
  if (num_components == 1)
    {
      assert((palette_entries_used >= 0) && (palette_entries_used <= 256));
      fread(map,1,(size_t)(4*palette_entries_used),in);
      for (n=palette_entries_used; n < 256; n++)
        map[4*n] = map[4*n+1] = map[4*n+2] = map[4*n+3] = 0;
      if (bytes)
        {
          for (n=0; n < palette_entries_used; n++)
            if ((map[4*n] != n) || (map[4*n+1] != n) || (map[4*n+2] != n))
              break;
          if (n == palette_entries_used)
            bytes = false; // No need to use palette
        }
    }
  if (bytes || nibbles || bits)
    {
      if ((palette == NULL) || palette->exists())
        {
          expand_palette = true; // Need to expand the palette here.
          precision = 8;
          for (n=0; n < 256; n++)
            if ((map[4*n] != map[4*n+1]) || (map[4*n] != map[4*n+2]))
              break; // Not a monochrome source.
          num_components = (n==256)?1:3;
        }
      else
        { // Set up the colour palette.
          palette->input_bits = precision;
          palette->output_bits = 8;
          palette->source_component = next_comp_idx;
          for (n=0; n < (1<<precision); n++)
            {
              palette->blue[n] = map[4*n+0];
              palette->green[n] = map[4*n+1];
              palette->red[n] = map[4*n+2];
            }
          for (n=0; n < (1<<precision); n++)
            map[n] = n; // Set identity permutation for now.
          if (nibbles || bytes)
            palette->rearrange(map); // Try to find a better permutation.
        }
    }
  if (offset > header_size)
    fseek(in,offset-header_size,SEEK_CUR);

  first_comp_idx = next_comp_idx;
  assert(num_components <= 4);
  for (n=0; n < num_components; n++)
    {
      dims.add_component(rows,cols,precision,false,next_comp_idx);
      if ((forced_prec[n] = dims.get_forced_precision(next_comp_idx)) > 30)
        { kdu_warning w; w << "Attempting to force the precision of component "
          << next_comp_idx
          << " beyond 30 bits; we will force to 30 bits only.";
          forced_prec[n] = 30;
        }
      if (forced_prec[n] != 0)
        dims.set_bit_depth(next_comp_idx,forced_prec[n]);
      next_comp_idx++;
    }
  incomplete_lines = NULL;
  free_lines = NULL;
  num_unread_rows = rows;
  if (bytes)
    line_bytes = cols;
  else if (nibbles)
    line_bytes = (cols+1)>>1;
  else if (bits)
    line_bytes = (cols+7)>>3;
  else
    line_bytes = cols*num_components;
  line_bytes += (-line_bytes) & 3; // Pad to a multiple of 4 bytes.
  initial_non_empty_tiles = 0; // Don't know yet.
}

/*****************************************************************************/
/*                               bmp_in::~bmp_in                             */
/*****************************************************************************/

bmp_in::~bmp_in()
{
  if ((num_unread_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image components "
        << first_comp_idx << " through "
        << first_comp_idx+num_components-1
        << " were consumed!";
    }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
  while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
  fclose(in);
}

/*****************************************************************************/
/*                                 bmp_in::get                               */
/*****************************************************************************/

bool
  bmp_in::get(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  int idx = comp_idx - this->first_comp_idx;
  assert((idx >= 0) && (idx < num_components));
  x_tnum = x_tnum*num_components+idx;
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(line.get_width() == 0);
      return true;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to read a new image line.
      assert(x_tnum == 0); // Must consume in very specific order.
      if (num_unread_rows == 0)
        return false;
      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(cols+7,num_components);
                          // Big enough for padding and expanding bits to bytes
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      if (fread(scan->buf,1,(size_t) line_bytes,in) != (size_t) line_bytes)
        { kdu_error e; e << "Image file for components " << first_comp_idx
          << " through " << first_comp_idx+num_components-1
          << " terminated prematurely!"; }
      num_unread_rows--;
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;
      if (bytes)
        map_palette_index_bytes(scan->buf,line.is_absolute());
      else if (nibbles)
        map_palette_index_nibbles(scan->buf,line.is_absolute());
      else if (bits)
        map_palette_index_bits(scan->buf,line.is_absolute());
    }
  assert((cols-scan->accessed_samples) >= line.get_width());

  int comp_offset = 0;
  if (num_components >= 3)
    {
      comp_offset = 2-idx;
      if (comp_offset < 0)
        { // Must be the alpha channel
          assert(num_components == 4);
          comp_offset = 3;
        }
    }

  kdu_byte *sp = scan->buf+num_components*scan->accessed_samples + comp_offset;
  int n=line.get_width();

  if (line.get_buf32() != NULL)
    {
      kdu_sample32 *dp = line.get_buf32();
      if (line.is_absolute())
        { // 32-bit absolute integers
          kdu_int32 offset = 128;
          if ((num_components == 1) && nibbles) offset = 8;
          if ((num_components == 1) && bits) offset = 1;
          for (; n > 0; n--, sp+=num_components, dp++)
            dp->ival = ((kdu_int32)(*sp)) - offset;
        }
      else
        { // true 32-bit floats
          for (; n > 0; n--, sp+=num_components, dp++)
            dp->fval = (((float)(*sp)) / 256.0F) - 0.5F;
        }
    }
  else
    {
      kdu_sample16 *dp = line.get_buf16();
      if (line.is_absolute())
        { // 16-bit absolute integers
          kdu_int16 offset = 128;
          if ((num_components == 1) && nibbles) offset = 8;
          if ((num_components == 1) && bits) offset = 1;
          for (; n > 0; n--, sp+=num_components, dp++)
            dp->ival = ((kdu_int16)(*sp)) - offset;
        }
      else
        { // 16-bit normalized representation.
          for (; n > 0; n--, sp+=num_components, dp++)
            dp->ival = (((kdu_int16)(*sp)) - 128) << (KDU_FIX_POINT-8);
        }
    }
  if (forced_prec[idx] != 0)
    force_sample_precision(line,forced_prec[idx],precision,false);

  scan->next_x_tnum++;
  if (idx == (num_components-1))
    scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == cols)
    { // Send empty line to free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }

  return true;
}

/*****************************************************************************/
/*                       bmp_in::map_palette_index_bytes                     */
/*****************************************************************************/

void
  bmp_in::map_palette_index_bytes(kdu_byte *buf, bool absolute)
{
  int n = cols;

  if (num_components == 3)
    { // Expand single component through palette
      assert(expand_palette);
      kdu_byte *sp = buf + n;
      kdu_byte *dp = buf + (3*n);
      kdu_byte *mp;
      for (; n > 0; n--)
        {
          mp = map + (((int) *(--sp))<<2);
          *(--dp) = mp[2]; *(--dp) = mp[1]; *(--dp) = mp[0];
        }
    }
  else if (expand_palette)
    {
      assert(num_components == 1);
      kdu_byte *sp = buf + n;
      kdu_byte *dp = buf + n;
      for (; n > 0; n--)
        *(--dp) = map[((int) *(--sp))<<2];
    }
  else
    { // Apply optimized permutation map to the palette indices
      assert(num_components == 1);
      for (; n > 0; n--, buf++)
        *buf = map[*buf];
    }
}

/*****************************************************************************/
/*                      bmp_in::map_palette_index_nibbles                    */
/*****************************************************************************/

void
  bmp_in::map_palette_index_nibbles(kdu_byte *buf, bool absolute)
{
  int n = (cols+1)>>1;

  if (num_components == 3)
    { // Expand single component through palette
      assert(expand_palette);
      kdu_byte *sp = buf + n;
      kdu_byte *dp = buf + (6*n);
      kdu_byte *mp;
      kdu_uint32 val;
      for (; n > 0; n--)
        {
          val = *(--sp);
          mp = map + ((val & 0x0F)<<2);
          *(--dp) = mp[2]; *(--dp) = mp[1]; *(--dp) = mp[0];
          val >>= 4;
          mp = map + ((val & 0x0F)<<2);
          *(--dp) = mp[2]; *(--dp) = mp[1]; *(--dp) = mp[0];
        }
    }
  else if (expand_palette)
    {
      assert(num_components == 1);
      kdu_byte *sp = buf + n;
      kdu_byte *dp = buf + (2*n);
      kdu_uint32 val;
      for (; n > 0; n--)
        {
          val = *(--sp);
          *(--dp) = map[(val & 0x0F) << 2];
          val >>= 4;
          *(--dp) = map[(val & 0x0F) << 2];
        }
    }
  else
    { // Apply optimized permutation map to the palette indices
      assert(num_components == 1);
      kdu_byte *sp = buf + n;
      kdu_byte *dp = buf + (2*n);
      kdu_byte val;
      if (absolute)
        { // Map nibbles and store in least significant 4 bits of byte
          for (; n > 0; n--)
            {
              val = *(--sp);
              *(--dp) = map[val & 0x0F];
              val >>= 4;
              *(--dp) = map[val & 0x0F];
            }
        }
      else
        { // Map nibbles and store in most significant 4 bits of byte
          for (; n > 0; n--)
            {
              val = *(--sp);
              *(--dp) = map[val & 0x0F]<<4;
              val >>= 4;
              *(--dp) = map[val & 0x0F]<<4;
            }
        }
    }
}

/*****************************************************************************/
/*                       bmp_in::map_palette_index_bits                      */
/*****************************************************************************/

void
  bmp_in::map_palette_index_bits(kdu_byte *buf, bool absolute)
{
  int b, n = (cols+7)>>3;

  if (num_components == 3)
    { // Expand single component through palette
      assert(expand_palette);
      kdu_byte *sp = buf + n;
      kdu_byte *dp = buf + (24*n);
      kdu_byte *mp;
      kdu_uint32 val;
      for (; n > 0; n--)
        {
          val = *(--sp);
          for (b=8; b > 0; b--, val>>=1)
            {
              mp = map + ((val & 1)<<2);
              *(--dp) = mp[2]; *(--dp) = mp[1]; *(--dp) = mp[0];
            }
        }
    }
  else if (expand_palette)
    {
      assert(num_components == 1);
      kdu_byte *sp = buf + n;
      kdu_byte *dp = buf + (8*n);
      kdu_uint32 val;
      for (; n > 0; n--)
        {
          val = *(--sp);
          for (b=8; b > 0; b--, val>>=1)
            *(--dp) = map[(val & 1)<<2];
        }
    }
  else
    { // Apply optimized permutation map to the palette indices
      assert(num_components == 1);
      kdu_byte *sp = buf + n;
      kdu_byte *dp = buf + (8*n);
      kdu_byte val;
      if (absolute)
        { // Store bits in LSB's of bytes
          for (; n > 0; n--)
            {
              val = *(--sp);
              for (b=8; b > 0; b--, val>>=1)
                *(--dp) = val&1;
            }
        }
      else
        { // Store bits in MSB's of bytes
          for (; n > 0; n--)
            {
              val = *(--sp);
              for (b=8; b > 0; b--, val>>=1)
                *(--dp) = (val&1)<<7;
            }
        }
    }
}


/* ========================================================================= */
/*                                   tif_in                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                               tif_in::tif_in                              */
/*****************************************************************************/

tif_in::tif_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
               kdu_rgb8_palette *palette, kdu_long skip_bytes)
{
  // Initialize state information in case we have to cleanup prematurely
  forced_prec = NULL;
  is_signed = NULL;
  float_minvals = float_maxvals = NULL;
  incomplete_lines = NULL;
  free_lines = NULL;
  num_unread_rows = 0;
  initial_non_empty_tiles = 0; // Don't know yet.
#ifdef KDU_INCLUDE_TIFF
  libtiff_in = NULL;
  chunk_buf = NULL;
#endif // KDU_INCLUDE_TIFF
  chunk_offsets = NULL;
  if (skip_bytes != 0)
    { kdu_error e; e << "Trying to open a TIFF file while skipping some of "
      "the initial bytes.  Current implementation does not support this."; }

  kdu_tiffdir tiffdir;
  if (!(src.open(fname,true,true) && tiffdir.opendir(&src)))
    { kdu_error e; e << "Unable to open TIFF source file, \""
      << fname << "\"."; }

  int n;
  kdu_uint16 photometrictype;
  kdu_uint32 imagewidth, imageheight;
  if (!((tiffdir.read_tag(KDU_TIFFTAG_ImageWidth16,1,&imagewidth) ||
         tiffdir.read_tag(KDU_TIFFTAG_ImageWidth32,1,&imagewidth)) &&
        (tiffdir.read_tag(KDU_TIFFTAG_ImageHeight16,1,&imageheight) ||
         tiffdir.read_tag(KDU_TIFFTAG_ImageHeight32,1,&imageheight)) &&
        tiffdir.read_tag(KDU_TIFFTAG_PhotometricInterp,1,&photometrictype)))
    { kdu_error e; e << "TIFF source file does not appear to contain all "
      "mandatory tags."; }

  bool tiled_tiff = false;
  if (tiffdir.read_tag(KDU_TIFFTAG_TileWidth16,1,&tile_width) ||
      tiffdir.read_tag(KDU_TIFFTAG_TileWidth32,1,&tile_width))
    { // Image is tiled
      tiled_tiff = true;
      if (!((tiffdir.read_tag(KDU_TIFFTAG_TileHeight16,1,&tile_height) ||
             tiffdir.read_tag(KDU_TIFFTAG_TileHeight32,1,&tile_height)) &&
            (tile_width > 0) && (tile_height > 0)))
        { kdu_error e; e << "Problem encountered in TIFF file: zero-valued "
          "tile width or tile height signalled in file directory."; }
    }
  else
    {
      tile_width = imagewidth;
      if (!(tiffdir.read_tag(KDU_TIFFTAG_RowsPerStrip16,1,&tile_height) ||
            tiffdir.read_tag(KDU_TIFFTAG_RowsPerStrip32,1,&tile_height)))
        tile_height = imageheight;
      if (tile_height > imageheight)
        tile_height = imageheight;
    }
  tiles_across = (int)((imagewidth+tile_width-1) / tile_width);
  tiles_down = (int)((imageheight+tile_height-1) / tile_height);
  chunks_per_component = tiles_across * tiles_down;

  samplesperpixel = 1; // TIFF Default
  kdu_uint16 compressiontype = KDU_TIFF_Compression_NONE; // TIFF Default
  kdu_uint16 extrasamples = 0;
  tiffdir.read_tag(KDU_TIFFTAG_SamplesPerPixel,1,&samplesperpixel);
  tiffdir.read_tag(KDU_TIFFTAG_Compression,1,&compressiontype);
  tiffdir.read_tag(KDU_TIFFTAG_ExtraSamples,1,&extrasamples);

  kdu_uint16 planarconfig = KDU_TIFF_PlanarConfig_CONTIG; // TIFF Default
  tiffdir.read_tag(KDU_TIFFTAG_PlanarConfig,1,&planarconfig);
  planar_organization =
    (samplesperpixel > 1) && (planarconfig != KDU_TIFF_PlanarConfig_CONTIG);

  precision = 0;
  num_components = (int) samplesperpixel;
  is_signed = new bool[num_components+2];
  for (n=0; n < num_components; n++)
    {
      if (!tiffdir.read_tag(KDU_TIFFTAG_BitsPerSample,1,&bitspersample))
        { kdu_error e; e << "TIFF source file does not appear to contain the "
          "required \"BitsPerSample\" tag -- or at least it does not have "
          "enough entries for this tag."; }
      if ((n > 0) && (precision != (int) bitspersample))
        { kdu_error e; e << "The TIFF reading code in this demo application "
          "cannot cope with TIFF files whose image planes each have different "
          "bit depths."; }
      precision = (int) bitspersample;

      kdu_uint16 sampleformat = KDU_TIFF_SampleFormat_UNSIGNED; // Default
      tiffdir.read_tag(KDU_TIFFTAG_SampleFormat,1,&sampleformat);
      if ((sampleformat == KDU_TIFF_SampleFormat_FLOAT) && (n == 0))
        {
          float_minvals = new double[num_components];
          float_maxvals = new double[num_components];
        }
      if (((sampleformat == KDU_TIFF_SampleFormat_FLOAT) &&
           (float_minvals == NULL)) ||
          ((sampleformat != KDU_TIFF_SampleFormat_FLOAT) &&
           (float_minvals != NULL)))
        { kdu_error e; e << "The TIFF reading code in this demo "
          "application cannot cope with TIFF files which contain some "
          "image planes with floating point samples and others with "
          "integer-valued samples.  We require all image planes "
          "to have the same sample format."; }
      if (sampleformat == KDU_TIFF_SampleFormat_FLOAT)
        {
          double minval, maxval;
          if (!((tiffdir.read_tag(KDU_TIFFTAG_SminSampleValueF,1,&minval) ||
                 tiffdir.read_tag(KDU_TIFFTAG_SminSampleValueD,1,&minval)) &&
                (tiffdir.read_tag(KDU_TIFFTAG_SmaxSampleValueF,1,&maxval) ||
                 tiffdir.read_tag(KDU_TIFFTAG_SmaxSampleValueD,1,&maxval)) &&
                (maxval > minval)))
            { kdu_error e; e << "TIFF source file contains floating point "
              "sample values, but no valid range for the sample values can "
              "be recovered from the file; this should typically be "
              "done via the `SminSampleValue' and `SmaxSampleValue' tags.  "
              "Without this information, it is not possible to sensibly "
              "compress the data."; }
          float_minvals[n] = minval;
          float_maxvals[n] = maxval;
          is_signed[n] = ((minval+maxval) < (0.25*maxval));
        }
      else if (sampleformat == KDU_TIFF_SampleFormat_UNSIGNED)
        is_signed[n] = false;
      else if (sampleformat == KDU_TIFF_SampleFormat_SIGNED)
        is_signed[n] = true;
      else
        { kdu_error e; e << "Unrecognized image sample format "
          "encountered in TIFF source file.  This application accepts "
          "unsigned or signed integer representations as well as single "
          "or double precision floating point representations, so you "
          "must have something really weird here!!"; }
    }
  cols = (int) imagewidth;
  rows = (int) imageheight;
  
  double xres=1.0, yres=1.0;
  kdu_uint16 resolution_unit=KDU_TIFF_ResolutionUnit_INCH; // TIFF Default
  tiffdir.read_tag(KDU_TIFFTAG_ResolutionUnit,1,&resolution_unit);
  tiffdir.read_tag(KDU_TIFFTAG_XResolution,1,&xres);
  tiffdir.read_tag(KDU_TIFFTAG_YResolution,1,&yres);
  xres *= 100.0;  yres *= 100.0; // Convert to pixels/metre from pixels/cm
  if (resolution_unit == KDU_TIFF_ResolutionUnit_INCH)
    { xres *= 1.0F / 2.54F;  yres *= 1.0F / 2.54F; }

  dims.set_resolution(cols,rows,
                      (resolution_unit != KDU_TIFF_ResolutionUnit_NONE),
                      xres,yres);

  // Set initial parameters -- these may change if there is a colour palette
  expand_palette = remap_samples = invert_first_component = false;
  if (float_minvals != NULL)
    {
      if (bitspersample == 32)
        sample_bytes = 4; // Single precision floats
      else if (bitspersample == 64)
        sample_bytes = 8; // Double precision floats
      else
        { kdu_error e; e << "TIFF image advertises floating point sample "
          "values, but the number of bits per sample is " << bitspersample
          << " -- it should be either 32 or 64."; }
      precision = 24; // Floating point samples converted to 24-bit ints
    }
  if (bitspersample <= 8)
    sample_bytes = 1;
  else if (bitspersample <= 16)
    sample_bytes = 2;
  else if (bitspersample <= 32)
    sample_bytes = 4;
  else
    { kdu_error e; e << "TIFF image advertises more than 32 bits per sample!  "
      "This is probably illegal, but certainly outrageous, given that "
      "this is bits-per-sample, not bits-per-pixel."; }
  need_buffer_unpack =
    (bitspersample != 8) && (bitspersample != 16) && (bitspersample != 32) &&
    (bitspersample != 64);

  if (planar_organization)
    {
      chunks_across = tiles_across * samplesperpixel;
      chunkline_samples = (int) tile_width;
      pixel_gap = sample_bytes;
    }
  else
    {
      chunks_across = tiles_across;
      chunkline_samples = (int)(samplesperpixel * tile_width);
      pixel_gap = sample_bytes * samplesperpixel;
    }
  chunkline_bytes = bitspersample * chunkline_samples;
  chunkline_bytes = (chunkline_bytes+7)>>3;

  num_unread_rows = rows;

  // Now look at the colour representation
  int num_colours=0; // Actual value is set below
  int colour_space_confidence=0; // Actual value is set below
  jp2_colour_space colour_space=JP2_sLUM_SPACE; // Reasonable default
  if (photometrictype == KDU_TIFF_PhotometricInterp_PALETTE)
    { // Source image uses an RGB palette
      num_colours = 3;
      colour_space_confidence = 1;
      colour_space = JP2_sRGB_SPACE;
      if (bitspersample > 8)
        { kdu_error e; e << "TIFF input file uses a palette with more than "
          "256 entries.  Large palettes like this are not supported within "
          "the current demo app."; }
      if (is_signed[0])
        { kdu_error e; e << "TIFF input file uses a palette with signed "
        "image sample values for its entries.  This makes no sense."; }
      int lut_size = (int)(1<<bitspersample);
      kdu_uint16 pusRed[256], pusGreen[256], pusBlue[256];
      if ((tiffdir.read_tag(KDU_TIFFTAG_ColorMap,lut_size,pusRed) +
           tiffdir.read_tag(KDU_TIFFTAG_ColorMap,lut_size,pusGreen) +
           tiffdir.read_tag(KDU_TIFFTAG_ColorMap,lut_size,pusBlue)) !=
          (3*lut_size))
        { kdu_error e; e << "TIFF input file does not appear to have a "
          "valid colour palette to match its declared photometric type."; }
      if ((palette != NULL) && (!palette->exists()) && (next_comp_idx == 0))
        { // Record palette info in `palette' and compress the original indices
          palette->input_bits = bitspersample;
          palette->output_bits = 8;
          palette->source_component = next_comp_idx;
          for (int iPaletteIndex = 0;
               iPaletteIndex < (1<<bitspersample);
               iPaletteIndex++)
            {
              palette->red[iPaletteIndex]   = (pusRed[iPaletteIndex]>>8);
              palette->green[iPaletteIndex] = (pusGreen[iPaletteIndex]>>8);
              palette->blue[iPaletteIndex]  = (pusBlue[iPaletteIndex]>>8);
              map[iPaletteIndex]            = iPaletteIndex;
            }
          if (bitspersample > 1)
            {
              remap_samples = true;
              palette->rearrange(map);
            }
        }
      else
        {
          expand_palette = true;
          num_components += 2; // Expand first component into 3; allow extras
          if (!planar_organization)
            pixel_gap = sample_bytes * num_components;
          for (n=num_components-1; n >= 3; n--)
            is_signed[n] = is_signed[n-2];
          for (; n >= 0; n--)
            is_signed[n] = false;
          assert(sample_bytes == 1);
          precision = 8; // Whatever the palette size, expanded precision is 8
          for (int iPaletteIndex = 0;
               iPaletteIndex < (1<<bitspersample);
               iPaletteIndex++ )
            {
              map[(iPaletteIndex << 2) + 0] =
                (kdu_byte)(pusRed[iPaletteIndex]>>8);
              map[(iPaletteIndex << 2) + 1] =
                (kdu_byte)(pusGreen[iPaletteIndex]>>8);
              map[(iPaletteIndex << 2) + 2] =
                (kdu_byte)(pusBlue[iPaletteIndex]>>8);
            }
        }
    }
  else if ((photometrictype == KDU_TIFF_PhotometricInterp_WHITEISZERO) ||
           (photometrictype == KDU_TIFF_PhotometricInterp_BLACKISZERO))
    { // Reversed monochromatic imagery
      num_colours = 1;
      colour_space_confidence = 1;
      colour_space = JP2_sLUM_SPACE;
      if ((bitspersample == 1) && (palette != NULL) && (next_comp_idx == 0))
        { // Write a palette to ensure that white is truly white
          palette->input_bits = 1;
          palette->output_bits = 8;
          palette->source_component = next_comp_idx;
          palette->red[0] = palette->green[0] = palette->blue[0] =
            (photometrictype==KDU_TIFF_PhotometricInterp_WHITEISZERO)?255:0;
          palette->red[1] = palette->green[1] = palette->blue[1] =
            (photometrictype==KDU_TIFF_PhotometricInterp_WHITEISZERO)?0:255;
        }
      else
        invert_first_component =
          (photometrictype == KDU_TIFF_PhotometricInterp_WHITEISZERO);
    }
  else if (photometrictype == KDU_TIFF_PhotometricInterp_RGB)
    {
      num_colours = 3;
      colour_space_confidence = 1;
      colour_space = JP2_sRGB_SPACE;
    }
  else if (photometrictype == KDU_TIFF_PhotometricInterp_SEPARATED)
    {
      kdu_uint16 inkset = KDU_TIFF_InkSet_CMYK;
      kdu_uint16 numberofinks = 4;
      tiffdir.read_tag(KDU_TIFFTAG_InkSet,1,&inkset);
      tiffdir.read_tag(KDU_TIFFTAG_NumberOfInks,1,&numberofinks);
      num_colours = (int) numberofinks;
      if (num_colours == 4)
        colour_space = JP2_CMYK_SPACE;
      if ((num_colours == 4) && (inkset == KDU_TIFF_InkSet_CMYK))
        colour_space_confidence = 1;
    }
  else
    num_colours = num_components - (int) extrasamples;

  if (next_comp_idx == 0)
    dims.set_colour_info(num_colours,(extrasamples==1),false,
                         colour_space_confidence,colour_space);

  forced_prec = new int[num_components];
  first_comp_idx = next_comp_idx;
  for (n=0; n < num_components; n++)
    {
      dims.add_component(rows,cols,precision,is_signed[n],next_comp_idx);
      if ((forced_prec[n] = dims.get_forced_precision(next_comp_idx)) > 30)
        { kdu_warning w; w << "Attempting to force the precision of component "
          << next_comp_idx
          << " beyond 30 bits; we will force to 30 bits only.";
          forced_prec[n] = 30;
        }
      if (forced_prec[n] != 0)
        dims.set_bit_depth(next_comp_idx,forced_prec[n]);
      next_comp_idx++;
    }

  // See if there are any optional tags whose contents we would like to record
  // in JP2 boxes -- if possible.
  if ((tiffdir.open_tag(((kdu_uint32) 33550)<<16) != 0) ||
      (tiffdir.open_tag(((kdu_uint32) 33922)<<16) != 0) ||
      (tiffdir.open_tag(((kdu_uint32) 34264)<<16) != 0) ||
      (tiffdir.open_tag(((kdu_uint32) 34735)<<16) != 0) ||
      (tiffdir.open_tag(((kdu_uint32) 34736)<<16) != 0) ||
      (tiffdir.open_tag(((kdu_uint32) 34737)<<16) != 0))
    { // Source file contains one or more GeoTIFF tags
      kdu_uint32 tag_type;
      kdu_tiffdir geotiff;
      geotiff.init(true); // GeoJP2 tags are supposed to use little-endian
      if ((tag_type=tiffdir.open_tag(((kdu_uint32) 33550)<<16)) != 0)
        geotiff.copy_tag(tiffdir,tag_type);
      if ((tag_type=tiffdir.open_tag(((kdu_uint32) 33922)<<16)) != 0)
        geotiff.copy_tag(tiffdir,tag_type);
      if ((tag_type=tiffdir.open_tag(((kdu_uint32) 34264)<<16)) != 0)
        geotiff.copy_tag(tiffdir,tag_type);
      if ((tag_type=tiffdir.open_tag(((kdu_uint32) 34735)<<16)) != 0)
        geotiff.copy_tag(tiffdir,tag_type);
      if ((tag_type=tiffdir.open_tag(((kdu_uint32) 34736)<<16)) != 0)
        geotiff.copy_tag(tiffdir,tag_type);
      if ((tag_type=tiffdir.open_tag(((kdu_uint32) 34737)<<16)) != 0)
        geotiff.copy_tag(tiffdir,tag_type);
      geotiff.write_tag(KDU_TIFFTAG_ImageWidth16,(kdu_uint16) 1);
      geotiff.write_tag(KDU_TIFFTAG_ImageHeight16,(kdu_uint16) 1);
      geotiff.write_tag(KDU_TIFFTAG_RowsPerStrip16,(kdu_uint16) 1);
      geotiff.write_tag(KDU_TIFFTAG_SamplesPerPixel,(kdu_uint16) 1);
      geotiff.write_tag(KDU_TIFFTAG_BitsPerSample,(kdu_uint16) 8);
      geotiff.write_tag(KDU_TIFFTAG_PlanarConfig,
                        KDU_TIFF_PlanarConfig_CONTIG);
      geotiff.write_tag(KDU_TIFFTAG_PhotometricInterp,
                        KDU_TIFF_PhotometricInterp_BLACKISZERO);
      geotiff.write_tag(KDU_TIFFTAG_ResolutionUnit,
                        KDU_TIFF_ResolutionUnit_NONE);
      geotiff.write_tag(KDU_TIFFTAG_XResolution,1.0);
      geotiff.write_tag(KDU_TIFFTAG_YResolution,1.0);
      geotiff.write_tag(KDU_TIFFTAG_StripByteCounts16,(kdu_uint16) 1);
      geotiff.write_tag(KDU_TIFFTAG_StripOffsets16,(kdu_uint16) 8);

      kdu_byte geojp2_uuid[16] = {0xB1,0x4B,0xF8,0xBD,0x08,0x3D,0x4B,0x43,
                                  0xA5,0xAE,0x8C,0xD7,0xD5,0xA6,0xCE,0x03};
      jp2_output_box *geo_box = dims.add_source_metadata(jp2_uuid_4cc);
      geo_box->write(geojp2_uuid,16);
      geotiff.write_header(geo_box,9);
      geo_box->write((kdu_byte) 0); // Write the dummy image sample
      geotiff.writedir(geo_box,9);
      geotiff.close();
    }

  // Finally, see if we can read this file using Kakadu's own simple TIFF
  // engine, or if we will need to borrow the services of LIBTIFF.
  rows_at_a_time = 1;
  if (compressiontype == KDU_TIFF_Compression_NONE)
    {
      int num_chunks = chunks_per_component;
      if (planar_organization)
        num_chunks *= samplesperpixel;
      chunk_offsets = new kdu_uint32[num_chunks];
      if (!(tiffdir.read_tag(KDU_TIFFTAG_TileOffsets,
                             num_chunks,chunk_offsets) ||
            tiffdir.read_tag(KDU_TIFFTAG_StripOffsets32,
                             num_chunks,chunk_offsets) ||
            tiffdir.read_tag(KDU_TIFFTAG_StripOffsets16,
                             num_chunks,chunk_offsets)))
        { kdu_error e; e << "Error trying to read strip or tile offsets "
          "from a TIFF source file."; }
      if (tiles_across > 1)
        { // Find a good number of rows to read at a time
          rows_at_a_time = (1<<20) / (cols*sample_bytes*num_components);
          if (rows_at_a_time < 1)
            rows_at_a_time = 1;
          else if (rows_at_a_time > (int) tile_height)
            rows_at_a_time = (int) tile_height;
        }

      if ((bitspersample == 16) || (bitspersample == 32) ||
          (bitspersample == 64))
        post_unpack_littlendian = tiffdir.is_littlendian();
               // For these bit-depths, the data is stored in the TIFF file
               // using its declared byte order.
      else
        post_unpack_littlendian = tiffdir.is_native_littlendian();
               // Samples whose bit-depth is not divisible by 8 are stored as
               // bytes and unpacked using `perform_buffer_unpack' into native
               // words.  For 8-bit samples it does not matter what the order
               // is.
    }
  else
    {
#ifdef KDU_INCLUDE_TIFF
      post_unpack_littlendian = tiffdir.is_native_littlendian();
           // If we have to use `libtiff_in', its `TIFFReadScanline' function
           // automatically adjusts the byte order for 16/32 bit samples to
           // the native machine byte order.
      tiffdir.close();
      src.close();
      if ((libtiff_in = TIFFOpen(fname,"r")) == NULL)
        { kdu_error e;
          e << "Unable to open input image file, \"" << fname <<"\"."; }
      if (tiled_tiff)
        {
          rows_at_a_time = (int) tile_height;
          int chunk_bytes = chunkline_bytes * rows_at_a_time;
          chunk_buf = new kdu_byte[chunk_bytes];
        }
      else
        {
          int reported_chunkline_bytes = (int) TIFFScanlineSize(libtiff_in);
          if (reported_chunkline_bytes != chunkline_bytes)
            assert(0);
          if (planar_organization)
            rows_at_a_time = (int) tile_height;
        }
#else
      kdu_error e; e << "The simple TIFF file reader in this demo application "
        "can only read uncompressed TIFF files.  This has nothing to do with "
        "Kakadu itself, of course.  If you would like to read compressed "
        "TIFF files, however, it should be sufficient to re-compile this "
        "application with the symbol \"KDU_INCLUDE_TIFF\" defined, and link "
        "it against the public-domain LIBTIFF library.";
#endif // !KDU_INCLUDE_TIFF
    }
}

/*****************************************************************************/
/*                              tif_in::~tif_in                              */
/*****************************************************************************/

tif_in::~tif_in()
{
  if ((num_unread_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
      w << "Not all rows of image components "
        << first_comp_idx << " through "
        << first_comp_idx+num_components-1
        << " were consumed!";
    }
  if (forced_prec != NULL)
    delete[] forced_prec;
  if (chunk_offsets != NULL)
    delete[] chunk_offsets;
  if (is_signed != NULL)
    delete[] is_signed;
  if (float_minvals != NULL)
    delete[] float_minvals;
  if (float_maxvals != NULL)
    delete[] float_maxvals;
  src.close();
#ifdef KDU_INCLUDE_TIFF
  if (libtiff_in != NULL)
    TIFFClose(libtiff_in);
  if (chunk_buf != NULL)
    delete[] chunk_buf;
#endif // KDU_INCLUDE_TIFF
}

/*****************************************************************************/
/*                                tif_in::get                                */
/*****************************************************************************/

bool
  tif_in::get(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  int width = line.get_width();
  int idx = comp_idx - this->first_comp_idx;
  assert((idx >= 0) && (idx < num_components));
  x_tnum = x_tnum*num_components+idx;
  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles))
    {
      assert(width == 0);
      return true;
    }

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
    {
      assert(scan->next_x_tnum >= x_tnum);
      if (scan->next_x_tnum == x_tnum)
        break;
    }
  if (scan == NULL)
    { // Need to read one or more new image lines.
      assert(x_tnum == 0); // Must consume in very specific order.
      if (num_unread_rows == 0)
        return false;

      // Figure out which vertical tile (or strip) we are in, and how many
      // rows to read.
      int next_row_idx = rows - num_unread_rows;
      int y_tile_idx = next_row_idx / tile_height;
      int row_in_tile = next_row_idx - y_tile_idx*tile_height;
      int r, rows_to_read=((int) tile_height)-row_in_tile;
      if (rows_to_read > rows_at_a_time)
        rows_to_read = rows_at_a_time;
      if (rows_to_read > num_unread_rows)
        rows_to_read = num_unread_rows;

      // Allocate the new row buffers
      image_line_buf *first_line_to_read=NULL;
      for (r=0; r < rows_to_read; r++)
        {
          if ((scan = free_lines) == NULL)
            scan = new
              image_line_buf(cols,sample_bytes*num_components,
                             chunkline_bytes*chunks_across+8);
          free_lines = scan->next;
          if (prev == NULL)
            incomplete_lines = scan;
          else
            prev->next = scan;
          prev = scan;
          scan->next = NULL;
          if (first_line_to_read == NULL)
            first_line_to_read = scan;
          scan->accessed_samples = 0;
          scan->next_x_tnum = 0;
        }

      // Scan through all horizontally adjacent chunks, reading
      // `rows_at_a_time' from each of them and unpacking their bits.
      int plane_idx, num_planes=(planar_organization)?((int)samplesperpixel):1;
      int chunk_idx = y_tile_idx*tiles_across;
      int chunk_offset=0;
      for (plane_idx=0; plane_idx < num_planes; plane_idx++,
           chunk_idx += chunks_per_component-tiles_across)
        {
          for (int x_tile_idx=0; x_tile_idx < tiles_across; x_tile_idx++,
               chunk_idx++, chunk_offset+=pixel_gap*tile_width)
            {
              scan = first_line_to_read;
              for (r=0; r < rows_to_read; r++, scan=scan->next)
                {
                  kdu_byte *result_buf = scan->buf + chunk_offset;
                  kdu_byte *read_buf = result_buf;
                  if (need_buffer_unpack)
                    {
                      assert(float_minvals == NULL);
                      read_buf += pixel_gap*tile_width-chunkline_bytes;
                      assert(read_buf >= result_buf);
                    }
                  bool read_failure = false;
#ifdef KDU_INCLUDE_TIFF
                  if (libtiff_in != NULL)
                    {
                      if (chunk_buf == NULL)
                        read_failure =
                          (TIFFReadScanline(libtiff_in,read_buf,
                                            (kdu_uint32)(next_row_idx+r),
                                            (tsample_t) plane_idx) < 0);
                      else
                        { // Read entire chunk into `chunk_buf' and then spit
                          // it back out to the line buffers.  We only need to
                          // do this when working with LibTIFF, since that is
                          // the only tile-oriented interface it offers,
                          // unfortuntely.
                          if (r==0)
                            {
                              assert((rows_at_a_time == (int) tile_height) &&
                                     (row_in_tile == 0));
                              read_failure =
                                (TIFFReadTile(libtiff_in,chunk_buf,
                                    tile_width * ((kdu_uint32) x_tile_idx),
                                    tile_height * ((kdu_uint32) y_tile_idx),
                                    0,(tsample_t) plane_idx) < 0);
                            }
                          memcpy(read_buf,chunk_buf+r*chunkline_bytes,
                                 (size_t) chunkline_bytes);
                        }
                    }
                  else
#endif // KDU_INCLUDE_TIFF
                    { // Look up the address of the chunk and read it directly.
                      if ((r==0) && ((row_in_tile==0) || (chunks_across > 1)))
                        {
                          kdu_uint32 addr = chunk_offsets[chunk_idx];
                          addr += row_in_tile*chunkline_bytes;
                          read_failure = !src.seek((kdu_long) addr);
                        }
                      if (read_failure ||
                          (src.read(read_buf,chunkline_bytes) !=
                           chunkline_bytes))
                        read_failure = true;
                    }
                  if (read_failure)
                    { kdu_error e;
                      e << "Error reading TIFF file.  Error occurred at row "
                        << next_row_idx+r
                        << " (0 is the first one)";
                      if (tiles_across > 1)
                        e << ", in horizontal tile " << x_tile_idx
                          << " (0 is the first one)";
                      if (planar_organization)
                        e << ", in plane " << plane_idx
                          << " (0 is the first one)";
                      e << ".";
                    }
                  if (need_buffer_unpack)
                    perform_buffer_unpack(read_buf,result_buf);
                  if (remap_samples &&
                      ((plane_idx==0) || !planar_organization))
                    perform_sample_remap(result_buf);
                }
            }
        }

      // Now that we have finished reading all relevant chunks of the new
      // set of lines, we may need to expand a colour palette
      if (expand_palette)
        for (scan=first_line_to_read,
             r=0; r < rows_to_read; r++, scan=scan->next)
          perform_palette_expand(scan->buf);

      num_unread_rows -= rows_to_read;
      scan = first_line_to_read;
      assert(scan != NULL);
    }

  assert((cols-scan->accessed_samples) >= width);

  // Now write data into the `line' buffer, performing conversions as required
  kdu_byte *src = scan->buf;
  if (planar_organization)
    src += sample_bytes * (idx*cols + scan->accessed_samples);
  else
    src += sample_bytes * (idx + num_components*scan->accessed_samples);
  if (float_minvals != NULL)
    {
      kdu_sample32 *buf32 = line.get_buf32();
      if (buf32 == NULL)
        { kdu_error e; e << "Attempting to pass floating point sample "
          "values found in source TIFF file to the compressor via only "
          "a 16-bit intermediate representation.  It is not worth "
          "implementing a downconverter for this case.  Use Kakadu's 32-bit "
          "sample data representation intsead."; }
      if (line.is_absolute())
        convert_floats_to_ints(src,buf32,width,precision,is_signed[idx],
                               float_minvals[idx],float_maxvals[idx],
                               sample_bytes,post_unpack_littlendian,
                               pixel_gap);
      else
        convert_floats_to_floats(src,buf32,width,precision,is_signed[idx],
                                 float_minvals[idx],float_maxvals[idx],
                                 sample_bytes,post_unpack_littlendian,
                                 pixel_gap);
    }
  else if (line.get_buf32() != NULL)
    {
      if (line.is_absolute())
        convert_words_to_ints(src,line.get_buf32(),width,precision,
                      is_signed[idx],sample_bytes,post_unpack_littlendian,
                      pixel_gap);
      else
        convert_words_to_floats(src,line.get_buf32(),width,precision,
                      is_signed[idx],sample_bytes,post_unpack_littlendian,
                      pixel_gap);
    }
  else
    {
      if (line.is_absolute())
        convert_words_to_shorts(src,line.get_buf16(),width,precision,
                      is_signed[idx],sample_bytes,post_unpack_littlendian,
                      pixel_gap);
      else
        convert_words_to_fixpoint(src,line.get_buf16(),width,precision,
                      is_signed[idx],sample_bytes,post_unpack_littlendian,
                      pixel_gap);
    }
  if (forced_prec[idx] != 0)
    force_sample_precision(line,forced_prec[idx],precision,is_signed[idx]);
  if (invert_first_component && (idx == 0))
    invert_line(line);

  // Finally, update the line reading state before returning.
  scan->next_x_tnum++;
  if (idx == (num_components-1))
    scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == cols)
    { // Send empty line to free list.
      if (initial_non_empty_tiles == 0)
        initial_non_empty_tiles = scan->next_x_tnum;
      else
        assert(initial_non_empty_tiles == scan->next_x_tnum);
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
    }
  return true;
}

/*****************************************************************************/
/*                        tif_in::perform_buffer_unpack                      */
/*****************************************************************************/

void
  tif_in::perform_buffer_unpack(kdu_byte *src, kdu_byte *dst)
{
  if (sample_bytes == 1)
    {
      kdu_byte *dp = dst;
      assert(bitspersample < 8);
      kdu_uint16 val = ((kdu_uint16) *(src++)) << 8;
      int free_lsbs = 8; // Number of LSB's of `val' not filled from `src'
      int bits_per_sample = bitspersample;
      int downshift = (16-bits_per_sample);
      for (int n=chunkline_samples; n > 0; n--)
        {
           if (free_lsbs >= 8)
             {
               free_lsbs -= 8;
               val |= ((kdu_uint16) *(src++)) << free_lsbs;
             }
           *(dp++) = (kdu_byte)(val >> downshift);
          val <<= bits_per_sample;
          free_lsbs += bits_per_sample;
        }
    }
  else if (sample_bytes == 2)
    {
      kdu_uint16 *dp = (kdu_uint16 *) dst;
      assert((bitspersample > 8) && (bitspersample < 16));
      kdu_uint16 out_val=0, in_val;
      int n, bits_needed=bitspersample;
      for (n=chunkline_bytes; n > 0; n--, src++)
        {
          in_val = *src;
          if (bits_needed <= 8)
            {
              out_val <<= bits_needed;  bits_needed -= 8;
              *(dp++) = out_val | (in_val >> -bits_needed);
              out_val = in_val; bits_needed += bitspersample;
            }
          else
            {
              out_val = (out_val<<8) | in_val;
              bits_needed -= 8;
            }
        }
    }
  else if (sample_bytes == 4)
    {
      kdu_uint32 *dp = (kdu_uint32 *) dst;
      assert((bitspersample > 16) && (bitspersample < 32));
      kdu_uint32 out_val=0, in_val;
      int n, bits_needed=bitspersample;
      for (n=chunkline_bytes; n > 0; n--, src++)
        {
          in_val = *src;
          if (bits_needed <= 8)
            {
              out_val <<= bits_needed;  bits_needed -= 8;
              *(dp++) = out_val | (in_val >> -bits_needed);
              out_val = in_val; bits_needed += bitspersample;
            }
          else
            {
              out_val = (out_val<<8) | in_val;
              bits_needed -= 8;
            }
        }
    }
  else
    assert(0);
}

/*****************************************************************************/
/*                       tif_in::perform_sample_remap                        */
/*****************************************************************************/

void
  tif_in::perform_sample_remap(kdu_byte *buf)
{
  assert(sample_bytes == 1);
  int n, gap = (planar_organization)?1:((int) samplesperpixel);
  for (n=tile_width; n > 0; n--, buf+=gap)
    *buf = map[*buf];
}

/*****************************************************************************/
/*                       tif_in::perform_palette_expand                      */
/*****************************************************************************/

void
  tif_in::perform_palette_expand(kdu_byte *buf)
{
  int n;
  assert(sample_bytes == 1);
  assert(num_components >= 3);
  if (planar_organization)
    { // In this case we need to shuffle component segments around inside
      // the line.
      for (int p=3; p < num_components; p++)
        {
          kdu_byte *src=buf+(p-2)*cols, *dst=buf+p*cols;
          for (n=0; n < cols; n++)
            dst[n] = src[n];
        }
      kdu_byte *buf1=buf+cols, *buf2=buf1+cols;
      for (n=0; n < cols; n++)
        {
          kdu_byte *mp = map + (((int) buf[n])<<2);
          buf[n] = mp[0];  buf1[n] = mp[1];  buf2[n] = mp[2];
        }
    }
  else
    { // Interleaved components
      if (num_components == 3)
        { // Simple case
          kdu_byte *src=buf+cols-1, *dst=buf+3*cols-3;
          for (n=cols; n > 0; n--, src--, dst-=3)
            {
              kdu_byte *mp = map + (((int) src[0])<<2);
              dst[0] = mp[0];  dst[1] = mp[1];  dst[2] = mp[2];
            }
        }
      else if (num_components == 4)
        { // Simple case with alpha
          kdu_byte *src=buf+2*cols-2, *dst=buf+4*cols-4;
          for (n=cols; n > 0; n--, src-=2, dst-=4)
            {
              kdu_byte *mp = map + (((int) src[0])<<2);
              dst[3] = src[1];
              dst[0] = mp[0];  dst[1] = mp[1];  dst[2] = mp[2];
            }
        }
      else
        { // General case of palette + extra samples
          int p;
          kdu_byte *src=buf+(num_components-2)*(cols-1);
          kdu_byte *dst=buf+num_components*(cols-1);
          for (n=cols; n > 0; n--, src+=num_components-2, dst+=num_components)
            {
              for (p=3; p < num_components; p++)
                dst[p] = src[p-2];
              kdu_byte *mp = map + (((int) src[0])<<2);
              dst[0] = mp[0];  dst[1] = mp[1];  dst[2] = mp[2];
            }
        }
    }
}
