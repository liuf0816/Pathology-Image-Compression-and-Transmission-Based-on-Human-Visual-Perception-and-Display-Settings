/*****************************************************************************/
// File: kdms_renderer.mm [scope = APPS/MACSHOW]
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
   Implements the main image management and rendering object, `kdms_renderer',
an instance of which is associated with each `kdms_window' object.
******************************************************************************/
#import "kdms_renderer.h"
#import "kdms_window.h"
#import "kdms_properties.h"
#import "kdms_metadata_editor.h"
#import "kdms_catalog.h"
#include "kdu_arch.h"

#ifdef __SSE__
#  ifndef KDU_X86_INTRINSICS
#    define KDU_X86_INTRINSICS
#  endif
#endif

#ifdef KDU_X86_INTRINSICS
#  include <tmmintrin.h>
#endif // KDU_X86_INTRINSICS

/*===========================================================================*/
/*                             INTERNAL FUNCTIONS                            */
/*===========================================================================*/

/*****************************************************************************/
/* STATIC                         copy_tile                                  */
/*****************************************************************************/

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
                        num_bytes+=(out->pass_lengths[z]=in->pass_lengths[z]);
                        out->pass_slopes[z] = in->pass_slopes[z];
                      }
                    if (out->max_bytes < num_bytes)
                      out->set_max_bytes(num_bytes,false);
                    memcpy(out->byte_buffer,in->byte_buffer,(size_t)num_bytes);
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
  // we can add one.
  FSRef fs_ref;
  CFStringRef extension_string =
    CFStringCreateWithCString(NULL,doc_suffix,kCFStringEncodingASCII);
  OSStatus os_status =
    LSGetApplicationForInfo(kLSUnknownType,kLSUnknownCreator,extension_string,
                            kLSRolesAll,&fs_ref,NULL);
  CFRelease(extension_string);
  if (os_status == 0)
    {
      char pathname[512]; *pathname = '\0';
      os_status = FSRefMakePath(&fs_ref,(UInt8 *) pathname,511);
      if ((os_status == 0) && (*pathname != '\0'))
        add_editor_for_doc_type(doc_suffix,pathname);
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
  
  FSRef fs_ref;
  OSStatus os_status = FSPathMakeRef((UInt8 *)app_pathname,&fs_ref,NULL);
  if (os_status == 0)
    {
      scan = new kdms_file_editor;
      scan->doc_suffix = new char[strlen(doc_suffix)+1];
      strcpy(scan->doc_suffix,doc_suffix);
      scan->app_pathname = new char[strlen(app_pathname)+1];
      strcpy(scan->app_pathname,app_pathname);
      if ((scan->app_name = strrchr(scan->app_pathname,'/')) != NULL)
        scan->app_name++;
      else
        scan->app_name = scan->app_pathname;
      scan->fs_ref = fs_ref;
      scan->next = editors;
      editors = scan;
      return scan;
    }
  return NULL;
}


/*===========================================================================*/
/*                           kdms_region_compositor                          */
/*===========================================================================*/

/******************************************************************************/
/*                  kdms_region_compositor::allocate_buffer                   */
/******************************************************************************/

kdu_compositor_buf *
  kdms_region_compositor::allocate_buffer(kdu_coords min_size,
                                          kdu_coords &actual_size,
                                          bool read_access_required)
{
  if (min_size.x < 1) min_size.x = 1;
  if (min_size.y < 1) min_size.y = 1;
  actual_size = min_size;
  
  kdms_compositor_buf *prev=NULL, *elt=NULL;
  
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
      elt = new kdms_compositor_buf;
      need_init = true;
    }
  elt->next = active_list;
  active_list = elt;
  elt->set_read_accessibility(read_access_required);
  if (need_init)
    {
      elt->size = actual_size;
      elt->row_gap = actual_size.x + ((-actual_size.x) & 3); // 16-byte align
      elt->handle = new kdu_uint32[elt->size.y*elt->row_gap+3];// Allow realign
      int align_offset = ((-_addr_to_kdu_int32(elt->handle)) & 0x0F) >> 2;
      elt->buf = elt->handle + align_offset;
    }
  return elt;
}

/******************************************************************************/
/*                    kdms_region_compositor::delete_buffer                   */
/******************************************************************************/

