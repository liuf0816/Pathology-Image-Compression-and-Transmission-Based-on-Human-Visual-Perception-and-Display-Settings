/*****************************************************************************/
// File: jpx.cpp [scope = APPS/JP2]
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
   Implements the internal machinery whose external interfaces are defined
in the compressed-io header file, "jpx.h".
******************************************************************************/

#include <assert.h>
#include <string.h>
#include <math.h>
#include <kdu_utils.h>
#include "jpx.h"
#include "jpx_local.h"
#include "kdu_file_io.h"

/* Note Carefully:
      If you want to be able to use the "kdu_text_extractor" tool to
   extract text from calls to `kdu_error' and `kdu_warning' so that it
   can be separately registered (possibly in a variety of different
   languages), you should carefully preserve the form of the definitions
   below, starting from #ifdef KDU_CUSTOM_TEXT and extending to the
   definitions of KDU_WARNING_DEV and KDU_ERROR_DEV.  All of these
   definitions are expected by the current, reasonably inflexible
   implementation of "kdu_text_extractor".
      The only things you should change when these definitions are ported to
   different source files are the strings found inside the `kdu_error'
   and `kdu_warning' constructors.  These strings may be arbitrarily
   defined, as far as "kdu_text_extractor" is concerned, except that they
   must not occupy more than one line of text.
*/
#ifdef KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("E(jpx.cpp)",_id);
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("W(jpx.cpp)",_id);
#  define KDU_TXT(_string) "<#>" // Special replacement pattern
#else // !KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("Error in Kakadu File Format Support:\n");
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("Warning in Kakadu File Format Support:\n");
#  define KDU_TXT(_string) _string
#endif // !KDU_CUSTOM_TEXT

#define KDU_ERROR_DEV(_name,_id) KDU_ERROR(_name,_id)
 // Use the above version for errors which are of interest only to developers
#define KDU_WARNING_DEV(_name,_id) KDU_WARNING(_name,_id)
 // Use the above version for warnings which are of interest only to developers


/* ========================================================================= */
/*                              jx_fragment_list                             */
/* ========================================================================= */

/*****************************************************************************/
/*                           jx_fragment_list::init                          */
/*****************************************************************************/

void
  jx_fragment_list::init(jp2_input_box *flst)
{
  assert(flst->get_box_type() == jp2_fragment_list_4cc);
  total_length = 0;  num_frags = 0; // Just to be sure
  kdu_uint16 nf;
  if (!flst->read(nf))
    { KDU_ERROR(e,0); e <<
        KDU_TXT("Error encountered reading fragment list (flst) box.  "
        "Unable to read the initial fragment count.");
    }
  jpx_fragment_list ifc(this);
  for (; nf > 0; nf--)
    {
      kdu_uint16 url_idx;
      kdu_uint32 off1, off2, len;
      if (!(flst->read(off1) && flst->read(off2) && flst->read(len) &&
            flst->read(url_idx)))
        { KDU_ERROR(e,1); e <<
            KDU_TXT("Error encountered reading fragment list (flst) "
            "box.  Contents of box terminated prematurely.");
        }
      kdu_long offset = (kdu_long) off2;
#ifdef KDU_LONG64
      offset += ((kdu_long) off1) << 32;
#endif // KDU_LONG64
      ifc.add_fragment((int) url_idx,offset,(kdu_long) len);
    }
  flst->close();
}

/*****************************************************************************/
/*                          jx_fragment_list::save_box                       */
/*****************************************************************************/

void
  jx_fragment_list::save_box(jp2_output_box *super_box)
{
  int n, actual_box_frags = num_frags;
#ifdef KDU_LONG64
  for (n=0; n < num_frags; n++)
    if ((frags[n].length >> 32) > 0)
      actual_box_frags += (int)((frags[n].length-1) / 0x00000000FFFFFFFF);
#endif // KDU_LONG64
  if (actual_box_frags >= (1<<16))
    { KDU_ERROR_DEV(e,2); e <<
        KDU_TXT("Trying to write too many fragments to a fragment "
        "list (flst) box.  Maximum number of fragments is 65535, but note "
        "that each written fragment must have a length < 2^32 bytes.  Very "
        "long fragments may thus need to be split, creating the appearance "
        "of a very large number of fragments.");
    }
  jp2_output_box flst;
  flst.open(super_box,jp2_fragment_list_4cc);
  flst.write((kdu_uint16) actual_box_frags);
  for (n=0; n < num_frags; n++)
    {
      int url_idx = frags[n].url_idx;
      kdu_long offset = frags[n].offset;
      kdu_long remaining_length = frags[n].length;
      kdu_uint32 frag_length;
      do {
          frag_length = (kdu_uint32) remaining_length;
#ifdef KDU_LONG64
            if ((remaining_length >> 32) > 0)
              frag_length = 0xFFFFFFFF;
          flst.write((kdu_uint32)(offset>>32));
          flst.write((kdu_uint32) offset);
#else // !KDU_LONG64
          flst.write((kdu_uint32) 0);
          flst.write((kdu_uint32) offset);
#endif // !KDU_LONG64
          flst.write(frag_length);
          flst.write((kdu_uint16) url_idx);
          remaining_length -= (kdu_long) frag_length;
          offset += (kdu_long) frag_length;
          actual_box_frags--;
        } while (remaining_length > 0);
    }
  flst.close();
  assert(actual_box_frags == 0);
}

/*****************************************************************************/
/*                          jx_fragment_list::finalize                       */
/*****************************************************************************/

void
  jx_fragment_list::finalize(jp2_data_references data_references)
{
  int n, url_idx;
  for (n=0; n < num_frags; n++)
    {
      url_idx = frags[n].url_idx;
      if (data_references.get_url(url_idx) == NULL)
        { KDU_ERROR(e,3); e <<
            KDU_TXT("Attempting to read or write a fragment list "
            "box which refers to one or more URL's, not found in the "
            "associated data references object "
            "(see `jpx_target::access_data_references').");
        }
    }
}


/* ========================================================================= */
/*                              jpx_fragment_list                            */
/* ========================================================================= */

/*****************************************************************************/
/*                        jpx_fragment_list::add_fragment                    */
/*****************************************************************************/

void
  jpx_fragment_list::add_fragment(int url_idx, kdu_long offset,
                                  kdu_long length)
{
  if (state->num_frags == state->max_frags)
    { // Augment fragment array
      state->max_frags += state->num_frags + 8;
      jx_frag *tmp_frags = new jx_frag[state->max_frags];
      if (state->frags != NULL)
        {
          for (int n=0; n < state->num_frags; n++)
            tmp_frags[n] = state->frags[n];
          delete[] state->frags;
        }
      state->frags = tmp_frags;
    }

  // See if we can join the fragment to the last one
  jx_frag *frag = state->frags+state->num_frags-1;
  if ((frag >= state->frags) && (frag->url_idx == url_idx) &&
      ((frag->offset+frag->length) == offset))
    frag->length += length;
  else
    {
      frag++;
      state->num_frags++;
      frag->url_idx = url_idx;
      frag->offset = offset;
      frag->length = length;
    }
  state->total_length += length;
}

/*****************************************************************************/
/*                      jpx_fragment_list::get_total_length                  */
/*****************************************************************************/

kdu_long
  jpx_fragment_list::get_total_length()
{
  return (state==NULL)?0:(state->total_length);
}

/*****************************************************************************/
/*                      jpx_fragment_list::get_num_fragments                 */
/*****************************************************************************/

int
  jpx_fragment_list::get_num_fragments()
{
  return (state==NULL)?0:(state->num_frags);
}

/*****************************************************************************/
/*                        jpx_fragment_list::get_fragment                    */
/*****************************************************************************/

bool
  jpx_fragment_list::get_fragment(int frag_idx, int &url_idx,
                                  kdu_long &offset, kdu_long &length)
{
  if ((frag_idx < 0) || (frag_idx >= state->num_frags))
    return false;
  jx_frag *frag = state->frags + frag_idx;
  url_idx = frag->url_idx;
  offset = frag->offset;
  length = frag->length;
  return true;
}

/*****************************************************************************/
/*                     jpx_fragment_list::locate_fragment                    */
/*****************************************************************************/

int
  jpx_fragment_list::locate_fragment(kdu_long pos, kdu_long &bytes_into_frag)
{
  if (pos < 0)
    return -1;
  int idx;
  for (idx=0; idx < state->num_frags; idx++)
    {
      bytes_into_frag = pos;
      pos -= state->frags[idx].length;
      if (pos < 0)
        return idx;
    }
  return -1;
}


/* ========================================================================= */
/*                               jpx_input_box                               */
/* ========================================================================= */

/*****************************************************************************/
/*                       jpx_input_box::jpx_input_box                        */
/*****************************************************************************/

jpx_input_box::jpx_input_box()
{
  frag_idx=last_url_idx=-1;  frag_start=frag_lim=0;
  url_pos=last_url_pos=0; flst_src=NULL;  frag_file=NULL;
  path_buf=NULL; max_path_len = 0;
}

/*****************************************************************************/
/*                         jpx_input_box::open_next                          */
/*****************************************************************************/

bool jpx_input_box::open_next()
{
  if (flst_src != NULL)
    return false;
  else
    return jp2_input_box::open_next();
}

/*****************************************************************************/
/*                          jpx_input_box::open_as                           */
/*****************************************************************************/

bool
  jpx_input_box::open_as(jpx_fragment_list frag_list,
                         jp2_data_references data_references,
                         jp2_family_src *ultimate_src,
                         kdu_uint32 box_type)
{
  if (is_open)
    { KDU_ERROR_DEV(e,4); e <<
        KDU_TXT("Attempting to call `jpx_input_box::open_as' without "
        "first closing the box.");
    }
  if (ultimate_src == NULL)
    { KDU_ERROR_DEV(e,5); e <<
        KDU_TXT("You must supply a non-NULL `ultimate_src' argument "
        "to `jpx_input_box::open_as'.");
    }

  this->frag_list = frag_list;
  this->flst_src = ultimate_src;
  if (flst_src->cache == NULL)
    this->data_references = data_references; // Prevents any data being read
                    // at all if the fragment list belongs to a caching data
                    // source.  This is the safest way to ensure that we
                    // don't have to wait (possibly indefinitely) for a data
                    // references box to be delivered by a JPIP server.  JPIP
                    // servers should not send fragment lists anyway; they
                    // should use the special JPIP equivalent box mechanism.

  this->locator = jp2_locator();
  this->super_box = NULL;
  this->src = NULL;

  this->box_type = box_type;
  this->original_box_length = frag_list.get_total_length();
  this->original_header_length = 0;
  this->next_box_offset = 0;
  this->contents_start = 0;
  this->contents_lim = frag_list.get_total_length();
  this->bin_id = -1;
  this->codestream_min = this->codestream_lim = this->codestream_id = -1;
  this->bin_class = 0;
  this->can_dereference_contents = false;
  this->rubber_length = false;
  this->is_open = true;
  this->is_locked = false;
  this->capabilities = KDU_SOURCE_CAP_SEQUENTIAL | KDU_SOURCE_CAP_SEEKABLE;
  this->pos = 0;
  this->partial_word_bytes = 0;
  frag_file = NULL;
  frag_idx = -1;
  last_url_idx = -1;
  frag_start = frag_lim = url_pos = last_url_pos = 0;
  return true;
}

/*****************************************************************************/
/*                           jpx_input_box::close                            */
/*****************************************************************************/

bool
  jpx_input_box::close()
{
  if (frag_file != NULL)
    { fclose(frag_file); frag_file = NULL; }
  if (path_buf != NULL)
    { delete[] path_buf; path_buf = NULL; }
  max_path_len = 0;
  frag_idx = -1;
  last_url_idx = -1;
  frag_start = frag_lim = url_pos = last_url_pos = 0;
  flst_src = NULL;
  frag_list = jpx_fragment_list(NULL);
  data_references = jp2_data_references(NULL);
  return jp2_input_box::close();
}

/*****************************************************************************/
/*                           jpx_input_box::seek                             */
/*****************************************************************************/

bool
  jpx_input_box::seek(kdu_long offset)
{
  if ((flst_src == NULL) || (contents_block != NULL))
    return jp2_input_box::seek(offset);
  assert(contents_start == 0);
  if (pos == offset)
    return true;
  kdu_long old_pos = pos;
  if (offset < 0)
    pos = 0;
  else
    pos = (offset < contents_lim)?offset:contents_lim;
  if ((frag_idx >= 0) && (pos >= frag_start) && (pos < frag_lim))
    { // Seeking within existing fragment
      url_pos += (pos-old_pos);
    }
  else
    { // Force re-determination of the active fragment
      frag_idx = -1;
      frag_start = frag_lim = url_pos = 0;
    }
  return true;
}

/*****************************************************************************/
/*                           jpx_input_box::read                             */
/*****************************************************************************/

int
  jpx_input_box::read(kdu_byte *buf, int num_bytes)
{
  if ((flst_src == NULL) || (contents_block != NULL))
    return jp2_input_box::read(buf,num_bytes);

  int result = 0;
  while (num_bytes > 0)
    {
      if ((frag_idx < 0) || (pos >= frag_lim))
        { // Find the fragment containing `pos'
          int url_idx;
          kdu_long bytes_into_fragment, frag_length;
          frag_idx = frag_list.locate_fragment(pos,bytes_into_fragment);
          if ((frag_idx < 0) ||
              !frag_list.get_fragment(frag_idx,url_idx,url_pos,frag_length))
            { // Cannot read any further
              frag_idx = -1;
              return result;
            }
          url_pos += bytes_into_fragment;
          frag_start = pos - bytes_into_fragment;
          frag_lim = frag_start + frag_length;
          if (url_idx != last_url_idx)
            { // Change the URL
              if (frag_file != NULL)
                {
                  fclose(frag_file);
                  frag_file = NULL;
                  last_url_idx = -1;
                }
              const char *url = "";
              if (url_idx != 0)
                {
                  if ((!data_references) ||
                      ((url = data_references.get_url(url_idx)) == NULL) ||
                      ((*url != '\0') && ((frag_file=url_fopen(url)) == NULL)))
                    { // Cannot read any further
                      frag_idx = -1;
                      return result;
                    }
                }
              last_url_pos = 0;
              last_url_idx = url_idx;
            }
        }
      if (url_pos != last_url_pos)
        {
          if (frag_file != NULL) // Don't have to seek `kdu_family_src'
            kdu_fseek(frag_file,url_pos);
          last_url_pos = url_pos;
        }

      int xfer_bytes = num_bytes;
      if ((pos+xfer_bytes) > frag_lim)
        xfer_bytes = (int)(frag_lim-pos);
      if (frag_file != NULL)
        { // Read from `frag_file'
          xfer_bytes = (int) fread(buf,1,(size_t) xfer_bytes,frag_file);
        }
      else
        { // Read from `flst_src'
          if (flst_src->cache != NULL)
            return result;
          flst_src->acquire_lock();
          if (flst_src->last_read_pos != pos)
            {
              if (flst_src->fp != NULL)
                kdu_fseek(flst_src->fp,pos);
              else if (flst_src->indirect != NULL)
                flst_src->indirect->seek(pos);
            }
          if (flst_src->fp != NULL)
            xfer_bytes = (int) fread(buf,1,(size_t) xfer_bytes,flst_src->fp);
          else if (flst_src->indirect != NULL)
            xfer_bytes = flst_src->indirect->read(buf,xfer_bytes);
          flst_src->last_read_pos = pos + xfer_bytes;
          flst_src->release_lock();
        }

      if (xfer_bytes == 0)
        break; // Cannot read any further

      num_bytes -= xfer_bytes;
      result += xfer_bytes;
      buf += xfer_bytes;
      pos += xfer_bytes;
      url_pos += xfer_bytes;
      last_url_pos += xfer_bytes;
    }
  return result;
}

/*****************************************************************************/
/*                          jpx_input_box::url_fopen                         */
/*****************************************************************************/

FILE *
  jpx_input_box::url_fopen(const char *url)
{
  bool is_absolute =
    (*url == '/') || (*url == '\\') ||
    ((url[0] != '\0') && (url[1] == ':') &&
     ((url[2] == '/') || (url[2] == '\\')));
  const char *rel_fname=NULL;
  if ((!is_absolute) && ((rel_fname = flst_src->get_filename()) != NULL))
    {
      int len = (int)(strlen(url)+strlen(rel_fname)+2);
      if (len > max_path_len)
        {
          max_path_len += len;
          if (path_buf != NULL)
            delete[] path_buf;
          path_buf = new char[max_path_len];
        }
      strcpy(path_buf,rel_fname);
      char *cp = path_buf+strlen(path_buf);
      while ((cp > path_buf) && (cp[-1] != '/') && (cp[-1] != '\\'))
        cp--;
      strcpy(cp,url);
      url = path_buf;
    }
  return fopen(url,"rb");
}


/* ========================================================================= */
/*                              jx_compatibility                             */
/* ========================================================================= */

/*****************************************************************************/
/*                        jx_compatibility::init_ftyp                        */
/*****************************************************************************/

bool
  jx_compatibility::init_ftyp(jp2_input_box *ftyp)
{
  assert(ftyp->get_box_type() == jp2_file_type_4cc);
  kdu_uint32 brand, minor_version, compat;
  ftyp->read(brand); ftyp->read(minor_version);

  bool jp2_compat=false, jpx_compat=false, jpxb_compat=false;
  while (ftyp->read(compat))
    if (compat == jp2_brand)
      jp2_compat = true;
    else if (compat == jpx_brand)
      jpx_compat = true;
    else if (compat == jpx_baseline_brand)
      jpxb_compat = jpx_compat = true;

  if (!ftyp->close())
    { KDU_ERROR(e,6); e <<
        KDU_TXT("JP2-family data source contains a malformed file type box.");
    }
  if (!(jp2_compat || jpx_compat))
    return false;
  this->is_jp2 = (brand == jp2_brand) || (!jpx_compat);
  this->is_jp2_compatible = jp2_compat;
  this->is_jpxb_compatible = jpxb_compat;
  this->have_rreq_box = false; // Until we find one
  return true;
}

/*****************************************************************************/
/*                        jx_compatibility::init_rreq                        */
/*****************************************************************************/

void
  jx_compatibility::init_rreq(jp2_input_box *rreq)
{
  assert(rreq->get_box_type() == jp2_reader_requirements_4cc);
  kdu_byte m, byte, mask_length = 0;
  int n, shift, m_idx;
  rreq->read(mask_length);
  for (shift=24, m_idx=0, m=0; (m < mask_length) && (m < 32); m++, shift-=8)
    {
      if (shift < 0)
        { shift=24; m_idx++; }
      rreq->read(byte);
      fully_understand_mask[m_idx] |= (((kdu_uint32) byte) << shift);
    }
  for (shift=24, m_idx=0, m=0; (m < mask_length) && (m<32); m++, shift-=8)
    {
      if (shift < 0)
        { shift=24; m_idx++; }
      rreq->read(byte);
      decode_completely_mask[m_idx] |= (((kdu_uint32) byte) << shift);
    }

  kdu_uint16 nsf;
  if (!rreq->read(nsf))
    { KDU_ERROR(e,7); e <<
        KDU_TXT("Malformed reader requirements (rreq) box found in "
        "JPX data source.  Box terminated unexpectedly.");
    }
  have_rreq_box = true;
  num_standard_features = max_standard_features = (int) nsf;
  standard_features = new jx_feature[max_standard_features];
  for (n=0; n < num_standard_features; n++)
    {
      jx_feature *fp = standard_features + n;
      rreq->read(fp->feature_id);
      for (shift=24, m_idx=0, m=0; (m < mask_length) && (m<32); m++, shift-=8)
        {
          if (shift < 0)
            { shift=24; m_idx++; }
          rreq->read(byte);
          fp->mask[m_idx] |= (((kdu_uint32) byte) << shift);
        }
      fp->supported = true;
      if (fp->feature_id == JPX_SF_CODESTREAM_FRAGMENTS_REMOTE)
        fp->supported = false; // The only one we know we don't support.
    }

  kdu_uint16 nvf;
  if (!rreq->read(nvf))
    { KDU_ERROR(e,8); e <<
        KDU_TXT("Malformed reader requirements (rreq) box found in "
        "JPX data source.  Box terminated unexpectedly.");
    }
  num_vendor_features = max_vendor_features = (int) nvf;
  vendor_features = new jx_vendor_feature[max_vendor_features];
  for (n=0; n < num_vendor_features; n++)
    {
      jx_vendor_feature *fp = vendor_features + n;
      if (rreq->read(fp->uuid,16) != 16)
        { KDU_ERROR(e,9); e <<
            KDU_TXT("Malformed reader requirements (rreq) box found "
            "in JPX data source. Box terminated unexpectedly.");
        }
      for (shift=24, m_idx=0, m=0; (m < mask_length) && (m<32); m++, shift-=8)
        {
          if (shift < 0)
            { shift=24; m_idx++; }
          if (!rreq->read(byte))
            { KDU_ERROR(e,10); e <<
                KDU_TXT("Malformed reader requirements (rreq) box "
                "found in JPX data source. Box terminated unexpectedly.");
            }
          fp->mask[m_idx] |= (((kdu_uint32) byte) << shift);
        }
      fp->supported = false;
    }
  if (!rreq->close())
    { KDU_ERROR(e,11); e <<
        KDU_TXT("Malformed reader requirements (rreq) box "
        "found in JPX data source.  Box appears to be too long.");
    }
}

/*****************************************************************************/
/*                     jx_compatibility::copy_from                           */
/*****************************************************************************/

void
  jx_compatibility::copy_from(jx_compatibility *src)
{
  jpx_compatibility src_ifc(src);
  is_jp2_compatible = is_jp2_compatible && src->is_jp2_compatible;
  is_jpxb_compatible = is_jpxb_compatible && src->is_jpxb_compatible;
  no_extensions = no_extensions &&
    src_ifc.check_standard_feature(JPX_SF_CODESTREAM_NO_EXTENSIONS);
  no_opacity = no_opacity &&
    src_ifc.check_standard_feature(JPX_SF_NO_OPACITY);
  no_fragments = no_fragments &&
    src_ifc.check_standard_feature(JPX_SF_CODESTREAM_CONTIGUOUS);
  no_scaling = no_scaling &&
    src_ifc.check_standard_feature(JPX_SF_NO_SCALING);
  single_stream_layers = single_stream_layers &&
    src_ifc.check_standard_feature(JPX_SF_ONE_CODESTREAM_PER_LAYER);
  int n, k;
  for (n=0; n < src->num_standard_features; n++)
    {
      jx_feature *fp, *feature = src->standard_features + n;
      if ((feature->feature_id == JPX_SF_CODESTREAM_NO_EXTENSIONS) ||
          (feature->feature_id == JPX_SF_NO_OPACITY) ||
          (feature->feature_id == JPX_SF_CODESTREAM_CONTIGUOUS) ||
          (feature->feature_id == JPX_SF_NO_SCALING) ||
          (feature->feature_id == JPX_SF_ONE_CODESTREAM_PER_LAYER))
        continue;
      for (fp=standard_features, k=0; k < num_standard_features; k++, fp++)
        if (fp->feature_id == feature->feature_id)
          break;
      if (k == num_standard_features)
        { // Add a new feature
          if (k == max_standard_features)
            {
              max_standard_features += k + 10;
              fp = new jx_feature[max_standard_features];
              for (k=0; k < num_standard_features; k++)
                fp[k] = standard_features[k];
              if (standard_features != NULL)
                delete[] standard_features;
              standard_features = fp;
              fp += k;
            }
          num_standard_features++;
          fp->feature_id = feature->feature_id;
        }
      for (k=0; k < 8; k++)
        {
          fp->fully_understand[k] |= feature->fully_understand[k];
          fp->decode_completely[k] |= feature->decode_completely[k];
          fully_understand_mask[k] |= fp->fully_understand[k];
          decode_completely_mask[k] |= fp->decode_completely[k];
        }
    }

  for (n=0; n < src->num_vendor_features; n++)
    {
      jx_vendor_feature *fp, *feature = src->vendor_features + n;
      for (fp=vendor_features, k=0; k < num_vendor_features; k++, fp++)
        if (memcmp(fp->uuid,feature->uuid,16) == 0)
          break;
      if (k == num_vendor_features)
        { // Add a new feature
          if (k == max_vendor_features)
            {
              max_vendor_features += k + 10;
              fp = new jx_vendor_feature[max_vendor_features];
              for (k=0; k < num_vendor_features; k++)
                fp[k] = vendor_features[k];
              if (vendor_features != NULL)
                delete[] vendor_features;
              vendor_features = fp;
              fp += k;
            }
          num_vendor_features++;
          memcpy(fp->uuid,feature->uuid,16);
        }
      for (k=0; k < 8; k++)
        {
          fp->fully_understand[k] |= feature->fully_understand[k];
          fp->decode_completely[k] |= feature->decode_completely[k];
          fully_understand_mask[k] |= fp->fully_understand[k];
          decode_completely_mask[k] |= fp->decode_completely[k];
        }
    }
}

/*****************************************************************************/
/*                  jx_compatibility::add_standard_feature                   */
/*****************************************************************************/

void
  jx_compatibility::add_standard_feature(kdu_uint16 feature_id,
                                         bool add_to_both)
{
  int n;
  jx_feature *fp = standard_features;
  for (n=0; n < num_standard_features; n++, fp++)
    if (fp->feature_id == feature_id)
      break;
  if (n < num_standard_features)
    return; // Feature requirements already set by application

  // Create the feature
  if (max_standard_features == n)
    {
      max_standard_features += n + 10;
      fp = new jx_feature[max_standard_features];
      for (n=0; n < num_standard_features; n++)
        fp[n] = standard_features[n];
      if (standard_features != NULL)
        delete[] standard_features;
      standard_features = fp;
      fp += n;
    }
  num_standard_features++;
  fp->feature_id = feature_id;
  
  // Find an unused "fully understand" sub-expression index and use it
  int m;
  kdu_uint32 mask, subx;

  for (m=0; m < 8; m++)
    if ((mask = fully_understand_mask[m]) != 0xFFFFFFFF)
      break;
  if (m < 8)
    { // Otherwise, we are all out of sub-expressions: highly unlikely.
      for (subx=1<<31; subx & mask; subx>>=1);
      fully_understand_mask[m] |= subx;
      fp->fully_understand[m] |= subx;
    }

  if (add_to_both)
    { // Find an unused "decode completely" sub-expression index and use it
      for (m=0; m < 8; m++)
        if ((mask = decode_completely_mask[m]) != 0xFFFFFFFF)
          break;
      if (m < 8)
        { // Otherwise, we are all out of sub-expressions: highly unlikely.
          for (subx=1<<31; subx & mask; subx>>=1);
          decode_completely_mask[m] |= subx;
          fp->decode_completely[m] |= subx;
        }
    }

  // Check for features which will negate any of the special negative features
  if ((feature_id == JPX_SF_OPACITY_NOT_PREMULTIPLIED) ||
      (feature_id == JPX_SF_OPACITY_BY_CHROMA_KEY))
    no_opacity = false;
  if ((feature_id == JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL_AND_ORDERED) ||
      (feature_id == JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL) ||
      (feature_id == JPX_SF_CODESTREAM_FRAGMENTS_LOCAL) ||
      (feature_id == JPX_SF_CODESTREAM_FRAGMENTS_REMOTE))
    no_fragments = false;
  if ((feature_id == JPX_SF_SCALING_WITHIN_LAYER) ||
      (feature_id == JPX_SF_SCALING_BETWEEN_LAYERS))
    no_scaling = false;
  if (feature_id == JPX_SF_MULTIPLE_CODESTREAMS_PER_LAYER)
    single_stream_layers = false;
}

/*****************************************************************************/
/*                       jx_compatibility::save_boxes                        */
/*****************************************************************************/

void
  jx_compatibility::save_boxes(jx_target *owner)
{
  // Start by writing the file-type box
  owner->open_top_box(&out,jp2_file_type_4cc);
  assert(!is_jp2); // We can read JP2 or JPX, but only write JPX.
  out.write(jpx_brand);
  out.write((kdu_uint32) 0);
  out.write(jpx_brand);
  if (is_jp2_compatible)
    out.write(jp2_brand);
  if (is_jpxb_compatible)
    out.write(jpx_baseline_brand);
  out.close();

  // Now for the reader requirements box.  First, set negative features
  assert(have_rreq_box);
  if (no_extensions)
    add_standard_feature(JPX_SF_CODESTREAM_NO_EXTENSIONS);
  if (no_opacity)
    add_standard_feature(JPX_SF_NO_OPACITY);
  if (no_fragments)
    add_standard_feature(JPX_SF_CODESTREAM_CONTIGUOUS);
  if (no_scaling)
    add_standard_feature(JPX_SF_NO_SCALING);
  if (single_stream_layers)
    add_standard_feature(JPX_SF_ONE_CODESTREAM_PER_LAYER);
  
  // Next, we need to merge the fully understand and decode completely
  // sub-expressions, which may require quite a bit of manipulation.
  kdu_uint32 new_fumask[8] = {0,0,0,0,0,0,0,0};
  kdu_uint32 new_dcmask[8] = {0,0,0,0,0,0,0,0};
  int k, n, src_word, dst_word, src_shift, dst_shift;
  kdu_uint32 src_mask, dst_mask;
  int total_mask_bits=0;
  for (src_word=dst_word=0, src_mask=dst_mask=(1<<31); src_word < 8; )
    {
      if (fully_understand_mask[src_word] & src_mask)
        {
          for (n=0; n < num_standard_features; n++)
            {
              jx_feature *fp = standard_features + n;
              if (fp->fully_understand[src_word] & src_mask)
                fp->mask[dst_word] |= dst_mask;
            }
          for (n=0; n < num_vendor_features; n++)
            {
              jx_vendor_feature *fp = vendor_features + n;
              if (fp->fully_understand[src_word] & src_mask)
                fp->mask[dst_word] |= dst_mask;
            }
          new_fumask[dst_word] |= dst_mask;
          total_mask_bits++;
          if ((dst_mask >>= 1) == 0)
            { dst_mask = 1<<31; dst_word++; }
        }
      if ((src_mask >>= 1) == 0)
        { src_mask = 1<<31; src_word++; }
    }

  for (src_word=0, src_shift=31, src_mask=(1<<31); src_word < 8; )
    {
      if (decode_completely_mask[src_word] & src_mask)
        { // Scan existing sub-expressions looking for one which matches
          bool expressions_match = false;
          for (k=0, dst_word=0, dst_shift=31, dst_mask=(1<<31);
               k < total_mask_bits; k++)
            {
              expressions_match=true;
              for (n=0; (n < num_standard_features) && expressions_match; n++)
                {
                  jx_feature *fp = standard_features + n;
                  if ((((fp->decode_completely[src_word] >> src_shift) ^
                        (fp->mask[dst_word] >> dst_shift)) & 1) != 0)
                    expressions_match = false;
                }
              for (n=0; (n < num_vendor_features) && expressions_match; n++)
                {
                  jx_vendor_feature *fp = vendor_features + n;
                  if ((((fp->decode_completely[src_word] >> src_shift) ^
                        (fp->mask[dst_word] >> dst_shift)) & 1) != 0)
                    expressions_match = false;
                }
              dst_shift--; dst_mask >>= 1;
              if (dst_shift < 0)
                { dst_mask = 1<<31; dst_shift=31; dst_word++; }
            }
          if (!expressions_match)
            {
              if (total_mask_bits == 256)
                continue; // Not enough bits to store this new sub-expression
              total_mask_bits++; // This is a new sub-expression
            }
          new_dcmask[dst_word] |= dst_mask;
        }
      src_shift--; src_mask >>= 1;
      if (src_shift < 0)
        { src_mask = 1<<31; src_shift=31; src_word++; }
    }

  for (n=0; n < 8; n++)
    {
      fully_understand_mask[n] = new_fumask[n];
      decode_completely_mask[n] = new_dcmask[n];
    }

  // Now we are ready to write the reader requirements box
  owner->open_top_box(&out,jp2_reader_requirements_4cc);
  kdu_byte mask_length = (kdu_byte)((total_mask_bits+7)>>3);
  out.write(mask_length);
  for (k=mask_length, src_word=0, src_shift=24; k > 0; k--, src_shift-=8)
    {
      if (src_shift < 0)
        { src_shift = 24; src_word++; }
      out.write((kdu_byte)(fully_understand_mask[src_word] >> src_shift));
    }
  for (k=mask_length, src_word=0, src_shift=24; k > 0; k--, src_shift-=8)
    {
      if (src_shift < 0)
        { src_shift = 24; src_word++; }
      out.write((kdu_byte)(decode_completely_mask[src_word] >> src_shift));
    }
  out.write((kdu_uint16) num_standard_features);
  for (n=0; n < num_standard_features; n++)
    {
      jx_feature *fp = standard_features + n;
      out.write(fp->feature_id);
      for (k=mask_length, src_word=0, src_shift=24; k > 0; k--, src_shift-=8)
        {
          if (src_shift < 0)
            { src_shift = 24; src_word++; }
          out.write((kdu_byte)(fp->mask[src_word] >> src_shift));
        }
    }
  out.write((kdu_uint16) num_vendor_features);
  for (n=0; n < num_vendor_features; n++)
    {
      jx_vendor_feature *fp = vendor_features + n;
      for (k=0; k < 16; k++)
        out.write(fp->uuid[k]);

      for (k=mask_length, src_word=0, src_shift=24; k > 0; k--, src_shift-=8)
        {
          if (src_shift < 0)
            { src_shift = 24; src_word++; }
          out.write((kdu_byte)(fp->mask[src_word] >> src_shift));
        }
    }
  out.close();
}


