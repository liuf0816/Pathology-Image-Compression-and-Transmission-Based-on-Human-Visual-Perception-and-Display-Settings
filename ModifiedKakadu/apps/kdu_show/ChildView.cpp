/******************************************************************************/
// File: ChildView.cpp [scope = APPS/SHOW]
// Version: Kakadu, V6.1
// Author: David Taubman
// Last Revised: 12 August, 2007
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
   Implementation of the child view window of the interactive JPEG2000 image
viewer application, "kdu_show".  Client-area windows messages are
processed (at least initially) here.
*******************************************************************************/

#include "stdafx.h"
#include "kdu_show.h"
#include "ChildView.h"
#include ".\childview.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CChildView

/******************************************************************************/
/*                          CChildView::CChildView                            */
/******************************************************************************/

CChildView::CChildView()
{
  app = NULL;
  pixel_scale = last_pixel_scale = 1;
  max_view_size = kdu_coords(10000,10000);
  sizing = false;
  last_size = kdu_coords(0,0);
  scroll_step = kdu_coords(1,1);
  scroll_page = scroll_step;
  scroll_end = kdu_coords(100000,100000);
  screen_size.x = GetSystemMetrics(SM_CXSCREEN);
  screen_size.y = GetSystemMetrics(SM_CYSCREEN);

  focusing = false;
  panning = false;
  showing_metainfo = false;

  HINSTANCE hinst = AfxGetInstanceHandle();
  normal_cursor = LoadCursor(NULL,IDC_ARROW);
  focusing_cursor = LoadCursor(NULL,IDC_CROSS);
  panning_cursor = LoadCursor(NULL,IDC_SIZEALL);
  overlay_cursor = LoadCursor(hinst,MAKEINTRESOURCE(IDC_META_CURSOR));
}

/******************************************************************************/
/*                          CChildView::~CChildView                           */
/******************************************************************************/

CChildView::~CChildView()
{
}

/******************************************************************************/
/*                            CChildView::set_app                             */
/******************************************************************************/

void
  CChildView::set_app(CKdu_showApp *app)
{
  this->app = app;
}

/******************************************************************************/
/*                       CChildView::set_max_view_size                        */
/******************************************************************************/

void
  CChildView::set_max_view_size(kdu_coords size, int pscale)
{
  max_view_size = size;
  pixel_scale = pscale;
  max_view_size.x*=pixel_scale;
  max_view_size.y*=pixel_scale;
  if ((last_size.x > 0) && (last_size.y > 0))
    { /* Otherwise, the windows message loop is bound to send us a
         WM_SIZE message some time soon, which will call the
         following function out of the 'OnSize' member function. */
      check_and_report_size();
    }
  last_pixel_scale = pixel_scale;
}

/******************************************************************************/
/*                     CChildView::check_and_report_size                      */
/******************************************************************************/

