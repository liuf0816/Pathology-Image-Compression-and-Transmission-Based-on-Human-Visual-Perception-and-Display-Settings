/*****************************************************************************/
// File: jpx.h [scope = APPS/COMPRESSED-IO]
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
   Builds upon the classes defined in "jp2.h" to provide a complete set
of services for working with the JPX file format.
******************************************************************************/

#ifndef JPX_H
#define JPX_H
#include "jp2.h"

// Classes defined here
class jpx_fragment_list;
class jpx_input_box;
class jpx_compatibility;
class jpx_composition;
class jpx_frame_expander;
class jpx_codestream_source;
class jpx_codestream_target;
class jpx_layer_source;
class jpx_layer_target;
struct jpx_roi;
class jpx_metanode;
class jpx_meta_manager;
class jpx_source;
class jpx_target;

// Classes defined elsewhere
struct jx_frame;
struct jx_metanode;
struct jx_meta_manager;
class jx_fragment_list;
class jx_compatibility;
class jx_composition;
class jx_codestream_source;
class jx_codestream_target;
class jx_layer_source;
class jx_layer_target;
class jx_source;
class jx_target;


/* ========================================================================= */
/*                                 Macros                                    */
/* ========================================================================= */

/*****************************************************************************/
/*                 Standard Reader Requirements Box Features                 */
/*****************************************************************************/

#define JPX_SF_CODESTREAM_NO_EXTENSIONS                    ((kdu_uint16) 1)
#define JPX_SF_MULTIPLE_LAYERS                             ((kdu_uint16) 2)
#define JPX_SF_JPEG2000_PART1_PROFILE0                     ((kdu_uint16) 3)
#define JPX_SF_JPEG2000_PART1_PROFILE1                     ((kdu_uint16) 4)
#define JPX_SF_JPEG2000_PART1                              ((kdu_uint16) 5)
#define JPX_SF_JPEG2000_PART2                              ((kdu_uint16) 6)
#define JPX_SF_CODESTREAM_USING_DCT                        ((kdu_uint16) 7)
#define JPX_SF_NO_OPACITY                                  ((kdu_uint16) 8)
#define JPX_SF_OPACITY_NOT_PREMULTIPLIED                   ((kdu_uint16) 9)
#define JPX_SF_OPACITY_PREMULTIPLIED                       ((kdu_uint16) 10)
#define JPX_SF_OPACITY_BY_CHROMA_KEY                       ((kdu_uint16) 11)
#define JPX_SF_CODESTREAM_CONTIGUOUS                       ((kdu_uint16) 12)
#define JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL_AND_ORDERED   ((kdu_uint16) 13)
#define JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL               ((kdu_uint16) 14)
#define JPX_SF_CODESTREAM_FRAGMENTS_LOCAL                  ((kdu_uint16) 15)
#define JPX_SF_CODESTREAM_FRAGMENTS_REMOTE                 ((kdu_uint16) 16)
#define JPX_SF_COMPOSITING_USED                            ((kdu_uint16) 17)
#define JPX_SF_COMPOSITING_NOT_REQUIRED                    ((kdu_uint16) 18)
#define JPX_SF_MULTIPLE_LAYERS_NO_COMPOSITING_OR_ANIMATION ((kdu_uint16) 19)
#define JPX_SF_ONE_CODESTREAM_PER_LAYER                    ((kdu_uint16) 20)
#define JPX_SF_MULTIPLE_CODESTREAMS_PER_LAYER              ((kdu_uint16) 21)
#define JPX_SF_SINGLE_COLOUR_SPACE                         ((kdu_uint16) 22)
#define JPX_SF_MULTIPLE_COLOUR_SPACES                      ((kdu_uint16) 23)
#define JPX_SF_NO_ANIMATION                                ((kdu_uint16) 24)
#define JPX_SF_ANIMATED_COVERED_BY_FIRST_LAYER             ((kdu_uint16) 25)
#define JPX_SF_ANIMATED_NOT_COVERED_BY_FIRST_LAYER         ((kdu_uint16) 26)
#define JPX_SF_ANIMATED_LAYERS_NOT_REUSED                  ((kdu_uint16) 27)
#define JPX_SF_ANIMATED_LAYERS_REUSED                      ((kdu_uint16) 28)
#define JPX_SF_ANIMATED_PERSISTENT_FRAMES                  ((kdu_uint16) 29)
#define JPX_SF_ANIMATED_NON_PERSISTENT_FRAMES              ((kdu_uint16) 30)
#define JPX_SF_NO_SCALING                                  ((kdu_uint16) 31)
#define JPX_SF_SCALING_WITHIN_LAYER                        ((kdu_uint16) 32)
#define JPX_SF_SCALING_BETWEEN_LAYERS                      ((kdu_uint16) 33)
#define JPX_SF_ROI_METADATA                                ((kdu_uint16) 34)
#define JPX_SF_IPR_METADATA                                ((kdu_uint16) 35)
#define JPX_SF_CONTENT_METADATA                            ((kdu_uint16) 36)
#define JPX_SF_HISTORY_METADATA                            ((kdu_uint16) 37)
#define JPX_SF_CREATION_METADATA                           ((kdu_uint16) 38)
#define JPX_SF_DIGITALLY_SIGNED                            ((kdu_uint16) 39)
#define JPX_SF_CHECKSUMMED                                 ((kdu_uint16) 40)
#define JPX_SF_DESIRED_REPRODUCTION                        ((kdu_uint16) 41)
#define JPX_SF_PALETTIZED_COLOUR                           ((kdu_uint16) 42)
#define JPX_SF_RESTRICTED_ICC                              ((kdu_uint16) 43)
#define JPX_SF_ANY_ICC                                     ((kdu_uint16) 44)
#define JPX_SF_sRGB                                        ((kdu_uint16) 45)
#define JPX_SF_sLUM                                        ((kdu_uint16) 46)
#define JPX_SF_sYCC                                        ((kdu_uint16) 70)
#define JPX_SF_BILEVEL1                                    ((kdu_uint16) 47)
#define JPX_SF_BILEVEL2                                    ((kdu_uint16) 48)
#define JPX_SF_YCbCr1                                      ((kdu_uint16) 49)
#define JPX_SF_YCbCr2                                      ((kdu_uint16) 50)
#define JPX_SF_YCbCr3                                      ((kdu_uint16) 51)
#define JPX_SF_PhotoYCC                                    ((kdu_uint16) 52)
#define JPX_SF_YCCK                                        ((kdu_uint16) 53)
#define JPX_SF_CMY                                         ((kdu_uint16) 54)
#define JPX_SF_CMYK                                        ((kdu_uint16) 55)
#define JPX_SF_LAB_DEFAULT                                 ((kdu_uint16) 56)
#define JPX_SF_LAB                                         ((kdu_uint16) 57)
#define JPX_SF_JAB_DEFAULT                                 ((kdu_uint16) 58)
#define JPX_SF_JAB                                         ((kdu_uint16) 59)
#define JPX_SF_esRGB                                       ((kdu_uint16) 60)
#define JPX_SF_ROMMRGB                                     ((kdu_uint16) 61)
#define JPX_SF_SAMPLES_NOT_SQUARE                          ((kdu_uint16) 62)
#define JPX_SF_LAYERS_HAVE_LABELS                          ((kdu_uint16) 63)
#define JPX_SF_CODESTREAMS_HAVE_LABELS                     ((kdu_uint16) 64)
#define JPX_SF_MULTIPLE_COLOUR_SPACES2                     ((kdu_uint16) 65)
#define JPX_SF_LAYERS_HAVE_DIFFERENT_METADATA              ((kdu_uint16) 66)



/* ========================================================================= */
/*                                 Classes                                   */
/* ========================================================================= */

/*****************************************************************************/
/*                             jpx_fragment_list                             */
/*****************************************************************************/

class jpx_fragment_list {
  /* [BIND: interface]
     [SYNOPSIS]
       Manages a list of byte ranges into (usually) external files/URL's.
       This information is either derived from a parsed fragment table box
       (ftbl) or will be written into one, depending on whether it is
       owned by a `jpx_codestream_source' or `jpx_codestream_target' object.
       [//]
       Objects of this class are merely interfaces to an internal
       object, which cannot be directly created by an application.
       Creation and destruction of the internal object, as well as saving
       and reading of the associated fragment list (flst) box are
       capabilities reserved for the internal machinery associated with
       the file format manager which provides the interface.
       [//]
       Objects which can provide a non-empty `jpx_fragment_list' interface
       include `jpx_codestream_source::access_fragment_list' and
       `jpx_target::access_fragment_list'.
  */
  // --------------------------------------------------------------------------
  public: // Lifecycle member functions
    jpx_fragment_list() { state = NULL; }
    jpx_fragment_list(jx_fragment_list *state) { this->state = state; }
      /* [SYNOPSIS]
           Do not call this function yourself.  See the `jpx_fragment_list'
           overview for more on how to get access to a non-empty interface.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns false if the interface is empty, meaning that it is not
           currently associated with any internal implementation.  See the
           `jpx_fragment_list' overview for more on how to get access to a
           non-empty interface.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS] Opposite of `exists', returning true if the interface is
         empty.
      */
  // --------------------------------------------------------------------------
  public: // Initialization member functions
    KDU_AUX_EXPORT void
      add_fragment(int url_idx, kdu_long offset, kdu_long length);
      /* [SYNOPSIS]
           Add a new fragment to the end of the list.  The `url_idx' may be
           0 if the fragment refers to a location in the master file which
           contains the fragment list.  Otherwise is must be positive.  It
           must be one of the valid indices provided by an associated
           `jp2_data_references' object.
           [//]
           Actual fragment list boxes can only record fragments whose length
           fits into a 32-bit unsigned word; however, this function allows you
           to add longer fragments.  When a longer fragment is written out
           to a fragment list box, multiple fragments may need to be written.
           Similarly, multiple contiguous fragments encountered when
           parsing a fragment list box may be collapsed into a single longer
           fragment.  These features are provided to overcome the lack
           of foresight in the design of the fragment list box.  As a
           result, however, the fragments managed by this object might not
           be in one-to-one correspondence with those actually written to
           or read from the underlying fragment list box.
      */
  // --------------------------------------------------------------------------
  public: // Access member functions.
    KDU_AUX_EXPORT kdu_long get_total_length();
      /* [SYNOPSIS]
           Returns the sum of the lengths of all fragments.
      */
    KDU_AUX_EXPORT int get_num_fragments();
      /* [SYNOPSIS]
           Returns the total number of fragments which can be retrieved using
           `get_fragment'.  Note that this may be smaller than the number of
           fragments recorded in a parsed fragment list box (flst) since
           contiguous fragments are automatically merged.  The value may also
           be smaller than the number of fragments added using `add_fragment',
           sicne again, contiguous fragments are automatically merged.
      */
    KDU_AUX_EXPORT bool
      get_fragment(int frag_idx, int &url_idx, kdu_long &offset,
                   kdu_long &length);
      /* [SYNOPSIS]
           Use this function to retrieve the fragments one by one.  The
           function returns false if `frag_idx' lies outside the range 0 to
           N-1 where N is the value returned by `get_num_fragments'.  The
           `url_idx' will be 0 if the fragment refers to the master file
           which contains the fragment list box.  In general, `url_idx' is
           the index which may be supplied to the associated data references
           object's `jp2_data_references::get_url' function to recover the
           details of the URL which is being referenced.
      */
    KDU_AUX_EXPORT int
      locate_fragment(kdu_long pos, kdu_long &bytes_into_fragment);
      /* [SYNOPSIS]
           Finds the fragment containing the location `pos' bytes from
           the start of the stream represented by the concatenated fragments.
           Returns -1 if `pos' does not lie in the range 0 to L-1, where
           L is the value returned by `get_total_length'.  Otherwise, the
           returned index may be supplied to `get_fragment'.
         [ARG: bytes_into_fragment]
           Used to return the number of bytes preceding `pos' which lie
           within the fragment identified by the returned index.
      */
  // --------------------------------------------------------------------------
  private: // Data
    jx_fragment_list *state;
  };

/*****************************************************************************/
/*                              jpx_input_box                                */
/*****************************************************************************/

class jpx_input_box : public jp2_input_box {
  /* [BIND: reference]
     [SYNOPSIS]
       Extends the functionality offered by `jp2_input_box' by adding the
       capability to read fragmented boxes based on their fragment list,
       as if the box had never been fragmented.  Specifically, a new
       version of the `open' function is provided which accepts
       `jpx_fragment_list' and `jp2_data_references' objects as its inputs,
       along with an arbitrary box type.  When this form of the `open'
       function is used, the `jp2_input_box' functions behave as if a
       contiguous box of the supplied type had been opened.  This
       functionality is key to the transparent implementation of
       fragment table boxes (fragmented, reused or externally stored
       codestreams) and cross reference boxes (fragmented, reused or
       externally stored boxes of other types).
       [//]
       As with `jp2_input_box', it is not safe to assign one `jpx_input_box'
       object directly to another.
  */
  // --------------------------------------------------------------------------
  public: // Lifecycle functions
    KDU_AUX_EXPORT jpx_input_box();
      /* [SYNOPSIS]
           After construction, you must call one of the `jp2_input_box::open'
           or `jpx_input_box::open' functions.
      */
    virtual ~jpx_input_box() { close(); }
      /* [SYNOPSIS]
           Closes the box if it is open.  It is safe to close boxes out
           of order, or even after their underlying `jp2_family_src' object
           has disappeared; however, sub-boxes should not be closed after the
           super-box used to open them has been deleted from memory.
      */
    jpx_input_box &operator=(jpx_input_box &rhs)
      { assert(0); return *this; }
      /* [SYNOPSIS]
           This assignment operator serves to prevent actual copying of one
           `jpx_input_box' object to another.  In debug mode, it raises an
           assertion.
      */
    bool exists() { return is_open; }
      /* [SYNOPSIS]
           Returns true if the box has been opened, but not yet closed.
      */
    bool operator!() { return !is_open; }
      /* [SYNOPSIS]
           Opposite of `exists'.
      */
    KDU_AUX_EXPORT virtual bool
      open_as(jpx_fragment_list frag_list, jp2_data_references data_refs,
              jp2_family_src *ultimate_src, kdu_uint32 box_type);
      /* [SYNOPSIS]
           This function provides an alternative way to open the underlying
           `jp2_input_box' so as to read the byte ranges referenced by a
           fragment list, as though this information were contained in a
           single contiguous box.  The supplied `frag_list' object provides a
           list of byte ranges into one or more files (or URL's), identified
           by the `data_refs' object.  These together represent the contents
           of a virtual box, whose box type may be arbitrarily supplied
           using `box_type'.  Fragment lists may be found within fragment
           table boxes, in which case they refer to codestreams and
           `box_type' is best set to `jp2_codestream_4cc', since the
           present function allows the codestream to be read as though it
           were a contiguous codestream.  Fragment lists may also be found
           within cross reference boxes, in which case the box type should
           be set to that of the cross-referenced box.  In any case, the
           choice of box type has no impact on how the box will be read,
           provided only that it must not be 0.
         [RETURNS]
           The function currently always returns true.
         [ARG: ultimate_src]
           This argument must not be NULL.  It must point to the
           `jp2_family_src' object from which the fragment list information
           was read.  This is important, because the fragment list has
           a special way of referring back to this source.  If the ultimate
           data source for this object is a dynamic cache, rather than a
           file or `kdu_compressed_source' object, the `open_as' function
           will succeed, but all calls to `read' will fail to actually read
           any data.  This could change in the future, but for the moment
           it is a reasonable policy, since caching data sources are typically
           used by JPIP client-server applications and JPIP provides a
           more efficient and architecture neutral mechanism to completely
           bypass all fragment lists.
         [ARG: data_refs]
           This interface is allowed to be empty.  In this case, however,
           any call to `read' which requires access to a fragment with
           non-zero url index will fail to read into that fragment.
      */
    KDU_AUX_EXPORT virtual bool open_next();
      /* [SYNOPSIS]
           Overrides `jp2_input_box::open_next' to handle the case in which
           the box was most recently opened to read from a fragment list
           (see `open_as').
      */
    KDU_AUX_EXPORT virtual bool close();
      /* [SYNOPSIS]
           Implements `kdu_compressed_source::close'.
           It is safe to call this function at any time, even if the box
           is not currently open or if its super-box has somehow been
           closed or the underlying `jp2_family_src' object has ceased to
           exist (usually an error condition, but still safe).  However,
           a super-box's members may be accessed by this function, so you
           should make sure that you do not destroy the super-box used to
           open a sub-box, until after the sub-box has at least closed.
         [RETURNS]
           True if the contents of the box were completely consumed by calls
           to `read', or by opening and closing of sub-boxes.
      */
  // --------------------------------------------------------------------------
  public: // Read functions
    KDU_AUX_EXPORT virtual bool seek(kdu_long offset);
      /* [SNOPSIS]
           Overrides `jp2_input_box::seek' so as to correctly implement
           seeking for the case in which the box was opened to read from
           the byte ranges referenced by a fragment list (see `open_as').
      */
    KDU_AUX_EXPORT virtual int read(kdu_byte *buf, int num_bytes);
      /* [SYNOPSIS]
           Overrides `jp2_input_box::read' so as to correctly handle
           reading based on fragment lists, for the case in which the
           box was opened using `open_as' rather than
           `jp2_input_box::open'.
      */
   // -------------------------------------------------------------------------
   protected: // Helper functions
     FILE *url_fopen(const char *url);
       /* Same as fopen(url,"rb") if `url' is an absolute path name.
          Otherwise, uses the information in `flst_src' to derive a base
          path name so as to open relative URL's correctly. */
   protected: // Data
     jpx_fragment_list frag_list;         // Empty interfaces if not open for
     jp2_data_references data_references; // fragmented reading
     int frag_idx; // Index of fragment currently being read (-1 if none open)
     kdu_long frag_start; // Start of fragment w.r.t. virtual box contents
     kdu_long frag_lim; // Location immediately beyond fragment
     kdu_long url_pos; // Location of `pos' relative to URL
     FILE *frag_file; // If NULL, reading from `flst_src'
     int last_url_idx; // Index of most recent URL (identifies `frag_file')
     kdu_long last_url_pos; // Most recent read position within URL
     jp2_family_src *flst_src; // Ultimate container of the fragment list box
                       // NULL if interface is not open for fragmented reading
     int max_path_len; // Length of `path_buf'
     char *path_buf; // buffer for storing temporary path information
  };

/*****************************************************************************/
/*                            jpx_compatibility                              */
/*****************************************************************************/

