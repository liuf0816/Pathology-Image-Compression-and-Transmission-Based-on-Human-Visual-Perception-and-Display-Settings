/*****************************************************************************/
// File: kdms_renderer.h [scope = APPS/MACSHOW]
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
   Defines the main image management and rendering object associated with
any given window's view.  The `kdms_renderer' object works hand in hand
with `kdms_window' and its document view, `kdms_document_view'.  In fact,
menu commands and other user events received by those objects are mostly
forwarded directly to the `kdms_renderer' object.
******************************************************************************/

#import <Cocoa/Cocoa.h>
#import "kdms_controller.h"
#import "kdms_metashow.h"
#include "kdu_messaging.h"
#include "kdu_compressed.h"
#include "kdu_file_io.h"
#include "jpx.h"
#include "kdu_region_compositor.h"

// External declarations
@class kdms_window;
@class kdms_document_view;
@class kdms_catalog;

// Declared here:
class kdms_file;
struct kdms_file_editor;
class kdms_file_services;
class kdms_compositor_buf;
class kdms_region_compositor;
class kdms_data_provider;
class kdms_playmode_stats;
class kdms_renderer;

enum kds_status_id {
  KDS_STATUS_LAYER_RES=0,
  KDS_STATUS_DIMS=1,
  KDS_STATUS_MEM=2,
  KDS_STATUS_CACHE=3,
  KDS_STATUS_PLAYSTATS=4
};

/*****************************************************************************/
/*                                 kdms_file                                 */
/*****************************************************************************/

class kdms_file {
private: // Private life-cycle functions
  friend class kdms_file_services;
  kdms_file(kdms_file_services *owner); // Links onto tail of `owner' list
  ~kdms_file(); // Unlinks from `owner' first.
public: // Member functions
  int get_id() { return unique_id; }
  /* Gets the unique identifier for this file, which can be used with
   `kdms_file_services::find_file'. */
  void release();
  /* Decrements the retain count.  Once the retain count reaches 0, the
   object is deleted, and the file might also be deleted if it was a
   temporary file. */
  void retain()
    { retain_count++; }
  const char *get_pathname() { return pathname; }
  bool get_temporary() { return is_temporary; }
  bool create_if_necessary(const char *initializer);
  /* Creates the file and sets some initial contents based on the supplied
   `initializer' string, if any.  Returns false if the file cannot be
   created, or it already exists.  If the `initializer' string is NULL,
   any file created here is opened as a binary file; otherwise, it is
   created to hold text. */
private: // Data
  int retain_count;
  int unique_id;
  kdms_file_services *owner;
  kdms_file *next, *prev; // Used to build a doubly-linked list
  char *pathname;
  bool is_temporary; // If true, the file is deleted once `retain_count'=0.
};

/*****************************************************************************/
/*                             kdms_file_editor                              */
/*****************************************************************************/

struct kdms_file_editor {
  kdms_file_editor()
    {
      doc_suffix = app_pathname = NULL;
      app_name = NULL;
      next = NULL;
    }
  ~kdms_file_editor()
    {
      if (doc_suffix != NULL) delete[] doc_suffix;
      if (app_pathname != NULL) delete[] app_pathname;
    }
  char *doc_suffix; // File extension for matching document types
  char *app_pathname; // Full pathname to the application bundle
  const char *app_name; // Points to the application name within full pathname
  FSRef fs_ref; // File-system reference for the editor
  kdms_file_editor *next;
};

/*****************************************************************************/
/*                            kdms_file_services                             */
/*****************************************************************************/

class kdms_file_services {
public: // Member functions
  kdms_file_services(const char *source_pathname);
    /* If `source_pathname' is non-NULL, all temporary files are created by
     removing its file suffix, if any, and then appending to the name to
     make it unique.  This is the way you should create the object when
     editing an existing file.  Otherwise, if `source_pathname' is NULL,
     a base pathname is created internally using the `tmpnam' function. */
  ~kdms_file_services();
  bool find_new_base_pathname();
    /* This function sets the base pathname for temporary files based upon
     the ANSI tmpnam() function.  It erases any existing base pathname and
     generates a new one which is derived from the name returned by tmpnam()
     after first testing that the file returned by tmpnam() can be created.
     If the file cannot be created, the function returns false.  The function
     is normally called only internally from within `confirm_base_pathname'. */
  kdms_file *retain_tmp_file(const char *suffix);
    /* Generates a unique filename.  Does not try to create the file at this
     point, but may do so later.  In any case, the file will be deleted once
     it is no longer needed.  The returned object has a retain count of 1.
     Once you release the object, it will be destroyed, and the temporary
     file deleted.  */
  kdms_file *retain_known_file(const char *pathname);
    /* As for the last function, but the returned object represents a
     non-temporary file (i.e., one that will not be deleted when it is no
     longer required).  Generally, this is an existing file supplied by the
     user.  You should supply the full pathname to the file.  The returned
     object has a retain count of at least 1.  If the same file is already
     retained for some other purpose, the retain count may be greater. */
  kdms_file *find_file(int identity);
    /* This function is used to recover a file whose ID was obtained
     previously using `kdms_file::get_id'.  It provides a robust mechanism
     for accessing files whose identity is stored indirectly via an integer
     and managed by the `jpx_meta_manager' interface. */
  kdms_file_editor *get_editor_for_doc_type(const char *doc_suffix, int which);
    /* This function is used to access an internal list of editors which
     can be used for files (documents) with the indicated suffix (extension).
     The function returns NULL if `which' is greater than or equal to the
     number of matching editors for such files which are known.  If the
     function has no stored editors for the file, it attempts to find some,
     using operating-system services.  You can also add editors using the
     `add_editor_for_doc_type' function. */
  kdms_file_editor *add_editor_for_doc_type(const char *doc_suffix,
                                            const char *app_pathname);
    /* Use this function if you want to add custom editors for files
     (documents) with a given suffix (extension), or if you want to move an
     existing editor to the head of the list. */
private:
  void confirm_base_pathname(); // Called when `retain_tmp_file' is first used
private: // Data
  friend class kdms_file;
  kdms_file *head, *tail; // Linked list of files
  int next_unique_id;
  char *base_pathname; // For temporary files
  bool base_pathname_confirmed; // True once we know we can write to above
  kdms_file_editor *editors; // Head of the list; new editors go to the head
};