/* ========================================================================= */
/*                             jpx_compatibility                             */
/* ========================================================================= */

/*****************************************************************************/
/*                         jpx_compatibility::is_jp2                         */
/*****************************************************************************/

bool
  jpx_compatibility::is_jp2()
{
  return (state != NULL) && state->is_jp2;
}

/*****************************************************************************/
/*                    jpx_compatibility::is_jp2_compatible                   */
/*****************************************************************************/

bool
  jpx_compatibility::is_jp2_compatible()
{
  return (state != NULL) && state->is_jp2_compatible;
}

/*****************************************************************************/
/*                    jpx_compatibility::is_jpxb_compatible                  */
/*****************************************************************************/

bool
  jpx_compatibility::is_jpxb_compatible()
{
  return (state != NULL) && state->is_jpxb_compatible;
}

/*****************************************************************************/
/*               jpx_compatibility::has_reader_requirements_box              */
/*****************************************************************************/

bool
  jpx_compatibility::has_reader_requirements_box()
{
  return (state != NULL) && state->have_rreq_box;
}

/*****************************************************************************/
/*                  jpx_compatibility::check_standard_feature                */
/*****************************************************************************/

bool
  jpx_compatibility::check_standard_feature(kdu_uint16 feature_id)
{
  if ((state == NULL) || !state->have_rreq_box)
    return false;
  for (int n=0; n < state->num_standard_features; n++)
    if (state->standard_features[n].feature_id == feature_id)
      return true;
  return false;
}

/*****************************************************************************/
/*                   jpx_compatibility::check_vendor_feature                 */
/*****************************************************************************/

bool
  jpx_compatibility::check_vendor_feature(kdu_byte uuid[])
{
  if ((state == NULL) || !state->have_rreq_box)
    return false;
  for (int n=0; n < state->num_vendor_features; n++)
    if (memcmp(state->vendor_features[n].uuid,uuid,16) == 0)
      return true;
  return false;
}

/*****************************************************************************/
/*                   jpx_compatibility::get_standard_feature                 */
/*****************************************************************************/

bool
  jpx_compatibility::get_standard_feature(int which, kdu_uint16 &feature_id)
{
  if ((state==NULL) || (!state->have_rreq_box) ||
      (which >= state->num_standard_features) || (which < 0))
    return false;
  feature_id = state->standard_features[which].feature_id;
  return true;
}

/*****************************************************************************/
/*              jpx_compatibility::get_standard_feature (extended)           */
/*****************************************************************************/

bool
  jpx_compatibility::get_standard_feature(int which, kdu_uint16 &feature_id,
                                          bool &is_supported)
{
  if ((state==NULL) || (!state->have_rreq_box) ||
      (which >= state->num_standard_features) || (which < 0))
    return false;
  feature_id = state->standard_features[which].feature_id;
  is_supported = state->standard_features[which].supported;
  return true;
}

/*****************************************************************************/
/*                    jpx_compatibility::get_vendor_feature                  */
/*****************************************************************************/

bool
  jpx_compatibility::get_vendor_feature(int which, kdu_byte uuid[])
{
  if ((state==NULL) || (!state->have_rreq_box) ||
      (which >= state->num_vendor_features) || (which < 0))
    return false;
  memcpy(uuid,state->vendor_features[which].uuid,16);
  return true;
}

/*****************************************************************************/
/*               jpx_compatibility::get_vendor_feature (extended)            */
/*****************************************************************************/

bool
  jpx_compatibility::get_vendor_feature(int which, kdu_byte uuid[],
                                        bool &is_supported)
{
  if ((state==NULL) || (!state->have_rreq_box) ||
      (which >= state->num_vendor_features) || (which < 0))
    return false;
  memcpy(uuid,state->vendor_features[which].uuid,16);
  is_supported = state->vendor_features[which].supported;
  return true;
}

/*****************************************************************************/
/*               jpx_compatibility::set_standard_feature_support             */
/*****************************************************************************/

void
  jpx_compatibility::set_standard_feature_support(kdu_uint16 feature_id,
                                                  bool is_supported)
{
  int n;
  if ((state != NULL) && state->have_rreq_box)
    {
      for (n=0; n < state->num_standard_features; n++)
        if (feature_id == state->standard_features[n].feature_id)
          {
            state->standard_features[n].supported = is_supported;
            break;
          }
    }
}

/*****************************************************************************/
/*                jpx_compatibility::set_vendor_feature_support              */
/*****************************************************************************/

void
  jpx_compatibility::set_vendor_feature_support(const kdu_byte uuid[],
                                                bool is_supported)
{
  int n;
  if ((state != NULL) && state->have_rreq_box)
    {
      for (n=0; n < state->num_vendor_features; n++)
        if (memcmp(uuid,state->vendor_features[n].uuid,16) == 0)
          {
            state->vendor_features[n].supported = is_supported;
            break;
          }
    }
}

/*****************************************************************************/
/*                  jpx_compatibility::test_fully_understand                 */
/*****************************************************************************/

bool
  jpx_compatibility::test_fully_understand()
{
  if (state == NULL)
    return false;
  if (!state->have_rreq_box)
    return true;

  int n, m;
  kdu_uint32 mask[8] = {0,0,0,0,0,0,0,0};
  for (n=0; n < state->num_standard_features; n++)
    {
      jx_compatibility::jx_feature *fp = state->standard_features + n;
      if (fp->supported)
        for (m=0; m < 8; m++)
          mask[m] |= fp->mask[m];
    }
  for (n=0; n < state->num_vendor_features; n++)
    {
      jx_compatibility::jx_vendor_feature *fp = state->vendor_features + n;
      if (fp->supported)
        for (m=0; m < 8; m++)
          mask[m] |= fp->mask[m];
    }
  for (m=0; m < 8; m++)
    if ((mask[m] & state->fully_understand_mask[m]) !=
        state->fully_understand_mask[m])
      return false;
  return true;
}

/*****************************************************************************/
/*                 jpx_compatibility::test_decode_completely                 */
/*****************************************************************************/

bool
  jpx_compatibility::test_decode_completely()
{
  if (state == NULL)
    return false;
  if (!state->have_rreq_box)
    return true;

  int n, m;
  kdu_uint32 mask[8] = {0,0,0,0,0,0,0,0};
  for (n=0; n < state->num_standard_features; n++)
    {
      jx_compatibility::jx_feature *fp = state->standard_features + n;
      if (fp->supported)
        for (m=0; m < 8; m++)
          mask[m] |= fp->mask[m];
    }
  for (n=0; n < state->num_vendor_features; n++)
    {
      jx_compatibility::jx_vendor_feature *fp = state->vendor_features + n;
      if (fp->supported)
        for (m=0; m < 8; m++)
          mask[m] |= fp->mask[m];
    }
  for (m=0; m < 8; m++)
    if ((mask[m] & state->decode_completely_mask[m]) !=
        state->decode_completely_mask[m])
      return false;
  return true;
}

/*****************************************************************************/
/*                       jpx_compatibility::copy                             */
/*****************************************************************************/

void
  jpx_compatibility::copy(jpx_compatibility src)
{
  state->copy_from(src.state);
}

/*****************************************************************************/
/*               jpx_compatibility::set_used_standard_feature                */
/*****************************************************************************/

void
  jpx_compatibility::set_used_standard_feature(kdu_uint16 feature_id,
                                               kdu_byte fusx_idx,
                                               kdu_byte dcsx_idx)
{
  if (state == NULL)
    return;
  state->have_rreq_box = true; // Just in case

  int n;
  jx_compatibility::jx_feature *fp = state->standard_features;
  for (n=0; n < state->num_standard_features; n++, fp++)
    if (fp->feature_id == feature_id)
      break;
  if (n == state->num_standard_features)
    { // Create the feature
      if (state->max_standard_features == n)
        {
          state->max_standard_features += n + 10;
          fp = new jx_compatibility::jx_feature[state->max_standard_features];
          for (n=0; n < state->num_standard_features; n++)
            fp[n] = state->standard_features[n];
          if (state->standard_features != NULL)
            delete[] state->standard_features;
          state->standard_features = fp;
          fp += n;
        }
      state->num_standard_features++;
    }

  fp->feature_id = feature_id;
  if (fusx_idx < 255)
    fp->fully_understand[fusx_idx >> 5] |= (1 << (31-(fusx_idx & 31)));
  if (dcsx_idx < 255)
    fp->decode_completely[dcsx_idx >> 5] |= (1 << (31-(dcsx_idx & 31)));
}

/*****************************************************************************/
/*                jpx_compatibility::set_used_vendor_feature                 */
/*****************************************************************************/

void
  jpx_compatibility::set_used_vendor_feature(const kdu_byte uuid[],
                                             kdu_byte fusx_idx,
                                             kdu_byte dcsx_idx)
{
  if (state == NULL)
    return;
  state->have_rreq_box = true; // Just in case

  int n;
  jx_compatibility::jx_vendor_feature *fp = state->vendor_features;
  for (n=0; n < state->num_vendor_features; n++, fp++)
    if (memcpy(fp->uuid,uuid,16) == 0)
      break;
  if (n == state->num_vendor_features)
    { // Create the feature
      if (state->max_vendor_features == n)
        {
          state->max_vendor_features += n + 10;
          fp = new
            jx_compatibility::jx_vendor_feature[state->max_vendor_features];
          for (n=0; n < state->num_vendor_features; n++)
            fp[n] = state->vendor_features[n];
          if (state->vendor_features != NULL)
            delete[] state->vendor_features;
          state->vendor_features = fp;
          fp += n;
        }
      state->num_vendor_features++;
    }

  memcpy(fp->uuid,uuid,16);
  if (fusx_idx < 255)
    {
      fp->fully_understand[fusx_idx>>5] |= (1<<(31-(fusx_idx & 31)));
      state->fully_understand_mask[fusx_idx>>5] |= (1<<(31-(fusx_idx & 31)));
    }
  if (dcsx_idx < 255)
    {
      fp->decode_completely[dcsx_idx>>5] |= (1<<(31-(dcsx_idx & 31)));
      state->decode_completely_mask[dcsx_idx>>5] |= (1<<(31-(dcsx_idx & 31)));
    }
}


/* ========================================================================= */
/*                               jx_composition                              */
/* ========================================================================= */

/*****************************************************************************/
/*                         jx_composition::add_frame                         */
/*****************************************************************************/

void
  jx_composition::add_frame()
{
  if (tail == NULL)
    {
      head = tail = new jx_frame;
      return;
    }
  if (tail->persistent)
    last_persistent_frame = tail;
  tail->next = new jx_frame;
  tail->next->prev = tail;
  tail = tail->next;
  tail->last_persistent_frame = last_persistent_frame;
  last_frame_max_lookahead = max_lookahead;
}

/*****************************************************************************/
/*                   jx_composition::donate_composition_box                  */
/*****************************************************************************/

void
  jx_composition::donate_composition_box(jp2_input_box &src,
                                         jx_source *owner)
{
  if (comp_in.exists())
    { KDU_WARNING(w,0); w <<
        KDU_TXT("JPX data source appears to contain multiple "
        "composition boxes!! This is illegal.  "
        "All but first will be ignored.");
      return;
    }
  comp_in.transplant(src);
  num_parsed_iset_boxes = 0;
  finish(owner);
}

/*****************************************************************************/
/*                           jx_composition::finish                          */
/*****************************************************************************/

bool
  jx_composition::finish(jx_source *owner)
{
  if (is_complete)
    return true;
  while ((!comp_in) && !owner->is_top_level_complete())
    if (!owner->parse_next_top_level_box())
      break;
  if (!comp_in)
    return owner->is_top_level_complete();

  assert(comp_in.get_box_type() == jp2_composition_4cc);
  if (!comp_in.is_complete())
    return false;
  while (sub_in.exists() || sub_in.open(&comp_in))
    {
      if (sub_in.get_box_type() == jp2_comp_options_4cc)
        {
          if (!sub_in.is_complete())
            return false;
          kdu_uint32 height, width;
          kdu_byte loop;
          if (!(sub_in.read(height) && sub_in.read(width) &&
                sub_in.read(loop) && (height>0) && (width>0)))
            { KDU_ERROR(e,12); e <<
                KDU_TXT("Malformed Composition Options (copt) box "
                "found in JPX data source.  Insufficient or illegal field "
                "values encountered.  The height and width parameters must "
                "also be non-zero.");
            }
          size.x = (int) width;
          size.y = (int) height;
          if (loop == 255)
            loop_count = 0;
          else
            loop_count = ((int) loop) + 1;
        }
      else if (sub_in.get_box_type() == jp2_comp_instruction_set_4cc)
        {
          if (!sub_in.is_complete())
            return false;
          kdu_uint16 flags, rept;
          kdu_uint32 tick;
          if (!(sub_in.read(flags) && sub_in.read(rept) &&
                sub_in.read(tick)))
            { KDU_ERROR(e,13); e <<
                KDU_TXT("Malformed Instruction Set (inst) box "
                "found in JPX data source.  Insufficient fields encountered.");
            }
          bool have_target_pos = ((flags & 1) != 0);
          bool have_target_size = ((flags & 2) != 0);
          bool have_life_persist = ((flags & 4) != 0);
          bool have_source_region = ((flags & 32) != 0);
          if (!(have_target_pos || have_target_size ||
                have_life_persist || have_source_region))
            {
              sub_in.close();
              num_parsed_iset_boxes++;
              continue;
            }
          kdu_long start_pos = sub_in.get_pos();
          for (int repeat_count=rept; repeat_count >= 0; repeat_count--)
            {
              int inum_idx = 0;
              jx_frame *start_frame = tail;
              jx_instruction *start_tail = (tail==NULL)?NULL:(tail->tail);
              while (parse_instruction(have_target_pos,have_target_size,
                                       have_life_persist,have_source_region,
                                       tick))
                {
                  tail->tail->iset_idx = num_parsed_iset_boxes;
                  tail->tail->inum_idx = inum_idx++;
                }
              if (sub_in.get_remaining_bytes() > 0)
                { KDU_ERROR(e,14); e <<
                    KDU_TXT("Malformed Instruction Set (inst) box "
                    "encountered in JPX data source.  Box appears to "
                    "be too long.");
                }
              sub_in.seek(start_pos); // In case we need to repeat it all
              if (repeat_count < 2)
                continue;

              /* At this point, we have only to see if we can handle
                 repeats by adding a repeat count to the last frame.  For
                 this to work, we need to know that this instruction set
                 represents exactly one frame, that the frame is
                 non-persistent, and that the `next_use' pointers in the
                 frame either point beyond the repeat sequence, or to the
                 immediate next repetition instance.  Even then, we
                 cannot be sure that the first frame has exactly the same
                 number of reused layers as the remaining ones, so we will
                 have to make a copy of the frame and repeat only the
                 second copy. */
              if (tail == start_frame)
                continue; // Instruction set did not create a new frame
              if (tail->duration == 0)
                continue; // Instruction set did not complete a frame
              if ((start_frame == NULL) && (tail != head))
                continue; // Instruction set created multiple frames
              if ((start_frame != NULL) &&
                  ((tail != start_frame->next) ||
                   (start_frame->tail != start_tail)))
                continue; // Instruction set contributed to more than one frame

              jx_instruction *ip;
              int max_repeats = INT_MAX;
              if (last_frame_max_lookahead >= tail->num_instructions)
                max_repeats =
                  (last_frame_max_lookahead / tail->num_instructions) - 1;
              if (max_repeats == 0)
                continue;
              int remaining = tail->num_instructions;
              int reused_layers_in_frame = 0;
              for (ip=tail->head; ip != NULL; ip=ip->next, remaining--)
                {
                  if (ip->next_reuse == tail->num_instructions)
                    reused_layers_in_frame++; // Re-used in next frame
                  else if (ip->next_reuse != 0)
                    break;
                }
              if (ip != NULL)
                continue; // No repetitions
               
              // If we get here, we can repeat at most `max_repeats' times
              assert(remaining == 0);
              assert(max_repeats >= 0);
              if (max_repeats <= 1)
                continue; // Need at least 2 repetitions, since we cannot
                          // reliably start repetiting until the 2'nd instance
              start_frame = tail;
              add_frame();
              repeat_count--;
              max_repeats--;
              max_lookahead -= tail->num_instructions;
              tail->increment =
                start_frame->num_instructions - reused_layers_in_frame;
              tail->persistent = start_frame->persistent;
              tail->duration = start_frame->duration;
              for (ip=start_frame->head; ip != NULL; ip=ip->next)
                {
                  jx_instruction *inst = tail->add_instruction(ip->visible);
                  inst->layer_idx = -1; // Evaluate this later on
                  inst->source_dims = ip->source_dims;
                  inst->target_dims = ip->target_dims;
                  if ((inst->next_reuse=ip->next_reuse) == 0)
                    inst->increment = tail->increment;
                }
              if (max_repeats == INT_MAX)
                { // No restriction on number of repetitions
                  if (rept == 0xFFFF)
                    tail->repeat_count = -1; // Repeat indefinitely
                  else
                    tail->repeat_count = repeat_count;
                  repeat_count = 0; // Finish looping
                }
              else
                {
                  if (rept == 0xFFFF)
                    tail->repeat_count = max_repeats;
                  else if (repeat_count > max_repeats)
                    {
                      tail->repeat_count = max_repeats;
                      repeat_count -= tail->repeat_count;
                    }
                  else
                    {
                      tail->repeat_count = repeat_count;
                      repeat_count = 0;
                    }
                }
              max_lookahead -= tail->repeat_count*tail->num_instructions;
              if (tail->repeat_count < 0)
                max_lookahead = 0;
            }
          num_parsed_iset_boxes++;
        }
      sub_in.close();
    }
  comp_in.close();
  is_complete = true;

  assign_layer_indices();
  remove_invisible_instructions();
  return true;
}

/*****************************************************************************/
/*                     jx_composition::parse_instruction                     */
/*****************************************************************************/

bool
  jx_composition::parse_instruction(bool have_target_pos,
                                    bool have_target_size,
                                    bool have_life_persist,
                                    bool have_source_region,
                                    kdu_uint32 tick)
{
  if (!(have_target_pos || have_target_size ||
        have_life_persist || have_source_region))
    return false;

  kdu_dims source_dims, target_dims;
  if (have_target_pos)
    {
      kdu_uint32 X0, Y0;
      if (!sub_in.read(X0))
        return false;
      if (!sub_in.read(Y0))
        { KDU_ERROR(e,15); e <<
            KDU_TXT("Malformed Instruction Set (inst) box "
            "found in JPX data source.  Terminated unexpectedly.");
        }
      target_dims.pos.x = (int) X0;
      target_dims.pos.y = (int) Y0;
    }
  else
    target_dims.pos = kdu_coords(0,0);

  if (have_target_size)
    {
      kdu_uint32 XS, YS;
      if (!(sub_in.read(XS) || have_target_pos))
        return false;
      if (!sub_in.read(YS))
        { KDU_ERROR(e,16); e <<
            KDU_TXT("Malformed Instruction Set (inst) box "
            "found in JPX data source.  Terminated unexpectedly.");
        }
      target_dims.size.x = (int) XS;
      target_dims.size.y = (int) YS;
    }
  else
    target_dims.size = size;

  bool persistent = true;
  kdu_uint32 life=0, next_reuse=0;
  if (have_life_persist)
    {
      if (!(sub_in.read(life) || have_target_pos || have_target_size))
        return false;
      if (!sub_in.read(next_reuse))
        { KDU_ERROR(e,17); e <<
            KDU_TXT("Malformed Instruction Set (inst) box "
            "found in JPX data source.  Terminated unexpectedly.");
        }
      if (life & 0x80000000)
        life &= 0x7FFFFFFF;
      else
        persistent = false;
    }

  if (have_source_region)
    {
      kdu_uint32 XC, YC, WC, HC;
      if (!(sub_in.read(XC) || have_target_pos || have_target_size ||
            have_life_persist))
        return false;
      if (!(sub_in.read(YC) && sub_in.read(WC) && sub_in.read(HC)))
        { KDU_ERROR(e,18); e <<
            KDU_TXT("Malformed Instruction Set (inst) box "
            "found in JPX data source.  Terminated unexpectedly.");
        }
      source_dims.pos.x = (int) XC;
      source_dims.pos.y = (int) YC;
      source_dims.size.x = (int) WC;
      source_dims.size.y = (int) HC;
    }

  if ((tail == NULL) || (tail->duration != 0))
    add_frame();
  jx_instruction *inst = tail->add_instruction((life != 0) || persistent);
  inst->source_dims = source_dims;
  inst->target_dims = target_dims;
  inst->layer_idx = -1; // Evaluate this later on.
  inst->next_reuse = (int) next_reuse;
  max_lookahead--;
  if (inst->next_reuse > max_lookahead)
    max_lookahead = inst->next_reuse;
  tail->duration = (int)(life*tick);
  tail->persistent = persistent;
  return true;
}

/*****************************************************************************/
/*                    jx_composition::assign_layer_indices                   */
/*****************************************************************************/

void
  jx_composition::assign_layer_indices()
{
  jx_frame *fp, *fpp;
  jx_instruction *ip, *ipp;
  int inum, reuse, layer_idx=0;
  for (fp=head; fp != NULL; fp=fp->next)
    for (inum=0, ip=fp->head; ip != NULL; ip=ip->next, inum++)
      {
        if (ip->layer_idx < 0)
          ip->layer_idx = layer_idx++;
        if ((reuse=ip->next_reuse) > 0)
          {
            for (ipp=ip, fpp=fp; reuse > 0; reuse--)
              {
                ipp = ipp->next;
                if (ipp == NULL)
                  {
                    if ((fpp->repeat_count > 0) && (fp != fpp))
                      { // Reusing across a repeated sequence of frames
                        reuse -= fpp->repeat_count*fpp->num_instructions;
                        if (reuse <= 0)
                          { KDU_ERROR(e,19); e <<
                              KDU_TXT("Illegal re-use "
                              "count found in a compositing instruction "
                              "within the JPX composition box.  The specified "
                              "re-use counts found in the box lead to "
                              "multiple conflicting definitions for the "
                              "compositing layer which should be used by a "
                              "particular instruction.");
                          }
                      }
                    fpp=fpp->next;
                    if (fpp == NULL)
                      break;
                    ipp = fpp->head;
                  }
              }
            if ((ipp != NULL) && (reuse == 0))
              ipp->layer_idx = ip->layer_idx;
          }
      }
}

/*****************************************************************************/
/*               jx_composition::remove_invisible_instructions               */
/*****************************************************************************/

void
  jx_composition::remove_invisible_instructions()
{
  jx_frame *fp, *fnext;
  for (fp=head; fp != NULL; fp=fnext)
    {
      fnext = fp->next;

      jx_instruction *ip, *inext;
      for (ip=fp->head; ip != NULL; ip=inext)
        {
          inext = ip->next;
          if (ip->visible)
            continue;

          fp->num_instructions--;
          if (ip->prev == NULL)
            {
              assert(fp->head == ip);
              fp->head = inext;
            }
          else
            ip->prev->next = inext;
          if (inext == NULL)
            {
              assert(fp->tail == ip);
              fp->tail = ip->prev;
            }
          else
            inext->prev = ip->prev;
          delete ip;
        }

      if (fp->head == NULL)
        { // Empty frame detected
          assert(fp->num_instructions == 0);
          if (fp->prev == NULL)
            {
              assert(head == fp);
              head = fnext;
            }
          else
            {
              fp->prev->next = fnext;
              fp->prev->duration += fp->duration;
            }
          if (fnext == NULL)
            {
              assert(tail == fp);
              tail = fp->prev;
            }
          else
            fnext->prev = fp->prev;
          delete fp;
        }
    }
}

/*****************************************************************************/
/*                         jx_composition::finalize                          */
/*****************************************************************************/

void
  jx_composition::finalize(jx_target *owner)
{
  if (is_complete)
    return;
  is_complete = true;

  if (head == NULL)
    return;

  // Start by going through all frames and instructions, verifying that each
  // frame has at least one instruction, expanding repeated frames, and
  // terminating the sequence once we find the first frame which uses
  // a non-existent compositing layer.
  int num_layers = owner->get_num_layers();
  jx_frame *fp;
  jx_instruction *ip;
  for (fp=head; fp != NULL; fp=fp->next)
    {
      if (fp->head == NULL)
        { KDU_ERROR_DEV(e,20); e <<
            KDU_TXT("You must add at least one compositing "
            "instruction to every frame created using "
            "`jpx_composition::add_frame'.");
        }
      if (fp->repeat_count != 0)
        { // Split off the first frame from the rest of the repeating sequence
          jx_frame *new_fp = new jx_frame;
          new_fp->persistent = fp->persistent;
          new_fp->duration = fp->duration;
          if (fp->repeat_count < 0)
            new_fp->repeat_count = -1;
          else
            new_fp->repeat_count = fp->repeat_count-1;
          fp->repeat_count = 0;
          jx_instruction *new_ip;
          for (ip=fp->head; ip != NULL; ip=ip->next)
            {
              assert(ip->visible);
              new_ip = new_fp->add_instruction(true);
              new_ip->layer_idx = ip->layer_idx + ip->increment;
              new_ip->increment = ip->increment;
              new_ip->source_dims = ip->source_dims;
              new_ip->target_dims = ip->target_dims;
            }
          new_fp->next = fp->next;
          new_fp->prev = fp;
          fp->next = new_fp;
          if (new_fp->next != NULL)
            new_fp->next->prev = new_fp;
          else
            {
              assert(fp == tail);
              tail = new_fp;
            }
        }
      bool have_invalid_layer = false;
      for (ip=fp->head; ip != NULL; ip=ip->next)
        {
          ip->next_reuse = 0; // Just in case not properly initialized
          if ((ip->layer_idx >= num_layers) || (ip->layer_idx < 0))
            have_invalid_layer = true;
          kdu_coords lim = ip->target_dims.pos + ip->target_dims.size;
          if (lim.x > size.x)
            size.x = lim.x;
          if (lim.y > size.y)
            size.y = lim.y;
        }
      if (have_invalid_layer && (fp != head))
        break;
    }

  if (fp != NULL)
    { // Delete this and all subsequent layers; can't play them
      tail = fp->prev;
      assert(tail != NULL);
      while ((fp=tail->next) != NULL)
        {
          tail->next = fp->next;
          delete fp;
        }
    }

  // Now introduce invisibles so as to get the layer sequence right
  // At this point, we will set `first_use' to true whenever we encounter
  // an instruction which uses a compositing layer for the first time.
  // This will help make the ensuing algorithm for finding the `next_reuse'
  // links more efficient.
  int layer_idx=0;
  for (fp=head; fp != NULL; fp=fp->next)
    {
      for (ip=fp->head; ip != NULL; ip=ip->next)
        {
          while (ip->layer_idx > layer_idx)
            {
              jx_instruction *new_ip = new jx_instruction;
              new_ip->visible = false;
              new_ip->layer_idx = layer_idx++;
              new_ip->first_use = true;
              new_ip->next = ip;
              new_ip->prev = ip->prev;
              ip->prev = new_ip;
              if (ip == fp->head)
                {
                  fp->head = new_ip;
                  assert(new_ip->prev == NULL);
                }
              else
                new_ip->prev->next = new_ip;
              fp->num_instructions++;
            }
          if (ip->layer_idx == layer_idx)
            {
              ip->first_use = true;
              layer_idx++;
            }
        }
    }

  // Finally, figure out the complete set of `next_reuse' links by walking
  // backwards through the entire sequence of instructions
  jx_frame *fscan;
  jx_instruction *iscan;
  int igap;
  for (fp=tail; fp != NULL; fp=fp->prev)
    {
      for (ip=fp->tail; ip != NULL; ip=ip->prev)
        {
          if (ip->first_use)
            continue;
          iscan = ip->prev;
          fscan = fp;
          igap = 1;
          while ((iscan == NULL) || (iscan->layer_idx != ip->layer_idx))
            {
              if (iscan == NULL)
                {
                  fscan = fscan->prev;
                  if (fscan == NULL)
                    break;
                  iscan = fscan->tail;
                }
              else
                {
                  iscan = iscan->prev;
                  igap++;
                }
            }
          assert(iscan != NULL); // Should not be possible if above algorithm
                     // for assigning invisible instructions worked correctly
          iscan->next_reuse = igap;
        }
    }
}

/*****************************************************************************/
/*                    jx_composition::adjust_compatibility                   */
/*****************************************************************************/

void
  jx_composition::adjust_compatibility(jx_compatibility *compatibility,
                                       jx_target *owner)
{
  if (!is_complete)
    finalize(owner);

  jx_frame *fp;
  jx_instruction *ip;

  if ((head == NULL) || (head->head == head->tail))
    compatibility->add_standard_feature(JPX_SF_COMPOSITING_NOT_REQUIRED);
  int num_layers = owner->get_num_layers();
  if (head == NULL)
    {
      if (num_layers > 1)
        compatibility->add_standard_feature(
          JPX_SF_MULTIPLE_LAYERS_NO_COMPOSITING_OR_ANIMATION);
      return;
    }
  compatibility->add_standard_feature(JPX_SF_COMPOSITING_USED);
  if (head == tail)
    compatibility->add_standard_feature(JPX_SF_NO_ANIMATION);
  else
    { // Check first layer coverage, layer reuse and frame persistence
      bool first_layer_covers = true;
      bool layers_reused = false;
      bool frames_all_persistent = true;
      for (fp=head; fp != NULL; fp=fp->next)
        {
          if ((fp->head->layer_idx != 0) ||
              (fp->head->target_dims.pos.x != 0) ||
              (fp->head->target_dims.pos.y != 0) ||
              (fp->head->target_dims.size != size))
            first_layer_covers = false;
          for (ip=fp->head; ip != NULL; ip=ip->next)
            if (ip->next_reuse > 0)
              layers_reused = true;
          if (!fp->persistent)
            frames_all_persistent = false;
        }
      if (first_layer_covers)
        compatibility->add_standard_feature(
          JPX_SF_ANIMATED_COVERED_BY_FIRST_LAYER);
      else
        compatibility->add_standard_feature(
          JPX_SF_ANIMATED_NOT_COVERED_BY_FIRST_LAYER);
      if (layers_reused)
        compatibility->add_standard_feature(JPX_SF_ANIMATED_LAYERS_REUSED);
      else
        compatibility->add_standard_feature(JPX_SF_ANIMATED_LAYERS_NOT_REUSED);
      if (frames_all_persistent)
        compatibility->add_standard_feature(
          JPX_SF_ANIMATED_PERSISTENT_FRAMES);
      else
        compatibility->add_standard_feature(
          JPX_SF_ANIMATED_NON_PERSISTENT_FRAMES);
    }

  // Check for scaling
  bool scaling_between_layers=false;
  for (fp=head; fp != NULL; fp=fp->next)
    for (ip=fp->head; ip != NULL; ip=ip->next)
      if (ip->source_dims.size != ip->target_dims.size)
        scaling_between_layers = true;
  if (scaling_between_layers)
    compatibility->add_standard_feature(JPX_SF_SCALING_BETWEEN_LAYERS);
}

