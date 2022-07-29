/******************************************************************************/
// File: kd_metashow.cpp [scope = APPS/SHOW]
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
   Implementation of the metadata display dialog in the "kdu_show" application.
*******************************************************************************/

#include "stdafx.h"
#include "kdu_show.h"
#include "kd_metashow.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/* ========================================================================== */
/*                           INTERNAL FUNCTIONS                               */
/* ========================================================================== */

/******************************************************************************/
/* STATIC                    make_printable_char                              */
/******************************************************************************/

static inline char
  make_printable_char(char ch)
{
  if ((ch < 0x20) || (ch > 0x7F))
    ch = 0x7F;
  return ch;
}

/*****************************************************************************/
/* STATIC                  find_matching_metanode                            */
/*****************************************************************************/

static kd_metanode *
  find_matching_metanode(kd_metanode *container, jpx_metanode jpx_node)
  /* Recursively searches through the metadata tree to see if we can find
   a node which references the supplied `jpx_node' object.  In the case of
   multiple matches, the deepest match in the tree will be found, which is
   generally the most useful behaviour. */
{
  kd_metanode *scan, *result;
  for (scan=container->children; scan != NULL; scan=scan->next)
    if ((result = find_matching_metanode(scan,jpx_node)) != NULL)
      return result;
  return (container->jpx_node == jpx_node)?container:NULL;
}


/* ========================================================================== */
/*                               kd_metanode                                  */
/* ========================================================================== */

/******************************************************************************/
/*                         kd_metanode::kd_metanode                           */
/******************************************************************************/

kd_metanode::kd_metanode(kd_metashow *owner, kd_metanode *parent)
{
  this->owner = owner;
  this->parent = parent;
  this->is_deleted = false;
  this->is_changed = false;
  this->ancestor_changed = false;
  box_type = 0;
  box_header_length = 0;
  codestream_idx = compositing_layer_idx = -1;
  child_list_complete = contents_complete = false;
  children = last_child = last_complete_child = next = NULL;
  name[0] = '\0';
  handle = NULL;
  can_display = false;
}

/******************************************************************************/
/*                        kd_metanode::~kd_metanode                           */
/******************************************************************************/

kd_metanode::~kd_metanode()
{
  while ((last_child=children) != NULL)
    {
      children = last_child->next;
      delete last_child;
    }
}

/******************************************************************************/
/*                       kd_metanode::update_display                          */
/******************************************************************************/

