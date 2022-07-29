/*****************************************************************************/
// File: kdms_window.mm [scope = APPS/MACSHOW]
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
  Implements the main `kdms_window' class and its `kdms_document_view', both
of which are Objective-C classes.   The embedded `kdms_renderer' object is
implemented separately in `kdms_renderer.mm' -- it is a C++ class but
also has to interact with Cocoa.
******************************************************************************/
#import "kdms_window.h"
#import "kdms_catalog.h"


/*===========================================================================*/
/*                            EXTERNAL FUNCTIONS                             */
/*===========================================================================*/

/*****************************************************************************/
/*                             interrogate_user                              */
/*****************************************************************************/

int interrogate_user(const char *message, const char *option_0,
                     const char *option_1, const char *option_2)
  /* Presents a warning message to the user with up to two options, returning
   the index of the selected option.  If `option_1' is an empty string, only
   the first option is presented to the user. */
{
  NSAlert *alert = [[NSAlert alloc] init];
  [alert setMessageText:[NSString stringWithUTF8String:message]];
  [alert addButtonWithTitle:[NSString stringWithUTF8String:option_0]];
  if (option_1[0] != '\0')
    {
      [alert addButtonWithTitle:[NSString stringWithUTF8String:option_1]];
      if (option_2[0] != '\0')
        [alert addButtonWithTitle:[NSString stringWithUTF8String:option_2]];
    }
  [alert setAlertStyle:NSInformationalAlertStyle];
  NSInteger result = [alert runModal];
  [alert release];
  if (result == NSAlertFirstButtonReturn)
    return 0;
  else if (result == NSAlertSecondButtonReturn)
    return 1;
  else
    return 2;
}

  
/*===========================================================================*/
/*                            kdms_document_view                             */
/*===========================================================================*/

@implementation kdms_document_view

/*****************************************************************************/
/*                     kdms_document_view:initWithFrame                      */
/*****************************************************************************/

- (id)initWithFrame:(NSRect)frame
{
  renderer = NULL;
  wnd = NULL;
  is_focussing = is_dragging = false;
  return [super initWithFrame:frame];
}

/*****************************************************************************/
/*                     kdms_document_view:set_renderer                       */
/*****************************************************************************/

- (void)set_renderer:(kdms_renderer *)the_renderer
{
  renderer = the_renderer;
}

/*****************************************************************************/
/*                       kdms_document_view:set_owner                        */
/*****************************************************************************/

- (void)set_owner:(kdms_window *)owner
{
  wnd = owner;
}

/*****************************************************************************/
/*                         kdms_document_view::isOpaque                      */
/*****************************************************************************/

- (BOOL)isOpaque
{
  return YES;
}

/*****************************************************************************/
/*                         kdms_document_view:drawRect                       */
/*****************************************************************************/

- (void)drawRect:(NSRect)clipping_rect
{
  if (renderer == NULL)
    return; // Should never happen when drawing is really required.
  
  const NSRect *dirty_rects;
  NSInteger i, num_dirty_rects=0;
  [self getRectsBeingDrawn:&dirty_rects count:&num_dirty_rects];
  for (i=0; i < num_dirty_rects; i++)
    {
      kdu_dims render_region =
        renderer->convert_region_from_doc_view(dirty_rects[i]);
      NSGraphicsContext* gc = [NSGraphicsContext currentContext];
      CGContextRef gc_ref = (CGContextRef)[gc graphicsPort];
      if (!renderer->paint_region(render_region,gc_ref))
        { // Need to erase the region
          CGRect cg_rect;
          cg_rect.origin.x = dirty_rects[i].origin.x;
          cg_rect.origin.y = dirty_rects[i].origin.y;
          cg_rect.size.width = dirty_rects[i].size.width;          
          cg_rect.size.height = dirty_rects[i].size.height;
          CGColorRef fill_colour= CGColorGetConstantColor(kCGColorWhite);
          CGContextSetFillColorWithColor(gc_ref,fill_colour);
          CGContextFillRect(gc_ref,cg_rect);
        }
    }
  
  kdu_dims focus_box;
  if (renderer->need_focus_outline(&focus_box))
    { // Draw the focus box
      NSRect rect = renderer->convert_region_to_doc_view(focus_box);
      NSPoint point1 = rect.origin;
      NSPoint point2 = rect.origin;
      point2.x += rect.size.width - 1.0F;
      point2.y += rect.size.height - 1.0F;
      for (int p=0; p < 2; p++)
        {
          NSBezierPath *path = [NSBezierPath bezierPath];
          [path setLineWidth:1.0];
          [path setLineJoinStyle:NSRoundLineJoinStyle];
          [path moveToPoint:NSMakePoint(point1.x,point1.y)];
          [path lineToPoint:NSMakePoint(point1.x,point2.y)];
          [path lineToPoint:NSMakePoint(point2.x,point2.y)];
          [path lineToPoint:NSMakePoint(point2.x,point1.y)];
          [path lineToPoint:NSMakePoint(point1.x,point1.y)];
          point1.x += 1.0F;  point1.y += 1.0F;
          point2.x -= 1.0F;  point2.y -= 1.0F;
          if (p == 0)
            [[NSColor brownColor] setStroke];
          else
            [[NSColor yellowColor] setStroke];
          [path stroke];
        }
    }
  
  if (is_focussing)
    { // Draw a temporary focus box now
      CGFloat pattern[2] = {5.0F,5.0F};
      for (int p=0; p < 2; p++)
        {
          NSBezierPath *path = [NSBezierPath bezierPath];
          [path setLineWidth:3.0];
          [path setLineJoinStyle:NSRoundLineJoinStyle];
          [path setLineDash:pattern count:2 phase:(p==0)?0.0F:5.0F];
          [path moveToPoint:NSMakePoint(mouse_start.x,mouse_start.y)];
          [path lineToPoint:NSMakePoint(mouse_start.x,mouse_last.y)];
          [path lineToPoint:NSMakePoint(mouse_last.x,mouse_last.y)];
          [path lineToPoint:NSMakePoint(mouse_last.x,mouse_start.y)];
          [path lineToPoint:NSMakePoint(mouse_start.x,mouse_start.y)];
          if (p == 0)
            [[NSColor brownColor] setStroke];
          else
            [[NSColor yellowColor] setStroke];
          [path stroke];
        }
    }
}

/*****************************************************************************/
/*                    kdms_document_view:start_focusbox_at                   */
/*****************************************************************************/

- (void)start_focusbox_at:(NSPoint)point
{
  if (is_focussing || is_dragging)
    [self cancel_focus_drag];
  mouse_start = mouse_last = point;
  is_focussing = true;
}

/*****************************************************************************/
/*                    kdms_document_view:start_viewdrag_at                   */
/*****************************************************************************/