void
  CChildView::check_and_report_size()
  /* The code here is very delicate.  The problem is that the framework
     enters into its own layout recalculations whenever the SetWindowPos
     function is called on the frame window.  This asserts certain internal
     guards against recursive re-entry into the layout recalculation code.
     To skirt around some of these complications, we override the frame
     object's OnSize function, letting it process an initial layout for the
     child and then calling this function here, which has to recover the
     layout generated by the frame (i.e., the parent).  This allows the code
     to re-enter the layout generation process -- unless somebody goes and
     modifies the implementation of some parts of the MFC framework.  Shame
     that MFC does not really protect the user against hidden implementation
     choices like this. */
{
  if (sizing)
    return; // We are already in the process of negotiating new dimensions.

  kdu_coords inner_size, outer_size, border_size;
  RECT rect;
  GetClientRect(&rect);
  inner_size.x = rect.right-rect.left;
  inner_size.y = rect.bottom-rect.top;
  kdu_coords target_size = inner_size;
  if (pixel_scale != last_pixel_scale)
    {
      target_size.x = (target_size.x * pixel_scale) / last_pixel_scale;
      target_size.y = (target_size.y * pixel_scale) / last_pixel_scale;
      last_pixel_scale = pixel_scale;
    }
  if (target_size.x > max_view_size.x)
    target_size.x = max_view_size.x;
  if (target_size.y > max_view_size.y)
    target_size.y = max_view_size.y;
  target_size.x -= target_size.x % pixel_scale;
  target_size.y -= target_size.y % pixel_scale;
  if (inner_size != target_size)
    { // Need to resize windows -- ugh.
      sizing = true; // Guard against unwanted recursion.

      CWnd *parent = GetParent();
      int iteration = 0;
      do {
          iteration++;
          // First determine the difference between the active child client area
          // and the frame window's outer boundary.  This total border area may
          // change during resizing, which is the reason for the loop.
          parent->GetWindowRect(&rect);
          outer_size.x = rect.right-rect.left;
          outer_size.y = rect.bottom-rect.top;
          
          border_size = outer_size - inner_size;
          if (iteration > 4)
            {
              target_size = outer_size - border_size;
              if (target_size.x < pixel_scale)
                target_size.x = pixel_scale;
              if (target_size.y < pixel_scale)
                target_size.y = pixel_scale;
            }
          outer_size = target_size + border_size; // Assuming no border change
                    iteration++;
          if (outer_size.x > screen_size.x)
            outer_size.x = screen_size.x;
          if (outer_size.y > screen_size.y)
            outer_size.y = screen_size.y;
          target_size = outer_size - border_size;
          parent->SetWindowPos(NULL,0,0,outer_size.x,outer_size.y,
                               SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);

          GetClientRect(&rect);
          inner_size.x = rect.right-rect.left;
          inner_size.y = rect.bottom-rect.top;
        } while ((inner_size.x < target_size.x) ||
                 (inner_size.y < target_size.y));
      target_size = inner_size; // If we could not achieve our original target,
      sizing = false;           // the child region will have to be a bit larger
    }

  last_size = inner_size;
  if (app != NULL)
    {
      CWnd *parent = GetParent();
      kdu_coords tmp = last_size;
      tmp.x = 1 + ((tmp.x-1)/pixel_scale);
      tmp.y = 1 + ((tmp.y-1)/pixel_scale);
      app->set_view_size(tmp);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CChildView message handlers

BEGIN_MESSAGE_MAP(CChildView,CWnd )
	//{{AFX_MSG_MAP(CChildView)
	ON_WM_PAINT()
	ON_WM_KEYDOWN()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_MOUSEMOVE()
  ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_KILLFOCUS()
	ON_WM_MOUSEACTIVATE()
	ON_WM_LBUTTONUP()
	ON_WM_KEYUP()
	//}}AFX_MSG_MAP
  ON_WM_MOUSEWHEEL()
//  ON_WM_WINDOWPOSCHANGED()
//  ON_WM_WINDOWPOSCHANGING()
//ON_WM_WINDOWPOSCHANGED()
END_MESSAGE_MAP()


/******************************************************************************/
/*                        CChildView::PreCreateWindow                         */
/******************************************************************************/

BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs) 
{
  if (!CWnd::PreCreateWindow(cs))
    return FALSE;

  cs.style &= ~WS_BORDER;
  cs.lpszClass = 
    AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
                        ::LoadCursor(NULL, IDC_ARROW),
                        HBRUSH(COLOR_WINDOW+1), NULL);
  return TRUE;
}

/******************************************************************************/
/*                            CChildView::OnPaint                             */
/******************************************************************************/

void CChildView::OnPaint() 
{
  CPaintDC dc(this); // device context for painting
  // Convert PAINTSTRUCT region to a `kdu_dims' structure.  The windows
  // RECT structure is poorly defined, but it turns out that the `bottom'
  // and `right' members refer to the point immediately beyond the lower right
  // hand corner of the rectangle (not the corner itself).  To minimize
  // confusion, we avoid using RECT structures ourself.
  kdu_dims region;
  region.pos.x = dc.m_ps.rcPaint.left / pixel_scale;
  region.pos.y = dc.m_ps.rcPaint.top / pixel_scale;
  region.size.x = (dc.m_ps.rcPaint.right+pixel_scale-1)/pixel_scale
                - region.pos.x;
  region.size.y = (dc.m_ps.rcPaint.bottom+pixel_scale-1)/pixel_scale
                - region.pos.y;
  if (app != NULL)
    app->paint_region(&dc,region,false);
}

