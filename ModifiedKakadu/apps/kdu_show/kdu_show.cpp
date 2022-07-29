/******************************************************************************/
// File: kdu_show.cpp [scope = APPS/SHOW]
// Version: Kakadu, V6.0
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
   Implements the application object of the interactive JPEG2000 viewer,
"kdu_show".  Menu processing and idle-time decompressor processing are all
controlled from here.  The "kdu_show" application demonstrates some of the
support offered by the Kakadu framework for interactive applications,
including persistence and incremental region-based decompression.
*******************************************************************************/

/*
5) Make changes to Unix "kdu_server" and test it out on the MAC
8) Update things in "Overview.txt"
9) Make minor corrections to precinct_ref handling in the core system
10) Update web-pages
*/

#include "stdafx.h"
#include <math.h>
#include "kdu_show.h"
#include "kd_metadata_editor.h"
#include "MainFrm.h"
#include "kdu_arch.h"
#include "kdu_messaging.h"
#include "kdu_threads.h"
#include "kdu_utils.h"
#include "kdu_show.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static const char *jpip_channel_types[4] = {"http","http-tcp","none",NULL};

CKdu_showApp theApp; // There can only be one application object.


/* ========================================================================== */
/*                            Internal Functions                              */
/* ========================================================================== */

/******************************************************************************/
/* INLINE                         find_scale                                  */
/******************************************************************************/

static inline float
  find_scale(int expand, int discard_levels)
  /* Returns `expand'*2^{-`discard_levels'}. */
{
  float scale = (float) expand;
  for (; discard_levels > 0; discard_levels--)
    scale *= 0.5F;
  return scale;
}

/******************************************************************************/
/* STATIC                      register_protocols                             */
/******************************************************************************/

static void
  register_protocols(const char *executable_path)
  /* Sets up the registry entries required to register the JPIP protocol
     variants which are currently supported.  URL's of the form
     <protocol name>:... will be passed into a newly launched instance of
     the present application when clicked within the iExplorer application. */
{
  DWORD disposition;
  HKEY root_key, icon_key, shell_key, open_key, command_key;

  const char *app_name = strrchr(executable_path,'\\');
  if (app_name == NULL)
    app_name = executable_path;
  else
    app_name++;

  const char *description = "JPIP Interactive Imaging Protocol";
  char *command_string = new char[strlen(executable_path)+4];
  strcpy(command_string,executable_path);
  strcat(command_string," %1");

  do {
      if (RegCreateKeyEx(HKEY_CLASSES_ROOT,"jpip",0,REG_NONE,
                         REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,
                         &root_key,&disposition) != ERROR_SUCCESS)
        continue;
      if (RegSetValueEx(root_key,NULL,0,REG_SZ,(kdu_byte *) description,
                        (int) strlen(description)+1) != ERROR_SUCCESS)
        continue;
      if (RegSetValueEx(root_key,"URL Protocol",0,REG_SZ,
                        (kdu_byte *)(""),1) != ERROR_SUCCESS)
        continue;
      if (RegCreateKeyEx(root_key,"DefaultIcon",0,REG_NONE,
                         REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,
                         &icon_key,&disposition) != ERROR_SUCCESS)
        continue;
      if (RegSetValueEx(icon_key,NULL,0,REG_SZ,(kdu_byte *) app_name,
                        (int) strlen(app_name)+1) != ERROR_SUCCESS)
        continue;
      if (RegCreateKeyEx(root_key,"shell",0,REG_NONE,
                         REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,
                         &shell_key,&disposition) != ERROR_SUCCESS)
        continue;
      if (RegCreateKeyEx(shell_key,"open",0,REG_NONE,
                         REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,
                         &open_key,&disposition) != ERROR_SUCCESS)
        continue;
      if (RegCreateKeyEx(open_key,"command",0,REG_NONE,
                         REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,
                         &command_key,&disposition) != ERROR_SUCCESS)
        continue;
      RegSetValueEx(command_key,NULL,0,REG_SZ,(kdu_byte *) command_string,
                    (int) strlen(command_string)+1);
    } while (0);

  delete[] command_string;
}

/******************************************************************************/
/* STATIC                    compare_file_pathnames                           */
/******************************************************************************/

static bool
  compare_file_pathnames(const char *fname1, const char *fname2)
{
  if ((fname1 == NULL) || (fname2 == NULL))
    return false;
  for (; (*fname1 != '\0') && (*fname2 != '\0'); fname1++, fname2++)
    {
      if (toupper(*fname1) == toupper(*fname2))
        continue;
      if (((*fname1 != '/') && (*fname1 != '\\')) ||
          ((*fname2 != '/') && (*fname2 != '\\')))
        break;
    }
  return (*fname1 == '\0') && (*fname2 == '\0');
}

/******************************************************************************/
/* STATIC                          copy_tile                                  */
/******************************************************************************/

static void
  copy_tile(kdu_tile tile_in, kdu_tile tile_out)
  /* Walks through all code-blocks of the tile in raster scan order, copying
     them from the input to the output tile. */
{
  int c, num_components = tile_out.get_num_components();
  for (c=0; c < num_components; c++)
    {
      kdu_tile_comp comp_in;  comp_in  = tile_in.access_component(c);
      kdu_tile_comp comp_out; comp_out = tile_out.access_component(c);
      int r, num_resolutions = comp_out.get_num_resolutions();
      for (r=0; r < num_resolutions; r++)
        {
          kdu_resolution res_in;  res_in  = comp_in.access_resolution(r);
          kdu_resolution res_out; res_out = comp_out.access_resolution(r);
          int b, min_band;
          int num_bands = res_in.get_valid_band_indices(min_band);
          for (b=min_band; num_bands > 0; num_bands--, b++)
            {
              kdu_subband band_in;  band_in = res_in.access_subband(b);
              kdu_subband band_out; band_out = res_out.access_subband(b);
              kdu_dims blocks_in;  band_in.get_valid_blocks(blocks_in);
              kdu_dims blocks_out; band_out.get_valid_blocks(blocks_out);
              if ((blocks_in.size.x != blocks_out.size.x) ||
                  (blocks_in.size.y != blocks_out.size.y))
                { kdu_error e; e << "Transcoding operation cannot proceed: "
                  "Code-block partitions for the input and output "
                  "code-streams do not agree."; }
              kdu_coords idx;
              for (idx.y=0; idx.y < blocks_out.size.y; idx.y++)
                for (idx.x=0; idx.x < blocks_out.size.x; idx.x++)
                  {
                    kdu_block *in  = band_in.open_block(idx+blocks_in.pos);
                    kdu_block *out = band_out.open_block(idx+blocks_out.pos);
                    if (in->K_max_prime != out->K_max_prime)
                      { kdu_error e;
                        e << "Cannot copy blocks belonging to subbands with "
                             "different quantization parameters."; }
                    if ((in->size.x != out->size.x) ||
                        (in->size.y != out->size.y))  
                      { kdu_error e; e << "Cannot copy code-blocks with "
                        "different dimensions."; }
                    out->missing_msbs = in->missing_msbs;
                    if (out->max_passes < (in->num_passes+2))
                      out->set_max_passes(in->num_passes+2,false);
                    out->num_passes = in->num_passes;
                    int num_bytes = 0;
                    for (int z=0; z < in->num_passes; z++)
                      {
                        num_bytes += (out->pass_lengths[z]=in->pass_lengths[z]);
                        out->pass_slopes[z] = in->pass_slopes[z];
                      }
                    if (out->max_bytes < num_bytes)
                      out->set_max_bytes(num_bytes,false);
                    memcpy(out->byte_buffer,in->byte_buffer,(size_t) num_bytes);
                    band_in.close_block(in);
                    band_out.close_block(out);
                  }
            }
        }
    }
}

/*****************************************************************************/
/* STATIC                write_metadata_box_from_file                        */
/*****************************************************************************/

static void
  write_metadata_box_from_file(jp2_output_box *container, kdms_file *file)
{
  const char *filename = file->get_pathname();
  FILE *fp = fopen(filename,"rb");
  if (fp != NULL)
    {
      kdu_byte buf[512];
      size_t xfer_bytes;
      while ((xfer_bytes = fread(buf,1,512,fp)) > 0)
        container->write(buf,(int)xfer_bytes);
    }
  fclose(fp);
}

/*****************************************************************************/
/* STATIC                    write_metanode_to_jp2                           */
/*****************************************************************************/

static void
  write_metanode_to_jp2(jpx_metanode node, jp2_output_box &tgt,
                        kdms_file_services *file_server)
/* Writes the JP2 box represented by `node' or (if `node' is a numlist) from
   its descendants, to the `tgt' object.  Does not recursively visit
   descendants since this would require the writing of asoc boxes, which are
   not part of the JP2 file format.  Writes only boxes which are JP2
   compatible.  Uses the `file_server' object to resolve boxes whose contents
   have been defined by the metadata editor. */
{
  bool rres;
  int num_cs, num_l;
  int c, num_children = 0;
  node.count_descendants(num_children);
  if (node.get_numlist_info(num_cs,num_l,rres))
    {
      jpx_metanode scan;
      for (c=0; c < num_children; c++)
        if ((scan=node.get_descendant(c)).exists())
          write_metanode_to_jp2(scan,tgt,file_server);
      return;
    }
  if (num_children > 0)
    return; // Don't write nodes with descendants
  
  kdu_uint32 box_type = node.get_box_type();
  if ((box_type != jp2_label_4cc) && (box_type != jp2_xml_4cc) &&
      (box_type != jp2_iprights_4cc) && (box_type != jp2_uuid_4cc) &&
      (box_type != jp2_uuid_info_4cc))
    return; // Not a box we should write to a JP2 file
  
  int num_bytes;
  const char *label = node.get_label();
  if (label != NULL)
    {
      tgt.open_next(jp2_label_4cc);
      tgt.set_target_size((kdu_long)(num_bytes=(int)strlen(label)));
      tgt.write((kdu_byte *)label,num_bytes);
      tgt.close();
      return;
    }
  
  jp2_input_box in_box;
  if (node.open_existing(in_box))
    {
      tgt.open_next(box_type);
      if (in_box.get_remaining_bytes() > 0)
        tgt.set_target_size((kdu_long)(in_box.get_remaining_bytes()));
      kdu_byte buf[512];
      while ((num_bytes = in_box.read(buf,512)) > 0)
        tgt.write(buf,num_bytes);
      tgt.close();
      return;
    }
  
  int i_param;
  void *addr_param;
  kdms_file *file;
  if (node.get_delayed(i_param,addr_param) &&
      (addr_param == (void *)file_server) &&
      ((file = file_server->find_file(i_param)) != NULL))
    {
      tgt.open_next(box_type);
      write_metadata_box_from_file(&tgt,file);
      tgt.close();
    }
}


/* ========================================================================== */
/*                     Error and Warning Message Handlers                     */
/* ========================================================================== */

/******************************************************************************/
/* CLASS                         core_messages_dlg                            */
/******************************************************************************/

class core_messages_dlg : public CDialog
{
  public:
    core_messages_dlg(const char *src, CWnd* pParent = NULL)
      : CDialog(core_messages_dlg::IDD, pParent)
      {
        int num_chars;
        const char *sp;
        for (sp=src, num_chars=0; *sp != '\0'; sp++, num_chars++)
          if ((*sp == '&') || (*sp == '\\'))
            num_chars++; // Need to replicate these special characters
        string = new char[num_chars+1];
        for (sp=src, num_chars=0; *sp != '\0'; sp++)
          {
            string[num_chars++] = *sp;
            if ((*sp == '&') || (*sp == '\\'))
              string[num_chars++] = *sp;
          }
        string[num_chars] = '\0';
      }
    ~core_messages_dlg()
      {
        if (string != NULL)
          delete[] string;
      }
// Dialog Data
	//{{AFX_DATA(core_messages_dlg)
	enum { IDD = IDD_CORE_MESSAGES };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(core_messages_dlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
private:
  CStatic *get_static()
    {
      return (CStatic *) GetDlgItem(IDC_MESSAGE);
    }
private:
  char *string;
protected:
	// Generated message map functions
	//{{AFX_MSG(core_messages_dlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/******************************************************************************/
/*                      core_messages_dlg::DoDataExchange                     */
/******************************************************************************/

void
  core_messages_dlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(core_messages_dlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(core_messages_dlg, CDialog)
	//{{AFX_MSG_MAP(core_messages_dlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/******************************************************************************/
/*                      core_messages_dlg::OnInitDialog                       */
/******************************************************************************/

BOOL core_messages_dlg::OnInitDialog() 
{
  CDialog::OnInitDialog();

  while (*string == '\n')
    string++; // skip leading empty lines, if any.
  get_static()->SetWindowText(string);

  // Find the height of the displayed text
  int text_height = 0;
  SIZE text_size;
  CDC *dc = get_static()->GetDC();
  const char *scan = string;
  while (*scan != '\0')
    {
      const char *sp = strchr(scan,'\n');
      if (sp == NULL)
        sp = scan + strlen(scan);
      if (scan != sp)
        text_size = dc->GetTextExtent(scan,(int)(sp-scan));
      text_height += text_size.cy;
      scan = (*sp != '\0')?sp+1:sp;
    }
  get_static()->ReleaseDC(dc);

  // Resize windows to fit the text height

  WINDOWPLACEMENT dialog_placement, static_placement;
  GetWindowPlacement(&dialog_placement);
  get_static()->GetWindowPlacement(&static_placement);
  int dialog_width = dialog_placement.rcNormalPosition.right -
    dialog_placement.rcNormalPosition.left;
  int static_width = static_placement.rcNormalPosition.right -
    static_placement.rcNormalPosition.left;
  int dialog_height = dialog_placement.rcNormalPosition.bottom -
    dialog_placement.rcNormalPosition.top;
  int static_height = static_placement.rcNormalPosition.bottom -
    static_placement.rcNormalPosition.top;

  get_static()->SetWindowPos(NULL,0,0,static_width,text_height,
                             SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);
  SetWindowPos(NULL,0,0,dialog_width,text_height+8+dialog_height-static_height,
               SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);  
  return TRUE;
}

/******************************************************************************/
/* CLASS                     kd_message_collector                             */
/******************************************************************************/

class kd_message_collector : public kdu_message {
  /* This object is used to collect message text within the
     `CPropertiesDlg' object, so as to format and print descriptions of
     codestream parameter attributes.  It derives from the non-thread-safe
     `kdu_message' object, since multiple threads cannot access the same
     object simultaneously within `CPropertiesDlg'. */
  public: // Member functions
    kd_message_collector()
      {
        max_chars = 10; num_chars = 0;
        buffer = new char[max_chars+1]; *buffer = '\0';
      }
    ~kd_message_collector() { delete[] buffer; }
    const char *get_text() { return buffer; }
    void put_text(const char *string)
      {
        int new_chars = (int) strlen(string);
        if ((num_chars+new_chars) > max_chars)
          {
            max_chars = (num_chars+new_chars)*2;
            char *buf = new char[max_chars+1];
            strcpy(buf,buffer);
            delete[] buffer;
            buffer = buf;
          }
        num_chars += new_chars;
        strcat(buffer,string);
      }
    void flush(bool end_of_message)
      {
        if (end_of_message)
          { num_chars = 0; *buffer = '\0'; }
      }
  private: // Data
    int num_chars, max_chars;
    char *buffer;
  };

/******************************************************************************/
/* CLASS                   kd_core_message_collector                          */
/******************************************************************************/

class kd_core_message_collector : public kdu_thread_safe_message {
  /* This object is used to implement error and warning message services
     which are registered globally for use by all threads which may
     use `kdu_error' or `kdu_warning'.  Since this may happen from multiple
     threads, we derive this object from the thread-safe messaging service,
     `kdu_thread_safe_message'.  It is important that we explicitly call the
     base object's `flush' function at the end of the derived object's
     `flush' implementation, since this will release the mutex which
     serializes access to the messaging service. */
  public: // Member functions
    kd_core_message_collector(bool for_errors)
      {
        max_chars = 10; num_chars = 0;
        buffer = new char[max_chars+1]; *buffer = '\0';
        raise_end_of_message_exception = for_errors;
      }
    ~kd_core_message_collector() { delete[] buffer; }
    void put_text(const char *string)
      {
        int new_chars = (int) strlen(string);
        if ((num_chars+new_chars) > max_chars)
          {
            max_chars = (num_chars+new_chars)*2;
            char *buf = new char[max_chars+1];
            strcpy(buf,buffer);
            delete[] buffer;
            buffer = buf;
          }
        num_chars += new_chars;
        strcat(buffer,string);
      }
    void flush(bool end_of_message)
      {
        if (end_of_message)
          {
            core_messages_dlg messages(buffer,theApp.frame_wnd);
            num_chars = 0;   *buffer = '\0';
            messages.DoModal();
            kdu_thread_safe_message::flush(true); // May release other threads
            if (raise_end_of_message_exception)
              throw (int) 0;
          }
        else
          kdu_thread_safe_message::flush(false);
      }
  private: // Data
    int num_chars, max_chars;
    char *buffer;
    bool raise_end_of_message_exception;
  };

static kd_core_message_collector warn_collector(false);
static kd_core_message_collector err_collector(true);
static kdu_message_formatter warn_formatter(&warn_collector,50);
static kdu_message_formatter err_formatter(&err_collector,50);


/*===========================================================================*/
/*                                 kdms_file                                 */
/*===========================================================================*/

/*****************************************************************************/
/*                           kdms_file::kdms_file                            */
/*****************************************************************************/

kdms_file::kdms_file(kdms_file_services *owner)
{
  this->owner = owner;
  unique_id = owner->next_unique_id++;
  retain_count = 0; // Caller needs to explicitly retain it
  pathname = NULL;
  is_temporary = false;
  next = NULL;
  if ((prev = owner->tail) == NULL)
    owner->head = this;
  else
    prev->next = this;
  owner->tail = this;
}

/*****************************************************************************/
/*                           kdms_file::~kdms_file                           */
/*****************************************************************************/

kdms_file::~kdms_file()
{
  if (owner != NULL)
    { // Unlink from owner.
      if (prev == NULL)
        {
          assert(this == owner->head);
          owner->head = next;
        }
      else
        prev->next = next;
      if (next == NULL)
        {
          assert(this == owner->tail);
          owner->tail = prev;
        }
      else
        next->prev = prev;
      prev = next = NULL;
      owner = NULL;
    }
  if (is_temporary && (pathname != NULL))
    remove(pathname);
  if (pathname != NULL)
    delete[] pathname;
}

/*****************************************************************************/
/*                             kdms_file::release                            */
/*****************************************************************************/

void kdms_file::release()
{
  retain_count--;
  if (retain_count == 0)
    delete this;
}

/*****************************************************************************/
/*                       kdms_file::create_if_necessary                      */
/*****************************************************************************/

bool kdms_file::create_if_necessary(const char *initializer)
{
  if ((pathname == NULL) || !is_temporary)
    return false;
  const char *mode = (initializer == NULL)?"rb":"r";
  FILE *file = fopen(pathname,mode);
  if (file != NULL)
    { // File already exists
      fclose(file);
      return false;
    }
  mode = (initializer == NULL)?"wb":"w";
  file = fopen(pathname,mode);
  if (file == NULL)
    return false; // Cannot write to file
  if (initializer != NULL)
    fwrite(initializer,1,strlen(initializer),file);
  fclose(file);
  return true;
}


/*===========================================================================*/
/*                             kdms_file_services                            */
/*===========================================================================*/

/*****************************************************************************/
/*                   kdms_file_services::kdms_file_services                  */
/*****************************************************************************/

kdms_file_services::kdms_file_services(const char *source_pathname)
{
  head = tail = NULL;
  next_unique_id = 1;
  editors = NULL;
  base_pathname_confirmed = false;
  base_pathname = NULL;
  if (source_pathname != NULL)
    {
      base_pathname = new char[strlen(source_pathname)+8];
      strcpy(base_pathname,source_pathname);
      const char *cp = strrchr(base_pathname,'.');
      if (cp == NULL)
        strcat(base_pathname,"_meta_");
      else
        strcpy(base_pathname + (cp-base_pathname),"_meta_");
    }
}

/*****************************************************************************/
/*                   kdms_file_services::~kdms_file_services                 */
/*****************************************************************************/

kdms_file_services::~kdms_file_services()
{
  while (head != NULL)
    delete head;
  if (base_pathname != NULL)
    delete[] base_pathname;
  kdms_file_editor *editor;
  while ((editor=editors) != NULL)
    {
      editors = editor->next;
      delete editor;
    }
}

/*****************************************************************************/
/*                 kdms_file_services::find_new_base_pathname                */
/*****************************************************************************/

bool kdms_file_services::find_new_base_pathname()
{
  if (base_pathname != NULL)
    delete[] base_pathname;
  base_pathname_confirmed = false;
  base_pathname = new char[L_tmpnam+8];
  tmpnam(base_pathname);
  FILE *fp = fopen(base_pathname,"wb");
  if (fp == NULL)
    return false;
  fclose(fp);
  remove(base_pathname);
  base_pathname_confirmed = true;
  strcat(base_pathname,"_meta_");
  return true;
}

/*****************************************************************************/
/*                 kdms_file_services::confirm_base_pathname                 */
/*****************************************************************************/

void kdms_file_services::confirm_base_pathname()
{
  if (base_pathname_confirmed)
    return;
  if (base_pathname != NULL)
    {
      FILE *fp = fopen(base_pathname,"rb");
      if (fp != NULL)
        {
          fclose(fp); // Existing base pathname already exists; can't test it
          // easily for writability; safest to make new base path
          find_new_base_pathname();
        }
      else
        {
          fp = fopen(base_pathname,"wb");
          if (fp != NULL)
            { // All good; can write to this volume
              fclose(fp);
              remove(base_pathname);
              base_pathname_confirmed = true;
            }
          else
            find_new_base_pathname();
        }
    }
  if (base_pathname == NULL)
    find_new_base_pathname();
}

/*****************************************************************************/
/*                    kdms_file_services::retain_known_file                  */
/*****************************************************************************/

kdms_file *kdms_file_services::retain_known_file(const char *pathname)
{
  kdms_file *file;
  for (file=head; file != NULL; file=file->next)
    if (strcmp(file->pathname,pathname) == 0)
      break;
  if (file == NULL)
    {
      file = new kdms_file(this);
      file->pathname = new char[strlen(pathname)+1];
      strcpy(file->pathname,pathname);
    }
  file->is_temporary = false;
  file->retain();
  return file;
}

/*****************************************************************************/
/*                     kdms_file_services::retain_tmp_file                   */
/*****************************************************************************/

kdms_file *kdms_file_services::retain_tmp_file(const char *suffix)
{
  if (!base_pathname_confirmed)
    confirm_base_pathname();
  kdms_file *file = new kdms_file(this);
  file->pathname = new char[strlen(base_pathname)+81];
  strcpy(file->pathname,base_pathname);
  char *cp = file->pathname + strlen(file->pathname);
  int extra_id = 0;
  bool found_new_filename = false;
  while (!found_new_filename)
    {
      if (extra_id == 0)
        sprintf(cp,"_tmp_%d.%s",file->unique_id,suffix);
      else
        sprintf(cp,"_tmp%d_%d.%s",extra_id,file->unique_id,suffix);
      FILE *fp = fopen(file->pathname,"rb");
      if (fp != NULL)
        {
          fclose(fp); // File already exists
          extra_id++;
        }
      else
        found_new_filename = true;
    }
  file->is_temporary = true;
  file->retain();
  return file;
}

/*****************************************************************************/
/*                        kdms_file_services::find_file                      */
/*****************************************************************************/

kdms_file *kdms_file_services::find_file(int identifier)
{
  kdms_file *scan;
  for (scan=head; scan != NULL; scan=scan->next)
    if (scan->unique_id == identifier)
      return scan;
  return NULL;
}

/*****************************************************************************/
/*               kdms_file_services::get_editor_for_doc_type                 */
/*****************************************************************************/

kdms_file_editor *
  kdms_file_services::get_editor_for_doc_type(const char *doc_suffix,
                                              int which)
{
  kdms_file_editor *scan;
  int n;
  
  for (n=0, scan=editors; scan != NULL; scan=scan->next)
    if (strcmp(scan->doc_suffix,doc_suffix) == 0)
      {
        if (n == which)
          return scan;
        n++;
      }
  
  if (n > 0)
    return NULL;
  
  // If we get here, we didn't find any matching editors at all.  Let's see if
  // we can add some.
  char extension[80];
  extension[0] = '.';
  strncpy(extension+1,doc_suffix,78);
  extension[79] = '\0';
  DWORD val_type, prog_id_len;
  char prog_id[256]; // Only collect prog-id's with 255 chars or less
  HKEY ext_key;
  if (RegOpenKeyEx(HKEY_CLASSES_ROOT,extension,0,KEY_READ,
                   &ext_key) == ERROR_SUCCESS)
    {
      HKEY openwith_key;
      if (RegOpenKeyEx(ext_key,"OpenWithProgids",0,KEY_READ,
                       &openwith_key) == ERROR_SUCCESS)
        {
          for (n=0; true; n++)
            {
              prog_id_len = 255;
              if (RegEnumValue(openwith_key,n,prog_id,&prog_id_len,
                               NULL,NULL,NULL,NULL) != ERROR_SUCCESS)
                break;
              add_editor_progid_for_doc_type(doc_suffix,prog_id);
            }
          RegCloseKey(openwith_key);
        }
      for (n=0; true; n++)
        {
          char valname[80];
          valname[0] = '\0';
          DWORD name_len=79;
          prog_id_len = 255;
          if (RegEnumValue(ext_key,n,valname,&name_len,NULL,
                           &val_type,(LPBYTE) prog_id,
                           &prog_id_len) != ERROR_SUCCESS)
            break;
          if ((prog_id_len > 0) && (val_type == REG_SZ) && (*valname == '\0'))
            add_editor_progid_for_doc_type(doc_suffix,prog_id);
        }
      RegCloseKey(ext_key);
    }

  // Now go back and try to answer the request again
  for (n=0, scan=editors; scan != NULL; scan=scan->next)
    if (strcmp(scan->doc_suffix,doc_suffix) == 0)
      {
        if (n == which)
          return scan;
        n++;
      }
  
  return NULL;
}

/*****************************************************************************/
/*           kdms_file_services::add_editor_progid_for_doc_type              */
/*****************************************************************************/

void
  kdms_file_services::add_editor_progid_for_doc_type(const char *doc_suffix,
                                                     const char *prog_id)
{
  HKEY prog_key;
  if (RegOpenKeyEx(HKEY_CLASSES_ROOT,prog_id,0,KEY_READ,
                   &prog_key) == ERROR_SUCCESS)
    {
      HKEY shell_key;
      if (RegOpenKeyEx(prog_key,"shell",0,KEY_READ,
                       &shell_key) == ERROR_SUCCESS)
        {
          HKEY action_key;
          for (int action=0; action < 2; action++)
            {
              const char *action_verb = (action==1)?"edit":"open";
              if (RegOpenKeyEx(shell_key,action_verb,0,KEY_READ,
                               &action_key) != ERROR_SUCCESS)
                continue;
              HKEY command_key;
              if (RegOpenKeyEx(action_key,"command",0,KEY_READ,
                               &command_key) == ERROR_SUCCESS)
                {
                  char name_string[81];
                  char command_string[MAX_PATH+81];
                  for (int n=0; true; n++)
                    {
                      DWORD val_type;
                      DWORD name_chars = 80;
                      DWORD command_chars = MAX_PATH+80;
                      name_string[0] = command_string[0] = '\0';
                      if (RegEnumValue(command_key,n,name_string,&name_chars,
                                       NULL,&val_type,(LPBYTE) command_string,
                                       &command_chars) != ERROR_SUCCESS)
                        break;
                      if (name_string[0] != '\0')
                        continue;

                      // Now we have the default value for the "command" key
                      if (val_type == REG_EXPAND_SZ)
                        { // Need to expand environment variables.
                          char expand_string[MAX_PATH+80];
                          DWORD expand_len =
                            ExpandEnvironmentStrings(command_string,
                                                     expand_string,MAX_PATH+79);
                          if ((expand_len == 0) ||
                              (expand_len > (MAX_PATH+79)))
                            break; // Expand not successful
                          strcpy(command_string,expand_string);
                        }
                      else if (val_type != REG_SZ)
                        break;
                      char *sep, *app_path = command_string;
                      while ((*app_path == ' ') || (*app_path == '\t'))
                        app_path++;
                      bool have_arg = false;
                      for (sep=app_path; *sep != '\0'; sep++)
                        if ((sep[0] == '%') && (sep[1] == '1'))
                        { have_arg = true; break; }
                      if ((sep > app_path) && have_arg && (sep[-1]=='\"'))
                        sep--;
                      while ((sep > command_string) &&
                             ((sep[-1] == ' ') || (sep[-1] == '\t')))
                        sep--;
                      *sep = '\0';
                      if (!have_arg)
                        { // Still OK if "/dde" is found
                          if (((sep-app_path) < 6) ||
                              (sep[-4] != '/') || (sep[-3] != 'd') ||
                              (sep[-2] != 'd') || (sep[-1] != 'e'))
                            break;
                        }
                      if (*app_path != '\0')
                        add_editor_for_doc_type(doc_suffix,app_path);
                      break;
                    }
                  RegCloseKey(command_key);
                }
              RegCloseKey(action_key);
            }
          RegCloseKey(shell_key);
        }
      RegCloseKey(prog_key);
    }
}

/*****************************************************************************/
/*               kdms_file_services::add_editor_for_doc_type                 */
/*****************************************************************************/

kdms_file_editor *
  kdms_file_services::add_editor_for_doc_type(const char *doc_suffix,
                                              const char *app_pathname)
{
  kdms_file_editor *scan, *prev;
  for (scan=editors, prev=NULL; scan != NULL; prev=scan, scan=scan->next)
    if (strcmp(scan->app_pathname,app_pathname) == 0)
      break;
  
  if (scan != NULL)
    { // Found an existing editor
      if (prev == NULL)
        editors = scan->next;
      else
        prev->next = scan->next;
      scan->next = editors;
      editors = scan;
      return scan;
    }
  
  scan = new kdms_file_editor;
  scan->doc_suffix = new char[strlen(doc_suffix)+1];
  strcpy(scan->doc_suffix,doc_suffix);
  scan->app_pathname = new char[strlen(app_pathname)+1];
  strcpy(scan->app_pathname,app_pathname);
  
  const char *cp, *name_start = scan->app_pathname;
  for (cp=name_start; *cp != '\0'; cp++)
    if ((*cp == '\"') && (scan->app_pathname[0] == '\"') && (cp > name_start))
      break;
    else if ((cp[0] == '.') && (toupper(cp[1]) == (int)'E') &&
             (toupper(cp[2]) == (int)'X') && (toupper(cp[3]) == (int)'E'))
      break;
    else
      if (((*cp == '/') || (*cp == '\\') || (*cp == ':')) && (cp[1] != '\0'))
        name_start = cp+1;
  scan->app_name = new char[(cp-name_start)+1];
  memcpy(scan->app_name,name_start,(cp-name_start));
  scan->app_name[cp-name_start] = '\0';

  kdms_file_editor *dup;
  for (dup=editors; dup != NULL; dup=dup->next)
    if (strcmp(dup->app_name,scan->app_name) == 0)
      {
        delete scan;
        return dup;
      }
  scan->next = editors;
  editors = scan;
  return scan;
}



/* ========================================================================== */
/*                                 kd_settings                                */
/* ========================================================================== */

/******************************************************************************/
/*                           kd_settings::kd_settings                         */
/******************************************************************************/

kd_settings::kd_settings()
{
  open_save_dir = NULL; open_idx = save_idx = 1;
  jpip_server = jpip_proxy = jpip_cache = NULL;
  jpip_request = jpip_channel = NULL;
  set_jpip_channel_type(jpip_channel_types[0]);
}

/******************************************************************************/
/*                          kd_settings::~kd_settings                         */
/******************************************************************************/

kd_settings::~kd_settings()
{
  if (open_save_dir != NULL)
    delete[] open_save_dir;
  if (jpip_server != NULL)
    delete[] jpip_server;
  if (jpip_proxy != NULL)
    delete[] jpip_proxy;
  if (jpip_cache != NULL)
    delete[] jpip_cache;
  if (jpip_request != NULL)
    delete[] jpip_request;
  if (jpip_channel != NULL)
    delete[] jpip_channel;
}

/******************************************************************************/
/*                        kd_settings::save_to_registry                       */
/******************************************************************************/

void
  kd_settings::save_to_registry(CWinApp *app)
{
  app->WriteProfileString("files","directory",get_open_save_dir());
  app->WriteProfileInt("files","open-index",open_idx);
  app->WriteProfileInt("files","save-index",save_idx);
  app->WriteProfileString("jpip","cache-dir",get_jpip_cache());
  app->WriteProfileString("jpip","server",get_jpip_server());
  app->WriteProfileString("jpip","request",get_jpip_request());
  app->WriteProfileString("jpip","channel-type",get_jpip_channel_type());
}

/******************************************************************************/
/*                       kd_settings::load_from_registry                      */
/******************************************************************************/

void
  kd_settings::load_from_registry(CWinApp *app)
{
  set_open_save_dir(app->GetProfileString("files","directory").GetBuffer(0));
  open_idx = app->GetProfileInt("files","open-index",1);
  save_idx = app->GetProfileInt("files","save-index",1);
  const char *missing_cache_dir = "***";
  CString existing_cache_dir =
    app->GetProfileString("jpip","cache-dir",missing_cache_dir).GetBuffer(0);
  if (strcmp(existing_cache_dir,"***") == 0)
    {
      char temp_path[MAX_PATH+1];
      int len = GetTempPath(MAX_PATH,temp_path);
      if (len > 0)
        set_jpip_cache(temp_path);
    }
  else
    set_jpip_cache(existing_cache_dir);
  set_jpip_server(app->GetProfileString("jpip","server").GetBuffer(0));
  set_jpip_request(app->GetProfileString("jpip","request").GetBuffer(0));
  CString c_string = app->GetProfileString("jpip","channel-type");
  const char *string = c_string.GetBuffer(0);
  if ((string == NULL) || (*string == '\0'))
    string = jpip_channel_types[0];
  set_jpip_channel_type(string);
}


/* ========================================================================== */
/*                               kd_bitmap_buf                                */
/* ========================================================================== */

/******************************************************************************/
/*                        kd_bitmap_buf::create_bitmap                        */
/******************************************************************************/

void
  kd_bitmap_buf::create_bitmap(kdu_coords size)
{
  if (bitmap != NULL)
    {
      assert(this->size == size);
      return;
    }

  memset(&bitmap_info,0,sizeof(bitmap_info));
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = size.x;
  bitmap_info.bmiHeader.biHeight = -size.y;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;
  void *buf_addr;
  bitmap = CreateDIBSection(NULL,&bitmap_info,DIB_RGB_COLORS,&buf_addr,NULL,0);
  if (bitmap == NULL)
    { kdu_error e; e << "Unable to allocate sufficient bitmap surfaces "
      "to service `kdu_region_compositor'."; }
  this->buf = (kdu_uint32 *) buf_addr;
  this->row_gap = size.x;
  this->size = size;
}


/* ========================================================================== */
/*                            kd_bitmap_compositor                            */
/* ========================================================================== */

/******************************************************************************/
/*                    kd_bitmap_compositor::allocate_buffer                   */
/******************************************************************************/

kdu_compositor_buf *
  kd_bitmap_compositor::allocate_buffer(kdu_coords min_size,
                                        kdu_coords &actual_size,
                                        bool read_access_required)
{
  if (min_size.x < 1) min_size.x = 1;
  if (min_size.y < 1) min_size.y = 1;
  actual_size = min_size;
  int row_gap = actual_size.x;

  kd_bitmap_buf *prev=NULL, *elt=NULL;

  // Start by trying to find a compatible buffer on the free list.
  for (elt=free_list, prev=NULL; elt != NULL; prev=elt, elt=elt->next)
    if (elt->size == actual_size)
      break;

  bool need_init = false;
  if (elt != NULL)
    { // Remove from the free list
      if (prev == NULL)
        free_list = elt->next;
      else
        prev->next = elt->next;
      elt->next = NULL;
    }
  else
    {
      // Delete the entire free list: one way to avoid accumulating buffers
      // whose sizes are no longer helpful
      while ((elt=free_list) != NULL)
        { free_list = elt->next;  delete elt; }

      // Allocate a new element
      elt = new kd_bitmap_buf;
      need_init = true;
    }
  elt->next = active_list;
  active_list = elt;
  elt->set_read_accessibility(read_access_required);
  if (need_init)
    elt->create_bitmap(actual_size);
  return elt;
}

/******************************************************************************/
/*                    kd_bitmap_compositor::delete_buffer                     */
/******************************************************************************/

void
  kd_bitmap_compositor::delete_buffer(kdu_compositor_buf *_buffer)
{
  kd_bitmap_buf *buffer = (kd_bitmap_buf *) _buffer;
  kd_bitmap_buf *scan, *prev;
  for (prev=NULL, scan=active_list; scan != NULL; prev=scan, scan=scan->next)
    if (scan == buffer)
      break;
  assert(scan != NULL);
  if (prev == NULL)
    active_list = scan->next;
  else
    prev->next = scan->next;

  scan->next = free_list;
  free_list = scan;
}


/* ========================================================================== */
/*                               CAboutDlg Class                              */
/* ========================================================================== */

class CAboutDlg : public CDialog {
  public:
    CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

  protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
  };

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/******************************************************************************/
/*                             CAboutDlg::CAboutDlg                           */
/******************************************************************************/

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
  //{{AFX_DATA_INIT(CAboutDlg)
  //}}AFX_DATA_INIT
}

/******************************************************************************/
/*                           CAboutDlg::DoDataExchange                        */
/******************************************************************************/

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CAboutDlg)
  //}}AFX_DATA_MAP
}