class jpx_compatibility {
  /* [BIND: interface]
     [SYNOPSIS]
       Manages compatibility information found in the file-type and
       reader requirements boxes of a JPX file, or the file-type box of a
       JP2 file.  Applications which are writing JPX files may wish to
       perform some explicit initialization using the interface functions
       offered by this object.
       [//]
       Objects of this class are merely interfaces to an internal
       object, which cannot be directly created by an application.
       Use `jpx_source::access_compatibility' or
       `jpx_target::access_compatibility' to obtain a non-empty interface.
  */
  // --------------------------------------------------------------------------
  public: // Construction/existence functions
    jpx_compatibility() { state = NULL; }
    jpx_compatibility(jx_compatibility *state) { this->state = state; }
      /* [SYNOPSIS]
           Do not call this function yourself.  Use
           `jpx_source::access_compatibility' or
           `jpx_target::access_compatibility' to obtain a non-empty interface.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns false if the interface is empty, meaning that it is not
           currently associated with any internal implementation.  See the
           `jpx_compatibility' overview for more on how to get access to a
           non-empty interface.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS] Opposite of `exists', returning true if the interface is
         empty.
      */
  // --------------------------------------------------------------------------
  public: // Functions used by readers
    KDU_AUX_EXPORT bool is_jp2();
      /* [SYNOPSIS]
           Returns true if the brand of the data source is JP2, not JPX.
      */
    KDU_AUX_EXPORT bool is_jp2_compatible();
      /* [SYNOPSIS]
           Returns true if the data source's compatibility list includes
           JP2.  It may be a JP2 file or a JPX file which is compatible with
           JP2.
      */
    KDU_AUX_EXPORT bool is_jpxb_compatible();
      /* [SYNOPSIS]
           Returns true if the data source is compatible with the baseline
           JPX specification, meaning that it includes "jpxb" in its
           compatibility list.  Amongst other things, this means that the
           first compositing layer uses only one codestream, with the
           following properties:
           [>>] The codestream is a JPEG2000 code-stream.
           [>>] The codestream can be decoded using only JP2000 Part 1
                capabilities, possibly in addition to certain multi-component
                decorrelation extensions from Part 2.
           [>>] The codestream is fully contained within the JPX file.
           [//]
           JP2 files are also considered to be JPX baseline compatible.
      */
    KDU_AUX_EXPORT bool has_reader_requirements_box();
      /* [SYNOPSIS]
           Returns true if the file contains a reader-requirements box or
           a reader requirements box will be written.  Strictly speaking,
           every JPX or JPX-compatible file must contains a reader
           requirements box, but early JPX files were sometimes written
           by other implementors without the reader requirements box.  For
           this reason, we adopt a more lenient position here.  If no
           reader requirements box is available, the `check_standard_feature',
           `check_vendor_feature' and other related functions will all
           return false.
      */
    KDU_AUX_EXPORT bool check_standard_feature(kdu_uint16 feature_id);
      /* [SYNOPSIS]
           Returns true if a feature with the indicated identifier appears
           in the reader requirements box.
         [ARG: feature_id]
           The following standard feature ID's are defined by JPX.  Asterisks
           (*) are used to identify features which are automatically detected
           for the purpose of writing a reader requirements box when
           generating a JPX file (see `set_used_standard_feature' for more
           on this).
           [>>] * `JPX_SF_CODESTREAM_NO_EXTENSIONS' = 1 -- the way this
                  feature is expressed in the JPX standard leaves it unclear
                  whether this means that the file contains at least one
                  codestream without extensions or that no codestreams in the
                  file have extensions; nor is the meaning of extensions
                  clarified.  For the purpose of writing JPX files, Kakadu
                  interprets it as meaning that no codestreams use any
                  PART2 extensions, but for the purpose of reading, Kakadu
                  does not expect it to mean anything in particular.  This
                  is probably the safest option.
           [>>] * `JPX_SF_MULTIPLE_LAYERS' = 2
           [>>] * `JPX_SF_JPEG2000_PART1_PROFILE0' = 3
           [>>] * `JPX_SF_JPEG2000_PART1_PROFILE1' = 4
           [>>] * `JPX_SF_JPEG2000_PART1' = 5
           [>>] * `JPX_SF_JPEG2000_PART2' = 6
           [>>] * `JPX_SF_NO_OPACITY' = 8 -- for the purpose of writing a
                  JPX file, Kakadu sets this feature if no opacity information
                  is used by any compositing layer, but it is not clear that
                  a generic reader can interpret its presence as meaning that.
           [>>] * `JPX_SF_OPACITY_NOT_PREMULTIPLIED' = 9
           [>>] * `JPX_SF_OPACITY_PREMULTIPLIED' = 10
           [>>] * `JPX_SF_OPACITY_BY_CHROMA_KEY' = 11
           [>>] * `JPX_SF_CODESTREAM_CONTIGUOUS' = 12
           [>>] * `JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL_AND_ORDERED' = 13
           [>>] * `JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL' = 14
           [>>] * `JPX_SF_CODESTREAM_FRAGMENTS_LOCAL' = 15
           [>>] * `JPX_SF_CODESTREAM_FRAGMENTS_REMOTE' = 16
           [>>] * `JPX_SF_COMPOSITING_USED' = 17
           [>>] * `JPX_SF_COMPOSITING_NOT_REQUIRED' = 18
           [>>] * `JPX_SF_MULTIPLE_LAYERS_NO_COMPOSITING_OR_ANIMATION' = 19
           [>>] * `JPX_SF_ONE_CODESTREAM_PER_LAYER' = 20
           [>>] * `JPX_SF_MULTIPLE_CODESTREAMS_PER_LAYER' = 21
           [>>]   `JPX_SF_SINGLE_COLOUR_SPACE' = 22
           [>>]   `JPX_SF_MULTIPLE_COLOUR_SPACES' = 23
           [>>] * `JPX_SF_NO_ANIMATION' = 24
           [>>] * `JPX_SF_ANIMATED_COVERED_BY_FIRST_LAYER' = 25
           [>>] * `JPX_SF_ANIMATED_NOT_COVERED_BY_FIRST_LAYER' = 26
           [>>] * `JPX_SF_ANIMATED_LAYERS_NOT_REUSED' = 27
           [>>] * `JPX_SF_ANIMATED_LAYERS_REUSED' = 28
           [>>] * `JPX_SF_ANIMATED_PERSISTENT_FRAMES' = 29
           [>>] * `JPX_SF_ANIMATED_NON_PERSISTENT_FRAMES' = 30
           [>>] * `JPX_SF_NO_SCALING' = 31
           [>>] * `JPX_SF_SCALING_WITHIN_LAYER' = 32
           [>>] * `JPX_SF_SCALING_BETWEEN_LAYERS' = 33
           [>>]   `JPX_SF_ROI_METADATA' = 34
           [>>]   `JPX_SF_IPR_METADATA' = 35
           [>>]   `JPX_SF_CONTENT_METADATA' = 36
           [>>]   `JPX_SF_HISTORY_METADATA' = 37
           [>>]   `JPX_SF_CREATION_METADATA' = 38
           [>>]   `JPX_SF_DIGITALLY_SIGNED' = 39
           [>>]   `JPX_SF_CHECKSUMMED' = 40
           [>>]   `JPX_SF_DESIRED_REPRODUCTION' = 41
           [>>] * `JPX_SF_PALETTIZED_COLOUR' = 42
           [>>] * `JPX_SF_RESTRICTED_ICC' = 43
           [>>] * `JPX_SF_ANY_ICC' = 44
           [>>] * `JPX_SF_sRGB' = 45
           [>>] * `JPX_SF_sLUM' = 46
           [>>] * `JPX_SF_BILEVEL1' = 47
           [>>] * `JPX_SF_BILEVEL2' = 48
           [>>] * `JPX_SF_YCbCr1' = 49
           [>>] * `JPX_SF_YCbCr2' = 50
           [>>] * `JPX_SF_YCbCr3' = 51
           [>>] * `JPX_SF_PhotoYCC' = 52
           [>>] * `JPX_SF_YCCK' = 53
           [>>] * `JPX_SF_CMY' = 54
           [>>] * `JPX_SF_CMYK' = 55
           [>>] * `JPX_SF_LAB_DEFAULT' = 56
           [>>] * `JPX_SF_LAB' = 57
           [>>] * `JPX_SF_JAB_DEFAULT' = 58
           [>>] * `JPX_SF_JAB' = 59
           [>>] * `JPX_SF_esRGB' = 60
           [>>] * `JPX_SF_ROMMRGB' = 61
           [>>] * `JPX_SF_SAMPLES_NOT_SQUARE' = 62
           [>>]   `JPX_SF_LAYERS_HAVE_LABELS' = 63
           [>>]   `JPX_SF_CODESTREAMS_HAVE_LABELS' = 64
           [>>]   `JPX_SF_MULTIPLE_COLOUR_SPACES2' = 65 -- appears to have
                  exactly the same meaning as standard feature code 23; the
                  standard feature list in the standard might not have been
                  carefully checked.
           [>>]   `JPX_SF_LAYERS_HAVE_DIFFERENT_METADATA' = 66
      */
    KDU_AUX_EXPORT bool check_vendor_feature(kdu_byte uuid[]);
      /* [SYNOPSIS]
           Returns true if a vendor feature with the indicated UUID
           appears in the reader requirements box.
         [ARG: uuid]
           A 16-byte array (128-bit number)
      */
    KDU_AUX_EXPORT bool
      get_standard_feature(int which, kdu_uint16 &feature_id);
      /* [SYNOPSIS]
           Use this function to enumerate the standard features found in a
           JPX reader requirements box.  Enumeration is achieved by
           incrementing `which' from 0 until the function returns false.
         [RETURNS]
           True if a valid feature is returned via `feature_id'.  Otherwise,
           there are at most `which' features.
         [ARG: which]
           If 0, the function returns the first standard feature
           listed in the reader requirements box via the `feature_id'
           argument.  Each consecutive value returns the next feature listed
           in the reader requirements box in sequence.  If the function
           returns false, the number of standard features is less than or
           equal to `which'.
         [ARG: feature_id]
           Used by the function to pass the actual feature ID value back
           to the caller.  The contents of this variable are undefined if
           the function returns false.  For a list of standard features
           see the `check_standard_feature' function.
      */
    KDU_AUX_EXPORT bool
      get_standard_feature(int which, kdu_uint16 &feature_id,
                           bool &is_supported);
      /* [SYNOPSIS]
           Same as the first form of this overloaded function, except that
           the additional `is_supported' argument is used to return an
           indication of whether the feature is currently marked as supported.
           By default, all standard features are supported, except for those
           features which the internal object inherently fails to properly
           support, but this situation may be changed through calls to
           `set_standard_feature_support'.
      */
    KDU_AUX_EXPORT bool
      get_vendor_feature(int which, kdu_byte uuid[]);
      /* [SYNOPSIS]
           Same as `get_standard_feature', but used to enumerate vendor
           features found in the reader requirements box.
         [RETURNS]
           False if `which' is greater than or equal to the number of
           vendor features found in the reader requirements box.
         [ARG: uuid]
           Points to a 16-byte array into which the UUID of the vendor
           feature will be written.  The contents of this array are
           undefined if the function returns false.
      */
    KDU_AUX_EXPORT bool
      get_vendor_feature(int which, kdu_byte uuid[], bool &is_supported);
      /* [SYNOPSIS]
           Same as the first form of this overloaded function, except that
           the additional `is_supported' argument is used to return an
           indication of whether the feature is currently marked as supported.
           By default, all vendor features are unsupported, except for those
           features which the internal object inherently supports (there may
           be some such features in the future, but it is unlikely).
           However, this situation may be changed through calls to
           `set_vendor_feature_support'.
      */
    KDU_AUX_EXPORT void
      set_standard_feature_support(kdu_uint16 feature_id, bool is_supported);
      /* [SYNOPSIS]
           This function is provided for applications which are reading
           an existing JPX data source.  The application typically uses
           this function to identify those features which it explicitly
           does not support (setting `is_supported' = false).
           [//]
           By default all standard features are marked as supported, except
           those whose support is the responsibility of the `jpx_source'
           object and which are known not to be currently supported.  By
           contrast, all vendor features (see `set_vendor_feature_support')
           are unsupported by default.
           [//]
           Once the collection  of supported features has been identified,
           the application may use the `test_fully_understand' and/or
           `test_decode_completely' function to evaluate the reader
           requirements expressions and hence deduce whether or not it will
           be able to fully understand all aspects of the data source, or at
           least completely decode the data source to obtain the result
           intended by its creator.
         [ARG: feature_id]
           For a list of standard features, see the `check_standard_feature'
           function.
         [ARG: is_supported]
           If true, support for this feature is being affirmed.  Otherwise,
           it is being declined.  Since all standard features are supported
           by default, unless the internal implementation inherently fails
           to support them, this function will normally be used with
           `is_supported' set to false.
      */
    KDU_AUX_EXPORT void
      set_vendor_feature_support(const kdu_byte uuid[], bool is_supported);
      /* [SYNOPSIS]
           This function plays a similar role to `set_standard_feature_support'
           but differs in the following respect.
           [//]
           All vendor features are, by default, NOT supported unless the
           internal implementation provides inherent support for some such
           feature (this could conceivably happen in the future, but is
           unlikely).
           [//]
           If the application itself offers support for some feature, it
           should add this feature explicitly here before calling the
           `test_fully_understand' or `test_decode_completely' functions.
           By contrast, all standard features are inherently supported,
           unless the internal implementation inherently does not support
           them.
         [ARG: uuid]
           Points to an array with 16 bytes (a 128 bit number), representing
           the relevant vendor feature's UUID.
         [ARG: is_supported]
           If true, support for this feature is being affirmed.  Otherwise,
           it is being declined.  Since all vendor features are unsupported
           by default, unless the internal implementation inherently supports
           them, this function will normally be used with `is_supported'
           set to true.
      */
    KDU_AUX_EXPORT bool test_fully_understand();
      /* [SYNOPSIS]
           Call this function (optionally after using
           `set_standard_feature_support' and/or `set_vendor_feature_support')
           to evaluate the "fully understand" expression in the reader
           requirements box, returning true if the application should be able
           to fully understand all aspects of the JPX data source being read.
      */
    KDU_AUX_EXPORT bool test_decode_completely();
      /* [SYNOPSIS]
           Call this function (optionally after using
           `set_standard_feature_support' and/or `set_vendor_feature_support')
           to evaluate the "decode completely" expression in the reader
           requirements box, returning true if the application should be able
           to decode the JPX data source to obtain the result intended by
           its creator.
           [//]
           The features required to decode the data source completely may
           be less than those required to fully understand all aspects of
           the data source.  As an example, an editor may have preserved some
           elements from an original source file which it did not even fully
           understand.  These features are not required to achieve the
           intent of the editor, but they would be required to fully
           understand all aspects of the data source.
      */
  // --------------------------------------------------------------------------
  public: // Functions used for JPX file generation
    KDU_AUX_EXPORT void copy(jpx_compatibility src);
      /* [SYNOPSIS]
           Use this function to copy compatibility information from the
           `src' object to the present object.
      */
    KDU_AUX_EXPORT void
      set_used_standard_feature(kdu_uint16 feature_id,
                                kdu_byte fully_understand_sub_expression=255,
                                kdu_byte decode_completely_sub_expression=255);
      /* [SYNOPSIS]
           This function is provided to allow applications to customize the
           contents of the reader requirements box.  The function is used
           both to identify the contents of the file and also to
           generate "fully understand" and "decode completely" expressions.
           [//]
           The "fully understand" and "decode completely" expressions are
           each evaluated by taking the logical AND of a collection of
           sub-expressions.  Each sub_expression is evaluated by taking the
           logical OR of a collection of features.  If any of the features
           identified by the sub-expression is available to the reader, that
           sub-expression evaluates to true.  If all sub-expressions
           evaluate to true, the corresponding "fully understand" or
           "decode completely" expression evaluates to true.  If there are
           no sub-expressions at all, the "fully understand" and
           "decode completely" expressions also evaluate to true.
           [//]
           For an explanation of the difference between the "fully understand"
           and "decode completely" expressions, see the comments appearing
           with the `test_fully_understand' and `test_decode_completely'
           functions.
           [//]
           In addition to the sub-expressions generated here, the `jpx_target'
           object builds its own sub-expressions to represent the features
           it knows about.  The features which it knows about are those
           marked with an asterisk in the list of standard features appearing
           with the `check_feature' function.  If any of those features
           are not explicitly used to form sub-expressions by means of the
           present function, they will be formed into sub-expressions which
           identify them as required both to completely decode and to fully
           understand the file's contents, wherever this makes sense.
         [ARG: feature_id]
           See the `check_standard_feature' function for a list of standard
           feature identifiers.
         [ARG: fully_understand_sub_expression]
           Indicates which sub-expression of the "fully understand" expression
           this feature is being added to.  The index has no particular
           meaning except to associate multiple features in the same
           sub-expression with one another.  However, valid
           sub-expression indices must lie in the range 0 to 254.  The
           special value of 255 means that the feature is not being added to
           any "fully understand" sub-expression.  This is useful if the
           feature is only to be added to "decode completely" sub-expressions.
           It also provides a useful mechanism for preventing the internal
           object from automatically asserting that the feature is required
           to fully understand the file, if that feature is believed to have
           been used.
         [ARG: decode_completely_sub_expression]
           Indicates which sub-expression of the "decode completely" expression
           this feature is being added to.  The index has no particular
           meaning except to associate multiple features in the same
           sub-expression with one another.  However, valid sub-expression
           indices must lie in the range 0 to 254.  The special value of 255
           means that the feature is not being added to any "decode completely"
           sub-expression.  This is useful if the feature is only to be
           added to "fully understand" sub-expressions.  It also provides a
           useful mechanism for preventing the internal object from
           automatically asserting that the feature is required to completely
           decode the file, if that feature is believed to have been used.
      */
    KDU_AUX_EXPORT void
      set_used_vendor_feature(const kdu_byte uuid[],
                              kdu_byte fully_understand_sub_expression=255,
                              kdu_byte decode_completely_sub_expression=255);
      /* [SYNOPSIS]
           Same as `set_used_standard_feature' but for a vendor feature.
         [ARG: uuid]
           Vendor features are identified by a 16 byte UUID (128 bit number).
         [ARG: fully_understand_sub_expression]
           See description of `set_used_standard_feature'.
         [ARG: decode_completely_sub_expression]
           See description of `set_used_standard_feature'.
      */
  private: // State
    jx_compatibility *state;
  };

/*****************************************************************************/
/*                             jpx_composition                               */
/*****************************************************************************/