/******************************************************************************/
/*                           CChildView::OnHScroll                            */
/******************************************************************************/

void CChildView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
  if (app == NULL)
    return;
  if (nSBCode == SB_THUMBPOSITION)
    {
      SCROLLINFO sc_info;
      GetScrollInfo(SB_HORZ,&sc_info);
      app->set_hscroll_pos(sc_info.nTrackPos);
    }
  else if (nSBCode == SB_LINELEFT)
    app->set_hscroll_pos(-scroll_step.x,true);
  else if (nSBCode == SB_LINERIGHT)
    app->set_hscroll_pos(scroll_step.x,true);
  else if (nSBCode == SB_PAGELEFT)
    app->set_hscroll_pos(-scroll_page.x,true);
  else if (nSBCode == SB_PAGERIGHT)
    app->set_hscroll_pos(scroll_page.x,true);
  else if (nSBCode == SB_LEFT)
    app->set_hscroll_pos(0);
  else if (nSBCode == SB_RIGHT)
    app->set_hscroll_pos(scroll_end.x);
}

/******************************************************************************/
/*                           CChildView::OnVScroll                            */
/******************************************************************************/

void CChildView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
  if (app == NULL)
    return;
  if (nSBCode == SB_THUMBPOSITION)
    {
      SCROLLINFO sc_info;
      GetScrollInfo(SB_VERT,&sc_info);
      app->set_vscroll_pos(sc_info.nTrackPos);
    }
  else if (nSBCode == SB_LINEUP)
    app->set_vscroll_pos(-scroll_step.y,true);
  else if (nSBCode == SB_LINEDOWN)
    app->set_vscroll_pos(scroll_step.y,true);
  else if (nSBCode == SB_PAGEUP)
    app->set_vscroll_pos(-scroll_page.y,true);
  else if (nSBCode == SB_PAGEDOWN)
    app->set_vscroll_pos(scroll_page.y,true);
  else if (nSBCode == SB_TOP)
    app->set_vscroll_pos(0);
  else if (nSBCode == SB_BOTTOM)
    app->set_vscroll_pos(scroll_end.y);
}

/******************************************************************************/
/*                           CChildView::OnKeyDown                            */
/******************************************************************************/

void CChildView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  if (focusing)
    cancel_focus_drag();
  if (app != NULL)
    { // Look out for keys which are not bound to the menu -- currently the only
      // keys of this form are those with obvious scrolling functions and the
      // escape key.
      if (nChar == VK_LEFT)
        app->set_hscroll_pos(-scroll_step.x,true);
      else if (nChar == VK_RIGHT)
        app->set_hscroll_pos(scroll_step.x,true);
      else if (nChar == VK_UP)
        app->set_vscroll_pos(-scroll_step.y,true);
      else if (nChar == VK_DOWN)
        app->set_vscroll_pos(scroll_step.y,true);
      else if (nChar == VK_PRIOR)
        app->set_vscroll_pos(-scroll_page.y,true);
      else if (nChar == VK_NEXT)
        app->set_vscroll_pos(scroll_page.y,true);
      else if (nChar == VK_CONTROL)
        {
          if (panning)
            {
              SetCursor(normal_cursor);
              panning = false;
            }
          kdu_coords tmp = last_mouse_point;
          tmp.x /= pixel_scale;   tmp.y /= pixel_scale;
          showing_metainfo = app->show_metainfo(tmp);
          if (showing_metainfo)
            SetCursor(overlay_cursor);
        }
    }
  CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

/******************************************************************************/
/*                             CChildView::OnKeyUp                            */
/******************************************************************************/

void
  CChildView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  if ((nChar == VK_CONTROL) && showing_metainfo)
    {
      showing_metainfo = false;
      app->kill_metainfo();
      SetCursor(normal_cursor);
    }
  CWnd::OnKeyUp(nChar, nRepCnt, nFlags);
}

