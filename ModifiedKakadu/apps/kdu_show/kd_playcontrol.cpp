/******************************************************************************/
// File: kd_playcontrol.cpp [scope = APPS/SHOW]
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
   Implementation of the animation/video play controls for "kdu_show".
*******************************************************************************/

#include "stdafx.h"
#include "kdu_show.h"
#include "kd_playcontrol.h"
#include ".\kd_playcontrol.h"
#include <math.h>


/* ========================================================================== */
/*                             kd_playcontrol                                 */
/* ========================================================================== */

/******************************************************************************/
/*                     kd_playcontrol::kd_playcontrol                         */
/******************************************************************************/

kd_playcontrol::kd_playcontrol(CKdu_showApp *app, CWnd* pParent)
	: CDialog()
{
  destroy_in_progress = false;
  this->app = app;
  track_idx = num_frames = frame_idx = -1;
  use_native_time_base = true;
  base_multiplier = 1.0F;
  frame_seconds_left = nominal_frame_period = actual_frame_period = -1.0F;
  in_playmode = false;
  play_repeat = false;
  update_timer = 0.0F;
  Create(kd_playcontrol::IDD, pParent);
  get_rate_x1_ctrl()->SetCheck(BST_CHECKED);
  get_base_fps_ctrl()->SetWindowText("30.0");
  update_all_controls();
  ShowWindow(SW_SHOW);
}

/******************************************************************************/
/*                    kd_playcontrol::~kd_playcontrol                         */
/******************************************************************************/

kd_playcontrol::~kd_playcontrol()
{
}

/******************************************************************************/
/*                         kd_playcontrol::update                             */
/******************************************************************************/

void
  kd_playcontrol::update(int track_idx, int num_frames, int frame_idx,
                         float frame_remains, float last_nominal_period,
                         float last_actual_period)
{
  if (destroy_in_progress)
    return;
  bool frame_change = (frame_idx != this->frame_idx);
  bool track_change = (track_idx != this->track_idx);
  if ((this->frame_seconds_left < 0.5F) && (frame_remains < 0.5F))
    frame_remains = this->frame_seconds_left = -1.0F;
  bool adjust_frame_remains = (frame_remains != this->frame_seconds_left);
  if ((num_frames == this->num_frames) &&
      !(track_change || frame_change || adjust_frame_remains))
    return;
  this->track_idx = track_idx;
  this->num_frames = num_frames;
  this->frame_idx = frame_idx;
  if (in_playmode)
    {
      this->frame_seconds_left = frame_remains;
      if ((nominal_frame_period < 0.0F) || (actual_frame_period < 0.0F))
        {
          nominal_frame_period = last_nominal_period;
          actual_frame_period = last_actual_period;
        }
      else if ((last_nominal_period >= 0.0F) && (last_actual_period >= 0.0F))
        {
          nominal_frame_period =
            nominal_frame_period*0.8F + last_nominal_period*0.2F;
          actual_frame_period =
            actual_frame_period*0.8F + last_actual_period*0.2F;
        }
      if (track_change)
        stop_playmode();
      else
        { // Just update the frame index and statistics
          if (frame_remains < 0.0F)
            { // Try to avoid very frequent updates
              if (frame_change)
                update_timer -= last_actual_period;
              if (update_timer > 0.0F)
                return;
              update_timer += 0.2F;
            }
          char buf[32];
          sprintf(buf,"%d",frame_idx+1);
          get_frame_ctrl()->SetWindowText(buf);
          if ((nominal_frame_period > 0.0F) && (actual_frame_period > 0.0F))
            sprintf(buf,"%3.0f",100*nominal_frame_period/actual_frame_period);
          else
            buf[0] = '\0';
          get_ontrack_percent_ctrl()->SetWindowText(buf);
          if (frame_remains > 0.0F)
            sprintf(buf,"%d",(int) ceil(this->frame_seconds_left));
          else if (frame_change)
            sprintf(buf,"%5.3f",last_nominal_period);
          else
            buf[0] = '\0';
          get_frame_period_ctrl()->SetWindowText(buf);
        }
    }
  else
    update_all_controls();
}

