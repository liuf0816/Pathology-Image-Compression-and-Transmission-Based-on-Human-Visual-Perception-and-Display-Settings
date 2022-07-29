/*****************************************************************************/
// File: kdms_window.h [scope = APPS/MACSHOW]
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
   Defines the `kdms_window' and `kdms_document_view' Objective-C classes,
which manage most of the Cocoa interaction for an single image/video source.
The rendering aspects are handled by the `kdms_renderer' object.  There is
always a single `kdms_renderer' object for each `kdms_window'.
******************************************************************************/

#import <Cocoa/Cocoa.h>
#import "kdms_renderer.h"
#import "kdms_controller.h"

#define KDMS_FOCUS_MARGIN 2.0F // Added to boundaries of focus region, in
  // screen space, to determine a region which safely covers the
  // focus box and any bounding outline.

/*****************************************************************************/
/*                             kdms_document_view                            */
/*****************************************************************************/

@interface kdms_document_view : NSView {
  kdms_renderer *renderer;
  kdms_window *wnd;
  bool is_focussing; // If a new focus box is being delineated
  bool is_dragging; // If a mouse-controlled panning operation is in progress
  NSPoint mouse_start; // Point at which focus commenced
  NSPoint mouse_last; // Point last reached by dragging mouse during focus
}
- (id)initWithFrame:(NSRect)frame;
- (void)set_renderer:(kdms_renderer *)renderer;
- (void)set_owner:(kdms_window *)owner;
- (BOOL)isOpaque;
- (void)drawRect:(NSRect)rect;
- (void)start_focusbox_at:(NSPoint)point;
     /* Called when the left mouse button is pressed without shift.
      This initiates the definition of a new focus box. */
- (void)start_viewdrag_at:(NSPoint)point;
     /* Called when the left mouse button is pressed with the shift key.
      This initiates a panning operation which is driven by mouse drag
      events. */
- (void)cancel_focus_drag;
     /* Called whenever a key is pressed.  This cancels any focus box
      delineation or mouse drag based panning operations. */
- (void)end_focus_drag_at:(NSPoint)point;
     /* Called when the left mouse button is released.  This ends any
      focus box delineation or mouse drag based panning operations. */
- (void)mouse_drag_to:(NSPoint)point;
     /* Called when a mouse drag event occurs with the left mouse button
      down.  This affects any current focus box delineation or view
      panning operations initiated by the `start_focusbox_at' or
      `start_viewdrag_at' functions, if they have not been cancelled. */
- (NSRect)rectangleFromMouseStartToPoint:(NSPoint)point;
      /* Returns the rectangle formed between `mouse_start' and `point'. */
- (void)invalidate_mouse_box;
      /* Utility function called from within the various focus/drag functions
       above, in order o invalidate the rectangle defined by `mouse_start'
       through `mouse_last', so that it will be redrawn. */
- (void)handle_double_click:(NSPoint)point;
      /* Called when a mouse double-click occurs inside the window.  As with
       the other functions above, `point' is a location expressed relative to
       the present document view. */
//-----------------------------------------------------------------------------
// User Event Override Functions
- (BOOL)acceptsFirstResponder;
- (void)keyDown:(NSEvent *)key_event;
- (void)mouseDown:(NSEvent *)mouse_event;
- (void)mouseDragged:(NSEvent *)mouse_event;
- (void)mouseUp:(NSEvent *)mouse_event;

@end

/*****************************************************************************/
/*                               kdms_window                                 */
/*****************************************************************************/

@interface kdms_window : NSWindow {
  kdms_window_manager *window_manager;
  kdms_renderer *renderer;
  kdms_document_view *doc_view;
  NSCursor *doc_cursor_normal; // Obtained from the controller
  NSCursor *doc_cursor_dragging; // Obtained from the controller
  NSScrollView *scroll_view;
  NSView *status_bar; // Contains the status panes
  NSTextField *status_panes[3];
  float status_height; // Height of status panes, measured in pixels
  int status_pane_lengths[3]; // Num chars currently in each status pane + 1
  char *status_pane_caches[3]; // Caches current string contents of each pane
  char status_pane_cache_buf[256]; // Provides storage for the above array
  NSScrollView *catalog_panel; // Created on demand for metadata catalog
  NSOutlineView *catalog_view; // Created on demand for metadata catalog
  float catalog_panel_width; // Positioned on the right of main scroll view
}