void
  kdms_region_compositor::delete_buffer(kdu_compositor_buf *_buffer)
{
  kdms_compositor_buf *buffer = (kdms_compositor_buf *) _buffer;
  kdms_compositor_buf *scan, *prev;
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


/*===========================================================================*/
/*                             kdms_data_provider                            */
/*===========================================================================*/

/*****************************************************************************/
/*                   kdms_data_provider::kdms_data_provider                  */
/*****************************************************************************/

kdms_data_provider::kdms_data_provider()
{
  provider_ref = NULL;
  buf = NULL;
  tgt_size = 0;
  display_with_focus = false;
  rows_above_focus = rows_below_focus = focus_rows = 0;
  cols_left_of_focus = cols_right_of_focus = focus_cols = 0;
  for (int i=0; i < 256; i++)
    {
      foreground_lut[i] = (kdu_byte)(40 + ((i*216) >> 8));
      background_lut[i] = (kdu_byte)((i*216) >> 8);
    }
}

/*****************************************************************************/
/*                  kdms_data_provider::~kdms_data_provider                  */
/*****************************************************************************/

kdms_data_provider::~kdms_data_provider()
{
  if (provider_ref != NULL)
    CGDataProviderRelease(provider_ref);
}

/*****************************************************************************/
/*                          kdms_data_provider::init                         */
/*****************************************************************************/

CGDataProviderRef
  kdms_data_provider::init(kdu_coords size, kdu_uint32 *buf, int row_gap,
                           bool display_with_focus, kdu_dims focus_box)
{
  off_t new_size = (off_t)(size.x*size.y);
  if ((provider_ref == NULL) || (tgt_size != new_size))
    { // Need to allocate a new CGDataProvider resource
      CGDataProviderDirectAccessCallbacks callbacks;
      callbacks.getBytePointer = NULL;
      callbacks.releaseBytePointer = NULL;
      callbacks.releaseProvider = NULL;
      callbacks.getBytes = kdms_data_provider::get_bytes_callback;
      if (provider_ref != NULL)
        CGDataProviderRelease(provider_ref);
      provider_ref =
        CGDataProviderCreateDirectAccess(this,(new_size<<2),&callbacks);
    }
  tgt_size = new_size;
  tgt_width = size.x;
  tgt_height = size.y;
  this->buf = buf;
  this->buf_row_gap = row_gap;
  if (!(this->display_with_focus = display_with_focus))
    return provider_ref;
  
  kdu_dims painted_region; painted_region.size = size;
  focus_box &= painted_region;
  if (focus_box.is_empty())
    { // Region to be painted is entirely in the foreground
      rows_above_focus = tgt_height; focus_rows = rows_below_focus = 0;
      cols_left_of_focus = tgt_width; focus_cols = cols_right_of_focus = 0;
      return provider_ref;
    }
  rows_above_focus = focus_box.pos.y;
  focus_rows = focus_box.size.y;
  rows_below_focus = tgt_height - rows_above_focus - focus_rows;
  assert((rows_above_focus >= 0) && (focus_rows >= 0) &&
         (rows_below_focus >= 0));
  cols_left_of_focus = focus_box.pos.x;
  focus_cols = focus_box.size.x;
  cols_right_of_focus = tgt_width - cols_left_of_focus - focus_cols;
  assert((cols_left_of_focus >= 0) && (focus_cols >= 0) &&
         (cols_right_of_focus >= 0));
  return provider_ref;
}

/*****************************************************************************/
/*                   kdms_data_provider::get_bytes_callback                  */
/*****************************************************************************/

size_t
  kdms_data_provider::get_bytes_callback(void *info, void *buffer,
                                         size_t offset,
                                         size_t original_byte_count)
{
  kdms_data_provider *obj = (kdms_data_provider *) info;
  assert(((offset & 3) == 0) &&
         ((original_byte_count & 3) == 0)); // While number of pels
  offset >>= 2; // Convert to a pixel offset
  int pels_left_to_transfer = (int)(original_byte_count >> 2);
  int tgt_row = (int)(offset / obj->tgt_width);
  int tgt_col = (int)(offset - tgt_row*obj->tgt_width);
  if (!obj->display_with_focus)
    { // No need for tone mapping curves
      kdu_uint32 *src = obj->buf + tgt_row*obj->buf_row_gap + tgt_col;
      kdu_uint32 *dst = (kdu_uint32 *) buffer;
#ifdef KDU_X86_INTRINSICS
      bool can_use_simd_speedups = (kdu_mmx_level >= 4); // Need SSSE3
      kdu_byte shuffle_control[16] = {3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12};
      __asm__ volatile ("movdqu %0,%%xmm0" : : "m" (shuffle_control[0]));
#endif // KDU_X86_INTRINSICS
      while (tgt_row < obj->tgt_height)
        {
          int copy_pels = obj->tgt_width-tgt_col;
          if (copy_pels > pels_left_to_transfer)
            copy_pels = pels_left_to_transfer;
          if (copy_pels == 0)
            return original_byte_count;
          pels_left_to_transfer -= copy_pels;
          int c = copy_pels;
          
#       ifdef KDU_X86_INTRINSICS
          if (can_use_simd_speedups &&
              !((_addr_to_kdu_int32(src) | _addr_to_kdu_int32(dst)) & 15))
            { // `src' and `dst' are both 16-byte aligned
              for (; c > 7; c-=8, src+=8, dst+=8)
                { // The following code uses the SSSE3 PSHUFB instruction with
                  // source operand XMM0 and destination operand XMM1.  It
                  // first loads XMM1 with *src and finishes by writing XMM1
                  // to *dst.  The slightly unrolled loop uses XMM2 as well.
                  __asm__ volatile ("movdqa %2,%%xmm1\n\t"
                                    ".byte 0x66; "
                                    ".byte 0x0F; "
                                    ".byte 0x38; "
                                    ".byte 0x00; "
                                    ".byte 0xC8 \n\t"
                                    "movdqa %%xmm1,%0\n\t"
                                    "movdqa %3,%%xmm2\n\t"
                                    ".byte 0x66; "
                                    ".byte 0x0F; "
                                    ".byte 0x38; "
                                    ".byte 0x00; "
                                    ".byte 0xD0 \n\t"
                                    "movdqa %%xmm2,%1"
                                    : "=m" (*dst), "=m" (*(dst+4))
                                    : "m" (*src), "m" (*(src+4))
                                    : "%xmm1", "%xmm2");
                }
            }
          else if (can_use_simd_speedups &&
                   !((_addr_to_kdu_int32(src) | _addr_to_kdu_int32(dst)) & 7))
            { // `src' and `dst' are both 8-byte aligned
              __asm__ volatile ("movdq2q %xmm0,%mm0");
              for (; c > 3; c-=4, src+=4, dst+=4)
                { // The following code uses the SSSE3 PSHUFB instruction with
                  // source operand MM0 and destination operand MM1.  It
                  // first loads MM1 with *src and finishes by writing MM1
                  // to *dst.
                  __asm__ volatile ("movq %2,%%mm1\n\t"
                                    ".byte 0x0F; "
                                    ".byte 0x38; "
                                    ".byte 0x00; "
                                    ".byte 0xC8 \n\t"
                                    "movq %%mm1,%0\n\t"
                                    "movq %3,%%mm2\n\t"
                                    ".byte 0x0F; "
                                    ".byte 0x38; "
                                    ".byte 0x00; "
                                    ".byte 0xD0 \n\t"
                                    "movq %%mm2,%1"
                                    : "=m" (*dst), "=m" (*(dst+2))
                                    : "m" (*src), "m" (*(src+2))
                                    : "%mm1", "%mm2");
                }
              __asm__ volatile ("emms");
            }
#       endif // KDU_X86_INTRINSICS
            
#       ifdef __BIG_ENDIAN__ // Executing on PowerPC architecture
          memcpy(dst,src,(size_t)(c<<2));
          dst += c;
          src += c;
#       else // Executing on Intel architecture
          for (; c > 0; c--, dst++, src++)
            {
              kdu_uint32 val = *src;
              *dst = ((val >> 24) + ((val >> 8) & 0xFF00) +
                      ((val << 8) & 0xFF0000) + (val << 24));
            }
#       endif // Executing on Intel architecture

          src += obj->buf_row_gap - (tgt_col + copy_pels);
          tgt_col = 0;
          tgt_row++;
        }
      return original_byte_count - (size_t)(pels_left_to_transfer<<2);
    }

  // If we get here, we need to use the tone mapping curves and adjust
  // the behaviour for background and foreground regions
  int region_rows[3] =
    {obj->rows_above_focus, obj->focus_rows, obj->rows_below_focus};
  if ((region_rows[0] -= tgt_row) < 0)
    {
      if ((region_rows[1] += region_rows[0]) < 0)
        {
          region_rows[2] += region_rows[1];
          region_rows[1] = 0;
        }
      region_rows[0] = 0;
    }      
  int region_cols[3] =
    {obj->cols_left_of_focus, obj->focus_cols, obj->cols_right_of_focus};
  if ((region_cols[0] -= tgt_col) < 0)
    {
      if ((region_cols[1] += region_cols[0]) < 0)
        {
          region_cols[2] += region_cols[1];
          region_cols[1] = 0;
        }
      region_cols[0] = 0;
    }
  kdu_byte *src = (kdu_byte *)(obj->buf + tgt_row*obj->buf_row_gap + tgt_col);
  kdu_byte *dst = (kdu_byte *) buffer;
  kdu_byte *lut = NULL;
  int inter_row_src_skip = (obj->buf_row_gap - obj->tgt_width)<<2;
  int reg_v, reg_h, r;
  for (reg_v=0; reg_v < 3; reg_v++)
    { // Scan through the vertical regions (bg, fg, bg)
      for (r=region_rows[reg_v]; r > 0; r--)
        {
          if (reg_v != 1)
            { // Everything is background
              lut = obj->background_lut;
              int copy_pels = region_cols[0]+region_cols[1]+region_cols[2];
              if (copy_pels > pels_left_to_transfer)
                copy_pels = pels_left_to_transfer;
              if (copy_pels <= 0)
                return original_byte_count;
              pels_left_to_transfer -= copy_pels;
              for (; copy_pels != 0; copy_pels--)
                {
#               ifdef __BIG_ENDIAN__ // Executing on PowerPC architecture
                  dst[0] = src[0];
                  dst[1] = lut[src[1]];
                  dst[2] = lut[src[2]];
                  dst[3] = lut[src[3]];
#               else // Executing on Intel architecture
                  dst[0] = src[3];
                  dst[1] = lut[src[2]];
                  dst[2] = lut[src[1]];
                  dst[3] = lut[src[0]];
#               endif // Executing on Intel architecture
                  dst += 4; src += 4;
                }
            }
          else
            { // Some columns may be foreground and some background
              for (reg_h=0; reg_h < 3; reg_h++)
                {
                  lut = (reg_h == 1)?obj->foreground_lut:obj->background_lut;
                  int copy_pels = region_cols[reg_h];
                  if (copy_pels == 0)
                    continue; // This region is empty
                  if (copy_pels > pels_left_to_transfer)
                    copy_pels = pels_left_to_transfer;
                  if (copy_pels <= 0)
                    return original_byte_count;
                  pels_left_to_transfer -= copy_pels;                  
                  for (; copy_pels != 0; copy_pels--)
                    {
#                   ifdef __BIG_ENDIAN__ // Executing on PowerPC architecture
                      dst[0] = src[0];
                      dst[1] = lut[src[1]];
                      dst[2] = lut[src[2]];
                      dst[3] = lut[src[3]];
#                   else // Executing on Intel architecture
                      dst[0] = src[3];
                      dst[1] = lut[src[2]];
                      dst[2] = lut[src[1]];
                      dst[3] = lut[src[0]];
#                   endif // Executing on Intel architecture
                      dst += 4; src += 4;
                    }
                }
            }
       
          // Adjust the `src' pointer
          src += inter_row_src_skip;
          
          // Reset the number of columns in each region
          region_cols[0] = obj->cols_left_of_focus;
          region_cols[1] = obj->focus_cols;
          region_cols[2] = obj->cols_right_of_focus;
        }
    }
  return original_byte_count - (size_t)(pels_left_to_transfer<<2);
}


/*===========================================================================*/
/*                            kdms_playmode_stats                            */
/*===========================================================================*/

/*****************************************************************************/
/*                        kdms_playmode_stats::update                        */
/*****************************************************************************/

void kdms_playmode_stats::update(int frame_idx, double start_time,
                                 double end_time, double cur_time)
{
  if ((num_updates_since_reset > 0) && (last_frame_idx == frame_idx))
    return; // Duplicate frame (should not happen, the way this
            // function is currently used)
  if (num_updates_since_reset > 0)
    {
      double interval = cur_time - cur_time_at_last_update;
      if (interval < 0.0001)
        interval = 0.0001;
      if (num_updates_since_reset == 1)
        mean_time_between_frames = interval;
      else
        {
          double update_weight = 0.5 * (mean_time_between_frames + interval);
          if (update_weight > 1.0)
            update_weight = 1.0;
          else if (update_weight < 0.025)
            update_weight = 0.025;
          if ((update_weight*num_updates_since_reset) < 1.0)
            update_weight = 1.0 / (double) num_updates_since_reset;
          mean_time_between_frames = interval*update_weight +
            (mean_time_between_frames*(1.0-update_weight));
        }
    }
  last_frame_idx = frame_idx;
  cur_time_at_last_update = cur_time;
  num_updates_since_reset++;
}


/*===========================================================================*/
/*                               kdms_renderer                               */
/*===========================================================================*/

/*****************************************************************************/
/*                        kdms_renderer::kdms_renderer                       */
/*****************************************************************************/

kdms_renderer::kdms_renderer(kdms_window *owner, 
                             kdms_document_view *doc_view,
                             NSScrollView *scroll_view,
                             kdms_window_manager *window_manager)
{
  this->window = owner;
  this->doc_view = doc_view;
  this->scroll_view = scroll_view;
  this->window_manager = window_manager;
  this->metashow = nil;
  this->window_identifier = window_manager->get_window_identifier(owner);

  catalog_source = nil;
  catalog_closed = false;
  
  open_file_pathname = NULL;
  open_file_urlname = NULL;
  open_filename = NULL;
  last_save_pathname = NULL;
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
  status_id = KDS_STATUS_LAYER_RES;

  in_playmode = false;
  playmode_repeat = false;
  playmode_reverse = false;
  pushed_last_frame = false;
  use_native_timing = true;
  custom_frame_interval = native_playback_multiplier = 1.0F;
  playclock_offset = 0.0;
  playclock_base = 0.0;
  playmode_frame_queue_size = 3;
  num_expired_frames_on_queue = 0;
  frame_presenter = window_manager->get_frame_presenter(owner);
  last_status_update_time = 0.0;
  
  pixel_scale = 1;
  image_dims.pos = image_dims.size = kdu_coords(0,0);
  buffer_dims = view_dims = image_dims;
  view_centre_x = view_centre_y = 0.0F;
  view_centre_known = false;

  enable_focus_box = false;
  highlight_focus = true;
  focus_anchors_known = false;
  focus_centre_x = focus_centre_y = focus_size_x = focus_size_y = 0.0F;
  focus_box.pos = focus_box.size = kdu_coords(0,0);
  overlays_enabled = overlay_flashing_enabled = false;
  overlay_painting_intensity = overlay_painting_colour = 0;
  overlay_size_threshold = 1;
  overlay_flash_wakeup_scheduled = false;
  
  buffer = NULL;
  generic_rgb_space = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
}

/*****************************************************************************/
/*                        kdms_renderer::~kdms_renderer                      */
/*****************************************************************************/

kdms_renderer::~kdms_renderer()
{
  perform_essential_cleanup_steps();
  
  if (file_server != NULL)
    delete file_server;
  if (compositor != NULL)
    { delete compositor; compositor = NULL; }
  if (thread_env.exists())
    thread_env.destroy();
  file_in.close();
  jpx_in.close();
  mj2_in.close();
  jp2_family_in.close();
  if (last_save_pathname != NULL)
    delete[] last_save_pathname;
  CGColorSpaceRelease(generic_rgb_space);
}

/*****************************************************************************/
/*             kdms_renderer::perform_essential_cleanup_steps                */
/*****************************************************************************/

void kdms_renderer::perform_essential_cleanup_steps()
{
  if (file_server != NULL)
    delete file_server;
  file_server = NULL;
  if ((window_manager != NULL) && (open_file_pathname != NULL))
    {
      file_in.close(); // Unlocks any files
      jp2_family_in.close(); // Unlocks any files
      window_manager->release_open_file_pathname(open_file_pathname);
      open_file_pathname = NULL;
      open_filename = NULL;
    }  
}

/*****************************************************************************/
/*                        kdms_renderer::close_file                          */
/*****************************************************************************/

void kdms_renderer::close_file()
{
  perform_essential_cleanup_steps();
  
  stop_playmode();
  if (compositor != NULL)
    { delete compositor; compositor = NULL; }
  if (metashow != nil)
    [metashow deactivate];
  file_in.close();
  jpx_in.close();
  mj2_in.close();
  jp2_family_in.close();
  
  open_file_pathname = NULL;
  open_file_urlname = NULL;
  open_filename = NULL;
  if (last_save_pathname != NULL)
    delete[] last_save_pathname;
  last_save_pathname = NULL;
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
  overlay_flash_wakeup_scheduled = false;
  buffer = NULL;
  
  [window setTitle:
   [NSString stringWithFormat:@"kdu_show %d: <no data>",window_identifier]];
  
  if ((catalog_source != nil) && (window != nil))
    [window remove_metadata_catalog];
  else if (catalog_source)
    set_metadata_catalog_source(NULL); // Just in case the window went away
  catalog_closed = false;  
}

/*****************************************************************************/
/*                         kdms_renderer::open_file                          */
/*****************************************************************************/

void kdms_renderer::open_file(const char *filename)
{
  if (filename != NULL)
    close_file();
  assert(compositor == NULL);
  enable_focus_box = false;
  focus_box.size = kdu_coords(0,0);
  // client_roi.init();
  processing_suspended = false;
  try
    {
      status_id = KDS_STATUS_LAYER_RES;
      jp2_family_in.open(filename,allow_seeking);
      compositor = new kdms_region_compositor(&thread_env);
      if (jpx_in.open(&jp2_family_in,true) >= 0)
        { // We have a JP2/JPX-compatible file.
          compositor->create(&jpx_in);
        }
      else if (mj2_in.open(&jp2_family_in,true) >= 0)
        {
          compositor->create(&mj2_in);
        }
      else
        { // Incompatible with JP2/JPX or MJ2. Try opening raw stream
          jp2_family_in.close();
          file_in.open(filename,allow_seeking);
          compositor->create(&file_in);
        }
      compositor->set_error_level(error_level);
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
      overlay_flash_wakeup_scheduled = false;
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
      open_file_pathname = window_manager->retain_open_file_pathname(filename);
      open_filename = strrchr(open_file_pathname,'/');
      if (open_filename == NULL)
        open_filename = open_file_pathname;
      else
        open_filename++;
      
      char *title = new char[strlen(open_filename)+40];
      sprintf(title,"kdu_show %d: %s",window_identifier,open_filename);
      [window setTitle:[NSString stringWithUTF8String:title]];
      delete[] title;
    }
  if ((metashow != nil) && jp2_family_in.exists())
    [metashow activateWithSource:&jp2_family_in andFileName:open_filename];
  if (jpx_in.exists() &&
      jpx_in.access_meta_manager().peek_touched_nodes(jp2_label_4cc).exists())
    [window create_metadata_catalog];
}

/*****************************************************************************/
/*                   kdms_renderer::invalidate_configuration                 */
/*****************************************************************************/

void kdms_renderer::invalidate_configuration()
{
  configuration_complete = false;
  max_components = -1; // Changed config may have different num image comps
  buffer = NULL;
  
  if (compositor != NULL)
    compositor->remove_compositing_layer(-1,false);
}

/*****************************************************************************/
/*                      kdms_renderer::initialize_regions                    */
/*****************************************************************************/

void kdms_renderer::initialize_regions()
{
  bool can_enlarge_window = fit_scale_to_window;
  
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
                assert(0);
                // update_client_window_of_interest();
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
                throw 0;
                /*
                if (jpip_client.is_one_time_request())
                  { // See if request is compatible with opening a layer
                    if (!check_one_time_request_compatibility())         
                      continue; // Mode changed to single component
                  }
                else
                  update_client_window_of_interest();
                */
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
                throw 0;
                /*
                if (jpip_client.is_one_time_request())
                  { // See if request is compatible with opening a frame
                    if (!check_one_time_request_compatibility())         
                      continue; // Mode changed to single layer or component
                  }
                else
                  update_client_window_of_interest();
                */
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
        /*
        if (fit_scale_to_window && jpip_client.is_one_time_request() &&
            !check_one_time_request_compatibility())
          { // Check we are opening the image in the intended manner
            compositor->remove_compositing_layer(-1,false);
            continue; // Open again in different mode
          }
        */
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
            /*
            if (enable_region_posting)
              update_client_window_of_interest();
            */
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
      buffer = compositor->get_composition_buffer(buffer_dims);
      if (adjust_focus_anchors())
        update_focus_box();
    }
  else
    { // Need to size the window, possibly for the first time.
      view_dims = buffer_dims = kdu_dims();
      image_dims = new_image_dims;
      if (adjust_focus_anchors())
        { view_centre_x = focus_centre_x; view_centre_y = focus_centre_y; }
      [doc_view setNeedsDisplay:YES];
      size_window_for_image_dims(can_enlarge_window);
    }
  if (in_playmode)
    { // See if we need to turn off play mode
      if ((single_component_idx >= 0) ||
          ((single_layer_idx >= 0) && jpx_in.exists()))
        stop_playmode();      
    }
  if (!in_playmode)
    display_status();
}

/*****************************************************************************/
/*                 kdms_renderer::get_max_scroll_view_size                   */
/*****************************************************************************/

bool kdms_renderer::get_max_scroll_view_size(NSSize &size)
{
  if ((buffer == NULL) || !image_dims)
    return false;
  NSSize doc_size; // Size of the `doc_view' frame
  doc_size.width = ((float) image_dims.size.x) * pixel_scale;
  doc_size.height = ((float) image_dims.size.y) * pixel_scale;
  size = [NSScrollView frameSizeForContentSize:doc_size
                         hasHorizontalScroller:YES
                           hasVerticalScroller:YES
                                    borderType:NSNoBorder];
  return true;
}

/*****************************************************************************/
/*                kdms_renderer::size_window_for_image_dims                  */
/*****************************************************************************/

void kdms_renderer::size_window_for_image_dims(bool first_time)
{
  NSRect max_window_rect = [[NSScreen mainScreen] visibleFrame];
  NSRect existing_window_rect = [window frame];
  if (!first_time)
    max_window_rect = existing_window_rect;
  
  // Find the size of the `doc_view' window and see if it can fit
  // completely inside the `scroll_view', given the window dimensions.
  NSSize doc_size; // Size of the `doc_view' frame
  doc_size.width = ((float) image_dims.size.x) * pixel_scale;
  doc_size.height = ((float) image_dims.size.y) * pixel_scale;
  NSRect scroll_rect, window_rect;
  scroll_rect.origin.x = scroll_rect.origin.y = 0.0F;
  scroll_rect.size = [NSScrollView frameSizeForContentSize:doc_size
                                     hasHorizontalScroller:YES
                                       hasVerticalScroller:YES
                                                borderType:NSNoBorder];
  NSRect content_rect = scroll_rect;
  content_rect.size.height += [window get_bottom_margin_height];
  content_rect.size.width += [window get_right_margin_width];
  window_rect = [window frameRectForContentRect:content_rect];
  [window setMaxSize:window_rect.size];
  
  window_rect.origin = existing_window_rect.origin;
  if (window_rect.size.width > max_window_rect.size.width)
    window_rect.size.width = max_window_rect.size.width;
  if (window_rect.size.height > max_window_rect.size.height)
    window_rect.size.height = max_window_rect.size.height;
  
  bool resize_window = false;
  if (window_rect.size.width != existing_window_rect.size.width)
    { // Need to adjust window width and position.
      resize_window = true;
      float left = existing_window_rect.origin.x;
      float right = left + window_rect.size.width;
      float max_left = max_window_rect.origin.x;
      float max_right = max_left + max_window_rect.size.width;
      float shift;
      if ((shift = (right - max_right)) > 0.0F)
        { right -= shift; left -= shift; }
      if ((shift = (max_left-left)) > 0.0F)
        { right += shift; left += shift; }
      window_rect.origin.x = left;
    }
  if (window_rect.size.height != existing_window_rect.size.height)
    { // Need to adjust window width and position.
      resize_window = true;
      float top =
        existing_window_rect.origin.y + existing_window_rect.size.height;
      float bottom = top - window_rect.size.height;
      float max_bottom = max_window_rect.origin.y;
      float max_top = max_bottom + max_window_rect.size.height;
      float shift;
      if ((shift = (max_bottom-bottom)) > 0.0F)
        { bottom += shift; top += shift; }
      if ((shift = (top - max_top)) > 0.0F)
        { bottom -= shift; top -= shift; }
      window_rect.origin.y = top - window_rect.size.height;
    }  
  if (resize_window)
    {
      if (first_time)
        [window placeUsingManagerWithFrameSize:window_rect.size];
      else
        [window setFrame:window_rect display:YES];
    }
  
  // Finally set the `doc_view'
  [doc_view setFrameSize:doc_size];
  if (view_centre_known)
    scroll_to_view_anchors();
  else if (first_time)
    { // Adjust `scroll_view' position to the top of the image.
      NSRect view_rect = [scroll_view documentVisibleRect];
      NSPoint new_view_origin;
      new_view_origin.x = 0.0F;
      new_view_origin.y = doc_size.height - view_rect.size.height;
      [doc_view scrollPoint:new_view_origin];
    }
  view_dims_changed();
}