/*****************************************************************************/
/*                          jx_composition::save_box                         */
/*****************************************************************************/

void
  jx_composition::save_box(jx_target *owner)
{
  if (!is_complete)
    finalize(owner);

  if (head == NULL)
    return; // No composition information provided; do not write the box.

  owner->open_top_box(&comp_out,jp2_composition_4cc);
  jp2_output_box sub;

  sub.open(&comp_out,jp2_comp_options_4cc);
  kdu_uint32 height = (kdu_uint32) size.y;
  kdu_uint32 width = (kdu_uint32) size.x;
  kdu_byte loop = (kdu_byte)(loop_count-1);
  sub.write(height);
  sub.write(width);
  sub.write(loop);
  sub.close();

  jx_frame *fp, *fnext;
  jx_instruction *ip;
  int iset_idx=0;
  for (fp=head; fp != NULL; fp=fnext, iset_idx++)
    {
      sub.open(&comp_out,jp2_comp_instruction_set_4cc);
      kdu_uint16 flags = 39; // Always record all the fields
      kdu_uint32 tick = 1; // All other values are utterly unnecessary!
      kdu_uint16 rept = 0;

      // Calculate number of repetitions by scanning future frames for
      // similarities
      for (fnext=fp->next; fnext != NULL; fnext=fnext->next, rept++)
        {
          if ((fnext->duration != fp->duration) ||
              (fnext->persistent != fp->persistent) ||
              (fnext->num_instructions != fp->num_instructions))
            break;
          jx_instruction *ip2;
          for (ip=fp->head, ip2=fnext->head;
               (ip != NULL) && (ip2 != NULL);
               ip=ip->next, ip2=ip2->next)
            if ((ip->next_reuse != ip2->next_reuse) ||
                (ip->visible != ip2->visible) ||
                (ip->source_dims != ip2->source_dims) ||
                (ip->target_dims != ip2->target_dims) ||
                !ip2->first_use)
              break;
          if ((ip != NULL) || (ip2 != NULL))
            break; // Frame is not compatible with previous one
        }
      sub.write(flags);
      sub.write(rept);
      sub.write(tick);
      int inum_idx = 0;
      for (ip=fp->head; ip != NULL; ip=ip->next)
        { // Record instruction
          kdu_uint32 life = 0;
          ip->iset_idx = iset_idx;
          ip->inum_idx = inum_idx++;
          if (ip != fp->tail)
            { // Non-terminal instructions in frame have duration=0
              if (ip->visible)
                life |= 0x80000000; // Add `persists' flag
            }
          else
            { // Terminate frame
              if (fp->persistent)
                {
                  life |= 0x80000000;
                  assert(ip->visible); // Can't have invisibles at end of
                                       // persistent frame
                }
              life |= ((kdu_uint32) fp->duration) & 0x7FFFFFFF;
            }
          sub.write((kdu_uint32) ip->target_dims.pos.x);
          sub.write((kdu_uint32) ip->target_dims.pos.y);
          sub.write((kdu_uint32) ip->target_dims.size.x);
          sub.write((kdu_uint32) ip->target_dims.size.y);
          sub.write(life);
          sub.write((kdu_uint32) ip->next_reuse);
          sub.write((kdu_uint32) ip->source_dims.pos.x);
          sub.write((kdu_uint32) ip->source_dims.pos.y);
          sub.write((kdu_uint32) ip->source_dims.size.x);
          sub.write((kdu_uint32) ip->source_dims.size.y);
        }
      sub.close();
    }

  comp_out.close();
}


/* ========================================================================= */
/*                             jpx_composition                               */
/* ========================================================================= */

/*****************************************************************************/
/*                      jpx_composition::get_global_info                     */
/*****************************************************************************/

int
  jpx_composition::get_global_info(kdu_coords &size)
{
  if (state == NULL)
    return -1;
  size = state->size;
  return state->loop_count;
}

/*****************************************************************************/
/*                      jpx_composition::get_next_frame                      */
/*****************************************************************************/

jx_frame *
  jpx_composition::get_next_frame(jx_frame *last_frame)
{
  if (state == NULL)
    return NULL;
  if (last_frame == NULL)
    return state->head;
  else
    return last_frame->next;
}

/*****************************************************************************/
/*                      jpx_composition::get_prev_frame                      */
/*****************************************************************************/

jx_frame *
  jpx_composition::get_prev_frame(jx_frame *last_frame)
{
  if (state == NULL)
    return NULL;
  if (last_frame == NULL)
    return NULL;
  return last_frame->prev;
}

/*****************************************************************************/
/*                      jpx_composition::get_frame_info                      */
/*****************************************************************************/

void
  jpx_composition::get_frame_info(jx_frame *frame_ref, int &num_instructions,
                                  int &duration, int &repeat_count,
                                  bool &is_persistent)
{
  assert(state != NULL);
  num_instructions = frame_ref->num_instructions;
  duration = frame_ref->duration;
  repeat_count = frame_ref->repeat_count;
  is_persistent = frame_ref->persistent;
}

/*****************************************************************************/
/*                 jpx_composition::get_last_persistent_frame                */
/*****************************************************************************/

jx_frame *
  jpx_composition::get_last_persistent_frame(jx_frame *frame_ref)
{
  if (state == NULL)
    return NULL;
  return frame_ref->last_persistent_frame;
}

/*****************************************************************************/
/*                      jpx_composition::get_instruction                     */
/*****************************************************************************/

bool
  jpx_composition::get_instruction(jx_frame *frame, int instruction_idx,
                                   int &layer_idx, int &increment,
                                   bool &is_reused, kdu_dims &source_dims,
                                   kdu_dims &target_dims)
{
  if (state == NULL)
    return false;
  if (instruction_idx >= frame->num_instructions)
    return false;
  jx_instruction *ip;
  for (ip=frame->head; instruction_idx > 0; instruction_idx--, ip=ip->next)
    assert(ip != NULL);
  layer_idx = ip->layer_idx;
  increment = ip->increment;
  is_reused = (ip->next_reuse != 0);
  source_dims = ip->source_dims;
  target_dims = ip->target_dims;
  return true;
}

/*****************************************************************************/
/*                     jpx_composition::get_original_iset                    */
/*****************************************************************************/

bool
  jpx_composition::get_original_iset(jx_frame *frame, int instruction_idx,
                                     int &iset_idx, int &inum_idx)
{
  if (state == NULL)
    return false;
  if (instruction_idx >= frame->num_instructions)
    return false;
  jx_instruction *ip;
  for (ip=frame->head; instruction_idx > 0; instruction_idx--, ip=ip->next)
    assert(ip != NULL);
  iset_idx = ip->iset_idx;
  inum_idx = ip->inum_idx;
  return true;
}

/*****************************************************************************/
/*                         jpx_composition::add_frame                        */
/*****************************************************************************/

jx_frame *
  jpx_composition::add_frame(int duration, int repeat_count,
                             bool is_persistent)
{
  if (state == NULL)
    return NULL;
  if ((state->tail != NULL) && (state->tail->duration == 0))
    { KDU_ERROR_DEV(e,21); e <<
        KDU_TXT("Attempting to add multiple animation frames which "
        "have a temporal duration of 0 milliseconds, using the "
        "`jpx_composition::add_frame' function.  This is clearly silly.");
    }
  state->add_frame();
  state->tail->duration = duration;
  state->tail->repeat_count = repeat_count;
  state->tail->persistent = is_persistent;
  return state->tail;
}

/*****************************************************************************/
/*                     jpx_composition::add_instruction                      */
/*****************************************************************************/

int
  jpx_composition::add_instruction(jx_frame *frame, int layer_idx,
                                   int increment, kdu_dims source_dims,
                                   kdu_dims target_dims)
{
  if (state == NULL)
    return -1;
  jx_instruction *inst = frame->add_instruction(true);
  inst->layer_idx = layer_idx;
  inst->increment = increment;
  inst->source_dims = source_dims;
  inst->target_dims = target_dims;
  return frame->num_instructions-1;
}

/*****************************************************************************/
/*                     jpx_composition::set_loop_count                       */
/*****************************************************************************/

void
  jpx_composition::set_loop_count(int count)
{
  if ((count < 0) || (count > 255))
    { KDU_ERROR_DEV(e,22); e <<
        KDU_TXT("Illegal loop count supplied to "
        "`jpx_composition::set_loop_count'.  Legal values must be in the "
        "range 0 (indefinite looping) to 255 (max explicit repetitions).");
    }
  state->loop_count = count;
}

/*****************************************************************************/
/*                           jpx_composition::copy                           */
/*****************************************************************************/

void
  jpx_composition::copy(jpx_composition src)
{
  assert((state != NULL) && (src.state != NULL));
  jx_frame *sp, *dp;
  jx_instruction *spi, *dpi;

  for (sp=src.state->head; sp != NULL; sp=sp->next)
    {
      if (sp->head == NULL)
        continue;
      state->add_frame();
      dp = state->tail;
      dp->duration = sp->duration;
      dp->repeat_count = sp->repeat_count;
      dp->persistent = sp->persistent;
      for (spi=sp->head; spi != NULL; spi=spi->next)
        {
          dpi = dp->add_instruction(true);
          dpi->layer_idx = spi->layer_idx;
          dpi->increment = spi->increment;
          dpi->source_dims = spi->source_dims;
          dpi->target_dims = spi->target_dims;
        }
    }
}


/* ========================================================================= */
/*                            jpx_frame_expander                             */
/* ========================================================================= */

/*****************************************************************************/
/*             jpx_frame_expander::test_codestream_visibility                */
/*****************************************************************************/

int
  jpx_frame_expander::test_codestream_visibility(jpx_source *source,
                                                 jx_frame *the_frame,
                                                 int the_iteration_idx,
                                                 bool follow_persistence,
                                                 int codestream_idx,
                                                 kdu_dims &composition_region,
                                                 kdu_dims codestream_roi,
                                                 bool ignore_use_in_alpha,
                                                 int initial_matches_to_skip)
{
  jpx_composition composition = source->access_composition();
  if (!composition)
    return -1;
  kdu_dims comp_dims; composition.get_global_info(comp_dims.size);
  if (composition_region.is_empty())
    composition_region = comp_dims;
  jpx_codestream_source stream =
  source->access_codestream(codestream_idx);
  if (stream.exists())
    {
      jp2_dimensions cs_dims = stream.access_dimensions();
      if (cs_dims.exists())
        {
          kdu_dims codestream_dims;
          codestream_dims.size = cs_dims.get_size();
          if (!codestream_roi.is_empty())
            codestream_roi &= codestream_dims;
          else
            codestream_roi = codestream_dims;
          if (codestream_roi.is_empty())
            return -1;
        }
    }
  
  int max_compositing_layers;
  bool max_layers_known =
    source->count_compositing_layers(max_compositing_layers);

  // In the following, we repeatedly cycle through the frame members (layers)
  // starting from the top-most layer.  We keep track of the frame member
  // in which the codestream was found in the last iteration of the loop,
  // if any, along with the region which it occupies on the composition
  // surface.  This is done via the following two variables.  While examining
  // members which come before (above in the composition sequence) this
  // last member, we determine whether or not they hide it and how much
  // of it they hide.  We examine members beyond the last member in order
  // to find new uses of the codestream, as required by the
  // `initial_matches_to_skip' argument.
  int last_member_idx=-1; // We haven't found any match yet
  int last_layer_idx=-1; // Compositing layer associated with last member
  kdu_dims last_composition_region;
  jx_frame *frame = the_frame; // Used to scan through the iterations
  int iteration_idx = the_iteration_idx;
  int member_idx = 0;
  while (frame != NULL)
    {
      int n, num_instructions, duration, repeat_count;
      bool is_persistent;
      composition.get_frame_info(frame,num_instructions,duration,
                                 repeat_count,is_persistent);
      if (iteration_idx < 0)
        iteration_idx = repeat_count;
      else if (iteration_idx > repeat_count)
        return -1; // Cannot open requested frame
          
      int layer_idx = -1;
      jpx_layer_source layer;
      kdu_dims source_dims, target_dims;
      for (n=num_instructions-1; n >= 0; n--, member_idx++)
        {
          int increment;
          bool is_reused;
          composition.get_instruction(frame,n,layer_idx,increment,
                                      is_reused,source_dims,target_dims);
          layer_idx += increment*iteration_idx;
          if ((layer_idx < 0) ||
              (max_layers_known &&
               (layer_idx >= max_compositing_layers)))
            continue; // Treat this layer as if it does not exist for now
          if (!(layer = source->access_layer(layer_idx,false)).exists())
            continue; // Treat layer as if it does not exist for now
          if (source_dims.is_empty())
            {
              source_dims.pos = kdu_coords();
              source_dims.size = layer.get_layer_size();
            }
          if (target_dims.is_empty())
            target_dims.size = source_dims.size;
          jp2_channels channels = layer.access_channels();
          if (!channels)
            continue; // Treat layer as if it does not exist for now
          int c, num_colours = channels.get_num_colours();
          int comp_idx, lut_idx, str_idx, key_val;
          if ((member_idx < last_member_idx) &&
              !last_composition_region.is_empty())
            { // See if this layer covers `last_composition_region'
              for (c=0; c < num_colours; c++)
                if (channels.get_opacity_mapping(c,comp_idx,lut_idx,
                                                 str_idx) ||
                    channels.get_premult_mapping(c,comp_idx,lut_idx,
                                                 str_idx) ||
                    channels.get_chroma_key(0,key_val))
                  break; // Layer not completely opaque
              if (c == num_colours)
                { // Layer is opaque; let's test for intersection
                  kdu_dims isect = last_composition_region & target_dims;
                  if (isect.size.x == last_composition_region.size.x)
                    {
                      if (isect.pos.y == last_composition_region.size.y)
                        {
                          last_composition_region.pos.y += isect.size.y;
                          last_composition_region.size.y -= isect.size.y;
                        }
                      else if ((isect.pos.y+isect.size.y) ==
                               (last_composition_region.pos.y+
                                last_composition_region.size.y))
                        last_composition_region.size.y -= isect.size.y; 
                    }
                  else if (isect.size.y == last_composition_region.size.y)
                    {
                      if (isect.pos.x == last_composition_region.size.x)
                        {
                          last_composition_region.pos.x += isect.size.x;
                          last_composition_region.size.x -= isect.size.x;
                        }
                      else if ((isect.pos.x+isect.size.x) ==
                               (last_composition_region.pos.x+
                                last_composition_region.size.x))
                        last_composition_region.size.x -= isect.size.x; 
                    }
                }
            }
          else if (member_idx == last_member_idx)
            { // Time to assess whether or not the last member is visible
              if (!last_composition_region.is_empty())
                {
                  if (initial_matches_to_skip == 0)
                    {
                      composition_region = last_composition_region;
                      return last_layer_idx;
                    }
                  else
                    initial_matches_to_skip--;
                }
              last_member_idx = -1;
              last_layer_idx = -1;
              last_composition_region = kdu_dims();
            }
          else if (last_member_idx < 0)
            { // Looking for a new member which contains the codestream
              for (c=0; c < num_colours; c++)
                {
                  if (channels.get_colour_mapping(c,comp_idx,
                                                  lut_idx,str_idx) &&
                      (str_idx == codestream_idx))
                    break;
                  if (ignore_use_in_alpha)
                    continue;
                  if (channels.get_opacity_mapping(c,comp_idx,
                                                   lut_idx,str_idx) &&
                      (str_idx == codestream_idx))
                    break;
                  if (channels.get_premult_mapping(c,comp_idx,
                                                   lut_idx,str_idx) &&
                      (str_idx == codestream_idx))
                    break;
                }
              if (c < num_colours)
                break; // Found matching codestream
            }
        }
              
      if (n >= 0)
        { // Found a new member; set up the `last_composition_region'
          // object and then start another iteration thru the members
          kdu_coords align, sampling, denominator;
          int c, str_idx;
          for (c=0; (str_idx =
                     layer.get_codestream_registration(c,align,sampling,
                                                       denominator)) >= 0;
               c++)
            if (str_idx == codestream_idx)
              break;
          if (str_idx < 0)
            { // Failed to identify the codestream; makes no sense really
              assert(0);
              return -1;
            }
          // Now convert `codestream_roi' into a region on the compositing
          // surface, storing this region in `last_composition_region'
          if (!codestream_roi)
            last_composition_region = target_dims;
          else
            {
              kdu_coords min = codestream_roi.pos;
              kdu_coords lim = min + codestream_roi.size;
              if ((denominator.x > 0) && (denominator.y > 0))
                {
                  min.x = long_floor_ratio(((kdu_long) min.x)*sampling.x,
                                           denominator.x);
                  min.y = long_floor_ratio(((kdu_long) min.y)*sampling.y,
                                           denominator.y);
                  lim.x = long_ceil_ratio(((kdu_long) lim.x)*sampling.x,
                                          denominator.x);
                  lim.y = long_ceil_ratio(((kdu_long) lim.y)*sampling.y,
                                          denominator.y);
                }
              last_composition_region.pos = min;
              last_composition_region.size = lim - min;
              last_composition_region &= source_dims;
              last_composition_region.pos -= source_dims.pos;
              if (target_dims.size != source_dims.size)
                { // Need to further scale the region
                  min = last_composition_region.pos;
                  lim = min + last_composition_region.size;
                  min.x =
                    long_floor_ratio(((kdu_long) min.x)*target_dims.size.x,
                                     source_dims.size.x);
                  min.y =
                    long_floor_ratio(((kdu_long) min.y)*target_dims.size.y,
                                     source_dims.size.y);
                  lim.x =
                    long_ceil_ratio(((kdu_long) lim.x)*target_dims.size.x,
                                    source_dims.size.x);
                  lim.y =
                    long_ceil_ratio(((kdu_long) lim.y)*target_dims.size.x,
                                    source_dims.size.x);
                  composition_region.pos = min;
                  composition_region.size = lim - min;
                }
              last_composition_region.pos += target_dims.pos;
              last_composition_region &= target_dims; // Just to be sure
            }
          last_member_idx = member_idx;
          last_layer_idx = layer_idx;
          member_idx = 0;
          frame = the_frame;
          iteration_idx = the_iteration_idx;
          continue;
        }
                    
      // See if there are more frames which must be composed with this one
      iteration_idx--;
      if ((frame != NULL) && ((iteration_idx < 0) || !is_persistent))
        {
          frame = composition.get_last_persistent_frame(frame);
          iteration_idx = -1; // Find last iteration at start of loop
        }
    }
  return -1; // Didn't find a match
}

/*****************************************************************************/
/*                      jpx_frame_expander::construct                        */
/*****************************************************************************/

bool
  jpx_frame_expander::construct(jpx_source *source, jx_frame *frame,
                                int iteration_idx, bool follow_persistence,
                                kdu_dims region_of_interest)
{
  non_covering_members = false;
  num_members = 0;
  jpx_composition composition = source->access_composition();
  if (!composition)
    return false;
  kdu_dims comp_dims; composition.get_global_info(comp_dims.size);
  if (region_of_interest.is_empty())
    region_of_interest = comp_dims;

  kdu_dims opaque_dims;
  kdu_dims opaque_roi; // Amount of region of interest which is opaque
  int max_compositing_layers;
  bool max_layers_known =
    source->count_compositing_layers(max_compositing_layers);
  while (frame != NULL)
    {
      int n, num_instructions, duration, repeat_count;
      bool is_persistent;
      composition.get_frame_info(frame,num_instructions,duration,
                                 repeat_count,is_persistent);
      if (iteration_idx < 0)
        iteration_idx = repeat_count;
      else if (iteration_idx > repeat_count)
        { // Cannot open requested frame
          num_members = 0;
          return true;
        }
      for (n=num_instructions-1; n >= 0; n--)
        {
          kdu_dims source_dims, target_dims;
          int layer_idx, increment;
          bool is_reused;
          composition.get_instruction(frame,n,layer_idx,increment,
                                      is_reused,source_dims,target_dims);

          // Calculate layer index and open the `jpx_layer_source' if possible
          layer_idx += increment*iteration_idx;
          if ((layer_idx < 0) ||
              (max_layers_known && (layer_idx >= max_compositing_layers)))
            { // Cannot open frame
              num_members = 0;
              return true;
            }

          jpx_layer_source layer = source->access_layer(layer_idx,false);
          if (source_dims.is_empty() && layer.exists())
            source_dims.size = layer.get_layer_size();
          if (target_dims.is_empty())
            target_dims.size = source_dims.size;
          kdu_dims test_dims = target_dims & comp_dims;
          kdu_dims test_roi = target_dims & region_of_interest;
          if ((!target_dims.is_empty()) &&
              ((opaque_dims & test_dims) == test_dims))
              continue; // Globally invisible

          // Add a new member
          if (max_members == num_members)
            {
              max_members += num_members+8;
              jx_frame_member *tmp = new jx_frame_member[max_members];
              for (int m=0; m < num_members; m++)
                tmp[m] = members[m];
              if (members != NULL)
                delete[] members;
              members = tmp;
            }
          jx_frame_member *mem = members + (num_members++);
          mem->frame = frame;
          mem->instruction_idx = n;
          mem->layer_idx = layer_idx;
          mem->source_dims = source_dims;
          mem->target_dims = target_dims;
          mem->covers_composition = (target_dims == comp_dims);
          mem->layer_is_accessible = layer.exists();
          mem->may_be_visible_under_roi =
            target_dims.is_empty() ||
            ((!test_roi.is_empty()) && ((test_roi & opaque_roi) != test_roi));
          mem->is_opaque = false; // Until proven otherwise
          if (layer.exists())
            { // Determine opacity of layer
              jp2_channels channels = layer.access_channels();
              int comp_idx, lut_idx, str_idx;
              kdu_int32 key_val;
              mem->is_opaque =
                !(channels.get_opacity_mapping(0,comp_idx,lut_idx,str_idx) ||
                  channels.get_premult_mapping(0,comp_idx,lut_idx,str_idx) ||
                  channels.get_chroma_key(0,key_val));
            }

          // Update the opaque regions and see if whole composition is covered
          if (mem->is_opaque)
            {
              if (test_dims.area() > opaque_dims.area())
                opaque_dims = test_dims;
              if (test_roi.area() > opaque_roi.area())
                opaque_roi = test_roi;
              if (opaque_dims == comp_dims)
                { // Can stop looking for more instructions
                  frame = NULL;
                  break;
                }
            }
        }
      
      // See if there are any more frames which must be composed with this one
      iteration_idx--;
      if ((frame != NULL) && ((iteration_idx < 0) || !is_persistent))
        {
          frame = composition.get_last_persistent_frame(frame);
          iteration_idx = -1; // Find last iteration at start of loop
        }
    }

  // Now go back over things to see if some extra members can be removed
  int n, m;
  bool can_access_all_members = true;
  for (n=0; n < num_members; n++)
    {
      if (!members[n].may_be_visible_under_roi)
        {
          if (!members[n].layer_is_accessible)
            { // May still need to include this member, unless included already
              int layer_idx = members[n].layer_idx;
              for (m=0; m < num_members; m++)
                if ((members[m].layer_idx == layer_idx) &&
                    ((m < n) || members[m].may_be_visible_under_roi))
                  { // We can eliminate the member instruction
                    members[n].layer_is_accessible = true; // Forces removal
                    break;
                  }
            }
          if (members[n].layer_is_accessible)
            {
              num_members--;
              for (m=n; m < num_members; m++)
                members[m] = members[m+1];
              n--; // So that we consider the next member correctly
              continue;
            }
        }
      if (!members[n].layer_is_accessible)
        can_access_all_members = false;
      else if (!members[n].covers_composition)
        non_covering_members = true;
    }

  return can_access_all_members;
}


/* ========================================================================= */
/*                                jx_numlist                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                          jx_numlist::~jx_numlist                          */
/*****************************************************************************/

jx_numlist::~jx_numlist()
{
  if ((codestream_indices != NULL) &&
      (codestream_indices != &single_codestream_idx))
    delete[] codestream_indices;
  if ((layer_indices != NULL) && (layer_indices != &single_layer_idx))
    delete[] layer_indices;
}

/*****************************************************************************/
/*                        jx_numlist::add_codestream                         */
/*****************************************************************************/

void
  jx_numlist::add_codestream(int idx)
{
  int n;
  for (n=0; n < num_codestreams; n++)
    if (codestream_indices[n] == idx)
      return;
  if (num_codestreams == 0)
    {
      assert(codestream_indices == NULL);
      max_codestreams = 1;
      codestream_indices = &single_codestream_idx;
    }
  else if (num_codestreams >= max_codestreams)
    { // Allocate appropriate array
      max_codestreams += 8;
      int *tmp = new int[max_codestreams];
      for (n=0; n < num_codestreams; n++)
        tmp[n] = codestream_indices[n];
      if (codestream_indices != &single_codestream_idx)
        delete[] codestream_indices;
      codestream_indices = tmp;
    }
  num_codestreams++;
  codestream_indices[n] = idx;
}

/*****************************************************************************/
/*                    jx_numlist::add_compositing_layer                      */
/*****************************************************************************/

void
  jx_numlist::add_compositing_layer(int idx)
{
  int n;
  for (n=0; n < num_compositing_layers; n++)
    if (layer_indices[n] == idx)
      return;
  if (num_compositing_layers == 0)
    {
      assert(layer_indices == NULL);
      max_compositing_layers = 1;
      layer_indices = &single_layer_idx;
    }
  else if (num_compositing_layers >= max_compositing_layers)
    { // Allocate appropriate array
      max_compositing_layers += 8;
      int *tmp = new int[max_compositing_layers];
      for (n=0; n < num_compositing_layers; n++)
        tmp[n] = layer_indices[n];
      if (layer_indices != &single_layer_idx)
        delete[] layer_indices;
      layer_indices = tmp;
    }
  num_compositing_layers++;
  layer_indices[n] = idx;
}

/*****************************************************************************/
/*                            jx_numlist::write                              */
/*****************************************************************************/

void
  jx_numlist::write(jp2_output_box &box)
{
  int n;
  for (n=0; n < num_codestreams; n++)
    box.write((kdu_uint32)(codestream_indices[n] | 0x01000000));
  for (n=0; n < num_compositing_layers; n++)
    box.write((kdu_uint32)(layer_indices[n] | 0x02000000));
  if (rendered_result)
    box.write((kdu_uint32) 0x00000000);
}


/* ========================================================================= */
/*                                jx_regions                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                       jx_regions::set_num_regions                         */
/*****************************************************************************/

void
  jx_regions::set_num_regions(int num)
{
  if (num < 0)
    num = 0;
  if (num <= max_regions)
    num_regions = num;
  else if (num == 1)
    {
      assert((regions == NULL) && (max_regions == 0));
      max_regions = num_regions = 1;
      regions = &bounding_region;
    }
  else
    {
      max_regions += num;
      jpx_roi *tmp = new jpx_roi[max_regions];
      for (int n=0; n < num_regions; n++)
        tmp[n] = regions[n];
      if ((regions != NULL) && (regions != &bounding_region))
        delete[] regions;
      regions = tmp;
      num_regions = num;
    }
}

/*****************************************************************************/
/*                            jx_regions::write                              */
/*****************************************************************************/

void
  jx_regions::write(jp2_output_box &box)
{
  kdu_byte num = (kdu_byte)(num_regions>255)?255:num_regions;
  box.write(num);
  jpx_roi *rp;
  for (rp=regions; num > 0; num--, rp++)
    {
      box.write((kdu_byte)((rp->is_encoded)?1:0));
      box.write((kdu_byte)((rp->is_elliptical)?1:0));
      box.write((kdu_byte) rp->coding_priority);
      kdu_dims rect = rp->region;
      if (rp->is_elliptical)
        { // Set `pos' to the centre of the ellipse
          rect.pos.x += (rect.size.x >> 1);
          rect.pos.y += (rect.size.y >> 1);
        }
      box.write(rect.pos.x);
      box.write(rect.pos.y);
      box.write(rect.size.x);
      box.write(rect.size.y);
    }
}


/* ========================================================================= */
/*                               jx_metanode                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                        jx_metanode::~jx_metanode                          */
/*****************************************************************************/

jx_metanode::~jx_metanode()
{
  assert((flags & JX_METANODE_DELETED) &&
         (head == NULL) && (tail == NULL) && (parent == NULL) &&
         (metagroup == NULL) && (next_link == NULL) && (prev_link == NULL) &&
         (prev_sibling == NULL)); // We only use `next_sibling' to link the
                                  // `deleted_nodes' list.
  switch (rep_id) {
    case JX_REF_NODE: if (ref != NULL) delete ref; break;
    case JX_NUMLIST_NODE: if (numlist != NULL) delete numlist; break;
    case JX_ROI_NODE: if (regions != NULL) delete regions; break;
    case JX_LABEL_NODE: if (label != NULL) delete label; break;
    default: break;
    };

  if ((flags & JX_METANODE_EXISTING) && (read_state != NULL))
    delete read_state;
  else if (write_state != NULL)
    delete write_state;
}

/*****************************************************************************/
/*                       jx_metanode::safe_delete                           */
/*****************************************************************************/

void
  jx_metanode::safe_delete()
{
  if (flags & JX_METANODE_DELETED)
    return; // Already deleted
  unlink_parent();
  if (metagroup != NULL)
    metagroup->unlink(this);
  assert((next_link == NULL) && (prev_link == NULL) && (metagroup == NULL) &&
         (parent == NULL) && (next_sibling == NULL) && (prev_sibling == NULL));

  append_to_touched_list(false);
  
  // Now recursively apply to the descendants
  while (head != NULL)
    head->safe_delete();
  assert((tail == NULL) && (num_descendants == 0));
  next_sibling = manager->deleted_nodes;
  manager->deleted_nodes = this;
  flags |= JX_METANODE_DELETED;  
}

/*****************************************************************************/
/*                       jx_metanode::unlink_parent                          */
/*****************************************************************************/

void
  jx_metanode::unlink_parent()
{
  if (parent != NULL)
    { // Unlink from sibling list
      if (prev_sibling == NULL)
        {
          assert(this == parent->head);
          parent->head = next_sibling;
        }
      else
        prev_sibling->next_sibling = next_sibling;
      if (next_sibling == NULL)
        {
          assert(this == parent->tail);
          parent->tail = prev_sibling;
        }
      else
        next_sibling->prev_sibling = prev_sibling;
      parent->num_descendants--;
      if (flags & JX_METANODE_IS_COMPLETE)
        parent->num_completed_descendants--;
      parent = next_sibling = prev_sibling = NULL;
    }
  else
    assert((next_sibling == NULL) && (prev_sibling == NULL));  
}

/*****************************************************************************/
/*                  jx_metanode::append_to_touched_list                      */
/*****************************************************************************/

