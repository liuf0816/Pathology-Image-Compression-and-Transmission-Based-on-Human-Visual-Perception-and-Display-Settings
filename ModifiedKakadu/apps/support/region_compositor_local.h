/******************************************************************************/
// File: region_compositor_local.h [scope = APPS/SUPPORT]
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
  Local definitions used in the implementation of `kdu_region_compositor'.
*******************************************************************************/
#ifndef REGION_COMPOSITOR_LOCAL_H
#define REGION_COMPOSITOR_LOCAL_H

#include "kdu_region_decompressor.h"
#include "kdu_region_compositor.h"

// Objects declared here
struct kdrc_queue;
struct kdrc_roinode;
class kdrc_overlay;
struct kdrc_refresh_elt;
class kdrc_refresh;
struct kdrc_codestream;
class kdrc_stream;
class kdrc_layer;

/******************************************************************************/
/*                                 kdrc_queue                                 */
/******************************************************************************/

struct kdrc_queue {
  public: // Member functions
    kdrc_queue()
      { composition_buffer=NULL; id=0; stamp=0; next=NULL; }
    ~kdrc_queue()
      { assert(composition_buffer == NULL); }
  public: // Data
    kdu_compositor_buf *composition_buffer;
    int id;
    kdu_long stamp;
    kdrc_queue *next;
  };

/******************************************************************************/
/*                                kdrc_roinode                                */
/******************************************************************************/

struct kdrc_roinode {
  public: // Member functions
    kdrc_roinode() { next=NULL; painted=false; }
  public: // Data
    jpx_metanode node;
    kdu_dims bounding_box; // Bounding box mapped to compositing grid
    kdrc_roinode *next;
    bool painted; // True once this node has been painted
  };
  /* Notes:
       This structure is used to manage lists of metadata nodes which have
     been found to be relevant to the current buffer surface.  Such lists
     are managed by `kdrc_overlay'. */

/******************************************************************************/
/*                                kdrc_overlay                                */
/******************************************************************************/

class kdrc_overlay {
  public: // Member functions
    kdrc_overlay(jpx_meta_manager meta_manager, int codestream_idx);
      /* Constructed from within `kdrc_stream::init' where a JPX data source
         is available.  Otherwise, the `kdrc_stream' object will have no
         overlay. */
    ~kdrc_overlay();
    void set_geometry(kdu_coords image_offset, kdu_coords subsampling,
                      bool transpose, bool vflip,
                      bool hflip, kdu_coords expansion_numerator,
                      kdu_coords expansion_denominator,
                      kdu_coords compositing_offset);
      /* This function is called from the owning `kdrc_stream' object whenever
         the scale or orientation changes.  It provides the parameters
         required to map regions on the compositing surface to and from
         regions on the codestream's high resolution canvas, expressed relative
         to the image origin on that canvas.
         [//]
         The arguments may be interpreted in terms of a mapping from the
         coordinates used by ROI description boxes to the corresponding
         coordinates on the compositing reference grid.  Let `R' denote
         an object of the `kdu_dims' class, corresponding to an ROI
         description region.  The following steps will map `R' to a
         region on the compositing reference grid:
            1) Add `image_offset' to `R'.pos.  This translates the region
               to one which is correctly registered against the codestream's
               high resolution canvas.
            2) Divide the starting (inclusive) and ending (exclusive)
               coordinates of the region by `subsampling', rounding all
               coordinates up.
            3) Apply `kdu_dims::to_apparent', supplying the `transpose',
               `vflip' and `hflip' parameters.
            4) Multiply the coordinates by `expansion_numerator' over
               `expansion_denominator'.
            5) Subtract `compositing_offset'.
      */
    bool set_buffer_surface(kdu_compositor_buf *buffer,
                            kdu_dims compositing_region,
                            bool look_for_new_metadata);
      /* Called only from within `kdrc_layer', after `set_geometry' has
         been called from `kdrc_stream'.  This function removes all
         inappropriate metadata from the current list and updates internal
         state to reflect the portion of the overlay buffer which has still
         to be painted.  The function returns true if any metadata falls
         within the `compositing_region'.
            The function does not actually paint any metadata overlays.  It
         just schedules them to be painted in subsequent calls to `process'.
            If `look_for_new_metadata' is true, the function examines all
         metadata within the buffer region to see if anything new has arrived
         (in a dynamic cache, for instance). */
    bool update_config(int min_display_size, int painting_param);
      /* If `painting_param' differs from the last used painting parameter,
         all metadata entries are marked as unpainted, so that subsequent calls
         to `process' will paint them with the new parameter.  If
         `min_display_size' has changed, some new overlay entries may be added
         or some may be removed.  In the latter case, the function returns
         true, signalling to the caller that the entire overlay surface will
         need to  be repainted. */
    bool process(kdu_dims &new_region);
      /* Call this function to process any unpainted overlay data onto the
         buffer installed by the last call to `set_buffer_surface'.  The
         function returns false if nothing more can be processed, either
         because there is no overlay buffer, or all metadata found in the last
         call to `set_buffer_surface' has been painted.  If the function does
         paint something, it returns true, setting `new_region' to the bounding
         box of the newly painted overlay region. */
    jpx_metanode search(kdu_coords point, int &codestream_idx);
      /* Does the work of `kdrc_layer::search_overlay', returning a non-empty
         interface and setting `codestream_idx' if a match is found. */
    void activate(kdu_region_compositor *compositor);
      /* Called from `kdrc_layer' when it is initiated or activated. */
    void deactivate();
      /* Called from `kdrc_layer' when it is deactivated. The function may
         also be called immediately prior to `activate' to initiate a
         complete refresh of the buffer surface. */
  private: // Helper functions
    void map_from_compositing_grid(kdu_dims &region);
    void map_to_compositing_grid(kdu_dims &region);
  private: // Identification members
    kdu_region_compositor *compositor; // NULL unless activated
    jpx_meta_manager meta_manager;
    int codestream_idx; // Overlays metadata belonging onto to this codestream
    int min_composited_size;
  private: // Geometry members passed in using `set_geometry'
    kdu_coords image_offset;
    kdu_coords subsampling;
    bool transpose, vflip, hflip;
    kdu_coords expansion_numerator;
    kdu_coords expansion_denominator;
    kdu_coords compositing_offset;
    int min_size;
  private: // Buffering members
    kdu_dims buffer_region; // Expressed in compositing domain
    kdu_compositor_buf *buffer;
    int painting_param;
  private: // Metadata list
    kdrc_roinode *head;
    kdrc_roinode *tail; // Points to last element in list managed by `head'
    kdrc_roinode *last_painted_node; // Points into list managed by `head'
    kdrc_roinode *free_nodes;
  };
  /* Notes:
       This class is used to manage overlay information which is generated
     from metadata found in a JPX source.  Each `kdrc_overlay' object is
     owned by exactly one `kdrc_stream' object.  The `kdrc_stream' object
     provides updates on the surface geometry as required.  However, it is
     `kdrc_layer' which actually activates and exercises the object.  Where
     an overlay surface buffer is required, it has exactly the same dimensions
     as the compositing layer's own composition buffer.
       The present object does not actually paint onto its overlay buffer
     directly.  Instead, it invokes virtual functions of the `compositor'
     object to do this.  This allows applications to simply override the
     overlay painting process, providing graphical effects which are most
     appropriate for exploiting application-specific metadata. */

