/*****************************************************************************/
// File: serve_local.h [scope = APPS/SERVER]
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

#ifndef SERVE_LOCAL_H
#define SERVE_LOCAL_H

#include <assert.h>
#include "kdu_serve.h"

#define KD_MAX_ACTIVE_CODESTREAMS 16
#define KD_MAX_WINDOW_AREA 1000000 // Total samples in active image components

// Defined here:
class kd_chunk_server;
class kd_chunk_output;
struct kd_stream;
struct kd_codestream_window;
struct kd_tile;
struct kd_tile_comp;
struct kd_resolution;
struct kd_precinct_block;
struct kd_precinct;
struct kd_active_precinct;
struct kd_active_bin;
struct kd_meta;
class kd_active_server;
struct kd_serve;


/*****************************************************************************/
/*                               kd_chunk_server                             */
/*****************************************************************************/

class kd_chunk_server {
  public: // Member functions
    kd_chunk_server(int chunk_size)
      {
        this->chunk_size = chunk_size; chunk_prefix_bytes = 0;
        chunk_groups = NULL; free_chunks = NULL;
      }
    ~kd_chunk_server();
    void set_chunk_prefix_bytes(int prefix_bytes)
      { /* Call this function to change the number of prefix bytes which are
           reserved for the application in each subsequent `kds_chunk' object
           returned by `get_chunk'.  The default prefix size is 0, but the
           value may be changed at any point. */
        assert(prefix_bytes < chunk_size);
        chunk_prefix_bytes = prefix_bytes;
      }
    kds_chunk *get_chunk();
      /* The returned chunk structure has most members initialized to 0,
         except for `max_bytes', `prefix_bytes' and `num_bytes'.  The value
         of `num_bytes' is initialized to `prefix_bytes'. */
    void release_chunk(kds_chunk *chunk);
      /* Releases a single chunk. */
  private: // Definitions
      struct kd_chunk_group {
          kd_chunk_group *next;
        };
  private: // Data
    int chunk_size;
    int chunk_prefix_bytes;
    kd_chunk_group *chunk_groups;
    kds_chunk *free_chunks;
  };

/*****************************************************************************/
/*                               kd_chunk_output                             */
/*****************************************************************************/

  /* The following class, provides `kdu_output' derived services for
     transferring compressed data and headers to a list of `kds_chunk'
     objects. */
class kd_chunk_output : public kdu_output {
  public: // Member functions
    kd_chunk_output()
      { chunk = NULL; }
    void open(kds_chunk *first_chunk, int skip_bytes,
              kd_chunk_server *extra_server=NULL)
      { /* After this function returns and prior to the `close' call,
           calls to the base object's `put' member function will augment the
           data already in `first_chunk', as indicated by the value of its
           `chunk_bytes' field on entry, skipping the indicated number of
           initial bytes supplied to `put'.  If additional chunks are
           required, the function will first try to follow the `next' links
           from `first_chunk', in each case augmenting the bytes which may
           already be stored in that buffer.  If no more buffers remain in
           the list attached to `first_chunk', further output data will
           simply be discarded. */
        this->chunk = first_chunk;
        this->skip_bytes = skip_bytes;
      }
    kds_chunk *close()
      /* Returns a pointer to the last chunk to which any information
         was written. */
      {
        flush_buf();
        kds_chunk *result=chunk; chunk = NULL;
        return result;
      }
  protected: // Implementation of base virtual member function
    virtual void flush_buf();
  private: // Data
    kds_chunk *chunk;
    int skip_bytes;
  };

/*****************************************************************************/
/*                                  kd_tile                                  */
/*****************************************************************************/

struct kd_tile {
  public: // Functions
    kd_tile()
      {
        comps = NULL; dispatched_header_bytes=0;
        header_fully_dispatched=uses_ycc=is_active=false;
        total_precincts=0; completed_precincts=0; 
        active_next = NULL;
      }
  public: // Data
    kdu_coords t_idx; // Absolute coordinates of tile in source code-stream.
    kd_stream *stream; // Code-stream to which the tile belongs
    int tnum; // Code-stream tile index, starting from 0 for top-left tile
    kdu_tile source; // Interface to the tile in the `source' codestream
    kdu_tile interchange; // Interface in the `interchange' codestream
    kd_tile_comp *comps; // Array of tile-comps, one for each component
    int num_layers; // Number of quality layers in the tile.
    int header_bytes; // Size of the tile header
    int dispatched_header_bytes; // Tile header bytes transferred already.
    int total_precincts; // Total precincts in this tile
    int completed_precincts; // Number of precincts completely transferred
    bool header_fully_dispatched; // If all tile header bytes transferred.
    bool uses_ycc; // If tile uses a YCC component transform
  public: // Links
    bool is_active; // If belongs to the active list managed by `kd_stream'
    kd_tile *active_next; // Next in active tiles list managed by `kd_stream'
  };

/*****************************************************************************/
/*                                 kd_tile_comp                              */
/*****************************************************************************/

struct kd_tile_comp {
  public: // Functions
    kd_tile_comp() { res = NULL; }
  public: // Data
    int c_idx; // Component index in source tile.
    kd_tile *tile;
    kdu_tile_comp source; // Interface into the `source' codestream
    kdu_tile_comp interchange; // Interface into `interchange' codestream
    int num_resolutions; // Total number of resolution levels available
    kd_resolution *res; // Array of resolutions, starting from lowest (LL)
  };