/******************************************************************************/
/*                         CChildView::OnLButtonDblClk                        */
/******************************************************************************/

void
  CChildView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
  kdu_coords mouse_point;
  mouse_point.x = point.x;  mouse_point.y = point.y;
  app->edit_metainfo(kdu_coords(mouse_point.x/pixel_scale,
                                mouse_point.y/pixel_scale));
}

/******************************************************************************/
/*                          CChildView::OnLButtonDown                         */
/******************************************************************************/

void
  CChildView::OnLButtonDown(UINT nFlags, CPoint point) 
{
  last_mouse_point.x = point.x;
  last_mouse_point.y = point.y;
  if ((nFlags & MK_CONTROL) != 0)
    {
      if ((nFlags & MK_SHIFT) != 0)
        {
          kdu_coords mouse_point;
          mouse_point.x = point.x;  mouse_point.y = point.y;
          app->set_compositing_layer(kdu_coords(mouse_point.x/pixel_scale,
                                                mouse_point.y/pixel_scale));
        }
      else if (showing_metainfo &&
               app->edit_metainfo(kdu_coords(last_mouse_point.x/pixel_scale,
                                             last_mouse_point.y/pixel_scale)))
        {
          showing_metainfo = false;
          SetCursor(normal_cursor);
          app->kill_metainfo();
        }
    }
  else if ((nFlags & MK_SHIFT) != 0)
    {
      panning = true;
      mouse_pressed_point = point;
    }
  else
    { // Start focusing operation
      focusing = true;
      focus_start.x = point.x;
      focus_start.y = point.y;
      focus_end = focus_start;
      focus_rect.left = point.x;
      focus_rect.right = point.x+1;
      focus_rect.top = point.y;
      focus_rect.bottom = point.y+1;
      SIZE border; border.cx = border.cy = 2;
      CDC *dc = GetDC();
      dc->DrawDragRect(&focus_rect,border,NULL,border);
      ReleaseDC(dc);
      app->suspend_processing(true);
      SetCapture();
      SetCursor(focusing_cursor);
    }
  CWnd::OnLButtonDown(nFlags, point);
}

/******************************************************************************/
/*                          CChildView::OnRButtonDown                         */
/******************************************************************************/

void CChildView::OnRButtonDown(UINT nFlags, CPoint point) 
{
  if (focusing)
    cancel_focus_drag();
}

/******************************************************************************/
/*                           CChildView::OnLButtonUp                          */
/******************************************************************************/

void
  CChildView::OnLButtonUp(UINT nFlags, CPoint point) 
{
  if (panning)
    {
      SetCursor(normal_cursor);
      panning = false;
    }
  else if (focusing)
    { // End focusing operation
      SIZE border; border.cx = border.cy = 2;
      SIZE zero_border; zero_border.cx = zero_border.cy = 0;
      CDC *dc = GetDC();
      dc->DrawDragRect(&focus_rect,zero_border,&focus_rect,border);
      ReleaseDC(dc);
      focusing = false;

      kdu_dims new_box;
      new_box.pos = focus_start; new_box.size = focus_end - focus_start;
      if (new_box.size.x < 0)
        { new_box.pos.x = focus_end.x; new_box.size.x = -new_box.size.x; }
      if (new_box.size.y < 0)
        { new_box.pos.y = focus_end.y; new_box.size.y = -new_box.size.y; }
      new_box.pos.x = new_box.pos.x / pixel_scale;
      new_box.pos.y = new_box.pos.y / pixel_scale;
      new_box.size.x = (new_box.size.x+pixel_scale-1)/pixel_scale;
      new_box.size.y = (new_box.size.y+pixel_scale-1)/pixel_scale;
      if (new_box.area() > 15)
        app->set_focus_box(new_box);
      app->suspend_processing(false);
      ReleaseCapture();
      SetCursor(normal_cursor);
    }
  CWnd::OnLButtonUp(nFlags, point);
}

/******************************************************************************/
/*                        CChildView::cancel_focus_drag                       */
/******************************************************************************/