/* ========================================================================== */
/*                              CJpipOpenDlg Class                            */
/* ========================================================================== */

class CJpipOpenDlg : public CDialog {
  public:
    CJpipOpenDlg(char *server_name, int server_len,
                 char *request, int request_len,
                 char *channel_type, int channel_length,
                 char *proxy_name, int proxy_len,
                 char *cache_dir_name, int cache_dir_len,
                 CWnd *pParent = NULL);

// Dialog Data
	//{{AFX_DATA(CJpipOpenDlg)
	enum { IDD = IDD_JPIP_OPEN };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CJpipOpenDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
  protected:
	// Generated message map functions
	//{{AFX_MSG(CJpipOpenDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

  private: // Base object overrides
    BOOL OnInitDialog();
    void OnOK();

  private: // Control access
    CEdit *get_server()
      { return (CEdit *) GetDlgItem(IDC_JPIP_SERVER); }
    CEdit *get_request()
      { return (CEdit *) GetDlgItem(IDC_JPIP_REQUEST); }
    CComboBox *get_channel_type()
      { return (CComboBox *) GetDlgItem(IDC_JPIP_CHANNEL_TYPE); }
    CEdit *get_proxy()
      { return (CEdit *) GetDlgItem(IDC_JPIP_PROXY); }
    CEdit *get_cache_dir()
      { return (CEdit *) GetDlgItem(IDC_JPIP_CACHE_DIR); }
  private:
    char *server_name, *request, *channel_type;
    char *proxy_name, *cache_dir_name;
    int server_len, request_len, channel_len, proxy_len, cache_dir_len;
  };

/******************************************************************************/
/*                          CJpipOpenDlg::CJpipOpenDlg                        */
/******************************************************************************/

CJpipOpenDlg::CJpipOpenDlg(char *server_name, int server_len,
                           char *request, int request_len,
                           char *channel_type, int channel_len,
                           char *proxy_name, int proxy_len,
                           char *cache_dir_name, int cache_dir_len,
                           CWnd *pParent /*=NULL*/)
	: CDialog(CJpipOpenDlg::IDD, pParent)
{
  this->server_name = server_name; this->server_len = server_len;
  this->request = request; this->request_len = request_len;
  this->channel_type = channel_type; this->channel_len = channel_len;
  this->proxy_name = proxy_name; this->proxy_len = proxy_len;
  this->cache_dir_name = cache_dir_name; this->cache_dir_len = cache_dir_len;
	//{{AFX_DATA_INIT(CJpipOpenDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

/******************************************************************************/
/*                          CJpipOpenDlg::OnInitDialog                        */
/******************************************************************************/

BOOL
  CJpipOpenDlg::OnInitDialog()
{
  get_server()->SetWindowText(server_name);
  get_request()->SetWindowText(request);
  get_proxy()->SetWindowText(proxy_name);
  get_cache_dir()->SetWindowText(cache_dir_name);
  const char **scan;
  for (scan=jpip_channel_types; *scan != NULL; scan++)
    get_channel_type()->AddString(*scan);
  scan = jpip_channel_types;
  if (channel_type[0] == '\0')
    strncpy(channel_type,*scan,channel_len-1);
  get_channel_type()->SelectString(-1,channel_type);
  if (*server_name == '\0')
    GotoDlgCtrl(GetDlgItem(IDC_JPIP_SERVER));
  else
    GotoDlgCtrl(GetDlgItem(IDC_JPIP_REQUEST));
  return 0;
}

/******************************************************************************/
/*                         CJpipOpenDlg::DoDataExchange                       */
/******************************************************************************/

void
  CJpipOpenDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CJpipOpenDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CJpipOpenDlg, CDialog)
	//{{AFX_MSG_MAP(CJpipOpenDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/******************************************************************************/
/*                              CJpipOpenDlg::OnOK                            */
/******************************************************************************/

void
  CJpipOpenDlg::OnOK()
{
  int string_len;
  string_len = get_server()->GetLine(0,server_name,server_len-1);
  server_name[string_len] = '\0';
  string_len = get_request()->GetLine(0,request,request_len-1);
  request[string_len] = '\0';
  string_len = get_proxy()->GetLine(0,proxy_name,proxy_len-1);
  proxy_name[string_len] = '\0';
  string_len = get_cache_dir()->GetLine(0,cache_dir_name,cache_dir_len-1);
  cache_dir_name[string_len] = '\0';
  int cb_idx = get_channel_type()->GetCurSel();
  if (cb_idx != CB_ERR)
    {
      string_len = get_channel_type()->GetLBTextLen(cb_idx);
      if (string_len < channel_len)
        get_channel_type()->GetLBText(cb_idx,channel_type);
    }
  if (*server_name == '\0')
    {
      MessageBox("You must enter a server name or IP address!",
                 "Kdu_show: Dialog entry error",MB_OK|MB_ICONERROR);
      GotoDlgCtrl(GetDlgItem(IDC_JPIP_SERVER));
      return;
    }
  if (*request == '\0')
    {
      MessageBox("You must enter an object (e.g., file name) to be served!",
                 "Kdu_show: Dialog entry error",MB_OK|MB_ICONERROR);
      GotoDlgCtrl(GetDlgItem(IDC_JPIP_REQUEST));
      return;
    }
  if (*channel_type == '\0')
    {
      MessageBox("You must select a channel type\n"
                 "(or \"none\" for stateless communications)!",
                 "Kdu_show: Dialog entry error",MB_OK|MB_ICONERROR);
      GotoDlgCtrl(GetDlgItem(IDC_JPIP_CHANNEL_TYPE));
      return;
    }
  EndDialog(IDOK);
}


/* ========================================================================== */
/*                              CPropertyHelpDlg                              */
/* ========================================================================== */

class CPropertyHelpDlg : public CDialog
{
// Construction
public:
	CPropertyHelpDlg(const char *string, kdu_coords placement,
                   CWnd* pParent = NULL);

// Dialog Data
	//{{AFX_DATA(CPropertyHelpDlg)
	enum { IDD = IDD_PROPERTYHELP };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPropertyHelpDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
private:
  CEdit *get_edit() { return (CEdit *) GetDlgItem(IDC_MESSAGE); }
private:
  const char *string;
  kdu_coords placement;
protected:

	// Generated message map functions
	//{{AFX_MSG(CPropertyHelpDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/******************************************************************************/
/*                     CPropertyHelpDlg::CPropertyHelpDlg                     */
/******************************************************************************/

CPropertyHelpDlg::CPropertyHelpDlg(const char *string, kdu_coords placement,
                                   CWnd* pParent)
	: CDialog(CPropertyHelpDlg::IDD, pParent)
{
  this->string = string;
  this->placement = placement;
}

/******************************************************************************/
/*                       CPropertyHelpDlg::OnInitDialog                       */
/******************************************************************************/

BOOL CPropertyHelpDlg::OnInitDialog() 
{
  CDialog::OnInitDialog();
  while (*string == '\n')
    string++; // Skip leading empty lines, if any
  const char *sp;
  char *dp, *out_string;
  int out_string_len;
  for (sp=string, out_string_len=1; *sp != '\0'; sp++, out_string_len++)
    if (*sp == '\n')
      out_string_len++;
  out_string = new char[out_string_len];
  for (sp=string, dp=out_string; *sp != '\0'; *(dp++)=*(sp++))
    if (*sp == '\n')
      *(dp++) = '\r';
  *(dp++) = '\0'; assert((dp-out_string) == out_string_len);
  get_edit()->SetWindowText(out_string);
  delete[] out_string;

  // Move dialog window to desired position
  SetWindowPos(NULL,placement.x,placement.y,0,0,
               SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);

  // Find the height of the displayed text
  int text_height=0, text_lines=0;
  SIZE text_size;
  CDC *dc = get_edit()->GetDC();
  while (*string != '\0')
    {
      sp = strchr(string,'\n');
      if (sp == NULL)
        sp = string + strlen(string);
      if (string != sp)
        text_size = dc->GetTextExtent(string,(int)(sp-string));
      text_height += text_size.cy;
      text_lines++;
      if (text_lines == 24)
        break;
      string = (*sp != '\0')?sp+1:sp;
    }
  get_edit()->ReleaseDC(dc);

  // Resize windows to fit the text height

  WINDOWPLACEMENT dialog_placement, edit_placement;
  GetWindowPlacement(&dialog_placement);
  get_edit()->GetWindowPlacement(&edit_placement);
  int dialog_width = dialog_placement.rcNormalPosition.right -
    dialog_placement.rcNormalPosition.left;
  int edit_width = edit_placement.rcNormalPosition.right -
    edit_placement.rcNormalPosition.left;
  int dialog_height = dialog_placement.rcNormalPosition.bottom -
    dialog_placement.rcNormalPosition.top;
  int edit_height = edit_placement.rcNormalPosition.bottom -
    edit_placement.rcNormalPosition.top;

  get_edit()->SetWindowPos(NULL,0,0,edit_width,text_height+8,
                           SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);
  SetWindowPos(NULL,0,0,dialog_width,text_height+8+dialog_height-edit_height,
               SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);

  return TRUE;  // return TRUE unless you set the focus to a control
}

/******************************************************************************/
/*                      CPropertyHelpDlg::DoDataExchange                      */
/******************************************************************************/

void CPropertyHelpDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPropertyHelpDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPropertyHelpDlg, CDialog)
	//{{AFX_MSG_MAP(CPropertyHelpDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/* ========================================================================== */
/*                               CPropertiesDlg                               */
/* ========================================================================== */

class CPropertiesDlg : public CDialog
{
// Construction
public:
  CPropertiesDlg(kdu_codestream codestream, const char *text,
                 CWnd* pParent=NULL);

// Dialog Data
	//{{AFX_DATA(CPropertiesDlg)
	enum { IDD = IDD_PROPERTIESBOX };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPropertiesDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CPropertiesDlg)
	afx_msg void OnDblclkPropertiesList();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
// ----------------------------------------------------------------------------
protected:
  BOOL OnInitDialog();
private:
  CListBox *get_list()
    {
      return (CListBox *) GetDlgItem(IDC_PROPERTIES_LIST);
    }
private:
  kdu_codestream codestream;
  const char *text;
};

BEGIN_MESSAGE_MAP(CPropertiesDlg, CDialog)
	//{{AFX_MSG_MAP(CPropertiesDlg)
	ON_LBN_DBLCLK(IDC_PROPERTIES_LIST, OnDblclkPropertiesList)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/******************************************************************************/
/*                        CPropertiesDlg::CPropertiesDlg                      */
/******************************************************************************/

CPropertiesDlg::CPropertiesDlg(kdu_codestream codestream,
                               const char *text, CWnd* pParent)
	: CDialog(CPropertiesDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(CPropertiesDlg)
		// NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  this->codestream = codestream;
  this->text = text;
}

/******************************************************************************/
/*                        CPropertiesDlg::DoDataExchange                      */
/******************************************************************************/

void CPropertiesDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CPropertiesDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
  //}}AFX_DATA_MAP
}

/******************************************************************************/
/*                         CPropertiesDlg::OnInitDialog                       */
/******************************************************************************/

BOOL CPropertiesDlg::OnInitDialog()
{
  CDialog::OnInitDialog();
  const char *string = text;
  int max_width = 0;;
  char tbuf[256];
  while (*string != '\0')
    {
      char *tp = tbuf;
      SIZE text_size;
      for (; (*string != '\0') && (*string != '\n'); string++)
        if ((tp-tbuf) < 255)
          *(tp++) = *string;
      *tp = '\0';
      get_list()->AddString(tbuf);
      CDC *dc = get_list()->GetDC();
      GetTextExtentPoint32(dc->m_hDC,tbuf,(int)(tp-tbuf),&text_size);
      get_list()->ReleaseDC(dc);
      if (text_size.cx > max_width)
        max_width = text_size.cx;
      if (*string == '\n')
        string++; // Skip to next line.
    }
  get_list()->SetCurSel(-1);
  get_list()->SetHorizontalExtent(max_width+10);
  return TRUE;
}

/******************************************************************************/
/*                   CPropertiesDlg::OnDblclkPropertiesList                   */
/******************************************************************************/

void CPropertiesDlg::OnDblclkPropertiesList() 
{
  int selection = get_list()->GetCurSel();
  int length = get_list()->GetTextLen(selection);
  char *string = new char[length+1];
  get_list()->GetText(selection,string);
  const char *attribute_id;
  kdu_params *obj = codestream.access_siz()->find_string(string,attribute_id);
  delete[] string;
  if (obj != NULL)
    {
      kd_message_collector collector;
      kdu_message_formatter formatter(&collector,60);

      obj->describe_attribute(attribute_id,formatter,true);
      formatter.flush();
      POINT point;
      GetCursorPos(&point);
      CPropertyHelpDlg help(collector.get_text(),
                            kdu_coords(point.x,point.y),this);
      help.DoModal();
    }
}


/* ========================================================================== */
/*                                  kd_static                                 */
/* ========================================================================== */

/******************************************************************************/
/*                            kd_static::kd_static                            */
/******************************************************************************/

kd_static::kd_static()
{
  is_created = false;
  last_label = NULL;
  anchor_on_left = true;
}

/******************************************************************************/
/*                               kd_static::show                              */
/******************************************************************************/

void
  kd_static::show(const char *label, CWnd *view_wnd, kdu_coords pos,
                  kdu_dims view_dims)
{
  pos -= view_dims.pos;
  if (pos.x >= view_dims.size.x)
    pos.x = view_dims.size.x-1;
  if (pos.y >= view_dims.size.y)
    pos.y = view_dims.size.y;
  if (pos.x < 0)
    pos.x = 0;
  if (pos.y < 0)
    pos.y = 0;

  CRect rect;
  if (!is_created)
    {
      // Create with dummy dimensions
      rect.top = 0; rect.left = 0; rect.bottom = 16; rect.right = 64;
      Create(NULL,WS_CHILD | WS_BORDER,rect,view_wnd);
      is_created = true;
    }

  if (label != last_label)
    {
      last_label = label;
      SetWindowText(label);

      // Find dimensions of the displayed text
      CDC *dc = GetDC();
      SIZE text_size = dc->GetTextExtent(label,(int) strlen(label));
      ReleaseDC(dc);

      // Size window to fit text and display
      anchor_on_left = (pos.x <= (view_dims.size.x>>1));
      last_pos = pos;
      last_size.x = text_size.cx+4;
      last_size.y = text_size.cy+4;
      int anchor_x = (anchor_on_left)?pos.x:(pos.x-last_size.x+1);
      SetWindowPos(NULL,anchor_x,pos.y-last_size.y,last_size.x,last_size.y,
                   SWP_SHOWWINDOW);
      Invalidate();
    }
  else if (pos != last_pos)
    {
      last_pos = pos;
      if (anchor_on_left)
        {
          if ((pos.x+last_size.x) > view_dims.size.x)
            anchor_on_left = false;
        }
      else
        {
          if (pos.x < last_size.x)
            anchor_on_left = true;
        }
      int anchor_x = (anchor_on_left)?pos.x:(pos.x-last_size.x+1);
      SetWindowPos(NULL,anchor_x,pos.y-last_size.y,last_size.x,last_size.y,
                   SWP_SHOWWINDOW);
    }
}

/******************************************************************************/
/*                               kd_static::hide                              */
/******************************************************************************/

void
  kd_static::hide()
{
  if (!is_created)
    return;
  last_label = NULL;
  ShowWindow(SW_HIDE);
}


/* ========================================================================== */
/*                       CKdu_showApp Class Implementation                    */
/* ========================================================================== */

BEGIN_MESSAGE_MAP(CKdu_showApp, CWinApp)
	//{{AFX_MSG_MAP(CKdu_showApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
	ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE, OnUpdateFileClose)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_VIEW_HFLIP, OnViewHflip)
	ON_UPDATE_COMMAND_UI(ID_VIEW_HFLIP, OnUpdateViewHflip)
	ON_COMMAND(ID_VIEW_VFLIP, OnViewVflip)
	ON_UPDATE_COMMAND_UI(ID_VIEW_VFLIP, OnUpdateViewVflip)
	ON_COMMAND(ID_VIEW_ROTATE, OnViewRotate)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ROTATE, OnUpdateViewRotate)
	ON_COMMAND(ID_VIEW_COUNTER_ROTATE, OnViewCounterRotate)
	ON_UPDATE_COMMAND_UI(ID_VIEW_COUNTER_ROTATE, OnUpdateViewCounterRotate)
	ON_COMMAND(ID_VIEW_ZOOM_OUT, OnViewZoomOut)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_OUT, OnUpdateViewZoomOut)
	ON_COMMAND(ID_VIEW_ZOOM_IN, OnViewZoomIn)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_IN, OnUpdateViewZoomIn)
	ON_COMMAND(ID_VIEW_RESTORE, OnViewRestore)
	ON_UPDATE_COMMAND_UI(ID_VIEW_RESTORE, OnUpdateViewRestore)
	ON_COMMAND(ID_MODE_FAST, OnModeFast)
	ON_UPDATE_COMMAND_UI(ID_MODE_FAST, OnUpdateModeFast)
	ON_COMMAND(ID_MODE_FUSSY, OnModeFussy)
	ON_UPDATE_COMMAND_UI(ID_MODE_FUSSY, OnUpdateModeFussy)
	ON_COMMAND(ID_MODE_RESILIENT, OnModeResilient)
	ON_UPDATE_COMMAND_UI(ID_MODE_RESILIENT, OnUpdateModeResilient)
	ON_COMMAND(ID_VIEW_WIDEN, OnViewWiden)
	ON_UPDATE_COMMAND_UI(ID_VIEW_WIDEN, OnUpdateViewWiden)
	ON_COMMAND(ID_VIEW_SHRINK, OnViewShrink)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHRINK, OnUpdateViewShrink)
	ON_COMMAND(ID_FILE_PROPERTIES, OnFileProperties)
	ON_UPDATE_COMMAND_UI(ID_FILE_PROPERTIES, OnUpdateFileProperties)
	ON_COMMAND(ID_COMPONENT1, OnComponent1)
	ON_UPDATE_COMMAND_UI(ID_COMPONENT1, OnUpdateComponent1)
	ON_COMMAND(ID_MULTI_COMPONENT, OnMultiComponent)
	ON_UPDATE_COMMAND_UI(ID_MULTI_COMPONENT, OnUpdateMultiComponent)
	ON_COMMAND(ID_LAYERS_LESS, OnLayersLess)
	ON_UPDATE_COMMAND_UI(ID_LAYERS_LESS, OnUpdateLayersLess)
	ON_COMMAND(ID_LAYERS_MORE, OnLayersMore)
	ON_UPDATE_COMMAND_UI(ID_LAYERS_MORE, OnUpdateLayersMore)
	ON_COMMAND(ID_MODE_RESILIENT_SOP, OnModeResilientSop)
	ON_UPDATE_COMMAND_UI(ID_MODE_RESILIENT_SOP, OnUpdateModeResilientSop)
	ON_COMMAND(ID_MODE_SEEKABLE, OnModeSeekable)
	ON_UPDATE_COMMAND_UI(ID_MODE_SEEKABLE, OnUpdateModeSeekable)
	ON_COMMAND(ID_STATUS_TOGGLE, OnStatusToggle)
	ON_UPDATE_COMMAND_UI(ID_STATUS_TOGGLE, OnUpdateStatusToggle)
	ON_COMMAND(ID_VIEW_REFRESH, OnViewRefresh)
	ON_UPDATE_COMMAND_UI(ID_VIEW_REFRESH, OnUpdateViewRefresh)
	ON_COMMAND(ID_FILE_DISCONNECT, OnFileDisconnect)
	ON_UPDATE_COMMAND_UI(ID_FILE_DISCONNECT, OnUpdateFileDisconnect)
	ON_COMMAND(ID_COMPONENT_NEXT, OnComponentNext)
	ON_UPDATE_COMMAND_UI(ID_COMPONENT_NEXT, OnUpdateComponentNext)
	ON_COMMAND(ID_COMPONENT_PREV, OnComponentPrev)
	ON_UPDATE_COMMAND_UI(ID_COMPONENT_PREV, OnUpdateComponentPrev)
	ON_COMMAND(ID_JPIP_OPEN, OnJpipOpen)
	ON_COMMAND(ID_FOCUS_OFF, OnFocusOff)
	ON_UPDATE_COMMAND_UI(ID_FOCUS_OFF, OnUpdateFocusOff)
	ON_COMMAND(ID_FOCUS_HIGHLIGHTING, OnFocusHighlighting)
	ON_UPDATE_COMMAND_UI(ID_FOCUS_HIGHLIGHTING, OnUpdateFocusHighlighting)
	ON_COMMAND(ID_FOCUS_WIDEN, OnFocusWiden)
	ON_COMMAND(ID_FOCUS_SHRINK, OnFocusShrink)
	ON_COMMAND(ID_FOCUS_LEFT, OnFocusLeft)
	ON_COMMAND(ID_FOCUS_RIGHT, OnFocusRight)
	ON_COMMAND(ID_FOCUS_UP, OnFocusUp)
	ON_COMMAND(ID_FOCUS_DOWN, OnFocusDown)
	ON_COMMAND(ID_VIEW_METADATA, OnViewMetadata)
	ON_UPDATE_COMMAND_UI(ID_VIEW_METADATA, OnUpdateViewMetadata)
	ON_COMMAND(ID_IMAGE_NEXT, OnImageNext)
	ON_UPDATE_COMMAND_UI(ID_IMAGE_NEXT, OnUpdateImageNext)
	ON_COMMAND(ID_IMAGE_PREV, OnImagePrev)
	ON_UPDATE_COMMAND_UI(ID_IMAGE_PREV, OnUpdateImagePrev)
	ON_COMMAND(ID_COMPOSITING_LAYER, OnCompositingLayer)
	ON_UPDATE_COMMAND_UI(ID_COMPOSITING_LAYER, OnUpdateCompositingLayer)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_AS, OnUpdateFileSaveAs)
	ON_COMMAND(ID_META_ADD, OnMetaAdd)
	ON_UPDATE_COMMAND_UI(ID_META_ADD, OnUpdateMetaAdd)
	ON_COMMAND(ID_OVERLAY_ENABLED, OnOverlayEnabled)
	ON_UPDATE_COMMAND_UI(ID_OVERLAY_ENABLED, OnUpdateOverlayEnabled)
	ON_COMMAND(ID_OVERLAY_DARKER, OnOverlayDarker)
	ON_UPDATE_COMMAND_UI(ID_OVERLAY_DARKER, OnUpdateOverlayDarker)
	ON_COMMAND(ID_OVERLAY_BRIGHTER, OnOverlayBrighter)
	ON_UPDATE_COMMAND_UI(ID_OVERLAY_BRIGHTER, OnUpdateOverlayBrighter)
	ON_COMMAND(ID_OVERLAY_FLASHING, OnOverlayFlashing)
	ON_UPDATE_COMMAND_UI(ID_OVERLAY_FLASHING, OnUpdateOverlayFlashing)
	ON_COMMAND(ID_OVERLAY_TOGGLE, OnOverlayToggle)
	ON_UPDATE_COMMAND_UI(ID_OVERLAY_TOGGLE, OnUpdateOverlayToggle)
	ON_COMMAND(ID_OVERLAY_DOUBLESIZE, OnOverlayDoublesize)
	ON_UPDATE_COMMAND_UI(ID_OVERLAY_DOUBLESIZE, OnUpdateOverlayDoublesize)
	ON_COMMAND(ID_OVERLAY_HALVESIZE, OnOverlayHalvesize)
	ON_UPDATE_COMMAND_UI(ID_OVERLAY_HALVESIZE, OnUpdateOverlayHalvesize)
	//}}AFX_MSG_MAP
  ON_COMMAND(ID_VIEW_OPTIMIZESCALE, OnViewOptimizescale)
  ON_UPDATE_COMMAND_UI(ID_VIEW_OPTIMIZESCALE, OnUpdateViewOptimizescale)
  ON_COMMAND(ID_TRACK_NEXT, OnTrackNext)
  ON_UPDATE_COMMAND_UI(ID_TRACK_NEXT, OnUpdateTrackNext)
  ON_COMMAND(ID_VIEW_PLAYCONTROL, OnViewPlaycontrol)
  ON_UPDATE_COMMAND_UI(ID_VIEW_PLAYCONTROL, OnUpdateViewPlaycontrol)
  ON_COMMAND(ID_VIEW_SCALE_X2, OnViewScaleX2)
  ON_COMMAND(ID_VIEW_SCALE_X1, OnViewScaleX1)
  ON_COMMAND(ID_MODES_SINGLETHREADED, OnModesSinglethreaded)
  ON_UPDATE_COMMAND_UI(ID_MODES_SINGLETHREADED, OnUpdateModesSinglethreaded)
  ON_COMMAND(ID_MODES_MULTITHREADED, OnModesMultithreaded)
  ON_UPDATE_COMMAND_UI(ID_MODES_MULTITHREADED, OnUpdateModesMultithreaded)
  ON_COMMAND(ID_MODES_MORETHREADS, OnModesMorethreads)
  ON_UPDATE_COMMAND_UI(ID_MODES_MORETHREADS, OnUpdateModesMorethreads)
  ON_COMMAND(ID_MODES_LESSTHREADS, OnModesLessthreads)
  ON_UPDATE_COMMAND_UI(ID_MODES_LESSTHREADS, OnUpdateModesLessthreads)
  ON_COMMAND(ID_MODES_RECOMMENDED_THREADS, OnModesRecommendedThreads)
  ON_UPDATE_COMMAND_UI(ID_MODES_RECOMMENDED_THREADS, OnUpdateModesRecommendedThreads)
  ON_COMMAND(ID_FILE_SAVEASLINKED, OnFileSaveAsLinked)
  ON_UPDATE_COMMAND_UI(ID_FILE_SAVEASLINKED, OnUpdateFileSaveAsLinked)
  ON_COMMAND(ID_FILE_SAVEASEMBEDDED, OnFileSaveAsEmbedded)
  ON_UPDATE_COMMAND_UI(ID_FILE_SAVEASEMBEDDED, OnUpdateFileSaveAsEmbedded)
  ON_COMMAND(ID_FILE_SAVEANDRELOAD, OnFileSaveReload)
  ON_UPDATE_COMMAND_UI(ID_FILE_SAVEANDRELOAD, OnUpdateFileSaveReload)
  END_MESSAGE_MAP()