/*****************************************************************************/
/*                                 kd_precinct                               */
/*****************************************************************************/

struct kd_precinct { // Server state: 8-bytes per precinct, if 32-bit ptrs
    kd_precinct() { cached_bytes = cached_packets = 0; active = NULL; }
    kdu_uint16 cached_bytes; // Bytes already cached by client.
    kdu_uint16 cached_packets; // Packets already available at client.
    kd_active_precinct *active;
  };
  /* Notes:
        This structure is the fundamental element in the cache model managed
     by the `kdu_serve' object.  It maintains information about the
     number of bytes of packet data which have already been transferred
     using `kdu_serve::generate_increments', as well as the number of
     packets which these bytes represent.
        Although undesirable, it can happen that the server application
     transfers only a fractional number of packets.  In this case, the
     `cached_packets' member holds the number of complete packets which have
     been transferred, while the `cached_bytes' member indicates
     the actual total number of bytes which the client is believed to have
     cached (or to be about to cache), including partial packets.  Partially
     cached packets may arise when abandoned messages are returned to the
     `kdu_serve::release_chunks' function, or when a non-trivial
     `max_data_bytes' argument is supplied to `kdu_serve::generate_increments'.
        If all packets of the precinct are known to have been delivered,
     the `cached_packets' member should be set exactly equal to the actual
     number of quality layers in the precinct's tile.   Moreover, this
     value will never be equal to 0xFFFF by design.
        If `cached_packets' has the special value 0xFFFF, the actual number
     of cached packets is unknown.  Only the number of cached bytes is
     known in this case.  The number of cached packets may be deduced from
     the number of cached bytes if necessary at a later point, although
     this requires significant interaction with the `kdu_codestream'
     machinery as packet generation must be synthesized.
        The `active' member holds NULL unless this precinct is currently
     active, in which case it points to a `kd_active_precinct' structure
     which maintains the state of the active precinct. */

/*****************************************************************************/
/*                              kd_precinct_block                            */
/*****************************************************************************/

struct kd_precinct_block {
    kd_precinct_block *next; // Used to build the free list.
    kd_precinct precincts[1]; // Actually a larger array, allocated as required
  };

/*****************************************************************************/
/*                                kd_resolution                              */
/*****************************************************************************/

struct kd_resolution {
  public: // Functions
    kd_resolution() { pblocks = NULL; free_pblocks = NULL; }
    kd_precinct *access_precinct(kdu_coords p_idx, bool create_if_missing=true)
      { /* Return pointer to cache entry for the indicated precinct (absolute
           index).  If `create_if_missing' is true, the function will create
           the containing precinct block, if it does not already exist.
           Otherwise, the function will return NULL in this case. */
        p_idx -= precinct_indices.pos;
        kdu_coords b_idx;
        b_idx.x = p_idx.x >> log_pblock_size;
        b_idx.y = p_idx.y >> log_pblock_size;
        kd_precinct_block **pb = pblocks + b_idx.x + b_idx.y*num_pblocks.x;
        if (*pb == NULL)
          {
            if (!create_if_missing) return NULL;
            *pb = free_pblocks;
            if (*pb == NULL)
              *pb = (kd_precinct_block *) malloc(pblock_bytes);
            else
              free_pblocks = (*pb)->next;
            memset((*pb),0,pblock_bytes);
          }
        p_idx.x-=b_idx.x<<log_pblock_size; p_idx.y-=b_idx.y<<log_pblock_size;
        return (*pb)->precincts + p_idx.x + (p_idx.y<<log_pblock_size);
      }
  public: // Data
    int r_idx; // Resolution index in source tile-component.
    kd_tile_comp *tile_comp;
    kdu_resolution source; // Interface into the `source' codestream
    kdu_resolution interchange; // Interface into `interchange' codestream
    kdu_dims precinct_indices; // Range of indices for accessing precincts
    int pblock_size; // Precinct cache divided into blocks (SxS precincts each)
    int log_pblock_size; // log_2(S), where S=`pblock_size'
    int pblock_bytes; // Num bytes occupied by precinct blocks for this res
    kdu_coords num_pblocks; // Dimensions of the array of precinct cache blocks
    kd_precinct_block **pblocks; // One entry for each precinct cache block.
    kd_precinct_block *free_pblocks; // Free list
  };
  /* Notes:
       The fundamental cache element is that of a precinct.  However, there
       can be an enormous number of precincts in an image (well over a
       million) if it is truly huge.  Moreover, the majority of these might
       all be in a single resolution of a single tile-component.  Since
       relatively few of these will ever be touched by a client, memory
       can be conserved by providing a block-based structure for accessing
       the precinct cache elements.  This structure is described and
       managed by the last few member variables of the present object, starting
       from the `pblock_size' member.
          The `pblock_size' member indicates the nominal width and the height
       of each precinct cache block.  Thus, each cache block manages an
       array of `pblock_size' x `pblock_size' precincts.
          The `log_pblock_size' member holds log_2(`pblock_size'), where
       `pblock_size' is guaranteed to be an exact power of 2.
          `num_pblocks' indicates the number of precinct cache blocks in
       each direction.
          `pblocks' holds one pointer for each cache block.  The pointers are
       organized in raster fashion, with `num_pblocks.x' pointers on each
       row.  Each element of the array holds either NULL or a pointer to
       a `kd_precinct_block' object, whose entries are themselves organized
       in raster scan fashion, measuring `num_pblocks' by `num_pblocks'.
          `free_pblocks' manages a list of free precinct cache block structures
       allowing the resources to be recycled.
  */

