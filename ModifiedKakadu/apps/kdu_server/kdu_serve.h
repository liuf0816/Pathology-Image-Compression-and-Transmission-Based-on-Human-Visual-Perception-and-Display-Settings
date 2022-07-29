/*****************************************************************************/
// File: kdu_serve.h [scope = APPS/SERVER]
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
   Header file describing interfaces to the platform independent core
services of the Kakadu server application.  A complete server implementation
requires network services, which may not be available on all platforms; the
"kdu_server" application also requires support for multi-threading.
******************************************************************************/

#ifndef KDU_SERVE_H
#define KDU_SERVE_H

#include <assert.h>
#include <string.h>
#include "kdu_elementary.h"
#include "kdu_compressed.h"
#include "kdu_messaging.h"
#include "kdu_cache.h" // For declarations of data-bin classes
#include "kdu_client_window.h" // For declaration of `kdu_window'

// Defined here:
struct kds_chunk;
class kds_id_encoder;
class kds_image_entities;
struct kds_metascope;
struct kds_metagroup;
class kdu_serve_target;
class kdu_serve;

// Defined elsewhere:
struct kd_active_precinct;
struct kd_active_bin;
struct kd_serve;


/*****************************************************************************/
/*                                 kds_chunk                                 */
/*****************************************************************************/

struct kds_chunk {
  /* [SYNOPSIS]
       This structure manages a contiguous chunk of memory into which a
       whole number of data tranfer messages are packed by the
       `kdu_serve::generate_increments' function.  In some cases, the
       chunk may be identified with a single network packet, but applications
       and communication protocols may assign any desired interpretation.
       [//]
       For more information, see the `kdu_serve::generate_increments'
       function.
  */
  public: // Data
    int max_bytes;
      /* [SYNOPSIS]
           Maximum number of bytes which can be recorded in the `data'
           array, including `prefix_bytes'.  The value of the `num_bytes'
           member is bounded by `max_bytes'.  The value of `max_bytes'
           must not be modified by applications, but it may be accessed to
           determine the legal range of indices which may be applied to the
           `data' array.
      */
    int prefix_bytes;
      /* [SYNOPSIS]
           This member indicates the number of initial bytes in the `data'
           buffer which have been reserved by the application.  The compressed
           data is stored in the remaining `num_bytes'-`prefix_bytes'
           elements of the array.  The number of prefix bytes to be reserved
           in each chunk is configured in the call to `kdu_serve::initialize'.
      */
    int num_bytes;
      /* [SYNOPSIS]
           This member indicates the total number of valid bytes in the
           `data' buffer, including the initial `prefix_bytes' reserved
           for use by the application.
      */
    kdu_byte *data;
      /* [SYNOPSIS]
           Buffer used to store chunk data.  The first `prefix_bytes' are
           reserved for use by the application.  The total number of valid
           bytes in the buffer, including the prefix bytes and all bytes
           written by the `kdu_serve::generate_increments' function is
           `num_bytes'.
           [//]
           Note that the `data' pointer is guaranteed to be aligned on an
           8-byte boundary.  In practice, it will probably point into the
           same block of memory which is allocated to hold the `kds_chunk'
           structure.
      */
    bool abandoned;
      /* [SYNOPSIS]
           This member is used by the `kdu_serve::release_chunks' function
           if its `check_abandoned' argument is set to true.  See the comments
           appearing with that function for an explanation.
      */
    kds_chunk *next;
      /* [SYNOPSIS]
           Used to build a linked list of chunks.
      */
  private: // Data
    friend struct kd_serve;
    friend class kdu_serve;
    friend class kd_chunk_server;
    kd_active_precinct *precincts;
    kd_active_bin *other_bins;
  };

/*****************************************************************************/
/*                               kds_id_encoder                              */
/*****************************************************************************/

class kds_id_encoder {
  /* [SYNOPSIS]
     This object provides the functionality required to encode all
     information in a JPIP message header which is required to uniquely
     identify the data-bin to which the message refers.  The object is
     generally passed to `kdu_serve::generate_increments' which uses these
     encoding rules to generate messages with correctly encoded headers.
     [//]
     The default implementation encodes data-bin identifiers using a variable
     number of bytes following the conventions of the JPIP standard
     (JPEG2000, Part 9, or IS 15444-9).  To understand these rules, one
     must first appreciate the concept of a VBAS (Variable length Byte Aligned
     Segment).  A VBAS is a string of bytes, all but the last of which has
     a most significant bit of 1.  The least significant 7 bits from each
     of these bytes are concatenated in order to form a 7K bit code, where
     K is the number of bytes in the VBAS.  A JPIP message header has the
     form:
     [>>] Bin-ID [, Class] [, CSn], Offset, Length [, Aux]
     [//]
     where each of the 6 items is a VBAS and those items enclosed in square
     brackets might not always appear.  The present object's `encode_id'
     function is used to encode the first 3 elements listed above.  The
     Bin-ID is always encoded, and the most significant 2 bits of its
     7K bit code identify whether or not each of the Class and CSn elements
     appear.
     [//]
     In addition to providing the `encode_id' function, the default
     implementation also remembers the class code and stream-id values of
     the most recently encoded message, so that the differential header
     encoding rules specified in the JPIP standard can be exploited for
     efficiency.  The `kdu_serve::generate_increments' function
     calls the `decouple' member at appropriate points so as to ensure
     that the differential encoding rules are not applied across
     boundaries at which the message stream may be broken.
     [//]
     Note that the default implementation described above may be
     altered by overriding the `encode_id' or other member functions of the
     default implementation in an object supplied to
     `kdu_serve::generate_increments'.
  */
  public: // Member functions
    kds_id_encoder() { exploit_last_message=false; }
    virtual ~kds_id_encoder() { return; }
      /* [SYNOPSIS]
           Keep compiler happy by defining a virtual destructor.
      */
    virtual void decouple() { exploit_last_message=false; }
      /* [SYNOPSIS]
           After this function is called, the next data-bin ID to be encoded
           by the `encode_id' function will be entirely independent of all
           previous headers.  Otherwise, the header may be coded differentially
           with respect to previous headers.  For more on how this function
           is used, consult the comments appearing with the function
           `kdu_serve::generate_increments'.
      */
    KDU_AUX_EXPORT virtual int
      encode_id(kdu_byte *dest, int databin_class, int stream_id,
                kdu_long bin_id, bool is_final, bool extended=false);
      /* [SYNOPSIS]
           Encodes the indicated data-bin identifier for the indicated
           data-bin class into a whole number of bytes which are recorded in
           the `dest' buffer.  If `dest' is NULL, the function only simulates
           the process, returning the total number of bytes which would have
           been written.  Since the ID encoding rules can be state
           dependent (as in JPIP), we note that simulation does not have
           any impact on state, so the results might only be valid when
           simulating the encoding of a data-bin ID for the first message
           following a fully encoded message.
         [RETURNS]
           The actual number of bytes occupied by the data-bin identifier
           written to the `dest' array.  This number of bytes includes the
           number of bytes required to represent the databin class, the
           codestream-id value, and the value of the `is_final' flag.  It
           does not include the cost of encoding auxiliary information
           associated with `extended' headers.
         [ARG: databin_class]
           Currently one of `KDU_MAIN_HEADER_DATABIN',
           `KDU_TILE_HEADER_DATABIN', `KDU_PRECINCT_DATABIN',
           `KDU_TILE_DATABIN' or `KDU_META_DATABIN', although new classes
           might be created in the future.
         [ARG: stream_id]
           Identifier of the code-stream to which the data-bin belongs.  Must
           be non-negative.  As a general rule, use 0 unless the target object
           you are transporting has multiple code-streams to distinguish.
         [ARG: bin_id]
           See the description of `kdu_serve::generate_increments' for a
           discussion of the identifiers which can be supplied for each
           data-bin class.
         [ARG: is_final]
           If true, the data-bin identifier is being used to signal the
           final contribution to that data-bin.  This fact must be encoded
           into the identifier itself, at least for message headers defined
           by the JPIP standard.
         [ARG: extended]
           If true, this message header has an extended form.  For the JPIP
           standard, this means that an extra VBAS (see general introduction
           to the present `kds_id_encoder' object) appears at the end of the
           header.  The extended VBAS is not encoded by this function, but
           the class code will be adjusted to reflect the presence of the
           extended VBAS if this flag is set to true.  The general policy,
           at least for JPIP, is that extended message headers contain
           additional information which is not strictly required for correct
           interpretation of the message contents, but may be useful to
           some clients.
      */
  protected: // Data
    bool exploit_last_message; // True if header coded differentially
    kdu_long last_stream_id;
    int last_class;
    bool last_extended;
  };