void
  kd_metanode::update_display()
{
  assert(can_display && (owner->display==this) && (owner->display_box_pos>=0));
  CEdit *edit = owner->get_edit_ctrl();
  CTreeCtrl *ctrl = owner->get_tree_ctrl();

  jp2_input_box box;
  box.open(owner->src,locator);
  bool box_is_complete = box.exists() && box.is_complete();
  if (!box_is_complete)
    {
      contents_complete = false;
      owner->get_tree_ctrl()->SetItemState(handle,0,TVIS_BOLD);
    }
  if (!box.exists())
    {
      edit->SetWindowText("...");
      return;
    }
  if (box_is_complete && !contents_complete)
    {
      contents_complete = true;
      owner->get_tree_ctrl()->SetItemState(handle,TVIS_BOLD,TVIS_BOLD);
    }

  int buf_len = owner->display_buf_len;
  char *tp, *buf = owner->display_buf;
  if (box_type == jp2_file_type_4cc)
    {
      if (!contents_complete)
        {
          edit->SetWindowText("...");
          return;
        }

      kdu_uint32 brand=0, minor_version=0;
      box.read(brand);
      box.read(minor_version);
      sprintf(buf,"Brand = \"");
      tp = buf + strlen(buf);
      *(tp++) = make_printable_char((char)(brand>>24));
      *(tp++) = make_printable_char((char)(brand>>16));
      *(tp++) = make_printable_char((char)(brand>>8));
      *(tp++) = make_printable_char((char)(brand>>0));
      sprintf(tp,"\"\r\nMinor version = %d\r\nCompatibility list:\r\n    ",
              minor_version);
      tp += strlen(tp);
      bool first_in_list = true;
      while (box.read(brand) && ((tp-buf) < (buf_len-10)))
        {
          if (first_in_list)
            first_in_list = false;
          else
            { *(tp++) = ','; *(tp++) = ' '; }
          *(tp++) = '\"';
          *(tp++) = make_printable_char((char)(brand>>24));
          *(tp++) = make_printable_char((char)(brand>>16));
          *(tp++) = make_printable_char((char)(brand>>8));
          *(tp++) = make_printable_char((char)(brand>>0));
          *(tp++) = '\"';
          *tp = '\0';
        }
      owner->display_box_pos = -1; // Indicates that the display is complete
      edit->SetWindowText(buf);
    }
  else if ((box_type == jp2_label_4cc) ||
           (box_type == jp2_xml_4cc) || (box_type == jp2_iprights_4cc))
    { // Displaying plain text directly
      box.seek(owner->display_box_pos);
      int num_bytes;
      char text[256];
      char *bp = buf +  owner->display_buf_pos;
      while ((num_bytes = box.read((kdu_byte *) text,256)) > 0)
        {
          owner->display_box_pos += num_bytes;
          for (tp=text; num_bytes > 0; tp++, num_bytes--)
            {
              if ((*tp == '\0') ||
                  ((*tp == '\n') && ((bp==buf) || (bp[-1] != '\r'))))
                { *(bp++) = '\r'; *(bp++) = '\n'; }
              else if ((bp != buf) && (bp[-1] == '\r') && (*tp != '\n'))
                { *(bp++) = '\n'; *(bp++) = *tp; }
              else
                *(bp++) = make_printable_char(*tp);
            }
          owner->display_buf_pos = (int)(bp - buf);
          if ((owner->display_buf_len - owner->display_buf_pos) <= 520)
            {
              owner->display_box_pos = -1; // So we don't try to add more text
              strcpy(bp,"...\r\n\r\n<truncated to save memory>");
              bp += strlen(bp);
              break;
            }
        }
      *bp = '\0';
      if (contents_complete)
        owner->display_box_pos = -1; // Indicates that display is complete
      else if (owner->display_box_pos >= 0)
        strcpy(bp,"...");
      edit->SetWindowText(buf);
    }
  else
    { // Print hex dump of box contents
      box.seek(owner->display_box_pos);
      int n, num_bytes;
      kdu_byte *dp, data[8];
      char val, *bp = buf +  owner->display_buf_pos;
      if (owner->display_buf_pos == 0)
        {
          strcpy(buf,
                 "   Hex Dump of Binary Contents:\r\n"
                 "----------------------------------\r\n");
          bp += strlen(bp);
          owner->display_buf_pos = (int)(bp - buf);
        }
      while ((num_bytes = box.read(data,8)) > 0)
        {
          if ((num_bytes < 8) && !contents_complete)
            break; // Wait until we have at least one line of data to write
          owner->display_box_pos += num_bytes;
          for (dp=data, n=0; n < num_bytes; n++, dp++)
            {
              val = (char)((*dp) >> 4);
              *(bp++) = (val < 10)?('0'+val):('A'+val-10);
              val = (char)((*dp) & 0x0F);
              *(bp++) = (val < 10)?('0'+val):('A'+val-10);
              *(bp++) = ' ';
            }
          for (; n < 8; n++)
            { *(bp++) = '-'; *(bp++) = '-'; *(bp++) = ' '; }
          *(bp++) = '|'; *(bp++) = ' ';
          for (tp=(char *) data, n=0; n < num_bytes; n++, tp++)
            *(bp++) = make_printable_char(*tp);
          *(bp++) = '\r';
          *(bp++) = '\n';
          owner->display_buf_pos = (int)(bp - buf);
          if ((owner->display_buf_len - owner->display_buf_pos) <= 520)
            {
              owner->display_box_pos = -1; // So we don't try to add more text
              strcpy(bp,"...\r\n\r\n<truncated to save memory>");
              bp += strlen(bp);
              break;
            }
        }
      *bp = '\0';
      if (contents_complete)
        owner->display_box_pos = -1; // Indicates that display is complete
      else if (owner->display_box_pos >= 0)
        strcpy(bp,"...");
      edit->SetWindowText(buf);
    }
}