/******************************************************************************/
/*                              kdrc_codestream                               */
/******************************************************************************/

struct kdrc_codestream {
  public: // Member functions
    kdrc_codestream(bool persistent, int cache_threshold)
      {
        head = NULL;  in_use=false;
        this->persistent = persistent;  this->cache_threshold = cache_threshold;
      }
    ~kdrc_codestream()
      {
        assert(head == NULL);
        if (ifc.exists())
          ifc.destroy();
      }
    void init(kdu_compressed_source *source);
      /* Open the JPEG2000 code-stream from a raw compressed data source. */
    void init(jpx_codestream_source stream);
      /* Open the JPEG2000 code-stream embedded within a JPX/JP2 data source. */
    void init(mj2_video_source *track, int frame_idx, int field_idx);
      /* Open the JPEG2000 code-stream embedded within an MJ2 video source. */
    bool restart(mj2_video_source *track, int frame_idx, int field_idx);
      /* Invokes `kdu_codestream::restart' if possible, rather than creating
         a new codestream engine.  If this is not possible, the function
         returns false. */
    void attach(kdrc_stream *user);
      /* After initialization, you must attach at least one `kdrc_stream'
         user.  Users are automatically attached at the head of the doubly
         linked list of common users.  If you later wish to move a user
         around, call `move_to_head' or `move_to_tail'. */
    void detach(kdrc_stream *user);
      /* Once all attached `kdrc_stream' users have detached, the present
         object will automatically be destroyed. */
    void move_to_head(kdrc_stream *user);
      /* Move this user to the head of the doubly linked list of users which
         share this code-stream. */
    void move_to_tail(kdrc_stream *user);
      /* Move this user to the tail of the doubly linked list of users which
         share this code-stream. */
  public: // Data
    bool persistent;
    int cache_threshold;
    jpx_input_box source_box; // Used when opening codestream inside JP2/JPX/MJ2
    kdu_codestream ifc;
    bool in_use; // If any user has an active `kdu_region_decompressor' engine
    kdu_dims canvas_dims; // Size of image on the high-resolution canvas
  public: // Links
    kdrc_stream *head; // Head of doubly-linked list of `kdrc_stream' objects
                       // which share this resource.
  };
  /* This structure is used to share a single `kdu_codestream' code-stream
     manager's machinery, state and buffering resources amongst potentially
     multiple compositing layers.  This allows the `kdrc_stream' object
     to be associated with exactly one compositing layer.  Note that at
     most one of the users may have an active `kdu_region_decompressor'
     object which has been started, but not yet finished. */

/******************************************************************************/
/*                                kdrc_stream                                 */
/******************************************************************************/