class jpx_composition {
  /* [BIND: interface]
     [SYNOPSIS]
       Manages the information associated with the composition box
       (box-type = `comp') which may be found in a JPX file.  At most
       one composition box may be found in a JPX file, describing the
       generation of composited and/or animated images from the various
       compositing layers.
       [//]
       The object provides an interface which is more natural than simply
       supplying or retrieving compositing instructions to or from the
       JPX composition box.  This means that the object may need to do quite
       a bit of work to make things look simple for you.  There are some
       ways to abuse this so that a seemingly simple animation ends up being
       extremely costly to encode into a JPX composition box.  The main thing
       to watch out for is that you should avoid describing animations in
       which the composition layers are played backwards.  The JPX composition
       box is designed with the idea of using layers one after the other, with
       possible re-use of layers at later points in the animation.  It is
       possible to describe animations in which the layers are played from
       the last one back to the first one, but such a description will not
       be efficient and may not be handled efficiently by some JPX
       readers (should not affect Kakadu readers, if they are based on the
       `kdu_region_compositor' object).
       [//]
       Objects of this class are merely interfaces to an internal
       object, which cannot be directly created by an application.
       Use `jpx_source::access_composition' or
       `jpx_target::access_composition' to obtain a non-empty interface.
  */
  public: // Lifecycle member functions
    jpx_composition() { state = NULL; }
    jpx_composition(jx_composition *state) { this->state = state; }
      /* [SYNOPSIS]
           Do not call this function yourself.  Use
           `jpx_source::access_composition' or
           `jpx_target::access_composition' to obtain a non-empty interface.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns false if the interface is empty, meaning that it is not
           currently associated with any internal implementation.  See the
           `jpx_composition' overview for more on how to get access to a
           non-empty interface.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS] Opposite of `exists', returning true if the interface is
         empty.
      */
  public: // Member functions for recovering composition instructions
    KDU_AUX_EXPORT void copy(jpx_composition src);
      /* [SYNOPSIS]
           Use this function to copy the composition instructions from the
           `src' object to the present object.
      */
    KDU_AUX_EXPORT int get_global_info(kdu_coords &size);
      /* [SYNOPSIS]
           Returns the total number of times the sequence of composited
           frames is to be iterated, along with the dimensions of the
           composition surface, onto which all composition layers are to be
           placed.
         [RETURNS]
           0 if the sequence of composited frames is to be repeated
           indefinitely.  Otherwise, a number in the range 1 to 255,
           indicating the number of times the video is to be played
           through.
      */
    KDU_AUX_EXPORT jx_frame *get_next_frame(jx_frame *last_frame);
      /* [SYNOPSIS]
           Use this function to walk through frames within an animation.
           Setting `last_frame' to NULL will cause the first frame to be
           accessed.  Supplying the pointer returned by this function to a
           subsequent call, will cause the next frame to be accessed.
         [RETURNS]
           An opaque pointer which may be used to access the frame's
           instructions, or to access the next frame in sequence.  The
           returned pointer will be NULL if and only if the requested frame
           does not exist.  This could happen if the `last_frame' argument
           refers to the very last available frame.
      */
    KDU_AUX_EXPORT jx_frame *get_prev_frame(jx_frame *last_frame);
      /* [SYNOPSIS]
           Use this function to walk backwards through frames within an
           animation, returning a pointer to the frame which immediately
           precedes that identified by the `last_frame' argument.  If there
           is none, the function returns NULL.  Note that, unlike
           `get_next_frame', the `last_frame' argument supplied to this
           function MUST NOT be NULL.
         [RETURNS]
           An opaque pointer which may be used to access the frame's
           instructions.  The returned pointer will be NULL if and only if
           the requested frame does not exist.  This could happen if the
           `last_frame' argument refers to the very first available frame.
      */
    KDU_AUX_EXPORT void
      get_frame_info(jx_frame *frame_ref, int &num_instructions,
                     int &duration, int &repeat_count,
                     bool &is_persistent);
      /* [SYNOPSIS]
           Returns overview information concerning a single frame.  The
           opaque `frame_ref' pointer may have been obtained by a previous
           call to `get_next_frame', `get_prev_frame', or
           `get_last_persistent_frame'.  It may not be NULL.
           [//]
           Each frame may need to be repeated some number of times, as
           returned via the `repeat_count' argument, before moving to the
           next frame.  Repetition does not mean that exactly the same
           composited image is played over and over again.  Instead, the
           actual layer indices associated with compositing instructions in
           the frame may need to be incremented between repeated frames.
           [//]
           Each frame consists of a sequence of compositing instructions
           which must be performed in order to build up a single composited
           image.  Consecutive frames represent distinct composited images
           to be displayed at distinct time instants.
         [ARG: num_instructions]
           Used to return the number of compositing instructions used to
           compose the current frame.  Use `get_instruction' to examine the
           details of each instruction.
         [ARG: duration]
           Used to return the number of milliseconds between this frame and
           the next.  This value cannot be zero, except possibly in the last
           frame -- this should only be possible when the global looping
           count is 1, but some crazy content creator could do something
           weird.
         [ARG: repeat_count]
           Used to return the number of times (in addition to the first time)
           which the frame should be repeated.  If 0, the frame is not
           repeated.  If negative, the frame is repeated indefinitely.
           Repeated frames are separated by `duration' milliseconds.  Repeated
           frames do not necessarily use the same compositing layers in their
           instructions.  See `get_instruction' for more on the adjustment of
           compositing layer indices between repeatitions of a frame.
         [ARG: is_persistent]
           Used to return an indication of whether or not the effects of
           compositing this frame should be used as a background for all
           future composition operations.  To render any given frame, one
           must compose all preceding persistent frames in sequence, followed
           by the frame of interest.
           [//]
           Although this seems like a simple idea, in some circumstances
           (e.g., dynamic region-of-interest rendering) it can create an
           enormous amount of work.  For this reason, persistent frames are
           best avoided.  The `kdu_region_compositor' object goes to a lot
           of effort to figure out which persistent frames are actually
           visible (i.e., not completely covered by new opaque layers) so as
           to avoid composing hundreds or thousands of layers together where
           frame persistence has been misused.  Unfortunately, the visibility
           of persistent frames in later frames cannot properly be figured
           out within the `kdu_composition' object, since composition
           information (e.g., to figure out which layers are needed) may be
           required before the opacity of compositing layers has become
           available from a dynamic cache.
      */
    KDU_AUX_EXPORT jx_frame *
      get_last_persistent_frame(jx_frame *frame_ref);
      /* [SYNOPSIS]
           Returns a reference to the most recent frame, preceding that
           referenced by the opaque `frame_ref' pointer, which is marked as
           persistent.  By following last persistent frame links, it is
           possible to walk backwards through the list of frames which
           must notionally be composed onto the composition surface prior
           to the current frame.  Invoking `get_frame_info' on each of these
           frames will cause the supplied `is_persistent' argument to be set
           to true.
           [//]
           This function enables dynamic renderers such as that embodied by
           `kdu_region_compositor' to efficiently scan through the
           compositing dependencies, stopping as soon as it encounters a
           complete opaque background.
      */
    KDU_AUX_EXPORT bool
      get_instruction(jx_frame *frame_ref, int instruction_idx,
                      int &layer_idx, int &increment, bool &is_reused,
                      kdu_dims &source_dims, kdu_dims &target_dims);
      /* [SYNOPSIS]
           The JPX composition box is built from a collection of instructions
           which each either add to the composition associated with the
           previous instruction or commence from scratch with a new
           composition surface.  Each instruction also has its own timing.
           These aspects are untangled here to create a set of distinct
           frames, each of which has its own set of compositing layers.
           [//]
           Each frame is composed by applying each of its compositing layers
           in turn to an initially blank compositing surface.  Each
           compositing layer is applied to the region of the compositing
           surface expressed by `target_dims', where the location parameters
           in `target_dims.pos' must be non-negative.  This region is
           generated by scaling (as required) the region of the source
           compositing layer described by `source_dims'.
           [//]
           Each instruction refers to exactly one compositing layer.  The
           index of this layer is returned via the `layer_idx' argument,
           but each time the frame is repeated (if the `repeat_count' returned
           via `get_next_frame' is non-zero), the layer index must be augmented
           by `increment' (might be 0).
         [RETURNS]
           False if `instruction_idx' is greater than or equal to the
           `num_instructions' value returned via `get_frame_info'.
         [ARG: frame_ref]
           Opaque pointer obtained from calls to `get_next_frame',
           `get_prev_frame' or `get_last_persistent_frame'.
         [ARG: instruction_idx]
           0 retrieves the first instruction in the frame.  Each successive
           instruction is retrieved using consecutive instruction indices,
           up to the L-1, where L is the number of instructions returned
           by `get_next_frame'.
         [ARG: layer_idx]
           Used to return the index of the compositing layer (starting from
           0 for the first one in the JPX data source) to be used with this
           instruction.
         [ARG: increment]
           Amount to be added to `layer_idx' each time the frame is repeated.
           We are not referring here to global looping, as identified by
           the loop counter returned by `get_global_info'.  We are referring
           only to repetitions of the frame, as identified by the
           `repeat_count' value retrieved using `get_next_frame'.
         [ARG: is_reused]
           Used to retrieve a boolean flag indicating whether or not the
           compositing layer associated with this instruction will be reused
           in any subsequent instruction in this or any future frame.  This
           can be useful for cache optimization.
         [ARG: source_dims]
           Region to be cropped from the source composition layer to use
           in this instruction.  The `source_dims.pos' member identifies
           the location of the cropped region relative to the upper left
           hand corner of the image itself; its coordinates must be
           non-negative.  All coordinates are expressed on the compositing
           layer's registration grid.  For a description of the registration
           grid see `jpx_layer_source::get_codestream_registration'.
           [//]
           If `source_dims' has zero area, the source region is taken to be
           the whole of the source compositing layer.
         [ARG: target_dims]
           Region within the composition surface to which the `source_dims'
           region of the compositing layer is to be mapped.  This may
           involve some scaling.
           [//]
           If `target_dims' has zero area, the size of the target region is
           taken to be identical to `source_dims.size', unless `source_dims'
           also has zero area, in which case the size of the target region
           is taken to be identical to the size of the source compositing
           layer.
      */
    KDU_AUX_EXPORT bool
      get_original_iset(jx_frame *frame_ref, int instruction_idx,
                        int &iset_idx, int &inum_idx);
      /* [SYNOPSIS]
           This function may be used to determine the original instruction
           set box from which the indicated frame instruction, as well as
           the location within that box.  This information is useful only
           for constructing JPIP requests (see
           `kdu_sampled_range::remapping_ids').
         [RETURNS]
           False if `instruction_idx' is greater than or equal to the
           `num_instructions' value returned via `get_frame_info'.
         [ARG: frame_ref]
           Opaque pointer obtained from calls to `get_next_frame',
           `get_prev_frame' or `get_last_persistent_frame'.
         [ARG: instruction_idx]
           Identifies the instruction within the frame identified by
           `frame_ref' for which the original box location is required.
           The interpretation of this argument is the same as in the
           `get_instruction' function.
         [ARG: iset_idx]
           Used to return the index (starting from 0) of the instruction set
           (`inst') box within the composition (`comp') box from which the
           indicated frame instruction was recovered.  If the composition
           box has not yet been written, a negative value will be returned
           via this argument.
         [ARG: inum_idx]
           Used to return the index of the specific
           instruction within the above-mentioned instruction set box, from
           which this frame instruction was recovered.  A value of 0 means
           that it was recovered from the first instruction in the
           instruction set box.  If the composition box has not yet been
           written, a negative value will be returned via this argument.
      */
  public: // Member functions for creating compositions
    KDU_AUX_EXPORT jx_frame *
      add_frame(int duration, int repeat_count, bool is_persistent);
      /* [SYNOPSIS]
           Use this function to add a frame, specifying the number of
           milliseconds (`duration') until the next frame (if any) is to
           be started.  You must call this function at least once to cause
           a composition box to be written into the JPX file being generated
           by the `jpx_target' object which supplied the present interface.
           [//]
           Each frame can be repeated to create simple animations or movies.
           To make more sophisticated animations, you may need to explicitly
           add multiple frames.  Where a frame is repeated, the layer indices
           associated with the compositing layers within each frame may
           either remain fixed, or be advanced in a regular fashion.  For
           more on this, see `add_instruction'.
           [//]
           You must add at least one instruction to each new frame which
           you create.
         [RETURNS]
           The an opaque pointer which should be passed to `add_instruction'
           to define the individual compositing layers which will constitute
           the frame.
         [ARG: duration]
           Time between this frame and the next, measured in milliseconds.
           If the frame is repeated, this is the time between the repetitions
           and the time between the last repetition and any frame which
           follows.  You should not specify a zero-valued duration, except
           possibly for the last frame in the sequence. The duration of the
           last frame is completely irrelevant.
         [ARG: repeat_count]
           If 0, the frame is not repeated.  Otherwise, this is the number
           of times the frame is to be repeated.  A negative value means
           that the frame should be repeated indefinitely.  This only
           makes sense for the last frame to be added.
         [ARG: is_persistent]
           If true, the compositing instructions in this frame contribute
           to a background composition which is to be used by all subsequent
           frames.  Otherwise, the instructions in this frame contribute
           only to the rendering of the frame itself.
           [//]
           Although persistent frames sound like a good idea, you should
           try to avoid them.  One reason for this is that they make it
           difficult for a player to start from an arbitrary frame in the
           sequence.  Another more insidious reason is that persistent
           frames make it difficult to do implement quality progressive
           compositions or dynamic rendering of regions of interest within
           a large composed image which cannot be fully stored in a buffer.
           The `kdu_region_compositor' object goes to a lot of effort to
           figure out which persistent frames can be ignored in such a
           rendering process, but it is far better as a content creator to
           simply avoid the use of persistent frames, or to use only a
           single persistent background frame.
      */
    KDU_AUX_EXPORT int
      add_instruction(jx_frame *frame_ref, int layer_idx, int increment,
                      kdu_dims source_dims, kdu_dims target_dims);
      /* [SYNOPSIS]
           Use this function to add one or more instructions to each added
           frame.
         [RETURNS]
           The sequence number (starting from 0) of the instruction being
           added.
         [ARG: frame_ref]
           Opaque pointer returned by `add_frame'.
         [ARG: layer_idx]
           Each instruction involves a single compositing layer, whose
           index is given here.  0 refers to the first compositing layer in
           the file.  If the frame is to be repeated, this is the index of
           the compositing layer to use in the first repetition.  Thereafter,
           the index is advanced by `increment' each time.
         [ARG: increment]
           Ignored unless the frame is repeated.  Each time the frame is
           repeated (see `repeat_count' argument to `add_frame'), the layer
           index is incremented by this value.  Frame repetition stops
           immediately if the layer index grows beyond the number of layers
           available from the file.  As an example, you might like to build
           a composite animation from a consecutive sequence of layers
           (`increment'=1) which are composed onto a fixed background layer
           (`increment'=0).
           [//]
           Note that there can be some weird sequences of layer increments
           which the internal machinery will not be able to represent
           efficiently.  As a general rule, you should try to use only
           positive and zero-valued increments (although negative increments
           are not forbidden) and you should try to use the same increments
           for all instructions in the frame.
         [ARG: source_dims]
           Specifies the cropped region within the compositing layer
           which is to be used by this instruction.  The region is
           expressed on the compositing layer's registration grid, as
           described with `jpx_layer_target::set_codestream_registration'.
         [ARG: target_dims]
           Specifies the region on the composition surface to which the
           cropped source region should be rendered.  This may involve
           some scaling.
           [//]
           Note, however, that compositing applications might not necessarily
           support arbitrary scaling factors.  Kakadu's powerful
           `kdu_region_compositor' object, for example, might not support
           anything other than integer-valued upsampling.
      */
    KDU_AUX_EXPORT void set_loop_count(int count);
      /* [SYNOPSIS]
           By default, the complete sequence of frames will be played through
           exactly once.  This is equivalent to the case `count'=1.
           Call this function to set a different value for the loop counter.
           A value of `count'=0 will cause the frame sequence to be repeated
           indefinitely.
           [//]
           When the frames are repeated, they are played again with exactly
           the same set of layers; the layer indices are not incremented
           upon global repetition.
           [//]
           Note that the maximum representable loop count is 255.  Supplying
           a larger value will result in the generation of an error.
      */
  private: // Data
    jx_composition *state;
  };

/*****************************************************************************/
/*                           jpx_frame_expander                              */
/*****************************************************************************/

class jpx_frame_expander {
  /* [BIND: reference]
       This is a standalone object, which provides useful services for
       applications wishing to generate image compositions and animations,
       or which need to form JPIP requests for such things.
       [//]
       To use the object, you simply pass the opaque `jx_frame' reference
       derived using `jpx_composition::get_next_frame' (or another similar
       function) into `construct', which proceeds to figure out all the
       compositing instructions whose compositing layers contribute to the
       composited frame, excluding those which are hidden by opaque layers
       with a higher Z order.
       [//]
       It may seem at first glance as though the present object does not
       provide a great deal of functionality over that which can be
       obtained using the `jpx_composition::get_instruction' function.
       However, it is important to bear in mind that the JPX data source
       may ultimately be fueled by a dynamic cache, whose contents grow
       over time.  As the contents of the cache grow, our knowledge about the
       elements which contribute to the creation of a composited frame can
       change.  For this reason, you may end up applying the `construct'
       function many times.  The types of changes which can occur are
       discussed more carefully in connection with that function.  For
       now, however, it is sufficient to appreciate that the various
       member functions offered by `jpx_composition' always return the
       same results -- they do not take into account information about the
       compositing layers themselves, which may become available later.
  */
  public: // member functions
    jpx_frame_expander()
      { num_members=max_members=0; members=NULL; non_covering_members=false; }
    ~jpx_frame_expander() { if (members != NULL) delete[] members; }
    void reset() { num_members = 0; non_covering_members=false; }
      /* [SYNOPSIS]
           May be used to reset the state to one which identifies no
           members.  There is no need to do this prior to calling
           `construct', since that function resets the object first.
      */
    KDU_AUX_EXPORT int
      test_codestream_visibility(jpx_source *source, jx_frame *frame,
                                 int iteration_idx, bool follow_persistence,
                                 int codestream_idx,
                                 kdu_dims &composition_region,
                                 kdu_dims codestream_roi=kdu_dims(),
                                 bool ignore_use_in_alpha=true,
                                 int initial_matches_to_skip=0);
      /* [SYNOPSIS]
           This powerful function, new to Kakadu v6.1, provides a method for
           determining the visibility of a codestream (or a region of interest
           defined on the codestream) in the overall composition associated
           with a given instance (given by `iteration_idx') of a given `frame'.
           One of the main applications for this function is determining
           whether a region-of-interest defined at the file-format level, for
           one or more codestreams, is visible in a frame, so that the next
           visible frame can be located and displayed when the region of
           interest is selected from a menu by an interactive user.  Under the
           same conditions, the function can be used to determine now suitable
           JPIP requests should be synthesized in response to the selection of
           a region of interest (e.g., by selection of an associated label
           from a text-based catalog), so that the relevant region can be
           efficiently fetched from a remote JPIP server.
           [//]
           The first four arguments have exactly the same interpretation as
           their namesakes in the `construct' function.  Unlike that function,
           however, the present function does not actually manipulate the
           internal structure of the object, so that it has no effect on the
           future return values of functions like `get_num_members',
          `has_covering_members' and the like.
           [//]
           Since a given codestream might appear multiple times in the
           construction of a frame from its various compositing layers, the
           function provides you with the option to walk through each layer
           in which the codestream (or an identified region of it) might be
           visible in the composition.  This is done via the optional
           `initial_matches_to_skip' argument.  The caller may use this
           argument to figure out the layer in which the codestream (or its
           region of interest) is most visible.  In most cases, however, the
           first match is a likely to be the most visible one, since the
           function works backwards from the top-most compositing layers to
           the bottom-most, the latter being more likely to be covered.
           [//]
           The function is only able to provide a conservative
           estimate of visibility, based upon intersecting the original
           region of interest with any opaque covering layers.  In particular,
           whenever an opaque layer covers the layer which contains this
           codestream's region of interest, the function estimates the
           visible portion of the region to be the largest rectangular
           subset of the original region, such that none of its rows and
           columns are completely covered.  Where multiple layers cover
           the region in complicated ways, this function may estimate the
           region to be visible even though it has been completely hidden.
           However, this is very unlikely.
         [RETURNS]
           The index of the compositing layer in which the codestream
           (or an identified region of the codestream) may be visible.
           A value of -1 is returned if no visible match is found.
         [ARG: source]
           See `construct'.
         [ARG: frame]
           See `construct'.
         [ARG: iteration_idx]
           See `construct'.
         [ARG: follow_persistence]
           See `construct'.
         [ARG: codestream_idx]
           Index of the codestream, whose visibility is being tested.
         [ARG: composition_region]
           On entry, if this region is non-empty, it identifies a region on
           the composited frame, in which the visibility of the codestream
           (or its region) is of interest.  This may, for example, be a
           viewport being used by an interactive viewer.  If empty, the
           region is considered to be that of the entire composited frame.
           [//]
           On exit, the `composition_region' object is set to contain the
           region of the codestream (restricted to any supplied
           `codestream_roi') which is actually visible, as expressed in
           the coordinate system of the composited image.
         [ARG: codestream_roi]
           If this region is non-empty, it identifies a region of
           interest whose visibility we are testing, expressed with respect
           to the high resolution reference grid of the codestream in question,
           except that the location of the region is expressed relative to
           the origin of the image on the codestream, which might not always
           be the same as the origin of the high resolution reference grid
           itself (this is the same convention used by ROI description boxes
           in the JPX file format -- no accident).  If the region is empty,
           the region of interest is considered to be the entire codestream.
         [ARG: ignore_use_in_alpha]
           If true, the function ignores codestreams which are used only for
           generating opacity data for a compositing layer.  This is usually
           a good idea, since the file format only allows regions of
           interest to be specified on a per-codestream basis.  If the
           codestream contains some components which are used only for
           alpha blending channels and those components are used in some
           compositing layers which do not employ the actual imagery, it
           is unlikely that we will be interested in the region of interest
           within such layers.
         [ARG: initial_matches_to_skip]
           If zero, the function returns the first compositing layer in
           which the codestream (or its `codestream_roi') is visible
           (or visible within the originally supplied `composition_region').
           If this argument is set to 1, the function returns the second
           compositing layer in which the visibility conditions are found
           to hold; and so forth.
      */
    KDU_AUX_EXPORT bool
      construct(jpx_source *source, jx_frame *frame, int iteration_idx,
                bool follow_persistence,
                kdu_dims region_of_interest=kdu_dims());
      /* [SYNOPSIS]
           This function resets the object to the empty state and then
           proceeds to add instructions from the supplied frame, starting
           from the last instruction (the one whose compositing layer will
           be painted on top of all the others) and working backwards (toward
           the background).  Each instruction contributes a single member,
           returned via `get_members', except for any instruction which is
           known to have no impact on the region of interest.  If the
           `region_of_interest' argument supplies an empty region, the
           region of interest is taken to be the entire composition
           surface.  In any event, an instruction has no impact on the region
           of interest if all imagery which it composits within that region
           lies underneath an existing opaque image region.
           [//]
           Note that it may not be possible to determine the opacity (or even
           size) of potential covering imagery if the relevant compositing
           layers are not yet accessible.
           [//]
           We say that a compositing layer is "not yet accessible" if the
           JPX data source is fueled by a dynamic cache which does not yet
           have sufficient contents to successfully access the layer using
           `jpx_source::access_layer'.  To clarify this important concept,
           we make the following two points:
           [>>] If we are able to determine that a relevant layer lies
                outside the range of compositing layers which can ever exist
                in the data source, we DO NOT think of the layer as
                "not yet accessible".  Instead, the function returns
                immediately with the object in its reset state (no members),
                since the frame does not really exist.  The way the JPX
                composition box works is that it is allowed to define frames
                which use non-existent layers, but once a reader encounters
                any such frame, it is supposed to ignore that frame and all
                subsequent frames.
           [>>] Apart from the condition mentioned above, a "not yet
                accessible" layer is one for which `jpx_source::access_layer'
                returns an empty interface, when called with a false value
                for its `need_stream_headers' argument.  Thus, a layer is
                deemed to be accessible if all the relevant JPX/JP2 boxes
                have been encountered, even if the relevant codestream
                main headers are not yet available.
           [//]
           Any instruction whose compositing layer is not yet accessible is
           included in the member list, even if it is known not to contribute
           to the region of interest, except if either of the following
           conditions occur:
           [>>] The composited imagery from this instruction is known to
                be globally invisible (not just within the region of interest,
                but not contributing to any part of the compositing surface);
                or
           [>>] The compositing layer already appears in some other
                contributing member instruction.
           [//]
           The reason for the above policy is that compositing layers are
           not accessible only if the JPX data source is being served by a
           dynamic cache, typically in a JPIP client-server application.  In
           order to open an interactive viewer for the frame, all relevant
           compositing layers will need to be accessible so a JPIP query will
           need to be formed which references the compositing layers which
           are not currently available.
           [//]
           The function stops examining instructions once the compositing
           surface is completely covered by opaque imagery.
           [//]
           If `follow_persistence' is true, the function continues to examine
           persistent frames which may lie beneath the current one, adding
           their instructions into the mix, until all persistent frames
           are accounted for.  It traverses the persistent frame list by
           using the `jpx_composition::get_last_persistent_frame' function.
           Again, this process stops if at any point the compositing
           surface is completely covered by opaque imagery.
         [RETURNS]
           False if any of the compositing layers which might contribute
           to the composited frame cannot yet be accessed via
           `jpx_source::access_layer', when invoked with a false value for
           its `need_stream_headers' argument.  In this case, calling the
           function again may result in a smaller number of members being
           added, since we might not yet be able to determine whether or not
           a layer is opaque or even whether it is hidden by another layer
           known to be opaque.
           [//]
           As noted above, we say that a compositing layer is not accessible
           only if insufficient information is currently available to open
           it or determine its existence.  If we can determine that any
           required compositing layer will never exist (by using
           `jpx_source::count_compositing_layers'), the present function will
           return true, but with the number of members set to 0.
         [ARG: source]
           JPX data source whose `jpx_source::access_composition' member
           was used to obtain the `jpx_composition' interface from which
           `frame' was recovered.
         [ARG: frame]
           An opaque pointer, recovered using `jpx_composition::get_next_frame'
           or any of the other member functions of `jpx_composition' which
           can be used to obtain frame references.
         [ARG: iteration_idx]
           Used to identify the particular frame within a repeated sequence.
           0 refers to the first frame.  The value supplied here may be as
           large as, but must be no larger than the repeat count returned
           via `jpx_composition::get_frame_info'.
         [ARG: follow_persistence]
           If true, the function examines persistent frames which form
           part of the background for the frame identified via `frame'.
      */
    bool has_non_covering_members() { return non_covering_members; }
      /* [SYNOPSIS]
           Returns true if at least one instruction encountered is known
           to produce imagery whose dimensions do not cover the entire
           compositing surface.  It may be that the coverage of some layers
           cannot yet be determined, in which case `construct' will have
           returned false, and a subsequent invocation of the `construct'
           function may cause this function to return true where it
           previously returned false.
      */
    int get_num_members() { return num_members; }
      /* [SYNOPSIS]
           Returns the number of members which were installed by the most
           recent call to `construct'.  Details of these members may
           be recovered using the `get_member' function.  If the most
           recent call to `construct' requested a non-existent frame, the
           value returned by the present function will be 0.
      */
    jx_frame *get_member(int which, int &instruction_idx, int &layer_idx,
                         bool &covers_composition,
                         kdu_dims &source_dims, kdu_dims &target_dims)
      {
        if ((which < 0) || (which >= num_members)) return NULL;
        jx_frame_member *mem = members+which;
        instruction_idx = mem->instruction_idx; layer_idx = mem->layer_idx;
        covers_composition = mem->covers_composition;
        source_dims = mem->source_dims; target_dims = mem->target_dims;
        return mem->frame;
      }
      /* [SYNOPSIS]
           Use this function to walk through the frame instructions which
           were found to contribute to the frame composition when `construct'
           was called.  Each member represents one contributing instruction.
           If the `construct' function returned false, some of these
           instructions might turn out later to be non-contributing, at
           which point calling `construct' again will cause such instructions
           not to be included as members.
         [RETURNS]
           The frame to which this member instruction belongs.  This will
           always be identical to the frame supplied to `construct', unless
           the `follow_persistence' argument to that function was true, in
           which case it may be an earlier persistent frame.
           [//]
           Returns NULL if the requested member does not exist.
         [ARG: which]
           Identifies the specific member of interest, where a value of 0
           refers to the top-most member instruction, and a value one less
           than that returned by `get_num_members' refers to the bottom-most
           (background) member instruction contributing to the composed frame.
           Values less than 0 or greater than or equal to the number of
           members will result in a NULL return.
         [ARG: instruction_idx]
           Used to return the index of the instruction within its frame.  This
           index may subsequently be passed to
           `jpx_composition::get_instruction', for example.
         [ARG: layer_idx]
           Used to return the index of the compositing layer associated with
           this member instruction.  This index may subsequently be passed
           to `jpx_source::access_layer'.
         [ARG: covers_composition]
           Used to return whether or not this member instruction is known to
           cover the compositing surface.  If the layer's size could not be
           determined completely at the time when `construct' was called
           (can happen only if the JPX data source is fueled by a dynamic
           cache which did not have enough information to open the
           compositing layer), this value will be set to false.
         [ARG: source_dims]
           Used to return the location and size of the source image region,
           relative to the complete source compositing layer.  The
           interpretation of this region is identical to that described
           with `jpx_composition::get_instruction', except that in the
           event that the source dimensions were not specified in the
           composition box and the composition layer is now accessible
           (guaranteed if the present function returns true), the source
           dimensions will automatically be set to represent the full
           compositing layer.  This saves the application the trouble of
           figuring out the missing information itself.
         [ARG: target_dims]
           Used to return the location and size of the imagery composited
           by this member instruction, expressed relative to the compositing
           surface.  The interpretation of this region is identical to that
           described with `jpx_composition::get_instruction', except that in
           the event that the target size was not specified in the composition
           box and the composition layer is now accessible (guaranteed if
           the present function returns true), the target size will
           automatically be set to be the same as the source size, which
           itself may be derived from the size of the compositing layer,
           if no source cropping was required.  This saves the application
           the trouble of figuring out the missing information itself.
      */
  private: // structures
      struct jx_frame_member {
          jx_frame *frame;
          int instruction_idx;
          int layer_idx;
          kdu_dims source_dims, target_dims;
          bool covers_composition;
          bool is_opaque;
          bool layer_is_accessible;
          bool may_be_visible_under_roi;
        };
  private: // data
    bool non_covering_members;
    int num_members;
    int max_members; // Size of `members' array
    jx_frame_member *members;
  };