void
  jx_metanode::append_to_touched_list(bool recursive)
{
  if ((box_type != 0) && (flags & JX_METANODE_BOX_COMPLETE))
    {
      // Unlink from touched list if required
      if (manager->touched_head == this)
        {
          assert(prev_touched == NULL);
          manager->touched_head = next_touched;
        }
      else if (prev_touched != NULL)
        prev_touched->next_touched = next_touched;
      if (manager->touched_tail == this)
        {
          assert(next_touched == NULL);
          manager->touched_tail = prev_touched;
        }
      else if (next_touched != NULL)
        next_touched->prev_touched = prev_touched;
  
      // Now link onto tail of touched list
      next_touched = NULL;
      if ((prev_touched = manager->touched_tail) == NULL)
        {
          assert(manager->touched_head == NULL);
          manager->touched_head = manager->touched_tail = this;
        }
      else
        manager->touched_tail = manager->touched_tail->next_touched = this;
    }

  if ((parent != NULL) && (parent->flags & (JX_METANODE_ANCESTOR_CHANGED |
                                            JX_METANODE_CONTENTS_CHANGED)))
    flags |= JX_METANODE_ANCESTOR_CHANGED;
  
  jx_metanode *scan;
  for (scan=head; (scan != NULL) && recursive; scan=scan->next_sibling)
    scan->append_to_touched_list(true);
}

/*****************************************************************************/
/*                       jx_metanode::append_child                           */
/*****************************************************************************/

void
  jx_metanode::append_child(jx_metanode *child)
{
  child->prev_sibling = tail;
  if (tail == NULL)
    head = tail = child;
  else
    tail = tail->next_sibling = child;
  child->parent = this;
  num_descendants++;
  if (child->flags & JX_METANODE_IS_COMPLETE)
    num_completed_descendants++;
}

/*****************************************************************************/
/*                       jx_metanode::add_descendant                         */
/*****************************************************************************/

jx_metanode *
  jx_metanode::add_descendant()
{
  jx_metanode *result = new jx_metanode(manager);
  append_child(result);
  return result;
}

/*****************************************************************************/
/*                         jx_metanode::add_numlist                          */
/*****************************************************************************/

jx_metanode *
  jx_metanode::add_numlist(int num_codestreams, const int *codestream_indices,
                           int num_layers, const int *layer_indices,
                           bool applies_to_rendered_result)
{
  jx_metanode *node = add_descendant();  
  node->box_type = jp2_number_list_4cc;
  node->flags |= JX_METANODE_BOX_COMPLETE;
  node->rep_id = JX_NUMLIST_NODE;
  node->numlist = new jx_numlist;
  int n;
  if (num_codestreams > 0)
    {
      node->numlist->num_codestreams =
        node->numlist->max_codestreams = num_codestreams;
      node->numlist->codestream_indices = new int[num_codestreams];
      for (n=0; n < num_codestreams; n++)
        node->numlist->codestream_indices[n] = codestream_indices[n];
    }
  if (num_layers > 0)
    {
      node->numlist->num_compositing_layers =
        node->numlist->max_compositing_layers = num_layers;
      node->numlist->layer_indices = new int[num_layers];
      for (n=0; n < num_layers; n++)
        node->numlist->layer_indices[n] = layer_indices[n];
    }
  node->numlist->rendered_result = applies_to_rendered_result;
  node->manager->link(node);
  node->append_to_touched_list(false);
  return node;
}

/*****************************************************************************/
/*                jx_metanode::update_completed_descendants                  */
/*****************************************************************************/

void
  jx_metanode::update_completed_descendants()
{
  assert((flags & JX_METANODE_DESCENDANTS_KNOWN) &&
         (flags & JX_METANODE_BOX_COMPLETE));
  jx_metanode *scan = this;
  while (((scan->flags & JX_METANODE_IS_COMPLETE) == 0) &&
         (scan->flags & JX_METANODE_DESCENDANTS_KNOWN) &&
         (scan->num_completed_descendants == scan->num_descendants))
    {
      scan->flags |= JX_METANODE_IS_COMPLETE;
      if (scan->parent == NULL)
        break;
      assert(scan->parent->num_completed_descendants <
             scan->parent->num_descendants);
      scan->parent->num_completed_descendants++;
      scan = scan->parent;
    }
}

/*****************************************************************************/
/*                      jx_metanode::donate_input_box                        */
/*****************************************************************************/

void
  jx_metanode::donate_input_box(jp2_input_box &src)
{
  assert((read_state == NULL) &&
         !(flags & (JX_METANODE_BOX_COMPLETE|JX_METANODE_DESCENDANTS_KNOWN)));
  flags |= JX_METANODE_EXISTING;
  
  // Add a locator for the new box.
  kdu_long file_pos = src.get_locator().get_file_pos();
  file_pos += src.get_box_header_length();
  jx_metaloc *locator = manager->metaloc_manager.get_locator(file_pos,true);
  locator->target = this;
  
  read_state = new jx_metaread();
  box_type = src.get_box_type();
  if (box_type == jp2_association_4cc)
    read_state->asoc.transplant(src);      
  else
    {
      read_state->box.transplant(src);
      flags |= JX_METANODE_DESCENDANTS_KNOWN;
    }
  finish_reading();
}

/*****************************************************************************/
/*                       jx_metanode::finish_reading                         */
/*****************************************************************************/

bool
  jx_metanode::finish_reading()
{
  if (!(flags & JX_METANODE_EXISTING))
    return true;
  if (read_state == NULL)
    {
      assert((flags & JX_METANODE_BOX_COMPLETE) &&
             (flags & JX_METANODE_DESCENDANTS_KNOWN));
      return true;
    }

  if (!(flags & JX_METANODE_BOX_COMPLETE))
    {
      if (!read_state->box)
        { // We still have to open the first sub-box
          assert(read_state->asoc.exists());
          read_state->box.open(&read_state->asoc);
          if (!read_state->box)
            return false;
          box_type = read_state->box.get_box_type();
          
          // Add a locator for the newly opened box
          kdu_long file_pos = read_state->box.get_locator().get_file_pos();
          file_pos += read_state->box.get_box_header_length();
          jx_metaloc *locator = manager->metaloc_manager.get_locator(file_pos,
                                                                     true);
          locator->target = this;
        }
      if (!read_state->box.is_complete())
        return false;
      flags |= JX_METANODE_BOX_COMPLETE;
      append_to_touched_list(false);
      int len = (int) read_state->box.get_remaining_bytes();
      if (box_type == jp2_number_list_4cc)
        {
          rep_id = JX_NUMLIST_NODE;
          numlist = new jx_numlist;
          if ((len == 0) || ((len & 3) != 0))
            { KDU_WARNING(w,0x26100801); w <<
                KDU_TXT("Malformed Number List (`nlst') box "
                "found in JPX data source.  Box body does not contain a "
                "whole (non-zero) number of 32-bit integers.");
              safe_delete();
              return true;
            }
          len >>= 2;
          for (; len > 0; len--)
            {
              kdu_uint32 idx; read_state->box.read(idx);
              if (idx == 0)
                numlist->rendered_result = true;
              else if (idx & 0x01000000)
                numlist->add_codestream((int)(idx & 0x00FFFFFF));
              else if (idx & 0x02000000)
                numlist->add_compositing_layer((int)(idx & 0x00FFFFFF));
            }
        }
      else if ((box_type == jp2_roi_description_4cc) && (len > 1))
        {
          rep_id = JX_ROI_NODE;
          regions = new jx_regions;
          kdu_byte num=0; read_state->box.read(num);
          regions->set_num_regions((int) num);
          int n;
          kdu_dims rect;
          kdu_coords bound_min, bound_lim, min, lim;
          for (n=0; n < (int) num; n++)
            {
              kdu_byte Rstatic, Rtyp, Rcp;
              kdu_uint32 Rlcx, Rlcy, Rwidth, Rheight;
              if (!(read_state->box.read(Rstatic) &&
                    read_state->box.read(Rtyp) &&
                    read_state->box.read(Rcp) &&
                    read_state->box.read(Rlcx) &&
                    read_state->box.read(Rlcy) &&
                    read_state->box.read(Rwidth) &&
                    read_state->box.read(Rheight) &&
                    (Rstatic < 2) && (Rtyp < 2)))
                { KDU_WARNING(w,0x26100802); w <<
                    KDU_TXT("Malformed ROI Descriptions (`roid') "
                    "box encountered in JPX data source.");
                  safe_delete();
                  return true;
                }
              regions->regions[n].is_elliptical = (Rtyp == 1);
              if (regions->regions[n].is_elliptical)
                { // Need odd dimensions, since centre encoded using integers
                  Rwidth |= 1; Rheight |= 1;
                }
              regions->regions[n].is_encoded = (Rstatic == 1);
              regions->regions[n].coding_priority = Rcp;
              rect.pos.x = (int) Rlcx;
              rect.pos.y = (int) Rlcy;
              rect.size.x = (int) Rwidth;
              rect.size.y = (int) Rheight;
              if (regions->regions[n].is_elliptical)
                {
                  rect.pos.x -= (int)(Rwidth>>1);
                  rect.pos.y -= (int)(Rheight>>1);
                }
              regions->regions[n].region = rect;
              int sz = (rect.size.x < rect.size.y)?rect.size.x:rect.size.y;
              if ((n == 0) || (sz > regions->min_size))
                regions->min_size = sz;
              min = rect.pos;
              lim = min + rect.size;
              if (n == 0)
                { bound_min = min; bound_lim = lim; }
              else
                { // Grow `bound' to include `rect'
                  if (min.x < bound_min.x)
                    bound_min.x = min.x;
                  if (min.y < bound_min.y)
                    bound_min.y = min.y;
                  if (lim.x > bound_lim.x)
                    bound_lim.x = lim.x;
                  if (lim.y > bound_lim.y)
                    bound_lim.y = lim.y;
                }
            }
          regions->bounding_region.region.pos = bound_min;
          regions->bounding_region.region.size = bound_lim - bound_min;
        }
      else if ((box_type == jp2_label_4cc) && (len < 8192))
        {
          rep_id = JX_LABEL_NODE;
          label = new char[len+1];
          read_state->box.read((kdu_byte *) label,len);
          label[len] = '\0';
        }
      else if (box_type == jp2_uuid_4cc)
        {
          rep_id = JX_REF_NODE;
          memset(data,0,16);
          read_state->box.read(data,16);
          ref = new jx_metaref;
          ref->src = manager->ultimate_src;
          ref->src_loc = read_state->box.get_locator();
        }
      else
        {
          rep_id = JX_REF_NODE;
          ref = new jx_metaref;
          ref->src = manager->ultimate_src;
          ref->src_loc = read_state->box.get_locator();
        }
      read_state->box.close();
      manager->link(this);
      if (flags & JX_METANODE_DESCENDANTS_KNOWN)
        update_completed_descendants();
    }

  while (!(flags & JX_METANODE_DESCENDANTS_KNOWN))
    {
      if (read_state->codestream_source != NULL)
        {
          if (!read_state->codestream_source->finish(false))
            return false;
          continue;
        }
      if (read_state->layer_source != NULL)
        {
          if (!read_state->layer_source->finish())
            return false;
          continue;
        }
      assert(read_state->asoc.exists());
      assert(!read_state->box);
      if (read_state->box.open(&read_state->asoc))
        {
          if (manager->test_box_filter(read_state->box.get_box_type()))
            {
              jx_metanode *node = add_descendant();
              node->donate_input_box(read_state->box);
            }
          else
            read_state->box.close(); // Skip this sub-box
        }
      else if (!read_state->asoc.is_complete())
        return false;
      else
        {
          flags |= JX_METANODE_DESCENDANTS_KNOWN;
          update_completed_descendants();
        }
    }

  delete read_state;
  read_state = NULL;
  return true;
}

/*****************************************************************************/
/*                        jx_metanode::load_recursive                        */
/*****************************************************************************/

void
  jx_metanode::load_recursive()
{
  if ((flags & JX_METANODE_EXISTING) && (read_state != NULL) &&
      !((flags & JX_METANODE_BOX_COMPLETE) &&
        (flags & JX_METANODE_DESCENDANTS_KNOWN)))
    finish_reading();

  for (jx_metanode *scan=head;
       (scan != NULL) && (num_completed_descendants != num_descendants);
       scan=scan->next_sibling)
    scan->load_recursive();
}

/*****************************************************************************/
/*                      jx_metanode::mark_for_writing                        */
/*****************************************************************************/

bool
  jx_metanode::mark_for_writing(jx_metagroup *context)
{
  bool marked_something = false;
  jx_metanode *scan;
  for (scan=head; scan != NULL; scan=scan->next_sibling)
    if (scan->mark_for_writing(context))
      marked_something = true;
  
  if (!marked_something)
    { // See if this node needs to be written, even though it has no
      // descendants which need to be written.
      if ((flags & JX_METANODE_WRITTEN) ||
          ((write_state != NULL) && (write_state->context != NULL)))
        return false; // Already written
      if (box_type == jp2_number_list_4cc)
        { // There is no need to write a numlist which is not associated
          // with any descendants or parents.  Let's see if this is the case.
          for (scan=parent; scan != NULL; scan=scan->parent)
            if ((scan->box_type != 0) && (scan->box_type != jp2_free_4cc) &&
                (scan->box_type != jp2_number_list_4cc))
              break;
          if (scan == NULL)
            return false;
        }
    }
  if (write_state == NULL)
    {
      write_state = new jx_metawrite;
      flags &= ~JX_METANODE_WRITTEN;
    }
  assert(!(flags & JX_METANODE_WRITTEN));
  write_state->context = context;
  assert((write_state->active_descendant == NULL) &&
         (!write_state->asoc) && (!write_state->box));
  return true;
}

/*****************************************************************************/
/*                            jx_metanode::write                             */
/*****************************************************************************/

jp2_output_box *
  jx_metanode::write(jp2_output_box *super_box, jx_target *target,
                     jx_metagroup *context, int *i_param, void **addr_param)
{
  if ((write_state == NULL) || (write_state->context != context))
    return NULL;

  assert(!(flags & JX_METANODE_WRITTEN));
  
  if ((box_type != 0) && (box_type != jp2_free_4cc) && (num_descendants > 0))
    { // Need to write box contents and descendants within an association box
      assert(parent != NULL);
      if (!write_state->asoc.exists())
        {
          if (super_box != NULL)
            write_state->asoc.open(super_box,jp2_association_4cc);
          else
            target->open_top_box(&write_state->asoc,jp2_association_4cc);
        }
      super_box = &write_state->asoc;
    }

  if (write_state->active_descendant == NULL)
    { // See if we need to write the box contents
      if (write_state->box.exists())
        { // Must be returning after having the application complete this box
          assert((rep_id == JX_REF_NODE) && (ref->src == NULL));
          write_state->box.close();
        }
      else if ((box_type != 0) && (box_type != jp2_free_4cc))
        {
          if (super_box != NULL)
            write_state->box.open(super_box,box_type);
          else
            target->open_top_box(&write_state->box,box_type);
          if (rep_id == JX_ROI_NODE)
            { // Write an ROI description box
              assert(box_type == jp2_roi_description_4cc);
              regions->write(write_state->box);
            }
          else if (rep_id == JX_NUMLIST_NODE)
            { // Write a number list box
              assert(box_type == jp2_number_list_4cc);
              numlist->write(write_state->box);
            }
          else if (rep_id == JX_LABEL_NODE)
            { // Write a label box
              assert(box_type == jp2_label_4cc);
              write_state->box.write((kdu_byte *) label,(int) strlen(label));
            }
          else if (rep_id == JX_REF_NODE)
            {
              if (ref->src != NULL)
                { // Copy source box.
                  jp2_input_box src;
                  src.open(ref->src,ref->src_loc);
                  kdu_long body_bytes = src.get_remaining_bytes();
                  write_state->box.set_target_size(body_bytes);
                  int xfer_bytes;
                  kdu_byte buf[1024];
                  while ((xfer_bytes=src.read(buf,1024)) > 0)
                    {
                      body_bytes -= xfer_bytes;
                      write_state->box.write(buf,xfer_bytes);
                    }
                  src.close();
                  assert(body_bytes == 0);
                }
              else
                { // Application must write this box
                  if (i_param != NULL)
                    *i_param = ref->i_param;
                  if (addr_param != NULL)
                    *addr_param = ref->addr_param;
                  return &(write_state->box);
                }
            }
          else
            assert(0);
          write_state->box.close();
        }
      write_state->active_descendant = head;
    }

  while (write_state->active_descendant != NULL)
    {
      jp2_output_box *interrupt =
        write_state->active_descendant->write(super_box,target,context,
                                              i_param,addr_param);
      if (interrupt != NULL)
        return interrupt;
      write_state->active_descendant =
        write_state->active_descendant->next_sibling;
    }

  write_state->asoc.close(); // Safe even if it was never open
  delete write_state;
  write_state = NULL;
  flags |= JX_METANODE_WRITTEN;
  return NULL; // All done
}


/* ========================================================================= */
/*                              jpx_metanode                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                      jpx_metanode::get_numlist_info                       */
/*****************************************************************************/

bool
  jpx_metanode::get_numlist_info(int &num_codestreams, int &num_layers,
                                 bool &applies_to_rendered_result)
{
  if ((state == NULL) || (state->rep_id != JX_NUMLIST_NODE))
    return false;
  assert(state->flags & JX_METANODE_BOX_COMPLETE);
  num_codestreams = state->numlist->num_codestreams;
  num_layers = state->numlist->num_compositing_layers;
  applies_to_rendered_result = state->numlist->rendered_result;
  return true;
}

/*****************************************************************************/
/*                  jpx_metanode::get_numlist_codestreams                    */
/*****************************************************************************/

const int *
  jpx_metanode::get_numlist_codestreams()
{
  if ((state == NULL) || (state->rep_id != JX_NUMLIST_NODE))
    return NULL;
  return state->numlist->codestream_indices;
}

/*****************************************************************************/
/*                     jpx_metanode::get_numlist_layers                      */
/*****************************************************************************/

const int *
  jpx_metanode::get_numlist_layers()
{
  if ((state == NULL) || (state->rep_id != JX_NUMLIST_NODE))
    return NULL;
  return state->numlist->layer_indices;
}

/*****************************************************************************/
/*                  jpx_metanode::get_numlist_codestream                     */
/*****************************************************************************/

int
  jpx_metanode::get_numlist_codestream(int which)
{
  if ((state == NULL) || (state->rep_id != JX_NUMLIST_NODE) ||
      (which < 0) || (state->numlist->num_codestreams <= which))
    return -1;
  return state->numlist->codestream_indices[which];
}

/*****************************************************************************/
/*                     jpx_metanode::get_numlist_layer                       */
/*****************************************************************************/

int
  jpx_metanode::get_numlist_layer(int which)
{
  if ((state == NULL) || (state->rep_id != JX_NUMLIST_NODE) ||
      (which < 0) || (state->numlist->num_compositing_layers <= which))
    return -1;
  return state->numlist->layer_indices[which];
}

/*****************************************************************************/
/*                       jpx_metanode::test_numlist                          */
/*****************************************************************************/

bool
  jpx_metanode::test_numlist(int codestream_idx, int layer_idx,
                             bool applies_to_rendered_result)
{
  if ((state == NULL) || (state->rep_id != JX_NUMLIST_NODE))
    return false;

  int n;
  if (applies_to_rendered_result && !state->numlist->rendered_result)
    return false;
  if (codestream_idx >= 0)
    {
      for (n=0; n < state->numlist->num_codestreams; n++)
        if (state->numlist->codestream_indices[n] == codestream_idx)
          break;
      if (n == state->numlist->num_codestreams)
        return false;
    }
  if (layer_idx >= 0)
    {
      for (n=0; n < state->numlist->num_compositing_layers; n++)
        if (state->numlist->layer_indices[n] == layer_idx)
          break;
      if (n == state->numlist->num_compositing_layers)
        return false;
    }
  return true;
}

/*****************************************************************************/
/*                      jpx_metanode::get_num_regions                        */
/*****************************************************************************/

int
  jpx_metanode::get_num_regions()
{
  if ((state == NULL) || (state->rep_id != JX_ROI_NODE))
    return 0;
  assert(state->flags & JX_METANODE_BOX_COMPLETE);
  return state->regions->num_regions;
}

/*****************************************************************************/
/*                        jpx_metanode::get_regions                          */
/*****************************************************************************/

const jpx_roi *
  jpx_metanode::get_regions()
{
  if ((state == NULL) || (state->rep_id != JX_ROI_NODE))
    return NULL;
  return state->regions->regions;
}

/*****************************************************************************/
/*                         jpx_metanode::get_region                          */
/*****************************************************************************/

jpx_roi
  jpx_metanode::get_region(int which)
{
  jpx_roi result;
  if ((state != NULL) && (state->rep_id == JX_ROI_NODE) &&
      (which >= 0) && (which < state->regions->num_regions))
    result = state->regions->regions[which];
  return result;
}

/*****************************************************************************/
/*                      jpx_metanode::get_bounding_box                       */
/*****************************************************************************/

kdu_dims
  jpx_metanode::get_bounding_box()
{
  kdu_dims result;
  if ((state != NULL) && (state->rep_id == JX_ROI_NODE) &&
      (state->regions->num_regions > 0))
    result = state->regions->bounding_region.region;
  return result;
}


/*****************************************************************************/
/*                        jpx_metanode::test_region                          */
/*****************************************************************************/

bool
  jpx_metanode::test_region(kdu_dims region)
{
  if ((state == NULL) || (state->rep_id != JX_ROI_NODE))
    return false;
  int n;
  for (n=0; n < state->regions->num_regions; n++)
    if (state->regions->regions[n].region.intersects(region))
      return true;
  return false;
}

/*****************************************************************************/
/*                       jpx_metanode::get_box_type                          */
/*****************************************************************************/

kdu_uint32
  jpx_metanode::get_box_type()
{
  return (state == NULL)?0:(state->box_type);
}

/*****************************************************************************/
/*                         jpx_metanode::get_label                           */
/*****************************************************************************/

const char *
  jpx_metanode::get_label()
{
  if ((state == NULL) || (state->rep_id != JX_LABEL_NODE))
    return NULL;
  return state->label;
}

/*****************************************************************************/
/*                         jpx_metanode::get_uuid                            */
/*****************************************************************************/

bool
  jpx_metanode::get_uuid(kdu_byte uuid[])
{
  if ((state == NULL) || (state->box_type != jp2_uuid_4cc) ||
      (state->rep_id != JX_REF_NODE))
    return false;
  memcpy(uuid,state->data,16);
  return true;
}

/*****************************************************************************/
/*                       jpx_metanode::get_existing                          */
/*****************************************************************************/

jp2_locator
  jpx_metanode::get_existing(jp2_family_src * &src)
{
  src = NULL;
  if ((state == NULL) || (state->rep_id != JX_REF_NODE))
    return jp2_locator();
  src = state->ref->src;
  return state->ref->src_loc;
}

/*****************************************************************************/
/*                        jpx_metanode::get_delayed                          */
/*****************************************************************************/

bool
  jpx_metanode::get_delayed(int &i_param, void * &addr_param)
{
  if ((state == NULL) || (state->rep_id != JX_REF_NODE) ||
      (state->ref->src != NULL))
    return false;
  i_param = state->ref->i_param;
  addr_param = state->ref->addr_param;
  return true;
}

/*****************************************************************************/
/*                     jpx_metanode::count_descendants                       */
/*****************************************************************************/

bool
  jpx_metanode::count_descendants(int &count)
{
  if (state == NULL)
    return false;
  if (!(state->flags & JX_METANODE_DESCENDANTS_KNOWN))
    {
      if (state->parent == NULL)
        { // We are at the root.  Parse as many top level boxes as possible
          jx_source *source = state->manager->source;
          while (source->parse_next_top_level_box());
          int n=0;
          jx_codestream_source *cs;
          jx_layer_source *lh;
          while ((cs = source->get_codestream(n++)) != NULL)
            cs->finish(false);
          n = 0;
          while ((lh = source->get_compositing_layer(n++)) != NULL)
            lh->finish();
        }
      else
        {
          assert((state->flags & JX_METANODE_EXISTING) &&
                 (state->read_state != NULL));
          state->finish_reading();
        }
    }
  count = state->num_descendants;
  return (state->flags & JX_METANODE_DESCENDANTS_KNOWN)?true:false;
}

/*****************************************************************************/
/*                      jpx_metanode::get_descendant                         */
/*****************************************************************************/

jpx_metanode
  jpx_metanode::get_descendant(int which)
{
  if ((state == NULL) || (which < 0) || (which >= state->num_descendants))
    return jpx_metanode(NULL);
  jx_metanode *node=state->head;
  for (; (which > 0) && (node != NULL); which--)
    node = node->next_sibling;
  if ((node != NULL) && !(node->flags & JX_METANODE_BOX_COMPLETE))
    node = NULL;
  return jpx_metanode(node);
}

/*****************************************************************************/
/*                        jpx_metanode::get_parent                           */
/*****************************************************************************/

jpx_metanode
  jpx_metanode::get_parent()
{
  return jpx_metanode((state==NULL)?NULL:state->parent);
}

/*****************************************************************************/
/*                      jpx_metanode::change_parent                          */
/*****************************************************************************/

bool
  jpx_metanode::change_parent(jpx_metanode new_parent)
{
  if (new_parent.state == state->parent)
    return false;
  for (jx_metanode *test=new_parent.state; test != NULL; test=test->parent)
    if (test == state)
      return false;
  state->unlink_parent();
  if (state->metagroup != NULL)
    state->metagroup->unlink(state);
  new_parent.state->append_child(state);
  state->manager->link(state);
  state->flags |= JX_METANODE_ANCESTOR_CHANGED;
  state->append_to_touched_list(true);
  return true;
}


/*****************************************************************************/
/*                        jpx_metanode::add_numlist                          */
/*****************************************************************************/

jpx_metanode
  jpx_metanode::add_numlist(int num_codestreams, const int *codestream_indices,
                            int num_layers, const int *layer_indices,
                            bool applies_to_rendered_result)
{
  assert(state != NULL);
  if (state->manager->target != NULL)
    { KDU_ERROR_DEV(e,25); e <<
        KDU_TXT("Trying to add metadata to a `jpx_target' object "
        "whose `write_metadata' function as already been called.");
    }
  jx_metanode *node = state->add_numlist(num_codestreams,codestream_indices,
                                         num_layers,layer_indices,
                                         applies_to_rendered_result);

  node->flags |= (JX_METANODE_DESCENDANTS_KNOWN | JX_METANODE_IS_COMPLETE);
  state->num_completed_descendants++;
  return jpx_metanode(node);
}

/*****************************************************************************/
/*                        jpx_metanode::add_regions                          */
/*****************************************************************************/

jpx_metanode
  jpx_metanode::add_regions(int num_regions, const jpx_roi *regions)
{
  assert(state != NULL);
  if (state->manager->target != NULL)
    { KDU_ERROR_DEV(e,26); e <<
        KDU_TXT("Trying to add metadata to a `jpx_target' object "
        "whose `write_metadata' function has already been called.");
    }
  jx_metanode *node = state->add_descendant();
  node->flags |= (JX_METANODE_IS_COMPLETE | JX_METANODE_BOX_COMPLETE |
                  JX_METANODE_DESCENDANTS_KNOWN);
  state->num_completed_descendants++;
  node->box_type = jp2_roi_description_4cc;
  node->rep_id = JX_ROI_NODE;
  node->regions = new jx_regions;
  node->regions->set_num_regions(num_regions);
  node->append_to_touched_list(false);
  
  kdu_dims rect;
  kdu_coords bound_min, bound_lim, min, lim;
  for (int n=0; n < num_regions; n++)
    {
      node->regions->regions[n] = regions[n];
      rect = node->regions->regions[n].region;
      int sz = (rect.size.x < rect.size.y)?rect.size.x:rect.size.y;
      if ((n == 0) || (sz > node->regions->min_size))
        node->regions->min_size = sz;
      min = rect.pos;
      lim = min + rect.size;
      if (n == 0)
        { bound_min = min; bound_lim = lim; }
      else
        { // Grow `bound' to include `rect'
          if (min.x < bound_min.x)
            bound_min.x = min.x;
          if (min.y < bound_min.y)
            bound_min.y = min.y;
          if (lim.x > bound_lim.x)
            bound_lim.x = lim.x;
          if (lim.y > bound_lim.y)
            bound_lim.y = lim.y;
        }
      node->regions->bounding_region.region.pos = bound_min;
      node->regions->bounding_region.region.size = bound_lim - bound_min;
    }
  node->manager->link(node);
  return jpx_metanode(node);
}

/*****************************************************************************/
/*                         jpx_metanode::add_label                           */
/*****************************************************************************/

jpx_metanode
  jpx_metanode::add_label(const char *text)
{
  assert(state != NULL);
  if (state->manager->target != NULL)
    { KDU_ERROR_DEV(e,27); e <<
        KDU_TXT("Trying to add metadata to a `jpx_target' object "
        "whose `write_metadata' function has already been called.");
    }
  jx_metanode *node = state->add_descendant();
  node->flags |= (JX_METANODE_IS_COMPLETE | JX_METANODE_BOX_COMPLETE |
                  JX_METANODE_DESCENDANTS_KNOWN);
  state->num_completed_descendants++;
  node->box_type = jp2_label_4cc;
  node->rep_id = JX_LABEL_NODE;
  node->label = new char[strlen(text)+1];
  strcpy(node->label,text);
  node->manager->link(node);
  node->append_to_touched_list(false);
  return jpx_metanode(node);
}

/*****************************************************************************/
/*                      jpx_metanode::change_to_label                        */
/*****************************************************************************/

void
  jpx_metanode::change_to_label(const char *text)
{
  if (state->metagroup != NULL)
    state->metagroup->unlink(state);
  switch (state->rep_id) {
    case JX_REF_NODE:
      if (state->ref != NULL) delete state->ref;
      break;
    case JX_NUMLIST_NODE:
      if (state->numlist != NULL) delete state->numlist;
      break;
    case JX_ROI_NODE:
      if (state->regions != NULL) delete state->regions;
      break;
    case JX_LABEL_NODE:
      if (state->label != NULL) delete state->label;
      break;
    default: break;
  };
  state->box_type = jp2_label_4cc;
  state->rep_id = JX_LABEL_NODE;
  state->label = new char[strlen(text)+1];
  strcpy(state->label,text);  
  if ((state->flags & JX_METANODE_EXISTING) && (state->read_state != NULL))
    { delete state->read_state; state->read_state = NULL; }
  state->flags &= ~(JX_METANODE_EXISTING | JX_METANODE_WRITTEN);
  state->flags |= (JX_METANODE_IS_COMPLETE | JX_METANODE_BOX_COMPLETE |
                   JX_METANODE_DESCENDANTS_KNOWN |
                   JX_METANODE_CONTENTS_CHANGED);
  state->manager->link(state);
  state->append_to_touched_list(true);
}

/*****************************************************************************/
/*                        jpx_metanode::add_delayed                          */
/*****************************************************************************/

jpx_metanode
  jpx_metanode::add_delayed(kdu_uint32 box_type, int i_param, void *addr_param)
{
  assert(state != NULL);
  if (state->manager->target != NULL)
    { KDU_ERROR_DEV(e,28); e <<
        KDU_TXT("Trying to add metadata to a `jpx_target' object "
        "whose `write_metadata' function has already been called.");
    }
  jx_metanode *node = state->add_descendant();
  node->flags |= (JX_METANODE_IS_COMPLETE | JX_METANODE_BOX_COMPLETE |
                  JX_METANODE_DESCENDANTS_KNOWN |
                  JX_METANODE_CONTENTS_CHANGED);
  state->num_completed_descendants++;
  node->box_type = box_type;
  node->rep_id = JX_REF_NODE;
  node->ref = new jx_metaref;
  node->ref->i_param = i_param;
  node->ref->addr_param = addr_param;
  node->manager->link(node);
  node->append_to_touched_list(false);
  return jpx_metanode(node);
}

/*****************************************************************************/
/*                     jpx_metanode::change_to_delayed                       */
/*****************************************************************************/