- (void)start_viewdrag_at:(NSPoint)point
{
  if (is_focussing || is_dragging)
    [self cancel_focus_drag];
  is_dragging = true;
  mouse_start = mouse_last = [self convertPointToBase:point];
  if (wnd)
    [wnd set_drag_cursor:YES];
}

/*****************************************************************************/
/*                    kdms_document_view:cancel_focus_drag                   */
/*****************************************************************************/

- (void)cancel_focus_drag
{
  if (is_dragging)
    {
      is_dragging = false;
      if (wnd)
        [wnd set_drag_cursor:NO];
    }
  if (is_focussing)
    {
      is_focussing = false;
      [self invalidate_mouse_box];
    }
}

/*****************************************************************************/
/*                     kdms_document_view:mouse_drag_to                      */
/*****************************************************************************/

- (void)mouse_drag_to:(NSPoint)point
{
  if (!(is_focussing || is_dragging))
    return;
  if (is_focussing)
    {
      [self invalidate_mouse_box];
      mouse_last = point;
      [self invalidate_mouse_box];
    }
  else if (is_dragging)
    {
      point = [self convertPointToBase:point];
      if (renderer != NULL)
        renderer->scroll_absolute(2.0F*(point.x-mouse_last.x),
                                  2.0F*(point.y-mouse_last.y),
                                  true);
      mouse_last = point;
    }
}

/*****************************************************************************/
/*                    kdms_document_view:end_focus_drag_at                   */
/*****************************************************************************/

- (void)end_focus_drag_at:(NSPoint)point
{
  if (is_focussing)
    {
      NSRect rect = [self rectangleFromMouseStartToPoint:point];
      [self cancel_focus_drag];
      if ((rect.size.width > 0.0F) && (rect.size.height > 0.0F) &&
          (renderer != NULL))
        {
          kdu_dims focus_box = renderer->convert_region_from_doc_view(rect);
          if ((focus_box.size.x > 1) && (focus_box.size.y > 1))
            renderer->set_focus_box(focus_box);
        }
    }
  else if (is_dragging)
    {
      is_dragging = false;
      if (wnd)
        [wnd set_drag_cursor:NO];
      point = [self convertPointToBase:point];
      if (renderer != NULL)
        renderer->scroll_absolute(2.0F*(point.x-mouse_last.x),
                                  2.0F*(point.y-mouse_last.y),
                                  true);
    }
}

/*****************************************************************************/
/*             kdms_document_view:rectangleFromMouseStartToPoint             */
/*****************************************************************************/

- (NSRect)rectangleFromMouseStartToPoint:(NSPoint)point
{
  NSRect rect;
  if (mouse_start.x <= point.x)
    {
      rect.origin.x = mouse_start.x;
      rect.size.width = point.x - mouse_start.x + 1.0F;
    }
  else
    {
      rect.origin.x = point.x;
      rect.size.width = mouse_start.x - point.x + 1.0F;
    }
  
  if (mouse_start.y <= point.y)
    {
      rect.origin.y = mouse_start.y;
      rect.size.height = point.y - mouse_start.y + 1.0F;
    }
  else
    {
      rect.origin.y = point.y;
      rect.size.height = mouse_start.y - point.y + 1.0F;
    }
  return rect;
}

/*****************************************************************************/
/*                  kdms_document_view:invalidate_mouse_box                  */
/*****************************************************************************/

- (void)invalidate_mouse_box
{
  NSRect dirty_rect = [self rectangleFromMouseStartToPoint:mouse_last];
  dirty_rect.origin.x -= KDMS_FOCUS_MARGIN;
  dirty_rect.size.width += 2.0F*KDMS_FOCUS_MARGIN;
  dirty_rect.origin.y -= KDMS_FOCUS_MARGIN;
  dirty_rect.size.height += 2.0F*KDMS_FOCUS_MARGIN;
  [self setNeedsDisplayInRect:dirty_rect];
}

/*****************************************************************************/
/*                  kdms_document_view:handle_double_click                   */
/*****************************************************************************/

- (void)handle_double_click:(NSPoint)point
{
  NSRect ns_rect;
  ns_rect.origin = point;
  ns_rect.size.width = ns_rect.size.height = 0.0F;
  kdu_coords coords = renderer->convert_region_from_doc_view(ns_rect).pos;
  renderer->edit_metadata_at_point(&coords);
}

/*****************************************************************************/
/*                kdms_document_view:acceptsFirstResponder                   */
/*****************************************************************************/

- (BOOL)acceptsFirstResponder
{
  return YES;
}

/*****************************************************************************/
/*                      kdms_document_view:mouseDown                         */
/*****************************************************************************/

- (void)mouseDown:(NSEvent *)mouse_event
{
  NSUInteger modifier_flags = [mouse_event modifierFlags];
  bool shift_key_down = (modifier_flags & NSShiftKeyMask)?true:false;
  NSPoint window_point = [mouse_event locationInWindow];
  NSPoint doc_view_point = [self convertPoint:window_point fromView:nil];
  if ([mouse_event clickCount] >= 2)
    [self handle_double_click:doc_view_point];
  else if (shift_key_down)
    [self start_viewdrag_at:doc_view_point];
  else
    [self start_focusbox_at:doc_view_point];
}

/*****************************************************************************/
/*                   kdms_document_view:mouseDragged                         */
/*****************************************************************************/

- (void)mouseDragged:(NSEvent *)mouse_event
{
  NSPoint window_point = [mouse_event locationInWindow];
  NSPoint doc_view_point = [self convertPoint:window_point fromView:nil];
  [self mouse_drag_to:doc_view_point];
}

/*****************************************************************************/
/*                        kdms_document_view:mouseUp                         */
/*****************************************************************************/

- (void)mouseUp:(NSEvent *)mouse_event
{
  NSPoint window_point = [mouse_event locationInWindow];
  NSPoint doc_view_point = [self convertPoint:window_point fromView:nil];
  [self end_focus_drag_at:doc_view_point];  
}

/*****************************************************************************/
/*                         kdms_document_view:keyDown                        */
/*****************************************************************************/

