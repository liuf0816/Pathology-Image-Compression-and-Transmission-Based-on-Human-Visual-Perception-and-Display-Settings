/*****************************************************************************/
// File: kdms_controller.h [scope = APPS/MACSHOW]
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
   Defines the main controller object for the interactive JPEG2000 viewer,
"kdu_macshow".
******************************************************************************/

#import <Cocoa/Cocoa.h>
#include "kdu_compressed.h"

// Defined elsewhere:
@class kdms_window;
class kdms_renderer;

// Defined here:
class kdms_frame_presenter;
class kdms_window_manager;
@class kdms_controller;

/*****************************************************************************/
/*                              EXTERNAL FUNCTIONS                           */
/*****************************************************************************/

extern bool kdms_compare_file_pathnames(const char *name1, const char *name2);
  /* Returns true if `name1' and `name2' refer to the same file.  If a simple
   string comparison returns false, the function converts both names to
   file system references, if possible, and performs the comparison on
   the references.  This helps minimize the risk of overwriting an existing
   file which the application is using. */

/*****************************************************************************/
/* CLASS                         kdms_frame_presenter                        */
/*****************************************************************************/

class kdms_frame_presenter {
      /* There is a unique frame presenter for each window managed by the
       `kdms_window_manager' object. */
public: // Member functions used by the `window_manager'
  kdms_frame_presenter(kdms_window_manager *window_manager, kdms_window *wnd,
                       int min_drawing_interval);
    /* The `min_drawing_interval' argument dictates the maximum rate at which
     the view will be painted.  The `draw_pending_frame' function is called
     regularly from a timed loop in the presentation thread.  The function
     will not actually paint any frame data to the screen if it has
     previously done so in any of the I-1 previous calls, where I is the
     value of `min_drawing_interval'. */
  ~kdms_frame_presenter();
  bool draw_pending_frame(CFRunLoopRef main_app_run_loop);
    /* Called from the presentation thread's run-loop at a regular rate.
     This function draws any frame previously registered by `present_frame',
     returning true if anything is drawn.  If requested, the main application's
     run-loop is sent a wakeup message after frame drawing completes. */
public: // Member functions used by the application thread
  void present_frame(kdms_renderer *painter, int frame_idx)
  {
    wake_app_once_painting_completes=false; // Only the app can write this flag
    this->painter = painter;
    this->frame_to_paint = frame_idx;
  }
    /* This function indicates that a frame (with the indicated index) is
     available for painting.  If the frame has already been painted, this call
     has no effect.  Otherwise, the presentation thread invokes
     the `painter->paint_view_from_presentation_thread' function at the
     next reasonable opportunity, considering reasonable maximum painting
     rates and the screen refresh rate. */
  bool disable();
    /* This function attempts to cancel any outstanding frame presentation
     request, so that the head of the `kdu_region_compositor' object's
     composition queue can be manipulated by calls to
     `kdu_region_compositor::pop_composition_buffer'.
     If the presentation thread is in the process of painting a frame,
     the function returns false immediately, without blocking.  In this case,
     it is not yet safe for the caller to manipulate the composition
     queue, but the object promises to wake up the main application thread
     once the presentation process is complete, so that the caller's
     `kdms_window::on_idle' function will eventually be called and get
     a chance to call the present function again. */
  void reset();
    /* Similar to `disable', except that this function blocks the caller
     until frame presentation can be firmly cancelled.  Moreover, once this
     is done, the `last_frame_painted' member is reset to a ridiculous value,
     so that any subsequent calls to `present_frame' will not be blocked from
     presenting the frame they provide. */
private: // Data
  kdms_window_manager *window_manager;
  kdms_window *window;
  NSGraphicsContext *graphics_context;
  kdu_mutex drawing_mutex; // Locked while the frame is being drawn
private: // State variables written only by the `app' thread
  kdms_renderer *painter; // If NULL, there is nothing to paint 
  bool wake_app_once_painting_completes; // If `disable' call failed
  int frame_to_paint; // Index of the frame waiting to be painted
private: // State variables written only by the presentation thread
  int min_drawing_interval; // Number of times `draw_pending_frame' must be
           // called after a view is painted, before it will be painted again.
  int drawing_downcounter; // Implements counter for `min_drawing_interval'
  int last_frame_painted; // Used to avoid painting a frame multiple times
};