/*****************************************************************************/
/*                           kds_image_entities                              */
/*****************************************************************************/

class kds_image_entities {
  /* [SYNOPSIS]
       This object is used by `kds_metascope' to define one or more image
       entities (codestreams or compositing layers).
  */
  public: // Member functions
    virtual ~kds_image_entities() { return; }
    bool test_codestream(int test_idx)
      {
        if (universal_flags & 0x01000000)
          return true;
        kdu_int32 idx;
        for (int n=0; n < num_entities; n++)
          if (((idx=entities[n] & 0x00FFFFFF) == test_idx) &&
              ((entities[n]>>24) == 1))
            return true;
        return false;
      }
    bool test_codestream_range(kdu_sampled_range &rg)
      {
        if (universal_flags & 0x01000000)
          return true;
        kdu_int32 idx;
        for (int n=0; n < num_entities; n++)
          if (((entities[n]>>24) == 1) &&
              ((idx=entities[n] & 0x00FFFFFF) >= rg.from) && (idx <= rg.to) &&
              ((rg.step == 1) || (((idx-rg.from)%rg.step) == 0)))
            return true;
        return false;
      }
    bool test_compositing_layer(int test_idx)
      {
        if (universal_flags & 0x02000000)
          return true;
        kdu_int32 idx;
        for (int n=0; n < num_entities; n++)
          if (((idx=entities[n] & 0x00FFFFFF) == test_idx) &&
              ((entities[n]>>24) == 2))
            return true;
        return false;
      }
    bool test_compositing_layer_range(kdu_sampled_range &rg)
      {
        if (universal_flags & 0x02000000)
          return true;
        kdu_int32 idx;
        for (int n=0; n < num_entities; n++)
          if (((entities[n]>>24) == 2) &&
              ((idx=entities[n] & 0x00FFFFFF) >= rg.from) && (idx <= rg.to) &&
              ((rg.step == 1) || (((idx-rg.from)%rg.step) == 0)))
            return true;
        return false;
      }
  protected: // Data
    int num_entities;
    int *entities; // List of image entities; most significant byte holds
    // type of image entity: 0x01 for codestreams, 0x02 for compositing layers
    kdu_int32 universal_flags; // 0x01000000=match all codestreams;
                               // 0x02000000=match all layers; may have both
  };

/*****************************************************************************/
/*                              kds_metascope                                */
/*****************************************************************************/

#define KDS_METASCOPE_MANDATORY                 ((kdu_int32) 0x00001)
#define KDS_METASCOPE_IMAGE_MANDATORY           ((kdu_int32) 0x00002)
#define KDS_METASCOPE_INCLUDE_NEXT_SIBLING      ((kdu_int32) 0x00010)
#define KDS_METASCOPE_LEAF                      ((kdu_int32) 0x00020)
#define KDS_METASCOPE_HAS_GLOBAL_DATA           ((kdu_int32) 0x00100)
#define KDS_METASCOPE_HAS_IMAGE_SPECIFIC_DATA   ((kdu_int32) 0x00200)
#define KDS_METASCOPE_HAS_IMAGE_WIDE_DATA       ((kdu_int32) 0x01000)
#define KDS_METASCOPE_HAS_REGION_SPECIFIC_DATA  ((kdu_int32) 0x02000)

struct kds_metascope {
  /* [SYNOPSIS]
       Used to describe image regions and image entities to which the
       metadata boxes in a particular `kds_metagroup' object relate,
       this object provides the information necessary for `kdu_serve' to
       appropriately sequence metadata into a stream of messages generated
       by the `kdu_serve::generate_increments' function, so that metadata is
       appropriately interleaved with compressed imagery, and only the
       metadata which is actually relevant to a client's interests need
       be included.
       [//]
       The information described in this object is designed to facilitate
       efficient access to the metadata which is actually required to
       satisfy a service request.  In particular, wherever a group has
       multiple sub-groups (represented by placeholders), each with
       potentially different scopes, the parent group's scope includes the
       union of its descendants' scopes.  This helps the server to efficiently
       skip over irrelevant groups in the tree.
  */
  public: // Initialization
    kds_metascope()
      { flags=sequence=max_discard_levels=0; entities=NULL; }
  public: // Data
    kdu_int32 flags;
      /* [SYNOPSIS]
           This word may contain any meaningful combination of the following
           flags:
           [>>] `KDS_METASCOPE_MANDATORY' -- If this flag is present and the
                metadata otherwise matches the current client request, the
                metadata must be sent to the client, regardless of whether
                or not a "metareq" request field has been used.
           [>>] `KDS_METASCOPE_IMAGE_MANDATORY' -- Similar to the above flag,
                except the matching metadata ceases to be mandatory if a
                "metareq" request field specifies "metadata-only".  Imagery
                related headers generally belong to this category
           [>>] `KDS_METASCOPE_INCLUDE_NEXT_SIBLING' -- If this flag is
                present, whenever the present group is considered relevant
                to a client request, all box headers in the next sibling
                group (boxes inside the same superbox), if any, must also be
                considered relevant to the client request.  When applied
                to a group of top-level boxes, this means that the next
                group of top-level box headers must also be sent to the client.
                The flag plays an important role in ensuring that a file
                format reader will be able to determine whether or not
                all necessary boxes have been received within certain
                scopes.  For example, within a JPX compositing layer header
                box, all immediate sub-boxes must be seen in order for a
                reader to determine whether or not any other relevant boxes
                might be left to receive, which might override defaults
                in a JP2 header box.
           [>>] `KDS_METASCOPE_LEAF' -- If this flag is present, the
                following flags (all commencing with the `HAS_' prefix) may
                be interpreted as final qualifiers, meaning that the metagroup
                and all of its sub-groups must be sent to the client so long
                as the client request matches this scope.  Otherwise, a
                scope match simply means that one of the sub-groups might
                have a scope match.
           [>>] `KDS_METASCOPE_HAS_GLOBAL_DATA' -- If present, either this
                metagroup or one of its descendants (via placeholders) holds
                global metadata, not associated with any particular image
                entity.  Note that this is not mutually exclusive with the
                following flag, since a metagroup may have multiple sub-groups,
                only some of which are global.
           [>>] `KDS_METASCOPE_HAS_IMAGE_SPECIFIC_DATA' -- If present,
                either this metagroup or one of its descendants (via
                placeholders) holds metadata which is associated with one
                or more specific image entities (codestreams or compositing
                layers).  In this case, the `entities' member must be
                inspected to determine the identities of these image
                entities.
           [>>] `KDS_METASCOPE_HAS_IMAGE_WIDE_DATA' -- This flag may be
                present only if `KDS_METASCOPE_HAS_IMAGE_SPECIFIC_DATA'
                is also present.  It indicates that this group, or one of
                its descendants (via placeholders), is associated with the
                entirety of some image entity.
           [>>] `KDS_METASCOPE_HAS_REGION_SPECIFIC_DATA' -- This flag may
                be present only if `KDS_METASCOPE_HAS_IMAGE_SPECIFIC_DATA'
                is also present.  It indicates that this group, or one of
                its descendants (via placeholders), is associated with a
                specific spatial region of an image entity.  In this case,
                the spatial region is identified by the `region' member.
      */
    kds_image_entities *entities;
      /* [SYNOPSIS]
           This member is never NULL, but it may contain no image entities.
      */
    kdu_int32 max_discard_levels;
      /* [SYNOPSIS]
           Maximum number of discard levels for a codestream, before
           spatially sensitive metadata in this group (or any of its
           sub-groups) becomes irrelevant to the client.  This field is
           ignored if the `KDS_METASCOPE_HAS_IMAGE_SPECIFIC_DATA' flag is
           missing.
      */
    kdu_dims region;
      /* [SYNOPSIS]
           Ignored unless the `KDS_METASCOPE_HAS_REGION_SPECIFIC_DATA' flag
           is present.  This member identifies the location and dimensions of
           the region, expressed on the high resolution canvas of the
           codestreams listed in the `entities' list, where the position
           is expressed relative to the upper left hand corner of the
           image region on this canvas.
      */
    kdu_int32 sequence;
      /* [SYNOPSIS]
           This parameter determines the priority with which the metadata
           should be sequenced by `kdu_serve::generate_increments' if the
           present scope is found to intersect with the client's interests.
           This sequencing priority is expressed relative to other metadata
           and also relative to compressed image elements.
           [//]
           `sequence' values may be either negative or positive, where data
           having a smaller sequencing index is sequenced earlier than data
           with a larger index, when `kdu_serve::generate_increments' is
           preparing messages for transmission to a client.  For reference,
           all code-stream header data-bins have an implicit sequence index
           of 0 and the sequence number for precinct data-bin contributions
           is equal to the quality layer index of the first packet associated
           with the data being delivered.  Where code-stream and metadata
           have identical sequence numbers, the metadata is sequenced first.
      */
  };