/*****************************************************************************/
/*                            kd_active_precinct                             */
/*****************************************************************************/

struct kd_active_precinct {
    kdu_precinct interchange;
    kd_precinct *cache;
    kdu_coords p_idx; // Absolute coordinates of precinct in its resolution
    kd_resolution *res; // Resolution level to which precinct belongs
    int stream_id; // ID of the code-stream to which the precinct belongs
    kdu_uint16 current_packets, current_bytes;
    kdu_uint16 prev_packets, prev_bytes;
    kdu_uint16 prev_packet_bytes, current_packet_bytes;
    kdu_uint16 next_layer;
    kdu_uint16 max_packets; // Num layers in the tile to which precinct belongs
    double relevance; // Region overlap factor, in range 0 to 1
    int log_relevance; // ~256*log_2(relevance)
    const int *layer_log_slopes; // See below
    kd_active_precinct *next;
    kd_active_precinct *prev;
  };
  /* Notes:
        This structure plays a central role in managing the active state of
     the server.  Whereas a `kd_precinct' structure might be maintained
     for every precinct in the image (although we would hope to have a lot
     of empty precinct blocks if the image is very large) the current
     structure is used only for precincts which belong to the current
     window maintained by the `kdu_serve' object.  The structure is used
     in two ways, as follows:
        a) It appears in a doubly linked list of currently active precincts
           maintained by the `kd_serve' object.  This list contains exactly
           one element for each precinct which belongs to the currently
           active window.  The `next' and `prev' fields serve as the links
           in this list.  When the window changes, the elements of this list
           which belong to the new window are salvaged, while the other
           elements are discarded, after closing their open precincts.  All
           elements in this list have non-NULL `interchange' objects, which
           serve to interface with an active precinct manager in the core
           system.
        b) It apears in a singly linked list of precincts attached to each
           chunk returned by the `kdu_serve::generate_increments'
           function. In this capacity, the structure serves to identify
           any data which was not transmitted in abandoned chunks
           returned to the `kdu_serve::release_chunks' function, so that
           the internal state can be modified to reflect the fact that
           this information did not arrive.  All elements in this list have
           NULL `interchange' objects (i.e., `interchange.exists' returns
           false).  The active interface into the core system is maintained
           by `cache->active->interchange' if `cache->active' is non-NULL.
        The `prev_bytes' and `prev_packets' members record the number of bytes
     and the number of whole packets which have previously been transferred
     in chunks returned by `kdu_serve::generate_increments'.  The value
     of `prev_bytes' may exceed the number of bytes associated with
     `prev_packets', if a packet has been partially transferred.
        The `current_bytes' and `current_packets' members record the
     total number of bytes and the number of complete packets which would be
     cached by the client if the chunks generated by
     `kdu_serve::generate_increments' were completely transferred to the
     client.  The value of `current_bytes' may exceed the number of bytes
     associated with `current_packets', if a packet has been partially
     transferred.
        The `prev_packet_bytes' member records the number of bytes associated
     with `prev_packets' whole packets, while `current_packet_bytes' records
     the number of bytes associated with `current_packets' whole packets.
     These are often identical to, but can be less than the `prev_bytes'
     and `current_bytes' values, respectively.
        The `next_layer' member holds the index of the next quality layer
     (equivalently, the index of the next packet) to be considered for
     inclusion in a collection of data being prepared by `generate_increments'.
     `next_layer' will commonly be equal to `current_packets', but may be
     larger, if `simulate_packet_increments' has encountered one or more
     packet too small for inclusion by themselves.
        As for the `kd_precinct' structure, the special value 0xFFFF for the
     `prev_packets' or `current_packets' fields has the interpretation that
     the actual number of packets is not known and must be deduced from the
     number of available bytes.  This special value appears only when the
     contents of a `kd_precinct' structure are used to initialize
     `kd_active_precinct'.  The `max_packets' member will never be equal to
     0xFFFF.
        The `layer_log_slopes' array points to the relevant code-stream's
     `layer_log_slopes' array, or to the `kd_serve' object's
     `dummy_layer_slopes' array, depending on whether or not all active
     code-streams have layer log slope information of their own.  If some
     do not, all active precincts must use the dummy slope information
     for consistency.
   */

/*****************************************************************************/
/*                                 kd_meta                                   */
/*****************************************************************************/