/*****************************************************************************/
/*                     kdms_renderer::view_dims_changed                      */
/*****************************************************************************/

void kdms_renderer::view_dims_changed()
{
  if (compositor == NULL)
    return;
  
  // Start by finding `view_dims' as the smallest rectangle (in the same
  // coordinate system as `image_dims') which spans the contents of
  // the `scroll_view'.
  NSRect ns_rect = [scroll_view documentVisibleRect];
  NSRect view_rect;
  view_rect.origin.x = (float) floor(ns_rect.origin.x);
  view_rect.origin.y = (float) floor(ns_rect.origin.y);
  view_rect.size.width = (float)
    ceil(ns_rect.size.width+ns_rect.origin.x) - view_rect.origin.x;
  view_rect.size.height = (float)
    ceil(ns_rect.size.height+ns_rect.origin.y) - view_rect.origin.y;
  
  kdu_dims new_view_dims = convert_region_from_doc_view(view_rect);
  new_view_dims &= image_dims;
  if (new_view_dims == view_dims)
    { // No change
      update_focus_box();
      return;
    }
  
  view_dims = new_view_dims;
  
  // Get preferred minimum dimensions for the new buffer region.
  kdu_coords size = view_dims.size;
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
  kdu_coords image_lim = image_dims.pos + image_dims.size;
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
  buffer = compositor->get_composition_buffer(buffer_dims);
  if (!focus_anchors_known)
    update_focus_box(true);
}

/*****************************************************************************/
/*                        kdms_renderer::paint_region                        */
/*****************************************************************************/

bool kdms_renderer::paint_region(kdu_dims region,
                                 CGContextRef graphics_context,
                                 NSPoint *presentation_offset)
{
  if (!buffer_dims.is_empty())
    region &= buffer_dims;
  if ((buffer == NULL) || (!buffer_dims) || (!image_dims))
    return false;
  
  kdu_coords rel_pos = region.pos - buffer_dims.pos;
  if (rel_pos.x & 3)
    { // Align start of rendering region on 16-byte boundary
      int offset = rel_pos.x & 3;
      region.pos.x -= offset;
      rel_pos.x -= offset;
      region.size.x += offset;
    }
  if (region.size.x & 3)
    { // Align end of region on 16-byte boundary
      region.size.x += (4-region.size.x) & 3;
    }

  bool display_with_focus = enable_focus_box && highlight_focus;  
  kdu_dims rel_focus_box = focus_box; // In case there is a focus box, make it
  rel_focus_box.pos -= region.pos;    // relative to the region being painted
  
  int buf_row_gap;
  kdu_uint32 *buf_data = buffer->get_buf(buf_row_gap,false);
  buf_data += rel_pos.y*buf_row_gap + rel_pos.x;

  NSRect target_region = convert_region_to_doc_view(region);
  CGRect cg_target_region;
  cg_target_region.origin.x = target_region.origin.x;
  cg_target_region.origin.y = target_region.origin.y;
  cg_target_region.size.width = target_region.size.width;
  cg_target_region.size.height = target_region.size.height;
  kdms_data_provider *provider = &app_paint_data_provider;
  if (presentation_offset != NULL)
    {
      cg_target_region.origin.x += presentation_offset->x;
      cg_target_region.origin.y += presentation_offset->y;
      provider = &presentation_data_provider;
    }

  CGImageRef image =
    CGImageCreate(region.size.x,region.size.y,8,32,
                  (region.size.x<<2),generic_rgb_space,
                  kCGImageAlphaPremultipliedFirst,
                  provider->init(region.size,buf_data,buf_row_gap,
                                 display_with_focus,rel_focus_box),
                  NULL,true,kCGRenderingIntentDefault);
  CGContextDrawImage(graphics_context,cg_target_region,image);
  CGImageRelease(image);
  return true;
}

/*****************************************************************************/
/*            kdms_renderer::paint_view_from_presentation_thread             */
/*****************************************************************************/

void kdms_renderer::paint_view_from_presentation_thread(NSGraphicsContext *gc)
{
  if (!view_dims)
    return;
  [gc saveGraphicsState];
  if ([scroll_view lockFocusIfCanDraw])
    {
      NSPoint doc_origin = {0.0F,0.0F};
      NSPoint doc_origin_on_window = [doc_view convertPointToBase:doc_origin];
      NSRect visible_rect = [scroll_view documentVisibleRect];
      visible_rect.origin.x += doc_origin_on_window.x;
      visible_rect.origin.y += doc_origin_on_window.y;
      [NSGraphicsContext setCurrentContext:gc];
      NSRectClip(visible_rect);
      CGContextRef gc_ref = (CGContextRef)[gc graphicsPort];
      paint_region(view_dims,gc_ref,&doc_origin_on_window);
      [gc flushGraphics];
      [scroll_view unlockFocus];
    }
  [gc restoreGraphicsState];
}

/*****************************************************************************/
/*                kdms_renderer::convert_region_from_doc_view                */
/*****************************************************************************/

kdu_dims kdms_renderer::convert_region_from_doc_view(NSRect rect)
{
  float scale_factor = 1.0F / (float) pixel_scale;
  kdu_coords region_min, region_lim;
  region_min.x = (int)
    round(rect.origin.x * scale_factor - 0.4);
  region_lim.x = (int)
    round((rect.origin.x+rect.size.width) * scale_factor + 0.4);
  region_lim.y = image_dims.size.y - (int)
    round(rect.origin.y * scale_factor - 0.4);
  region_min.y = image_dims.size.y - (int)
    round((rect.origin.y+rect.size.height) * scale_factor + 0.4);
  kdu_dims region;
  region.pos = region_min + image_dims.pos;
  region.size = region_lim - region_min;
  return region;
}

/*****************************************************************************/
/*                  kdms_renderer::convert_region_to_doc_view                */
/*****************************************************************************/

NSRect kdms_renderer::convert_region_to_doc_view(kdu_dims region)
{
  float scale_factor = (float) pixel_scale;
  kdu_coords rel_pos = region.pos - image_dims.pos;
  NSRect doc_rect;
  doc_rect.origin.x = rel_pos.x * scale_factor;
  doc_rect.size.width = region.size.x * scale_factor;
  doc_rect.origin.y = (image_dims.size.y-rel_pos.y-region.size.y)*scale_factor;
  doc_rect.size.height = region.size.y * scale_factor;
  return doc_rect;
}

/*****************************************************************************/
/*                           kdms_renderer::on_idle                          */
/*****************************************************************************/

bool kdms_renderer::on_idle()
{
  if (processing_suspended)
    return false;
  
  if (!(configuration_complete && (buffer != NULL)))
    return false; // Nothing to process
  
  bool processed_something = false;
  kdu_dims new_region;
  try {
    processed_something = compositor->process(128000,new_region);
    if ((!processed_something) &&
        !compositor->get_total_composition_dims(image_dims))
      { // Must have invalid scale
        compositor->flush_composition_queue();
        buffer = NULL;
        initialize_regions(); // This call will find a satisfactory scale
        return false;
      }
  }
  catch (int) { // Problem occurred.  Only safe thing is to close the file.
    close_file();
    return false;
  }

  if (in_playmode)
    adjust_frame_queue_presentation_and_wakeup(CFAbsoluteTimeGetCurrent());
  else if (processed_something)
    { // Paint any newly decompressed region right away.
      new_region &= view_dims; // No point in painting invisible regions.
      if (!new_region.is_empty())
        {
          if (pixel_scale > 1)
            { // Adjust the invalidated region so interpolated painting
              // produces no cracks.
              new_region.pos.x--; new_region.pos.y--;
              new_region.size.x += 2; new_region.size.y += 2;
            }
          NSRect dirty_rect = convert_region_to_doc_view(new_region);
          [doc_view displayRect:dirty_rect];
        }
      display_status(); // All processing done, keep status up to date
      if (overlays_enabled && overlay_flashing_enabled)
        {
          if (!overlay_flash_wakeup_scheduled)
            {
              window_manager->schedule_wakeup(window,this,
                                              CFAbsoluteTimeGetCurrent()+0.8);
              overlay_flash_wakeup_scheduled = true;
            }
        }
    }
  
  return ((compositor != NULL) && !compositor->is_processing_complete());
}

/*****************************************************************************/
/*         kdms_renderer::adjust_frame_queue_presentation_and_wakeup         */
/*****************************************************************************/

void kdms_renderer::adjust_frame_queue_presentation_and_wakeup(double cur_time)
{
  assert(playmode_frame_queue_size >= 2);
  
  // Find out exactly how many frames on the queue have already passed
  // their expiry date
  kdu_long stamp;
  int display_frame_idx; // Will be the frame index of any frame we present
  double display_end_time; // Will be the end-time for any frame we present
  bool have_current_frame_on_queue = false; // Will be true if the queue
          // contains a frame whose end time has not yet been reached.
  while (compositor->inspect_composition_queue(num_expired_frames_on_queue,
                                               &stamp,&display_frame_idx))
    {
      display_end_time = (0.001*stamp) + playclock_base;
      if (display_end_time > cur_time)
        { have_current_frame_on_queue = true; break; }
      num_expired_frames_on_queue++;
    }
  
  // See if playmode should stop
  if (pushed_last_frame && !have_current_frame_on_queue)
    {
      stop_playmode();
      return;
    }
    
  // Now see how many frames we can safely pop from the queue, ensuring that
  // at least one frame remains to be presented.
  bool have_new_frame_to_push =
    compositor->is_processing_complete() && !pushed_last_frame;
  int num_frames_to_pop = num_expired_frames_on_queue;
  if (!(have_current_frame_on_queue || have_new_frame_to_push))
    num_frames_to_pop--;
  
  // Now try to pop these frames if we can
  bool need_to_present_new_frame = false;
  if ((num_frames_to_pop > 0) && frame_presenter->disable())
    {
      for (; num_frames_to_pop > 0;
           num_frames_to_pop--, num_expired_frames_on_queue--)
        compositor->pop_composition_buffer();
      buffer = compositor->get_composition_buffer(buffer_dims);
      need_to_present_new_frame = true; // Disabled the presenter, so we will
          // need to re-present something, even if we don't have a current
          // frame to present.  In any case, `display_frame_idx' and
          // `display_end_time' are correct for the frame to be presented.
    }
  
  // Now see if we can push any newly generated frame onto the queue
  if (have_new_frame_to_push &&
      !compositor->inspect_composition_queue(playmode_frame_queue_size-1))
    { // Can push a new frame onto the composition queue
      double pushed_frame_end_time = get_absolute_frame_end_time();
      double pushed_frame_start_time = get_absolute_frame_start_time();
      compositor->set_surface_initialization_mode(false);
      compositor->push_composition_buffer(
          (kdu_long)((pushed_frame_end_time-playclock_base)*1000),frame_idx);
      playmode_stats.update(frame_idx,pushed_frame_start_time,
                            pushed_frame_end_time,cur_time);
      
      // Set up the next frame to render
      if (!set_frame(frame_idx+((playmode_reverse)?-1:1)))
        pushed_last_frame = true;
      if (pushed_last_frame && playmode_repeat)
        {
          if (!playmode_reverse)
            set_frame(0);
          else if (num_frames > 0)
            set_frame(num_frames-1);
          else
            set_frame(100000); // Let `set_frame' find `num_frames'
          playclock_offset +=
            pushed_frame_end_time - get_absolute_frame_start_time();
          pushed_last_frame = false;
        }
      if (!have_current_frame_on_queue)
        { // The newly pushed frame should be presented, unless we were
          // unable to disable the `frame_presenter'
          if (num_expired_frames_on_queue==0) // Else `disable' call failed
            need_to_present_new_frame = true;
          display_end_time = pushed_frame_end_time;
          display_frame_idx = frame_idx;
          have_current_frame_on_queue = (display_end_time > cur_time);
          if (num_expired_frames_on_queue == 0)
            need_to_present_new_frame = true;
        }
      buffer = compositor->get_composition_buffer(buffer_dims);
      double overdue = cur_time - pushed_frame_start_time;
      if (overdue > 0.1)
        { // We are running behind by more than 100ms
          window_manager->broadcast_playclock_adjustment(0.5*overdue);
             // Broadcasting the adjustment to all windows allows multiple
             // video playback windows to keep roughy in sync.
        }
    }
  
  if (need_to_present_new_frame)
    {
      frame_presenter->present_frame(this,display_frame_idx);
      if (cur_time > (last_status_update_time+0.25))
        {
          display_status(display_frame_idx);
          last_status_update_time = cur_time;
        }
    }
  if (have_current_frame_on_queue && compositor->is_processing_complete())
    window_manager->schedule_wakeup(window,this,display_end_time);
}

/*****************************************************************************/
/*                          kdms_renderer::wakeup                            */
/*****************************************************************************/

void kdms_renderer::wakeup(CFAbsoluteTime scheduled_time,
                           CFAbsoluteTime current_time)
{
  if (in_playmode)
    adjust_frame_queue_presentation_and_wakeup(current_time);
  else if (overlays_enabled && overlay_flashing_enabled && (compositor!=NULL))
    { // Flashing overlays
      overlay_flash_wakeup_scheduled = false;
      overlay_painting_colour++;
      compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
            (overlay_painting_colour<<8)+(overlay_painting_intensity & 0xFF));
    }
}

