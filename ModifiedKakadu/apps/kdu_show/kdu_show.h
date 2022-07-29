/*****************************************************************************/
// File: kdu_show.h [scope = APPS/SHOW]
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
/******************************************************************************
Description:
   Defines the main application object for the interactive JPEG2000 viewer,
"kdu_show".
******************************************************************************/
#ifndef KDU_SHOW_H
#define KDU_SHOW_H

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#if !(defined _MSC_VER && (_MSC_VER >= 1300)) // If not .NET
#  define DWORD_PTR DWORD
#  define UINT_PTR kdu_uint32
#  define INT_PTR kdu_int32
#endif // Not .NET

#include "resource.h"       // main symbols
#include "kdu_messaging.h"
#include "kdu_compressed.h"
#include "kdu_file_io.h"
#include "jpx.h"
#include "kdu_client.h"
#include "kdu_clientx.h"
#include "kdu_region_compositor.h"
#include "MainFrm.h"
#include "kd_metashow.h"
#include "kd_playcontrol.h"

// Declared here:
class kdms_file;
struct kdms_file_editor;
class kdms_file_services;
class kdu_notifier;
class kd_settings;
class kd_static;
class kd_bitmap_buf;
class kd_bitmap_compositor;
class CKdu_showApp;

enum kds_status_id {
    KDS_STATUS_LAYER_RES=0,
    KDS_STATUS_DIMS=1,
    KDS_STATUS_MEM=2,
    KDS_STATUS_CACHE=3
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
      doc_suffix = app_pathname = app_name = NULL;
      next = NULL;
    }
  ~kdms_file_editor()
    {
      if (doc_suffix != NULL) delete[] doc_suffix;
      if (app_pathname != NULL) delete[] app_pathname;
      if (app_name != NULL) delete[] app_name;
    }
  char *doc_suffix; // File extension for matching document types
  char *app_pathname; // Either the application pathname or else the first
                      // part of a command to run the application.
  char *app_name; // Extracted from `app_pathname'.
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
  void add_editor_progid_for_doc_type(const char *doc_suffix,
                                      const char *progid);
    /* Used internally to add editors based on their ProgID's, as discovered
       by scanning the registry under HKEY_CLASSES_ROOT/.<suffix>. */
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
/*                                 kdu_notifier                              */
/*****************************************************************************/

class kdu_notifier : public kdu_client_notifier {
  public: // Member functions
    kdu_notifier() { wnd = NULL; }
    kdu_notifier(HWND wnd) { this->wnd = wnd; }
    void set_wnd(HWND wnd)
      { assert(this->wnd == NULL); this->wnd = wnd; }
    void notify()
      { // Pump the message queue to ensure idle time processing occurs
        if (wnd != NULL)
          PostMessage(wnd,WM_NULL,0,0);
      }
  private: // Data
    HWND wnd;
  };

/*****************************************************************************/
/*                                 kd_settings                               */
/*****************************************************************************/