/*****************************************************************************/
/*                           kdms_compositor_buf                             */
/*****************************************************************************/

class kdms_compositor_buf : public kdu_compositor_buf {
public: // Member functions and overrides
  kdms_compositor_buf()
    { handle=NULL; next=NULL; }
  virtual ~kdms_compositor_buf()
    {
      if (handle != NULL) delete[] handle;
      buf = NULL; // Remember to do this whenever overriding the base class
    }
protected: // Additional data
  friend class kdms_region_compositor;
  kdu_uint32 *handle;
  kdu_coords size;
  kdms_compositor_buf *next;
};

/*****************************************************************************/
/*                          kdms_region_compositor                           */
/*****************************************************************************/

class kdms_region_compositor : public kdu_region_compositor {
  /* This class augments the `kdu_region_compositor' class with display
   buffer management logic which allocates image buffers whose lines are
   all aligned on 16-byte boundaries.  It also recycles buffers, to
   minimize excessive allocation and reallocation of large blocks of
   memory.  This can potentially improve cache utilization in video
   applications.  You can take this class as a template for designing richer
   buffer management systems -- e.g. ones which might utilize display
   hardware explicitly. */
public: // Member functions
  kdms_region_compositor(kdu_thread_env *env=NULL,
                         kdu_thread_queue *env_queue=NULL)
                 : kdu_region_compositor(env,env_queue)
    { active_list = free_list = NULL; }
  ~kdms_region_compositor()
    {
      pre_destroy(); // Destroy all buffers before base destructor called
      assert(active_list == NULL);
      while ((active_list=free_list) != NULL)
        { free_list=active_list->next; delete active_list; }
    }
  virtual kdu_compositor_buf *
    allocate_buffer(kdu_coords min_size, kdu_coords &actual_size,
                    bool read_access_required);
  virtual void delete_buffer(kdu_compositor_buf *buffer);
private: // Data
  kdms_compositor_buf *active_list;
  kdms_compositor_buf *free_list;
};

/*****************************************************************************/
/*                            kdms_data_provider                             */
/*****************************************************************************/

class kdms_data_provider {
public: // Member functions
  kdms_data_provider();
  ~kdms_data_provider();
  CGDataProviderRef init(kdu_coords size, kdu_uint32 *buf, int row_gap,
                         bool display_with_focus=false,
                         kdu_dims focus_box=kdu_dims());
    /* This function sets up the data provider for painting imagery in
       response to calls to its `get_bytes_callback' function.  The region
       to be painted has dimensions given by `size' and the data is buffered
       in the array given by `buf', whose successive image rows are
       separated by `row_gap'.  The start of the buffer is aligned with
       the upper left corner of the region to be painted, as supplied in
       an outer call to `CGImageCreate' (see `kdms_renderer::paint_region').
       The caller should attempt to adjust the region to be painted so
       that `buf' is aligned on a 16-byte boundary.  This maximizes the
       opportunity for efficient SIMD implementations of the process which
       transfers data to the GPU when `get_bytes_callback' is called.
         The internal implementation selects between a number of transfer
       strategies, depending on the machine architecture, word alignment
       and whether or not a focus box is required.
         The `focus_box' identifies a region (possibly empty) relative to
       the top-left corner of the region to be painted, where a focus box
       is to be highlighted.  Focus box highlighting is performed by passing
       the imagery inside and outside the focus box through two different
       intensity tone curves, so that the focussing region appears
       brighter.  This behaviour occurs whenever the `display_with_focus'
       argument is true. The supplied `focus_box' need not be contained
       within (or even intersect) the region being painted. */
private: // The provider callback function
  static size_t get_bytes_callback(void *info, void *buffer,
                                   size_t offset, size_t count);
private: // Data
  off_t tgt_size; // Total number of dwords offered by the provider
  int tgt_width, tgt_height; // Derived from the `init's `size' argument
  int buf_row_gap; // Copy of `init's `row_gap' argument
  kdu_uint32 *buf; // Copy of `init's `buf' argument
  bool display_with_focus; // Indicates that tone curves are required
  int rows_above_focus; // Rows above focus box when focussing
  int focus_rows; // Rows inside focus box when focussing
  int rows_below_focus; // Rows below focus box when focussing
  int cols_left_of_focus; // Columns to the left of focus box when focussing
  int focus_cols; // Columns inside focus box when focussing
  int cols_right_of_focus; // Columns to the right of focus box when focussing
  CGDataProviderRef provider_ref;
  kdu_byte foreground_lut[256]; // Foreground tone curve for use when focussing
  kdu_byte background_lut[256]; // Background tone curve for use when focussing
};

/*****************************************************************************/
/*                            kdms_playmode_stats                            */
/*****************************************************************************/

class kdms_playmode_stats {
public: // Member functions
  kdms_playmode_stats() { reset(); }
  void reset() // Called by `kdms_renderer::start_playmode'
    {
      last_frame_idx = -1;
      num_updates_since_reset = 0;
      cur_time_at_last_update = 0.0;
      mean_time_between_frames = 1.0; // Any value which can be inverted
    }
  void update(int frame_idx, double start_time, double end_time,
              double cur_time);
    // Called each time a rendered frame is pushed onto the playback queue
  double get_avg_frame_rate()
    {
      if ((num_updates_since_reset<2) || (mean_time_between_frames <= 0.0001))
        return -1.0;
      else
        return 1.0 / mean_time_between_frames;
    }
private: // Data
  int num_updates_since_reset; // Number of calls to `update' since `reset'
  int last_frame_idx; // Allows us to ignore duplicate calls to `update'
  double cur_time_at_last_update; // Value of `cur_time' in last `update' call
  double mean_time_between_frames;
};

/*****************************************************************************/
/*                               kdms_renderer                               */
/*****************************************************************************/