/*****************************************************************************/
/*                      kdms_renderer::refresh_display                       */
/*****************************************************************************/

void kdms_renderer::refresh_display()
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
        buffer = compositor->get_composition_buffer(buffer_dims);
    }
  if (metashow != nil)
    [metashow update_metadata];
}

/*****************************************************************************/
/*                       kdms_renderer::increase_scale                       */
/*****************************************************************************/

float kdms_renderer::increase_scale(float from_scale)
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

/*****************************************************************************/
/*                       kdms_renderer::decrease_scale                       */
/*****************************************************************************/

float kdms_renderer::decrease_scale(float from_scale)
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

/*****************************************************************************/
/*                      kdms_renderer::change_frame_idx                      */
/*****************************************************************************/

void kdms_renderer::change_frame_idx(int new_frame_idx)
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

/*****************************************************************************/
/*                 kdms_renderer::find_compatible_frame_idx                  */
/*****************************************************************************/

int
  kdms_renderer::find_compatible_frame_idx(int num_streams, const int *streams,
                                      int num_layers, const int *layers,
                                      int num_regions, const jpx_roi *regions,
                                      int &compatible_codestream_idx,
                                      int &compatible_layer_idx)
{
  if (!composition_rules)
    composition_rules = jpx_in.access_composition();
  if (!composition_rules)
    return -1; // No frames can be accessed.
  
  int scan_frame_idx, scan_frame_iteration;
  jx_frame *scan_frame;
  int lim_frame_idx=0; // Loop ends when scan_frame_idx advances to this value
  if (frame != NULL)
    {
      lim_frame_idx = scan_frame_idx = this->frame_idx;
      scan_frame_iteration = this->frame_iteration;
      scan_frame = this->frame;
    }
  else
    {
      lim_frame_idx = scan_frame_idx = 0;
      scan_frame_iteration = 0;
      scan_frame = composition_rules.get_next_frame(NULL);
    }
  if (scan_frame == NULL)
    return -1; // Unable to access any frames yet; maybe cache needs more data
  
  bool is_persistent;
  int num_instructions, duration, repeat_count;
  composition_rules.get_frame_info(scan_frame,num_instructions,duration,
                                   repeat_count,is_persistent);
  do {
    if (num_streams > 0)
      { // Have to match codestream
        int n;
        kdu_dims composition_region;
        for (n=0; n < num_streams; n++)
          {
            int r, l_idx, cs_idx = streams[n];
            if (num_regions > 0)
              { // Have to find visible region
                for (r=0; r < num_regions; r++)
                  { // Have to match
                    l_idx = frame_expander.test_codestream_visibility(
                                     &jpx_in,scan_frame,scan_frame_iteration,
                                     true,cs_idx,composition_region,
                                     regions[r].region);
                    if ((l_idx >= 0) && (num_layers > 0))
                      { // Have to match one of the compositing layers
                        for (int p=0; p < num_layers; p++)
                          if (layers[p] == l_idx)
                            { // Found a match
                              compatible_codestream_idx = cs_idx;
                              compatible_layer_idx = l_idx;
                              return scan_frame_idx;
                            }
                      }
                    else if (l_idx >= 0)
                      { // Any compositing layer will do
                        compatible_codestream_idx = cs_idx;
                        compatible_layer_idx = l_idx;
                        return scan_frame_idx;
                      }
                  }
              }
            else
              { // Any visible portion of the codestream will do
                l_idx = frame_expander.test_codestream_visibility(
                                   &jpx_in,scan_frame,scan_frame_iteration,
                                   true,cs_idx,composition_region);
                if ((l_idx >= 0) && (num_layers > 0))
                  { // Have to match one of the compositing layers
                    for (int p=0; p < num_layers; p++)
                      if (layers[p] == l_idx)
                        { // Found a match
                          compatible_codestream_idx = cs_idx;
                          compatible_layer_idx = l_idx;
                          return scan_frame_idx;
                        }
                  }
                else if (l_idx >= 0)
                  { // Any compositing layer will do
                    compatible_codestream_idx = cs_idx;
                    compatible_layer_idx = l_idx;
                    return scan_frame_idx;
                  }
              }
          }
      }
    else
      { // Only have to match compositing layers
        frame_expander.construct(&jpx_in,scan_frame,scan_frame_iteration,true);
        int m, num_members = frame_expander.get_num_members();
        for (m=0; m < num_members; m++)
          {
            bool covers;
            int n, inst_idx, l_idx;
            kdu_dims src_dims, tgt_dims;
            if (frame_expander.get_member(m,inst_idx,l_idx,covers,
                                          src_dims,tgt_dims) == NULL)
              continue;
            for (n=0; n < num_layers; n++)
              if (layers[n] == l_idx)
                {
                  frame_expander.reset();
                  compatible_layer_idx = l_idx;
                  return scan_frame_idx;
                }
          }
      }
    scan_frame_idx++;
    scan_frame_iteration++;
    if (scan_frame_iteration > repeat_count)
      { // Need to access next frame
        scan_frame = composition_rules.get_next_frame(scan_frame);
        if (scan_frame == NULL)
          {
            scan_frame = composition_rules.get_next_frame(NULL);
            scan_frame_idx = 0;
            assert(scan_frame != NULL);
          }
        scan_frame_iteration = 0;
        composition_rules.get_frame_info(scan_frame,num_instructions,
                                         duration,repeat_count,
                                         is_persistent);
      }
  } while (scan_frame_idx != lim_frame_idx);

  return -1; // Didn't find any match          
}

/*****************************************************************************/
/*              kdms_renderer::convert_region_to_focus_anchors               */
/*****************************************************************************/

void kdms_renderer::convert_region_to_focus_anchors(kdu_dims region,
                                                    kdu_dims ref)
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

/*****************************************************************************/
/*              kdms_renderer::convert_focus_anchors_to_region               */
/*****************************************************************************/

void kdms_renderer::convert_focus_anchors_to_region(kdu_dims &region,
                                                    kdu_dims ref)
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

/*****************************************************************************/
/*                    kdms_renderer::adjust_focus_anchors                    */
/*****************************************************************************/

bool kdms_renderer::adjust_focus_anchors()
{
  if (image_dims.is_empty() ||
      !(focus_anchors_known && configuration_complete))
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

/*****************************************************************************/
/*                  kdms_renderer::calculate_view_anchors                    */
/*****************************************************************************/

void kdms_renderer::calculate_view_anchors()
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

/*****************************************************************************/
/*                  kdms_renderer::scroll_to_view_anchors                    */
/*****************************************************************************/

void kdms_renderer::scroll_to_view_anchors()
{
  if (!view_centre_known)
    return;
  NSSize doc_size = [doc_view frame].size;
  NSRect view_rect = [scroll_view documentVisibleRect];
  NSPoint new_view_origin;
  new_view_origin.x = (float)
    floor(doc_size.width * view_centre_x - 0.5F*view_rect.size.width);
  new_view_origin.y = (float)
    floor(doc_size.height*(1.0F-view_centre_y)-0.5F*view_rect.size.height);
  view_centre_known = false;
  [doc_view scrollPoint:new_view_origin];
}

/*****************************************************************************/
/*                 kdms_renderer::scroll_relative_to_view                    */
/*****************************************************************************/

void kdms_renderer::scroll_relative_to_view(float h_scroll, float v_scroll)
{
  NSSize doc_size = [doc_view frame].size;
  NSRect view_rect = [scroll_view documentVisibleRect];
  NSPoint new_view_origin = view_rect.origin;
  
  h_scroll *= view_rect.size.width;
  v_scroll *= view_rect.size.height;
  if (h_scroll > 0.0F)
    h_scroll = ((float) ceil(h_scroll / pixel_scale)) * pixel_scale;
  else if (h_scroll < 0.0F)
    h_scroll = ((float) floor(h_scroll / pixel_scale)) * pixel_scale;
  if (v_scroll > 0.0F)
    v_scroll = ((float) ceil(v_scroll / pixel_scale)) * pixel_scale;
  else if (v_scroll < 0.0F)
    v_scroll = ((float) floor(v_scroll / pixel_scale)) * pixel_scale;
  
  new_view_origin.x += h_scroll;
  new_view_origin.y -= v_scroll;
  if (new_view_origin.x < 0.0F)
    new_view_origin.x = 0.0F;
  if ((new_view_origin.x + view_rect.size.width) > doc_size.width)
    new_view_origin.x = doc_size.width - view_rect.size.width;
  if ((new_view_origin.y + view_rect.size.height) > doc_size.height)
    new_view_origin.y = doc_size.height - view_rect.size.height;
  [doc_view scrollPoint:new_view_origin];
}

/*****************************************************************************/
/*                     kdms_renderer::scroll_absolute                        */
/*****************************************************************************/

void kdms_renderer::scroll_absolute(float h_scroll, float v_scroll,
                                    bool use_doc_coords)
{
  if (!use_doc_coords)
    {
      h_scroll *= pixel_scale;
      v_scroll *= pixel_scale;
      v_scroll = -v_scroll;
    }
  
  NSSize doc_size = [doc_view frame].size;
  NSRect view_rect = [scroll_view documentVisibleRect];
  NSPoint new_view_origin = view_rect.origin;
  new_view_origin.x += h_scroll;
  new_view_origin.y += v_scroll;
  if (new_view_origin.x < 0.0F)
    new_view_origin.x = 0.0F;
  if ((new_view_origin.x + view_rect.size.width) > doc_size.width)
    new_view_origin.x = doc_size.width - view_rect.size.width;
  if ((new_view_origin.y + view_rect.size.height) > doc_size.height)
    new_view_origin.y = doc_size.height - view_rect.size.height;
  [doc_view scrollPoint:new_view_origin];
}

/*****************************************************************************/
/*                     kdms_renderer::get_new_focus_box                      */
/*****************************************************************************/

kdu_dims kdms_renderer::get_new_focus_box()
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
    new_focus_box.pos.x =
      view_dims.pos.x+view_dims.size.x-new_focus_box.size.x;
  if (new_focus_box.pos.y < view_dims.pos.y)
    new_focus_box.pos.y = view_dims.pos.y;
  if ((new_focus_box.pos.y+new_focus_box.size.y) >
      (view_dims.pos.y+view_dims.size.y))
    new_focus_box.pos.y =
      view_dims.pos.y+view_dims.size.y-new_focus_box.size.y;
  new_focus_box &= view_dims;
  
  if (!new_focus_box)
    new_focus_box = view_dims;
  return new_focus_box;
}

/*****************************************************************************/
/*                      kdms_renderer::set_focus_box                         */
/*****************************************************************************/

void kdms_renderer::set_focus_box(kdu_dims new_box)
{
  if ((compositor == NULL) || (buffer == NULL))
    return;
  kdu_dims previous_box = focus_box;
  bool previous_enable = enable_focus_box;
  if (new_box.area() < (kdu_long) 100)
    enable_focus_box = false;
  else
    { focus_box = new_box; enable_focus_box = true; }
  focus_box = get_new_focus_box();
  enable_focus_box = (focus_box != view_dims);
  if (previous_enable != enable_focus_box)
    [doc_view setNeedsDisplay:YES];
  else if (enable_focus_box)
    {
      for (int p=0; p < 2; p++)
        {
          NSRect repaint_rect =
            convert_region_to_doc_view((p==0)?previous_box:focus_box);
          repaint_rect.origin.x -= KDMS_FOCUS_MARGIN;
          repaint_rect.origin.y -= KDMS_FOCUS_MARGIN;
          repaint_rect.size.width += 2.0F*KDMS_FOCUS_MARGIN;
          repaint_rect.size.height += 2.0F*KDMS_FOCUS_MARGIN;
          [doc_view setNeedsDisplayInRect:repaint_rect];
        }
    }
}

/*****************************************************************************/
/*                    kdms_renderer::need_focus_outline                      */
/*****************************************************************************/

bool kdms_renderer::need_focus_outline(kdu_dims *focus_box)
{
  *focus_box = this->focus_box;
  return enable_focus_box;
}

/*****************************************************************************/
/*                      kdms_renderer::update_focus_box                      */
/*****************************************************************************/

void kdms_renderer::update_focus_box(bool view_may_be_scrolling)
{
  kdu_dims new_focus_box = get_new_focus_box();
  bool new_focus_enable = (new_focus_box != view_dims);
  if ((new_focus_box != focus_box) || (new_focus_enable != enable_focus_box))
    {
      bool previous_enable = enable_focus_box;
      kdu_dims previous_box = focus_box;
      focus_box = new_focus_box;
      enable_focus_box = new_focus_enable;
      if ((previous_enable != enable_focus_box) ||
          (enable_focus_box && view_may_be_scrolling))
        [doc_view setNeedsDisplay:YES];
      else if (enable_focus_box)
        {
          for (int p=0; p < 2; p++)
            {
              NSRect repaint_rect =
              convert_region_to_doc_view((p==0)?previous_box:focus_box);
              repaint_rect.origin.x -= KDMS_FOCUS_MARGIN;
              repaint_rect.origin.y -= KDMS_FOCUS_MARGIN;
              repaint_rect.size.width += 2.0F*KDMS_FOCUS_MARGIN;
              repaint_rect.size.height += 2.0F*KDMS_FOCUS_MARGIN;
              [doc_view setNeedsDisplayInRect:repaint_rect];
            }
        }
    }
}

/*****************************************************************************/
/*                      kdms_renderer::display_status                        */
/*****************************************************************************/

