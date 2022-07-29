/******************************************************************************/
// File: MainFrm.cpp [scope = APPS/SHOW]
// Version: Kakadu, V6.0
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
   Implementation of a small number of frame window functions for the
interactive JPEG2000 image viewer application, "kdu_show".  Client-area windows
messages are processed by the child view object, implemented in
 "ChildView.cpp".
*******************************************************************************/

#include "stdafx.h"
#include "kdu_show.h"

#include "MainFrm.h"
#include ".\mainfrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
	ON_WM_DROPFILES()
	ON_WM_TIMER()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
//  ON_WM_WINDOWPOSCHANGED()
//ON_WM_WINDOWPOSCHANGED()
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	// ID_INDICATOR_CAPS,
	// ID_INDICATOR_NUM,
	// ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

/******************************************************************************/
/*                          CMainFrame::CMainFrame                            */
/******************************************************************************/

CMainFrame::CMainFrame()
{
  app = NULL;
}

/******************************************************************************/
/*                          CMainFrame::~CMainFrame                           */
/******************************************************************************/

CMainFrame::~CMainFrame()
{
}

/******************************************************************************/
/*                           CMainFrame::OnCreate                             */
/******************************************************************************/

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    return -1;

  // create a view to occupy the client area of the frame
  if (!m_wndView.Create(NULL, NULL,
                        AFX_WS_DEFAULT_VIEW | WS_HSCROLL | WS_VSCROLL,
                        CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
    {
      TRACE0("Failed to create view window\n");
      return -1;
    }

  // create a toolbar
  toolbar_scale_x2 = false;
  if ((!m_wndToolBar.Create(this,CBRS_TOP|WS_VISIBLE)) ||
      (!m_wndToolBar.LoadToolBar(IDR_TOOLBAR1)))
    {
      TRACE0("Failed to create toolbar\n");
      return -1;      // fail to create
    }

  // create a status bar
  if (!m_wndStatusBar.Create(this) ||
      !m_wndStatusBar.SetIndicators(indicators,
      sizeof(indicators)/sizeof(UINT)))
    {
      TRACE0("Failed to create status bar\n");
      return -1;      // fail to create
    }
  return 0;
}

/******************************************************************************/
/*                        CMainFrame::select_toolbar                          */
/******************************************************************************/

void
  CMainFrame::select_toolbar(bool scale_x2)
{
  if (scale_x2 == toolbar_scale_x2)
    return;
  toolbar_scale_x2 = scale_x2;
  if (scale_x2)
    m_wndToolBar.LoadToolBar(IDR_TOOLBAR2);
  else
    m_wndToolBar.LoadToolBar(IDR_TOOLBAR1);
}

/******************************************************************************/
/*                        CMainFrame::PreCreateWindow                         */
/******************************************************************************/

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
  if( !CFrameWnd::PreCreateWindow(cs) )
    return FALSE;
  cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
  cs.lpszClass = AfxRegisterWndClass(0);
  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
  CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
  CFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

/******************************************************************************/
/*                           CMainFrame::OnSetFocus                           */
/******************************************************************************/

void CMainFrame::OnSetFocus(CWnd* pOldWnd)
{
  m_wndView.SetFocus();
}

/******************************************************************************/
/*                            CMainFrame::OnCmdMsg                            */
/******************************************************************************/

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra,
                          AFX_CMDHANDLERINFO* pHandlerInfo)
{
  // let the view have first crack at the command
  if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
    return TRUE;

  // otherwise, do default handling
  return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

/******************************************************************************/
/*                             CMainFrame::OnSize                             */
/******************************************************************************/

void CMainFrame::OnSize(UINT nType, int cx, int cy) 
{
  CFrameWnd::OnSize(nType, cx, cy); // Let the framework resize the client first
  if (nType == SIZE_RESTORED)
    m_wndView.check_and_report_size();
}

/******************************************************************************/
/*                           CMainFrame::OnDropFiles                          */
/******************************************************************************/

void CMainFrame::OnDropFiles(HDROP hDropInfo) 
{
  char fname[1024];
  DragQueryFile(hDropInfo,0,fname,1023);
  DragFinish(hDropInfo);
  if (app != NULL)
    app->open_file(fname);
}

/******************************************************************************/
/*                             CMainFrame::OnTimer                            */
/******************************************************************************/
#if (defined _MSC_VER && (_MSC_VER>=1300))
  void
    CMainFrame::OnTimer(UINT_PTR nIDEvent) 
  {
    if (nIDEvent == KDU_SHOW_REFRESH_TIMER_IDENT)
      app->refresh_display();
    else if (nIDEvent == KDU_SHOW_FLASH_TIMER_IDENT)
      app->flash_overlays();
  }
#else // Older versions of the compiler not equipped for Win64 compatibility
  void
    CMainFrame::OnTimer(UINT nIDEvent) 
  {
    if (nIDEvent == KDU_SHOW_REFRESH_TIMER_IDENT)
      app->refresh_display();
    else if (nIDEvent == KDU_SHOW_FLASH_TIMER_IDENT)
      app->flash_overlays();
  }
#endif // _MSC_VER < 1300