/******************************************************************************/
/*                      kd_metanode::update_structure                         */
/******************************************************************************/

void
  kd_metanode::update_structure(jp2_input_box *this_box,
                                bool container_complete,
                                jpx_meta_manager meta_manager)
{
  bool editing_state_changed = false;
  bool jpx_node_just_found = false;
  if (meta_manager.exists())
    {
      if (!jpx_node.exists())
        {
          jpx_node =
            meta_manager.locate_node(locator.get_file_pos()+box_header_length);
          if (jpx_node.exists())
            jpx_node_just_found = true;
        }
      if (jpx_node.exists() && (!is_changed) && jpx_node.is_changed())
        is_changed = editing_state_changed = true;
      if (jpx_node.exists() && (!ancestor_changed) &&
          jpx_node.ancestor_changed())
        ancestor_changed = editing_state_changed = true;
      if (jpx_node.exists() && (!is_deleted) && jpx_node.is_deleted())
        is_deleted = editing_state_changed = true;
    }

  kd_metanode *child;
  bool added_something = false;

  if (!child_list_complete)
    { // Add onto the child list, if we can
      jp2_input_box box;
      if (this_box == NULL)
        {
          if (last_child == NULL)
            box.open(owner->src);
          else
            {
              box.open(owner->src,last_child->locator);
              if (box.exists())
                {
                  box.close();
                  box.open_next();
                }
            }
        }
      else
        { // Open sub-boxes in sequence, until we get to the end
          box.open(this_box);
          for (child=children; (child!=NULL) && box.exists(); child=child->next)
            {
              box.close();
              box.open_next();
            }
        }
      while (box.exists())
        { // Have a new child
          added_something = true;
          if (box.get_remaining_bytes() < 0)
            container_complete = true; // Rubber length box must be last
          child = new kd_metanode(owner,this);
          if (last_child == NULL)
            children = last_child = child;
          else
            last_child = last_child->next = child;
          child->box_type = box.get_box_type();
          child->locator = box.get_locator();
          child->box_header_length = box.get_box_header_length();
          child->child_list_complete = !jp2_is_superbox(child->box_type);
          child->insert_into_view();
          box.close();
          box.open_next();
        }

      if (added_something)
        owner->get_tree_ctrl()->Expand(handle,TVE_EXPAND);
    }

  if (container_complete && !contents_complete)
    { // Change the appearance of the tree node
      child_list_complete = contents_complete = true;
      owner->get_tree_ctrl()->SetItemState(handle,TVIS_BOLD,TVIS_BOLD);
      CEdit *edit = owner->get_tree_ctrl()->GetEditControl();
    }

  // Now scan through any incomplete children, trying to complete them
  if (last_complete_child != NULL)
    child = last_complete_child->next;
  else
    child = children;
  for (; child != NULL; child=child->next)
    {
      jp2_input_box box;
      box.open(owner->src,child->locator);
      child->update_structure(&box,box.is_complete(),meta_manager);
      box.close();
    }
}

/******************************************************************************/
/*                      kd_metanode::insert_into_view                         */
/******************************************************************************/