/******************************************************************************/
/*                         CKdu_showApp::CKdu_showApp                         */
/******************************************************************************/

CKdu_showApp::CKdu_showApp()
{
  child_wnd = NULL;

  open_file_pathname = NULL;
  open_file_urlname = NULL;
  open_filename = NULL;
  last_save_pathname = NULL;
  oversave_pathname = NULL;
  save_without_asking = false;

  compositor = NULL;
  num_recommended_threads = kdu_get_num_processors();
  if (num_recommended_threads < 2)
    num_recommended_threads = 0;
  num_threads = num_recommended_threads;
  if (num_threads > 0)
    {
      thread_env.create();
      for (int k=1; k < num_threads; k++)
        if (!thread_env.add_thread())
          num_threads = k;
    }
  in_idle = false;
  processing_suspended = false;

  file_server = NULL;
  have_unsaved_edits = false;

  allow_seeking = true;
  error_level = 0;
  max_display_layers = 1<<16;
  transpose = vflip = hflip = false;
  min_rendering_scale = -1.0F;
  rendering_scale = 1.0F;
  max_components = max_codestreams = max_compositing_layer_idx = -1;
  single_component_idx = single_codestream_idx = single_layer_idx = 0;
  frame_idx = 0;
  frame_start = frame_end = 0.0;
  frame = NULL;
  frame_iteration = 0;
  frame_expander.reset();
  fit_scale_to_window = false;
  configuration_complete = false;

  jpip_progress = NULL;
  status_id = KDS_STATUS_LAYER_RES;

  in_playmode = false;
  playmode_repeat = false;
  pushed_last_frame = false;
  use_native_timing = true;
  custom_fps = rate_multiplier = 1.0F;
  max_queue_size = 2;
  max_queue_period = 1.0; // Process up to 1 second ahead
  playclock_base = 0;
  playstart_instant = 0.0;
  frame_time_offset = 0.0;
  last_actual_time = last_nominal_time = -1.0;

  pixel_scale = 1;
  image_dims.pos = image_dims.size = kdu_coords(0,0);
  buffer_dims = view_dims = image_dims;
  view_centre_x = view_centre_y = 0.0F;
  view_centre_known = false;

  enable_focus_box = false;
  highlight_focus = true;
  focus_anchors_known = false;
  focus_centre_x = focus_centre_y = focus_size_x = focus_size_y = 0.0F;
  focus_pen.CreatePen(PS_DOT,0,0x000000FF);
  focus_box.pos = focus_box.size = kdu_coords(0,0);
  enable_region_posting = false;

  overlays_enabled = overlay_flashing_enabled = false;
  overlay_painting_intensity = overlay_painting_colour = 0;
  overlay_size_threshold = 1;
  metainfo_label = NULL;

  buffer = NULL;
  compatible_dc.CreateCompatibleDC(NULL);
  strip_extent = kdu_coords(0,0);
  stripmap = NULL;
  stripmap_buffer = NULL;
  for (int i=0; i < 256; i++)
    {
      foreground_lut[i] = (kdu_byte)(40 + ((i*216) >> 8));
      background_lut[i] = (kdu_byte)((i*216) >> 8);
    }

  refresh_timer_id = 0;
  flash_timer_id = 0;
  last_transferred_bytes = 0;

  metashow = NULL;
  playcontrol = NULL;
}

/*****************************************************************************/
/*             CKdu_showApp::perform_essential_cleanup_steps                 */
/*****************************************************************************/

void CKdu_showApp::perform_essential_cleanup_steps()
{
  if (file_server != NULL)
    delete file_server;
  file_server = NULL;
  if ((oversave_pathname != NULL) && (open_file_pathname != NULL))
    {
      file_in.close(); // Unlocks any files
      jp2_family_in.close(); // Unlocks any files
      MoveFileEx(oversave_pathname,open_file_pathname,
                 MOVEFILE_REPLACE_EXISTING);
      delete[] oversave_pathname;
      oversave_pathname = NULL;
    }
}

/******************************************************************************/
/*                        CKdu_showApp::~CKdu_showApp                         */
/******************************************************************************/

CKdu_showApp::~CKdu_showApp()
{
  perform_essential_cleanup_steps();
  if (jpip_progress != NULL)
    { delete jpip_progress; jpip_progress = NULL; }
  if (compositor != NULL)
    { delete compositor; compositor = NULL; }
  if (thread_env.exists())
    thread_env.destroy();
  if (metashow != NULL)
    delete metashow;
  if (playcontrol != NULL)
    delete playcontrol;
  file_in.close();
  jpx_in.close();
  mj2_in.close();
  jp2_family_in.close();
  jpip_client.close();
  jpip_client.install_context_translator(NULL);
  if (stripmap != NULL)
    { DeleteObject(stripmap); stripmap = NULL; stripmap_buffer = NULL; }
  if (oversave_pathname != NULL)
    delete[] oversave_pathname;
  if (open_file_pathname != NULL)
    delete[] open_file_pathname;
  if (open_file_urlname != NULL)
    delete[] open_file_urlname;
  if (last_save_pathname != NULL)
    delete[] last_save_pathname;
}

/******************************************************************************/
/*                         CKdu_showApp::InitInstance                         */
/******************************************************************************/

BOOL CKdu_showApp::InitInstance()
{
  AfxEnableControlContainer();
#if _MFC_VER < 0x0500
#  ifdef _AFXDLL
     Enable3dControls(); // Call this when using MFC in a shared DLL
#  else
     Enable3dControlsStatic(); // Call this when linking to MFC statically
#  endif
#endif // _MFC_VER
  SetRegistryKey(_T("Kakadu"));
  LoadStdProfileSettings(4);
  settings.load_from_registry(this);
  char executable_path[_MAX_PATH+1];
  int path_len = GetModuleFileName(m_hInstance,executable_path,_MAX_PATH);
  if (path_len > 0)
    {
      executable_path[path_len] = '\0';
      register_protocols(executable_path);
    }
  frame_wnd = new CMainFrame;
  m_pMainWnd = frame_wnd;
  child_wnd = frame_wnd->get_child();
  child_wnd->set_app(this);
  frame_wnd->LoadFrame(IDR_MAINFRAME,
                   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                   WS_THICKFRAME | WS_MINIMIZEBOX,
                   NULL,NULL);
  frame_wnd->ShowWindow(SW_SHOW);
  frame_wnd->UpdateWindow();
  frame_wnd->set_app(this);
  frame_wnd->DragAcceptFiles(TRUE);
  client_notifier.set_wnd(frame_wnd->m_hWnd);

  kdu_customize_errors(&err_formatter);
  kdu_customize_warnings(&warn_formatter);

  char *filename = m_lpCmdLine;
  while ((*filename != '\0') && ((*filename == ' ') || (*filename == '\t')))
    filename++;
  if (*filename != '\0')
    {
      if (*filename == '\"')
        {
          char *ch = strrchr(filename+1,'\"');
          if (ch != NULL)
            {
              filename++;
              *ch = '\0';
            }
        }
      open_file(filename);
    }
  return TRUE;
}

/******************************************************************************/
/*                        CKdu_showApp::SaveAllModified                       */
/******************************************************************************/

BOOL CKdu_showApp::SaveAllModified()
{
  if (have_unsaved_edits && (frame_wnd != NULL) &&
      (frame_wnd->MessageBox("You have unsaved edits that will be closed "
                             "when this application terminates.  "
                             "Are you sure you want to quit the application?",
                             "Kdu_show: Unsaved Edits",
                              MB_YESNO | MB_ICONWARNING) == IDNO))
    return FALSE;
  return TRUE;
}

/******************************************************************************/
/*                          CKdu_showApp::close_file                          */
/******************************************************************************/

void
  CKdu_showApp::close_file()
{
  perform_essential_cleanup_steps();

  stop_playmode();
  if (compositor != NULL)
    { delete compositor; compositor = NULL; }
  if (metashow != NULL)
    metashow->deactivate();
  file_in.close();
  jpx_in.close();
  mj2_in.close();
  jp2_family_in.close();
  jpip_client.close();
  jpip_client.install_context_translator(NULL);

  open_filename = NULL;
  if (oversave_pathname != NULL)
    delete[] oversave_pathname;
  if (open_file_pathname != NULL)
    delete[] open_file_pathname;
  if (open_file_urlname != NULL)
    delete[] open_file_urlname;
  if (last_save_pathname != NULL)
    delete[] last_save_pathname;
  oversave_pathname = NULL;
  open_file_pathname = open_file_urlname = last_save_pathname = NULL;
  save_without_asking = false;
  
  if (file_server != NULL)
    { delete file_server; file_server = NULL; }
  have_unsaved_edits = false;

  composition_rules = jpx_composition(NULL);
  processing_suspended = false;
  fit_scale_to_window = false;
  frame_idx = 0;
  frame_start = frame_end = 0.0;
  frame = NULL;
  frame_iteration = 0;
  frame_expander.reset();
  configuration_complete = false;
  transpose = vflip = hflip = false;
  image_dims.pos = image_dims.size = kdu_coords(0,0);
  buffer_dims = view_dims = image_dims;
  view_centre_known = false;
  focus_anchors_known = false;
  enable_focus_box = false;
  overlays_enabled = overlay_flashing_enabled = false;
  overlay_painting_intensity = overlay_painting_colour = 0;
  buffer = NULL;
  if (refresh_timer_id != 0)
    m_pMainWnd->KillTimer(refresh_timer_id);
  if (flash_timer_id != 0)
    m_pMainWnd->KillTimer(flash_timer_id);
  refresh_timer_id = 0;
  flash_timer_id = 0;
  last_transferred_bytes = 0;
  if (child_wnd != NULL)
    {
      child_wnd->set_max_view_size(kdu_coords(20000,20000),pixel_scale);
      if (child_wnd->GetSafeHwnd() != NULL)
        child_wnd->Invalidate();
    }
  m_pMainWnd->SetWindowText("<no data>");
  if (playcontrol != NULL)
    {
      playcontrol->OnClose();
      assert(playcontrol == NULL);
    }
}

/******************************************************************************/
/*                          CKdu_showApp::open_file                           */
/******************************************************************************/

void
  CKdu_showApp::open_file(char *filename)
{
  if (filename != NULL)
    {
      if (have_unsaved_edits &&
          (frame_wnd->MessageBox("You have edited the existing file but not "
                                 "saved these edits ... perhaps you saved the "
                                 "file in a format which cannot hold all the "
                                 "metadata (use JPX).  Do you still want to "
                                 "close the existing file to open a new one?",
                                 "Kdu_show: Unsaved Edits",
                                 MB_YESNO | MB_ICONWARNING) == IDNO))
        return; // Do nothing
      close_file();
    }
  assert(compositor == NULL);
  enable_focus_box = false;
  focus_box.size = kdu_coords(0,0);
  client_roi.init();
  processing_suspended = false;
  enable_region_posting = false;
  bool is_jpip = false;
  try {
      if (filename != NULL)
        {
          if ((((toupper(filename[0]) == (int) 'J') &&
                (toupper(filename[1]) == (int) 'P') &&
                (toupper(filename[2]) == (int) 'I') &&
                (toupper(filename[3]) == (int) 'P')) ||
               ((toupper(filename[0]) == (int) 'H') &&
                (toupper(filename[1]) == (int) 'T') &&
                (toupper(filename[2]) == (int) 'T') &&
                (toupper(filename[3]) == (int) 'P'))) &&
              (filename[4] == ':'))
            { // Open as an interactive client-server application
              is_jpip = true;
              char *prefix = filename + 5;
              while ((*prefix == '/') || (*prefix == '\\'))
                prefix++;
              char *fstart;
              for (fstart=prefix; *fstart != '\0'; fstart++)
                if ((*fstart == '/') || (*fstart == '\\'))
                  break;
              if (*fstart == '\0')
                { kdu_error e;
                  e << "Illegal JPIP request, \"" << filename
                    << "\".  General form is \"<prot>://<hostname>[:<port>]/"
                       "<resource>[?<query>]\"\nwhere <prot> is \"jpip\" or "
                       "\"http\" and forward slashes may be substituted "
                       "for back slashes if desired.";
                }
              *fstart = '\0';
              jpip_client.connect(prefix,settings.get_jpip_proxy(),
                                  fstart+1,settings.get_jpip_channel_type(),
                                  settings.get_jpip_cache());
              *fstart = '/';
              status_id = KDS_STATUS_CACHE;
              jpip_client.install_notifier(&client_notifier);
              cumulative_transmission_time = 0;
              last_transmission_start = 0;
              jp2_family_in.open(&jpip_client);
              if (metashow != NULL)
                metashow->activate(&jp2_family_in);
            }
          else
            { // Open as a local file
              status_id = KDS_STATUS_LAYER_RES;
              jp2_family_in.open(filename,allow_seeking);
              compositor = new kd_bitmap_compositor(&thread_env);
              if (jpx_in.open(&jp2_family_in,true) >= 0)
                { // We have a JP2/JPX-compatible file.
                  compositor->create(&jpx_in);
                  if (metashow != NULL)
                    metashow->activate(&jp2_family_in);
                }
              else if (mj2_in.open(&jp2_family_in,true) >= 0)
                {
                  compositor->create(&mj2_in);
                  if (metashow != NULL)
                    metashow->activate(&jp2_family_in);
                }
              else
                { // Incompatible with JP2/JPX or MJ2. Try opening as raw stream
                  jp2_family_in.close();
                  file_in.open(filename,allow_seeking);
                  compositor->create(&file_in);
                }
              compositor->set_error_level(error_level);
            }
        }
      else if (jpip_client.is_active())
        { // See if we can open the client yet
          assert((compositor == NULL) && jp2_family_in.exists());
          bool bin0_complete = false;
          if (jpip_client.get_databin_length(KDU_META_DATABIN,0,0,
                                             &bin0_complete) > 0)
            { // Open as a JP2/JPX family file
              if (metashow != NULL)
                metashow->update_tree();
              if (jpx_in.open(&jp2_family_in,false))
                {
                  compositor = new kd_bitmap_compositor(&thread_env);
                  compositor->create(&jpx_in);
                }
              jpip_client.install_context_translator(&jpx_client_translator);
            }
          else if (bin0_complete)
            { // Open as a raw file
              if (metashow != NULL)
                metashow->update_tree();
              assert(!jpx_in.exists());
              bool hdr_complete;
              jpip_client.get_databin_length(KDU_MAIN_HEADER_DATABIN,0,0,
                                             &hdr_complete);
              if (hdr_complete)
                {
                  jpip_client.set_read_scope(KDU_MAIN_HEADER_DATABIN,0,0);
                  compositor = new kd_bitmap_compositor(&thread_env);
                  compositor->create(&jpip_client);
                }
            }
          if (compositor == NULL)
            {
              if (!jpip_client.is_alive())
                { kdu_error e; e << "Unable to recover sufficient information "
                  "from remote server (or a local cache) to open the image."; }
              return; // Come back later
            }
          compositor->set_error_level(error_level);
          client_roi.init();
          enable_region_posting = true;
        }
    }
  catch (int)
    {
      close_file();
    }

  if (compositor != NULL)
    {
      max_display_layers = 1<<16;
      transpose = vflip = hflip = false;
      min_rendering_scale = -1.0F;
      rendering_scale = 1.0F;
      single_component_idx = -1;
      single_codestream_idx = 0;
      max_components = -1;
      frame_idx = 0;
      frame_start = frame_end = 0.0;
      num_frames = -1;
      frame = NULL;
      composition_rules = jpx_composition(NULL);

      if (jpx_in.exists())
        {
          max_codestreams = -1;  // Unknown as yet
          max_compositing_layer_idx = -1; // Unknown as yet
          single_layer_idx = -1; // Try starting in composite frame mode
        }
      else if (mj2_in.exists())
        {
          single_layer_idx = 0; // Start in composed single layer (track) mode
        }
      else
        {
          max_codestreams = 1;
          max_compositing_layer_idx = 0;
          num_frames = 0;
          single_layer_idx = 0; // Start in composed single layer mode
        }

      overlays_enabled = true; // Default starting position
      overlay_flashing_enabled = false;
      overlay_painting_intensity = overlay_painting_colour = 0;
      compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
           (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));

      invalidate_configuration();
      fit_scale_to_window = true;
      image_dims = buffer_dims = view_dims = kdu_dims();
      buffer = NULL;
      initialize_regions();
    }
  if (filename != NULL)
    {
      assert((open_file_pathname == NULL) && (open_file_urlname == NULL));
      if (is_jpip)
        {
          open_file_urlname = new char[strlen(filename)+1];
          strcpy(open_file_urlname,filename);
          open_filename = open_file_urlname;
        }
      else
        {
          open_file_pathname = new char[MAX_PATH+1]; // Plenty
          int path_len =
            GetFullPathName(filename,MAX_PATH,
                            open_file_pathname,&open_filename);
          if (path_len >= MAX_PATH)
            path_len--;
          open_file_pathname[path_len] = '\0';
          if (open_filename > open_file_pathname)
            {
              char sep = open_filename[-1];
              open_filename[-1] = '\0';
              settings.set_open_save_dir(open_file_pathname);
              open_filename[-1] = sep;
            }
        }
      char title[256];
      strcpy(title,"kdu_show: ");
      strncat(title,open_filename,255-strlen(title));
      m_pMainWnd->SetWindowText(title);
    }
}

/******************************************************************************/
/*                        CKdu_showApp::increase_scale                        */
/******************************************************************************/

float
  CKdu_showApp::increase_scale(float from_scale)
{
  float min_scale = from_scale*1.30F;
  float max_scale = from_scale*2.70F;
  if (compositor == NULL)
    return min_scale;
  kdu_dims region_basis;
  if (configuration_complete)
    region_basis = (enable_focus_box)?focus_box:view_dims;
  return compositor->find_optimal_scale(region_basis,from_scale,
                                        min_scale,max_scale);
}

/******************************************************************************/
/*                        CKdu_showApp::decrease_scale                        */
/******************************************************************************/

float
  CKdu_showApp::decrease_scale(float from_scale)
{
  float max_scale = from_scale/1.30F;
  float min_scale = from_scale/2.70F;
  if (min_rendering_scale > 0.0F)
    {
      if (min_scale < min_rendering_scale)
        min_scale = min_rendering_scale;
      if (max_scale < min_rendering_scale)
        max_scale = min_rendering_scale;
    }
  if (compositor == NULL)
    return max_scale;
  kdu_dims region_basis;
  if (configuration_complete)
    region_basis = (enable_focus_box)?focus_box:view_dims;
  float new_scale = compositor->find_optimal_scale(region_basis,from_scale,
                                                   min_scale,max_scale);
  if (new_scale > max_scale)
    min_rendering_scale = new_scale; // This is as small as we can go
  return new_scale;
}

/******************************************************************************/
/*                       CKdu_showApp::change_frame_idx                       */
/******************************************************************************/

void
  CKdu_showApp::change_frame_idx(int new_frame_idx)
  /* Note carefully that, on entry to this function, only the `frame_start'
     time can be relied upon.  The function sets `frame_end' from scratch,
     rather than basing the newly calculated value on a previous one. */
{
  if (new_frame_idx < 0)
    new_frame_idx = 0;
  if ((num_frames >= 0) && (new_frame_idx >= (num_frames-1)))
    new_frame_idx = num_frames-1;
  if ((new_frame_idx == frame_idx) && (frame_end > frame_start))
    return; // Nothing to do

  if (composition_rules.exists() && (frame != NULL))
    {
      int num_instructions, duration, repeat_count, delta;
      bool is_persistent;

      if (new_frame_idx == 0)
        {
          frame = composition_rules.get_next_frame(NULL);
          frame_iteration = 0;
          frame_idx = 0;
          frame_start = 0.0;
          composition_rules.get_frame_info(frame,num_instructions,duration,
                                           repeat_count,is_persistent);
          frame_end = frame_start + 0.001*duration;
        }

      while (frame_idx < new_frame_idx)
        {
          composition_rules.get_frame_info(frame,num_instructions,duration,
                                           repeat_count,is_persistent);
          delta = repeat_count - frame_iteration;
          if (delta > 0)
            {
              if ((frame_idx+delta) > new_frame_idx)
                delta = new_frame_idx - frame_idx;
              frame_iteration += delta;
              frame_idx += delta;
              frame_start += delta * 0.001*duration;
              frame_end = frame_start + 0.001*duration;
            }
          else
            {
              jx_frame *new_frame = composition_rules.get_next_frame(frame);
              frame_end = frame_start + 0.001*duration; // Just in case
              if (new_frame == NULL)
                {
                  num_frames = frame_idx+1;
                  break;
                }
              else
                { frame = new_frame; frame_iteration=0; }
              composition_rules.get_frame_info(frame,num_instructions,duration,
                                               repeat_count,is_persistent);
              frame_start = frame_end;
              frame_end += 0.001*duration;
              frame_idx++;
            }
        }

      while (frame_idx > new_frame_idx)
        {
          composition_rules.get_frame_info(frame,num_instructions,duration,
                                           repeat_count,is_persistent);
          if (frame_iteration > 0)
            {
              delta = frame_idx - new_frame_idx;
              if (delta > frame_iteration)
                delta = frame_iteration;
              frame_iteration -= delta;
              frame_idx -= delta;
              frame_start -= delta * 0.001*duration;
              frame_end = frame_start + 0.001*duration;
            }
          else
            {
              frame = composition_rules.get_prev_frame(frame);
              assert(frame != NULL);
              composition_rules.get_frame_info(frame,num_instructions,duration,
                                               repeat_count,is_persistent);
              frame_iteration = repeat_count;
              frame_idx--;
              frame_start -= 0.001*duration;
              frame_end = frame_start + 0.001*duration;
            }
        }
    }
  else if (mj2_in.exists() && (single_layer_idx >= 0))
    {
      mj2_video_source *track =
        mj2_in.access_video_track((kdu_uint32)(single_layer_idx+1));
      if (track == NULL)
        return;
      frame_idx = new_frame_idx;
      track->seek_to_frame(new_frame_idx);
      frame_start = ((double) track->get_frame_instant()) /
        ((double) track->get_timescale());
      frame_end = frame_start + ((double) track->get_frame_period()) /
        ((double) track->get_timescale());
    }
}

/******************************************************************************/
/*                   CKdu_showApp::invalidate_configuration                   */
/******************************************************************************/

void
  CKdu_showApp::invalidate_configuration()
{
  configuration_complete = false;
  max_components = -1; // Changed config may have different num image components
  buffer = NULL;

  if (compositor != NULL)
    compositor->remove_compositing_layer(-1,false);
}

/******************************************************************************/
/*                      CKdu_showApp::initialize_regions                      */
/******************************************************************************/

void
  CKdu_showApp::initialize_regions()
{
  if (child_wnd != NULL)
    child_wnd->cancel_focus_drag();

  bool first_time_through = fit_scale_to_window;
        // Use this flag later to determine if we need to fire off the play
        // controls.

  // Reset the buffer and view dimensions to zero size.
  buffer = NULL;
  frame_expander.reset();
  while (!configuration_complete)
    { // Install configuration
      try {
          if (single_component_idx >= 0)
            { // Check for valid codestream index before opening
              if (jpx_in.exists())
                {
                  int count=max_codestreams;
                  if ((count < 0) && !jpx_in.count_codestreams(count))
                    { // Actual number not yet known, but have at least `count'
                      if (single_codestream_idx >= count)
                        return; // Come back later once more data is in cache
                    }
                  else
                    { // Number of codestreams is now known
                      max_codestreams = count;
                      if (single_codestream_idx >= max_codestreams)
                        single_codestream_idx = max_codestreams-1;
                    }
                }
              else if (mj2_in.exists())
                {
                  kdu_uint32 trk;
                  int frm, fld;
                  if (!mj2_in.find_stream(single_codestream_idx,trk,frm,fld))
                    return; // Come back later once more data is in cache
                  if (trk == 0)
                    {
                      int count;
                      if (mj2_in.count_codestreams(count))
                        max_codestreams = count;
                      single_codestream_idx = count-1;
                    }
                }
              else
                { // Raw codestream
                  single_codestream_idx = 0;
                }
              int idx = compositor->set_single_component(single_codestream_idx,
                                        single_component_idx,
                                        KDU_WANT_CODESTREAM_COMPONENTS);
              if (idx < 0)
                { // Cannot open codestream yet; waiting for cache
                  update_client_window_of_interest();
                  return;
                }
              kdrc_stream *str =
                compositor->get_next_codestream(NULL,true,false);
              kdu_codestream codestream = compositor->access_codestream(str);
              codestream.apply_input_restrictions(0,0,0,0,NULL);
              max_components = codestream.get_num_components();
              if (single_component_idx >= max_components)
                single_component_idx = max_components-1;
            }
          else if (single_layer_idx >= 0)
            { // Check for valid compositing layer index before opening
              if (jpx_in.exists())
                {
                  frame_idx = 0;
                  frame_start = frame_end = 0.0;
                  int count=max_compositing_layer_idx+1;
                  if ((count <= 0) && !jpx_in.count_compositing_layers(count))
                    { // Actual number not yet known, but have at least `count'
                      if (single_layer_idx >= count)
                        return; // Come back later once more data is in cache
                    }
                  else
                    { // Number of compositing layers is now known
                      max_compositing_layer_idx = count-1;
                      if (single_layer_idx > max_compositing_layer_idx)
                        single_layer_idx = max_compositing_layer_idx;
                    }
                }
              else if (mj2_in.exists())
                {
                  int track_type;
                  kdu_uint32 track_idx = (kdu_uint32)(single_layer_idx+1);
                  bool loop_check = false;
                  mj2_video_source *track = NULL;
                  while (track == NULL)
                    {
                      track_type = mj2_in.get_track_type(track_idx);
                      if (track_type == MJ2_TRACK_IS_VIDEO)
                        {
                          track = mj2_in.access_video_track(track_idx);
                          if (track == NULL)
                            return; // Come back later once more data in cache
                          num_frames = track->get_num_frames();
                          if (num_frames == 0)
                            { // Skip over track with no frames
                              track_idx = mj2_in.get_next_track(track_idx);
                              continue;
                            }
                        }
                      else if (track_type == MJ2_TRACK_NON_EXISTENT)
                        { // Go back to the first track
                          if (loop_check)
                            { kdu_error e; e << "Motion JPEG2000 source "
                              "has no video tracks with any frames!"; }
                          loop_check = true;
                          track_idx = mj2_in.get_next_track(0);
                        }
                      else if (track_type == MJ2_TRACK_MAY_EXIST)
                        return; // Come back later once more data is in cache
                      else
                        { // Go to the next track
                          track_idx = mj2_in.get_next_track(track_idx);
                        }
                      single_layer_idx = ((int) track_idx) - 1;
                    }
                  if (frame_idx >= num_frames)
                    frame_idx = num_frames-1;
                  change_frame_idx(frame_idx);
                }
              else
                { // Raw codestream
                  single_layer_idx = 0;
                }
              if (!compositor->add_compositing_layer(single_layer_idx,
                                                     kdu_dims(),kdu_dims(),
                                                     false,false,false,
                                                     frame_idx,0))
                { // Cannot open compositing layer yet; waiting for cache
                  if (jpip_client.is_one_time_request())
                    { // See if request is compatible with opening a layer
                      if (!check_one_time_request_compatibility())         
                        continue; // Mode changed to single component
                    }
                  else
                    update_client_window_of_interest();
                  return;
                }
            }
          else
            { // Attempt to open frame
              if (num_frames == 0)
                { // Downgrade to single layer mode
                  single_layer_idx = 0;
                  frame_idx = 0;
                  frame_start = frame_end = 0.0;
                  frame = NULL;
                  continue;
                }
              assert(jpx_in.exists());
              if (frame == NULL)
                {
                  if (!composition_rules)
                    composition_rules = jpx_in.access_composition();
                  if (!composition_rules)
                    return; // Cannot open composition rules yet; wait for cache
                  assert((frame_idx == 0) && (frame_iteration == 0));
                  frame = composition_rules.get_next_frame(NULL);
                  if (frame == NULL)
                    { // Downgrade to single layer mode
                      single_layer_idx = 0;
                      frame_idx = 0;
                      frame_start = frame_end = 0.0;
                      num_frames = 0;
                      continue;
                    }
                }

              if (!frame_expander.construct(&jpx_in,frame,frame_iteration,true))
                {
                  if (jpip_client.is_one_time_request())
                    { // See if request is compatible with opening a frame
                      if (!check_one_time_request_compatibility())         
                        continue; // Mode changed to single layer or component
                    }
                  else
                    update_client_window_of_interest();
                  return; // Can't open all frame members yet; waiting for cache
                }
                  
              if (frame_expander.get_num_members() == 0)
                { // Requested frame does not exist
                  if ((num_frames < 0) || (num_frames > frame_idx))
                    num_frames = frame_idx;
                  if (frame_idx == 0)
                    { kdu_error e; e << "Cannot render even the first "
                      "composited frame described in the JPX composition "
                      "box due to unavailability of the required compositing "
                      "layers in the original file.  Viewer will fall back "
                      "to single layer rendering mode."; }
                  change_frame_idx(frame_idx-1);
                  continue; // Loop around to try again
                }

              compositor->set_frame(&frame_expander);
            }
          if (fit_scale_to_window && jpip_client.is_one_time_request() &&
              !check_one_time_request_compatibility())
            { // Check we are opening the image in the intended manner
              compositor->remove_compositing_layer(-1,false);
              continue; // Open again in different mode
            }
          compositor->set_max_quality_layers(max_display_layers);
          compositor->cull_inactive_layers(3); // Really an arbitrary amount of
                                               // culling in this demo app.
          configuration_complete = true;
          min_rendering_scale=-1.0F; // Need to discover from scratch each time
        }
      catch (int) { // Try downgrading to a more primitive viewing mode
          if ((single_component_idx >= 0) ||
              !(jpx_in.exists() || mj2_in.exists()))
            { // Already in single component mode; nothing more primitive exists
              close_file();
              return;
            }
          else if (single_layer_idx >= 0)
            { // Downgrade from single layer mode to single component mode
              single_component_idx = 0;
              single_codestream_idx = 0;
              single_layer_idx = -1;
              if (enable_region_posting)
                update_client_window_of_interest();
            }
          else
            { // Downgrade from frame animation mode to single layer mode
              frame = NULL;
              num_frames = 0;
              frame_idx = 0;
              frame_start = frame_end = 0.0;
              single_layer_idx = 0;
            }
        }
    }

  // Get size at current scale, possibly adjusting the scale if this is the
  // first time through
  float new_rendering_scale = rendering_scale;
  kdu_dims new_image_dims = image_dims;
  try {
      bool found_valid_scale=false;
      while (fit_scale_to_window || !found_valid_scale)
        {
          if ((min_rendering_scale > 0.0F) &&
              (new_rendering_scale < min_rendering_scale))
            new_rendering_scale = min_rendering_scale;
          compositor->set_scale(transpose,vflip,hflip,new_rendering_scale);
          found_valid_scale = 
            compositor->get_total_composition_dims(new_image_dims);
          if (!found_valid_scale)
            { // Increase scale systematically before trying again
              int invalid_scale_code = compositor->check_invalid_scale_code();
              if (invalid_scale_code & KDU_COMPOSITOR_SCALE_TOO_SMALL)
                {
                  min_rendering_scale = increase_scale(new_rendering_scale);
                  if (new_rendering_scale > 1000.0F)
                    {
                      if (single_component_idx >= 0)
                        { kdu_error e;
                          e << "Cannot display the image.  Seems to "
                          "require some non-trivial scaling.";
                        }
                      else
                        {
                          { kdu_warning w; w << "Cannot display the image.  "
                            "Seems to require some non-trivial scaling.  "
                            "Downgrading to single component mode.";
                          }
                          single_component_idx = 0;
                          invalidate_configuration();
                          if (compositor->set_single_component(0,0,
                                           KDU_WANT_CODESTREAM_COMPONENTS) < 0)
                            return;
                          configuration_complete = true;
                        }
                    }
                }
              else if ((invalid_scale_code & KDU_COMPOSITOR_CANNOT_FLIP) &&
                       (vflip || hflip))
                {
                  kdu_warning w; w << "This image cannot be decompressed "
                    "with the requested geometry (horizontally or vertically "
                    "flipped), since it employs a packet wavelet transform "
                    "in which horizontally (resp. vertically) high-pass "
                    "subbands are further decomposed in the horizontal "
                    "(resp. vertical) direction.  Only transposed decoding "
                    "will be permitted.";
                  vflip = hflip = false;
                }
              else
                {
                  if (single_component_idx >= 0)
                    { kdu_error e;
                      e << "Cannot display the image.  Unexplicable error "
                        "code encountered in call to "
                        "`kdu_region_compositor::get_total_composition_dims'.";
                    }
                  else
                    {
                      { kdu_warning w; w << "Cannot display the image.  "
                        "Seems to require some incompatible set of geometric "
                        "manipulations for the various composed codestreams.";
                      }
                      single_component_idx = 0;
                      invalidate_configuration();
                      if (compositor->set_single_component(0,0,
                                       KDU_WANT_CODESTREAM_COMPONENTS) < 0)
                        return;
                      configuration_complete = true;
                    }
                }
            }
          else if (fit_scale_to_window)
            {
              kdu_coords max_tgt_size =
                kdu_coords(1000/pixel_scale,1000/pixel_scale);
              if (jpip_client.is_one_time_request() &&
                  jpip_client.get_window_in_progress(&tmp_roi) &&
                  (tmp_roi.resolution.x > 0) && (tmp_roi.resolution.y > 0))
                { // Roughly fit scale to match the resolution
                  max_tgt_size = tmp_roi.resolution;
                  max_tgt_size.x += (3*max_tgt_size.x)/4;
                  max_tgt_size.y += (3*max_tgt_size.y)/4;
                  if (!tmp_roi.region.is_empty())
                    { // Configure view and focus box based on one-time request
                      focus_centre_x = (((float) tmp_roi.region.pos.x) +
                                        0.5F*((float) tmp_roi.region.size.x)) /
                                       ((float) tmp_roi.resolution.x);
                      focus_centre_y = (((float) tmp_roi.region.pos.y) +
                                        0.5F*((float) tmp_roi.region.size.y)) /
                                       ((float) tmp_roi.resolution.y);
                      focus_size_x = ((float) tmp_roi.region.size.x) /
                                     ((float) tmp_roi.resolution.x);
                      focus_size_y = ((float) tmp_roi.region.size.y) /
                                     ((float) tmp_roi.resolution.y);
                      focus_codestream = focus_layer = -1;
                      if (single_component_idx >= 0)
                        focus_codestream = single_codestream_idx;
                      else if (single_layer_idx >= 0)
                        focus_layer = single_layer_idx;
                      enable_focus_box = true;
                      view_centre_x = focus_centre_x;
                      view_centre_y = focus_centre_y;
                      view_centre_known = true;
                    }
                }
              float max_x = new_rendering_scale *
                ((float) max_tgt_size.x) / ((float) new_image_dims.size.x);
              float max_y = new_rendering_scale *
                ((float) max_tgt_size.y) / ((float) new_image_dims.size.y);
              while (((min_rendering_scale < 0.0) ||
                      (new_rendering_scale > min_rendering_scale)) &&
                     ((new_rendering_scale > max_x) ||
                      (new_rendering_scale > max_y)))
                {
                  new_rendering_scale = decrease_scale(new_rendering_scale);
                  found_valid_scale = false;
                }
              fit_scale_to_window = false;
            }
        }
    }
  catch (int) { // Some error occurred while parsing code-streams
      close_file();
      return;
    }

  // Install the dimensioning parameters we just found, checking to see
  // if the window needs to be resized.
  rendering_scale = new_rendering_scale;
  if ((image_dims == new_image_dims) && (!view_dims.is_empty()) &&
      (!buffer_dims.is_empty()))
    { // No need to resize the window
      compositor->set_buffer_surface(buffer_dims);
      buffer = compositor->get_composition_bitmap(buffer_dims);
      if (adjust_focus_anchors())
        update_focus_box();
    }
  else
    { // Send a message to the child view window identifying the
      // maximum allowable image dimensions.  We expect to hear back (possibly
      // after some windows message processing) concerning
      // the actual view dimensions via the `set_view_size' function.
      view_dims = buffer_dims = kdu_dims();
      image_dims = new_image_dims;
      if (adjust_focus_anchors())
        { view_centre_x = focus_centre_x; view_centre_y = focus_centre_y; }
      child_wnd->Invalidate();
      child_wnd->set_max_view_size(image_dims.size,pixel_scale);
    }
  update_client_window_of_interest();
  if (in_playmode)
    { // See if we need to turn off play mode.
      if ((single_component_idx >= 0) ||
          ((single_layer_idx >= 0) && jpx_in.exists()))
        stop_playmode();
    }
  display_status();
}

