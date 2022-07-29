/******************************************************************************/
// File: ChildView.h [scope = APPS/SHOW]
// Version: Kakadu, V6.1
// Author: David Taubman
// Last Revised: 12 August, 2007
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
/*******************************************************************************
Description:
   MFC-based class definitions and message mapping macros for the single child
view window in the interactive JPEG2000 image viewer, "kdu_show".
*******************************************************************************/

#if !defined(AFX_CHILDVIEW_H__9C359608_5C42_42FF_BE3C_E011ABAAF4AA__INCLUDED_)
#define AFX_CHILDVIEW_H__9C359608_5C42_42FF_BE3C_E011ABAAF4AA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CKdu_showApp; // Forward declaration.

/////////////////////////////////////////////////////////////////////////////
// CChildView window

class CChildView : public CWnd
{
// Construction
public:
  CChildView();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildView)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
  virtual ~CChildView();

	// Generated message map functions
protected:
	//{{AFX_MSG(CChildView)
	afx_msg void OnPaint();
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg int OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
// ----------------------------------------------------------------------------
public: // Access methods for use by the frame window
  void set_app(CKdu_showApp *app);
  void set_max_view_size(kdu_coords size, int pixel_scale);
    /* Called by the application object whenever the image dimensions
       change for one reason or another.  This is used to prevent the
       window from growing larger than the dimensions of the image (unless
       the image is too tiny).  During periods when no image is loaded, the
       maximum view size should be set sufficiently large that it does not
       restrict window resizing.  The `size' value refers to the number
       of image pixels which are available for display.  Each of these
       occupies `pixel_scale' by `pixel_scale' display pixels.*/
  void set_scroll_metrics(kdu_coords step, kdu_coords page, kdu_coords end)
    { /* This function is called by the application to set or adjust the
         interpretation of incremental scrolling commands.  In particular, these
         parameters are adjusted whenever the window size changes.
            The `step' coordinates identify the amount to scroll by when an
         arrow key is used, or the scrolling arrows are clicked.
            The `page' coordinates identify the amount to scroll by when the
         page up/down keys are used or the empty scroll bar region is clicked.
            The `end' coordinates identify the maximum scrolling position. */
      scroll_step = step;
      scroll_page = page;
      scroll_end = end;
    }
  void check_and_report_size();
    /* Called when the window size may have changed or may need to be
       adjusted to conform to maximum dimensions established by the
       image -- e.g., called from inside the frame's OnSize message handler,
       or when the image dimensions may have changed. */
  void cancel_focus_drag();
    /* Called if the user cancels an in-progress focus box definition
       operation (mouse click down, drag, mouse click again to finish)
       by pressing the escape key, or any other method which may be
       provided. */
private: // Data
  CKdu_showApp *app;
  int pixel_scale; // Display pixels per image pixel
  int last_pixel_scale; // So we can resize display in `check_and_report_size'
  kdu_coords screen_size; // Dimensions of the display
  kdu_coords max_view_size; // Max size for client area (in display pixels)
  kdu_coords last_size;
  bool sizing; // Flag to prevent recursive calls to resizing functions.
  kdu_coords scroll_step, scroll_page, scroll_end;
  kdu_coords last_mouse_point; // Last known mouse location

  bool focusing; // True only when defining focus box region with mouse clicks
  kdu_coords focus_start; // Coordinates of start point in focusing operation
  kdu_coords focus_end; // Coordinates of end point in focusing operation
  RECT focus_rect; // Most recent focus box during focusing.

  bool panning;
  CPoint mouse_pressed_point;

  bool showing_metainfo; // If the `overlay_cursor' is on.

  HCURSOR normal_cursor;
  HCURSOR focusing_cursor;
  HCURSOR panning_cursor;
  HCURSOR overlay_cursor; // Cursor used when mouse is over a metadata overlay
public:
  afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHILDVIEW_H__9C359608_5C42_42FF_BE3C_E011ABAAF4AA__INCLUDED_)
