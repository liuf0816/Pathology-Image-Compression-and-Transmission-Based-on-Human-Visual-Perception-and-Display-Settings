/******************************************************************************/
// File: region_decompressor_local.h [scope = APPS/SUPPORT]
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
   Provides local definitions for the implementation of the
`kdu_region_decompressor' object.
*******************************************************************************/

#ifndef REGION_DECOMPRESSOR_LOCAL_H
#define REGION_DECOMPRESSOR_LOCAL_H

#include "kdu_region_decompressor.h"

// Declared here
struct kd_tile_bank;
struct kd_component;
struct kd_channel;

/******************************************************************************/
/*                                 kd_tile_bank                               */
/******************************************************************************/

struct kd_tile_bank {
  public: // Construction/destruction
    kd_tile_bank()
      {
        max_tiles=num_tiles=0; tiles=NULL; engines=NULL;
        env_queue=NULL; queue_bank_idx=0; freshly_created=false;
      }
    ~kd_tile_bank()
      {
        if (tiles != NULL) delete[] tiles;
        if (engines != NULL) delete[] engines;
      }
  public: // Data
    int max_tiles; // So that `tiles' and `engines' arrays can be reallocated
    int num_tiles; // 0 if the bank is not currently in use
    kdu_coords first_tile_idx; // Absolute index of first tile in bank
    kdu_dims dims; // Region represented by the bank, on the codestream canvas
    kdu_tile *tiles; // Array of `max_tiles' tile interfaces
    kdu_multi_synthesis *engines; // Array of `max_tiles' synthesis engines
    kdu_thread_queue *env_queue; // Queue for these tiles, if multi-threading
    kdu_long queue_bank_idx; // Index supplied to `kdu_thread_env::add_queue'
    bool freshly_created; // True only when the bank has just been created by
      // `kdu_region_decompressor::start_tile_bank' and has not yet been used
      // to decompress or render any data.
  };

/******************************************************************************/
/*                                 kd_component                               */
/******************************************************************************/

struct kd_component {
    int rel_comp_idx; // Index to be used after `apply_input_restrictions'
    int bit_depth;
    bool is_signed;
    int palette_bits; // See below
    int num_line_users; // Num channels using the component's `line' buffer
    bool using_shorts; // Tile-bank dependent; see notes below.
    bool can_use_directly; // If `line' is 16-bit fixed-point && no expansion
    int needed_line_samples; // Used for state information in `process_generic'
    int new_line_samples; // Number of newly decompressed samples in `line'
    kdu_dims dims; // Remainder of current tile-bank region; see notes below.
    kdu_line_buf *line; // Various interpretations; see below
    kdu_line_buf line_store; // Tile-bank dependent; see notes below
    kdu_line_buf indices; // See notes below
    kdu_coords expansion_numerator; // See notes below
    kdu_coords expansion_denominator; // See notes below
    kdu_coords expansion_counter; // Tile-bank dependent; see notes below
    kdu_coords registration_offset; // Recovered from the codestream, expressed
                                    // relative to `expansion_denominator'.
  };
  /* Notes:
        Most members of this structure are filled out by the call to
     `kdu_region_decompressor::start', after which they remain unaffected
     by calls to the `kdu_region_decompressor::process' function.  The dynamic
     members are as follows:
            >> `using_shorts', `can_use_directly', `valid_data',
               `new_line_samples', `line', `line_store', `dims' and
               `expansion_counter'.
        The `dims', `expansion_numerator', `expansion_denominator' and `offset'
     members together identify the size, location and interpolation properties
     of the current tile bank's region within this image component.
     Specifically:
            >> The `dims' member holds the location and dimensions of
               the component region on the code-stream canvas, after
               possible geometric transformation and/or discarding of
               resolution levels.  This region is restricted to the
               current tile bank (see notes at the end of the declaration
               of `kdu_region_decompressor' for more on tile banks).
               `dims.size.y' is decremented and `dims.pos.y' is incremented
               whenever a new line of the tile bank is generated in this
               component.
            >> The `expansion_numerator' and `expansion_denominator' members
               identify the amount by which the original dimensions in `dims'
               are to be expanded.  The expansion factor (ratio of numerator
               to denominator) is guaranteed to be >= 1 in both the horizontal
               and vertical directions.  These quantities are obtained by
               scaling the values supplied to `kdu_region_decompressor::start'
               by the ratio between this image component's sub-sampling factors
               and those of the reference image component.  They do not vary
               with calls to the `kdu_region_decompressor::process' function.
            >> The `expansion_counter' member maintains the ratio counter used
               for expansion.  When rendering a new line of samples,
               `expansion_counter.x' becomes the initial state of the
               horizontal ratio counter, which is decremented by
               `expansion_denominator.x' each time a new output sample is
               generated.  If the ratio counter becomes <= 0, the source
               sample is advanced by 1 and the ratio counter is incremented by
               `expansion_numerator.x'.  Similarly, in the vertical direction,
               each time a new line of interpolated output samples is
               generated, `expansion_counter.y' is decremented by
               `expansion_denominator.y'.  If `expansion_counter.y' becomes
               <= 0, a new line of source samples must be decompressed and
               `expansion_counter.y' is incremented by `expansion_numerator.y'.
               The initial value written to `expansion_counter' reflects the
               location of the source sample at codestream canvas location
               `dims.pos'.  This value also takes into account any component
               registration offsets recorded in the codestream's CRG marker
               segment.
        If `palette_bits' is non-zero, the `indices' buffer will be non-empty
     (its `exists' member will return true) and the code-stream sample values
     will be converted to palette indices immediately after (or during)
     decompression.
        The `line' member provides access to the sample values which directly
     represent a single line of the component, within the current tile bank.
     If `num_line_users' is 0, all channels which use this component do so
     via the palette indices stored in its `indices' buffer.  In this case,
     tile-component samples are converted directly to palette indices on the
     fly and the `line' member will be NULL.  If `num_line_users' is non-zero
     and there are multiple tiles in the current tile bank, `line' points
     to the internal `line_store' buffer and individual tile-component lines
     are transferred (possibly with numerical conversion) as they are
     decompressed.  If `num_line_users' is non-zero and there is only one
     tile in the current tile bank, `line' directly references the buffer
     returned by the single tile engine's `kdu_multi_synthesis::get_line'
     function; in this case, `line_store' need not be allocated, except in
     the case where the tile-component has zero height -- in that case,
     `line_store' is used to hold an all-zero line, since the
     `kdu_multi_synthesis::get_line' function will not return a buffer.
        The `using_shorts' and `can_use_directly' members refer explicitly to
     the buffer referenced by `line' (whenever it is non-NULL).  If
     `using_shorts' is true, this line always uses a 16-bit representation.
     Where there are multiple tiles in the current tile bank, the value of
     `using_shorts' is determined so as to minimize computational and
     memory access effort during conversions from decompressed tile data
     to the final rendered channel data, while respecting data accuracy
     requirements supplied by the application.  The `can_use_directly'
     member is set to true if the `line' buffer can be used directly
     as a channel buffer.
 */