/******************************************************************************/
/*                      CKdu_showApp::suspend_processing                      */
/******************************************************************************/

void
  CKdu_showApp::suspend_processing(bool suspend)
{
  processing_suspended = suspend;
}

/******************************************************************************/
/*                       CKdu_showApp::refresh_display                        */
/******************************************************************************/

void
  CKdu_showApp::refresh_display()
{
  if ((compositor == NULL) || in_playmode)
    return;
  if (configuration_complete && !compositor->refresh())
    { // Can no longer trust buffer surfaces
      kdu_dims new_image_dims;
      bool have_valid_scale = 
        compositor->get_total_composition_dims(new_image_dims);
      if ((!have_valid_scale) || (new_image_dims != image_dims))
        initialize_regions();
      else
        buffer = compositor->get_composition_bitmap(buffer_dims);
    }
  if (metashow != NULL)
    metashow->update_tree();
  if (refresh_timer_id != 0)
    { // Kill outstanding requests for a delayed refresh.
      m_pMainWnd->KillTimer(refresh_timer_id);
      refresh_timer_id = 0;
    }
}

/******************************************************************************/
/*                       CKdu_showApp::flash_overlays                         */
/******************************************************************************/

void
  CKdu_showApp::flash_overlays()
{
  if (flash_timer_id != 0)
    { // Kill outstanding requests for overlay flashing
      m_pMainWnd->KillTimer(flash_timer_id);
      flash_timer_id = 0;
    }
  if (!(overlays_enabled && overlay_flashing_enabled && (compositor != NULL)))
    return;
  overlay_painting_colour++;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
    (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));
}

/******************************************************************************/
/*               CKdu_showApp::convert_region_to_focus_anchors                */
/******************************************************************************/

void
  CKdu_showApp::convert_region_to_focus_anchors(kdu_dims region, kdu_dims ref)
{
  region &= ref;
  if (!region)
    {
      focus_anchors_known = enable_focus_box = false;
      return;
    }
  focus_centre_x =
    (float)(region.pos.x + region.size.x/2 - ref.pos.x) / ((float) ref.size.x);
  focus_centre_y =
    (float)(region.pos.y + region.size.y/2 - ref.pos.y) / ((float) ref.size.y);
  focus_size_x =
    ((float) region.size.x) / ((float) ref.size.x);
  focus_size_y =
    ((float) region.size.y) / ((float) ref.size.y);
  focus_anchors_known = true;
}

/******************************************************************************/
/*               CKdu_showApp::convert_focus_anchors_to_region                */
/******************************************************************************/

void
  CKdu_showApp::convert_focus_anchors_to_region(kdu_dims &region, kdu_dims ref)
{
  region.size.x = (int)(0.5F + ref.size.x * focus_size_x);
  region.size.y = (int)(0.5F + ref.size.y * focus_size_y);
  if (region.size.x <= 0)
    region.size.x = 1;
  if (region.size.y <= 0)
    region.size.y = 1;
  region.pos.x = (int)(0.5F + ref.pos.x +
    (focus_centre_x-0.5F*focus_size_x) * ref.size.x);
  region.pos.y = (int)(0.5F + ref.pos.y +
    (focus_centre_y-0.5F*focus_size_y) * ref.size.y);
}

/******************************************************************************/
/*                     CKdu_showApp::adjust_focus_anchors                     */
/******************************************************************************/

bool
  CKdu_showApp::adjust_focus_anchors()
{
  if (image_dims.is_empty() || !(focus_anchors_known && configuration_complete))
    return false;
  if (focus_codestream >= 0)
    {
      if (single_component_idx >= 0)
        return false; // No change in image type
      kdu_dims stream_region, stream_dims =
        compositor->find_codestream_region(focus_codestream,0,false);
      if (stream_dims.is_empty())
        { // Cannot map focus across from unused codestream
          focus_anchors_known = enable_focus_box = false;
        }
      else
        {
          convert_focus_anchors_to_region(stream_region,stream_dims);
          convert_region_to_focus_anchors(stream_region,image_dims);
          focus_codestream = -1;
          focus_layer = (single_layer_idx >= 0)?single_layer_idx:-1;
        }
    }
  else if (focus_layer >= 0)
    {
      if (single_layer_idx >= 0)
        return false; // Adjustments required only when focus was generated for
                      // a more primitive image type (map layer to frame)
      kdu_dims layer_region, layer_dims =
        compositor->find_layer_region(focus_layer,0,false);
      if (layer_dims.is_empty())
        { // Cannot map focus across from unused compositing layer
          focus_anchors_known = enable_focus_box = false;
        }
      else
        {
          convert_focus_anchors_to_region(layer_region,layer_dims);
          convert_region_to_focus_anchors(layer_region,image_dims);
          focus_layer = -1;
        }
    }
  else
    return false; // Adjustments required only when focus was generated for
                  // a more primitive image type (frames are the most advanced)
  return true;
}


/******************************************************************************/
/*                    CKdu_showApp::calculate_view_anchors                    */
/******************************************************************************/

void
  CKdu_showApp::calculate_view_anchors()
{
  if ((buffer == NULL) || !image_dims)
    return;
  view_centre_known = true;
  view_centre_x =
    (float)(view_dims.pos.x + view_dims.size.x/2 - image_dims.pos.x) /
      ((float) image_dims.size.x);
  view_centre_y =
    (float)(view_dims.pos.y + view_dims.size.y/2 - image_dims.pos.y) /
      ((float) image_dims.size.y);
  if (enable_focus_box)
    convert_region_to_focus_anchors(focus_box,image_dims);
  focus_codestream = focus_layer = -1;
  if (single_component_idx >= 0)
    focus_codestream = single_codestream_idx;
  else if (single_layer_idx >= 0)
    focus_layer = single_layer_idx;
}

/******************************************************************************/
/*                        CKdu_showApp::set_view_size                         */
/******************************************************************************/

void
  CKdu_showApp::set_view_size(kdu_coords size)
{
  if ((compositor == NULL) || !configuration_complete)
    return;
  if (child_wnd != NULL)
    child_wnd->cancel_focus_drag();

  // Set view region to the largest subset of the image region consistent with
  // the size of the new viewing region.
  kdu_dims new_view_dims = view_dims;
  new_view_dims.size = size;
  if (view_centre_known)
    {
      new_view_dims.pos.x = image_dims.pos.x - (size.x / 2) +
        (int) floor(0.5 + image_dims.size.x*view_centre_x);
      new_view_dims.pos.y = image_dims.pos.y - (size.y / 2) +
        (int) floor(0.5 + image_dims.size.y*view_centre_y);
      view_centre_known = false;
    }
  if (new_view_dims.pos.x < image_dims.pos.x)
    new_view_dims.pos.x = image_dims.pos.x;
  if (new_view_dims.pos.y < image_dims.pos.y)
    new_view_dims.pos.y = image_dims.pos.y;
  kdu_coords view_lim = new_view_dims.pos + new_view_dims.size;
  kdu_coords image_lim = image_dims.pos + image_dims.size;
  if (view_lim.x > image_lim.x)
    new_view_dims.pos.x -= view_lim.x-image_lim.x;
  if (view_lim.y > image_lim.y)
    new_view_dims.pos.y -= view_lim.y-image_lim.y;
  new_view_dims &= image_dims;
  bool need_redraw = new_view_dims.pos != view_dims.pos;
  view_dims = new_view_dims;

  // Get preferred minimum dimensions for the new buffer region.
  size = view_dims.size;
  if (!in_playmode)
    { // A small boundary minimizes impact of scrolling
      size.x += (size.x>>4)+100;
      size.y += (size.y>>4)+100;
    }

  // Make sure buffered region is no larger than image
  if (size.x > image_dims.size.x)
    size.x = image_dims.size.x;
  if (size.y > image_dims.size.y)
    size.y = image_dims.size.y;
  kdu_dims new_buffer_dims;
  new_buffer_dims.size = size;
  new_buffer_dims.pos = buffer_dims.pos;

  // Make sure the buffer region is contained within the image
  kdu_coords buffer_lim = new_buffer_dims.pos + new_buffer_dims.size;
  if (buffer_lim.x > image_lim.x)
    new_buffer_dims.pos.x -= buffer_lim.x-image_lim.x;
  if (buffer_lim.y > image_lim.y)
    new_buffer_dims.pos.y -= buffer_lim.y-image_lim.y;
  if (new_buffer_dims.pos.x < image_dims.pos.x)
    new_buffer_dims.pos.x = image_dims.pos.x;
  if (new_buffer_dims.pos.y < image_dims.pos.y)
    new_buffer_dims.pos.y = image_dims.pos.y;
  assert(new_buffer_dims == (new_buffer_dims & image_dims));

  // See if the buffered region includes any new locations at all.
  if ((new_buffer_dims.pos != buffer_dims.pos) ||
      (new_buffer_dims != (new_buffer_dims & buffer_dims)) ||
      (view_dims != (view_dims & new_buffer_dims)))
    { // We will need to reshuffle or resize the buffer anyway, so might
      // as well get the best location for the buffer.
      new_buffer_dims.pos.x = view_dims.pos.x;
      new_buffer_dims.pos.y = view_dims.pos.y;
      new_buffer_dims.pos.x -= (new_buffer_dims.size.x-view_dims.size.x)/2;
      new_buffer_dims.pos.y -= (new_buffer_dims.size.y-view_dims.size.y)/2;
      if (new_buffer_dims.pos.x < image_dims.pos.x)
        new_buffer_dims.pos.x = image_dims.pos.x;
      if (new_buffer_dims.pos.y < image_dims.pos.y)
        new_buffer_dims.pos.y = image_dims.pos.y;
      buffer_lim = new_buffer_dims.pos + new_buffer_dims.size;
      if (buffer_lim.x > image_lim.x)
        new_buffer_dims.pos.x -= buffer_lim.x - image_lim.x;
      if (buffer_lim.y > image_lim.y)
        new_buffer_dims.pos.y -= buffer_lim.y - image_lim.y;
      assert(view_dims == (view_dims & new_buffer_dims));
      assert(new_buffer_dims == (image_dims & new_buffer_dims));
      assert(view_dims == (new_buffer_dims & view_dims));
    }

  // Set surface and get buffer
  compositor->set_buffer_surface(new_buffer_dims);
  buffer = compositor->get_composition_bitmap(buffer_dims);

  // Now reflect changes in the view size to the appearance of scroll bars.

  SCROLLINFO sc_info; sc_info.cbSize = sizeof(sc_info);
  sc_info.fMask = SIF_DISABLENOSCROLL | SIF_ALL;
  sc_info.nMin = 0;
  sc_info.nMax = image_dims.size.x-1;
  sc_info.nPage = view_dims.size.x;
  sc_info.nPos = view_dims.pos.x - image_dims.pos.x;
  child_wnd->SetScrollInfo(SB_HORZ,&sc_info);
  sc_info.fMask = SIF_DISABLENOSCROLL | SIF_ALL;
  sc_info.nMin = 0;
  sc_info.nMax = image_dims.size.y-1;
  sc_info.nPage = view_dims.size.y;
  sc_info.nPos = view_dims.pos.y - image_dims.pos.y;
  child_wnd->SetScrollInfo(SB_VERT,&sc_info);
  kdu_coords step = view_dims.size;
  step.x = (step.x >> 4) + 1;
  step.y = (step.y >> 4) + 1;
  kdu_coords page = view_dims.size - step;
  child_wnd->set_scroll_metrics(step,page,image_dims.size-view_dims.size);

  // Update related display properties
  if (need_redraw)
    child_wnd->Invalidate();
  child_wnd->UpdateWindow();
  update_focus_box();
  if (jpip_progress != NULL)
    { delete jpip_progress;  jpip_progress = NULL; }
}

/******************************************************************************/
/*                      CKdu_showApp::resize_stripmap                         */
/******************************************************************************/

void
  CKdu_showApp::resize_stripmap(int min_width)
{
  if (min_width <= strip_extent.x)
    return;
  strip_extent.x = min_width + 100;
  strip_extent.y = 100;
  memset(&stripmap_info,0,sizeof(stripmap_info));
  stripmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  stripmap_info.bmiHeader.biWidth = strip_extent.x;
  stripmap_info.bmiHeader.biHeight = -strip_extent.y;
  stripmap_info.bmiHeader.biPlanes = 1;
  stripmap_info.bmiHeader.biBitCount = 32;
  stripmap_info.bmiHeader.biCompression = BI_RGB;
  if (stripmap != NULL)
    DeleteObject(stripmap);
  stripmap =
    CreateDIBSection(NULL,&stripmap_info,DIB_RGB_COLORS,
                     (void **) &stripmap_buffer,NULL,0);
}

/******************************************************************************/
/*                       CKdu_showApp::set_hscroll_pos                        */
/******************************************************************************/

void
  CKdu_showApp::set_hscroll_pos(int pos, bool relative_to_last)
{
  if ((compositor == NULL) || (buffer == NULL))
    return;
  if (child_wnd != NULL)
    child_wnd->cancel_focus_drag();

  view_centre_known = false;
  if (relative_to_last)
    pos += view_dims.pos.x;
  else
    pos += image_dims.pos.x;
  if (pos < image_dims.pos.x)
    pos = image_dims.pos.x;
  if ((pos+view_dims.size.x) > (image_dims.pos.x+image_dims.size.x))
    pos = image_dims.pos.x+image_dims.size.x-view_dims.size.x;
  if (pos != view_dims.pos.x)
    {
      RECT update;
      child_wnd->ScrollWindowEx((view_dims.pos.x-pos)*pixel_scale,0,
                                NULL,NULL,NULL,&update,0);
      view_dims.pos.x = pos;
      if (view_dims != (view_dims & buffer_dims))
        { // The view is no longer fully contained in the buffered region.
          buffer_dims.pos.x =
            view_dims.pos.x - (buffer_dims.size.x-view_dims.size.x)/2;
          if (buffer_dims.pos.x < image_dims.pos.x)
            buffer_dims.pos.x = image_dims.pos.x;
          int image_lim = image_dims.pos.x+image_dims.size.x;
          int buf_lim = buffer_dims.pos.x + buffer_dims.size.x;
          if (buf_lim > image_lim)
            buffer_dims.pos.x -= (buf_lim-image_lim);
          compositor->set_buffer_surface(buffer_dims);
          buffer = compositor->get_composition_bitmap(buffer_dims);
        }
      // Repaint the erased area -- note that although the scroll function
      // is supposed to be able to invalidate the relevant regions of the
      // window, rendering this code unnecessary, that functionality appears
      // to be able to fail badly under certain extremely fast scrolling
      // sequences. Best to do the job ourselves.
      update.left = update.left / pixel_scale;
      update.right = (update.right+pixel_scale-1) / pixel_scale;
      update.top = update.top / pixel_scale;
      update.bottom = (update.bottom+pixel_scale-1) / pixel_scale;
      kdu_dims update_dims;
      update_dims.pos.x = update.left;
      update_dims.pos.y = update.top;
      update_dims.size.x = update.right-update.left;
      update_dims.size.y = update.bottom-update.top;

      update_dims.pos += view_dims.pos; // Get into the global coordinate system
      CDC *dc = child_wnd->GetDC();
      paint_region(dc,update_dims);
      child_wnd->ReleaseDC(dc);
    }
  child_wnd->SetScrollPos(SB_HORZ,pos-image_dims.pos.x);
  update_focus_box();
}

/******************************************************************************/
/*                       CKdu_showApp::set_vscroll_pos                        */
/******************************************************************************/

void
  CKdu_showApp::set_vscroll_pos(int pos, bool relative_to_last)
{
  if ((compositor == NULL) || (buffer == NULL))
    return;
  if (child_wnd != NULL)
    child_wnd->cancel_focus_drag();

  view_centre_known = false;
  if (relative_to_last)
    pos += view_dims.pos.y;
  else
    pos += image_dims.pos.y;
  if (pos < image_dims.pos.y)
    pos = image_dims.pos.y;
  if ((pos+view_dims.size.y) > (image_dims.pos.y+image_dims.size.y))
    pos = image_dims.pos.y+image_dims.size.y-view_dims.size.y;
  if (pos != view_dims.pos.y)
    {
      RECT update;
      child_wnd->ScrollWindowEx(0,(view_dims.pos.y-pos)*pixel_scale,
                                NULL,NULL,NULL,&update,0);
      view_dims.pos.y = pos;
      if (view_dims != (view_dims & buffer_dims))
        { // The view is no longer fully contained in the buffered region.
          buffer_dims.pos.y =
            view_dims.pos.y - (buffer_dims.size.y-view_dims.size.y)/2;
          if (buffer_dims.pos.y < image_dims.pos.y)
            buffer_dims.pos.y = image_dims.pos.y;
          int image_lim = image_dims.pos.y+image_dims.size.y;
          int buf_lim = buffer_dims.pos.y + buffer_dims.size.y;
          if (buf_lim > image_lim)
            buffer_dims.pos.y -= (buf_lim-image_lim);
          compositor->set_buffer_surface(buffer_dims);
          buffer = compositor->get_composition_bitmap(buffer_dims);
        }
      // Repaint the erased area -- note that although the scroll function
      // is supposed to be able to invalidate the relevant regions of the
      // window, rendering this code unnecessary, that functionality appears
      // to be able to fail badly under certain extremely fast scrolling
      // sequences.  Best to do the job ourselves.
      update.left = update.left / pixel_scale;
      update.right = (update.right+pixel_scale-1) / pixel_scale;
      update.top = update.top / pixel_scale;
      update.bottom = (update.bottom+pixel_scale-1) / pixel_scale;
      kdu_dims update_dims;
      update_dims.pos.x = update.left;
      update_dims.pos.y = update.top;
      update_dims.size.x = update.right-update.left;
      update_dims.size.y = update.bottom-update.top;
      update_dims.pos += view_dims.pos; // Get into the global coordinate system
      CDC *dc = child_wnd->GetDC();
      paint_region(dc,update_dims);
      child_wnd->ReleaseDC(dc);
    }
  child_wnd->SetScrollPos(SB_VERT,pos-image_dims.pos.y);
  update_focus_box();
}

/******************************************************************************/
/*                       CKdu_showApp::set_scroll_pos                         */
/******************************************************************************/

void
  CKdu_showApp::set_scroll_pos(int pos_x, int pos_y, bool relative_to_last)
{
  if ((compositor == NULL) || (buffer == NULL))
    return;
  view_centre_known = false;
  if (relative_to_last)
    {
      pos_y += view_dims.pos.y;
      pos_x += view_dims.pos.x;
    }
  else
    {
      pos_y += image_dims.pos.y;
      pos_x += image_dims.pos.x;
    }
  if (pos_y < image_dims.pos.y)
    pos_y = image_dims.pos.y;
  if ((pos_y+view_dims.size.y) > (image_dims.pos.y+image_dims.size.y))
    pos_y = image_dims.pos.y+image_dims.size.y-view_dims.size.y;
  if (pos_x < image_dims.pos.x)
    pos_x = image_dims.pos.x;
  if ((pos_x+view_dims.size.x) > (image_dims.pos.x+image_dims.size.x))
    pos_x = image_dims.pos.x+image_dims.size.x-view_dims.size.x;

  if ((pos_y != view_dims.pos.y) || (pos_x != view_dims.pos.x))
    {
      RECT update;
      child_wnd->ScrollWindowEx((view_dims.pos.x-pos_x)*pixel_scale,
                                (view_dims.pos.y-pos_y)*pixel_scale,
                                NULL,NULL,NULL,&update,0);
      view_dims.pos.x = pos_x;
      view_dims.pos.y = pos_y;
      if (view_dims != (view_dims & buffer_dims))
        { // The view is no longer fully contained in the buffered region.
          buffer_dims.pos.x =
            view_dims.pos.x - (buffer_dims.size.x-view_dims.size.x)/2;
          buffer_dims.pos.y =
            view_dims.pos.y - (buffer_dims.size.y-view_dims.size.y)/2;
          if (buffer_dims.pos.x < image_dims.pos.x)
            buffer_dims.pos.x = image_dims.pos.x;
          if (buffer_dims.pos.y < image_dims.pos.y)
            buffer_dims.pos.y = image_dims.pos.y;
          kdu_coords image_lim = image_dims.pos + image_dims.size;
          kdu_coords buf_lim = buffer_dims.pos + buffer_dims.size;
          if (buf_lim.y > image_lim.y)
            buffer_dims.pos.y -= (buf_lim.y - image_lim.y);
          if (buf_lim.x > image_lim.x)
            buffer_dims.pos.x -= (buf_lim.x - image_lim.x);
          compositor->set_buffer_surface(buffer_dims);
          buffer = compositor->get_composition_bitmap(buffer_dims);
        }
      // Repaint the erased area -- note that although the scroll function
      // is supposed to be able to invalidate the relevant regions of the
      // window, rendering this code unnecessary, that functionality appears
      // to be able to fail badly under certain extremely fast scrolling
      // sequences.  Best to do the job ourselves.
      update.left = update.left / pixel_scale;
      update.right = (update.right+pixel_scale-1) / pixel_scale;
      update.top = update.top / pixel_scale;
      update.bottom = (update.bottom+pixel_scale-1) / pixel_scale;
      kdu_dims update_dims;
      update_dims.pos.x = update.left;
      update_dims.pos.y = update.top;
      update_dims.size.x = update.right-update.left;
      update_dims.size.y = update.bottom-update.top;
      update_dims.pos += view_dims.pos; // Get into the global coordinate system
      CDC *dc = child_wnd->GetDC();
      paint_region(dc,update_dims);
      child_wnd->ReleaseDC(dc);
    }
  child_wnd->SetScrollPos(SB_HORZ,pos_x-image_dims.pos.x);
  child_wnd->SetScrollPos(SB_VERT,pos_y-image_dims.pos.y);
  update_focus_box();  
}

/******************************************************************************/
/*                        CKdu_showApp::paint_region                          */
/******************************************************************************/