- (void)keyDown:(NSEvent *)key_event
{
  [self cancel_focus_drag];
  NSUInteger modifier_flags = [key_event modifierFlags];
  if (modifier_flags & NSNumericPadKeyMask)
    {
      bool shift_key_down = (modifier_flags & NSShiftKeyMask)?true:false;
      bool command_key_down = (modifier_flags & NSCommandKeyMask)?true:false;
      NSString *the_arrow = [key_event charactersIgnoringModifiers];
      if (([the_arrow length] > 0) && !command_key_down)
        {
          unichar key_char = [the_arrow characterAtIndex:0];
          if (key_char == NSLeftArrowFunctionKey)
            {
              if ((renderer != NULL) && !shift_key_down)
                renderer->scroll_relative_to_view(-0.2F,0.0F);
              else if ((renderer != NULL) && renderer->can_do_FocusLeft(nil))
                renderer->menu_FocusLeft();
              return;
            }
          else if (key_char == NSRightArrowFunctionKey)
            {
              if ((renderer != NULL) && !shift_key_down)
                renderer->scroll_relative_to_view(0.2F,0.0F);
              else if ((renderer != NULL) && renderer->can_do_FocusRight(nil))
                renderer->menu_FocusRight();
              return;
            }
          else if (key_char == NSUpArrowFunctionKey)
            {
              if ((renderer != NULL) && !shift_key_down)
                renderer->scroll_relative_to_view(0.0F,-0.2F);
              else if ((renderer != NULL) && renderer->can_do_FocusUp(nil))
                renderer->menu_FocusUp();
              return;
            }
          else if (key_char == NSDownArrowFunctionKey)
            {
              if ((renderer != NULL) && !shift_key_down)
                renderer->scroll_relative_to_view(0.0F,0.2F);
              else if ((renderer != NULL) && renderer->can_do_FocusDown(nil))
                renderer->menu_FocusDown();
              return;
            }
        }
    }
  [super keyDown:key_event];
}


@end // kdms_document_view

/*===========================================================================*/
/*                                kdms_window                                */
/*===========================================================================*/

@implementation kdms_window

/*****************************************************************************/
/*                 kdms_window:initWithManager:andCursors                    */
/*****************************************************************************/

- (void)initWithManager:(kdms_window_manager *)manager
            andCursors:(NSCursor **)cursors
{
  int p; // Used to index status panes
  
  // Initialize the private members to empty values for the moment
  window_manager = nil; // Until we are sure everything has worked
  renderer = NULL;
  doc_view = nil;
  scroll_view = nil;
  status_bar = nil;
  doc_cursor_normal = cursors[0];
  doc_cursor_dragging = cursors[1];
  status_height = 0.0F;
  for (p=0; p < 3; p++)
    {
      status_panes[p] = nil;
      status_pane_lengths[p] = 0;
      status_pane_caches[p] = NULL;
    }
  catalog_panel = nil;
  catalog_view = nil;
  catalog_panel_width = 0.0F;
  
  // Find an initial frame location and size for the window
  NSRect screen_rect = [[NSScreen mainScreen] frame];
  NSRect frame_rect; 
  frame_rect.origin.x = frame_rect.origin.y = 0.0F;
  frame_rect.size.width = (float) ceil(screen_rect.size.width * 0.3);
  frame_rect.size.height = (float) ceil(screen_rect.size.height * 0.3);
  NSUInteger window_style = NSTitledWindowMask | NSClosableWindowMask |
                            NSMiniaturizableWindowMask | NSResizableWindowMask;  
  NSRect content_rect = [NSWindow contentRectForFrameRect:frame_rect
                         styleMask:window_style];
                         
  // Create window and link it in
  [super initWithContentRect:content_rect
                   styleMask:window_style
                     backing:NSBackingStoreBuffered defer:NO];
  manager->add_window(self);
  window_manager = manager; // Now we are on manager's list, we can record it
  
  // Get content view
  NSView *super_view = [self contentView];
  content_rect = [super_view frame]; // Get the relative view rectangle; size
              // should be the same, but origin is now relative to the window.

  // Determine the font, colours and dimensions for the status panes
  CGFloat font_size = [NSFont systemFontSizeForControlSize:NSSmallControlSize];
  NSFont *status_font = [NSFont systemFontOfSize:font_size];
  status_height = (float)
    ceil([status_font ascender] - [status_font descender] + 4.0F);
  NSColor *status_background =
    [NSColor colorWithDeviceRed:0.95F green:0.95F blue:1.0F alpha:1.0F];
  NSColor *status_text_colour =
    [NSColor colorWithDeviceRed:0.3F green:0.1F blue: 0.0F alpha:1.0F];
  
  // Create status bar and panes
  NSRect status_bar_rect = content_rect;
  status_bar_rect.size.height = status_height;
  status_bar = [NSView alloc];
  [status_bar initWithFrame:status_bar_rect];
  [status_bar setAutoresizingMask:(NSViewWidthSizable | NSViewMinXMargin)];
  [super_view addSubview:status_bar];
  NSRect pane_rects[3];
  for (p=0; p < 3; p++)
    {
      pane_rects[p] = content_rect;
      pane_rects[p].size.height = status_height;
    }
  pane_rects[0].size.width = pane_rects[2].size.width =
    (float) floor(content_rect.size.width * 0.3F);
  pane_rects[1].size.width = content_rect.size.width -
    2 * pane_rects[0].size.width;
  pane_rects[1].origin.x += pane_rects[0].size.width;
  pane_rects[2].origin.x = pane_rects[1].origin.x + pane_rects[1].size.width;
  for (p=0; p < 3; p++)
    {
      NSTextField *pane = status_panes[p] = [NSTextField alloc];
      [pane initWithFrame:pane_rects[p]];
      [pane setBezeled:YES];
      [pane setBezelStyle:NSTextFieldSquareBezel];
      [pane setEditable:NO];
      [pane setDrawsBackground:YES];
      [pane setBackgroundColor:status_background];
      [pane setTextColor:status_text_colour];
      [pane setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable |
                                 NSViewMinXMargin | NSViewMaxXMargin)];
      NSTextFieldCell *cell = [pane cell];
      [cell setFont:status_font];
      [status_bar addSubview:pane];
      [pane release];
    }
  
  // Create `NSScrollView' for the window
  NSRect scroll_view_rect = content_rect;
  scroll_view_rect.origin.y += pane_rects[0].size.height;
  scroll_view_rect.size.height -= pane_rects[0].size.height;
  int resizing_mask = NSViewWidthSizable | NSViewHeightSizable;
  scroll_view = [NSScrollView alloc];
  [scroll_view initWithFrame:scroll_view_rect];
  [scroll_view setBorderType:NSNoBorder];
  [scroll_view setAutoresizingMask:resizing_mask];
  [scroll_view setHasVerticalScroller:YES];
  [scroll_view setHasHorizontalScroller:YES];
  [[scroll_view contentView] setCopiesOnScroll:YES];
  [super_view addSubview:scroll_view];
  
  // Create `renderer' and `kdms_document_view' objects for the window
  doc_view = [kdms_document_view alloc];
  [doc_view initWithFrame:content_rect];
  renderer = new kdms_renderer(self,doc_view,scroll_view,manager);
  [doc_view set_renderer:renderer];
  [doc_view set_owner:self];
  [scroll_view setDocumentView:doc_view];
  [scroll_view setDocumentCursor:doc_cursor_normal];
  
  // Set things up to receive notifications of changes in the `scroll_view'.
  [[NSNotificationCenter
            defaultCenter] addObserver:self
                              selector:@selector(scroll_view_BoundsDidChange:)
                                  name:NSViewBoundsDidChangeNotification
                                object:[scroll_view contentView]];
  [[NSNotificationCenter
            defaultCenter] addObserver:self
                              selector:@selector(scroll_view_BoundsDidChange:)
                                  name:NSViewFrameDidChangeNotification
                                object:[scroll_view contentView]];
    
  // Release local references and display the window
  renderer->close_file(); // Causes the correct title to be written
  [scroll_view release]; // Retained by the window's content view
  [status_bar release]; // Retained by the window's content view
  [doc_view release]; // Retained by the scroll view
  [self center];
  [self makeKeyAndOrderFront:self]; // Display the window
}