class kdms_renderer {
public: // Main public member functions
  kdms_renderer(kdms_window *owner,
                kdms_document_view *doc_view,
                NSScrollView *scroll_view,
                kdms_window_manager *window_manager);
  ~kdms_renderer();
  void perform_essential_cleanup_steps();
    /* This function performs only those cleanup steps which are required to
     correctly manage non-memory resources (generally, temporary files).  It
     is invoked if the application is about to terminate without giving
     the destructor an opportunity to execute. */
  void open_file(const char *fname);
    /* This function enters with `filename'=NULL if it is being called to
     complete an overlapped opening operation.  This happens with the
     "kdu_client" compressed data source, which must first be connected
     to a server and then, after some time has passed, becomes ready
     to actually open a code-stream or JP2-family data source. */
  void close_file();
  void view_dims_changed();
    /* This function is typically invoked automatically when the
     `doc_view' object's bounds or frame rectangle change, e.g., due
     to resizing or scrolling. */
  bool paint_region(kdu_dims region, CGContextRef graphics_context,
                    NSPoint *presentation_offset=NULL);
    /* This function does all the actual work of drawing imagery
     to the window.  The `region' is expressed in the same coordinate
     system as `view_dims' and `image_dims'.  Returns false if nothing
     can be painted, due to the absence of any rendering machinery
     at the present time.  In this case, the caller should take action
     to erase the region itself.
        If this function is called from `paint_view_from_presentation_thread'
     its `presentation_offset' argument will be non-NULL.  In this
     case, the *`presentation_offset' value is added from the
     locations of the region which is painted, to account for the fact
     that the graphics context is based on the window geometry rather
     than the document view geometry. */
  void paint_view_from_presentation_thread(NSGraphicsContext *gc);
    /* This function is used to paint directly to the `doc_view' object
     from the presentation thread.  Calls to this function arrive from the
     `kdms_frame_presenter' object when it needs to present a frame. */
  NSRect convert_region_to_doc_view(kdu_dims region);
    /* Converts a `region', defined in the same coordinate system as
     `view_dims', `buffer_dims' and `image_dims', to a region within
     the `doc_view' coordinate system, which runs from (0,0) at
     its lower left hand corner to (image_dims.size.x*pixel_scale-1,
     image_dims.size.y*pixel_scale-1) at its upper right hand corner. */
  kdu_dims convert_region_from_doc_view(NSRect doc_rect);
    /* Does the reverse of`convert_region_to_doc_view', except that the
     returned region is expanded as required to ensure that it covers
     the `doc_rect' rectangle. */
  bool on_idle();
    /* This function is called when the run-loop is about to go idle.
     This is where we perform our decompression and render processing.
     The function should return true if a subsequent call to the function
     would be able to perform additional processing.  Otherwise, it
     should return false.  The function is invoked from our run-loop
     observer, which sits inside the `kdms_controller' object. */
  void refresh_display();
    /* Called from `initialize_regions' and also from any point where the
     image needs to be decompressed again.  The function is public so
     that it can be invoked while processing the WM_TIMER message received
     by the frame window. */
  void wakeup(CFAbsoluteTime scheduled_wakeup_time,
              CFAbsoluteTime current_time);
    /* This function is called from the `kdms_window_manager' object if a
     wakeup was previously scheduled, once the scheduled time has arrived.
     For convenience, the function is passed the original scheduled wakeup
     time and the current time. */
  void set_codestream(int codestream_idx);
    /* Called from `kdms_metashow' when the user clicks on a code-stream box,
     this function switches to single component mode to display the
     image components of the indicated code-stream. */
  void set_compositing_layer(int layer_idx);
    /* Called from `kdms_metashow' when the user clicks on a compositing layer
     header box, this function switches to single layer mode to display the
     indicated compositing layer. */
  void set_compositing_layer(kdu_coords point);
    /* This form of the function determines the compositing layer index
     based on the supplied location.  It searches for the top-most
     compositing layer which falls underneath the current mouse position
     (given by `point', relative to the upper left corner of the view
     window).  If none is found, no action is taken. */
  bool set_frame(int frame_idx);
    /* Called from `kd_playcontrol' and also internally, this function is
     used to change the frame within an MJ2 track or a JPX animation.
     It invokes `change_frame_idx' but also configures the processing
     machinery to decompress the new frame.  Values which are currently
     illegal will be corrected automatically.  The function returns true
     if it produced a change in the current frame. */
  void stop_playmode();
    /* Wraps up all the functionality to move out of video playback mode. */
  void start_playmode(bool reverse);
    /* Wraps up all the functionality to commence video playback mode.
     The way in which video is played depends upon the existing state of
     the following parameters: `use_native_timing', `custom_fps',
     `rate_multiplier' and `playmode_repeat', each of which may be configured
     using menu commands.  The `reverse' argument indicates whether the
     video playback mode will be entered for playing video in the forward
     or the reverse direction. */
  void adjust_playclock(double delta)
    { if (in_playmode) playclock_offset += delta; }
    /* This function is called by the window manager whenever any video
     playback window finds that it is getting behind and needs to make
     adjustments to its playback clock, so that future playback will not
     try too aggressively to make up for lost time.  The `delta' value is
     added to the value of `playclock_offset', if we are in playmode.  If
     not, it doesn't matter.  Doing things in this way allows the playclock
     adjustments to be broadcast to all windows so that they can remain
     roughly in sync. */
// ----------------------------------------------------------------------------
public: // Public member functions which change the appearance
  bool get_max_scroll_view_size(NSSize &size);
    /* This function returns false and does nothing unless there is a valid
     rendering configuration installed.  If there is, the function returns
     true after setting `size' to the maximum size that the scroll-view's
     frame can take on. */
  void scroll_relative_to_view(float h_scroll, float v_scroll);
    /* This function causes the displayed region to scroll by the indicated
     amounts, expressed as a fraction of the dimensions of the currently
     visible region of the image. */
  void scroll_absolute(float h_scroll, float v_scroll, bool use_doc_coords);
    /* This function causes the displayed region o scroll by the indicated
     amount in the horizontal and vertical directions.  If `use_doc_coords'
     is false, the scrolling displacement is expressed relative to the
     canvas coordinate system used by `view_dims' and `image_dims'.  Otherwise
     the scrolling displacement is expressed relative to the coordinate
     system used by `doc_view', with `scroll_y' representing an upward
     rather than downward scrolling amount. */
  void set_focus_box(kdu_dims new_box);
    /* Called from the `doc_view' object when the user finishes entering a
     new focus box.  Note that the coordinates of `new_box' have already
     been converted to the same coordinate system as `view_dims',
     `buffer_dims' and `image_dims', using `convert_region_from_doc_view'. */  
  bool need_focus_outline(kdu_dims *focus_box);
    /* This function returns true if a focus box needs to be outlined by
     the `kdms_document_view::drawRect' function which is calling it,
     setting the current focus regin (in the `view_dims' coordinate system)
     into the supplied `focus_box' object. */
// ----------------------------------------------------------------------------
public: // Public member functions which are used for metadata
  void set_metadata_catalog_source(kdms_catalog *source);
    /* If `source' is nil, this function releases any existing
     `catalog_source' object.  Otherwise, the function installs the new
     object as the internal `catalog_source' member and parses JPX source
     data into it. */
  void edit_metadata_at_point(kdu_coords *point=NULL);
    /* If `point' is NULL, the function obtains the location at which to
     edit the metadata from the current mouse position.  Otherwise, `point'
     is interpreted with respect to the same coordinate system as
     `view_dims', `buffer_dims' and `image_dims'. */
  void edit_metanode(jpx_metanode node);
    /* Opens the metadata editor to edit the supplied node and any of its
     descendants.  This function may be called from the `kdms_metashow'
     object if the user clicks the edit button.  If the node is not
     directly editable, the function searches its descendants for a
     collection of top-level editable objects. */
  void show_imagery_for_metanode(jpx_metanode node);
    /* Changes the current view so as to best reveal the imagery associated
     with the supplied metadata node.  The function examines the node and
     its ancestors to find the first ROI (region of interest) node and the
     first numlist node it can.  If there is no numlist, but there is an ROI
     node, the ROI is associated with the first codestream.  If the numlist
     node's image entities do no intersect with what is being displayed
     or none of an ROI node's associated codestreams are being displayed, the
     display is changed to include at least one of the relevant image
     entities.  If there is an ROI node, the imagery is panned and zoomed
     in order to make the region clearly visible and the focus box is
     adjusted to fit around the ROI node's bounding box.
     Evidently, this is quite a powerful function.
     If `node' happens to be non-existent, the function does nothing. */
  jpx_meta_manager get_meta_manager()
    { return (!jpx_in)?jpx_meta_manager():jpx_in.access_meta_manager(); }
    /* Returns an empty interface unless there is an open JPX file.  This
     function is used by the `metashow' tool to cross-reference meta data
     which it directly parses with that which is currently maintained by
     the JPX meta-manager, possibly having been edited. */
  void metadata_changed(bool new_data_only);
    /* This function is called from within the metadata editor when changes
     are made to the metadata structure, which might need to be reflected in
     the overlay display and/or an open metashow window.  If `new_data_only'
     is true, no changes have been made to existing metadata. */
// ----------------------------------------------------------------------------
private: // Private implementation functions
  void invalidate_configuration();
    /* Call this function if the compositing configuration parameters
     (`single_component_idx', `single_codestream_idx', `single_layer_idx')
     are changed in any way.  The function removes all compositing layers
     from the current configuration installed into the `compositor' object.
     It sets the `configuration_complete' flag to false, the `bitmap'
     member to NULL, the image dimensions to 0, and posts a new client
     request to the `kdu_client' object if required.   After calling this
     function, you should generally call `initialize_regions'. */
  void initialize_regions();
    /* Called by `open_file' after first opening an image (or JPIP client
     session) and also whenever the composition is changed (e.g., a change
     in the JPX compositing layers which are to be used, or a change to
     single-component mode rendering).  In the former case, `first_time'
     will be set to true, meaning that the function should attempt to select
     a scale which fits the image roughly to the display (or a typical
     display). */
  void size_window_for_image_dims(bool can_enlarge_window);
    /* This function is called when `image_dims' may have changed,
     including when `image_dims' is set for the first time.  It adjusts
     the size of the `doc_view' contents rectangle to reflect the new image
     dimensions.  If `can_enlarge_window' is true, it tries to adjust the
     size of the containing window, so that the image can be fully displayed.
     Otherwise, it acts only to reduce the window size, if the window is
     too large for the image that needs to be displayed. This process
     should result in the issuing of a notification message that the bounds
     and/or frame of the `doc_view' object have changed, which ultimately
     results in a call to the `view_dims_changed' function, but we invoke
     that function explicitly from here anyway, just to be sure that
     `view_dims' gets set. */
  float increase_scale(float from_scale);
    /* This function uses `kdu_region_compositor::find_optimal_scale' to
     find a suitable scale for rendering the current focus or view which
     is at least a little (e.g. 30%) larger than `from_scale'. */
  float decrease_scale(float to_scale);
    /* Same as `increase_scale' but decreases the scale by at least a
     fixed small fraction (e.g. 30%), unless a known minimum scale
     factor is encountered. */
  void change_frame_idx(int new_frame_idx);
    /* This function is used to manipulate the `frame', `frame_idx',
     `frame_iteration' and `frame_instant' members.  It does not do anything
     else. */
  int find_compatible_frame_idx(int n_streams, const int *streams,
                                int n_layers, const int *layers,
                                int num_regions, const jpx_roi *regions,
                                int &compatible_codestream_idx,
                                int &compatible_layer_idx);
    /* This function is used when searching for a composited frame which
     matches a JPX metadata node, in the sense defined by the
     `show_imagery_for_metanode' function.
     [//]
     Starting from the current frame (if any), it scans through the frames
     in the JPX source, looking for one which matches at least one of the
     codestreams in `streams' (if there are any) and at least one of the
     compositing layers in `layers' (if there are any).  Moreover, if the
     `regions' array is non-empty, at least one of the regions must be
     visible, with respect to at least one of the codestreams in the
     `streams' array.  In any case, if a match is found and the `streams'
     array is non-empty, the matching codestream is written to the
     `compatible_codestream_idx' argument; otherwise, this argument is left
     untouched.  Similarly, if a match is found, the index of the matching
     compositing layer (the layer in the frame within which the match is
     found) is written to `compatible_layer_idx'.
     [//]
     If necessary, the function loops back to the first frame and continues
     searching until all frames have been examined.  If a match is found, the
     relevant frame index is returned.  Otherwise, the function returns -1. */  
  void convert_region_to_focus_anchors(kdu_dims region, kdu_dims ref_dims);
    /* This function finds the focus `focus_centre_x', `focus_centre_y',
     `focus_size_x' and `focus_size_y' from the supplied `region'.  The
     focus anchors express the centre and size of the region relative to
     the `ref_dims' canvas, within which it sits.  The function also sets
     `focus_anchors_known' to true. */
  void convert_focus_anchors_to_region(kdu_dims &region, kdu_dims ref_dims);
    /* This function is the reverse of the above one.  In particular, it
     applies the relative focus anchors `focus_centre_x', `focus_centre_y',
     `focus_size_x' and `focus_size_y' to the canvas supplied by `ref_dims',
     writing the resulting region into `region'. */
  bool adjust_focus_anchors();
    /* If `focus_codestream' and `focus_layer' indicate that the current
     focus anchors belong to a different type of image object to that
     which is currently installed in the `compositor', the focus anchors
     are adjusted from the old image object to the new wherever possible.
     The function returns true if focus anchors are enabled and any
     adjustment is made.  Otherwise, it returns false.  The function is
     invoked at the end of the `initialize_regions' cycle once the new
     compositor configuration has been determined. */
  void calculate_view_anchors();
    /* Call this function to determine the centre of the current view relative
     to the entire image, the centre of the focus box, if any, relative
     to the entire image, and the dimensions of the focus box, if any,
     relative to the dimensions of the entire image.  The function sets
     `view_centre_known' and `focus_anchors_known' to true.  The anchor
     coordinates and dimensions are used by subsequent calls to
     `scroll_to_view_anchors'. */
  void scroll_to_view_anchors();
    /* Adjust scrolling position so that the previously calculated view
       anchor is at the centre of the visible region, if possible. */
  void update_focus_box(bool view_may_be_scrolling=false);
    /* Called whenever a change in the focus box may be required.  This
     function calls `get_new_focus_box' and then repaints the region of
     the screen affected by any focus changes.  If the function is called
     in response to a change in the visible region, it is possible that
     a scrolling activity is in progress.  This should be signalled by
     setting the `view_may_be_scrolling' argument to true, since then any
     change in the focus box will require the entire view to be repainted.
     This is because the scrolling action moves portions of the surface
     and we cannot be sure in which order the focus box repainting and
     data moving will occur, since it is determined by the Cocoa framework. */
  kdu_dims get_new_focus_box();
    /* Performs most of the work of the `update_focus_box' and `set_focus_box'
     functions. */
  void display_status(int playmode_frame_idx=-1);
    /* Displays information in the status bar.  The particular type of
     information displayed depends upon the value of the `status_id'
     member variable.  The argument is used only in playmode, to provide
     the index of the frame which is currently being displayed,
     as opposed to `frame_idx', which is the frame currently being
     rendered. */
private: // Functions related to video playback
  void adjust_frame_queue_presentation_and_wakeup(double current_time);
    /* This function is called from `on_idle' and `wakeup' in playmode, to
     make whatever adjustments are required to the compositor's frame queue.
        It first determines how many frames can be safely popped from the head
     of the queue, so that there will always be at least one complete frame
     left for the frame presentation thread to present to the window.  These
     popped frames must all have expired.  If there is a newly generated
     frame (not yet pushed onto the queue), we may be able to pop all frames
     which are currently on the queue.
        It then attempts to pop the determined number of expired frames and
     push any newly generated frame onto the queue.  This might not be
     possible if the frame presenter is in the process of presenting a frame,
     since we cannot disturb the head of the frame queue in that case.
        If the above steps result in a new frame on the head of the queue,
     the function informs the `frame_presenter' of this new frame.  If we
     need to wait until the frame at the head of the queue expires before we
     can do any more rendering of new frames (because the queue is full), the
     function schedules a wakeup call for the frame at the head of the
     queue's expiry date.
        This function also takes responsibility for stopping playmode, in
     the event that the last frame in the sequence has been pushed onto the
     queue and all frames have passed their expiry date. */
  CFAbsoluteTime get_absolute_frame_end_time()
    { double rel_time;
      if (use_native_timing)
        rel_time = native_playback_multiplier *
          ((playmode_reverse)?(-frame_start):frame_end);
      else
        rel_time = custom_frame_interval *
          ((playmode_reverse)?(-frame_idx):(frame_idx+1));
      return rel_time + playclock_offset + playclock_base;
    }
    /* This is a utilty function to ensure that we always provide a consistent
     conversion from the track-relative frame times identified by the
     member variables `frame_start' and `frame_end' to absolute frame end
     times, as used during video playback.  The conversion depends upon the
     current playback settings (forward, reverse, frame rate multipliers,
     etc. */
  CFAbsoluteTime get_absolute_frame_start_time()
    { double rel_time;
      if (use_native_timing)
        rel_time = native_playback_multiplier *
          ((playmode_reverse)?(-frame_end):frame_start);
      else
        rel_time = custom_frame_interval *
          ((playmode_reverse)?(-frame_idx-1):frame_idx);
      return rel_time + playclock_offset + playclock_base;
    }
private: // Functions related to file saving  
  bool save_over(); // Saves over the existing file; returns false if failed.
  void save_as_jpx(const char *filename, bool preserve_codestream_links,
                   bool force_codestream_links);
  void save_as_jp2(const char *filename);
  void save_as_raw(const char *filename);
  void copy_codestream(kdu_compressed_target *tgt, kdu_codestream src);
  
//-----------------------------------------------------------------------------
// Member Variables
public: // Reference to window and document view objects.
  kdms_window *window;
  kdms_document_view *doc_view;
  NSScrollView *scroll_view;
  kdms_window_manager *window_manager; // For wakeup calls
  kdms_metashow *metashow; // NULL if metashow window not created yet
  kdms_catalog *catalog_source; // NULL if metadata catalog is not visible
  bool catalog_closed; // True only if catalog was explicitly closed
  int window_identifier; // Unique identifier, displayed as part of title
  bool have_unsaved_edits;
  
private: // File and URL names retained by the `window_manager'
  const char *open_file_pathname; // File we are reading; NULL for a URL
  const char *open_file_urlname; // JPIP URL we are reading; NULL if not JPIP
  const char *open_filename; // Points to filename within path or URL
  char *last_save_pathname; // Last file to which user saved anything 
  bool save_without_asking; // If the user does not want to be asked in future

private: // Compressed data source and rendering machinery
  bool allow_seeking;
  int error_level; // 0 = fast, 1 = fussy, 2 = resilient, 3 = resilient_sop
  kdu_simple_file_source file_in;
  jp2_family_src jp2_family_in;
  jpx_source jpx_in;
  mj2_source mj2_in;
  jpx_composition composition_rules; // Composition/rendering info if available
  kdms_region_compositor *compositor;
  kdu_thread_env thread_env; // Multi-threaded environment
  int num_threads; // If 0 use single-threaded machinery with `thread_env'=NULL
  int num_recommended_threads; // If 0, recommend single-threaded machinery
  bool processing_suspended;
  
private: // External file state management (for editing and saving changes)
  kdms_file_services *file_server; // Created on-demand for metadata editing

private: // Modes and status which may change while the image is open
  int max_display_layers;
  bool transpose, vflip, hflip; // Current geometry flags.
  float min_rendering_scale; // Negative if unknown
  float rendering_scale; // Supplied to `kdu_region_compositor::set_scale'
  int single_component_idx; // Negative when performing a composite rendering
  int max_components; // -ve if not known
  int single_codestream_idx; // Used with `single_component'
  int max_codestreams; // -ve if not known
  int single_layer_idx; // For rendering one layer at a time; -ve if not in use
  int max_compositing_layer_idx; // -ve if not known
  int frame_idx; // Index of current frame
  double frame_start; // Frame start time (secs) w.r.t. track/animation start
  double frame_end; // Frame end time (secs) w.r.t. track/animation start
  jx_frame *frame; // NULL, if unable to open the above frame yet
  int frame_iteration; // Iteration within `frame' object, if repeated
  int num_frames; // -ve if not known
  jpx_frame_expander frame_expander; // Used to find frame membership
  bool configuration_complete; // If required compositing layers are installed
  bool fit_scale_to_window; // Used when opening file for first time
  kds_status_id status_id; // Info currently displayed in status window
  
private: // State information for video/animation playback services
  bool in_playmode;
  bool playmode_repeat;
  bool playmode_reverse; // For playing backwards
  bool pushed_last_frame;
  bool use_native_timing;
  float custom_frame_interval; // Frame period if `use_native_timing' is false
  float native_playback_multiplier; // Amount to scale the native frame times
  CFAbsoluteTime playclock_base; // These 2 quantities (offset+base) are added
  double playclock_offset; // to relative frame times.  We keep a separate base
                   // to subtract from display times to find `kdu_long'
                   // time-stamps even if `kdu_long' is only a 32-bit integer.
  int playmode_frame_queue_size; // Max size of jitter-absorbtion queue
  int num_expired_frames_on_queue; // Num frames waiting to be popped
  kdms_frame_presenter *frame_presenter; // Paints frames from separate thread
  double last_status_update_time; // We only update status periodically.
  kdms_playmode_stats playmode_stats;
  
private: // Image and viewport dimensions
  int pixel_scale; // Number of display pels per image pel
  kdu_dims image_dims; // Dimensions & location of complete composed image
  kdu_dims buffer_dims; // Uses same coordinate system as `image_dims'
  kdu_dims view_dims; // Uses same coordinate system as `image_dims'
  float view_centre_x, view_centre_y;
  bool view_centre_known;

private: // Focus parameters
  bool enable_focus_box; // If disabled, focus box is identical to view port.
  bool highlight_focus; // If true, focus region is highlighted.
  bool focus_anchors_known; // See `calculate_view_anchors'.
  float focus_centre_x, focus_centre_y, focus_size_x, focus_size_y;
  int focus_codestream, focus_layer; // If both -ve, focus is w.r.t. frame
  kdu_dims focus_box; // Uses same coordinate system as `view_dims'.

private: // Overlay parameters
  bool overlays_enabled;
  bool overlay_flashing_enabled;
  int overlay_painting_intensity;
  int overlay_painting_colour;
  int overlay_size_threshold; // Min composited size for an overlay to be shown
  bool overlay_flash_wakeup_scheduled;

private: // Image buffer and painting resources
  kdu_compositor_buf *buffer; // Get from `compositor' object; NULL if invalid
  CGColorSpaceRef generic_rgb_space;
  kdms_data_provider app_paint_data_provider; // For app `paint_region' calls
  kdms_data_provider presentation_data_provider; // For presentation thread
                                                 // calls to `paint_region'

//-----------------------------------------------------------------------------
// File Menu Handlers
public:
  void menu_FileSave();
  bool can_do_FileSave(NSMenuItem *item)
    { return ((compositor != NULL) && have_unsaved_edits &&
              (open_file_pathname != NULL) && !mj2_in.exists()); }
  void menu_FileSaveAs(bool preserve_codestream_links,
                       bool force_codestream_links);
  bool can_do_FileSaveAs(NSMenuItem *item)
    { return (compositor != NULL) && !in_playmode; }
  void menu_FileSaveReload();
  bool can_do_FileSaveReload(NSMenuItem *item)
    { return ((compositor != NULL) && have_unsaved_edits &&
              (open_file_pathname != NULL) && (!mj2_in.exists()) &&
              (window_manager->get_open_file_retain_count(
                                             open_file_pathname) == 1) &&
              !in_playmode); }
  void menu_FileProperties();
  bool can_do_FileProperties(NSMenuItem *item)
    { return (compositor != NULL); }

public:
//-----------------------------------------------------------------------------
// View Menu Handlers
public:
  void menu_ViewHflip();
	bool can_do_ViewHflip(NSMenuItem *item)
    { return (buffer != NULL); }
  void menu_ViewVflip();
	bool can_do_ViewVflip(NSMenuItem *item)
    { return (buffer != NULL); }
  void menu_ViewRotate();
	bool can_do_ViewRotate(NSMenuItem *item)
    { return (buffer != NULL); }
  void menu_ViewCounterRotate();
	bool can_do_ViewCounterRotate(NSMenuItem *item)
    { return (buffer != NULL); }
  void menu_ViewZoomIn();
	bool can_do_ViewZoomIn(NSMenuItem *item)
    { return (buffer != NULL); }
  void menu_ViewZoomOut();
	bool can_do_ViewZoomOut(NSMenuItem *item)
    { return (buffer != NULL); }
  void menu_ViewWiden();
	bool can_do_ViewWiden(NSMenuItem *item)
    { return (buffer == NULL) ||
             ((view_dims.size.x < image_dims.size.x) ||
              (view_dims.size.y < image_dims.size.y)); }
  void menu_ViewShrink();
	bool can_do_ViewShrink(NSMenuItem *item)
    { return true; }
  void menu_ViewRestore();
	bool can_do_ViewRestore(NSMenuItem *item)
    { return (buffer != NULL) && (transpose || vflip || hflip); }
  void menu_ViewRefresh();
	bool can_do_ViewRefresh(NSMenuItem *item)
    { return (compositor != NULL); }
  void menu_ViewLayersLess();
	bool can_do_ViewLayersLess(NSMenuItem *item)
    { return (compositor != NULL) && (max_display_layers > 1); }
  void menu_ViewLayersMore();
	bool can_do_ViewLayersMore(NSMenuItem *item)
    { return (compositor != NULL) &&
             (max_display_layers <
              compositor->get_max_available_quality_layers()); }
  void menu_ViewOptimizeScale();
	bool can_do_ViewOptimizeScale(NSMenuItem *item)
    { return (compositor != NULL) && configuration_complete; }
  void menu_ViewPixelScaleX1();
	bool can_do_ViewPixelScaleX1(NSMenuItem *item)
    { [item setState:((pixel_scale==1)?NSOnState:NSOffState)];
      return true; }
  void menu_ViewPixelScaleX2();
	bool can_do_ViewPixelScaleX2(NSMenuItem *item)
    { [item setState:((pixel_scale==2)?NSOnState:NSOffState)];
      return true; }
  