/*****************************************************************************/
/*                             kds_metagroup                                 */
/*****************************************************************************/

struct kds_metagroup {
  /* [SYNOPSIS]
       This object plays a critical role in the implementation of any
       JPIP server which is able to support intelligent delivery of
       metadata together with imagery to its clients.  It is used to
       build a tree-structured manifest of the metadata which is available,
       together with pre-digested information concerning the relevance
       of that metadata to particular image code-streams, or specific
       regions components or resolutions of those code-streams.
       [//]
       The metadata tree need not contain an entry for every single metadata
       box from the original JP2-family file.  Instead, the granularity of the
       representation may be selected by the `kdu_serve_target' object
       which creates and owns the tree.
       [//]
       The elements of each metadata-bin are represented by a list of
       `kds_metagroup' objects, connected via their `next' members.  Each
       element in this list represents one or more boxes (all JP2-family
       files are built from boxes) at the top level of the contents of
       that data-bin.  The tree starts at metadata-bin 0, whose top-level
       boxes are the top-level boxes of the original JP2-family file.
       [//]
       Amongst the list of `kds_metagroup' objects which represent the
       contents of a metadata-bin, there may be one or more placeholder
       boxes.  Placeholders have a rich and powerful structure, as
       described in the JPIP standard (to become IS 15444-9).  In this
       case, the `phld' member of the `kds_metagroup' structure will
       point to another list of `kds_metagroup' objects, representing
       the data-bin to which the placeholder refers.  The `phld_bin_id'
       member provides the corresponding metadata-bin identifier.
  */
  public: // Member functions
    virtual ~kds_metagroup() { return; }
      /* [SYNOPSIS]
           Allows deletion from the base of a derived object.
      */
  public: // Data
    bool is_placeholder;
      /* [SYNOPSIS]
           True if the group consists of a placeholder.  Where a meta group
           contains a placeholder box , it must contain nothing other than that
           placeholder, and `phld_bin_id' holds the identifier of the
           metadata-bin to which the placeholder refers.  Also, in this
           case the `box_types' array and `last_box_type' member hold the
           type of the box which has been replaced by the placeholder (the
           type of the box whose header is embedded within the placeholder).
           [//]
           If the placeholder represents an incremental code-stream (one
           which is to be served via code-stream header, precinct, tile
           and metadata-bins, the `phld' member will be NULL, `phld_bin_id'
           will be 0, and the `box_types' and `last_box_type' members hold
           the type of an appropriate code-stream box such as "jp2c".
      */
    bool is_last_in_bin;
      /* [SYNOPSIS]
           True if this metadata group contains the final JP2 box in its
           data-bin.
      */
    bool is_rubber_length;
      /* [SYNOPSIS]
           True if the length of this metadata-group extends to the end
           of the file, but is not otherwise known.  In this case, `length'
           will be set to `KDU_INT32_MAX'.
      */
    kdu_int32 length;
      /* [SYNOPSIS]
           Indicates the total number of bytes from the relevant metadata-bin
           which belong to this group.  This value will be equal to
           `KDU_INT32_MAX' if `is_rubber_length' is true.
      */
    kdu_int32 num_box_types;
      /* [SYNOPSIS]
           Identifies the number of entries in the `box_types' array.
      */
    kdu_uint32 *box_types;
      /* [SYNOPSIS]
           Array of `num_box_types' box type codes, providing the types of
           all boxes which are represented by this object, including
           sub-boxes.  There need not be one entry here for each box, but
           only one entry for each different type of box, regardless of
           whether it appears at the top level or elsewhere.  The reason
           for providing this information is to allow `kdu_serve' to
           determine whether or not the present group of boxes contains
           any box types in which the client may be interested, where the
           client explicitly provides a `metareq' query field to identify
           box types which are of interest.
           [//]
           If `phld' is non-NULL, this list should include the type of the
           box whose header appears within the placeholder box, but it need
           not include the types of the boxes which appear within the
           placeholder's data-bin (see `phld_bin_id').
           [//]
           It can happen that this list is empty (`num_box_types'=0).  This
           happens if this group contains no visible box headers.  In
           particular, this happens if the group represents the contents of
           a metadata-bin whose header appears within a placeholder box
           in another metadata-bin (and hence another metadata-group).
      */
    kdu_int32 last_box_header_prefix;
      /* [SYNOPSIS]
           If the group represents only one box, this is the number of bytes
           in the header of that box and `num_box_types' must be 1.  However,
           if the group contains multiple boxes, this is the number of bytes
           from the start of the group up until the end of the header of the
           last box in the group.  The type of this last box is given by the
           `last_box_type' member.  By "last box" we do not just mean the
           last top level box, but rather the last box of any type listed in
           the `box_types' array, whether a sub-box or not.  If the client
           asks only for headers of a box in this group, the server will only
           be able to avoid sending the body of the last box in the group.
           [//]
           If the box contains a placeholder, `last_box_header_prefix' must be
           identical to `length', since the placeholder box itself represents
           the original box header.
           [//]
           If the box contains no box headers at all (`last_box_type'=0 and
           `num_box_types'=0), the value of `last_box_header_prefix' should
           be 0.
      */
    kdu_uint32 last_box_type;
      /* [SYNOPSIS]
           Holds the box type code for the last box in the group, whether
           it is a sub-box or not.  See `last_box_header_prefix' for more on
           the significance of final boxes.  If the last box is a placeholder,
           this member contains the type code of the box header which is
           embedded within the placeholder (the type of the original box
           which was replaced by the placeholder).  The "phld" box type
           itself never appears within the `box_types' array or the
           `last_box_type' member.  If the `box_types' list is empty,
           meaning that this object represents only the raw contents of
           a box whose header is in the placeholder of another metadata-bin,
           then `last_box_type' will also be 0.
      */
    kds_metagroup *next;
      /* [SYNOPSIS]
           Points to the next entry in the list of metadata groups which
           are at the top level of a given metadata-bin.
      */
    kds_metagroup *phld;
      /* [SYNOPSIS]
           NULL unless this group consists of a placeholder box which
           references another metadata-bin.  Note that incremental
           code-stream placeholders stand for a code-stream which is
           to be replaced by header and precinct data-bins, synthesized by
           `kdu_serve'.  In this case, the `phld' member should be NULL,
           although `is_placeholder' will be true.  This condition uniquely
           identifies the presence of an incremental code-stream placeholder.
      */
    kdu_long phld_bin_id;
      /* [SYNOPSIS]
           This member is valid if and only if `phld' is non-NULL.  It
           provides the data-bin identifier of the new metadata-bin
           whose contents are represented by the meta group list
           referenced by `phld'.
      */
    kds_metascope *scope;
      /* [SYNOPSIS]
           Points to a single `kds_metascope' object, which is used by
           `kdu_serve' to determine whether or not the metadata group is
           relevant to a client's interests and, if so, how the metadata
           should be sequenced relative to other metadata and compressed
           image data.
           [//]
           If the object represents the contents of another box whose header
           appears in a placeholder somewhere, the `scope' pointer will
           generally point back to the scope of the group which contains
           the placeholder, but it need not.  In any event the `scope'
           pointer should never be NULL.
      */
  };

/*****************************************************************************/
/*                             kdu_serve_target                              */
/*****************************************************************************/