/*****************************************************************************/
/*                             kdms_window:dealloc                           */
/*****************************************************************************/

- (void)dealloc
{
  if (window_manager != NULL)
    {
      kdms_window_manager *manager = window_manager;
      window_manager = nil;
      manager->remove_window(self);
    }
  if (renderer != NULL)
    { delete renderer; renderer = NULL; }
  [super dealloc];
}

/*****************************************************************************/
/*                           kdms_window:open_file                           */
/*****************************************************************************/

- (void)open_file:(NSString *)fname
{
  if ((renderer != NULL) && renderer->have_unsaved_edits &&
      (interrogate_user("You have edited the existing file but not saved "
                        "these edits ... perhaps you saved the file in a "
                        "format which cannot hold all the metadata ("
                        "use JPX).  Do you still want to close the existing "
                        "file to open a new one?",
                        "Cancel","Close without Saving") == 0))
      return;
  const char *filename = [fname UTF8String];
  if (window_manager->check_open_file_replaced(filename) &&
      !interrogate_user("A file you have elected to open is out of date, "
                        "meaning that another open window in the application "
                        "has saved over the file but not yet released its "
                        "access, so that the saved copy can replace the "
                        "original.  Do you still wish to open the old copy?",
                        "Cancel","Open Anyway"))
    return;
  renderer->open_file(filename);
}

/*****************************************************************************/
/*                   kdms_window:application_can_terminate                   */
/*****************************************************************************/

- (bool)application_can_terminate
{
  if ((renderer == NULL) || !renderer->have_unsaved_edits)
    return true;
  return (interrogate_user("You have unsaved edits in one or more windows that "
                           "will be closed when this application terminates.  "
                           "Are you sure you want to quit the application?",
                           "Cancel","Quit Application") == 1);
}

/*****************************************************************************/
/*                    kdms_window:application_terminating                    */
/*****************************************************************************/

- (void)application_terminating
{
  // Add code for handling any urgent actions before the application
  // terminates (without invoking `dealloc').
  if (renderer != NULL)
    renderer->perform_essential_cleanup_steps();
}

/*****************************************************************************/
/*                             kdms_window:close                             */
/*****************************************************************************/

- (void) close
{
  if (renderer != NULL)
    {
      if (renderer->have_unsaved_edits)
        {
          if (interrogate_user("You have edited this file but not saved these "
                               "edits ... perhaps you saved the file in a "
                               "format which cannot hold all the metadata ("
                               "use JPX).  Do you still want to close?",
                               "Cancel","Close without Saving") == 0)
            return;
        }
      if (renderer->metashow)
        [renderer->metashow close];
    }
  [super close];  
}

/*****************************************************************************/
/*                            kdms_window:on_idle                            */
/*****************************************************************************/

- (bool)on_idle
{
  if (renderer == NULL)
    return false;
  else
    return renderer->on_idle();
}

/*****************************************************************************/
/*                       kdms_window:adjust_playclock                        */
/*****************************************************************************/

- (void)adjust_playclock:(double)delta
{
  if (renderer != NULL)
    renderer->adjust_playclock(delta);
}

/*****************************************************************************/
/*                kdms_window:placeUsingManagerWithFrameSize                 */
/*****************************************************************************/

- (void)placeUsingManagerWithFrameSize:(NSSize)frame_size
{
  if (window_manager != NULL)
    window_manager->place_window(self,frame_size);
}

/*****************************************************************************/
/*                    kdms_window:create_metadata_catalog                    */
/*****************************************************************************/

- (void)create_metadata_catalog
{
  [self remove_metadata_catalog]; // Reset everything, just in case
  if (renderer == NULL)
    return;
  
  // Start by disabling auto-resizing and resizing the window itself
  NSView *super_view = [self contentView];
  [super_view setAutoresizesSubviews:NO];
  NSSize max_window_size = [self maxSize];
  catalog_panel_width = 200.0F; // A reasonable panel width
  max_window_size.width += catalog_panel_width;
  [self setMaxSize:max_window_size];
  NSRect frame_rect = [self frame];
  frame_rect.size.width += catalog_panel_width;
  [self setFrame:frame_rect display:NO];
  
  // Find the bounding rectangles for the top-level views
  NSRect content_rect = [super_view frame];
  
  NSRect scroll_view_rect = content_rect;
  scroll_view_rect.origin.y = status_height;
  scroll_view_rect.size.height -= scroll_view_rect.origin.y;
  scroll_view_rect.size.width -= catalog_panel_width;
  [scroll_view setFrame:scroll_view_rect];
  
  NSRect status_bar_rect = scroll_view_rect;
  status_bar_rect.origin.y = 0.0F;
  status_bar_rect.size.height = status_height;
  [status_bar setFrame:status_bar_rect];
    
  NSRect catalog_panel_rect = content_rect;
  catalog_panel_rect.origin.x = scroll_view_rect.size.width;
  catalog_panel_rect.size.width -= catalog_panel_rect.origin.x;
  catalog_panel = [NSScrollView alloc];
  [catalog_panel initWithFrame:catalog_panel_rect];
  [catalog_panel setBorderType:NSBezelBorder];
  [catalog_panel setAutoresizingMask:NSViewHeightSizable|NSViewMinXMargin];
  [catalog_panel setHasVerticalScroller:YES];
  [catalog_panel setHasHorizontalScroller:YES];
  [super_view addSubview:catalog_panel];
  [catalog_panel release];

  catalog_view = [NSOutlineView alloc];
  [catalog_view initWithFrame:[[catalog_panel contentView] frame]];

  [catalog_view setBackgroundColor:
   [NSColor colorWithDeviceRed:1.0F green:1.0F blue:0.95F alpha:1.0F]];
  [catalog_panel setDocumentView:catalog_view];
  [catalog_view release]; // Retained by catalog_panel
  
  NSTableColumn *outline_column = [NSTableColumn alloc];
  [outline_column initWithIdentifier:@"Outline Column"];
  [[outline_column headerCell] setStringValue:@"Metadata Catalog"];
  [outline_column setEditable:NO];
  [outline_column setHidden:NO];
  [catalog_view addTableColumn:outline_column];
  [catalog_view setOutlineTableColumn:outline_column];
  [catalog_view sizeLastColumnToFit];
  [outline_column setWidth:800.0F];
  [outline_column release]; // Retained by catalog_view
  
  [super_view setAutoresizesSubviews:YES];
  kdms_catalog *catalog_data_source = [kdms_catalog alloc];
  [catalog_data_source initWithOutlineView:catalog_view renderer:renderer];
  renderer->set_metadata_catalog_source(catalog_data_source);
}