class kdrc_stream {
  public: // Member functions
    kdrc_stream(kdu_region_compositor *owner, bool persistent,
                int cache_threshold, kdu_thread_env *env,
                kdu_thread_queue *env_queue);
    ~kdrc_stream();
    void init(kdu_compressed_source *source, kdrc_stream *sibling);
      /* Use this form of the `init' function to process a raw code-stream.
         May generate an error through `kdu_error' if there is something
         wrong with the data source.  If `sibling' is non-NULL, it must
         point to an existing `kdrc_stream' object which uses the same
         code-stream.  In this case, the existing `kdrc_codestream' object
         will be shared, rather than created afresh. */
    void init(jpx_codestream_source stream, jpx_layer_source layer,
              jpx_source *jpx_src, bool alpha_only, kdrc_stream *sibling);
      /* Use this form of the `init' function to open a codestream within
         a JPX/JP2 compatible data source and associate all relevant
         rendering information.  The function will configure the object to
         produce both colour channels and an alpha channel (if one is
         available).  May generate an error through `kdu_error' if there is
         something wrong with the data source, or if the required colour
         rendering will not be possible.  The interpretation of `sibling'
         is discussed above, with the first form of the `init' function. */
    void init(mj2_video_source *track, int frame_idx, int field_idx,
              kdrc_stream *sibling);
      /* Use this form of the `init' function to open a codestream within
         an MJ2 video track and associate all relevant rendering information.
         The function configures the object to produce both colour channels and
         an alpha channel (if one is available).  May generate an error
         through `kdu_error' if there is something wrong with the data
         source, or if the required colour rendering will not be possible.
         The interpretation of `sibling' is discussed above, with the first
         form of the `init' function. */
    void set_error_level(int error_level);
      /* Call this function at any time after `init'.
         The `error_level' identifies the level of error resilience
         with which the code-stream is to be parsed.  A value of 0 means use
         the `fast' mode; 1 means use the `fussy' mode; 2 means use the
         `resilient' mode; and 3 means use the `resilient_sop' mode.   */
    void set_max_quality_layers(int max_quality_layers)
      { this->max_display_layers = max_quality_layers; }
      /* Note that this function has no effect until a new rendering cycle
         is started.  To ensure that the new rendering cycle starts
         immediately, you should call `invalidate_surface'. */
    int set_mode(int single_component,
                 kdu_component_access_mode access_mode);
      /* Use this to put the object into or take it out of
         single-component rendering mode.  If `single_component' is negative,
         the object will be taken out of single-component mode.  Otherwise,
         it will be put into that mode.  After this call, you must call
         `set_scale' to install a valid scale.
            The `access_mode' argument is disregarded if `single_component' is
         negative, since then we are always interested in output components,
         rather than codestream components.
            Returns the actual index of the component to be decompressed, which
         may be different to `single_component' if that value was too large. */
    void set_thread_env(kdu_thread_env *env, kdu_thread_queue *env_queue);
      /* Called if `kdu_region_compositor::set_thread_env' changes something. */
    void change_frame(int frame_idx);
      /* Use this function to change the codestream which is actually
         rendered, for streams which were initialized with an `mj2_video_source'
         object.  This is equivalent to first destroying the existing stream
         and then recreating and initializing it with the new frame index,
         but the same field and interlacing parameters which were originally
         supplied to `init'.  It is not necessary to call `set_scale'
         or `set_buffer_surface' again, if a valid scale was already
         installed. */
    int get_num_channels() { return mapping.num_channels; }
    int get_num_colours() { return mapping.num_colour_channels; }
    int get_components_in_use(int *indices);
      /* Returns the total number of image components which are in use,
         writing their indices into the supplied array, which must have
         space to accommodate at least 4 entries to be guaranteed that
         overwriting will not occur. */
    bool is_alpha_premultiplied() { return alpha_is_premultiplied; }
    int get_primary_component_idx()
      {
        if (single_component >= 0)
          return single_component;
        else
          return reference_component;
      }
    kdrc_overlay *get_overlay() { return overlay; }
    kdu_dims set_scale(kdu_dims full_source_dims, kdu_dims full_target_dims,
                       kdu_coords source_sampling,
                       kdu_coords source_denominator,
                       bool transpose, bool vflip, bool hflip, float scale,
                       int &invalid_scale_code);
      /* Called after initialilzation and whenever the image is zoomed,
         rotated, etc.  After a call to this function, `set_buffer_surface'
         must also be called.
           `full_target_dims' identifies the location and size of the full
         image region as it is supposed to appear on the compositing grid at
         full size, without any transposition, rotation or scaling.  If the
         area of this region is 0, it will automatically be assigned a
         position of (0,0) and an area equal to that of `full_source_dims';
         if the latter has zero area, it will first be assigned a default
         area in the manner described below.
           `full_source_dims' identifies the location and size of the complete
         source region to be used in generating the target image region.  This
         region is expressed relative to the source image (i.e., current
         code-stream image) upper left hand corner, but at a scale in which
         each code-stream canvas grid point occupies
         `source_sampling.x'/`source_denominator.x' horizontal source grid
         points and `source_sampling.y'/`source_denominator.y' vertical source
         grid points.  These sampling factors are normally both equal to 1,
         unless the code-stream is used by a JPX compositing layer whose
         embedded code-stream registration box specifies non-trivial sampling
         factors.
            If `full_source_dims' has zero area, it will automatically be
         set to the full canvas dimensions of the current source image,
         scaled up by the sampling factors given by `source_sampling' and
         `source_denominator'.  For MJ2 sources, the `full_source_dims'
         argument is always treated as if it were empty on entry.
            Note carefully that none of the `full_source_dims',
         `full_target_dims', `source_sampling' or `source_denominator'
         arguments are sensitive to the effects of image re-orientation and
         dynamic scaling, so their values will be the same each time this
         function is called, regardless of the values taken by the remaining
         arguments.
           In the special case where the `source_sampling' factors are 0,
         the image is rendered at its own native size, corresponding to 
         the full resolution size of the image component associated with
         the first colour channel, scaled only by the value of `scale'.
           `vflip, `hflip' and `transpose' identify additional geometric
         transformations which are to be applied on the compositing grid.
           `scale' represents the amount by which the full target dimensions
         are to be stretched or shrunk down during the rendering process.
           The function attempts to approximate the required scaling
         transformations by a reasonable combination of resolution level
         discarding and integer-valued upsampling.  It returns the actual
         location and dimensions of the region which can be painted on the
         compositing grid, accounting for all geometric changes associated
         with the last four arguments.
           If the requested scale is too small, or the required flipping
         cannot be performed, the function returns an empty region, identifying
         the specific problem via the `invalid_scale_code' argument.  The
         returned code contains the logical OR of one or more of the single-bit
         flags, `KDU_COMPOSITOR_SCALE_TOO_SMALL' and
         `KDU_COMPOSITOR_CANNOT_FLIP'. */
    float find_optimal_scale(float anchor_scale, float min_scale,
                             float max_scale,
                             kdu_dims full_source_dims=kdu_dims(),
                             kdu_dims full_target_dims=kdu_dims(),
                             kdu_coords source_sampling=kdu_coords(),
                             kdu_coords source_denominator=kdu_coords());
      /* Does most of the work of `kdu_region_compositor::find_optimal_scale'.
         If any of the last four arguments is non-zero, the stream belongs
         to a compositing layer so the scale factors induced by the compositing
         process must also be considered. */
    void get_component_scale_factors(kdu_dims full_source_dims,
                                     kdu_dims full_target_dims,
                                     kdu_coords source_sampling,
                                     kdu_coords source_denominator,
                                     double &scale_x, double &scale_y);
      /* Does the work of `kdu_region_compositor::get_component_info'. */
    bool is_scale_valid() { return have_valid_scale; }
      /* Returns false if `set_scale' has not yet been called, or if the
         scale set in the last call to `set_scale' turns out to be unattainable.
         This might not be caught until a later call to `process' in which
         a tile-component is encountered whose number of resolution levels
         is too small to support the small rendering size requested. */
    kdu_dims find_composited_region(bool apply_cropping);
      /* If `apply_cropping' is true, this function just returns the region
         occupied by the stream on the compositing surface.  Otherwise, it
         returns the region which would be occupied by the stream on the
         compositing surface if it had not been cropped down. */
    void get_packet_stats(kdu_dims region, kdu_long &precinct_samples,
                          kdu_long &packet_samples,
                          kdu_long &max_packet_samples);
      /* Used to implement `kdu_region_compositor::get_codestream_packets',
         this function scans the precincts which contribute to the
         reconstruction of `region' (expressed on the composition canvas),
         returning: 1) the sum of their contributing areas via
         `precinct_samples' (this is figured out using
         `kdu_resolution::get_precinct_area'); 2) the sum of their
         contributing packets (from `kdu_resolution::get_precinct_packets')
         scaled by the corresponding precinct areas; and 3) the sum of
         the maximum possible number of contributing packets (from
         `kdu_tile::get_num_layers') scaled by the corresponding precinct
         areas. */
    void set_buffer_surface(kdu_compositor_buf *buffer,
                            kdu_dims compositing_region,
                            bool start_from_scratch);
      /* Called when the buffered region is panned, or when the buffer must
         be resized.  Note that `compositing_region' expresses the region
         represented by the buffer on the compositing grid, which must be
         offset by `compositing_offset' in order to find the region on the
         codestream's rendering grid.  If `start_from_scratch' is true, no
         previously generated data can be re-used, so generation of content
         for the new buffer surface must start from scratch. */
    kdu_dims map_region(kdu_dims region);
      /* Does the work of `kdu_region_compositor::map_region'.  If the
         region does not intersect with this codestream, the function returns
         an empty region.  Otherwise, it returns the coordinates of the
         region, mapped to the codestream's high resolution canvas,
         but adjusted so that the position is expressed relative to the
         upper left hand corner of the image region on the codestream
         canvas. */
    kdu_dims inverse_map_region(kdu_dims region);
      /* Does the work of `kdu_region_compositor::inverse_map_region'.  The
         supplied region is expressed on the codestream's high resolution
         canvas, but adjusted so that the position is expressed relative to
         the upper left hand corner of the image region on the codestream
         canvas.  The returned region is expressed in the composition
         coordinate system. */
    bool process(int suggested_increment, kdu_dims &new_region,
                 int &invalid_scale_code);
      /* Gives this object the opportunity to process some more imagery,
         writing the results to the buffer surface supplied via
         `set_buffer_surface'.  The function may need to configure a new
         region in process first.  Upon return, it will generally update the
         `fraction_left' member to reflect the actual fraction of the imagery
         which has still to be generated.  This allows the caller to
         allocate processing resources to those code-streams which are
         furthest behind in their processing.
            The function returns with `new_region' set to the new rectangular
         region which has just been processed, expressed with respect to
         the compositing grid.  If this region is non-empty, the caller may
         attempt to update the final composited image based upon the new
         information.
            The function may return false only if the number of DWT levels
         in some tile-component is found to be too small to accommodate the
         current scale factor, or any required flipping cannot be performed.
         In either case, the cause of the problem is identified via the
         `invalid_scale_code' argument.  The returned code contains the
         logical OR of one or more of the single-bit flags,
         `KDU_COMPOSITOR_SCALE_TOO_SMALL' and `KDU_COMPOSITOR_CANNOT_FLIP'.
            The function may catch and rethrow an integer exception generated
         by a `kdu_error' handler, if some other processing error occurs. */
    void stop_processing()
      {
        if (processing)
          { decompressor.finish(); processing=codestream->in_use=false; }
      }
    bool can_process_now()
      { // Returns true if processing is not complete, and no other user of
        // the same code-stream resource is currently processing
        return (!is_complete) && (processing || !codestream->in_use);
      }
    void invalidate_surface();
      /* Called after `set_scale' to terminate all processing and invalidate
         all processed data.  Also called from `kdu_region_compositor::refresh'
         to force regeneration of the buffer surface. */
    void adjust_refresh(kdu_dims region, kdrc_refresh *refresh);
      /* Called from within `kdrc_refresh::adjust', this function should not
         be called unless a valid scale has been set.  The function checks to
         see whether all or part of the supplied `region' will be regenerated
         as a result of future calls to this function's `process' function.
         It uses `refresh->add' to add back any subsets of the `region' which
         will not be regenerated in this way. */
  private: // Helper functions
    void configure_subsampling();
      /* This function is called from within `init' and `set_mode' to
         fill out the `active_subsampling' and `min_subsampling' arrays.
         These arrays hold up to 33 entries, corresponding to each possible
         number of discarded resolution levels d from 0 to `max_discard_levels'.
         The value of `max_discard_levels' cannot possibly exceed 32.
            If `single_component' >= 0, both arrays hold the sub-sampling
         factors for this one component.  Otherwise, `active_subsampling'
         holds the subsampling factors for the `reference_component' while
         `min_subsampling' holds the minimum horizontal and vertical
         subsampling factors associated with any of the source components
         identified by the `mapping' object. */
#define KDRC_PRIORITY_SHIFT 8
#define KDRC_PRIORITY_MAX (1<<KDRC_PRIORITY_SHIFT)
    void update_completion_status()
      { /* Updates the `is_complete' and `priority' members based upon the areas
           of the `active_region', `valid_region', `incomplete_region' and
           `region_in_process' members. */
        if ((!active_region) || (active_region == valid_region))
          { is_complete = true; priority = 0; return; }
        is_complete = false;
        kdu_long active = active_region.area();
        kdu_long valid = valid_region.area();
        if (processing)
          valid += (region_in_process.area() - incomplete_region.area());
        if (active == 0)
          priority = 0;
        else
          priority = (int)(((active-valid)<<KDRC_PRIORITY_SHIFT)/active);
      }
  private: // Information passed into constructor or  `init'
    kdu_region_compositor *owner;
    bool persistent;
    bool alpha_only;
    bool alpha_is_premultiplied;
    int cache_threshold;
    mj2_video_source *mj2_track; // NULL if not initialized for video
    int mj2_frame_idx; // 0 if `mj2_track'=NULL
    int mj2_field_idx; // 0 if `mj2_track'=NULL
  private: // Multi-threading state
    kdu_thread_env *env;
    kdu_thread_queue *env_queue;
  private: // Members which are unaffected by zooming, panning or rotation
    kdrc_overlay *overlay; // Created if `init' is supplied a JPX data source
    int max_display_layers; // Max layers to be used in decompression
    kdu_channel_mapping mapping;
    kdu_region_decompressor decompressor;
    