/*****************************************************************************/
/* STRUCT                         kdms_window_list                           */
/*****************************************************************************/

struct kdms_window_list {
  kdms_window *wnd;
  int window_identifier; // See `kdms_window_manager::get_window_identifier'
  kdms_renderer *wakeup_obj; // Non-NULL only if there is a wakeup is scheduled
  CFAbsoluteTime wakeup_time; // Relevant only if `wakeup_obj' is non-NULL
  kdms_frame_presenter *frame_presenter;
  kdms_window_list *next;
  kdms_window_list *prev;
};

/*****************************************************************************/
/* STRUCT                     kdms_open_file_record                          */
/*****************************************************************************/

struct kdms_open_file_record {
  kdms_open_file_record()
    {
      open_pathname = save_pathname = NULL;
      retain_count = 0; next = NULL;
    }
  ~kdms_open_file_record()
    {
      if (open_pathname != NULL) delete[] open_pathname;
      if (save_pathname != NULL) delete[] save_pathname;
    }
  int retain_count;
  char *open_pathname;
  char *save_pathname; // Non-NULL if there is a valid saved file which needs
                       // to replace the existing file before closing.
  kdms_open_file_record *next;
};

/*****************************************************************************/
/* CLASS                        kdms_window_manager                          */
/*****************************************************************************/