  //-----------------------------------------------------------------------------
  // Focus Menu Handlers
public:
  void menu_FocusOff();
	bool can_do_FocusOff(NSMenuItem *item)
    { return enable_focus_box; }
  void menu_FocusHighlighting();
	bool can_do_FocusHighlighting(NSMenuItem *item)
    { [item setState:(highlight_focus?NSOnState:NSOffState)];
      return true; }
  void menu_FocusWiden();
	bool can_do_FocusWiden(NSMenuItem *item)
    { return enable_focus_box; }
  void menu_FocusShrink();
	bool can_do_FocusShrink(NSMenuItem *item)
    { return enable_focus_box; }
  void menu_FocusLeft();
	bool can_do_FocusLeft(NSMenuItem *item)
    { return enable_focus_box && (focus_box.pos.x > image_dims.pos.x); }
  void menu_FocusRight();
	bool can_do_FocusRight(NSMenuItem *item)
    { return enable_focus_box &&
      ((focus_box.pos.x+focus_box.size.x) <
       (image_dims.pos.x+image_dims.size.x)); }
  void menu_FocusUp();
	bool can_do_FocusUp(NSMenuItem *item)
    { return enable_focus_box && (focus_box.pos.y > image_dims.pos.y); }
  void menu_FocusDown();
	bool can_do_FocusDown(NSMenuItem *item)
    { return enable_focus_box &&
      ((focus_box.pos.y+focus_box.size.y) <
       (image_dims.pos.y+image_dims.size.y)); }
  