    int single_component; // If non-negative, we are not using `mapping'
    int reference_component; // Index of first colour component in `mapping'
    int active_component; // single_component if >= 0,  else reference_component
    kdu_component_access_mode component_access_mode; // This member will equal
       // `KDU_WANT_OUTPUT_COMPONENTS' except possibly in single component mode
    kdu_coords active_subsampling[33]; // See `configure_subsampling'
    kdu_coords min_subsampling[33]; // See `configure_subsampling'

    int max_discard_levels; // May change as we process more tiles
    bool can_flip; // May change as we process more tiles
  private: // Members which are affected by zooming and rotation
    bool have_valid_scale; // False until first call to `set_scale'
    bool transpose, vflip, hflip; // Identifies current rotation settings
    float scale; // Identifies the current scale
    kdu_dims full_source_dims; // Value supplied in last call to `set_scale'
    kdu_dims full_target_dims; // Value supplied in last call to `set_scale'
    kdu_coords source_sampling; // Value supplied in last call to `set_scale'
    kdu_coords source_denominator; // Value supplied in last call to `set_scale'
    int discard_levels; // Number of levels to discard
    kdu_coords expansion_numerator;   // Numerator and denominator for rational
    kdu_coords expansion_denominator; // reference component expansion factors
    kdu_dims rendering_dims; // Full source image region w.r.t. rendering grid
    kdu_coords compositing_offset; // Rendering origin - compositing origin
  private: // Members which relate to panning of the buffered surface
    kdu_coords buffer_origin; // Expressed on rendering grid
    kdu_dims active_region; // `rendering_dims' intersected with buffer region
    kdu_compositor_buf *buffer; // Points to first pixel at `buffer_origin'
    bool little_endian; // If machine word order is little-endian
  private: // Members used to keep track of incremental processing
    kdu_dims valid_region; // Subset of `active_region' known to have valid data
    bool processing;
    kdu_dims region_in_process; // Full region being processed (rendering grid)
    kdu_dims incomplete_region; // Remaining portion of `region_in_process'
  public: // Links and progress info
    int codestream_idx; // Index of the code-stream within the source
    int layer_idx; // Index of the compositing layer or -1 if opened as raw
    kdrc_layer *layer; // NULL if not attached to a containing layer
    int priority; // Helps compositor decide which stream needs more processing
    bool is_complete; // When we have finished processing the active region
    bool is_active; // Set by `kdu_compositor'
    kdrc_stream *next; // Next in `kdu_compositor::active_streams' list.
    kdrc_codestream *codestream;
    kdrc_stream *next_codestream_user; // Next object using same code-stream
    kdrc_stream *prev_codestream_user; // Previous object using same code-stream
  };
  /* Notes:
       To understand the way this object works, you must first understand that
     there are several coordinate systems at work simultaneously:
     1) There is one global "compositing grid".  Most coordinates passed
        across this object's functions are expressed in terms of the global
        compositing grid.
     2) Each codestream has its own "rendering grid" whose scale matches that
        of the compositing grid, but the origin of the codestream's rendering
        grid is displaced relative to that of the compositing grid by the
        amount stored in the `compositing_offset' member.  Most internal
        coordinates are expressed in terms of the rendering grid.  The
        interpretation of the rendering grid is exactly the same as that
        adopted by the `kdu_region_decompressor' object.
     3) Each codestream has its own high-resolution canvas against which
        all component samples are registered.  The dimensions of the canvas
        are fixed, while those of the compositing and reference grids change
        as a function of zoom and rotation parameters.  The rendering
        grid is related to the codestream canvas as follows.  We first find
        the dimensions of the reference component after any discarded
        resolution levels have been taken into account; these dimensions
        are related to those of the high-res canvas by sub-sampling by the
        relevant component sub-sampling factors and the number of discarded
        DWT levels.  We apply any required rotation/flipping to these
        dimensions and then expand by the factors stored in the `expansion'
        member.
    
       The `rendering_dims' region represents the portion of the code-stream
     image (expressed on the rendering grid) which is to be used for
     compositing.  In most cases, this will be the entire image, but
     some compositing rules use a cropped version of the image.
       The `active_region' represents the portion of the `rendering_dims'
     region which lies on the current buffer surface.  This is also
     expressed with respect to the rendering grid.
       The `valid_region' member identifies the subset of `active_region'
     for which image samples are known to have been generated.
  */