class kdms_window_manager {
public: // Startup/shutdown member functions
  kdms_window_manager();
    /* Note: it is not safe to delete this object explicitly; since the
     `menuAppQuit' message may be received in the controller while
     `run_loop_callback' is testing for user events which need
     to be interleaved with decompression processing.  Thus,
     the `kdms_controller::menuAppQuit' function should terminate
     the application without explicitly deleting the window manager,
     leaving the operating system to clean it up. */
  void configure_presentation_manager();
    /* Called from the presentation thread, right after it is launched,
     before its run-loop is entered.  This gives the object a chance to
     install a timer and callback function to manage period frame
     presentation events. */
  bool application_can_terminate();
    /* Sends `application_can_terminate' messages to each window in turn
     until a false return is obtained, in which case the function returns
     false.  If all windows are happy to terminate, the function returns
     true. */
  void send_application_terminating_messages();
    /* Sends `application_terminating' messages to all windows. */
// ---------------------------------------------------------------------------
public: // Window list manipulation functions
  void add_window(kdms_window *wnd);
  void remove_window(kdms_window *wnd);
  kdms_window *access_window(int idx);
    /* Retrieve the idx'th window in the list, starting from idx=0. */
  int get_window_identifier(kdms_window *wnd);
    /* Retrieves the integer identifier which is associated with the
     indicated window (0 if the window cannot be found).  The identifier
     is currently set equal to the number of `add_window' calls which
     occurred prior to and including the one which added this window. */
  void place_window(kdms_window *wnd, NSSize frame_size);
    /* Place the window at a good location. */
  void reset_placement_engine();
    /* Resets the placement engine so that new window placement operations
     will start again from the top-left corner of the screen. */
// ---------------------------------------------------------------------------
public: // Menu action broadcasting functions
  kdms_window *get_next_action_window(kdms_window *caller);
    /* Called from within window-specific menu action handlers to determine the
     next window, if any, to which the menu action should be passed.  The
     function returns nil if there is none (the normal situation).  The
     function may be called recursively.  It knows how to prevent indefinite
     recursion by identifying the key window (the one which should have
     received the menu action call in the first place.  If there is no
     key window when the function is called and the caller is not the
     key window, the function always returns nil for safety. */
  void set_action_broadcasting(bool broadcast_once,
                               bool broadcast_indefinitely);
    /* This function is used to configure the behavior of calls to
     `get_next_action_window'.  If both arguments are false, the latter
     function will always return nil.  If `broadcast_once' is false, the
     `get_next_action_window' function will return each window in turn for
     one single cycle.  If `broadcast_indefinitely' is true, the function
     will work to broadcast all menu actions to all windows. */
  bool is_broadcasting_actions_indefinitely()
    { return broadcast_actions_indefinitely; }
  void broadcast_playclock_adjustment(double delta);
    /* Called from any window in playback mode, which is getting behind its
     desired playback rate.  This function makes adjustments to all window's
     play clocks so that they can remain roughly in sync. */
// ---------------------------------------------------------------------------
public: // Timer scheduling functions
  void schedule_wakeup(kdms_window *wnd, kdms_renderer *wakeup_obj,
                       CFAbsoluteTime time);
    /* Schedules a wakeup call for the supplied window at the indicated
     time.  A call to `wakeup_obj->wakeup' will be executed at this time (or
     shortly after) passing the same value of `time'.  At most one wakeup
     time may be maintained for each window, as identified by the `wnd'
     argument, so this function may change any previously installed wakeup
     time.  All wakeup times are managed internally to this object by a single
     run-loop timer object, so as to minimize overhead and encourage
     synchronization of frame playout times where there are multiple
     windows.
        If the `time' has already passed, this function will not call the
     `wakeup_obj->wakeup' function immediately.  This is a safety measure
     to prevent unbounded recursion in case `schedule_wakeup' is invoked
     from within the `wakeup' function itself (a common occurrence).
     Instead, the `wakeup' call will be made once the threads run-loop
     gets control back again and invokes the `timer_callback' function.
        If the `wakeup_obj' argument is NULL, this function simply cancels
     any pending wakeup call for the window. */
  kdms_frame_presenter *get_frame_presenter(kdms_window *wnd);
    /* Returns the frame presenter object associated with the window, for
     use in presenting live video frames efficiently, in the background
     presentation thread. */
// ---------------------------------------------------------------------------
public: // Management of image files for safe sharing
  const char *retain_open_file_pathname(const char *file_pathname);
    /* This function declares that a window is about to open a file whose
     name is supplied as `file_pathname'.  If the file is already opened
     by another window, its retain count is incremented.  Otherwise, a
     new internal record of the file pathname is made.  In any case, the
     returned pointer corresponds to the internal file pathname buffer
     managed by this object, which saves the caller from having to
     copy the file to its own persistent storage. */
  void release_open_file_pathname(const char *file_pathname);
    /* Releases a file previously retained via `retain_open_file_pathname'.
     If a temporary file has previously been used to save over an existing
     open file, and the retain count reaches 0, this function deletes the
     original file and replaces it with the temporary file. */
  const char *get_save_file_pathname(const char *file_pathname);
    /* This function is used to avoid overwriting open files when trying
     to save to an existing file.  The pathname of the file you want to
     save to is supplied as the argument.  The function either returns
     that same pathname (without copying it to an internal buffer) or else
     it returns a temporary pathname that should be use instead, remembering
     to move the temporary file into the original file once its retain
     count reaches zero, as described above in connection with the
     `release_open_file_pathname' function. */
  void declare_save_file_invalid(const char *file_pathname);
    /* This function is called if an attempt to save failed.  You supply
     the same pathname supplied originally by `get_save_file_pathname' (even
     if that was just the pathname you passed into the function).  The file
     is deleted and, if necessary, any internal reminder to copy that file
     over the original once the retain count reaches zero is removed. */
  int get_open_file_retain_count(const char *file_pathname);
    /* Returns the file's retain count. */
  bool check_open_file_replaced(const char *file_pathname);
    /* Returns false if the supplied file pathname already has an alternate
     save pathname, which will be used to replace the file once its retain
     count reaches zero, as explained in connection with the
     `release_open_file_pathname' function. */
// ---------------------------------------------------------------------------
public: // Static callback functions
  static void
    run_loop_callback(CFRunLoopObserverRef observer,
                      CFRunLoopActivity activity, void *info);
  static void
    timer_callback(CFRunLoopTimerRef timer, void *info);
  static void
    presentation_timer_callback(CFRunLoopTimerRef timer, void *info);
// ---------------------------------------------------------------------------
private: // Helper functions
  void install_next_scheduled_wakeup();
    /* Scans the window list to find the next window which requires a wakeup
     call.  If the time has already passed, executes its wakeup function
     immediately and continue to scan; otherwise, sets the timer for future
     wakeup.  This function attempts to execute any pending wakeup calls
     in order. */
// ---------------------------------------------------------------------------
private: // Window management
  kdms_window_list *windows; // Head of linked list of windows
  int next_window_identifier;
  kdms_window_list *next_idle_window; /* Points to next window to scan for
     idle-time processing.  NULL if we should start scanning from the
     start of the list next time `run_loop_callback' is called. */
  kdms_window *last_known_key_wnd; // nil if none is known to be key
  bool broadcast_actions_once;
  bool broadcast_actions_indefinitely;
private: // Auto-placement information; these quantities are expressed in
         // screen coordinates, using the "vertical starts at top" convention
  kdu_coords next_window_pos; // For next window to be placed on current row
  int next_window_row; // Start of next row of windows
  kdu_coords cycle_origin; /* Origin of current placement cycle. */
private: // Information for timed wakeups
  kdms_window_list *next_window_to_wake;
  bool will_check_best_window_to_wake; // This flag is set while in (or about
         // to call `install_next_scheduled_wakeup'.  In this case, a call
         // to `schedule_wakeup' should not try to determine the next window
         // to wake up by itself.
  CFRunLoopTimerRef timer;
private: // Run-loop observer
  CFRunLoopObserverRef main_observer;
private: // Data required to manage the presentation thread
  CFRunLoopRef main_app_run_loop; // So frame presenters can wake the main app
  CFRunLoopTimerRef presentation_timer;
  int min_presentation_drawing_interval;
  kdu_mutex window_list_change_mutex; // Locked by the main thread before
         // changing the window list.  Locked by the presentation thread
         // before scanning the window list for windows whose frame presenter
         // needs to be serviced.
private: // Data required to safely manage open files in the face of saving
  kdms_open_file_record *open_file_list;
};

/*****************************************************************************/
/* INTERFACE                      kdms_controller                            */
/*****************************************************************************/

@interface kdms_controller : NSObject
{
  NSCursor *cursors[2];
  kdms_window_manager *window_manager;
}

// ----------------------------------------------------------------------------
// Startup member functions
- (void)awakeFromNib;
- (void)presentationThreadEntry:(id)param;

// ----------------------------------------------------------------------------
// Application delegated functions
- (BOOL)application:(NSApplication *)app openFile:(NSString *)filename;
- (void)application:(NSApplication *)app openFiles:(NSArray *)filenames;

// ----------------------------------------------------------------------------
// Menu functions
- (IBAction)menuWindowNew:(NSMenuItem *)sender;
- (IBAction)menuWindowArrange:(NSMenuItem *)sender;
- (IBAction)menuWindowBroadcastOnce:(NSMenuItem *)sender;
- (IBAction)menuWindowBroadcastIndefinitely:(NSMenuItem *)sender;
- (IBAction)menuFileOpenNew:(NSMenuItem *)sender;
- (IBAction)menuAppQuit:(NSMenuItem *)sender;

- (BOOL) validateMenuItem:(NSMenuItem *)menuitem;
 
@end

