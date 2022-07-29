/*****************************************************************************/
// File: kdu_merge.cpp [scope = APPS/MERGE]
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
   Utility for merging multiple JP2/JPX files into a single JPX file.
******************************************************************************/
#include <string.h>
#include <stdio.h> // so we can use `sscanf' for arg parsing.
#include <math.h>
#include <assert.h>
#include <iostream>
// Kakadu core includes
#include "kdu_elementary.h"
#include "kdu_messaging.h"
#include "kdu_compressed.h"
// Application includes
#include "kdu_args.h"
#include "kdu_file_io.h"
#include "jpx.h"
#include "kdu_merge.h"

/* ========================================================================= */
/*                         Set up messaging services                         */
/* ========================================================================= */

class kdu_stream_message : public kdu_message {
  public: // Member classes
    kdu_stream_message(std::ostream *stream)
      { this->stream = stream; }
    void put_text(const char *string)
      { (*stream) << string; }
    void flush(bool end_of_message=false)
      { stream->flush(); }
  private: // Data
    std::ostream *stream;
  };

static kdu_stream_message cout_message(&std::cout);
static kdu_stream_message cerr_message(&std::cerr);
static kdu_message_formatter pretty_cout(&cout_message);
static kdu_message_formatter pretty_cerr(&cerr_message);


/* ========================================================================= */
/*                             Internal Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                        print_version                               */
/*****************************************************************************/

static void
  print_version()
{
  kdu_message_formatter out(&cout_message);
  out.start_message();
  out << "This is Kakadu's \"kdu_merge\" application.\n";
  out << "\tCompiled against the Kakadu core system, version "
      << KDU_CORE_VERSION << "\n";
  out << "\tCurrent core system version is "
      << kdu_get_core_version() << "\n";
  out.flush(true);
  exit(0);
}

/*****************************************************************************/
/* STATIC                        print_usage                                 */
/*****************************************************************************/

static void
  print_usage(char *prog, bool comprehensive=false)
{
  kdu_message_formatter out(&cout_message);

  out << "Usage:\n  \"" << prog << " ...\n";
  out.set_master_indent(3);
  out << "-i <JP2/JPX/MJ2 file 1>[:<track>][:<from>-<to>][,...]\n";
  if (comprehensive)
    out << "\tA comma-separated list of one or more input sources, each of "
           "which must be JP2, JPX or MJ2 compatible.  By default, the "
           "output file is constructed by concatenating the compositing "
           "layers from each JP2/JPX input, and the fields (if interlaced) "
           "or frames from each MJ2 input; however, more sophisticated "
           "behaviour may be achieved using the `-jpx_layers' or "
           "`-mj2_tracks' argument.\n"
           "\t   MJ2 file names can be followed by up to two optional "
           "qualifiers, which may be used to identify a particular video "
           "track within the file and/or a particular range of frames "
           "within the track.  The track qualifier, T, if provided, is not "
           "the actual track number within the MJ2 source, since "
           "MJ2 track numbers are arbitrary (not necessarily contiguous) "
           "natural numbers.  Instead, track qualifier values are strictly "
           "sequential.  A track qualifier of T=0 translates to the "
           "video track which has the smallest MJ2 track number; a track "
           "qualifier of T=1 translates to the video track which has "
           "the second smallest MJ2 track number; and so forth.  If no "
           "track qualifier is provided, the effect is the same as "
           "specifying T=0.  The second type of qualifier which may be "
           "provided is the range of frame indices which are to be "
           "extracted from the track in question.  The range consists of "
           "a <from> index and a <to> index.  The first frame has an index "
           "of 0 and the range is inclusive.  Note that the range "
           "refers to frames, not fields; each frame may contain two "
           "field codestreams, if the track happens to be interlaced.\n";
  out << "-o <JPX or MJ2 file>\n";
  if (comprehensive)
    out << "\tName of the JPX or MJ2 file to which the merged result is to be "
           "written.  If the file suffix is anything other than \".mj2\" or "
           "\".mjp2\" (not case sensitive), a JPX file is written.  Although "
           "not enforced here, a JPX file should really be assigned a suffix "
           "of \".jpf\" or \".jpx\", where the former is the official "
           "registered suffix -- \".jpx\" was already in use by some other "
           "registered application when the JPEG2000 project came along.\n";
  out << "-no_interleave -- don't interleave JPX headers with codestreams\n";
  if (comprehensive)
    out << "\tBy default, JPX codestream header boxes and compositing layer "
           "header boxes are interleaved with the codestreams themselves so "
           "as to create a representation which can be read as efficiently "
           "as possible, by applications which wish to render the image "
           "elements in sequence.  This is especially useful, if you are "
           "including a video clip in a JPX file.  If the present argument "
           "is specified, all headers will be written up front, followed by "
           "all the codestreams.  The argument is ignored if the output "
           "is an MJ2 file, rather than a JPX file.\n";
  out << "-links -- record links rather than actual codestream data\n";
  if (comprehensive)
    out << "\tBy default, codestream data from the input files is directly "
           "copied into the merged file.  If this argument is given, "
           "however, the merged file only contains links to the "
           "source codestreams.  This capability is currently supported "
           "only when writing a JPX file, in which case the links are "
           "represented using JPX fragment table (ftbl) boxes.  There "
           "is no fundamental reason, however, why it could not be "
           "supported in the future for MJ2 outputs as well.\n";
  out << "-jpx_layers (<input>:<elt>)|(<space>,[alpha,]<channels>) [...]\n";
  if (comprehensive)
    out << "\tThis argument is to be used only when generating JPX files.  "
           "By default, the original compositing layers from each JP2/JPX "
           "input source, and the codestreams belonging to each MJ2 input "
           "source are simply concatenated to form the output JPX file.  "
           "This argument, however, allows you to reorganize layers, "
           "construct new layers, define layers which share code-stream data, "
           "and even define layers which draw from codestreams in different "
           "original files.  For example, it is possible, to define two "
           "layers which render colour images from different sets of "
           "components in a single code-stream, or to define one layer "
           "which draws its colour and alpha data from different "
           "code-streams.\n"
           "\t   The argument takes one or more separate parameters, each "
           "of which defines a single compositing layer in the final JPX "
           "file.  Each argument takes one of the following two forms:\n"
           "\t\t   a) An input source index (starting from 1) separated by a "
           "colon from the index (starting from 0) of the element from "
           "that source which is to be used for the new compositing layer.  "
           "A JP2 source has one element corresponding to its single "
           "codestream.  The elements of JPX sources are their compositing "
           "layers.  The elements of MJ2 input sources are their codestreams, "
           "subject to the track and range qualifiers which may have "
           "been specified in the \"-i\" argument.  For example, \"1:3\" "
           "includes the fourth layer (or MJ2 codestream) from the first "
           "input source specified via the \"-i\" argument.\n"
           "\t\t   b) A colour space identifier, followed by a "
           "comma-separated list of colour channel specifiers.  Each channel "
           "specifier has the form <input>:<stream>/<component>[+<lut>], "
           "where <input> is the index of the input source (starting from 1), "
           "<stream> is the index of the code-stream within that input source "
           "(starting from 0), <component> is the index of the image "
           "component within that file (starting from 0), and <lut> is an "
           "optional index of a palette lookup table (0 for the first palette "
           "LUT associated with the code-stream) to be used in deriving the "
           "channel samples from the image component.  The <space> parameter "
           "may be any of the following strings:\n"
           "\t\t\t`bilevel1', `bilevel2', `YCbCr1', `YCbCr2', `YCbCr3', "
           "`PhotoYCC', `CMY', `CMYK', `YCCK', `CIELab', `CIEJab', "
           "`sLUM', `sRGB', `sYCC', `esRGB', `esYCC', `ROMMRGB', "
           "`YPbPr60',  `YPbPr50'\n"
           "\t\tA description of each of these enumerated colour spaces may "
           "be found in IS15444-2 (very sketchy) or in the comments "
           "provided with the Kakadu function `jp2_colour::init' (more "
           "comprehensive).\n"
           "\t\t  If the keyword \"alpha\" appears immediately after the "
           "colour space name (including a comma separator), the last "
           "channel specifier identifies an alpha channel.  For example, "
           "\"sLUM,alpha,1:0/0+0,2:0/0\" defines a luminance layer based on "
           "component 0 of code-stream 0 in file 1, mapped through its "
           "palette index 0, with an additional alpha channel based on "
           "component 0 or code-stream 0 in the second input file.\n";
  out << "-mj2_tracks (P|I1|I2):<from>-[<to>][@<fps>][,<from>-...] [...]\n";
  if (comprehensive)
    out << "\tThis argument must be used when generating MJ2 files.  It "
           "takes one or more separate parameter strings, each of which "
           "represents a separate output track to be generated.  Each track "
           "must be designated as either progressive ('P') or interlaced "
           "('I1' or `I2').  The `I1' designation means that the first "
           "field provides the first row of the frame, while `I2' means "
           "that the first frame row is provided by the second field.\n"
           "\t   The components of each track are described by a "
           "comma-separated list of input element ranges and associated "
           "playback speeds.  The <from> and <to> values are zero-based "
           "indices into the collection of frames (not fields) represented by "
           "the set of all input sources supplied to the \"-i\" argument.  "
           "If you omit the <to> parameter, or specify a value which is "
           "larger than the amount of source data which is available, "
           "the generated video track continues until the end of the "
           "available data.  The <fps> value identifies play speed in "
           "frames per second; the reciprocal of this value is assigned "
           "as the duration of each corresponding output frame.  If "
           "you omit this value, timing is based on the source files, "
           "in which case the first input frame at least must be drawn "
           "from an MJ2 file.\n"
           "\t   When building an interlaced output track, all MJ2 source "
           "frames which are used must also be interlaced.  Also, each "
           "frame in the <from>-<to> range which refers to a JP2/JPX "
           "compositing layer, actually refers to two such layers, the "
           "dimensions of which must be compatible with interlacing "
           "(the heights of the two fields may differ by 1 if the interlaced "
           "frame is to have an odd height).  When building a progressive "
           "output track, all MJ2 source frames which are used must also be "
           "progressive.  Apart from these conditions, the dimensions of "
           "all frame components of a track must be compatible and "
           "JPX compositing layers which are used must be representable in "
           "the MJ2 format -- e.g., no multi-codestream layers.  Any "
           "violation of these conditions will result in a terminal error.\n";
  out << "-composit [<iterations>@<fps>*]<layer-descriptor>[,...] [...]\n";
  if (comprehensive)
    out << "\tThis argument may be used only when generating a JPX file.  "
           "You may use it to create a JPX composition box describing "
           "compositions and/or animations based on the compositing layers "
           "which are being written to the file.  Animations are constructed "
           "from a sequence of frame descriptors, where each frame descriptor "
           "consists of a separate command line token.  Each frame descriptor "
           "commences with an integer number of iterations, followed by the "
           "`@' character and then a real-valued number of frames-per-second "
           "at which to play the frame iterations.  This is followed by a "
           "`*' character and then a comma-separated list of layer "
           "descriptors, corresponding to the succession of layers which "
           "are to be composed within each frame.  For a single composition, "
           "the iteration count and frame rate (everything up to and "
           "including the `*' character) may be skipped.  If an <iterations> "
           "value of 0 is supplied, the frame is made persistent, meaning "
           "that it will serve as a background for all successive frames.\n"
           "\t   Each layer descriptor has the following form:\n"
           "\t<layer>[+<inc>][:(<xc>,<yc>,<wc>,<hc>)][@(<x0>,<y0>,<scale>)]\n"
           "\twhere <layer> is the index (starting from 0) of the compositing "
           "layer being written into the file which is to be used; <inc> is "
           "an optional increment to be applied to the initial layer index "
           "after each successive iteration; <xc> and <yc> are real-valued "
           "relative cropping offsets to be applied to the layer, expressed "
           "in the range 0 to 1; <ws> and <hs> are real valued cropped "
           "dimensions, expressed relative to the original width and height "
           "of the layer as fractions in the range 0 to 1; <x0> and <y0> "
           "are horizontal and vertical offsets of the composited layer "
           "from the upper left hand corner of the compositing surface, "
           "expressed as fractions of the layer width and height, in the "
           "range 0 to 1; and <scale> is an integer-valued scale factor, "
           "indicating the amount by which the layer samples are to be "
           "scaled up when placing it on the compositing surface.  You are "
           "strongly recommended to use only power-of-2 scaling factors, "
           "since \"kdu_show\" might otherwise be unable to correctly "
           "align the composited images at certain reduced scales, leading "
           "to some visual confusion.\n"
           "\t   It is OK to provide compositing/animation instructions "
           "for more compositing layers than the number which are actually "
           "written to the file.  In this case, a compliant reader should "
           "process all frames/layers up until the point where there are "
           "no further layers left.\n";
  out << "-album [<seconds/frame>]\n";
  if (comprehensive)
    out << "\tThe use of this command causes a JPX composition box "
           "to be generated, the first frame of which contains all "
           "compositing layers, arranged in a rectangular array, as a "
           "photo album.  This is followed by one frame for each individual "
           "compositing layer, scaled roughly to match the frame.  If some "
           "compositing layers are much smaller than others, they will be "
           "scaled up so that all images on the initial frame appear "
           "with roughly the same size.  You may still add additional frames "
           "to the composition by using the `-composit' argument, but this "
           "would be relatively unusual.  The argument takes one "
           "optional parameter, which indicates the number of seconds "
           "between frames when the album is played as an animation.  "
           "The default value is 5 seconds.\n";
  out << "-s <switch file>\n";
  if (comprehensive)
    out << "\tSwitch to reading arguments from a file.  In the file, argument "
           "strings are separated by whitespace characters, including spaces, "
           "tabs and new-line characters.  Comments may be included by "
           "introducing a `#' or a `%' character, either of which causes "
           "the remainder of the line to be discarded.  Any number of "
           "\"-s\" argument switch commands may be included on the command "
           "line.\n";
  out << "-quiet -- suppress informative messages.\n";
  out << "-version -- print core system version I was compiled against.\n";
  out << "-v -- abbreviation of `-version'\n";
  out << "-usage -- print a comprehensive usage statement.\n";
  out << "-u -- print a brief usage statement.\"\n\n";

  out.flush();
  exit(0);
}