  //-----------------------------------------------------------------------------
  // Mode Menu Handlers
public:
  void menu_ModeFast();
	bool can_do_ModeFast(NSMenuItem *item)
    { [item setState:((error_level==0)?NSOnState:NSOffState)];
      return true; }
  void menu_ModeFussy();
	bool can_do_ModeFussy(NSMenuItem *item)
    { [item setState:((error_level==1)?NSOnState:NSOffState)];
      return true; }
  void menu_ModeResilient();
	bool can_do_ModeResilient(NSMenuItem *item)
    { [item setState:((error_level==2)?NSOnState:NSOffState)];
      return true; }
  void menu_ModeResilientSOP();
	bool can_do_ModeResilientSOP(NSMenuItem *item)
    { [item setState:((error_level==3)?NSOnState:NSOffState)];
      return true; }
  void menu_ModeSingleThreaded();
	bool can_do_ModeSingleThreaded(NSMenuItem *item)
    { [item setState:((num_threads==0)?NSOnState:NSOffState)];
      return true; }
  void menu_ModeMultiThreaded();
	bool can_do_ModeMultiThreaded(NSMenuItem *item); // See .mm file
  void menu_ModeMoreThreads();
	bool can_do_ModeMoreThreads(NSMenuItem *item)
    { return true; }
  void menu_ModeLessThreads();
	bool can_do_ModeLessThreads(NSMenuItem *item)
    { return (num_threads > 0); }
  void menu_ModeRecommendedThreads();
	bool can_do_ModeRecommendedThreads(NSMenuItem *item); // See .mm file
  