struct kd_meta {
  public: // Member functions
    kd_meta()
      { metagroup=NULL; next_in_bin=prev_in_bin=phld=active_next=NULL; }
    ~kd_meta()
      { // Recursively deletes the sub-trees.
        kd_meta *mp;
        while ((mp=phld) != NULL)
          { phld = mp->next_in_bin; delete mp; }
      }
    int init(const kds_metagroup *metagroup, kdu_long bin_id,
             int bin_offset, int tree_depth);
      /* Recursive function, initializes a newly constructed `kd_meta'
         object to associate itself with the supplied `metagroup' and
         then recursively creates the placeholder list if `metagroup->phld'
         is non-NULL.  Returns the total length of the supplied
         `kds_metagroup' object and all its descendants (via `phld' links).
         The `bin_offset' argument holds the offset from the start of the
         data-bin until the first byte of the current metagroup.  The
         `tree_depth' argument holds 0 if `bin_id'=0 and larger values for
         each successively deeper data-bin in the hierachy.  `tree_depth'
         also corresponds to the depth of the top level boxes in the
         metadata-bin within the JP2 box hierarchy. */
    bool find_active_scope_and_sequence(kdu_window &window,
                                        kd_codestream_window *stream_windows,
                                        bool force_in_scope=false,
                                        bool force_headers_in_scope=false,
                                        int force_max_sequence=INT_MAX,
                                        int *max_descendant_bytes=NULL);
      /* Tests the scope of the current object against the supplied window,
         setting the `in_scope' and `sequence' members accordingly.  If
         `window.metareq' is non-NULL, the function scans the list of metadata
         requests to determine whether or not a metadata group should be
         considered in scope and, if so, how much of it is in scope.
         Otherwise, everything which can be considered relevant to the
         window region is considered in scope.
            If the `phld' member is non-NULL, the function recursively passes
         through the metagroup list associated with the placeholder,
         evaluating the scope and sequence of those elements.  If any
         are in scope, the present object must also be brought into scope
         and its sequence priority adjusted to be at least as low (low
         sequence means high priority) as that of the in-scope placeholder
         elements.  Once an in-scope element is found, the function also
         scans backwards through the list of elements which belong to the
         same data-bin, setting them all to be in scope, with sequence
         numbers no larger than that found for the current element.  This
         is important since data-bins are delivered in linear fashion.
            If `force_in_scope' is true, the object will be forced into
         scope regardless of the window, with a sequence number at least as
         large as the value supplied via `force_max_sequence'.  This is used
         to force the contents of a box to be in scope when the box header
         (embedded in a placeholder) is in scope, unless only the header is
         actually being requested.
            If `force_headers_in_scope' is true, the headers of all
         boxes in the meta-group and all its descendants are forced into scope.
         This is required when a `kdu_metareq' entry is encountered with
         the `kdu_metareq::recurse' member set to true.
            The `force_max_sequence' argument is used with the above arguments
         to pass the sequence number so far assigned to the parent group.
            If `max_descendant_bytes' is non-NULL, the total number
         of bytes to be forced into scope by `force_in_scope' from
         all relevant sub-boxes, is limited by the value referenced by this
         argument.  The value is adjusted as the function passes recursively
         through the metagroup hierarchy.
            The function returns true only if the contents of some box
         have been placed in scope (not just the headers). */
    void force_active_scope_and_sequence(kdu_int32 max_sequence);
      /* Passes recursively through this metagroup and all its descendants,
         placing them in scope, with a sequence index no larger than
         `max_sequence', and requiring that the entire contents of all
         boxes encountered in this way be placed in scope.  The function
         is currently invoked only if an association box is encountered,
         one or more of whose sub-boxes have been placed in scope along
         with their contents. */
    void include_active_groups(kd_meta *start);
      /* Checks to see if the object is in scope (has a `kds_metascope' entry
         which intersects with the current window).  If so, it is added into
         the active list at a place starting no earlier than that identified
         by `start', which is necessarily non-NULL.  Since `start' must be
         non-NULL, it is possible that the caller needs to make `start'
         reference the current object.  The implementation needs to be
         aware of this possibility to avoid creating self-referential loops.
            The function recursively passes through the metagroups attached
         to a non-NULL `phld' link, including them also in the active list,
         so long as they are in scope.  Each time elements are included in
         the active list, they are included in such a way as to ensure that
         sequence numbers are non-decreasing as the list is traversed.
         The active list is connected via the `active_next' members. */
  public: // Data
    kdu_long bin_id; // metadata-bin identifier
    int tree_depth; // Depth of top-level boxes in the bin within box hierarchy
    const kds_metagroup *metagroup;
    kd_meta *next_in_bin; // Parallels `kd_metagroup::next'
    kd_meta *prev_in_bin; // Points to previous element in the same data-bin
    kd_meta *phld; // Same as `kds_metagroup::phld' for this parallel structure
    kdu_int32 bin_offset; // Offset from start of data-bin to this group
    kdu_int32 num_bytes; // Same as `metagroup->length'
    kdu_int32 dispatched_bytes; // Num bytes already sent out
    bool fully_dispatched; // If all bytes have been fully transferred
    bool in_scope; // True if the metagroup lies within current window's scope
    int max_content_bytes; // See below
    int sequence; // Sequence priority if `in_scope' is true
    int active_length; // Max bytes to be sent, based on `max_content_bytes'
    kd_meta *active_next; // For linking active metadata list in `kdu_serve'
  };
  /* Notes:
        The `max_content_bytes' member identifies an upper bound on the
        number of content bytes which are of interest to the client from
        any JP2 box represented by this object.  In practice, the server
        will attempt to deliver all bytes up to and including the header of
        the last JP2 box represented by the `kds_metagroup' object, plus
        up to `max_content_bytes' additional bytes, coming from the contents
        of the last JP2 box in the group.  The `kds_metagroup' object
        essentially controls the granularity with which metadata may be
        selectively disseminated.  Note that `max_content_bytes' is adjusted
        along with `in_scope' and `sequence', whenever the
        `find_active_scope_and_sequence' member function is called.
  */

/*****************************************************************************/
/*                               kd_active_bin                               */
/*****************************************************************************/