/*****************************************************************************/
/*                    kdms_window:remove_metadata_catalog                    */
/*****************************************************************************/

- (void)remove_metadata_catalog
{
  NSView *super_view = [self contentView];
  [super_view setAutoresizesSubviews:NO];
  
  if (catalog_panel)
    {
      [catalog_panel removeFromSuperview];
      catalog_panel = nil; // Should have been auto-released by above function
      catalog_view = nil; // Should have been auto-released by above function
      catalog_panel_width = 0.0F;
    }
  NSRect frame_rect = [self frame];
  if (renderer != NULL)
    {
      NSSize max_size;
      if (renderer->get_max_scroll_view_size(max_size))
        {
          max_size.height += status_height;
          NSRect max_content_rect;
          max_content_rect.size = max_size;
          max_content_rect.origin.x = max_content_rect.origin.y = 0.0F;
          NSRect max_window_rect =
            [self frameRectForContentRect:max_content_rect];
          bool need_frame_resize = false;
          if (frame_rect.size.width > max_window_rect.size.width)
            {
              frame_rect.size.width = max_window_rect.size.width;
              need_frame_resize = true;
            }
          if (frame_rect.size.height > max_window_rect.size.height)
            {
              frame_rect.size.height = max_window_rect.size.height;
              need_frame_resize = true;
            }
          if (need_frame_resize)
            [self setFrame:frame_rect display:NO];
          [self setMaxSize:max_window_rect.size];
        }
    }
  frame_rect = [super_view frame];
  if (status_bar)
    { // Move status bar down to the bottom of the window
      NSRect bar_rect = frame_rect;
      bar_rect.size.height = status_height;
      [status_bar setFrame:bar_rect];
    }
  if (scroll_view)
    { // Adjust scroll_view to occupy the entire window, apart from status bar
      NSRect scroll_rect = frame_rect;
      scroll_rect.size.height -= status_height;
      scroll_rect.origin.y = status_height;
      scroll_rect.origin.x = 0.0F;
      [scroll_view setFrame:scroll_rect];
    }

  [super_view setAutoresizesSubviews:YES];
  if (renderer != NULL)
    renderer->set_metadata_catalog_source(NULL);
}

/*****************************************************************************/
/*                       kdms_window:get_bottom_margin_height                       */
/*****************************************************************************/

- (float)get_bottom_margin_height
{
  return status_height;
}

/*****************************************************************************/
/*                    kdms_window:get_right_margin_width                     */
/*****************************************************************************/

- (float)get_right_margin_width
{
  return catalog_panel_width; 
}

/*****************************************************************************/
/*                       kdms_window:set_status_strings                      */
/*****************************************************************************/

- (void)set_status_strings:(char **)three_strings
{
  int p; // Panel index
  
  // Start by finding the status panes which have changed, and allocating
  // space to cache the new status strings.
  int total_chars = 0; // Including NULL chars.
  bool pane_updated[3] = {false,false,false};
  for (p=0; p < 3; p++)
    {
      const char *string = three_strings[p];
      if (string == NULL)
        string = "";
      int len = strlen(string)+1;
      pane_updated[p] = ((len != status_pane_lengths[p]) ||
                         (strcmp(string,status_pane_caches[p]) != 0));
      if ((total_chars + len) <= 256)
        { // Can cache this string
          status_pane_lengths[p] = len;
          status_pane_caches[p] = status_pane_cache_buf + total_chars;
          total_chars += len;
        }
      else
        { // Cannot cache this string
          status_pane_lengths[p] = 0;
          status_pane_caches[p] = NULL;
        }          
    }
  
  // Now update any status panes whose text has changed and also cache the
  // new text if we can, for next time.  Caching allows us to avoid updating
  // too many status panes, which can speed things up in video applications,
  // for example.
  for (p=0; p < 3; p++)
    {
      if (!pane_updated[p])
        continue;
      const char *string = three_strings[p];
      if (string == NULL)
        string = "";
      if (status_pane_caches[p] != NULL)
        strcpy(status_pane_caches[p],string);
      NSString *ns_string = [NSString stringWithUTF8String:string];
      NSTextFieldCell *cell = [status_panes[p] cell];
      [cell setTitle:ns_string];
    }
}

/*****************************************************************************/
/*                        kdms_window:set_drag_cursor                        */
/*****************************************************************************/

- (void)set_drag_cursor:(BOOL)is_dragging
{
  if (!scroll_view)
    return;
  if (is_dragging)
    [scroll_view setDocumentCursor:doc_cursor_dragging];
  else
    [scroll_view setDocumentCursor:doc_cursor_normal];
}

/*****************************************************************************/
/*                          kdms_window:menuFile....                         */
/*****************************************************************************/

- (IBAction) menuFileOpen:(NSMenuItem *)sender
{
  NSArray *file_types =
    [NSArray arrayWithObjects:@"jp2",@"jpx",@"jpf",@"mj2",@"j2c",@"j2k",nil];
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  if ([panel runModalForTypes:file_types] == NSOKButton)
    {
      NSArray *filenames = [panel filenames];
      if (filenames.count >= 1)
        {
          NSString *filename = [[panel filenames] objectAtIndex:0];
          [self open_file:filename];
        }
    }
}

- (IBAction) menuFileSave:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FileSave();
  [window_manager->get_next_action_window(self) menuFileSave:sender];
}

- (IBAction) menuFileSaveAs:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FileSaveAs(true,false);
}

- (IBAction) menuFileSaveAsLinked:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FileSaveAs(true,true);
}

- (IBAction) menuFileSaveAsEmbedded:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FileSaveAs(false,false);
}

- (IBAction) menuFileSaveReload:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FileSaveReload();
  [window_manager->get_next_action_window(self) menuFileSaveReload:sender];
}

- (IBAction) menuFileProperties:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FileProperties();
}

/*****************************************************************************/
/*                          kdms_window:menuView....                         */
/*****************************************************************************/

- (IBAction) menuViewHflip:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewHflip();
  [window_manager->get_next_action_window(self) menuViewHflip:sender];
}

- (IBAction) menuViewVflip:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewVflip();
  [window_manager->get_next_action_window(self) menuViewVflip:sender];
}

- (IBAction) menuViewRotate:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewRotate();
  [window_manager->get_next_action_window(self) menuViewRotate:sender];
}