/*****************************************************************************/
/* STATIC                       has_mj2_suffix                               */
/*****************************************************************************/

static bool
  has_mj2_suffix(char *fname)
  /* Returns true if the file-name has the suffix, ".mj2" or ".mjp2", where the
     check is case insensitive. */
{
  char *cp = strrchr(fname,'.');
  if (cp == NULL)
    return false;
  cp++;
  if ((*cp != 'm') && (*cp != 'M'))
    return false;
  cp++;
  if ((*cp != 'j') && (*cp != 'J'))
    return false;
  cp++;
  if (*cp == '2')
    return true;
  if ((*cp != 'p') && (*cp != 'P'))
    return false;
  cp++;
  if (*cp != '2')
    return false;
  return true;
}

/*****************************************************************************/
/* STATIC                     parse_common_args                              */
/*****************************************************************************/

static mg_source_spec *
  parse_common_args(kdu_args &args, char * &ofname,
                    bool &quiet, int &num_inputs, int &num_codestreams,
                    bool &interleave_headers,
                    bool &link_to_codestreams)
  /* Returns a linked list of output layer specifications. */
{
  if ((args.get_first() == NULL) || (args.find("-u") != NULL))
    print_usage(args.get_prog_name());
  if (args.find("-usage") != NULL)
    print_usage(args.get_prog_name(),true);
  if ((args.find("-version") != NULL) || (args.find("-v") != NULL))
    print_version();

  mg_source_spec *inputs = NULL;
  ofname = NULL;
  quiet = false;
  interleave_headers = true;
  link_to_codestreams = false;
  num_inputs = num_codestreams = 0;

  if (args.find("-i") != NULL)
    {
      mg_source_spec *tail=NULL;
      char *string, *cp;
      for (string=args.advance(); string != NULL; string=cp)
        {
          if (tail == NULL)
            inputs = tail = new mg_source_spec;
          else
            tail = tail->next = new mg_source_spec;
          size_t filename_len = strlen(string);
          if ((cp=strchr(string,',')) != NULL)
            {
              filename_len = (size_t)(cp-string);
              cp++;
            }
          tail->filename = new char[filename_len+1];
          memcpy(tail->filename,string,filename_len);
          tail->filename[filename_len] = '\0';
          char *sep = strchr(tail->filename,':');
          if (sep != NULL)
            *sep = '\0';
          tail->family_src.open(tail->filename);
          if (tail->mj2_src.open(&(tail->family_src),true) > 0)
            {
              int rel_track = 0;
              int from = 0;
              int to = INT_MAX;
              // Look for optional track or frame-range qualifiers
              if (sep != NULL)
                {
                  *sep = ':';
                  if (sscanf(sep+1,"%d-%d",&from,&to) != 2)
                    {
                      if (sscanf(sep+1,"%d",&rel_track) != 1)
                        { kdu_error e; e << "Unrecognizable qualifier "
                          "found with MJ2 input specification, \""
                          << tail->filename << "\"."; }
                      char *sep2 = strchr(sep+1,':');
                      if ((sep2 != NULL) &&
                          (sscanf(sep2+1,"%d-%d",&from,&to) != 2))
                        { kdu_error e; e << "Unrecognizable second qualifier "
                          "found with MJ2 input specification, \""
                          << tail->filename << "\"."; }
                    }
                }
              if ((to < from) || (from < 0) || (rel_track < 0))
                { kdu_error e; e << "Invalid track or frame range qualifiers "
                  "found with MJ2 input specification, \""
                  << tail->filename << "\"."; }
              if (sep != NULL)
                *sep = '\0';

              kdu_uint32 track_idx = 0;
              do { // Look for video track
                  track_idx = tail->mj2_src.get_next_track(track_idx);
                  tail->video_source =
                    tail->mj2_src.access_video_track(track_idx);
                  if ((tail->video_source != NULL) && (rel_track > 0))
                    { rel_track--;  tail->video_source = NULL; }
                } while ((track_idx != 0) && (tail->video_source == NULL));
              if (tail->video_source == NULL)
                { kdu_error e; e << "MJ2 video source, \"" << tail->filename
                  << "\", contains no video track matching the requested "
                  "specifications."; }

              tail->num_frames = tail->video_source->get_num_frames();
              if ((tail->num_frames-1) > to)
                tail->num_frames = to+1;
              tail->first_frame_idx = from;
              tail->num_frames -= from;
              if (tail->num_frames < 1)
                { kdu_error e; e << "MJ2 video source, \"" << tail->filename
                  << "\", contains no frames matching the requested "
                  "specifications."; }
              tail->field_order = tail->video_source->get_field_order();
              tail->num_codestreams = tail->num_fields =
                tail->num_frames  * ((tail->field_order==KDU_FIELDS_NONE)?1:2);
              tail->num_layers = tail->num_codestreams;              
            }
          else if (tail->jpx_src.open(&(tail->family_src),true) > 0)
            {
              tail->jpx_src.count_codestreams(tail->num_codestreams);
              tail->jpx_src.count_compositing_layers(tail->num_layers);
              tail->num_frames = tail->num_fields = tail->num_layers;
              tail->field_order = KDU_FIELDS_NONE;
            }
          else
            { kdu_error e; e << "Source file, \"" << tail->filename <<
              "\" does not appear to be compatible with any of the "
              "JP2, JPX or MJ2 file formats."; }
          tail->base_codestream_idx = num_codestreams;
          num_codestreams += tail->num_codestreams;
          num_inputs++;
        }
      args.advance();
    }
  if (num_inputs < 1)
    { kdu_error e;
      e << "You must supply at least one input file using \"-i\"."; }

  if (args.find("-o") != NULL)
    {
      char *string = args.advance();
      if (string == NULL)
        { kdu_error e; e << "\"-o\" argument requires a file name!"; }
      ofname = new char[strlen(string)+1];
      strcpy(ofname,string);
      args.advance();
    }
  else
    { kdu_error e; e << "You must supply an output file using \"-o\"."; }

  if (args.find("-no_interleave") != NULL)
    {
      interleave_headers = false;
      args.advance();
    }
  
  if (args.find("-links") != NULL)
    {
      link_to_codestreams = true;
      args.advance();
    }

  if (args.find("-quiet") != NULL)
    {
      quiet = true;
      args.advance();
    }

  return inputs;
}