/*****************************************************************************/
/*                          jpx_codestream_source                            */
/*****************************************************************************/

class jpx_codestream_source {
  /* [BIND: interface]
     [SYNOPSIS]
       Manages a single codestream within a JPX file.  Specifically, this
       object manages the codestream-specific attributes which may be
       found either in a codestream header (chdr) box or the global
       JP2 header box.  It also provides access to the code-stream itself.
       The box recovered via `open_stream' may be passed directly to
       `kdu_codestream::create'.
       [//]
       Objects of this class are merely interfaces to an internal
       object, which cannot be directly created by an application.
       Use `jpx_source::access_codestream' to obtain a non-empty interface.
  */
  public: // Member functions
    jpx_codestream_source() { state = NULL; }
    jpx_codestream_source(jx_codestream_source *state) { this->state = state; }
      /* [SYNOPSIS]
           Do not call this function yourself.  Use
           `jpx_source::access_codestream' to obtain a non-empty interface.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns false if the interface is empty, meaning that it is not
           currently associated with any internal implementation.  See the
           `jpx_codestream_source' overview for more on how to get access to a
           non-empty interface.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS] Opposite of `exists', returning true if the interface is
         empty.
      */
    KDU_AUX_EXPORT int get_codestream_id();
      /* [SYNOPSIS]
           Returns the identifier of the codestream managed by this object.
           Codestream identifiers correspond exactly with the order in
           which they appear within the JPX file, starting from 0.
      */
    KDU_AUX_EXPORT jp2_locator get_header_loc();
      /* [SYNOPSIS]
           Use this function to obtain a `jp2_locator' object which describes
           the location of the codestream header (chdr) box for this
           codestream.  If there is no codestream header box, the returned
           object's `jp2_locator::is_null' member will return true.
      */
    KDU_AUX_EXPORT jp2_dimensions
      access_dimensions(bool finalize_compatibility=false);
      /* [SYNOPSIS]
           Returns an interface which may be used to access the information
           recorded in the Image Header (ihdr) and Bits Per Component (bpcc)
           boxes of either the relevant codestream header (chdr) box or the
           default JP2 header box.
         [ARG: finalize_compatibility]
           This argument is important only if you wish to use the
           returned interface with `jp2_dimensions::copy' to copy the
           information to another `jp2_dimensions' object, associated with
           a JP2/JPX/MJ2 file you are writing.  In this case, setting this
           argument to true will save you all the hassle of opening the
           codestream, reading its main header and calling the
           `jp2_dimensions::finalize_compatibility' function yourself.  The
           only thing you need to take care of before calling the function
           with this argument set to true is that the box provided by
           `open_stream' is not currently being used.  The function only
           goes to the trouble of opening the stream and accessing its
           header information if this has not been done previously.
      */
    KDU_AUX_EXPORT jp2_palette access_palette();
      /* [SYNOPSIS]
           Returns an object which may be used to access any information
           recorded in a "Palette" box found within the codestream header
           (chdr) box or a relevant "Palette" box found within the default
           JP2 heade box.  If there is none, the object's
           `jp2_palette::get_num_luts' function will return 0.
      */
    KDU_AUX_EXPORT jp2_locator get_stream_loc();
      /* [SYNOPSIS]
           The returned `jp2_locator' object may be used to open the
           contiguous codestream box, or fragment table box which is
           associated with the actual code-stream data.  This function will
           never return a null locator (i.e., `jp2_locator::is_null' will
           never return true), even where a JPX file is dynamically
           served via JPIP.  The reason is that `jpx_source::access_codestream'
           will return a non-empty `jpx_codestream_source' interface only
           once sufficient information has become available for the
           codestream itself at least to be opened.
      */
    KDU_AUX_EXPORT bool stream_ready();
      /* [SYNOPSIS]
           Returns true if the reference returned by `get_stream_loc' refers
           to a contiguous codestream whose main header is available, or if
           it refers to a fragment table box whose fragment list is completely
           available.  In the latter case, it is not generally possible to
           assess whether the main header is available.  The return value can
           be false only if the source is ultimately fueled by a dynamic
           cache (i.e., a `jp2_family_src' object whose
           `jp2_family_src::open' function was passed a `kdu_cache' data
           source), and then only if the present interface was acquired by
           supplying a `false' value for the `need_main_header' argument to
           `jpx_source::access_codestream'.  Fragment tables should not
           generally be served directly by interactive servers so the fact
           that we cannot detect main headers for fragment tables should be
           of no concern.  If the function returns false, you should not
           pass the box returned by `open_stream' or any box opened using
           the `get_stream_loc' reference into `kdu_codestream::create'.
           Calling the function again, once more data has been added to the
           dynamic cache, may result in a return value which is true.
      */
    KDU_AUX_EXPORT jpx_fragment_list access_fragment_list();
      /* [SYNOPSIS]
           This function returns a non-empty fragment list interface if
           the codestream is represented by a fragment table box and the
           embedded fragment list box's contents are fully available.
           Otherwise it returns an empty interface.  Equivalently, a
           non-empty interface is returned if the codestream is ultimately
           represented by a fragment table and `stream_ready' returns true.
           [//]
           The returned fragment list may be passed to `jpx_input_box::open'
           to open the codestream to which the fragments point, as though
           it were a contiguous codestream.
      */
    KDU_AUX_EXPORT jpx_input_box *
      open_stream(jpx_input_box *my_resource=NULL);
      /* [SYNOPSIS]
           This function returns NULL if and only if `stream_ready' returns
           false.  Otherwise, the function either directly opens the
           contiguous codestream box or else it indirectly opens a
           virtual codestream box via its fragment list (if the codestream
           is represented by a fragment table).  In either case, the
           returned object's `jpx_input_box::get_box_type' function will
           return type `jp2_codestream_4cc' and the object will behave
           as if the codestream data were contiguous.
           [//]
           If `my_resource' is NULL, the function uses an internal
           `jpx_input_box' resource.  Every `jpx_codestream_source' object
           has one of these, which must be closed before it can be opened
           again (otherwise, the present function will generate an error
           through `kdu_error').  By passing in your own `jpx_input_box'
           object, however, as the `my_resource' argument, you can
           open multiple instances of the same codestream.  Regardless of
           whether the `my_resource' argument is NULL or not, a NULL return
           value has the same significance.
           [//]
           Other less convenient ways exist to open an input box to access
           the codestream data.  One approach is to pass the locator
           returned via `get_stream_loc' into `jp2_input_box::open'.  If
           this does not yield a box of type `jp2_codestream_4cc', however,
           you would need to pass the fragment list returned by
           `access_fragment_list' into `jpx_input_box::open'.  These
           possibilities are handled internally by the present function.
      */
  private: // State
    jx_codestream_source *state;
  };

/*****************************************************************************/
/*                          jpx_codestream_target                            */
/*****************************************************************************/

class jpx_codestream_target {
  /* [BIND: interface]
     [SYNOPSIS]
       Manages a single codestream within a JPX file.  Specifically, this
       object manages the codestream-specific attributes which will be
       recorded either in a codestream header (chdr) box or the global
       JP2 header box.
       [//]
       Objects of this class are merely interfaces to an internal
       object, which cannot be directly created by an application.
       Use `jpx_target::add_codestream' to obtain a non-empty interface.
  */
  public: // Member functions
    jpx_codestream_target() { state = NULL; }
    jpx_codestream_target(jx_codestream_target *state) { this->state = state; }
      /* [SYNOPSIS]
           Do not call this function yourself.  Use
           `jpx_target::add_codestream' to obtain a non-empty interface.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns false if the interface is empty, meaning that it is not
           currently associated with any internal implementation.  See the
           `jpx_codestream_target' overview for more on how to get access to a
           non-empty interface.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS] Opposite of `exists', returning true if the interface is
         empty.
      */
    KDU_AUX_EXPORT int get_codestream_id();
      /* [SYNOPSIS]
           Returns the identifier of the codestream managed by this object.
           Codestream identifiers correspond exactly with the order in
           which they appear within the JPX file, starting from 0.
      */
    KDU_AUX_EXPORT jp2_dimensions access_dimensions();
      /* [SYNOPSIS]
           Provides an interface which may be used for setting up the
           information to be written to the Image Header (ihdr) and Bits Per
           Component (bpcc) boxes.
           [//]
           You ARE REQUIRED to complete this initialization before calling
           `jpx_target::write_headers'.  The most convenient way to initialize
           the dimensions is usually to use the second form of the overloaded
           `jp2_dimensions::init' function, passing in the finalized
           `siz_params' object returned by `kdu_codestream::access_siz'.
      */
    KDU_AUX_EXPORT jp2_palette access_palette();
      /* [SYNOPSIS]
           Provides an interface which may be used for setting up a
           "Palette" (pclr) box.
           [//]
           It is NOT NECESSARY to access or initialize any palette
           information; the default behaviour is to not associate any
           palette with the codestream.
      */
    KDU_AUX_EXPORT void set_breakpoint(int i_param, void *addr_param);
      /* [SYNOPSIS]
           If you wish to record any specific boxes within the codestream
           header (chdr) box for this codestream, over and above the
           Image Header (ihdr), Bits Per Component (bpcc), Palette (pclr) and
           Component Mapping (cmap) boxes which are automatically generated,
           you should install a breakpoint.  Doing this will ensure firstly
           that a Codestream Header box is always written (even if all the
           other boxes are already recorded in the JP2 header box).  Secondly,
           the relevant call to `jpx_target::write_headers' will return
           prematurely, providing you with an opportunity to write the
           additional boxes, after which `jpx_target::write_headers' must be
           called again to resume the header generation process.
         [ARG: i_param]
           This is an arbitrary integer identifier which will be returned by
           `jpx_target::write_headers'.  You might use this to provide an
           interpretation for the `addr_param' value, allowing you to reliably
           cast it to some appropriate object reference.  Alternatively, you
           might use either or neither of the two parameters.
         [ARG: addr_param]
           This is an arbitrary address which will be returned by
           `jpx_target::write_headers'.
      */
    KDU_AUX_EXPORT jpx_fragment_list access_fragment_list();
      /* [SYNOPSIS]
           Returns the fragment list used by `add_fragment' to add
           references to codestreams contained in external files.
      */
    KDU_AUX_EXPORT void
      add_fragment(const char *url, kdu_long offset, kdu_long length);
      /* [SYNOPSIS]
           This function wraps up the functionalities of
           `jpx_fragment_list::add_fragment' and
           `jp2_data_references::add_url', working with the fragment list
           maintained by the present object and the data references object
           maintained by the containing `jpx_target' object.  The former
           may be recovered using `access_fragment_list', while the latter
           may be recovered using `jpx_target::access_data_references'.
           [//]
           The purpose of the fragment is to define the actual JPEG2000
           codestream described by this object in terms of a collection of
           references to byte ranges in (usually) external files/URL's.
           The concatenated list of fragments represents the complete
           codestream.  If you call this function, or add a fragment directly
           to the `jpx_fragment_list' object returned by
           `access_fragment_list', the codestream will be recorded using
           a fragment table box rather than a contiguous codestream box.
           For this reason, you must not invoke `open_stream' or
           `access_stream'.  Instead, before closing the containing
           `jpx_target' object, you should be sure to call
           `write_fragment_table'.
         [ARG: url]
           If NULL or an empty string, the fragment refers to a location
           in the current file -- the main JPX file.  In practice, this is
           unlikely to be possible unless you have written your own
           media data (mdat) box and buried the codestream inside it --
           a trick used commonly by Motion JPEG2000 files.
      */
    KDU_AUX_EXPORT void write_fragment_table();
      /* [SYNOPSIS]
           Use this function to write the fragment table box which records
           any information supplied via previous calls to
           `add_fragment' -- or fragments explicitly added to the fragment
           list object returned by `access_fragment_list'.  You may do this
           at any point where you would normally write a JPEG2000
           contiguous codestream via the interface returned by `open_stream'.
      */
    KDU_AUX_EXPORT jp2_output_box *open_stream();
      /* [SYNOPSIS]
           Returns a pointer to an internal `jp2_output_box' object to
           which you should write the codestream data itself.  This is
           normally accomplished by passing the resulting pointer to
           `kdu_codestream::create', but you may also write to the
           box directly (e.g., to copy a codestream from one file to
           another).
           [//]
           You may call this function only once, and you must close the
           returned box before attempting to open any other codestream
           box.  Furthermore, you must call `jpx_target::write_headers'
           first to generate the header information, after which you
           must open, write and close each codestream box in order.  If
           your application violates any of these conditions, an error
           will be delivered through `kdu_error'.
           [//]
           In many cases, you may wish to create a `kdu_codestream' object
           before the codestream box can be opened -- i.e., before the
           JPX file header can be written.  In particular, it is often
           convenient to use an open `kdu_codestream' to initialize the
           `jp2_dimensions' object.  The `access_stream' function enables
           you to obtain the interface required to create a `kdu_codestream'
           object prior to the point at which the present function is called,
           but note that you MUST at least call `open_stream' before making
           any attempt to flush codestream data with `kdu_codestream::flush'.
      */
    KDU_AUX_EXPORT kdu_compressed_target *access_stream();
      /* [SYNOPSIS]
           Returns a pointer to the same internal `jp2_output_box' object which
           will be returned by `open_stream', but does not actually open any
           box.  This function may be called at any time at all, enabling
           you to pass the returned interface to `kdu_codestream::create'
           before opening an output box.  This can be quite important, since
           it allows you to use the created `kdu_codestream' object to
           initialize `jp2_dimensions' and even to start compressing the
           imagery before actually writing the JPX file header.  Note,
           however, that you will have to call `open_stream' at some point
           before invoking `kdu_codestream::flush'.
         [RETURNS]
           Pointer to the internal `jp2_output_box' object, which is
           deliberately cast to the base of its derivation hierarchy
           (`kdu_compressed_target') so as to discourage you from using it
           for anything other than passing to `kdu_codestream::create'.
           Note that the returned object cannot be written to until after
           `open_stream' has been called, but this does not stop you from
           passing it to `kdu_codestream::create'.
      */
  private: // State
    jx_codestream_target *state;
  };

/*****************************************************************************/
/*                             jpx_layer_source                              */
/*****************************************************************************/

class jpx_layer_source {
  /* [BIND: interface]
     [SYNOPSIS]
       Manages a single compositing layer within a JPX file.  Specifically,
       this object manages the layer-specific attributes which are found
       either in a compositing layer header (jplh) box or the global
       JP2 header box.  If the JPX file contains no compositing layer header
       boxes, the first codestream will be assigned its own layer,
       using the default information recorded in the JP2 header box to
       deduce the relevant colour space and channel mapping rules.
       [//]
       Objects of this class are merely interfaces to an internal
       object, which cannot be directly created by an application.
       Use `jpx_source::access_layer' to obtain a non-empty interface.
  */
  public: // Member functions
    jpx_layer_source() { state = NULL; }
    jpx_layer_source(jx_layer_source *state) { this->state = state; }
      /* [SYNOPSIS]
           Do not call this function yourself.  Use
           `jpx_source::access_layer' to obtain a non-empty interface.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns false if the interface is empty, meaning that it is not
           currently associated with any internal implementation.  See the
           `jpx_layer_source' overview for more on how to get access to a
           non-empty interface.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS] Opposite of `exists', returning true if the interface is
         empty.
      */
    KDU_AUX_EXPORT int get_layer_id();
      /* [SYNOPSIS]
           Returns the sequence number of this compositing layer.  If the
           JPX file contains compositing layer header (jplh) boxes, the
           index corresponds directly to the order in which those boxes
           are found within the file, starting from 0.  Otherwise, the first
           codestream is understood to represent a compositing layer (with
           index 0) and there will be no others.
      */
    KDU_AUX_EXPORT jp2_locator get_header_loc();
      /* [SYNOPSIS]
           Use this function to obtain a `jp2_locator' object which describes
           the location of the compositing layer header (jplh) box for this
           codestream.  If there is no such box, the returned object's
           `jp2_locator::is_null' member will return true.
      */
    KDU_AUX_EXPORT jp2_channels access_channels();
      /* [SYNOPSIS]
           Returns an object which may be used to access information
           recorded in relevant "Component Mapping" and "Channel Definition"
           boxes.  The information from such boxes is merged into a uniform
           set of channel mapping rules, accessed through the returned
           `jp2_channels' object.
      */
    KDU_AUX_EXPORT jp2_resolution access_resolution();
      /* [SYNOPSIS]
           Returns an object which may be used to access aspect-ratio,
           capture and suggested display resolution information, for
           assistance in some rendering applications.
      */
    KDU_AUX_EXPORT jp2_colour access_colour(int which);
      /* [SYNOPSIS]
           Returns an object which may be used to access information
           recorded in a Colour Description (colr) box, which indicates the
           interpretation of colour image data for rendering purposes.  The
           returned `jp2_colour' object also provides convenient colour
           transformation functions, to convert data which uses a custom
           ICC profile into one of the standard rendering spaces.
         [RETURNS]
           An empty interface (one whose `exists' function returns false) if
           `which' is greater than or equal to the number of colour
           description boxes in the JPX file which may be applied to this
           compositing layer.
         [ARG: which]
           If this argument is 0, the returned object will be an interface
           to the information in the first Colour Description (colr) box in
           the JPX file which is relevant to this compositing layer.  If there
           are more than one such Colour Description boxes a value of `which'=1
           will cause an interface to the information in the second box to
           be returned, and so forth.  If `which' is greater than or equal
           to the number of relevant colour description boxes, the function
           will return an empty interface (one whose `exists' function returns
           false).
      */
    KDU_AUX_EXPORT int get_num_codestreams();
      /* [SYNOPSIS]
           Returns the total number of codestreams which are used by this
           compositing layer.  Typically there will be only one codestream
           associated with each layer, but multiple codestreams are possible.
      */
    KDU_AUX_EXPORT int get_codestream_id(int which);
      /* [SYNOPSIS]
           If `which' is 0, the function returns the identifier of the first
           codestream used by this compositing layer.  If `which' is 1, the
           function returns the identifier of the second codestream used
           by the compositing layer, and so forth.  If `which' is greater than
           or equal to the value returned by `get_num_codestreams', the
           present function will return -1.  Otherwise, the identifier
           returned by this function may be passed to
           `jpx_source::access_codestream' to gain access to the
           codestream itself.
           [//]
           Any codestream identified here can be opened immediately, even
           if the ultimate data source is a dynamic cache derived from
           `kdu_cache'.  This is because a non-empty `jpx_layer_source'
           interface will never be returned by `jpx_source::access_layer'
           until every one of its codestreams has become available for
           opening.  Moreover, as documented in connection with
           `jpx_codestream_source::open_stream', for this to happen, the
           embedded code-stream's main header must at least be fully
           available from a local file or from the dynamic cache.
      */
    KDU_AUX_EXPORT kdu_coords get_layer_size();
      /* [SYNOPSIS]
           Returns the dimensions of this compositing layer, as they appear
           on its registration grid.  In the absence of any codestream
           registration (creg) box, there is only one code-stream in each
           compositing layer and the dimensions of the layer are those
           of the image region occupied by that code-stream on its high
           resolution canvas.  Where a codestream registration box exists,
           there may be multiple code-streams, each of which has its
           high resolution grid adjusted by the sampling factors and offsets
           returned via `get_codestream_registration'.  In this case, the
           layer size is determined to be the intersection of the resampled
           regions associated with each codestream, rounded outwards to a
           whole number of resampled pixels, as described by the JPX
           standard (accounting for corrigenda).
      */
    KDU_AUX_EXPORT bool have_stream_headers();
      /* [SYNOPSIS]
           Returns true if the codestream main headers for all codestreams
           associated with this compositing layer are currently available.
           The return value might be false only if the source is ultimately
           fueled by a dynamic cache, some of whose main header data-bins
           are not yet completed.  See `jpx_source::access_layer' for
           more on the conditions which can allow a compositing layer
           interface to be obtained for which some codestream main headers
           might not yet be available.
      */
    KDU_AUX_EXPORT int
      get_codestream_registration(int which, kdu_coords &alignment,
                                  kdu_coords &sampling,
                                  kdu_coords &denominator);
      /* [SYNOPSIS]
           This function plays a similar role to `get_codestream_id' and in
           fact it has the same return value.  However, it returns additional
           information about the alignment and scaling of multiple
           codestreams used by this compositing layer against one another.
         [RETURNS]
           Identifier of the codestream enumerated via `which'.  Codestream
           indices start from 0 and correspond to the order of appearance
           of the codestream within the data source.  Returns -1 if the
           number of codestreams used by this compositing layer is less than
           or equal to `which'.
         [ARG: which]
           0 to ask about the first codestream used by this compositing layer;
           1 to ask about the second codestream used by the layer; and so
           forth.
         [ARG: alignment]
           Used to return horizontal and vertical alignment offsets.
           Specifically, the upper left hand sample of the code-stream's
           image region is located `alignment.x'/`denominator.x' samples
           to the right and `alignment.y'/`denominator.y' samples below
           the origin of this compositing layer's reference grid.  The
           JPX standard (accounting for corrigenda) requires these
           alignment offsets to lie in the half open interval [0,1), which
           means that 0 <= `alignment.x' < `denominator.x' and
           0 <= `alignment.y' < `denominator.y'.
         [ARG: sampling]
           Used to return horizontal and vertical sampling factors for the
           codestream.  Specifically, adjacent columns on the codestream's
           high resolution reference grid are separated by
           `sampling.x'/`denominator.x' samples on this compositing layer's
           reference grid.  Similarly, adjacent rows on the codestream's
           high resolution reference grid are separated by
           `sampling.y'/`denominator.y' samples on the compositing layer
           reference grid.
         [ARG: denominator]
           Used to return the denominator for the `alignment' and `sampling'
           expressions.  The values of `denominator.x' and `denominator.y' are
           guaranteed to be strictly positive.  Each compositing layer is
           guaranteed to have a single unique denominator, so that this
           argument will be set to the same values no matter what value is
           supplied for `which'.  This is true even if the value of `which'
           does not correspond to a valid codestream.
      */
  private: // State
    jx_layer_source *state;
  };

