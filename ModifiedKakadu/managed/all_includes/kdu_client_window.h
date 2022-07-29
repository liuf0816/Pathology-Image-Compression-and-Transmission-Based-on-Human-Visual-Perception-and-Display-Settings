/*****************************************************************************/
// File: kdu_client_window.h [scope = APPS/CLIENT-SERVER]
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
  Describes the interface for the `kdu_window' class which is used to describe
generic windows into a JPEG2000 image (or file with multiple images), for
use with JPIP client and server components.
******************************************************************************/
#ifndef KDU_CLIENT_WINDOW_H
#define KDU_CLIENT_WINDOW_H

#include "kdu_compressed.h"

// Defined here:
struct kdu_sampled_range;
class  kdu_range_set;
struct kdu_metareq;
struct kdu_window;

// The following are used for the `kdu_sampled_range::context_type' member
#define KDU_JPIP_CONTEXT_NONE       ((int) 0)
#define KDU_JPIP_CONTEXT_JPXL       ((int) 1)
#define KDU_JPIP_CONTEXT_MJ2T       ((int) 2)
#define KDU_JPIP_CONTEXT_TRANSLATED ((int) -1)

// The following flags are used in the `kdu_metareq::qualifier' member
#define KDU_MRQ_ALL     ((int) 1)
#define KDU_MRQ_GLOBAL  ((int) 2)
#define KDU_MRQ_STREAM  ((int) 4)
#define KDU_MRQ_WINDOW  ((int) 8)
#define KDU_MRQ_DEFAULT ((int) 14) // GLOBAL | STREAM | WINDOW
#define KDU_MRQ_ANY     ((int) 15)

/*****************************************************************************/
/*                            kdu_sampled_range                              */
/*****************************************************************************/