void
  jpx_metanode::change_to_delayed(kdu_uint32 box_type, int i_param,
                                  void *addr_param)
{
  if (state->metagroup != NULL)
    state->metagroup->unlink(state);
  switch (state->rep_id) {
    case JX_REF_NODE:
      if (state->ref != NULL) delete state->ref;
      break;
    case JX_NUMLIST_NODE:
      if (state->numlist != NULL) delete state->numlist;
      break;
    case JX_ROI_NODE:
      if (state->regions != NULL) delete state->regions;
      break;
    case JX_LABEL_NODE:
      if (state->label != NULL) delete state->label;
      break;
    default: break;
  };
  state->box_type = box_type;
  state->rep_id = JX_REF_NODE;
  state->ref = new jx_metaref;
  state->ref->i_param = i_param;
  state->ref->addr_param = addr_param;  
  if ((state->flags & JX_METANODE_EXISTING) && (state->read_state != NULL))
    { delete state->read_state; state->read_state = NULL; }
  state->flags &= ~(JX_METANODE_EXISTING | JX_METANODE_WRITTEN);
  state->flags |= (JX_METANODE_IS_COMPLETE | JX_METANODE_BOX_COMPLETE |
                   JX_METANODE_DESCENDANTS_KNOWN |
                   JX_METANODE_CONTENTS_CHANGED);
  
  state->manager->link(state);
  state->append_to_touched_list(true);
}

/*****************************************************************************/
/*                          jpx_metanode::add_copy                           */
/*****************************************************************************/

jpx_metanode
  jpx_metanode::add_copy(jpx_metanode src, bool recursive)
{
  assert((state != NULL) && (src.state != NULL));
  if (state->manager->target != NULL)
    { KDU_ERROR_DEV(e,29); e <<
        KDU_TXT("Trying to add metadata to a `jpx_target' object "
        "whose `write_metadata' function has already been called.");
    }

  jpx_metanode result;

  if (src.state->rep_id == JX_NUMLIST_NODE)
    result = add_numlist(src.state->numlist->num_codestreams,
                         src.state->numlist->codestream_indices,
                         src.state->numlist->num_compositing_layers,
                         src.state->numlist->layer_indices,
                         src.state->numlist->rendered_result);
  else if (src.state->rep_id == JX_ROI_NODE)
    result = add_regions(src.state->regions->num_regions,
                         src.state->regions->regions);
  else if (src.state->rep_id == JX_LABEL_NODE)
    result = add_label(src.state->label);
  else if (src.state->rep_id == JX_REF_NODE)
    {
      jx_metanode *node = state->add_descendant();
      node->flags |= (JX_METANODE_IS_COMPLETE | JX_METANODE_BOX_COMPLETE |
                      JX_METANODE_DESCENDANTS_KNOWN);
      state->num_completed_descendants++;
      node->box_type = src.state->box_type;
      node->rep_id = JX_REF_NODE;
      node->ref = new jx_metaref;
      *(node->ref) = *(src.state->ref);
      node->manager->link(node);
      node->append_to_touched_list(false);
      result = jpx_metanode(node);
    }
  else
    assert(0);
  if (recursive)
    {
      src.state->finish_reading();      
      for (jx_metanode *scan=src.state->head;
           scan != NULL; scan=scan->next_sibling)
        {
          scan->finish_reading();
          if (!(scan->flags & JX_METANODE_BOX_COMPLETE))
            continue;
          result.add_copy(jpx_metanode(scan),true);
        }
    }
  return result;
}

/*****************************************************************************/
/*                        jpx_metanode::delete_node                          */
/*****************************************************************************/

void
  jpx_metanode::delete_node()
{
  assert(state != NULL);

  if ((state->parent != NULL) && (state->parent->rep_id == JX_NUMLIST_NODE) &&
      (state->parent->head == state) && (state->parent->tail == state) &&
      (state->metagroup != NULL) && (state->metagroup->roigroup != NULL))
    { // We are deleting an ROI description object whose parent serves
      // only to associate it with a number list.  Kill the parent too.
      state->parent->safe_delete();
          // Does all the unlinking and puts on deleted list
    }
  else
    state->safe_delete(); // Does all the unlinking and puts on deleted list
  state = NULL;
}

/*****************************************************************************/
/*                        jpx_metanode::is_changed                           */
/*****************************************************************************/

bool
  jpx_metanode::is_changed()
{
  return ((state != NULL) && (state->flags & JX_METANODE_CONTENTS_CHANGED));  
}

/*****************************************************************************/
/*                     jpx_metanode::ancestor_changed                        */
/*****************************************************************************/

bool
  jpx_metanode::ancestor_changed()
{
  return ((state != NULL) && (state->flags & JX_METANODE_ANCESTOR_CHANGED));  
}

/*****************************************************************************/
/*                        jpx_metanode::is_deleted                           */
/*****************************************************************************/

bool
  jpx_metanode::is_deleted()
{
  return ((state == NULL) || (state->flags & JX_METANODE_DELETED));  
}

/*****************************************************************************/
/*                          jpx_metanode::touch                              */
/*****************************************************************************/

void
  jpx_metanode::touch()
{
  if (state != NULL)
    state->append_to_touched_list(true);
}

/*****************************************************************************/
/*                       jpx_metanode::set_state_ref                         */
/*****************************************************************************/

void
  jpx_metanode::set_state_ref(void *ref)
{
  if (state != NULL)
    state->app_state_ref = ref;
}

/*****************************************************************************/
/*                       jpx_metanode::get_state_ref                         */
/*****************************************************************************/

void *
  jpx_metanode::get_state_ref()
{
  return (state==NULL)?NULL:(state->app_state_ref);
}


/* ========================================================================= */
/*                               jx_metagroup                                */
/* ========================================================================= */

/*****************************************************************************/
/*                            jx_metagroup::link                             */
/*****************************************************************************/

void
  jx_metagroup::link(jx_metanode *node)
{
  node->metagroup = this;
  node->prev_link = tail;
  node->next_link = NULL;
  if (tail == NULL)
    head = tail = node;
  else
    tail = tail->next_link = node;
}

/*****************************************************************************/
/*                           jx_metagroup::unlink                            */
/*****************************************************************************/

void
  jx_metagroup::unlink(jx_metanode *node)
{
  assert(node->metagroup == this);
  if (node->prev_link == NULL)
    {
      assert(node == head);
      head = node->next_link;
    }
  else
    node->prev_link->next_link = node->next_link;
  if (node->next_link == NULL)
    {
      assert(node == tail);
      tail = node->prev_link;
    }
  else
    node->next_link->prev_link = node->prev_link;
  node->metagroup = NULL;
  node->next_link = node->prev_link = NULL;

  jx_roigroup *roig = roigroup;
  if ((head == NULL) && (roig != NULL))
    {
      assert(roig->level == 0);
      int offset = (int)(this - roig->metagroups);
      assert((offset >= 0) && (offset < (JX_ROIGROUP_SIZE*JX_ROIGROUP_SIZE)));
      kdu_coords loc;
      loc.y = offset / JX_ROIGROUP_SIZE;
      loc.x = offset - loc.y * JX_ROIGROUP_SIZE;
      roig->delete_child(loc);
    }
}

/*****************************************************************************/
/*                     jx_metagroup::mark_for_writing                        */
/*****************************************************************************/

bool
  jx_metagroup::mark_for_writing()
{
  jx_metanode *scan, *parent;
  bool marked_something = false;
  for (scan=head; scan != NULL; scan=scan->next_link)
    {
      if (scan->mark_for_writing(this))
        { // Mark all parents on which this one depends for writing
          marked_something = true;
          for (parent=scan->parent; parent != NULL; parent=parent->parent)
            {
              if (parent->write_state == NULL)
                {
                  parent->write_state = new jx_metawrite;
                  parent->flags &= ~(JX_METANODE_WRITTEN);
                }
              assert(!(parent->flags & JX_METANODE_WRITTEN));
              if (parent->write_state->context == this)
                break; // Already taken care of this one
              parent->write_state->context = this;
              assert((parent->write_state->active_descendant == NULL) &&
                     (!parent->write_state->asoc) &&
                     (!parent->write_state->box));
            }
        }
    }
  return marked_something;
}


/* ========================================================================= */
/*                               jx_roigroup                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                        jx_roigroup::~jx_roigroup                          */
/*****************************************************************************/

jx_roigroup::~jx_roigroup()
{
  if (level == 0)
    return;
  jx_roigroup **scan = sub_groups;
  for (int n=JX_ROIGROUP_SIZE*JX_ROIGROUP_SIZE; n > 0; n--, scan++)
    if (*scan != NULL)
      delete *scan;
}

/*****************************************************************************/
/*                        jx_roigroup::delete_child                          */
/*****************************************************************************/

void
  jx_roigroup::delete_child(kdu_coords loc)
{
  assert((loc.x >= 0) && (loc.x < JX_ROIGROUP_SIZE) &&
         (loc.y >= 0) && (loc.y < JX_ROIGROUP_SIZE));
  int n, offset = loc.x + loc.y*JX_ROIGROUP_SIZE;
  bool any_left = false;
  if (level == 0)
    {
      assert(metagroups[offset].head == NULL);
      for (n=0; n < (JX_ROIGROUP_SIZE*JX_ROIGROUP_SIZE); n++)
        if (metagroups[n].head != NULL)
          { any_left = true; break; }
    }
  else
    {
      assert(sub_groups[offset] != NULL);
      delete sub_groups[offset];
      sub_groups[offset] = NULL;
      for (n=0; n < (JX_ROIGROUP_SIZE*JX_ROIGROUP_SIZE); n++)
        if (sub_groups[n] != NULL)
          { any_left = true; break; }
    }
  if (any_left)
    return;

  if (parent == NULL)
    {
      assert(this == manager->roi_scales[scale_idx]);
      manager->roi_scales[scale_idx] = NULL;
      delete this;
    }
  else
    {
      loc = group_dims.pos - parent->group_dims.pos;
      loc.x = loc.x / parent->elt_size.x;
      loc.y = loc.y / parent->elt_size.y;
      assert((loc.x >= 0) && (loc.x < JX_ROIGROUP_SIZE) &&
             (loc.y >= 0) && (loc.y < JX_ROIGROUP_SIZE));
      assert(this == parent->sub_groups[loc.x+loc.y*JX_ROIGROUP_SIZE]);
      parent->delete_child(loc);
    }
}


/* ========================================================================= */
/*                            jx_metaloc_manager                             */
/* ========================================================================= */

/*****************************************************************************/
/*                  jx_metaloc_manager::jx_metaloc_manager                   */
/*****************************************************************************/

jx_metaloc_manager::jx_metaloc_manager()
{
  locator_heap = NULL;
  block_heap = NULL;
  root = allocate_block(); // Allocates an empty block of references
}
  
/*****************************************************************************/
/*                  jx_metaloc_manager::~jx_metaloc_manager                  */
/*****************************************************************************/

jx_metaloc_manager::~jx_metaloc_manager()
{
  jx_metaloc_alloc *elt;
  while ((elt=locator_heap) != NULL)
    {
      locator_heap = elt->next;
      delete elt;
    }
  jx_metaloc_block_alloc *blk;
  while ((blk=block_heap) != NULL)
    {
      block_heap = blk->next;
      delete blk;
    }
}

/*****************************************************************************/
/*                      jx_metaloc_manager::get_locator                      */
/*****************************************************************************/

jx_metaloc *
  jx_metaloc_manager::get_locator(kdu_long pos, bool create_if_necessary)
{
  jx_metaloc_block *container = root;
  int idx;
  
  jx_metaloc *elt = container;
  while ((elt != NULL) && (elt == container))
    {
      elt = NULL;
      for (idx=container->active_elts-1; idx >= 0; idx--)
        if (container->elts[idx]->loc <= pos)
          { elt = container->elts[idx];  break; }
      if ((elt != NULL) && elt->is_block())
        container = (jx_metaloc_block *) elt; // Descend into sub-block
    }
  if ((elt != NULL) && (elt->loc == pos))
    return elt; // Found an existing match
  if (!create_if_necessary)
    return NULL;
  
  // If we get here, we have to create a new element
  // Ideally, the element goes right after location `idx' within
  // `container'; if `idx' is -ve, we need to position it before the first
  // element.  We can shuffle elements within `container' around to make this
  // possible, unless `container' is full. In that case, we want to adjust
  // things while keeping the tree as balanced as possible.  To do that, we
  // insert a new container at the same level and redistribute the branches.
  // Inserting a new container may require recursive application of the
  // procedure all the way up to the root of the tree.  These operations are
  // all accomplished by the recursive function `insert_into_metaloc_block'.
  assert(container != NULL);
  elt = allocate_locator();
  elt->loc = pos;
  insert_into_metaloc_block(container,elt,idx);
  return elt;
}

/*****************************************************************************/
/*              jx_metaloc_manager::insert_into_metaloc_block                */
/*****************************************************************************/

void
  jx_metaloc_manager::insert_into_metaloc_block(jx_metaloc_block *container,
                                                jx_metaloc *elt, int idx)
{
  assert((idx >= -1) && (idx < container->active_elts));
  int n;
  if (container->active_elts < JX_METALOC_BLOCK_DIM)
    {
      for (n=container->active_elts-1; n > idx; n--)
        container->elts[n+1] = container->elts[n]; // Shuffle existing elements
      container->active_elts++;
      container->elts[idx+1] = elt;
      if (idx == -1)
        container->loc = elt->loc; // Container location comes from first elt
      if (elt->is_block())
        ((jx_metaloc_block *) elt)->parent = container;
    }
  else
    { // Container is full, need to insert a new container at the same level
      jx_metaloc_block *new_container = allocate_block();
      if (idx == (container->active_elts-1))
        { // Don't bother redistributing elements between `container' and
          // `new_container'; we are probably just discovering elements in
          // linear order here, so it is probably wasteful to plan for fuure
          // insertions at this point.
          new_container->elts[0] = elt;
          new_container->active_elts = 1;
          new_container->loc = elt->loc;
          if (elt->is_block())
            ((jx_metaloc_block *) elt)->parent = new_container;
        }
      else
        {
          new_container->active_elts = (container->active_elts >> 1);
          container->active_elts -= new_container->active_elts;
          for (n=0; n < new_container->active_elts; n++)
            new_container->elts[n] = container->elts[container->active_elts+n];
          assert((container->active_elts > 0) &&
                 (new_container->active_elts > 0));
          new_container->loc = new_container->elts[0]->loc;
          if (idx < container->active_elts)
            insert_into_metaloc_block(container,elt,idx);
          else
            insert_into_metaloc_block(new_container,elt,
                                      idx-container->active_elts);
        }

      jx_metaloc_block *parent = container->parent;
      if (parent == NULL)
        { // Create new root node
          assert(container == root);
          root = allocate_block();
          root->active_elts = 2;
          root->elts[0] = container;
          root->elts[1] = new_container;
          root->loc = container->loc;
          container->parent = root;
          new_container->parent = root;
        }
      else
        {
          for (n=0; n < parent->active_elts; n++)
            if (parent->elts[n] == container)
              { // This condition should be matched by exactly one entry; if
                // not, nothing disastrous happens, we just don't insert the
                // new container and so we lose some references, but there
                // will be no memory leaks.
                insert_into_metaloc_block(parent,new_container,n);
                break;
              }
        }
    }
}


/* ========================================================================= */
/*                             jx_meta_manager                               */
/* ========================================================================= */

/*****************************************************************************/
/*                      jx_meta_manager::jx_meta_manager                     */
/*****************************************************************************/

jx_meta_manager::jx_meta_manager()
{
  ultimate_src = NULL;
  source = NULL;
  target = NULL;
  tree = NULL;
  unassociated_nodes.init();
  numlist_nodes.init();
  for (int n=0; n < 32; n++)
    roi_scales[n] = NULL;
  last_added_node = NULL;
  active_metagroup = NULL;
  num_filter_box_types = max_filter_box_types = 6;
  filter_box_types = new kdu_uint32[max_filter_box_types];
  filter_box_types[0] = jp2_label_4cc;
  filter_box_types[1] = jp2_xml_4cc;
  filter_box_types[2] = jp2_iprights_4cc;
  filter_box_types[3] = jp2_number_list_4cc;
  filter_box_types[4] = jp2_roi_description_4cc;
  filter_box_types[5] = jp2_uuid_4cc;
  deleted_nodes = NULL;
  touched_head = touched_tail = NULL;
}

/*****************************************************************************/
/*                     jx_meta_manager::~jx_meta_manager                     */
/*****************************************************************************/

jx_meta_manager::~jx_meta_manager()
{
  tree->safe_delete();
  tree = NULL;
  for (int n=0; n < 32; n++)
    if (roi_scales[n] != NULL)
      delete roi_scales[n];
  if (filter_box_types != NULL)
    delete[] filter_box_types;
  jx_metanode *node;
  while ((node=deleted_nodes) != NULL)
    {
      deleted_nodes = node->next_sibling;
      delete node;
    }
}

/*****************************************************************************/
/*                     jx_meta_manager::test_box_filter                      */
/*****************************************************************************/

bool
  jx_meta_manager::test_box_filter(kdu_uint32 type)
{
  if (type == jp2_association_4cc)
    return true;
  if (num_filter_box_types == 0)
    return true; // Accept everything
  for (int n=0; n < num_filter_box_types; n++)
    if (type == filter_box_types[n])
      return true;
  return false;
}

/*****************************************************************************/
/*                           jx_meta_manager::link                           */
/*****************************************************************************/

void
  jx_meta_manager::link(jx_metanode *node)
{
  last_added_node = node;
  if (node->rep_id == JX_NUMLIST_NODE)
    { // This is a number list node
      numlist_nodes.link(node);
    }
  else if (node->rep_id == JX_ROI_NODE)
    { // This is an ROI node
      kdu_dims region = node->regions->bounding_region.region;
      int max_dim = (region.size.x>region.size.y)?region.size.x:region.size.y;
      int scale_idx = 0;
      while ((max_dim > (JX_FINEST_SCALE_THRESHOLD<<scale_idx)) &&
             (scale_idx < 31))
        scale_idx++;
      jx_roigroup *roig = roi_scales[scale_idx];
      if (roig == NULL)
        roig = roi_scales[scale_idx] = new jx_roigroup(this,scale_idx,0);
      while ((region.pos.x >= roig->group_dims.size.x) ||
             (region.pos.y >= roig->group_dims.size.y))
        { // Need to create a new, larger root, with all existing groups
          // as subordinates
          jx_roigroup *tmp = new jx_roigroup(this,scale_idx,roig->level+1);
          tmp->sub_groups[0] = roig;
          roig = roig->parent = tmp;
          roi_scales[scale_idx] = roig;
        }
      jx_metagroup *mgrp = NULL;
      while (mgrp == NULL)
        {
          int p_x = region.pos.x - roig->group_dims.pos.x;
          int p_y = region.pos.y - roig->group_dims.pos.y;
          assert((p_x >= 0) && (p_y >= 0));
          p_x = p_x / roig->elt_size.x;
          p_y = p_y / roig->elt_size.y;
          assert((p_x < JX_ROIGROUP_SIZE) && (p_y < JX_ROIGROUP_SIZE));
          if (roig->level > 0)
            {
              jx_roigroup **sub = roig->sub_groups+p_y*JX_ROIGROUP_SIZE+p_x;
              if (*sub == NULL)
                {
                  *sub = new jx_roigroup(this,scale_idx,roig->level-1);
                  (*sub)->parent = roig;
                  (*sub)->group_dims.pos.x =
                    roig->group_dims.pos.x + p_x*roig->elt_size.x;
                  (*sub)->group_dims.pos.y =
                    roig->group_dims.pos.y + p_y*roig->elt_size.y;
                }
              roig = *sub;
            }
          else
            mgrp = roig->metagroups + p_y*JX_ROIGROUP_SIZE + p_x;
        }
      mgrp->roigroup = roig;
      mgrp->link(node);
    }
  else if (node->box_type != jp2_free_4cc)
    { // For all other node types, check if they are descended from numlist/ROI
      jx_metanode *test;
      for (test=node->parent; test != NULL; test=test->parent)
        if ((test->rep_id == JX_NUMLIST_NODE) ||
            (test->rep_id == JX_ROI_NODE))
          return; // Don't enter it onto any metagroup list
      
      // If we get here, we have an unassociated node
      unassociated_nodes.link(node);
    }
}

/*****************************************************************************/
/*                      jx_meta_manager::write_metadata                      */
/*****************************************************************************/

jp2_output_box *
  jx_meta_manager::write_metadata(int *i_param, void **addr_param)
{
  int next_scale_idx=0;
  jx_roigroup *roig=NULL;
  bool started_numlists=false;
  bool started_unassociated=false;
  kdu_coords roi_loc; // Location within current roigroup, if any
  int roi_offset=0; // Offset within current roigroup to `roi_loc'
  if (active_metagroup != NULL)
    {
      roig = active_metagroup->roigroup;
      if (roig == NULL)
        {
          next_scale_idx = 32; // We have already written all ROI metadata
          if (active_metagroup == &numlist_nodes)
            started_numlists = true;
          else if (active_metagroup == &unassociated_nodes)
            started_numlists = started_unassociated = true;
          else
            assert(0);
        }
      else
        {
          roi_offset = (int)(active_metagroup - roig->metagroups);
          assert((roi_offset >= 0) &&
                 (roi_offset < (JX_ROIGROUP_SIZE*JX_ROIGROUP_SIZE)));
          roi_loc.y = roi_offset / JX_ROIGROUP_SIZE;
          roi_loc.x = roi_offset - roi_loc.y*JX_ROIGROUP_SIZE;
        }
    }

  jp2_output_box sub, *interrupt=NULL;
  do {
      while (active_metagroup == NULL)
        { // Hunt for the next active metagroup
          assert(!active_out);
          if (roig == NULL)
            {
              if (next_scale_idx < 32)
                {
                  if ((roig = roi_scales[next_scale_idx++]) == NULL)
                    continue; // Try again with next scale, if there is one
                  roi_loc = kdu_coords(0,0);
                }
              else if (!started_numlists)
                {
                  started_numlists = true;
                  active_metagroup = &numlist_nodes;
                  break; // Found the metagroup we want
                }
              else if (!started_unassociated)
                {
                  started_unassociated = true;
                  active_metagroup = &unassociated_nodes;
                  active_metagroup->mark_for_writing();
                    // All other metagroups will be marked when we detect an
                    // `active_out' box which is not open, but the unassociated
                    // boxes are not written within any `active_out' superbox.
                  break; // Found the metagroup we want
                }
              else
                return NULL; // All done!!
            }
          else if (roi_loc.x < (JX_ROIGROUP_SIZE-1))
            { // Advance horizontally within current roigroup at current level
              roi_loc.x++;
            }
          else if (roi_loc.y < (JX_ROIGROUP_SIZE-1))
            { // Advance vertically within current roigroup at current level
              roi_loc.y++; roi_loc.x = 0;
            }
          else
            { // Close current group and go up to the parent
              roig->active_out.close();
              jx_roigroup *parent = roig->parent;
              if (parent != NULL)
                {
                  roi_loc = roig->group_dims.pos - parent->group_dims.pos;
                  roi_loc.x = roi_loc.x / parent->elt_size.x;
                  roi_loc.y = roi_loc.y / parent->elt_size.y;
                  assert((roi_loc.x >= 0) && (roi_loc.x < JX_ROIGROUP_SIZE) &&
                         (roi_loc.y >= 0) && (roi_loc.y < JX_ROIGROUP_SIZE));
                }
              roig = parent;
              continue; // Still need to advance within the parent group
            }

          // If we get here, we must be in an roigroup at some level
          assert(roig != NULL);
          roi_offset = roi_loc.x + roi_loc.y*JX_ROIGROUP_SIZE;
          while (roig->level > 0)
            { // Descend to the leaves
              if (roig->sub_groups[roi_offset] == NULL)
                break; // Need to go to next group at same/higher level
              roig = roig->sub_groups[roi_offset];
              roi_loc = kdu_coords(0,0);
              roi_offset = 0;
            }
          if (roig->level > 0)
            continue; // Advance to next roi/meta group at same/higher level
          active_metagroup = roig->metagroups + roi_offset;
        }

      if ((!active_out) && !started_unassociated)
        { // Writing for the active metagroup has not yet commenced
          if (!active_metagroup->mark_for_writing())
            { // Nothing to write; don't start an output context
              active_metagroup = NULL;
              continue;
            }
          char explanation[80]; // To go in the free box.
          explanation[0] = '\0';
          if (roig != NULL)
            { // Open boxes, as required to form the scale-space hierarchy
              bool all_open = roig->active_out.exists();
              while (!all_open)
                {
                  all_open = true;
                  jx_roigroup *pscan=roig;
                  while ((pscan->parent != NULL) &&
                         !pscan->parent->active_out)
                    { pscan = pscan->parent; all_open = false; }
                  if (pscan->parent != NULL)
                    {
                      pscan->active_out.open(&pscan->parent->active_out,
                                             jp2_association_4cc);
                      sprintf(explanation,
                              "ROI group: x0=%d; y0=%d; w=%d; h=%d",
                              pscan->group_dims.pos.x,
                              pscan->group_dims.pos.y,
                              pscan->group_dims.size.x,
                              pscan->group_dims.size.y);
                    }
                  else
                    {
                      target->open_top_box(&pscan->active_out,
                                           jp2_association_4cc);
                      sprintf(explanation,"ROI scale %d",next_scale_idx-1);
                    }
                  sub.open(&pscan->active_out,jp2_free_4cc);
                  sub.write((kdu_byte *) explanation,(int)strlen(explanation));
                  sub.close();
                }
              active_out.open(&roig->active_out,jp2_association_4cc);
              sprintf(explanation,"ROI group: x0=%d; y0=%d; w=%d; h=%d",
                      roig->group_dims.pos.x+roig->elt_size.x*roi_loc.x,
                      roig->group_dims.pos.y+roig->elt_size.y*roi_loc.y,
                      roig->elt_size.x,roig->elt_size.y);
            }
          else
            {
              target->open_top_box(&active_out,jp2_association_4cc);
              strcpy(explanation,"Metadata associated with image entities");
            }

          sub.open(&active_out,jp2_free_4cc); // So there will be no particular
          sub.write((kdu_byte *) explanation,(int)strlen(explanation));
          sub.close();                        // associations
        }

      if (active_out.exists())
        interrupt =
          tree->write(&active_out,target,active_metagroup,i_param,addr_param);
      else
        { // Write unassociated nodes at the top level
          assert(active_metagroup == &unassociated_nodes);
          interrupt =
            tree->write(NULL,target,active_metagroup,i_param,addr_param);
        }
      if (interrupt == NULL)
        {
          active_metagroup = NULL; // Finished with this group; move on to next
          active_out.close();
        }
    } while (interrupt == NULL);

  return interrupt;
}


/* ========================================================================= */
/*                            jpx_meta_manager                               */
/* ========================================================================= */

/*****************************************************************************/
/*                      jpx_meta_manager::access_root                        */
/*****************************************************************************/

jpx_metanode
  jpx_meta_manager::access_root()
{
  jpx_metanode result;
  if (state != NULL)
    result = jpx_metanode(state->tree);
  return result;
}

/*****************************************************************************/
/*                      jpx_meta_manager::locate_node                        */
/*****************************************************************************/

jpx_metanode
  jpx_meta_manager::locate_node(kdu_long file_pos)
{
  jx_metaloc *metaloc = NULL;
  if (state != NULL)
    metaloc = state->metaloc_manager.get_locator(file_pos,false);
  return jpx_metanode((metaloc == NULL)?NULL:(metaloc->target));
}

/*****************************************************************************/
/*                   jpx_meta_manager::get_touched_nodes                     */
/*****************************************************************************/

jpx_metanode
  jpx_meta_manager::get_touched_nodes()
{
  jx_metanode *node=NULL;
  if ((state != NULL) && ((node=state->touched_head) != NULL))
    {
      assert(node->prev_touched == NULL);
      if ((state->touched_head = node->next_touched) == NULL)
        {
          assert(node == state->touched_tail);
          state->touched_tail = NULL;
        }
      else
        state->touched_head->prev_touched = NULL;
      node->next_touched = NULL;
    }
  return jpx_metanode(node);
}

/*****************************************************************************/
/*                  jpx_meta_manager::peek_touched_nodes                     */
/*****************************************************************************/

jpx_metanode
  jpx_meta_manager::peek_touched_nodes(kdu_uint32 box_type,
                                       jpx_metanode last_peeked)
{
  jx_metanode *node=NULL;
  if (state != NULL)
    {
      node = last_peeked.state;
      if (node == NULL)
        node = state->touched_head;
      else if ((node->prev_touched == NULL) && (node != state->touched_head))
        node = NULL;
      else
        node = node->next_touched;
      for (; node != NULL; node=node->next_touched)
        if ((box_type == 0) || (node->box_type == box_type))
          break;
    }
  return jpx_metanode(node);
}

/*****************************************************************************/
/*                         jpx_meta_manager::copy                            */
/*****************************************************************************/

void
  jpx_meta_manager::copy(jpx_meta_manager src)
{
  assert((state != NULL) && (src.state != NULL));
  int dummy_count;
  src.access_root().count_descendants(dummy_count); // Finish top level parsing
  jpx_metanode tree = access_root();
  jx_metanode *scan;
  for (scan=src.state->tree->head; scan != NULL; scan=scan->next_sibling)
    tree.add_copy(jpx_metanode(scan),true);
}

/*****************************************************************************/
/*                    jpx_meta_manager::set_box_filter                       */
/*****************************************************************************/

void
  jpx_meta_manager::set_box_filter(int num_types, kdu_uint32 *types)
{
  assert((state != NULL) && (state->filter_box_types != NULL));
  if (num_types > state->max_filter_box_types)
    {
      state->max_filter_box_types += num_types;
      delete[] state->filter_box_types;
      state->filter_box_types = NULL;
      state->filter_box_types = new kdu_uint32[state->max_filter_box_types];
      for (int n=0; n < num_types; n++)
        state->filter_box_types[n] = types[n];
      state->num_filter_box_types = num_types;
    }
}

/*****************************************************************************/
/*                     jpx_meta_manager::load_matches                        */
/*****************************************************************************/

bool
  jpx_meta_manager::load_matches(int num_codestreams,
                                 const int codestream_indices[],
                                 int num_compositing_layers,
                                 const int layer_indices[])
{
  jx_metanode *last_one = state->last_added_node;
  // Start by reading as many top-level boxes as possible
  while (state->source->parse_next_top_level_box());

  // Now finish parsing all relevant codestream and layer headers
  if (num_codestreams < 0)
    {
      num_codestreams = state->source->get_num_codestreams();
      codestream_indices = NULL;
    }
  if (num_compositing_layers < 0)
    {
      num_compositing_layers = state->source->get_num_compositing_layers();
      layer_indices = NULL;
    }
  int n;
  for (n=0; n < num_codestreams; n++)
    {
      int idx = (codestream_indices==NULL)?n:(codestream_indices[n]);
      jx_codestream_source *cs =
        state->source->get_codestream(idx);
      if (cs != NULL)
        cs->finish(false);
    }
  for (n=0; n < num_compositing_layers; n++)
    {
      int idx = (layer_indices==NULL)?n:(layer_indices[n]);
      jx_layer_source *ls =
        state->source->get_compositing_layer(idx);
      if (ls != NULL)
        ls->finish();
    }

  // Finally, recursively scan through the metadata, finishing as many
  // nodes as possible.
  state->tree->load_recursive();

  return (last_one != state->last_added_node);
}

/*****************************************************************************/
/*                   jpx_meta_manager::enumerate_matches                     */
/*****************************************************************************/