/*****************************************************************************/
/* STATIC                      parse_track_specs                             */
/*****************************************************************************/

static mg_track_spec *
  parse_track_specs(kdu_args &args, int &num_tracks,
                    mg_source_spec *inputs, int num_inputs)
{
  num_tracks = 0;
  mg_track_spec *tracks=NULL, *tail=NULL;

  if (args.find("-mj2_tracks") == NULL)
    { kdu_error e; e << "You must supply an \"-mj2_tracks\" argument to "
      "generate an MJ2 output file."; }

  char *string = args.advance();
  for (; (string != NULL) && (*string != '-'); string=args.advance())
    {
      if (tail == NULL)
        tracks = tail = new mg_track_spec;
      else
        tail = tail->next = new mg_track_spec;
      num_tracks++;

      if ((string[0] == 'P') && (string[1] == ':'))
        { tail->field_order = KDU_FIELDS_NONE; string += 2; }
      else if ((string[0] == 'I') && (string[1] == '1') && (string[2] == ':'))
        { tail->field_order = KDU_FIELDS_TOP_FIRST; string += 3; }
      else if ((string[0] == 'I') && (string[1] == '2') && (string[2] == ':'))
        { tail->field_order = KDU_FIELDS_TOP_SECOND; string += 3; }
      else
        { kdu_error e; e << "Malformed track specifier found in "
          "\"-mj2_tracks\" argument at \"" << string << "\".  Expected "
          "progressive/interlaced identifier."; }

      tail->segs = NULL;
      mg_track_seg *seg = NULL;
      do {
          if (seg != NULL)
            string++;
          char *cp;
          int from, to=-1;
          double fps = -1.0;
          if (((from = strtol(string,&cp,10)) < 0) ||
              (cp == string) || (*cp != '-'))
            { kdu_error e; e << "Malformed track  specifier found in "
              "\"-mj2_tracks\" argument at \"" << string << "\".  Expected "
              "range/fps segment with at least a <from> value followed by "
              "a dash."; }
          cp++;
          if ((*cp != '\0') && (*cp != ',') && (*cp != '@'))
            {
              if (((to = strtol(string=cp,&cp,10)) < from) || (cp == string))
                { kdu_error e; e << "Malformed track  specifier found in "
                  "\"-mj2_tracks\" argument at \"" << string << "\".  "
                  "Expected <to> value in range/fps segment."; }
            }
          if ((*cp != '\0') && (*cp != ','))
            {
              if ((*cp != '@') || ((fps = strtod(string=cp+1,&cp)) <= 0.0))
                { kdu_error e; e << "Malformed track  specifier found in "
                  "\"-mj2_tracks\" argument at \"" << string << "\".  "
                  "Expected <fps> value in range/fps segment."; }
            }
          string = cp;
          if (seg == NULL)
            tail->segs = seg = new mg_track_seg;
          else
            seg = seg->next = new mg_track_seg;
          seg->from = from;
          seg->to = to;
          seg->fps = (float) fps;
        } while (*string == ',');

      if (*string != '\0')
        { kdu_error e; e << "Malformed track  specifier found in "
          "\"-mj2_tracks\" argument at \"" << string << "\"."; }
    }

  if (tracks == NULL)
    { kdu_error e; e << "You must supply at least one track specifier "
      "with the \"-mj2_tracks\" argument."; }

  return tracks;
}

/*****************************************************************************/
/* STATIC                     create_layer_specs                             */
/*****************************************************************************/

