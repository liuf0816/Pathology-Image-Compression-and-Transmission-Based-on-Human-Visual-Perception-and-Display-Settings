/******************************************************************************/
// File: kdu_region_compositor.h [scope = APPS/SUPPORT]
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
   Builds upon the functionality offered by `kdu_region_decompressor' to offer
general compositing services for JPX/JP2 files, MJ2 files and raw codestreams.
This function wraps up even more of the operations required to construct a
highly sophisiticated and efficient browser for JPEG2000 files (local and
remotely served via JPIP), doing most of the real work of the "kdu_show"
application.  The implementation is entirely platform independent.
*******************************************************************************/
#ifndef KDU_REGION_COMPOSITOR_H
#define KDU_REGION_COMPOSITOR_H

#include <string.h>
#include "jpx.h"
#include "mj2.h"

// Objects declared here
class kdu_compositor_buf;
class kdu_region_compositor;

// Objects declared elsewhere
struct kdrc_queue;
class kdrc_overlay;
class kdrc_stream;
class kdrc_layer;
class kdrc_refresh;

/******************************************************************************/
/*                             kdu_compositor_buf                             */
/******************************************************************************/

class kdu_compositor_buf {
  /* [BIND: reference]
     [SYNOPSIS]
       This object is used to allocate and manage image buffers within
       `kdu_region_compositor'.  The purpose of managing buffers through
       a dynamic object is to allow advanced types of buffering in which
       the address of the buffer may change each time it is used.  This
       can happen, for example, if buffers are allocated so as to represent
       resources on a display frame buffer.
  */
  public: // Member functions
    kdu_compositor_buf()
      { internal=locked_for_read=locked_for_write=read_access_allowed=false;
        buf=NULL; row_gap=0; }
    virtual ~kdu_compositor_buf()
      { if (buf != NULL) delete[] buf; }
      /* [SYNOPSIS]
           If you are providing your own buffers, based on a derived version
           of this class, beware that the internal `buf' resource will be
           passed to the C++ `delete' operator if it is non-NULL.  If you
           have allocated it using anything other than `new', be sure to
           override the destructor and set `buf' to NULL before exiting your
           version of the function.
      */
    KDU_AUX_EXPORT void init(kdu_uint32 *buf, int row_gap)
      { this->buf = buf;  this->row_gap = row_gap;  }
     /* [SYNOPSIS]
          This function should be called immediately after construction,
          when no buffer has yet been installed.
          [//]
          As a general rule, any other changes in the buffer pointer should
          be handled by overriding `lock_buf' and/or `set_read_accessibility'.
          [//]
          You can, however, change the internal buffer by calling this function
          at any point when a call to `kdu_region_compositor::process' is
          not in progress.
        [ARG: buf]
          [BIND: donate]
          This argument supplies the physical buffer which is to be used
          by this object.  The argument is marked as "BIND: donate" for the
          purpose of binding to other computer languages.  In C# and Visual Basic,
          the `buf' argument is of type `IntPtr' which might be obtained, for
          example, by a call to `Bitmap.LockBits'.  In Java, donated arrays
          are bound as type `long', which could potentially be used to represent
          an opaque pointer, assuming you have some other object (typically
          with a native interface) which is prepared to return a pointer to
          a valid block of memory.  Of course, incorrect use of this function
          could be dangerous.
        [ARG: row_gap]
          Indicates the gap between consecutive rows, measured in 4-byte pixels.
     */
    bool is_read_access_allowed()
      { return read_access_allowed; }
      /* [SYNOPSIS]
           This function is used internally by `kdu_region_compositor'
           in places where reading from a buffer is optional -- e.g.,
           to copy overlapping data from one region to another when the
           buffer surface is shifted.  If the function returns false,
           read-access is not available and so the optional activity will
           not be performed.
      */
    virtual bool set_read_accessibility(bool read_access_required)
      {
        read_access_allowed = true; // Default implementation always gives
        return true;    // read access.  Generally, you should do the same,
           // even if `read_access_required' is false, except in special
           // circumstances where the application knows that it is better to
           // allocate fast write-only memory (e.g., a display buffer).
      }
      /* [SYNOPSIS]
           This function may be used to alter the type of memory access
           which can be performed on the buffer.  If read access was not
           previously allowed, but is now required, memory resources may
           need to be allocated in a different way.
           [//]
           If the memory manager is unable to retain the contents of a
           buffer which was originally allocated as write-only, but must
           now be available for read-write access, it must return
           false.  This informs the caller that the buffer's contents must
           be marked as invalid so that they will be regenerated by
           subsequent calls to `kdu_region_compositor::process'.
         [RETURNS]
           False if the buffer's contents must be regenerated.  This
           only happens if `read_access_required' is true and the memory
           manager cannot preserve the contents of the originally allocated
           buffer.
      */
    kdu_uint32 *get_buf(int &row_gap, bool read_write)
      { /* [SYNOPSIS]
           This function is used internally by `kdu_region_compositor'
           to lock buffers before using them.  The persistence of an
           image buffer is not assumed between calls to
           `kdu_region_compositor::process'. */
        if ((!locked_for_write) || (read_write && !locked_for_read))
          lock_buf(read_write);
        row_gap = this->row_gap; return buf;
      }
    void get_region(kdu_dims src_region, kdu_int32 tgt_buf[],
                    int tgt_offset=0, int tgt_row_gap=0)
      {
        src_region &= accessible_region;  assert(tgt_offset >= 0);
        if (tgt_row_gap == 0) tgt_row_gap = src_region.size.x;
        assert(this->buf != NULL);
        kdu_uint32 *src_buf = buf + src_region.pos.x+src_region.pos.y*row_gap;
        int m=src_region.size.y, nbytes=src_region.size.x<<2;
        for (; m > 0; m--, src_buf+=row_gap, tgt_buf+=tgt_row_gap)
          memcpy(tgt_buf,src_buf,(size_t) nbytes);
      }
      /* [SYNOPSIS]
           This function is provided mainly for binding to foreign languages
           (particularly Java), where it is not possible to access the array
           returned via `get_buf' and where no possibility exists to donate an
           array which is accessible to that language via the `init' function.
           In that case, the only option left is to copy the contents of the
           buffer into another array which is understood in the foreign
           language.
         [ARG: src_region]
           Region on the source buffer which you want to copy, measured
           relative to the start of the buffer -- i.e., not an absolute
           region.  If part of the specified region lies off the buffer,
           the missing portion will not be transferred.
         [ARG: tgt_buf]
           Buffer into which the results are to be written.  Note that the
           buffer type is defined as `kdu_int32' rather than `kdu_uint32'
           as a convenience for Java bindings, where no 32-bit
           unsigned representation exists.  Java image processing applications
           commonly work with 32-bit signed pixel representations.
         [ARG: tgt_offset]
           Number of pixels from the start of the supplied `tgt_buf' array
           at which you would like to start writing the data.  This must not
           be negative, of course.
         [ARG: tgt_row_gap]
           Separation between rows in the target buffer, measured in pixels.
           This cannot be negative.  If zero, the pixel corresponding to the
           `src_region' will be written contiguously into the buffer.
      */
  protected:
    virtual void lock_buf(bool read_write)
      {
        assert(read_access_allowed || !read_write);
        locked_for_read = true; locked_for_write = read_write;
      }
      /* [SYNOPSIS]
           This function is called by `get_buf' if it fails to find
           the buffer available for use.  You may need to override this
           function to create buffers with special types of behaviour.
         [ARG: read_write]
           If true, the caller needs to be able to read from and write to
           the buffer.  Otherwise, only write access is required.
      */
  private: // Members used only inside `kdu_region_compositor'
    friend class kdu_region_compositor;
    kdu_dims accessible_region; // Region which can be accessed via the
             // `get_region' function -- this is used for access checking.
    bool internal; // True if `kdu_region_compositor::allocate_buffer' returned
             // NULL, meaning that the object was constructed by
             // `kdu_region_compositor::internal_allocate_buffer' and must be
             // deleted using `kdu_region_compositor::internal_delete_buffer'.
  protected: // Data
    bool read_access_allowed;
    bool locked_for_read, locked_for_write;
    kdu_uint32 *buf;
    int row_gap;
  };

/******************************************************************************/
/*                             kdu_region_compositor                          */
/******************************************************************************/

#define KDU_COMPOSITOR_SCALE_TOO_SMALL ((int) 1)
#define KDU_COMPOSITOR_CANNOT_FLIP     ((int) 2)