/*****************************************************************************/
/*                             jpx_layer_target                              */
/*****************************************************************************/

class jpx_layer_target {
  /* [BIND: interface]
     [SYNOPSIS]
       Manages a single compositing layer within a JPX file.  Specifically,
       this object manages the layer-specific attributes which will be
       recorded either in a compositing layer header (jplh) box or the global
       JP2 header box.
       [//]
       Objects of this class are merely interfaces to an internal
       object, which cannot be directly created by an application.
       Use `jpx_target::add_layer' to obtain a non-empty interface.
  */
  public: // Member functions
    jpx_layer_target() { state = NULL; }
    jpx_layer_target(jx_layer_target *state) { this->state = state; }
      /* [SYNOPSIS]
           Do not call this function yourself.  Use
           `jpx_target::add_layer' to obtain a non-empty interface.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns false if the interface is empty, meaning that it is not
           currently associated with any internal implementation.  See the
           `jpx_layer_target' overview for more on how to get access to a
           non-empty interface.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS] Opposite of `exists', returning true if the interface is
         empty.
      */
    KDU_AUX_EXPORT jp2_channels access_channels();
      /* [SYNOPSIS]
           Provides an interface which may be used to specify the
           relationship between code-stream image components and colour
           reproduction channels (colour intensity channels, opacity
           channels, and pre-multiplied opacity channels).  This information
           is used to construct appropriate Component Mapping and
           Channel Definitions boxes, but is provided in a manner which
           conceals most of the mess associated with those boxes.
           [//]
           It is NOT NECESSARY to access or initialize the `jp2_channels'
           object directly; the default behaviour is to assign colour
           intensity channels to the initial code-stream image components
           in order.
      */
    KDU_AUX_EXPORT jp2_resolution access_resolution();
      /* [SYNOPSIS]
           Provides an interface which may be used to specify the
           aspect ratio and physical resolution of the resolution canvas
           grid for a single codestream used by this compositing layer, or
           (if the layer uses multiple codestreams) of the compositing
           layer reference grid.
           [//]
           It is NOT NECESSARY to access or initialize the `jp2_resolution'
           object.
      */
    KDU_AUX_EXPORT jp2_colour add_colour(int prec=0, kdu_byte approx=0);
      /* [SYNOPSIS]
           Provides an interface which may be used for setting up the
           information in a Color Description (colr) box.  You may call
           this function multiple times to provide multiple colour
           descritions for a single compositing layer.
           [//]
           You ARE REQUIRED to add at least one colour box, and complete its
           initialization before calling `jpx_target::write_headers'.
         [ARG: prec]
           For an explanation of precedence, see the comments appearing with
           `jpx_colour::get_precedence'.
         [ARG: approx]
           For an explanation of approximation levels, see the comments
           appearing with `jpx_colour::get_approximation_level'.
      */
    KDU_AUX_EXPORT jp2_colour access_colour(int which);
      /* [SYNOPSIS]
           Returns interfaces to the same objects created by `add_colour'.
         [RETURNS]
           An empty interface (one whose `exists' function returns false) if
           `which' is greater than or equal to the number of colour
           descriptions which have been added so far.
         [ARG: which]
           If this argument is 0, the returned object will be an interface
           to the information in the first added colour description.
      */
    KDU_AUX_EXPORT void
      set_codestream_registration(int codestream_id, kdu_coords alignment,
                                  kdu_coords sampling,
                                  kdu_coords denominator);
      /* [SYNOPSIS]
           Use this function to specify the alignment of multiple codestreams
           which are shared by this compositing layer.  If you do not
           specify the alignment for any codestream which is used by the
           channel mappings supplied via the interface returned by
           `access_channels', that codestream will be aligned directly with
           the compositing registation grid by default.  That is, the
           origin of the codestream's high resolution reference grid will
           be aligned with the origin of the compositing layer's registration
           grid, and the spacing between consecutive columns and rows on
           the codestream's reference grid will be exactly one column or
           row spacing (as appropriate) on the compositing layer's registration
           grid.  In this case, the dimensions of the composition layer will be
           the minima of the corresponding dimensions of each codestream.
         [ARG: codestream_id]
           This should be one of the codestream ID's used to specify channel
           mapping rules via the `jp2_channels' interface functions.
           Codestream identifiers start from 0 and correspond exactly with
           the order in which the codestream will appear in the JPX file,
           which is also the order in which the `jpx_codestream_target'
           interfaces were created using `jpx_target::add_codestream'.
         [ARG: alignment]
           Supplies horizontal and vertical alignment offsets.
           Specifically, the upper left hand sample of the code-stream's
           image region is located `alignment.x'/`denominator.x' samples
           to the right and `alignment.y'/`denominator.y' samples below
           the origin of this compositing layer's reference grid.  The
           JPX standard (accounting for corrigenda) requires these
           alignment offsets to lie in the half open interval [0,1), which
           means you must ensure that 0 <= `alignment.x' < `denominator.x'
           and 0 <= `alignment.y' < `denominator.y'.
           [//]
           Note that `alignment' values must also lie in the range 0 to 255.
         [ARG: sampling]
           Supplies horizontal and vertical sampling factors for the
           codestream.  Specifically, adjacent columns on the codestream's
           high resolution reference grid are separated by
           `sampling.x'/`denominator.x' samples on this compositing layer's
           reference grid.  Similarly, adjacent rows on the codestream's
           high resolution reference grid are separated by
           `sampling.y'/`denominator.y' samples on the compositing layer
           reference grid.
           [//]
           Note that `sampling' factors must lie in the range 1 to 255.
         [ARG: denominator]
           Supplies the denominator for the `alignment' and `sampling'
           expressions.  The values of `denominator.x' and `denominator.y'
           must be strictly positive.  Each compositing layer may have only
           one unique denominator, so calling this function multiple times
           with different denominator values will cause an error to be
           generated through `kdu_error'.
      */
    KDU_AUX_EXPORT void set_breakpoint(int i_param, void *addr_param);
      /* [SYNOPSIS]
           If you wish to record any specific boxes within the compositing
           layer header box, over and above the Channel Definitions (cdef),
           Colour Group (cgrp), Resolution (res ) and Codestream Registration
           (creg) boxes which are generated automatically, you should install
           a breakpoint.  Doing this will ensure firstly that a Compositing
           Layer Header (jplh) box is always written.  Secondly, the call to
           `jpx_target::write_headers' will return prematurely, providing you
           with an opportunity to write the additional boxes, after which
           `jpx_target::write_headers' must be called again to resume the
           header generation process.
         [ARG: i_param]
           This is an arbitrary integer identifier which will be returned by
           `jpx_target::write_headers'.  You might use this to provide an
           interpretation for the `addr_param' value, allowing you to reliably
           cast it to some appropriate object reference.  Alternatively, you
           might use either or neither of the two parameters.
         [ARG: addr_param]
           This is an arbitrary address which will be returned by
           `jpx_target::write_headers'.
      */
  private: // State
    jx_layer_target *state;
  };

/*****************************************************************************/
/*                                 jpx_roi                                   */
/*****************************************************************************/

struct jpx_roi {
  /* [BIND: copy]
     [SYNOPSIS]
       This structure manages a single region of interest, as it appears within
       a JPX ROI Descriptions (`roid') box.  It helps keep information
       about the image region together with additional attributes of the
       region.
  */
  public: // Functions
    jpx_roi() { is_elliptical=is_encoded=false; coding_priority=0; }
      /* [SYNOPSIS]
           Initializes the object to represent a rectangular region which
           does not necessarily correspond to an encoded ROI within any
           code-stream.  That is, `is_elliptical' and `is_encoded' are
           both set to false.
      */
  public: // Data
    kdu_dims region;
      /* [SYNOPSIS]
           For rectangular regions (`is_elliptical'=false) this member holds
           the actual region of interest.  Otherwise, it describes the
           bounding box of an elliptical region of interest whose major
           and minor axes are aligned with the horizontal and vertical
           cardinal axes.
           [//]
           Note that regions are defined only for code-streams, not any
           other image entities which may be found within a JPX file such
           as compositing layers or a composited image.
           [//]
           Note also that the region is expressed relative to the upper
           left hand corner of the image as it appears on the relevant
           code-stream's high resolution grid.
      */
    bool is_elliptical;
      /* [SYNOPSIS]
           If true, the `region' member identifes the bounding box of an
           elliptical region whose maxor and minor axes correspond to the
           width and height (or vice-versa) of that bounding box.
      */
    bool is_encoded;
      /* [SYNOPSIS]
           If true, the region described here has actually been encoded
           as a prioritized ROI within the code-streams with which it is
           associated.  JPEG2000 Part 1 code-streams may encode ROI's
           only using the MAXSHIFT method, also known as the implicit
           method.  JPEG2000 Part 2 code-streams may use explicit ROI
           coding.  If this member is false, the ROI is a semantic entity,
           described only at the level of the file format.
      */
    kdu_byte coding_priority;
      /* [SYNOPSIS]
           Indicates the coding priority for use in transcoding images which
           contain code-stream ROI's -- i.e., ROI's for which `is_encoded'
           is true.
      */
  };

/*****************************************************************************/
/*                               jpx_metanode                                */
/*****************************************************************************/