/******************************************************************************/
/*                   kd_playcontrol::update_all_controls                      */
/******************************************************************************/

void
  kd_playcontrol::update_all_controls()
{
  if (destroy_in_progress)
    return;

  char buf[32];

  if (track_idx < 0)
    {
      in_playmode = false;
      get_track_ctrl()->SetWindowText("----");
      get_frame_ctrl()->SetWindowText("----");
      get_base_native_ctrl()->EnableWindow(FALSE);
      get_base_custom_ctrl()->EnableWindow(FALSE);
      get_rate_x1_ctrl()->EnableWindow(FALSE);
      get_rate_x2_ctrl()->EnableWindow(FALSE);
      get_rate_d2_ctrl()->EnableWindow(FALSE);
      get_rate_d4_ctrl()->EnableWindow(FALSE);
      get_track_next_ctrl()->EnableWindow(FALSE);
      get_reset_ctrl()->EnableWindow(FALSE);
      get_stop_ctrl()->EnableWindow(FALSE);
      get_play_ctrl()->EnableWindow(FALSE);
      get_fwd1_ctrl()->EnableWindow(FALSE);
      get_fwd5_ctrl()->EnableWindow(FALSE);
      get_back1_ctrl()->EnableWindow(FALSE);
      get_back5_ctrl()->EnableWindow(FALSE);
    }
  else
    {
      if (track_idx == 0)
        {
          get_track_ctrl()->SetWindowText("JPX animation");
          get_track_next_ctrl()->EnableWindow(FALSE);
        }
      else
        {
          sprintf(buf,"%d",track_idx);
          get_track_ctrl()->SetWindowText(buf);
          get_track_next_ctrl()->EnableWindow(TRUE);
        }
      get_base_native_ctrl()->EnableWindow(TRUE);
      get_base_custom_ctrl()->EnableWindow(TRUE);
      get_rate_x1_ctrl()->EnableWindow(TRUE);
      get_rate_x2_ctrl()->EnableWindow(TRUE);
      get_rate_d2_ctrl()->EnableWindow(TRUE);
      get_rate_d4_ctrl()->EnableWindow(TRUE);
      get_reset_ctrl()->EnableWindow((frame_idx==0)?FALSE:TRUE);
      get_stop_ctrl()->EnableWindow((in_playmode)?TRUE:FALSE);
      if (in_playmode ||
          ((frame_idx==(num_frames-1)) && !(play_repeat && (num_frames>1))))
        get_play_ctrl()->EnableWindow(FALSE);
      else
        get_play_ctrl()->EnableWindow(TRUE);
      get_fwd1_ctrl()->EnableWindow((frame_idx==(num_frames-1))?FALSE:TRUE);
      get_fwd5_ctrl()->EnableWindow((frame_idx==(num_frames-1))?FALSE:TRUE);
      get_back1_ctrl()->EnableWindow((frame_idx>0)?TRUE:FALSE);
      get_back5_ctrl()->EnableWindow((frame_idx>0)?TRUE:FALSE);
      sprintf(buf,"%d",frame_idx+1);
      get_frame_ctrl()->SetWindowText(buf);
    }

  if (use_native_time_base)
    {
      get_base_native_ctrl()->SetCheck(BST_CHECKED);
      get_base_fps_ctrl()->EnableWindow(FALSE);
    }
  else
    {
      get_base_custom_ctrl()->SetCheck(BST_CHECKED);
      get_base_fps_ctrl()->EnableWindow((track_idx>=0)?TRUE:FALSE);
    }

  get_repeat_ctrl()->SetCheck((play_repeat)?BST_CHECKED:BST_UNCHECKED);
  
  if ((nominal_frame_period > 0.0F) && (actual_frame_period > 0.0F))
    sprintf(buf,"%3.0f",100*nominal_frame_period/actual_frame_period);
  else
    buf[0] = '\0';
  get_ontrack_percent_ctrl()->SetWindowText(buf);

  if (frame_seconds_left < 0.0F)
    buf[0] = '\0';
  else
    sprintf(buf,"%d",(int) ceil(frame_seconds_left));
  get_frame_period_ctrl()->SetWindowText(buf);

  update_timer = 0.0F;
}