  //-----------------------------------------------------------------------------
  // Navigation Menu Handlers
public:  
  void menu_NavComponent1();
	bool can_do_NavComponent1(NSMenuItem *item)
    { [item setState:(((compositor != NULL) &&
                       (single_component_idx==0))?NSOnState:NSOffState)];
      return (compositor != NULL) && (single_component_idx != 0); }
  void menu_NavMultiComponent();
	bool can_do_NavMultiComponent(NSMenuItem *item)
    {
      [item setState:(((compositor != NULL) &&
                       (single_component_idx < 0) &&
                       ((single_layer_idx < 0) || (num_frames==0) || !jpx_in))?
                      NSOnState:NSOffState)];
      return (compositor != NULL) &&
             ((single_component_idx >= 0) ||
              ((single_layer_idx >= 0) && (num_frames != 0) &&
               jpx_in.exists()));
    }
  void menu_NavComponentNext();
	bool can_do_NavComponentNext(NSMenuItem *item)
  { return (compositor != NULL) &&
           ((max_components < 0) ||
            (single_component_idx < (max_components-1))); }
  void menu_NavComponentPrev();
	bool can_do_NavComponentPrev(NSMenuItem *item)
    { return (compositor != NULL) && (single_component_idx != 0); }
  void menu_NavImageNext();
	bool can_do_NavImageNext(NSMenuItem *item); // Lengthy decision in .mm file
  void menu_NavImagePrev();
	bool can_do_NavImagePrev(NSMenuItem *item); // Lengthy decision in .mm file
  void menu_NavCompositingLayer();
	bool can_do_NavCompositingLayer(NSMenuItem *item)
    { [item setState:(((compositor != NULL) &&
                       (single_layer_idx >= 0))?NSOnState:NSOffState)];
      return (compositor != NULL) && (single_layer_idx < 0); }
  void menu_NavTrackNext();
	bool can_do_NavTrackNext(NSMenuItem *item)
    { return (compositor != NULL) && mj2_in.exists(); }