void
  kd_metanode::insert_into_view()
{
  if (box_type == 0)
    strcpy(name,"<root>");
  else
    {
      name[0] = (char)(box_type>>24);
      name[1] = (char)(box_type>>16);
      name[2] = (char)(box_type>>8);
      name[3] = (char)(box_type>>0);
      name[4] = '\0';
    }

  if ((box_type == jp2_codestream_4cc) && (parent != NULL) &&
      (parent->parent == NULL))
    codestream_idx = owner->num_codestreams++;
  else if (box_type == jp2_codestream_header_4cc)
    codestream_idx = owner->num_jpch++;
  else if (box_type == jp2_compositing_layer_hdr_4cc)
    compositing_layer_idx = owner->num_jplh++;

  char *cp = name + strlen(name);

  if (box_type == jp2_file_type_4cc)
    {
      can_display = true;
      strcpy(cp," [expand]");
    }
  else if ((box_type == jp2_file_type_4cc) || (box_type == jp2_label_4cc) ||
      (box_type == jp2_xml_4cc) || (box_type == jp2_iprights_4cc))
    {
      can_display = true;
      strcpy(cp," [show text]");
    }
  else if ((!jp2_is_superbox(box_type)) && (box_type != jp2_codestream_4cc) &&
           (parent != NULL))
    {
      can_display = true;
      strcpy(cp," [show hex]");
    }
  else if (codestream_idx >= 0)
    sprintf(cp," [show stream %d]",codestream_idx+1);
  else if (compositing_layer_idx >= 0)
    sprintf(cp," [show layer %d]",compositing_layer_idx+1);
  else if (box_type == jp2_composition_4cc)
    strcpy(cp," [show composition]");

  CTreeCtrl *ctrl = owner->get_tree_ctrl();
  handle = ctrl->InsertItem(name,(parent==NULL)?TVI_ROOT:(parent->handle));
  ctrl->SetItemData(handle,(DWORD_PTR) this);
}


/* ========================================================================== */
/*                              kd_metashow                                   */
/* ========================================================================== */

/******************************************************************************/
/*                        kd_metashow::kd_metashow                            */
/******************************************************************************/

kd_metashow::kd_metashow(CKdu_showApp *app, CWnd* pParent)
	: CDialog()
{
  started = false; // Avoid executing code in message handlers for a while
  this->app = app;
  src = NULL;
  tree = display = NULL;
  Create(kd_metashow::IDD, pParent);
  get_tree_ctrl()->SetBkColor(0x00A0FFFF);
  ShowWindow(SW_SHOW);

  display_buf_len = get_edit_ctrl()->GetLimitText();
  if (display_buf_len < 1024)
    display_buf_len = 1024;
  display_buf = new char[display_buf_len];
  display_buf_pos = display_box_pos = 0;
  display = NULL;

  num_codestreams = num_jplh = num_jpch = 0;

  RECT rect;
  GetClientRect(&rect);
  dialog_dims.x = rect.right - rect.left;
  dialog_dims.y = rect.bottom - rect.top;
  get_tree_ctrl()->GetWindowRect(&rect);
  tree_dims.x = rect.right - rect.left;
  tree_dims.y = rect.bottom - rect.top;
  get_edit_ctrl()->GetWindowRect(&rect);
  edit_dims.x = rect.right - rect.left;
  edit_dims.y = rect.bottom - rect.top;
  started = true; // Enable code in message handlers
}

/******************************************************************************/
/*                        kd_metashow::~kd_metashow                           */
/******************************************************************************/

kd_metashow::~kd_metashow()
{
  if (tree != NULL)
    delete tree;
  if (display_buf != NULL)
    delete[] display_buf;
}

/******************************************************************************/
/*                          kd_metashow::activate                             */
/******************************************************************************/

void
  kd_metashow::activate(jp2_family_src *src)
{
  deactivate();
  this->src = src;
  tree = new kd_metanode(this,NULL);
  tree->insert_into_view();
  update_tree();
}

/******************************************************************************/
/*                         kd_metashow::update_tree                           */
/******************************************************************************/

void
  kd_metashow::update_tree()
{
  if ((src == NULL) || (tree == NULL))
    return;
  jpx_meta_manager meta_manager = app->get_meta_manager();
  if (meta_manager.exists())
    meta_manager.load_matches(-1,NULL,-1,NULL);
  tree->update_structure(NULL,src->is_top_level_complete(),meta_manager);
  if ((display != NULL) && (display_box_pos >= 0))
    display->update_display();
}

/******************************************************************************/
/*                         kd_metashow::deactivate                            */
/******************************************************************************/