static mg_layer_spec *
  create_layer_specs(kdu_args &args, int &num_layers,
                     mg_source_spec *inputs, int num_inputs)
{
  mg_layer_spec *layers=NULL;
  num_layers = 0;

  if (args.find("-jpx_layers") != NULL)
    {
      int file_idx;
      char *cp, *string = args.advance();
      mg_layer_spec *tail=NULL;
      for (; (string != NULL) && (*string != '-'); string=args.advance())
        {
          if (tail == NULL)
            layers = tail = new mg_layer_spec;
          else
            tail = tail->next = new mg_layer_spec;
          num_layers++;
          if (((file_idx=strtol(string,&cp,10)) > 0) && (cp > string) &&
              (*cp == ':'))
            { // Using an existing layer
              if (file_idx > num_inputs)
                { kdu_error e; e << "Layer specification " << num_layers
                  << " supplied with the \"-jpx_layers\" argument refers to "
                  "a non-existent input file!"; }
              for (tail->file=inputs; file_idx > 1; file_idx--)
                tail->file = tail->file->next;
              string = cp+1;
              tail->source_layer_idx = strtol(string,&cp,10);
              if ((cp == string) || (*cp != '\0') ||
                  (tail->source_layer_idx < 0) ||
                  (tail->source_layer_idx >= tail->file->num_layers))
                { kdu_error e; e << "Layer specification " << num_layers
                  << " supplied with the \"-jpx_layers\" argument refers to "
                  "a non-existent compositing layer in a source file."; }
            }
          else
            { // Building a new layer from scratch
              cp = strchr(string,',');
              if (cp == NULL)
                { kdu_error e; e << "Expected a colour space type, followed "
                  "by a comma-separated list of channel specifiers in "
                  "layer specification " << num_layers
                  << " of the `-jpx_space' argument."; }
              *cp = '\0';
              if (strcmp(string,"bilevel1") == 0)
                tail->space = JP2_bilevel1_SPACE;
              else if (strcmp(string,"bilevel2") == 0)
                tail->space = JP2_bilevel2_SPACE;
              else if (strcmp(string,"YCbCr1") == 0)
                tail->space = JP2_YCbCr1_SPACE;
              else if (strcmp(string,"YCbCr2") == 0)
                tail->space = JP2_YCbCr2_SPACE;
              else if (strcmp(string,"YCbCr3") == 0)
                tail->space = JP2_YCbCr3_SPACE;
              else if (strcmp(string,"PhotoYCC") == 0)
                tail->space = JP2_PhotoYCC_SPACE;
              else if (strcmp(string,"CMY") == 0)
                tail->space = JP2_CMY_SPACE;
              else if (strcmp(string,"CMYK") == 0)
                tail->space = JP2_CMYK_SPACE;
              else if (strcmp(string,"YCCK") == 0)
                tail->space = JP2_YCCK_SPACE;
              else if (strcmp(string,"CIELab") == 0)
                tail->space = JP2_CIELab_SPACE;
              else if (strcmp(string,"CIEJab") == 0)
                tail->space = JP2_CIEJab_SPACE;
              else if (strcmp(string,"sLUM") == 0)
                tail->space = JP2_sLUM_SPACE;
              else if (strcmp(string,"sRGB") == 0)
                tail->space = JP2_sRGB_SPACE;
              else if (strcmp(string,"sYCC") == 0)
                tail->space = JP2_sYCC_SPACE;
              else if (strcmp(string,"esRGB") == 0)
                tail->space = JP2_esRGB_SPACE;
              else if (strcmp(string,"esYCC") == 0)
                tail->space = JP2_esYCC_SPACE;
              else if (strcmp(string,"ROMMRGB") == 0)
                tail->space = JP2_ROMMRGB_SPACE;
              else if (strcmp(string,"YPbPr60_SPACE") == 0)
                tail->space = JP2_YPbPr60_SPACE;
              else if (strcmp(string,"YPbPr50_SPACE") == 0)
                tail->space = JP2_YPbPr50_SPACE;
              else
                { kdu_error e; e << "Unrecognized colour space type, \""
                  << string << "\", in layer specication " << num_layers
                  << " of the `-jpx_space' argument."; }
              string = cp+1;
              if (strncmp(string,"alpha,",strlen("alpha,")) == 0)
                {
                  tail->num_alpha_channels++; tail->num_colour_channels--;
                  string += strlen("alpha,");
                }
              mg_channel_spec *last_channel=NULL;
              while (*string != '\0')
                {
                  if (*string == ',')
                    { string++; continue; }
                  tail->num_colour_channels++;
                  if (last_channel == NULL)
                    last_channel = tail->channels = new mg_channel_spec;
                  else
                    last_channel = last_channel->next = new mg_channel_spec;
                  file_idx = strtol(string,&cp,10);
                  if ((file_idx < 1) || (file_idx > num_inputs) ||
                      (cp == string) || (*cp != ':'))
                    { kdu_error e; e << "Invalid file identifier supplied "
                      "in channel specifier associated with layer "
                      "specification " << num_layers << " of the `-jpx_space' "
                      "argument.  Problem occurred at:\n\t\""
                      << string << "\"."; }
                  for (last_channel->file=inputs; file_idx > 1; file_idx--)
                    last_channel->file = last_channel->file->next;
                  string = cp+1;
                  last_channel->codestream_idx = strtol(string,&cp,10);
                  if ((cp == string) || (*cp != '/') ||
                      (last_channel->codestream_idx < 0) ||
                      (last_channel->codestream_idx >=
                       last_channel->file->num_codestreams))
                    { kdu_error e; e << "Layer specification " << num_layers
                      << " supplied with the \"-jpx_layers\" argument "
                      "contains a channel specification with an invalid "
                      "code-stream identifier.  Problem encountered at:\n\t\""
                      << string << "\"."; }
                  string = cp+1;
                  last_channel->component_idx = strtol(string,&cp,10);
                  if ((cp == string) || (last_channel->component_idx < 0))
                    { kdu_error e; e << "Layer specification " << num_layers
                      << " supplied with the \"-jpx_layers\" argument "
                      "contains a channel specification with an invalid "
                      "component identifier.  Problem encountered at:\n\t\""
                      << string << "\"."; }
                  string = cp;
                  if (*string == '+')
                    {
                      string++;
                      last_channel->lut_idx = strtol(string,&cp,10);
                      if ((cp == string) || (last_channel->lut_idx < 0))
                        { kdu_error e; e << "Layer specification "
                          << num_layers << " supplied with the "
                          "\"-jpx_layers\" argument contains a channel "
                          "specification with an invalid palette LUT index.  "
                          "Problem encountered at:\n\t\"" << string << "\"."; }
                      string = cp;
                    }
                  if ((*string != '\0') && (*string != ','))
                    { kdu_error e; e << "Layer specification " << num_layers
                      << " supplied with the \"-jpx_layers\" argument "
                      "contains a channel specification which cannot be "
                      "completely parsed.  Problem encountered at:\n\t\""
                      << string << "\"."; }
                }
              if (tail->num_colour_channels < 1)
                { kdu_error e; e << "Insufficient channel specifiers found "
                  "in layer specification " << num_layers
                  << " of the `-jpx_space' argument."; }
            }
        }
      if (layers == NULL)
        { kdu_error e; e << "The \"-jpx_layers\" argument must be followed "
          "by at least one layer specification string."; }
    }
  else
    { // Create the default set of compositing layers
      mg_source_spec *in;
      mg_layer_spec *tail=NULL;
      for (in=inputs; in != NULL; in=in->next)
        {
          num_layers += in->num_layers;
          for (int l=0; l < in->num_layers; l++)
            {
              if (tail == NULL)
                layers = tail = new mg_layer_spec;
              else
                tail = tail->next = new mg_layer_spec;
              tail->file = in;
              tail->source_layer_idx = l;
            }
        }
    }

  return layers;
}

/*****************************************************************************/
/* STATIC                    generate_album_frame                            */
/*****************************************************************************/

static void
  generate_album_frame(jpx_composition composition, mg_layer_spec *layer_specs,
                       float seconds_per_frame)
{
  if (layer_specs == NULL)
    return;

  if (seconds_per_frame > (0.001F*INT_MAX))
    seconds_per_frame = 0.001F*INT_MAX;
  int frame_duration = (int)(0.5F + 1000.0F * seconds_per_frame);
  if (frame_duration < 1)
    frame_duration = 1;

  // Start by determining the album dimensions
  mg_layer_spec *lp;
  int num_layers=0;
  kdu_coords max_size;
  for (lp=layer_specs; lp != NULL; lp=lp->next)
    {
      num_layers++;
      if (lp->size.x > max_size.x)
        max_size.x = lp->size.x;
      if (lp->size.y > max_size.y)
        max_size.y = lp->size.y;
    }
  int surround = ((max_size.x > max_size.y)?max_size.x:max_size.y) >> 4;
  int layers_per_row = 1;
  while ((layers_per_row*layers_per_row) < num_layers)
    layers_per_row++;
  
  // Now create the cover frame and its instructions

  jx_frame *frame = composition.add_frame(frame_duration,0,false);
  kdu_dims target_dims, source_dims;
  int layer_idx, row_idx, col_idx;
  kdu_coords lim, cover_size;
  for (layer_idx=row_idx=col_idx=0, lp=layer_specs;
       lp != NULL;
       lp=lp->next, col_idx++, layer_idx++)
    {
      target_dims.size = source_dims.size = lp->size;
      source_dims.pos = kdu_coords(0,0);
      if (col_idx == layers_per_row)
        { col_idx = 0; row_idx++; }
      target_dims.pos.x = col_idx*(max_size.x+surround);      
      target_dims.pos.y = row_idx*(max_size.y+surround);

      double sc_x = ((double) max_size.x) / target_dims.size.x;
      double sc_y = ((double) max_size.y) / target_dims.size.y;
      if (sc_x < sc_y)
        {
          target_dims.size.x = max_size.x;
          target_dims.size.y = (int) (target_dims.size.y * sc_x + 0.5);
        }
      else
        {
          target_dims.size.y = max_size.y;
          target_dims.size.x = (int) (target_dims.size.x * sc_y + 0.5);
        }

      composition.add_instruction(frame,layer_idx,0,source_dims,target_dims);
      lim = target_dims.pos + target_dims.size;
      if (lim.x > cover_size.x)
        cover_size.x = lim.x;
      if (lim.y > cover_size.y)
        cover_size.y = lim.y;
    }

  // Finally, create frames for each individual layer
  for (layer_idx=0, lp=layer_specs; lp != NULL; lp=lp->next, layer_idx++)
    {
      target_dims.size = source_dims.size = lp->size;
      source_dims.pos = target_dims.pos = kdu_coords(0,0);

      double scale = ((double) cover_size.x) / lp->size.x;
      target_dims.size.x = cover_size.x;
      target_dims.size.y = (int) (lp->size.y * scale + 0.5);

      frame = composition.add_frame(frame_duration,0,false);
      composition.add_instruction(frame,layer_idx,0,source_dims,target_dims);
    }
}

/*****************************************************************************/
/* STATIC               parse_and_write_composition_info                     */
/*****************************************************************************/