void kdms_renderer::display_status(int playmode_frame_idx)
{
  int p;
  char char_buf[300];
  char *panel_text[3] = {char_buf,char_buf+100,char_buf+200};
  for (p=0; p < 3; p++)
    *(panel_text[p]) = '\0'; // Reset all panels to empty strings.
  
  // Find resolution information
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
    
  // Fill in status pane 0 with resolution information
  if (compositor != NULL)
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
        sprintf(panel_text[0],"Res=%1.1f%%",res_percent);
      else if ((x_percent > (0.99F*y_percent)) &&
               (x_percent < (1.01F*y_percent)))
        sprintf(panel_text[0],"Res=%1.1f%% (%1.1f%%)",res_percent,x_percent);
      else
        sprintf(panel_text[0],"Res=%1.1f%% (x:%1.1f%%,y:%1.1f%%)",
                res_percent,x_percent,y_percent);
    }
  
  // Fill in status pane 1 with image/component/frame information
  if (compositor != NULL)
    {
      char *sp = panel_text[1];
      if (single_component_idx >= 0)
        {
          sprintf(sp,"Comp=%d/",single_component_idx+1);
          sp += strlen(sp);
          if (max_components <= 0)
            *(sp++) = '?';
          else
            { sprintf(sp,"%d",max_components); sp += strlen(sp); }
          sprintf(sp,", Stream=%d/",single_codestream_idx+1);
          sp += strlen(sp);
          if (max_codestreams <= 0)
            strcpy(sp,"?");
          else
            sprintf(sp,"%d",max_codestreams);
        }
      else if ((single_layer_idx >= 0) && !mj2_in)
        { // Report compositing layer index
          sprintf(sp,"C.Layer=%d/",single_layer_idx+1);
          sp += strlen(sp);
          if (max_compositing_layer_idx < 0)
            strcpy(sp,"?");
          else
            sprintf(sp,"%d",max_compositing_layer_idx+1);
        }
      else if ((single_layer_idx >= 0) && mj2_in.exists())
        { // Report track and frame indices, rather than layer index
          sprintf(sp,"Trk:Frm=%d:%d",single_layer_idx+1,
                  (playmode_frame_idx<0)?(frame_idx+1):(playmode_frame_idx+1));
        }
      else
        {
          sprintf(sp,"Frame=%d/",
                  (playmode_frame_idx<0)?(frame_idx+1):(playmode_frame_idx+1));
          sp += strlen(sp);
          if (num_frames <= 0)
            strcpy(sp,"?");
          else
            sprintf(sp,"%d",num_frames);
        }   
    }

  if (status_id == KDS_STATUS_CACHE)
    status_id = KDS_STATUS_LAYER_RES;
  
  // Fill in status pane 2
  if ((compositor != NULL) && (status_id == KDS_STATUS_LAYER_RES))
    {
      if (!compositor->is_processing_complete())
        strcpy(panel_text[2],"WORKING");
      else
        {
          int max_layers = compositor->get_max_available_quality_layers();
          if (max_display_layers >= max_layers)
            sprintf(panel_text[2],"Q.Layers=all ");
          else
            sprintf(panel_text[2],"Q.Layers=%d/%d ",
                    max_display_layers,max_layers);
        }
    }
  else if ((compositor != NULL) && (status_id == KDS_STATUS_DIMS))
    {
      if (!compositor->is_processing_complete())
        strcpy(panel_text[2],"WORKING");
      else
        sprintf(panel_text[2],"HxW=%dx%d ",
                image_dims.size.y,image_dims.size.x);
    }
  else if ((compositor != NULL) && (status_id == KDS_STATUS_MEM))
    {
      if (!compositor->is_processing_complete())
        strcpy(panel_text[2],"WORKING");
      else
        {
          kdu_long bytes = 0;
          kdrc_stream *str=NULL;
          while ((str=compositor->get_next_codestream(str,false,true)) != NULL)
            {
              kdu_codestream ifc = compositor->access_codestream(str);
              bytes += ifc.get_compressed_data_memory()
                     + ifc.get_compressed_state_memory();
            }
          /*
           if (jpip_client.is_active())
           bytes += jpip_client.get_peak_cache_memory();
           */
          sprintf(panel_text[2],"%5g MB compressed data memory",1.0E-6*bytes);
        }
    }
  else if ((compositor != NULL) && (status_id == KDS_STATUS_PLAYSTATS))
    {
      double avg_frame_rate = playmode_stats.get_avg_frame_rate();
      if (avg_frame_rate > 0.0)
        sprintf(panel_text[2],"%4.2g fps",avg_frame_rate);
    }

  [window set_status_strings:panel_text];
}

/*****************************************************************************/
/*                      kdms_renderer::set_codestream                        */
/*****************************************************************************/

void kdms_renderer::set_codestream(int codestream_idx)
{
  calculate_view_anchors();
  if ((single_component_idx<0) && focus_anchors_known &&
      !image_dims.is_empty())
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

/*****************************************************************************/
/*                  kdms_renderer::set_compositing_layer                     */
/*****************************************************************************/

void kdms_renderer::set_compositing_layer(int layer_idx)
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

/*****************************************************************************/
/*                  kdms_renderer::set_compositing_layer                     */
/*****************************************************************************/

void kdms_renderer::set_compositing_layer(kdu_coords point)
{
  if (compositor == NULL)
    return;
  point += view_dims.pos;
  int layer_idx, stream_idx;
  if (compositor->find_point(point,layer_idx,stream_idx) && (layer_idx >= 0))
    set_compositing_layer(layer_idx);
}

/*****************************************************************************/
/*                        kdms_renderer::set_frame                           */
/*****************************************************************************/

bool kdms_renderer::set_frame(int new_frame_idx)
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
  else
    {
      invalidate_configuration();
      initialize_regions();
    }
  return true;
}

/*****************************************************************************/
/*                      kdms_renderer::stop_playmode                         */
/*****************************************************************************/

void kdms_renderer::stop_playmode()
{
  if (!in_playmode)
    return;
  playmode_reverse = false;
  pushed_last_frame = false;
  in_playmode = false;
  if (status_id == KDS_STATUS_PLAYSTATS)
    status_id = KDS_STATUS_LAYER_RES;
  window_manager->schedule_wakeup(window,NULL,0); // Cancel any pending wakeups
  frame_presenter->reset(); // Blocking call
  compositor->set_surface_initialization_mode(true);
  if (compositor != NULL)
    compositor->flush_composition_queue();
  num_expired_frames_on_queue = 0;
  if (compositor != NULL)
    {
      buffer = compositor->get_composition_buffer(buffer_dims);
      if (!compositor->is_processing_complete())
        compositor->refresh(); // Make sure the surface is properly initialized
    }
  [doc_view setNeedsDisplay:YES];
  display_status();
}

/*****************************************************************************/
/*                     kdms_renderer::start_playmode                         */
/*****************************************************************************/

void kdms_renderer::start_playmode(bool reverse)
{
  if (!source_supports_playmode())
    return;
  if ((single_component_idx >= 0) ||
      ((single_layer_idx >= 0) && jpx_in.exists()))
    return;
  
  compositor->flush_composition_queue();
  num_expired_frames_on_queue = 0;

  // Make sure we have valid `frame_start' and `frame_end' times
  if (frame_end <= frame_start)
    change_frame_idx(frame_idx); // Generate new `frame_end' time

  // Start the clock running
  this->playmode_reverse = reverse;
  this->playclock_base = CFAbsoluteTimeGetCurrent();
  this->playclock_offset = 0.0;
  this->playclock_offset = playclock_base - get_absolute_frame_start_time();
  pushed_last_frame = false;
  in_playmode = true;
  last_status_update_time = playclock_base;
  playmode_stats.reset();
  status_id = KDS_STATUS_PLAYSTATS;
  overlay_flash_wakeup_scheduled = false; // So overlay flashing can start
          // again normally once we stop playmode
  
  // Setting things up to decode from scratch means that the image dimensions
  // may possibly change.  Actually, I don't think this can happen unless
  // the `start_playmode' function is called while we are already in
  // playmode, since then we may have been displaying a different frame.  In
  // any case, it doesn't do any harm to re-initialize the buffer and viewport
  // on entering playback mode.
  calculate_view_anchors();
  view_dims = buffer_dims = kdu_dims();
  buffer = NULL;
  if (adjust_focus_anchors())
    { view_centre_x = focus_centre_x; view_centre_y = focus_centre_y; }
  [doc_view setNeedsDisplay:YES];
  size_window_for_image_dims(false);
}

/*****************************************************************************/
/*                kdms_renderer::set_metadata_catalog_source                 */
/*****************************************************************************/

void kdms_renderer::set_metadata_catalog_source(kdms_catalog *source)
{
  if ((source == nil) && (this->catalog_source != nil))
    catalog_closed = true;
  if (this->catalog_source != nil)
    [this->catalog_source release];
  this->catalog_source = source;
  if ((source != nil) && jpx_in.exists())
    {
      jpx_meta_manager meta_manager = jpx_in.access_meta_manager();
      meta_manager.access_root().touch();
      [source update:meta_manager];
    }
}

/*****************************************************************************/
/*                  kdms_renderer::edit_metadata_at_point                    */
/*****************************************************************************/

void kdms_renderer::edit_metadata_at_point(kdu_coords *point_ref)
{
  if ((compositor == NULL) || !jpx_in)
    return;
  
  kdu_coords point; // Will hold mouse location in `view_dims' coordinates
  if (point_ref != NULL)
    point = *point_ref;
  else
    {
      NSRect ns_rect;
      ns_rect.origin = [window mouseLocationOutsideOfEventStream];
      ns_rect.origin = [doc_view convertPoint:ns_rect.origin fromView:nil];
      ns_rect.size.width = ns_rect.size.height = 0.0F;
      point = convert_region_from_doc_view(ns_rect).pos;
    }
  
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
    return; // Nothing to edit; user needs to add metadata
  
  if (file_server == NULL)
    file_server = new kdms_file_services(open_file_pathname);
  kdms_metadata_editor *editor = [kdms_metadata_editor alloc];
  int counted_codestreams=0, counted_compositing_layers=0;
  jpx_in.count_codestreams(counted_codestreams);
  jpx_in.count_compositing_layers(counted_compositing_layers);
  [editor initWithNumCodestreams:counted_codestreams
                          layers:counted_compositing_layers
                     metaManager:manager
                    fileServices:file_server
                        editable:YES];
  [editor configureWithEditList:&metalist];
  if ([editor run_modal:this])
    { // Above function invokes `metadata_changed' as required.
      have_unsaved_edits = true;
    }
  
  [editor release]; // Cleans up properly, even if an exception occurred
}

/*****************************************************************************/
/*                      kdms_renderer::edit_metanode                         */
/*****************************************************************************/

void kdms_renderer::edit_metanode(jpx_metanode node)
{
  if ((compositor == NULL) || (!jpx_in) || (!node) || node.is_deleted())
    return;
  jpx_meta_manager manager = jpx_in.access_meta_manager();
  kdms_matching_metalist metalist;
  metalist.append_node_expanding_numlists_and_free_boxes(node);
  if (metalist.get_head() == NULL)
    return; // Nothing to edit; user needs to add metadata
  
  if (file_server == NULL)
    file_server = new kdms_file_services(open_file_pathname);
  kdms_metadata_editor *editor = [kdms_metadata_editor alloc];
  int counted_codestreams=0, counted_compositing_layers=0;
  jpx_in.count_codestreams(counted_codestreams);
  jpx_in.count_compositing_layers(counted_compositing_layers);
  [editor initWithNumCodestreams:counted_codestreams
                          layers:counted_compositing_layers
                     metaManager:manager
                    fileServices:file_server
                        editable:YES];
  [editor configureWithEditList:&metalist];
  if ([editor run_modal:this])
    { // Above function invokes `metadata_changed' as required.
      have_unsaved_edits = true;
    }  
  [editor release]; // Cleans up properly, even if an exception occurred  
}

/*****************************************************************************/
/*                kdms_renderer::show_imagery_for_metanode                   */
/*****************************************************************************/