- (IBAction) menuViewCounterRotate:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewCounterRotate();
  [window_manager->get_next_action_window(self) menuViewCounterRotate:sender];
}

- (IBAction) menuViewZoomIn:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewZoomIn();
  [window_manager->get_next_action_window(self) menuViewZoomIn:sender];
}

- (IBAction) menuViewZoomOut:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewZoomOut();
  [window_manager->get_next_action_window(self) menuViewZoomOut:sender];
}

- (IBAction) menuViewWiden:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewWiden();
  [window_manager->get_next_action_window(self) menuViewWiden:sender];
}

- (IBAction) menuViewShrink:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewShrink();
  [window_manager->get_next_action_window(self) menuViewShrink:sender];
}

- (IBAction) menuViewRestore:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewRestore();
  [window_manager->get_next_action_window(self) menuViewRestore:sender];
}

- (IBAction) menuViewRefresh:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewRefresh();
  [window_manager->get_next_action_window(self) menuViewRefresh:sender];
}

- (IBAction) menuViewLayersLess:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewLayersLess();
  [window_manager->get_next_action_window(self) menuViewLayersLess:sender];
}

- (IBAction) menuViewLayersMore:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewLayersMore();
  [window_manager->get_next_action_window(self) menuViewLayersMore:sender];
}

- (IBAction) menuViewOptimizeScale:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewOptimizeScale();
  [window_manager->get_next_action_window(self) menuViewOptimizeScale:sender];
}

- (IBAction) menuViewPixelScaleX1:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewPixelScaleX1();
  [window_manager->get_next_action_window(self) menuViewPixelScaleX1:sender];
}

- (IBAction) menuViewPixelScaleX2:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ViewPixelScaleX2();
  [window_manager->get_next_action_window(self) menuViewPixelScaleX2:sender];
}

/*****************************************************************************/
/*                         kdms_window:menuFocus....                         */
/*****************************************************************************/

- (IBAction) menuFocusOff:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FocusOff();
  [window_manager->get_next_action_window(self) menuFocusOff:sender];
}

- (IBAction) menuFocusHighlighting:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FocusHighlighting();
  [window_manager->get_next_action_window(self) menuFocusHighlighting:sender];
}

- (IBAction) menuFocusWiden:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FocusWiden();
  [window_manager->get_next_action_window(self) menuFocusWiden:sender];
}

- (IBAction) menuFocusShrink:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FocusShrink();
  [window_manager->get_next_action_window(self) menuFocusShrink:sender];
}

- (IBAction) menuFocusLeft:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FocusLeft();
  [window_manager->get_next_action_window(self) menuFocusLeft:sender];
}

- (IBAction) menuFocusRight:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FocusRight();
  [window_manager->get_next_action_window(self) menuFocusRight:sender];
}

- (IBAction) menuFocusUp:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FocusUp();
  [window_manager->get_next_action_window(self) menuFocusUp:sender];
}

- (IBAction) menuFocusDown:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_FocusDown();
  [window_manager->get_next_action_window(self) menuFocusDown:sender];
}

/*****************************************************************************/
/*                          kdms_window:menuMode....                         */
/*****************************************************************************/

- (IBAction) menuModeFast:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ModeFast();
  [window_manager->get_next_action_window(self) menuModeFast:sender];
}

- (IBAction) menuModeFussy:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ModeFussy();
  [window_manager->get_next_action_window(self) menuModeFussy:sender];
}

- (IBAction) menuModeResilient:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ModeResilient();
  [window_manager->get_next_action_window(self) menuModeResilient:sender];
}

- (IBAction) menuModeResilientSOP:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ModeResilientSOP();
  [window_manager->get_next_action_window(self) menuModeResilientSOP:sender];
}

- (IBAction) menuModeSingleThreaded:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ModeSingleThreaded();
  [window_manager->get_next_action_window(self) menuModeSingleThreaded:sender];
}

- (IBAction) menuModeMultiThreaded:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ModeMultiThreaded();
  [window_manager->get_next_action_window(self) menuModeMultiThreaded:sender];
}

- (IBAction) menuModeMoreThreads:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ModeMoreThreads();
  [window_manager->get_next_action_window(self) menuModeMoreThreads:sender];
}

- (IBAction) menuModeLessThreads:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ModeLessThreads();
  [window_manager->get_next_action_window(self) menuModeLessThreads:sender];
}

- (IBAction) menuModeRecommendedThreads:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_ModeRecommendedThreads();
  [window_manager->get_next_action_window(self) menuModeRecommendedThreads:sender];
}

/*****************************************************************************/
/*                          kdms_window:menuNav....                          */
/*****************************************************************************/

- (IBAction) menuNavComponent1:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_NavComponent1();
  [window_manager->get_next_action_window(self) menuNavComponent1:sender];
}

- (IBAction) menuNavMultiComponent:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_NavMultiComponent();
  [window_manager->get_next_action_window(self) menuNavMultiComponent:sender];
}

- (IBAction) menuNavComponentNext:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_NavComponentNext();
  [window_manager->get_next_action_window(self) menuNavComponentNext:sender];
}

- (IBAction) menuNavComponentPrev:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_NavComponentPrev();
  [window_manager->get_next_action_window(self) menuNavComponentPrev:sender];
}

- (IBAction) menuNavImageNext:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_NavImageNext();
  [window_manager->get_next_action_window(self) menuNavImageNext:sender];
}

- (IBAction) menuNavImagePrev:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_NavImagePrev();
  [window_manager->get_next_action_window(self) menuNavImagePrev:sender];
}

- (IBAction) menuNavCompositingLayer:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_NavCompositingLayer();
  [window_manager->get_next_action_window(self) menuNavCompositingLayer:sender];
}

- (IBAction) menuNavTrackNext:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_NavTrackNext();
  [window_manager->get_next_action_window(self) menuNavTrackNext:sender];
}

/*****************************************************************************/
/*                        kdms_window:menuMetadata...                        */
/*****************************************************************************/

- (IBAction) menuMetadataCatalog:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_MetadataCatalog();
  [window_manager->get_next_action_window(self) menuMetadataCatalog:sender];
}

- (IBAction) menuMetadataShow:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_MetadataShow();
  [window_manager->get_next_action_window(self) menuMetadataShow:sender];
}

- (IBAction) menuMetadataAdd:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_MetadataAdd();
  [window_manager->get_next_action_window(self) menuMetadataAdd:sender];
}

- (IBAction) menuMetadataEdit:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_MetadataEdit();
  [window_manager->get_next_action_window(self) menuMetadataEdit:sender];
}

- (IBAction) menuOverlayEnable:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_OverlayEnable();
  [window_manager->get_next_action_window(self) menuOverlayEnable:sender];
}

- (IBAction) menuOverlayFlashing:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_OverlayFlashing();
  [window_manager->get_next_action_window(self) menuOverlayFlashing:sender];
}