class kd_settings {
  public: // Member functions
    kd_settings();
    ~kd_settings();
    void save_to_registry(CWinApp *app);
    void load_from_registry(CWinApp *app);
    const char *get_open_save_dir()
      {
        return ((open_save_dir==NULL)?"":open_save_dir);
      }
    int get_open_idx() { return open_idx; }
    int get_save_idx() { return save_idx; }
    const char *get_jpip_server()
      {
        return ((jpip_server==NULL)?"":jpip_server);
      }
    const char *get_jpip_proxy()
      {
        return ((jpip_proxy==NULL)?"":jpip_proxy);
      }
    const char *get_jpip_cache()
      {
        return ((jpip_cache==NULL)?"":jpip_cache);
      }
    const char *get_jpip_request()
      {
        return ((jpip_request==NULL)?"":jpip_request);
      }
    const char *get_jpip_channel_type()
      {
        return ((jpip_channel==NULL)?"":jpip_channel);
      }
    void set_open_save_dir(const char *string)
      {
        if (open_save_dir != NULL) delete[] open_save_dir;
        open_save_dir = new char[strlen(string)+1];
        strcpy(open_save_dir,string);
      }
    void set_open_idx(int idx) {open_idx = idx; }
    void set_save_idx(int idx) {save_idx = idx; }
    void set_jpip_server(const char *string)
      {
        if (jpip_server != NULL) delete[] jpip_server;
        jpip_server = new char[strlen(string)+1];
        strcpy(jpip_server,string);
      }
    void set_jpip_proxy(const char *string)
      {
        if (jpip_proxy != NULL) delete[] jpip_proxy;
        jpip_proxy = new char[strlen(string)+1];
        strcpy(jpip_proxy,string);
      }
    void set_jpip_cache(const char *string)
      {
        if (jpip_cache != NULL) delete[] jpip_cache;
        jpip_cache = new char[strlen(string)+1];
        strcpy(jpip_cache,string);
      }
    void set_jpip_request(const char *string)
      {
        if (jpip_request != NULL) delete[] jpip_request;
        jpip_request = new char[strlen(string)+1];
        strcpy(jpip_request,string);
      }
    void set_jpip_channel_type(const char *string)
      {
        if (jpip_channel != NULL) delete[] jpip_channel;
        jpip_channel = new char[strlen(string)+1];
        strcpy(jpip_channel,string);
      }
  private: // Data
    char *open_save_dir;
    int open_idx, save_idx;
    char *jpip_server;
    char *jpip_proxy;
    char *jpip_cache;
    char *jpip_request;
    char *jpip_channel;
  };

/*****************************************************************************/
/*                                  kd_static                                */
/*****************************************************************************/

class kd_static : public CStatic {
  /* This object is used to display labels found in metadata when induced
     to do so by the `kdu_show::show_metainfo' function. */
  public: // Member functions
    kd_static();
    void show(const char *label, CWnd *view_wnd, kdu_coords pos,
              kdu_dims view_dims);
      /* If necessary, this object creates the static window for the first
         time.  If necessary, it installs the `label' text and resizes the
         window.  If necessary, it positions the window so that its bottom
         left or right hand corner is at `pos', expressed in the same
         coordinate system as `view_dims' (`pos'=`view_dims.pos' corresponds
         to the upper left hand corner of `view_wnd').  The decision as to
         which corner should be placed at `pos' is based on how close `pos'
         is to the left and right view boundaries and on past decisions. */
    void hide();
      /* Hides the window, but does not destroy the underlying resources. */
  private: // Data
    bool is_created;
    const char *last_label;
    kdu_coords last_pos;
    kdu_coords last_size;
    bool anchor_on_left; // Otherwise, label box anchored on right.
  };

/*****************************************************************************/
/*                               kd_bitmap_buf                               */
/*****************************************************************************/

class kd_bitmap_buf : public kdu_compositor_buf {
  public: // Member functions and overrides
    kd_bitmap_buf()
      { bitmap=NULL; next=NULL; }
    virtual ~kd_bitmap_buf()
      {
        if (bitmap != NULL) DeleteObject(bitmap);
        buf = NULL; // Remember to do this whenever overriding the base class
      }
    void create_bitmap(kdu_coords size);
      /* If this function finds that a bitmap buffer is already available,
         it returns without doing anything. */
    HBITMAP access_bitmap(kdu_uint32 * &buf, int &row_gap)
      { buf=this->buf; row_gap=this->row_gap; return bitmap; }
  protected: // Additional data
    friend class kd_bitmap_compositor;
    HBITMAP bitmap; // NULL if not using a bitmap
    BITMAPINFO bitmap_info; // Description for `bitmap' if non-NULL
    kdu_coords size;
    kd_bitmap_buf *next;
  };

/*****************************************************************************/
/*                            kd_bitmap_compositor                           */
/*****************************************************************************/