/******************************************************************************/
/*                                 kdrc_layer                                 */
/******************************************************************************/

class kdrc_layer {
  public: // Member functions
    kdrc_layer(kdu_region_compositor *owner);
    ~kdrc_layer();
    void init(jpx_layer_source layer, kdu_dims full_source_dims,
              kdu_dims full_target_dims, bool transpose, bool vflip,
              bool hflip);
      /* Does all the work of `kdu_region_compositor::add_compositing_layer'.
         May generate an error through `kdu_error' if the layer cannot be
         rendered.
            If one or more of the required codestream main headers is not
         yet available, the function does not actually create any
         codestreams.  In this case, the layer will be entered on the
         active list, but the `get_stream' function will return NULL regardless
         of its argument.  Once more information becomes available in a
         dynamic cache, you may be able to complete the initialization
         process by invoking `reinit'.
            The `full_source_dims' argument identifies the portion of this
         layer which is to be used for compositing.  This region is expressed
         on the layer's own reference grid, relative to the upper left hand
         corner of the layer image on that grid.  The layer reference grid
         is often identical to the high resolution canvas of its code-stream
         (or code-streams).  More generally, however, the layer reference
         grid is related to the code-stream canvas grids by the information
         returned via `jpx_layer_source::get_codestream_registration'.
            The `full_target_dims' argument identifies the region of the
         composited image onto which the source region should be composited
         (scaling may be involved).  These coordinates are expressed relative
         to the compositing reference grid which would be used if the
         image were rendered with a scale of 1 and without any rotation,
         flipping or transposition. */
    void init(mj2_video_source *track, int frame_idx, int field_handling,
              kdu_dims full_target_dims, bool transpose, bool vflip,
              bool hflip);
      /* Use this version of the `init' function to add compositing layers
         which are tracks from an MJ2 data source.  The `full_source_dims'
         argument is missing, since Motion JPEG2000 does not support cropping
         of the input frames.  The `frame_idx' and `field_handling'
         arguments have the same interpretation as their namesakes, passed
         into `add_compositing_layer'. */
    bool reinit()
      {
        if (streams[0] == NULL)
          {
            if (jpx_layer.exists())
              init(jpx_layer,full_source_dims,full_target_dims,
                   init_transpose,init_vflip,init_hflip);
            else
              init(mj2_track,mj2_frame_idx,mj2_field_handling,full_target_dims,
                   init_transpose,init_vflip,init_hflip);
          }
        return (streams[0] != NULL);
      }
      /* Call this function if the original call to `init' did not complete
         due to one or more missing codestream main headers. */
    bool change_frame(int frame_idx, bool all_or_nothing);
      /* This function may be called to change only the frame associated with
         an object initialized using an MJ2 video source.  If the required
         codestream(s) can be opened the relevant calls to
         `kdrc_stream::change_frame' are made.  Otherwise, the
         `mj2_pending_frame_change' flag is set, deferring the actual frame
         change until the next call to this function or to `set_scale' or
         `kdu_region_compositor::refresh'.  If `streams[0]' is NULL on
         entry, the object was never completely initialized, so this function
         simply invokes `reinit', after setting up the correct frame index.
         The function returns true if it succeeds in making `frame_idx' the
         new frame index to be rendered within this layer, even if this
         required no actual change in the frame index.  If
         `all_or_nothing' is true, the function does not actually invoke
         the `kdrc_stream::change_frame' function on any codestream until
         it is sure that all required codestreams can be opened. */
    bool change_frame()
      { // Try 1st form of `change_frame' again, if change is still pending
        if (!mj2_pending_frame_change) return false;
        return change_frame(mj2_frame_idx,false);
      }
    void activate(kdu_dims full_source_dims, kdu_dims full_target_dims,
                  bool init_transpose, bool init_vflip, bool init_hflip,
                  int frame_idx, int field_handling);
      /* Call this function when moving an existing layer onto the active
         list.  It is assumed that the caller will be marking the entire
         composition as invalid so that a new set of calls to `set_scale'
         and `set_buffer_surface' will be invoked before any processing
         takes place.  For MJ2 sources, the `full_source_dims' argument is
         ignored.  For all other sources, the `frame_idx' and `field_handling'
         arguments are ignored. */
    void deactivate();
      /* Call this function when moving a layer from the active list to the
         inactive list. */
    kdu_dims set_scale(bool transpose, bool vflip, bool hflip, float scale,
                       int &invalid_scale_code);
      /* Sets the orientation and scale at which the image is to be composed.
         The interpretation of these parameters is identical to that described
         in connection with `kdu_region_compositor::set_scale'.
            The function returns the full region which will be occupied by this
         layer on the compositing grid.  If this region is empty, the
         `scale' parameter may have been too small or the requested flipping
         might not be possible.  These conditions are identified via the
         `invalid_scale_code' argument, which will be set to the logical OR
         of the relevant 1-bit flags, `KDU_COMPOSITOR_SCALE_TOO_SMALL' or
         `KDU_COMPOSITOR_CANNOT_FLIP'.  Note, that the scale may subsequently
         be found to be too small, or flipping may be found to be impossible
         while actually processing the imagery.  This is because it may not be
         until then that a tile-component with insufficient DWT levels is
         encountered.
            If the call to `init' did not succeed, due to the lack of
         availability of codestream headers, the present function simply
         returns an empty region, with `invalid_scale_code' set to 0 */
    float find_optimal_scale(float anchor_scale, float min_scale,
                             float max_scale);
      /* Used to implement `kdu_region_compositor::find_optimal_scale'. */
    void get_component_scale_factors(kdrc_stream *stream,
                                     double &scale_x, double &scale_y);
      /* Used to implement `kdu_region_compositor::get_codestream_info'. */
    void set_buffer_surface(kdu_dims region, kdu_dims visible_region,
                            kdu_compositor_buf *compositing_buffer,
                            bool can_skip_surface_init);
      /* Called when the buffered region is panned, or when the buffer must
         be resized.  The `region' argument identifies the region occupied
         by the composited image buffer on the compositing grid.  If
         the present layer covers only a subset of the composited image, the
         layer's internal buffer surface may be set to a represent a smaller
         region.  The `visible_region' argument identifies the subset of the
         buffer `region' which is not completely covered by opaque
         compositing layers which are closer to the foreground than the
         current layer.  At most this region must actually be rendered for
         correct composition.  The `compositing_buffer' argument should be NULL
         if the present object's own rendering buffer is to be used as the
         composited image.  This happens if there is only one layer and it does
         not use alpha blending.  In this case, `compositing_row_gap' is
         ignored.
            The `can_skip_surface_init' argument indicates whether or
         not a newly allocated layer buffer (or layer buffer region) needs to
         be initialized -- generally this is required only for incremental
         rendering, where as-yet-unrendered portions of the image buffer might
         need to be displayed.  This argument is ignored if `compositing_buffer'
         is non-NULL. */
    void get_buffer_region(kdu_dims &region)
      { region = buffer_region; }
    kdu_compositor_buf *get_layer_buffer() { return buffer; }
      /* Used by `kdu_region_compositor' to get the rendering buffer of a
         single layer which does not require alpha compositing, so this buffer
         can be returned by `kdu_region_compositor::get_composition_buffer'
         instead of a separate compositing buffer.  This saves the work of
         copying everything to a separate buffer. */
    kdu_compositor_buf *take_layer_buffer(bool can_skip_surface_init);
      /* Used by `kdu_region_compositor::push_composition_buffer' if the
         surface buffer happens to be owned by a single compositing layer.
         This function takes ownership of the layer's buffer, forcing it to
         allocate a new one and invalidate its stream surfaces.  The function
         must not be called if there is a global compositing buffer -- an
         internal check is performed to be sure.  If `can_skip_surface_init'
         is true, initialization of the newly allocated surface buffer can
         be skipped.  This is possible if the layer buffer is being taken
         in order to push it onto the composited buffer queue, rather than
         turning it into a global composition buffer. */
    void change_compositing_buffer(kdu_compositor_buf *compositing_buffer)
      {
        assert(this->compositing_buffer != NULL);
        this->compositing_buffer = compositing_buffer;
      }
      /* Used by `kdu_region_compositor::push_composition_buffer' to reflect
         changes in the address of a global composition buffer into the
         present object.  The actual buffer surface region should not have
         changed since the last call to `set_buffer_surface'. */
    kdu_long measure_visible_area(kdu_dims region,
                                  bool assume_visible_through_alpha);
      /* This function measures the area of the subset of `region' within
         which the current compositing layer is visible.  If
         `assume_visible_through_alpha' is true, the layer is assumed to
         be hidden only by foreground layers which have no alpha channel.
         Otherwise, it is assumed to be hidden by any foregound data,
         regardless of alpha blending. */
    void get_visible_packet_stats(kdrc_stream *stream, kdu_dims region,
                                  kdu_long &precinct_samples,
                                  kdu_long &packet_samples,
                                  kdu_long &max_packet_samples);
      /* Used to implement `kdu_region_compositor::get_codestream_packets',
         this function accumulates contributions to its last three arguments
         from calls to `stream->get_packet_stats', based on those rectangular
         subsets of `region' which are considered to be visible.  These
         subsets are found by recursive application of the function, starting
         from the compositing layer which contains `stream'. */
    bool map_region(int &codestream_idx, kdu_dims &region);
      /* Does the work of `kdu_region_compositor::map_region', invoking the
         `kdrc_stream::map_region' function on each relevant codestream. */