class jpx_metanode {
  /* [BIND: interface]
     [SYNOPSIS]
       This object is used to interact with an internal metadata tree,
       which is created to reflect the associations between metadata in
       a JPX file.  By metadata, here we mean all of the box types
       described in connection with the `jpx_meta_manager::set_box_filter'
       function, plus any association (`asoc') boxes and the first sub-box
       of any association box.  Boxes which are not represented by this
       object are those which are directly related to rendering the
       compressed imagery.  Typical examples of boxes managed by the object
       are XML boxes, number list boxes, label boxes, ROI description boxes,
       UUID boxes and IP-rights boxes.
       [//]
       Each intermediate node in the tree (i.e. everything other than the root
       and the leaves) corresponds to an association relationship, which could
       be represented using a JPX association (`asoc') box.  In some cases,
       the relationship might be represented by some other means, such
       as inclusion of the relevant metadata within a codestream header or
       compositing layer header box, but it is easiest to understand the
       tree structure by assuming that each intermediate node corresponds
       to exactly one association box.  The first sub-box of an association
       box plays a special role, describing an entity to which all subsequent
       sub-boxes are to be associated.  These subsequent sub-boxes are
       represented by the node's descendants (branches in the tree), while
       the contents of the first box are considered to belong to the node
       itself.
       [//]
       In cases where the first sub-box of an association is itself an
       association box, it is unclear exactly what association was
       intended.  For this reason, we do not include the first sub-box of
       an association amongst the descendants of a node.  All other sub-boxes
       are treated as descendants (branches) whose nodes may be further
       split if they contain association boxes.
       [//]
       There are two important cases for the first sub-box of an association
       box, as follows:
         [>>] If the first sub-box of an association is a number list (`nlst')
              box, all descendants of the node are associated with the image
              entities listed in the number list box.  These entities may be
              codestreams, compositing layers, or the entire composited image.
         [>>] If the first sub-box of an association is an ROI description
              (`roid') box, all descendants of the node are associated with
              the rectangular and/or elliptical regions described by that
              box.
       [//]
       The above two special cases are represented explicitly and the
       association parameters may be explicitly created and retrieved
       via the various member functions offered by the `jpx_metanode'
       interface.
       [//]
       As mentioned previously, the associations embodied by an
       intermediate node in the metadata tree might not necessarily arise
       from the appearance of an `asoc' box within the JPX file.  In
       particular, wherever any of the metadata boxes mentioned above appears
       within a codestream header or a compositing layer header box, the
       metadata will be treated as though it had been contained within an
       `asoc' box whose first sub-box was a number list box which referenced
       the relevant codestream or compositing layer.  When the information
       in a `jpx_metanode' tree is used to write a new JPX file, however,
       all associations will be formed using `asoc' boxes, rather than by
       inclusion within codestream header or compositing layer header boxes.
       [//]
       Leaves in the tree have no descendants.  A leaf node represents a
       single JP2 box, which is not further divided through the use of
       `asoc' boxes.
       [//]
       In the above, we have described intermediate nodes and leaf nodes.
       The one additional type of node is the tree root, to which access
       is granted using `jpx_meta_manager::access_root'.  All descendants
       of the root node correspond to metadata which (at least notionally)
       resides at the top level of the file.  Whereas intermediate nodes
       have their own box, which corresponds to the first sub-box of an
       association box (whether real or implied), the root node has no
       box of its own; it has only descendants and its box-type is
       returned as 0.
       [//]
       The `jpx_metanode' interface is designed to meet the needs of file
       readers and file writers, including incremental file parsers, which
       may be accessing a JPX data source which is being supplied by a
       dynamic cache whose contents are written in some arbitrary order by
       a remote JPIP server.  Both reading and writing functions may be
       used together to interactively edit an existing metadata tree.  For
       this reason, provision is made to read boxes which are described by
       any of the methods allowed during writing, including boxes which
       are described by reference to an existing node (possibly in another
       metadata tree) and boxes which are described by placeholders which
       are used to defer box generation until the file is actually being
       written.
       [//]
       For the added convenience of incremental readers, editors and other
       interesting applications which may interact with JP2/JPX metadata,
       the `jpx_meta_manager' object provides methods for retrieving metanodes
       based on the file address of their constituent boxes, for enumerating
       metanodes based on their association with image entities and/or
       regions of interest, and for efficiently scanning through metanodes
       which have been recently parsed from the data source or changed
       by editing operations.  Furthermore, the `jpx_metanode' interface
       provides methods for identifying whether a node has been changed
       by editing operations and for saving an application-defined pointer
       (usually a pointer into some data structure maintained by the
       application).  Together, these features can be used to efficiently
       track changes in the metadata structure due to editing or incremental
       reading of a data source (typically, as it becomes available from a
       dynamic cache).
       [//]
       Note carefully that objects of the `jpx_metanode' class are merely
       interfaces to an internal object, which cannot be directly created
       by an application.  Use the member functions offered by
       `jpx_meta_manager' to obtain a non-empty interface.
  */
  //---------------------------------------------------------------------------
  public: // Interface management functions
    jpx_metanode() { state = NULL; }
    jpx_metanode(jx_metanode *state) { this->state = state; }
      /* [SYNOPSIS]
           Do not call this function yourself.  Use the member functions
           provided by `jpx_meta_manager' to obtain a non-empty interface.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns false if the interface is empty, meaning that it is not
           currently associated with any internal implementation.  See the
           `jpx_metanode' overview for more on how to get access to a
           non-empty interface.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS] Opposite of `exists', returning true if the interface is
         empty.
      */
    bool operator==(const jpx_metanode &node) { return (state == node.state); }
      /* [SYNOPSIS]
           Returns true if the internal objects referenced by the two
           interfaces are identical.
      */
    bool operator!=(const jpx_metanode &node) { return (state != node.state); }
      /* [SYNOPSIS]
           Returns false if the internal objects referenced by the two
           interfaces are identical.
      */
  //---------------------------------------------------------------------------
  public: // Information retrieval functions
    KDU_AUX_EXPORT bool
      get_numlist_info(int &num_codestreams, int &num_layers,
                       bool &applies_to_rendered_result);
      /* [SYNOPSIS]
           Use this function to determine whether or not the node is
           represented by a number list.  A number list is a list of
           image entities: codestreams; compositing layers; or the
           complete "rendered result".  In many cases, the number list
           information is derived from a JPX number list (`nlst') box.
           However, it can also be derived by the containment of metadata
           within a JPX codestream header box or compositing layer header
           box.  The ensuing discussion assumes that the function returns
           true, meaning that the node is represented by a number list.
           [//]
           If this is an intermediate node, the number list is at least
           notionally the first sub-box of an association box, and all of
           the node's descendants (both immediate and indirect descendants)
           are deemed to be associated with each and every one of the image
           entities in the number list.
           [//]
           As mentioned above, it is also possible that an intermediate node
           is represented by a number list, without the appearance of an
           actual number list box within the JPX data source.  This happens
           if relevant metadata is encountered while parsing a JPX codestream
           header box or a JPX compositing layer header box.  In this case,
           a "virtual" number list is created, to refer to exactly one
           image entity -- the codestream or compositing layer, whose header
           generated the node.  The node's descendants then represent
           the metadata sub-boxes found within the relevant header.  It is
           important to realize that this kind of "virtual" number list
           can always be written into a JPX file as a real number list box
           within an association box, and indeed the JPX file writer always
           writes metadata in this way, rather than encapsulating it within
           header boxes.
           [//]
           If this node is a leaf, the number list corresponds to an
           isolated number list (`nlst') box, all by itself.  This is
           semantically less useful, of course, but still legal.
           [//]
           In addition to identifying whether or not the node is represented
           by a number list, this function also provides information
           concerning the number of codestreams and compositing layers which
           are referenced by the number list, and whether or not the
           number list references the complete "rendered result".
         [RETURNS]
           True only if the box is represented by a number list.
         [ARG: num_codestreams]
           Used to return the number of image entities in the number list
           which are codestreams.
         [ARG: num_layers]
           Used to return the number of image entities in the number list
           which are compositing layers.
         [ARG: applies_to_rendered_result]
           Used to return a boolean variable, which evaluates to true if the
           number list includes a reference to the complete "rendered result"
           of the JPX data source.  The variable is set to false if the number
           list contains references only to codestreams and/or compositing
           layers.  The actual meaning of the "rendered result" is unclear
           from the JPX specification, but it may refer to any of the
           results produced by applying the compositing instructions in a
           composition (`comp') box, if one is present.  Access to these
           compositing instructions is provided via the `jpx_composition'
           interface.
      */
    KDU_AUX_EXPORT const int *get_numlist_codestreams();
      /* [SYNOPSIS]
           Returns NULL unless `get_numlist_info' returned true.  In that
           event, the function returns a pointer to the internal array of
           codestream indices which are recorded in the number list.
           This could still be NULL, if there are no codestream indices
           referenced by the number list.  The number of entries in the
           array may be found using the `get_numlist_info' function.
           [//]
           Note that all codestream indices start from 0 for the first
           codestream in the JPX file.
      */
    KDU_AUX_EXPORT const int *get_numlist_layers();
      /* [SYNOPSIS]
           Returns NULL unless `get_numlist_info' returned true.  In that
           event, the function returns a pointer to the internal array of
           compositing layer indices which are recorded in the number list.
           This could still be NULL, if there are no compositing layer indices
           referenced by the number list.  The number of entries in the
           array may be found using the `get_numlist_info' function.
           [//]
           Note that all layer indices start from 0 for the first
           compositing layer in the JPX file.
      */
    KDU_AUX_EXPORT int get_numlist_codestream(int which);
      /* [SYNOPSIS]
           Use this to access a particular element of the internal array
           returned by `get_numlist_codestreams'.  If `which' is greater than
           or equal to the number of codestreams referenced by the number list,
           or if `get_numlist_info' returns false, this function returns -1.
      */
    KDU_AUX_EXPORT int get_numlist_layer(int which);
      /* [SYNOPSIS]
           Use this to access a particular element of the internal array
           returned by `get_numlist_layers'.  If `which' is greater than
           or equal to the number of compositing layers referenced by the
           number list, or if `get_numlist_info' returns false, this function
           returns -1.
      */
    KDU_AUX_EXPORT bool
      test_numlist(int codestream_idx, int compositing_layer_idx,
                   bool applies_to_rendered_result);
      /* [SYNOPSIS]
           THIS FUNCTION IS BEING DEPRECATED!
           It is not clear whether any application currently uses it, in
           preference o `get_numlist_info', `get_numlist_streams' and
           `get_numlist_layers'.  More significantly, the existence of
           this function suggests an interpretation of numlist associations
           as associations with image entities which satisfy codestream,
           compositing layer and rendered result constraints.  However, it
           is unclear from the JPX file format whether this is a correct
           interpretation.  If a numlist identifies codestreams and
           compositing layers, for example, association with that numlist
           should probably be considered an association with any image
           entity which satisfies any one of the codestream or compositing
           layer constraints.  Since the situation is not completely clear,
           and may be confused by this function, we prefer to mark the
           function as deprecated.  Avoid using it in new applications.
           [//]
           Returns true if `get_numlist' returns true AND the number list
           contains a reference to the codestream index supplied by
           `codestream_idx' (if non-negative), AND the number list contains
           a reference to the compositing layer index supplied by
           `compositing_layer_idx' (if non-negative), AND the number list
           contains a reference to the "rendered result" (if
           `applies_to_rendered_result' is true).
         [ARG: codestream_idx]
           If negative, this argument is ignored.  Otherwise, the number list
           must contain a reference to the indicated codestream index (indices
           start from 0) if the function is to return true.
         [ARG: compositing_layer_idx]
           If negative, this argument is ignored.  Otherwise, the number list
           must contain a reference to the indicated compositing layer index
           (indices start from 0) if the function is to return true.
         [ARG: applies_to_rendered_result]
           If true, the number list must contain a reference to the "rendered
           result" if the function is to return true.  As explained in
           connection with the `get_numlist_info' function, it might not
           always be entirely clear what the "rendered result" is.
      */
    KDU_AUX_EXPORT int get_num_regions();
      /* [SYNOPSIS]
           Use this function to determine whether or not the node is
           represented by an ROI description box.  If it is, the function
           returns the number of rectangular or elliptical regions in the
           ROI description box; otherwise, it returns 0.  The remainder of
           the discussion here assumes that the function returns a value
           greater than 0.
           [//]
           If this is an intermediate node, the ROI description box is the
           first sub-box of an association box, and all of the node's
           descendants (both immediate and indirect descendants) are deemed
           to be associated with the set of regions described by the ROI
           description box.  Although this is the most useful case, it can
           happen that the node is a leaf node, involving no association
           box and no descendants.
      */
    KDU_AUX_EXPORT const jpx_roi *get_regions();
      /* [BIND: no-java]
         [SYNOPSIS]
           Returns a pointer to the internal array of regions (and their
           attributes) associatd with an ROI description box.
         [RETURNS]
           Non-NULL only if the node represents an ROI description box;
           i.e., if `get_num_regions' returns non-zero.
      */
    KDU_AUX_EXPORT jpx_roi get_region(int which);
      /* [SYNOPSIS]
           Use this to access a particular element of the internal array
           returned by `get_regions'.  If `which' is greater than
           or equal to the number of regions returned by `get_num_regions',
           this function returns an empty region.
      */
    KDU_AUX_EXPORT kdu_dims get_bounding_box();
      /* [SYNOPSIS]
           Returns an empty region if `get_num_regions' returns 0.  Otherwise,
           this function returns the smallest rectangular region which contains
           all the regions in the ROI description box.
      */
    KDU_AUX_EXPORT bool test_region(kdu_dims region);
      /* [SYNOPSIS]
           Returns true if the node represents an ROI description box
           (i.e., if `get_num_regions' returns non-zero) AND if one or more
           of the regions in the ROI description box intersects with the
           supplied `region'.
      */
    KDU_AUX_EXPORT kdu_uint32 get_box_type();
      /* [SYNOPSIS]
           Returns the type code of the box associated with this node.
           For the root node, this function will always return 0.  In the
           event that this node was created to manage metadata found within
           a codestream header box, or a compositing layer header box, the
           returned box type will be `jp2_number_list_4cc' even though there
           was no actual number list box in the source.
      */
    KDU_AUX_EXPORT const char *get_label();
      /* [SYNOPSIS]
           Returns non-NULL if and only if `get_box_type' returns
           `jp2_label_4cc' -- i.e., the `lbl ' box type code.  The body of
           all label boxes is stored explicitly by the internal machinery and
           the null-terminated text string is returned here.
           [//]
           Both intermediate and leaf nodes may be represented by labels.
           If an intermediate node is represented by a label, all of the
           node's descendants are semantically associated with the label.
      */
    KDU_AUX_EXPORT bool get_uuid(kdu_byte uuid[]);
      /* [SYNOPSIS]
           Returns true only if `get_box_type' returns `jp2_uuid_4cc' -- i.e.,
           the `uuid' box type code -- and the box was read from a JP2/JPX
           source.  The first 16 bytes of UUID boxes are stored explicitly
           by the internal machinery and returned here via the `uuid' argument,
           which must be capable of holding 16 entries.  If you need to
           access the entire box, use `get_existing' to get the coordinates
           which must be supplied to `jp2_input_box::open'.
      */
    KDU_AUX_EXPORT jp2_locator get_existing(jp2_family_src * &src);
      /* [SYNOPSIS]
           Provides the location of the box associated with this node.  For
           intermediate nodes, this is the first sub-box of the association
           box, whose type is returned via `get_box_type'.  The location of
           the association box itself will not be returned by any member
           function here.
           [//]
           For the root node, and for intermediate nodes which are created
           to represent metadata contained within codestream header and
           compositing layer header boxes, this function returns a null
           locator (one whose `jp2_locator::is_null' function returns true).
           [//]
           The function also returns a null locator if the present node
           has been created using one of the `add_...' interfaces, rather
           than by parsing an input JPX data source.
         [ARG: src]
           Used to return a pointer to the ultimate data source within which
           the box resides.
      */
    bool open_existing(jp2_input_box &box)
      {
      /* [SYNOPSIS]
           This function uses `get_existing' to identify the `jp2_family_src'
           object and location within that source of the box associated
           with the current node.  If `get_existing' returns a non-empty
           location, the function uses it to open the supplied `box' and
           return true.  Otherwise, the function simply returns false.
      */
        jp2_family_src *src;  jp2_locator loc;
        if (!(loc = get_existing(src))) return false;
        box.open(src,loc); return true;
      }
    KDU_AUX_EXPORT bool get_delayed(int &i_param, void * &addr_param);
      /* [SYNOPSIS]
           Returns true if this box was created using `add_delayed', rather
           than by parsing a JPX data source.
         [ARG: i_param]
           Used to return the integer parameter supplied to `add_delayed'.
         [ARG: addr_param]
           Used to return the address parameter supplied to `add_delayed'.
      */
    KDU_AUX_EXPORT bool count_descendants(int &count);
      /* [SYNOPSIS]
           If the number of descendants from this node is already
           known, this function returns true, writing that number into `count'.
           The number may not be known only if the ultimate source of
           information for a JPX data source is a dynamic cache whose
           contents have not been fully written (say by a remote JPIP
           server).  In this case, the function attempts to parse as far
           as it can into the super-box whose contents define the descendants
           of this node (the super-box is either an `asoc' box, or a
           codestream or compositing layer header box).  If it encounters
           the end of the relevant super-box, the function again returns
           true, writing the number of descendants into `count'.
           [//]
           If the function is unable to parse to a point at which the number
           of descendants can be known, it returns false, writing the
           number of descendants encountered up to that point into the
           `count' argument.
           [//]
           Note that leaf nodes never have descendants, so this function
           will always return true with `count' set to 0.
         [RETURNS]
           False if it is possible that more descendants remain in the
           underlying JPX data source, but the function is unable to parse
           any further into the source at this point.  This generally means
           that the underlying `jp2_family_src' object is fueled by a
           dynamic cache (i.e., a `kdu_cache' object).
      */
    KDU_AUX_EXPORT jpx_metanode get_descendant(int which);
      /* [SYNOPSIS]
           Use this function to access each descendant of an intermediate
           node.  If `which' is greater than or equal to the number of
           descendants, the function returns an empty interface, but note
           that this condition may be temporary -- see
           `count_descendants' for more on this.
      */
    KDU_AUX_EXPORT jpx_metanode get_parent();
      /* [SYNOPSIS]
           Returns a reference to the intermediate node from which the
           present node descends.  If the present node is the root of the
           metadata tree, the function returns an empty interface.
      */
    KDU_AUX_EXPORT bool change_parent(jpx_metanode new_parent);
      /* [SYNOPSIS]
           Removes the node from its current parent (decrementing its
           descendant count) and adds it as a descendant of the supplied
           `new_parent' node.  If this removes a numlist node's last
           descendant, the numlist node is automatically deleted.
         [RETURNS]
           False if no change is made.  This happens if `new_parent' is
           already a descendant of the current node or the node's
           current parent.
      */
    KDU_AUX_EXPORT jpx_metanode
      add_numlist(int num_codestreams, const int *codestream_indices,
                  int num_compositing_layers, const int *layer_indices,
                  bool applies_to_rendered_result);
      /* [SYNOPSIS]
           Use this function to add a descendant to the present node, assigning
           this descendant to represent a number list box.  If this
           node is assigned its own descendants, it will be represented by
           an association box, whose first sub-box is the number list box;
           in this, the most useful case, the node's descendants will all
           be associate with the image entities identified via the function's
           arguments.  If no descendants are added to the new node, it will be
           represented as an isolated number list box, all by itself.
         [RETURNS]
           Reference to the newly created node.
         [ARG: num_codestreams]
           Number of codestream indices to be referenced by the number list.
         [ARG: codestream_indices]
           Array with `num_codestreams' entries. holding the indices of the
           codestreams to be referenced by the number list.  This array is
           copied internally.
         [ARG: num_compositing_layers]
           Number of compositing layer indices to be referenced by the
           number list.
         [ARG: layer_indices]
           Array with `num_compositing_layers' entries. holding the indices of
           the compositing layers to be referenced by the number list.  This
           array is copied internally.
         [ARG: applies_to_rendered_result]
           True if the number list is to contain a reference to the "rendered
           result".  See `get_numlist_info' for a discussion of the meaning
           of the rendered result.
      */
    KDU_AUX_EXPORT jpx_metanode
      add_regions(int num_regions, const jpx_roi *regions);
      /* [SYNOPSIS]
           Use this function to add a descendant to the present node, assigning
           this descendant to represent an ROI description box.  If this
           node is assigned its own descendants, it will be represented by
           an association box, whose first sub-box is the ROI description
           box; in this, the most useful case, the node's descendants will all
           be associated with the image regions supplied via the `regions'
           array.  If no descendants are added to the new node, it will be
           represented as an isolated ROI description box all by itself.
         [RETURNS]
           Reference to the newly created node.
         [ARG: num_regions]
           Number of entries in the `regions' array.
      */
    KDU_AUX_EXPORT jpx_metanode
      add_label(const char *text);
      /* [SYNOPSIS]
           Use this function to add a descendant to the present node, assigning
           this descendant to represent a label (`lbl ') box.  If this node
           is assigned its own descendants, it will be represented by an
           association box, whose first sub-box is the label box; in this
           case, the node's descendants will all be associated with the text
           label.  Otherwise, the node represents an isolated label all by
           itself.  Both cases can be useful.
         [RETURNS]
           Reference to the newly created node.
         [ARG: text]
           Null-terminated text string to be stored in the body of the label
           box; the string is copied internally.
      */
    KDU_AUX_EXPORT void
      change_to_label(const char *text);
      /* [SYNOPSIS]
           Use this function to change the type of the current node to one
           which represents a label (`lbl ') box, if necessary, and replace
           its contents (if any) with the supplied label string.  If the
           node already has descendants, it is represented by an association
           box, whose first sub-box is now a label box.
         [ARG: text]
           Null-terminated text string to be stored in the body of the label
           box; the string is copied internally.
      */
    KDU_AUX_EXPORT jpx_metanode
      add_delayed(kdu_uint32 box_type, int i_param, void *addr_param);
      /* [SYNOPSIS]
           Use this function to add a descendant to the present node,
           assigning this descendant to represent a box of the indicated
           type, whose contents will not be provided until the metadata
           tree has been completed and `jpx_target::write_metadata' is
           invoked.  If the new node is assigned its own descendants, it
           will be represented by an association box, whose first sub-box
           is the box referenced here via `box_type' and the placeholder
           parameters.  More commonly, however, this function would be used
           to add leaf nodes, such as XML nodes, which might contain a lot
           of data.
         [RETURNS]
           Reference to the newly created node.
         [ARG: i_param]
           Application-defined integer parameter which will be returned via
           `jpx_target::write_metadata' when the time comes to write this
           box to the file.  You might use this to identify the type of
           object whose reference has been encoded via `addr_param'.
         [ARG: addr_param]
           Application-defined address which will be returned via
           `jpx_target::write_metadata' when the time comes to write this
           box to the file.
      */
    KDU_AUX_EXPORT void
      change_to_delayed(kdu_uint32 box_type, int i_param, void *addr_param);
      /* [SYNOPSIS]
           Use this function to change the type and/or delayed node
           identification parameters of the current node.  The operation is
           similar to deleting the existing node and then reinstantiating it
           using `add_delayed', except that the descendants and order within
           its sibling list are retained.
      */
    KDU_AUX_EXPORT jpx_metanode
      add_copy(jpx_metanode src, bool recursive);
      /* [SYNOPSIS]
           Use this function to add a descendant to the present node,
           assigning the descendant to represent a box whose type and
           contents are obtained by copying those of the `src' object.
           [//]
           Note carefully that `src' may be an interface to a different
           metadata tree.  Its contents might not be explicitly copied
           here; we might only preserve references.  As a result, you
           must make sure that the underlying `jp2_family_src' object,
           from which the contents of `src' might have been read, remains
           open until after the present box has been written using
           `jpx_target::write_metadata'.  However, it is safe to invoke
           `jpx_metanode::delete' on the `src' object after it has been
           copied.  This operation can be particularly useful during
           editing operations.
         [ARG: recursive]
           If true, the `src' node's descendants will be copied recursively.
           While doing so, the function will attempt to parse as many of
           the descendant boxes as possible -- if the source is fuelled
           ultimately by a dynamic cache, the descendants of an available
           node may grow, as more data arrives from a remote server.
      */
    KDU_AUX_EXPORT void delete_node();
      /* [SYNOPSIS]
           You may use this function during editing operations to delete
           an existing node and all of its descendants from the metadata
           tree.
           [//]
           Note that deletion only moves the node onto an internal
           "deleted list" and unlinks it from parent and descendants.  You
           can regain access to the node (e.g., via
           `jpx_meta_manager::locate_node') even after it has been deleted.
           Amongst other things, this allows you to test that it has been
           deleted by calling the `is_deleted' function.
      */
    KDU_AUX_EXPORT bool is_changed();
      /* [SYNOPSIS]
           Returns true if the node's contents have changed in any way, as
           a result of calls to `change_to_label', `change_to_delayed' or
           any other such editing function which might be provided in the
           future.
      */
    KDU_AUX_EXPORT bool ancestor_changed();
      /* [SYNOPSIS]
           Returns true if the node's parent or any of its ancestors have
           changed as a result of calls to `change_parent' or calls to
           `change_to_label' or `change_to_delayed' on any of the node's
           ancestors.
      */
    KDU_AUX_EXPORT bool is_deleted();
      /* [SYNOPSIS]
           Returns true if the node has been deleted as a result of a call to
           `delete_node'.
      */
    KDU_AUX_EXPORT void touch();
      /* [SYNOPSIS]
           This function provides a mechanism for forcing the node and
           all of its descendants to be appended to the internal list of
           touched nodes.  Nodes are also added to the list of touched nodes
           when they are first created, when they are changed, when any of
           their ancestors are changed, or when they are deleted.  For
           more information, see the description of
           `jpx_meta_manager::get_touched_nodes'.
           [//]
           Note that the touched list only ever contains metanodes whose
           underlying box is complete.  When reading from a dynamic cache,
           a box may have been encountered, but its contents not yet full
           available (for example, a label box might be encountered, but
           the cache does not yet contain the entire label).  When this
           happens, the box's metanode is created and made available, but
           it is not added to the touched list until the box has been
           completely read.  Touching metanodes like this will not have
           any effect.  Also, a metanode whose box has not yet been read
           will not yet have any available descendants, so no descendants
           can be touched either, even if their underlying boxes are
           available in the cache.
      */
    KDU_AUX_EXPORT void set_state_ref(void *ref);
      /* [SYNOPSIS]
           You can use this function to record an application-defined pointer
           with the metanode.  Typically, this would be used to help
           efficiently synchronize changes in the metadata structure with
           an application-specific representation of some or all of the
           metadata.  The supplied pointer survives editing and deleting of
           metadata nodes (since `delete_node' does not deallocate the
           memory itself).  However, these pointers are not copied by the
           `add_copy' function or by `jpx_meta_manager's copy functions.
      */
    KDU_AUX_EXPORT void *get_state_ref();
      /* [SYNOPSIS]
           Use this function to retrieve any application-defined pointer
           saved internally to the metanode by a previous call to
           `set_state_ref'.  Returns NULL if there is none.
      */
  private: // Data
    friend class jpx_meta_manager;
    jx_metanode *state;
  };

/*****************************************************************************/
/*                             jpx_meta_manager                              */
/*****************************************************************************/