static void
  parse_and_write_composition_info(kdu_args &args, jpx_composition composition,
                                   mg_layer_spec *layer_specs)
{
  if (args.find("-composit") == NULL)
    return;

  char *cp, *delim, *string;
  while (((string = args.advance()) != NULL) && (*string != '-'))
    {
      int duration=INT_MAX, repeat_count=0;
      bool persistent = false;
      if ((delim = strchr(string,'*')) != NULL)
        { // Have repeat/frame rate spec
          float fps;
          if ((sscanf(string,"%d@%f*",&repeat_count,&fps) != 2) ||
              (repeat_count < 0))
            { kdu_error e; e << "Malformed iterations/fps fields in a "
              "frame specification supplied with the `-composit' argument.  "
              "Frame specification is\n\t   \"" << string << "\"."; }
          if (repeat_count == 0)
            persistent = true;
          else
            repeat_count--;
          if (fps > 0.00001F)
            duration = (int)(0.5F + 1000.0F / fps);
          if (duration < 1)
            duration = 1;
          string = delim+1;
        }
      jx_frame *frame =
        composition.add_frame(duration,repeat_count,persistent);
      while (*string != '\0')
        {
          int layer_idx = strtol(string,&cp,10);
          if ((cp == string) || (layer_idx < 0))
            { kdu_error e; e << "Illegal or missing layer index found "
              "while attempting to parse layer specification at\n\t  \""
              << string << "\"."; }
          string = cp;
          int increment = 0;
          if (*string == '+')
            {
              string++;
              increment = strtol(string,&cp,10);
              if (string == cp)
                { kdu_error e; e << "Expected integer-valued increment "
                  "when parsing layer specification at\n\t  \""
                  << string << "\"."; }
              string = cp;
            }

          kdu_dims source_dims, target_dims;
          mg_layer_spec *lp;
          int n;
          for (lp=layer_specs, n=layer_idx; (n > 0) && (lp != NULL); n--)
            lp = lp->next;
          if (lp == NULL)
            { kdu_error e; e << "Non-existent compositing layer referenced "
              "by `-composit' argument."; }
          target_dims.size = source_dims.size = lp->size;

          if (*string == ':')
            {
              float xc, yc, wc, hc;
              if ((sscanf(string+1,"(%f,%f,%f,%f)",&xc,&yc,&wc,&hc) != 4) ||
                  (xc < 0.0F) || (yc < 0.0F) || (wc <= 0.0F) || (hc <= 0.0F) ||
                  ((xc+wc) > 1.0F) || ((yc+hc) > 1.0F))
                { kdu_error e; e << "Malformed cropping spec found "
                  "in layer specification.  Problem encountered at\n\t  \""
                  << string << "\"."; }
              string = strchr(string,')');
              assert(string != NULL);
              string++;
              source_dims.pos.x = (int) (xc*source_dims.size.x);
              source_dims.pos.y = (int) (yc*source_dims.size.y);
              if (source_dims.pos.x > source_dims.size.x)
                source_dims.pos.x = source_dims.size.x;
              if (source_dims.pos.y > source_dims.size.y)
                source_dims.pos.y = source_dims.size.y;
              kdu_coords max_size = source_dims.size - source_dims.pos;
              source_dims.size.x = (int) (wc*source_dims.size.x);
              source_dims.size.y = (int) (hc*source_dims.size.y);
              if (max_size.x < source_dims.size.x)
                source_dims.size.x = max_size.x;
              if (max_size.y < source_dims.size.y)
                source_dims.size.y = max_size.y;
              target_dims.size = source_dims.size;
            }

          if (*string == '@')
            {
              float x0, y0;
              int scale;
              if ((sscanf(string+1,"(%f,%f,%d)",&x0,&y0,&scale) != 3) ||
                  (x0 < 0.0F) || (y0 < 0.0F) || (scale < 1))
                { kdu_error e; e << "Malformed target scaling/offset found "
                  "in layer specification.  Problem encountered at\n\t  \""
                  << string << "\"."; }
              string = strchr(string,')');
              assert(string != NULL);
              string++;
              target_dims.size.x *= scale;
              target_dims.size.y *= scale;
              target_dims.pos.x = (int) (x0*target_dims.size.x);
              target_dims.pos.y = (int) (y0*target_dims.size.y);
            }

          composition.add_instruction(frame,layer_idx,increment,
                                      source_dims,target_dims);
          if (*string == ',')
            string++;
          else if (*string != '\0')
            { kdu_error e; e << "Malformed text encountered in layer "
              "specification.  Problem encountered at\n\t  \""
              << string << "\"."; }
        }
    }
}

/*****************************************************************************/
/* STATIC                  create_codestream_headers                         */
/*****************************************************************************/

static void
  create_codestream_headers(jpx_target &out, mg_source_spec *in)
{
  in->codestream_targets = new jpx_codestream_target[in->num_codestreams];
  for (int c=0; c < in->num_codestreams; c++)
    {
      jp2_dimensions in_dims;
      jp2_palette palette;
      if (in->video_source != NULL)
        {
          in_dims = in->video_source->access_dimensions();
          palette = in->video_source->access_palette();
        }
      else
        {
          jpx_codestream_source src = in->jpx_src.access_codestream(c);
          palette = src.access_palette();
          in_dims = src.access_dimensions(true);
        }
      jpx_codestream_target tgt = out.add_codestream();
      in->codestream_targets[c] = tgt;
      tgt.access_palette().copy(palette);
      if (in->field_order == KDU_FIELDS_NONE)
        tgt.access_dimensions().copy(in_dims);
      else
        { // Need to convert frame dimensions to field dimensions
          assert(in->video_source != NULL);
          kdu_coords size = in_dims.get_size();
          if (in->field_order == KDU_FIELDS_TOP_FIRST)
            size.y = (size.y + 1 - (c & 1)) >> 1;
          else
            size.y = (size.y + (c & 1)) >> 1;
          jp2_dimensions out_dims = tgt.access_dimensions();
          int n, num_components = in_dims.get_num_components();
          out_dims.init(size,num_components,
                        in_dims.colour_space_known(),
                        in_dims.get_compression_type());
          out_dims.finalize_compatibility(in_dims);
          for (n=0; n < num_components; n++)
            out_dims.set_precision(n,in_dims.get_bit_depth(n),
                                   in_dims.get_signed(n));
        }
    }
}

/*****************************************************************************/
/* STATIC                      copy_source_layer                             */
/*****************************************************************************/

static kdu_coords
  copy_source_layer(jpx_layer_target tgt, mg_source_spec *file,
                    int source_layer_idx)
  /* Copies the indicated layer from the supplied `file' to the `tgt'
     compositing layer, adding `file->base_codestream_idx' to the code-stream
     indices of all channels, to account for the fact that the output file
     may contain other codestreams prior to those included
     from the source file.  Returns the size of the layer. */
{
  jpx_layer_source jpx_src;
  if (file->video_source == NULL)
    jpx_src = file->jpx_src.access_layer(source_layer_idx);

  int n=0;
  jp2_colour colour_src, colour_tgt;
  while ((jpx_src.exists() &&
          (colour_src=jpx_src.access_colour(n)).exists()) ||
         ((n == 0) &&
          (colour_src=file->video_source->access_colour()).exists()))
    {
      colour_tgt = tgt.add_colour(colour_src.get_precedence(),
                                  colour_src.get_approximation_level());
      colour_tgt.copy(colour_src);
      n++;
    }

  if (jpx_src.exists())
    tgt.access_resolution().copy(jpx_src.access_resolution());
  else
    tgt.access_resolution().copy(file->video_source->access_resolution());

  jp2_channels channels_src;
  if (jpx_src.exists())
    channels_src = jpx_src.access_channels();
  else
    channels_src = file->video_source->access_channels();
  jp2_channels channels_tgt=tgt.access_channels();
  int num_colours = channels_src.get_num_colours();
  channels_tgt.init(num_colours);
  int codestream_offset = file->base_codestream_idx;
  if (!jpx_src)
    codestream_offset += source_layer_idx;
  for (n=0; n < num_colours; n++)
    {
      int comp_idx, lut_idx, stream_idx;
      kdu_int32 key_val;
      if (channels_src.get_colour_mapping(n,comp_idx,lut_idx,stream_idx))
        channels_tgt.set_colour_mapping(n,comp_idx,lut_idx,
                                        stream_idx+codestream_offset);
      if (channels_src.get_opacity_mapping(n,comp_idx,lut_idx,stream_idx))
        channels_tgt.set_opacity_mapping(n,comp_idx,lut_idx,
                                         stream_idx+codestream_offset);
      if (channels_src.get_premult_mapping(n,comp_idx,lut_idx,stream_idx))
        channels_tgt.set_premult_mapping(n,comp_idx,lut_idx,
                                         stream_idx+codestream_offset);
      if (channels_src.get_chroma_key(n,key_val))
        channels_tgt.set_chroma_key(n,key_val);
    }

  if (jpx_src.exists())
    {
      int num_streams = jpx_src.get_num_codestreams();
      for (n=0; n < num_streams; n++)
        {
          kdu_coords alignment, sampling, denominator;
          int codestream_idx =
            jpx_src.get_codestream_registration(n,alignment,sampling,
                                                denominator);
          codestream_idx += codestream_offset;
          tgt.set_codestream_registration(codestream_idx,alignment,sampling,
                                          denominator);
        }
      return jpx_src.get_layer_size();
    }
  else
    {
      kdu_coords layer_size =
        file->video_source->access_dimensions().get_size();
      if (file->field_order != KDU_FIELDS_NONE)
        { // Convert frame dimensions into field dimensions, and then
          // double the height to produce a compositing layer which will
          // be rendered at the correct size.
          if (file->field_order == KDU_FIELDS_TOP_FIRST)
            layer_size.y = (layer_size.y + 1 - (source_layer_idx & 1)) >> 1;
          else
            layer_size.y = (layer_size.y + (source_layer_idx & 1)) >> 1;
          kdu_coords field_scaling;
          field_scaling.y = 2;  field_scaling.x = 1;
          tgt.set_codestream_registration(codestream_offset,kdu_coords(0,0),
                                          field_scaling,kdu_coords(1,1));
          layer_size.y *= field_scaling.y;
        }
      return layer_size;
    }
}

