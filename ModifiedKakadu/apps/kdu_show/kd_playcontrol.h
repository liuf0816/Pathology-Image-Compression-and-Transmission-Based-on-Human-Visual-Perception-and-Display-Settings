/******************************************************************************/
// File: kd_playcontrol.h [scope = APPS/SHOW]
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
   Header file for "kd_metashow.cpp".
*******************************************************************************/
#ifndef KD_PLAYCONTROL_H
#define KD_PLAYCONTROL_H

#pragma once

// Defined here
class kd_playcontrol;

// Defined elsewhere
class CKduShow;

/******************************************************************************/
/*                               kd_playcontrol                               */
/******************************************************************************/

class kd_playcontrol : public CDialog {
  //----------------------------------------------------------------------------
  public: // Member functions
  	kd_playcontrol(CKdu_showApp *app, CWnd* pParent = NULL);
    virtual ~kd_playcontrol();
    void update(int track_idx, int num_frames, int frame_idx,
                float frame_seconds_left=-1.0F,
                float last_nominal_period=-1.0F,
                float last_actual_period=-1.0F);
      /* This function should be called whenever the current frame, track
         or other properties change.  It is invoked from the CKdu_showApp
         object.
            If `track_idx' is 0, we are displaying the composite
         video/animation specified by a JPX animation or an MJ2 movie.  If
         `track_idx' is negative, we are not currently displaying playable
         content.  Otherwise, `track_idx' is the positive index of the track
         within a Motion JPEG2000 source.
            If `num_frames' is <= 0, we do not currently know the number of
         available frames.
            The `frame_idx' values starts from 0 for the first frame.
            The `frame_seconds_left' argument is used only when in play mode
         and then only when its value is positive.  It holds the nominal
         number of seconds remaining before the next frame change (assuming
         the application is able to keep up with the timing requirements).
            The last two arguments are also used only in play mode and when
         both values are non-negative.  In this case, they represent the
         nominal and actual number of seconds which should have elapsed
         between the point at which the last and current frame were displayed.
         These values are used to display statistics identifying the degree
         to which the system is able to keep its timing on track. */
    void stop_playmode();
      /* Called if any action requires continuous play-back to stop */  //----------------------------------------------------------------------------
  private: // Helper functions
    void update_all_controls();
      /* Updates all the controls in the dialog. */
  //----------------------------------------------------------------------------
  private: // Access to controls
    CStatic *get_track_ctrl()
      { return (CStatic *) GetDlgItem(IDC_PLAY_TRACK); }
    CButton *get_track_next_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_NEXT_TRACK); }
    CButton *get_base_native_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_BASE_NATIVE); }
    CButton *get_base_custom_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_BASE_CUSTOM); }
    CEdit *get_base_fps_ctrl()
      { return (CEdit *) GetDlgItem(IDC_PLAY_BASE_FPS); }
    CButton *get_rate_x1_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_X1); }
    CButton *get_rate_x2_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_X2); }
    CButton *get_rate_d2_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_D2); }
    CButton *get_rate_d4_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_D4); }
    CEdit *get_frame_ctrl()
      { return (CEdit *) GetDlgItem(IDC_PLAY_FRAME); }
    CButton *get_reset_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_RESET); }
    CButton *get_stop_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_STOP); }
    CButton *get_play_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_PLAY); }
    CButton *get_fwd1_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_FWD1); }
    CButton *get_fwd5_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_FWD5); }
    CButton *get_back1_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_BACK1); }
    CButton *get_back5_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_BACK5); }
    CButton *get_repeat_ctrl()
      { return (CButton *) GetDlgItem(IDC_PLAY_REPEAT); }
    CStatic *get_frame_period_ctrl()
      { return (CStatic *) GetDlgItem(IDC_PLAY_FRAME_PERIOD); }
    CStatic *get_ontrack_percent_ctrl()
      { return (CStatic *) GetDlgItem(IDC_PLAY_ONTRACK_PERCENT); }
  //----------------------------------------------------------------------------
  private: // My data
    bool destroy_in_progress;
    CKdu_showApp *app;
    int track_idx; // -ve if not known
    int num_frames; // -ve if not known
    int frame_idx; // -ve if not known
    bool use_native_time_base;
    float base_multiplier;
    bool in_playmode;
    bool play_repeat;
    float frame_seconds_left; // < 0 if no information yet
    float nominal_frame_period; // < 0 if no information yet
    float actual_frame_period; // < 0 if no information yet
    float update_timer; // Used in play mode to avoid excessive counter updates

  //----------------------------------------------------------------------------
  public: // Class wizard stuff
    // Dialog Data
	  enum { IDD = IDD_PLAY_CONTROL };

  protected:
	  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	  DECLARE_MESSAGE_MAP()
    virtual void PostNcDestroy();
public:
  afx_msg void OnClose();
  afx_msg void OnBnClickedPlayNextTrack();
  afx_msg void OnBnClickedPlayBaseNative();
  afx_msg void OnBnClickedPlayBaseCustom();
//  afx_msg void OnEnChangePlayBaseFps();
  afx_msg void OnBnClickedPlayX1();
  afx_msg void OnBnClickedPlayX2();
  afx_msg void OnBnClickedPlayD2();
  afx_msg void OnBnClickedPlayD4();
  afx_msg void OnBnClickedPlayReset();
  afx_msg void OnBnClickedPlayStop();
  afx_msg void OnBnClickedPlayPlay();
  afx_msg void OnBnClickedPlayFwd1();
  afx_msg void OnBnClickedPlayFwd5();
  afx_msg void OnBnClickedPlayBack1();
  afx_msg void OnBnClickedPlayBack5();
protected:
  virtual void OnOK();
  public:
    afx_msg void OnBnClickedPlayRepeat();
};

#endif // KD_PLAYCONTROL