class jpx_meta_manager {
  /* [BIND:interface]
     [SYNOPSIS]
       This object provides access to the metadata tree managed by the
       `jpx_metanode' interface.  For a thorough discussion of the structure
       and contents of metadata trees, refer to the extensive comments
       appearing with `jpx_metanode'.  You should review these concepts before
       trying to understand the ensuing text.  For more information on the
       exact box types which are represented by the metadata tree, see
       the `set_box_filter' function.
       [//]
       In addition to granting access to the metadata tree, the present
       object adds an additional layer of organization on the metadata,
       providing efficient access to boxes of interest and
       facilitating the creation of files whose metadata organization is
       amenable to efficient access in remote client-server applications.
       [//]
       This additional layer of organization consists of a number of
       linked lists.  Each node in the metadata tree may reside on at
       most one of these lists, although some nodes might not reside on
       any list.  These lists are not apparent to the application, but it
       is worth offering some explanation here so that you can appreciate
       what is going on.  The lists may be divided into three types,
       as described below.
       [//]
       A first type of list serves to link metadata nodes which are
       represented by an ROI description box.  This includes leaf nodes
       which contain an ROI description box, as well as intermediate nodes
       whose association box's first sub-box is an ROI description box.
       The common feature of all these nodes is that the corresponding
       `jpx_metanode::get_num_regions' function returns a non-zero number
       of image regions.
       [//]
       There may be many lists of this first type, which are managed
       using a scale-space structure.  The multi-scale aspects of the
       structure are generated by dividing the relevant nodes, i, on
       the basis of a dimension D_i.  D_i is the largest dimension of
       the smallest bounding box which contains all regions in the ROI
       description box.
         [>>] The multi-scale structure is based on a collection of thresholds,
              T0=0 < T1 < T2 < ... If D_i lies in the range T(k-1) < D_i <= Tk,
              then node i is associated with scale k.  The value of T1 is
              currently fixed at 8, but there is no need for the
              application ever to know what this value is and it might
              easily change in the future.
         [>>] Associated with each scale k, is a spatial partition whose
              dimensions are proportional to the scale threshold, Tk.
              Each element in the partition has its own list, into which the
              nodes whose upper left hand corner lies within that partition
              are inserted.
         [>>] To further improve access efficiency, the lists within each
              scale are further arranged into an hierarchical structure.
       [//]
       The second type of list serves to link all metadata nodes which are
       represented by a number list.  This includes leaf nodes which
       contain a number list box, intermediate nodes corresponding to
       association boxes whose first sub-box is a number list box, and
       intermediate nodes which serve to collect metadata found inside
       codestream or compositing layer header boxes.  The common feature
       of all these nodes is that the corresponding
       `jpx_metanode::get_numlist_info' function returns true.  There
       is currently exactly one of these lists.
       [//]
       A third type of list manages all nodes are not linked into or
       descended from any node which is linked into either of the list
       types mentioned above.  These nodes represent metadata which is
       not associated with any specific image entity or image region.
       Such metadata may be considered to describe properties of the
       file itself, rather than its imagery.  We do not expect to encounter
       too much of this type of metadata, so only one list is used.
       [//]
       Note that any node which is associated with a number list and/or
       an ROI description box, but which is not itself a number list or
       ROI description, will not be linked into any of the lists.
       [//]
       The lists described above have no impact on the way
       the metadata tree actually appears.  They serve only to improve
       the efficiency with which searches are made for metadata associated
       with a specific image region or image entity.  The relevant searches
       are facilitated by the `enumerate_matches' function.
       [//]
       The lists are also used when writing metadata to a JPX file.  The
       idea is that if the lists tend to improve the efficiency with which
       a file's metadata tree can be searched, then organizing the metadata
       within each list into a separate top level group of boxes in the
       file will also be beneficial. In particular, this will help reduce
       the number of top level boxes whose contents need to be expanded
       by a JPIP server when delivering metadata to a client -- each
       such box will typically be represented by a JPIP placeholder box.
       [//]
       It is worth knowing that Kakadu's JPX file writer builds these top
       level groupings by creating association boxes whose first sub-box
       is a free box.  Since the contents of a placeholder are associated
       only with the first sub-box, and that box has no semantics, this
       mechanism serves to create groups without any unintended semantic
       overtones.
       [//]
       It is also worth knowing that some nodes in the metadata tree may
       have descendants which reside on different lists and hence are
       included in different top level groups by the Kakadu JPX file
       writer.  Where this happens, the common boxes must be duplicated,
       either explicitly or by reference, which may create some redundancy
       in the file.
       [//]
       Note carefully that objects of the `jpx_meta_manager' class are merely
       interfaces to an internal object, which cannot be directly created
       by an application.  Use `jpx_target::access_meta_manager' or
       `jpx_source::access_meta_manager' to obtain a non-empty interface.
  */
  //---------------------------------------------------------------------------
  public: // Interface management functions
    jpx_meta_manager() { state = NULL; }
    jpx_meta_manager(jx_meta_manager *state) { this->state = state; }
      /* [SYNOPSIS]
           Do not call this function yourself.  Use
           `jpx_source::access_meta_manager' or
           `jpx_target::access_meta_manager' to obtain a non-empty interface.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns false if the interface is empty, meaning that it is not
           currently associated with any internal implementation.  See the
           `jpx_meta_manager' overview for more on how to get access to a
           non-empty interface.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS] Opposite of `exists', returning true if the interface is
         empty.
      */
  //---------------------------------------------------------------------------
  public: // General functions
    KDU_AUX_EXPORT void
      set_box_filter(int num_box_types, kdu_uint32 *box_types);
      /* [SYNOPSIS]
           This function may be used to control the box types which are
           included in the metadata tree as a JPX data source is being
           parsed.  By default, the following box types are included:
           [>>] label boxes (`lbl ')
           [>>] XML boxes (`xml ');
           [>>] IP-rights boxes (`jp2i');
           [>>] number list boxes (`nlst');
           [>>] ROI description boxes (`roid');
           [>>] UUID boxes (`uuid');
           [//]
           In addition, all association (`asoc') boxes, and the first
           sub-box of every association box, are automatically included.
           [//]
           You may change the default set of boxes listed above by providing
           an array of box types to be included.  Note that association boxes
           and their first sub-boxes are always included, regardless of the
           supplied filter.  Apart from this, boxes which do not match the
           filter and which are not otherwise required for rendering imagery,
           will be discarded.  The following box types are currently
           considered to be required for rendering imagery; including them
           in a box filter list will have no effect:
           [>>] JP2 signature (`jP  ');
           [>>] file-type (`ftyp');
           [>>] reader requirements (`rreq');
           [>>] JP2 header (`jp2h');
           [>>] codestream header (`jpch');
           [>>] compositing layer header (`jplh');
           [>>] JPX data references (`dtbl');
           [>>] cross reference (`cref');
           [>>] image header (`ihdr');
           [>>] bits per component (`bpcc');
           [>>] colour description (`colr');
           [>>] opacity (`opct');
           [>>] colour group (`cgrp');
           [>>] palette colour (`pclr');
           [>>] component mapping (`cmap');
           [>>] channel definition (`cdef');
           [>>] resolution (`res ');
           [>>] capture resolution (`resc');
           [>>] display resolution (`resd');
           [>>] codestream registration (`creg');
           [>>] desired reproductions (`drep');
           [>>] composition (`comp');
           [>>] composition options (`copt');
           [>>] composition instructions (`inst');
           [>>] contiguous codestream (`jp2c');
           [>>] fragment table (`ftbl');
           [>>] fragment list (`flst')
           [//]
           If you are going to call this function, you should do so right
           after opening the JPX data source, so as to avoid missing
           or keeping boxes you do not want.
           [//]
           Note that this function has no impact on which boxes you can
           explicitly add to a metadata tree (typically for writing a JPX
           file) via the `insert_node' function or the various interface
           functions offered by `jpx_metanode'.
         [ARG: num_box_types]
           If 0, the `box_types' array is ignored and the filter is
           configured to pass all box types.  Otherwise, only those box
           types specified via the `box_types' array will be parsed.
         [ARG: box_types]
           Array containing `num_box_types' entries.  There are no
           restrictions on the content of this array, but note that some
           box types (those required for rendering imagery) may not appear
           in the metadata tree even if they are specified in this array.
      */
    KDU_AUX_EXPORT jpx_metanode access_root();
      /* [SYNOPSIS]
           Access the root node in the metadata tree.
           See `jpx_metanode' for a comprehensive explanation of the tree
           structure and the role played by its root.
      */
    KDU_AUX_EXPORT jpx_metanode locate_node(kdu_long file_pos);
      /* [SYNOPSIS]
           This function searches for a metanode whose contents were obtained
           by reading a box whose contents are located at `file_pos' in the
           box's original file.
           [//]
           It is useful to remember that a `jpx_metanode' which is obtained
           from an asoc (Association) box directly represents both the
           asoc box and its first sub-box, with any remaining sub-boxes
           interpreted as the node's descendants.  As a result, there are
           two file positions which can be used to locate such a metanode.
         [RETURNS]
           An empty `jpx_metanode' if no match could be found.  In the case
           of input sources which are loaded from a dynamic cache, this
           might mean that you need to wait until the cache contains more
           contents and/or invoke `load_matches' to load any new boxes which
           might have arrived.
         [ARG: file_pos]
           Absolute location of the first byte of the box contents (not its
           header) within the original file to which the box belongs.  If
           the box was read via a cache, you can still obtain the absolute
           location within its original file by adding the box header length
           to the value returned by `jp2_locator::get_file_pos', where
           `jp2_locator' may, for example, be returned by
           `jp2_input_box::get_locator'.  You can obtain the box header length
           by calling `jp2_input_box::get_box_header_length'.
      */
    KDU_AUX_EXPORT jpx_metanode get_touched_nodes();
      /* [SYNOPSIS]
           You can use this function to traverse all nodes in the metadata
         tree which have been created for the first time, deleted,
         changed in any way, or explicitly touched by `jpx_metanode::touch'.
         Once a node is returned by this function, it will not be returned
         again, unless it is again added onto the internal list of touched
         nodes, by one of the following events:
         [>>] If a node is deleted, using `jpx_metanode::delete_node' it
              will be appended to the internal list of touched nodes.  Of
              course, its descendants will also be deleted and appended to
              the list.  Remember that deleted nodes are not physically
              deleted from memory; one good reason for this is to enable
              applications to track such deletions via the present function.
         [>>] If a node's ancestry is changed by `jpx_metanode::change_parent'
              the node and all of its descencants will be marked as having a
              changed ancestry and appended to the list of touched nodes.
         [>>] If a node's data is changed by `jpx_metanode::change_to_label'
              or `jpx_metanode::change_to_delayed', the node will be
              appended to the list of touched nodes and all of its
              descendants will be marked as having a changed ancestry and
              also appended.
         [>>] If `jpx_metanode::touch' is invoked, the node and all of its
              descedants will be appended to the list of touched nodes.
         [//]
         This function is provided to facilitate efficient updating of
         application-defined structures as metadata becomes available
         (e.g., from a dynamic cache) or as metadata is edited.
         [//]
         To find out what changes actually occurred, you can use the
         functions `jpx_metanode::is_changed', `jpx_metanode::is_deleted'
         and `jpx_metanode::ancestor_changed'.  All of these functions may
         return false if the node is newly parsed from a data source or
         otherwise created, or if the node found its way onto the touched
         node list as a result of a call to `jpx_metanode::touch'.
         [//]
         One feature of this function is that metanodes which are freshly
         created as a result of parsing an input file are not considered o
         be touched until their principle box has been completely parsed
         (this is the first sub-box of an association box, or else it is
         the sole box associated with the metanode).  This means that every
         metanode you find on the touched list is available in full, except
         that its descendants, if any, might not yet all be available.  Once
         they become available, they too will be added to the touched list.
         [//]
         Noting that all of the conditions which cause a node to be added
         to the internal list of touched nodes also cause all of the node's
         descendants to be added to the list, we are able to guarantee that
         parents always appear in the list before their descendants, if any.
         This can simplify some applications of the function.
      */
    KDU_AUX_EXPORT jpx_metanode
      peek_touched_nodes(kdu_uint32 box_type,
                         jpx_metanode last_peeked=jpx_metanode());
      /* [SNOPSIS]
           This function provides an alternate way of accessing the touched
           node list, from that offered by `get_touched_nodes'.  Unlike
           `get_touched_nodes', this function does not remove nodes from the
           touched list.  It scans through the list for the first node which
           matches the supplied `box_type' (or any node, if `box_type'=0).
           If `last_peeked' is non-empty, the function starts scanning
           immediately after the `last_peeked' box in the touched list.
         [ARG: box_type]
           If 0 the function matches metanodes with any box type.
         [ARG: last_peeked]
           If non-empty, the function treats this metanode as the result
           of a previous call to `peek_touched_nodes' and starts scanning
           from the next metanode in the list of touched nodes.  If the
           `last_peeked' box does not belong to the touched nodes list, the
           function returns an empty interface.
      */
    KDU_AUX_EXPORT void copy(jpx_meta_manager src);
      /* [SYNOPSIS]
           Use this function to copy the structure and contents of an
           entire metadata tree into the present object's metadata tree.
           This operation can be used to copy a source file's metadata
           tree to a new output file, possibly after some editing.  Note
           that this function uses `jpx_metanode::add_copy' so the conditions
           described there also apply here.  Specifically, you must be sure
           to keep any ultimate `jp2_family_src' object associated with `src'
           open until after the current metadata tree has been written using
           `jpx_target::write_metadata'.
           [//]
           One side effect of this function is that it also updates the
           `src' object to include as much metadata as can possibly be
           parsed from its ultimate data source.
      */
    KDU_AUX_EXPORT bool
      load_matches(int num_codestreams, const int codestream_indices[],
                   int num_compositing_layers, const int layer_indices[]);
      /* [SYNOPSIS]
           Call this function to make sure that as many relevant
           metadata boxes have been parsed as possible prior to using
           `enumerate_matches' to scan through the nodes which match a
           particular set of criteria.  The present function attempts to
           parse any metadata boxes which have not already been fully parsed.
           The function also attempts to recover any metadata which may be
           nested inside codestream header boxes or compositing layer header
           boxes, where the codestreams and compositing layers of interest
           are identified by the function's arguments.
         [RETURNS]
           True if, as a result of calling this function, one or more
           new nodes which could be returned via a subsequent call to
           `enumerate_matches' have become available.
         [ARG: num_codestreams]
           Number of codestream indices supplied via the `codestream_indices'
           array.  If this argument is negative, all codestreams are
           considered to be of interest, and `codestream_indices' is ignored.
         [ARG: codestream_indices]
           Array with `num_codestreams' elements.  In addition to metadata
           boxes which appear at the top level of the file (or nested within
           association boxes which are at the top level of the file), the
           function also looks for any unparsed metadata within codestream
           header boxes belonging to the identified codestreams.
         [ARG: num_compositing_layers]
           Number of compositing layer indices supplied via the `layer_indices'
           array.  If this argument is negative, all compositing layers are
           considered to be of interest, and `layer_indices' is ignored.
         [ARG: layer_indices]
           Array with `num_compositing_layers' elements.  In addition to
           metadata boxes which appear at the top level of the file (or
           nested within association boxes which are at the top level of the
           file), the function also looks for any unparsed metadata within
           compositing layer header boxes belonging to the identified
           compositing layers.
      */
    KDU_AUX_EXPORT jpx_metanode
      enumerate_matches(jpx_metanode last_node,
                        int codestream_idx, int compositing_layer_idx,
                        bool applies_to_rendered_result,
                        kdu_dims region, int min_size,
                        bool exclude_region_numlists=false);
      /* [SYNOPSIS]
           This function represents the metadata search facilities of the
           `jpx_meta_manager' object.  It exploits the lists mentioned in
           the `jpx_meta_manager' overview discussion to avoid scanning
           through the entire metadata tree.
           [//]
           If `last_node' is an empty interface, the function searches for
           the first node which matches the conditions established
           by the remaining arguments.
           [//]
           If `last_node' is not an empty interface, the function searches
           for the first node which follows the `last_node' and satisfies
           the conditions.  For reliable behaviour, any non-empty `last_node'
           interface should refer to a node which itself matches the
           conditions.
           [//]
           You should be aware that the order in which this function
           enumerates the matching nodes is not generally predictable.
           In fact, the order may potentially change each time you invoke
           the `load_matches' function.  In particular, if the file is located
           on a remote server, delivered via JPIP, you may need to call
           `load_matches' frequently, and after each call the only way
           to enumerate all metadata which matches some criterion is to
           start from scratch, calling this function first with an empty
           `last_node' interface and then until it returns an empty
           interface.
           [//]
           To understand the constraints imposed by the various arguments
           it is convenient to define three types of nodes:
             [>>] "ROI Nodes" are those whose `jpx_metanode::get_num_regions'
                  function returns non-zero.  These are also exactly the
                  nodes which are found within the first type of internal
                  list described in the introduction to the `jpx_meta_manager'
                  object.
             [>>] "Numlist Nodes" are those whose
                  `jpx_metanode::get_numlist_info' function returns true.
                  These are also exactly the nodes which are found within
                  the second type of internal list described in the
                  introduction to the `jpx_meta_manager' object.
             [>>] "Unassociated Nodes" are those which do not satisfy
                  either of the criteria above AND are not descended from
                  any node which does.  These are exactly the nodes which
                  are found within the third type of internal list
                  described in the introduction to the `jpx_meta_manager'
                  object.
           [//]
           The function matches only those nodes which belong to one of
           the above three categories.  It matches "ROI Nodes" if and only
           if `region' is a non-empty region.  It matches "Numlist Nodes"
           if and only if `region' is an empty region and either
           `codestream_idx'>=0, `compositing_layer_idx'>=0 or
           `applies_to_rendered_result'=true.  It matches "Unassociated Nodes"
           if and only if `codestream_idx'=-1, `compositing_layer_idx'=-1,
           `applies_to_rendered_result'=false, and `region' is an empty
           region.
           [//]
           In some cases, the application may be interested in finding
           "Numlist Nodes" which contain metadata associated with the
           entities in the number list, but not with a specific ROI.  To
           facilitate this, the `exclude_region_numlists' argument may be
           set to true (see below).
         [RETURNS]
           An empty interface if no match can be found.
         [ARG: codestream_idx]
           If non-negative, the function matches only "Numlist Nodes" which
           reference the indicated codestream index or
           "ROI Nodes" which have a "Numlist Node" in their ancestry which
           references the indicated codestream index.  The latter case
           applies only if `region' is non-empty.
         [ARG: compositing_layer_idx]
           If non-negative, the function matches only "Numlist Nodes" which
           reference the indicated compositing layer index or
           "ROI Nodes" which have a "Numlist Node" in their ancestry which
           references the indicated compositing layer index.  The latter case
           applies only if `region' is non-empty.
         [ARG: applies_to_rendered_result]
           If true, the function matches only "Numlist Nodes" which
           reference the "rendered result" or "ROI Nodes" which have a
           "Numlist Node" in their ancestry which references the "rendered
           result".  The latter case applies only if `region' is non-empty.
         [ARG: region]
           If non-empty, the function matches only "ROI Nodes" whose
           bounding box intersects with the supplied region.  Note that this
           does not necessarily mean that any of the individual regions in
           the ROI description box will intersect with `region', although
           it usually does mean this.  See `jpx_metanode::get_bounding_box'
           for more on the bounding box.
         [ARG: min_size]
           Ignored unless `region' is a non-empty region.  In this case,
           the function matches only "ROI Nodes" which contain at least
           one region, both of whose dimensions are no smaller than
           `min_size'.
           [//]
           This argument facilitates the efficient deployment of
           resolution sensitive metadata browsing systems.  An interactive
           image browser, for example, may choose not to generate overlay
           information for spatially-sensitive metadata whose spatial
           dimensions are too small to be clearly discerned at the current
           viewing resolution.  For example, streets might only become
           apparent on an image of a metropolitan area at some sufficiently
           fine level of image detail.  Cluttering the image with metadata
           holding the street names before the streets can be clearly
           discerned would be inadvisable (and computationally inefficient).
         [ARG: exclude_region_numlists]
           If true, the function will skip over any "Numlist Nodes" whose
           only immediate descendants are "ROI Nodes".  The argument is
           relevant only if `region' is empty and either
           `codestream_idx' >= 0, `compositing_layer_idx' >= 0 or
           `applies_to_rendered_result' is true.
      */
    KDU_AUX_EXPORT jpx_metanode
      insert_node(int num_codestreams, const int *codestream_indices,
                  int num_compositing_layers, const int *layer_indices,
                  bool applies_to_rendered_result, int num_regions,
                  const jpx_roi *regions);
      /* [SYNOPSIS]
           This function provides an especially convenient mechanism for
           creating new metadata nodes.  Although it is possible to create
           the nodes directly using functions such as
           `jpx_metanode::add_numlist' and `jpx_metanode::add_regions',
           multiple function calls may be needed to determine whether an
           appropriate association context has already been created.
           [//]
           The present function does all the work for you.  It analyzes the
           existing metadata tree to see if it already contains a
           node which is associated with exactly the same set of
           image entities (codestreams, compositing layers or the rendered
           result).  If so, the function returns a reference to that node or,
           if `num_regions' > 0, it returns a reference to a newly created
           ROI description node which is added as a descendant to that
           node.
           [//]
           If necessary, the function creates new entries in the metadata
           tree to hold a number list representing the image entities
           specified via the first five arguments and the image regions
           specified via the last two arguments, returning a reference to
           the relevant node.
           [//]
           If `num_regions' is 0, the returned node lies in a context which
           does not associated it with any image region.  Otherwise, there
           must be at least one codestream specified, since ROI description
           boxes are meaningless unless associated with codestreams.  If
           `num_regions' is non-zero, the function always creates a new
           ROI description node, even if an identical one already exists
           with the same image entity associations.  This saves us the
           overhead of tracking down unlikely events.
           [//]
           If `num_codestreams' and `num_compositing_layers' are 0 and
           `applies_to_rendered_result' is false, the returned node lies
           in a context which does not associate it with any number list.
           [//]
           If both of the above sets of conditions apply, the function simply
           returns the root of the metadata tree.
         [ARG: num_codestreams]
           Number of codestreams with which the returned node must be
           associated.  It should be associated with no more and no
           fewer.
         [ARG: codestream_indices]
           Array with `num_codestreams' entries containing the indices
           of the codestreams with which the returned node must be
           associated.  All indices start from 0, which represents the
           first codestream in the file.
         [ARG: num_compositing_layers]
           Number of compositing layers with which the returned node must be
           associated.  It should be associated with no more and no
           fewer.
         [ARG: layer_indices]
           Array with `num_compositing_layers' entries containing the indices
           of the compositing layers with which the returned node must be
           associated.  All indices start from 0, which represents the
           first compositing layer in the file.
         [ARG: applies_to_rendered_result]
           True if the returned node is to be associated with the "rendered
           result"; otherwise it is not to be associated with the rendered
           result.
         [ARG: num_regions]
           Number of elements in the `regions' array.  If this is non-zero,
           the `num_codestreams' argument must also be non-zero, since 
           the contents of an ROI description box are meaningful primarily
           when its regions are associated with some code-stream.  In the
           future, we may remove this restriction, since the JPX file format
           does allow for the possibility of unassociated ROI description
           boxes, which are interpreted as belonging to the rendered result.
           For the purpose of creating JPX metadata from scratch, however,
           it is unlikely that any flexibility is sacrificed by forcing the
           application to describe regions with respect to a codestream
           coordinate system.
           [//]
           If this argument is zero, the returned node is not to be
           associated with any image regions through ROI description boxes.
         [ARG: regions]
           Array with `num_regions' entries, holding the collection of
           image regions which are to be recorded in an ROI description
           box with which the returned node is to be associated.
       */
  private: // Data
    jx_meta_manager *state;
  };

/*****************************************************************************/
/*                                jpx_source                                 */
/*****************************************************************************/

