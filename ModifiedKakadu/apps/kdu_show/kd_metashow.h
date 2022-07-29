/******************************************************************************/
// File: kd_metashow.h [scope = APPS/SHOW]
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
   Header file for "kd_metashow.cpp".
*******************************************************************************/
#if !defined(AFX_KD_METASHOW_H__785839DD_97E0_49CC_B54B_5BB82E75579A__INCLUDED_)
#define AFX_KD_METASHOW_H__785839DD_97E0_49CC_B54B_5BB82E75579A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "jp2.h"

// Defined here
struct kd_metanode;
class kd_metashow;

// Defined elsewhere
class CKduShow;

/******************************************************************************/
/*                                kd_metanode                                 */
/******************************************************************************/

struct kd_metanode {
  public: // Member functions
    kd_metanode(kd_metashow *owner, kd_metanode *parent);
    ~kd_metanode();
    void update_display();
      /* Called only if `can_display' is true and this node is to be displayed
         in the edit box.  The `owner->display' member must already point
         to the current node. */
    void update_structure(jp2_input_box *this_box,
                          bool container_complete,
                          jpx_meta_manager meta_manager);
      /* `this_box' will be NULL only at the root node; otherwise, it
         points to an open box representing the node's contents.  This box
         is used to open further sub-boxes if the `child_list_complete'
         member is false.  The `container_complete' argument should be set to
         true if the node's contents are known to be complete.  If
         `this_box' is NULL, the value will be true if information about the
         headers of all boxes at the file level is known.  Otherwise, this
         argument will be true if and only if `this_box'->is_complete() returns
         true. */
    void insert_into_view();
      /* Call this function after all members have been initialized, other
         than `name', `handle' and `can_display', to create a name for the
         node, determine whether or not it corresponds to a box type whose
         contents can be displayed as text, and insert it into the tree view
         control. */
  public: // Links
    kd_metashow *owner;
    kd_metanode *parent;
  public: // Data
    bool is_deleted; // If node has been deleted in JPX metadata editor
    bool is_changed; // If node has been altered in JPX metadata editor
    bool ancestor_changed; // If any ancestor was altered in JPX metadata editor
    kdu_uint32 box_type; // 0 if this is the root node
    jp2_locator locator; // Use this to open a box at any point in the future
    int box_header_length; // Use this to invoke `jpx_meta_manager::locate_node'
    jpx_metanode jpx_node; // Empty until we can resolve it (if ever)
    int codestream_idx; // -ve if does not refer to a code-stream
    int compositing_layer_idx; // -ve if does not refer to a compositing layer
    bool child_list_complete; // See below
    bool contents_complete; // See below
    kd_metanode *children; // Points to a list of child nodes, if any
    kd_metanode *last_child; // Last element in the `children' list
    kd_metanode *last_complete_child; // See below
    kd_metanode *next; // Points to next node at current level
  public: // Interaction with the Tree View Window
    char name[80]; // Short text description for this node
    HTREEITEM handle;
    bool can_display; // False unless a leaf whose contents can be displayed
  };
  /* Notes:
         The `child_list_complete' member is false if we cannot be sure that
       all children have been found yet.  In this case, the list of children
       may need to be grown next time `update' is called.  The fact that
       the list of children is complete does not mean that each child in
       the list is itself complete.  This information may be determined by
       using the `last_complete_child' member.
         The `contents_complete' member is true if the node is a super-box
       and its child list is complete, or if the node is a leaf in the tree
       and the corresponding JP2 box's contents are complete.
         The `last_complete_child' member points to the last element in the
       `children' list whose `is_complete' function returns true.  If no
       children are known to have been completely constructed,
       `last_complete_child' will be NULL. */

/******************************************************************************/
/*                                kd_metashow                                 */
/******************************************************************************/

class kd_metashow : public CDialog {
  //----------------------------------------------------------------------------
  public: // Member functions
	  kd_metashow(CKdu_showApp *app, CWnd* pParent=NULL);
    ~kd_metashow();
    void activate(jp2_family_src *src);
      /* Called from `CKdu_showApp::open_file' when a new file is opened.
         This function augments the meta-data structure as much as it can,
         inserting it into the tree view, before returning.  If the ultimate
         data source is a dynamic cache, you may have to invoke `update_tree'
         when new data arrives in the cache. */
    void update_tree();
      /* Updates the current tree and view to reflect any new data which may
         have arrived for the `jp2_family_src' object which was passed into
         `activate'.  This does nothing unless the ultimate source of data
         is a dynamic cache, whose contents may have expanded since the last
         call to `activate' or `update_tree'.  Returns true if and only if
         the meta-data representation is known to be complete -- i.e., all
         tree nodes must be complete and there must be no more tree nodes
         whose headers are not yet known.  It is safe to call this function at
         any time, even if `activate' has not been called. */
    void deactivate();
      /* Called from `CKdu_showApp::close_file'.  Safe to call this at any
         time, even if the object has not previously been activated.  The
         function clears the contents of the current tree and view. */
    void select_matching_metanode(jpx_metanode jpx_node);
      /* Cause the displayed tree-view to be expanded as required to display
         and select the identified node. */
  //----------------------------------------------------------------------------
  private: // Helper functions
    CTreeCtrl *get_tree_ctrl()
      {
        return (CTreeCtrl *) GetDlgItem(IDC_META_TREE);
      }
    CEdit *get_edit_ctrl()
      {
        return (CEdit *) GetDlgItem(IDC_META_TEXT);
      }
  private: // My Data
    friend struct kd_metanode;
    CKdu_showApp *app; // Non-NULL if we need to delete via application
    jp2_family_src *src; // Null if there is no active meta-data tree
    int num_codestreams; // Num contiguous codestream boxes found
    int num_jpch; // Number of codestream header boxes found
    int num_jplh; // Number of compositing layer header boxes found
    kd_metanode *tree; // Local skeleton of the meta-data tree
    kd_metanode *display; // NULL, or node being displayed in the edit box
    char *display_buf;
    int display_buf_len; // Size of `display_buf' array.
    int display_buf_pos; // Position of next character to add to `display_buf'
    int display_box_pos; // Position of next_display character in JP2 box;
                         // Becomes negative if the box is fully displayed.
    kdu_coords dialog_dims; // Height and width of dialog since last `OnSize'
    kdu_coords tree_dims; // Height & width of tree control since last `OnSize'
    kdu_coords edit_dims; // Height & width of edit control since last `OnSize'
    bool started;

  //----------------------------------------------------------------------------
  public: // Class wizard stuff
    //{{AFX_DATA(kd_metashow)
	enum { IDD = IDD_META_STRUCTURE };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(kd_metashow)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
  protected:
    virtual void OnOK();

	// Generated message map functions
	//{{AFX_MSG(kd_metashow)
	afx_msg void OnClose();
	afx_msg void OnSelchangedMetaTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
  };

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KD_METASHOW_H__785839DD_97E0_49CC_B54B_5BB82E75579A__INCLUDED_)