/******************************************************************************/
/*                     kd_playcontrol::stop_playmode                          */
/******************************************************************************/

void
  kd_playcontrol::stop_playmode()
{
  if (!in_playmode)
    return;
  in_playmode = false; // Essential to call this before `app->stop_playmode'
  if (app != NULL)
    app->stop_playmode();
  frame_seconds_left = nominal_frame_period = actual_frame_period = -1.0F;
  update_timer = 0.0F;
  update_all_controls();
}

/******************************************************************************/
/*                     kd_playcontrol::DoDataExchange                         */
/******************************************************************************/

void kd_playcontrol::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(kd_playcontrol, CDialog)
  ON_WM_CLOSE()
  ON_BN_CLICKED(IDC_PLAY_NEXT_TRACK, OnBnClickedPlayNextTrack)
  ON_BN_CLICKED(IDC_PLAY_BASE_NATIVE, OnBnClickedPlayBaseNative)
  ON_BN_CLICKED(IDC_PLAY_BASE_CUSTOM, OnBnClickedPlayBaseCustom)
  ON_BN_CLICKED(IDC_PLAY_X1, OnBnClickedPlayX1)
  ON_BN_CLICKED(IDC_PLAY_X2, OnBnClickedPlayX2)
  ON_BN_CLICKED(IDC_PLAY_D2, OnBnClickedPlayD2)
  ON_BN_CLICKED(IDC_PLAY_D4, OnBnClickedPlayD4)
  ON_BN_CLICKED(IDC_PLAY_RESET, OnBnClickedPlayReset)
  ON_BN_CLICKED(IDC_PLAY_STOP, OnBnClickedPlayStop)
  ON_BN_CLICKED(IDC_PLAY_PLAY, OnBnClickedPlayPlay)
  ON_BN_CLICKED(IDC_PLAY_FWD1, OnBnClickedPlayFwd1)
  ON_BN_CLICKED(IDC_PLAY_FWD5, OnBnClickedPlayFwd5)
  ON_BN_CLICKED(IDC_PLAY_BACK1, OnBnClickedPlayBack1)
  ON_BN_CLICKED(IDC_PLAY_BACK5, OnBnClickedPlayBack5)
  ON_BN_CLICKED(IDC_PLAY_REPEAT, OnBnClickedPlayRepeat)
END_MESSAGE_MAP()

/******************************************************************************/
/*                      kd_playcontrol::PostNCDestroy                         */
/******************************************************************************/

void kd_playcontrol::PostNcDestroy()
{
  destroy_in_progress = true;
  if (app != NULL)
    {
      assert(app->playcontrol == this);
      if (in_playmode)
        app->stop_playmode();
      app->playcontrol = NULL;
      app = NULL;
    }
  CDialog::PostNcDestroy();
  delete this;
}

/******************************************************************************/
/*                         kd_playcontrol::OnClose                            */
/******************************************************************************/

void kd_playcontrol::OnClose()
{
  DestroyWindow();
}

/******************************************************************************/
/*                           kd_playcontrol::OnOK                             */
/******************************************************************************/

void kd_playcontrol::OnOK()
{
  if (GetFocus()->m_hWnd == get_base_fps_ctrl()->m_hWnd)
    {
       if (in_playmode)
         stop_playmode();
       OnBnClickedPlayPlay();
    }
}

/******************************************************************************/
/*                 kd_playcontrol::OnBnClickedPlayNextTrack                   */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayNextTrack()
{
  if (app != NULL)
    app->OnTrackNext();
}

/******************************************************************************/
/*                 kd_playcontrol::OnBnClickedPlayBaseNative                  */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayBaseNative()
{
  if (use_native_time_base)
    return; // No change
  use_native_time_base = true;
  update_all_controls();
  stop_playmode();
}