class jpx_source {
  /* [BIND: reference]
     [SYNOPSIS]
       Provides extensive support for interacting with JPX files.
  */
  // --------------------------------------------------------------------------
  public: // Member functions
    KDU_AUX_EXPORT jpx_source();
    virtual ~jpx_source() { close(); }
    jpx_source &operator=(jpx_source &rhs) { assert(0); return *this; }
      /* [SYNOPSIS]
           This assignment operator serves to prevent actual copying of one
           `jpx_source' object to another.  In debug mode, it raises an
           assertion.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns true if `open' has been called more recently than
           `close'.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS]
           Opposite of `exists'.
      */
    KDU_AUX_EXPORT virtual int
      open(jp2_family_src *src, bool return_if_incompatible);
      /* [SYNOPSIS]
           This function is able to open both JP2 and JPX compatible data
           sources.  Note that the most correct suffix to use for JPX files
           is actually ".jpf", not ".jpx", although the latter is not an
           unreasonable choice.  If the data source is not compatible with
           either the JPX or JP2 specifications, an error will be generated
           through `kdu_error', unless `return_if_incompatible' is true.
           [//]
           It is illegal to invoke this function on an object which has
           previously been opened, but not yet closed.
         [RETURNS]
           Three possible values, as follows:
           [>>] 0 if insufficient information is available from the `src'
                object to complete the opening operation.  In practice,
                this can occur only if the `src' object is fueled by a
                dynamic cache (a `kdu_cache' object).  In order to complete
                the opening of a JP2 data source, the signature and file-type
                boxes must both be found.  In order to complete the opening
                of a JPX file, the reader requirements box must also be
                found or else it must be discovered to be missing (some
                earlier JPX writers did not generate the JPX box, even though
                it is formally required), but we can tell if it is missing
                pretty easily, since it is required to follow the file-type
                box.
           [>>] 1 if successful.
           [>>] -1 if the data source is found not to be compatible with the
                JPX or JP2 specifications on the basis of the signature or
                file-type boxes (or their absence).  This value will not
                be returned unless `return_if_incompatible' is true (see
                below).  If it is returned, the object will be left in the
                closed state.
         [ARG: src]
           The `jp2_family_src' object must already have been opened.  It
           should generally support seeking, since non-seekable sources will
           cause errors to be generated unless the `jpx_source' is used
           in a manner which is consistent with linearly reading the
           data sources from beginning to end -- such use is possible for
           appropriately organized files.
         [ARG: return_if_incompatible]
           If false, an error will be generated through `kdu_error' if the
           data source is found to be incompatible with either the JPX or JP2
           specifications.  If true, incompatibility will not generate an
           error, but the function will return -1, leaving the application
           free to pass the `src' object to another file format reader
           (unless the `src' is not seekable, in which case it must be closed
           and re-opened before trying a different file format reader).
      */
    KDU_AUX_EXPORT jp2_family_src *get_ultimate_src();
      /* [SYNOPSIS]
           Returns a pointer to the `src' object which was passed to `open',
           or NULL if the object is not currently open.
      */
    KDU_AUX_EXPORT jpx_compatibility access_compatibility();
      /* [SYNOPSIS]
           Returns an interface which may be used to discover whether or
           not the data source is compatible with JP2 or baseline JPX, and also
           to discover the set of features which have been used (as recorded
           in a reader requirements box) and which features are considered
           essential for decoding.  Returns an empty interface if the
           `open' function has not yet returned successfully.
      */
    KDU_AUX_EXPORT jp2_data_references access_data_references();
      /* [SYNOPSIS]
           This function returns an interface which may be used to interpret
           the contents of fragment table and/or fragment list boxes.  If
           the JPX data references box (`dtbl') cannot yet be read for some
           reason (e.g., insufficient data available so far from a dynamic
           caching data source), the function returns an empty interface
           (i.e., one whose `jp2_data_references::exists' function returns
           false).
      */
    KDU_AUX_EXPORT bool count_codestreams(int &count);
      /* [SYNOPSIS]
           If the number of codestreams in the JPX/JP2 data source is already
           known, this function returns true, writing that number into `count'.
           Otherwise, the function attempts to parse further into the data
           source in order to count the number of codestreams which are
           available.  If it encounters the end of the source, or is otherwise
           able to deduce that the number of codestreams is known, it again
           returns true, writing the number of codestreams into `count'.
           [//]
           If the function is unable to parse to a point at which the number
           of codestreams can be known, it returns false, writing the
           number of codestreams encountered up to that point into the
           `count' argument.
           [//]
           A codestream is considered to have been encountered once either
           its codestream header box is encountered or its contiguous
           codestream (jp2c) or fragment table (ftbl) box has been
           encountered.  To actually access the codestream via the
           `access_codestream' function, more information is generally
           required.  In particular, the codestream's main header must be
           available, and either its codestream header box must have been
           encountered, or the reader must be able to deduce that there will
           be none (normally, this can only be deduced once the end of the
           file has been encountered, so JPX writers are strongly advised
           to include codestream header boxes).  However, these conditions
           need not all be met for the codestream's existence to be
           determined by the present function.
         [RETURNS]
           False if it is possible that more codestreams remain in the
           data source, but the function is unable to parse any further
           into the source at this point.  This generally means that the
           underlying `jp2_family_src' object is fueled by a dynamic cache
           (i.e., a `kdu_cache' object).
      */
    KDU_AUX_EXPORT bool count_compositing_layers(int &count);
      /* [SYNOPSIS]
           This function plays a similar role to `count_codestreams' but
           counts the number of compositing layers instead.  If the data
           source contains compositing layer header (jplh) boxes,
           the function counts the number of such boxes.  If it contains no
           compositing layer header boxes, the function returns 1, treating
           the first codestream as a compositing layer and using the
           JP2 header to derive colour space and other information which
           would normally appear within a compositing layer header box.
           [//]
           In either case, the function returns true, unless it cannot be
           sure what the count is, because insufficient elements from the data
           source are currently available.  This generally means that the
           underlying `jp2_family_src' object is fueled by a dynamic cache
           (i.e., a `kdu_cache' object).
           [//]
           When the number of compositing layers cannot yet be determined,
           the function writes the number of compositing layer header boxes
           found so far into the `count' argument.  This may be 0 if the
           data source contains no compositing layer header boxes and the
           end of the source has not yet been encountered.
           [//]
           JPX data sources need not necessarily contain compositing layer
           header boxes; in this case, each codestream represents a separate
           compositing layer.  However, the reader cannot make this judgement
           until the end of the file has been encountered.  If the data
           source is ultimately fueled by a dynamic cache whose top-level
           databin is incomplete, the function may thus have to return with
           `count' equal to 0, even if a large number of codestreams have
           been encountered already.  For these reasons, JPX writers are
           strongly encouraged to include compositing layer header boxes,
           even if they are not strictly required.
        [RETURNS]
           False if the number of compositing layers cannot yet be
           determined, because the function is unable to parse any further
           into the source at this point.  This generally means that the
           underlying `jp2_family_src' object is fueled by a dynamic cache
           (i.e., a `kdu_cache' object).
      */
    KDU_AUX_EXPORT jpx_codestream_source
      access_codestream(int which, bool need_main_header=true);
      /* [SYNOPSIS]
           Provides access to the codestream identified by `which'.  If
           `which' is 0, this is the first codestream in the data source; if
           `which' is 1, it is the second codestream in the data source, and
           so forth.
           [//]
           If the function is unable to access the requested codestream at
           present, it returns an empty interface (one whose
           `jpx_codestream_source::exists' function returns false).  This may
           happen either because the total number of codestreams in the
           data source is less than or equal to `which', or because the
           function is not yet able to parse far enough into the data source
           to recover the relevant boxes along with the code-stream's main
           header (if `need_main_header' is true).  The latter conditions
           can occur only if the underlying `jp2_family_src' objct is fueled
           by a dynamic cache (i.e., a `kdu_cache' object).
           [//]
           Before this function will return a non-empty interface, it must
           have encountered the contiguous codestream (jp2c) or fragment
           table (ftbl) box which holds the codestream's contents.  For
           data sources which are fueled by a dynamic cache, the entire
           main header must also be available already, unless
           `need_main_header' is false, in which case the
           `jpx_codestream_source::stream_ready' interface can be used to
           determine the point at which the main header is available.  If
           the codestream is represented by a fragment table box, however,
           it is not possible to check for main header availability, so the
           `need_main_header' argument is ignored.  The object must also
           have either seen all top level box headers in the data source,
           or have encountered both the JP2 header (jp2h) box
           and the relevant codestream header (chdr) box.
           [//]
           In other words, the function will not return a non-empty interface
           until it can be sure that all relevant information has been parsed
           from the data source and the codestream box itself can at least
           be successfully opened using `jpx_codestream_source::open_stream'.
           These conditions are more stringent than those required to count
           codestreams, so that `which' may be much less than the number
           returned by `count_codestreams', yet this function still returns
           an empty interface.  This just means that the underlying dynamic
           cache must receive more data before the codestream can be
           accessed.
         [ARG: need_main_header]
           If true, the `kdu_codestream_source::stream_ready' function
           of any non-empty returned interface will always return true.
      */
    KDU_AUX_EXPORT jpx_layer_source
      access_layer(int which, bool need_stream_headers=true);
      /* [SYNOPSIS]
           Similar to `access_codestream' but accesses the information
           associated with the compositing layer identified by `which'.  If
           `which' is 0, this is the first compositing layer in the data
           source; if `which' is 1, the second compositing layer is being
           requested, and so forth.  All data sources managed by this object
           are deemed to have at least one compositing layer, even if they
           do not contain explicit compositing layer header boxes.  Even a
           basic JP2 file always has one compositing layer and one codestream.
           If the data source contains compositing layer header boxes, there
           is one compositing layer for each such header box.  Otherwise
           (and this can only be determined by parsing to the end of the
           file first), each codestream corresponds to a separate compositing
           layer.
           [//]
           If the function is unable to access the requested layer at present,
           it returns an empty interface (one whose `jpx_layer_source::exists'
           function returns false).  This may happen either because the
           total number of compositing layers in the data source is less than
           or equal to `which', or because the function is not yet able to
           parse far enough into the data source to recover the relevant
           boxes.  The latter generally occurs only if the underlying
           `jp2_family_src' object is fueled by a dynamic cache (i.e., a
           `kdu_cache' object).
           [//]
           Before the function will return a non-empty interface, it must
           have found and parsed the compositing layer header (jplh) box, if
           any, in addition to any JP2 header (jp2h) box which is required
           for recovering default parameters, and any codestream header
           box whose component mapping box might be required to interpret
           channel mapping rules.  Alternatively, we must have reached the
           end of the data source or have other means of knowing that there
           will be no compositing layer header box for this layer.
           These complexities arise because JPX was created to allow quite
           a bit of flexibility for writers, but this has obvious adverse
           implications for readers, leaving them unable to make solid
           incremental decisions often until all boxes have been seen.  The
           Kakadu implementation does its best to hide all such irregularities
           from the application.
           [//]
           For each required codestream, the embedded codestream box must
           also be available before a non-empty interface will be returned.
           Moreover, if `need_stream_headers' is true, all code-stream main
           headers associated with the required codestreams must also be
           available.
         [ARG: need_stream_headers]
           If true, each codestream associated with a returned non-empty
           interface can be successfully accessed by calling
           `access_codestream' will a `need_main_header' argument of
           true.  Otherwise, successful access of each required codestream
           is only guaranteed with a false value for the `need_main_header'
           argument to `access_codestream'
      */
    KDU_AUX_EXPORT int get_num_layer_codestreams(int which_layer);
      /* [SYNOPSIS]
           This function is equivalent to calling `access_layer', followed
           by `jpx_layer_source::get_num_codestreams' with one exception -- if
           the layer cannot yet be opened due to the unavailability of some
           header box or the relevant codestream main headers, the present
           function may still succeed in reporting the actual number of
           codestreams used by the compositing layer.  This is useful when
           the image is being served by a JPIP server which needs to be
           informed of the codestreams which are required before it will
           supply their main headers required to access the compositing
           layer via `access_layer'.
         [RETURNS]
           0 if the layer does not exist or if insufficient information is
           currently available concerning the layer's codestreams.
      */
    KDU_AUX_EXPORT int
      get_layer_codestream_id(int which_layer, int which_stream);
      /* [SYNOPSIS]
           This function is equivalent to calling `access_layer', followed
           by `jpx_layer_source::get_codestream_id' with one exception -- if
           the layer cannot yet be opened due to the unavailability of some
           header box or the relevant codestream main headers, the present
           function may still succeed in reporting the index of the codestream
           or codestreams used by the compositing layer.  This is useful when
           the image is being served by a JPIP server which needs to be
           informed of the codestreams which are required before it will
           supply their main headers required to access the compositing
           layer via `access_layer'.
         [RETURNS]
           -1 if `which_stream' is greater than or equal to the value returned
           via `get_num_layer_codestreams'.
      */
    KDU_AUX_EXPORT jpx_composition access_composition();
      /* [SYNOPSIS]
           Use this function to gain access to the `jpx_composition' object,
           to learn the contents of any composition box, describing
           complex multi-layer compositions and/or animation.
         [RETURNS]
           An empty interface (one whose `jpx_composition::exists' member
           returns false) if the composition box cannot yet be fully
           parsed.  This can happen only if the ultimate source of data is
           a dynamic cache which does not yet have sufficient data.  In
           many cases, the composition box may become available before the
           composition layers to which it refers can be opened, but the
           opposite may also be the case.
      */
    KDU_AUX_EXPORT jpx_meta_manager access_meta_manager();
      /* [SYNOPSIS]
           Call this function any time after a successful call to `open'
           to gain access to the infrastructure used to manage metadata
           which is not directly related to image interpretation.  See
           the description of `jpx_meta_manager' and (first) `jpx_metanode'
           to learn more about the management structure, and what metadata
           is actually managed.
           [//]
           Note that you are allowed to edit the metadata associated with
           an open JPX data source, although the edits will not automatically
           be reflected in the original source.  Nevertheless, you can use
           `jpx_meta_manager::copy' to copy an edited collection of
           metadata to a new file managed by `jpx_target'.
           [//]
           Note also that the interface returned via the present function
           ceases to be valid once `close' is called.
      */
    KDU_AUX_EXPORT virtual bool close();
      /* [SYNOPSIS]
           It is safe to call this function even if the object was never
           completely opened, or has been closed since it was last opened;
           in either of these case it will return false.
         [RETURNS]
           True, unless the object was not already open.
      */
  private: // State
    jx_source *state;
  };

/*****************************************************************************/
/*                                jpx_target                                 */
/*****************************************************************************/

class jpx_target {
  /* [BIND: reference]
     [SYNOPSIS]
       Provides full support for generating or copying JPX files.
       Note that the most correct suffix to use for JPX files is actually
       ".jpf", not ".jpx", although the latter is still a reasonable choice. 
       The typical sequence of operations used to generate a JPX file is
       as follows:
       [>>] Create and open a `jp2_family_tgt' object.
       [>>] Pass this object to `jpx_target::open'.
       [>>] Add at least one codestream using `jpx_target::add_codestream' and
            at least one composition layer using `jpx_target::add_layer'.
       [>>] Configure the various parameters of the `jpx_codestream_target'
            and `jpx_layer_target' interfaces returned via the above-mentioned
            functions.
       [>>] Call `write_headers' to write the JPX file header and as many
            of the compositing layer and codestream header boxes as you
            like.  If you have installed breakpoints using
            `jpx_codestream_target::set_breakpoint'
            or `jpx_layer_target::set_breakpoint', you must call
            `write_headers' multiple times, writing additional boxes of your
            choice after each breakpoint.
       [>>] Optionally write additional boxes after `write_headers' returns
            NULL.
       [>>] Pass through each `jpx_codestream_target' interface in turn,
            using its `jpx_codestream_target::open_stream' function to open a
            contiguous codestream box.  Pass this box directly to 
            `kdu_codestream::create' and generate the codestream using
            `kdu_codestream::flush'.  Alternatively, you may write directly to
            the open box (e.g., to copy a code-stream, or write a non-JPEG2000
            codestream).  As a final alternative, you may use
            `jpx_codestream_target::write_fragment_table' to write a fragment
            table box in place of a contiguous codestream box.
       [>>] If you have used the `codestream_threshold' feature of the
            `write_headers' function you may need to invoke it again
            periodically, so as to interleave codestream header boxes and/or
            compositing layer header boxes with the codestreams (or
            fragment tables) themselves.
       [>>] Optionally write additional boxes after generating each
            codestream.
       [>>] Call `close' to finish the JPX file (it may or may not write
            additional boxes) and clean up internal resources.
       [>>] Call `jp2_family_tgt::close' when you are completely finished.
       [//]
       Note that this object will make every effort to produce JPX files
       which are also JP2 compatible, recording the outcome of the
       relevant compatibility tests in the file-type box.  For this reason,
       an application may choose to always write JPX files, even if the
       JP2 feature set is sufficient for its purposes.
  */
  // --------------------------------------------------------------------------
  public: // Member functions
    KDU_AUX_EXPORT jpx_target();
    virtual ~jpx_target() { close(); }
    jpx_target &operator=(jpx_target &rhs) { assert(0); return *this; }
      /* [SYNOPSIS]
           This assignment operator serves to prevent actual copying of one
           `jpx_target' object to another.  In debug mode, it raises an
           assertion to help you catch such mistakes.
      */
    bool exists() { return (state != NULL); }
      /* [SYNOPSIS]
           Returns true if `open' has been called more recently than
           `close'.
      */
    bool operator!() { return (state == NULL); }
      /* [SYNOPSIS]
           Opposite of `exists'.
      */
    KDU_AUX_EXPORT virtual void open(jp2_family_tgt *tgt);
      /* [SYNOPSIS]
           It is illegal to invoke this function on an object which has
           previously been opened, but not yet closed.
      */
    KDU_AUX_EXPORT jpx_compatibility access_compatibility();
      /* [SYNOPSIS]
           Returns an interface which may be used to specify features which
           are used in the JPX file being generated, along with those
           features which are required to correctly interpret the file.
           It is not absolutely necessary for the application to perform
           its own compatibility initialization, since the object will
           attempt to automatically assess the features which have been
           used, assuming that all of them are essential for correct
           decoding of the file.  However, the object cannot be aware of
           features which are included with additional boxes inserted by
           the application (e.g., after breakpoints between calls to
           `write_headers').
      */
    KDU_AUX_EXPORT jp2_data_references access_data_references();
      /* [SYNOPSIS]
           This function returns an interface which may be used to specify
           external URL's used by fragment list and fragment table boxes.
           You may find, however, that you do not need to directly access
           the `jp2_data_references' object, since the relevant entries
           are added automatically if you call
           `jpx_codestream_target::add_fragment'.
      */
    KDU_AUX_EXPORT jpx_codestream_target add_codestream();
      /* [SYNOPSIS]
           You must add at least one codestream to each JPX file, but you
           may add more.  You may even add codestreams which are not
           referenced from any `jp2_channels' object (see
           `jpx_layer_target::access_channels').
      */
    KDU_AUX_EXPORT jpx_layer_target add_layer();
      /* [SYNOPSIS]
           You must add at least one layer to each JPX file.
      */
    KDU_AUX_EXPORT jpx_composition access_composition();
      /* [SYNOPSIS]
           Use this function to gain access to the `jpx_composition' object
           whose `add_frame' and `add_instruction' functions may be used to
           build a composition box, describing complex multi-layer
           compositions and/or animation.
      */
    KDU_AUX_EXPORT jpx_meta_manager access_meta_manager();
      /* [SYNOPSIS]
           Call this function any time after a call to `open'
           to gain access to the infrastructure used to manage metadata
           which is not directly related to image interpretation.  See
           the description of `jpx_meta_manager' and (first) `jpx_metanode'
           to learn more about the management structure, and what metadata
           is actually managed.
           [//]
           For many applications, the function `jpx_meta_manager::insert_node',
           in conjunction with `jpx_metanode::add_label' and
           `jpx_metanode::add_delayed' provide most of the funcionality
           required to build rich metadata descriptions.
           [//]
           See `write_metadata' for more on the actual writing of metadata
           to the output file.
           [//]
           Note that the interface returned via the present function
           ceases to be valid once `close' is called.
      */
    KDU_AUX_EXPORT jp2_output_box *
      write_headers(int *i_param=NULL, void **addr_param=NULL,
                    int codestream_threshold=-1);
      /* [SYNOPSIS]
           Call this function once all codestreams, compositing layers
           and composition instructions have been added, and sufficiently
           initialized.  The function scans the various parameters to be
           written into JPX boxes, reorganizing the information if possible,
           so as to try to ensure that the generated file is JPX compatible
           and maximize the likelihood that it can also be JP2 compatible.
           [//]
           All of the fixed-position headers are then written, including
           the JP2 signature box, the file-type box, the reader requirements
           box and the JP2 header box.
           [//]
           If `codestream_threshold' < 0, all codestream header boxes
           and compositing layer header boxes will also be written at this
           point.  Otherwise, the function writes all codestream header
           boxes, up to and including the one corresponding to
           the codestream whose index is `codestream_threshold' (indices
           run from 0), and all compositing layer header boxes, up to
           and including the first one whose compositing layer uses the
           codestream whose index is `codestream_threshold'.
           [//]
           When used in this way, the function returns with a NULL
           pointer, once it has written this limited set of headers, leaving
           the application to call the function again, with a larger value of
           `codestream_threshold', once it is prepared to write later
           codestreams.  In this way, compositing layer and codestream
           header boxes may be interleaved with the contiguous codestream
           or fragment table codestream boxes.
           [//]
           If you have installed a breakpoint using one of the functions
           `jpx_codestream_target::set_breakpoint' or
           `jpx_layer_target::set_breakpoint', the present function will
           return prematurely, with a pointer to the open super-box
           (either a Codestream Header (chdr) box, or a Compositing Layer
           Header (jplh) box) associated with the context in which the
           breakpoint was installed.  In this case, the function must be
           called again, until all breakpoints have been passed.
         [RETURNS]
           NULL if the requested headers have been completely written.
           This does not necessarily mean that the function need not be
           called again.  Indeed, it may need to be called again if the
           number of codestreams to be written is greater than
           `codestream_threshold'+1.  As explained above, this allows
           codestream header boxes and compositing layer header boxes to
           be interleaved with the codestream data itself, or with
           fragment tables pointing to that data.
           [//]
           A non-NULL return means that a breakpoint has been encountered,
           which usually means that the application should write some custom
           box as a sub-box of the open returned super-box, before invoking
           `write_headers' again.
         [ARG: i_param]
           If non-NULL, the integer parameter passed as an argument to the
           relevant `jpx_codestream_target::set_breakpoint' or
           `jpx_layer_target::set_breakpoint' function is returned in
           *`i_param'.
         [ARG: addr_param]
           If non-NULL, the address parameter passed as an argument to the
           relevant `jpx_codestream_target::set_breakpoint' or
           `jpx_layer_target::set_breakpoint' function is returned in
           *`addr_param'.
      */
    KDU_AUX_EXPORT jp2_output_box *
      write_metadata(int *i_param=NULL, void **addr_param=NULL);
      /* [SYNOPSIS]
           Use this function to write the metadata managed via
           `jpx_meta_manager' (see `access_meta_manager') to the actual
           output file.  You must not do this until after `write_headers'
           has been successfully completed, but you can otherwise choose
           to write metadata at any point in the file you like.  Some
           applications may choose to write the metadata prior to
           embedded code-streams, while others might choose to write it all
           at the end of the file.  The latter is probably a better choice
           form the perspective of editing flexibility.
           [//]
           The function may need to be called multiple times before the
           metadata is completely written.  Exactly like `write_headers'
           the function will return with a non-NULL pointer whenever it
           encounters a breakpoint.  A breakpoint is any `jpx_metanode'
           which was created using `jpx_metanode::add_delayed'.
           [//]
           You should not attempt to add any new metadata after the first
           call to this function.
         [RETURNS]
           NULL if all of the available metadata has been written.
           Otherwise, the function has encountered a `jpx_metadata'
           node which was created using `jpx_metanode::add_delayed'.
           In this case, the function returns a pointer to an open
           `jpx_output_box' object which has the correct box type code --
           the one passed to `jpx_metanode::add_delayed'.  You have only
           to write the box's contents.
         [ARG: i_param]
           If non-NULL, the integer parameter supplied to
           `jpx_metanode::add_delayed' is written into *`i_param'.
         [ARG: addr_param]
           If non-NULL, the address parameter supplied to
           `jpx_metanode::add_delayed' is written into *`addr_param'.
      */
    KDU_AUX_EXPORT virtual bool close();
      /* [SYNOPSIS]
           Call this function only after you have written the JPX header and
           generated contiguous codestream boxes or fragment tables for each
           codestream.  It is possible that the function will generate
           additional trailing boxes of its own.  If you fail to do this, an
           error will be generated through `kdu_error'.
           [//]
           It is safe to call this function even if the object was never
           opened, or has been closed since it was last opened, but in this
           case the function will return false.
         [RETURNS]
           True, unless the object was not open.
      */
  private:
    jx_target *state;
  };

#endif // JPX_H