struct kd_active_bin {
    kd_stream *stream; // If data-bin refers to a main code-stream header
    kd_tile *tile; // If data-bin refers to a tile header
    kd_meta *meta; // If data-bin refers to a metadata-bin
    int prev_bytes;
    kd_active_bin *next;
  };
  /* Notes:
       This structure plays a similar, though much simpler role for code-stream
     header data-bins and metadata-bins as that played by `kd_active_precinct'
     for precinct data-bins.  A singly-linked list of these structures is
     attached to each `kds_chunk' object, identifying the non-precinct
     data-bins which contribute to that chunk.  If an abandoned
     chunk is returned to `kdu_serve::release_chunks', the
     cache model for any data-bins identified in that chunk can be updated.
        If `stream' is non-NULL, `stream->dispatched_header_bytes'
     must be set to the value of `prev_bytes' unless
     `stream->dispatched_header_bytes' already has an even smaller
     non-negative value.
        Similarly, if `tile' is non-NULL, `tile->dispatched_header_bytes' is
     set to `prev_bytes', unless it is already smaller.
        If `meta' is non-NULL, `meta->dispatched_bytes' is
     set to `prev_bytes' unless it is already smaller than that value.
        Exactly one of the `stream', `tile' and `meta' members will be
     non-NULL. */

/*****************************************************************************/
/*                              kd_active_server                             */
/*****************************************************************************/

class kd_active_server {
  public: // Member functions
    kd_active_server()
      { groups = NULL; free_list = NULL; }
    ~kd_active_server();
    kd_active_precinct *get_precinct();
      /* The returned record has all fields initialized to zero. */
    kd_active_bin *get_bin()
      { /* The returned record has all fields initialized to zero. */
        return (kd_active_bin *) get_precinct();
      }
    void release_precincts(kd_active_precinct *list)
      { /* Releases an entire list of active precincts, connected via their
           `next' fields. */
        kd_active_precinct *elt;
        while ((elt=list) != NULL)
          { list = elt->next; elt->next = free_list; free_list = elt; }
      }
    void release_bins(kd_active_bin *list)
      { /* Releases an entire list of active ancillary data-bin references,
           connected via their `next' fields. */
        kd_active_bin *elt;
        kd_active_precinct *pelt;
        while ((elt=list) != NULL)
          {
            list = elt->next; pelt = (kd_active_precinct *) elt;
            pelt->next = free_list; free_list = pelt;
          }
      }
  private: // Definitions
      struct kd_active_group {
          kd_active_group *next;
          kd_active_precinct precincts[32]; // Actual size is much larger
        };
  private: // Data
    kd_active_group *groups;
    kd_active_precinct *free_list; // List of recycled precincts
  };

/*****************************************************************************/
/*                           kd_codestream_window                            */
/*****************************************************************************/

struct kd_codestream_window {
  public: // Member functions
    kd_codestream_window();
    ~kd_codestream_window()
      {
        if (codestream_components != NULL) delete[] codestream_components;
        if (context_components != NULL) delete[] context_components;
        if (scratch_components != NULL) delete[] scratch_components;
      }
    kdu_long initialize(kd_stream *stream, kdu_coords resolution,
                        kdu_dims region, int round_direction,
                        kdu_range_set &component_ranges,
                        int num_context_components=0,
                        const int *context_component_indices=NULL);
      /* Associates the object with the given `stream' and configures the
         `active_discard_levels', `active_region', `active_tiles',
         `component_ranges' and `context_component_indices' members.
         Returns a rough estimate of the area represented by the view window,
         summed over all image components and taking their sub-sampling
         factors into account.
            The `component_ranges' argument identifies the set of
         codestream components which might belong to the window.  If this is
         an empty set, the internal `component_ranges' member is
         initialized to represent all codestream image components.  If
         `num_context_components' <= 0, there are no further restrictions
         on the set of relevant codestream components.  Otherwise,
         `context_component_indices' must point to an array with
         `num_context_components' entries, which identify the set of
         output image components which are of interest -- these have
         been derived from a codestream context.  For a discussion of
         the differences between output and codestream image components,
         see the `kdu_codestream::apply_input_restrictions' function.
         For our present purposes here, it is sufficient to realize that
         each output component may be formed from multiple codestream
         components and that the mapping between output components and
         codestream components may vary from tile-to-tile.  For this reason,
         we defer the final determination of those codestream components
         which are of interest to the `kd_stream::include_active_precincts'
         function, at which point tiles of interest are opened.  At that
         point, the set of codestream components which are associated
         with the output image components will be intersected with the
         components identified by the `component_ranges' member.
            Since it is not generally possible to exactly determine the
         number of codestream components which belong to the window here,
         we estimate this quantity based upon the information supplied
         by `component_ranges' and `context_component_indices' arguments.
         This information is then used to derive the function's return
         value, which is supposed to represent the total number of
         codestream samples in the window. */
    int *get_scratch_components(int num_needed)
      {
        if (num_needed > max_scratch_components)
          {
            max_scratch_components += num_needed;
            if (scratch_components != NULL)
              delete[] scratch_components;
            scratch_components = new int[max_scratch_components];
          }
        return scratch_components;
      }
  public: // Members installed by `initialize'
    kd_stream *stream;
    int stream_idx; // Same as `stream->stream_id'
    int active_discard_levels; // Divide canvas dims by 2^d to get active dims
    kdu_dims active_region; // view dims mapped to the high-res canvas
    kdu_dims active_tiles; // Indices of tiles which cover `active_region'
    kdu_range_set component_ranges; // See `initialize'
    bool expand_ycc; // True if `component_ranges' contains any of comps 0,1,2
    int num_codestream_components;
    int max_codestream_components;
    int *codestream_components; // See below
    int num_context_components;
    int max_context_components; // Num elements in `context_component_indices'
    int *context_components;
  public: // Members managed by `kd_serve' directly
    int context_type; // If != 0, window is translated from codestream context
    int context_idx; // Index of context, if `context' is non-NULL
    int member_idx; // Codestream member within context, if `context' non-NULL
    int remapping_ids[2]; // Copied from `kdu_sampled_range::remapping_ids'
    kd_codestream_window *next; // For managing linked lists
    kd_codestream_window *prev; // List of active stream windows doubly linked
  private: // Scratch space for calculating tile-components in use
    int max_scratch_components;
    int *scratch_components;
  };
  /* Notes:
       The `codestream_components' array contains an expansion of the
     individual component indices identified by `component_ranges', except
     that if any of the first 3 components is identified within these
     ranges (i.e., if `expand_ycc' is true), all 3 will be mentioned in the
     `codestream_components' array.  Although redundant, this array simplifies
     the process of walking through relevant components when building
     active precinct lists. */