- (IBAction) menuOverlayToggle:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_OverlayToggle();
  [window_manager->get_next_action_window(self) menuOverlayToggle:sender];
}

- (IBAction) menuOverlayBrighter:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_OverlayBrighter();
  [window_manager->get_next_action_window(self) menuOverlayBrighter:sender];
}

- (IBAction) menuOverlayDarker:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_OverlayDarker();
  [window_manager->get_next_action_window(self) menuOverlayDarker:sender];
}

- (IBAction) menuOverlayDoubleSize:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_OverlayDoubleSize();
  [window_manager->get_next_action_window(self) menuOverlayDoubleSize:sender];
}

- (IBAction) menuOverlayHalveSize:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_OverlayHalveSize();
  [window_manager->get_next_action_window(self) menuOverlayHalveSize:sender];
}

/*****************************************************************************/
/*                          kdms_window:menuPlay....                         */
/*****************************************************************************/

- (IBAction) menuPlayStartForward:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayStartForward();
  [window_manager->get_next_action_window(self) menuPlayStartForward:sender];
}

- (IBAction) menuPlayStartBackward:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayStartBackward();
  [window_manager->get_next_action_window(self) menuPlayStartBackward:sender];
}

- (IBAction) menuPlayStop:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayStop();
  [window_manager->get_next_action_window(self) menuPlayStop:sender];
}

- (IBAction) menuPlayRepeat:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayRepeat();
  [window_manager->get_next_action_window(self) menuPlayRepeat:sender];
}

- (IBAction) menuPlayNative:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayNative();
  [window_manager->get_next_action_window(self) menuPlayNative:sender];
}

- (IBAction) menuPlayCustom:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayCustom();
  [window_manager->get_next_action_window(self) menuPlayCustom:sender];
}

- (IBAction) menuPlayCustomRateUp:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayCustomRateUp();
  [window_manager->get_next_action_window(self) menuPlayCustomRateUp:sender];
}

- (IBAction) menuPlayCustomRateDown:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayCustomRateDown();
  [window_manager->get_next_action_window(self) menuPlayCustomRateDown:sender];
}

- (IBAction) menuPlayNativeRateUp:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayNativeRateUp();
  [window_manager->get_next_action_window(self) menuPlayNativeRateUp:sender];
}

- (IBAction) menuPlayNativeRateDown:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_PlayNativeRateDown();
  [window_manager->get_next_action_window(self) menuPlayNativeRateDown:sender];
}

/*****************************************************************************/
/*                         kdms_window:menuStatus....                        */
/*****************************************************************************/

- (IBAction) menuStatusToggle:(NSMenuItem *)sender
{
  if (renderer != NULL) renderer->menu_StatusToggle();
  [window_manager->get_next_action_window(self) menuStatusToggle:sender];
}


/*****************************************************************************/
/*                        kdms_window:validateMenuItem                       */
/*****************************************************************************/