class kdu_serve_target {
  /* [BIND: reference]
     [SYNOPSIS]
     Abstract base class providing the interface functions to be implemented by
     real JPEG2000 target managers.  A JPEG2000 target is any object (usually
     a JPEG2000 file) which provides access to one or more JPEG2000
     code-streams and/or JPEG2000 metadata.  Rather than supplying the file
     itself to `kdu_serve', the idea is to supply an appropriately derived
     `kdu_serve_target' object.  This has many advantages.  Firstly, it means
     that `kdu_serve' does not have to actually understand specific file
     formats.  Secondly, it allows the file to be partially digested outside
     the main service object and the results to be shared amongst multiple
     `kdu_serve' objects, and also cached locally to disk.  All such matters
     are application dependent, so they are best moved outside the
     implementation of `kdu_serve' itself.
  */
  public: // Member functions
    virtual ~kdu_serve_target() { return; }
      /* [SYNOPSIS]
           Virtual destructor allows destruction of the derived class from
           its abstract interface, declared here.
      */
    virtual int *get_codestream_ranges(int &num_ranges,
                                       int compositing_layer_idx=-1)=0;
      /* [SYNOPSIS]
           Returns an array of code-stream ID ranges.  The number of ranges
           in the returned array is returned via the `num_ranges' argument.
           Each range consists of a pair of consecutive integers in the
           array, the first of which is the identifier of the
           first code-stream in the range, while the second is the identifier
           of the last code-stream in the range (inclusive).
           [//]
           The returned code-stream ranges include the identifiers of
           all JPEG2000 code-streams as well as all other code-streams
           whose image data can be accessed via the JPIP protocol.
           Non-JPEG2000 code-streams are transported as raw metadata boxes.
           Whether or not a code-stream identifier corresponds to a
           JPEG2000 code-stream which can be served as an incremental
           code-stream (via code-stream header and precinct data-bins), may
           be determined by calling `attach_to_codestream'.
         [ARG: compositing_layer_idx]
           If negative, the returned codestream ranges describe all
           code-streams available from the data source. Otherwise,
           the returned ranges describe all code-streams belonging to
           the indicated compositing layer.  Raw codestreams and simple
           JP2 files are considered to have exactly one compositing layer,
           containing the one and only codestream.  JPX data sources may
           have many compositing layers, each of which may reference more
           than one code-stream.  If the indicated compositing layer does
           not exist, the function returns NULL, setting `num_ranges' to 0.
      */
    virtual kdu_codestream attach_to_codestream(int codestream_id,
                                                kd_serve *thread_handle)=0;
      /* [SYNOPSIS]
           Use this function to gain access to one of the target object's
           code-streams.  The function will return an empty interface (one
           whose `kdu_codestream::exists' function returns false) if
           `codestream_id' refers to a code-stream which is to be served as
           a raw metadata box, or if `codestream_id' does not belong to any
           of the ranges returned via `get_codestream_ranges'.
           [//]
           Otherwise, the returned object provides an interface to an open
           `kdu_codestream', created for reading.
           [//]
           Note carefully, that you should not actually use the code-stream
           interface without first locking it with the `lock_codestreams'
           function.  This is because multiple `kdu_serve' objects running
           in different threads of execution might be attempting to access
           the object simultaneously.
           [//]
           Note also that the `kdu_codestream' machinery should have been
           configured internally to operate in the `persistent' mode.
        [ARG: thread_handle]
           You must supply a physical address which is unique to the
           present thread of execution; this will normally be the `kd_serve'
           object embedded inside a `kdu_serve' interface, so we declare
           the address as being a pointer to `kd_serve' to as to encourage
           compiler checking for consistency.  However, the object is never
           actually inspected; only its address is used.
           [//]
           The `thread_handle' is used to keep track of who has acquired a
           lock on the code-stream, enabling the lock to be released in the
           event that the owner detaches from the code-stream without first
           releasing the lock.  This can happen if an error condition caused
           an exception in the owner of the lock.
      */
    virtual void detach_from_codestream(int codestream_id,
                                        kd_serve *thread_handle)=0;
      /* [SYNOPSIS]
           Call this function once you have no further use for the
           `kdu_codestream' interface returned via `attach_to_codestream'.
           The internal machinery may destroy the code-stream interface, so
           be sure not to call this function unless you have completely
           ceased using the code-stream machinery.
           [//]
           It is only necessary to call this function if a previous call to
           `attach_to_codestream' with this `codestream_id' value returned a
           non-empty interface.  However, it is safe to call the function
           even if the call to `attach_to_codestream' did return an
           empty interface.
         [ARG: thread_handle]
           Must be the same address which was passed into the
           `attach_to_codestream' function.
      */
    virtual void lock_codestreams(int num_codestreams,
                                  int *codestream_indices,
                                  kd_serve *thread_handle)=0;
      /* [SYNOPSIS]
           Call this function before accessing a `kdu_codestream' object
           returned by `attach_to_codestream' to ensure that other
           threads of execution do not access it at the same time.
           [//]
           To avoid potential deadlocks, the caller must release all locked
           codestreams before you call this function again.  For this
           reason, the caller may wish to lock multiple codestreams at a
           time.  The function returns only once all requested codestreams
           have been locked.  If an error occurs, an appropriate exception
           will be thrown.
        [ARG: thread_handle]
           Must be the same address which was passed into the
           `attach_to_codestream' function.  This may or may not be used,
           depending on the implementation.
      */
    virtual void release_codestreams(int num_codestreams,
                                     int *codestream_indices,
                                     kd_serve *thread_handle)=0;
      /* [SYNOPSIS]
           See `lock_codestreams' for an explanation.  The `thread_handle'
           address must be identical to that used when `lock_codestreams' was
           called.
      */
    virtual int get_num_context_members(int context_type, int context_idx,
                                        int remapping_ids[])
      { return 0; }
      /* [SYNOPSIS]
           This is one of four functions which work together to translate
           codestream contexts into codestreams, and view windows expressed
           relative to a codestream context into view windows expressed
           relative to the individual codestreams associated with those
           contexts.  A codestream context might be a JPX compositing layer
           or an MJ2 video track, for instance.  The context is generally
           a higher level imagery object which may be represented by one
           or more codestreams.  The context is identified by the
           `context_type' and `context_idx' arguments, but may also be
           modified by the `remapping_ids' array, as explained below.
         [RETURNS]
           The number of codestreams which belong to the context.  If the
           object is unable to translate this context, it should return 0.
         [ARG: context_type]
           Identifies the type of context.  Currently defined context type
           identifiers (in "kdu_client_window.h") are
           `KDU_JPIP_CONTEXT_JPXL' (for JPX compositing layer contexts) and
           `KDU_JPIP_CONTEXT_MJ2T' (for MJ2 video tracks).  For more
           information on these, see `kdu_sampled_range::context_type'.
         [ARG: context_idx]
           Identifies the specific index of the JPX compositing layer,
           MJ2 video track or other context.  Note that JPX compositing
           layers are numbered from 0, while MJ2 video track numbers start
           from 1.
         [ARG: remapping_ids]
           Array containing two integers, whose values may alter the
           context membership and/or the way in which view windows are to
           be mapped onto each member codestream.  For more information on
           the interpretation of remapping id values, see the comments
           appearing with `kdu_sampled_range::remapping_ids' and
           `kdu_sampled_range::context_type'.
           [//]
           If the supplied `remapping_ids' values cannot be processed as-is,
           they may be modified by this function, but all of the other
           functions which accept a `remapping_ids' argument (i.e.,
           `get_context_codestream', `get_context_components' and
           `perform_context_remapping') must be prepared to operate
           correctly with the modified remapping values.
      */
    virtual int get_context_codestream(int context_type,
                                       int context_idx, int remapping_ids[],
                                       int member_idx)
      { return -1; }
      /* [SYNOPSIS]
           See `get_num_context_members' for a general introduction.  The
           `context_type', `context_idx' and `remapping_ids' arguments have
           the same interpretation as in that function.  The `member_idx'
           argument is used to enumerate the individual codestreams which
           belong to the context.  It runs from 0 to the value returned by
           `get_num_context_members' less 1.
         [RETURNS]
           The index of the codestream associated with the indicated member
           of the codestream context.  If the object is unable to translate
           this context, it should return -1.
      */
    virtual const int *get_context_components(int context_type,
                                              int context_idx,
                                              int remapping_ids[],
                                              int member_idx,
                                              int &num_components)
      { return NULL; }
      /* [SYNOPSIS]
           See `get_num_context_members' for a general introduction.  The
           `context_type', `context_idx', `remapping_ids' and `member_idx'
           arguments have the same interpretation as in
           `get_context_codestream', except that this function returns a
           list of all image components from the codestream which are used
           by this context.
         [RETURNS]
           An array with one entry for each image component used by the
           indicated codestream context, from the specific codestream
           associated with the indicated context member.  This is the
           same codestream whose index is returned `get_context_codestream'.
           Each element in the array holds the index (starting from 0) of
           the image component.  If the object is unable to translate
           the indicated context, it should return NULL.
         [ARG: num_components]
           Used to return the number of elements in the returned array.
      */
    virtual bool perform_context_remapping(int context_type,
                                           int context_idx,
                                           int remapping_ids[],
                                           int member_idx,
                                           kdu_coords &resolution,
                                           kdu_dims &region)
      { return false; }
      /* [SYNOPSIS]
           See `get_num_context_members' for a general introduction.  The
           `context_type', `context_idx', `remapping_ids' and `member_idx'
           arguments have the same interpretation as in
           `get_context_codestream', except that this function serves to
           translate view window coordinates.
         [RETURNS]
           False if translation of the view window coordinates is not
           possible for any reason.
         [ARG: resolution]
           On entry, this argument represents the full size of the image, as
           it is to be rendered within the indicated codestream context.  Upon
           return, the values of this argument will be converted to represent
           the full size of the codestream, required to render the context at
           the full size indicated on entry.
         [ARG: region]
           On entry, this argument represents the location and dimensions of
           the view window, expressed relative to the full size image
           at the resolution given by `resolution'.  Upon return, the
           values of this argument will be converted to represent the
           location and dimensions of the view window within the codestream.
      */
    virtual const kds_metagroup *get_metatree()=0;
      /* [SYNOPSIS]
           Returns the metadata partition tree.  The tree must be complete;
           it is not permitted to grow later.  If the target consists of one
           or more raw code-streams without any metadata at all, the function
           should still return a non-NULL pointer to a tree with exactly one
           `kds_metagroup' object, whose length is 0.
      */
    virtual int read_metagroup(const kds_metagroup *group, kdu_byte *buf,
                               int offset, int num_bytes)=0;
      /* [SYNOPSIS]
           Reads a range of bytes from the data associated with a given
           `kds_metagroup' object.  This group may represent a single
           JP2-family box at the top level of its corresponding metadata-bin.
           However, it may also represent a collection of boxes.
         [RETURNS]
           The number of bytes actually read.  This may be less than
           `num_bytes' only if not all data for the relevant JP2 boxes is
           available locally at the current time.  This enables
           JPIP proxies to be implemented, which retrieve data from another
           server.  If the data is not currently available, it will likely
           become available in the future, but the proxy server should
           proceed to serve up other data to its local clients, rather than
           wait.
      */
    virtual int find_roi(int stream_id, const char *roi_name=NULL)
      { return 0; }
      /* [SYNOPSIS]
           Tries to match the supplied `roi_name' string with a known
           region of interest, as defined by the codestream whose index is
           given by `stream_id'.  If a match is found, the function returns
           a positive integer identifier, which may be used to retrieve
           further details about the ROI.  Otherwise, the function returns 0.
         [RETURNS]
           A valid ROI is always strictly positive.  0 indicates that there
           was no match.
         [ARG: roi_name]
           If NULL, the function returns the index of a default (or
           dynamically assigned ROI).  If none exists, the function again
           returns 0.
      */
    virtual const char *get_roi_details(int index, kdu_coords &resolution,
                                        kdu_dims &region) { return NULL; }
      /* [SYNOPSIS]
           Supply an index previously recovered via `find_roi' to retrieve
           details about that ROI.  The supplied `index' must be non-negative.
         [RETURNS]
           The name of the ROI.  This is identical to the name which was
           matched by `find_roi'.  If the supplied `index' is not valid,
           the function returns NULL.
         [ARG: resolution]
           Used to return the resolution (full image size) associated with
           the ROI.  Has the same interpretation as `kdu_window::resolution'.
         [ARG: region]
           Used to return the image region associated with the ROI.  Has
           the same interpretation as `kdu_window::region'.
      */
  };