/*****************************************************************************/
/*                                 kd_stream                                 */
/*****************************************************************************/

struct kd_stream {
  public: // Member functions
    kd_stream(kd_serve *owner);
    ~kd_stream();
    void initialize(int stream_id, kdu_codestream codestream,
                    kdu_serve_target *target, bool ignore_relevance_info);
      /* Does most of the construction work. */
    void init_tile(kd_tile *tp);
      /* Performs initialization steps for a specific tile when it is
         accessed for the first time.  The `tp->t_idx' member must be
         valid on entry, but `tp->tnum' is set automatically.  This
         function should not be called from a context in which the
         underlying codestream has any input restrictions applied
         (such restrictions are applied only within `include_active_precincts'
         and lifted before that function returns). */
    void init_tile(kd_tile *tp, int tnum);
      /* Alternate form of `init_tile' which may be used to initialize a
         tile when the `tp->t_idx' field has not yet been initialized.  The
         location is calculated from the tile number, `tnum'. */
    int include_active_tiles(kd_codestream_window *stream_windows);
      /* This function is called from within `kd_serve::generate_active_lists'
         to mark up the list of active tiles for this codestream.  On entry,
         the list of active tiles should be empty.  The list of active
         tiles is derived from all codestream windows in the list
         headed by `stream_windows' which use the current codestream.
         The function returns the maximum number of quality layers associated
         with any of the active tiles. */
    void include_active_precincts(kd_active_precinct * &old_list,
                                  kd_active_precinct * &new_head,
                                  kd_active_precinct * &new_tail,
                                  kd_codestream_window *stream_windows,
                                  kd_active_server *active_server,
                                  int *dummy_layer_log_slopes,
                                  bool ignore_relevance_info);
      /* Call this function after each element of the `stream_windows' list
         has had its `kd_codestream_window::initialize' function called.
         The function scans through the `stream_windows' list looking for
         objects which are associated with the present stream.  For each
         such object, the function scans through all new active precincts,
         inserting them into the `new_list', and removing them from the
         `old_list' if necessary.  This function does a great deal of the
         work of `kd_serve::generate_active_lists'.
            If `dummy_layer_log_slopes' is non-NULL, it should be used in
         place of `kd_stream::layer_log_slopes' when writing the
         `layer_log_slopes' member of each active precinct.
            If `ignore_relevance_info' is false, the active precincts within
         each resolution are ordered based on their log-relevance values.
         Note that the log-relevance values are used only to order active
         precincts within resolutions, not between resolutions.  This ensures
         that lower resolutions always precede higher resolutions, which is
         generally desirable.  It also limits the number of precincts which
         must be included in the sorting operation, thereby avoiding
         excessive computation. */
    void open_active_tiles()
      {
        for (kd_tile *tp=active_tiles; tp != NULL; tp=tp->active_next)
          if (!tp->interchange)
            tp->interchange = interchange.open_tile(tp->t_idx);
      }
    void close_active_tiles()
      {
        for (kd_tile *tp=active_tiles; tp != NULL; tp=tp->active_next)
          if (tp->interchange.exists()) tp->interchange.close();
      }
  public: // Configuration parameters and resources
    kd_serve *owner;
    int stream_id;
    int *layer_log_slopes; // See below
    int max_layer_slopes; // Size of above array
    kdu_serve_target *target;
    kdu_codestream source; // Obtained from `kdu_serve_target'.
    kdu_codestream interchange; // Our own private interchange manager
    kdu_dims image_dims; // Dimensions of full image
    kdu_dims tile_partition; // pos=tile origin on canvas; size=tile size
    int num_components;
    kdu_coords *component_subs;
    int num_output_components;
    kdu_coords *output_component_subs;
    int total_tiles;
    kdu_dims tile_indices; // Range of indices used for accessing tiles
    kd_tile *tiles; // Array of tiles, in raster scan order
    int main_header_bytes; // Size of the main header
  public: // State information
    int dispatched_header_bytes; // Initial main header bytes transferred.
    bool header_fully_dispatched; // True if main header completely transferred
    int completed_tiles; // Total tiles whose contents are fully transferred
  public: // Window related state information
    bool is_active; // True if referenced by any active `kd_codestream_window'
    bool is_locked; // If codestream locked, except in `generate_increments'
    kd_tile *active_tiles; // Head of active tile list
  public: // Links
    kd_stream *next; // Next code-stream in sequence
  };
  /* Notes:
       `layer_log_slopes' is an array with `max_layer_slopes' entries.  The
     value of `max_layer_slopes' is guaranteed to be at least as large as
     `max_active_layers'+1, which means that we will generally have to invent
     at least one layer slope threshold.  If the code-stream contains no
     information about the quality layer slope thresholds in a COM marker
     segment, we will have to invent thresholds for all of the layers.  Where
     the information is not invented, the value of `layer_log_slopes'[i] is
     equal to 256*log_2(`slope') where `slope' is the distortion length
     slope (gradient measured as the reduction in MSE distortion, divided by
     the increase in code length) threshold associated with quality layer i.
     The values in this array are non-increasing.  Where `layer_log_slopes'[i]
     must be invented, it is set to `layer_log_slopes'[i-1]-2^15.
  */