void
  CChildView::cancel_focus_drag()
{
  if (!focusing)
    return;
  SIZE border; border.cx = border.cy = 2;
  SIZE zero_border; zero_border.cx = zero_border.cy = 0;
  CDC *dc = GetDC();
  dc->DrawDragRect(&focus_rect,zero_border,&focus_rect,border);
  ReleaseDC(dc);
  focusing = false;
  app->suspend_processing(false);
  ReleaseCapture();
  SetCursor(normal_cursor);
}

/******************************************************************************/
/*                           CChildView::OnMouseMove                          */
/******************************************************************************/

void
  CChildView::OnMouseMove(UINT nFlags, CPoint point) 
{
  last_mouse_point.x = point.x;
  last_mouse_point.y = point.y;
  if (nFlags & MK_CONTROL)
    {
      if (focusing)
        cancel_focus_drag();
      if (panning)
        {
          SetCursor(normal_cursor);
          panning = false;
        }
      if (app != NULL)
        showing_metainfo =
          app->show_metainfo(kdu_coords(last_mouse_point.x/pixel_scale,
                                        last_mouse_point.y/pixel_scale));
      if (showing_metainfo)
        SetCursor(overlay_cursor);
    }
  else if (panning)
    {
      SetCursor(panning_cursor);
      if (app != NULL)
        {
          app->set_scroll_pos(
               (2*(mouse_pressed_point.x-point.x)+pixel_scale-1)/pixel_scale,
               (2*(mouse_pressed_point.y-point.y)+pixel_scale-1)/pixel_scale,
               true);
          mouse_pressed_point = point;
        }
    }
  else if (focusing)
    {
      focus_end.x = point.x;
      focus_end.y = point.y;
      RECT new_rect;
      if (focus_start.x < focus_end.x)
        { new_rect.left = focus_start.x; new_rect.right = focus_end.x; }
      else
        { new_rect.left = focus_end.x; new_rect.right = focus_start.x; }
      if (focus_start.y < focus_end.y)
        { new_rect.top = focus_start.y; new_rect.bottom = focus_end.y; }
      else
        { new_rect.top = focus_end.y; new_rect.bottom = focus_start.y; }
      new_rect.right++; new_rect.bottom++;
      SIZE border; border.cx = border.cy = 2;
      CDC *dc = GetDC();
      dc->DrawDragRect(&new_rect,border,&focus_rect,border);
      ReleaseDC(dc);
      focus_rect = new_rect;
    }
}

/******************************************************************************/
/*                          CChildView::OnKillFocus                           */
/******************************************************************************/

void
  CChildView::OnKillFocus(CWnd* pNewWnd) 
{
  CWnd ::OnKillFocus(pNewWnd);
  if (focusing)
    cancel_focus_drag();
  if (panning)
    {
      SetCursor(normal_cursor);
      panning = false;
    }
  if (showing_metainfo)
    {
      showing_metainfo = false;
      SetCursor(normal_cursor);
      app->kill_metainfo();
    }
}

/******************************************************************************/
/*                         CChildView::OnMouseActivate                        */
/******************************************************************************/

int
  CChildView::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message) 
{
  if (this != GetFocus())
    { // Mouse click is activating window; prevent further mouse messages
      CWnd::OnMouseActivate(pDesktopWnd,nHitTest,message);
      return MA_ACTIVATEANDEAT;
    }
  else
    return CWnd::OnMouseActivate(pDesktopWnd,nHitTest,message);
}

/******************************************************************************/
/*                         CChildView::OnMouseWheel                           */
/******************************************************************************/

BOOL
  CChildView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
  if (app == NULL)
    return false;
  if ( nFlags & MK_CONTROL )  // Zoom
    if ( zDelta > 0 )
      app->OnViewZoomIn();
    else
      app->OnViewZoomOut();
  else                        // Scroll
    if (nFlags & MK_SHIFT)          // Horizontal
      app->set_hscroll_pos(zDelta/10,true);
    else                            // Vertical
      app->set_vscroll_pos(zDelta/10,true);
  return TRUE;
}