void kdms_renderer::show_imagery_for_metanode(jpx_metanode node)
{
  if ((compositor == NULL) || (!jpx_in) || (!node) || !configuration_complete)
    return;
  
  bool rendered_result=false;
  int num_streams=0, num_layers=0, num_regions=0;
  jpx_metanode numlist, roi;
  for (; node.exists() && !(numlist.exists() && roi.exists());
       node=node.get_parent())
    if ((!numlist) &&
        node.get_numlist_info(num_streams,num_layers,rendered_result))
      numlist = node;
    else if ((!roi) && ((num_regions = node.get_num_regions()) > 0))
      roi = node;

  const int *layers = NULL;
  const int *streams = NULL;
  if (numlist.exists())
    {
      streams = numlist.get_numlist_codestreams();
      layers = numlist.get_numlist_layers();
    }
  else if (!roi)
    return; // Nothing to do

  bool have_unassociated_roi = roi.exists() && (streams == NULL);
  if (have_unassociated_roi)
    { // An ROI must be associated with one or more codestreams, or else it
      // must be associated with nothing other than the rendered result (an
      // unassociated ROI).  This last case is probably rare, but we
      // accommodate it here anyway, by enforcing the lack of association
      // with anything other than the rendered result.
      num_layers = num_streams = 0;
      layers = NULL;
    }
  
  if (!composition_rules)
    composition_rules = jpx_in.access_composition();
  bool match_any_frame = false;
  bool match_any_layer = false;
  if (rendered_result || have_unassociated_roi)
    {
      if (composition_rules.exists() &&
          (composition_rules.get_next_frame(NULL) == NULL))
        match_any_layer = true;
      else
        match_any_frame = true;
    }

  bool have_compatible_layer = false;
  int compatible_codestream=-1; // This is the codestream against which
                                // we will assess the `roi' if there is one
  int compatible_codestream_layer=-1; // This is the compositing layer against
                                // which we should assess the `roi' if any.
  
  int m, n, c, p;
  if (streams == NULL)
    { // Compatible imagery consists either of a rendered result of a
      // single compositing layer only.  This may require us to display
      // a more complex object than is currently being rendered.  Also,
      // because `num_streams' is 0, we only need to match ROI's, not
      // image entities.
      if ((num_layers == 0) && match_any_frame)
        { // Only a composited frame will be compatible with the metadata
          if (!composition_rules)
            return; // Waiting for more data from the cache before navigating
          set_frame(0);
          if (!have_unassociated_roi)
            return;
        }
      else if ((single_component_idx >= 0) && match_any_layer)
        {
          set_compositing_layer(0);
          if (!have_unassociated_roi)
            return;
        }
      else if ((single_component_idx >= 0) && (num_layers > 0))
        { // Need to display a compositing layer.  If possible, display one
          // which contains the currently displayed codestream.
          int layer_idx = -1;
          for (m=0; (m < num_layers) && (layer_idx < 0); m++)
            for (n=0; (c=jpx_in.get_layer_codestream_id(layers[m],n))>=0; n++)
              if (c == single_codestream_idx)
                { layer_idx = layers[m]; break; }
          set_compositing_layer((layer_idx >= 0)?layer_idx:layers[0]);
          return;
        }
    }
  if (have_unassociated_roi)
    have_compatible_layer = true; // We have already bumped things up as req'd.
  const jpx_roi *regions = NULL;
  if (roi.exists())
    regions = roi.get_regions();
  
  // Now see what changes are required to match the image entities
  if ((frame != NULL) && !have_compatible_layer)
    { // We are displaying a frame.  Let's try to keep doing that.
      int compatible_frame_idx = -1;
      if (match_any_frame && !roi)
        return; // Current frame is just fine; nothing else to do
      compatible_frame_idx = // Must find a matching codestream.
        find_compatible_frame_idx(num_streams,streams,num_layers,layers,
                                  num_regions,regions,compatible_codestream,
                                  compatible_codestream_layer);
      if ((compatible_frame_idx < 0) && (num_layers > 0) && (num_streams > 0))
        { // Try matching just the codestreams or just the layers
          if (regions == NULL)
            compatible_frame_idx =
              find_compatible_frame_idx(0,NULL,num_layers,layers,0,NULL,
                                        compatible_codestream,
                                        compatible_codestream_layer);
          if (compatible_frame_idx < 0)
            find_compatible_frame_idx(num_streams,streams,0,NULL,
                                      num_regions,regions,
                                      compatible_codestream,
                                      compatible_codestream_layer);
        }
      if (compatible_frame_idx >= 0)
        {
          have_compatible_layer = true;
          if (compatible_frame_idx != frame_idx)
            set_frame(compatible_frame_idx);
        }
    }

  if ((single_layer_idx >= 0) && !have_compatible_layer)
    { // We are already displaying a single compositing layer; try to keep
      // doing this.
      if (match_any_layer)
        have_compatible_layer = true;
      else
        for (n=0; n < num_layers; n++)
          if (layers[n] == single_layer_idx)
            { have_compatible_layer = true; break; }      
      for (n=0;
           ((c=jpx_in.get_layer_codestream_id(single_layer_idx,n)) >= 0) &&
           (compatible_codestream < 0); n++)
        for (p=0; p < num_streams; p++)
          if (c == streams[p])
            { compatible_codestream = c; have_compatible_layer=true; break; }
      if (roi.exists() && (compatible_codestream < 0))
        have_compatible_layer = false;
    }

  if (have_compatible_layer && !roi)
    return;
  
  if ((single_component_idx < 0) && !have_compatible_layer)
    { // See if we can change to any compositing layer which is compatible
      int layer_idx = -1;
      for (m=0; (m < num_layers) && !have_compatible_layer; m++)
        {
          layer_idx = layers[m];
          for (n=0; ((c=jpx_in.get_layer_codestream_id(layer_idx,n)) >= 0) &&
                    (compatible_codestream < 0); n++)
            for (p=0; p < num_streams; p++)
              if (c == streams[p])
                { compatible_codestream=c; break; }
          have_compatible_layer = (compatible_codestream >= 0) || !roi;
        }
      int count_layers=1;
      jpx_in.count_compositing_layers(count_layers);
      for (m=0; (m < count_layers) && !have_compatible_layer; m++)
        {
          layer_idx = m;
          for (n=0; ((c=jpx_in.get_layer_codestream_id(layer_idx,n)) >= 0) &&
                    (compatible_codestream < 0); n++)
            for (p=0; p < num_streams; p++)
              if (c == streams[p])
                { compatible_codestream=c; break; }
          have_compatible_layer = ((compatible_codestream >= 0) ||
                                   (match_any_layer && !roi));
        }
      if (have_compatible_layer)
        set_compositing_layer(layer_idx);
    }
  
  if (!have_compatible_layer)
    { // See if we can display a single codestream which is compatible; it
      // is possible that we are already displaying a single compatible
      // codestream.
      for (n=0; n < num_streams; n++)
        if (streams[n] == single_codestream_idx)
          { compatible_codestream = single_codestream_idx; break; }
      if ((compatible_codestream < 0) && (num_streams > 0))
        compatible_codestream = streams[0];
      if (compatible_codestream >= 0)
        set_codestream(compatible_codestream);
    }
  
  if ((!roi) || (!configuration_complete) ||
      ((compatible_codestream < 0) && !have_unassociated_roi))
    return;  // Nothing we can do or perhaps need to do.
  
  // Now we need to find the region of interest on the compatible codestream
  // (or the rendered result if we have an unassociated ROI).
  kdu_dims roi_region = roi.get_bounding_box();
  kdu_dims mapped_roi_region =
    compositor->inverse_map_region(roi_region,compatible_codestream,
                                   compatible_codestream_layer);
  mapped_roi_region &= image_dims;
  if (!mapped_roi_region)
    return; // Region does not appear to intersect with the image surface.
  while ((mapped_roi_region.size.x > view_dims.size.x) ||
         (mapped_roi_region.size.y > view_dims.size.y))
    {
      if (rendering_scale == min_rendering_scale)
        break;
      rendering_scale = decrease_scale(rendering_scale);
      calculate_view_anchors();
      view_dims = kdu_dims(); // Force full window init in `initialize_regions'
      initialize_regions();
      mapped_roi_region =
        compositor->inverse_map_region(roi_region,compatible_codestream,
                                       compatible_codestream_layer);
    }
  while ((mapped_roi_region.size.x < (view_dims.size.x >> 1)) &&
         (mapped_roi_region.size.y < (view_dims.size.y >> 1)) &&
         (mapped_roi_region.area() < 400))
    {
      rendering_scale = increase_scale(rendering_scale);
      calculate_view_anchors();
      view_dims = kdu_dims(); // Force full window init in `initialize_regions'
      initialize_regions();
      mapped_roi_region =
        compositor->inverse_map_region(roi_region,compatible_codestream,
                                       compatible_codestream_layer);
    }
  if ((mapped_roi_region & view_dims) != mapped_roi_region)
    { // The ROI region is not completely visible; centre the view over the
      // region then.
      convert_region_to_focus_anchors(mapped_roi_region,image_dims);
      view_centre_x = focus_centre_x;
      view_centre_y = focus_centre_y;
      view_centre_known = true;
      focus_anchors_known = false; // Don't need computed focus anchors here
      scroll_to_view_anchors();
    }
  focus_anchors_known = false;
  set_focus_box(mapped_roi_region);
}

/*****************************************************************************/
/*                     kdms_renderer::metadata_changed                       */
/*****************************************************************************/

void kdms_renderer::metadata_changed(bool new_data_only)
{
  if (compositor == NULL)
    return;
  compositor->update_overlays(!new_data_only);
  if (metashow != nil)
    [metashow update_metadata];
  if ((catalog_source == nil) && jpx_in.exists() &&
      jpx_in.access_meta_manager().peek_touched_nodes(jp2_label_4cc).exists())
    [window create_metadata_catalog];
  else if (catalog_source != nil)
    {
      jpx_meta_manager meta_manager = jpx_in.access_meta_manager();
      [catalog_source update:meta_manager];
    }
}

/*****************************************************************************/
/*                       kdms_renderer::save_as_jp2                          */
/*****************************************************************************/

void kdms_renderer::save_as_jp2(const char *filename)
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

/*****************************************************************************/
/*                       kdms_renderer::save_as_jpx                          */
/*****************************************************************************/

void kdms_renderer::save_as_jpx(const char *filename,
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
        jpx_out.write_headers(NULL,NULL,n);
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

/*****************************************************************************/
/*                       kdms_renderer::save_as_raw                          */
/*****************************************************************************/

void kdms_renderer::save_as_raw(const char *filename)
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

/*****************************************************************************/
/*                     kdms_renderer::copy_codestream                        */
/*****************************************************************************/

void kdms_renderer::copy_codestream(kdu_compressed_target *tgt,
                                    kdu_codestream src)
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

/*****************************************************************************/
/*                        kdms_renderer::save_over                           */
/*****************************************************************************/

bool kdms_renderer::save_over()
{
  const char *save_pathname =
    window_manager->get_save_file_pathname(open_file_pathname);
  const char *suffix = strrchr(open_filename,'.');
  
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
    window_manager->declare_save_file_invalid(save_pathname);
    return false;
  }
  return true;
}

/*****************************************************************************/
/*                      kdms_renderer::menu_FileSave                         */
/*****************************************************************************/

void kdms_renderer::menu_FileSave()
{
  if ((compositor == NULL) || in_playmode || mj2_in.exists() ||
      (!have_unsaved_edits) || (open_file_pathname == NULL))
    return;
  if (!save_without_asking)
    {
      int response =
        interrogate_user("You are about to save to the file which you are "
                         "currently viewing.  In practice, the saved data "
                         "will be written to a temporary file which will "
                         "not replace the open file until after you close "
                         "it the window or load another file into it.  "
                         "Nevertheless, this operation will eventually "
                         "overwrite the existing image file.  In the process, "
                         "there is a chance that you might lose some "
                         "metadata which could not be internally "
                         "represented.  Are you sure you wish to do this?",
                         "Cancel","Proceed","Don't ask me again");
      if (response == 0)
        return;
      if (response == 2)
        save_without_asking = true;
    }
  save_over();
}

/*****************************************************************************/
/*                   kdms_renderer::menu_FileSaveReload                      */
/*****************************************************************************/

void kdms_renderer::menu_FileSaveReload()
{
  if ((compositor == NULL) || in_playmode || mj2_in.exists() ||
      (!have_unsaved_edits) || (open_file_pathname == NULL) ||
      (window_manager->get_open_file_retain_count(open_file_pathname) != 1))
    return;
  if (!save_over())
    return;
  NSString *filename = [NSString stringWithUTF8String:open_file_pathname];
  bool save_without_asking = this->save_without_asking;
  close_file();
  open_file([filename UTF8String]);
  this->save_without_asking = save_without_asking;
}

/*****************************************************************************/
/*                     kdms_renderer::menu_FileSaveAs                        */
/*****************************************************************************/

void kdms_renderer::menu_FileSaveAs(bool preserve_codestream_links,
                                    bool force_codestream_links)
{
  if ((compositor == NULL) || in_playmode)
    return;

  // Get the file name
  NSString *directory = nil;
  if (last_save_pathname != NULL)
    directory = [NSString stringWithUTF8String:last_save_pathname];
  else if (open_file_pathname != NULL)
    directory = [NSString stringWithUTF8String:open_file_pathname];
  if (directory != NULL)
    directory = [directory stringByDeletingLastPathComponent];

  NSString *suggested_file = nil;
  const char *suffix = NULL;
  if (last_save_pathname != NULL)
    {
      suggested_file = [NSString stringWithUTF8String:last_save_pathname];
      suffix = strrchr(last_save_pathname,'.');
      suggested_file = [suggested_file lastPathComponent];
    }
  else
    {
      suggested_file = [NSString stringWithUTF8String:open_filename];
      suffix = strrchr(open_filename,'.');
      suggested_file =
        [suggested_file stringByReplacingPercentEscapesUsingEncoding:
         NSASCIIStringEncoding];
    }
  if (suggested_file == nil)
    suggested_file = @"";
  
  int s;
  char suggested_file_suffix[5] = {'\0','\0','\0','\0','\0'};
  if ((suffix != NULL) && (strlen(suffix) <= 4)) 
    for (s=0; (s < 4) && (suffix[s+1] != '\0'); s++)
      suggested_file_suffix[s] = (char) tolower(suffix[s+1]);
  
  NSArray *file_types = nil;
  if (jpx_in.exists())
    {
      if (force_codestream_links)
        file_types = [NSArray arrayWithObjects:@"jpx",@"jpf",nil];
      else
        file_types = [NSArray arrayWithObjects:
                      @"jp2",@"jpx",@"jpf",@"j2c",@"j2k",nil];
      if (have_unsaved_edits &&
          (strcmp(suggested_file_suffix,"jpx") != 0) &&
          (strcmp(suggested_file_suffix,"jpf") != 0))
        suggested_file = [[suggested_file stringByDeletingPathExtension]
                          stringByAppendingPathExtension:@"jpx"];
    }
  else if (mj2_in.exists())
    {
      file_types = [NSArray arrayWithObjects:@"jp2",@"j2c",@"j2k",nil];
      if (strcmp(suggested_file_suffix,"mj2") != 0)
        suggested_file = [[suggested_file stringByDeletingPathExtension]
                          stringByAppendingPathExtension:@"jp2"];
    }
  else
    {
      file_types = [NSArray arrayWithObjects:@"j2c",@"j2k",nil]; 
      if ((strcmp(suggested_file_suffix,"j2c") != 0) &&
          (strcmp(suggested_file_suffix,"j2k") != 0))
        suggested_file = [[suggested_file stringByDeletingPathExtension]
                          stringByAppendingPathExtension:@"j2c"];
    }
  
  NSSavePanel *panel = [NSSavePanel savePanel];
  [panel setTitle:@"Select filename for saving"];
  [panel setCanCreateDirectories:YES];
  [panel setCanSelectHiddenExtension:YES];
  [panel setExtensionHidden:NO];
  [panel setAllowedFileTypes:file_types];
  [panel setAllowsOtherFileTypes:NO];
  if ([panel runModalForDirectory:directory file:suggested_file] != NSOKButton)
    return;
  
  // Remember the last saved file name
  const char *chosen_pathname = [[panel filename] UTF8String];
  suffix = strrchr(chosen_pathname,'.');
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
  try { // Protect the entire file writing process against exceptions
    bool may_overwrite_link_source = false;
    if (force_codestream_links && jpx_suffix && (open_file_pathname != NULL))
      if (kdms_compare_file_pathnames(chosen_pathname,open_file_pathname))
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
              if (kdms_compare_file_pathnames(chosen_pathname,url_name))
                may_overwrite_link_source = true;
          }
      }
    if (may_overwrite_link_source)
      { kdu_error e; e << "You are about to overwrite an existing file "
        "which is being linked by (or potentially about to be linked by) "
        "the file you are saving.  This operation is too dangerous to "
        "attempt."; }
    save_pathname = window_manager->get_save_file_pathname(chosen_pathname);
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
      window_manager->declare_save_file_invalid(save_pathname);
  }
}

/*****************************************************************************/
/*                   kdms_renderer::menu_FileProperties                      */
/*****************************************************************************/

void kdms_renderer::menu_FileProperties() 
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
  
  NSRect my_rect = [window frame];
  NSPoint top_left_point = my_rect.origin;
  top_left_point.x += my_rect.size.width * 0.1F;
  top_left_point.y += my_rect.size.height * 0.9F;
  
  kdms_properties *properties_dialog = [kdms_properties alloc];
  if (![properties_dialog initWithTopLeftPoint:top_left_point
                                the_codestream:codestream
                              codestream_index:c_idx])
    close_file();
  else
    [properties_dialog run_modal];
  [properties_dialog release];
}

/*****************************************************************************/
/*                      kdms_renderer::menu_ViewVflip                        */
/*****************************************************************************/

void kdms_renderer::menu_ViewVflip() 
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