struct kdu_sampled_range {
  /* [BIND: copy]
     [SYNOPSIS]
       This structure is used to keep track of a range of values which may
       have been sub-sampled.  In the context of the `kdu_window' object,
       it provides an efficient mechanism for expressing lists of code-stream
       indices or image component indices.
       [//]
       The range includes all values of the form `from' + k*`step' which
       are no larger than `to', where k is a non-negative integer.
       [//]
       In addition to ranges, the object may also be used to keep track of
       geometric qualifiers and expansions (in terms of member codestreams)
       for ranges of higher level objects such as JPX compositing layers.
  */
  //---------------------------------------------------------------------------
  public: // Member functions
    kdu_sampled_range()
      {
        remapping_ids[0]=remapping_ids[1]=-1; context_type=0; expansion=NULL;
        from=0; to=-1; step=1;
      }
      /* [SYNOPSIS]
           Default constructor, creates an empty range with `to' < `from'.
      */
    kdu_sampled_range(int val)
      {
        remapping_ids[0]=remapping_ids[1]=-1; context_type=0; expansion=NULL;
        from=to=val; step=1;
      }
      /* [SYNOPSIS]
           Constructs an empty range which represents a single value, `val'.
      */
    kdu_sampled_range(int from, int to)
      {
        remapping_ids[0]=remapping_ids[1]=-1; context_type=0; expansion=NULL;
        this->from=from; this->to=to; step=1; assert(to >= from);
      }
      /* [SYNOPSIS]
           Constructs a simple range (not sub-sampled) from `from' to `to',
           inclusive.  The range is empty if `to' < `from'.
      */
    kdu_sampled_range(int from, int to, int step)
      {
        remapping_ids[0]=remapping_ids[1]=-1; context_type=0; expansion=NULL;
        this->from=from; this->to=to; this->step=step;
        assert((to >= from) && (step >= 1));
      }
      /* [SYNOPSIS]
           Constructs the most general sub-sampled range.  `step' must be
           strictly greater than 0.  The range is empty if `to' < `from'.
      */
    bool is_empty() { return (to < from); }
      /* [SYNOPSIS]
           Returns true if the range is empty, meaning that its `to' member
           is smaller than its `from' member.
      */
    bool operator!() { return (to < from); }
      /* [SYNOPSIS]
           Same as `is_empty'.
      */
    int get_from() { return from; }
      /* [SYNOPSIS] Gets the value of the public `from' member.  Useful for
         non-native language bindings such as Java. */
    int get_to() { return to; }
      /* [SYNOPSIS] Gets the value of the public `to' member.  Useful for
         non-native language bindings such as Java. */
    int get_step() { return step; }
      /* [SYNOPSIS] Gets the value of the public `step' member.  Useful for
         non-native language bindings such as Java. */
    int get_remapping_id(int which)
      { return ((which>=0)&&(which<2))?(remapping_ids[which]):0; }
      /* [SYNOPSIS] Retrieves entries in the public `remapping_ids' member
         array. If `which' is anything other than 0 or 1, the function
         simply returns 0. */
    int get_context_type() { return context_type; }
      /* [SYNOPSIS] Gets the value of the public `context_type' member.
         Useful for non-native language bindings such as Java. */
    kdu_range_set *get_context_expansion() { return expansion; }
      /* [SYNOPSIS] Returns the public `expansion' member, which is NULL
         unless a `context_type' is non-zero and an expansion for the
         context has been computed. */
    void set_from(int from) { this->from=from; }
      /* [SYNOPSIS] Sets the value of the public `from' member.  Useful for
         non-native language bindings such as Java. */
    void set_to(int to) { this->to=to; }
      /* [SYNOPSIS] Sets the value of the public `to' member.  Useful for
         non-native language bindings such as Java. */
    void set_step(int step) { this->step=step; }
      /* [SYNOPSIS] Sets the value of the public `step' member.  Useful for
         non-native language bindings such as Java. */
    void set_remapping_id(int which, int id_val)
      { if ((which >= 0) && (which < 2)) remapping_ids[which]=id_val; }
      /* [SYNOPSIS] Sets entries in the public `remapping_ids' member
         array. If `which' is anything other than 0 or 1, the function
         does nothing. */
    void set_context_type(int ctp) { this->context_type=ctp; }
      /* [SYNOPSIS] Sets the value of the public `context_type' member.
         Useful for non-native language bindings such as Java. */
  public: // Data
    int from;
      /* [SYNOPSIS] Inclusive, non-negative lower bound of the range. */
    int to;
      /* [SYNOPSIS] Inclusive, non-negative upper bound of the range. */
    int step;
      /* [SYNOPSIS] Strictly positive step value. */
    int remapping_ids[2];
      /* [SYNOPSIS]
           Two parameters which may be used to signal the mapping of
           the view-window resolution and region onto the coordinates of
           particular codestreams.  The interpretation of these parameters
           depends on the `context_type' member.  They shall be ignored
           if `context_type' is 0.
      */
    int context_type;
      /* [SYNOPSIS]
           This member is 0 (`KDU_JPIP_CONTEXT_NONE') unless the present
           object describes a range of image entities (e.g. JPX compositing
           layers or MJ2 video tracks) within a codestream context, or a
           range of codestream indices which were obtained by translating
           such a context.  Currently, the only legal non-zero values are
           those defined by the following macros:
           [>>] `KDU_JPIP_CONTEXT_JPXL' --
                In this case, the indices in the sampled range correspond
                to JPX compositing layer indices, starting from 0 for the
                first compositing layer in the data source.  Ranges with this
                context type may be used by the `kdu_window::contexts' range
                set.  The `remapping_ids' correspond (in order) to the
                index (from 0) of an instruction set box, and the index
                (from 0) of an instruction within that instruction set
                box, which together define the mapping of view-window
                coordinates from a complete composited image to the
                individual codestream reference grids.  If
                `remapping_ids'[0] is negative, the view-window
                coordinates are mapped from the compositing layer registration
                grid (rather than a complete composited image) to the
                individual codestream reference grids.  For a full set of
                equations describing this remapping process, see the JPIP
                standard (FCD-2).
           [>>] `KDU_JPIP_CONTEXT_MJ2T' -- In this case, the indices in the
                sampled range correspond to MJ2 video tracks, starting from
                1 for the first track (a quirk of MJ2 is that tracks are
                numbered from 1 rather than 0).  Ranges with this context type
                may be used by the `kdu_window::contexts' range set.  In this
                case, `remapping_ids'[0] is used to control view window
                transformations, while `remapping_ids'[1] is used to control
                temporal transformations.  A negative value for
                `remapping_ids'[0] means that there shall be no view window
                remapping; a value of 0 means that remapping is
                to be performed at the track level; and a value of 1 means
                that remapping is to be performed at the movie level.  A
                negative value for `remapping_ids'[1] means that the first
                codestream in the track is to be taken as the start of the
                range of codestreams which belong to the context.  A value
                of 0 means that the context starts with the first
                codestream in the track which was captured at the time
                when the request is processed (the JPIP "now" time).  The
                value of `remapping_ids'[1] may be ignored if video is
                not being captured live.
           [>>] `KDU_JPIP_CONTEXT_TRANSLATED' -- In this case, the indices in
                the sampled range correspond to codestreams which have been
                implicitly included within the `kdu_window::codestreams'
                range set as a result of translating a codestream context
                found in the `kdu_window::contexts' range set.  Such entries
                may be generated by a JPIP server.  The `remapping_ids'[0]
                value, in this case, holds the index (starting from 0) of
                the context range, found within `kdu_window::contexts' whose
                translation yielded the codestreams identified by the
                present range.  `remapping_ids'[1] identifies which element
                of the referenced context range was translated.  A value
                of 0 means that the first element in the referenced context
                range was translated to yield the range of codestreams
                identified here.  A value of 1 means that the second element
                in the context range was translated, and so forth.  As an
                example, suppose the second sampled range in
                `kdu_window::contexts' holds the range "5-8:2" with
                context-type `KDU_JPIP_CONTEXT_JPXL', then if
                the present range has `remapping_ids'[0]=1 and
                `remapping_ids'[1]=1, the range represents codestreams
                (and their associated view-windows) which were translated
                from the JPX compositing layer with index 7.
      */
    kdu_range_set *expansion;
      /* [SYNOPSIS]
           If non-NULL, this member points to a `kdu_range_set' which
           provides an expansion of the codestream identifiers associated
           with a context.  The field may be non-NULL only if `context_type'
           is non-zero and refers to a codestream context (e.g., JPX
           compositing layers or MJ2 video tracks).  Moveover, the individual
           `kdu_sampled_range' objects within the referenced
           `expansion' must have zero-valued `context_type' members.
           [//]
           The object referenced by `expansion' is not considered to be
           owned by the present object, and it will not be destroyed when
           the present object is destroyed.  It must be managed separately.
           In particular, `kdu_window' provides for the management of
           codestream context expansions.
           [//]
           In JPIP client-server communication, expansions play a slightly
           different role to codestream ranges with the
           `KDU_JPIP_CONTEXT_TRANSLATED' context type.  Specifically, when
           a client request includes one or more codestream contexts,
           managed by `kdu_window::contexts', the server responds by
           sending back the requested codestream contexts (or as many as
           it could translate), expanding each into a set of codestream
           ranges.  The server also sends back information about the actual
           codestreams for which it will serve data, which are generally
           a subset of those requested, directly or indirectly via
           codestream contexts.  This information is managed by the
           `kdu_window::codestreams' range set.  Ranges in that set have
           the special `KDU_JPIP_CONTEXT_TRANSLATED' context type if and
           only if they represent data which is being served as a result
           of translating a codestream context.  Otherwise they have a
           context type of 0.  The distinction between codestreams which
           are being served as a result of context translation and
           codestreams which are being served as a result of a direct
           codestream request can be very important.  It allows the
           client and server to figure out whether or not a new request
           is the same, a superset of, or a subset of a previous request,
           possibly modified by the server, for which a response is in
           progress.
      */
  };