/*****************************************************************************/
/* STATIC                       create_new_layer                             */
/*****************************************************************************/

static kdu_coords
  create_new_layer(jpx_layer_target tgt, mg_layer_spec *spec)
{
  tgt.add_colour().init(spec->space);

  int n;
  jp2_channels channels = tgt.access_channels();
  channels.init(spec->num_colour_channels);
  mg_channel_spec *cp = spec->channels;
  for (n=0; n < spec->num_colour_channels; n++, cp=cp->next)
    channels.set_colour_mapping(n,cp->component_idx,cp->lut_idx,
                            cp->codestream_idx+cp->file->base_codestream_idx);
  if (spec->num_alpha_channels)
    {
      assert(cp != NULL);
      for (n=0; n < spec->num_colour_channels; n++)
        channels.set_opacity_mapping(n,cp->component_idx,cp->lut_idx,
                            cp->codestream_idx+cp->file->base_codestream_idx);
    }
  kdu_coords ref_size, layer_size;
  for (cp=spec->channels; cp != NULL; cp=cp->next)
    {
      kdu_coords codestream_size;
      if (cp->file->video_source != NULL)
        {
          codestream_size =
            cp->file->video_source->access_dimensions().get_size();
          if (cp->file->field_order == KDU_FIELDS_TOP_FIRST)
            codestream_size.y =
              (codestream_size.y + 1 - (cp->codestream_idx & 1)) >> 1;
          else if (cp->file->field_order == KDU_FIELDS_TOP_SECOND)
            codestream_size.y =
              (codestream_size.y + (cp->codestream_idx & 1)) >> 1;
          if (cp == spec->channels)
            {
              ref_size = codestream_size;
              if (cp->file->field_order != KDU_FIELDS_NONE)
                ref_size.y *= 2;
            }
        }
      else
        {
          jpx_codestream_source codestream =
            cp->file->jpx_src.access_codestream(cp->codestream_idx);
          codestream_size = codestream.access_dimensions().get_size();
          if (cp == spec->channels)
            ref_size = codestream_size;
        }

      kdu_coords denominator(16,16);
      kdu_coords sampling = denominator;
      if (cp == spec->channels)
        layer_size = ref_size;
      else
        {
          if (ref_size.x > codestream_size.x)
            sampling.x = (int)
              ((((kdu_long) ref_size.x) * denominator.x) / codestream_size.x);
          if (ref_size.y > codestream_size.y)
            sampling.y = (int)
              ((((kdu_long) ref_size.y) * denominator.y) / codestream_size.y);
          codestream_size.x = (int)
            ((((kdu_long) codestream_size.x) * sampling.x) / denominator.x);
          codestream_size.y = (int)
            ((((kdu_long) codestream_size.y) * sampling.y) / denominator.y);
          if (codestream_size.x < layer_size.x)
            layer_size.x = codestream_size.x;
          if (codestream_size.y < layer_size.y)
            layer_size.y = codestream_size.y;
        }
      tgt.set_codestream_registration(
                           cp->codestream_idx+cp->file->base_codestream_idx,
                           kdu_coords(0,0),sampling,denominator);
    }
  return layer_size;
}

/*****************************************************************************/
/* STATIC             write_jpx_headers_and_codestreams                      */
/*****************************************************************************/

static void
  write_jpx_headers_and_codestreams(jpx_target &out,
                                    mg_source_spec *in,
                                    bool interleave_headers,
                                    bool link_to_codestreams)
{
  if (!interleave_headers)
    out.write_headers();
  kdu_byte *buf = new kdu_byte[1<<16];
  int c, next_codestream_idx = 0;
  for (; in != NULL; in=in->next)
    {
      if (in->video_source != NULL)
        in->video_source->seek_to_frame(in->first_frame_idx);
      int num_bytes;
      for (c=0; c < in->num_codestreams; c++, next_codestream_idx++)
        {
          if (interleave_headers)
            out.write_headers(NULL,NULL,next_codestream_idx);

          jpx_codestream_target tgt = in->codestream_targets[c];
          jp2_input_box *box_in=NULL;
          jp2_output_box *box_out=NULL;
          if (!link_to_codestreams)
            box_out = tgt.open_stream();
          jpx_fragment_list frags_in;
          jp2_data_references data_refs_in;

          if (in->video_source != NULL)
            {
              if (in->video_source->open_image() < 0)
                { kdu_error e; e << "Unable to open all video frames "
                  "whose existence is indicated within an MJ2 input file."; }
              box_in = in->video_source->access_image_box();
            }
          else
            {
              jpx_codestream_source src = in->jpx_src.access_codestream(c);
              if (link_to_codestreams)
                {
                  frags_in = src.access_fragment_list();
                  data_refs_in = in->jpx_src.access_data_references();
                }
              if (!(frags_in.exists() && data_refs_in.exists()))
                box_in = src.open_stream();
            }

          if (link_to_codestreams)
            { // Write link
              kdu_long offset, length;
              if (frags_in.exists() && data_refs_in.exists())
                {
                  int frag_idx, num_frags = frags_in.get_num_fragments();
                  for (frag_idx=0; frag_idx < num_frags; frag_idx++)
                    {
                      int url_idx;
                      const char *url_path;
                      if (frags_in.get_fragment(frag_idx,url_idx,
                                                offset,length) &&
                          ((url_path=data_refs_in.get_url(url_idx)) != NULL))
                        tgt.add_fragment(url_path,offset,length);
                    }
                }
              else
                {
                  jp2_locator loc = box_in->get_locator();
                  offset=loc.get_file_pos() + box_in->get_box_header_length();
                  length = box_in->get_remaining_bytes();
                  if (length < 0)
                    for (length=0; (num_bytes=box_in->read(buf,1<<16)) > 0; )
                      length += num_bytes;
                  tgt.add_fragment(in->filename,offset,length);
                }
              tgt.write_fragment_table();
            }
          else
            {
              if (box_in->get_remaining_bytes() < 0)
                box_out->write_header_last();
              else
                box_out->set_target_size(box_in->get_remaining_bytes());
            }

          if (box_out != NULL)
            {
              while ((num_bytes = box_in->read(buf,1<<16)) > 0)
                box_out->write(buf,num_bytes);
              box_out->close();
            }
          if (box_in != NULL)
            box_in->close();
          if (in->video_source != NULL)
            in->video_source->close_image();
        }
    }
  delete[] buf;

  if (interleave_headers)
    out.write_headers(); // Write any outstanding headers
}

/*****************************************************************************/
/* STATIC                      copy_descendants                              */
/*****************************************************************************/