  //-----------------------------------------------------------------------------
  // Metadata Menu Handlers
public:
  void menu_MetadataCatalog();
  bool can_do_MetadataCatalog(NSMenuItem *item)
    { [item setState:((catalog_source)?NSOnState:NSOffState)];
      return (catalog_source || jpx_in.exists()); }
  void menu_MetadataShow();
	bool can_do_MetadataShow(NSMenuItem *item)
    { [item setState:((metashow != nil)?NSOnState:NSOffState)];
      return true; }
  void menu_MetadataAdd();
  bool can_do_MetadataAdd(NSMenuItem *item)
    { return (compositor != NULL) && jpx_in.exists(); }
  void menu_MetadataEdit();
  bool can_do_MetadataEdit(NSMenuItem *item)
    { return (compositor != NULL) && jpx_in.exists(); }
  void menu_OverlayEnable();
  bool can_do_OverlayEnable(NSMenuItem *item)
    { [item setState:(((compositor != NULL) && jpx_in.exists() &&
                       overlays_enabled)?NSOnState:NSOffState)];
      return (compositor != NULL) && jpx_in.exists(); } 
  void menu_OverlayFlashing();
  bool can_do_OverlayFlashing(NSMenuItem *item)
    { [item setState:(((compositor != NULL) && jpx_in.exists() &&
                       overlay_flashing_enabled &&
                       overlays_enabled)?NSOnState:NSOffState)];
      return (compositor != NULL) && jpx_in.exists() && overlays_enabled &&
             !in_playmode; }  
  void menu_OverlayToggle();
  bool can_do_OverlayToggle(NSMenuItem *item)
    { return (compositor != NULL) && jpx_in.exists(); }
  void menu_OverlayBrighter();
  bool can_do_OverlayBrighter(NSMenuItem *item)
    { return (compositor != NULL) && jpx_in.exists() && overlays_enabled; }
  void menu_OverlayDarker();
  bool can_do_OverlayDarker(NSMenuItem *item)
    { return (compositor != NULL) && jpx_in.exists() && overlays_enabled; }
  void menu_OverlayDoubleSize();
  bool can_do_OverlayDoubleSize(NSMenuItem *item)
    { return (compositor != NULL) && jpx_in.exists() && overlays_enabled; }
  void menu_OverlayHalveSize();
  bool can_do_OverlayHalveSize(NSMenuItem *item)
    { return (compositor != NULL) && jpx_in.exists() && overlays_enabled &&
      (overlay_size_threshold > 1); }
  