/*****************************************************************************/
/*                      kdms_renderer::menu_ViewHflip                        */
/*****************************************************************************/

void kdms_renderer::menu_ViewHflip() 
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

/*****************************************************************************/
/*                     kdms_renderer::menu_ViewRotate                        */
/*****************************************************************************/

void kdms_renderer::menu_ViewRotate() 
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

/*****************************************************************************/
/*                 kdms_renderer::menu_ViewCounterRotate                     */
/*****************************************************************************/

void kdms_renderer::menu_ViewCounterRotate() 
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

/*****************************************************************************/
/*                     kdms_renderer::menu_ViewZoomIn                        */
/*****************************************************************************/

void kdms_renderer::menu_ViewZoomIn() 
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

/*****************************************************************************/
/*                     kdms_renderer::menu_ViewZoomOut                       */
/*****************************************************************************/

void kdms_renderer::menu_ViewZoomOut()
{
  if (buffer == NULL)
    return;
  if (rendering_scale == min_rendering_scale)
    return;
  calculate_view_anchors();
  rendering_scale = decrease_scale(rendering_scale);
  view_dims = kdu_dims(); // Force full window init inside `initialize_regions'
  initialize_regions();
}

/*****************************************************************************/
/*                  kdms_renderer::menu_ViewOptimizeScale                    */
/*****************************************************************************/

void kdms_renderer::menu_ViewOptimizeScale()
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

/*****************************************************************************/
/*                      kdms_renderer::menu_ViewWiden                        */
/*****************************************************************************/

void kdms_renderer::menu_ViewWiden() 
{
  NSSize view_size = [[scroll_view contentView] frame].size;
  NSSize doc_size = [doc_view frame].size;
  NSSize expand; // This is how much we want to grow window size
  expand.width = doc_size.width - view_size.width;
  if (expand.width > view_size.width)
    expand.width = view_size.width;
  expand.height = doc_size.height - view_size.height;
  if (expand.height > view_size.height)
    expand.height = view_size.height;
  
  NSRect frame_rect = [window frame];
  NSRect screen_rect = [[NSScreen mainScreen] frame];
  
  calculate_view_anchors();
  
  float min_x = screen_rect.origin.x;
  if (min_x > frame_rect.origin.x)
    min_x = frame_rect.origin.x;
  float lim_x = screen_rect.origin.x+screen_rect.size.width;
  if (lim_x < (frame_rect.origin.x+frame_rect.size.width))
    lim_x = frame_rect.origin.x+frame_rect.size.width;
  
  float min_y = screen_rect.origin.y;
  if (min_y > frame_rect.origin.y)
    min_y = frame_rect.origin.y;
  float lim_y = screen_rect.origin.y+screen_rect.size.height;
  if (lim_y < (frame_rect.origin.y+frame_rect.size.height))
    lim_y = frame_rect.origin.y+frame_rect.size.height;
  
  float add_to_width = screen_rect.size.width - frame_rect.size.width;
  if (add_to_width < 0.0F)
    add_to_width = 0.0F;
  else if (add_to_width > expand.width)
    add_to_width = expand.width;
  float add_to_height = screen_rect.size.height - frame_rect.size.height;
  if (add_to_height < 0.0F)
    add_to_height = 0.0F;
  else if (add_to_height > expand.height)
    add_to_height = expand.height;
  frame_rect.size.width += add_to_width;
  frame_rect.size.height += add_to_height;
  frame_rect.origin.x -= (float) floor(add_to_width*0.5);
  frame_rect.origin.y -= (float) floor(add_to_height*0.5);
  
  if (frame_rect.origin.x < min_x)
    frame_rect.origin.x = min_x;
  if (frame_rect.origin.y < min_y)
    frame_rect.origin.y = min_y;
  if ((frame_rect.origin.x+frame_rect.size.width) > lim_x)
    frame_rect.origin.x = lim_x - frame_rect.size.width;
  if ((frame_rect.origin.y+frame_rect.size.height) > lim_y)
    frame_rect.origin.y = lim_y - frame_rect.size.height;
  
  [window setFrame:frame_rect display:YES];
  scroll_to_view_anchors();
}

/*****************************************************************************/
/*                     kdms_renderer::menu_ViewShrink                        */
/*****************************************************************************/

void kdms_renderer::menu_ViewShrink() 
{
  NSSize view_size = [[scroll_view contentView] frame].size;
  NSSize new_view_size = view_size;
  new_view_size.width *= 0.5F;
  new_view_size.height *= 0.5F;
  if (new_view_size.width < 50.0F)
    new_view_size.width = 50.0F;
  if (new_view_size.height < 30.0F)
    new_view_size.height = 30.0F;

  NSSize half_reduce;
  half_reduce.width = floor((view_size.width - new_view_size.width) * 0.5F);
  half_reduce.height = floor((view_size.height - new_view_size.height) * 0.5F);
  if (half_reduce.width < 0.0F)
    half_reduce.width = 0.0F;
  if (half_reduce.height < 0.0F)
    half_reduce.height = 0.0F;
  
  calculate_view_anchors();
  
  NSRect frame_rect = [window frame];
  frame_rect.size.width -= 2.0F*half_reduce.width;
  frame_rect.size.height -= 2.0F*half_reduce.height;  
  frame_rect.origin.x += half_reduce.width;
  frame_rect.origin.y += half_reduce.height;
  
  [window setFrame:frame_rect display:YES];
  scroll_to_view_anchors();
}

/*****************************************************************************/
/*                     kdms_renderer::menu_ViewRestore                       */
/*****************************************************************************/

void kdms_renderer::menu_ViewRestore() 
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

/*****************************************************************************/
/*                     kdms_renderer::menu_ViewRefresh                       */
/*****************************************************************************/

void kdms_renderer::menu_ViewRefresh()
{
  refresh_display();
}

/*****************************************************************************/
/*                    kdms_renderer::menu_ViewLayersLess                     */
/*****************************************************************************/

void kdms_renderer::menu_ViewLayersLess()
{
  if (compositor == NULL)
    return;
  int max_layers = compositor->get_max_available_quality_layers();
  if (max_display_layers > max_layers)
    max_display_layers = max_layers;
  if (max_display_layers <= 1)
    { max_display_layers = 1; return; }
  max_display_layers--;
  status_id = KDS_STATUS_LAYER_RES;
  compositor->set_max_quality_layers(max_display_layers);
  refresh_display();
  display_status();  
}

/*****************************************************************************/
/*                    kdms_renderer::menu_ViewLayersMore                     */
/*****************************************************************************/

void kdms_renderer::menu_ViewLayersMore()
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
      status_id = KDS_STATUS_LAYER_RES;
      compositor->set_max_quality_layers(max_display_layers);
      display_status();
    }  
}

/*****************************************************************************/
/*                   kdms_renderer::menu_ViewPixelScaleX1                    */
/*****************************************************************************/

void kdms_renderer::menu_ViewPixelScaleX1()
{
  if (pixel_scale != 1)
    {
      pixel_scale = 1;
      size_window_for_image_dims(false);
    }
}

/*****************************************************************************/
/*                   kdms_renderer::menu_ViewPixelScaleX2                    */
/*****************************************************************************/

void kdms_renderer::menu_ViewPixelScaleX2()
{
  if (pixel_scale != 2)
    {
      pixel_scale = 2;
      calculate_view_anchors();
      if (focus_anchors_known)
        { view_centre_x = focus_centre_x; view_centre_y = focus_centre_y; }
      size_window_for_image_dims(true);
    }
}

/*****************************************************************************/
/*                       kdms_renderer::menu_FocusOff                        */
/*****************************************************************************/

void kdms_renderer::menu_FocusOff()
{
  if (enable_focus_box)
    {
      kdu_dims empty_box;
      empty_box.size = kdu_coords(0,0);
      set_focus_box(empty_box);
    }  
}

/*****************************************************************************/
/*                  kdms_renderer::menu_FocusHighlighting                    */
/*****************************************************************************/

void kdms_renderer::menu_FocusHighlighting()
{
  highlight_focus = !highlight_focus;
  if (enable_focus_box)
    [doc_view setNeedsDisplay:YES];
}

/*****************************************************************************/
/*                      kdms_renderer::menu_FocusWiden                       */
/*****************************************************************************/

void kdms_renderer::menu_FocusWiden()
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

/*****************************************************************************/
/*                     kdms_renderer::menu_FocusShrink                       */
/*****************************************************************************/

void kdms_renderer::menu_FocusShrink()
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

/*****************************************************************************/
/*                      kdms_renderer::menu_FocusLeft                        */
/*****************************************************************************/

void kdms_renderer::menu_FocusLeft()
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
        scroll_absolute(new_box.pos.x-view_dims.pos.x,0,false);
    }
  set_focus_box(new_box);  
}

/*****************************************************************************/
/*                      kdms_renderer::menu_FocusRight                       */
/*****************************************************************************/

void kdms_renderer::menu_FocusRight()
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
        scroll_absolute(new_lim-view_lim,0,false);
    }
  set_focus_box(new_box);  
}

/*****************************************************************************/
/*                       kdms_renderer::menu_FocusUp                         */
/*****************************************************************************/

void kdms_renderer::menu_FocusUp()
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
        scroll_absolute(0,new_box.pos.y-view_dims.pos.y,false);
    }
  set_focus_box(new_box);  
}

/*****************************************************************************/
/*                      kdms_renderer::menu_FocusDown                        */
/*****************************************************************************/

void kdms_renderer::menu_FocusDown()
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
        scroll_absolute(0,new_lim-view_lim,false);
    }
  set_focus_box(new_box);  
}

/*****************************************************************************/
/*                       kdms_renderer::menu_ModeFast                        */
/*****************************************************************************/

void kdms_renderer::menu_ModeFast()
{
  error_level = 0;
  if (compositor != NULL)
    compositor->set_error_level(error_level);
}

/*****************************************************************************/
/*                       kdms_renderer::menu_ModeFussy                       */
/*****************************************************************************/

void kdms_renderer::menu_ModeFussy()
{
  error_level = 1;
  if (compositor != NULL)
    compositor->set_error_level(error_level);
}

/*****************************************************************************/
/*                    kdms_renderer::menu_ModeResilient                      */
/*****************************************************************************/

void kdms_renderer::menu_ModeResilient()
{
  error_level = 2;
  if (compositor != NULL)
    compositor->set_error_level(error_level);
}

/*****************************************************************************/
/*                   kdms_renderer::menu_ModeResilientSOP                    */
/*****************************************************************************/

void kdms_renderer::menu_ModeResilientSOP()
{
  error_level = 3;
  if (compositor != NULL)
    compositor->set_error_level(error_level);
}

/*****************************************************************************/
/*                  kdms_renderer::menu_ModeSingleThreaded                   */
/*****************************************************************************/

void kdms_renderer::menu_ModeSingleThreaded()
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

/*****************************************************************************/
/*                  kdms_renderer::menu_ModeMultiThreaded                    */
/*****************************************************************************/

void kdms_renderer::menu_ModeMultiThreaded()
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

/*****************************************************************************/
/*                 kdms_renderer::can_do_ModeMultiThreaded                   */
/*****************************************************************************/

bool kdms_renderer::can_do_ModeMultiThreaded(NSMenuItem *item)
{
  const char *menu_string = "Multi-Threaded";
  char menu_string_buf[80];
  if (num_threads > 0)
    {
      sprintf(menu_string_buf,"Multi-Threaded (%d threads)",num_threads);
      menu_string = menu_string_buf;
    }
  NSString *ns_string = [NSString stringWithUTF8String:menu_string];
  [item setTitle:ns_string];
  [item setState:((num_threads > 0)?NSOnState:NSOffState)];
  return true;
}

/*****************************************************************************/
/*                   kdms_renderer::menu_ModeMoreThreads                     */
/*****************************************************************************/

void kdms_renderer::menu_ModeMoreThreads()
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

/*****************************************************************************/
/*                   kdms_renderer::menu_ModeLessThreads                     */
/*****************************************************************************/

void kdms_renderer::menu_ModeLessThreads()
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

/*****************************************************************************/
/*                kdms_renderer::menu_ModeRecommendedThreads                 */
/*****************************************************************************/

void kdms_renderer::menu_ModeRecommendedThreads()
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
  
/*****************************************************************************/
/*              kdms_renderer::can_do_ModeRecommendedThreaded                */
/*****************************************************************************/

bool kdms_renderer::can_do_ModeRecommendedThreads(NSMenuItem *item)
{
  const char *menu_string = "Recommended: single threaded";
  char menu_string_buf[80];
  if (num_recommended_threads > 0)
    {
      sprintf(menu_string_buf,"Recommended: %d threads",
              num_recommended_threads);
      menu_string = menu_string_buf;
    }
  NSString *ns_string = [NSString stringWithUTF8String:menu_string];
  [item setTitle:ns_string];
  [item setState:((num_threads > 0)?NSOnState:NSOffState)];
  return true;
}

/*****************************************************************************/
/*                    kdms_renderer::menu_NavComponent1                      */
/*****************************************************************************/

void kdms_renderer::menu_NavComponent1() 
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

/*****************************************************************************/
/*                  kdms_renderer::menu_NavComponentNext                     */
/*****************************************************************************/

void kdms_renderer::menu_NavComponentNext() 
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
  
/*****************************************************************************/
/*                  kdms_renderer::menu_NavComponentPrev                     */
/*****************************************************************************/

void kdms_renderer::menu_NavComponentPrev() 
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

/*****************************************************************************/
/*                  kdms_renderer::menu_NavMultiComponent                    */
/*****************************************************************************/

