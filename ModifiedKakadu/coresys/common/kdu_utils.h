/*****************************************************************************/
// File: kdu_utils.h [scope = CORESYS/COMMON]
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
   Provides some handy in-line functions.
******************************************************************************/

#ifndef KDU_UTILS_H
#define KDU_UTILS_H

#include <assert.h>
#include "kdu_elementary.h"

/* ========================================================================= */
/*                            Convenient Inlines                             */
/* ========================================================================= */

/*****************************************************************************/
/* INLINE                           kdu_read                                 */
/*****************************************************************************/

static inline int
  kdu_read(kdu_byte * &bp, kdu_byte *end, int nbytes)
{ /* [SYNOPSIS]
       Reads an integer quantity having an `nbytes' bigendian
       representation from the array identified by `bp'.  During the process,
       `bp' is advanced `nbytes' positions.  If this pushes it up to or past
       the `end' pointer, the function throws an exception of type
       `kdu_byte *'.
       [//]
       The byte order is assumed to be big-endian.  If the local machine
       architecture is little-endian, the input bytes are reversed.
     [RETURNS]
       The value of the integer recovered from the first `nbytes' bytes of
       the buffer.
     [ARG: bp]
       Pointer to the first byte in the buffer from which the integer is
       to be recovered.
     [ARG: end]
       Points immediately beyond the last valid entry in the buffer.
     [ARG: nbytes]
       Number of bytes from the buffer which are to be converted into a
       big-endian integer.  Must be one of 1, 2, 3 or 4.
  */
  int val;

  assert(nbytes <= 4);
  if ((end-bp) < nbytes)
    throw bp;
  val = *(bp++);
  if (nbytes > 1)
    val = (val<<8) + *(bp++);
  if (nbytes > 2)
    val = (val<<8) + *(bp++);
  if (nbytes > 3)
    val = (val<<8) + *(bp++);
  return val;
}

/*****************************************************************************/
/* INLINE                       kdu_read_float                               */
/*****************************************************************************/

static inline float
  kdu_read_float(kdu_byte * &bp, kdu_byte *end)
{ /* [SYNOPSIS]
       Reads a 4-byte single-precision floating point quantity from the
       array identified by `bp'.  During the process, `bp' is advanced
       4 bytes.  If this pushes it up to or past the `end' pointer, the
       function throws an exception of type `kdu_byte *'.
       [//]
       The byte order is assumed to be big-endian.  If the local machine
       architecture is little-endian, the input bytes are reversed.
     [RETURNS]
       The value of the floating point quantity recovered from the first
       4 bytes of the buffer.
     [ARG: bp]
       Pointer to the first byte in the buffer from which the integer is
       to be recovered.
     [ARG: end]
       Points immediately beyond the last valid entry in the buffer.
  */
  if ((end-bp) < 4)
    throw bp;
  float val;
  kdu_byte *val_p = (kdu_byte *) &val;
  int n, machine_uses_big_endian = 1;
  ((kdu_byte *) &machine_uses_big_endian)[0] = 0;
  if (machine_uses_big_endian)
    for (n=0; n < 4; n++)
      val_p[n] = *(bp++);
  else
    for (n=3; n >= 0; n--)
      val_p[n] = *(bp++);
  return val;
}

/*****************************************************************************/
/* INLINE                       kdu_read_double                              */
/*****************************************************************************/

static inline double
  kdu_read_double(kdu_byte * &bp, kdu_byte *end)
{ /* [SYNOPSIS]
       Same as `kdu_read_float', but reads an 8-byte double-precision
       quantity from the `bp' array.
  */
  if ((end-bp) < 8)
    throw bp;
  double val;
  kdu_byte *val_p = (kdu_byte *) &val;
  int n, machine_uses_big_endian = 1;
  ((kdu_byte *) &machine_uses_big_endian)[0] = 0;
  if (machine_uses_big_endian)
    for (n=0; n < 8; n++)
      val_p[n] = *(bp++);
  else
    for (n=7; n >= 0; n--)
      val_p[n] = *(bp++);
  return val;
}

/*****************************************************************************/
/* INLINE                          ceil_ratio                                */
/*****************************************************************************/

static inline int
  ceil_ratio(int num, int den)
{ /* [SYNOPSIS]
       Returns the ceiling function of the ratio `num' / `den', where
       the denominator is required to be strictly positive.
     [RETURNS] Non-negative ratio.
     [ARG: num] Non-negative numerator.
     [ARG: den] Non-negative denomenator.
  */
  assert(den > 0);
  if (num <= 0)
    return -((-num)/den);
  else
    return 1+((num-1)/den);
}

/*****************************************************************************/
/* INLINE                          floor_ratio                               */
/*****************************************************************************/

static inline int
  floor_ratio(int num, int den)
{ /* [SYNOPSIS]
       Returns the floor function of the ratio `num' / `den', where
       the denominator is required to be strictly positive.
     [RETURNS] Non-negative ratio.
     [ARG: num] Non-negative numerator.
     [ARG: den] Non-negative denomenator.
  */
  assert(den > 0);
  if (num < 0)
    return -(1+((-num-1)/den));
  else
    return num/den;
}

/*****************************************************************************/
/* INLINE                       long_ceil_ratio                              */
/*****************************************************************************/

static inline int
  long_ceil_ratio(kdu_long num, kdu_long den)
{ /* [SYNOPSIS]
       Returns the ceiling function of the ratio `num' / `den', where
       the denominator is required to be strictly positive.  The result
       must fit within a signed 32-bit integer, even if the numerator or
       denominator do not.
     [RETURNS] Non-negative ratio.
     [ARG: num] Non-negative numerator.
     [ARG: den] Non-negative denomenator.
  */
  assert(den > 0);
  if (num <= 0)
    {
      num = -((-num)/den);
      assert((num >= (kdu_long) KDU_INT32_MIN));
    }
  else
    {
      num = 1+((num-1)/den);
      assert((num <= (kdu_long) KDU_INT32_MAX));
    }
  return (int) num;
}

/*****************************************************************************/
/* INLINE                       long_floor_ratio                             */
/*****************************************************************************/

static inline int
  long_floor_ratio(kdu_long num, kdu_long den)
{ /* [SYNOPSIS]
       Returns the floor function of the ratio `num' / `den', where
       the denominator is required to be strictly positive.  The result
       must fit within a signed 32-bit integer, even if the numerator or
       denominator do not.
     [RETURNS] Non-negative ratio.
     [ARG: num] Non-negative numerator.
     [ARG: den] Non-negative denomenator.
  */
  assert(den > 0);
  if (num < 0)
    {
      num = -(1+((-num-1)/den));
      assert((num >= (kdu_long) KDU_INT32_MIN));
    }
  else
    {
      num = num/den;
      assert((num <= (kdu_long) KDU_INT32_MAX));
    }
  return (int) num;
}


#endif // KDU_UTILS_H