//-----------------------------------------------------------------------------
// Initialization/Termination Functions
- (void)initWithManager:(kdms_window_manager *)manager
             andCursors:(NSCursor **)cursors;
    /* Initializes a newly allocated window.  The `cursors' array holds
     pointers to two cursors (normal and dragging), which are shared
     by all windows. */
- (void)dealloc; // Removes us from the `controller's list
- (void)open_file:(NSString *)filename;
- (bool)application_can_terminate;
     // Used by the window manager to see if the application should be allowed
     // to quit at this point in time -- typically, if there are unsaved
     // edits, the implementation will interrogate the user in order to
     // decide.
- (void)application_terminating;
- (void)close;

//-----------------------------------------------------------------------------
// Functions used to route information from the window manager to the renderer
- (bool)on_idle; // Calls `renderer->on_idle'.
- (void)adjust_playclock:(double)delta; // Calls `renderer->adjust_playclock'
//-----------------------------------------------------------------------------
// Utility Functions for managed objects
- (void)placeUsingManagerWithFrameSize:(NSSize)frame_size;
    // Called by the `kdms_renderer' object when it first opens an image.
    // The supplied `frame_size' will fit within the screen; the position
    // at which the window is to be placed is being left to the
    // `kdms_window_manager' object to decide.
- (void)create_metadata_catalog;
    // Called from the main menu or from within `kdms_renderer' to create
    // the `catalog_panel' and associated objects to manage a metadata
    // catalog.  Also invokes `renderer->set_metadata_catalog_source'.
- (void)remove_metadata_catalog;
    // Called from the main menu or from within `kdms_renderer' to remove
    // the `catalog_panel' and associated objects.  Also invokes
    // `renderer->set_metadata_catalog_source' with a NULL argument.
- (void)set_status_strings:(char **)three_strings;
    // Takes an array of three character strings, whose contents
    // are written to the left, centre and right status bar panels.
    // Use NULL of an empty string for a panel you want to leave empty
- (float)get_bottom_margin_height; // Return height of status bar
- (float)get_right_margin_width; // Return width of any active catalog panel
- (void) set_drag_cursor:(BOOL)is_dragging;
    // If yes, a cursor is shown to indicate dragging of the view around.

//-----------------------------------------------------------------------------
// Menu Functions
- (IBAction) menuFileOpen:(NSMenuItem *)sender;
- (IBAction) menuFileSave:(NSMenuItem *)sender;
- (IBAction) menuFileSaveAs:(NSMenuItem *)sender;
- (IBAction) menuFileSaveAsLinked:(NSMenuItem *)sender;
- (IBAction) menuFileSaveAsEmbedded:(NSMenuItem *)sender;
- (IBAction) menuFileSaveReload:(NSMenuItem *)sender;
- (IBAction) menuFileProperties:(NSMenuItem *)sender;

- (IBAction) menuViewHflip:(NSMenuItem *)sender;
- (IBAction) menuViewVflip:(NSMenuItem *)sender;
- (IBAction) menuViewRotate:(NSMenuItem *)sender;
- (IBAction) menuViewCounterRotate:(NSMenuItem *)sender;
- (IBAction) menuViewZoomIn:(NSMenuItem *)sender;
- (IBAction) menuViewZoomOut:(NSMenuItem *)sender;
- (IBAction) menuViewWiden:(NSMenuItem *)sender;
- (IBAction) menuViewShrink:(NSMenuItem *)sender;
- (IBAction) menuViewRestore:(NSMenuItem *)sender;
- (IBAction) menuViewRefresh:(NSMenuItem *)sender;
- (IBAction) menuViewLayersLess:(NSMenuItem *)sender;
- (IBAction) menuViewLayersMore:(NSMenuItem *)sender;
- (IBAction) menuViewOptimizeScale:(NSMenuItem *)sender;
- (IBAction) menuViewPixelScaleX1:(NSMenuItem *)sender;
- (IBAction) menuViewPixelScaleX2:(NSMenuItem *)sender;