    jpx_metanode search_overlay(kdu_coords point, int &codestream_idx,
                                bool &is_opaque);
      /* Does the work of `kdu_region_compositor::search_overlays'.  Returns
         a non-empty interface if a matching metadata node is found.  Sets
         `is_opaque' to true if the current layer is opaque and contains
         `point'.  In this case, there is no point in searching for matches
         in lower layers. */
    void update_composition(kdu_dims region,
                            kdu_uint32 compositing_buffer_background);
      /* Called whenever new data is generated on any layer surface. */
    void configure_overlay(bool enable, int min_display_size,
                           int painting_param);
      /* Does the work of `kdu_compositor::configure_overlays'. */
    void update_overlay(bool start_from_scratch, bool delay_painting);
      /* Called when the overlay information might have changed for some
         reason.  Does nothing unless overlays are in use.
            If `start_from_scratch' is true, the function erases the
         overlay buffer and reads and paints all the overlay information
         from scratch.  If `start_from_scratch' is false, all existing overlay
         information is considered to remain valid and the function checks
         only to see if it needs to add to the overlay.
            If `delay_painting' is true, new metadata will not actually
         be painted to the overlay buffer.  Instead, it will be scheduled
         to be painted progressively when `kdu_compositor::process' is
         next called.  This option is particularly attractive when you do
         not expect the surface to be repainted as a result of imagery
         updates.
            One notable side effect of this function is that the arrival of
         new metadata may cause an overlay buffer to be created and then in
         turn may cause a separate composition buffer to be established in
         `kdu_compositor'.  If this happens, however, the new compositing
         buffer will be identical to the buffer which was last returned
         by `kdu_compositor::get_composition_buffer', so the application
         need not be aware of the change. */
    bool process_overlay(kdu_dims &new_region);
      /* Called from `kdu_compositor::process', this function returns true
         if any changes are made to the overlay surface, returning the
         affected region via `new_region'.  If a recent call to `update_overlay'
         specified the `start_from_scratch' and `delay_painting' options,
         this function will returns with `new_region' set to the region occupied
         by the entire buffer surface, since some previously painted metadata
         may need to be erased in this case. */
    kdrc_stream *get_stream(int which)
      { return ((which<0)||(which>1))?NULL:(streams[which]); }
  private: // Members which are unaffected by global zoom, pan or rotation
    kdu_region_compositor *owner;
    jpx_layer_source jpx_layer; // Copied during the first form of `init'
    mj2_video_source *mj2_track; // Copied during the second form of `init'
    int mj2_frame_idx; // Valid only if `mj2_track' is non-NULL
    int mj2_field_handling; // Copy of value passed to second form of `init'
    bool mj2_pending_frame_change; // See below
    bool init_transpose; // Copy of flag passed into `init'
    bool init_vflip; // Copy of flag passed into `init'
    bool init_hflip; // Copy of flag passed into `init'
    kdu_dims full_source_dims; // Copy of region passed into `init'
    kdu_dims full_target_dims; // Copy of region passed into `init'
    kdrc_stream *streams[2];          // There are up to two streams per layer;
    kdu_coords stream_sampling[2];    // JPX sources, the second one may be used
    kdu_coords stream_denominator[2]; // if alpha is in a separate codestream.
  private: // Members which are affected by zooming and rotation
    bool have_valid_scale; // If `set_scale' has been called successfully
    bool transpose, vflip, hflip; // Identifies current rotation settings
    float scale; // Identifies the current scale
    kdu_dims layer_region; // Region occupied by layer on compositing surface
  private: // Members which relate to panning of the layer's buffered surface
    kdu_compositor_buf *buffer; // Buffer to which codestream data is rendered
    kdu_dims buffer_region; // Visible region occupied by above buffer
    kdu_coords buffer_size; // Actual size of buffer
  private: // Members which are related to overlays
    kdrc_overlay *overlay;
    kdu_compositor_buf *overlay_buffer;
    kdu_coords overlay_buffer_size;
    bool repaint_entire_overlay; // If next call to `process_overlay' should
                     // identify the entire overlay surface as the new region.
  private: // Members which relate to compositing
    kdu_compositor_buf *compositing_buffer; // Null if no compositing required
    kdu_dims compositing_buffer_region; // Region occupied by above buffer
  public: // Links and public info
    bool have_alpha_channel; // True if either codestream will produce alpha
    bool alpha_is_premultiplied; // Meaningful only if `have_alpha_channel' true
    bool have_overlay_info; // True if there is non-trivial overlay info
    int layer_idx; // Index of compositing layer within its data-source
    kdrc_layer *next; // active/inactive layer lists managed by `kdu_compositor'
    kdrc_layer *prev; // active list is doubly-linked
  };
  /* Notes:
       Each layer served by at most two `kdrc_layer' objects.  The
       `main_stream' object manages the colour channels and, if possible, alpha
       information.  The `aux_stream' object manages alpha information, if it
       is not already available from the `main_stream' object.  Each
       code-stream may have its own sub-sampling factors relating its canvas
       coordinates to those of a layer reference grid.  In fact, these
       sampling factors may be rational valued, being equal to the ratio
       between the `main_stream_sampling' and `main_stream_denominator'
       coordinates, or the `aux_stream_sampling' and `aux_stream_denominator'
       coordinates, as appropriate.  In many cases, however, the layer
       reference grid is identical to the high resolution canvas of one or
       both code-streams.  The sub-sampling factors indicate the number of
       layer grid points between each pair of canvas grid points.
          The `full_source_dims' member identifies the portion of this
       compositing layer which is actually to be composited.  The coordinates
       are expressed, independent of any rotation or zooming factors, relative
       to the upper left hand corner of the image, as it appears on the layer
       grid (see above for the relationship between the layer grid and the
       high resolution canvas associated with each codestream).
          The `full_target_dims' member identifies the region of the composited
       image onto which the above source region is to be composited.  This
       region is expressed relative to the compositing grid which would be
       used if there were no scaling and no rotation.
          The `mj2_pending_frame_change' flag is true if the frame(s)
       represented by the current `streams' object(s) are not the correct ones.
       When a frame change is requested, but the required frames are not
       currently available from an MJ2 video source (not yet available in the
       dynamic cache), we leave the streams pointing to the codestreams
       associated with a previous frame so as not to interrupt video playback.
       However, each time `set_scale' or `kdu_region_compositor::refresh' is
       called, an attempt will be made to open the correct frame if it has
       since become available.  This is done using the `change_frame' function.
          The `layer_region' member identifies the full extent of the
       composited image region associated with this layer, expressed
       relative to the compositing grid, at the current scale and accounting
       for any rotation, flipping or transposition.
          The `layer_buffer_region' is always a subset of `layer_region'.  It
       identifies the region on the compositing grid which is currently
       buffered within the `layer_buffer'.
          The `compositing_buffer_region' member is ignored unless
       `compositing_buffer' is non-NULL.  In this case, imagery from this
       layer is to be composited onto the `compositing_buffer' when the
       `update_composition' function is called.  The region occupied by the
       compositing buffer is also expressed relative to the compositing grid,
       but may be larger (no smaller) than the current `layer_buffer_region'.
       This is because some layers may occupy only a portion of the composited
       image. */