  //-----------------------------------------------------------------------------
  // Play Menu Handlers
public:
  bool source_supports_playmode()
    { return (compositor != NULL) &&
      (mj2_in.exists() || jpx_in.exists()) &&
      (num_frames != 0) && (num_frames != 1); }
  void menu_PlayStartForward() { start_playmode(false); }
	bool can_do_PlayStartForward(NSMenuItem *item)
    { return source_supports_playmode() &&
      (single_component_idx < 0) &&
      ((single_layer_idx < 0) || !jpx_in.exists()) &&
      (playmode_reverse || !in_playmode); }
  void menu_PlayStartBackward() { start_playmode(true); }
	bool can_do_PlayStartBackward(NSMenuItem *item)
    { return source_supports_playmode() &&
      (single_component_idx < 0) &&
      ((single_layer_idx < 0) || !jpx_in.exists()) &&      
      !(playmode_reverse && in_playmode); }
  void menu_PlayStop() { stop_playmode(); }
	bool can_do_PlayStop(NSMenuItem *item)
    { return in_playmode; }
  void menu_PlayRepeat() { playmode_repeat = !playmode_repeat; }
  bool can_do_PlayRepeat(NSMenuItem *item)
    { [item setState:((playmode_repeat)?NSOnState:NSOffState)];
      return source_supports_playmode(); }
  void menu_PlayNative();
  bool can_do_PlayNative(NSMenuItem *item);
  void menu_PlayCustom();
  bool can_do_PlayCustom(NSMenuItem *item);  
  void menu_PlayCustomRateUp();
  bool can_do_PlayCustomRateUp(NSMenuItem *item)
    { return source_supports_playmode(); }
  void menu_PlayCustomRateDown();
  bool can_do_PlayCustomRateDown(NSMenuItem *item)
    { return source_supports_playmode(); }
  void menu_PlayNativeRateUp();
  bool can_do_PlayNativeRateUp(NSMenuItem *item) 
    { return source_supports_playmode(); }
  void menu_PlayNativeRateDown();
  bool can_do_PlayNativeRateDown(NSMenuItem *item)
    { return source_supports_playmode(); }
  
  //-----------------------------------------------------------------------------
  // Status Menu Handlers
public:  
  void menu_StatusToggle();
	bool can_do_StatusToggle(NSMenuItem *item)
    { return (compositor != NULL); }
};