jpx_metanode
  jpx_meta_manager::enumerate_matches(jpx_metanode last_node,
                                      int codestream_idx,
                                      int compositing_layer_idx,
                                      bool applies_to_rendered_result,
                                      kdu_dims region, int min_size,
                                      bool exclude_region_numlists)
{
  if (state == NULL)
    return jpx_metanode(NULL);

  jx_metanode *node = last_node.state;
  if (!region.is_empty())
    { // Search in the ROI hierarchy
      int next_scale_idx = 0;
      jx_metagroup *mgrp=NULL;
      jx_roigroup *roig=NULL;
      kdu_coords loc; // Location within current ROI group object
      int offset=0; // loc.x + loc.y*JX_ROIGROUP_SIZE

      // min & lim give the range of ROI start coords which could be compatible
      kdu_coords lim = region.pos + region.size;
      kdu_coords min;
      if (node != NULL)
        { // Find out where we currently are and configure `min'
          mgrp = node->metagroup;
          roig = mgrp->roigroup;
          next_scale_idx = roig->scale_idx+1;
          offset = (int)(mgrp - roig->metagroups);
          assert((offset>=0) && (offset<(JX_ROIGROUP_SIZE*JX_ROIGROUP_SIZE)));
          loc.y = offset / JX_ROIGROUP_SIZE;
          loc.x = offset - loc.y*JX_ROIGROUP_SIZE;
          min = region.pos;
          min.x -= (JX_FINEST_SCALE_THRESHOLD<<roig->scale_idx)-1;
          min.y -= (JX_FINEST_SCALE_THRESHOLD<<roig->scale_idx)-1;
          node = node->next_link;
        }
      while ((roig != NULL) || (next_scale_idx < 32))
        {
          if (node == NULL)
            { // Need to advance to next list, group or scale
              if (roig == NULL)
                { // Advance to next scale
                  roig = state->roi_scales[next_scale_idx++];
                  if (roig == NULL)
                    continue;
                  min = region.pos;
                  min.x -= (JX_FINEST_SCALE_THRESHOLD<<roig->scale_idx)-1;
                  min.y -= (JX_FINEST_SCALE_THRESHOLD<<roig->scale_idx)-1;
                  loc = min - roig->group_dims.pos;
                  loc.x = (loc.x<0)?0:(loc.x/roig->elt_size.x);
                  loc.y = (loc.y<0)?0:(loc.y/roig->elt_size.y);
                  if ((loc.x>=JX_ROIGROUP_SIZE) || (loc.y>=JX_ROIGROUP_SIZE))
                    { // No intersection with this group
                      roig = NULL;
                      continue;
                    }
                }
              else if (((loc.x+1) < JX_ROIGROUP_SIZE) &&
                       (lim.x > (roig->group_dims.pos.x +
                                 (loc.x+1)*roig->elt_size.x)))
                loc.x++;
              else if (((loc.y+1) < JX_ROIGROUP_SIZE) &&
                       (lim.y > (roig->group_dims.pos.y +
                                 (loc.y+1)*roig->elt_size.y)))
                { 
                  loc.y++;
                  loc.x = min.x - roig->group_dims.pos.x;
                  loc.x = (loc.x < 0)?0:(loc.x / roig->elt_size.x);
                }
              else
                { // Go to group's parent
                  jx_roigroup *parent = roig->parent;
                  if (parent != NULL)
                    {
                      min = region.pos;
                      min.x-=(JX_FINEST_SCALE_THRESHOLD<<parent->scale_idx)-1;
                      min.y-=(JX_FINEST_SCALE_THRESHOLD<<parent->scale_idx)-1;
                      loc = roig->group_dims.pos - parent->group_dims.pos;
                      loc.x = loc.x / parent->elt_size.x;
                      loc.y = loc.y / parent->elt_size.y;
                      assert((loc.x >= 0) && (loc.x < JX_ROIGROUP_SIZE) &&
                             (loc.y >= 0) && (loc.y < JX_ROIGROUP_SIZE));
                    }
                  roig = parent;
                  continue; // Still need to advance within the parent group
                }

              // If we get here, we are in the next compatible metagroup or
              // roigroup at some level.  Descend back down to the metagroup
              // level, if possible.
              offset = loc.x + loc.y*JX_ROIGROUP_SIZE;
              while (roig->level > 0)
                {
                  if (roig->sub_groups[offset] == NULL)
                    break; // Need to go to next group at same/higher level
                  roig = roig->sub_groups[offset];
                  min = region.pos;
                  min.x -= (JX_FINEST_SCALE_THRESHOLD<<roig->scale_idx)-1;
                  min.y -= (JX_FINEST_SCALE_THRESHOLD<<roig->scale_idx)-1;
                  loc = min - roig->group_dims.pos;
                  loc.x = (loc.x<0)?0:(loc.x/roig->elt_size.x);
                  loc.y = (loc.y<0)?0:(loc.y/roig->elt_size.y);
                  assert((loc.x<JX_ROIGROUP_SIZE) && (loc.y<JX_ROIGROUP_SIZE));
                  offset = loc.x + loc.y*JX_ROIGROUP_SIZE;
                }
              if (roig->level > 0)
                continue; // Advance to next group/list at same/higher level
              mgrp = roig->metagroups + offset;
              node = mgrp->head;
              if (node == NULL)
                continue;
            }

          // If we get here, `node' is non-NULL.  See if it conforms to the
          // constraints or not.
          assert((node->rep_id == JX_ROI_NODE) && (node->regions != NULL));
          bool found_match = false;
          if ((node->regions->min_size >= min_size) &&
              node->regions->bounding_region.region.intersects(region))
            { // Found a spatial match; look for numlist match also
              if ((codestream_idx < 0) && (compositing_layer_idx < 0) &&
                  !applies_to_rendered_result)
                found_match = true;
              else
                {
                  jx_metanode *scan=node->parent;
                  while ((scan != NULL) &&
                         !((scan->rep_id == JX_NUMLIST_NODE) &&
                           (scan->numlist != NULL)))
                    scan = scan->parent;
                  if ((scan != NULL) &&
                      ((!applies_to_rendered_result) ||
                       scan->numlist->rendered_result))
                    {
                      int n;
                      found_match = true;
                      if (codestream_idx >= 0)
                        {
                          for (n=0; n < scan->numlist->num_codestreams; n++)
                            if (scan->numlist->codestream_indices[n] ==
                                codestream_idx)
                              break;
                          if (n == scan->numlist->num_codestreams)
                            found_match = false;
                        }
                      if (compositing_layer_idx >= 0)
                        {
                          for (n=0;
                               n < scan->numlist->num_compositing_layers; n++)
                            if (scan->numlist->layer_indices[n] ==
                                compositing_layer_idx)
                              break;
                          if (n == scan->numlist->num_compositing_layers)
                            found_match = false;
                        }
                    }
                }
            }
          if (found_match)
            return jpx_metanode(node);
          node = node->next_link;
        }
    }
  else if ((codestream_idx >= 0) || (compositing_layer_idx >= 0) ||
           applies_to_rendered_result)
    { // Search in numlist group
      while (1)
        {
          if (node == NULL)
            node = state->numlist_nodes.head;
          else
            {
              assert(node->metagroup == &state->numlist_nodes);
              node = node->next_link;
            }
          if (node == NULL)
            break;
          if ((node->rep_id != JX_NUMLIST_NODE) || (node->numlist == NULL))
            continue;
          if (exclude_region_numlists)
            {
              jx_metanode *dp;
              for (dp=node->head; dp != NULL; dp=dp->next_link)
                if (dp->rep_id != JX_ROI_NODE)
                  break;
              if (dp == NULL)
                continue; // Node contains no non-ROI descendants
            }
          if (applies_to_rendered_result && !node->numlist->rendered_result)
            continue; // Failed to match numlist
          int n;
          if (codestream_idx >= 0)
            {
              for (n=0; n < node->numlist->num_codestreams; n++)
                if (node->numlist->codestream_indices[n]==codestream_idx)
                  break;
              if (n == node->numlist->num_codestreams)
                continue; // No match found
            }
          if (compositing_layer_idx >= 0)
            {
              for (n=0; n < node->numlist->num_compositing_layers; n++)
                if (node->numlist->layer_indices[n]==compositing_layer_idx)
                  break;
              if (n == node->numlist->num_compositing_layers)
                continue; // No match found
            }
          return jpx_metanode(node); // Found a complete match
        }
    }
  else
    { // Search in unassociated group
      if (node == NULL)
        node = state->unassociated_nodes.head;
      else
        {
          assert(node->metagroup == &state->unassociated_nodes);
          node = node->next_link;
        }
    }

  return jpx_metanode(node);
}

/*****************************************************************************/
/*                      jpx_meta_manager::insert_node                        */
/*****************************************************************************/

jpx_metanode
  jpx_meta_manager::insert_node(int num_codestreams,
                                const int codestream_indices[],
                                int num_compositing_layers,
                                const int layer_indices[],
                                bool applies_to_rendered_result,
                                int num_regions, const jpx_roi *regions)
{
  assert(state != NULL);
  assert((num_codestreams >= 0) && (num_compositing_layers >= 0));
  if ((num_regions > 0) && (num_codestreams == 0))
    { KDU_ERROR_DEV(e,30); e <<
        KDU_TXT("You may not use `jpx_meta_manager::insert_node' to "
        "insert a region-specific metadata node which is not also associated "
        "with at least one codestream.  The reason is that ROI description "
        "boxes have meaning only when associated with codestreams, as "
        "described in the JPX standard.");
    }
  if ((num_codestreams == 0) && (num_compositing_layers == 0) &&
      !applies_to_rendered_result)
    return jpx_metanode(state->tree);

  int n, k, idx;
  jx_metanode *scan;
  for (scan=state->tree->head; scan != NULL; scan=scan->next_sibling)
    {
      if (!((scan->flags & JX_METANODE_BOX_COMPLETE) &&
            (scan->rep_id == JX_NUMLIST_NODE)))
        continue;
      if (applies_to_rendered_result != scan->numlist->rendered_result)
        continue;

      // Check that `codestream_indices' are all in the numlist node
      for (n=0; n < num_codestreams; n++)
        {
          idx = codestream_indices[n];
          for (k=0; k < scan->numlist->num_codestreams; k++)
            if (scan->numlist->codestream_indices[k] == idx)
              break;
          if (k == scan->numlist->num_codestreams)
            break; // No match
        }
      if (n < num_codestreams)
        continue;

      // Check that numlist node's codestreams are all in `codestrem_indices'
      for (n=0; n < scan->numlist->num_codestreams; n++)
        {
          idx = scan->numlist->codestream_indices[n];
          for (k=0; k < num_codestreams; k++)
            if (codestream_indices[k] == idx)
              break;
          if (k == num_codestreams)
            break; // No match
        }
      if (n < scan->numlist->num_codestreams)
        continue;

      // Check that `layer_indices' are all in the numlist node
      for (n=0; n < num_compositing_layers; n++)
        {
          idx = layer_indices[n];
          for (k=0; k < scan->numlist->num_compositing_layers; k++)
            if (scan->numlist->layer_indices[k] == idx)
              break;
          if (k == scan->numlist->num_compositing_layers)
            break; // No match
        }
      if (n < num_compositing_layers)
        continue;

      // Check that the numlist node's layers are all in `layer_indices'
      for (n=0; n < scan->numlist->num_compositing_layers; n++)
        {
          idx = scan->numlist->layer_indices[n];
          for (k=0; k < num_compositing_layers; k++)
            if (layer_indices[k] == idx)
              break;
          if (k == num_compositing_layers)
            break; // No match
        }
      if (n < scan->numlist->num_compositing_layers)
        continue;

      break; // We have found a compatible number list
    }

  jpx_metanode result(scan);
  if (scan == NULL)
    result = access_root().add_numlist(num_codestreams,codestream_indices,
                                       num_compositing_layers,layer_indices,
                                       applies_to_rendered_result);
  if (num_regions == 0)
    return result;

  return result.add_regions(num_regions,regions);
}


/* ========================================================================= */
/*                              jx_registration                              */
/* ========================================================================= */

/*****************************************************************************/
/*                           jx_registration::init                           */
/*****************************************************************************/

void
  jx_registration::init(jp2_input_box *creg)
{
  if (codestreams != NULL)
    { KDU_ERROR(e,31); e <<
        KDU_TXT("JPX data source appears to contain multiple "
        "JPX Codestream Registration (creg) boxes within the same "
        "compositing layer header (jplh) box.");
    }
  final_layer_size.x = final_layer_size.y = 0; // To be calculated later
  kdu_uint16 xs, ys;
  if (!(creg->read(xs) && creg->read(ys) && (xs > 0) && (ys > 0)))
    { KDU_ERROR(e,32); e <<
        KDU_TXT("Malformed Codestream Registration (creg) box found "
        "in JPX data source.  Insufficient or illegal fields encountered.");
    }
  denominator.x = (int) xs;
  denominator.y = (int) ys;
  int box_bytes = (int) creg->get_remaining_bytes();
  if ((box_bytes % 6) != 0)
    { KDU_ERROR(e,33); e <<
        KDU_TXT("Malformed Codestream Registration (creg) box found "
        "in JPX data source.  Box size does not seem to be equal to 4+6k "
        "where k must be the number of referenced codestreams.");
    }
  num_codestreams = max_codestreams = (box_bytes / 6);
  codestreams = new jx_layer_stream[max_codestreams];
  for (int c=0; c < num_codestreams; c++)
    {
      jx_layer_stream *str = codestreams + c;
      kdu_uint16 cdn;
      kdu_byte xr, yr, xo, yo;
      if (!(creg->read(cdn) && creg->read(xr) && creg->read(yr) &&
            creg->read(xo) && creg->read(yo)))
        assert(0); // Should not be possible
      if ((xr == 0) || (yr == 0))
        { KDU_ERROR(e,34); e <<
            KDU_TXT("Malformed Codestream Registration (creg) box "
            "found in JPX data source.  Illegal (zero-valued) resolution "
            "parameters found for codestream " << cdn << ".");
        }
      if ((denominator.x <= (int) xo) || (denominator.y <= (int) yo))
        { KDU_ERROR(e,35); e <<
            KDU_TXT("Malformed Codestream Registration (creg) box "
            "found in JPX data source.  Alignment offsets must be strictly "
            "less than the denominator (point density) parameters, as "
            "explained in the JPX standard (accounting for corrigenda).");
        }
      str->codestream_id = (int) cdn;
      str->sampling.x = xr;
      str->sampling.y = yr;
      str->alignment.x = xo;
      str->alignment.y = yo;
    }
  creg->close();
}

/*****************************************************************************/
/*                     jx_registration::finalize (reading)                   */
/*****************************************************************************/

void
  jx_registration::finalize(int channel_id)
{
  if (codestreams == NULL)
    { // Create default registration info
      num_codestreams = max_codestreams = 1;
      codestreams = new jx_layer_stream[1];
      codestreams->codestream_id = channel_id;
      codestreams->sampling = kdu_coords(1,1);
      codestreams->alignment = kdu_coords(0,0);
      denominator = kdu_coords(1,1);
    }
}

/*****************************************************************************/
/*                     jx_registration::finalize (writing)                   */
/*****************************************************************************/

void
  jx_registration::finalize(j2_channels *channels)
{
  if ((denominator.x == 0) || (denominator.y == 0))
    denominator = kdu_coords(1,1);
  int n, k, c, cid;
  for (k=0; k < channels->num_colours; k++)
    {
      j2_channels::j2_channel *cp = channels->channels + k;
      for (c=0; c < 3; c++)
        {
          cid = cp->codestream_idx[c];
          if (cid < 0)
            continue;
          for (n=0; n < num_codestreams; n++)
            if (codestreams[n].codestream_id == cid)
              break;
          if (n == num_codestreams)
            { // Add a new one
              if (n >= max_codestreams)
                { // Allocate new array
                  max_codestreams += max_codestreams + 2;
                  jx_layer_stream *buf = new jx_layer_stream[max_codestreams];
                  for (n=0; n < num_codestreams; n++)
                    buf[n] = codestreams[n];
                  if (codestreams != NULL)
                    delete[] codestreams;
                  codestreams = buf;
                }
              num_codestreams++;
              codestreams[n].codestream_id = cid;
              codestreams[n].sampling = denominator;
              codestreams[n].alignment = kdu_coords(0,0);
            }
        }
    }
}

/*****************************************************************************/
/*                        jx_registration::save_boxes                        */
/*****************************************************************************/

void
  jx_registration::save_box(jp2_output_box *super_box)
{
  assert(num_codestreams > 0);
  int n;
  jx_layer_stream *str;

  jp2_output_box creg;
  creg.open(super_box,jp2_registration_4cc);
  creg.write((kdu_uint16) denominator.x);
  creg.write((kdu_uint16) denominator.y);
  for (n=0; n < num_codestreams; n++)
    {
      str = codestreams + n;
      int cdn = str->codestream_id;
      int xr = str->sampling.x;
      int yr = str->sampling.y;
      int xo = str->alignment.x;
      int yo = str->alignment.y;
      if ((cdn < 0) || (cdn >= (1<<16)) || (xr <= 0) || (yr <= 0) ||
          (xr > 255) || (yr > 255) || (xo < 0) || (yo < 0) ||
          (xo >= denominator.x) || (yo >= denominator.y))
        { KDU_ERROR_DEV(e,36); e <<
            KDU_TXT("One or more codestreams registration parameters "
            "supplied using `jpx_layer_target::set_codestream_registration' "
            "cannot be recorded in a legal JPX Codestream Registration (creg) "
            "box.");
        }
      if (xo > 255)
        xo = 255;
      if (yo > 255)
        yo = 255;
      creg.write((kdu_uint16) cdn);
      creg.write((kdu_byte) xr);
      creg.write((kdu_byte) yr);
      creg.write((kdu_byte) xo);
      creg.write((kdu_byte) yo);
    }
  creg.close();
}


/* ========================================================================= */
/*                            jx_codestream_source                           */
/* ========================================================================= */

/*****************************************************************************/
/*                    jx_codestream_source::donate_chdr_box                  */
/*****************************************************************************/

void
  jx_codestream_source::donate_chdr_box(jp2_input_box &src)
{
  if (restrict_to_jp2)
    { src.close(); return; }
  assert((!header_loc) && (!chdr) && (!sub_box));
  chdr.transplant(src);
  header_loc = chdr.get_locator();
  finish(false);
}

/*****************************************************************************/
/*                 jx_codestream_source::donate_codestream_box               */
/*****************************************************************************/

void
  jx_codestream_source::donate_codestream_box(jp2_input_box &src)
{
  assert(!stream_loc);
  stream_box.transplant(src);
  stream_loc = stream_box.get_locator();
  if ((stream_box.get_box_type() == jp2_fragment_table_4cc) &&
      !parse_fragment_list())
    return;
  stream_main_header_found =
    (!stream_box.has_caching_source()) || stream_box.set_codestream_scope(0);
}

/*****************************************************************************/
/*                  jx_codestream_source::parse_fragment_list                */
/*****************************************************************************/

bool
  jx_codestream_source::parse_fragment_list()
{
  if (fragment_list != NULL)
    return true;
  assert(stream_box.get_box_type() == jp2_fragment_table_4cc);
  if (chdr.exists())
    return false; // Can't re-deploy `sub_box' until codestream header parsed
  if (!stream_box.is_complete())
    return false;
  while (sub_box.exists() || sub_box.open(&stream_box))
    {
      if (sub_box.get_box_type() != jp2_fragment_list_4cc)
        { // Skip over irrelevant sub-box
          sub_box.close();
          continue;
        }
      if (!sub_box.is_complete())
        return false;
      fragment_list = new jx_fragment_list;
      fragment_list->init(&sub_box);
      sub_box.close();
      stream_box.close();
      stream_box.open_as(jpx_fragment_list(fragment_list),
                         owner->get_data_references(),
                         ultimate_src,jp2_codestream_4cc);
      return true;
    }
  return false; 
}

/*****************************************************************************/
/*                        jx_codestream_source::finish                       */
/*****************************************************************************/

bool
  jx_codestream_source::finish(bool need_embedded_codestream)
{
  if (metadata_finished &&
      !(need_embedded_codestream && stream_loc.is_null()))
    return true;

  // Parse top level boxes as required
  while (((!header_loc) || (need_embedded_codestream && !stream_loc)) &&
         (!restrict_to_jp2) && !owner->is_top_level_complete())
    if (!owner->parse_next_top_level_box())
      break;

  if (chdr.exists())
    { // Parse as much as we can of the codestream header box
      bool chdr_complete = chdr.is_complete();
      while (sub_box.exists() || sub_box.open(&chdr))
        {
          bool sub_complete = sub_box.is_complete();
          if (sub_box.get_box_type() == jp2_image_header_4cc)
            {
              if (!sub_complete)
                return false;
              dimensions.init(&sub_box);
            }
          else if (sub_box.get_box_type() == jp2_bits_per_component_4cc)
            {
              if (!sub_complete)
                return false;
              dimensions.process_bpcc_box(&sub_box);
              have_bpcc = true;
            }
          else if (sub_box.get_box_type() == jp2_palette_4cc)
            {
              if (!sub_complete)
                return false;
              palette.init(&sub_box);
            }
          else if (sub_box.get_box_type() == jp2_component_mapping_4cc)
            {
              if (!sub_complete)
                return false;
              component_map.init(&sub_box);
            }
          else if (owner->test_box_filter(sub_box.get_box_type()))
            { // Put into the metadata management system
              if (metanode == NULL)
                {
                  jx_metanode *tree = owner->get_metatree();
                  metanode = tree->add_numlist(1,&id,0,NULL,false);
                  metanode->flags |= JX_METANODE_EXISTING;
                  metanode->read_state = new jx_metaread;
                  metanode->read_state->codestream_source = this;
                }
              metanode->add_descendant()->donate_input_box(sub_box);
              assert(!sub_box.exists());
            }
          else
            sub_box.close(); // Skip it
        }

      // See if we have enough of the codestream header to finish parsing it
      if (chdr_complete)
        {
          chdr.close();
          if (metanode != NULL)
            {
              metanode->flags |= JX_METANODE_DESCENDANTS_KNOWN;
              metanode->update_completed_descendants();
              delete metanode->read_state;
              metanode->read_state = NULL;
            }
        }
      else if (dimensions.is_initialized() && have_bpcc &&
               palette.is_initialized() && component_map.is_initialized())
        { // Donate `chdr' to the metadata management machinery to extract
          // any remaining boxes if it can.
          if (metanode == NULL)
            {
              jx_metanode *tree = owner->get_metatree();
              metanode = tree->add_numlist(1,&id,0,NULL,false);
              metanode->flags |= JX_METANODE_EXISTING;
              metanode->read_state = new jx_metaread;
            }
          metanode->read_state->codestream_source = NULL;
          metanode->read_state->asoc.transplant(chdr);
          assert(!chdr);
        }
    }

  if (chdr.exists() ||
      ((!restrict_to_jp2) && (!header_loc) && !owner->is_top_level_complete()))
    return false; // Still waiting for the codestream header box

  if (!metadata_finished)
    {
      if (!dimensions.is_initialized())
        { // Dimensions information must be in default header box
          j2_dimensions *dflt = owner->get_default_dimensions();
          if (dflt == NULL)
            return false;
          if (!dflt->is_initialized())
            { KDU_ERROR(e,37); e <<
                KDU_TXT("JPX source contains no image header box for "
                "a codestream.  The image header (ihdr) box cannot be found in "
                "a codestream header (chdr) box, and does not exist within a "
                "default JP2 header (jp2h) box.");
            }
          dimensions.copy(dflt);
        }

      if (!palette.is_initialized())
        { // Palette info might be in default header box
          j2_palette *dflt = owner->get_default_palette();
          if (dflt == NULL)
            return false;
          if (dflt->is_initialized())
            palette.copy(dflt);
        }

      if (palette.is_initialized() && !component_map.is_initialized())
        { // Component mapping info must be in default header box
          j2_component_map *dflt = owner->get_default_component_map();
          if (dflt == NULL)
            return false;
          if (!dflt->is_initialized())
            { KDU_ERROR(e,38); e <<
                KDU_TXT("JPX source contains a codestream with a palette "
                "(pclr) box, but no component mapping (cmap) box.  This "
                "illegal situation has been detected after examining both the "
                "codestream header (chdr) box, if any, for that codestream, "
                "and the default JP2 header (jp2h) box, if any.");
            }
          component_map.copy(dflt);
        }

      dimensions.finalize();
      palette.finalize();
      component_map.finalize(&dimensions,&palette);
      metadata_finished = true;
    }

  if (need_embedded_codestream && !stream_loc)
    return false;
  return true;
}


/* ========================================================================= */
/*                           jpx_codestream_source                           */
/* ========================================================================= */

/*****************************************************************************/
/*                  jpx_codestream_source::get_codestream_id                 */
/*****************************************************************************/

int
  jpx_codestream_source::get_codestream_id()
{
  assert((state != NULL) && state->metadata_finished);
  return state->id;
}

/*****************************************************************************/
/*                   jpx_codestream_source::get_header_loc                   */
/*****************************************************************************/

jp2_locator
  jpx_codestream_source::get_header_loc()
{
  assert((state != NULL) && state->metadata_finished);
  return state->header_loc;
}

/*****************************************************************************/
/*                jpx_codestream_source::access_fragment_list                */
/*****************************************************************************/

jpx_fragment_list
  jpx_codestream_source::access_fragment_list()
{
  assert(state != NULL);
  if (state->is_stream_ready() && (state->fragment_list != NULL))
    return jpx_fragment_list(state->fragment_list);
  return jpx_fragment_list(NULL);
}

/*****************************************************************************/
/*                 jpx_codestream_source::access_dimensions                  */
/*****************************************************************************/

jp2_dimensions
  jpx_codestream_source::access_dimensions(bool finalize_compatibility)
{
  assert((state != NULL) && state->metadata_finished);
  jp2_dimensions result(&state->dimensions);
  if (finalize_compatibility && (!state->compatibility_finalized))
    {
      jpx_input_box *src = open_stream();
      if (src != NULL)
        {
          kdu_codestream cs;
          try {
              cs.create(src);
              result.finalize_compatibility(cs.access_siz());
            }
          catch (int)
            { }
          if (cs.exists())
            cs.destroy();
          src->close();
          state->compatibility_finalized = true;
        }
    }
  return result;
}

/*****************************************************************************/
/*                   jpx_codestream_source::access_palette                   */
/*****************************************************************************/

jp2_palette
  jpx_codestream_source::access_palette()
{
  assert((state != NULL) && state->metadata_finished);
  return jp2_palette(&state->palette);
}

/*****************************************************************************/
/*                   jpx_codestream_source::get_stream_loc                   */
/*****************************************************************************/

jp2_locator
  jpx_codestream_source::get_stream_loc()
{
  assert((state != NULL) && state->metadata_finished);
  return state->stream_loc;
}

/*****************************************************************************/
/*                    jpx_codestream_source::stream_ready                    */
/*****************************************************************************/

bool
  jpx_codestream_source::stream_ready()
{
  assert(state != NULL);
  return state->is_stream_ready();
}

/*****************************************************************************/
/*                    jpx_codestream_source::open_stream                     */
/*****************************************************************************/

jpx_input_box *
  jpx_codestream_source::open_stream(jpx_input_box *app_box)
{
  assert(state != NULL);
  if (!state->is_stream_ready())
    return NULL;
  if (app_box == NULL)
    {
      if (!state->stream_opened)
        {
          assert(state->stream_box.exists());
          state->stream_opened = true;
          return &state->stream_box;
        }
      if (state->stream_box.exists())
        { KDU_ERROR_DEV(e,39); e <<
            KDU_TXT("Attempting to use `jpx_codestream_source::open_stream' a "
            "second time, to gain access to the same codestream, without "
            "first closing the box.  To maintain multiple open instances "
            "of the same codestream, you should supply your own "
            "`jpx_input_box' object, rather than attempting to use the "
            "internal resource multiple times concurrently.");
        }
      app_box = &(state->stream_box);
    }
  if (state->fragment_list != NULL)
    app_box->open_as(jpx_fragment_list(state->fragment_list),
                     state->owner->get_data_references(),
                     state->ultimate_src,jp2_codestream_4cc);
  else
    app_box->open(state->ultimate_src,state->stream_loc);
  return app_box;
}


/* ========================================================================= */
/*                              jx_layer_source                              */
/* ========================================================================= */

/*****************************************************************************/
/*                      jx_layer_source::donate_jplh_box                     */
/*****************************************************************************/

void
  jx_layer_source::donate_jplh_box(jp2_input_box &src)
{
  assert((!header_loc) && (!jplh) && (!sub_box));
  jplh.transplant(src);
  header_loc = jplh.get_locator();
  finish();
}

/*****************************************************************************/
/*                          jx_layer_source::finish                          */
/*****************************************************************************/