static void
  copy_descendants(jpx_metanode src, jpx_metanode dst,
                   int num_src_streams, int *src_stream_map, int *stream_temp,
                   int num_dst_layers, int *dst_layer_map, int *layer_temp,
                   jpx_meta_manager dst_manager, bool inside_numlist)
  /* This recursive function copies all descendants of the `src' node to
     become descendants of the `dst' node.  Wherever a number list node is
     encountered, its entries are transcribed using the stream and layer
     mappings provided via the 3'rd, 4'th, 6'th and 7'th arguments.
     Specifically, a codestream index n in the input number list node is
     transcribed to an output codestream index of `src_stream_map[n]',
     unless it is negative, meaning that the source codestream is not used.
     Layer indices are treated somewhat differently, since a single source
     layer may potentially be used by multiple destination layers.  A
     compositing layer index of n in the input number list node is
     transcribed to the union of all indices, k, such that
     `dst_layer_map[k]'=n.
        If an input number list is associated with the composited result
     rather than a specific codestream or compositing layer, this association
     will not be passed through to the output number list, since its
     interpretation is unclear.
        If the output number list would be empty, neither that
     node, nor any of its descendants are copied.
        The `stream_temp' and `layer_temp' arrays are provided for working
     storage, allowing the function to create lists of translated codestream
     and compositing layer indices for number lists.
        The `dst_manager' object's `jpx_meta_manager::insert_node' function
     is used to create number lists so that common number lists can be
     shared.
        The `inside_numlist' argument indicates whether or not a number list
     has been seen in the ancestry of the node being copied.  If not, a
     special number list is automatically created to contain metadata which
     was global to the source.  This special number list references all
     layers and codestreams from the original source which are used by the
     output JPX file. */
{
  bool rendered_result;
  int n, d, num_descendants, num_streams, num_layers;
  src.count_descendants(num_descendants);
  if (num_descendants == 0)
    return;
  for (d=0; d < num_descendants; d++)
    {
      jpx_metanode nd, ns = src.get_descendant(d);
      if (ns.get_numlist_info(num_streams,num_layers,rendered_result))
        {
          int k, num_xlt_streams=0, num_xlt_layers=0;
          const int *orig_streams = ns.get_numlist_codestreams();
          const int *orig_layers = ns.get_numlist_layers();
          for (n=0; n < num_streams; n++)
            {
              k = orig_streams[n];
              if ((k >= 0) && (k < num_src_streams) &&
                  (src_stream_map[k] >= 0))
                stream_temp[num_xlt_streams++] = src_stream_map[k];
            }
          for (n=0; n < num_dst_layers; n++)
            {
              for (k=0; k < num_layers; k++)
                if (orig_layers[k] == dst_layer_map[n])
                  {
                    layer_temp[num_xlt_layers++] = n;
                    break;
                  }
            }
          nd = dst_manager.insert_node(num_xlt_streams,stream_temp,
                                       num_xlt_layers,layer_temp,false,
                                       0,NULL);
          copy_descendants(ns,nd,num_src_streams,src_stream_map,stream_temp,
                           num_dst_layers,dst_layer_map,layer_temp,
                           dst_manager,true);
        }
      else if ((ns.get_box_type() == jp2_free_4cc) ||
               (ns.get_box_type() == jp2_mdat_4cc))
        { // These need not be copied; only their descendants
          copy_descendants(ns,dst,num_src_streams,src_stream_map,stream_temp,
                           num_dst_layers,dst_layer_map,layer_temp,
                           dst_manager,inside_numlist);
        }
      else
        { // All other node types get copied, but first see if we need to
          // place them inside a special number list
          jpx_metanode container = dst;
          if (!inside_numlist)
            {
              for (n=num_streams=0; n < num_src_streams; n++)
                if (src_stream_map[n] >= 0)
                  stream_temp[num_streams++] = src_stream_map[n];
              for (n=num_layers=0; n < num_dst_layers; n++)
                if (dst_layer_map[n] >= 0)
                  layer_temp[num_layers++] = n;
              container = dst_manager.insert_node(num_streams,stream_temp,
                                                  num_layers,layer_temp,
                                                  false,0,NULL);
            }
          nd = container.add_copy(ns,false);
          copy_descendants(ns,nd,num_src_streams,src_stream_map,stream_temp,
                           num_dst_layers,dst_layer_map,layer_temp,
                           dst_manager,true);
        }
    }
}

/*****************************************************************************/
/* STATIC                        copy_metadata                               */
/*****************************************************************************/

static void
  copy_metadata(jpx_meta_manager meta_in, jpx_meta_manager meta_out,
                mg_source_spec *in, mg_layer_spec *layer_specs)
{
  mg_layer_spec *lp;
  int n, num_dst_layers, num_src_streams;
  int *dst_layer_map, *src_stream_map, *layer_temp, *stream_temp;

  num_src_streams = in->num_codestreams;
  src_stream_map = new int[num_src_streams];
  stream_temp = new int[num_src_streams];
  for (n=0; n < num_src_streams; n++)
    if (in->codestream_targets[n].exists())
      src_stream_map[n] = in->codestream_targets[n].get_codestream_id();
    else
      src_stream_map[n] = -1;

  num_dst_layers = 0;
  for (lp=layer_specs; lp != NULL; lp=lp->next)
    num_dst_layers++;
  dst_layer_map = new int[num_dst_layers];
  layer_temp = new int[num_dst_layers];
  for (n=0, lp=layer_specs; lp != NULL; lp=lp->next, n++)
    if (lp->file == in)
      dst_layer_map[n] = lp->source_layer_idx;
    else
      dst_layer_map[n] = -1; // This destination layer does not use this source
  copy_descendants(meta_in.access_root(),meta_out.access_root(),
                   num_src_streams,src_stream_map,stream_temp,
                   num_dst_layers,dst_layer_map,layer_temp,meta_out,false);
  delete[] dst_layer_map;
  delete[] layer_temp;
  delete[] src_stream_map;
  delete[] stream_temp;
}

/*****************************************************************************/
/* STATIC                       generate_jpx                                 */
/*****************************************************************************/

static void
  generate_jpx(jp2_family_tgt *family_tgt, kdu_args &args,
               mg_source_spec *inputs, int num_inputs, int num_codestreams,
               bool interleave_headers, bool link_to_codestreams,
               bool quiet)
{
  int num_layers=0;
  mg_layer_spec *layer, *layer_specs =
    create_layer_specs(args,num_layers,inputs,num_inputs);

  jpx_target out;
  out.open(family_tgt);

  // Create composition layers
  for (layer=layer_specs; layer != NULL; layer=layer->next)
    {
      jpx_layer_target tgt = out.add_layer();
      if (layer->file != NULL)
        layer->size =
          copy_source_layer(tgt,layer->file,layer->source_layer_idx);
      else
        layer->size = create_new_layer(tgt,layer);
    }

  // Create composition instructions, if any
  if (args.find("-album") != NULL)
    {
      const char *string = args.advance();
      float secs;
      if ((string == NULL) || (sscanf(string,"%f",&secs) == 0) || (secs < 0.0))
        secs = 5.0F;
      if (string != NULL)
        args.advance();
      generate_album_frame(out.access_composition(),layer_specs,secs);
    }
  parse_and_write_composition_info(args,out.access_composition(),
                                   layer_specs);

  // Create codestream headers
  mg_source_spec *in;
  for (in=inputs; in != NULL; in=in->next)
    create_codestream_headers(out,in);

  // Copy metadata, translating associated stream/layer indices
  for (in=inputs; in != NULL; in=in->next)
    if (in->jpx_src.exists())
      copy_metadata(in->jpx_src.access_meta_manager(),
                    out.access_meta_manager(),in,layer_specs);

  // Write JPX headers and output all the code-stream (or frag table) boxes
  write_jpx_headers_and_codestreams(out,inputs,interleave_headers,
                                    link_to_codestreams);

  out.write_metadata();
  out.close();

  // Cleanup
  while ((layer=layer_specs) != NULL)
    {
      layer_specs = layer->next;
      delete layer;
    }

  // Statistics
  if (!quiet)
    {
      pretty_cout << "\nMerged " << num_codestreams << " code-streams from "
                  << num_inputs << " input files\n"
                  << "Wrote " << num_layers << " compositing layers into the "
                     "output file.\n";
    }
}

/*****************************************************************************/
/* STATIC                  configure_track_params                            */
/*****************************************************************************/

static void
  configure_track_params(mj2_video_target *tgt, mg_source_spec *in,
                         int source_layer_idx)
  /* Initializes the palette, resolution, colour and other specifications
     associated with the `tgt' object, based upon the `in' object.  If
     `in' represents a JP2/JPX source, `source_layer_idx' identifies the
     particular compositing layer to be used. */
{
  if (in->video_source != NULL)
    {
      kdu_int16 op_red, op_green, op_blue, mode =
        in->video_source->get_graphics_mode(op_red,op_green,op_blue);
      tgt->set_graphics_mode(mode,op_red,op_green,op_blue);
      tgt->access_colour().copy(in->video_source->access_colour());
      tgt->access_palette().copy(in->video_source->access_palette());
      tgt->access_channels().copy(in->video_source->access_channels());
      tgt->access_resolution().copy(in->video_source->access_resolution());
    }
  else
    {
      jpx_layer_source jpx_src =
        in->jpx_src.access_layer(source_layer_idx);
      if (jpx_src.get_num_codestreams() != 1)
        { kdu_error e; e << "Trying to include a complex JPX compositing "
          "layer (one with multiple codestreams) as a frame in the "
          "target MJ2 file.  Problem encountered in layer " <<
          source_layer_idx << " (starting from 1) of \""
          << in->filename << "\"."; }
      jpx_codestream_source cs_src =
        in->jpx_src.access_codestream(jpx_src.get_codestream_id(0));
      tgt->access_palette().copy(cs_src.access_palette());
      tgt->access_colour().copy(jpx_src.access_colour(0));
      tgt->access_resolution().copy(jpx_src.access_resolution());
      jp2_channels channels_src = jpx_src.access_channels();
      jp2_channels channels_tgt = tgt->access_channels();
      int n, num_colours = channels_src.get_num_colours();
      int alpha_comp = -1, premult_alpha_comp = -1;
      channels_tgt.init(num_colours);
      for (n=0; n < num_colours; n++)
        {
          int comp_idx, lut_idx, stream_idx;
          if (channels_src.get_colour_mapping(n,comp_idx,lut_idx,stream_idx))
            channels_tgt.set_colour_mapping(n,comp_idx,lut_idx,0);
          if (channels_src.get_opacity_mapping(n,comp_idx,lut_idx,stream_idx))
            {
              channels_tgt.set_opacity_mapping(n,comp_idx,lut_idx,0);
              if (n == 0)
                alpha_comp = comp_idx;
              else if (alpha_comp != comp_idx)
                alpha_comp = -1;
            }
          if (channels_src.get_premult_mapping(n,comp_idx,lut_idx,stream_idx))
            {
              channels_tgt.set_premult_mapping(n,comp_idx,lut_idx,0);
              alpha_comp = -1;
              if (n == 0)
                premult_alpha_comp = comp_idx;
              else if (premult_alpha_comp != comp_idx)
                premult_alpha_comp = -1;
            }
        }
      if (alpha_comp >= 0)
        tgt->set_graphics_mode(MJ2_GRAPHICS_ALPHA);
      else if (premult_alpha_comp >= 0)
        tgt->set_graphics_mode(MJ2_GRAPHICS_PREMULT_ALPHA);
    }
}