- (BOOL) validateMenuItem:(NSMenuItem *)item
{
  if ([item action] == @selector(menuFileSave:))
    return ((renderer != NULL) && renderer->can_do_FileSave(item))?YES:NO;
  if ([item action] == @selector(menuFileSaveAs:))
    return ((renderer != NULL) && renderer->can_do_FileSaveAs(item))?YES:NO;
  if ([item action] == @selector(menuFileSaveAsLinked:))
    return ((renderer != NULL) && renderer->can_do_FileSaveAs(item))?YES:NO;
  if ([item action] == @selector(menuFileSaveAsEmbedded:))
    return ((renderer != NULL) && renderer->can_do_FileSaveAs(item))?YES:NO;
  if ([item action] == @selector(menuFileSaveReload:))
    return ((renderer != NULL) && renderer->can_do_FileSaveReload(item))?YES:NO;
  if ([item action] == @selector(menuFileProperties:))
    return ((renderer != NULL) && renderer->can_do_FileProperties(item))?YES:NO;
    
  if ([item action] == @selector(menuViewHflip:))
    return ((renderer != NULL) && renderer->can_do_ViewHflip(item))?YES:NO;
  if ([item action] == @selector(menuViewVflip:))
    return ((renderer != NULL) && renderer->can_do_ViewVflip(item))?YES:NO;
  if ([item action] == @selector(menuViewRotate:))
    return ((renderer != NULL) && renderer->can_do_ViewRotate(item))?YES:NO;
  if ([item action] == @selector(menuViewCounterRotate:))
    return ((renderer != NULL) && renderer->can_do_ViewCounterRotate(item))?YES:NO;
  if ([item action] == @selector(menuViewZoomIn:))
    return ((renderer != NULL) && renderer->can_do_ViewZoomIn(item))?YES:NO;
  if ([item action] == @selector(menuViewZoomOut:))
    return ((renderer != NULL) && renderer->can_do_ViewZoomOut(item))?YES:NO;
  if ([item action] == @selector(menuViewWiden:))
    return ((renderer != NULL) && renderer->can_do_ViewWiden(item))?YES:NO;
  if ([item action] == @selector(menuViewShrink:))
    return ((renderer != NULL) && renderer->can_do_ViewShrink(item))?YES:NO;
  if ([item action] == @selector(menuViewRestore:))
    return ((renderer != NULL) && renderer->can_do_ViewRestore(item))?YES:NO;
  if ([item action] == @selector(menuViewOptimizeScale:))
    return ((renderer != NULL) && renderer->can_do_ViewOptimizeScale(item))?YES:NO;
  if ([item action] == @selector(menuViewPixelScaleX1:))
    return ((renderer != NULL) && renderer->can_do_ViewPixelScaleX1(item))?YES:NO;
  if ([item action] == @selector(menuViewPixelScaleX2:))
    return ((renderer != NULL) && renderer->can_do_ViewPixelScaleX2(item))?YES:NO;
  
  if ([item action] == @selector(menuFocusOff:))
    return ((renderer != NULL) && renderer->can_do_FocusOff(item))?YES:NO;
  if ([item action] == @selector(menuFocusHighlighting:))
    return ((renderer != NULL) && renderer->can_do_FocusHighlighting(item))?YES:NO;
  if ([item action] == @selector(menuFocusWiden:))
    return ((renderer != NULL) && renderer->can_do_FocusWiden(item))?YES:NO;
  if ([item action] == @selector(menuFocusShrink:))
    return ((renderer != NULL) && renderer->can_do_FocusShrink(item))?YES:NO;  
  if ([item action] == @selector(menuFocusLeft:))
    return ((renderer != NULL) && renderer->can_do_FocusLeft(item))?YES:NO;  
  if ([item action] == @selector(menuFocusRight:))
    return ((renderer != NULL) && renderer->can_do_FocusRight(item))?YES:NO;  
  if ([item action] == @selector(menuFocusUp:))
    return ((renderer != NULL) && renderer->can_do_FocusUp(item))?YES:NO;  
  if ([item action] == @selector(menuFocusDown:))
    return ((renderer != NULL) && renderer->can_do_FocusDown(item))?YES:NO;  
  
  if ([item action] == @selector(menuModeFast:))
    return ((renderer != NULL) && renderer->can_do_ModeFast(item))?YES:NO;
  if ([item action] == @selector(menuModeFussy:))
    return ((renderer != NULL) && renderer->can_do_ModeFussy(item))?YES:NO;
  if ([item action] == @selector(menuModeResilient:))
    return ((renderer != NULL) && renderer->can_do_ModeResilient(item))?YES:NO;
  if ([item action] == @selector(menuModeResilientSOP:))
    return ((renderer != NULL) && renderer->can_do_ModeResilientSOP(item))?YES:NO;
  if ([item action] == @selector(menuModeSingleThreaded:))
    return ((renderer != NULL) && renderer->can_do_ModeSingleThreaded(item))?YES:NO;
  if ([item action] == @selector(menuModeMultiThreaded:))
    return ((renderer != NULL) && renderer->can_do_ModeMultiThreaded(item))?YES:NO;
  if ([item action] == @selector(menuModeMoreThreads:))
    return ((renderer != NULL) && renderer->can_do_ModeMoreThreads(item))?YES:NO;
  if ([item action] == @selector(menuModeLessThreads:))
    return ((renderer != NULL) && renderer->can_do_ModeLessThreads(item))?YES:NO;
  if ([item action] == @selector(menuModeRecommendedThreads:))
    return ((renderer != NULL) && renderer->can_do_ModeRecommendedThreads(item))?YES:NO;

  if ([item action] == @selector(menuNavComponent1:))
    return ((renderer != NULL) && renderer->can_do_NavComponent1(item))?YES:NO;
  if ([item action] == @selector(menuNavMultiComponent:))
    return ((renderer != NULL) && renderer->can_do_NavMultiComponent(item))?YES:NO;
  if ([item action] == @selector(menuNavComponentNext:))
    return ((renderer != NULL) && renderer->can_do_NavComponentNext(item))?YES:NO;
  if ([item action] == @selector(menuNavComponentPrev:))
    return ((renderer != NULL) && renderer->can_do_NavComponentPrev(item))?YES:NO;
  if ([item action] == @selector(menuNavImageNext:))
    return ((renderer != NULL) && renderer->can_do_NavImageNext(item))?YES:NO;
  if ([item action] == @selector(menuNavImagePrev:))
    return ((renderer != NULL) && renderer->can_do_NavImagePrev(item))?YES:NO;
  if ([item action] == @selector(menuNavCompositingLayer:))
    return ((renderer != NULL) && renderer->can_do_NavCompositingLayer(item))?YES:NO;
  if ([item action] == @selector(menuNavTrackNext:))
    return ((renderer != NULL) && renderer->can_do_NavTrackNext(item))?YES:NO;

  if ([item action] == @selector(menuMetadataCatalog:))
    return ((renderer != NULL) && renderer->can_do_MetadataCatalog(item))?YES:NO;
  if ([item action] == @selector(menuMetadataShow:))
    return ((renderer != NULL) && renderer->can_do_MetadataShow(item))?YES:NO;
  if ([item action] == @selector(menuMetadataAdd:))
    return ((renderer != NULL) && renderer->can_do_MetadataAdd(item))?YES:NO;
  if ([item action] == @selector(menuMetadataEdit:))
    return ((renderer != NULL) && renderer->can_do_MetadataEdit(item))?YES:NO;
  if ([item action] == @selector(menuOverlayEnable:))
    return ((renderer != NULL) && renderer->can_do_OverlayEnable(item))?YES:NO;
  if ([item action] == @selector(menuOverlayFlashing:))
    return ((renderer != NULL) && renderer->can_do_OverlayFlashing(item))?YES:NO;
  if ([item action] == @selector(menuOverlayToggle:))
    return ((renderer != NULL) && renderer->can_do_OverlayToggle(item))?YES:NO;
  if ([item action] == @selector(menuOverlayBrighter:))
    return ((renderer != NULL) && renderer->can_do_OverlayBrighter(item))?YES:NO;
  if ([item action] == @selector(menuOverlayDarker:))
    return ((renderer != NULL) && renderer->can_do_OverlayDarker(item))?YES:NO;
  if ([item action] == @selector(menuOverlayDoubleSize:))
    return ((renderer != NULL) && renderer->can_do_OverlayDoubleSize(item))?YES:NO;
  if ([item action] == @selector(menuOverlayHalveSize:))
    return ((renderer != NULL) && renderer->can_do_OverlayHalveSize(item))?YES:NO;
  
  if ([item action] == @selector(menuPlayStartForward:))
    return ((renderer != NULL) && renderer->can_do_PlayStartForward(item))?YES:NO;
  if ([item action] == @selector(menuPlayStartBackward:))
    return ((renderer != NULL) && renderer->can_do_PlayStartBackward(item))?YES:NO;
  if ([item action] == @selector(menuPlayStop:))
    return ((renderer != NULL) && renderer->can_do_PlayStop(item))?YES:NO;  
  if ([item action] == @selector(menuPlayRepeat:))
    return ((renderer != NULL) && renderer->can_do_PlayRepeat(item))?YES:NO;  
  if ([item action] == @selector(menuPlayNative:))
    return ((renderer != NULL) && renderer->can_do_PlayNative(item))?YES:NO;  
  if ([item action] == @selector(menuPlayCustom:))
    return ((renderer != NULL) && renderer->can_do_PlayCustom(item))?YES:NO;  
  if ([item action] == @selector(menuPlayNativeRateUp:))
    return ((renderer != NULL) && renderer->can_do_PlayNativeRateUp(item))?YES:NO;  
  if ([item action] == @selector(menuPlayNativeRateDown:))
    return ((renderer != NULL) && renderer->can_do_PlayNativeRateDown(item))?YES:NO;  
  if ([item action] == @selector(menuPlayCustomRateUp:))
    return ((renderer != NULL) && renderer->can_do_PlayCustomRateUp(item))?YES:NO;  
  if ([item action] == @selector(menuPlayCustomRateDown:))
    return ((renderer != NULL) && renderer->can_do_PlayCustomRateDown(item))?YES:NO;  
  
  if ([item action] == @selector(menuStatusToggle:))
    return ((renderer != NULL) && renderer->can_do_StatusToggle(item))?YES:NO;

  return YES;
}

/*****************************************************************************/
/*                 kdms_window:scroll_view_BoundsDidChange                   */
/*****************************************************************************/

- (void) scroll_view_BoundsDidChange:(NSNotification *)notification
{
  if (renderer != NULL)
    renderer->view_dims_changed();
}
  

@end // kdms_window