/*****************************************************************************/
/*                              kdu_range_set                                */
/*****************************************************************************/

class kdu_range_set {
  /* [BIND: reference]
     [SYNOPSIS]
       This structure is used to keep track of a set of indices.  The set is
       represented as a union of (potentially sub-sampled) ranges, i.e. as
       a union of `kdu_sampled_range's.  The lower bounds of the ranges
       are arranged in order, from smallest to largest, but the ranges
       might still overlap.  Each sampled range may have an additional
       geometric mapping qualifier.  Ranges with different geometric mapping
       qualifiers will not be merged together into a single range, even
       if they do overlap.
  */
  //---------------------------------------------------------------------------
  public: // Member functions
    kdu_range_set() { ranges=NULL; num_ranges=max_ranges=0; next=NULL; }
    KDU_AUX_EXPORT ~kdu_range_set();
    kdu_range_set operator=(const kdu_range_set &src)
      {
        assert(0);
        kdu_range_set empty_result;
        return empty_result;
      }
      /* [SYNOPSIS]
           Direct assignment of `kdu_range_set' objects is illegal.
           This function exists to prevent you from making such assignments
           by mistake.  In debug mode, an assertion will be raised if you
           have accidentally done this.
      */
    KDU_AUX_EXPORT void copy_from(kdu_range_set &src);
      /* [SYNOPSIS]
           Effectively initializes the current object and then adds each of
           the ranges in the `src' object.  Does not copy expansions -- i.e.,
           leaves the `kdu_sampled_range::expansions' member to NULL in
           each copied range.
      */
    bool is_empty() { return (num_ranges==0); }
      /* [SYNOPSIS]
           Returns true if the set is empty -- no indices are present.
      */
    bool operator!() { return (num_ranges==0); }
      /* [SYNOPSIS]
           Same as `is_empty'.
      */
    KDU_AUX_EXPORT bool contains(kdu_range_set &rhs);
      /* [SYNOPSIS]
           Returns true if this set contains all the indices which belong to
           the `rhs' object.  Expansions (see `kdu_sampled_range::expansion')
           are not checked to determine containment, since it is assumed that
           any expansion of an image entity into its codestreams will always
           be the same.  Likewise, ranges with a
           `kdu_sampled_range::context_type' value of
           `KDU_JPIP_CONTEXT_TYPE_TRANSLATED' are also deliberately ignored,
           since they represent server translations of information which is
           otherwise being signalled.
      */
    bool equals(kdu_range_set &rhs)
      { return (this->contains(rhs) && rhs.contains(*this)); }
      /* [SYNOPSIS]
           Returns true if this object holds exactly the same set of
           indices as the `rhs' object.  As for the `contains' function,
           expansions are not checked, and ranges with a
           `kdu_sampled_range::context_type' value of
           `KDU_JPIP_CONTEXT_TYPE_TRANSLATED' are also deliberately ignored,
           since they represent server translations of information which is
           otherwise being signalled.
      */
    bool operator==(kdu_range_set &rhs) { return this->equals(rhs); }
      /* [SYNOPSIS] Same as `equals'. */
    bool operator!=(kdu_range_set &rhs) { return !this->equals(rhs); }
      /* [SYNOPSIS] Opposite of `equals'. */
    void init() { num_ranges = 0; }
      /* [SYNOPSIS]
           Initializes the object to the empty set, but does not destroy
           previously allocated internal resources.
      */
    KDU_AUX_EXPORT void
      add(kdu_sampled_range range, bool allow_merging=true);
      /* [SYNOPSIS]
           Add a new (possibly sub-sampled) range of indices to those already
           in the set.  The function may simply add this range onto the end
           of the list of ranges, but it might also find a more efficient
           way to represent the indices with the same, or a smaller number of
           ranges.  If the new range fills in a hole, for example, the
           number of distinct ranges in the list might be reduced.  The
           function is not required to find the most efficient representation
           for the set of indices represented by all calls to this
           function since the last call to `init', since finding the
           most efficient representation is a non-trivial problem,
           and probably not worth the effort.
           [//]
           The function adjusts the `to' members of the various ranges
           to ensure that they take the smallest possible values which
           are consistent with the set of indices represented by the
           range.  This means that the `to' member will actually be equal
           to the last index in the range.
           [//]
           Where the `range.context_type' member is non-zero, the range will
           be merged only with other sampled ranges having exactly the same
           context type and exactly the same remapping identifiers
           (see `kdu_sampled_range::context_type' and
           `kdu_sampled_range::remapping_ids').
         [ARG: allow_merging]
           If false, the range will not be merged with any others; it will
           simply be appended to the end of the existing set of ranges.  This
           is particularly useful when constructing expansions of requested
           codestream contexts, since it is most useful for the client to
           receive separate expansions for each individually requested
           codestream context range, without having them merged.
      */
    void add(int val) { add(kdu_sampled_range(val)); }
      /* [SYNOPSIS]
           Adds a single index to the set.  Equivalent to calling the first
           form of the `add' function with a range which contains just the
           one element, `val', with a `context_type' value of 0.
      */
    void add(int from, int to) { add(kdu_sampled_range(from,to)); }
      /* [SYNOPSIS]
           Adds a contiguous range of indices to the set.  Equivalent to
           calling the first form of the `add' function with a
           `kdu_sampled_range' object whose `kdu_sampled_range::from' and
           `kdu_sampled_range::to' members are identical to the `from' and
           `to' arguments, with `kdu_sampled_range::step' set to 1 and
           `kdu_sampled_range::context_type' set to 0.
      */
    int get_num_ranges() { return num_ranges; }
      /* [SYNOPSIS]
           Returns the number of distinct ranges in the set.
      */
    kdu_sampled_range get_range(int n)
      {
        if ((n >= 0) && (n < num_ranges))
          return ranges[n];
        return kdu_sampled_range(); // Empty range
      }
      /* [SYNOPSIS]
           Returns the `n'th element from the internal list of
           (potentially sub-sampled) index ranges whose union represents
           the set of all indices in the set.  `n' must be
           non-negative.  If it is equal to or larger than the number of
           ranges in the list, the function returns an empty range, having
           its `to' member smaller than its `from' member.
      */
    kdu_sampled_range *access_range(int n)
      {
        if ((n >= 0) && (n < num_ranges))
          return ranges+n;
        else
          return NULL;
      }
      /* [SYNOPSIS]
           Same as `get_range' except that it returns a pointer to the
           internal `kdu_sampled_range' object, rather than a copy of its
           contents.  Returns NULL if `n' is not a valid range index.
      */
    bool test(int index)
      {
        for (int n=0; n < num_ranges; n++)
          if ((ranges[n].from >= 0) && (ranges[n].from <= index) &&
              (ranges[n].to >= index) &&
              ((ranges[n].step == 1) ||
               (((index-ranges[n].from) % ranges[n].step) == 0)))
            return true;
        return false;
      }
      /* [SYNOPSIS]
           Performs membership testing.  Returns true if `index' belongs
           to the set, meaning that it can be found within one of the
           ranges whose union represents the set.
      */
    KDU_AUX_EXPORT int expand(int *buf, int accept_min, int accept_max);
      /* [SYNOPSIS]
           Writes all unique indices which fall within the ranges defined in this
           set, while simultaneously lying within the interval
           [`accept_min',`accept_max'] into the supplied buffer, returning the
           number of written indices.  It is your responsibility to ensure that
           `buf' is large enough to accept all required indices, which can easily
           be done by ensuring at least that `buf' can hold
           `accept_max'+1-`accept_min' entries.
      */
  private: // Data
    int max_ranges; // Size of the `ranges' array
    int num_ranges; // Number of valid elements in the `ranges' array
    kdu_sampled_range *ranges;
  public: // Data
    kdu_range_set *next; // May be used to build linked lists of range sets
  };