/*****************************************************************************/
/* STATIC                     write_mj2_image                                */
/*****************************************************************************/

static void
  write_mj2_image(mj2_video_target *tgt, mg_source_spec *in,
                  int source_layer_idx)
  /* Uses `tgt::open_image' and `tgt::close_image' to write a single
     image (single field for interlaced tracks), copying its contents
     from the codestream belonging to the `source_layer_idx' compositing
     layer of a JPX source or to the `source_layer_idx'th codestream of
     an MJ2 source. */
{
  kdu_codestream codestream; // We need this to execute `tgt->close_image'.
  jpx_input_box box_in;
  if (in->video_source != NULL)
    {
      int frame_idx, field_idx;
      if (in->field_order == KDU_FIELDS_NONE)
        { frame_idx = source_layer_idx;  field_idx = 0; }
      else
        { frame_idx = source_layer_idx>>1; field_idx = source_layer_idx&1; }
      frame_idx += in->first_frame_idx;
      in->video_source->seek_to_frame(frame_idx);
      in->video_source->open_stream(field_idx,&box_in);
      codestream.create(&box_in);
      box_in.close();
      in->video_source->open_stream(field_idx,&box_in);
    }
  else
    {
      jpx_layer_source jpx_src = in->jpx_src.access_layer(source_layer_idx);
      jpx_codestream_source cs_src =
        in->jpx_src.access_codestream(jpx_src.get_codestream_id(0));
      cs_src.open_stream(&box_in);
      codestream.create(&box_in);
      box_in.close();
      cs_src.open_stream(&box_in);
    }

  int num_bytes;
  kdu_byte *buf = new kdu_byte[1<<16];
  tgt->open_image();
  while ((num_bytes = box_in.read(buf,1<<16)) > 0)
    tgt->write(buf,num_bytes);
  tgt->close_image(codestream);
  codestream.destroy();
  delete[] buf;
  box_in.close();
}

/*****************************************************************************/
/* STATIC                       generate_mj2                                 */
/*****************************************************************************/

static void
  generate_mj2(jp2_family_tgt *family_tgt, kdu_args &args,
               mg_source_spec *inputs, int num_inputs, bool quiet)
{
  int num_tracks=0, num_frames=0;
  mg_track_spec *track, *track_specs =
    parse_track_specs(args,num_tracks,inputs,num_inputs);

  mj2_target out;
  out.open(family_tgt);

  for (track=track_specs; track != NULL; track=track->next)
    {
      mg_track_seg *seg;
      mj2_video_target *tgt = out.add_video_track();
      tgt->set_field_order(track->field_order);
      kdu_uint32 timescale = 0;
      bool need_init = true;
      double exact_period = -1.0;
      for (seg=track->segs; seg != NULL; seg=seg->next)
        {
          // Navigate to the first frame
          int fields_per_frame = (track->field_order==KDU_FIELDS_NONE)?1:2;
          int n=seg->from*fields_per_frame;
          mg_source_spec *in=inputs;
          while ((in != NULL) && (n > 0))
            { // Skip through the sources until we get to the `from' element
              if (in->num_fields > n)
                break;
              n -= in->num_fields;
              in = in->next;
            }
          int src_field = n;

          // Scan through the frames
          for (n=seg->from; (seg->to < seg->from) || (n <= seg->to); n++)
            {
              if (in == NULL)
                break;

              if (seg->fps > 0.0F)
                exact_period = 1.0 / seg->fps;
              else if (in->video_source != NULL)
                {
                  in->video_source->seek_to_frame(src_field/fields_per_frame);
                  exact_period =
                    ((double) in->video_source->get_frame_period()) /
                    ((double) in->video_source->get_timescale());
                }
              else if (exact_period <= 0.0)
                { kdu_error e; e << "No timing information available in "
                  "source data used to generate video track.  You must "
                  "supply an explicit frame rate via the \"-mj2_tracks\" "
                  "argument."; }
              kdu_uint32 period;
              if (need_init)
                { // Writing the first frame for this track; set the timescale
                  if (in->video_source != NULL)
                    timescale = in->video_source->get_timescale();
                  else
                    { // Find a good timescale to match the exact period
                      kdu_uint32 ticks_per_sec, best_ticks;
                      double error, best_error=1000.0; // Ridiculous value
                      for (ticks_per_sec=1000; ticks_per_sec < 2000;
                           ticks_per_sec++)
                        {
                          period=(kdu_uint32)(exact_period*ticks_per_sec+0.5);
                          error = fabs(exact_period -
                            ((double) period) / ((double) ticks_per_sec));
                          if (error < best_error)
                            { best_error=error;  best_ticks=ticks_per_sec; }
                        }
                      timescale = best_ticks;
                    }
                  tgt->set_timescale(timescale);
                }

              period = (kdu_uint32)(exact_period*timescale + 0.5);
              tgt->set_frame_period(period);

              for (int p=0; p < fields_per_frame; p++, need_init=false)
                {
                  if (in == NULL)
                    break;
                  if ((in->video_source != NULL) &&
                      (in->field_order != track->field_order))
                    { kdu_error e; e << "Trying to construct an MJ2 video "
                      "track from source MJ2 frames which do not have the "
                      "same interlacing convention as the output track."; }
                  if ((in->field_order != KDU_FIELDS_NONE) && (p & 1))
                    { kdu_error e; e << "Trying to construct an "
                      "interlaced video track from segments of MJ2 "
                      "interlaced source material and JP2/JPX source material "
                      "where the latter does not provide an even number of "
                      "compositing layers."; }
                  if (need_init)
                    configure_track_params(tgt,in,src_field);
                  write_mj2_image(tgt,in,src_field);

                  // Advance to next source field
                  src_field++;
                  if (in->num_fields == src_field)
                    {
                      in = in->next;
                      src_field = 0;
                    }
                }
              num_frames++;
            }
        }
    }

  out.close();
  while ((track=track_specs) != NULL)
    {
      track_specs = track->next;
      delete track;
    }

  // Statistics
  if (!quiet)
    {
      pretty_cout << "\nWrote " << num_tracks << " video tracks, "
                     "containing a total of " << num_frames
                  << " frames to the output file.\n";
    }
}


/* ========================================================================= */
/*                             External Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/*                                   main                                    */
/*****************************************************************************/

int main(int argc, char *argv[])
{
  kdu_customize_warnings(&pretty_cout);
  kdu_customize_errors(&pretty_cerr);
  kdu_args args(argc,argv,"-s");

  // Collect simple arguments.
  char *ofname=NULL;
  bool quiet, interleave_headers, link_to_codestreams;
  int num_inputs, num_codestreams;
  mg_source_spec *in, *inputs =
    parse_common_args(args,ofname,quiet,num_inputs,num_codestreams,
                      interleave_headers,link_to_codestreams);


  // Create output file
  jp2_family_tgt family_tgt;
  family_tgt.open(ofname);

  if (!has_mj2_suffix(ofname))
    generate_jpx(&family_tgt,args,inputs,num_inputs,num_codestreams,
                 interleave_headers,link_to_codestreams,quiet);
  else
    {
      if (link_to_codestreams)
        { kdu_error e; e << "the `-links' argument cannot currently "
          "be used when generating an MJ2 output file."; }
      generate_mj2(&family_tgt,args,inputs,num_inputs,quiet);
    }

  // Cleanup
  family_tgt.close();
  while ((in=inputs) != NULL)
    { inputs = in->next; delete in; }
  delete[] ofname;

  if (args.show_unrecognized(pretty_cout) != 0)
    { kdu_error e; e << "There were unrecognized command line arguments!"; }

  return 0;
}