bool
  jx_layer_source::finish()
{
  if (finished)
    return true;

  // Parse top level boxes as required
  while ((!header_loc) && !owner->are_layer_header_boxes_complete())
    if (!owner->parse_next_top_level_box())
      break;

  j2_colour *cscan;

  if (jplh.exists())
    { // Parse as much as we can of the compositing layer header box
      bool jplh_complete = jplh.is_complete();
      jp2_input_box *parent_box = (cgrp.exists())?(&cgrp):(&jplh);
      while (sub_box.exists() || sub_box.open(parent_box) ||
             ((parent_box==&cgrp) && cgrp.is_complete() &&
              cgrp.close() && sub_box.open(parent_box=&jplh)))
        {
          if (sub_box.get_box_type() == jp2_colour_group_4cc)
            {
              if (cgrp.exists())
                { // Ignore nested colour group box
                  sub_box.close(); continue;
                }
              cgrp.transplant(sub_box);
              parent_box = &cgrp;
              continue;
            }
          bool sub_complete = sub_box.is_complete();
          if (sub_box.get_box_type() == jp2_colour_4cc)
            {
              if (!sub_complete)
                return false;
              if (!cgrp.exists())
                { // Colour description boxes must be inside a colour group box
                  KDU_WARNING_DEV(w,1); w <<
                    KDU_TXT("Colour description (colr) box found "
                    "inside a compositing layer header (jplh) box, but not "
                    "wrapped by a colour group (cgrp) box.  This is "
                    "technically a violation of the JPX standard, but we will "
                    "parse the box anyway.");
                }
              for (cscan=&colour; cscan->next != NULL; cscan=cscan->next);
              if (cscan->is_initialized())
                cscan = cscan->next = new j2_colour;
              cscan->init(&sub_box);
            }
          else if ((sub_box.get_box_type() == jp2_channel_definition_4cc) ||
                   (sub_box.get_box_type() == jp2_opacity_4cc))
            {
              if (!sub_complete)
                return false;
              channels.init(&sub_box);
            }
          else if (sub_box.get_box_type() == jp2_resolution_4cc)
            {
              if (!sub_complete)
                return false;
              resolution.init(&sub_box);
            }
          else if (sub_box.get_box_type() == jp2_registration_4cc)
            {
              if (!sub_complete)
                return false;
              registration.init(&sub_box);
            }
          else if (owner->test_box_filter(sub_box.get_box_type()))
            { // Put into the metadata management system
              if (metanode == NULL)
                {
                  jx_metanode *tree = owner->get_metatree();
                  metanode = tree->add_numlist(0,NULL,1,&id,false);
                  metanode->flags |= JX_METANODE_EXISTING;
                  metanode->read_state = new jx_metaread;
                  metanode->read_state->layer_source = this;
                }
              metanode->add_descendant()->donate_input_box(sub_box);
              assert(!sub_box.exists());
            }
          else
            sub_box.close(); // Discard it
        }

      // See if we have enough of the layer header to finish parsing it
      if (jplh_complete)
        {
          jplh.close();
          if (metanode != NULL)
            {
              metanode->flags |= JX_METANODE_DESCENDANTS_KNOWN;
              metanode->update_completed_descendants();
              delete metanode->read_state;
              metanode->read_state = NULL;
            }
        }
      else if (colour.is_initialized() && (!cgrp) &&
               channels.is_initialized() && resolution.is_initialized() &&
               registration.is_initialized())
        { // Donate `jplh' to the metadata management machinery to extract
          // any remaining boxes if it can.
          if (metanode == NULL)
            {
              jx_metanode *tree = owner->get_metatree();
              metanode = tree->add_numlist(0,NULL,1,&id,false);
              metanode->flags |= JX_METANODE_EXISTING;
              metanode->read_state = new jx_metaread;
            }
          metanode->read_state->layer_source = NULL;
          metanode->read_state->asoc.transplant(jplh);
          assert(!jplh);
        }
    }

  if (jplh.exists() ||
      ((!header_loc) && !owner->are_layer_header_boxes_complete()))
    return false; // Still waiting for the compositing layer header box

  // Check that all required codestreams are available
  registration.finalize(id); // Safe to do this multiple times
  int cs_id, cs_which;
  for (cs_which=0; cs_which < registration.num_codestreams; cs_which++)
    {
      cs_id = registration.codestreams[cs_which].codestream_id;
      jx_codestream_source *cs = owner->get_codestream(cs_id);
      if ((cs == NULL) && owner->is_top_level_complete())
        { KDU_ERROR(e,40); e <<
            KDU_TXT("Encountered a JPX compositing layer box which "
            "utilizes a non-existent codestream!");
        }
      if ((cs == NULL) || !cs->finish(true))
        return false;
    }

  // See if we need to evaluate the layer dimensions
  if ((registration.final_layer_size.x == 0) ||
      (registration.final_layer_size.y == 0))
    {
      registration.final_layer_size = kdu_coords(0,0);
      for (cs_which=0; cs_which < registration.num_codestreams; cs_which++)
        {
          cs_id = registration.codestreams[cs_which].codestream_id;
          jx_codestream_source *cs = owner->get_codestream(cs_id);
          jpx_codestream_source cs_ifc(cs);
          kdu_coords size = cs_ifc.access_dimensions().get_size();
          size.x *= registration.codestreams[cs_which].sampling.x;
          size.y *= registration.codestreams[cs_which].sampling.y;
          size += registration.codestreams[cs_which].alignment;
          if ((cs_which == 0) || (size.x < registration.final_layer_size.x))
            registration.final_layer_size.x = size.x;
          if ((cs_which == 0) || (size.y < registration.final_layer_size.y))
            registration.final_layer_size.y = size.y;
        }
      registration.final_layer_size.x =
        ceil_ratio(registration.final_layer_size.x,registration.denominator.x);
      registration.final_layer_size.y =
        ceil_ratio(registration.final_layer_size.y,registration.denominator.y);
    }

  // Collect defaults, as required
  if (!colour.is_initialized())
    { // Colour description must be in the default header box
      j2_colour *dflt = owner->get_default_colour();
      if (dflt == NULL)
        return false;
      for (cscan=&colour;
           (dflt != NULL) && dflt->is_initialized();
           dflt=dflt->next)
        {
          if (cscan->is_initialized())
            cscan = cscan->next = new j2_colour;
          cscan->copy(dflt);
        }
    }

  if (!channels.is_initialized())
    { // Channel defs information might be in default header box
      j2_channels *dflt = owner->get_default_channels();
      if (dflt == NULL)
        return false;
      if (dflt->is_initialized())
        channels.copy(dflt);
    }
  
  if (!resolution.is_initialized())
    { // Resolution information might be in default header box
      j2_resolution *dflt = owner->get_default_resolution();
      if (dflt == NULL)
        return false;
      if (dflt->is_initialized())
        resolution.copy(dflt);
    }

  // Finalize boxes
  int num_colours = 0;
  for (cscan=&colour; (cscan != NULL) && (num_colours == 0); cscan=cscan->next)
    num_colours = cscan->get_num_colours();
  channels.finalize(num_colours,false);
  for (cs_which=0; cs_which < registration.num_codestreams; cs_which++)
    {
      cs_id = registration.codestreams[cs_which].codestream_id;
      jx_codestream_source *cs = owner->get_codestream(cs_id);
      assert(cs != NULL);
      channels.find_cmap_channels(cs->get_component_map(),cs_id);
    }
  if (!channels.all_cmap_channels_found())
    { KDU_ERROR(e,41); e <<
        KDU_TXT("JP2/JPX source is internally inconsistent.  "
        "Either an explicit channel mapping box, or the set of channels "
        "implicitly identified by a colour space box, cannot all be "
        "associated with available code-stream image components.");
    }
  for (cscan=&colour; cscan != NULL; cscan=cscan->next)
    cscan->finalize(&channels);
  finished = true;
  return true;
}

/*****************************************************************************/
/*                    jx_layer_source::check_stream_headers                  */
/*****************************************************************************/

bool
  jx_layer_source::check_stream_headers()
{
  if (!finished)
    return false;
  int cs_id, cs_which;
  for (cs_which=0; cs_which < registration.num_codestreams; cs_which++)
    {
      cs_id = registration.codestreams[cs_which].codestream_id;
      jx_codestream_source *cs = owner->get_codestream(cs_id);
      assert(cs != NULL);
      if (!cs->is_stream_ready())
        return false;
    }
  stream_headers_available = true;
  return true;
}


/* ========================================================================= */
/*                             jpx_layer_source                              */
/* ========================================================================= */

/*****************************************************************************/
/*                       jpx_layer_source::get_layer_id                      */
/*****************************************************************************/

int
  jpx_layer_source::get_layer_id()
{
  assert((state != NULL) && state->finished);
  return state->id;
}

/*****************************************************************************/
/*                      jpx_layer_source::get_header_loc                     */
/*****************************************************************************/

jp2_locator
  jpx_layer_source::get_header_loc()
{
  assert((state != NULL) && state->finished);
  return state->header_loc;
}

/*****************************************************************************/
/*                     jpx_layer_source::access_channels                     */
/*****************************************************************************/

jp2_channels
  jpx_layer_source::access_channels()
{
  assert((state != NULL) && state->finished);
  return jp2_channels(&state->channels);
}

/*****************************************************************************/
/*                    jpx_layer_source::access_resolution                    */
/*****************************************************************************/

jp2_resolution
  jpx_layer_source::access_resolution()
{
  assert((state != NULL) && state->finished);
  return jp2_resolution(&state->resolution);
}

/*****************************************************************************/
/*                      jpx_layer_source::access_colour                      */
/*****************************************************************************/

jp2_colour
  jpx_layer_source::access_colour(int which)
{
  assert((state != NULL) && state->finished);
  j2_colour *scan=&state->colour;
  for (; (which > 0) && (scan != NULL); which--)
    scan = scan->next;
  return jp2_colour(scan);
}

/*****************************************************************************/
/*                   jpx_layer_source::get_num_codestreams                   */
/*****************************************************************************/

int
  jpx_layer_source::get_num_codestreams()
{
  assert((state != NULL) && state->finished);
  return state->registration.num_codestreams;
}

/*****************************************************************************/
/*                    jpx_layer_source::get_codestream_id                    */
/*****************************************************************************/

int
  jpx_layer_source::get_codestream_id(int which)
{
  assert((state != NULL) && state->finished);
  if ((which < 0) || (which >= state->registration.num_codestreams))
    return -1;
  return state->registration.codestreams[which].codestream_id;
}

/*****************************************************************************/
/*                   jpx_layer_source::have_stream_headers                   */
/*****************************************************************************/

bool
  jpx_layer_source::have_stream_headers()
{
  assert(state != NULL);
  return state->all_streams_ready();
}

/*****************************************************************************/
/*                    jpx_layer_source::get_layer_size                       */
/*****************************************************************************/

kdu_coords
  jpx_layer_source::get_layer_size()
{
  assert((state != NULL) && state->finished);
  return state->registration.final_layer_size;
}

/*****************************************************************************/
/*               jpx_layer_source::get_codestream_registration               */
/*****************************************************************************/

int
  jpx_layer_source::get_codestream_registration(int which,
                                                kdu_coords &alignment,
                                                kdu_coords &sampling,
                                                kdu_coords &denominator)
{
  assert((state != NULL) && state->finished);
  denominator = state->registration.denominator;
  if ((which < 0) || (which >= state->registration.num_codestreams))
    return -1;
  alignment = state->registration.codestreams[which].alignment;
  sampling = state->registration.codestreams[which].sampling;
  return state->registration.codestreams[which].codestream_id;
}


/* ========================================================================= */
/*                                 jx_source                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                            jx_source::jx_source                           */
/*****************************************************************************/

jx_source::jx_source(jp2_family_src *src)
{
  ultimate_src = src;
  ultimate_src_id = src->get_id();
  have_signature = false;
  have_file_type = false;
  have_reader_requirements = false;
  is_completely_open = false;
  restrict_to_jp2 = false;
  in_parse_next_top_level_box = false;
  num_codestreams = 0;
  num_layers = 0;
  top_level_complete = false;
  jp2h_box_found = jp2h_box_complete = false;
  have_dtbl_box = false;
  codestreams = NULL;
  layers = NULL;
  last_chdr = NULL;
  last_jp2c_or_ftbl = NULL;
  last_jplh = NULL;
  meta_manager.source = this;
  meta_manager.ultimate_src = src;
  meta_manager.tree = new jx_metanode(&meta_manager);
  meta_manager.tree->flags |= JX_METANODE_EXISTING;
}

/*****************************************************************************/
/*                           jx_source::~jx_source                           */
/*****************************************************************************/

jx_source::~jx_source()
{
  jx_codestream_source *cp;
  jx_layer_source *lp;
  j2_colour *cscan;

  while ((cp=codestreams) != NULL)
    {
      codestreams = cp->next;
      delete cp;
    }
  while ((lp=layers) != NULL)
    {
      layers = lp->next;
      delete lp;
    }
  while ((cscan=default_colour.next) != NULL)
    { default_colour.next = cscan->next; delete cscan; }
}

/*****************************************************************************/
/*                     jx_source::parse_next_top_level_box                   */
/*****************************************************************************/

bool
  jx_source::parse_next_top_level_box(bool already_open)
{
  if (top_level_complete || in_parse_next_top_level_box)
    {
      assert(!already_open);
      return false;
    }
  if (!already_open)
    {
      assert(!top_box);
      if (!top_box.open_next())
        {
          if (!ultimate_src->is_top_level_complete())
            return false;

          if (!top_box.open_next()) // Try again, to avoid race conditions
            {
              top_level_complete = true;

              if (num_layers == 0)
                { // We encountered no compositing layer header boxes, so there
                  // must be one layer per codestream
                  while (num_layers < num_codestreams)
                    {
                      if (last_jplh == NULL)
                        last_jplh = layers =
                          new jx_layer_source(this,num_layers);
                      else
                        last_jplh = last_jplh->next =
                          new jx_layer_source(this,num_layers);
                      num_layers++;
                    }
                }

              return false;
            }
        }
    }

  in_parse_next_top_level_box = true;
  if (top_box.get_box_type() == jp2_dtbl_4cc)
    {
      if (have_dtbl_box)
        { KDU_ERROR(e,42); e <<
            KDU_TXT("JP2-family data source appears to contain "
            "more than one data reference (dtbl) box.  At most one should be "
            "found in the file.");
        }
      have_dtbl_box = true;
      dtbl_box.transplant(top_box);
      if (dtbl_box.is_complete())
        {
          data_references.init(&dtbl_box);
          assert(!dtbl_box);
        }
    }
  else if (top_box.get_box_type() == jp2_header_4cc)
    {
      if (jp2h_box_found)
        { KDU_ERROR(e,43); e <<
            KDU_TXT("JP2-family data source contains more than one "
            "top-level JP2 header (jp2h) box.");
        }
      jp2h_box_found = true;
      jp2h_box.transplant(top_box);
      finish_jp2_header_box();
    }
  else if ((top_box.get_box_type() == jp2_codestream_4cc) ||
           (top_box.get_box_type() == jp2_fragment_table_4cc))
    {
      if ((codestreams == NULL) ||
          ((last_jp2c_or_ftbl != NULL) && (last_jp2c_or_ftbl->next == NULL)))
        { // We have found a new codestream
          if (codestreams == NULL)
            last_jp2c_or_ftbl = codestreams =
              new jx_codestream_source(this,ultimate_src,num_codestreams,
                                       restrict_to_jp2);
          else
            last_jp2c_or_ftbl = last_jp2c_or_ftbl->next =
              new jx_codestream_source(this,ultimate_src,num_codestreams,
                                       restrict_to_jp2);
          num_codestreams++;
        }
      else if (last_jp2c_or_ftbl == NULL)
        last_jp2c_or_ftbl = codestreams;
      else
        last_jp2c_or_ftbl = last_jp2c_or_ftbl->next;
      last_jp2c_or_ftbl->donate_codestream_box(top_box);
      assert(!top_box);
    }
  else if (top_box.get_box_type() == jp2_codestream_header_4cc)
    {
      if ((codestreams == NULL) ||
          ((last_chdr != NULL) && (last_chdr->next == NULL)))
        { // We have found a new codestream
          if (codestreams == NULL)
            last_chdr = codestreams =
              new jx_codestream_source(this,ultimate_src,num_codestreams,
                                       restrict_to_jp2);
          else
            last_chdr = last_chdr->next =
              new jx_codestream_source(this,ultimate_src,num_codestreams,
                                       restrict_to_jp2);
          num_codestreams++;
        }
      else if (last_chdr == NULL)
        last_chdr = codestreams;
      else
        last_chdr = last_chdr->next;
      last_chdr->donate_chdr_box(top_box);
      assert(!top_box);
    }
  else if ((top_box.get_box_type() == jp2_compositing_layer_hdr_4cc) &&
           !restrict_to_jp2)
    {
      if (last_jplh == NULL)
        last_jplh = layers = new jx_layer_source(this,num_layers);
      else
        last_jplh = last_jplh->next = new jx_layer_source(this,num_layers);
      num_layers++;
      last_jplh->donate_jplh_box(top_box);
      assert(!top_box);
    }
  else if (top_box.get_box_type() == jp2_composition_4cc)
    {
      composition.donate_composition_box(top_box,this);
      assert(!top_box);
    }
  else if (meta_manager.test_box_filter(top_box.get_box_type()))
    { // Build into meta-data structure
      jx_metanode *node = meta_manager.tree->add_descendant();
      node->donate_input_box(top_box);
      assert(!top_box);
    }
  else
    top_box.close();
  in_parse_next_top_level_box = false;

  if (restrict_to_jp2 && (num_layers == 0) && (num_codestreams > 0))
    { // JP2 files have one layer, corresponding to the 1st and only codestream
      assert(layers == NULL);
      last_jplh = layers = new jx_layer_source(this,0);
      num_layers = 1;
    }

  return true;
}

/*****************************************************************************/
/*                      jx_source::finish_jp2_header_box                     */
/*****************************************************************************/

bool
  jx_source::finish_jp2_header_box()
{
  if (jp2h_box_complete)
    return true;
  while ((!jp2h_box_found) && !is_top_level_complete())
    if (!parse_next_top_level_box())
      break;
  if (!jp2h_box_found)
    return is_top_level_complete();

  assert(jp2h_box.exists());
  while (sub_box.exists() || sub_box.open(&jp2h_box))
    {
      bool sub_complete = sub_box.is_complete();
      if (sub_box.get_box_type() == jp2_image_header_4cc)
        {
          if (!sub_complete)
            return false;
          default_dimensions.init(&sub_box);
        }
      else if (sub_box.get_box_type() == jp2_bits_per_component_4cc)
        {
          if (!sub_complete)
            return false;
          default_dimensions.process_bpcc_box(&sub_box);
          have_default_bpcc = true;
        }
      else if (sub_box.get_box_type() == jp2_palette_4cc)
        {
          if (!sub_complete)
            return false;
          default_palette.init(&sub_box);
        }
      else if (sub_box.get_box_type() == jp2_component_mapping_4cc)
        {
          if (!sub_complete)
            return false;
          default_component_map.init(&sub_box);
        }
      else if (sub_box.get_box_type() == jp2_colour_4cc)
        {
          if (!sub_complete)
            return false;
          j2_colour *cscan;
          for (cscan=&default_colour; cscan->next != NULL; cscan=cscan->next);
          if (cscan->is_initialized())
            cscan = cscan->next = new j2_colour;
          cscan->init(&sub_box);
        }
      else if (sub_box.get_box_type() == jp2_channel_definition_4cc)
        {
          if (!sub_complete)
            return false;
          default_channels.init(&sub_box);
        }
      else if (sub_box.get_box_type() == jp2_resolution_4cc)
        {
          if (!sub_complete)
            return false;
          default_resolution.init(&sub_box);
        }
      else
        sub_box.close(); // Skip over other boxes.
    }

  // See if we have enough of the JP2 header to finish parsing it
  if (jp2h_box.is_complete() ||
      (default_dimensions.is_initialized() && have_default_bpcc &&
       default_palette.is_initialized() &&
       default_component_map.is_initialized() &&
       default_channels.is_initialized() &&
       default_colour.is_initialized() &&
       default_resolution.is_initialized()))
    {
      jp2h_box.close();
      default_resolution.finalize();
      jp2h_box_complete = true;
      return true;
    }
  return false;
}


/* ========================================================================= */
/*                                 jpx_source                                */
/* ========================================================================= */

/*****************************************************************************/
/*                           jpx_source::jpx_source                          */
/*****************************************************************************/

jpx_source::jpx_source()
{
  state = NULL;
}

/*****************************************************************************/
/*                              jpx_source::open                             */
/*****************************************************************************/

int
  jpx_source::open(jp2_family_src *src, bool return_if_incompatible)
{
  if (state == NULL)
    state = new jx_source(src);
  if (state->is_completely_open)
    { KDU_ERROR_DEV(e,44); e <<
        KDU_TXT("Attempting invoke `jpx_source::open' on a JPX "
        "source object which has been completely opened, but not yet closed.");
    }
  if ((state->ultimate_src != src) ||
      (src->get_id() != state->ultimate_src_id))
    { // Treat as though it was closed
      delete state;
      state = new jx_source(src);
    }

  // First, check for a signature box
  if (!state->have_signature)
    {
      if (!((state->top_box.exists() || state->top_box.open(src)) &&
            state->top_box.is_complete()))
        {
          if (src->uses_cache())
            return 0;
          else
            {
              close();
              if (return_if_incompatible)
                return -1;
              else
                { KDU_ERROR(e,45); e <<
                    KDU_TXT("Data source supplied to `jpx_source::open' "
                    "does not commence with a valid JP2-family signature "
                    "box.");
                }
            }
        }
      kdu_uint32 signature;
      if ((state->top_box.get_box_type() != jp2_signature_4cc) ||
          !(state->top_box.read(signature) && (signature == jp2_signature) &&
            (state->top_box.get_remaining_bytes() == 0)))
        {
          close();
          if (return_if_incompatible)
            return -1;
          else
            { KDU_ERROR(e,46); e <<
                KDU_TXT("Data source supplied to `jpx_source::open' "
                "does not commence with a valid JP2-family signature box.");
            }
        }
      state->top_box.close();
      state->have_signature = true;
    }

  // Next, check for a file-type box
  if (!state->have_file_type)
    {
      if (!((state->top_box.exists() || state->top_box.open_next()) &&
            state->top_box.is_complete()))
        {
          if (src->uses_cache())
            return 0;
          else
            {
              close();
              if (return_if_incompatible)
                return -1;
              else
                { KDU_ERROR(e,47);  e <<
                    KDU_TXT("Data source supplied to `jpx_source::open' "
                    "does not contain a correctly positioned file-type "
                    "(ftyp) box.");
                }
            }
        }
      if (state->top_box.get_box_type() != jp2_file_type_4cc)
        {
          close();
          if (return_if_incompatible)
            return -1;
          else
            { KDU_ERROR(e,48); e <<
                KDU_TXT("Data source supplied to `jpx_source::open' "
                "does not contain a correctly positioned file-type "
                "(ftyp) box.");
            }
        }
      if (!state->compatibility.init_ftyp(&state->top_box))
        { // Not compatible with JP2 or JPX.
          close();
          if (return_if_incompatible)
            return -1;
          else
            { KDU_ERROR(e,49); e <<
                KDU_TXT("Data source supplied to `jpx_source::open' "
                "contains a correctly positioned file-type box, but that box "
                "does not identify either JP2 or JPX as a compatible file "
                "type.");
            }
        }
      assert(!state->top_box);
      state->have_file_type = true;
      jpx_compatibility compat(&state->compatibility);
      state->restrict_to_jp2 = compat.is_jp2();
    }

  if (state->restrict_to_jp2)
    { // Ignore reader requirements box, if any
      state->is_completely_open = true;
      return 1;
    }

  // Need to check for a reader requirements box
  assert(!state->have_reader_requirements);
  if (!((state->top_box.exists() || state->top_box.open_next()) &&
        ((state->top_box.get_box_type() != jp2_reader_requirements_4cc) ||
         state->top_box.is_complete())))
    {
      if (src->uses_cache())
        return 0;
      else
        {
          close();
          if (return_if_incompatible)
            return -1;
          else
            { KDU_ERROR(e,50); e <<
                KDU_TXT("Data source supplied to `jpx_source::open' "
                "does not contain a correctly positioned reader "
                "requirements box.");
            }
        }
    }
  if (state->top_box.get_box_type() == jp2_reader_requirements_4cc)
    { // Allow the reader requirements box to be missing, even though it
      // is not strictly legal
      state->compatibility.init_rreq(&state->top_box);
      state->have_reader_requirements = true;
      assert(!state->top_box);
    }
  else
    state->parse_next_top_level_box(true);

  // If we get here, we have done everything
  state->is_completely_open = true;
  return 1;
}

/*****************************************************************************/
/*                        jpx_source::get_ultimate_src                      */
/*****************************************************************************/

jp2_family_src *
  jpx_source::get_ultimate_src()
{
  if (state == NULL)
    return NULL;
  else
    return state->ultimate_src;
}

/*****************************************************************************/
/*                              jpx_source::close                            */
/*****************************************************************************/

bool
  jpx_source::close()
{
  if (state == NULL)
    return false;
  bool result = state->is_completely_open;
  delete state;
  state = NULL;
  return result;
}

/*****************************************************************************/
/*                     jpx_source::access_data_references                    */
/*****************************************************************************/

jp2_data_references
  jpx_source::access_data_references()
{
  if ((state == NULL) || !state->is_completely_open)
    return jp2_data_references(NULL);
  while ((!state->have_dtbl_box) && (!state->top_level_complete) &&
         state->parse_next_top_level_box());
  if (state->dtbl_box.exists())
    {
      if (!state->dtbl_box.is_complete())
        return jp2_data_references(NULL);
      state->data_references.init(&state->dtbl_box);
      assert(!(state->dtbl_box));
    }
  if (state->have_dtbl_box || state->top_level_complete)
    return jp2_data_references(&(state->data_references));
  else
    return jp2_data_references(NULL);
}

/*****************************************************************************/
/*                      jpx_source::access_compatibility                     */
/*****************************************************************************/

jpx_compatibility
  jpx_source::access_compatibility()
{
  jpx_compatibility result;
  if ((state != NULL) & state->is_completely_open)
    result = jpx_compatibility(&state->compatibility);
  return result;
}

/*****************************************************************************/
/*                        jpx_source::count_codestreams                      */
/*****************************************************************************/

bool
  jpx_source::count_codestreams(int &count)
{
  if ((state == NULL) || !state->is_completely_open)
    { count = 0; return false; }
  while ((!state->top_level_complete) && state->parse_next_top_level_box());
  count = state->num_codestreams;
  return state->top_level_complete;
}

/*****************************************************************************/
/*                   jpx_source::count_compositing_layers                    */
/*****************************************************************************/

bool
  jpx_source::count_compositing_layers(int &count)
{
  if ((state == NULL) || !state->is_completely_open)
    { count = 0; return false; }
  if (!state->restrict_to_jp2)
    while ((!state->top_level_complete) && state->parse_next_top_level_box());
  count = state->num_layers;
  if ((count < 1) && state->restrict_to_jp2)
    count = 1; // We always have at least one, even if we have to fake it
  return (state->top_level_complete || state->restrict_to_jp2);
}

/*****************************************************************************/
/*                       jpx_source::access_codestream                       */
/*****************************************************************************/

jpx_codestream_source
  jpx_source::access_codestream(int which, bool need_main_header)
{
  jpx_codestream_source result;
  if ((state == NULL) || (!state->is_completely_open) || (which < 0))
    return result; // Return an empty interface
  while ((state->num_codestreams <= which) && (!state->top_level_complete))
    if (!state->parse_next_top_level_box())
      {
        if ((which == 0) && state->top_level_complete)
          { KDU_ERROR(e,51); e <<
              KDU_TXT("JPX data source appears to contain no "
              "codestreams at all.");
          }
        return result; // Return an empty interface
      }
  jx_codestream_source *cs = state->codestreams;
  for (int n=0; n < which; n++)
    cs = cs->next;
  if (cs->finish(true))
    {
      if ((!need_main_header) || cs->is_stream_ready())
        result = jpx_codestream_source(cs);
    }
  else if (state->top_level_complete && !cs->have_all_boxes())
    { KDU_ERROR(e,52); e <<
        KDU_TXT("JPX data source appears to contain an incomplete "
        "codestream.  Specifically, this either means that an embedded "
        "contiguous or fragmented codestream has been found without "
        "sufficient descriptive metadata, or that a codestream header box "
        "has been found without any matching embedded contiguous or "
        "fragmented codestream.  Both of these conditions are illegal for "
        "JPX and JP2 data sources.");
    }
  return result;
}

/*****************************************************************************/
/*                          jpx_source::access_layer                         */
/*****************************************************************************/

jpx_layer_source
  jpx_source::access_layer(int which, bool need_stream_headers)
{
  jpx_layer_source result;
  if ((state == NULL) || (!state->is_completely_open) || (which < 0))
    return result; // Return an empty interface
  if (state->restrict_to_jp2 && (which != 0))
    return result; // Return empty interface

  while ((state->num_layers <= which) && (!state->top_level_complete))
    if (!state->parse_next_top_level_box())
      break;
  if (state->num_layers <= which)
    return result; // Return an empty interface

  jx_layer_source *layer = state->layers;
  for (int n=0; n < which; n++)
    layer = layer->next;
  if (layer->finish())
    {
      if ((!need_stream_headers) || layer->all_streams_ready())
        result = jpx_layer_source(layer);
    }
  return result;
}

/*****************************************************************************/
/*                    jpx_source::get_num_layer_codestreams                  */
/*****************************************************************************/

int
  jpx_source::get_num_layer_codestreams(int which_layer)
{
  if ((state == NULL) || (!state->is_completely_open) || (which_layer < 0))
    return 0;
  if (state->restrict_to_jp2 && (which_layer != 0))
    return 0;

  while ((state->num_layers <= which_layer) && (!state->top_level_complete))
    if (!state->parse_next_top_level_box())
      break;
  if (state->num_layers <= which_layer)
    return 0;

  jx_layer_source *layer = state->layers;
  for (int n=0; n < which_layer; n++)
    layer = layer->next;
  layer->finish(); // Try to finish it

  jx_registration *reg = layer->access_registration();
  return reg->num_codestreams;
}

/*****************************************************************************/
/*                     jpx_source::get_layer_codestream_id                   */
/*****************************************************************************/

int
  jpx_source::get_layer_codestream_id(int which_layer, int which_stream)
{
  if ((state == NULL) || (!state->is_completely_open) ||
      (which_layer < 0) || (which_stream < 0))
    return -1;
  if (state->restrict_to_jp2 && (which_layer != 0))
    return -1;

  while ((state->num_layers <= which_layer) && (!state->top_level_complete))
    if (!state->parse_next_top_level_box())
      break;
  if (state->num_layers <= which_layer)
    return -1;

  jx_layer_source *layer = state->layers;
  for (int n=0; n < which_layer; n++)
    layer = layer->next;
  layer->finish(); // Try to finish it

  jx_registration *reg = layer->access_registration();
  if (which_stream >= reg->num_codestreams)
    return -1;
  return reg->codestreams[which_stream].codestream_id;
}

/*****************************************************************************/
/*                       jpx_source::access_composition                      */
/*****************************************************************************/

jpx_composition
  jpx_source::access_composition()
{
  jpx_composition result;
  if ((state != NULL) && state->composition.finish(state))
    result = jpx_composition(&state->composition);
  return result;
}

/*****************************************************************************/
/*                      jpx_source::access_meta_manager                      */
/*****************************************************************************/

jpx_meta_manager
  jpx_source::access_meta_manager()
{
  jpx_meta_manager result;
  if (state != NULL)
    result = jpx_meta_manager(&state->meta_manager);
  return result;
}


/* ========================================================================= */
/*                           jx_codestream_target                            */
/* ========================================================================= */

/*****************************************************************************/
/*                  jpx_codestream_target::get_codestream_id                 */
/*****************************************************************************/

int
  jpx_codestream_target::get_codestream_id()
{
  assert(state != NULL);
  return state->id;
}

/*****************************************************************************/
/*                      jx_codestream_target::finalize                       */
/*****************************************************************************/

void
  jx_codestream_target::finalize()
{
  if (finalized)
    return;
  dimensions.finalize();
  palette.finalize();
  component_map.finalize(&dimensions,&palette);
  finalized = true;
}

/*****************************************************************************/
/*                     jx_codestream_target::write_chdr                      */
/*****************************************************************************/

jp2_output_box *
  jx_codestream_target::write_chdr(int *i_param, void **addr_param)
{
  assert(finalized && !chdr_written);
  if (chdr.exists())
    { // We must have been processing a breakpoint.
      assert(have_breakpoint);
      chdr.close();
      chdr_written = true;
      return NULL;
    }
  owner->open_top_box(&chdr,jp2_codestream_header_4cc);
  if (!owner->get_default_dimensions()->compare(&dimensions))
    dimensions.save_boxes(&chdr);
  j2_palette *def_palette = owner->get_default_palette();
  j2_component_map *def_cmap = owner->get_default_component_map();
  if (def_palette->is_initialized())
    { // Otherwise, there will be no default component map either
      if (!def_palette->compare(&palette))
        palette.save_box(&chdr);
      if (!def_cmap->compare(&component_map))
        component_map.save_box(&chdr,true); // Force generation of a cmap box
    }
  else
    {
      palette.save_box(&chdr);
      component_map.save_box(&chdr);
    }
  if (have_breakpoint)
    {
      if (i_param != NULL)
        *i_param = this->i_param;
      if (addr_param != NULL)
        *addr_param = this->addr_param;
      return &chdr;
    }
  chdr.close();
  chdr_written = true;
  return NULL;
}

/*****************************************************************************/
/*                    jx_codestream_target::copy_defaults                    */
/*****************************************************************************/

void
  jx_codestream_target::copy_defaults(j2_dimensions &default_dims,
                                      j2_palette &default_plt,
                                      j2_component_map &default_map)
{
  default_dims.copy(&dimensions);
  default_plt.copy(&palette);
  default_map.copy(&component_map);
}

/*****************************************************************************/
/*               jx_codestream_target::adjust_compatibility                  */
/*****************************************************************************/

void
  jx_codestream_target::adjust_compatibility(jx_compatibility *compatibility)
{
  assert(finalized);
  int profile, compression_type;
  compression_type = dimensions.get_compression_type(profile);
  if (compression_type == JP2_COMPRESSION_TYPE_JPEG2000)
    {
      if (profile < 0)
        profile = Sprofile_PROFILE2; // Reasonable guess, if never initialized
      if (profile == Sprofile_PROFILE0)
        compatibility->add_standard_feature(JPX_SF_JPEG2000_PART1_PROFILE0);
      else if (profile == Sprofile_PROFILE1)
        compatibility->add_standard_feature(JPX_SF_JPEG2000_PART1_PROFILE1);
      else if ((profile == Sprofile_PROFILE2) ||
               (profile = Sprofile_CINEMA2K) ||
               (profile == Sprofile_CINEMA4K))
        compatibility->add_standard_feature(JPX_SF_JPEG2000_PART1);
      else
        {
          compatibility->add_standard_feature(JPX_SF_JPEG2000_PART2);
          compatibility->have_non_part1_codestream();
          if (id == 0)
            {
              compatibility->set_not_jp2_compatible();
              if (!dimensions.get_jpxb_compatible())
                compatibility->set_not_jpxb_compatible();
            }
        }
    }
  else
    {
      if (id == 0)
        {
          compatibility->set_not_jp2_compatible();
          compatibility->set_not_jpxb_compatible();
        }
      compatibility->have_non_part1_codestream();
      if (compression_type == JP2_COMPRESSION_TYPE_JPEG)
        compatibility->add_standard_feature(JPX_SF_CODESTREAM_USING_DCT);
    }
}