/*****************************************************************************/
/*                               kdu_metareq                                 */
/*****************************************************************************/

struct kdu_metareq {
  /* [BIND: reference]
     [SYNOPSIS]
       This structure is used to describe metadata in which the client is
       interested.  It is used to build a linked list of descriptors which
       provide a complete description of the client's metadata interests,
       apart from any metadata which the server should deem to be mandatory
       for the client to correctly interpret requested imagery.  The order
       of elements in the list is immaterial to the list's interpretation.
  */
  //---------------------------------------------------------------------------
  public: // Data
    kdu_uint32 box_type;
      /* [SYNOPSIS] 
           JP2-family box 4 character code, or else 0 (wildcard).
      */
    int qualifier;
      /* [SYNOPSIS]
           Provides qualification of the conditions under which a box
           matching the `box_type' value should be considered to be of
           interest to the client.  This member takes the logical OR of
           one or more of the following flags: `KDU_MRQ_ALL', `KDU_MRQ_GLOBAL',
           `KDU_MRQ_STREAM' or `KDU_MRQ_WINDOW', corresponding to JPIP
           "metareq" request field qualifiers of "/a", "/g", "/s" and "/w",
           respectively.  The word should not be equal to 0.  Each of the flags
           represents a context, consisting of a collection of boxes which
           might be of interest.  The boxes which are of interest to the client
           are restricted to those boxes which belong to one of the contexts
           for which a flag is present.
           [>>] `KDU_MRQ_WINDOW' identifies a context which consists of all
                boxes which are known to be directly associated with a
                specific (non-global) spatial region of one or more
                codestreams, so long as the spatial region and other aspects
                of the metadata intersect with the requested view window.
           [>>] `KDU_MRQ_STREAM' identifies a context which consists of all
                boxes which are known to be associated with one or more
                codestreams or codestream contexts associated with the
                view window, but not with any specific spatial region of
                any codestream.
           [>>] `KDU_MRQ_GLOBAL' identifies a context which consists of all
                boxes which are relevant to the requested view window, but
                do not fall within either of the above two categories.
           [>>] `KDU_MRQ_ALL' identifies a context consisting of all JP2
                boxes in the entire target.
           [//]
           Note that the first three contexts mentioned above are mutually
           exclusive, yet their union is generally only a subset of the
           `KDU_MRQ_ALL' context.  The union of the first 3 flags is
           conveniently given by the macro, `KDU_MRQ_DEFAULT', since it is
           the default qualifier for JPIP `metareq' requests which do not
           explicitly identify a box context.
      */
    bool priority;
      /* [SYNOPSIS]
           True if the boxes described by this record are to have higher
           priority than image data when delivered by the server.  Otherwise,
           they have lower priority than the image data described by the
           request window.
      */
    int byte_limit;
      /* [SYNOPSIS]
           Indicates the maximum number of bytes from the contents of any
           JP2 box conforming to the new `kdu_metareq' record, which are
           of interest to the client.  May not be negative.  Guaranteed to
           be 0 if `recurse' is true.
      */
    bool recurse;
      /* [SYNOPSIS]
           Indicates that the client is also interested in all descendants
           (via sub-boxes) of any box which matches the `box_type' and
           other attributes specified by the various members of this structure.
           If true, the `byte_limit' member will be 0, meaning that the
           client is assumed to be interested only in the header of boxes
           which match the `box_type' and the descendants of such boxes.
      */
    kdu_long root_bin_id;
      /* [SYNOPSIS]           
           Indicates the metadata-bin identifier of the data-bin which is
           to be used as the root when processing this record.  If non-zero,
           the scope of the record is reduced to the set of JP2 boxes which
           are found in the indicated data-bin or are linked from it via
           placeholders.
      */
    int max_depth;
      /* [SYNOPSIS]
           Provides a limit on the maximum depth to which the server is
           expected to descend within the metadata tree to find JP2 boxes
           conforming to the other attributes of this record.  A value of 0
           means that the client is interested only in boxes which are found
           at the top level of the data-bin identified by `root_bin_id'.
           A value of 1 means that the client is additionally interested
           in sub-boxes of boxes which are found at the top level of the
           data-bin identified by `root_bin_id'; and so forth.
      */
    kdu_metareq *next;
      /* [SYNOPSIS]
           Link to the next element of the list, or else NULL.
      */
    int dynamic_depth;
      /* [SYNOPSIS]
           The interpretation of this member is not formally defined here.  It
           is provided to allow metadata analyzers to save temporary state
           information.  In particular, it is used by `kdu_serve' to save
           the maximum absolute depth of data-bins which are covered by the
           request, or else -1 if we are currently analyzing a point in the
           metadata tree which is not descended from a metadata-bin with
           the `root_bin_id' identifier.
      */
  //---------------------------------------------------------------------------
  public: // Member functions
    bool operator==(kdu_metareq &rhs)
      { /* [SYNOPSIS] Returns true only if all members are identical to those
           of `rhs'. */
        return (box_type == rhs.box_type) && (priority == rhs.priority) &&
               (qualifier == rhs.qualifier) &&
               (byte_limit == rhs.byte_limit) && (recurse == rhs.recurse) &&
               (root_bin_id == rhs.root_bin_id) &&
               (max_depth == rhs.max_depth);
      }
    bool equals(kdu_metareq &rhs) { return (*this == rhs); }
      /* [SYNOPSIS] Same as `operator==', returning true only if all members
         of this structure are identical to those of `rhs'. */
    kdu_uint32 get_box_type() { return box_type; }
      /* [SYNOPSIS] Returns the value of the public `box_type' member.
         Useful for non-native language bindings such as Java. */
    int get_qualifier() { return qualifier; }
      /* [SYNOPSIS] Returns the value of the public `qualifier' member.
         Useful for non-native language bindings such as Java.  Note that
         the return value is a union of one or more of the flags:
         `KDU_MRQ_ALL', `KDU_MRQ_GLOBAL', `KDU_MRQ_STREAM' and
         `KDU_MRQ_WINDOW'. */
    bool get_priority() { return priority; }
      /* [SYNOPSIS] Returns the value of the public `priority' member.
         Useful for non-native language bindings such as Java. */
    int get_byte_limit() { return byte_limit; }
      /* [SYNOPSIS] Returns the value of the public `byte_limit' member.
         Useful for non-native language bindings such as Java. */
    bool get_recurse() { return recurse; }
      /* [SYNOPSIS] Returns the value of the public `recurse' member.
         Useful for non-native language bindings such as Java. */
    kdu_long get_root_bin_id() { return root_bin_id; }
      /* [SYNOPSIS] Returns the value of the public `root_bin_id' member.
         Useful for non-native language bindings such as Java. */
    int get_max_depth() { return max_depth; }
      /* [SYNOPSIS] Returns the value of the public `max_depth' member.
         Useful for non-native language bindings such as Java. */
    kdu_metareq *get_next() { return next; }
      /* [SYNOPSIS] Returns the value of the public `next' member.
         Useful for non-native language bindings such as Java. */
  };

