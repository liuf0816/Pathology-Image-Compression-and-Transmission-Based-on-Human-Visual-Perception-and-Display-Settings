/*****************************************************************************/
// File: kdu_merge.h [scope = APPS/MERGE]
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
   Header file for "kdu_merge.cpp"
******************************************************************************/
#ifndef KDU_MERGE_H
#define KDU_MERGE_H

#include "jpx.h"
#include "mj2.h"

/*****************************************************************************/
/*                               mg_source_spec                              */
/*****************************************************************************/

struct mg_source_spec {
  public: // Member functions
    mg_source_spec()
      {
        filename = NULL;
        num_codestreams = num_layers = num_frames = num_fields = 0;
        field_order = KDU_FIELDS_NONE;
        first_frame_idx = base_codestream_idx = 0;
        next=NULL; codestream_targets = NULL; video_source = NULL;
      }
    ~mg_source_spec()
      {
        if (filename != NULL)
          delete[] filename;
        if (codestream_targets != NULL)
          delete[] codestream_targets;
        jpx_src.close();
        mj2_src.close();
        family_src.close();
      }
  public: // Data
    char *filename;
    jp2_family_src family_src;
    jpx_source jpx_src;
    mj2_source mj2_src;
    mj2_video_source *video_source; // For MJ2 tracks
    int num_codestreams;
    int num_layers; // One layer per codestream for MJ2 sources
    int num_frames; // One frame per layer for JPX sources
    int num_fields; // Number of video fields which can be made from the source
    kdu_field_order field_order; // For MJ2 sources
    int first_frame_idx; // Seeking offset for first used frame in MJ2 track
    int base_codestream_idx; // Index of first codestream as it will appear in
                             // a JPX output codestream
    jpx_codestream_target *codestream_targets; // One for each code-stream
    mg_source_spec *next;
  };

/*****************************************************************************/
/*                               mg_channel_spec                             */
/*****************************************************************************/

struct mg_channel_spec { // For JPX output files
  public: // Member functions
    mg_channel_spec()
      { file=NULL; codestream_idx=component_idx=lut_idx=-1; next=NULL; }
  public: // Data
    mg_source_spec *file;
    int codestream_idx;
    int component_idx;
    int lut_idx;
    mg_channel_spec *next;
  };

/*****************************************************************************/
/*                               mg_layer_spec                               */
/*****************************************************************************/

struct mg_layer_spec { // For JPX output files
  public: // Member functions
    mg_layer_spec()
      { file=NULL; channels=NULL; next=NULL;
        source_layer_idx=num_colour_channels=num_alpha_channels=0; }
    ~mg_layer_spec()
      { mg_channel_spec *cp;
        while ((cp=channels) != NULL)
          { channels=cp->next; delete cp; }
      }
  public: // Members for existing layes
    mg_source_spec *file; // NULL if we are building a layer from scratch
    int source_layer_idx;
  public: // Members for building a layer from scratch
    jp2_colour_space space;
    int num_colour_channels;
    int num_alpha_channels; // At most 1
    mg_channel_spec *channels; // Linked list
  public: // Common members
    mg_layer_spec *next;
    kdu_coords size; // Size of the first code-stream used by the layer; filled
                     // in when writing layers to output file.
  };

/*****************************************************************************/
/*                               mg_track_seg                                */
/*****************************************************************************/

struct mg_track_seg { // For MJ2 output files
  public: // Member functions
    mg_track_seg()
      { from=to=0;  fps=0.0F;  next = NULL; }
  public: // Data
    int from, to;
    float fps;
    mg_track_seg *next;
  };

/*****************************************************************************/
/*                               mg_track_spec                               */
/*****************************************************************************/

struct mg_track_spec { // For MJ2 output files
  public: // Member functions
    mg_track_spec()
      { field_order = KDU_FIELDS_NONE;  segs = NULL;  next = NULL; }
    ~mg_track_spec()
      { mg_track_seg *tmp;
        while ((tmp=segs) != NULL)
          { segs = tmp->next; delete tmp; }
      }
  public: // Data
    kdu_field_order field_order;
    mg_track_seg *segs;
    mg_track_spec *next;
  };

#endif // KDU_MERGE_H