class kdu_region_compositor {
  /* [BIND: reference]
     [SYNOPSIS]
       An object of this class provides a complete system for managing
       dynamic decompression, rendering and composition of JPEG2000
       compressed imagery.  This very powerful object is the workhorse
       of the "kdu_show" application, abstracting virtually all platform
       independent functionality.
       [//]
       The object can handle rendering of everything from a single component
       of a single raw code-stream, to the composition of multiple
       compositing layers from a JPX file or of multiple video tracks from
       an MJ2 (Motion JPEG2000) file.  Moreover, it supports efficient
       scaling, rotation and dynamic region-based rendering.
       [//]
       The implementation is built on top of `kdu_region_decompressor' which
       supports only a single code-stream.  Unlike that function, however,
       this object also abstracts all the machinery required for dynamic
       panning of a buffered surface over the composited image region.
       [//]
       When working with JPX sources, you may dynamically build up a
       composited image surface by adding and removing compositing layers.
       You may re-use layers which were partially decompressed in a
       previous composition, or you may recycle their resources immediately.
       [//]
       When working with MJ2 sources, you may dynamically build up a
       composited movie frame by adding frames from individual video tracks.
       In fact, there is a conceptual similarity between video tracks in
       MJ2 and compositing layers in JPX, so that the present object
       refers to both types of object as a compositing layers and provides
       interface functions which handle them in a generic way.  The
       interpretation of a compositing layer depends only on whether the
       object was constructed from a `jpx_source' or an `mj2_source' data
       source.
  */
  //----------------------------------------------------------------------------
  public: // Lifecycle member functions
    KDU_AUX_EXPORT
      kdu_region_compositor(kdu_thread_env *env=NULL,
                            kdu_thread_queue *env_queue=NULL);
      /* [SYNOPSIS]
           If you use this constructor, you must issue a subsequent call to
           `create' before using other member functions.  The `env' and
           `env_queue' arguments are used to initialize the multi-threading
           state which may subsequently be changed using the `set_thread_env'
           function -- see that function for an explanation.
      */
    KDU_AUX_EXPORT
      kdu_region_compositor(kdu_compressed_source *source,
                            int persistent_cache_threshold=256000);
      /* [SYNOPSIS]  See `create'.  */
    KDU_AUX_EXPORT
      kdu_region_compositor(jpx_source *source,
                            int persistent_cache_threshold=256000);
      /* [SYNOPSIS]  See `create'.  */
    KDU_AUX_EXPORT
      kdu_region_compositor(mj2_source *source,
                            int persistent_cache_threshold=256000);
      /* [SYNOPSIS] See `create'. */
    virtual ~kdu_region_compositor() { pre_destroy(); }
      /* [SYNOPSIS]
           Note that when implementing a derived class, you should generally
           call `pre_destroy' from that class's destructor, to ensure
           proper sequencing of the destruction operations.
      */
    KDU_AUX_EXPORT virtual void pre_destroy();
      /* [SYNOPSIS]
           Does all the work of the destructor which may rely upon calling
           virtual functions which are overridden in a derived class.  In
           particular, all buffering resources are cleaned up here so that
           `delete_buffer', which may have been implemented in a derived
           class, will not be called after the derived portion has been
           destroyed.
           [//]
           When implementing a derived class, you should call this function
           from the derived class's destructor.
      */
    KDU_AUX_EXPORT void
      create(kdu_compressed_source *source,
             int persistent_cache_threshold=256000);
      /* [SYNOPSIS]
           Configures the object to use a raw code-stream source.  If the
           object is currently in-use, this function generates an error.
         [ARG: persistent_cache_threshold]
           If non-negative, each `kdu_codestream' object created within this
           object will automatically be put in the persistent mode (see
           `kdu_codestream::set_persistent') and assigned a cache threshold
           equal to the indicated value.  You should always select
           this option, unless you do not plan to use the compositor
           interactively.  If you only intend to use one composition
           configuration and you only intend to call `set_scale' and
           `set_buffer_surface' once prior to doing all processing, you can
           get away with non-persistent code-streams, which will save on
           memory.
      */
    KDU_AUX_EXPORT void
      create(jpx_source *source, int persistent_cache_threshold=256000);
      /* [SYNOPSIS]
           Creates the object to use a JPX/JP2 compatible data source.  If the
           object is currently in-use, this function generates an error.
         [ARG: persistent_cache_threshold]
           If non-negative, each `kdu_codestream' object created within this
           object will automatically be put in the persistent mode (see
           `kdu_codestream::set_persistent') and assigned a cache threshold
           equal to the indicated value.  You should always select
           this option, unless you do not plan to use the compositor
           interactively.  If you only intend to use one composition
           configuration and you only intend to call `set_scale' and
           `set_buffer_surface' once prior to doing all processing, you can
           get away with non-persistent code-streams, which will save on
           memory.
      */
    KDU_AUX_EXPORT void
      create(mj2_source *source, int persistent_cache_threshold=256000);
      /* [SYNOPSIS]
           Creates the object to use an MJ2 data source.  If the
           object is currently in-use, this function generates an error.
         [ARG: persistent_cache_threshold]
           If non-negative, each `kdu_codestream' object created within this
           object will automatically be put in the persistent mode (see
           `kdu_codestream::set_persistent') and assigned a cache threshold
           equal to the indicated value.  You should always select
           this option, unless you do not plan to use the compositor
           interactively.  If you only intend to use one composition
           configuration and you only intend to call `set_scale' and
           `set_buffer_surface' once prior to doing all processing within
           any given codestream, you can get away with non-persistent
           code-streams, which will save on memory.
      */
    KDU_AUX_EXPORT void set_error_level(int error_level);
      /* [SYNOPSIS]
           Specifies the degree of error detection/resilience to be associated
           with the Kakadu code-stream management machinery.  The same error
           level will be associated with all code-streams opened within the
           supplied `jpx_source' object, including those code-streams which
           are currently open and those which may be opened in the future.
           [//]
           The following values are defined:
           [>>] 0 -- minimal error checking (see `kdu_codestream::set_fast');
           [>>] 1 -- fussy mode, without error recovery (see
                `kdu_codestream::set_fussy');
           [>>] 2 -- resilient mode, without the SOP assumption (see
                `kdu_codestream::set_resilient');
           [>>] 3 -- resilient mode, with the SOP assumption (see
                `kdu_codestream::set_resilient').
           [//]
           If you never call this function, all code-streams will be opened
           in the "fast" mode (`error_level'=0).
      */
    void set_surface_initialization_mode(bool pre_initialize)
      {
        initialize_surfaces_on_next_refresh =
          (pre_initialize && can_skip_surface_initialization);
        can_skip_surface_initialization = !pre_initialize;
      }
      /* [SYNOPSIS]
           This function was introduced for the first time in v5.0 so that
           video rendering applications can proceed with the maximum possible
           efficiency.  However, you may find it useful in other applications.
           [//]
           By default, whenever a new frame buffer is allocated, its
           contents are initialized to an appropriate default state.  This
           means that the buffer retrieved via `get_composition_buffer' can
           be painted to a display, even before the imagery has been fully
           decompressed.  Although initializing the buffer does not take long,
           this overhead can noticeably slow down video rendering applications.
           Moreover, when motion video is being displayed, the buffer surface
           is always fully rendered prior to being displayed, so as to avoid
           tearing.  For these applications, or any application in which you
           know that the `process' function will be called until rendering is
           complete before displaying anything, it is a good idea to call this
           function with `pre_initialize' set to false.
           [//]
           You can change the pre-initialization mode at any time.  However,
           changes will not be reflected until new buffers are allocated or
           the `refresh' function is called.  If you want to switch from
           a real-time video rendering mode to incremental rendering of a
           single frame or image, it is a good idea to first call this
           function with `pre_initialize' to true and then call `refresh',
           so as to be sure that surface buffers which are currently only
           partially rendered can be meaningfully displayed.  The call to
           `refresh' can be safely omitted, of course, if
           `is_processing_complete' already returns true.
           [//]
           If this all seems to hard for you, and you don't care about the
           last iota of speed, you don't need to bother with this function
           at all.
      */
  //----------------------------------------------------------------------------
  public: // Functions used to build and tear down compositions
    KDU_AUX_EXPORT bool
      add_compositing_layer(int layer_idx, kdu_dims full_source_dims,
                            kdu_dims full_target_dims, bool transpose=false,
                            bool vflip=false, bool hflip=false,
                            int frame_idx=0, int field_handling=2);
      /* [SYNOPSIS]
           Use this function to build up a composited image from one or more
           compositing layers.  You must add at least one compositing
           layer, either directly or via `set_frame', or else call
           `set_single_component', before using this object to render imagery.
           You can add layers to or remove them from the composition at any
           time, although you should call `get_composition_buffer' again after
           you have finished modifying the layer configuration to get a buffer
           which will legitimately represent the composed imagery.
           [//]
           Regardless of whether the referenced layer has already be added,
           it will be placed at the end of the compositing list.  Where
           multiple layers are to be composited together, they are rendered
           onto the composition surface in the order given by this list.
           [//]
           For MJ2 (Motion JPEG2000) data sources, the `layer_idx' argument
           is interpreted as the MJ2 track index, minus 1, so that 0
           represents the first track.  In this case, the `frame_idx'
           and `field_idx' arguments may be used to control the particular
           frame to be rendered and the way in which fields should be
           treated for interlaced material.
           [//]
           Note that this function may cause an error to be generated through
           `kdu_error' if composition is not possible (e.g. due to colour
           conversion problems), or if an error occurs while examining the
           embedded code-streams.  For many applications, the implementation
           of `kdu_error' will throw an exception.
         [RETURNS]
           True if the requested compositing layer could be accessed within
           the source with which the present object was opened.  A false return
           occurs only when a JPX/JP2/MJ2 data-source is served by a dynamic
           cache which does not currently contain sufficient information to
           open the compositing layer, along with all of its codestream
           main headers.  If the requested layer simply does not exist, an
           error will be generated through `kdu_error' rather than returning
           false.  For MJ2 sources, the function also generates an error
           if the requested layer (track) is found not to be a video track.
         [ARG: layer_idx]
           For JPX data sources, this is the index (starting from 0) of the
           compositing layer within that source.  For raw code-streams, this
           argument must be 0.  For MJ2 data sources, this is the track
           number minus 1, so that 0 corresponds to the first track.
         [ARG: full_source_dims]
           Ignored if the data source is not a JP2/JPX source -- i.e., if
           it is a raw codestream or a MJ2 (Motion JPEG2000) source.  See below
           for what happens in such cases.
           [//]
           For JP2/JPX sources, this argument identifies the portion of
           this layer which is to be used for compositing.  This region is
           expressed on the layer's own reference
           grid, relative to the upper left hand corner of the layer image on
           that grid.  The layer reference grid is often identical to the high
           resolution canvas of its code-stream (or code-streams).  More
           generally, however, the layer reference grid is related to the
           code-stream canvas grids by the information returned via
           `jpx_layer_source::get_codestream_registration'.
           [//]
           If this region is empty (`full_source_dims.is_empty' returns true)
           or if the data source is anything other than JP2/JPX,
           the entire source layer will be used for compositing.
           [//]
           If the data source is a raw code-stream, the `full_source_dims' and
           `full_target_dims' arguments are ignored and the composited
           image dimensions at full size (scale=1) are taken to be those
           of the first image component, rather than those of the
           high resolution canvas.
         [ARG: full_target_dims]
           Ignored if the data source is a raw code-stream (see
           `full_source_dims' for an explanation of what happens in this case).
           [//]
           For JP2/JPX or MJ2 sources, this argument identifies the
           region of the composited image onto which the source
           region should be composited.  Scaling may be required, which
           could cause some image degradation.  The coordinates are
           expressed relative to the composited image which would be produced
           if it were rendered without any scaling (zooming), rotation,
           flipping or transposition of the composed image.  In particular,
           the actual dimensions of the composed image may be altered by the
           `set_scale' function.
         [ARG: transpose]
           This argument may be used together with `vflip' and `hflip' to
           adjust the geometry of the layer's source material prior to scaling
           and offsetting it to the `full_target_dims' region.  The geometric
           transformations performed here are equivalent to those described
           for the `kdu_codestream::change_appearance' function.  Note that
           these geometric transformations are in addition to any which might
           be specified by `set_scale'.  They provide useful means of
           incorporating track-specific geometric tranformations which may
           be specified in an MJ2 data source, recovered via
           `mj2_video_source::get_cardinal_geometry', but may be used in
           conjunction with any data source.
         [ARG: vflip]
           See `transpose'.
         [ARG: hflip]
           See `transpose'.
         [ARG: frame_idx]
           For MJ2 data sources, this argument specifies the frame number
           (starting from 0) within the track (layer).  Note, however, that
           only one compositing layer is created for any given track, so
           adding a new layer with the same `layer_idx' as an existing one,
           but a different `frame_idx' will effectively cause the previous
           layer to be removed first.  For non-MJ2 data sources, this argument
           should be 0.  If the frame does not exist, the function generates
           an error.
         [ARG: field_handling]
           For MJ2 data sources, this argument specifies the way in which
           fields are to be handled where the frames are interlaced.  The
           argument is ignored if the frames are progressive or the data source
           is not MJ2.  The following values are defined:
           [>>] 0 -- render the frame from field 0 only
           [>>] 1 -- render the frame from field 1 only
           [>>] 2 -- render the frame from field 0 and field 1 (interlaced)
           [>>] 3 -- render the frame from field 1 of the current frame and
                     field 0 of the next frame (interlaced), or from field 1
                     alone if this is already the last frame.
      */
    KDU_AUX_EXPORT bool
      change_compositing_layer_frame(int layer_idx, int frame_idx);
      /* [SYNOPSIS]
           This function provides an efficient mechanism for changing the
           particular frame which is currently being used to render the
           indicated compositing layer.  The function is useful only when
           the compositing layer happens to represent a Motion JPEG2000 video
           track, since JPX compositing layers do not have multiple frames.
           The advantage of using this function is that no further calls to
           `set_scale' or `set_buffer_surface' are required to prepare the
           rendering process.  Furthermore, the internal machinery will
           attempt to reuse codestream structures in an efficient way,
           rather than creating them from scratch.
           [//]
           If `layer_idx' does not correspond to a currently active
           compositing layer, this function will generate an error through
           `kdu_error'.
         [RETURNS]
           False if the requested frame does not exist or cannot yet be
           opened because the source is served by a dynamic cache, which
           does not yet have sufficient information.  In this case, the
           object will continue to produce content based on the original
           frame, but subsequent calls to `refresh' and `set_scale' will
           attempt to make the frame change which was requested here, if more
           information has since become available in the cache -- it is not
           necessary, therefore, to call this function again if a false
           return was produced while using a valid frame index.
         [ARG: layer_idx]
           Same interpretation as in `add_compositing_layer'.
         [ARG: frame_idx]
           Same interpretation as in `add_compositing_layer'.
      */
    KDU_AUX_EXPORT void
      remove_compositing_layer(int layer_idx, bool permanent);
      /* [SYNOPSIS]
           Use this function to remove layers from the current composition
           and/or to permanently destroy the resources allocated to layers.
           You can simultaneously remove all layers by setting `layer_idx' to
           a negative value.
           [//]
           Unused layers can also be implicitly removed in the process of
           adding new layers, by using the `set_frame' function.
         [ARG: permanent]
           If true, the layer is permanently destroyed, cleaning up its
           internal resources (they may be substantial).
           [//]
           Otherwise, the layer is moved onto an inactive list, from which
           it may be retrieved later with the `add_compositing_layer'
           function (or indirectly using `set_frame').  The layer may later
           be permanently removed from the inactive list by calling the
           present function at any time, with `permanent' set to true.  In
           addition, layers which have been inactive for some time may be
           destroyed by calling `cull_inactive_layers'.
         [ARG: layer_idx]
           If negative, all layers are simultaneously removed.  Otherwise,
           only the layer (if any) which matches this index is affected.
      */
    KDU_AUX_EXPORT int
      set_single_component(int stream_idx, int component_idx,
          kdu_component_access_mode access_mode=KDU_WANT_CODESTREAM_COMPONENTS);
      /* [SYNOPSIS]
           Calling this function will automatically remove (not permanently)
           all layers previously added using `add_compositing_layer', after
           which it prepares the object to render the compositing surface
           using just the single component or the indicated code-stream.
           [//]
           The optional `access_mode' argument determines whether
           the requested image component should be a final output component
           (after applying any colour transforms or Part 2 multi-component
           transforms defined at the codestream level) or a raw codestream
           component (obtained after decoding and inverse spatial wavelet
           transformation).  In the former case (`KDU_WANT_OUTPUT_COMPONENTS'),
           additional components may need to be decompressed internally, so
           that the requested component can be reconstructed.  Prior to Kakadu
           v5.0, this function could only return codestream components, so the
           default value for the `access_mode' argument is set to
           `KDU_WANT_CODESTREAM_COMPONENTS' for backward compatibility.
           [//]
           If a compositing layer using this code-stream has already been
           opened (and not permanently destroyed), the present function will
           re-use the open code-stream resource, rather than opening a new
           one.  Similarly, if the code-stream has been used to display a
           different single component previously, the code-stream resource
           can generally be re-used.
           [//]
           Note that this function may cause an error to be generated through
           `kdu_error' if some problem occurs while opening the code-stream.
           [//]
           If the data source is a raw codestream, `stream_idx' must be 0.  If
           the data source is JP2/JPX, `stream_idx' is the positional index
           of the codestream (starting from 0) within the source.  If the
           data source is MJ2, `stream_idx' is the unique codestream index
           whose interpretation is described in connection with
           `mj2_video_source::get_stream_idx'.
         [RETURNS]
           -1 if the code-stream cannot yet be opened.  This happens only
           when a JPX/JP2/MJ2 data source is served by a dynamic cache which
           does not yet have enough information to actually open the
           code-stream, or to verify whether or not it exists.  If the
           requested code-stream is known not to exist, the function
           generates an appropriate error through `kdu_error', rather than
           returning.
           [//]
           If successful, the function returns the index of the actual
           image component opened, which may be different to the
           supplied `component_idx' if that value was outside the
           range of available components for the code-stream.
      */
    KDU_AUX_EXPORT void cull_inactive_layers(int max_inactive);
      /* [SYNOPSIS]
           This function may be used to remove some of the least recently
           used layers from the internal inactive list.  Compositing layers
           are added to this list when `remove_compositing_layer' is called
           with a `permanent' argument of false.  Old layers are also moved
           to the inactive list when `set_frame' is used to advance to a
           new frame in an animation or movie.
         [ARG: max_inactive]
           Number of most recently active compositing layers to leave on the
           inactive list.
      */
    KDU_AUX_EXPORT void set_frame(jpx_frame_expander *expander);
      /* [SYNOPSIS]
           Use this function to add an entire set of compositing layers,
           obtained by invoking `jpx_frame_expander::get_member' on the
           supplied `expander' object.  The `expander' object's
           `jpx_frame_expander::construct' function should have already
           been called (generally with its `follow_persistence' argument
           set to true) to find the compositing layers and instructions
           associated with the composited frame.  Moreover, the call to
           `jpx_frame_expander::construct' must have returned true, meaning
           that all required compositing layers are available.  Note, however,
           that some of the codestream main headers might not yet be available.
           This is acceptable when opening a frame.
           [//]
           The function essentially just adds all of the relevant compositing
           layers in turn, as though you had explicitly called
           `add_compositing_layer', moving all layers which are no longer
           required to the inactive list.  However, it also uses the
           JPX data source's `jpx_composition' object to recover the
           dimensions of the compositing surface, placing the frames onto
           this surface.
           [//]
           Since the function does not permanently destroy any of the
           compositing layers which may have previously been in use, you
           may wish to invoke `cull_inactive_layers' after each
           successful return from the present function.
      */
  //----------------------------------------------------------------------------
  public: // Functions for adjusting/querying the composed image appearance
    KDU_AUX_EXPORT void
      set_scale(bool transpose, bool vflip, bool hflip, float scale);
      /* [SYNOPSIS]
           Sets any rotation, flipping or transposition to be performed,
           and the scale at which the image is to be composed.
           The interpretation of the `transpose', `hflip' and `vflip' arguments
           is identical to that described in conjunction with the
           `kdu_codestream::change_appearance' function.
           [//]
           After calling this function, and before calling `process', you
           must call `set_buffer_surface' to identify the portion of the
           image you want rendered.  You must not assume that the image
           buffer returned by a previous call to `get_composition_buffer'
           is still valid, so you should generally call that function again
           also to get a handle to the buffer which will hold the composited
           image.
           [//]
           Note that the current function does not actually verify that the
           scale parameters are compatible with the composition being
           constructed.  It merely sets some internal state information
           which will be used to adjust the internal configuration on the
           next call to `get_composition_buffer', `get_total_composition_dims'
           or `process'.  Any of these functions may return a NULL response
           (each function has an obvious NULL return value) if the scale
           parameters were found to be invalid during any step which they
           executed internally (this could happen half way through processing
           a region, if a tile-component with insufficient DWT levels is
           encountered, for example).
         [ARG: scale]
           A value larger than 1 implies that the image should be composed
           at a larger (zoomed in) size than the nominal size associated with
           the `add_compositing_layer' function.  Conversely, a value smaller
           than 1 implies a zoomed out representation.
           [//]
           Note that the actual scale factors which must be applied to
           individual codestream image components may be quite different,
           since their composition rules may require additional scaling
           factors.
           [//]
           Note also that the optimal scale at which to render an individual
           codestream is a positive integer or a reciprocal power of 2.
           Since you may not know how the global scale parameter supplied
           here translates into the scale factor used for a particular
           codestream of interest, this object provides a separate function,
           `find_optimal_scale', which may be used to guide the selection
           of optimal scale factors, based on the codestream content which
           is being used to render the information in a particular region
           of interest.
      */
    KDU_AUX_EXPORT float
      find_optimal_scale(kdu_dims region, float scale_anchor,
                         float min_scale, float max_scale,
                         int *compositing_layer_idx=NULL,
                         int *codestream_idx=NULL,
                         int *component_idx=NULL);
      /* [SYNOPSIS]
           The principle purpose of this function is to find good scale
           factors to supply to `set_scale' so as to minimize the risk of
           unpleasant rendering artifacts caused by imposing non-natural
           scaling factors on decompressed codestream data.
           [//]
           Optimal scaling factors are those which scale the principle
           codestream image component within a region by either a positive
           integer or a reciprocal power of 2.  The principle codestream
           image component is generally the first component involved in colour
           intensity reproduction for the compositing layer which contributes
           the largest visible area to the supplied `region'.  The following
           special cases apply:
           [>>] If a valid scale has not yet been set (i.e. if
                `get_total_composition_dims' returns false), the `region'
                argument cannot be used, since the region must be assessed
                relative to an existing scale/geometry configuration
                created by the last call to `set_scale'.  In this case,
                the principle codestream is derived from the top-most
                composition layer, regardless of its size.
           [>>] If no compositing layer or raw codestream contributes to the
                supplied `region', even though a valid scale has already
                been installed, we say that there are no optimal scaling
                factors.  Then, in accordance with the rules outlined below,
                the function returns the nearest value to `scale_anchor'
                which lies in the range from `min_scale' to `max_scale'.
           [//]
           If no optimal scaling factors lie in the range from `min_scale'
           to `max_scale', the function returns the nearest value to
           `scale_anchor' which does lie in this range.  If multiple optimal
           scaling factors lie in the range, the function returns the one
           which is closest to `scale_anchor'.  If `scale_anchor', `min_scale'
           and `max_scale' are all identical, the function is guaranteed to
           return the value of `scale_anchor', but this can nevertheless be
           useful if the information returned via the last three arguments
           is of interest.
         [ARG: region]
           The function looks for the compositing layer (or a raw codestream)
           which has the largest visible area in this region, on which to base
           its scaling recommendations.  If the supplied `region' is empty,
           or if a valid scale has not previously been installed (i.e., if
           `get_total_composition_dims' returns false), the top-most
           compositing layer (or raw codestream) is used to compute the
           sizing information.
         [ARG: compositing_layer_idx]
           If non-NULL, this argument is used to return the index of the
           compositing layer which was found to occupy the largest visible
           area within `region'.  If `region' is empty or a valid scale has
           not previously been installed (i.e., if `get_total_composition_dims'
           returns false), the value returned via this argument is the index
           of the top-most active compositing layer, regardless of its area.
           If the function was unable to find any relevant compositing layer,
           this value will be set to -1.  Remember that for MJ2 data sources,
           the compositing layer index is actually the track index minus 1.
           For more on this, see `add_compositing_layer'.
         [ARG: codestream_idx]
           If non-NULL, this argument is used to return the index of the
           codestream which holds the principle image component within the
           compositing layer returned via `compositing_layer_idx'.  If
           rendering is based on a raw codestream, this codestream's
           index (always 0) is returned.  For JP2/JPX data sources, the index
           is the ordinal position of the relevant codestream within the file
           (0 for the first one).  For MJ2 data sources, the value returned here
           is the absolute codestream index described in conjunction with
           `mj2_video_source::get_stream_idx'.  If the function was unable to
           find any relevant compositing layer or raw codestream, the value
           will be set to -1.
         [ARG: component_idx]
           If non-NULL, this argument is used to return the index of the
           particular image component within the selected codestream, on which
           all scaling recommendations and other information is based.  As
           mentioned above, this "principle" image component is generally the
           one responsible for producing the first input channel to the colour
           rendering process.  If the function was unable to find any relevant
           compositing layer or raw codestream, this value will be set to -1.
           [//]
           The component index returned here represents an output image
           component (see `kdu_codestream::apply_input_restrictions') except
           in the case where we are in single component mode and the
           `set_single_component' function was called with an
           `access_mode' argument of `KDU_WANT_CODESTREAM_COMPONENTS'.
      */
    KDU_AUX_EXPORT void set_buffer_surface(kdu_dims region,
                                           kdu_int32 background=-1);
      /* [SYNOPSIS]
           Use this function to change the location, size and/or background
           colour of the buffered region within the complete composited image.
           [//]
           Note that the actual buffer region might be different to that
           specified, if the one supplied does not lie fully within the
           composited image region.  Call `get_composition_buffer' to find out
           the actual buffer region.
           [//]
           Note also that any buffer previously recovered using
           `get_composition_buffer' will no longer be valid.  You must call
           that function again to obtain a valid buffer.
         [ARG: background]
           You can use this optional argument to control the colour (and alpha)
           of the background onto which imagery is painted.  The background
           colour is of interest when the imagery is alpha blended.  The
           background alpha is of interest if you would like to be able to
           blend the the final composited image onto yet another buffer
           in your application.
           [//]
           To be specific, if opaque imagery is
           composited over some region of the background, the alpha value in
           that region will be changed to 255.  If, however, partially
           transparent imagery is blended over some region, having an alpha
           value of Ai, and the background (supplied here) has an alpha value
           of Ab, the alpha value in that location will be changed to a
           suitably rounded version of
           [>>]   255 - [(255-Ab) * (255-Ai) / 255]
           [//]
           This means, for example, that fully transparent imagery, composed on
           top of a background will not alter the alpha value of the background.
           If also means that the alpha value will be 255 (opaque) if either
           the background or the composed imagery are opaque.
           [//]
           Note that the most significant byte of the `background' word holds
           the alpha value; the second most significant byte holds the red
           value; this is followed by the green value and then blue in the
           least significant byte of the 32-bit word.
      */
    int check_invalid_scale_code() { return invalid_scale_code; }
      /* [SYNOPSIS]
           This function allows you to figure out what went wrong if any
           of the functions `process', `get_total_composition_dims' or
           `get_composition_buffer' returns a false or NULL result.  As
           noted in connection with those functions, this can happen if
           the scale requested by the most recent call to `set_scale' is
           too small, given the limited number of DWT levels which can
           be discarded during decompression of some relevant tile-component.
           Alternatively, it may happen because the requested rendering
           process calls for geometric flipping or one or more image
           surfaces, and this operation cannot be performed dynamically
           during decompression, due to the use of adverse packet wavelet
           decomposition structures.  These conditions are identified by
           the presence of one or both of the following error flags in
           the returned word:
           [>>] `KDU_COMPOSITOR_SCALE_TOO_SMALL' -- remedy: increase the scale.
           [>>] `KDU_COMPOSITOR_CANNOT_FLIP' -- remedy: try alternate rendering
                geometry.
           [//]
           If the return value is 0, none of these problems has been detected.
           It is worth noting, however, that the `invalid_scale_code' value
           returned here is reset to 0 each time `set_scale' is called, after
           which an attempt must be made to call one of
           `process', `get_total_composition_dims' or `get_composition_buffer'
           before an error code can be detected.
           [//]
           It is also worth noting that rendering might proceed fine within
           some region, without generating an invalid scale code, but this
           may change later when we come to rendering a different region.
           This is because the adverse conditions responsible for the code
           may vary from tile-component to tile-component within JPEG2000
           codestreams.
       */
    KDU_AUX_EXPORT bool get_total_composition_dims(kdu_dims &dims);
      /* [SYNOPSIS]
           Retrieves the location and size of the total composited image, based
           on the current set of compositing layers (or the raw image
           component) together with the information supplied to `set_scale'.
           This is the region within which the composition buffer's
           region may roam (for interactive panning).
           [//]
           Note that the origin of the composition buffer might not lie at
           (0,0). In fact, it almost certainly will not lie at (0,0) if the
           image has been re-oriented.
         [RETURNS]
           False if composition cannot be performed at the current scale or
           with the current flipping requirements.  These conditions may not
           be uncovered until some processing has been performed, since it may
           not be until that point that some tile-component of some code-stream
           is found to have insufficient DWT levels.  If this happens, you
           should call `check_invalid_scale_code' and make appropriate
           changes in the scale or rendering geometry before proceeding.
      */
    KDU_AUX_EXPORT kdu_compositor_buf *
      get_composition_buffer(kdu_dims &region);
      /* [SYNOPSIS]
           Returns a pointer to the buffer which stores composited imagery,
           along with information about the region occupied by the buffer
           on the composited image.  To recover the origin of the composited
           image, use `get_total_composition_dims'.
           [//]
           You should call this function again, whenever you do anything
           which may affect the geometry of the composited image.
           Specifically, you should call this function again
           after any call to `set_buffer_surface', `set_scale',
           `add_compositing_layer', `remove_compositing_layer' or
           `set_single_component'.
           [//]
           Note that the buffer returned by this function may or may not
           be the current working buffer which is affected by calls to
           `process'.  In particular, if `push_composition_buffer' has been
           used to create a non-empty queue of fully composed composition
           buffers, the present function returns a pointer to the head of
           the queue.  If the queue is empty, the returned pointer refers
           to the current working buffer.  Whether or not the composition
           buffer queue is empty may itself be determined by calling
           `inspect_buffer_queue' with an `elt' argument of 0.
         [RETURNS]
           NULL if composition cannot be performed at the current scale, or
           with the current flipping requirements -- see the description of
           `get_total_composition_dims' for more on this, particularly with
           reference to the need to call `check_invalid_scale_code'.
           [//]
           Otherwise, the returned buffer uses 4 bytes to represent each pixel.
           We define the buffer as an array of 32-bit words so as to force
           alignment on 4-byte boundaries.  The most significant byte in each
           word represents alpha, followed by red, green and blue which is in
           the least significant byte of the word.
      */
    KDU_AUX_EXPORT bool
      push_composition_buffer(kdu_long stamp, int id);
      /* [SYNOPSIS]
           This function is used together with `get_composition_buffer'
           and `pop_composition_buffer' to manage an internal queue of
           processed composition buffers.  This service may be used to
           implement a jitter-absorbtion buffer for video applications, in
           which case the `stamp' argument might hold a timestamp, identifying
           the point at which the generated frame should actually be displayed.
           [//]
           Only completely processed composition buffers may be pushed onto
           the tail of the queue.  That means that `is_processing_complete'
           should return true before this function will succeed.  Otherwise,
           the present function will simply return false and do nothing.
           [//]
           If the function succeeds (returns true), the processed surface
           buffer is appended to the tail of the internal queue and
           a new buffer is allocated for subsequent processing operations.
           The new buffer is marked as empty so that the next call to
           `is_processing_complete' will return false, and future calls to
           `process' are required to paint it.
           [//]
           The internal management of composition buffers may be visualized
           as follows:
           [>>] Q_1 (head)  Q_2 ... Q_N (tail)  W
           [//]
           Here, the processed composition buffer queue consists of N
           elements and W is the current working buffer, which may or
           may not be completely processed.  When `push_composition_buffer'
           succeeds, W becomes the new tail of the queue, and a new W is
           allocated.
           [//]
           The `pop_composition_buffer' function is used to remove the head
           of the queue, while `inspect_composition_buffer' is used to
           examine the existence and the `stamp' and `id' values of any of the
           N elements currently on the queue.
           [//]
           When the queue is non-empty, `get_composition_buffer' actually
           returns a pointer to the head of the queue (without removing it).
           When the queue is empty, however, the `get_composition_buffer'
           function just returns a pointer to the working buffer, W.
           [//]
           If the location or size of the current buffer region is changed
           by a call to `set_buffer_surface', the queue will automatically
           be deleted, leaving only a working buffer, W, with the appropriate
           dimensions.  The same thing happens if `set_scale' is called, or if
           any adjustments are made to the composition (e.g., through
           `add_compositing_layer' or `set_frame') which affect the scale or
           buffer surface region which is being rendered.  This ensures that
           all elements of the queue always represent an identically sized
           and positioned surface region.  However, changes in the elements
           used to build the compositing buffer which do not affect the
           current scale or buffer region will leave the composition queue
           intact.
         [RETURNS]
           True if a valid, fully composed buffer is available (i.e., if
           `is_processing_complete' returns true).  Otherwise, the function
           returns false and does nothing.
         [ARG: stamp]
           Arbitrary value stored along with the queued buffer, which can be
           recovered using `inspect_composition_queue'.  When implementing a
           jitter-absorbtion buffer, this will typically be a timestamp.
         [ARG: id]
           Arbitrary identifier stored along with the queued buffer, which
           can be recovered using `inspect_composition_queue'.  This might
           be a frame index, or a reference to additional information stored
           by the application.
      */
    KDU_AUX_EXPORT bool
      pop_composition_buffer();
      /* [SYNOPSIS]
           Removes the head of the composition buffer queue (see
           `push_composition_buffer'), returning false if the queue was
           already empty.  If the function returns true, any pointer
           returned via a previous call to `get_composition_buffer' should
           be treated as invalid.
      */
    KDU_AUX_EXPORT bool
      inspect_composition_queue(int elt, kdu_long *stamp=NULL, int *id=NULL);
      /* [SYNOPSIS]
           Use this function to inspect specific elements on the processed
           composition buffer queue.  For a discussion of this queue, see
           the discussion accompanying the `push_composition_buffer' function.
         [RETURNS]
           False if the element identified by `elt' does not exist.  If
           `elt'=0 and the function returns false, the composition buffer
           queue is empty.
         [ARG: elt]
           Identifies the particular queue element to be inspected.  A value
           of 0 corresponds to the head of the queue.  A value of N-1
           corresponds to the tail of the queue, where the queue holds N
           elements.
         [ARG: stamp]
           If non-NULL and the function returns true, this argument is used
           to return the stamp value which was supplied in the corresponding
           call to `push_composition_buffer'.
         [ARG: id]
           If non-NULL and the function returns true, this argument is used
           to return the identifier which was supplied in the corresponding
           call to `push_composition_buffer'.
      */
    KDU_AUX_EXPORT void flush_composition_queue();
      /* [SYNOPSIS]
           Removes all elements from the jitter-absorbtion queue managed
           by `push_composition_buffer' and `pop_composition_buffer'.
      */
    KDU_AUX_EXPORT void set_max_quality_layers(int quality_layers);
      /* [SYNOPSIS]
           Sets the maximum number of quality layers to be decompressed
           within any given code-stream.  By default, all quality layers
           will be decompressed.  Note that this function does not actually
           affect the imagery which has already been decompressed.  Nor
           does it cause that imagery to be decompressed again from scratch
           in future calls to `process'.  To ensure that this happens, you
           should call `refresh' before further calls to `process'.
      */
    KDU_AUX_EXPORT int get_max_available_quality_layers();
      /* [SYNOPSIS]
           Returns the maximum number of quality layers which are available
           (even if not yet loaded into a dynamic cache) from any code-stream
           currently in use.  This value is useful when determining a suitable
           value to be supplied to `set_max_quality_layers'.  Returns 0 if
           there are no code-streams currently in use.
      */
  //----------------------------------------------------------------------------
  public: // Functions used to control dynamic processing
    KDU_AUX_EXPORT virtual kdu_thread_env *
      set_thread_env(kdu_thread_env *env, kdu_thread_queue *env_queue);
      /* [SYNOPSIS]
           From Kakadu version 5.1, this function offers the option of
           multi-threaded processing, which allows enhanced throughput on 
           multi-processor (or hyperthreading) platforms.  Multi-threaded
           processing may be useful even if there is only one physical
           (or virtual) processor, since it allows decompression work to
           continue while the main application is blocked on an I/O
           condition or other event which does not involve the CPU's resources.
           [//]
           To introduce multi-threaded processing, invoke this function you
           have simply to create a suitable `kdu_thread_env' environment by
           following the instructions found with the definition of
           `kdu_thread_env', and then pass the object into this function.
           [//]
           In the simplest case, the owner of your multi-threaded processing
           group is the one which calls all of the `kdu_region_compositor'
           interface functions.  In which case the `env' object should belong
           to this owning thread and there is no need to do anything more,
           other than invoke `kdu_thread_entity::destroy' once you are
           completely finished using the multi-threaded environment.
           [//]
           It is possible, however, to have one of the auxiliary worker threads
           within your thread group access the `kdu_region_compositor' object.
           This can be useful for video or stereo processing applications,
           where separate `kdu_region_compositor' objects are created to
           manage two different image buffers.  In this case, you may create
           separate queues for each of the objects, and have the various calls
           to `kdu_region_compositor' delivered from jobs which are run on the
           relevant queue.  This allows for parallel processing of multiple
           image composition tasks, which can be helpful in minimizing the
           amount of thread idle time in environments with many processors.
           When operating in this way, however, you must observe the following
           strict requirements:
           [>>] The thread identified by the `env' object supplied to this
                function must be the only one which is used to call any of
                this object's interface functions, from that point until
                the present function is invoked again.
           [>>] Where this function is used to identify that a new thread will
                be calling the object's interface functions (i.e., where the
                `env' argument identifies a different thread to the previous
                one), you must be quite sure that all internal processing has
                stopped.  This can be achieved by ensuring that whenever a
                job on a thread queue needs to use the present object's
                interface functions, it does not return until either
                `is_processing_complete' returns true or `halt_processing' has
                been called.  Later, if another job is run on a different
                thread, it will be able to successfully register itself as the
                new user of the object's interface functions.  Each such
                job should call this function as its first task, to identify
                the context from which calls to `process' and other functions
                will be delivered.
         [RETURNS]
           NULL if this function has never been called before, or else the
           `env' value which was supplied in the last call to this function.
           If the return value differs from the supplied `env' argument,
           access control to this object's member function is assumed to
           be getting assigned to a new thread of execution.  This is legal
           only if there is no work which is still in progress under the
           previous thread environment -- a condition which can be avoided
           by having the previous access thread call `halt_processing' or
           ensure that `is_processing_complete' returns true before
           releasing controll.  Failure to observe this constraint will
           result in the delivery of a suitable error message through
           `kdu_error'.
         [ARG: env]
           NULL if multi-threaded processing is to be disabled.  Otherwise,
           this argument identifies the thread which will henceforth have
           exclusive access to the object's member functions.  As mentioned
           above, the thread with access rights to this object's member
           functions may be changed only when there is no outstanding
           processing (i.e., when `is_processing_complete' returns true or
           when no call to `process' has occurred since the last call to
           `halt_processing').
           [//]
           As a convenience to the user, if `env' points to an object whose
           `kdu_thread_entity::exists' function returns false, the behaviour
           will be the same as if `env' were NULL.
         [ARG: env_queue]
           The queue referenced by this argument will be passed through to
           `kdu_region_decompressor::start' whenever that function is called
           henceforth.  The `env_queue' may be NULL, as discussed in connection
           with `kdu_region_decompressor::start'.  You would normally only
           use a non-NULL `env_queue' if you intended to manage multiple
           `kdu_region_compositor' object's concurrently, in which case you
           would do this by accessing them from within jobs registered on the
           relevant object's `env_queue'.  The application would then
           synchronize its access to composited results by passing the
           relevant queue into the `kdu_thread_entity::synchronize' function
           or `kdu_thread_entity::register_synchronized_job' function.
      */
    KDU_AUX_EXPORT virtual bool
      process(int suggested_increment, kdu_dims &new_region);
      /* [SYNOPSIS]
           Call this function regularly (e.g. during an idle processing loop,
           or within a tight loop in a processing thread) until
           `is_processing_complete' returns false.  You will generally need
           to start calling the function again after making any changes to
           the compositing layers, scale, orientation, overlays, or
           buffered region.  You may use the returned `new_region' to
           control all painting in your image application, except where the
           screen must be repainted after being overwritten by an independent
           event.
           [//]
           This function may catch and rethrow an integer exception generated
           by a `kdu_error' handler if an error occurred in processing the
           underlying code-stream.
         [RETURNS]
           False only under one of the following two conditions.  In either
           case, `new_region' is guaranteed to be empty upon return.
           [>>] All processing for the current compositing buffer surface
                was already complete before this function entered;
                this can be verified by calling `is_processing_complete'.
           [>>] During processing, the function encountered a code-stream
                tile-component which does not have sufficient DWT levels to
                permit rendering at the current scale; this can be verified
                by calling `check_invalid_scale_code'.  In response, the
                application may wish to increase the scale and recommence
                rendering.
           [>>] During processing, the function encountered a code-stream
                tile-component which needed to be flipped, yet did not
                support flipping due to the use of certain types of packet
                wavelet decomposition structures; this can be verified by
                calling `check_invalid_scale_code'.  In response, the
                application may wish to change the geometric orientation
                under which it attempts to render the imagery.
         [ARG: suggested_increment]
           The meaning of this argument is identical to its namesake in the
           `kdu_region_decompressor' object.
         [ARG: new_region]
           Upon successful return, this record holds the dimensions of a
           new region which contains at least one newly composited pixel.
           An interactive application may choose to repaint this region
           immediately.
      */
    KDU_AUX_EXPORT bool is_processing_complete()
      { return processing_complete && !composition_invalid; }
      /* [SYNOPSIS]
           Returns true if all processing for the current composition buffer
           surface is complete, meaning that further calls to `process' will
           do nothing.  This situation may change if any change is made to
           the set of buffer surface position, the scale or orientation, the
           number of quality layers to be rendered, or the set (or order) of
           layers to be composed.
      */
    KDU_AUX_EXPORT bool refresh();
      /* [SYNOPSIS]
           Call this function to force the buffer surfaces to be rendered
           and composited again, from the raw codestream data.  This is
           particularly useful when operating with a data source which is
           fed by a dynamic cache whose contents might have expanded since
           the buffer surface was last rendered and composited.
           [//]
           No processing actually takes place within this function; it simply
           schedules the processing to take place in subsequent calls to the
           `process' function.
           [//]
           The call is ignored if the object was constructed with a
           negative `persistent_cache_threshold' argument, since non-persistent
           code-streams do not allow any image regions to be decompressed
           over again.
           [//]
           If one or more of the layers involved in a frame constructed by
           `set_frame' did not previously have sufficient codestream main
           header data to be completely initialized, the present function
           tries to initialize those layers.  If this is successful, the
           image dimensions, buffer surface and dimensions last retrieved via
           `get_total_composition_dims' and `get_composition_buffer' may
           be invalid, so the function returns false, informing the caller
           that it should update its record of the image dimensions and
           buffer surfaces.
         [RETURNS]
           False if the image dimensions, buffer surface and dimensions
           retrieved via a previous call to `get_total_composition_dims'
           and/or `get_composition_buffer' can no longer be trusted.
      */
    KDU_AUX_EXPORT void halt_processing();
      /* [SYNOPSIS]
           Call this function if you want to use any of the internal
           codestreams in a manner which may interfere with internal
           processing.  For example, you may wish to change the appearance,
           open tiles and so forth.  The effects of this call will be entirely
           lost on the next call to `process'.  A similar effect can be
           obtained by calling `refresh', except that this will result in
           everything being decompressed again from scratch in subsequent calls
           to `refresh'.  The present call may discard some partially processed
           results, but leaves all completely processed image regions marked
           as completely processed.
      */
  //----------------------------------------------------------------------------
  public: // Functions used to map locations/regions to codestreams/layers
    KDU_AUX_EXPORT bool
      find_point(kdu_coords point, int &layer_idx, int &codestream_idx);
      /* [SYNOPSIS]
           This function searches through all compositing layers which
           are involved in the current presentation to find the top-most
           compositing layer in which `point' resides.  If one exists,
           the function returns true, and uses `layer_idx' and
           `codestream_idx' to return information about the matching
           compositing layer and its primary codestream.  Otherwise, the
           function returns false.
         [ARG: point]
           Location of a point on the compositing grid, expressed using the
           same coordinate system as that associated with `set_buffer_surface'.
         [ARG: layer_idx]
           Used to return the zero-based index of the top-most compositing
           layer which contributes to imagery at location `point'.  If the
           composited image consists only of a single codestream, the
           value of this argument is set to -1, while the value of
           `codestream_idx' is set to a non-trivial value.  The value of this
           argument is not altered if the function returns false.
         [ARG: codestream_idx]
           Used to return the zero-based index of the primary codestream 
           which contributes to imagery at `point'.  The value of this
           argument is not altered if the function returns false.
      */
    KDU_AUX_EXPORT bool
      map_region(int &codestream_idx, kdu_dims &region);
      /* [SYNOPSIS]
           This function attempts to map the region found, on entry, in the
           `region' argument to the high resolution grid of some codestream.
           The function first finds the uppermost codestream whose contents
           are visible within the supplied region.  If none is found, the
           function returns false.  It one is found, its index is written to
           `codestream_idx' and the region is mapped onto that codestream's
           high resolution canvas and returned via `region'.  Note that
           the `region' supplied on entry is expressed within the same
           coordinate system as that supplied to `set_buffer_surface',
           while the region returned on exit is expressed on the codestream's
           high resolution canvas, after undoing the effects of rotation,
           flipping and scaling.  The returned region is offset so that its
           location is expressed relative to the upper left hand corner of
           the image region on the codestream canvas, which is not necessarily
           the canvas origin.
      */
    KDU_AUX_EXPORT kdu_dims
      inverse_map_region(kdu_dims region, int codestream_idx,
                         int layer_idx=-1);
      /* [SYNOPSIS]
           This function essentially performs the inverse mapping of
           `map_region'.  The `region' argument identifies a region on
           the indicated codestream's high resolution grid, except that
           the location of the region is expressed relative to the upper
           left hand corner of the codestream's image region on that grid
           (usually, but not necessarily the same as the canvas origin).
           [//]
           The returned region idenifies the location and dimensions
           of the original region, as it appears within the coordinate
           system of the composited image, as used by `set_buffer_surface'.
           The function returns an empty region if the supplied codestream
           does not belong to the current composition.
         [ARG: codestream_idx]
           You are allowed to supply a -ve `codestream_idx' argument to
           this function.  If you do this, the `region' is interpreted as
           a region on the current composited surface, corresponding to
           a rendering scale of 1.0 and with the original geometry.  In
           this case, the function corrects only for rendering scale and
           geometry flags (horizontal and vertical flips and transposition).
         [ARG: layer_idx]
           Ignored if `codestream_idx' is < 0.
           If this argument is -ve, the function hunts for the top-most
           layer which contains the codestream, if any.  Otherwise, the
           function performs the inverse mapping for the codestream, as it
           is used by the indicated compositing layer, returning an empty
           region if the codestream does not exist with that layer.
           The compositing layer is identified via its index within the
           data source; for non-JPX data sources, the only value which
           makes sense is 0.
      */
    KDU_AUX_EXPORT kdu_dims
      find_layer_region(int layer_idx, int instance, bool apply_cropping);
      /* [SYNOPSIS]
           Returns the location and dimensions of the indicated compositing
           layer, as it appears on the composited image.  The returned
           dimensions are expressed using the same coordinate system as that
           associated with `set_buffer_surface'.
           [//]
           The same compositing layer can actually be used multiple times
           within a composition.  To distinguish between the different possible
           uses, the `instance' argument may be scanned from 0 and up, until
           the function returns an empty region.
         [RETURNS]
           An empty region if the indicated layer or instance are not used
           within the current composition.
         [ARG: layer_idx]
           Index, starting from 0, of the compositing layer within the data
           source.  For non-JPX data sources, the only value for which a
           non-empty region can be returned is 0.
         [ARG: instance]
           If 0, the top-most instance of the compositing layer will be used;
           if 1, the second-top-most instance of the compositing layer is used;
           and so forth.
         [ARG: apply_cropping]
           If true, the returned region corresponds to the portion of the
           compositing layer which actually contributes to the composited
           image.  Note, however, that the compositing layer may have been
           cropped before being placed on the compositing surface.  If this
           argument is false, the function returns the size of the complete
           compositing layer, and the location of its upper left hand corner,
           as if the cropping had not been performed, but all scaling and
           geometric adjustments are as before.  In this case, the
           returned region need not lie wholly within the composited image
           region returned by `get_total_composition_dims'.
      */
    KDU_AUX_EXPORT kdu_dims
      find_codestream_region(int codestream_idx, int instance,
                             bool apply_cropping);
      /* [SYNOPSIS]
           Returns the location and dimensions of the indicated codestream,
           as it appears on the composited image.  The returned
           dimensions are expressed using the same coordinate system as that
           associated with `set_buffer_surface'.
           [//]
           The same codestream can actually be used multiple times
           within a composition.  To distinguish between the different possible
           uses, the `instance' argument may be scanned from 0 and up, until
           the function returns an empty region.
         [RETURNS]
           An empty region if the indicated codestream or instance are not used
           within the current composition.
         [ARG: codestream_idx]
           Index, starting from 0, of the codestream in the data source.  For
           non-JPX data sources, the only value for which a non-empty region
           can be returned is 0.
         [ARG: instance]
           If 0, the top-most instance of the codestream will be used;
           if 1, the second-top-most instance of the codestream is used;
           and so forth.
         [ARG: apply_cropping]
           If true, the returned region corresponds to the portion of the
           codestream which actually contributes to the composited
           image.  Note, however, that compositing layers and codestreams
           may have been cropped before being placed on the compositing
           surface.  If this argument is false, the function returns the size
           of the complete codestream, and the location of its upper left hand
           corner, as if the cropping had not been performed, but all scaling
           and geometric adjustments are as before.  In this case,
           the returned region need not lie wholly within the composited image
           region returned by `get_total_composition_dims'.
      */
  //----------------------------------------------------------------------------
  public: // Functions used to interact with the internal codestreams
    KDU_AUX_EXPORT kdrc_stream *
      get_next_codestream(kdrc_stream *last_stream_ref,
                          bool only_active_codestreams, bool no_duplicates);
      /* [SYNOPSIS]
           You may use this function to walk through the various
           codestreams which are currently open within the object.  The
           codestreams are referenced by an opaque pointer type,
           `kdrc_stream', which is provided only to enhance type checking
           (it could be any address for all you care).
           [//]
           It is important to realize that an underlying JPEG2000 code-stream
           may be used multiple times within a single image composition.
           For example, a single composited JPX frame may use the same
           code-stream to create a compositing layer which is placed at the
           top left hand corner of the compositing surface, and another layer
           which is overlayed at the bottom right hand corner of the
           compositing surface, possibly even with different scales.
           [//]
           To distinguish between such multiple uses, the present function is
           best understood as returning references to "layer codestreams".
           A layer codestream is associated with exactly one real JPEG2000
           code-stream, and at most one compositing layer.  Both the
           code-stream index and the layer index may subsequently be retrieved
           by calling `get_codestream_info'.
           [//]
           If you are using the present function to scan through real JPEG2000
           code-streams, you may wish to set the `no_duplicates' argument
           to true.  Where a JPEG2000 code-stream is shared by multiple
           layer codestreams, this option will ensure that only one of
           the layer codestreams will appear in the sequence.  You have
           no guarantees regarding which of the layer codestreams which
           share a common JPEG2000 code-stream will appear in the sequence,
           except that if any of them is active, the layer codestream
           which appears in the sequence will be one of the active ones.
           [//]
           There are two types of layer codestreams: 1) those which are
           currently in use (active), contributing to the composited image
           which is being (or has been) built; and 2) those which are
           inactive, but have not been permanently destroyed.  Included in
           the latter category are code-streams which were opened to serve
           compositing layers which have since been removed using
           `remove_compositing_layer', but not permanently.  The
           `only_active_codestreams' argument determines whether you wish to
           scan through only the active code-streams, or both the active and
           inactive code-streams.
           [//]
           To access the first layer codestream in sequence, supply a
           NULL `last_stream_ref' argument.  The next layer codestreams in
           sequence may then be retrieved by supplying the previously
           returned pointer as the `last_stream_ref' argument in a
           subsequent call to this function.  The actual sequence in
           which the layer codestreams appear has no particular significance
           and may potentially change each time a set of compositing layers
           is configured.  However, you can be guaranteed that all active
           layer codestreams will appear before any inactive layer codestreams
           in the sequence.
           [//]
           You may optionally restrict the search to those layer-codestreams
           which are used to produce image intensity samples which are visible
           or potentially visible (i.e., not completely covered by opaque
           foreground content) within a defined region, given by the
           `visibility_region' argument.
         [RETURNS]
           Opaque pointer which may be passed to `access_codestream' or
           `get_codestream_info', or used with the present function to
           advance to the next layer codestream in sequence.
         [ARG: last_stream_ref]
           If NULL, the returned pointer references the first layer
           codestream in sequence.  Otherwise, the function returns a reference
           to the stream following that identified by `last_stream_ref',
           after skipping over any duplicates (if `no_duplicates'=true).
         [ARG: only_active_codestreams]
           If true, the sequence of layer codestreams returned by this
           function will include only those on the internal active list;
           not the inactive ones.
         [ARG: no_duplicates]
           If true, the sequence of layer codestreams returned by this
           function will skip layer codestreams as required to ensure
           that only one of the layer codestreams which use any given
           JPEG2000 code-stream will be seen.
      */
    KDU_AUX_EXPORT kdrc_stream *
      get_next_visible_codestream(kdrc_stream *last_stream_ref,
                                  kdu_dims region, bool include_alpha);
      /* [SYNOPSIS]
           Similar to `get_next_codestream', you may use this function to
           walk through the various codestreams which are currently open
           within the object.  In this case, however, only those codestreams
           which contribute to the reconstruction of the supplied `region'
           are visited and returned.  As for `get_next_codestream',
           the returned pointer should be treated as an opaque reference which
           is meaningful only when used in conjunction with this and other
           interface functions provided by this particular object.
           [//]
           Note that visible codestreams are necessarily active, following
           the definition provided in the description of `get_next_codestream'.
           Moreover, if no scale has been successfully installed using
           `set_scale' (i.e., if `get_total_composition_dims' returns false),
           no codestream can possibly be visible within the supplied
           `region', since a rendering coordinate system has not yet been
           established -- in such a case, the function must return NULL.
         [RETURNS]
           Opaque pointer which may be passed to `access_codestream' or
           `get_codestream_info', or used with the present function to
           advance to the next layer codestream in sequence.
         [ARG: last_stream_ref]
           If NULL, the returned pointer references the first layer
           codestream in sequence.  Otherwise, the function returns a reference
           to the stream following that identified by `last_stream_ref'.
         [ARG: region]
           Region within which visibility is to be assessed, described
           with respect to the same rendering coordinate system as that used
           by `set_buffer_surface'.
         [ARG: include_alpha]
           If false, the function passes over any layer codestreams whose
           contribution to the `region' consists only in the provision of
           alpha blending data.
      */
    KDU_AUX_EXPORT kdu_codestream
      access_codestream(kdrc_stream *stream_ref);
      /* [SYNOPSIS]
           Use this function with the `kdrc_stream' pointer returned by a
           previous call to `get_next_codestream' or
           `get_next_visible_codestream'.  It returns an interface
           to the actual JPEG2000 code-stream being used by the layer
           codestream.
           [//]
           Note that the `kdrc_stream' pointers returned by
           `get_next_codestream' or `get_next_visible_codestream' must be
           considered invalid and not passed to the present function,
           if the composition has since been modified by a call to any of
           `set_frame', `add_compositing_layer', `remove_compositing_layer',
           `set_single_component' or `cull_inactive_layers'.
           [//]
           Note also that this function automatically halts any internal
           processing which is using the identified codestream, so that you
           will be free to change its appearance via the
           `kdu_codestream::apply_input_restrictions' and/or
           `kdu_codestream::change_appearance' functions.  When you receive
           the codestream, however, its appearance is in an unknown state, so
           any operations you wish to perform which depend upon the codestream
           appearance should be preceded by calls to the above-mentioned
           functions.
           [//]
           As a general rule of thumb, you should not continue to access the
           codestream interface returned by this function after subsequent
           invocation of any of the `kdu_region_compositor' object's
           interface functions.  This is by far the safest policy.  In
           particular, you should definitely refrain from calling the
           `process' or `set_scale' functions while continuing to use the
           returned codestream interface.
           [//]
           Since frequent halting and restarting of internal processing can
           be computationally expensive, it is best to avoid calling this
           function too frequently, or to do so only when the object is in
           the idle state (as indicated by the `is_processing_complete'
           function).
      */
    KDU_AUX_EXPORT bool
      get_codestream_info(kdrc_stream *stream_ref, int &codestream_idx,
                          int &compositing_layer_idx,
                          int *components_in_use=NULL,
                          int *principle_component_idx=NULL,
                          float *principle_component_scale_x=NULL,
                          float *principle_component_scale_y=NULL);
      /* [SYNOPSIS]
           Use this function with the `kdrc_stream' pointer returned by
           a previous call to `get_next_codestream' or
           `get_next_visible_codestream'.  It returns information
           concerning the associated layer codestream.  For a discussion of
           "layer codestreams", see the `get_next_codestream' function.
           [//]
           Note that the `kdrc_stream' pointers returned by
           `get_next_codestream' and `get_next_visible_codestream' must be
           considered invalid and not passed to the present function, if the
           composition has since been modified by a call to any of
           `set_frame', `add_compositing_layer', `remove_compositing_layer',
           `set_single_component' or `cull_inactive_layers'.
         [RETURNS]
           True if the indicated layer codestream is on the internal active
           list; for a discussion of the active list, see the comments
           appearing with `get_next_codestream'.
         [ARG: stream_ref]
           Pointer returned by a previous call to `get_next_codestream' or
           `get_next_visible_codestream'.
         [ARG: codestream_idx]
           Used to return the absolute index of the JPEG2000 code-stream
           associated with this layer codestream, as it appears within
           its data source.
         [ARG: compositing_layer_idx]
           Used to return the absolute index of the compositing layer
           associated with this layer codestream, as it appears within its
           data source.  If the codestream has never been associated with
           any compositing layer (e.g., only opened as a result of calls to
           `set_single_component'), this variable is set to -1.
         [ARG: components_in_use]
           If non-NULL, this argument must point to an array with at least
           4 elements.  The array's entries are filled out with the indices
           (starting from 0) of the image components which are actually being
           used within this code-stream.  All four entries of the array are
           filled out, but once all components which are in use have been
           accounted for, remaining entries are set to -1.
           [//]
           Except in the event that the object is in the single component mode
           (see `set_single_component'), the components identified here are
           "output image components", meaning that they are the components
           produced after performing any multi-component transform which might
           be specified at the code-stream level.  To understand the
           distinction between output components and codestream components
           (the ones produced by inverse spatial wavelet transformation), see
           the description of the `kdu_codestream::apply_input_restrictions'
           function.  It suffices here to note that the set of output components
           which are being used may be larger or smaller than the corresponding
           set of codestream image components.  In single component mode,
           the single component identified here will be identical to that which
           was passed to `set_single_components'.
         [ARG: principle_component_idx]
           If non-NULL, this argument is used to return the index of the
           image component of this codestream whose dimensions form the
           basis of scaling decisions for rendering purposes.  If there
           are multiple components being used, this will generally be the
           component which is used to generate the first colour channel
           supplied to any colour mapping process.  The interpretation of
           this component index is identical to that of the component indices
           returned via the `components_in_use' argument.
         [ARG: principle_component_scale_x]
           If non-NULL, this argument is used to return the amount by which
           the principle image component is horizontally scaled to obtain
           its contributions to the composited result, assuming that
           `set_scale' is called with a `scale' value of 1.0.  The actual
           component scaling factor can be deduced by multiplying the
           returned value by the global scaling factor supplied to
           `set_scale'.  The value returned here does not rely upon a
           successful call to `set_scale' ever having been made and is
           independent of any geometric transformations requested in a
           call to `set_scale'.  Thus, if you are really rendering
           (or intending to render) the surface in a transposed fashion,
           you should swap the horizontal and vertical scales returned
           by this function in order to obtain scaling factors which are
           appropriate for your intended rendering orientation.
         [ARG: principle_component_scale_y]
           Same as `principle_component_scale_x', but used to return
           the vertical scaling factor associated with the principle
           image component.
      */
    KDU_AUX_EXPORT bool
      get_codestream_packets(kdrc_stream *stream_ref,
                    kdu_dims region, kdu_long &visible_precinct_samples,
                    kdu_long &visible_packet_samples,
                    kdu_long &max_visible_packet_samples);
      /* [SYNOPSIS]
           It is best to wait until all processing is complete before invoking
           this function, since it will halt any current processing which
           uses the codestream -- frequently halting and restarting the
           processing within a codestream can cause considerable computational
           overhead.
           [//]
           This function may be used to discover the degree to which
           codestream packets which are relevant to the visible portion
           of `region' are available for decompression.  This information, in
           turn, may be used as a measure of the amount of relevant information
           which has been loaded into a dynamic cache, during remote browsing
           with JPIP, for example.  To obtain this information, the
           function uses `kdu_resolution::get_precinct_samples' and
           `kdu_resolution::get_precinct_packets',scanning the precincts
           which are relevant to the supplied `region' according to their
           visible area.  The `region' argument is expressed with the same
           rendering coordinate system as that associated with
           `get_buffer_surface', but the sample counts returned by the
           last three arguments represent relevant actual JPEG2000 subband
           samples.  Samples produced by the codestream
           are said to be visible if they are not covered by any opaque
           composition layer which is closer to the foreground.  A foreground
           layer is opaque if it has no alpha blending channel.
           [//]
           The value returned via `visible_precinct_samples' is intended
           to represent the total number of subband samples which
           contribute to the reconstruction of any visible samples within
           `region'.  While it will normally hold exactly this value, you
           should note that some samples may be counted multiple times if
           there are partially covering foreground compositing layers.  This
           is because the function internally segments the visible portion of
           `region' into a collection of disjoint rectangles (this is always
           possible) and then figures out the sample counts for each region
           separately, adding the results.  Since the DWT is expansive,
           wherever more than one adjacent rectangle is required to cover the
           region, some samples will be counted more than once.
           [//]
           The value returned via `visible_packet_samples' is similar to
           that returned via `visible_precinct_samples', except that each
           subband sample, n, which contributes to the visible portion of
           `region', contributes Pn to the `visible_packet_samples' count,
           where Pn is the number of packets which are currently available
           for the precinct to which it belongs, from the compressed data
           source.  This value is recovered using
           `kdu_resolution::get_precinct_packets'.
           [//]
           The value returned via `max_visible_packet_samples' is similar to
           that returned via `visible_precinct_samples', except that each
           subband sample, n, which contributes to the visible portion of
           `region', contributes Ln to the `visible_packet_samples' count,
           where Ln is the maximum number of packets which could potentially
           become available for the precinct to which it belongs.  This
           value is recovered using `kdu_tile::get_num_layers'.
           [//]
           Where samples are counted multiple times (as described
           above), they are counted multiple times in the computation of
           all three sample counters, so that the ratio between
           `visible_packet_samples' and `max_visible_packet_samples' will be
           1 if and only if all possible packets are currently available for
           all precincts containing subband samples which are involved in the
           reconstruction of the visible portion of `region'.
         [RETURNS]
           False if the "layer codestream" makes no visible contribution to
           `region'.
         [ARG: stream_ref]
           Pointer returned by a previous call to `get_next_visible_codestream'.
         [ARG: region]
           Region of interest, expressed within the same rendering coordinate
           system as that used by `set_buffer_surface'.
      */
  //----------------------------------------------------------------------------
  public: // Functions used to manage metadata overlays
    KDU_AUX_EXPORT void
      configure_overlays(bool enable, int min_display_size,
                         int painting_param);
      /* [SYNOPSIS]
           Use this function to enable or disable the rendering of spatially
           sensitive metadata as overlay information on the compositing
           surface.  When enabled, overlay information is generated as
           required during calls to `process' and folded into the composited
           result.  If overlay surfaces were previously active, disabling
           the overlay functionality may cause the next call to `process'
           to return a `new_region' which is the size of the entire composited
           image, so that your application will know that it needs to repaint
           the whole thing.
           [//]
           Overlays are available only when working with JPX data sources.
         [ARG: enable]
           If true, overlays are enabled (or remain enabled).  If false,
           overlays are disabled and the remaining arguments are ignored.
         [ARG: min_display_size]
           Specifies the minimum size occupied by the overlay's bounding
           box on the composited surface, at the prevailing scale.  If an
           overlay has a bounding box which will be rendered with both
           dimensions smaller than `min_display_size', it will not be
           rendered at all.  Setting a modest value for this parameter, say
           in the range 4 to 8, helps reduce clutter when viewing an image
           with a lot of metadata.  On the other hand, this can make it hard
           to locate tiny regions of significance when zoomed out to a small
           scale.
         [ARG: painting_param]
           Parameter passed to the overridable `custom_paint_overlay'
           function, which may be used to control the appearance of the overlay
           imagery.  The default implementation in `paint_overlay' uses this
           parameter as follows:
           [>>] The least significant 8 bits represent a signed 2's complement
                number, which controls the intensity of the overlay pattern.
                A value of 0 represents "normal" intensity; negative values
                represent reduced intensity (-128 being the minimum); and
                positive values represent higher intensities (+127 being the
                maximum).
           [>>] The remaining bits are used for flashing the overlay.  Each
                time you increment the value represented by these bits, the
                overlay will be modulated periodically to a different colour.
                The expectation is that if you want a flashing overlay, you
                will call the present function every half second or so.
           [//]
           Each time you call this function with a different value for the
           `painting_param' argument, all active metadata is scheduled for
           repainting with the new parameter.  The actual painting occurs
           during calls to `process' -- each call will repaint at most one
           overlay region.
      */
    KDU_AUX_EXPORT void update_overlays(bool start_from_scratch);
      /* [SYNOPSIS]
           Call this function if you have made any changes in the metadata
           associated with a JPX data source and want them to be reflected
           on overlays folded into the composited image.  This function does
           not actually paint any overlay data itself, but it does schedule
           the updates to be performed in subsequent calls to the `process'
           function.
           [//]
           You MUST call this function with `start_from_scratch' set to true,
           immediately after using `jpx_metanode::delete' to delete one or
           more metadata nodes.  Otherwise, there is a risk that ongoing
           metadata manipulation in this object will cause an access violation.
           [//]
           If you have only added metadata, however, it is sufficient to call
           this function with `start_from_scratch' set to false.
         [ARG: start_from_scratch]
           If true, all overlay surfaces will be erased and all previously
           discovered metadata will be treated as invalid.  Subsequent calls
           to `process' will then generate the metadata overlays from
           scratch.  In fact, the next call to `process' may return a new
           region which represents the entire buffer surface, forcing the
           application to repaint the entire buffer.
      */
    KDU_AUX_EXPORT jpx_metanode
      search_overlays(kdu_coords point, int &codestream_idx);
      /* [SYNOPSIS]
           This function searches through all overlay metadata, returning
           a non-empty interface only if a node is found whose bounding box
           includes the supplied point.  The function returns an empty
           interface if overlays are not currently enabled, or if the
           relevant bounding box is not currently visible, as determined by
           the opacity of layers which may lie on top of a layer whose
           codestream actually contains the metadata.
           [//]
           Overlays are available only when working with JPX data sources.
         [RETURNS]
           An empty interface unless a match is found, in which case the
           returned object is represented by an ROI description box -- i.e.,
           `jpx_metanode::get_num_regions' is guaranteed to return a value
           greater than 0.
         [ARG: point]
           Location of a point on the compositing grid, expressed using the
           same coordinate system as that associated with `set_buffer_surface'.
         [ARG: codestream_idx]
           Used to return the zero-based index of the codestream whose overlay
           contains the metadata whose bounding box contains `point'.
      */
    virtual bool
      custom_paint_overlay(kdu_compositor_buf *buffer, kdu_dims buffer_region,
                           kdu_dims bounding_region, int codestream_idx,
                           jpx_metanode node, int painting_param,
                           kdu_coords image_offset, kdu_coords subsampling,
                           bool transpose, bool vflip, bool hflip,
                           kdu_coords expansion_numerator,
                           kdu_coords expansion_denominator,
                           kdu_coords compositing_offset) { return false; }
      /* [SYNOPSIS]
         [BIND: callback]
           This function is called from the internal machinery whenever the
           overlay for some spatially sensitive metadata needs to be painted.
           The default implementation returns false, meaning that overlays
           should be painted using the regular `paint_overlay' function.
           To implement a custom overlay painter, you should override this
           present function in a derived class and return true.  In that
           case, `paint_overlay' will not be called.  It is also possible
           to directly override the `paint_overlay' function, but this does
           not provide a solution for alternate language bindings and is thus
           not recommended.  The present function can be implemented in any
           Java, C#, Visual Basic or other managed class derived from the
           `kdu_region_compositor' object's language binding.
           [//]
           Overlays are available only when working with JPX data sources.
           [//]
           The `buffer' argument provides the upper left hand corner of
           a raster scan overlay buffer, which represents the entire buffered
           area associated with a particular codestream.  For reliable
           behaviour, you should confine your painting to the supplied
           `bounding_region'.  The reason for this is that anything painted
           outside this region might not be correctly erased or repainted as
           the associated metadata region moves onto and off the overlay buffer
           surface (due to user panning, for example).
           [//]
           While detailed descriptions of the various arguments appear
           below, it is worth describing the geometric mapping process
           up front.  Region sensitive metadata is always associated with
           specific codestreams, via an ROI description box.  The ROI
           description box describes the regions on the codestream's
           high resolution canvas, with locations offset relative to the
           upper left hand corner of the image region on this canvas (the
           image region might not necessarily start at the canvas origin).
           By contrast, the overlay buffer's coordinate system is referred
           to a compositing reference grid, which depends upon compositing
           instructions in the JPX data source, as well as the current
           scale and orientation of the rendering surface.  To map from an
           ROI description region, `R', to the compositing reference grid,
           the following steps are required:
           [>>] Add `image_offset' to all locations in `R'.  This translates
              the region to one which is correctly registered against the
              codestream's high resolution canvas.
           [>>] Divide the starting (or entry) coordinates Ex,Ey and ending
              (or finishing) coordinates Fx,Fy, by the factors in
              `subsampling', rounding upwards.  Here (Ex,Ey) corresponds
              to the upper left hand corner of the region, while the
              finishing coordinates are exclusive upper bounds to that
              (Fx-1,Fy-1) corresponds to the lower right hand corner of the
              region.  The policy of dividing these coordinates and rounding
              upwards is the convention adopted by the JPEG2000 canvas
              coordinate system, as described in the book by Taubman and
              Marcellin.
           [>>] Apply `kdu_dims::to_apparent', supplying the `transpose'
              `vflip' and `hflip' parameters, in order to map the
              region coordinates to the orientatation currently being
              used for the rendering surface.
           [>>] Multiply the coordinates of the region by the factors found
                in `expansion_numerator' and divide by the factors found in
                `expansion_denominator'.
           [>>] Subtract `compositing_offset' from all region coordinates.
         [ARG: buffer]
           Pointer to the current overlay buffer.  The size and location
           of this buffer are described by the `buffer_region' argument.
           The overlay buffer has alpha, R, G and B components and is alpha
           blended with the image component associated with this overlay.
           When painting the buffer, it is your responsibility to write alpha,
           R, G and B values at each desired location.  You are not responsible
           for performing the alpha blending itself.  The organization of the
           four channels within each 32-bit word is as follows:
           [>>] the most significant byte of the word holds the alpha value;
           [>>] the second most significant byte holds the red channel value;
           [>>] the next byte holds the green channel value; and
           [>>] the least significant byte holds the blue channel value.
         [ARG: buffer_region]
           Region occupied by the overlay buffer, expressed on the compositing
           reference grid.  See the description of region mapping conventions
           in the introduction to this function.
         [ARG: bounding_region]
           Bounding rectangle, containing all of the regions described by
           the ROI description box associated with `node'.  This bounding
           rectangle is not necessarily fully contained within the
           `buffer_region', but they should intersect.  It uses the same
           coordinate system as `buffer_region' -- i.e., the compositing
           reference grid, as opposed to the codestream canvas.
         [ARG: codestream_idx]
           Identity of the codestream with which this overlay is associated.
           Each overlay is associated with exactly one codestream, since
           region-specific metadata is always registered against the
           canvas of some codestream.
         [ARG: node]
           This is guaranteed to represent an ROI description box.  Use the
           `jpx_metanode::get_regions' member (for example) to recover the
           geometrical properties of the region.  Use other
           `jpx_metanode' members to examine any descendant nodes, describing
           metadata which is associated with this spatial region.  A typical
           implementation might look for label boxes, XML boxes or UUID boxes
           (with associated URL's) in the descendants of `node'.
         [ARG: painting_param]
           This is a copy of the parameter supplied in the most recent call
           to `configure_overlays'.  Your implementation may use this
           parameter to adjust the appearance of the overlay image.
         [ARG: image_offset]
           See the 5 coordinate transformation steps described in the
           introduction to this function.
         [ARG: subsampling]
           See the 5 coordinate transformation steps described in the
           introduction to this function.
         [ARG: transpose]
           See the 5 coordinate transformation steps described in the
           introduction to this function.
         [ARG: vflip]
           See the 5 coordinate transformation steps described in the
           introduction to this function.
         [ARG: hflip]
           See the 5 coordinate transformation steps described in the
           introduction to this function.
         [ARG: expansion_numerator]
           See the 5 coordinate transformation steps described in the
           introduction to this function.
         [ARG: expansion_denominator]
           See the 5 coordinate transformation steps described in the
           introduction to this function.
         [ARG: compositing_offset]
           See the 5 coordinate transformation steps described in the
           introduction to this function.
      */
  protected:
    virtual void
      paint_overlay(kdu_compositor_buf *buffer, kdu_dims buffer_region,
                    kdu_dims bounding_region, int codestream_idx,
                    jpx_metanode node, int painting_param,
                    kdu_coords image_offset, kdu_coords subsampling,
                    bool transpose, bool vflip, bool hflip,
                    kdu_coords expansion_numerator,
                    kdu_coords expansion_denominator,
                    kdu_coords compositing_offset);
      /* [SYNOPSIS]
           See `custom_paint_overlay' for a descrition.  The present
           function implements the default overlay painting policy, which
           simply paints shaded boxes or ellipses, as appropriate, onto the
           overlay buffer, for each regoin of an ROI metanode.  More
           sophisticated implementations may override this function, or
           (preferably) override the `custom_paint_overlay' function, to
           paint more interesting overlays, possibly incorporating text
           labels.  If you wish to do this from a foreign language, such
           as Java, C# or Visual Basic, you must override the
           `custom_paint_overlay' function.
      */
  //----------------------------------------------------------------------------
  public: // Virtual functions to be overridden
    virtual kdu_compositor_buf *
      allocate_buffer(kdu_coords min_size, kdu_coords &actual_size,
                      bool read_access_required) { return NULL; }
      /* [SYNOPSIS]
         [BIND: callback]
           Override this function to provide your own image buffer
           allocation.  For example, you may want to allocate the 
           image buffer as part of a system resource from which rendering
           to a display can be accomplished more easily or efficiently.
           Note that each pixel is represented by 4 bytes.  You may allocate
           a larger buffer than the one requested, returning the actual
           buffer size via the `actual_size' argument.  This reduces the
           likelihood that reallocation will be necessary during interactive
           viewing.
           [//]
           The `read_access_required' argument is provided to support
           efficient use of special purpose memory resources which might
           only support writing.  If the internal machinery only intends
           to write to the buffer, it sets this flag to false.  It can
           happen that a buffer which was originally allocated as write-only
           must later be given read access, due to unforeseen changes.
           For example, a compositing layer which was originally rendered
           directly onto the frame might later need to be composed with
           other compositing layers (due to changes in the frame contents)
           for which read access is required.  Whenever the access type
           changes, `kdu_compositor_buf::set_read_accessibility' will be
           called.  The buffer manager has the option to allocate memory in
           a different way whenever the access type changes; it also has the
           option to instruct the internal machinery to regenerate the buffer
           when this happens, as explained in the description accompanying
           `kdu_compositor_buf::set_read_accessibility'.
           [//]
           For maximum memory access efficiency, when overriding this function,
           you should attempt to allocate `kdu_compositor_buf' objects whose
           referenced memory buffer is aligned on a 16-byte boundary.  For
           example, you might allocate a buffer which is somewhat larger than
           required, so that the `kdu_compositor_buf::buf' member can be
           rounded up to the nearest 16-byte boundary.
         [RETURNS]
           If you do not intend to allocate the buffer yourself, the function
           should return NULL (this is what the default implementation does),
           in which case the buffer will be internally allocated and internally
           deleted, without any call to `delete_buffer'.
      */
    virtual void delete_buffer(kdu_compositor_buf *buf) { return; }
      /* [SYNOPSIS]
         [BIND: callback]
           Override this function to deallocate any resources which you
           allocated using an overridden `allocate_buffer' implementation.
           The function will not be called if the buffer was allocated
           internally due to a NULL return from `allocate_buffer'.
      */
  private: // Helper functions
    friend class kdrc_layer;
    friend class kdrc_overlay;
    void init(kdu_thread_env *env, kdu_thread_queue *env_queue);
      /* Called as the first step by each constructor. */
    void set_layer_buffer_surfaces();
      /* Called either within `set_buffer_surface' or, if a valid composition
         was not valid when that function was called, from within
         `update_composition'.  This function scans backwards through the
         compositing layers (i.e., starting with the upper-most layer and
         working back to the background), invoking their respective
         `kdrc_layer::set_buffer_surface' functions.  In the process, however,
         the function also calculates the portion of the buffer region which
         is completely covered by foreground layers, adjusting the buffer
         region supplied to the lower layers to reflect the fact that they
         may be partially or fully obscured.  This saves processing unnecessary
         imagery.  The function also sets `processing_complete' to false. */
    bool update_composition();
      /* Called from within `process', `get_total_composition_dims' or
         `get_composition_buffer' if any change had previously been made to
         the set of compositing layers or the compositing scale/orientation.
         This is where the scale of each individual compositing layer is set,
         where the dimensions of the final composition are determined, and
         where a separate compositing buffer may be allocated. */
    void donate_compositing_buffer(kdu_compositor_buf *buffer,
                                   kdu_dims buffer_region,
                                   kdu_coords buffer_size);
      /* This function is called from `kdrc_layer::set_buffer_surface' or
         `kdrc_layer::update_overlay' if it is discovered that a separate
         compositing buffer will need to be used after all, to accommodate
         the newly discovered need for metadata overlay information. */
    bool retract_compositing_buffer(kdu_coords &buffer_size);
      /* This function is called from inside `kdrc_layer::process_overlay' if
         the overlay buffer has been recently disabled so that a separate
         compositing buffer might no longer be required.  The function
         returns true if the caller can reclaim the global compositing buffer
         for its own layer buffer, deallocating its separate layer buffer.
         If the function returns false, the caller should leave the separate
         compositing buffer alone. */
    kdrc_stream *add_active_stream(int codestream_idx, int layer_idx,
                                   bool alpha_only);
    void remove_stream(kdrc_stream *stream, bool permanent);
      /* The above functions are used by `kdrc_layer' as well as the present
         object to add or remove codestreams from the active list.  The
         `add_active_stream' function first searches the inactive list for
         an existing codestream which matches the request parameters,
         creating a new one only if necessary.  The `remove_active_stream'
         function moves the stream to the inactive list, unless `permanent'
         is true, in which case it destroys the resources.  To add a
         code-stream without any layer, use a negative `layer_idx' value. */
    kdu_compositor_buf *
      internal_allocate_buffer(kdu_coords min_size, kdu_coords &actual_size,
                               bool read_access_required);
      /* This function is used internally to manage the allocation process.
         It calls the overridable callback function `allocate_buffer', doing
         its own allocation only if the latter returns NULL. */
    void internal_delete_buffer(kdu_compositor_buf *buf);
      /* This function is used internally to manage the deletion process.  If
         the buffer was allocated internally, it is deleted internally.
         Otherwise, if the buffer was allocated using `allocate_buffer', it
         is passed to the overridable callback function, `delete_buffer'. */
  private: // Data sources
    kdu_compressed_source *raw_src; // Installed by the 1st constructor
    jpx_source *jpx_src; // Installed by the 2nd constructor
    mj2_source *mj2_src; // Installed by the 3rd constructor
    jpx_input_box single_component_box; // Used to open individual codestream
    int error_level;
    bool persistent_codestreams;
    int codestream_cache_threshold;
  private: // Current composition state
    bool have_valid_scale;
    int max_quality_layers;
    bool vflip, hflip, transpose;
    float scale;
    int invalid_scale_code; // Error code from last scale-dependent failure
    kdu_dims fixed_composition_dims; // If compositing confined to a fixed frame
    kdu_dims default_composition_dims; // For frame, if can't initialize layers
    kdu_dims total_composition_dims;
    kdu_compositor_buf *composition_buffer; // NULL if first layer is only one
    kdu_dims buffer_region;
    kdu_coords buffer_size; // Actual dimensions of memory buffer
    kdu_uint32 buffer_background;
    bool processing_complete; // If the buffered region is fully composited
    bool composition_invalid; // If layers, scale or buffer surface changed
    bool enable_overlays;
    bool can_skip_surface_initialization;
    bool initialize_surfaces_on_next_refresh;
    int overlay_painting_param;
    int overlay_min_display_size;
  private: // Resource lists
    kdrc_queue *queue_head; // Least recent element on jitter-absorbtion queue
    kdrc_queue *queue_tail; // Most recent element on jitter-absorbtion queue
    kdrc_queue *queue_free; // Recycling list for queue elements.
    kdrc_layer *active_layers; // Doubly-linked active compositing layers list
    kdrc_layer *last_active_layer; // Tail of doubly-linked active layers list
    kdrc_layer *inactive_layers; // Singly-linked list of layers not in use
    kdrc_stream *active_streams; // Singly-linked active layer codestreams list
    kdrc_stream *inactive_streams; // Singly-linked list of unused codestreams
    kdrc_refresh *refresh_mgr; // Manages regions which must be refreshed via
                         // calls to `process' and would not otherwise be
                         // refreshed in the course of decompression processing
  private: // Multi-threading
    kdu_thread_env *env;
    kdu_thread_queue *env_queue;
  };

#endif // KDU_REGION_COMPOSITOR_H