void
  CKdu_showApp::paint_region(CDC *dc, kdu_dims region, bool is_absolute)
{
  if (!is_absolute)
    region.pos += view_dims.pos;
  kdu_dims region_on_buffer = region;
  region_on_buffer &= buffer_dims; // Intersect with buffer region
  region_on_buffer.pos -= buffer_dims.pos; // Make relative to buffer region
  if (region_on_buffer.size != region.size)
    { /* Need to erase the region first and then modify it to reflect the
         region we are about to actually paint. */
      dc->BitBlt(region.pos.x*pixel_scale,region.pos.y*pixel_scale,
                 region.size.x*pixel_scale,region.size.y*pixel_scale,
                 NULL,0,0,WHITENESS);
      region = region_on_buffer;
      region.pos += buffer_dims.pos; // Convert to absolute region
    }
  region.pos -= view_dims.pos; // Make relative to curent view
  if ((!region_on_buffer) || (buffer == NULL))
    return;
  assert(region.size == region_on_buffer.size);

  kdu_coords screen_pos;

  int bitmap_row_gap;
  kdu_uint32 *bitmap_buffer;
  HBITMAP bitmap = buffer->access_bitmap(bitmap_buffer,bitmap_row_gap);
  if (enable_focus_box && highlight_focus)
    { // Paint through intermediate strip buffer
      assert(bitmap != NULL);
      kdu_dims focus_region = focus_box;
      focus_region.pos -= view_dims.pos; // Make relative to view port
      focus_region &= region; // Get portion of focus box inside paint region
      if (!focus_region)
        focus_region.pos = region.pos;
      int focus_above = focus_region.pos.y - region.pos.y;
      int focus_height = focus_region.size.y;
      int focus_left = focus_region.pos.x - region.pos.x;
      int focus_width = focus_region.size.x;
      int focus_right = region.size.x - focus_left - focus_width;
      assert((focus_above >= 0) && (focus_height >= 0) &&
             (focus_left >= 0) && (focus_width >= 0) && (focus_right >= 0));
      if (strip_extent.x < region.size.x)
        resize_stripmap(region.size.x);
      while (region.size.y > 0)
        {
          int i, j, xfer_height = region.size.y;
          if (xfer_height > strip_extent.y)
            xfer_height = strip_extent.y;
          kdu_uint32 *sp, *spp = bitmap_buffer +
            (region_on_buffer.pos.x + region_on_buffer.pos.y*bitmap_row_gap);
          kdu_uint32 *dp, *dpp = stripmap_buffer;
          for (j=xfer_height; j > 0; j--,
               spp+=bitmap_row_gap, dpp+=strip_extent.x,
               focus_above--)
            {
              sp = spp; dp = dpp;
              kdu_uint32 val;
              kdu_byte *lut;
              if ((focus_above <= 0) && (focus_height > 0))
                {
                  focus_height--;
                  for (lut=background_lut, i=focus_left; i > 0; i--)
                    {
                      val = *(sp++);
                      *(dp++) = 0xFF000000 +
                        (((kdu_uint32) lut[(val>>16)&0xFF])<<16) +
                        (((kdu_uint32) lut[(val>> 8)&0xFF])<< 8) +
                        (((kdu_uint32) lut[(val    )&0xFF])    );
                    }
                  for (lut=foreground_lut, i=focus_width; i > 0; i--)
                    {
                      val = *(sp++);
                      *(dp++) = 0xFF000000 +
                        (((kdu_uint32) lut[(val>>16)&0xFF])<<16) +
                        (((kdu_uint32) lut[(val>> 8)&0xFF])<< 8) +
                        (((kdu_uint32) lut[(val    )&0xFF])    );
                    }
                  i = focus_right;
                }
              else
                i = region.size.x;
              for (lut=background_lut; i > 0; i--)
                {
                  val = *(sp++);
                  *(dp++) = 0xFF000000 +
                    (((kdu_uint32) lut[(val>>16)&0xFF])<<16) +
                    (((kdu_uint32) lut[(val>> 8)&0xFF])<< 8) +
                    (((kdu_uint32) lut[(val    )&0xFF])    );
                }
            }
          HBITMAP old_bitmap = (HBITMAP)
            SelectObject(compatible_dc.m_hDC,stripmap);
          if (pixel_scale == 1)
            dc->BitBlt(region.pos.x,region.pos.y,region.size.x,xfer_height,
                       &compatible_dc,0,0,SRCCOPY);
          else
            dc->StretchBlt(region.pos.x*pixel_scale,region.pos.y*pixel_scale,
                           region.size.x*pixel_scale,xfer_height*pixel_scale,
                           &compatible_dc,0,0,region.size.x,xfer_height,
                           SRCCOPY);
          SelectObject(compatible_dc.m_hDC,old_bitmap);
          region.pos.y += xfer_height;
          region_on_buffer.pos.y += xfer_height;
          region.size.y -= xfer_height;
        }
    }
  else
    { // Paint bitmap directly
      SelectObject(compatible_dc.m_hDC,bitmap);
      if (pixel_scale == 1)
        dc->BitBlt(region.pos.x,region.pos.y,region.size.x,region.size.y,
                   &compatible_dc,region_on_buffer.pos.x,region_on_buffer.pos.y,
                   SRCCOPY);
      else
        dc->StretchBlt(region.pos.x*pixel_scale,region.pos.y*pixel_scale,
                       region.size.x*pixel_scale,region.size.y*pixel_scale,
                       &compatible_dc,
                       region_on_buffer.pos.x,region_on_buffer.pos.y,
                       region.size.x,region.size.y,SRCCOPY);
    }
  if (enable_focus_box)
    outline_focus_box(dc);
}

/******************************************************************************/
/*                        CKdu_showApp::display_status                        */
/******************************************************************************/

void
  CKdu_showApp::display_status()
{
  if (in_playmode)
    return;

  if (playcontrol != NULL)
    {
      int track_idx = -1;
      if (single_component_idx < 0)
        {
          if (single_layer_idx < 0)
            track_idx = 0;
          else if (mj2_in.exists())
            track_idx = single_layer_idx+1;
        }
      playcontrol->update(track_idx,num_frames,frame_idx);
    }

  char string[128]; string[0] = '\0';
  char *sp = string;

  kdu_dims basis_region = (enable_focus_box)?focus_box:view_dims;
  float component_scale_x=-1.0F, component_scale_y=-1.0F;
  if ((compositor != NULL) && configuration_complete)
    {
      kdrc_stream *scan =
        compositor->get_next_visible_codestream(NULL,basis_region,false);
      if ((scan != NULL) &&
          (compositor->get_next_visible_codestream(scan,basis_region,
                                                   false) == NULL))
        {
          int cs_idx, layer_idx;
          compositor->get_codestream_info(scan,cs_idx,layer_idx,NULL,NULL,
                                          &component_scale_x,
                                          &component_scale_y);
          component_scale_x *= rendering_scale;
          component_scale_y *= rendering_scale;
          if (transpose)
            {
              float tmp=component_scale_x;
              component_scale_x=component_scale_y;
              component_scale_y=tmp;
            }
        }
    }

  if ((status_id == KDS_STATUS_CACHE) && !jpip_client.is_active())
    status_id = KDS_STATUS_LAYER_RES;

  if ((jpip_progress != NULL) && (status_id != KDS_STATUS_CACHE))
    { delete jpip_progress; jpip_progress = NULL; }
  if ((compositor == NULL) && !jpip_client.is_active())
    string[0] = '\0';
  else if ((status_id == KDS_STATUS_LAYER_RES) ||
           (status_id == KDS_STATUS_DIMS))
    {
      float res_percent = 100.0F*rendering_scale;
      float x_percent = 100.0F*component_scale_x;
      float y_percent = 100.0F*component_scale_y;
      if ((x_percent > (0.99F*res_percent)) &&
          (x_percent < (1.01F*res_percent)) &&
          (y_percent > (0.99F*res_percent)) &&
          (y_percent < (1.01F*res_percent)))
        x_percent = y_percent = -1.0F;
      if (x_percent < 0.0F)
        sprintf(sp,"Res=%1.1f%%",res_percent);
      else if ((x_percent > (0.99F*y_percent)) &&
               (x_percent < (1.01F*y_percent)))
        sprintf(sp,"Res=%1.1f%% (%1.1f%%)",res_percent,x_percent);
      else
        sprintf(sp,"Res=%1.1f%% (x:%1.1f%%,y:%1.1f%%)",
                res_percent,x_percent,y_percent);
      sp += strlen(sp);

      if (single_component_idx >= 0)
        {
          sprintf(sp,", Comp=%d/",single_component_idx+1);
          sp += strlen(sp);
          if (max_components <= 0)
            *(sp++) = '?';
          else
            { sprintf(sp,"%d",max_components); sp += strlen(sp); }
          sprintf(sp,", Stream=%d/",single_codestream_idx+1);
          sp += strlen(sp);
          if (max_codestreams <= 0)
            *(sp++) = '?';
          else
            { sprintf(sp,"%d",max_codestreams); sp += strlen(sp); }
        }
      else if ((single_layer_idx >= 0) && !mj2_in)
        { // Report compositing layer index
          sprintf(sp,", C.Layer=%d/",single_layer_idx+1);
          sp += strlen(sp);
          if (max_compositing_layer_idx < 0)
            *(sp++) = '?';
          else
            { sprintf(sp,"%d",max_compositing_layer_idx+1); sp += strlen(sp); }
        }
      else if ((single_layer_idx >= 0) && mj2_in.exists())
        { // Report track and frame indices, rather than layer index
          sprintf(sp,", Track:Frame=%d:%d",single_layer_idx+1,frame_idx+1);
          sp += strlen(sp);
        }
      else
        {
          sprintf(sp,", Frame=%d/",frame_idx+1);
          sp += strlen(sp);
          if (num_frames <= 0)
            *(sp++) = '?';
          else
            { sprintf(sp,"%d",num_frames); sp += strlen(sp); }
        }

      *(sp++) = '\t';
      if (compositor != NULL)
        {
          if (!compositor->is_processing_complete())
            { strcpy(sp,"WORKING"); sp += strlen(sp); }
          if (status_id == KDS_STATUS_DIMS)
            sprintf(sp,"\tHxW=%dx%d ",image_dims.size.y,image_dims.size.x);
          else        
            {
              int max_layers = compositor->get_max_available_quality_layers();
              if (max_display_layers >= max_layers)
                sprintf(sp,"\tQ.Layers=all ");
              else
                sprintf(sp,"\tQ.Layers=%d/%d ",max_display_layers,max_layers);
            }
        }
    }
  else if (status_id == KDS_STATUS_MEM)
    {
      kdu_long bytes = 0;
      if (compositor != NULL)
        {
          kdrc_stream *str=NULL;
          while ((str=compositor->get_next_codestream(str,false,true)) != NULL)
            {
              kdu_codestream ifc = compositor->access_codestream(str);
              bytes += ifc.get_compressed_data_memory()
                     + ifc.get_compressed_state_memory();
            }
        }
      if (jpip_client.is_active())
        bytes += jpip_client.get_peak_cache_memory();
      sprintf(sp,"Compressed working/cache memory = %5g MB",1.0E-6*bytes);
      sp += strlen(sp);
      if ((compositor != NULL) && !compositor->is_processing_complete())
        { strcpy(sp,"\t\tWORKING "); sp += strlen(sp); }
    }
  else if (status_id == KDS_STATUS_CACHE)
    {
      float received_kbytes = ((float) 1.0E-3) *
        jpip_client.get_received_bytes();
      strcpy(sp,jpip_client.get_status());
      sp += strlen(sp);
      sprintf(sp,"\t\t%8.1f kB",received_kbytes);
      sp += strlen(sp);
      if ((compositor != NULL) && !compositor->is_processing_complete())
        { strcpy(sp,"WORKING "); sp += strlen(sp); }
      if (cumulative_transmission_time || last_transmission_start)
        {
          double time = cumulative_transmission_time;
          if (last_transmission_start)
            time += (double)(clock() - last_transmission_start);
          time *= (1.0 / CLOCKS_PER_SEC);
          if (time > 0.0)
            sprintf(sp,"; %7.1f kB/s ",received_kbytes / time);
        }
      if (jpip_progress == NULL)
        {
          RECT rc;
          frame_wnd->get_status_bar()->GetItemRect(0,&rc);
          int size = rc.right-rc.left;
          rc.left += (3*size) >> 3;
          rc.right -= (3*size) >> 3;
          jpip_progress = new CProgressCtrl();
          jpip_progress->Create(WS_CHILD|WS_VISIBLE,rc,
                                frame_wnd->get_status_bar(),1);
          jpip_progress->SetRange(0,100);
          jpip_progress->SetPos(0);
          jpip_progress->SetStep(1);
        }
    }
  frame_wnd->SetMessageText(string);
  if ((jpip_progress != NULL) && (compositor != NULL) &&
      compositor->is_processing_complete())
    {
      kdu_long samples=0, packet_samples=0, max_packet_samples=0, s, p, m;
      kdrc_stream *stream=NULL;
      while ((compositor != NULL) &&
             ((stream = compositor->get_next_visible_codestream(stream,
                                              basis_region,true)) != NULL))
        {
          compositor->get_codestream_packets(stream,basis_region,s,p,m);
          samples += s;  packet_samples += p;  max_packet_samples += m;
        }
      int pos = 0;
      if (packet_samples > 0)
        pos = (int) floor(0.01+100.0 * (((double) packet_samples) /
                                       ((double) max_packet_samples)));
      jpip_progress->SetPos(pos);
    }
}

/******************************************************************************/
/*            CKdu_showApp::check_one_time_request_compatibility              */
/******************************************************************************/

bool
  CKdu_showApp::check_one_time_request_compatibility()
{
  if ((!jpx_in.exists()) || (single_component_idx >= 0))
    return true;
  if (!jpip_client.get_window_in_progress(&tmp_roi))
    return true; // Window information not available yet

  int c;
  kdu_sampled_range *rg;
  bool compatible = true;
  if ((single_component_idx < 0) && (single_layer_idx < 0) &&
      (frame_idx == 0) && (frame != NULL))
    { // Check compatibility of default animation frame
      int m, num_members = frame_expander.get_num_members();
      for (m=0; m < num_members; m++)
        {
          int instruction_idx, layer_idx;
          bool covers_composition;
          kdu_dims source_dims, target_dims;
          int remapping_ids[2];
          jx_frame *frame =
            frame_expander.get_member(m,instruction_idx,layer_idx,
                                      covers_composition,source_dims,
                                      target_dims);
          composition_rules.get_original_iset(frame,instruction_idx,
                                           remapping_ids[0],remapping_ids[1]);
          for (c=0; (rg=tmp_roi.contexts.access_range(c)) != NULL; c++)
            if ((rg->from <= layer_idx) && (rg->to >= layer_idx) &&
                (((layer_idx-rg->from) % rg->step) == 0))
              {
                if (covers_composition &&
                    (rg->remapping_ids[0] < 0) && (rg->remapping_ids[1] < 0))
                  break; // Found compatible layer
                if ((rg->remapping_ids[0] == remapping_ids[0]) &&
                    (rg->remapping_ids[1] == remapping_ids[1]))
                  break; // Found compatible layer
              }
          if (rg == NULL)
            { // No appropriate context request for layer; see if it needs one
              if (covers_composition && (layer_idx == 0) &&
                  jpx_in.access_compatibility().is_jp2_compatible())
                {
                  for (c=0; (rg=tmp_roi.codestreams.access_range(c))!=NULL; c++)
                    if (rg->from == 0)
                      break;
                }
            }
          if (rg == NULL)
            { // No response for this frame layer; must downgrade
              compatible = false;
              frame_idx = 0;
              frame_start = frame_end = 0.0;
              frame = NULL;
              single_layer_idx = 0; // Try this one
              break;
            }
        }
    }

  if (compatible)
    return true;

  bool have_codestream0 = false;
  for (c=0; (rg=tmp_roi.codestreams.access_range(c)) != NULL; c++)
    {
      if (rg->from == 0)
        have_codestream0 = true;
      if (rg->context_type == KDU_JPIP_CONTEXT_TRANSLATED)
        { // Use the compositing layer translated here
          int range_idx = rg->remapping_ids[0];
          int member_idx = rg->remapping_ids[1];
          rg = tmp_roi.contexts.access_range(range_idx);
          if (rg == NULL)
            continue;
          single_layer_idx = rg->from + rg->step*member_idx;
          if (single_layer_idx > rg->to)
            continue;
          return false; // Found a suitable compositing layer
        }
    }

  if (have_codestream0 && jpx_in.access_compatibility().is_jp2_compatible())
    {
      single_layer_idx = 0;
      return false; // Compositing layer 0 is suitable
    }

  // If we get here, we could not find a compatible layer
  single_layer_idx = -1;
  single_component_idx = 0;
  if ((rg=tmp_roi.codestreams.access_range(0)) != NULL)
    single_codestream_idx = rg->from;
  else
    single_codestream_idx = 0;

  return compatible;
}

/******************************************************************************/
/*              CKdu_showApp::update_client_window_of_interest                */
/******************************************************************************/

void
  CKdu_showApp::update_client_window_of_interest()
{
  if ((compositor == NULL) || (!jpip_client.is_alive()) ||
      jpip_client.is_one_time_request())
    return;

  // Begin by trying to find the location, size and resolution of the
  // view window.  We might have to guess at these parameters here if we
  // do not currently have an open image.
  kdu_dims res_dims, roi_dims;
  if (!image_dims.is_empty())
    {
      res_dims = image_dims;
      res_dims.from_apparent(transpose,vflip,hflip);
    }
  else
    { // Try to guess what the image dimensions would be at the current scale
      if (single_component_idx >= 0)
        { // Use size of the codestream, if we can determine it
          if (!jpx_in.exists())
            {
              kdrc_stream *sref =
                compositor->get_next_codestream(NULL,false,false);
              compositor->access_codestream(sref).get_dims(-1,res_dims);
            }
          else
            {
              jpx_codestream_source stream =
                jpx_in.access_codestream(single_codestream_idx);
              if (stream.exists())
                res_dims.size = stream.access_dimensions().get_size();
            }
        }
      else if (single_layer_idx >= 0)
        { // Use size of compositing layer, if we can evaluate it
          if (!jpx_in.exists())
            {
              kdrc_stream *sref =
                compositor->get_next_codestream(NULL,false,false);
              if (sref != NULL)
                {
                  compositor->access_codestream(sref).get_dims(-1,res_dims);
                  res_dims.pos = kdu_coords(0,0);
                }
            }
          else
            {
              jpx_layer_source layer = jpx_in.access_layer(single_layer_idx);
              if (layer.exists())
                res_dims.size = layer.get_layer_size();
            }
        }
      else
        { // Use size of composition surface, if we can find it
          if (composition_rules.exists())
            composition_rules.get_global_info(res_dims.size);
        }
      res_dims.pos = kdu_coords(0,0);
      res_dims.size.x = (int) ceil(res_dims.size.x * rendering_scale);
      res_dims.size.y = (int) ceil(res_dims.size.y * rendering_scale);
    }

  if (enable_focus_box && !focus_box.is_empty())
    {
      roi_dims = focus_box;
      roi_dims.from_apparent(transpose,vflip,hflip);
    }
  else if (focus_anchors_known && !res_dims.is_empty())
    {
      res_dims.to_apparent(transpose,vflip,hflip);
      convert_focus_anchors_to_region(roi_dims,res_dims);
      roi_dims.from_apparent(transpose,vflip,hflip);
      res_dims.from_apparent(transpose,vflip,hflip);
    }
  else if (!view_dims.is_empty())
    {
      roi_dims = view_dims;
      roi_dims.from_apparent(transpose,vflip,hflip);
    }
  else
    roi_dims = res_dims;
  if (roi_dims.is_empty())
    roi_dims = res_dims;
  roi_dims &= res_dims;

  // Now, we have as much information as we can guess concerning the
  // intended view window.  Let's use it to create a window request.
  tmp_roi.init();
  roi_dims.pos -= res_dims.pos; // Make relative to current resolution
  tmp_roi.resolution = res_dims.size;
  tmp_roi.region = roi_dims;
  tmp_roi.round_direction = 0;

  if (configuration_complete && (max_display_layers < (1<<16)) &&
      (max_display_layers < compositor->get_max_available_quality_layers()))
    tmp_roi.max_layers = max_display_layers;

  if (single_component_idx >= 0)
    {
      tmp_roi.components.add(single_component_idx);
      tmp_roi.codestreams.add(single_codestream_idx);
    }
  else if (single_layer_idx >= 0)
    { // single compositing layer
      if ((single_layer_idx == 0) &&
          ((!jpx_in) || jpx_in.access_compatibility().is_jp2_compatible()))
        tmp_roi.codestreams.add(0); // Maximize chance of success even if
                                    // server cannot translate contexts
      kdu_sampled_range rg;
      rg.from = rg.to = single_layer_idx;
      rg.context_type = KDU_JPIP_CONTEXT_JPXL;
      tmp_roi.contexts.add(rg);
    }
  else if (frame != NULL)
    { // Request whole animation frame
      kdu_dims scaled_roi_dims;
      if (configuration_complete &&
          !(roi_dims.is_empty() || res_dims.is_empty()))
        {
          kdu_coords comp_size; composition_rules.get_global_info(comp_size);
          double scale_x = ((double) comp_size.x) / ((double) res_dims.size.x);
          double scale_y = ((double) comp_size.y) / ((double) res_dims.size.y);
          kdu_coords min = roi_dims.pos;
          kdu_coords lim = min + roi_dims.size;
          min.x = (int) floor(scale_x * min.x);
          lim.x = (int) ceil(scale_x * lim.x);
          min.y = (int) floor(scale_y * min.y);
          lim.y = (int) ceil(scale_y * lim.y);
          scaled_roi_dims.pos = min;
          scaled_roi_dims.size = lim - min;
        }
      kdu_sampled_range rg;
      frame_expander.construct(&jpx_in,frame,frame_iteration,true,
                               scaled_roi_dims);
      int m, num_members = frame_expander.get_num_members();
      for (m=0; m < num_members; m++)
        {
          int instruction_idx, layer_idx;
          bool covers_composition;
          kdu_dims source_dims, target_dims;
          jx_frame *frame =
            frame_expander.get_member(m,instruction_idx,layer_idx,
                                      covers_composition,source_dims,
                                      target_dims);
          rg.from = rg.to = layer_idx;
          rg.context_type = KDU_JPIP_CONTEXT_JPXL;
          if (covers_composition)
            {
              if ((layer_idx == 0) && (num_members == 1) &&
                  jpx_in.access_compatibility().is_jp2_compatible())
                tmp_roi.codestreams.add(0); // Maximize chance that server will
                      // respond correctly, even if it can't translate contexts
              rg.remapping_ids[0] = rg.remapping_ids[1] = -1;
            }
          else
            {
              composition_rules.get_original_iset(frame,instruction_idx,
                                                  rg.remapping_ids[0],
                                                  rg.remapping_ids[1]);
            }
          tmp_roi.contexts.add(rg);
        }
    }

  // Post the window
  if (jpip_client.post_window(&tmp_roi))
    {
      client_roi.copy_from(tmp_roi);
      if (last_transmission_start == 0)
        { // Server was idle; start timing server responses again
          last_transmission_start = clock();
        }
    }
}

/******************************************************************************/
/*                     CKdu_showApp::change_client_focus                      */
/******************************************************************************/

void
  CKdu_showApp::change_client_focus(kdu_coords actual_resolution,
                                    kdu_dims actual_region)
{
  // Find the relative location and dimensions of the new focus box
  focus_centre_x =
    (((float) actual_region.pos.x) + 0.5F*((float) actual_region.size.x)) /
    ((float) actual_resolution.x);
  focus_centre_y =
    (((float) actual_region.pos.y) + 0.5F*((float) actual_region.size.y)) /
    ((float) actual_resolution.y);
  focus_size_x = ((float) actual_region.size.x) / ((float) actual_resolution.x);
  focus_size_y = ((float) actual_region.size.y) / ((float) actual_resolution.y);

  // Correct for geometric transformations
  if (transpose)
    {
      float tmp=focus_size_x; focus_size_x=focus_size_y; focus_size_y=tmp;
      tmp=focus_centre_x; focus_centre_x=focus_centre_y; focus_centre_y=tmp;
    }
  if (image_dims.pos.x < 0)
    focus_centre_x = 1.0F - focus_centre_x;
  if (image_dims.pos.y < 0)
    focus_centre_y = 1.0F - focus_centre_y;

  // Draw the focus box from its relative coordinates.
  focus_anchors_known = true;
  enable_focus_box = true;
  bool posting_state = enable_region_posting;
  enable_region_posting = false;
  update_focus_box();
  enable_region_posting = posting_state;
}

/******************************************************************************/
/*                       CKdu_showApp::set_focus_box                          */
/******************************************************************************/

void
  CKdu_showApp::set_focus_box(kdu_dims new_box)
{
  if ((compositor == NULL) || (buffer == NULL))
    return;
  new_box.pos += view_dims.pos;
  CDC *dc = child_wnd->GetDC();
  kdu_dims previous_box = focus_box;
  bool previous_enable = enable_focus_box;
  if (new_box.area() < (kdu_long) 100)
    enable_focus_box = false;
  else
    { focus_box = new_box; enable_focus_box = true; }
  focus_box = get_new_focus_box();
  enable_focus_box = (focus_box != view_dims);
  if (previous_enable != enable_focus_box)
    paint_region(dc,view_dims);
  else if (enable_focus_box)
    {
      paint_region(dc,previous_box);
      paint_region(dc,focus_box);
    }
  child_wnd->ReleaseDC(dc);
  if (jpip_client.is_alive())
    update_client_window_of_interest();
}

/******************************************************************************/
/*                      CKdu_showApp::get_new_focus_box                       */
/******************************************************************************/

kdu_dims
  CKdu_showApp::get_new_focus_box()
{
  kdu_dims new_focus_box = focus_box;
  if ((!enable_focus_box) || (!focus_box))
    new_focus_box = view_dims;
  if (enable_focus_box && focus_anchors_known)
    { // Generate new focus box dimensions from anchor coordinates.
      convert_focus_anchors_to_region(new_focus_box,image_dims);
      focus_anchors_known = false;
    }

  // Adjust box to fit into view port.
  if (new_focus_box.pos.x < view_dims.pos.x)
    new_focus_box.pos.x = view_dims.pos.x;
  if ((new_focus_box.pos.x+new_focus_box.size.x) >
      (view_dims.pos.x+view_dims.size.x))
    new_focus_box.pos.x = view_dims.pos.x+view_dims.size.x-new_focus_box.size.x;
  if (new_focus_box.pos.y < view_dims.pos.y)
    new_focus_box.pos.y = view_dims.pos.y;
  if ((new_focus_box.pos.y+new_focus_box.size.y) >
      (view_dims.pos.y+view_dims.size.y))
    new_focus_box.pos.y = view_dims.pos.y+view_dims.size.y-new_focus_box.size.y;
  new_focus_box &= view_dims;

  if (!new_focus_box)
    new_focus_box = view_dims;
  return new_focus_box;
}

/******************************************************************************/
/*                       CKdu_showApp::update_focus_box                       */
/******************************************************************************/

void
  CKdu_showApp::update_focus_box()
{
  kdu_dims new_focus_box = get_new_focus_box();
  bool new_focus_enable = (new_focus_box != view_dims);
  if ((new_focus_box != focus_box) || (new_focus_enable != enable_focus_box))
    {
      bool previous_enable = enable_focus_box;
      kdu_dims previous_box = focus_box;
      focus_box = new_focus_box;
      enable_focus_box = new_focus_enable;
      CDC *dc = child_wnd->GetDC();
      if (previous_enable != enable_focus_box)
        paint_region(dc,view_dims);
      else if (enable_focus_box)
        {
          paint_region(dc,previous_box);
          paint_region(dc,focus_box);
        }
      child_wnd->ReleaseDC(dc);
    }
  if (enable_region_posting)
    update_client_window_of_interest();
}

/******************************************************************************/
/*                      CKdu_showApp::outline_focus_box                       */
/******************************************************************************/

void
  CKdu_showApp::outline_focus_box(CDC *dc)
{
  if ((!enable_focus_box) || (focus_box == view_dims))
    return;
  kdu_coords box_min = focus_box.pos - view_dims.pos;
  kdu_coords box_lim = box_min + focus_box.size;
  POINT pt;
  dc->SelectObject(focus_pen);
  dc->SetBkColor(0x00FFFF00);
  pt.x = box_min.x*pixel_scale; pt.y = box_min.y*pixel_scale; dc->MoveTo(pt);
  pt.y = (box_lim.y-1)*pixel_scale; dc->LineTo(pt);
  pt.x = (box_lim.x-1)*pixel_scale; dc->LineTo(pt);
  pt.y = box_min.y*pixel_scale; dc->LineTo(pt);
  pt.x = box_min.x*pixel_scale; dc->LineTo(pt);
}

/******************************************************************************/
/*                            CKdu_showApp::OnIdle                            */
/******************************************************************************/

BOOL CKdu_showApp::OnIdle(LONG lCount) 
  /* Note: this function implements a very simple heuristic for scheduling
     regions for processing.  It is intended primarily to demonstrate
     capabilities of the underlying framework -- in particular the
     `kdu_region_compositor' object and the rest of the Kakadu JPEG2000
     platform on which it is built. */
{
  if (processing_suspended)
    return CWinApp::OnIdle(lCount);
  if (in_idle)
    return FALSE;
  if (compositor == NULL)
    {
      if (jpip_client.is_active())
        open_file(NULL); // Try to complete the client source opening operation.
      display_status();
      return FALSE; // Don't need to be called from recursive loop again.
    }
  in_idle = true;

  if (jpip_client.is_active() && last_transmission_start &&
      jpip_client.is_idle())
    {
      cumulative_transmission_time += (clock() - last_transmission_start);
      last_transmission_start = 0;
    }

  double time=0.0, frame_seconds_left=-1.0;
  if (in_playmode)
    { // See if we are ready to display a new processed frame
      int track_idx = -1;
      if (frame != NULL)
        track_idx = 0;
      else if (mj2_in.exists() && (single_component_idx < 0))
        track_idx = single_layer_idx+1;

      time = playstart_instant +
        ((double)(clock()-playclock_base))*(1.0/CLOCKS_PER_SEC);

      int display_frame_idx;
      kdu_long stamp=0;
      bool queue_exists =
        compositor->inspect_composition_queue(0,&stamp,&display_frame_idx);
      double timestamp = ((double) stamp)*0.001; // Time when frame display ends
      frame_seconds_left = timestamp - time;
      if (queue_exists && (frame_seconds_left <= 0.0) &&
          compositor->inspect_composition_queue(1,&stamp,&display_frame_idx))
        {
          double nominal_period = timestamp - last_nominal_time;
          double actual_period = time - last_actual_time;
          if ((last_actual_time < 0.0) || (last_nominal_time < 0.0))
            nominal_period = actual_period = -1.0;

          compositor->pop_composition_buffer();
          buffer = compositor->get_composition_bitmap(buffer_dims);
          CDC *dc = child_wnd->GetDC();
          paint_region(dc,view_dims);
          child_wnd->ReleaseDC(dc);

          playcontrol->update(track_idx,num_frames,display_frame_idx,-1.0F,
                              (float) nominal_period,
                              (float) actual_period);
          if ((time - timestamp) > 0.5)
            { // Adjust playclock_base to avoid playing catch up for lags
              // which exceed half a second
              double delta = time-timestamp-0.5;
              playclock_base += (clock_t)(delta*CLOCKS_PER_SEC);
              time -= delta;
            }
          last_nominal_time = timestamp;
          last_actual_time = time;
          in_idle = false;
          return TRUE; // Give the message queue another go
        }
      else if (queue_exists && (frame_seconds_left <= 0.0) && pushed_last_frame)
        {
          stop_playmode();
          in_idle = false;
          return TRUE;
        }
      if (queue_exists)
        playcontrol->update(track_idx,num_frames,display_frame_idx,
                            (float) frame_seconds_left);
    }

  kdu_dims new_region;
  bool need_more_processing = (configuration_complete && (buffer != NULL));
  try {
      if (need_more_processing && !compositor->process(16384,new_region))
        {
          need_more_processing = false;
          if (!compositor->get_total_composition_dims(image_dims))
            { // Must have invalid scale
              compositor->flush_composition_queue();
              buffer = NULL;
              initialize_regions(); // This call will find a satisfactory scale
              in_idle = false;
              return FALSE;
            }
          else if (in_playmode && !pushed_last_frame)
            { // See if we should push this frame onto the queue
              double adjusted_end_instant, adjusted_start_instant;
              if (use_native_timing)
                {
                  adjusted_start_instant = frame_start/rate_multiplier;
                  adjusted_end_instant = frame_end/rate_multiplier;
                }
              else
                {
                  adjusted_start_instant = adjusted_end_instant =
                    ((double) frame_idx) / (custom_fps*rate_multiplier);
                  adjusted_end_instant += 1.0 / (custom_fps*rate_multiplier);
                }
              adjusted_start_instant += frame_time_offset;
              adjusted_end_instant += frame_time_offset;
              if ((!compositor->inspect_composition_queue(max_queue_size-1)) &&
                  ((adjusted_start_instant-time) <= max_queue_period))
                {
                  compositor->set_surface_initialization_mode(false);
                       // Avoids wasting time initializing new buffer surfaces
                  compositor->push_composition_buffer(
                    (kdu_long)(adjusted_end_instant*1000),frame_idx);
                  if (!set_frame(frame_idx+1))
                    {
                      if (playmode_repeat)
                        {
                          set_frame(0);
                          frame_time_offset = adjusted_end_instant;
                        }
                      else
                        pushed_last_frame = true;
                    }
                  need_more_processing = true;
                }
            }
        }
    }
  catch (int) { // Problem occurred.  Only safe thing is to close the file.
      close_file();
      in_idle = false;
      return FALSE;
    }

  if ((compositor != NULL) && !compositor->inspect_composition_queue(0))
    { // Paint any newly decompressed region right away.
      new_region &= view_dims; // No point in painting invisible regions.
      if (!new_region.is_empty())
        {
          CDC *dc = child_wnd->GetDC();
          paint_region(dc,new_region);
          child_wnd->ReleaseDC(dc);
        }
    }

  if (need_more_processing)
    {
      in_idle = false;
      return TRUE;
    }

  if (in_playmode)
    { // Set up a timer to make sure we get woken up in time to paint the
      // next frame when it comes due or to update the amount of time left until
      // a frame change, for frames with a long duration.
      kdu_long stamp;
      if (compositor->inspect_composition_queue(0,&stamp))
        {
          double time = playstart_instant +
            ((double)(clock()-playclock_base))*(1.0/CLOCKS_PER_SEC);
          stamp -= (kdu_long)(1000.0*time); // Stamp is already in miliseconds
          if (stamp > 250)
            stamp = 250;
          refresh_timer_id =
            m_pMainWnd->SetTimer(KDU_SHOW_REFRESH_TIMER_IDENT,
                                 (kdu_uint32)(1+stamp),NULL);
        }
      else if (refresh_timer_id != 0)
        {
          m_pMainWnd->KillTimer(refresh_timer_id);
          refresh_timer_id = 0;
        }
    }
  else
    display_status(); // All processing done, keep status up to date

  if (jpip_client.is_active())
    {
      // See if focus box needs to be changed
      if (jpip_client.get_window_in_progress(&tmp_roi))
        {
          if ((client_roi.region != tmp_roi.region) ||
              (client_roi.resolution != tmp_roi.resolution))
            change_client_focus(tmp_roi.resolution,tmp_roi.region);
          client_roi.copy_from(tmp_roi);
        }
    
      // See if cache has been updated.
      int new_bytes = (int)
        (jpip_client.get_transferred_bytes(KDU_MAIN_HEADER_DATABIN) +
         jpip_client.get_transferred_bytes(KDU_TILE_HEADER_DATABIN) +
         jpip_client.get_transferred_bytes(KDU_PRECINCT_DATABIN) +
         jpip_client.get_transferred_bytes(KDU_META_DATABIN));
      if (new_bytes != last_transferred_bytes)
        {
          if (!configuration_complete)
            initialize_regions();
          if ((refresh_timer_id == 0) && !in_playmode)
            refresh_timer_id =
              m_pMainWnd->SetTimer(KDU_SHOW_REFRESH_TIMER_IDENT,1000,NULL);
          last_transferred_bytes = new_bytes;
        }
    }

  if (overlays_enabled && overlay_flashing_enabled &&
      (flash_timer_id == 0) && !in_playmode)
    flash_timer_id =
      m_pMainWnd->SetTimer(KDU_SHOW_FLASH_TIMER_IDENT,800,NULL);

  in_idle = false;
  return CWinApp::OnIdle(lCount); // Give low-level idle processing a go.
}