void kdms_renderer::menu_NavMultiComponent()
{
  if ((compositor == NULL) ||
      ((single_component_idx < 0) &&
       ((single_layer_idx < 0) || (num_frames == 0) || !jpx_in)))
    return;
  if ((num_frames == 0) || !jpx_in)
    menu_NavCompositingLayer();
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
  
/*****************************************************************************/
/*                    kdms_renderer::menu_NavImageNext                       */
/*****************************************************************************/

void kdms_renderer::menu_NavImageNext() 
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

/*****************************************************************************/
/*                   kdms_renderer::can_do_NavImageNext                      */
/*****************************************************************************/

bool kdms_renderer::can_do_NavImageNext(NSMenuItem *item)
{
  if (single_component_idx >= 0)
    return (compositor != NULL) &&
           ((max_codestreams < 0) ||
            (single_codestream_idx < (max_codestreams-1)));
  else if ((single_layer_idx >= 0) && !mj2_in)
    return (compositor != NULL) &&
           ((max_compositing_layer_idx < 0) ||
            (single_layer_idx < max_compositing_layer_idx));
  else if (single_layer_idx >= 0)
    return (compositor != NULL) &&
           ((num_frames < 0) || (frame_idx < (num_frames-1)));
  else
    return (compositor != NULL) && (frame != NULL) &&
            ((num_frames < 0) || (frame_idx < (num_frames-1)));
}

/*****************************************************************************/
/*                    kdms_renderer::menu_NavImagePrev                       */
/*****************************************************************************/

void kdms_renderer::menu_NavImagePrev() 
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

/*****************************************************************************/
/*                   kdms_renderer::can_do_NavImagePrev                      */
/*****************************************************************************/

bool kdms_renderer::can_do_NavImagePrev(NSMenuItem *item)
{
  if (single_component_idx >= 0)
    return (compositor != NULL) && (single_codestream_idx > 0);
  else if ((single_layer_idx >= 0) && !mj2_in)
    return (compositor != NULL) && (single_layer_idx > 0);
  else if (single_layer_idx >= 0)
    return (compositor != NULL) && (frame_idx > 0);
  else
    return (compositor != NULL) && (frame != NULL) && (frame_idx > 0);
}

/*****************************************************************************/
/*                    kdms_renderer::menu_NavTrackNext                       */
/*****************************************************************************/

void kdms_renderer::menu_NavTrackNext()
{
  if ((compositor == NULL) || !mj2_in)
    return;
  if (single_layer_idx < 0)
    menu_NavCompositingLayer();
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

/*****************************************************************************/
/*                 kds_renderer::menu_NavCompositingLayer                    */
/*****************************************************************************/

void kdms_renderer::menu_NavCompositingLayer() 
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

/*****************************************************************************/
/*                   kdms_renderer::menu_MetadataCatalog                     */
/*****************************************************************************/

void kdms_renderer::menu_MetadataCatalog()
{
  if ((!catalog_source) && jpx_in.exists())
    [window create_metadata_catalog];
  else if (catalog_source)
    [window remove_metadata_catalog];
}

/*****************************************************************************/
/*                     kdms_renderer::menu_MetadataShow                      */
/*****************************************************************************/

void kdms_renderer::menu_MetadataShow()
{
  if (metashow != nil)
    [metashow makeKeyAndOrderFront:window];
  else
    {
      metashow = [kdms_metashow alloc];
      [metashow initWithOwner:this andIdentifier:window_identifier];
      if (jp2_family_in.exists())
        [metashow activateWithSource:&jp2_family_in andFileName:open_filename];
    }
}

/*****************************************************************************/
/*                    kdms_renderer::menu_MetadataAdd                        */
/*****************************************************************************/

void kdms_renderer::menu_MetadataAdd()
{
  if ((compositor == NULL) || (!jpx_in))
    return;
  if (file_server == NULL)
    file_server = new kdms_file_services(open_file_pathname);
  kdms_metadata_editor *editor = [kdms_metadata_editor alloc];
  int counted_codestreams=1, counted_compositing_layers=1;
  jpx_in.count_codestreams(counted_codestreams);
  jpx_in.count_compositing_layers(counted_compositing_layers);
  jpx_meta_manager manager = jpx_in.access_meta_manager();
  [editor initWithNumCodestreams:counted_codestreams
                       layers:counted_compositing_layers
                     metaManager:manager
                    fileServices:file_server
                        editable:YES];
  int initial_codestream_idx=0;
  kdu_dims initial_region = focus_box;
  if (enable_focus_box &&
      compositor->map_region(initial_codestream_idx,initial_region))
    [editor configureWithInitialRegion:initial_region
                                stream:initial_codestream_idx
                                 layer:-1
               appliesToRenderedResult:false];
  else if (single_component_idx >= 0)
    [editor configureWithInitialRegion:kdu_dims()
                                stream:single_codestream_idx
                                 layer:-1
               appliesToRenderedResult:false];
  else if (single_layer_idx >= 0)
    [editor configureWithInitialRegion:kdu_dims()
                                stream:-1
                                 layer:single_layer_idx
               appliesToRenderedResult:false];
  else
    [editor configureWithInitialRegion:kdu_dims()
                                stream:-1
                                 layer:-1
               appliesToRenderedResult:true];
  if ([editor run_modal:this])
    { // Above function invokes `metadata_changed' as required.
      have_unsaved_edits = true;
    }
  [editor release]; // Cleans up properly, even if an exception occurred
}

/*****************************************************************************/
/*                    kdms_renderer::menu_MetadataEdit                       */
/*****************************************************************************/

void kdms_renderer::menu_MetadataEdit()
{
  if ((compositor == NULL) || !jpx_in.exists())
    return;
  if (catalog_source != nil)
    {
      jpx_metanode metanode = [catalog_source get_selected_metanode];
      if (metanode.exists())
        {
          jpx_metanode scan = metanode;
          while ((scan=scan.get_parent()).exists())
            if (scan.get_num_regions() > 0)
              { // Start editor at the ROI containing the catalog selection
                metanode = scan;
                break;
              }
          edit_metanode(metanode);
          return;
        }
    }
  edit_metadata_at_point(NULL);
}

/*****************************************************************************/
/*                   kdms_renderer::menu_OverlayEnable                       */
/*****************************************************************************/

void kdms_renderer::menu_OverlayEnable() 
{
  if ((compositor == NULL) || !jpx_in)
    return;
  overlays_enabled = !overlays_enabled;
  overlay_flashing_enabled = false;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
                                 (overlay_painting_colour<<8) +
                                 (overlay_painting_intensity & 0xFF));
}

/*****************************************************************************/
/*                   kds_renderer::menu_OverlayFlashing                      */
/*****************************************************************************/

void kdms_renderer::menu_OverlayFlashing()
{
  if ((compositor == NULL) || (!jpx_in) || (!overlays_enabled) || in_playmode)
    return;
  overlay_flashing_enabled = !overlay_flashing_enabled;
  if (!overlay_flashing_enabled)
    overlay_painting_colour = 0;
  else
    {
      overlay_painting_colour++;
      window_manager->schedule_wakeup(window,this,
                                      CFAbsoluteTimeGetCurrent()+0.8);
      overlay_flash_wakeup_scheduled = true;
    }
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
                                 (overlay_painting_colour<<8) +
                                 (overlay_painting_intensity & 0xFF));
}

/*****************************************************************************/
/*                    kds_renderer::menu_OverlayToggle                       */
/*****************************************************************************/

void kdms_renderer::menu_OverlayToggle()
{
  if ((compositor == NULL) || !jpx_in)
    return;
  if (!overlays_enabled)
    {
      overlays_enabled = true;
      overlay_flashing_enabled = !in_playmode;
      overlay_painting_colour = 0;
    }
  else if (overlay_flashing_enabled && !in_playmode)
    {
      overlay_flashing_enabled = false;
      overlay_painting_colour++;
    }
  else
    overlays_enabled = false;
  if (overlay_flashing_enabled)
    {
      window_manager->schedule_wakeup(window,this,
                                      CFAbsoluteTimeGetCurrent()+0.8);
      overlay_flash_wakeup_scheduled = true;      
    }
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
                                 (overlay_painting_colour<<8) +
                                 (overlay_painting_intensity & 0xFF));  
}

/*****************************************************************************/
/*                   kds_renderer::menu_OverlayBrighter                      */
/*****************************************************************************/

void kdms_renderer::menu_OverlayBrighter()
{
  if ((compositor == NULL) || (!jpx_in) || (!overlays_enabled) ||
      (overlay_painting_intensity == 127))
    return;
  overlay_painting_intensity++;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
                                 (overlay_painting_colour<<8) +
                                 (overlay_painting_intensity & 0xFF));  
}

/*****************************************************************************/
/*                    kds_renderer::menu_OverlayDarker                       */
/*****************************************************************************/

void kdms_renderer::menu_OverlayDarker()
{
  if ((compositor == NULL) || (!jpx_in) || (!overlays_enabled) ||
      (overlay_painting_intensity == -128))
    return;
  overlay_painting_intensity--;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
                                 (overlay_painting_colour<<8) +
                                 (overlay_painting_intensity & 0xFF));  
}

/*****************************************************************************/
/*                  kds_renderer::menu_OverlayDoubleSize                     */
/*****************************************************************************/

void kdms_renderer::menu_OverlayDoubleSize()
{
  if ((compositor == NULL) || (!jpx_in) || (!overlays_enabled) ||
      (overlay_size_threshold & 0x40000000))
    return;
  overlay_size_threshold <<= 1;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
                                 (overlay_painting_colour<<8) +
                                 (overlay_painting_intensity & 0xFF));    
}

/*****************************************************************************/
/*                   kds_renderer::menu_OverlayHalveSize                     */
/*****************************************************************************/

void kdms_renderer::menu_OverlayHalveSize()
{
  if ((compositor == NULL) || (!jpx_in) || (!overlays_enabled) ||
      (overlay_size_threshold == 1))
    return;
  overlay_size_threshold >>= 1;
  compositor->configure_overlays(overlays_enabled,overlay_size_threshold,
                                 (overlay_painting_colour<<8) +
                                 (overlay_painting_intensity & 0xFF));    
}

/*****************************************************************************/
/*                      kds_renderer::menu_PlayNative                        */
/*****************************************************************************/

void kdms_renderer::menu_PlayNative()
{
  if (!source_supports_playmode())
    return;
  if (use_native_timing)
    return;
  use_native_timing = true;
  if (in_playmode)
    {
      stop_playmode();
      start_playmode(playmode_reverse);
    }
}

/*****************************************************************************/
/*                     kds_renderer::can_do_PlayNative                       */
/*****************************************************************************/

bool kdms_renderer::can_do_PlayNative(NSMenuItem *item)
{
  if (!source_supports_playmode())
    return false;
  const char *menu_string = "Native play rate";
  char menu_string_buf[80];
  if ((native_playback_multiplier > 1.01F) ||
      (native_playback_multiplier < 0.99F))
    {
      sprintf(menu_string_buf,"Native play rate x %5.2g",
              1.0F/native_playback_multiplier);
      menu_string = menu_string_buf;
    }
  NSString *ns_string = [NSString stringWithUTF8String:menu_string];
  [item setTitle:ns_string];
  [item setState:((use_native_timing)?NSOnState:NSOffState)];
  return true;
}

/*****************************************************************************/
/*                     kds_renderer::menu_PlayCustom                         */
/*****************************************************************************/

void kdms_renderer::menu_PlayCustom()
{
  if (!source_supports_playmode())
    return;
  if (!use_native_timing)
    return;
  use_native_timing = false;
  if (in_playmode)
    {
      stop_playmode();
      start_playmode(playmode_reverse);
    }
}

/*****************************************************************************/
/*                     kds_renderer::can_do_PlayCustom                       */
/*****************************************************************************/

bool kdms_renderer::can_do_PlayCustom(NSMenuItem *item)
{
  if (!source_supports_playmode())
    return false;
  char menu_string_buf[80];
  sprintf(menu_string_buf,"Custom frame rate = %5.2g fps",
          1.0F/custom_frame_interval);
  NSString *ns_string = [NSString stringWithUTF8String:menu_string_buf];
  [item setTitle:ns_string];
  [item setState:((use_native_timing)?NSOffState:NSOnState)];
  return true;
}
  
/*****************************************************************************/
/*                  kds_renderer::menu_PlayNativeRateUp                      */
/*****************************************************************************/

void kdms_renderer::menu_PlayNativeRateUp()
{
  if (!source_supports_playmode())
    return;
  use_native_timing = true;
  native_playback_multiplier *= 1.0F / (float) sqrt(2.0);
  if (in_playmode)
    {
      stop_playmode();
      start_playmode(playmode_reverse);
    }
}

/*****************************************************************************/
/*                 kds_renderer::menu_PlayNativeRateDown                     */
/*****************************************************************************/

void kdms_renderer::menu_PlayNativeRateDown()
{
  if (!source_supports_playmode())
    return;
  use_native_timing = true;
  native_playback_multiplier *= (float) sqrt(2.0); 
  if (in_playmode)
    {
      stop_playmode();
      start_playmode(playmode_reverse);
    }
}

/*****************************************************************************/
/*                  kds_renderer::menu_PlayCustomRateUp                      */
/*****************************************************************************/

void kdms_renderer::menu_PlayCustomRateUp()
{
  if (!source_supports_playmode())
    return;
  use_native_timing = false;
  float frame_rate = 1.0F / custom_frame_interval;
  if (frame_rate > 4.5F)
    frame_rate += 5.0F;
  else if (frame_rate > 0.9F)
    {
      if ((frame_rate += 1.0F) > 5.0F)
        frame_rate = 5.0F;
    }
  else if ((frame_rate *= (float) sqrt(2.0)) > 1.0F)
    frame_rate = 1.0F;
  custom_frame_interval = 1.0F / frame_rate;
  if (in_playmode)
    {
      stop_playmode();
      start_playmode(playmode_reverse);
    }
}

/*****************************************************************************/
/*                 kds_renderer::menu_PlayCustomRateDown                     */
/*****************************************************************************/

void kdms_renderer::menu_PlayCustomRateDown()
{
  if (!source_supports_playmode())
    return;
  use_native_timing = false;
  float frame_rate = 1.0F / custom_frame_interval;
  if (frame_rate < 1.2F)
    frame_rate *= 1.0F / (float) sqrt(2.0);
  else if (frame_rate < 5.5F)
    {
      if ((frame_rate -= 1.0F) < 1.0F)
        frame_rate = 1.0F;
    }
  else if ((frame_rate -= 5.0F) < 5.0F)
    frame_rate = 5.0F;
  custom_frame_interval = 1.0F / frame_rate;
  if (in_playmode)
    {
      stop_playmode();
      start_playmode(playmode_reverse);
    }
}

/*****************************************************************************/
/*                   kdms_renderer::menu_StatusToggle                        */
/*****************************************************************************/

void kdms_renderer::menu_StatusToggle() 
{
  if (status_id == KDS_STATUS_LAYER_RES)
    status_id = KDS_STATUS_DIMS;
  else if (status_id == KDS_STATUS_DIMS)
    status_id = KDS_STATUS_MEM;
  else if (status_id == KDS_STATUS_MEM)
    status_id = KDS_STATUS_CACHE;
  else if (status_id == KDS_STATUS_CACHE)
    status_id = KDS_STATUS_PLAYSTATS;
  else
    status_id = KDS_STATUS_LAYER_RES;
  display_status();
}