/******************************************************************************/
/*                             kdrc_refresh_elt                               */
/******************************************************************************/

struct kdrc_refresh_elt {
    kdu_dims region;
    kdrc_refresh_elt *next;
  };

/******************************************************************************/
/*                               kdrc_refresh                                 */
/******************************************************************************/

class kdrc_refresh {
  /* This object is used by `kdu_compositor' to keep track of regions which
     need to be updated and will not be updated automatically during calls
     to `kdrc_stream::process'. */
  public: // Member functions
    kdrc_refresh() { free_elts = list = NULL; }
    ~kdrc_refresh()
      {
        kdrc_refresh_elt *scan;
        while ((scan=list) != NULL)
          { list = scan->next; delete scan; }
        while ((scan=free_elts) != NULL)
          { free_elts = scan->next; delete scan; }
      }
    void add_region(kdu_dims region);
      /* Add a new region which needs to be refreshed.  You may add as many
         as you like; they may later be eliminated during calls to the
         `adjust' function. */
    bool pop_region(kdu_dims &region)
      { // Called from within `kdu_region_compositor::process'.
        kdrc_refresh_elt *result = list;
        if (result == NULL) return false;
        list = result->next; result->next = free_elts; free_elts = result;
        region = result->region;
        return true;
      }
    void adjust(kdu_dims buffer_region);
      /* Called when the buffer surface changes.  Regions scheduled for
         refresh are intersected with the new buffer region, and removed if
         this leaves them empty. */
    void adjust(kdrc_stream *stream);
      /* After any change in the buffer surface, this function is passed each
         active stream in turn.  It passes through each of the elements on its
         refresh list, as it stood on entry, removing those elements and
         passing them to `stream->adjust_refresh', which in turn calls
         `add_region' to add back any portions of the supplied regions which
         will not otherwise be composited as a result of calls to
         `kdrc_stream::process'.  After passing through all the active streams
         in this way, we can be sure that the remaining regions will not be
         composited as a result of decompression processing; they must be
         explicitly refreshed. */
    void reset();
      /* Same as calling `adjust' with an empty region, but a little more
         efficient and more intuitive. */
  private: // Data
    kdrc_refresh_elt *free_elts;
    kdrc_refresh_elt *list; // List of regions which need refreshing
  };

#endif // REGION_COMPOSITOR_LOCAL_H