/******************************************************************************/
/*                          CKdu_showApp::OnAppAbout                          */
/******************************************************************************/

void CKdu_showApp::OnAppAbout()
{
  CAboutDlg aboutDlg;
  aboutDlg.DoModal();
}

/******************************************************************************/
/*                          CKdu_showApp::OnFileOpen                          */
/******************************************************************************/

void CKdu_showApp::OnFileOpen() 
{
  char filename[MAX_PATH];
  char initial_dir[MAX_PATH];
  OPENFILENAME ofn;
  memset(&ofn,0,sizeof(ofn)); ofn.lStructSize = sizeof(ofn);
  strcpy(initial_dir,settings.get_open_save_dir());
  if (*initial_dir == '\0')
    GetCurrentDirectory(MAX_PATH,initial_dir);

  ofn.hwndOwner = m_pMainWnd->GetSafeHwnd();
  ofn.lpstrFilter =
    "JP2-family file (*.jp2, *.jpx, *.jpf, *.mj2)\0*.jp2;*.jpx;*.jpf;*.mj2\0"
    "JP2 files (*.jp2)\0*.jp2\0"
    "JPX files (*.jpx, *.jpf)\0*.jpx;*.jpf\0"
    "MJ2 files (*.mj2)\0*.mj2\0"
    "Uwrapped code-streams (*.j2c, *.j2k)\0*.j2c;*.j2k\0"
    "All files (*.*)\0*.*\0\0";
  ofn.nFilterIndex = settings.get_open_idx();
  if ((ofn.nFilterIndex > 5) || (ofn.nFilterIndex < 1))
    ofn.nFilterIndex = 1;
  ofn.lpstrFile = filename; filename[0] = '\0';
  ofn.nMaxFile = MAX_PATH-1;
  ofn.lpstrTitle = "Open JPEG2000 Compressed Image";
  ofn.lpstrInitialDir = initial_dir;
  ofn.Flags = OFN_FILEMUSTEXIST;
  if (!GetOpenFileName(&ofn))
    return;

  strcpy(initial_dir,filename);
  if ((ofn.nFileOffset > 0) &&
      (initial_dir[ofn.nFileOffset-1] == '/') ||
      (initial_dir[ofn.nFileOffset-1] == '\\'))
    ofn.nFileOffset--;
  initial_dir[ofn.nFileOffset] = '\0';
  settings.set_open_save_dir(initial_dir);
  settings.set_open_idx(ofn.nFilterIndex);
  open_file(filename);
}

/******************************************************************************/
/*                          CKdu_showApp::OnFileClose                         */
/******************************************************************************/

void CKdu_showApp::OnFileClose() 
{
  close_file();
}

/******************************************************************************/
/*                       CKdu_showApp::OnUpdateFileClose                      */
/******************************************************************************/

void CKdu_showApp::OnUpdateFileClose(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) || jpip_client.is_active());
}

/******************************************************************************/
/*                           CKdu_showApp::OnJpipOpen                         */
/******************************************************************************/

void CKdu_showApp::OnJpipOpen() 
{
  char server_name[MAX_PATH];
  char proxy_name[MAX_PATH];
  char request[MAX_PATH];
  char channel_type[15];
  char cache_dir_name[MAX_PATH];

  // Initialize fields
  strncpy(server_name,settings.get_jpip_server(),MAX_PATH);
  server_name[MAX_PATH-1] = '\0';
  strncpy(proxy_name,settings.get_jpip_proxy(),MAX_PATH);
  proxy_name[MAX_PATH-1] = '\0';
  strncpy(request,settings.get_jpip_request(),MAX_PATH);
  request[MAX_PATH-1] = '\0';
  strncpy(channel_type,settings.get_jpip_channel_type(),15);
  channel_type[14] = '\0';
  strncpy(cache_dir_name,settings.get_jpip_cache(),MAX_PATH);
  cache_dir_name[MAX_PATH-1] = '\0';

  // Run dialog
  CJpipOpenDlg open_dlg(server_name,MAX_PATH,request,MAX_PATH,
                        channel_type,15,proxy_name,MAX_PATH,
                        cache_dir_name,MAX_PATH);
  if (open_dlg.DoModal() != IDOK)
    return;

  // Save settings
  settings.set_jpip_server(server_name);
  settings.set_jpip_request(request);
  settings.set_jpip_channel_type(channel_type);
  settings.set_jpip_proxy(proxy_name);
  settings.set_jpip_cache(cache_dir_name);
  settings.save_to_registry(this);

  // Create string.
  char url_string[2*MAX_PATH+30]; // Plenty of space.
  strcpy(url_string,"jpip://");
  strcat(url_string,server_name);
  strcat(url_string,"/");
  strcat(url_string,request);
  open_file(url_string);
}

/******************************************************************************/
/*                       CKdu_showApp::OnFileDisconnect                       */
/******************************************************************************/

void CKdu_showApp::OnFileDisconnect() 
{
  if (jpip_client.is_alive())
    jpip_client.disconnect(true,2000);
}

/******************************************************************************/
/*                     CKdu_showApp::OnUpdateFileDisconnect                   */
/******************************************************************************/

void CKdu_showApp::OnUpdateFileDisconnect(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(jpip_client.is_alive());
}

/******************************************************************************/
/*                        CKdu_showApp::OnFileProperties                      */
/******************************************************************************/

void CKdu_showApp::OnFileProperties() 
{
  if (compositor == NULL)
    return;
  compositor->halt_processing(); // Stops any current processing
  kdrc_stream *str = compositor->get_next_codestream(NULL,true,true);
  if (str == NULL)
    return;
  int c_idx, l_idx;
  kdu_codestream codestream = compositor->access_codestream(str);
  compositor->get_codestream_info(str,c_idx,l_idx);

  kd_message_collector collector;
  collector << "Properties for code-stream " << c_idx << "\n\n";

  // Textualize any comments
  kdu_codestream_comment com;
  const char *comtext;
  while ((comtext = (com = codestream.get_comment(com)).get_text()) != NULL)
    collector << ">>>>> Code-stream comment:\n" << comtext << "\n";

  // Textualize the main code-stream header
  kdu_params *root = codestream.access_siz();
  collector << "<<<<< Main Header >>>>>\n";
  root->textualize_attributes(collector,-1,-1);

  // Textualize the tile headers
  codestream.apply_input_restrictions(0,0,0,0,NULL);
  codestream.change_appearance(false,false,false);
  kdu_dims valid_tiles; codestream.get_valid_tiles(valid_tiles);
  kdu_coords idx;
  for (idx.y=0; idx.y < valid_tiles.size.y; idx.y++)
    for (idx.x=0; idx.x < valid_tiles.size.x; idx.x++)
      {
        kdu_dims tile_dims;
        codestream.get_tile_dims(valid_tiles.pos+idx,-1,tile_dims);
        int tnum = idx.x + idx.y*valid_tiles.size.x;
        collector << "<<<<< Tile " << tnum << " >>>>>"
                  << " Canvas coordinates: "
                  << "y = " << tile_dims.pos.y
                  << "; x = " << tile_dims.pos.x
                  << "; height = " << tile_dims.size.y
                  << "; width = " << tile_dims.size.x << "\n";

        // Open and close each tile in case the code-stream has not yet been
        // fully parsed
        try {
            kdu_tile tile = codestream.open_tile(valid_tiles.pos+idx);
            tile.close();
          }
        catch (int) { // Error occurred during tile open
            close_file();
            return;
          }

        root->textualize_attributes(collector,tnum,tnum);
      }

  // Display the properties.
  CPropertiesDlg properties(codestream,collector.get_text());
  properties.DoModal();
}

/******************************************************************************/
/*                     CKdu_showApp::OnUpdateFileProperties                   */
/******************************************************************************/

void CKdu_showApp::OnUpdateFileProperties(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(compositor != NULL);
}


/******************************************************************************/
/*                          CKdu_showApp::OnViewHflip                         */
/******************************************************************************/

void CKdu_showApp::OnViewHflip()
{
  if (buffer == NULL)
    return;
  hflip = !hflip;
  calculate_view_anchors();
  view_centre_x = 1.0F - view_centre_x;
  if (focus_anchors_known)
    focus_centre_x = 1.0F - focus_centre_x;
  view_dims = kdu_dims(); // Force full window init inside `initialize_regions'
  initialize_regions();
}

/******************************************************************************/
/*                       CKdu_showApp::OnUpdateViewHflip                      */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewHflip(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(buffer != NULL);
}

/******************************************************************************/
/*                          CKdu_showApp::OnViewVflip                         */
/******************************************************************************/

void CKdu_showApp::OnViewVflip() 
{
  if (buffer == NULL)
    return;
  vflip = !vflip;
  calculate_view_anchors();
  view_centre_y = 1.0F - view_centre_y;
  if (focus_anchors_known)
    focus_centre_y = 1.0F - focus_centre_y;
  view_dims = kdu_dims(); // Force full window init inside `initialize_regions'
  initialize_regions();
}

/******************************************************************************/
/*                       CKdu_showApp::OnUpdateViewVflip                      */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewVflip(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(buffer != NULL);
}

/******************************************************************************/
/*                          CKdu_showApp::OnViewRotate                        */
/******************************************************************************/

void CKdu_showApp::OnViewRotate() 
{
  if (buffer == NULL)
    return;
  // Need to effectively add an extra transpose, followed by an extra hflip.
  transpose = !transpose;
  bool tmp = hflip; hflip = vflip; vflip = tmp;
  hflip = !hflip;
  calculate_view_anchors();
  float f_tmp = view_centre_y;
  view_centre_y = view_centre_x;
  view_centre_x = 1.0F-f_tmp;
  if (focus_anchors_known)
    {
      f_tmp = focus_centre_y;
      focus_centre_y = focus_centre_x;
      focus_centre_x = 1.0F-f_tmp;
      f_tmp = focus_size_x; focus_size_x = focus_size_y; focus_size_y = f_tmp;
    }
  view_dims = kdu_dims(); // Force full window init inside `initialize_regions'
  initialize_regions();
}

/******************************************************************************/
/*                       CKdu_showApp::OnUpdateViewRotate                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewRotate(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(buffer != NULL);
}

/******************************************************************************/
/*                      CKdu_showApp::OnViewCounterRotate                     */
/******************************************************************************/