class kd_bitmap_compositor : public kdu_region_compositor {
  /* This class augments the platorm independent `kdu_region_compositor'
     class with display buffer management logic which allocates system
     bitmaps.  You can take this as a template for designing richer
     buffer management systems -- e.g. ones which utilize display hardware
     through DirectDraw. */
  public: // Member functions
    kd_bitmap_compositor(kdu_thread_env *env=NULL,
                         kdu_thread_queue *env_queue=NULL)
                         : kdu_region_compositor(env,env_queue)
      { active_list = free_list = NULL; }
    ~kd_bitmap_compositor()
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
    kd_bitmap_buf *get_composition_bitmap(kdu_dims &region)
      { return (kd_bitmap_buf *) get_composition_buffer(region); }
      /* Use this instead of `kdu_compositor::get_composition_buffer'. */
  private: // Data
    kd_bitmap_buf *active_list;
    kd_bitmap_buf *free_list;
  };

/*****************************************************************************/
/*                                 CKdu_showApp                              */
/*****************************************************************************/

class CKdu_showApp : public CWinApp
{
public:
  CKdu_showApp();
  ~CKdu_showApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKdu_showApp)
public:
	virtual BOOL InitInstance();
	virtual BOOL OnIdle(LONG lCount);
  virtual BOOL SaveAllModified();
	//}}AFX_VIRTUAL

// Implementation