/* ========================================================================= */
/*                          jpx_codestream_target                            */
/* ========================================================================= */

/*****************************************************************************/
/*                  jpx_codestream_target::access_dimensions                 */
/*****************************************************************************/

jp2_dimensions
  jpx_codestream_target::access_dimensions()
{
  assert(state != NULL);
  return jp2_dimensions(&state->dimensions);
}

/*****************************************************************************/
/*                   jpx_codestream_target::access_palette                   */
/*****************************************************************************/

jp2_palette
  jpx_codestream_target::access_palette()
{
  assert(state != NULL);
  return jp2_palette(&state->palette);
}

/*****************************************************************************/
/*                    jpx_codestream_target::set_breakpoint                  */
/*****************************************************************************/

void
  jpx_codestream_target::set_breakpoint(int i_param, void *addr_param)
{
  assert(state != NULL);
  state->have_breakpoint = true;
  state->i_param = i_param;
  state->addr_param = addr_param;
}

/*****************************************************************************/
/*                 jpx_codestream_target::access_fragment_list               */
/*****************************************************************************/

jpx_fragment_list
  jpx_codestream_target::access_fragment_list()
{
  return jpx_fragment_list(&(state->fragment_list));
}

/*****************************************************************************/
/*                     jpx_codestream_target::add_fragment                   */
/*****************************************************************************/

void
  jpx_codestream_target::add_fragment(const char *url, kdu_long offset,
                                      kdu_long length)
{
  int url_idx = state->owner->get_data_references().add_url(url);
  access_fragment_list().add_fragment(url_idx,offset,length);
}

/*****************************************************************************/
/*                 jpx_codestream_target::write_fragment_table               */
/*****************************************************************************/

void
  jpx_codestream_target::write_fragment_table()
{
  if (!state->owner->can_write_codestreams())
    { KDU_ERROR_DEV(e,53); e <<
        KDU_TXT("You may not call "
        "`jpx_codestream_target::open_stream' or "
        "`jpx_codestream_target::write_fragment_table' until after the JPX "
        "file header has been written using `jpx_target::write_headers'.");
    }
  if (state->codestream_opened)
    { KDU_ERROR_DEV(e,54); e <<
        KDU_TXT("You may not call "
        "`jpx_codestream_target::open_stream' or "
        "`jpx_codestream_target::write_fragment_table' multiple times for the "
        "same code-stream.");
    }
  if (state->fragment_list.is_empty())
    { KDU_ERROR_DEV(e,55); e <<
        KDU_TXT("You must not call "
        "`jpx_codestream_target::write_fragment_table' without first adding "
        "one or more references to codestream fragments.");
    }
  state->fragment_list.finalize(state->owner->get_data_references());
  state->owner->open_top_box(&state->jp2c,jp2_fragment_table_4cc);
  state->fragment_list.save_box(&(state->jp2c));
  state->jp2c.close();
  state->codestream_opened = true;
}

/*****************************************************************************/
/*                     jpx_codestream_target::open_stream                    */
/*****************************************************************************/

jp2_output_box *
  jpx_codestream_target::open_stream()
{
  assert(state != NULL);
  if (!state->owner->can_write_codestreams())
    { KDU_ERROR_DEV(e,56); e <<
        KDU_TXT("You may not call "
        "`jpx_codestream_target::open_stream' or "
        "`jpx_codestream_target::write_fragment_table' until after the JPX "
        "file header has been written using `jpx_target::write_headers'.");
    }
  if (!state->fragment_list.is_empty())
    { KDU_ERROR_DEV(e,57); e <<
        KDU_TXT("You may not call `open_stream' "
        "on a `jpx_codestream_target' object to which one or more "
        "codestream fragment references have already been added.  Each "
        "codestream must be represented by exactly one contiguous codestream "
        "or one fragment table, but not both.");
    }
  if (state->codestream_opened)
    { KDU_ERROR_DEV(e,58); e <<
        KDU_TXT("You may not call "
        "`jpx_codestream_target::open_stream' or "
        "`jpx_codestream_target::write_fragment_table' multiple times for the "
        "same code-stream.");
    }
  state->owner->open_top_box(&state->jp2c,jp2_codestream_4cc);
  state->codestream_opened = true;
  return &state->jp2c;
}

/*****************************************************************************/
/*                    jpx_codestream_target::access_stream                   */
/*****************************************************************************/

kdu_compressed_target *
  jpx_codestream_target::access_stream()
{
  assert(state != NULL);
  return &state->jp2c;
}


/* ========================================================================= */
/*                             jx_layer_target                               */
/* ========================================================================= */

/*****************************************************************************/
/*                        jx_layer_target::finalize                          */
/*****************************************************************************/

bool
  jx_layer_target::finalize()
{
  if (finalized)
    return need_creg;
  resolution.finalize();
  j2_colour *cscan;
  int num_colours = 0;
  for (cscan=&colour; cscan != NULL; cscan=cscan->next)
    if (num_colours == 0)
      num_colours = cscan->get_num_colours();
    else if ((num_colours != cscan->get_num_colours()) &&
             (cscan->get_num_colours() != 0))
      { KDU_ERROR_DEV(e,59); e <<
          KDU_TXT("The `jpx_layer_target::add_colour' function has "
          "been used to add multiple colour descriptions for a JPX "
          "compositing layer, but not all colour descriptions have the "
          "same number of colour channels.");
      }
  channels.finalize(num_colours,true);

  registration.finalize(&channels);
  need_creg = false; // Until proven otherwise
  int cs_id, cs_which;
  for (cs_which=0; cs_which < registration.num_codestreams; cs_which++)
    {
      cs_id = registration.codestreams[cs_which].codestream_id;
      if (cs_id != this->id)
        need_creg = true;
      jx_codestream_target *cs = owner->get_codestream(cs_id);
      if (cs == NULL)
        { KDU_ERROR_DEV(e,60); e <<
            KDU_TXT("Application has configured a JPX compositing "
            "layer box which utilizes a non-existent codestream!");
        }
      channels.add_cmap_channels(cs->get_component_map(),cs_id);
      jpx_codestream_target cs_ifc(cs);
      kdu_coords size = cs_ifc.access_dimensions().get_size();
      if ((registration.codestreams[cs_which].sampling !=
           registration.denominator) ||
          (registration.codestreams[cs_which].alignment.x != 0) ||
          (registration.codestreams[cs_which].alignment.y != 0))
        need_creg = true;
      size.x *= registration.codestreams[cs_which].sampling.x;
      size.y *= registration.codestreams[cs_which].sampling.y;
      size += registration.codestreams[cs_which].alignment;
      if ((cs_which == 0) || (size.x < registration.final_layer_size.x))
        registration.final_layer_size.x = size.x;
      if ((cs_which == 0) || (size.y < registration.final_layer_size.y))
        registration.final_layer_size.y = size.y;
    }
  registration.final_layer_size.x =
    ceil_ratio(registration.final_layer_size.x,registration.denominator.x);
  registration.final_layer_size.y =
    ceil_ratio(registration.final_layer_size.y,registration.denominator.y);
  for (cscan=&colour; cscan != NULL; cscan=cscan->next)
    cscan->finalize(&channels);
  finalized = true;
  return need_creg;
}

/*****************************************************************************/
/*                        jx_layer_target::write_jplh                        */
/*****************************************************************************/

jp2_output_box *
  jx_layer_target::write_jplh(bool write_creg_box, int *i_param,
                              void **addr_param)
{
  assert(finalized && !jplh_written);
  if (jplh.exists())
    { // We must have been processing a breakpoint.
      assert(have_breakpoint);
      jplh.close();
      jplh_written = true;
      return NULL;
    }
  owner->open_top_box(&jplh,jp2_compositing_layer_hdr_4cc);
  j2_colour *cscan, *dscan, *default_colour=owner->get_default_colour();
  for (cscan=&colour; cscan != NULL; cscan=cscan->next)
    {
      for (dscan=default_colour; dscan != NULL; dscan=dscan->next)
        if (cscan->compare(dscan))
          break;
      if (dscan == NULL)
        break; // Could not find a match
    }
  if (cscan != NULL)
    { // Default colour spaces not identical to current ones
      jp2_output_box cgrp;
      cgrp.open(&jplh,jp2_colour_group_4cc);
      for (cscan=&colour; cscan != NULL; cscan=cscan->next)
        cscan->save_box(&cgrp);
      cgrp.close();
    }
  if (!owner->get_default_channels()->compare(&channels))
    channels.save_box(&jplh,(id==0));
  if (write_creg_box)
    registration.save_box(&jplh);
  if (!owner->get_default_resolution()->compare(&resolution))
    resolution.save_box(&jplh);
  if (have_breakpoint)
    {
      if (i_param != NULL)
        *i_param = this->i_param;
      if (addr_param != NULL)
        *addr_param = this->addr_param;
      return &jplh;
    }
  jplh.close();
  jplh_written = true;
  return NULL;
}

/*****************************************************************************/
/*                       jx_layer_target::copy_defaults                      */
/*****************************************************************************/

void
  jx_layer_target::copy_defaults(j2_resolution &default_res,
                                 j2_channels &default_channels,
                                 j2_colour &default_colour)
{
  default_res.copy(&resolution);
  if (!channels.needs_opacity_box())
    default_channels.copy(&channels); // Can't write opacity box in jp2 header
  j2_colour *src, *dst;
  for (src=&colour, dst=&default_colour; src != NULL; src=src->next)
    {
      dst->copy(src);
      if (src->next != NULL)
        {
          assert(dst->next == NULL);
          dst = dst->next = new j2_colour;
        }
    }
}

/*****************************************************************************/
/*                  jx_layer_target::adjust_compatibility                    */
/*****************************************************************************/

void
  jx_layer_target::adjust_compatibility(jx_compatibility *compatibility)
{
  assert(finalized);
  if (id > 0)
    compatibility->add_standard_feature(JPX_SF_MULTIPLE_LAYERS);

  if (channels.uses_palette_colour())
    compatibility->add_standard_feature(JPX_SF_PALETTIZED_COLOUR);
  if (channels.has_opacity())
    compatibility->add_standard_feature(JPX_SF_OPACITY_NOT_PREMULTIPLIED);
  if (channels.has_premultiplied_opacity())
    compatibility->add_standard_feature(JPX_SF_OPACITY_PREMULTIPLIED);
  if (channels.needs_opacity_box())
    {
      compatibility->add_standard_feature(JPX_SF_OPACITY_BY_CHROMA_KEY);
      if (id == 0)
        compatibility->set_not_jp2_compatible();
    }

  if (registration.num_codestreams > 1)
    {
      compatibility->add_standard_feature(
                                  JPX_SF_MULTIPLE_CODESTREAMS_PER_LAYER);
      if (id == 0)
        {
          compatibility->set_not_jp2_compatible();
          compatibility->set_not_jpxb_compatible();
        }
      for (int n=1; n < registration.num_codestreams; n++)
        if (registration.codestreams[n].sampling !=
            registration.codestreams[0].sampling)
          {
            compatibility->add_standard_feature(JPX_SF_SCALING_WITHIN_LAYER);
            break;
          }
    }

  jp2_resolution res(&resolution);
  float aspect = res.get_aspect_ratio();
  if ((aspect < 0.99F) || (aspect > 1.01F))
    compatibility->add_standard_feature(JPX_SF_SAMPLES_NOT_SQUARE);

  bool have_jp2_compatible_space = false;
  bool have_non_vendor_space = false;
  int best_precedence = -128;
  j2_colour *cbest=NULL, *cscan;
  for (cscan=&colour; cscan != NULL; cscan=cscan->next)
    {
      jp2_colour clr(cscan);
      if (clr.get_precedence() > best_precedence)
        {
          best_precedence = clr.get_precedence();
          cbest = cscan;
        }
    }
  for (cscan=&colour; cscan != NULL; cscan=cscan->next)
    { // Add feature flags to represent each colour description.
      if (cscan->is_jp2_compatible())
        have_jp2_compatible_space = true;
      jp2_colour clr(cscan);
      jp2_colour_space space = clr.get_space();
      bool add_to_both = (cscan==cbest);
      if (space == JP2_bilevel1_SPACE)
        compatibility->add_standard_feature(JPX_SF_BILEVEL1,add_to_both);
      else if (space == JP2_YCbCr1_SPACE)
        compatibility->add_standard_feature(JPX_SF_YCbCr1,add_to_both);
      else if (space == JP2_YCbCr2_SPACE)
        compatibility->add_standard_feature(JPX_SF_YCbCr2,add_to_both);
      else if (space == JP2_YCbCr3_SPACE)
        compatibility->add_standard_feature(JPX_SF_YCbCr3,add_to_both);
      else if (space == JP2_PhotoYCC_SPACE)
        compatibility->add_standard_feature(JPX_SF_PhotoYCC,add_to_both);
      else if (space == JP2_CMY_SPACE)
        compatibility->add_standard_feature(JPX_SF_CMY,add_to_both);
      else if (space == JP2_CMYK_SPACE)
        compatibility->add_standard_feature(JPX_SF_CMYK,add_to_both);
      else if (space == JP2_YCCK_SPACE)
        compatibility->add_standard_feature(JPX_SF_YCCK,add_to_both);
      else if (space == JP2_CIELab_SPACE)
        {
          if (clr.check_cie_default())
            compatibility->add_standard_feature(JPX_SF_LAB_DEFAULT,
                                                add_to_both);
          else
            compatibility->add_standard_feature(JPX_SF_LAB,add_to_both);
        }
      else if (space == JP2_bilevel2_SPACE)
        compatibility->add_standard_feature(JPX_SF_BILEVEL2,add_to_both);
      else if (space == JP2_sRGB_SPACE)
        compatibility->add_standard_feature(JPX_SF_sRGB,add_to_both);
      else if (space == JP2_sLUM_SPACE)
        compatibility->add_standard_feature(JPX_SF_sLUM,add_to_both);
      else if (space == JP2_sYCC_SPACE)
        compatibility->add_standard_feature(JPX_SF_sYCC,add_to_both);
      else if (space == JP2_CIEJab_SPACE)
        {
          if (clr.check_cie_default())
            compatibility->add_standard_feature(JPX_SF_JAB_DEFAULT,
                                                add_to_both);
          else
            compatibility->add_standard_feature(JPX_SF_JAB,add_to_both);
        }
      else if (space == JP2_esRGB_SPACE)
        compatibility->add_standard_feature(JPX_SF_esRGB,add_to_both);
      else if (space == JP2_ROMMRGB_SPACE)
        compatibility->add_standard_feature(JPX_SF_ROMMRGB,add_to_both);
      else if (space == JP2_YPbPr60_SPACE)
        {} // Need a compatibility code for this -- none defined originally
      else if (space == JP2_YPbPr50_SPACE)
        {} // Need a compatibility code for this -- none defined originally
      else if (space == JP2_esYCC_SPACE)
        {} // Need a compatibility code for this -- none defined originally
      else if (space == JP2_iccLUM_SPACE)
        compatibility->add_standard_feature(JPX_SF_RESTRICTED_ICC,add_to_both);
      else if (space == JP2_iccRGB_SPACE)
        compatibility->add_standard_feature(JPX_SF_RESTRICTED_ICC,add_to_both);
      else if (space == JP2_iccANY_SPACE)
        compatibility->add_standard_feature(JPX_SF_ANY_ICC,add_to_both);
      else if (space == JP2_vendor_SPACE)
        {} // Need a compatibility code for this -- none defined originally
      if (space != JP2_vendor_SPACE)
        have_non_vendor_space = true;
    }
  if ((id == 0) && !have_jp2_compatible_space)
    compatibility->set_not_jp2_compatible();
  if ((id == 0) && !have_non_vendor_space)
    compatibility->set_not_jpxb_compatible();
}

/*****************************************************************************/
/*                     jx_layer_target::uses_codestream                      */
/*****************************************************************************/

bool
  jx_layer_target::uses_codestream(int idx)
{
  for (int n=0; n < registration.num_codestreams; n++)
    if (registration.codestreams[n].codestream_id == idx)
      return true;
  return false;
}


/* ========================================================================= */
/*                            jpx_layer_target                               */
/* ========================================================================= */

/*****************************************************************************/
/*                    jpx_layer_target::access_channels                      */
/*****************************************************************************/

jp2_channels
  jpx_layer_target::access_channels()
{
  assert(state != NULL);
  return jp2_channels(&state->channels);
}

/*****************************************************************************/
/*                   jpx_layer_target::access_resolution                     */
/*****************************************************************************/

jp2_resolution
  jpx_layer_target::access_resolution()
{
  assert(state != NULL);
  return jp2_resolution(&state->resolution);
}

/*****************************************************************************/
/*                       jpx_layer_target::add_colour                        */
/*****************************************************************************/

jp2_colour
  jpx_layer_target::add_colour(int precedence, kdu_byte approx)
{
  assert(state != NULL);
  if ((precedence < -128) || (precedence > 127) || (approx > 4))
    { KDU_ERROR_DEV(e,61); e <<
        KDU_TXT("Invalid `precedence' or `approx' parameter supplied "
        "to `jpx_layer_target::add_colour'.  Legal values for the precedence "
        "parameter must lie in the range -128 to +127, while legal values "
        "for the approximation level parameter are 0, 1, 2, 3 and 4.");
    }
  if (state->last_colour == NULL)
    state->last_colour = &state->colour;
  else
    state->last_colour = state->last_colour->next = new j2_colour;
  state->last_colour->precedence = precedence;
  state->last_colour->approx = approx;
  return jp2_colour(state->last_colour);
}

/*****************************************************************************/
/*                     jpx_layer_target::access_colour                       */
/*****************************************************************************/

jp2_colour
  jpx_layer_target::access_colour(int which)
{
  assert(state != NULL);
  j2_colour *scan = NULL;
  if (which >= 0)
    for (scan=&state->colour; (scan != NULL) && (which > 0); which--)
      scan = scan->next;
  return jp2_colour(scan);
}

/*****************************************************************************/
/*               jpx_layer_target::set_codestream_registration               */
/*****************************************************************************/

void
  jpx_layer_target::set_codestream_registration(int codestream_id,
                                                kdu_coords alignment,
                                                kdu_coords sampling,
                                                kdu_coords denominator)
{
  int n;
  assert(state != NULL);
  jx_registration *reg = &state->registration;
  jx_registration::jx_layer_stream *str;
  if (reg->num_codestreams == 0)
    reg->denominator = denominator;
  else if (reg->denominator != denominator)
    { KDU_ERROR_DEV(e,62); e <<
        KDU_TXT("The denominator values supplied via all calls to "
        "`jpx_layer_target::set_codestream_registration' within the same "
        "compositing layer must be identical.  This is because the "
        "codestream registration (creg) box can record only one denominator "
        "(point density) to be shared by all the codestream sampling and "
        "alignment parameters.");
    }
  if ((denominator.x < 1) || (denominator.x >= (1<<16)) ||
      (denominator.y < 1) || (denominator.y >= (1<<16)) ||
      (alignment.x < 0) || (alignment.y < 0) ||
      (alignment.x > 255) || (alignment.y > 255) ||
      (alignment.x >= denominator.x) || (alignment.y >= denominator.y) ||
      (sampling.x < 1) || (sampling.y < 1) ||
      (sampling.x > 255) || (sampling.y > 255))
    { KDU_ERROR_DEV(e,63); e <<
        KDU_TXT("Illegal alignment or sampling parameters passed to "
        "`jpx_layer_target::set_codestream_registration'.  All quantities "
        "must be non-negative (non-zero for sampling factors) and no larger "
        "than 255; moreover, the alignment offsets must be strictly less than "
        "the denominator (point density) values.");
    }

  for (n=0; n < reg->num_codestreams; n++)
    {
      str = reg->codestreams + n;
      if (str->codestream_id == codestream_id)
        break;
    }
  if (n == reg->num_codestreams)
    { // Add new element
      if (reg->max_codestreams == reg->num_codestreams)
        { // Augment array
          reg->max_codestreams += n + 2;
          str = new jx_registration::jx_layer_stream[reg->max_codestreams];
          for (n=0; n < reg->num_codestreams; n++)
            str[n] = reg->codestreams[n];
          if (reg->codestreams != NULL)
            delete[] reg->codestreams;
          reg->codestreams = str;
        }
      reg->num_codestreams++;
      str = reg->codestreams + n;
    }
  str->codestream_id = codestream_id;
  str->sampling = sampling;
  str->alignment = alignment;
}

/*****************************************************************************/
/*                      jpx_layer_target::set_breakpoint                     */
/*****************************************************************************/

void
  jpx_layer_target::set_breakpoint(int i_param, void *addr_param)
{
  assert(state != NULL);
  state->have_breakpoint = true;
  state->i_param = i_param;
  state->addr_param = addr_param;
}


/* ========================================================================= */
/*                                jx_target                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                           jx_target::jx_target                            */
/*****************************************************************************/

jx_target::jx_target(jp2_family_tgt *tgt)
{
  this->ultimate_tgt = tgt;
  last_opened_top_box = NULL;
  last_codestream_threshold = 0;
  need_creg_boxes = false;
  headers_in_progress = main_header_complete = false;
  num_codestreams = 0;
  num_layers = 0;
  codestreams = NULL;
  last_codestream = NULL;
  layers = NULL;
  last_layer = NULL;
  meta_manager.tree = new jx_metanode(&meta_manager);
}

/*****************************************************************************/
/*                          jx_target::~jx_target                            */
/*****************************************************************************/

jx_target::~jx_target()
{
  jx_codestream_target *cp;
  jx_layer_target *lp;
  j2_colour *cscan;

  while ((cp=codestreams) != NULL)
    {
      codestreams = cp->next;
      delete cp;
    }
  while ((lp=layers) != NULL)
    {
      layers = lp->next;
      delete lp;
    }
  while ((cscan=default_colour.next) != NULL)
    { default_colour.next = cscan->next; delete cscan; }
}

/*****************************************************************************/
/*                         jx_target::open_top_box                           */
/*****************************************************************************/

void
  jx_target::open_top_box(jp2_output_box *box, kdu_uint32 box_type)
{
  if ((last_opened_top_box != NULL) && (last_opened_top_box->exists()))
    { KDU_ERROR_DEV(e,64); e <<
        KDU_TXT("Attempting to open a new top-level box within "
        "a JPX file, while another top-level box is already open!  "
        "Problem may be caused by failing to complete a code-stream and "
        "close its box before attempting to write a second code-stream.");
    }
  last_opened_top_box = NULL;
  box->open(ultimate_tgt,box_type);
  last_opened_top_box = box;
}


/* ========================================================================= */
/*                                 jpx_target                                */
/* ========================================================================= */

/*****************************************************************************/
/*                           jpx_target::jpx_target                          */
/*****************************************************************************/

jpx_target::jpx_target()
{
  state = NULL;
}

/*****************************************************************************/
/*                              jpx_target::open                             */
/*****************************************************************************/

void
  jpx_target::open(jp2_family_tgt *tgt)
{
  if (state != NULL)
    { KDU_ERROR_DEV(e,65); e <<
        KDU_TXT("Attempting to open a `jpx_target' object which "
        "is already opened for writing a JPX file.");
    }
  state = new jx_target(tgt);
}

/*****************************************************************************/
/*                             jpx_target::close                             */
/*****************************************************************************/
bool
  jpx_target::close()
{
  if (state == NULL)
    return false;
  jx_codestream_target *cp;
  for (cp=state->codestreams; cp != NULL; cp=cp->next)
    if (!cp->is_complete())
      break;
  
  if (state->main_header_complete && (cp != NULL))
    { KDU_WARNING_DEV(w,2); w <<
        KDU_TXT("Started writing a JPX file, but failed to "
        "write all codestreams before calling `jpx_target::close'.");
    }
  else if (state->headers_in_progress)
    { KDU_WARNING_DEV(w,3); w <<
        KDU_TXT("Started writing JPX file headers, but failed "
        "to finish initiated sequence of calls to "
        "`jpx_target::write_headers'.  Problem is most likely due to "
        "the use of `jpx_codestream_target::set_breakpoint' or "
        "`jpx_layer_target::set_breakpoint' and failure to handle the "
        "breakpoints via multiple calls to `jpx_target::write_headers'.");
    }
  else if (state->main_header_complete)
    { // Write any outstanding headers
      bool missed_breakpoints = false;
      while (write_headers() != NULL)
        missed_breakpoints = true;
      if (missed_breakpoints)
        { KDU_WARNING_DEV(w,4); w <<
            KDU_TXT("Failed to catch all breakpoints installed "
            "via `jpx_codestream_target::set_breakpoint' or "
            "`jpx_layer_target::set_breakpoint'.  All required compositing "
            "layer header boxes and codestream header boxes have been "
            "automatically written while closing the file, but some of these "
            "included application-installed breakpoints where the application "
            "would ordinarily have written its own extra boxes.  This "
            "suggests that the application has failed to make sufficient "
            "explicit calls to `jpx_target::write_headers'.");
        }
    }

  if (access_data_references().get_num_urls() > 0)
    {
      jp2_output_box dtbl; state->open_top_box(&dtbl,jp2_dtbl_4cc);
      state->data_references.save_box(&dtbl);
    }

  delete state;
  state = NULL;
  return true;
}

/*****************************************************************************/
/*                     jpx_target::access_data_references                    */
/*****************************************************************************/

jp2_data_references
  jpx_target::access_data_references()
{
  return jp2_data_references(&state->data_references);
}

/*****************************************************************************/
/*                      jpx_target::access_compatibility                     */
/*****************************************************************************/

jpx_compatibility
  jpx_target::access_compatibility()
{
  jpx_compatibility result;
  if (state != NULL)
    result = jpx_compatibility(&state->compatibility);
  return result;
}

/*****************************************************************************/
/*                          jpx_target::add_codestream                       */
/*****************************************************************************/

jpx_codestream_target
  jpx_target::add_codestream()
{
  jpx_codestream_target result;
  if (state != NULL)
    {
      assert(!(state->main_header_complete || state->headers_in_progress));
      jx_codestream_target *cs =
        new jx_codestream_target(state,state->num_codestreams);
      if (state->last_codestream == NULL)
        state->codestreams = state->last_codestream = cs;
      else
        state->last_codestream = state->last_codestream->next = cs;
      state->num_codestreams++;
      result = jpx_codestream_target(cs);
    }
  return result;
}

/*****************************************************************************/
/*                            jpx_target::add_layer                          */
/*****************************************************************************/

jpx_layer_target
  jpx_target::add_layer()
{
  jpx_layer_target result;
  if (state != NULL)
    {
      assert(!(state->main_header_complete || state->headers_in_progress));
      jx_layer_target *layer =
        new jx_layer_target(state,state->num_layers);
      if (state->last_layer == NULL)
        state->layers = state->last_layer = layer;
      else
        state->last_layer = state->last_layer->next = layer;
      state->num_layers++;
      result = jpx_layer_target(layer);
    }
  return result;
}

/*****************************************************************************/
/*                       jpx_target::access_composition                      */
/*****************************************************************************/

jpx_composition
  jpx_target::access_composition()
{
  jpx_composition result;
  if (state != NULL)
    result = jpx_composition(&state->composition);
  return result;
}

/*****************************************************************************/
/*                      jpx_target::access_meta_manager                      */
/*****************************************************************************/

jpx_meta_manager
  jpx_target::access_meta_manager()
{
  jpx_meta_manager result;
  if (state != NULL)
    result = jpx_meta_manager(&state->meta_manager);
  return result;
}

/*****************************************************************************/
/*                        jpx_target::write_headers                          */
/*****************************************************************************/

jp2_output_box *
  jpx_target::write_headers(int *i_param, void **addr_param,
                            int codestream_threshold)
{
  assert(state != NULL);
  if ((state->num_codestreams == 0) || (state->num_layers == 0))
    { KDU_ERROR_DEV(e,66); e <<
        KDU_TXT("Attempting to write a JPX file which has no "
        "code-stream, or no compositing layer.");
    }

 
  jx_codestream_target *cp;
  jx_layer_target *lp;

  if (!(state->main_header_complete || state->headers_in_progress))
    {
      // Start by finalizing everything
      state->need_creg_boxes = false;
      for (cp=state->codestreams; cp != NULL; cp=cp->next)
        cp->finalize();
      for (lp=state->layers; lp != NULL; lp=lp->next)
        if (lp->finalize())
          state->need_creg_boxes = true;
      state->composition.finalize(state);

      // Next, initialize the defaults
      state->codestreams->copy_defaults(state->default_dimensions,
                                        state->default_palette,
                                        state->default_component_map);
      state->layers->copy_defaults(state->default_resolution,
                                   state->default_channels,
                                   state->default_colour);

      // Now add compatibility information
      for (cp=state->codestreams; cp != NULL; cp=cp->next)
        cp->adjust_compatibility(&state->compatibility);
      for (lp=state->layers; lp != NULL; lp=lp->next)
        lp->adjust_compatibility(&state->compatibility);
      state->composition.adjust_compatibility(&state->compatibility,state);

      // Now write the initial header boxes
      state->open_top_box(&state->box,jp2_signature_4cc);
      state->box.write(jp2_signature);
      state->box.close();

      state->compatibility.save_boxes(state);
      state->composition.save_box(state);
      state->open_top_box(&state->box,jp2_header_4cc);
      state->default_dimensions.save_boxes(&state->box);
      j2_colour *compat, *cscan;
      for (compat=&state->default_colour; compat != NULL; compat=compat->next)
        if (compat->is_jp2_compatible())
          break;
      if (compat != NULL)
        compat->save_box(&state->box); // Try save JP2 compatible colour first
      for (cscan=&state->default_colour; cscan != NULL; cscan=cscan->next)
        if (cscan != compat)
          cscan->save_box(&state->box);
      state->default_palette.save_box(&state->box);
      state->default_component_map.save_box(&state->box);
      state->default_channels.save_box(&state->box,true);
      state->default_resolution.save_box(&state->box);
      state->box.close();
      state->main_header_complete = true;
    }

  // In case the application accidentally changes the threshold value between
  // incomplete calls to this function, we remember it here.  This ensures
  // that we take up at the correct place again after an application-installed
  // breakpoint
  if (state->headers_in_progress)
    codestream_threshold = state->last_codestream_threshold;
  state->last_codestream_threshold = codestream_threshold;

  state->headers_in_progress = true;

  int idx;
  jp2_output_box *bpt_box=NULL;
  for (idx=0, cp=state->codestreams; cp != NULL; cp=cp->next, idx++)
    {
      if ((codestream_threshold >= 0) && (idx > codestream_threshold))
        break; // We have written enough for now
      if ((!cp->is_chdr_complete()) &&
          ((bpt_box = cp->write_chdr(i_param,addr_param)) != NULL))
        return bpt_box;
    }
  for (lp=state->layers; lp != NULL; lp=lp->next)
    {
      if ((!lp->is_jplh_complete()) &&
          ((bpt_box = lp->write_jplh(state->need_creg_boxes,
                                     i_param,addr_param)) != NULL))
        return bpt_box;
      if ((codestream_threshold >= 0) &&
          lp->uses_codestream(codestream_threshold))
        break;
    }

  state->headers_in_progress = false;
  return NULL;
}

/*****************************************************************************/
/*                        jpx_target::write_metadata                         */
/*****************************************************************************/

jp2_output_box *
  jpx_target::write_metadata(int *i_param, void **addr_param)
{
  if (state->meta_manager.active_metagroup == NULL)
    { // Calling this function for the first time
      if (state->meta_manager.target != NULL)
        { KDU_ERROR_DEV(e,67); e <<
            KDU_TXT("Trying to invoke `jpx_target::write_metadata' "
            "after all metadata has already been written to the file.");
        }
      state->meta_manager.target = state;
    }
  return state->meta_manager.write_metadata(i_param,addr_param);
}