void CKdu_showApp::OnViewCounterRotate() 
{
  if (buffer == NULL)
    return;
  // Need to effectively add an extra transpose, followed by an extra vflip.
  transpose = !transpose;
  bool tmp = hflip; hflip = vflip; vflip = tmp;
  vflip = !vflip;
  calculate_view_anchors();
  float f_tmp = view_centre_x;
  view_centre_x = view_centre_y;
  view_centre_y = 1.0F-f_tmp;
  if (focus_anchors_known)
    {
      f_tmp = focus_centre_x;
      focus_centre_x = focus_centre_y;
      focus_centre_y = 1.0F-f_tmp;
      f_tmp = focus_size_x; focus_size_x = focus_size_y; focus_size_y = f_tmp;
    }
  view_dims = kdu_dims(); // Force full window init inside `initialize_regions'
  initialize_regions();
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateViewCounterRotate                  */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewCounterRotate(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(buffer != NULL);
}

/******************************************************************************/
/*                         CKdu_showApp::OnViewZoomOut                        */
/******************************************************************************/

void CKdu_showApp::OnViewZoomOut() 
{
  if (buffer == NULL)
    return;
  if (rendering_scale == min_rendering_scale)
    return;
  rendering_scale = decrease_scale(rendering_scale);
  calculate_view_anchors();
  view_dims = kdu_dims(); // Force full window init inside `initialize_regions'
  initialize_regions();
}

/******************************************************************************/
/*                      CKdu_showApp::OnUpdateViewZoomOut                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewZoomOut(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((buffer != NULL) &&
                 ((min_rendering_scale < 0.0F) ||
                  (rendering_scale > min_rendering_scale)));
}

/******************************************************************************/
/*                         CKdu_showApp::OnViewZoomIn                         */
/******************************************************************************/

void CKdu_showApp::OnViewZoomIn() 
{
  if (buffer == NULL)
    return;
  rendering_scale = increase_scale(rendering_scale);
  calculate_view_anchors();
  if (focus_anchors_known)
    { view_centre_x = focus_centre_x; view_centre_y = focus_centre_y; }
  view_dims = kdu_dims(); // Force full window init inside `initialize_regions'
  initialize_regions();
}

/******************************************************************************/
/*                      CKdu_showApp::OnUpdateViewZoomIn                      */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewZoomIn(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(buffer != NULL);
}

/******************************************************************************/
/*                      CKdu_showApp::OnViewOptimizescale                     */
/******************************************************************************/

void CKdu_showApp::OnViewOptimizescale()
{
  if ((compositor == NULL) || !configuration_complete)
    return;
  float min_scale = rendering_scale * 0.5F;
  float max_scale = rendering_scale * 2.0F;
  if ((min_rendering_scale > 0.0F) && (min_scale < min_rendering_scale))
    min_scale = min_rendering_scale;
  kdu_dims region_basis = (enable_focus_box)?focus_box:view_dims;
  float new_scale =
    compositor->find_optimal_scale(region_basis,rendering_scale,
                                   min_scale,max_scale);
  if (new_scale == rendering_scale)
    return;
  calculate_view_anchors();
  if (focus_anchors_known)
    { view_centre_x = focus_centre_x; view_centre_y = focus_centre_y; }
  view_dims = kdu_dims(); // Force full window init inside `initialize_regions'
  rendering_scale = new_scale;
  initialize_regions();
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateViewOptimizescale                  */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewOptimizescale(CCmdUI *pCmdUI)
{
  pCmdUI->Enable((compositor != NULL) && configuration_complete);
}

/******************************************************************************/
/*                         CKdu_showApp::OnViewRestore                        */
/******************************************************************************/

void CKdu_showApp::OnViewRestore() 
{
  if (buffer == NULL)
    return;
  calculate_view_anchors();
  if (vflip)
    {
      vflip = false;
      view_centre_y = 1.0F-view_centre_y;
      if (focus_anchors_known)
        focus_centre_y = 1.0F-focus_centre_y;
    }
  if (hflip)
    {
      hflip = false;
      view_centre_x = 1.0F-view_centre_x;
      if (focus_anchors_known)
        focus_centre_x = 1.0F-focus_centre_x;
    }
  if (transpose)
    {
      float tmp;
      transpose = false;
      tmp = view_centre_y;
      view_centre_y = view_centre_x;
      view_centre_x = tmp;
      if (focus_anchors_known)
        {
          tmp = focus_centre_y;
          focus_centre_y = focus_centre_x;
          focus_centre_x = tmp;
          tmp = focus_size_y;
          focus_size_y = focus_size_x;
          focus_size_x = tmp;
        }
    }
  view_dims = kdu_dims(); // Force full window init inside `initialize_regions'
  initialize_regions();
}

/******************************************************************************/
/*                      CKdu_showApp::OnUpdateViewRestore                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewRestore(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((buffer != NULL) && (transpose || vflip || hflip));
}

/******************************************************************************/
/*                        CKdu_showApp::OnViewWiden                           */
/******************************************************************************/

void CKdu_showApp::OnViewWiden() 
{
  RECT rect;
  kdu_dims target_dims;
  kdu_coords limits;

  // Find screen dimensions
  CDC *dc = child_wnd->GetDC();
  limits.x = dc->GetDeviceCaps(HORZRES);
  limits.y = dc->GetDeviceCaps(VERTRES);
  child_wnd->ReleaseDC(dc);

  // Find current window location and dimensions
  m_pMainWnd->GetWindowRect(&rect);
  target_dims.pos.x = rect.left;
  target_dims.pos.y = rect.top;
  target_dims.size.x = rect.right-rect.left;
  target_dims.size.y = rect.bottom-rect.top;

  // Calculate new dimensions
  kdu_coords new_size = target_dims.size;
  new_size.x += new_size.x / 2;
  new_size.y += new_size.y / 2;
  if (new_size.x > limits.x)
    new_size.x = limits.x;
  if (new_size.y > limits.y)
    new_size.y = limits.y;
  target_dims.pos.x -= (new_size.x - target_dims.size.x) / 2;
  target_dims.pos.y -= (new_size.y - target_dims.size.y) / 2;
  target_dims.size = new_size;

  // Make sure new location fits inside window
  if (target_dims.pos.x < 0)
    target_dims.pos.x = 0;
  if (target_dims.pos.y < 0)
    target_dims.pos.y = 0;
  limits -= target_dims.pos + target_dims.size;
  if (limits.x < 0)
    target_dims.pos.x += limits.x;
  if (limits.y < 0)
    target_dims.pos.y += limits.y;

  // Reposition window
  m_pMainWnd->SetWindowPos(NULL,target_dims.pos.x,target_dims.pos.y,
                           target_dims.size.x,target_dims.size.y,
                           SWP_SHOWWINDOW | SWP_NOZORDER);
}

/******************************************************************************/
/*                      CKdu_showApp::OnUpdateViewWiden                       */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewWiden(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((buffer == NULL) ||
                 ((view_dims.size.x < image_dims.size.x) ||
                  (view_dims.size.y < image_dims.size.y)));
}

/******************************************************************************/
/*                        CKdu_showApp::OnViewShrink                          */
/******************************************************************************/

void CKdu_showApp::OnViewShrink() 
{
  RECT rect;
  kdu_coords target_size;

  m_pMainWnd->GetWindowRect(&rect);
  target_size.x = rect.right-rect.left;
  target_size.y = rect.bottom-rect.top;
  target_size.x -= target_size.x / 3;
  target_size.y -= target_size.y / 3;
  if (target_size.x < 100)
    target_size.x = rect.right-rect.left;
  if (target_size.y < 100)
    target_size.y = rect.bottom-rect.top;
  m_pMainWnd->SetWindowPos(NULL,0,0,target_size.x,target_size.y,
                           SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOZORDER);
}

/******************************************************************************/
/*                      CKdu_showApp::OnUpdateViewShrink                      */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewShrink(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(TRUE);
}

/******************************************************************************/
/*                          CKdu_showApp::OnModeFast                          */
/******************************************************************************/

void CKdu_showApp::OnModeFast() 
{
  error_level = 0;
  if (compositor != NULL)
    compositor->set_error_level(error_level);
}

/******************************************************************************/
/*                       CKdu_showApp::OnUpdateModeFast                       */
/******************************************************************************/

void CKdu_showApp::OnUpdateModeFast(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(TRUE);
  pCmdUI->SetCheck(error_level==0);
}

/******************************************************************************/
/*                         CKdu_showApp::OnModeFussy                          */
/******************************************************************************/

void CKdu_showApp::OnModeFussy() 
{
  error_level = 1;
  if (compositor != NULL)
    compositor->set_error_level(error_level);
}

/******************************************************************************/
/*                      CKdu_showApp::OnUpdateModeFussy                       */
/******************************************************************************/

void CKdu_showApp::OnUpdateModeFussy(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(TRUE);
  pCmdUI->SetCheck(error_level==1);
}

/******************************************************************************/
/*                       CKdu_showApp::OnModeResilient                        */
/******************************************************************************/

void CKdu_showApp::OnModeResilient() 
{
  error_level = 2;
  if (compositor != NULL)
    compositor->set_error_level(error_level);
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateModeResilient                      */
/******************************************************************************/

void CKdu_showApp::OnUpdateModeResilient(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(TRUE);
  pCmdUI->SetCheck(error_level==2);
}

/******************************************************************************/
/*                     CKdu_showApp::OnModeResilientSop                       */
/******************************************************************************/

void CKdu_showApp::OnModeResilientSop() 
{
  error_level = 3;
  if (compositor != NULL)
    compositor->set_error_level(error_level);
}

/******************************************************************************/
/*                 CKdu_showApp::OnUpdateModeResilientSop                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateModeResilientSop(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(TRUE);
  pCmdUI->SetCheck(error_level==3);
}

/******************************************************************************/
/*                       CKdu_showApp::OnModeSeekable                         */
/******************************************************************************/

void CKdu_showApp::OnModeSeekable() 
{
  allow_seeking = !allow_seeking;
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateModeSeekable                       */
/******************************************************************************/

void CKdu_showApp::OnUpdateModeSeekable(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(compositor == NULL); // No changes allowed when loaded
  pCmdUI->SetCheck(allow_seeking);
}

/******************************************************************************/
/*                    CKdu_showApp::OnModesSinglethreaded                     */
/******************************************************************************/

void CKdu_showApp::OnModesSinglethreaded()
{
  if (thread_env.exists())
    {
      if (compositor != NULL)
        compositor->halt_processing();
      thread_env.destroy();
      if (compositor != NULL)
        compositor->set_thread_env(NULL,NULL);
    }
  num_threads = 0;
}

/******************************************************************************/
/*                 CKdu_showApp::OnUpdateModesSinglethreaded                  */
/******************************************************************************/

void CKdu_showApp::OnUpdateModesSinglethreaded(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(num_threads==0);
}

/******************************************************************************/
/*                     CKdu_showApp::OnModesMultithreaded                     */
/******************************************************************************/

void CKdu_showApp::OnModesMultithreaded()
{
  if (thread_env.exists())
    return;
  num_threads = 1;
  thread_env.create();
  if (compositor != NULL)
    {
      compositor->halt_processing();
      compositor->set_thread_env(&thread_env,NULL);
    }
}

/******************************************************************************/
/*                  CKdu_showApp::OnUpdateModesMultithreaded                  */
/******************************************************************************/

void CKdu_showApp::OnUpdateModesMultithreaded(CCmdUI *pCmdUI)
{
  if (pCmdUI->m_pMenu != NULL)
    {
      const char *menu_string = "Multi-Threaded";
      char menu_string_buf[80];
      if (num_threads > 0)
        {
          sprintf(menu_string_buf,"Multi-Threaded (%d threads)",num_threads);
          menu_string = menu_string_buf;
        }
      pCmdUI->m_pMenu->ModifyMenu(pCmdUI->m_nID,MF_BYCOMMAND,pCmdUI->m_nID,
                                  menu_string);
    }
  pCmdUI->SetCheck(num_threads > 0);
}

/******************************************************************************/
/*                      CKdu_showApp::OnModesMorethreads                      */
/******************************************************************************/

void CKdu_showApp::OnModesMorethreads()
{
  if (compositor != NULL)
    compositor->halt_processing();
  num_threads++;
  if (thread_env.exists())
    thread_env.destroy();
  thread_env.create();
  for (int k=1; k < num_threads; k++)
    if (!thread_env.add_thread())
      num_threads = k;
  if (compositor != NULL)
    compositor->set_thread_env(&thread_env,NULL);
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateModesMorethreads                   */
/******************************************************************************/

void CKdu_showApp::OnUpdateModesMorethreads(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(TRUE);
}

/******************************************************************************/
/*                      CKdu_showApp::OnModesLessthreads                      */
/******************************************************************************/

void CKdu_showApp::OnModesLessthreads()
{
  if (num_threads == 0)
    return;
  if (compositor != NULL)
    compositor->halt_processing();
  num_threads--;
  if (thread_env.exists())
    thread_env.destroy();
  if (num_threads > 0)
    {
      thread_env.create();
      for (int k=1; k < num_threads; k++)
        if (!thread_env.add_thread())
          num_threads = k;
    }
  if (compositor != NULL)
    compositor->set_thread_env(&thread_env,NULL);
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateModesLessthreads                   */
/******************************************************************************/

void CKdu_showApp::OnUpdateModesLessthreads(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(num_threads > 0);
}

/******************************************************************************/
/*                    CKdu_showApp::OnModesRecommendedThreads                 */
/******************************************************************************/

void CKdu_showApp::OnModesRecommendedThreads()
{
  if (num_threads == num_recommended_threads)
    return;
  if (compositor != NULL)
    compositor->halt_processing();
  num_threads = num_recommended_threads;
  if (thread_env.exists())
    thread_env.destroy();
  if (num_threads > 0)
    {
      thread_env.create();
      for (int k=1; k < num_threads; k++)
        if (!thread_env.add_thread())
          num_threads = k;
    }
  if (compositor != NULL)
    compositor->set_thread_env(&thread_env,NULL);
}

/******************************************************************************/
/*                CKdu_showApp::OnUpdateModesRecommendedThreads               */
/******************************************************************************/

void CKdu_showApp::OnUpdateModesRecommendedThreads(CCmdUI *pCmdUI)
{
  if (pCmdUI->m_pMenu != NULL)
    {
      const char *menu_string = "Recommended: single threaded";
      char menu_string_buf[80];
      if (num_recommended_threads > 0)
        {
          sprintf(menu_string_buf,"Recommended: %d threads",
                  num_recommended_threads);
          menu_string = menu_string_buf;
        }
      pCmdUI->m_pMenu->ModifyMenu(pCmdUI->m_nID,MF_BYCOMMAND,pCmdUI->m_nID,
                                  menu_string);
    }
}

/******************************************************************************/
/*                       CKdu_showApp::OnStatusToggle                         */
/******************************************************************************/

void CKdu_showApp::OnStatusToggle() 
{
  if (status_id == KDS_STATUS_LAYER_RES)
    status_id = KDS_STATUS_DIMS;
  else if (status_id == KDS_STATUS_DIMS)
    status_id = KDS_STATUS_MEM;
  else if (status_id == KDS_STATUS_MEM)
    status_id = KDS_STATUS_CACHE;
  else
    status_id = KDS_STATUS_LAYER_RES;
  display_status();
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateStatusToggle                       */
/******************************************************************************/

void CKdu_showApp::OnUpdateStatusToggle(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(compositor != NULL);
}

/******************************************************************************/
/*                        CKdu_showApp::set_codestream                        */
/******************************************************************************/

void
  CKdu_showApp::set_codestream(int codestream_idx)
{
  calculate_view_anchors();
  if ((single_component_idx<0) && focus_anchors_known && !image_dims.is_empty())
    { // Try to map focus region to this codestream
      kdu_dims region; convert_focus_anchors_to_region(region,image_dims);
      kdu_dims stream_dims, stream_region;
      kdu_long area, max_area=0;
      int instance, best_instance=0;
      for (instance=0;
           !(stream_dims =
             compositor->find_codestream_region(codestream_idx,
                                                instance,true)).is_empty();
           instance++)
        {
          stream_region = stream_dims & region;
          area = stream_region.area();
          if (area > max_area)
            { max_area = area; best_instance = instance; }
        }
      if (max_area == 0)
        focus_anchors_known = enable_focus_box = false;
      else
        {
          stream_dims =
            compositor->find_codestream_region(codestream_idx,
                                               best_instance,false);
          stream_region = stream_dims & region;
          convert_region_to_focus_anchors(stream_region,stream_dims);
          focus_layer = -1;
          focus_codestream = codestream_idx;
          view_centre_x = focus_centre_x; view_centre_y = focus_centre_y;
        }
    }
  single_component_idx = 0;
  single_codestream_idx = codestream_idx;
  single_layer_idx = -1;
  frame = NULL;
  invalidate_configuration();
  initialize_regions();
}

/******************************************************************************/
/*                    CKdu_showApp::set_compositing_layer                     */
/******************************************************************************/

void
  CKdu_showApp::set_compositing_layer(int layer_idx)
{
  calculate_view_anchors();
  if ((frame != NULL) && focus_anchors_known && !image_dims.is_empty())
    { // Try to map focus region to this compositing layer
      kdu_dims region; convert_focus_anchors_to_region(region,image_dims);
      kdu_dims layer_dims, layer_region;
      kdu_long area, max_area=0;
      int instance, best_instance=0;
      for (instance=0;
           !(layer_dims =
             compositor->find_layer_region(layer_idx,
                                           instance,true)).is_empty();
           instance++)
        {
          layer_region = layer_dims & region;
          area = layer_region.area();
          if (area > max_area)
            { max_area = area; best_instance = instance; }
        }
      if (max_area == 0)
        focus_anchors_known = enable_focus_box = false;
      else
        {
          layer_dims =
            compositor->find_layer_region(layer_idx,best_instance,false);
          layer_region = layer_dims & region;
          convert_region_to_focus_anchors(layer_region,layer_dims);
          focus_layer = layer_idx;
          focus_codestream = -1;
          view_centre_x = focus_centre_x; view_centre_y = focus_centre_y;
        }
    }
  int translate_codestream =
    (single_component_idx < 0)?-1:single_codestream_idx;
  if (layer_idx != single_layer_idx)
    {
      frame_idx = 0;
      frame_start = frame_end = 0.0;
    }
  else
    translate_codestream = -1;
  single_component_idx = -1;
  single_layer_idx = layer_idx;
  frame = NULL;
  if (translate_codestream && mj2_in.exists())
    {
      kdu_uint32 trk;
      int frm, fld;
      if (mj2_in.find_stream(translate_codestream,trk,frm,fld) &&
          (trk == (kdu_uint32)(single_layer_idx+1)))
        change_frame_idx(frm);
    }
  invalidate_configuration();
  initialize_regions();
}

/******************************************************************************/
/*                    CKdu_showApp::set_compositing_layer                     */
/******************************************************************************/

void
  CKdu_showApp::set_compositing_layer(kdu_coords point)
{
  if (compositor == NULL)
    return;
  point += view_dims.pos;
  int layer_idx, stream_idx;
  if (compositor->find_point(point,layer_idx,stream_idx) && (layer_idx >= 0))
    set_compositing_layer(layer_idx);
}

/******************************************************************************/
/*                         CKdu_showApp::set_frame                            */
/******************************************************************************/

bool
  CKdu_showApp::set_frame(int new_frame_idx)
{
  if ((compositor == NULL) ||
      (single_component_idx >= 0) ||
      ((single_layer_idx >= 0) && jpx_in.exists()))
    return false;
  if (new_frame_idx < 0)
    new_frame_idx = 0;
  if ((num_frames > 0) && (new_frame_idx >= num_frames))
    new_frame_idx = num_frames-1;
  int old_frame_idx = this->frame_idx;
  if (new_frame_idx == old_frame_idx)
    return false;
  change_frame_idx(new_frame_idx);
  if (frame_idx == old_frame_idx)
    return false;

  if (mj2_in.exists())
    compositor->change_compositing_layer_frame(single_layer_idx,frame_idx);

  invalidate_configuration();
  initialize_regions();
  return true;
}

/******************************************************************************/
/*                      CKdu_showApp::start_playmode                           */
/******************************************************************************/

void
  CKdu_showApp::start_playmode(bool use_native_timing, float custom_fps,
                               float rate_multiplier, bool repeat)
{
  if (compositor == NULL)
    return;
  if ((single_component_idx >= 0) ||
      ((single_layer_idx >= 0) && jpx_in.exists()))
    return;
  
  compositor->flush_composition_queue();

  // Record playmode timing parameters and start the clock running
  this->use_native_timing = use_native_timing;
  this->custom_fps = custom_fps;
  this->rate_multiplier = rate_multiplier;
  if (use_native_timing)
    this->playstart_instant = frame_start/rate_multiplier;
  else
    this->playstart_instant = ((double) frame_idx)/(custom_fps*rate_multiplier);
  playclock_base = clock();
  if (frame_end <= frame_start)
    change_frame_idx(frame_idx); // Generate new `frame_end' time

  playmode_repeat = false;
  last_actual_time = last_nominal_time = -1.0;
  playmode_repeat = repeat;
  in_playmode = true;
  pushed_last_frame = false;
  frame_time_offset = 0.0;

  calculate_view_anchors();
  view_dims = buffer_dims = kdu_dims();
  buffer = NULL;
  if (adjust_focus_anchors())
    { view_centre_x = focus_centre_x; view_centre_y = focus_centre_y; }
  child_wnd->Invalidate();
  child_wnd->set_max_view_size(image_dims.size,pixel_scale);
      // Causes a call back into `set_view_size'.

  // Write message on status bar -- it will not be updated during playmode
  frame_wnd->SetMessageText("\tPlay Mode");
}

/******************************************************************************/
/*                      CKdu_showApp::stop_playmode                           */
/******************************************************************************/

void
  CKdu_showApp::stop_playmode()
{
  if (!in_playmode)
    return;
  playmode_repeat = false;
  pushed_last_frame = false;
  frame_time_offset = 0.0;
  last_actual_time = last_nominal_time = -1.0;
  in_playmode = false;
  if (refresh_timer_id != 0)
    {
      m_pMainWnd->KillTimer(refresh_timer_id);
      refresh_timer_id = 0;
    }

  compositor->set_surface_initialization_mode(true);
  if (compositor != NULL)
    compositor->flush_composition_queue();
  buffer = compositor->get_composition_bitmap(buffer_dims);
  if (!compositor->is_processing_complete())
    compositor->refresh(); // Make sure the surface is properly initialized

  // Repaint based on the current buffer, if any
  CDC *dc = child_wnd->GetDC();
  paint_region(dc,view_dims);
  child_wnd->ReleaseDC(dc);
  display_status();

  if (playcontrol != NULL)
    playcontrol->stop_playmode();
}

/******************************************************************************/
/*                        CKdu_showApp::show_metainfo                         */
/******************************************************************************/

bool
  CKdu_showApp::show_metainfo(kdu_coords point)
{
  if (compositor == NULL)
    {
      kill_metainfo();
      return false;
    }
  point += view_dims.pos;
  int codestream_idx; // Not used here.
  jpx_metanode node;
  if (overlays_enabled)
    node = compositor->search_overlays(point,codestream_idx);
  if ((!node) && jpx_in.exists())
    { // Search for global metadata
      int stream_idx, layer_idx;
      jpx_meta_manager manager = jpx_in.access_meta_manager();
      if (compositor->find_point(point,layer_idx,stream_idx))
        {
          manager.load_matches(1,&stream_idx,(layer_idx<0)?0:1,&layer_idx);
          if (layer_idx >= 0)
            node = manager.enumerate_matches(jpx_metanode(),-1,layer_idx,
                                             false,kdu_dims(),0,true);
          if (!node)
            node = manager.enumerate_matches(jpx_metanode(),stream_idx,-1,
                                             false,kdu_dims(),0,true);
        }
      if (!node)
        node = manager.enumerate_matches(jpx_metanode(),-1,-1,true,
                                         kdu_dims(),0,true);
      if (!node)
        node = manager.enumerate_matches(jpx_metanode(),-1,-1,false,
                                         kdu_dims(),0,true);
    }
  if (!node)
    {
      kill_metainfo();
      return false;
    }

  if (node != metainfo_node)
    { // New node
      metainfo_node = node;
      metainfo_label = NULL;
      int n, num_descendants=0;
      bool is_complete = node.count_descendants(num_descendants);
      metainfo_label = node.get_label(); // In case node is global
      for (n=0; (n < num_descendants) && (metainfo_label == NULL); n++)
        {
          jpx_metanode child = node.get_descendant(n);
          if (child.exists())
            metainfo_label = child.get_label();
          else
            is_complete = false;
        }
      if ((metainfo_label == NULL) && !is_complete)
        metainfo_node = jpx_metanode(NULL); // So we search again next time
    }

  if (metainfo_label != NULL)
    {
      suspend_processing(true);
      metainfo_show.show(metainfo_label,child_wnd,point,view_dims);
    }
  else
    {
      metainfo_show.hide();
      suspend_processing(false);
    }
  return true;
}

/******************************************************************************/
/*                        CKdu_showApp::kill_metainfo                         */
/******************************************************************************/

void
  CKdu_showApp::kill_metainfo()
{
  metainfo_label = NULL;
  metainfo_node = jpx_metanode();
  metainfo_show.hide();
  suspend_processing(false);
}

/******************************************************************************/
/*                        CKdu_showApp::edit_metainfo                         */
/******************************************************************************/

bool
  CKdu_showApp::edit_metainfo(kdu_coords point)
{
  if ((compositor == NULL) || !jpx_in)
    {
      kill_metainfo();
      return false;
    }
  point += view_dims.pos;

  int stream_idx=-1, layer_idx=-1;
  jpx_meta_manager manager = jpx_in.access_meta_manager();
  jpx_metanode roi_node;
  kdms_matching_metalist metalist;
  if (overlays_enabled)
    roi_node = compositor->search_overlays(point,stream_idx);
  if (roi_node.exists())
    metalist.append_node_expanding_numlists_and_free_boxes(roi_node);
  else
    {
      if (compositor->find_point(point,layer_idx,stream_idx))
        manager.load_matches((stream_idx >= 0)?1:0,&stream_idx,
                             (layer_idx >= 0)?1:0,&layer_idx);
      metalist.append_image_wide_nodes(manager,layer_idx,stream_idx);
    }
  if (metalist.get_head() == NULL)
    return false; // Nothing to edit; user needs to add metadata
  
  if (file_server == NULL)
    file_server = new kdms_file_services(open_file_pathname);
  int counted_codestreams=0, counted_compositing_layers=0;
  jpx_in.count_codestreams(counted_codestreams);
  jpx_in.count_compositing_layers(counted_compositing_layers);
  kd_metadata_editor editor(counted_codestreams,
                            counted_compositing_layers,manager,
                            file_server,!jpip_client.is_alive());
  editor.configure(&metalist);
  if (editor.run_modal(this))
    { // Above function invokes `metadata_changed' as required.
      have_unsaved_edits = true;
    }
  kill_metainfo();
  return true;
}

/*****************************************************************************/
/*                     CKdu_showApp::metadata_changed                        */
/*****************************************************************************/

void CKdu_showApp::metadata_changed(bool new_data_only)
{
  if (compositor != NULL)
    compositor->update_overlays(!new_data_only);
  if (metashow != NULL)
    metashow->update_tree();
}


/******************************************************************************/
/*                       CKdu_showApp::OnMultiComponent                       */
/******************************************************************************/

void CKdu_showApp::OnMultiComponent() 
{
  if ((compositor == NULL) ||
      ((single_component_idx < 0) &&
       ((single_layer_idx < 0) || (num_frames == 0) || !jpx_in)))
    return;
  if ((num_frames == 0) || !jpx_in)
    OnCompositingLayer();
  else
    {
      calculate_view_anchors();
      single_component_idx = -1;
      single_layer_idx = -1;
      frame_idx = 0;
      frame_start = frame_end = 0.0;
      frame = NULL; // Let `initialize_regions' try to find it.
      frame_iteration = 0;
    }
  invalidate_configuration();
  initialize_regions();
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateMultiComponent                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateMultiComponent(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) &&
                 ((single_component_idx >= 0) ||
                  ((single_layer_idx >= 0) && (num_frames != 0) &&
                    jpx_in.exists())));
  pCmdUI->SetCheck((compositor != NULL) &&
                   (single_component_idx < 0) &&
                   ((single_layer_idx < 0) || (num_frames == 0) || !jpx_in));
}
	
/******************************************************************************/
/*                    CKdu_showApp::OnCompositingLayer                        */
/******************************************************************************/

void
 CKdu_showApp::OnCompositingLayer() 
{
  if ((compositor == NULL) || (single_layer_idx >= 0))
    return;
  int layer_idx = 0;
  if (frame != NULL)
    { // See if we can find a good layer to go to
      kdu_coords point;
      if (this->enable_focus_box)
        {
          point.x = focus_box.pos.x + (focus_box.size.x>>1);
          point.y = focus_box.pos.y + (focus_box.size.y>>1);
        }
      else if (!view_dims.is_empty())
        {
          point.x = view_dims.pos.x + (view_dims.size.x>>1);
          point.y = view_dims.pos.y + (view_dims.size.y>>1);
        }
      int stream_idx; // Not actually used
      if (!(compositor->find_point(point,layer_idx,stream_idx) &&
            (layer_idx >= 0)))
        layer_idx = 0;
    }
  else if (mj2_in.exists() && (single_component_idx >= 0))
    { // Find layer based on the track to which the single component belongs
      kdu_uint32 trk;
      int frm, fld;
      if (mj2_in.find_stream(single_codestream_idx,trk,frm,fld) && (trk != 0))
        layer_idx = (int)(trk-1);
    }
  set_compositing_layer(layer_idx);
}

/******************************************************************************/
/*                 CKdu_showApp::OnUpdateCompositingLayer                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateCompositingLayer(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && (single_layer_idx < 0));
  pCmdUI->SetCheck((compositor != NULL) && (single_layer_idx >= 0));
}

/******************************************************************************/
/*                        CKdu_showApp::OnTrackNext                           */
/******************************************************************************/

void CKdu_showApp::OnTrackNext()
{
  if ((compositor == NULL) || !mj2_in)
    return;
  if (single_layer_idx < 0)
    OnCompositingLayer();
  else
    {
      single_layer_idx++;
      if ((max_compositing_layer_idx >= 0) &&
          (single_layer_idx > max_compositing_layer_idx))
        single_layer_idx = 0;
      invalidate_configuration();
      initialize_regions();
    }
}

/******************************************************************************/
/*                     CKdu_showApp::OnUpdateTrackNext                        */
/******************************************************************************/

void CKdu_showApp::OnUpdateTrackNext(CCmdUI *pCmdUI)
{
  pCmdUI->Enable((compositor != NULL) && mj2_in.exists());
}

/******************************************************************************/
/*                        CKdu_showApp::OnComponent1                          */
/******************************************************************************/

void CKdu_showApp::OnComponent1() 
{
  if ((compositor == NULL) || (single_component_idx == 0))
    return;
  if (single_component_idx < 0)
    { // See if we can find a good codestream to go to
      kdu_coords point;
      if (this->enable_focus_box)
        {
          point.x = focus_box.pos.x + (focus_box.size.x>>1);
          point.y = focus_box.pos.y + (focus_box.size.y>>1);
        }
      else if (!view_dims.is_empty())
        {
          point.x = view_dims.pos.x + (view_dims.size.x>>1);
          point.y = view_dims.pos.y + (view_dims.size.y>>1);
        }
      int stream_idx, layer_idx;
      if (!compositor->find_point(point,layer_idx,stream_idx))
        stream_idx = 0;
      set_codestream(stream_idx);
    }
  else
    {
      single_component_idx = 0;
      frame = NULL;
      calculate_view_anchors();
      invalidate_configuration();
      initialize_regions();
    }
}

/******************************************************************************/
/*                     CKdu_showApp::OnUpdateComponent1                       */
/******************************************************************************/

void CKdu_showApp::OnUpdateComponent1(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && (single_component_idx != 0));
  pCmdUI->SetCheck((compositor != NULL) && (single_component_idx == 0));
}

/******************************************************************************/
/*                       CKdu_showApp::OnComponentNext                        */
/******************************************************************************/

void CKdu_showApp::OnComponentNext()
{
  if ((compositor == NULL) ||
      ((max_components >= 0) && (single_component_idx >= (max_components-1))))
    return;
  if (single_component_idx < 0)
    { // See if we can find a good codestream to go to
      kdu_coords point;
      if (this->enable_focus_box)
        {
          point.x = focus_box.pos.x + (focus_box.size.x>>1);
          point.y = focus_box.pos.y + (focus_box.size.y>>1);
        }
      else if (!view_dims.is_empty())
        {
          point.x = view_dims.pos.x + (view_dims.size.x>>1);
          point.y = view_dims.pos.y + (view_dims.size.y>>1);
        }
      int stream_idx, layer_idx;
      if (!compositor->find_point(point,layer_idx,stream_idx))
        stream_idx = 0;
      set_codestream(stream_idx);
    }
  else
    {
      single_component_idx++;
      frame = NULL;
      calculate_view_anchors();
      invalidate_configuration();
      initialize_regions();
    }
}

/******************************************************************************/
/*                    CKdu_showApp::OnUpdateComponentNext                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateComponentNext(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) &&
                 ((max_components < 0) ||
                  (single_component_idx < (max_components-1))));
}

/******************************************************************************/
/*                       CKdu_showApp::OnComponentPrev                        */
/******************************************************************************/

void CKdu_showApp::OnComponentPrev() 
{
  if ((compositor == NULL) || (single_component_idx == 0))
    return;
  if (single_component_idx < 0)
    { // See if we can find a good codestream to go to
      kdu_coords point;
      if (this->enable_focus_box)
        {
          point.x = focus_box.pos.x + (focus_box.size.x>>1);
          point.y = focus_box.pos.y + (focus_box.size.y>>1);
        }
      else if (!view_dims.is_empty())
        {
          point.x = view_dims.pos.x + (view_dims.size.x>>1);
          point.y = view_dims.pos.y + (view_dims.size.y>>1);
        }
      int stream_idx, layer_idx;
      if (!compositor->find_point(point,layer_idx,stream_idx))
        stream_idx = 0;
      set_codestream(stream_idx);
    }
  else
    {
      single_component_idx--;
      frame = NULL;
      calculate_view_anchors();
      invalidate_configuration();
      initialize_regions();
    }
}

/******************************************************************************/
/*                    CKdu_showApp::OnUpdateComponentPrev                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateComponentPrev(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && (single_component_idx != 0));
}

/******************************************************************************/
/*                       CKdu_showApp::OnImageNext                            */
/******************************************************************************/

void
  CKdu_showApp::OnImageNext() 
{
  if (compositor == NULL)
    return;
  if (single_component_idx >= 0)
    { // Advance to next code-stream
      if ((max_codestreams > 0) &&
          (single_codestream_idx >= (max_codestreams-1)))
        return;
      single_codestream_idx++;
      single_component_idx = 0;
    }
  else if ((single_layer_idx >= 0) && !mj2_in)
    { // Advance to next compositing layer
      if ((max_compositing_layer_idx >= 0) &&
          (single_layer_idx >= max_compositing_layer_idx))
        return;
      single_layer_idx++;
    }
  else if (single_layer_idx >= 0)
    { // Advance to next frame in track
      if ((num_frames > 0) && (frame_idx == (num_frames-1)))
        return;
      change_frame_idx(frame_idx+1);
    }
  else
    { // Advance to next frame in animation
      if (frame == NULL)
        return; // Still have not been able to open the composition box
      if ((num_frames > 0) && (frame_idx >= (num_frames-1)))
        return;
      change_frame_idx(frame_idx+1);
    }
  calculate_view_anchors();
  invalidate_configuration();
  initialize_regions();
}

/******************************************************************************/
/*                    CKdu_showApp::OnUpdateImageNext                         */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateImageNext(CCmdUI* pCmdUI) 
{
  if (single_component_idx >= 0)
    pCmdUI->Enable((compositor != NULL) &&
                   ((max_codestreams < 0) ||
                    (single_codestream_idx < (max_codestreams-1))));
  else if ((single_layer_idx >= 0) && !mj2_in)
    pCmdUI->Enable((compositor != NULL) &&
                   ((max_compositing_layer_idx < 0) ||
                    (single_layer_idx < max_compositing_layer_idx)));
  else if (single_layer_idx >= 0)
    pCmdUI->Enable((compositor != NULL) &&
                   ((num_frames < 0) || (frame_idx < (num_frames-1))));
  else
    pCmdUI->Enable((compositor != NULL) && (frame != NULL) &&
                   ((num_frames < 0) ||
                    (frame_idx < (num_frames-1))));
}

/******************************************************************************/
/*                       CKdu_showApp::OnImagePrev                            */
/******************************************************************************/

void
  CKdu_showApp::OnImagePrev() 
{
  if (compositor == NULL)
    return;
  if (single_component_idx >= 0)
    { // Go to previous code-stream
      if (single_codestream_idx == 0)
        return;
      single_codestream_idx--;
      single_component_idx = 0;
    }
  else if ((single_layer_idx >= 0) && !mj2_in)
    { // Go to previous compositing layer
      if (single_layer_idx == 0)
        return;
      single_layer_idx--;
    }
  else if (single_layer_idx >= 0)
    { // Go to previous frame in track
      if (frame_idx == 0)
        return;
      change_frame_idx(frame_idx-1);
    }
  else
    { // Go to previous frame
      if ((frame_idx == 0) || (frame == NULL))
        return;
      change_frame_idx(frame_idx-1);
    }
  calculate_view_anchors();
  invalidate_configuration();
  initialize_regions();
}

/******************************************************************************/
/*                    CKdu_showApp::OnUpdateImagePrev                         */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateImagePrev(CCmdUI* pCmdUI) 
{
  if (single_component_idx >= 0)
    pCmdUI->Enable((compositor != NULL) && (single_codestream_idx > 0));
  else if ((single_layer_idx >= 0) && !mj2_in)
    pCmdUI->Enable((compositor != NULL) && (single_layer_idx > 0));
  else if (single_layer_idx >= 0)
    pCmdUI->Enable((compositor != NULL) && (frame_idx > 0));
  else
    pCmdUI->Enable((compositor != NULL) && (frame != NULL) && (frame_idx > 0));
}

/******************************************************************************/
/*                        CKdu_showApp::OnLayersLess                          */
/******************************************************************************/

void CKdu_showApp::OnLayersLess()
{
  if (compositor == NULL)
    return;
  int max_layers = compositor->get_max_available_quality_layers();
  if (max_display_layers > max_layers)
    max_display_layers = max_layers;
  if (max_display_layers <= 1)
    { max_display_layers = 1; return; }
  max_display_layers--;
  if (enable_region_posting)
    update_client_window_of_interest();
  else
    status_id = KDS_STATUS_LAYER_RES;
  compositor->set_max_quality_layers(max_display_layers);
  refresh_display();
  display_status();
}

/******************************************************************************/
/*                     CKdu_showApp::OnUpdateLayersLess                       */
/******************************************************************************/

void CKdu_showApp::OnUpdateLayersLess(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && (max_display_layers > 1));
}

/******************************************************************************/
/*                        CKdu_showApp::OnLayersMore                          */
/******************************************************************************/

void CKdu_showApp::OnLayersMore() 
{
  if (compositor == NULL)
    return;
  int max_layers = compositor->get_max_available_quality_layers();
  bool need_update = (max_display_layers < max_layers);
  max_display_layers++;
  if (max_display_layers >= max_layers)
    max_display_layers = 1<<16;
  if (need_update)
    {
      refresh_display();
      if (enable_region_posting)
        update_client_window_of_interest();
      else
        status_id = KDS_STATUS_LAYER_RES;
      compositor->set_max_quality_layers(max_display_layers);
      display_status();
    }
}

/******************************************************************************/
/*                     CKdu_showApp::OnUpdateLayersMore                       */
/******************************************************************************/

void CKdu_showApp::OnUpdateLayersMore(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) &&
                 (max_display_layers <
                  compositor->get_max_available_quality_layers()));
}

/******************************************************************************/
/*                        CKdu_showApp::OnViewRefresh                         */
/******************************************************************************/

void CKdu_showApp::OnViewRefresh() 
{
  refresh_display();
}

/******************************************************************************/
/*                     CKdu_showApp::OnUpdateViewRefresh                      */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewRefresh(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(compositor != NULL);
}

/******************************************************************************/
/*                         CKdu_showApp::OnFocusOff                           */
/******************************************************************************/

void CKdu_showApp::OnFocusOff() 
{
  if (enable_focus_box)
    {
      kdu_dims empty_box;
      empty_box.size = kdu_coords(0,0);
      set_focus_box(empty_box);
    }
}

/******************************************************************************/
/*                      CKdu_showApp::OnUpdateFocusOff                        */
/******************************************************************************/

void CKdu_showApp::OnUpdateFocusOff(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(enable_focus_box);
}

/******************************************************************************/
/*                     CKdu_showApp::OnFocusHighlighting                      */
/******************************************************************************/

void CKdu_showApp::OnFocusHighlighting()
{
  highlight_focus = !highlight_focus;
  if (enable_focus_box)
    {
      CDC *dc = child_wnd->GetDC();
      paint_region(dc,view_dims);
      child_wnd->ReleaseDC(dc);
    }
}

/******************************************************************************/
/*                  CKdu_showApp::OnUpdateFocusHighlighting                   */
/******************************************************************************/

void CKdu_showApp::OnUpdateFocusHighlighting(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(highlight_focus);
}

/******************************************************************************/
/*                        CKdu_showApp::OnFocusWiden                          */
/******************************************************************************/

void CKdu_showApp::OnFocusWiden()
{
  if (!enable_focus_box)
    return;
  kdu_dims new_box = focus_box;
  new_box.pos.x -= new_box.size.x >> 2;
  new_box.pos.y -= new_box.size.y >> 2;
  new_box.size.x += new_box.size.x >> 1;
  new_box.size.y += new_box.size.y >> 1;
  new_box &= view_dims;
  new_box.pos -= view_dims.pos;
  set_focus_box(new_box);
}

/******************************************************************************/
/*                        CKdu_showApp::OnFocusShrink                         */
/******************************************************************************/

void CKdu_showApp::OnFocusShrink()
{
  kdu_dims new_box = focus_box;
  if (!enable_focus_box)
    new_box = view_dims;
  new_box.pos.x += new_box.size.x >> 3;
  new_box.pos.y += new_box.size.y >> 3;
  new_box.size.x -= new_box.size.x >> 2;
  new_box.size.y -= new_box.size.y >> 2;
  new_box.pos -= view_dims.pos;
  set_focus_box(new_box);
}

/******************************************************************************/
/*                         CKdu_showApp::OnFocusLeft                          */
/******************************************************************************/

void CKdu_showApp::OnFocusLeft()
{
  if (!enable_focus_box)
    return;
  kdu_dims new_box = focus_box;
  new_box.pos.x -= 20;
  if (new_box.pos.x < view_dims.pos.x)
    {
      if (new_box.pos.x < image_dims.pos.x)
        new_box.pos.x = image_dims.pos.x;
      if (new_box.pos.x < view_dims.pos.x)
        set_hscroll_pos(new_box.pos.x-view_dims.pos.x,true);
    }
  new_box.pos -= view_dims.pos;
  set_focus_box(new_box);
}

/******************************************************************************/
/*                        CKdu_showApp::OnFocusRight                          */
/******************************************************************************/

void CKdu_showApp::OnFocusRight()
{
  if (!enable_focus_box)
    return;
  kdu_dims new_box = focus_box;
  new_box.pos.x += 20;
  int new_lim = new_box.pos.x + new_box.size.x;
  int view_lim = view_dims.pos.x + view_dims.size.x;
  if (new_lim > view_lim)
    {
      int image_lim = image_dims.pos.x + image_dims.size.x;
      if (new_lim > image_lim)
        {
          new_box.pos.x -= (new_lim-image_lim);
          new_lim = image_lim;
        }
      if (new_lim > view_lim)
        set_hscroll_pos(new_lim-view_lim,true);
    }
  new_box.pos -= view_dims.pos;
  set_focus_box(new_box);
}

/******************************************************************************/
/*                          CKdu_showApp::OnFocusUp                           */
/******************************************************************************/

void CKdu_showApp::OnFocusUp()
{
  if (!enable_focus_box)
    return;
  kdu_dims new_box = focus_box;
  new_box.pos.y -= 20;
  if (new_box.pos.y < view_dims.pos.y)
    {
      if (new_box.pos.y < image_dims.pos.y)
        new_box.pos.y = image_dims.pos.y;
      if (new_box.pos.y < view_dims.pos.y)
        set_vscroll_pos(new_box.pos.y-view_dims.pos.y,true);
    }
  new_box.pos -= view_dims.pos;
  set_focus_box(new_box);
}

/******************************************************************************/
/*                        CKdu_showApp::OnFocusDown                           */
/******************************************************************************/

void CKdu_showApp::OnFocusDown()
{
  if (!enable_focus_box)
    return;
  kdu_dims new_box = focus_box;
  new_box.pos.y += 20;
  int new_lim = new_box.pos.y + new_box.size.y;
  int view_lim = view_dims.pos.y + view_dims.size.y;
  if (new_lim > view_lim)
    {
      int image_lim = image_dims.pos.y + image_dims.size.y;
      if (new_lim > image_lim)
        {
          new_box.pos.y -= (new_lim-image_lim);
          new_lim = image_lim;
        }
      if (new_lim > view_lim)
        set_vscroll_pos(new_lim-view_lim,true);
    }
  new_box.pos -= view_dims.pos;
  set_focus_box(new_box);
}

/******************************************************************************/
/*                      CKdu_showApp::OnViewMetadata                          */
/******************************************************************************/

void CKdu_showApp::OnViewMetadata() 
{
  if (metashow != NULL)
    return;
  metashow = new kd_metashow(this);
  if (jp2_family_in.exists())
    metashow->activate(&jp2_family_in);
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateViewMetadata                       */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewMetadata(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(metashow == NULL);
}

/******************************************************************************/
/*                    CKdu_showApp::OnViewPlaycontrol                         */
/******************************************************************************/

void CKdu_showApp::OnViewPlaycontrol()
{
  if ((playcontrol != NULL) || jpip_client.is_active())
    return;
  if (!mj2_in)
    {
      if (!composition_rules)
        return;
      if ((frame == NULL) &&
          (composition_rules.get_next_frame(NULL) == NULL))
        return;
    }
  playcontrol = new kd_playcontrol(this);
}

/******************************************************************************/
/*                  CKdu_showApp::OnUpdateViewPlaycontrol                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateViewPlaycontrol(CCmdUI *pCmdUI)
{
  pCmdUI->Enable((playcontrol == NULL) && (!jpip_client.is_active()) &&
                 (mj2_in.exists() ||
                  ((composition_rules.exists()) &&
                   ((frame != NULL) ||
                    (composition_rules.get_next_frame(NULL) != NULL)))));
}

/******************************************************************************/
/*                       CKdu_showApp::OnViewScaleX2                          */
/******************************************************************************/

void CKdu_showApp::OnViewScaleX2()
{
  frame_wnd->select_toolbar(true);
  if (pixel_scale != 2)
    {
      pixel_scale = 2;
      calculate_view_anchors();
      if (focus_anchors_known)
        { view_centre_x = focus_centre_x; view_centre_y = focus_centre_y; }
      child_wnd->Invalidate();
      child_wnd->set_max_view_size(image_dims.size,pixel_scale);
    }
}

/******************************************************************************/
/*                       CKdu_showApp::OnViewScaleX1                          */
/******************************************************************************/

void CKdu_showApp::OnViewScaleX1()
{
  frame_wnd->select_toolbar(false);
  if (pixel_scale != 1)
    {
      pixel_scale = 1;
      child_wnd->Invalidate();
      child_wnd->set_max_view_size(image_dims.size,pixel_scale);
    }
}

/******************************************************************************/
/*                       CKdu_showApp::OnFileSaveAs                           */
/******************************************************************************/

void
  CKdu_showApp::OnFileSaveAs()
{
  if ((compositor == NULL) || in_playmode)
    return;
  save_as(true,false);
}

/******************************************************************************/
/*                    CKdu_showApp::OnUpdateFileSaveAs                        */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateFileSaveAs(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && !in_playmode);
}

/******************************************************************************/
/*                    CKdu_showApp::OnFileSaveAsLinked                        */
/******************************************************************************/

void CKdu_showApp::OnFileSaveAsLinked()
{
  if ((compositor == NULL) || in_playmode)
    return;
  save_as(true,true);
}

/******************************************************************************/
/*                  CKdu_showApp::OnUpdateFileSaveAsLinked                    */
/******************************************************************************/

void CKdu_showApp::OnUpdateFileSaveAsLinked(CCmdUI *pCmdUI)
{
  pCmdUI->Enable((compositor != NULL) && (!in_playmode) &&
                 jp2_family_in.exists() && !jp2_family_in.uses_cache());
}

/******************************************************************************/
/*                   CKdu_showApp::OnFileSaveAsEmbedded                       */
/******************************************************************************/

void CKdu_showApp::OnFileSaveAsEmbedded()
{
  if ((compositor == NULL) || in_playmode)
    return;
  save_as(false,false);
}

/******************************************************************************/
/*                 CKdu_showApp::OnUpdateFileSaveAsEmbedded                   */
/******************************************************************************/

void CKdu_showApp::OnUpdateFileSaveAsEmbedded(CCmdUI *pCmdUI)
{
  pCmdUI->Enable((compositor != NULL) && !in_playmode);
}

/******************************************************************************/
/*                      CKdu_showApp::OnFileSaveReload                        */
/******************************************************************************/

void CKdu_showApp::OnFileSaveReload()
{
  if ((compositor == NULL) || in_playmode || mj2_in.exists() ||
      (!have_unsaved_edits) || (open_file_pathname == NULL))
    return;
  if (!save_without_asking)
    {
      int response =
        frame_wnd->MessageBox("You are about to save to the file which you are "
                              "currently viewing.  In the process, "
                              "there is a chance that you might lose some "
                              "metadata which could not be internally "
                              "represented.  Do you want to be asked this "
                              "question again in the future?",
                              "Kdu_show: About to overwrite file",
                              MB_YESNOCANCEL | MB_ICONWARNING);
      if (response == IDCANCEL)
        return;
      if (response == IDNO)
        save_without_asking = true;
    }
  if (!save_over())
    return;
  char *filename = new char[strlen(open_file_pathname)+1];
  strcpy(filename,open_file_pathname);
  bool save_without_asking = this->save_without_asking;
  open_file(filename);
  this->save_without_asking = save_without_asking;
  delete[] filename;
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateFileSaveReload                     */
/******************************************************************************/

void CKdu_showApp::OnUpdateFileSaveReload(CCmdUI *pCmdUI)
{
  pCmdUI->Enable((compositor != NULL) && have_unsaved_edits &&
                 (open_file_pathname != NULL) && (!mj2_in.exists()) &&
                 !in_playmode);
}

/******************************************************************************/
/*                         CKdu_showApp::OnMetaAdd                            */
/******************************************************************************/

void
  CKdu_showApp::OnMetaAdd() 
{
  if ((compositor == NULL) || (!jpx_in) || jpip_client.is_alive())
    return;
  if (file_server == NULL)
    file_server = new kdms_file_services(open_file_pathname);
  int counted_codestreams=1, counted_compositing_layers=1;
  jpx_in.count_codestreams(counted_codestreams);
  jpx_in.count_compositing_layers(counted_compositing_layers);
  jpx_meta_manager manager = jpx_in.access_meta_manager();
  kd_metadata_editor editor(counted_codestreams,
                            counted_compositing_layers,manager,
                            file_server,true);
  int initial_codestream_idx=0;
  kdu_dims initial_region = focus_box;
  if (enable_focus_box &&
      compositor->map_region(initial_codestream_idx,initial_region))
    editor.configure(initial_region,initial_codestream_idx,-1,false);
  else if (single_component_idx >= 0)
    editor.configure(kdu_dims(),single_codestream_idx,-1,false);
  else if (single_layer_idx >= 0)
    editor.configure(kdu_dims(),-1,single_layer_idx,false);
  else
    editor.configure(kdu_dims(),-1,-1,true);
  if (editor.run_modal(this))
    { // Above function invokes `metadata_changed' as required.
      have_unsaved_edits = true;
    }
}

/******************************************************************************/
/*                      CKdu_showApp::OnUpdateMetaAdd                         */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateMetaAdd(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && jpx_in.exists() &&
                 !jpip_client.is_alive());
}


/******************************************************************************/
/*                     CKdu_showApp::OnOverlayEnabled                         */
/******************************************************************************/

void
  CKdu_showApp::OnOverlayEnabled() 
{
  if ((compositor == NULL) || !jpx_in)
    return;
  overlays_enabled = !overlays_enabled;
  overlay_flashing_enabled = false;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
    (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));
}

/******************************************************************************/
/*                  CKdu_showApp::OnUpdateOverlayEnabled                      */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateOverlayEnabled(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck((compositor != NULL) && jpx_in.exists() &&
                   overlays_enabled);
  pCmdUI->Enable((compositor != NULL) && jpx_in.exists());
}

/******************************************************************************/
/*                     CKdu_showApp::OnOverlayFlashing                        */
/******************************************************************************/

void
  CKdu_showApp::OnOverlayFlashing() 
{
  if ((compositor == NULL) || (!jpx_in) || !overlays_enabled)
    return;
  overlay_flashing_enabled = !overlay_flashing_enabled;
  if (!overlay_flashing_enabled)
    overlay_painting_colour = 0;
  else
    overlay_painting_colour++;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
           (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));
}