- (IBAction) menuFocusOff:(NSMenuItem *)sender;
- (IBAction) menuFocusHighlighting:(NSMenuItem *)sender;
- (IBAction) menuFocusWiden:(NSMenuItem *)sender;
- (IBAction) menuFocusShrink:(NSMenuItem *)sender;
- (IBAction) menuFocusLeft:(NSMenuItem *)sender;
- (IBAction) menuFocusRight:(NSMenuItem *)sender;
- (IBAction) menuFocusUp:(NSMenuItem *)sender;
- (IBAction) menuFocusDown:(NSMenuItem *)sender;

- (IBAction) menuModeFast:(NSMenuItem *)sender;
- (IBAction) menuModeFussy:(NSMenuItem *)sender;
- (IBAction) menuModeResilient:(NSMenuItem *)sender;
- (IBAction) menuModeResilientSOP:(NSMenuItem *)sender;
- (IBAction) menuModeSingleThreaded:(NSMenuItem *)sender;
- (IBAction) menuModeMultiThreaded:(NSMenuItem *)sender;
- (IBAction) menuModeMoreThreads:(NSMenuItem *)sender;
- (IBAction) menuModeLessThreads:(NSMenuItem *)sender;
- (IBAction) menuModeRecommendedThreads:(NSMenuItem *)sender;

- (IBAction) menuNavComponent1:(NSMenuItem *)sender;
- (IBAction) menuNavMultiComponent:(NSMenuItem *)sender;
- (IBAction) menuNavComponentNext:(NSMenuItem *)sender;
- (IBAction) menuNavComponentPrev:(NSMenuItem *)sender;
- (IBAction) menuNavImageNext:(NSMenuItem *)sender;
- (IBAction) menuNavImagePrev:(NSMenuItem *)sender;
- (IBAction) menuNavCompositingLayer:(NSMenuItem *)sender;
- (IBAction) menuNavTrackNext:(NSMenuItem *)sender;

- (IBAction) menuMetadataCatalog:(NSMenuItem *)sender;
- (IBAction) menuMetadataShow:(NSMenuItem *)sender;
- (IBAction) menuMetadataAdd:(NSMenuItem *)sender;
- (IBAction) menuMetadataEdit:(NSMenuItem *)sender;
- (IBAction) menuOverlayEnable:(NSMenuItem *)sender;
- (IBAction) menuOverlayFlashing:(NSMenuItem *)sender;
- (IBAction) menuOverlayToggle:(NSMenuItem *)sender;
- (IBAction) menuOverlayBrighter:(NSMenuItem *)sender;
- (IBAction) menuOverlayDarker:(NSMenuItem *)sender;
- (IBAction) menuOverlayDoubleSize:(NSMenuItem *)sender;
- (IBAction) menuOverlayHalveSize:(NSMenuItem *)sender;

- (IBAction) menuPlayStartForward:(NSMenuItem *)sender;
- (IBAction) menuPlayStartBackward:(NSMenuItem *)sender;
- (IBAction) menuPlayStop:(NSMenuItem *)sender;
- (IBAction) menuPlayRepeat:(NSMenuItem *)sender;
- (IBAction) menuPlayNative:(NSMenuItem *)sender;
- (IBAction) menuPlayCustom:(NSMenuItem *)sender;
- (IBAction) menuPlayCustomRateUp:(NSMenuItem *)sender;
- (IBAction) menuPlayCustomRateDown:(NSMenuItem *)sender;
- (IBAction) menuPlayNativeRateUp:(NSMenuItem *)sender;
- (IBAction) menuPlayNativeRateDown:(NSMenuItem *)sender;

- (IBAction) menuStatusToggle:(NSMenuItem *)sender;

- (BOOL) validateMenuItem:(NSMenuItem *)menuitem;

//-----------------------------------------------------------------------------
// Notification Functions
- (void) scroll_view_BoundsDidChange:(NSNotification *)notification;

@end

/*****************************************************************************/
/*                            External Functions                             */
/*****************************************************************************/

extern int interrogate_user(const char *message, const char *option_0,
                            const char *option_1="", const char *option_2="");
  /* Presents a warning message to the user with up to three options, returning
   the index of the selected option.  If `option_1' is an empty string, only
   the first option is presented to the user.  If `option_2' is emty, at most
   two options are presented to the user. */