/*****************************************************************************/
/*                                kdu_window                                 */
/*****************************************************************************/

struct kdu_window {
  /* [BIND: reference]
     [SYNOPSIS]
       This structure defines the elements which can be used to identify
       a spatial region of interest, an image resolution of interest and
       image components of interest, in a manner which does not depend
       upon the coordinate system used by any particular JPEG2000
       code-stream.  Together, we refer to these parameters as identifying
       a "window" into the full image.
  */
  //---------------------------------------------------------------------------
  public: // Member functions
    KDU_AUX_EXPORT kdu_window();
    KDU_AUX_EXPORT ~kdu_window();
    KDU_AUX_EXPORT void init();
      /* [SYNOPSIS]
           Sets all dimensions to zero.  Sets the `max_layers' value to 0
           (default value, meaning no restriction).  Sets the collections
           of components and codestreams to empty.  Sets `round_direction'
           to -1 (round-down is the default).  Sets the `metareq' list to
           empty (NULL).
      */
    KDU_AUX_EXPORT void copy_from(kdu_window &src, bool copy_expansions=false);
      /* [SYNOPSIS]
           Copies the window represented by `src'.  If `copy_expansions' is
           true, the context expansions (see `kdu_sampled_range::expansion')
           within the `contexts' range set are also copied.  Otherwise,
           expansions are not copied, and `kdu_sampled_range::expansion' is
           set to NULL for each copied element of the `contexts' object.
      */
    KDU_AUX_EXPORT bool equals(kdu_window &rhs);
      /* [SYNOPSIS]
           Returns true if `rhs' represents exactly the same window
           as the current object.  Does not check expansions (see
           `kdu_sampled_range::expansion'), since the expansion for each
           range in the `contexts' set should be unique.
      */
    KDU_AUX_EXPORT bool contains(kdu_window &rhs);
      /* [SYNOPSIS]
           Returns true if `rhs' describes a window which is fully contained
           within the window described by the current object.  Containment
           refers not only to the image window geometry, image components,
           image resolution, image region and code-streams, but also to the
           number of quality layers and the collection of metadata elements
           which are of interest, as expressed by the various members of the
           present structure.  Does not check expansions (see
           `kdu_sampled_range::expansion'), since the expansion for each
           range in the `contexts' set should be unique.
      */
  //---------------------------------------------------------------------------
  public: // Data and interfaces for function-only bindings
    kdu_coords resolution;
      /* [SYNOPSIS]
           Describes the preferred image resolution in terms of the width and
           the height of the full image.  In the simplest case where no
           geometric mapping is required, the request is serviced by
           discarding r highest resolution levels, where r is chosen so that
           `resolution.x' and `resolution.y' are as close as possible to
           ceil(Xsiz/2^r)-ceil(Xoff/2^r) and ceil(Ysiz/2^r)-ceil(Yoff/2^r),
           where the sense of "close" is determined by the `round_direction'
           member.  Xsiz and Ysiz, Xoff and Yoff are the dimensions recorded
           in the codestream's SIZ marker segment.
           [//]
           Where geometric mapping information is provided via codestream
           `contexts', additional transformations are applied to match the
           resolution of each codestream to the resolution of the
           composited result which is identified by the present member.
           [//]
           If both `resolution.x' and `resolution.y' are 0, no compressed
           image data is being requested.  Such a request requires the server
           to send only the essential marker segments from main
           codestream headers, plus any metadata which is required for correct
           rendering.  Such a request might be sent by a client when it first
           connects to the server, but would not normally be issued
           explicitly at the application levels.
      */
    kdu_coords get_resolution()
      { return resolution; }
      /* [SYNOPSIS] Returns a copy of the public `resolution' member.
         Useful for non-native language bindings such as Java. */
    void set_resolution(kdu_coords resolution)
      { this->resolution=resolution; }
      /* [SYNOPSIS] Sets the public `resolution' member.
         Useful for non-native language bindings such as Java. */
  //---------------------------------------------------------------------------
    int round_direction;
      /* [SYNOPSIS]
           Indicates the preferred direction for converting the `resolution'
           size to the size of an actual available image resolution in
           each codestream.  The available resolutions for each codestream
           are found by starting with the image size on the codestream's
           high resolution canvas, and dividing down by multiples of 2,
           following the usual rounding conventions of the JPEG2000
           canvas coordinate system.  Thus, the available image dimensions
           correspond essentially to the codestream image dimensions obtained
           by discarding a whole number of highest resolution levels,
           except that the number of discarded DWT levels is not necessarily
           limited by the number of DWT levels used in each relevant
           tile component.
           [>>] A value of 0 means that rounding is to the nearest available
                codestream image resolution, in the sense of area.
                Specifically, the actual codestream image resolution's
                dimensions, X and Y, should minimize |XY - xy| where x and y
                are the values of `resolution.x' and `resolution.y'.
           [>>] A positive value means that the requested resolution should
                be rounded up to the nearest actual available codestream
                resolution.  That is, the actual selected dimensions, X and Y,
                must satisfy X >= `resolution.x' and Y >= `resolution.y',
                if possible.
           [>>] A negative value means that the requested resolution should
                be rounded down to the nearest actual available codestream
                resolution.  That is, the actual selected dimensions, X and Y,
                must satisfy X <= `resolution.x' and Y <= `resolution.y', if
                possible.  Note: this is the default value set by `init'.
      */
    int get_round_direction()
      { return round_direction; }
      /* [SYNOPSIS] Returns the value of the public `round_direction' member.
         Useful for non-native language bindings such as Java. */
    void set_round_direction(int direction)
      { round_direction = direction; }
      /* [SYNOPSIS] Sets the value of the public `round_direction' member.
         Useful for non-native language bindings such as Java. */
  //---------------------------------------------------------------------------
    kdu_dims region;
      /* [SYNOPSIS]
           Describes a spatial region, expressed relative to the image
           resolution defined by the `resolution' member.
      */
    kdu_dims get_region() { return region; }
      /* [SYNOPSIS] Returns a copy of the public `region' member.
         Useful for non-native language bindings such as Java. */
    void set_region(kdu_dims region) { this->region=region; }
      /* [SYNOPSIS] Sets the public `region' member.
         Useful for non-native language bindings such as Java. */
  //---------------------------------------------------------------------------
    kdu_range_set components;
      /* [SYNOPSIS]
           Set of image components which belong to the window.  If this member
           is empty (`components.is_empty' returns true), the window includes
           all possible image components.  You should avoid adding
           ranges to a `components' member whose `kdu_sampled_range::step'
           member is anything other than 1.
      */
    kdu_range_set *access_components() { return &components; }
      /* [SYNOPSIS]
           Returns a pointer (reference) to the public `components' member.
           This is useful for non-native language bindings such as Java.
      */
  //---------------------------------------------------------------------------
    kdu_range_set codestreams;
      /* [SYNOPSIS]
           Set of codestreams which belong to the window.  If this member
           is empty, no particular restriction is imposed on the components
           being accessed.  If no information is provided by any other
           members (e.g., `compositing_layers') from which codestreams
           of interest may be deduced, the default behaviour depends on the
           type of image being served.
      */
    kdu_range_set *access_codestreams() { return &codestreams; }
      /* [SYNOPSIS]
           Returns a pointer (reference) to the public `codestreams' member.
           This is useful for non-native language bindings such as Java.
      */
  //---------------------------------------------------------------------------
    kdu_range_set contexts;
      /* [SYNOPSIS]
           This member is used to identify a collection of codestream
           contexts which a server is expected to expand the context into
           a collection of codestreams.  Currently, there are only two
           defined context types: JPX compositing layers; and MJ2
           tracks.  Accordingly, each `kdu_sampled_range' element in the
           `contexts' object must have a non-zero
           `kdu_sampled_range::context_type' member, which must match one of
           the macros `KDU_JPIP_CONTEXT_JPXL' or `KDU_JPIP_CONTEXT_MJ2T'.
      */
    kdu_range_set *access_contexts() { return &contexts; }
      /* [SYNOPSIS]
           Returns a pointer (reference) to the public `contexts'
           member.  This is useful for non-native language bindings such
           as Java.
      */
    KDU_AUX_EXPORT kdu_range_set *create_context_expansion(int which);
      /* [SYNOPSIS]
           This function creates a new internally managed `kdu_range_set'
           object and associates it with a particular `kdu_sampled_range'
           element within the `contexts' object.  That element is the one
           with index `which'.  It may be recovered by invoking
           `contexts.get_range' with `which' as the argument.  Once the
           function returns, the caller may add the indices of the codestreams
           associated with the relevant contexts using `kdu_range_set::add'.
      */
    KDU_AUX_EXPORT const char *parse_context(const char *string);
      /* [SYNOPSIS]
           Parses a single JPIP context-range specifier from the supplied
           string, returning a pointer to any unparsed segment of the
           string.  Since JPIP "context" request fields and response headers
           may contain multiple context-range expressions, separated by
           commas (for requests) or semi-colons (for response headers), the
           function will often return a pointer to one of these separators.
           The caller decides how to invoke this function again where a
           list of context-range expressions is available.  If an error
           occurs, the function will return with a pointer to the character
           at which the error was detected, adding nothing to the internal
           `contexts' range set.  If an unrecognized context string is
           encountered, the function skips over it, returning a pointer to
           the character immediately following the unrecognized context-range
           expression.  The rules for JPIP context-range expressions, allows
           the function to skip unrecognized expressions effectively.
           [//]
           The context-range expression which is parsed, may contain a
           codestream expansion (separated by "=" in the string).  If so,
           the expansion is processed and recorded via the
           `create_context_expansion' function.
           [//]
           Upon successful completion, a recognized context-range expression
           will result in exactly one range being appended to the end of the
           `contexts' member (without merging or reordering), and possibly
           an associated expansion being generated.
      */
  //---------------------------------------------------------------------------
    int max_layers;
      /* [SYNOPSIS]
           Identifies the maximum number of code-stream quality layers
           which are to be considered as belonging to the window.  If this is
           zero, or any value larger than the actual number of layers in
           the code-stream, the window is considered to embody all available
           quality layers.
      */
    int get_max_layers() { return max_layers; }
      /* [SYNOPSIS] Returns the value of the public `max_layers' member.
         Useful for non-native language bindings such as Java. */
    void set_max_layers(int val) { max_layers = val; }
      /* [SYNOPSIS] Sets the value of the public `max_layers' member.
         Useful for non-native language bindings such as Java. */
  //---------------------------------------------------------------------------
    bool metadata_only;
      /* [SYNOPSIS]
           If this flag is true, the client is not interested in receiving
           anything other than the metadata which would be returned in
           response to the requested window.  No compressed imagery, or
           even image headers should be sent.  Note that setting this member
           to true may have no effect unless a non-empty `metareq' list is
           also provided (e.g. by calls to `add_metareq').
      */
    bool get_metadata_only() { return metadata_only; }
      /* [SYNOPSIS] Returns the value of the public `metadata_only' member.
         Useful for non-native language bindings such as Java. */
    void set_metadata_only(bool val) { metadata_only = val; }
      /* [SYNOPSIS] Sets the value of the public `metadata_only' member.
         Useful for non-native language bindings such as Java.  Note that
         the `init_metareq' and `parse_metareq' function also set the
         value of the `metadata_only' member.  In particular, `init_metareq'
         sets it to false, while `parse_metareq' sets it to true if a
         "!!" token is encountered in the parsed text. */
    kdu_metareq *metareq;
      /* [SYNOPSIS]
           Points to a linked list of `kdu_metareq' objects.  If NULL, the
           client is interested in all metadata the server deems to be
           important, delivered in the order the server chooses.  If non-NULL
           only those items identified by this list are of interest to the
           client, unless items are omitted which the server deems to be
           essential for the client to correctly interpret the requested
           imagery.
           [//]
           Do not add to this list manually.  Instead, use the `add_metareq'
           function to build a list of `kdu_metareq' objects.  Note that
           the `init' function resets the `metareq' list to empty (NULL).
      */
    kdu_metareq *get_metareq(int index)
      { kdu_metareq *req=metareq;
        for (; (req != NULL) && (index > 0); req=req->next, index--);
        return req;
      }
      /* [SYNOPSIS]
           Returns NULL if the number of entries on the `metareq' list is
           equal to or less than `index'.  Otherwise, the function returns
           the particular entry on the `metareq' list which is indexed by
           the `index' value.  Useful for non-native language bindings such
           as Java.
      */
    void init_metareq()
      { kdu_metareq *req;
        while ((req=metareq) != NULL)
          { metareq=req->next; req->next=free_metareqs; free_metareqs=req; }
        metadata_only = false;
        have_metareq_all = have_metareq_global =
          have_metareq_stream = have_metareq_window = false;
      }
      /* [SYNOPSIS]
           Initializes the `metareq' list to the empty state and sets
           the `metadata_only' member to false.  This function
           is automatically called by `init', but it can be useful to
           initialize only the `metareq' list and nothing else.
      */
    KDU_AUX_EXPORT void
      add_metareq(kdu_uint32 box_type, int qualifier=KDU_MRQ_DEFAULT,
                  bool priority=false, int byte_limit=INT_MAX,
                  bool recurse=false, kdu_long root_bin_id=0,
                  int max_depth=INT_MAX);
      /* [SYNOPSIS]
           Use this function to add a new entry to the `metareq' list.
           The order of elements in the list is not important.  Each element
           in the list describes a particular `box_type' of interest.  This
           should either be a valid JP2-family box signature (4 character
           code) or 0; in the latter case, all box types are implied.  The
           interpretation of the other arguments are described below.
         [ARG: box_type]
           JP2-family box 4 character code, or else 0 (wildcard).
         [ARG: qualifier]
           Must be a union of one or more of the flags, `KDU_MRQ_ALL',
           `KDU_MRQ_GLOBAL', `KDU_MRQ_STREAM' and `KDU_MRQ_WINDOW'.  At least
           one of these flags must be present.  The default value,
           `KDU_MRQ_DEFAULT' is equivalent to the union of `KDU_MRQ_GLOBAL',
           `KDU_MRQ_STREAM' and `KDU_MRQ_WINDOW'.  For a detailed explanation
           of the role played by these qualifiers, see the comments appearing
           with the `kdu_metareq::qualifier' member.
         [ARG: byte_limit]
           Indicates the maximum number of bytes from the contents of any
           JP2 box conforming to the new `kdu_metareq' record, which are
           of interest to the client.  May not be negative.  Note that
           this argument is ignored if `recurse' is true.
         [ARG: priority]
           True if the boxes described by the new `kdu_metareq' structure
           are to be assigned higher priority than image data when the
           server determines the order in which to transmit information which
           is relevant to the window request.  If false, these boxes will
           have lower priority than image data (unless the priority is
           boosted by another record in the `metareq' list).  If no
           image data is to be returned (see `metadata_only'), the `priority'
           value may still have the effect of causing some high priority
           metadata to be sent before low priority metadata.
         [ARG: recurse]
           Indicates that the client is also interested in all descendants
           (via sub-boxes) of any box which matches the `box_type' and
           other attributes specified by the various arguments.  If true,
           the `byte_limit' argument will be ignored, but treated as 0,
           meaning that the client is assumed to be interested only in the
           header of boxes which match the `box_type' and the descendants
           of such boxes.
         [ARG: root_bin_id]
           Indicates the metadata-bin identifier of the data-bin which is
           to be used as the root when processing the new `kdu_metareq'
           record.  If non-zero, the scope of the record is reduced to
           the set of JP2 boxes which are found in the indicated data-bin
           or are linked from it via placeholders.
         [ARG: max_depth]
           Provides a limit on the maximum depth to which the server is
           expected to descend within the metadata tree to find JP2 boxes
           conforming to the other attributes of this record.  A value of 0
           means that the client is interested only in boxes which are found
           at the top level of the data-bin identified by `root_bin_id'.
           A value of 1 means that the client is additionally interested
           in sub-boxes of boxes which are found at the top level of the
           data-bin identified by `root_bin_id'; and so forth.
      */
    KDU_AUX_EXPORT const char *parse_metareq(const char *string);
      /* [SYNOPSIS]
           Parse the contents of a JPIP "metareq=" request field, adding
           elements to the `metareq' list as we go.  This function does not
           reset the `metareq' list before it starts; instead, it simply
           appends the results obtained by parsing the supplied `string' to
           any existing list.  Call `init_metareq' first if you wish to
           start with an empty list.
           [//]
           Note that this function will set `metadata_only' to true if it
           encounters the "!!" token.  If this token is encountered, it
           must appear at the end of the string.
         [RETURNS]
           NULL if the `string' is successfully parsed.  Otherwise, returns
           a pointer to the first point in the supplied `string' at which
           a parsing error occurred.
      */
    bool have_metareq_all;
      /* [SYNOPSIS]
           Provided to facilitate efficient searching, this member is true
           if any only if the `metareq' list contains at least one entry
           whose `kdu_metareq::qualifier' member contains the `KDU_MRQ_ALL'
           flag.
      */
    bool have_metareq_global;
      /* [SYNOPSIS]
           Provided to facilitate efficient searching, this member is true
           if any only if the `metareq' list contains at least one entry
           whose `kdu_metareq::qualifier' member contains the `KDU_MRQ_GLOBAL'
           flag.
      */
    bool have_metareq_stream;
      /* [SYNOPSIS]
           Provided to facilitate efficient searching, this member is true
           if any only if the `metareq' list contains at least one entry
           whose `kdu_metareq::qualifier' member contains the `KDU_MRQ_STREAM'
           flag.
      */
    bool have_metareq_window;
      /* [SYNOPSIS]
           Provided to facilitate efficient searching, this member is true
           if any only if the `metareq' list contains at least one entry
           whose `kdu_metareq::qualifier' member contains the `KDU_MRQ_WINDOW'
           flag.
      */
  private:
    kdu_metareq *free_metareqs;
    kdu_range_set *expansions; // List of objects for use as context expansions
    kdu_range_set *last_used_expansion; // Points to last element of the above
                            // list to be allocated by `add_context_expansion'.
  };

#endif // KDU_CLIENT_WINDOW_H