/*****************************************************************************/
/*                                 kd_serve                                  */
/*****************************************************************************/

struct kd_serve {
  public: // Member functions
    kd_serve(kdu_serve *owner);
    ~kd_serve();
    void initialize(kdu_serve_target *target, int max_chunk_size,
                    bool ignore_relevance_info);
      /* Implements `kdu_serve::initialize'. */
    void process_window_changes();
      /* Called from within `kdu_serve::generate_increments' if the
         `window_changed' member is true.  Modifies the window and associated
         parameters to make it legal.  Returns with `window_changed' false,
         with the `is_active' member of each `kd_stream' object set to
         indicate whether or not that code-stream is relevant to the window,
         and with the `stream_windows' list configured.  Removes active
         tiles from all codestreams. */
    void generate_active_lists();
      /* Called from within `kdu_serve::generate_increments' if the
         `active_lists_invalid' member is true.  Modifies the existing lists
         of active precincts, active tiles, and active metadata groups to
         conform to those required by the current window. */
    int simulate_packet_generation(int suggested_body_bytes,
                                   int max_body_bytes, bool align);
      /* This function is called from within `generate_increments' to
         simulate packet lengths and hence determine the number of packets
         and the number of bytes by which to advance each precinct in the
         active list.  Returns the total number of body bytes associated with
         the simulated packet contributions, whose lengths and packet numbers
         are recorded in the `kd_active_precinct' structures in the current
         active precinct list. */
    kds_chunk *
      generate_header_increment(kds_chunk *chunk, kds_id_encoder *id_encoder,
                                kd_stream *str, int tnum,
                                int skip_bytes, int increment, bool is_final,
                                bool decouple_chunks);
      /* Generates `increment' extra bytes from a main or tile header of
         the indicated code-stream, writing the result into a list of data
         chunks, starting with the one referenced by `chunk'.  On entry,
         `chunk' is also the last element in the supplied list, but the
         function allocates new chunks as necessary, using the `chunk_server'
         object, returning a pointer to the last chunk in the list.  The
         initial `skip_bytes' of the header are skipped.  `tnum' is -1 if we
         are writing a main code-stream header.  Otherwise, it is the index
         of the tile whose header is being written.  If `decouple_chunks'
         is true, the `id_encoder' object's `decouple' function is
         called each time a new chunk is created. */
    kds_chunk *
      generate_meta_increment(kd_meta *meta,
                              kds_id_encoder *id_encoder,
                              int increment, kds_chunk *chunk,
                              bool decouple_chunks);
      /* Generates `increment' extra bytes from the supplied group of metadata
         boxes, augmenting the state of the group and also the global count
         of dispatched metadata bytes, as it goes.  On entry, `chunk' is
         the last element in a list of data chunks; the data is written to
         that chunk.  The function allocates further data chunks as necessary
         and returns a pointer to the last chunk in the list.  If
         `decouple_chunks' is true, the `id_encoder' object's `decouple'
         function is called each time a new chunk is created. */
    kds_chunk *generate_increments(int suggested_data_bytes,
                                   int &max_data_bytes, bool align,
                                   bool use_extended_message_headers,
                                   bool decouple_chunks,
                                   kds_id_encoder *id_encoder);
      /* Does most of the work of `kdu_serve::generate_increments'. */
  //---------------------------------------------------------------------------
  public:
    void adjust_cache_model(int databin_class, int stream_min, int stream_max,
                            kdu_long bin_id, int available_bytes,
                            int available_packets, bool truncate);
    void adjust_cache_model(int stream_min, int stream_max, int tmin, int tmax,
                            int cmin, int cmax, int rmin, int rmax,
                            int pmin, int pmax, int available_bytes,
                            int available_packets, bool truncate);
      /* These functions implement `kdu_serve::augment_cache_model' or
         `kdu_serve::truncate_cache_model', depending on the value of the
         `truncate' flag. */
    void augment_precinct_cache(kd_precinct *precinct, int available_bytes,
                                int available_packets, kd_tile *tile);
    void truncate_precinct_cache(kd_precinct *precinct, int available_bytes,
                                 int available_packets, kd_tile *tile);
      /* These functions do the actual work of `adjust_cache_model' once the
         the precinct has been identified. */
    void augment_stream_header_cache(kd_stream *stream, int available_bytes);
    void truncate_stream_header_cache(kd_stream *stream, int available_bytes);
      /* Same as above function, but for code-stream main headers. */
    void augment_tile_header_cache(kd_tile *tile, int available_bytes);
    void truncate_tile_header_cache(kd_tile *tile, int available_bytes);
      /* Same as above function, but for tile headers. */
    void augment_meta_cache(kd_meta *meta, int available_bytes);
    void truncate_meta_cache(kd_meta *meta, int available_bytes);
      /* Same as above function, but for metadata-bins.  This function
         differs from the above only in the sense that `kd_meta' represents
         only one group of boxes from a metadata-bin; there may be others.
         Nevertheless, `available_bytes' refers to the entire metadata-bin. */
  //---------------------------------------------------------------------------
  private: // Helper functions
    kd_stream *add_active_codestream(int stream_idx);
      /* Adds to the `active_codestream_indices' array and creates a new
         `kd_stream' object, as required, returning the relevant `kd_stream'
         object.  Does not actually set `kd_stream::is_active' to true so
         that the caller can determine whether the codestream has already
         been activated or not.  Returns NULL if the requested codestream
         can be delivered only as meta-data, or if the maximum number of
         active codestreams has been exceeded. */
    kd_codestream_window *add_stream_window();
      /* Adds a new codestream window to the end of the `stream_windows' list,
         returning it (without any initialization). */
  //---------------------------------------------------------------------------
  public: // Embedded services
    kd_chunk_server *chunk_server;
    kd_active_server *active_server;
  public: // Structural information
    kdu_serve_target *target;
    bool ignore_relevance_info;
    int max_dummy_layer_slopes;
    int *dummy_layer_slopes; // Use these if streams don't have slope info
    kd_stream *streams; // List of all code-streams
    kd_meta *metatree; // Metadata is global
    int total_meta_bytes; // Total bytes in metadata-bins
  public: // Global completion counters
    int dispatched_meta_bytes; // Number of above bytes written to chunks
    kds_chunk *extra_data_head; // Holds the outstanding extra data pushed in
    kds_chunk *extra_data_tail; // by `kdu_serve::push_extra_data'
    int extra_data_bytes; // Number of data bytes in the above list.
  public: // Window state information
    int num_active_codestreams;
    int active_codestream_indices[KD_MAX_ACTIVE_CODESTREAMS];
    kd_codestream_window *stream_windows;
    kd_codestream_window *last_stream_window;
    kd_codestream_window *free_stream_windows; // Recycling list
    kdu_window current_window; // Returned by `kdu_serve::get_window'.
    bool window_changed; // If `kdu_serve::set_window' changed above member
    kdu_window last_translated_window; // See below
    kd_active_precinct *active_precincts;
    kd_meta *active_metagroups;
    bool active_lists_invalid; // True if above lists need conforming to window
    kd_active_precinct *precinct_ptr; // Preserve precinct list scanning state
    kd_meta *meta_ptr; // Preserve active metagroup list scanning state
    int active_threshold; // See below
    int next_active_threshold; // See below
    int max_active_layers;
    int completed_layers; // Num fully transferred layers from active precincts
    bool scan_first_layer; // See below
  private:
    kdu_range_set context_set; // Temporary storage for requested contexts
    kdu_range_set stream_set; // Temporary storage for requested codestreams
    kds_id_encoder default_id_encoder;
    kd_chunk_output out;
    kdu_serve *owner;
  };
  /* Notes:
        `last_translated_window' holds a copy of the window, as passed into
     `kdu_serve::set_window', which was last translated via
     `process_window_changes'.  If `window_changed' is false, this window
     differs from `current_window' only in regard to changes which have
     been made by the server, either in translating codestream contexts,
     or in restricting the region of interest so as to conserve resources.
     If `window_changed' is true, the `last_translated_window' and
     `current_window' members may be used to identify any changes between
     the previous and current requests.
        `precinct_ptr' holds a pointer to the next element of the
     `active_precincts' list which is to be considered by the
     `simulate_packet_generation' function.
        `active_threshold' holds the threshold which is currently being used by
     `simulate_packet_generation' to determine the next packet contribution
     to be included in the batch of increments being prepared by
     `generate_increments'.  At each element in the active precinct
     list, `active_threshold' is compared against `layer_log_slopes'[i] +
     delta where `i' is the value of the element's `next_layer' member,
     delta is the value of the element's `log_relevance' member, and
     `layer_log_slopes' is the `kd_stream::layer_log_slopes' array for
     the element's code-stream.  If `active_threshold' is less than or
     equal to this value, the number of packets for that precinct is
     considered for augmentation.
        `next_active_threshold' is the value which should be assigned to
     `active_threshold' once `active_ptr' reaches the end of the `active_list'.
     It is computed incrementally, while the active list is being processed,
     in such a way as to minimize the number of processing passes, while
     maintaining the desired ordering properties.  Specifically, while scanning
     through the list of active precincts, the value of `next_active_threshold'
     is set to the smallest value which ensures that no more than 1 layer
     will be included from any precinct during the next scan.
       `scan_first_layer' is set to true whenever the list of active precincts
     is changed.  When this member is true, the next scan through the active
     precinct list inside `simulate_parsing' will ignore the `active_threshold'
     value and consider the first layer of every precinct as a candidate for
     inclusion in the increments being prepared by `generate_increments',
     unless that layer has already been included.  The reason for treating
     the first layer differently is that its log-slope-threshold value is
     not a reliable indication of the actual distortion-length slopes of the
     underlying code-block coding passes; the slopes may have been arbitrarily
     steeper than this threshold.  As a result, the R-D optimal layer
     resequencing algorithm may fail to allocate sufficient weight to
     lower resolution precincts, which often have reduced log-relevance
     due to the fact that their region of influence is larger than the
     window region.  After the first scan through the active precinct list,
     `scan_first_layer' reverts to false, but it may be set to true again
     if the cache model for one of the active precincts is downgraded.
  */

#endif // SERVE_LOCAL_H