void
  kd_metashow::deactivate()
{
  get_tree_ctrl()->DeleteAllItems();
  get_edit_ctrl()->SetWindowText("");
  if (tree != NULL)
    delete tree;
  tree = display = NULL;
  src = NULL;
  num_codestreams = num_jpch = num_jplh = 0;
}

/******************************************************************************/
/*                   kd_metashow::select_matching_metanode                    */
/******************************************************************************/

void
  kd_metashow::select_matching_metanode(jpx_metanode jpx_node)
{
  kd_metanode *node = find_matching_metanode(tree,jpx_node);
  if (node != NULL)
    {
      get_tree_ctrl()->EnsureVisible(node->handle);
      get_tree_ctrl()->SelectItem(node->handle);
    }
}

/******************************************************************************/
/*                      kd_metashow::DoDataExchange                           */
/******************************************************************************/

void kd_metashow::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(kd_metashow)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(kd_metashow, CDialog)
	//{{AFX_MSG_MAP(kd_metashow)
	ON_WM_CLOSE()
	ON_NOTIFY(TVN_SELCHANGED, IDC_META_TREE, OnSelchangedMetaTree)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/******************************************************************************/
/*                       kd_metashow::PostNcDestroy                           */
/******************************************************************************/

void kd_metashow::PostNcDestroy() 
{
  CDialog::PostNcDestroy();
  if (app != NULL)
    {
      assert(app->metashow == this);
      app->metashow = NULL;
      app = NULL;
    }
  delete this;
}

/******************************************************************************/
/*                           kd_metashow::OnClose                             */
/******************************************************************************/

void kd_metashow::OnClose() 
{
  DestroyWindow();	
}

/******************************************************************************/
/*                            kd_metashow::OnOK                               */
/******************************************************************************/

void kd_metashow::OnOK() 
{
  DestroyWindow();	
}

/******************************************************************************/
/*                     kd_metashow::OnSelchangedMetaTree                      */
/******************************************************************************/

void kd_metashow::OnSelchangedMetaTree(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

  kd_metanode *node = (kd_metanode *) pNMTreeView->itemNew.lParam;
  *pResult = 0;
  if (node == NULL)
    return;
  if (display != NULL)
    { // Clear the display
      display_box_pos = -1;
      display_buf_pos = 0;
      display = NULL;
      get_edit_ctrl()->SetWindowText("");
    }
  if (node->can_display)
    { // Display the contents of this node
      display = node;
      display_buf_pos = display_box_pos = 0;
      node->update_display();
    }
  if (node->codestream_idx >= 0)
    app->set_codestream(node->codestream_idx);
  else if (node->compositing_layer_idx >= 0)
    app->set_compositing_layer(node->compositing_layer_idx);
  else if (node->box_type == jp2_composition_4cc)
    app->OnMultiComponent();
}

/******************************************************************************/
/*                            kd_metashow::OnSize                             */
/******************************************************************************/

void kd_metashow::OnSize(UINT nType, int cx, int cy) 
{
  CDialog::OnSize(nType, cx, cy);
  if (!started)
    return;

  kdu_coords new_dims;
  new_dims.x = cx; new_dims.y = cy;
  if (dialog_dims != new_dims)
    {
      kdu_coords change = new_dims - dialog_dims;
      tree_dims.y += change.y;
      edit_dims.y += change.y;
      edit_dims.x += change.x;
      if ((edit_dims.x >= 50) && (edit_dims.y >= 50))
        get_edit_ctrl()->SetWindowPos(NULL,0,0,edit_dims.x,edit_dims.y,
                                      SWP_NOMOVE | SWP_NOOWNERZORDER);
      if ((tree_dims.x >= 50) && (tree_dims.y >= 50))
        get_tree_ctrl()->SetWindowPos(NULL,0,0,tree_dims.x,tree_dims.y,
                                      SWP_NOMOVE | SWP_NOOWNERZORDER);
    }
  dialog_dims = new_dims;
}