/******************************************************************************/
/*                 kd_playcontrol::OnBnClickedPlayBaseCustom                  */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayBaseCustom()
{
  if (!use_native_time_base)
    return; // No change
  use_native_time_base = false;
  update_all_controls();
  stop_playmode();
  get_base_fps_ctrl()->SetFocus();
  get_base_fps_ctrl()->SetSel(0,-1);
}

/******************************************************************************/
/*                     kd_playcontrol::OnBnClickedPlayX1                      */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayX1()
{
  this->base_multiplier = 1.0F;
  stop_playmode();
}

/******************************************************************************/
/*                     kd_playcontrol::OnBnClickedPlayX2                      */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayX2()
{
  this->base_multiplier = 2.0F;
  stop_playmode();
}

/******************************************************************************/
/*                     kd_playcontrol::OnBnClickedPlayD2                      */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayD2()
{
  this->base_multiplier = 0.5F;
  stop_playmode();
}

/******************************************************************************/
/*                     kd_playcontrol::OnBnClickedPlayD4                      */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayD4()
{
  this->base_multiplier = 0.25F;
  stop_playmode();
}

/******************************************************************************/
/*                    kd_playcontrol::OnBnClickedPlayReset                    */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayReset()
{
  stop_playmode();
  if (app != NULL)
    app->set_frame(0);
}

/******************************************************************************/
/*                    kd_playcontrol::OnBnClickedPlayStop                     */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayStop()
{
  if (in_playmode)
    stop_playmode();
}

/******************************************************************************/
/*                    kd_playcontrol::OnBnClickedPlayPlay                     */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayPlay()
{
  if (in_playmode || (app == NULL))
    return;
  if ((num_frames > 0) && (frame_idx >= (num_frames-1)) &&
      !(play_repeat && (num_frames > 1)))
    return;

  float custom_fps = 1.0F;
  if (!use_native_time_base)
    {
      char buf[32];
      int len = get_base_fps_ctrl()->GetLine(0,buf,31);
      buf[len] = '\0';
      if ((sscanf(buf,"%f",&custom_fps) != 1) || (custom_fps <= 0.0F))
        {
          MessageBox("Enter a valid base frames/s value or select native "
                     "base timing.","Dialog entry error",
                     MB_OK|MB_ICONERROR);
          get_base_fps_ctrl()->SetFocus();
          get_base_fps_ctrl()->SetSel(0,-1);
          return;
        }
    }
  in_playmode = true;
  frame_seconds_left = nominal_frame_period = actual_frame_period = -1.0F;
  update_timer = 0.0F;
  app->start_playmode(use_native_time_base,custom_fps,base_multiplier,
                      play_repeat);
  update_all_controls();
}

/******************************************************************************/
/*                    kd_playcontrol::OnBnClickedPlayFwd1                     */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayFwd1()
{
  stop_playmode();
  if (app != NULL)
    app->set_frame(frame_idx+1);
}

/******************************************************************************/
/*                    kd_playcontrol::OnBnClickedPlayFwd5                     */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayFwd5()
{
  stop_playmode();
  if (app != NULL)
    app->set_frame(frame_idx+5);
}

/******************************************************************************/
/*                    kd_playcontrol::OnBnClickedPlayBack1                    */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayBack1()
{
  stop_playmode();
  if (app != NULL)
    app->set_frame(frame_idx-1);
}

/******************************************************************************/
/*                    kd_playcontrol::OnBnClickedPlayBack5                    */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayBack5()
{
  stop_playmode();
  if (app != NULL)
    app->set_frame(frame_idx-5);
}

/******************************************************************************/
/*                   kd_playcontrol::OnBnClickedPlayRepeat                    */
/******************************************************************************/

void kd_playcontrol::OnBnClickedPlayRepeat()
{
  play_repeat = !play_repeat;
  get_repeat_ctrl()->SetCheck((play_repeat)?BST_CHECKED:BST_UNCHECKED);
  if (in_playmode)
    {
      stop_playmode();
      OnBnClickedPlayPlay();
    }
  else
    update_all_controls();
}