/******************************************************************************/
/*                                  kd_channel                                */
/******************************************************************************/

struct kd_channel {
    kd_component *source; // Source component for this channel.
    kdu_line_buf lines[3]; // Up to 3 interpolated and mapped image lines.
    kdu_line_buf *ref_line1; // See below
    kdu_line_buf *ref_line2; // See below
    kdu_line_buf *cur_line;  // See below
    kdu_sample16 *lut; // Palette mapping LUT.  NULL if no palette.
    int native_precision; // Used if `kdu_region_decompressor::convert'
    bool native_signed;   // supplies a `precision_bits' argument of 0.
    bool using_shorts; // Indicates whether `lines' have a short representation
    kdu_uint16 stretch_residual; // See below
  };
  /* Notes:
        If `lut' is non-NULL, or colour conversion is required, the `line'
     buffer is guaranteed to hold 16-bit fixed point quantities.  When a
     palette is involved, these are obtained through the palette mapping.
     Otherwise, the values are obtained from the code-stream image component
     samples.  If no conversions are required, the `line' buffer holds
     either a 16-bit fixed point representation, or a 32-bit fixed point
     representation.  The 32-bit fixed point representation has the same
     interpretation as the 16-bit version, but with KDU_FIX_POINT+16
     fraction bits, rather than KDU_FIX_POINT fraction bits.  Note that
     this is different from either of the 32-bit representations nominally
     managed by a `kdu_line_buf' structure.
        The `lines' array contains three distinct line buffers, which may
     be referenced by the `ref_line1', `ref_line2' and `cur_line' pointers.
     `ref_line2' points to the most recently generated (horizontally
     interpolated and/or mapped) line of image data for the channel, while
     `ref_line1' points to the one generated prior to that one.
     If the expansion factor is 1 or there is no colour transform,
     `cur_line' simply points to `ref_line2'.  Otherwise, `cur_line' points
     to a different line buffer, into which `ref_line1'=`ref_line2' is
     copied prior to colour transformation.  If the vertical expansion
     factor is rational, bilinear interpolation is performed between
     `ref_line1' and `ref_line2', to produce `cur_line'.
        The `stretch_residual' member is used to implement the white stretching
     policy described with the `kdu_region_decompressor::set_white_stretch'
     function.  Any required stretching is applied immediately after component
     samples are transferred to the channel.  If the source `bit_depth', P, is
     >= the `kdu_region_decompressor::white_stretch_precision' value, B, the
     value of `stretch_residual' will be 0.  Otherwise, `stretch_residual' is
     set to floor(2^{16} * ((1-2^{-B})/(1-2^{-P}) - 1)), which necessarily
     lies in the range 0 to 0xFFFF.  The white stretching policy may then
     be implemented by adding (x*`stretch_residual') / 2^16
     to each sample, x, after converting to an unsigned representation.
     In practice, we perform the conversions on signed quantities by
     introducing appropriate offsets. */

#endif // REGION_DECOMPRESSOR_LOCAL_H