/*****************************************************************************/
/*                                kdu_serve                                  */
/*****************************************************************************/

class kdu_serve {
  /* [BIND: reference]
     [SYNOPSIS]
       Provides the platform independent component of a powerful and
       flexible JPIP server system.
  */
  public: // Member functions
    kdu_serve() { state = NULL; }
    ~kdu_serve() { destroy(); }
    KDU_AUX_EXPORT void
      initialize(kdu_serve_target *target, int max_chunk_size,
                 int chunk_prefix_bytes, bool ignore_relevance_info=false);
      /* [SYNOPSIS]
           The real construction takes place here -- somewhat safer, since
           we prefer to avoid having exceptions thrown from within the
           constructor itself.  The function configures the server to work
           with the existing `kdu_serve_target' object, which should have
           already been created and configured as necessary.
           [//]
           A single `target' may be shared between multiple 'kdu_serve'
           objects, although `kdu_serve_target::lock_codestreams' and
           `kdu_serve_target::release_codestreams' may need to implement
           appropriate synchronization mechanisms if the `kdu_serve'
           objects are to operate in different threads of execution.
           In general, it is important that all other users of the `target'
           object close their open tiles while calls to the present function
           (`initialize') and the `generate_increments' function are in
           progress.
           [//]
           Once the `kdu_serve' object has been initialized, the application
           should call the `set_window' function to identify the image
           code-streams, image components, resolution, spatial region, etc.,
           which are of interest.  The application then issues one or more
           calls to `generate_increments', letting `kdu_serve' itself decide
           what compressed data and metadata increments should be assembled
           (usually, on behalf of an interactive client).  In interactive
           client-server applications, the serving application will also
           listen for changes in the client's window of interest, issuing new
           calls to `set_window', as appropriate.  This affects the present
           object's decisions as to what compressed data should actually be
           packaged up in response to subsequent calls to
           `generate_increments'.
         [ARG: max_chunk_size]
           This argument determines the maximum size of the data chunks into
           which the `generate_increments' function packs compressed data.
           This size includes any prefix which the application reserves in
           its calls to `generate_increments'.
         [ARG: chunk_prefix_bytes]
           Indicates the number of bytes which should be reserved at the
           beginning of each data chunk, for application use.  This value will
           be written into the `kds_chunk::prefix_bytes' member of each
           chunk in the list returned by `generate_increments'.
         [ARG: ignore_relevance_info]
           If this argument is true, the object will serve all relevant
           precinct data layer by layer, with the lowest resolution subbands
           appearing before the higher resolution subbands, within each layer.
           This is the behaviour offered by previous implementations of the
           object.  On the other hand, if this argument is false, the
           function will process the precincts in order of their relevance
           (percentage overlap with the region of interest).  Moreover, if
           information is available concerning the rate-distortion slope
           thresholds associated with the code-stream quality layers, this
           information is used in conjunction with the relevance factors to
           re-sequence the original code-stream packets, in a manner which
           is expected to reduce the distortion within the window of interest
           as quickly as possible.  This often means that lower resolution
           precincts and precincts with relatively little overlap with the
           window tend to get sequenced later (sometimes several layers later)
           than they otherwise would.
      */
    KDU_AUX_EXPORT void destroy();
      /* [SYNOPSIS]
           Does nothing unless the object has already been initialized.
           Allows the object to be re-initialized.
      */
    KDU_AUX_EXPORT void set_window(kdu_window &window);
      /* [SYNOPSIS]
           Call this function to update the image code-streams, components,
           resolution, spatial region and number of quality layers
           (together designated "the window") which are to be used in
           responding to future calls to `generate_increments'.
           The server does not care how the window parameters are determined,
           but it may need to modify the window parameters in order to
           satisfy internal resource constraints.  The `get_window' function
           may be used to determine the actual window parameters which are
           being used at any given point.
           [//]
           Upon initialization, the window is set to refer to all image
           components of the first available code-stream, taken over the
           whole image, but viewed at the lowest available resolution (usually
           a thumbnail).  Of course, these initial defaults may be overridden
           at any time, even before the first call to `generate_increments'.
           [//]
           Although this function may be called as often as needed, there
           is a computational overhead associated with changes in the region
           of interest.  The next time `generate_increments' is called, it
           will have to build a new list of active precincts, which might
           easily take a milisecond or more.  Perhaps even more importantly,
           code-block data for the new precincts will need to be loaded from
           `kdu_codestream' objects accessed via
           `kdu_serve_target::access_codestream'.  This may indirectly trigger
           disk accesses, packet parsing and cache management activities.
           For this reason, once you have committed to calling
           `generate_increments' in response to some new window, you should
           aim to generate a significant amount of data for that window before
           moving on to a new window.  The actual minimum amount of data which
           the application chooses to generate for one window before moving to
           another might (ideally) be based upon simultaneous estimates of the
           computational complexity and the communication bandwidth to a
           remote client.
         [ARG: window]
           The interpretation of this object and its members is carefully
           described in the comments accompanying the `kdu_window'
           object.
      */
    KDU_AUX_EXPORT bool get_window(kdu_window &window);
      /* [SYNOPSIS]
           Retrieves the window parameters which are to be used in the next
           call to `generate_increments' unless `set_window' is called first.
           [//]
           The principle reason for calling this function is to learn about
           any modifications which the server made to the window parameters
           supplied in a previous call to `set_window'.  As noted in
           connection with that function, the server may need to make such
           changes in order to satisfy its own resource constraints.  In
           particular, very large windows (large spatial regions and/or
           large number of components) may require excessive memory to be
           allocated by the server.
           [//]
           It is important to realize that the server may not make any
           necessary alterations to the window parameters supplied to
           `set_window' until after the first subsequent call to
           `generate_increments', or possibly `augment_cache_model' or
           `truncate_cache_model'.  Thus, the most meaningful point at which
           to call the present function is after the first (or a subsequent)
           call to `generate_increments' which follows a `set_window' call.
         [RETURNS]
           True if the window is still active, in the sense that not all
           data which is relevant to the window has yet been packaged up
           in calls to `generate_increments'.  If the function returns
           false, subsequent calls to `generate_increments' are guaranteed
           to return NULL, meaning that the window has been fully served.
      */
    KDU_AUX_EXPORT void
      augment_cache_model(int databin_class, int stream_min,
                          int stream_max, kdu_long bin_id,
                          int available_bytes, int available_packets=0);
      /* [SYNOPSIS]
           The object maintains a model of the cache maintained by a client
           (real or hypothetical) which immediately caches all compressed
           data packaged up by the `generate_increments' function.  The
           present function provides a mechanism for identifying additional
           data which are available in the client cache, even though they
           may not have been delivered by way of the `generate_increments'
           function.
           [//]
           This function informs the `kdu_serve' object that its cache model
           should include the initial `available_bytes' of the data-bin
           whose in-class identifier is `bin_id', whose class is given
           by the `databin_class' argument, and whose code-stream lies within
           the range given by the `stream_min' and `stream_max' arguments.
           If the cache model already includes at least this number of bytes
           for the indicated data-bin, the function has no effect.
           [//]
           If `available_bytes' is negative the server is being informed that
           it should assume that the  entire contents of the relevant data-bin
           are already available at the client.
           [//]
           If `available_bytes'=0 and `available_packets'>0, the server is
           instead being asked to augment its cache model to the whole
           packet boundary immediately following the first `available_packets'
           of a precinct data-bin.  The `available_packets' argument is
           ignored if `available_bytes' is non-zero.
           [//]
           The `stream_min' and `stream_max' arguments identify a range of
           code-streams over which the cache model truncation statement
           should be applied.  Frequently, this range will contain only one
           code-stream; however, it may contain a larger number of streams.
           The code-streams actually affected by the command are
           restricted to those which belong to the object's model set (the set
           of code-streams for which a cache model is currently being applied).
           Unless specific action is taken to alter the model set, it will
           include all code-streams which have been referenced by window
           commands (including this one), but not necessarily all code-streams
           referenced by cache model statements.  This means that the
           function call may have no effect if the referenced code-stream
           or streams lie outside the current model set.  Note, however,
           that the client may augment the server's model set by appropriately
           configuring the `kdu_window' object supplied to `set_window'.
           The present object may also alter the model set, removing some
           code-streams where its resources do not permit simultaneous
           modeling of all streams which might be in the client's model set.
           These changes will be reflected in the window returned via
           `get_window'.
           [//]
           If `bin_id' is negative, the function is automatically applied to
           all data-bins which belong to the current window.  This necessarily
           means that the function's effects are limited to those code-streams
           which lie within the scope of the current window, in addition to
           the restrictions provided by the `stream_min' and `stream_max'
           arguments.
         [ARG: databin_class]
           Should be one of `KDU_MAIN_HEADER_DATABIN',
           `KDU_TILE_HEADER_DATABIN', `KDU_PRECINCT_DATABIN',
           `KDU_TILE_DATABIN' or `KDU_META_DATABIN'.
         [ARG: stream_min]
           Lower bound for the range of code-stream identifiers being
           referenced by this function.  See notes above on the interaction
           between code-stream ranges and model sets.  This argument is
           ignored if `databin_class' is KDU_META_DATABIN.
         [ARG: stream_max]
           Upper bound for the range of code-stream identifiers being
           referenced by this function.  See notes above on the interaction
           between code-stream ranges and model sets.  This argument is
           ignored if `databin_class' is KDU_META_DATABIN.
         [ARG: bin_id]
           Unique identifier of the data-bin within its class.  If negative,
           all data-bins relevant to the current window are adjusted in
           accordance with the other arguments.
         [ARG: available_bytes]
           Minimum number of bytes for this data-bin which should be
           reflected in the cache model.  If negative, the client
           is to be treated as having a complete cached copy of the entire
           data-bin.
         [ARG: available_packets]
           This argument is ignored unless `databin_class' is
           `KDU_PRECINCT_DATABIN' and `available_bytes' is 0.  In this case,
           the server is being asked to advance its cache model for the
           relevant precinct to the point immediately after the first
           `available_packets', unless the cache model already reflects a
           location beyond this point.  A negative value is interpreted
           as if `available_bytes' were negative, meaning that the entire
           contents of the data-bin are to be marked as cached.
      */
    KDU_AUX_EXPORT void
      truncate_cache_model(int databin_class, int stream_min,
                           int stream_max, kdu_long bin_id,
                           int available_bytes, int available_packets=0);
      /* [SYNOPSIS]
           The object manages a model of the cache maintained by a client
           (real or hypothetical) which immediately caches all compressed
           data packaged up by the `generate_increments' function.  The
           present function provides a mechanism for deleting elements from
           this cache model, either in whole or in part.  This may be
           useful if the client needs to delete elements from its cache to
           limit memory consumption.  It also provides a mechanism for the
           client to explicitly inform the server of the elements which
           it wants.  To do this, the client may first add all relevant
           elements to the server's cache model, using a negative
           `bin_id' argument in the call to `augment_cache_model';
           it may then remove the elements which it actually wants to receive,
           using the present function.  This strategy for forming client
           requests will often be inefficient, but it gives the client
           the control which may be needed to access the image in unusual
           ways.
           [//]
           The `stream_min' and `stream_max' arguments identify a range of
           code-streams over which the cache model truncation statement
           should be applied.  Frequently, this range will contain only one
           code-stream; however, it may contain a larger number of streams.
           The code-streams actually affected by the command are
           restricted to those which belong to the object's model set (the set
           of code-streams for which a cache model is currently being applied).
           Unless specific action is taken to alter the model set, it will
           include all code-streams which have been referenced by window
           commands (including this one).  However, the client may augment
           the server's model set by appropriately configuring the
           `kdu_window' object supplied to `set_window'.  The present object
           may also alter the model set, removing some code-streams where
           its resources do not permit simultaneous modeling of all streams
           which might be in the client's model set.  These changes will
           be reflected in the window returned via `get_window'.
           [//]
           The function informs the server that its cache model should
           include at most the initial `available_bytes' of the data-bin
           whose in-class identifier is `bin_id' and whose class is given by
           the `databin_class' argument.  If the cache model currently has
           fewer bytes for this data-bin, the function has no effect.  If
           `available_bytes' is less than or equal to zero, the object is
           being informed that it should remove the data-bin from its cache
           model, meaning that it will be re-transmitted from scratch when
           serving any window for which the data-bin is relevant.
           [//]
           The `available_packets' argument is ignored, unless `databin_class'
           is `KDU_PRECINCT_DATABIN' and `available_bytes' is 0.  In this
           case `available_packets' indicates the maximum number of packets
           from the relevant precinct data-bin, which should be kept in the
           server's cache model.  Note that this means that you must choose
           to specify either a packet limit or a byte limit.  There is
           no way to specify non-trivial limits on both the length and the
           number of packets simultaneously.  If you call this function
           multiple times specifying alternately a byte limit and a packet
           limit, it is possible that the internal machinery will use only
           the most recently specified limit, rather than taking the
           minimum of the two.  This is because reconciling byte and packet
           limits may be a computationally intensive task if the precinct
           data does not already happen to be in memory.
           [//]
           If `bin_id' is negative, the function is automatically applied to
           all data-bins which belong to the current window.  In this case,
           the code-stream range supplied via `stream_min' and `stream_max'
           is also restricted to those code-streams which are referenced by
           the current window.
         [ARG: databin_class]
           Should be one of `KDU_MAIN_HEADER_DATABIN',
           `KDU_TILE_HEADER_DATABIN', `KDU_PRECINCT_DATABIN',
           `KDU_TILE_DATABIN' or `KDU_META_DATABIN'.
         [ARG: stream_min]
           Lower bound for the range of code-stream identifiers being
           referenced by this function.  See notes above on the interaction
           between code-stream ranges and model sets.  This argument is
           ignored if `databin_class' is KDU_META_DATABIN.
         [ARG: stream_max]
           Upper bound for the range of code-stream identifiers being
           referenced by this function.  See notes above on the interaction
           between code-stream ranges and model sets.  This argument is
           ignored if `databin_class' is KDU_META_DATABIN.
         [ARG: bin_id]
           Unique identifier of the data-bin within its class.  If negative,
           all data-bins relevant to the current window are adjusted in
           accordance with the other arguments.
         [ARG: available_bytes]
           Maximum number of bytes for this data-bin which should be
           reflected in the cache model.  Negative values are
           treated as if 0.
         [ARG: available_packets]
           This argument is ignored unless `databin_class' is
           `KDU_PRECINCT_DATABIN' and `available_bytes'<=0.  In this case, a
           packet limit applies instead of a byte limit.  The server is being
           asked to truncate its cache model to the indicated packet
           boundary.  Negative values are treated as if 0.
      */
    KDU_AUX_EXPORT void
      augment_cache_model(int stream_min, int stream_max,
                          int tmin, int tmax, int cmin, int cmax,
                          int rmin, int rmax, int pmin, int pmax,
                          int available_bytes, int available_packets);
      /* [SYNOPSIS]
           This form of the overloaded `augment_cache_model' function has
           the same interpretation as the first, except in the following
           two respects: 1) it applies only to precinct data-bins; and 2) the
           relevant precincts are identified through their explicit
           coordinates: their tile, component, resolution and precinct
           indices.  The actual values assumed by the indices are controlled
           by lower and upper bounds.  For example, `cmin' and `cmax' identify
           lower and upper bounds (inclusive) on the image components being
           referenced.  Tile and precinct indices are treated somewhat
           differently, since both refer to 2D grids.  In this case, a
           range of tiles or precincts refers to the rectangular region whose
           upper left hand corner has the index supplied by the lower bound
           and whose lower right hand corner has the index supplied by the
           upper bound of the range.
           [//]
           The upper bound of any range may, of course, be equal to the
           lower bound, in which case we refer to it as a "simple range".
           If the upper bound is larger than the lower bound, the range is
           said to be "complex".  If the lower bound is negative, the range
           refers to all values of the relevant coordinate, and is also said
           to be complex.
           [//]
           Importantly, if any of the ranges is complex,
           the effect of the function may be restricted to those
           precincts which actually contribute to the current window (as
           specified via the most recent `set_window' call, but possibly
           modified by the server -- modifications may be determined by
           calling `get_window').
           [//]
           Due to the potential for ambiguous interpretation, complex ranges
           are allowed within the JPIP protocol only for stateless queries.
           Since the effects of stateless cache model manipulation do not
           extend beyond the query in which they appear, any impact on
           cache elements which do not belong to the requested window is
           irrelevant.
      */
    KDU_AUX_EXPORT void
      truncate_cache_model(int stream_min, int stream_max,
                           int tmin, int tmax, int cmin, int cmax,
                           int rmin, int rmax, int pmin, int pmax,
                           int available_bytes, int available_packets);
      /* [SYNOPSIS]
           This form of the overloaded `truncate_cache_model' function has
           the same interpretation as the first, except in the following
           two respects: 1) it applies only to precinct data-bins; and 2) the
           relevant precincts are identified through their explicit
           coordinates: their tile, component, resolution and precinct
           indices.  The actual values assumed by the indices are given by
           lower and upper bounds.  For example, `cmin' and `cmax' identify
           lower and upper bounds (inclusive) on the image components being
           referenced.  Tile and precinct indices are treated somewhat
           differently, since both refer to 2D grids.  In this case, a
           range of tiles or precincts refers to the rectangular region whose
           upper left hand corner has the index supplied by the lower bound
           and whose lower right hand corner has the index supplied by the
           upper bound of the range.
           [//]
           The upper bound of any range may, of course, be equal to the
           lower bound, in which case we refer to it as a "simple range".
           If the upper bound is larger than the lower bound, the range is
           said to be "complex".  If the lower bound is negative, the range
           refers to all values of the relevant coordinate, and is also said
           to be complex.
           [//]
           Importantly, if any of the ranges is complex,
           the effect of the function may be restricted to those
           precincts which actually contribute to the current window (as
           specified via the most recent `set_window' call, but possibly
           modified by the server -- modifications may be determined by
           calling `get_window').
           [//]
           Due to the potential for ambiguous interpretation, complex ranges
           are allowed within the JPIP protocol only for stateless queries.
           Since the effects of stateless cache model manipulation do not
           extend beyond the query in which they appear, any impact on
           cache elements which do not belong to the requested window is
           irrelevant.
      */
    KDU_AUX_EXPORT bool get_image_done();
      /* [SYNTHESIS]
           Call this function to determine whether the entire target
           has been packaged up through calls to the `generate_increments'
           function.  If this is the case, there is no point in calling
           `generate_increments' ever again and hence no point in calling
           `set_window' again.
           [//]
           It should be noted that the object bases its information upon
           an internal cache model, which reflects the contents of a
           client cache which is presumed to have been instantaneously
           filled with all data packaged up by `generate_increments'.  The
           cache model is also affected by calls to
           `augment_cache_model' and `truncate_cache_model'.
      */
    KDU_AUX_EXPORT int
      push_extra_data(kdu_byte *data, int num_bytes,
                      kds_chunk *chunk_list=NULL);
      /* [SYNOPSIS]
           This function may be used by server applications to insert
           additional data into the list of chunks which are served by
           the object's `generate_increments' function.
           [//]
           The object maintains an internal queue of outstanding data which
           must be delivered prior to any further compressed data.  When
           `generate_increments' is called, it first pulls data from the
           head of this outstanding data queue; only once it is empty, can
           compressed data increments be generated and included in the
           list of data chunks which it returns.  Calling the present function
           augments this internal outstanding data queue.
           [//]
           If the `chunk_list' argument is non-NULL, the function behaves
           slightly differently, appending the data to the existing list of
           `kds_chunk' buffers, all of which were previously returned by
           one or more calls to `generate_increments', and augmenting this
           list as necessary.  In this case, the data is not added to the
           internal queue of outstanding data.
           [//]
           The extra data supplied to this function is always written as
           a single contiguous block, which must be contained in a single
           data chunk. If the data cannot be appended to an existing chunk
           without overflowing, a new chunk is created.  If the data is too
           long to fit in a single chunk at all, the function generates an
           error through `kdu_error'.  Of course, this is undesirable, so
           you should make sure the amount of data you supply does not exceed
           the maximum chunk length, minus the chunk prefix size, as supplied
           to the present object's constructor.
         [RETURNS]
           The number of bytes which are left in the last chunk written by
           this function.  If `num_bytes' is 0, the function does nothing
           except return the number of bytes which are left in the last
           element of the internal extra data chunk list, or the
           `chunk_list', if non-NULL.  The return value could be 0,
           if the last chunk in the list is full.
         [ARG: data]
           Points to a memory block containing the extra data.  This may
           be null only if `num_bytes' is 0.
         [ARG: num_bytes]
           Number of bytes of extra data in the `data' buffer.  May be zero,
           if you are only interested in the return value.
         [ARG: chunk_list]
           List of `kds_chunk' objects returned by one or more previous
           calls to `generate_increments', which have not been returned to
           the object using `release_chunks'.  This list may be NULL, if you
           want the extra data to be appended to the object's internal
           extra data queue, for inclusion in the chunk lists returned by
           subsequent calls to `generate_increments'.
      */
    KDU_AUX_EXPORT kds_chunk *
      generate_increments(int suggested_data_bytes, 
                          int &max_data_bytes, bool align=false,
                          bool use_extended_message_headers=false,
                          bool decouple_chunks=true,
                          kds_id_encoder *id_encoder=NULL);
      /* [SYNOPSIS]
           This function returns a linked list of `kds_chunk' objects,
           representing new elements of the JPEG2000 compressed imagery
           or metadata which is relevant to the window most recently
           passed to `set_window'.  The chunks contain one or more whole
           JPIP messages, each of which represents a byte range from one
           such element.  Possible elements are metadata-bins, precinct
           data-bins, code-stream main header data-bins and tile header
           data-bins.  The object does not currently support delivery of
           tile data-bins, which offer substantially less flexibility.
           It is possible that some messages which have previously been
           generated by this function will overlap with the new messages,
           resulting in redundant transmission by the server application.
           This may happen if the `truncate_cache_model' function is used
           to clear the server's memory of information it may previously
           have generated.
           [//]
           Server applications typically call this function repeatedly,
           indicating the total number of data bytes which they would like
           to be contained in the returned chunks.  Note carefully, that
           the `suggested_data_bytes' limit includes only the bodies of the
           messages contained in the returned list of `kds_chunk' objects.
           Messages and their headers are described by the JPIP standard
           (currently committee draft, but will become IS 15444-9).
           [//]
           Applications will ideally select `suggested_data_bytes' on the basis
           of their current estimates regarding the communication bandwidth.
           If the application attempts to transfer too much data from the
           current window to a remote client, the client's window of interest
           may change before the data can be transmitted.  On the other hand,
           each call to this function can be quite expensive, so servers will
           not want to call `generate_increments' more frequently than
           necessary.  Ideally, the function is called only once, between
           changes in the client's window of interest, but changes in the
           client's interests are not generally predictable in an interactive
           application.
           [//]
           Prior to return, the `kdu_serve' object updates its internal cache
           model, based on the assumption that all of the chunks will be
           successfully transmitted to a client which inserts all relevant
           data into its cache.  In some cases, the application may opt not
           to send some or all of the chunks to a client.  For example,
           if a remote user changes his/her window of interest dramatically
           before all outstanding chunks have been transmitted, the server
           application may choose to abandon some of the chunks.  This is
           acceptable, so long as the application informs the `kdu_serve'
           object that it has abandoned these chunks.  This may be done by
           asserting `check_abandoned'=true in the call to `release_chunks'.
           In this case, the application is expected to have marked all
           of the chunks which were reliably transmitted by setting their
           `abandoned' fields to false.
           [//]
           If the application has used the `push_extra_data' function to
           push extra data onto the internal extra data queue (as opposed
           to pushing it onto the end of a list of chunks returned by this
           function), the extra data will be included in the first chunk
           returned by this function, until there is no extra data left.
           In this case, it is possible that the present function will
           return no compressed image or metadata elements at all, since the
           byte limit may be exhausted by the extra data.
         [RETURNS]
           The function will NEVER return a NULL pointer.  If there is no
           data available, the function may return a single chunk, whose
           `num_bytes' field is set to 0.  This ensures that there is always
           a non-empty list of data chunks which can be passed to the
           `push_extra_data' function, if required.
         [ARG: suggested_data_bytes]
           Suggested number of bytes in the bodies of the messages from
           which the returned chunks are composed.  Each chunk is
           a concatenated list of messages, each containing a header, followed
           by a body, which consists of a byte range from a single data-bin
           (precinct, code-stream header or metadata-bin).  The message
           headers are not included in the limit represented by
           `suggested_data_bytes' or `max_data_bytes'.  The
           `suggested_data_bytes' limit is not interpreted strictly; instead,
           the function tries to include whole packets from as many relevant
           precinct data-bins as possible, until the suggested limit is
           exceeded.  It may thus be exceeded by a considerable margin if
           packets are very large.
         [ARG: max_data_bytes]
           This argument provides a strict limit on the number of data bytes
           which can be included in the bodies of messages in the returned
           chunk list.  The limit will not be exceeded unless it is
           impossible to avoid.  This means that partial packets may be
           included from the final precinct data-bin which contributes to
           the chunk list.  Upon return, the value of this variable is
           decremented by the actual number of message body bytes, allowing
           the caller to keep track of the number of bytes which can be
           generated in a subsequent call to the function, unless the
           limit has changed.  The only event in which the limit may be
           exceeded is if the limit is greater than 0 but not sufficiently
           large to allow the inclusion of a single message.
         [ARG: align]
           If true, each message generated by this function which crosses
           a precinct packet boundary will finish at a subsequent packet
           boundary.  The option has no effect on messages which correspond
           to data-bins other than precinct data-bins.
         [ARG: use_extended_message_headers]
           If true, each message which corresponds to a precinct data-bin
           will use the extended header format, which includes an
           indication of the number of complete quality layers represented
           by the message together with all bytes from the precinct which
           precede those in the message.
         [ARG: decouple_chunks]
           If true (this is a reasonable default), the first message written
           to each chunk will have its header coded in a manner which does
           not rely on successful delivery of messages in other chunks.
           Specifically, in this case the `id_encoder->decouple' function will
           be called prior to writing each new chunk.  For applications in
           which the messages are delivered over a reliable stream oriented
           connection, it may be sufficient to decouple the message header
           coding only at the commencement of each response stream sent
           by the server (or even once per channel).  In such cases, the
           application may choose to realize some small efficiency
           improvements by setting this argument to false.  However, the
           application is then responsible for explicitly invoking the
           `id_encoder->decouple' function at appropriate junctures.
         [ARG: id_encoder]
           Used to supply a custom function for encoding unique data-bin
           identifiers.  If this argument is NULL, the default id encoding
           implementation will be used, as described in the initial comments
           appearing with the `kds_id_encoder' class.
      */
    KDU_AUX_EXPORT void
      release_chunks(kds_chunk *chunks, bool check_abandoned=false);
      /* [SYNOPSIS]
           Recycles the resources returned by `generate_increments'.  The
           `check_abandoned' argument is provided for the benefit of
           advanced server applications which may decide to abandon some
           of the data chunks which have not yet been transmitted to a
           remote client.  Abandonment means that the application will not
           transmit these chunks or that it has transmitted them, but is
           not certain whether or not they have been or ever will be
           received correctly, and is not prepared to wait to find out.
           The decision to abandon data chunks may be made in response to
           a substantial change in the remote user's window of interest,
           but this decision is the responsibility of the server
           application, not the present object.
           [//]
           If a chunk has been abandoned (and `check_abandoned' is true),
           the function restores its cache model associated with all
           data-bins which contributed to the abandoned chunk, so as to
           reflect the state which they had prior to the point at which
           the `generate_increments' function was called (unless they have
           already been restored to an even more primitive state as a
           result of discovering other abandoned chunks).  This process
           may leave the cache model in a state where the number of cached
           bytes for some precinct does not correspond to a whole number
           of packets.  This is possible because packets which are larger
           than the maximum chunk size must be broken down into smaller
           messages, only some of which may belong to abandoned chunks.
           [//]
           In the event that a fatal error occurs, successful cleanup of all
           resources does not necessitate that outstanding buffers be released
           using this function.   The internal chunk server keeps track of
           all resources which it has allocated and destroys them automatically
           when `kdu_serve' is destroyed.  Note, however, that this means
           you should clean up any objects which may access `kds_chunk'
           objects before destroying the `kdu_serve' object.
        */
  private: // Data
    kd_serve *state;
  };

#endif // KDU_SERVE_H