/******************************************************************************/
/*                  CKdu_showApp::OnUpdateOverlayFlashing                     */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateOverlayFlashing(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck((compositor != NULL) && jpx_in.exists() &&
                   overlay_flashing_enabled && overlays_enabled);
  pCmdUI->Enable((compositor != NULL) && jpx_in.exists() && overlays_enabled);
}

/******************************************************************************/
/*                      CKdu_showApp::OnOverlayToggle                         */
/******************************************************************************/

void
  CKdu_showApp::OnOverlayToggle() 
{
  if ((compositor == NULL) || !jpx_in)
    return;
  if (!overlays_enabled)
    {
      overlays_enabled = overlay_flashing_enabled = true;
      overlay_painting_colour = 0;
    }
  else if (overlay_flashing_enabled)
    {
      overlay_flashing_enabled = false;
      overlay_painting_colour++;
    }
  else
    overlays_enabled = false;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
           (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateOverlayToggle                      */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateOverlayToggle(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && jpx_in.exists());
}

/******************************************************************************/
/*                      CKdu_showApp::OnOverlayDarker                         */
/******************************************************************************/

void
  CKdu_showApp::OnOverlayDarker() 
{
  if ((compositor == NULL) || (!jpx_in) || (!overlays_enabled) ||
      (overlay_painting_intensity == -128))
    return;
  overlay_painting_intensity--;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
           (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));
}

/******************************************************************************/
/*                   CKdu_showApp::OnUpdateOverlayDarker                      */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateOverlayDarker(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && jpx_in.exists() && overlays_enabled);
}

/******************************************************************************/
/*                     CKdu_showApp::OnOverlayBrighter                        */
/******************************************************************************/

void
  CKdu_showApp::OnOverlayBrighter() 
{
  if ((compositor == NULL) || (!jpx_in) || (!overlays_enabled) ||
      (overlay_painting_intensity == 127))
    return;
  overlay_painting_intensity++;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
           (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));
}

/******************************************************************************/
/*                  CKdu_showApp::OnUpdateOverlayBrighter                     */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateOverlayBrighter(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && jpx_in.exists() && overlays_enabled);
}

/******************************************************************************/
/*                    CKdu_showApp::OnOverlayDoublesize                       */
/******************************************************************************/

void
  CKdu_showApp::OnOverlayDoublesize() 
{
  if ((compositor == NULL) || (!jpx_in) || (!overlays_enabled) ||
      (overlay_size_threshold & 0x40000000))
    return;
  overlay_size_threshold <<= 1;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
           (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));
}

/******************************************************************************/
/*                 CKdu_showApp::OnUpdateOverlayDoublesize                    */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateOverlayDoublesize(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && jpx_in.exists() && overlays_enabled);  
}

/******************************************************************************/
/*                    CKdu_showApp::OnOverlayHalvesize                        */
/******************************************************************************/

void
  CKdu_showApp::OnOverlayHalvesize() 
{
  if ((compositor == NULL) || (!jpx_in) || (!overlays_enabled) ||
      (overlay_size_threshold == 1))
    return;
  overlay_size_threshold >>= 1;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
           (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));
}

/******************************************************************************/
/*                 CKdu_showApp::OnUpdateOverlayHalvesize                     */
/******************************************************************************/

void
  CKdu_showApp::OnUpdateOverlayHalvesize(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((compositor != NULL) && jpx_in.exists() && overlays_enabled &&
                 (overlay_size_threshold > 1));
}

/******************************************************************************/
/*                   CKdu_showApp::get_save_file_pathname                     */
/******************************************************************************/

const char *
  CKdu_showApp::get_save_file_pathname(const char *path)
{
  if ((open_file_pathname != NULL) &&
      compare_file_pathnames(open_file_pathname,path))
    {
      if (oversave_pathname == NULL)
        {
          oversave_pathname = new char[strlen(path)+2];
          strcpy(oversave_pathname,path);
          strcat(oversave_pathname,"~");
        }
      return oversave_pathname;
    }
  return path;
}

/******************************************************************************/
/*                 CKdu_showApp::declare_save_file_invalid                    */
/******************************************************************************/

void
  CKdu_showApp::declare_save_file_invalid(const char *path)
{
  if ((open_file_pathname != NULL) &&
      compare_file_pathnames(open_file_pathname,path))
    return; // Safety measure to prevent deleting open file; shouldn't happen
  else if ((oversave_pathname != NULL) &&
           compare_file_pathnames(oversave_pathname,path))
    {
      delete[] oversave_pathname;
      oversave_pathname = NULL;
    }
  remove(path); 
}

/*****************************************************************************/
/*                        CKdu_showApp::save_over                            */
/*****************************************************************************/

bool
  CKdu_showApp::save_over()
{
  const char *save_pathname =
    get_save_file_pathname(open_file_pathname);
  const char *suffix = strrchr(open_filename,'.');
  bool result = true;
  BeginWaitCursor();
  try { // Protect the entire file writing process against exceptions
    if ((suffix != NULL) &&
        (toupper(suffix[1]) == 'J') && (toupper(suffix[2]) == 'P') &&
        ((toupper(suffix[3]) == 'X') || (toupper(suffix[3]) == 'F') ||
         (suffix[3] == '2')))
      { // Generate a JP2 or JPX file.
        if (!(jpx_in.exists() || mj2_in.exists()))
          { kdu_error e; e << "Cannot write JP2-family file if input data "
            "source is a raw codestream.  Too much information must "
            "be invented.  Probably the file you opened had a \".jp2\", "
            "\".jpx\" or \".jpf\" suffix but was actually a raw codestream "
            "not embedded within any JP2 family file structure.  Use "
            "the \"Save As\" option to save it as a codestream again if "
            "you like."; }
        if ((suffix[3] == '2') || !jpx_in)
          save_as_jp2(save_pathname);
        else
          {
            save_as_jpx(save_pathname,true,false);
            have_unsaved_edits = false;
          }
      }
    else
      save_as_raw(save_pathname);
    
    if (last_save_pathname != NULL)
      delete[] last_save_pathname;
    last_save_pathname = new char[strlen(open_file_pathname)+1];
    strcpy(last_save_pathname,open_file_pathname);
  }
  catch (int)
  {
    declare_save_file_invalid(save_pathname);
    result = false;
  }
  EndWaitCursor();
  return result;
}

/*****************************************************************************/
/*                         CKdu_showApp::save_as                             */
/*****************************************************************************/

void
  CKdu_showApp::save_as(bool preserve_codestream_links,
                        bool force_codestream_links)
{
  if ((compositor == NULL) || in_playmode)
    return;

  // Get the file name
  OPENFILENAME ofn;
  memset(&ofn,0,sizeof(ofn)); ofn.lStructSize = sizeof(ofn);
  char initial_dir_buf[MAX_PATH+1];
  const char *initial_dir = settings.get_open_save_dir();
  if ((initial_dir == NULL) || (*initial_dir == '\0'))
    {
      initial_dir = initial_dir_buf;
      GetCurrentDirectory(MAX_PATH,initial_dir_buf);
    }

  char filename[MAX_PATH+5];
  filename[0] = '\0';
  if ((last_save_pathname != NULL) &&
      (strlen(last_save_pathname) < MAX_PATH))
    strcpy(filename,last_save_pathname);
  else if ((open_file_pathname != NULL) &&
           (strlen(open_file_pathname) < MAX_PATH))
    strcpy(filename,open_file_pathname);
  if (mj2_in.exists())
    {
      char *suffix = strrchr(filename,'.');
      if (suffix == NULL)
        *filename = '\0';
      else
        strcpy(suffix,".jp2");
    }
  
  int num_choices = 1;

  ofn.hwndOwner = m_pMainWnd->GetSafeHwnd();
  if (jpx_in.exists())
    {
      ofn.lpstrFilter =
        "JP2 compatible file (*.jp2, *.jpx, *.jpf)\0*.jp2;*.jpx;*.jpf\0"
        "JPX file (*.jpx, *.jpf)\0*.jpx;*.jpf\0"
        "JPEG2000 unwrapped code-stream (*.j2c, *.j2k)\0*.j2c;*.j2k\0\0";
      ofn.lpstrDefExt = "jpf";
      num_choices = 3;
    }
  else if (mj2_in.exists())
    {
      ofn.lpstrFilter =
        "JP2 file (*.jp2)\0*.jp2\0"
        "JPEG2000 unwrapped code-stream (*.j2c, *.j2k)\0*.j2c;*.j2k\0\0";
      ofn.lpstrDefExt = "jp2";
      num_choices = 2;
    }
  else
    {
      ofn.lpstrFilter =
        "JPEG2000 unwrapped code-stream (*.j2c, *.j2k)\0*.j2c;*.j2k\0\0";
      ofn.lpstrDefExt = "j2c";
      num_choices = 1;
    }
  ofn.nFilterIndex = settings.get_save_idx();
  if ((ofn.nFilterIndex < 1) || ((int) ofn.nFilterIndex > num_choices))
    ofn.nFilterIndex = 1;
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  
  ofn.lpstrInitialDir = initial_dir;
  ofn.lpstrTitle = "Save as JPEG2000 File";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
  if (!GetSaveFileName(&ofn))
    return;

  // Adjust `settings' based on the outcome before we do anything else
  if ((ofn.nFileOffset > 0) &&
      (filename[ofn.nFileOffset-1] == '/') ||
      (filename[ofn.nFileOffset-1] == '\\'))
    ofn.nFileOffset--;
  char sep = filename[ofn.nFileOffset];
  filename[ofn.nFileOffset] = '\0';
  settings.set_open_save_dir(filename);
  settings.set_save_idx(ofn.nFilterIndex);
  filename[ofn.nFileOffset] = sep;
  
  // Remember the last saved file name
  const char *chosen_pathname = filename;
  const char *suffix = strrchr(filename,'.');
  bool jp2_suffix = ((suffix != NULL) &&
                     (toupper(suffix[1]) == 'J') &&
                     (toupper(suffix[2]) == 'P') &&
                     (suffix[3] == '2'));
  bool jpx_suffix = ((suffix != NULL) &&
                     (toupper(suffix[1]) == 'J') &&
                     (toupper(suffix[2]) == 'P') &&
                     ((toupper(suffix[3]) == 'X') ||
                      (toupper(suffix[3]) == 'F')));
  
  // Now save the file
  const char *save_pathname = NULL;
  BeginWaitCursor();
  try { // Protect the entire file writing process against exceptions
    bool may_overwrite_link_source = false;
    if (force_codestream_links && jpx_suffix && (open_file_pathname != NULL))
      if (compare_file_pathnames(chosen_pathname,open_file_pathname))
        may_overwrite_link_source = true;
    if (jpx_in.exists() && !jp2_family_in.uses_cache())
      {
        jp2_data_references data_refs;
        if (jpx_in.exists())
          data_refs = jpx_in.access_data_references();
        if (data_refs.exists())
          {
            int u;
            const char *url_name;
            for (u=0; (url_name = data_refs.get_url(u)) != NULL; u++)
              if (compare_file_pathnames(chosen_pathname,url_name))
                may_overwrite_link_source = true;
          }
      }
    if (may_overwrite_link_source)
      { kdu_error e; e << "You are about to overwrite an existing file "
        "which is being linked by (or potentially about to be linked by) "
        "the file you are saving.  This operation is too dangerous to "
        "attempt."; }
    save_pathname = this->get_save_file_pathname(chosen_pathname);
    if (jpx_suffix || jp2_suffix)
      { // Generate a JP2 or JPX file.
        if (!(jpx_in.exists() || mj2_in.exists()))
          { kdu_error e; e << "Cannot write JP2-family file if input data "
            "source is a raw codestream.  Too much information must "
            "be invented."; }
        if (jp2_suffix || !jpx_in)
          save_as_jp2(save_pathname);
        else
          {
            save_as_jpx(save_pathname,preserve_codestream_links,
                        force_codestream_links);
            have_unsaved_edits = false;
          }
      }
    else
      save_as_raw(save_pathname);
    
    if (last_save_pathname != NULL)
      delete[] last_save_pathname;
    last_save_pathname = new char[strlen(chosen_pathname)+1];
    strcpy(last_save_pathname,chosen_pathname);
  }
  catch (int)
  {
    if (save_pathname != NULL)
      this->declare_save_file_invalid(save_pathname);
  }
  EndWaitCursor();
}

/******************************************************************************/
/*                        CKdu_showApp::save_as_jp2                           */
/******************************************************************************/

void
  CKdu_showApp::save_as_jp2(const char *filename)
{
  jp2_family_tgt tgt; 
  jp2_target jp2_out;
  jp2_input_box stream_box;
  int jpx_layer_idx=-1, jpx_stream_idx=-1;
  
  if (jpx_in.exists())
    {
      jpx_layer_idx = 0;
      if (single_layer_idx >= 0)
        jpx_layer_idx = single_layer_idx;
      
      jpx_layer_source layer_in = jpx_in.access_layer(jpx_layer_idx);
      if (!(layer_in.exists() && (layer_in.get_num_codestreams() == 1)))
        { kdu_error e;
          e << "Cannot write JP2 file based on the currently active "
          "compositing layer.  The layer either is not yet available, or "
          "else it uses multiple codestreams.  Try saving as a JPX file.";
        }
      jpx_stream_idx = layer_in.get_codestream_id(0);
      jpx_codestream_source stream_in=jpx_in.access_codestream(jpx_stream_idx);
      
      tgt.open(filename);
      jp2_out.open(&tgt);
      jp2_out.access_dimensions().copy(stream_in.access_dimensions(true));
      jp2_out.access_colour().copy(layer_in.access_colour(0));
      jp2_out.access_palette().copy(stream_in.access_palette());
      jp2_out.access_resolution().copy(layer_in.access_resolution());
      
      jp2_channels channels_in = layer_in.access_channels();
      jp2_channels channels_out = jp2_out.access_channels();
      int n, num_colours = channels_in.get_num_colours();
      channels_out.init(num_colours);
      for (n=0; n < num_colours; n++)
        { // Copy channel information, converting only the codestream ID
          int comp_idx, lut_idx, stream_idx;
          channels_in.get_colour_mapping(n,comp_idx,lut_idx,stream_idx);
          channels_out.set_colour_mapping(n,comp_idx,lut_idx,0);
          if (channels_in.get_opacity_mapping(n,comp_idx,lut_idx,stream_idx))
            channels_out.set_opacity_mapping(n,comp_idx,lut_idx,0);
          if (channels_in.get_premult_mapping(n,comp_idx,lut_idx,stream_idx))
            channels_out.set_premult_mapping(n,comp_idx,lut_idx,0);
        }
      
      jp2_locator stream_loc = stream_in.get_stream_loc();
      stream_box.open(&jp2_family_in,stream_loc);
    }
  else
    {
      assert(mj2_in.exists());
      int frm, fld;
      mj2_video_source *track = NULL;
      if (single_component_idx >= 0)
        { // Find layer based on the track to which single component belongs
          kdu_uint32 trk;
          if (mj2_in.find_stream(single_codestream_idx,trk,frm,fld) &&
              (trk > 0))
            track = mj2_in.access_video_track(trk);
        }
      else
        {
          assert(single_layer_idx >= 0);
          track = mj2_in.access_video_track((kdu_uint32)(single_layer_idx+1));
          frm = frame_idx;
          fld = 0;
        }
      if (track == NULL)
        { kdu_error e; e << "Insufficient information available to open "
          "current video track.  Perhaps insufficient information has been "
          "received so far during a JPIP browsing session.";
        }
      
      tgt.open(filename);
      jp2_out.open(&tgt);
      jp2_out.access_dimensions().copy(track->access_dimensions());
      jp2_out.access_colour().copy(track->access_colour());
      jp2_out.access_palette().copy(track->access_palette());
      jp2_out.access_resolution().copy(track->access_resolution());
      jp2_out.access_channels().copy(track->access_channels());
      
      track->seek_to_frame(frm);
      track->open_stream(fld,&stream_box);
      if (!stream_box)
        { kdu_error e; e << "Insufficient information available to open "
          "relevant video frame.  Perhaps insufficient information has been "
          "received so far during a JPIP browsing session.";
        }
    }
  
  jp2_out.write_header();
  
  if (jpx_in.exists())
    { // See if we can copy some metadata across
      jpx_meta_manager meta_manager = jpx_in.access_meta_manager();
      jpx_metanode scan;
      while ((scan=meta_manager.enumerate_matches(scan,-1,jpx_layer_idx,false,
                                                  kdu_dims(),0,true)).exists())
        write_metanode_to_jp2(scan,jp2_out,file_server);
      while ((scan=meta_manager.enumerate_matches(scan,jpx_stream_idx,-1,false,
                                                  kdu_dims(),0,true)).exists())
        write_metanode_to_jp2(scan,jp2_out,file_server);
      if (!jpx_in.access_composition())
        { // In this case, rendered result same as composition layer
          while ((scan=meta_manager.enumerate_matches(scan,-1,-1,true,
                                                      kdu_dims(),0,
                                                      true)).exists())
            write_metanode_to_jp2(scan,jp2_out,file_server);
        }
      while ((scan=meta_manager.enumerate_matches(scan,-1,-1,false,
                                                  kdu_dims(),0,true)).exists())
        write_metanode_to_jp2(scan,jp2_out,file_server);
    }
  
  jp2_out.open_codestream();
  
  if (jp2_family_in.uses_cache())
    { // Need to generate output codestreams from scratch -- transcoding
      kdu_codestream cs_in;
      try { // Protect the `cs_in' object, so we can destroy it
        cs_in.create(&stream_box);
        copy_codestream(&jp2_out,cs_in);
        cs_in.destroy();
      }
      catch (int val)
      {
        if (cs_in.exists())
          cs_in.destroy();
        throw val;
      }
    }
  else
    { // Simply copy the box contents directly
      kdu_byte *buffer = new kdu_byte[1<<20];
      try {
        int xfer_bytes;
        while ((xfer_bytes=stream_box.read(buffer,(1<<20))) > 0)
          jp2_out.write(buffer,xfer_bytes);
      }
      catch (int val)
      {
        delete[] buffer;
        throw val;
      }
      delete[] buffer;
    }
  stream_box.close();
  jp2_out.close();
  tgt.close();
}

/******************************************************************************/
/*                        CKdu_showApp::save_as_jpx                           */
/******************************************************************************/

void
  CKdu_showApp::save_as_jpx(const char *filename,
                            bool preserve_codestream_links,
                            bool force_codestream_links)
{
  assert(jpx_in.exists());
  if ((open_file_pathname == NULL) || jp2_family_in.uses_cache())
    force_codestream_links = preserve_codestream_links = false;
  if (force_codestream_links)
    preserve_codestream_links = true;
  int n, num_codestreams, num_compositing_layers;
  jpx_in.count_codestreams(num_codestreams);
  jpx_in.count_compositing_layers(num_compositing_layers);
  jpx_layer_source default_layer; // To substitute for missing ones
  for (n=0; (n < num_compositing_layers) && !default_layer; n++)
    default_layer = jpx_in.access_layer(n);
  if (!default_layer)
    { kdu_error e; e << "Cannot write JPX file.  Not even one of the source "
      "file's compositing layers is available yet."; }
  jpx_codestream_source default_stream = // To substitute for missing ones
    jpx_in.access_codestream(default_layer.get_codestream_id(0));

  // Create output file
  jp2_family_tgt tgt; tgt.open(filename);
  jpx_target jpx_out; jpx_out.open(&tgt);

  // Allocate local arrays
  jpx_codestream_target *out_streams =
    new jpx_codestream_target[num_codestreams];
  jpx_codestream_source *in_streams =
    new jpx_codestream_source[num_codestreams];
  kdu_byte *buffer = new kdu_byte[1<<20];

  kdu_codestream cs_in;

  try { // Protect local arrays

      // Generate output layers
      for (n=0; n < num_compositing_layers; n++)
        {
          jpx_layer_source layer_in = jpx_in.access_layer(n);
          if (!layer_in)
            layer_in = default_layer;
          jpx_layer_target layer_out = jpx_out.add_layer();
          jp2_colour colour_in, colour_out;
          int k=0;
          while ((colour_in = layer_in.access_colour(k++)).exists())
            {
              colour_out =
                layer_out.add_colour(colour_in.get_precedence(),
                                     colour_in.get_approximation_level());
              colour_out.copy(colour_in);
            }
          layer_out.access_resolution().copy(layer_in.access_resolution());
          layer_out.access_channels().copy(layer_in.access_channels());
          int num_streams = layer_in.get_num_codestreams();
          for (k=0; k < num_streams; k++)
            {
              kdu_coords alignment, sampling, denominator;
              int codestream_idx =
                layer_in.get_codestream_registration(k,alignment,sampling,
                                                     denominator);
              layer_out.set_codestream_registration(codestream_idx,alignment,
                                                    sampling,denominator);
            }
        }

      // Copy composition instructions and compatibility info
      jpx_out.access_compatibility().copy(jpx_in.access_compatibility());
      jpx_out.access_composition().copy(jpx_in.access_composition());

      // Copy codestream headers
      for (n=0; n < num_codestreams; n++)
        {
          in_streams[n] = jpx_in.access_codestream(n);
          if (!in_streams[n])
            in_streams[n] = default_stream;
          out_streams[n] = jpx_out.add_codestream();
          out_streams[n].access_dimensions().copy(
                                in_streams[n].access_dimensions(true));
          out_streams[n].access_palette().copy(
                                in_streams[n].access_palette());
        }

      // Write JPX headers and all the code-streams
      jp2_data_references data_refs_in;
      if (preserve_codestream_links)
        data_refs_in = jpx_in.access_data_references();
      for (n=0; n < num_codestreams; n++)
        {
          jpx_out.write_headers(NULL,NULL,n); // Interleaved header writing
          jpx_input_box box_in;
          jpx_fragment_list frags_in;
          if (preserve_codestream_links)
            frags_in = in_streams[n].access_fragment_list();
          if (!(frags_in.exists() && data_refs_in.exists()))
            in_streams[n].open_stream(&box_in);
          jp2_output_box *box_out = NULL;
          if ((!force_codestream_links) && box_in.exists())
            box_out = out_streams[n].open_stream();
          int xfer_bytes;
          if (jp2_family_in.uses_cache())
            { // Need to generate output codestreams from scratch -- transcoding
              assert(box_in.exists() && (box_out != NULL));
              cs_in.create(&box_in);
              copy_codestream(box_out,cs_in);
              cs_in.destroy();
              box_out->close();
            }
          else if (box_out != NULL)
            { // Simply copy the box contents directly
              if (box_in.get_remaining_bytes() > 0)
                box_out->set_target_size(box_in.get_remaining_bytes());
              else
                box_out->write_header_last();
              while ((xfer_bytes=box_in.read(buffer,1<<20)) > 0)
                box_out->write(buffer,xfer_bytes);
              box_out->close();
            }
          else
            { // Writing a fragment list
              kdu_long offset, length;            
              if (box_in.exists())
                { // Generate fragment table from embedded source codestream
                  assert(force_codestream_links);
                  jp2_locator loc = box_in.get_locator();
                  offset = loc.get_file_pos() + box_in.get_box_header_length();
                  length = box_in.get_remaining_bytes();
                  if (length < 0)
                    for (length=0; (xfer_bytes=box_in.read(buffer,1<<20)) > 0; )
                      length += xfer_bytes;
                  out_streams[n].add_fragment(open_file_pathname,offset,length);                
                }
              else
                { // Copy fragment table from linked source codestream
                  assert(frags_in.exists() && data_refs_in.exists());
                  int frag_idx, num_frags = frags_in.get_num_fragments();
                  for (frag_idx=0; frag_idx < num_frags; frag_idx++)
                    {
                      int url_idx;
                      const char *url_path;
                      if (frags_in.get_fragment(frag_idx,url_idx,
                                                offset,length) &&
                          ((url_path=data_refs_in.get_url(url_idx)) != NULL))
                        out_streams[n].add_fragment(url_path,offset,length);
                    }
                }
              out_streams[n].write_fragment_table();
            }

          box_in.close();
        }

      // Copy JPX metadata
      int i_param=0;
      void *addr_param=NULL;
      jp2_output_box *metadata_container=NULL;
      jpx_meta_manager meta_in = jpx_in.access_meta_manager();
      jpx_meta_manager meta_out = jpx_out.access_meta_manager();
      meta_out.copy(meta_in);
      while ((metadata_container =
              jpx_out.write_metadata(&i_param,&addr_param)) != NULL)
        { // Note that we are only writing headers up to and including
          // those which are required for codestream `n'.
          kdms_file *file;
          if ((file_server != NULL) && (addr_param == (void *)file_server) &&
              ((file = file_server->find_file(i_param)) != NULL))
            write_metadata_box_from_file(metadata_container,file);
        }
    }
  catch (int val)
    {
      delete[] in_streams;
      delete[] out_streams;
      delete[] buffer;
      if (cs_in.exists())
        cs_in.destroy();
      throw val;
    }

  // Clean up
  delete[] in_streams;
  delete[] out_streams;
  delete[] buffer;
  jpx_out.close();
  tgt.close();
}

/******************************************************************************/
/*                        CKdu_showApp::save_as_raw                           */
/******************************************************************************/

void
  CKdu_showApp::save_as_raw(const char *filename)
{
  compositor->halt_processing();
  kdrc_stream *ref = compositor->get_next_codestream(NULL,false,false);
  if (ref == NULL)
    { kdu_error e; e << "No active codestream is available for writing the "
      "requested output file."; }
  kdu_codestream cs_in = compositor->access_codestream(ref);
  kdu_simple_file_target file_out;
  file_out.open(filename);
  copy_codestream(&file_out,cs_in);
  file_out.close();
}

/******************************************************************************/
/*                      CKdu_showApp::copy_codestream                         */
/******************************************************************************/

void
  CKdu_showApp::copy_codestream(kdu_compressed_target *tgt, kdu_codestream src)
{
  src.apply_input_restrictions(0,0,0,0,NULL,KDU_WANT_OUTPUT_COMPONENTS);
  siz_params *siz_in = src.access_siz();
  siz_params init_siz; init_siz.copy_from(siz_in,-1,-1);
  kdu_codestream dst; dst.create(&init_siz,tgt);
  try { // Protect the `dst' codestream
      siz_params *siz_out = dst.access_siz();
      siz_out->copy_from(siz_in,-1,-1);
      siz_out->parse_string("ORGgen_plt=yes");
      siz_out->finalize_all(-1);

      // Now ready to perform the transfer of compressed data between streams
      kdu_dims tile_indices; src.get_valid_tiles(tile_indices);
      kdu_coords idx;
      for (idx.y=0; idx.y < tile_indices.size.y; idx.y++)
        for (idx.x=0; idx.x < tile_indices.size.x; idx.x++)
          {
            kdu_tile tile_in = src.open_tile(idx+tile_indices.pos);
            int tnum = tile_in.get_tnum();
            siz_out->copy_from(siz_in,tnum,tnum);
            siz_out->finalize_all(tnum);
            kdu_tile tile_out = dst.open_tile(idx+tile_indices.pos);
            assert(tnum == tile_out.get_tnum());
            copy_tile(tile_in,tile_out);
            tile_in.close();
            tile_out.close();
          }
      dst.trans_out();
    }
  catch (int val)
    {
      if (dst.exists())
        dst.destroy();
      throw val;
    }
  dst.destroy();
}