public:
	//{{AFX_MSG(CKdu_showApp)
	afx_msg void OnAppAbout();
	afx_msg void OnFileClose();
	afx_msg void OnUpdateFileClose(CCmdUI* pCmdUI);
	afx_msg void OnFileOpen();
	afx_msg void OnViewHflip();
	afx_msg void OnUpdateViewHflip(CCmdUI* pCmdUI);
	afx_msg void OnViewVflip();
	afx_msg void OnUpdateViewVflip(CCmdUI* pCmdUI);
	afx_msg void OnViewRotate();
	afx_msg void OnUpdateViewRotate(CCmdUI* pCmdUI);
	afx_msg void OnViewCounterRotate();
	afx_msg void OnUpdateViewCounterRotate(CCmdUI* pCmdUI);
	afx_msg void OnViewZoomOut();
	afx_msg void OnUpdateViewZoomOut(CCmdUI* pCmdUI);
	afx_msg void OnViewZoomIn();
	afx_msg void OnUpdateViewZoomIn(CCmdUI* pCmdUI);
	afx_msg void OnViewRestore();
	afx_msg void OnUpdateViewRestore(CCmdUI* pCmdUI);
	afx_msg void OnModeFast();
	afx_msg void OnUpdateModeFast(CCmdUI* pCmdUI);
	afx_msg void OnModeFussy();
	afx_msg void OnUpdateModeFussy(CCmdUI* pCmdUI);
	afx_msg void OnModeResilient();
	afx_msg void OnUpdateModeResilient(CCmdUI* pCmdUI);
	afx_msg void OnViewWiden();
	afx_msg void OnUpdateViewWiden(CCmdUI* pCmdUI);
	afx_msg void OnViewShrink();
	afx_msg void OnUpdateViewShrink(CCmdUI* pCmdUI);
	afx_msg void OnFileProperties();
	afx_msg void OnUpdateFileProperties(CCmdUI* pCmdUI);
	afx_msg void OnComponent1();
	afx_msg void OnUpdateComponent1(CCmdUI* pCmdUI);
	afx_msg void OnMultiComponent();
	afx_msg void OnUpdateMultiComponent(CCmdUI* pCmdUI);
	afx_msg void OnLayersLess();
	afx_msg void OnUpdateLayersLess(CCmdUI* pCmdUI);
	afx_msg void OnLayersMore();
	afx_msg void OnUpdateLayersMore(CCmdUI* pCmdUI);
	afx_msg void OnModeResilientSop();
	afx_msg void OnUpdateModeResilientSop(CCmdUI* pCmdUI);
	afx_msg void OnModeSeekable();
	afx_msg void OnUpdateModeSeekable(CCmdUI* pCmdUI);
	afx_msg void OnStatusToggle();
	afx_msg void OnUpdateStatusToggle(CCmdUI* pCmdUI);
	afx_msg void OnViewRefresh();
	afx_msg void OnUpdateViewRefresh(CCmdUI* pCmdUI);
	afx_msg void OnFileDisconnect();
	afx_msg void OnUpdateFileDisconnect(CCmdUI* pCmdUI);
	afx_msg void OnComponentNext();
	afx_msg void OnUpdateComponentNext(CCmdUI* pCmdUI);
	afx_msg void OnComponentPrev();
	afx_msg void OnUpdateComponentPrev(CCmdUI* pCmdUI);
	afx_msg void OnJpipOpen();
	afx_msg void OnFocusOff();
	afx_msg void OnUpdateFocusOff(CCmdUI* pCmdUI);
	afx_msg void OnFocusHighlighting();
	afx_msg void OnUpdateFocusHighlighting(CCmdUI* pCmdUI);
	afx_msg void OnFocusWiden();
	afx_msg void OnFocusShrink();
	afx_msg void OnFocusLeft();
	afx_msg void OnFocusRight();
	afx_msg void OnFocusUp();
	afx_msg void OnFocusDown();
	afx_msg void OnViewMetadata();
	afx_msg void OnUpdateViewMetadata(CCmdUI* pCmdUI);
	afx_msg void OnImageNext();
	afx_msg void OnUpdateImageNext(CCmdUI* pCmdUI);
	afx_msg void OnImagePrev();
	afx_msg void OnUpdateImagePrev(CCmdUI* pCmdUI);
	afx_msg void OnCompositingLayer();
	afx_msg void OnUpdateCompositingLayer(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveAs();
	afx_msg void OnUpdateFileSaveAs(CCmdUI* pCmdUI);
	afx_msg void OnMetaAdd();
	afx_msg void OnUpdateMetaAdd(CCmdUI* pCmdUI);
	afx_msg void OnOverlayEnabled();
	afx_msg void OnUpdateOverlayEnabled(CCmdUI* pCmdUI);
	afx_msg void OnOverlayDarker();
	afx_msg void OnUpdateOverlayDarker(CCmdUI* pCmdUI);
	afx_msg void OnOverlayBrighter();
	afx_msg void OnUpdateOverlayBrighter(CCmdUI* pCmdUI);
	afx_msg void OnOverlayFlashing();
	afx_msg void OnUpdateOverlayFlashing(CCmdUI* pCmdUI);
	afx_msg void OnOverlayToggle();
	afx_msg void OnUpdateOverlayToggle(CCmdUI* pCmdUI);
	afx_msg void OnOverlayDoublesize();
	afx_msg void OnUpdateOverlayDoublesize(CCmdUI* pCmdUI);
	afx_msg void OnOverlayHalvesize();
	afx_msg void OnUpdateOverlayHalvesize(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
// ----------------------------------------------------------------------------
public: // Access methods for use by the window objects
  void open_file(char *filename);
    /* This function enters with `filename'=NULL if it is being called to
       complete an overlapped opening operation.  This happens with the
       "kdu_client" compressed data source, which must first be connected
       to a server and then, after some time has passed, becomes ready
       to actually open a code-stream or JP2-family data source. */
  void close_file();
  void suspend_processing(bool suspend);
    /* Call this function to suspend or resume idle time processing of
       compressed image data.  This is useful when drawing onto the display
       surface, since idle time processing may cause its own calls to
       `paint_region'. */
  void refresh_display();
    /* Called from `initialize_regions' and also from any point where the
       image needs to be decompressed again.  The function is public so
       that it can be invoked while processing the WM_TIMER message received
       by the frame window. */
  void flash_overlays();
    /* Called when the overlay flash timer goes off. */
  void set_view_size(kdu_coords size);
    /* This function should be called whenever the client view window's
       dimensions may have changed.  The function should be called at least
       whenever the WM_SIZE message is received by the client view window.
       The current object expects to receive this message before it can begin
       processing a newly opened image.  If `size' defines a larger region
       than the currently loaded image, the actual `view_dims' region used
       for rendering purposes will be smaller than the client view window's
       dimensions. */
  void resize_stripmap(int min_width);
    /* This function is called whenever the current width of the strip map
       buffer is too small. */
  void set_hscroll_pos(int pos, bool relative_to_last=false);
    /* Identifies a new left hand boundary position for the current view.
       If `relative_to_last' is true, the new position is expressed relative
       to the existing left hand view boundary.  Otherwise, the new position
       is expressed relative to the image origin. */
  void set_vscroll_pos(int rel_pos, bool relative_to_last=false);
    /* Same as `set_hscroll_pos', but for vertical scrolling. */
  void set_scroll_pos(int rel_xpos, int rel_ypos, bool relative_to_last=false);
    /* This is a combined scrolling function that allows scrolling in two
       directions simultaneously */
  void paint_region(CDC *dc, kdu_dims region, bool is_absolute=true);
    /* Paints a portion of the viewable region to the supplied device
       context.  The supplied `region' is expressed in the same absolute
       coordinate system as `view_dims', unless `is_absolute' is false, in
       which case `region.pos' identifies the location of the upper left
       hand corner of the region, relative to the current view.  If the
       region is not completely contained inside the current view region, or
       no file has yet been loaded, the background white colour is painted into
       the entire region first, and then the portion which intersects with the
       current view region is painted.  This allows for the possibility that
       the client view region might be larger than the rendering view region
       (e.g., because the image is too small to occupy the minimum window
       size). */
  void set_focus_box(kdu_dims new_box);
    /* Called from the Child View object when the user finishes entering a
       new focus box.  Note that the coordinates of `new_box' are expressed
       relative to the view port. */
  void set_codestream(int codestream_idx);
    /* Called from `kd_metashow' when the user clicks on a code-stream box,
       this function switches to single component mode to display the
       image components of the indicated code-stream. */
  void set_compositing_layer(int layer_idx);
    /* Called from `kd_metashow' when the user clicks on a compositing layer
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
    /* Called from `kdu_playcontrol' when exiting play mode. */
  void start_playmode(bool use_native_timing, float custom_fps,
                      float rate_multiplier, bool repeat);
    /* Called from `kdu_playcontrol' when entering play mode. */
  bool show_metainfo(kdu_coords point);
    /* This function is called when the mouse moves over the view window while
       the control key is held down.  It searches for spatially sensitive
       metadata which falls underneath the current mouse position (given by
       `point' relative to the upper left corner of the view window).  If
       none is found, the function returns false.  Otherwise, it returns true
       after first displaying any appropriate label information.  The caller
       uses the return value to adjust the cursor. */
  void kill_metainfo();
    /* Called when the control key is lifted, if the last call to
       `show_metainfo' returned true, this function removes any metadata
       labels which are currently being displayed. */
  bool edit_metainfo(kdu_coords point);
    /* Similar to `show_metainfo' except that the metadata is edited.  The
       function returns true if metadata editing was initiated, in which case
       the editing cursor should be switched off. */
  jpx_meta_manager get_meta_manager()
    { return (!jpx_in)?jpx_meta_manager():jpx_in.access_meta_manager(); }
  void metadata_changed(bool new_data_only);
    /* This function is called from within the metadata editor when changes
       are made to the metadata structure, which might need to be reflected in
       the overlay display and/or an open metashow window.  If `new_data_only'
       is true, no changes have been made to existing metadata. */
private: // Private implementation functions
  void perform_essential_cleanup_steps();
    /* Called immediately from the destructor and from `close_file' so as
       to ensure that important file saving/deleting steps are performed. */
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
       coordinates and dimensions are used by the next calls to
       `set_view_size' and `get_new_focus_box' calls to position the
       view port and anchor pox.  The most common use of this function is
       to preserve (or modify) the key properties  of the view port and
       focus box when the image is zoomed, rotated, flipped, or specialized
       to a different image component.  */
  void  display_status();
    /* Displays information in the status bar.  The particular type of
       information displayed depends upon the value of the `status_id'
       member variable. */
  bool check_one_time_request_compatibility();
    /* Checks the server response from a one-time JPIP request (if the
       response is available yet) to determine if it is compatible with the
       mode in which we are currently trying to open the image.  If so, the
       function returns true.  Otherwise, the function returns false after
       first downgrading the image opening mode.  If we are currently trying
       to open an animation frame, we downgrade the operation to one in which
       we are trying to open a single compositing layer.  If this is also
       incompatible, we may downgrade the opening mode to one in which we
       are trying to open only a single image component of a single
       codestream. */
  void update_client_window_of_interest();
    /* Called if the compressed data source is a "kdu_client"
       object, whenever the viewing region and/or resolution may
       have changed.  This function computes and passes information
       about the client's window of interest to the remote server. */
  void change_client_focus(kdu_coords actual_resolution,
                           kdu_dims actual_region);
    /* Called if the compressed data source is a "kdu_client" object,
       whenever the server's reply to a region of interest request indicates
       that it is processing a different region to that which was requested
       by the client.  The changes are reflected by redrawing the focus
       box, as necessary, while being careful not to issue a request for a
       new region of interest from the server in the process. */
  kdu_dims get_new_focus_box();
    /* Performs most of the work of the `update_focus_box' and `set_focus_box'
       functions. */
  void update_focus_box();
    /* Called whenever a change in the viewport may require the focus box
       to be adjusted.  The focus box must always be a subset of the
       viewport. */
  void outline_focus_box(CDC *dc);
    /* This function is automatically called from within `paint_region' if
       the focus box is enabled.  It pens an outline around the current focus
       box. */
private: // Functions related to file saving
  const char *get_save_file_pathname(const char *file_pathname);
    /* This function is used to avoid overwriting open files when trying
       to save to an existing file.  The pathname of the file you want to
       save to is supplied as the argument.  The function either returns
       that same pathname (without copying it to an internal buffer) or else
       it returns a temporary pathname that should be use instead, remembering
       to move the temporary file into the original file once it is closed.
       The temporary file is stored in the `oversave_pathname' member. */
  void declare_save_file_invalid(const char *file_pathname);
    /* This function is called if an attempt to save failed.  You supply
       the same pathname supplied originally by `get_save_file_pathname' (even
       if that was just the pathname you passed into the function).  The file
       is deleted and, if necessary, any internal reminder to copy that file
       over the original once it is closed, is removed. */
  void save_as(bool preserve_codestream_links, bool force_codestream_links);
  bool save_over(); // Saves over the existing file; returns false if failed.
  void save_as_jpx(const char *filename, bool preserve_codestream_links,
                   bool force_codestream_links);
  void save_as_jp2(const char *filename);
  void save_as_raw(const char *filename);
  void copy_codestream(kdu_compressed_target *tgt, kdu_codestream src);
public:
  CMainFrame *frame_wnd;
  kd_metashow *metashow; // NULL if not active
  kd_playcontrol *playcontrol; // NULL if not active

private: // Links to other window objects
  CChildView *child_wnd; // Null until the frame is created.
  CProgressCtrl *jpip_progress; // NULL unless JPIP progress in status bar

private: // File and URL names retained by the `window_manager'
  char *open_file_pathname; // File we are reading; NULL for a URL
  char *open_file_urlname; // JPIP URL we are reading; NULL if not JPIP
  char *open_filename; // Points to filename within path or URL
  char *last_save_pathname; // Last file to which user saved anything
  char *oversave_pathname; // If non-NULL, holds the name of a file which
                           // should overwrite `open_file_pathname' when closed
  bool save_without_asking; // If the user does not want to be asked in future
  bool have_unsaved_edits;

private: // Compressed data source and rendering machinery
  bool allow_seeking;
  int error_level; // 0 = fast, 1 = fussy, 2 = resilient, 3 = resilient_sop
  kdu_simple_file_source file_in;
  jp2_family_src jp2_family_in;
  jpx_source jpx_in;
  mj2_source mj2_in;
  jpx_composition composition_rules; // Composition/rendering info, if available
  kdu_client jpip_client;
  kdu_clientx jpx_client_translator;
  kdu_notifier client_notifier; // For `client_input.install_notifier'.
  kd_settings settings; // Manages load/save and JPIP settings.
  kd_bitmap_compositor *compositor;
  kdu_thread_env thread_env; // Multi-threaded environment
  int num_threads; // If 0 use single-threaded machinery with `thread_env'=NULL
  int num_recommended_threads; // If 0, recommend single-threaded machinery
  bool in_idle; // Indicates we are inside the `OnIdle' function
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
  bool use_native_timing;
  float custom_fps; // Frame rate to use if `use_native_timing' is false
  float rate_multiplier; // Amount to scale the playback speed
  int max_queue_size; // Max size of jitter-absorbtion queue
  double max_queue_period; // Max seconds spanned by queue
  clock_t playclock_base; // Subtract from `clock()' to get playback instant
  double playstart_instant; // Value of `frame_start' when play mode started
  double last_actual_time; // For calculating actual frame processing times
  double last_nominal_time; // For calculating nominal frame processing times
  double frame_time_offset; // Add to target frame display times in play back
  bool playmode_repeat;
  bool pushed_last_frame;

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
  CPen focus_pen;
  kdu_dims focus_box; // Uses same coordinate system as `view_dims'.
  bool enable_region_posting; // True if focus changes can be sent to server.
  kdu_window client_roi; // Last posted/in-progress client-server window.
  kdu_window tmp_roi; // Temp work space to avoid excessive new/delete usage

private: // Overlay parameters
  bool overlays_enabled;
  bool overlay_flashing_enabled;
  int overlay_painting_intensity;
  int overlay_painting_colour;
  int overlay_size_threshold; // Min composited size for an overlay to be shown
  kd_static metainfo_show; // Affected by a successful call to `show_metainfo'
  jpx_metanode metainfo_node; // Left by successful call to `show_metainfo'
  const char *metainfo_label; // Set in call to `show_metainfo'

private: // Image buffer and painting resources
  kd_bitmap_buf *buffer; // Get from `compositor' object; NULL if invalid
  CDC compatible_dc; // Compatible DC for use with BitBlt function
  kdu_coords strip_extent; // Dimensions of strip buffer
  HBITMAP stripmap; // Manages strip buffer for indirect painting
  BITMAPINFO stripmap_info; // Used in calls to CreateDIBSection.
  kdu_uint32 *stripmap_buffer; // Buffer managed by `stripmap' object.
  kdu_byte foreground_lut[256];
  kdu_byte background_lut[256];

private: // Periodic event machinery
# define KDU_SHOW_REFRESH_TIMER_IDENT 10392 // Random identifier
# define KDU_SHOW_FLASH_TIMER_IDENT   10393 // ID for overlay flashing timer
  UINT_PTR refresh_timer_id; // 0 if no refresh timer installed.
  UINT_PTR flash_timer_id; // 0 if no overlay flash timer
  int last_transferred_bytes; // Used to check for cache updates.
  clock_t cumulative_transmission_time;
  clock_t last_transmission_start;

public:
  afx_msg void OnViewOptimizescale();
  afx_msg void OnUpdateViewOptimizescale(CCmdUI *pCmdUI);
  afx_msg void OnTrackNext();
  afx_msg void OnUpdateTrackNext(CCmdUI *pCmdUI);
  afx_msg void OnViewPlaycontrol();
  afx_msg void OnUpdateViewPlaycontrol(CCmdUI *pCmdUI);
  afx_msg void OnViewScaleX2();
  afx_msg void OnViewScaleX1();
  afx_msg void OnModesSinglethreaded();
  afx_msg void OnUpdateModesSinglethreaded(CCmdUI *pCmdUI);
  afx_msg void OnModesMultithreaded();
  afx_msg void OnUpdateModesMultithreaded(CCmdUI *pCmdUI);
  afx_msg void OnModesMorethreads();
  afx_msg void OnUpdateModesMorethreads(CCmdUI *pCmdUI);
  afx_msg void OnModesLessthreads();
  afx_msg void OnUpdateModesLessthreads(CCmdUI *pCmdUI);
public:
  afx_msg void OnModesRecommendedThreads();
public:
  afx_msg void OnUpdateModesRecommendedThreads(CCmdUI *pCmdUI);
  afx_msg void OnFileSaveAsLinked();
  afx_msg void OnUpdateFileSaveAsLinked(CCmdUI *pCmdUI);
  afx_msg void OnFileSaveAsEmbedded();
  afx_msg void OnUpdateFileSaveAsEmbedded(CCmdUI *pCmdUI);
  afx_msg void OnFileSaveReload();
  afx_msg void OnUpdateFileSaveReload(CCmdUI *pCmdUI);
};
     
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // KDU_SHOW_H
