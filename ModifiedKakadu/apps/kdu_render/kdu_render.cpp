/*****************************************************************************/
// File: kdu_render.cpp [scope = APPS/RENDER]
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
   The purpose of this demo application is to provide a collection of examples
showing how the power `kdu_region_decompressor' and `kdu_region_compositor'
objects can be used to decompress and render a JPEG2000 compressed data
source to a memory buffer -- in this case, the memory buffer is written
directly to a BMP file.
******************************************************************************/

// System includes
#include <string.h>
#include <math.h>
#include <assert.h>
#include <iostream>
// Kakadu core includes
#include "kdu_arch.h"
#include "kdu_elementary.h"
#include "kdu_messaging.h"
#include "kdu_params.h"
#include "kdu_compressed.h"
#include "kdu_sample_processing.h"
// Application includes
#include "kdu_args.h"
#include "kdu_file_io.h"
#include "jp2.h"
#include "kdu_region_decompressor.h"
#include "kdu_region_compositor.h" // Required for demos 3 and 4


//+++++++++++++++++++++++++++++++++++++++++++++++++++
// The actual demo functions appear at the end of this
// file.  Here are the stubs.  For simplicity here, each
// function takes file names for its input and output,
// but Kakadu is vastly more general than this.

static void render_demo_1(const char *in, const char *out);
static void render_demo_2(const char *in, const char *out);
static void render_demo_3(const char *in, const char *out);
static void render_demo_4(const char *in, const char *out);
static void render_demo_5(const char *in, const char *out);
static void render_demo_cpu(const char *in, int num_threads);


//+++++++++++++++++++++++++++++++++++++++++++++++++++
// While not strictly necessary, just about any serious Kakadu application
// will configure error and warning message services.  Here is an example
// of how to do it, which includes support for throwing exceptions when
// errors occur -- if you don't do this, the program will exit when an 
// error occurs.

/* ========================================================================= */
/*                         Set up messaging services                         */
/* ========================================================================= */

class kdu_stream_message : public kdu_message {
  public: // Member classes
    kdu_stream_message(std::ostream *stream, bool throw_exc)
      { this->stream = stream; this->throw_exc = throw_exc; }
    void put_text(const char *string)
      { (*stream) << string; }
    void flush(bool end_of_message=false)
      {
        stream->flush();
        if (end_of_message && throw_exc)
          throw (int) 1;
      }
  private: // Data
    std::ostream *stream;
    bool throw_exc;
  };

static kdu_stream_message cout_message(&std::cout,false);
static kdu_stream_message cerr_message(&std::cerr,true);
static kdu_message_formatter pretty_cout(&cout_message);
static kdu_message_formatter pretty_cerr(&cerr_message);


//+++++++++++++++++++++++++++++++++++++++++++++++++++
// This stuff is only to make the demo write output
// files (BMP files) for ease of testing via the command
// line.  It has nothing to do with Kakadu proper.

/* ========================================================================= */
/*                            BMP Writing Stuff                              */
/* ========================================================================= */

/*****************************************************************************/
/*                               bmp_header                                  */
/*****************************************************************************/

struct bmp_header {
    kdu_uint32 size; // Size of this structure: must be 40
    kdu_int32 width; // Image width
    kdu_int32 height; // Image height; -ve means top to bottom.
    kdu_uint32 planes_bits; // Planes in 16 LSB's (must be 1); bits in 16 MSB's
    kdu_uint32 compression; // Only accept 0 here (uncompressed RGB data)
    kdu_uint32 image_size; // Can be 0
    kdu_int32 xpels_per_metre; // We ignore these
    kdu_int32 ypels_per_metre; // We ignore these
    kdu_uint32 num_colours_used; // Entries in colour table (0 means use default)
    kdu_uint32 num_colours_important; // 0 means all colours are important.
  };
  /* Notes:
        This header structure must be preceded by a 14 byte field, whose
     first 2 bytes contain the string, "BM", and whose next 4 bytes contain
     the length of the entire file.  The next 4 bytes must be 0. The final
     4 bytes provides an offset from the start of the file to the first byte
     of image sample data.
        If the bit_count is 1, 4 or 8, the structure must be followed by
     a colour lookup table, with 4 bytes per entry, the first 3 of which
     identify the blue, green and red intensities, respectively. */

/*****************************************************************************/
/* STATIC                       to_little_endian                             */
/*****************************************************************************/

static void
  to_little_endian(kdu_int32 * words, int num_words)
  /* Used to convert the BMP header structure to a little-endian word
     organization for platforms which use the big-endian convention. */
{
  kdu_int32 test = 1;
  kdu_byte *first_byte = (kdu_byte *) &test;
  if (*first_byte)
    return; // Machine uses little-endian architecture already.
  kdu_int32 tmp;
  for (; num_words--; words++)
    {
      tmp = *words;
      *words = ((tmp >> 24) & 0x000000FF) +
               ((tmp >> 8)  & 0x0000FF00) +
               ((tmp << 8)  & 0x00FF0000) +
               ((tmp << 24) & 0xFF000000);
    }
}

/*****************************************************************************/
/* STATIC                       write_bmp_file                               */
/*****************************************************************************/

static void
  write_bmp_file(const char *fname, kdu_coords size, kdu_int32 buffer[],
                 int buffer_row_gap)
{
  FILE *file = fopen(fname,"wb");
  if (file == NULL)
    { kdu_error e; e << "Unable to open output file, \"" << fname << "\"."; }
  kdu_byte magic[14];
  bmp_header header;
  int header_bytes = 14+sizeof(header);
  assert(header_bytes == 54);
  int line_bytes = size.x*4; line_bytes += (4-line_bytes)&3;
  int file_bytes = line_bytes * size.y + header_bytes;
  magic[0] = 'B'; magic[1] = 'M';
  magic[2] = (kdu_byte) file_bytes;
  magic[3] = (kdu_byte)(file_bytes>>8);
  magic[4] = (kdu_byte)(file_bytes>>16);
  magic[5] = (kdu_byte)(file_bytes>>24);
  magic[6] = magic[7] = magic[8] = magic[9] = 0;
  magic[10] = (kdu_byte) header_bytes;
  magic[11] = (kdu_byte)(header_bytes>>8);
  magic[12] = (kdu_byte)(header_bytes>>16);
  magic[13] = (kdu_byte)(header_bytes>>24);
  header.size = 40;
  header.width = size.x;
  header.height = size.y;
  header.planes_bits = 1; // Set `planes'=1 (mandatory)
  header.planes_bits |= (32 << 16); // Set bits-per-pel to upper word
  header.compression = 0;
  header.image_size = 0;
  header.xpels_per_metre = header.ypels_per_metre = 0;
  header.num_colours_used = header.num_colours_important = 0;
  to_little_endian((kdu_int32 *) &header,10);
  fwrite(magic,1,14,file);
  fwrite(&header,1,40,file);
  for (int r=size.y; r > 0; r--, buffer += buffer_row_gap)
    if (fwrite(buffer,1,(size_t) line_bytes,file) != (size_t) line_bytes)
      { kdu_error e;
        e << "Unable to write to output file.  Device may be full."; }
  fclose(file);
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++
// This stuff is just to make the command-line
// interface look nice.  It has nothing to do with
// Kakadu proper.

/* ========================================================================= */
/*                           Usage Statement Stuff                           */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                        print_version                               */
/*****************************************************************************/

static void
  print_version()
{
  kdu_message_formatter out(&cout_message);
  out.start_message();
  out << "This is Kakadu's \"kdu_render\" application.\n";
  out << "\tCompiled against the Kakadu core system, version "
      << KDU_CORE_VERSION << "\n";
  out << "\tCurrent core system version is "
      << kdu_get_core_version() << "\n";
  out.flush(true);
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
  out << "-i <compressed file>\n";
  if (comprehensive)
    out << "\tThe range of JPEG2000 family files which are accepted depends "
           "upon the particular code fragment which is executed to do the "
           "decompression and rendering.  This is selected by the `-demo' "
           "argument, but the default should be to use the most sophisticated "
           "of the demonstrations.  Currently, demo 4 accepts any raw "
           "codestream, JP2 file, JPX file or MJ2 file, performing "
           "compositions (possibly from multiple codestreams), "
           "colour space conversions, and inverting complex multi-component "
           "transforms as required.\n";
  out << "-o <bmp output file>\n";
  if (comprehensive)
    out << "\tThe only output image file format produced by this simple "
           "demonstration program is BMP.  In fact, for added simplicity, "
           "the output file will always have a 32 bpp representation, even "
           "if the original image was monochrome.\n";
  out << "-demo <which demo to run>\n";
  if (comprehensive)
    out << "\tCurrently takes an integer in the range 1 through 5, "
           "corresponding to code fragments which demonstrate different "
           "ways to very simply get up and running with decompression and "
           "rendering of JPEG2000 files.  The current default is to use "
           "demo 4.  All of the demos essentially do the same thing, but "
           "they take a progressively wider range of input files, up to "
           "demo 4 (see \"-i\" for what it can accept).  Demo 5 is the "
           "same as demo 5, but it runs a multi-threaded processing "
           "environment with two threads of execution.  It is expected "
           "that you will use this argument in conjunction with a debugger "
           "to step through different implementation strategies and see "
           "how they work.\n";
  out << "-cpu [<num_threads>]\n";
  if (comprehensive)
    out << "\tIf this option is supplied, neither the `-demo' nor the `-o' "
           "arguments should appear.  In this case, the rendering approach "
           "is almost identical to that associated with `-demo 2', which "
           "is based on direct invocation of the `kdu_region_decompressor' "
           "object, except that a multi-threaded processing environment may "
           "be used to obtain the maximum processing speed.  The only other "
           "difference in this case is that the image is decompressed "
           "incrementally into a small temporary memory buffer, rather than "
           "a single whole-image memory buffer.  This is exactly what you "
           "would probably do in a real application involving huge images.  "
           "It allows you to measure the CPU time required to render a huge "
           "image, without including the time taken to actually write the "
           "image to an output file.  In fact, you cannot supply an output "
           "file with this argument.  If the `num_threads' parameter is 0, "
           "Kakadu's single-threaded processing environment is used; if "
           "`num_threads' is 1, the multi-threaded processing environment is "
           "used with exactly one processing thread -- this is still single "
           "threaded, but involves a different implicit processing order and "
           "some overhead.  Normally, you would set `num_threads' to 0 or "
           "the number of processors in your system.  If you omit the "
           "parameter altogether, the number of threads will be set equal "
           "to the application's best guess as to the number of CPU's in "
           "your system (usually correct).\n";
  out << "-version -- print core system version I was compiled against.\n";
  out << "-v -- abbreviation of `-version'\n";
  out << "-usage -- print a comprehensive usage statement.\n";
  out << "-u -- print a brief usage statement.\"\n\n";
  out.flush();
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++
// A simple main() function, which just reads the
// command-line arguments, installs the error/warning
// handlers and runs the selected demo function.

/*****************************************************************************/
/*                                   main                                    */
/*****************************************************************************/

int main(int argc, char *argv[])
{
  kdu_customize_warnings(&pretty_cout); // Deliver warnings to stdout.
  kdu_customize_errors(&pretty_cerr); // Deliver errors to stderr + throw exc
  kdu_args args(argc,argv); // Ingest args


  // Collect arguments.

  if ((args.get_first() == NULL) || (args.find("-u") != NULL))
    { print_usage(args.get_prog_name());  return 0; }
  if (args.find("-usage") != NULL)
    { print_usage(args.get_prog_name(),true);  return 0; }
  if ((args.find("-version") != NULL) || (args.find("-v") != NULL))
    { print_version();  return 0; }

  int failure_code = 0; // No failure
  char *in_fname=NULL, *out_fname=NULL;
  try { // Any errors generated inside here will produce exceptions
      int num_threads = -1;
      int demo = 4;
      if (args.find("-cpu") != NULL)
        {
          demo = 0;
          const char *string = args.advance();
          if ((string != NULL) && (*string != '-') &&
              (sscanf(string,"%d",&num_threads) == 1))
            args.advance();
          else
            num_threads = kdu_get_num_processors();
        }

      if (args.find("-demo") != NULL)
        {
          if (demo == 0)
            { kdu_error e; e << "The `-demo' argument may not be used in "
              "conjunction with `-cpu'."; }
          const char *string = args.advance();
          if ((string == NULL) || (sscanf(string,"%d",&demo) != 1) ||
              (demo < 1) || (demo > 5))
            { kdu_error e; e<<"`-demo' switch needs an integer from 1 to 5."; }
          args.advance();
        }

      if (args.find("-i") != NULL)
        {
          const char *string = args.advance();
          if (string == NULL)
            { kdu_error e; e << "Must supply a name with the `-i' switch."; }
          in_fname = new char[strlen(string)+1];
          strcpy(in_fname,string);
          args.advance();
        }
      else
        { kdu_error e; e << "Must supply an input file via the `-i' switch."; }

      if (args.find("-o") != NULL)
        {
          if (demo == 0)
            { kdu_error e; e << "The `-o' argument may not be used in "
              "conjunction with `-cpu'."; }
          const char *string = args.advance();
          if (string == NULL)
            { kdu_error e; e << "Must supply a name with the `-o' switch."; }
          out_fname = new char[strlen(string)+1];
          strcpy(out_fname,string);
          args.advance();
        }
      else if (demo > 0)
        { kdu_error e; e<<"Must supply an output file via the `-o' switch."; }

      if (args.show_unrecognized(pretty_cout) != 0)
        { kdu_error e; e<<"There were unrecognized command line arguments!"; }

      switch (demo) {
        case 0: render_demo_cpu(in_fname,num_threads); break;
        case 1: render_demo_1(in_fname,out_fname);  break;
        case 2: render_demo_2(in_fname,out_fname);  break;
        case 3: render_demo_3(in_fname,out_fname);  break;
        case 4: render_demo_4(in_fname,out_fname);  break;
        case 5: render_demo_5(in_fname,out_fname);  break;
        default: assert(0); // Should not be possible
        }
    }
  catch (int)
    {
      failure_code = 1;
    }

  if (in_fname != NULL)
    delete[] in_fname;
  if (out_fname != NULL)
    delete[] out_fname;
  return failure_code;
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++
// Finally, the demo functions themselves.  Note, each
// of these demo functions uses a memory buffer into
// which it decompresses the whole image.  While this
// sort of thing might seem convenient, JPEG2000 really
// comes into its own when dealing with large images.
// In this setting, interactive decompression of
// selected pieces of the image is the interesting
// thing to do.  The code fragments here do not do
// this, but they all use objects which are designed to
// decompress/render arbitrary regions efficiently.

/* ========================================================================= */
/*                        DEMONSTRATION FUNCTIONS                            */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                  render_demo_1 (raw only)                          */
/*****************************************************************************/

static void
  render_demo_1(const char *in_fname, const char *out_fname)
  /* This function demonstrates decompression of a raw codestream
     via the `kdu_region_decompressor' object.
       Using the `kdu_region_decompressor' object is a little more complex
     and provides less features than the higher level `kdu_region_compositor'
     object -- the latter uses `kdu_region_decompressor'.  However,
     `kdu_region_decompressor' provides you with more choices in regard
     to the bit-depth at which samples are rendered and the organization
     of memory buffers.  To understand this, consult the descriptions of
     the various overloaded `kdu_region_decompressor::process' functions. */
{
  // Open a suitable compressed data source object
  kdu_simple_file_source input;
  input.open(in_fname);

  // Create the code-stream management machinery for the compressed data
  kdu_codestream codestream;
  codestream.create(&input);
  codestream.change_appearance(false,true,false); // Flip vertically to deal
              // with the fact that BMP files have a bottom-up organization;
              // this way, we can write the buffer without having to flip it.

  // Configure a `kdu_channel_mapping' object for `kdu_region_decompressor'
  kdu_channel_mapping channels;
  channels.configure(codestream);

  // Determine expansion factors and the reference component, against which
  // the image will be sized by `kdu_region_decompressor'.  The important
  // requirement is that all component expansion factors must be >= 1 for
  // `kdu_region_decompressor' to work.  In this demo function, we will simply
  // look for the component which has the highest resolution and make this
  // the reference; in `render_demo_2' we adopt a more robust approach, to
  // handle cases in which no one component has the highest resolution in
  // both directions.
  int c, c_idx, ref_component=0;
  kdu_coords subs, min_subs;
  for (c=0; c < channels.num_channels; c++)
    {
      c_idx = channels.source_components[c];
      codestream.get_subsampling(c_idx,subs,true);
      if ((c==0) || (subs.x < min_subs.x) || (subs.y < min_subs.y))
        { min_subs = subs;  ref_component = c_idx; }
    }

  // Determine dimensions and create output buffer
  kdu_region_decompressor decompressor;
  kdu_dims image_dims = decompressor.get_rendered_image_dims(codestream,
                                               &channels,-1,0,kdu_coords(1,1));
  kdu_int32 *buffer = new kdu_int32[(int) image_dims.area()];
  if (buffer == NULL)
    { kdu_error e; e << "Insufficient memory to buffer whole decompressed "
      "image.  Use \"kdu_expand\" or \"kdu_buffered_expand\" to decompress "
      "the image incrementally, or build your implementation to use "
      "\"kdu_region_decompressor\" in an incremental fashion.";
    }

  // Start region decompressor and render the image to the buffer
  try { // Catch errors to properly deallocate resources before returning
      decompressor.start(codestream,&channels,-1,0,INT_MAX,image_dims,
                         kdu_coords(1,1));
      kdu_dims new_region, incomplete_region=image_dims;
      while (decompressor.process(buffer,image_dims.pos,image_dims.size.x,
                                  100000,0,incomplete_region,new_region) &&
             !incomplete_region.is_empty()); // Loop until buffer is filled
      decompressor.finish();
      write_bmp_file(out_fname,image_dims.size,buffer,image_dims.size.y);
    }
  catch (...)
    {
      delete[] buffer;
      codestream.destroy();
      throw; // Rethrow the exception
    }

  // Cleanup
  delete[] buffer;
  codestream.destroy();
}


/*****************************************************************************/
/* STATIC                 render_demo_2 (raw or JP2)                         */
/*****************************************************************************/

static void
  render_demo_2(const char *in_fname, const char *out_fname)
  /* This function demonstrates decompression of either a raw codestream
     or a JP2 file, using the the `kdu_region_decompressor' object.
       Using the `kdu_region_decompressor' object is a little more complex
     and provides less features than the higher level `kdu_region_compositor'
     object -- the latter uses `kdu_region_decompressor'.  However,
     `kdu_region_decompressor' provides you with more choices in regard
     to the bit-depth at which samples are rendered and the organization
     of memory buffers.  To understand this, consult the descriptions of
     the various overloaded `kdu_region_decompressor::process' functions. */
{
  // Start by checking whether the input file belongs to the JP2 family of
  // file formats by testing the signature.
  jp2_family_src src;
  jp2_input_box box;
  src.open(in_fname);
  bool is_jp2 = box.open(&src) && (box.get_box_type() == jp2_signature_4cc);
  src.close();

  // Open a suitable compressed data source object
  kdu_compressed_source *input = NULL;
  kdu_simple_file_source file_in;
  jp2_family_src jp2_ultimate_src;
  jp2_source jp2_in;
  if (!is_jp2)
    {
      file_in.open(in_fname);
      input = &file_in;
    }
  else
    {
      jp2_ultimate_src.open(in_fname);
      if (!jp2_in.open(&jp2_ultimate_src))
        { kdu_error e; e << "Supplied input file, \"" << in_fname
                         << "\", does not appear to contain any boxes."; }
      if (!jp2_in.read_header())
        assert(0); // Should not be possible for file-based sources
      input = &jp2_in;
    }

  // Create the code-stream management machinery for the compressed data
  kdu_codestream codestream;
  codestream.create(input);
  codestream.change_appearance(false,true,false); // Flip vertically to deal
              // with the fact that BMP files have a bottom-up organization;
              // this way, we can write the buffer without having to flip it.

  // Configure a `kdu_channel_mapping' object for `kdu_region_decompressor'
  kdu_channel_mapping channels;
  if (jp2_in.exists())
    channels.configure(&jp2_in,false);
  else
    channels.configure(codestream);

  // Determine expansion factors and the reference component, against which
  // the image will be sized by `kdu_region_decompressor'.  The important
  // requirement is that all component expansion factors must be >= 1 for
  // `kdu_region_decompressor' to work.
  int c, ref_component = channels.source_components[0]; // A simple choice
  kdu_coords ref_subs; codestream.get_subsampling(ref_component,ref_subs,true);
  kdu_coords subs, min_subs=ref_subs;
  for (c=0; c < channels.num_channels; c++)
    {
      codestream.get_subsampling(channels.source_components[c],subs,true);
      if (subs.x < min_subs.x)
        min_subs.x = subs.x;
      if (subs.y < min_subs.y)
        min_subs.y = subs.y;
    }
  kdu_coords expand_numerator(1,1), expand_denominator(1,1);
  if (min_subs.x < ref_subs.x)
    { // Expand horizontally to fully utilize the most sub-sampled component
      expand_numerator.x = ref_subs.x;
      expand_denominator.x = min_subs.x;
    }
  if (min_subs.y < ref_subs.y)
    { // Expand vertically to fully utilize the most sub-sampled component
      expand_numerator.y = ref_subs.y;
      expand_denominator.y = min_subs.y;
    }

  // Determine dimensions and create output buffer
  kdu_region_decompressor decompressor;
  kdu_dims image_dims =
    decompressor.get_rendered_image_dims(codestream,&channels,-1,0,
                                         expand_numerator,expand_denominator);
  kdu_int32 *buffer = new kdu_int32[(int) image_dims.area()];
  if (buffer == NULL)
    { kdu_error e; e << "Insufficient memory to buffer whole decompressed "
      "image.  Use \"kdu_expand\" or \"kdu_buffered_expand\" to decompress "
      "the image incrementally, or build your implementation to use "
      "\"kdu_region_decompressor\" in an incremental fashion.";
    }

  // Start region decompressor and render the image to the buffer
  try { // Catch errors to properly deallocate resources before returning
      decompressor.start(codestream,&channels,-1,0,INT_MAX,image_dims,
                         expand_numerator,expand_denominator);
      kdu_dims new_region, incomplete_region=image_dims;
      while (decompressor.process(buffer,image_dims.pos,image_dims.size.x,
                                  100000,0,incomplete_region,new_region) &&
             !incomplete_region.is_empty()); // Loop until buffer is filled
      decompressor.finish();
      write_bmp_file(out_fname,image_dims.size,buffer,image_dims.size.y);
    }
  catch (...)
    {
      delete[] buffer;
      codestream.destroy();
      throw; // Rethrow the exception
    }

  // Cleanup
  delete[] buffer;
  codestream.destroy();
}


/*****************************************************************************/
/* STATIC             render_demo_3 (raw, JP2, JPX or MJ2)                   */
/*****************************************************************************/

static void
  render_demo_3(const char *in_fname, const char *out_fname)
  /* This function demonstrates decompression of either a raw codestream
     or a JP2, JPX or MJ2 file, using the the `kdu_region_compositor' object.
     In the case of JPX files, only the first compositing layer is
     decompressed.  In the case of MJ2 files, only one frame is
     decompressed. */
{
  // Open a suitable compressed data source object
  kdu_region_compositor compositor;
  kdu_simple_file_source file_in;
  jp2_family_src src;
  jpx_source jpx_in;
  mj2_source mj2_in;
  src.open(in_fname);
  if (jpx_in.open(&src,true) >= 0)
    { // We have a JP2/JPX-compatible file
      compositor.create(&jpx_in); 
      compositor.add_compositing_layer(0,kdu_dims(),kdu_dims());
    }
  else if (mj2_in.open(&src,true) >= 0)
    { // We have an MJ2-compatible file
      kdu_uint32 track_idx = mj2_in.get_next_track(0);
      mj2_video_source *track;
      while ((mj2_in.get_track_type(track_idx) != MJ2_TRACK_IS_VIDEO) ||
             ((track=mj2_in.access_video_track(track_idx)) == NULL) ||
             (track->get_num_frames() == 0))
        if ((track_idx = mj2_in.get_next_track(0)) == 0)
          { kdu_error e; e << "MJ2 source contains no video frames at all."; }
      compositor.create(&mj2_in); 
      compositor.add_compositing_layer((int)(track_idx-1),
                                 kdu_dims(),kdu_dims(),false,false,false,0,0);
    }
  else
    { // Incompatible with JP2/JPX or MJ2.  Try opening as raw codestream
      src.close();
      file_in.open(in_fname);
      compositor.create(&file_in);
      compositor.add_compositing_layer(0,kdu_dims(),kdu_dims());
    }

  // Render the image onto the frame buffer created by `kdu_region_compositor'.
  // You can override the buffer allocation method to create frames with the
  // configuration of your choosing.
  compositor.set_scale(false,true,false,1.0F); // Full size, with vertical
            // flipping, so we can write the buffer directly to a BMP file
            // (remember that BMP files are upside down with respect to most
            // image file formats).
  kdu_dims new_region, image_dims;
  compositor.get_total_composition_dims(image_dims);
  compositor.set_buffer_surface(image_dims); // Render the whole thing
  kdu_compositor_buf *buf = compositor.get_composition_buffer(image_dims);
  int buffer_row_gap;
  kdu_uint32 *buffer = buf->get_buf(buffer_row_gap,false);
  while (compositor.process(100000,new_region));
  write_bmp_file(out_fname,image_dims.size,(kdu_int32 *)buffer,buffer_row_gap);
}


/*****************************************************************************/
/* STATIC     render_demo_4 (raw, JP2, JPX, MJ2 + JPX compositions)          */
/*****************************************************************************/

static void
  render_demo_4(const char *in_fname, const char *out_fname)
  /* Same as `render_demo_3', except that for JPX files which contain
     a composition box (these are used for animation amongst other things),
     the first composited frame in the animation is rendered. */
{
  // Open a suitable compressed data source object
  kdu_region_compositor compositor;
  kdu_simple_file_source file_in;
  jp2_family_src src;
  jpx_source jpx_in;
  mj2_source mj2_in;
  src.open(in_fname);
  if (jpx_in.open(&src,true) >= 0)
    { // We have a JP2/JPX-compatible file
      compositor.create(&jpx_in); 
      jpx_composition compositing_rules = jpx_in.access_composition();
      jpx_frame_expander frame_expander;
      jx_frame *frame;
      if (compositing_rules.exists() &&
          ((frame = compositing_rules.get_next_frame(NULL)) != NULL) &&
          frame_expander.construct(&jpx_in,frame,0,true) &&
          (frame_expander.get_num_members() > 0))
        compositor.set_frame(&frame_expander);
      else
        compositor.add_compositing_layer(0,kdu_dims(),kdu_dims());
    }
  else if (mj2_in.open(&src,true) >= 0)
    { // We have an MJ2-compatible file
      kdu_uint32 track_idx = mj2_in.get_next_track(0);
      mj2_video_source *track;
      while ((mj2_in.get_track_type(track_idx) != MJ2_TRACK_IS_VIDEO) ||
             ((track=mj2_in.access_video_track(track_idx)) == NULL) ||
             (track->get_num_frames() == 0))
        if ((track_idx = mj2_in.get_next_track(0)) == 0)
          { kdu_error e; e << "MJ2 source contains no video frames at all."; }
      compositor.create(&mj2_in); 
      compositor.add_compositing_layer((int)(track_idx-1),
                                 kdu_dims(),kdu_dims(),false,false,false,0,0);
    }
  else
    { // Incompatible with JP2/JPX or MJ2.  Try opening as raw codestream
      src.close();
      file_in.open(in_fname);
      compositor.create(&file_in);
      compositor.add_compositing_layer(0,kdu_dims(),kdu_dims());
    }

  // Render the image onto the frame buffer created by `kdu_region_compositor'.
  // You can override the buffer allocation method to create frames with the
  // configuration of your choosing.
  compositor.set_scale(false,true,false,1.0F); // Full size, with vertical
            // flipping, so we can write the buffer directly to a BMP file
            // (remember that BMP files are upside down with respect to most
            // image file formats).
  kdu_dims new_region, image_dims;
  compositor.get_total_composition_dims(image_dims);
  compositor.set_buffer_surface(image_dims); // Render the whole thing
  kdu_compositor_buf *buf = compositor.get_composition_buffer(image_dims);
  int buffer_row_gap;
  kdu_uint32 *buffer = buf->get_buf(buffer_row_gap,false);
  while (compositor.process(100000,new_region));
  write_bmp_file(out_fname,image_dims.size,(kdu_int32 *)buffer,buffer_row_gap);
}

/*****************************************************************************/
/* STATIC     render_demo_5 (raw, JP2, JPX, MJ2 + JPX compositions)          */
/*****************************************************************************/

static void
  render_demo_5(const char *in_fname, const char *out_fname)
  /* Same as `render_demo_4', except that the implementation uses a
     multi-threaded processing environment with two threads, just to
     show you how to do it. */
{
  // Create a multi-threaded processing environment
  kdu_thread_env env;
  env.create(); // Creates the single "owner" thread
  env.add_thread(); // Adds an extra "worker" thread

  // Open a suitable compressed data source object
  kdu_region_compositor compositor;
  kdu_simple_file_source file_in;
  jp2_family_src src;
  jpx_source jpx_in;
  mj2_source mj2_in;
  src.open(in_fname);
  if (jpx_in.open(&src,true) >= 0)
    { // We have a JP2/JPX-compatible file
      compositor.create(&jpx_in); 
      jpx_composition compositing_rules = jpx_in.access_composition();
      jpx_frame_expander frame_expander;
      jx_frame *frame;
      if (compositing_rules.exists() &&
          ((frame = compositing_rules.get_next_frame(NULL)) != NULL) &&
          frame_expander.construct(&jpx_in,frame,0,true) &&
          (frame_expander.get_num_members() > 0))
        compositor.set_frame(&frame_expander);
      else
        compositor.add_compositing_layer(0,kdu_dims(),kdu_dims());
    }
  else if (mj2_in.open(&src,true) >= 0)
    { // We have an MJ2-compatible file
      kdu_uint32 track_idx = mj2_in.get_next_track(0);
      mj2_video_source *track;
      while ((mj2_in.get_track_type(track_idx) != MJ2_TRACK_IS_VIDEO) ||
             ((track=mj2_in.access_video_track(track_idx)) == NULL) ||
             (track->get_num_frames() == 0))
        if ((track_idx = mj2_in.get_next_track(0)) == 0)
          { kdu_error e; e << "MJ2 source contains no video frames at all."; }
      compositor.create(&mj2_in); 
      compositor.add_compositing_layer((int)(track_idx-1),
                                 kdu_dims(),kdu_dims(),false,false,false,0,0);
    }
  else
    { // Incompatible with JP2/JPX or MJ2.  Try opening as raw codestream
      src.close();
      file_in.open(in_fname);
      compositor.create(&file_in);
      compositor.add_compositing_layer(0,kdu_dims(),kdu_dims());
    }

  // Render the image onto the frame buffer created by `kdu_region_compositor'.
  // You can override the buffer allocation method to create frames with the
  // configuration of your choosing.
  compositor.set_scale(false,true,false,1.0F); // Full size, with vertical
            // flipping, so we can write the buffer directly to a BMP file
            // (remember that BMP files are upside down with respect to most
            // image file formats).
  kdu_dims new_region, image_dims;
  compositor.get_total_composition_dims(image_dims);
  compositor.set_buffer_surface(image_dims); // Render the whole thing
  kdu_compositor_buf *buf = compositor.get_composition_buffer(image_dims);
  int buffer_row_gap;
  kdu_uint32 *buffer = buf->get_buf(buffer_row_gap,false);

  compositor.set_thread_env(&env,NULL); // You can install the multi-threaded
         // processing environment at any time, except when the `compositor'
         // is in the middle of processing -- if you need to change the
         // multi-threaded processing environment at an unknown point, the
         // safest thing to do is call `compositor.halt_processing' first.
         // You can even supply the same multi-threaded processing environment
         // to multiple `kdu_region_compositor' or `kdu_region_decompressor'
         // objects, simultaneously, allowing the work associated with each
         // such object to be distributed over the available working threads.
         // See the description of `kdu_region_compositor::set_thread_env' for
         // more ideas on what you can do with multi-threading in Kakadu.
  while (compositor.process(100000,new_region));
  env.terminate(NULL,true); // Best to always call this prior to `env.destroy'
             // just to be sure that all deferred synchronized jobs finish
             // running before we destroy the environment.  In the present
             // case it doesn't matter, since we have not registered any
             // synchronized jobs, deferred or otherwise -- see
             // `kdu_thread_entity::register_synchronized_job' for more info.
  env.destroy(); // Not strictly necessary, since the destructor will call it.

  write_bmp_file(out_fname,image_dims.size,(kdu_int32 *)buffer,buffer_row_gap);
}

/*****************************************************************************/
/* STATIC                      render_demo_cpu                               */
/*****************************************************************************/

static void
  render_demo_cpu(const char *in_fname, int num_threads)
  /* This function demonstrates decompression of either a raw codestream
     or a JP2 file, using the the `kdu_region_decompressor' object, writing
     results to a temporary memory buffer, which can be much smaller than
     the full image, in the case of huge images.  This is done to provide
     realistic CPU processing times for the `kdu_region_decompressor' object
     when used with huge images.  No output file is actually written. */
{
  // Start by checking whether the input file belongs to the JP2 family of
  // file formats by testing the signature.
  jp2_family_src src;
  jp2_input_box box;
  src.open(in_fname);
  bool is_jp2 = box.open(&src) && (box.get_box_type() == jp2_signature_4cc);
  src.close();

  // Open a suitable compressed data source object
  kdu_compressed_source *input = NULL;
  kdu_simple_file_source file_in;
  jp2_family_src jp2_ultimate_src;
  jp2_source jp2_in;
  if (!is_jp2)
    {
      file_in.open(in_fname);
      input = &file_in;
    }
  else
    {
      jp2_ultimate_src.open(in_fname);
      if (!jp2_in.open(&jp2_ultimate_src))
        { kdu_error e; e << "Supplied input file, \"" << in_fname
                         << "\", does not appear to contain any boxes."; }
      if (!jp2_in.read_header())
        assert(0); // Should not be possible for file-based sources
      input = &jp2_in;
    }

  // Create a multi-threaded processing environment
  kdu_thread_env env;
  kdu_thread_env *env_ref = NULL;
  if (num_threads > 0)
    {
      env.create(); // Creates the single "owner" thread
      for (int nt=1; nt < num_threads; nt++)
        if (!env.add_thread())
          num_threads = nt; // Unable to create all the threads requested
      env_ref = &env;
    }

  // Create the code-stream management machinery for the compressed data
  kdu_codestream codestream;
  codestream.create(input);

  // Configure a `kdu_channel_mapping' object for `kdu_region_decompressor'
  kdu_channel_mapping channels;
  if (jp2_in.exists())
    channels.configure(&jp2_in,false);
  else
    channels.configure(codestream);

  // Determine expansion factors and the reference component, against which
  // the image will be sized by `kdu_region_decompressor'.  The important
  // requirement is that all component expansion factors must be >= 1 for
  // `kdu_region_decompressor' to work.
  int c, ref_component = channels.source_components[0]; // A simple choice
  kdu_coords ref_subs; codestream.get_subsampling(ref_component,ref_subs,true);
  kdu_coords subs, min_subs=ref_subs;
  for (c=0; c < channels.num_channels; c++)
    {
      codestream.get_subsampling(channels.source_components[c],subs,true);
      if (subs.x < min_subs.x)
        min_subs.x = subs.x;
      if (subs.y < min_subs.y)
        min_subs.y = subs.y;
    }
  kdu_coords expand_numerator(1,1), expand_denominator(1,1);
  if (min_subs.x < ref_subs.x)
    { // Expand horizontally to fully utilize the most sub-sampled component
      expand_numerator.x = ref_subs.x;
      expand_denominator.x = min_subs.x;
    }
  if (min_subs.y < ref_subs.y)
    { // Expand vertically to fully utilize the most sub-sampled component
      expand_numerator.y = ref_subs.y;
      expand_denominator.y = min_subs.y;
    }

  // Determine dimensions and create temporary buffer
  kdu_region_decompressor decompressor;
  kdu_dims image_dims =
    decompressor.get_rendered_image_dims(codestream,&channels,-1,0,
                                         expand_numerator,expand_denominator);
  int max_region_pixels = (1<<16); // Provide at most a 256 kB buffer
  if (image_dims.area() < (kdu_long) max_region_pixels)
    max_region_pixels = (int) image_dims.area();
  kdu_int32 *buffer = new kdu_int32[max_region_pixels];
  if (buffer == NULL)
    { kdu_error e; e << "Insufficient memory to buffer whole decompressed "
      "image.  Use \"kdu_expand\" or \"kdu_buffered_expand\" to decompress "
      "the image incrementally, or build your implementation to use "
      "\"kdu_region_decompressor\" in an incremental fashion.";
    }

  // Start region decompressor and render the image to the buffer
  try { // Catch errors to properly deallocate resources before returning
      clock_t start_time = clock();
      decompressor.start(codestream,&channels,-1,0,INT_MAX,image_dims,
                         expand_numerator,expand_denominator,false,
                         KDU_WANT_OUTPUT_COMPONENTS,true,env_ref,NULL);
      kdu_dims new_region, incomplete_region=image_dims;
      while (decompressor.process(buffer,kdu_coords(0,0),0,(1<<18),
                                  max_region_pixels,incomplete_region,
                                  new_region) &&
             !incomplete_region.is_empty()); // Loop until buffer is filled
      decompressor.finish();
      clock_t end_time = clock();
      double processing_time = ((double)(end_time-start_time))/CLOCKS_PER_SEC;
      double processing_rate = ((double) image_dims.area()) / processing_time;
      pretty_cout << "Total processing time = "
                  << processing_time
                  << " seconds\n";
      pretty_cout << "Processing rate = "
                  << (processing_rate/1000000.0)
                  << " pixels/micro-second\n";
      if (num_threads == 0)
        pretty_cout << "Processed using the single-threaded environment\n";
      else
        pretty_cout << "Processed using the multi-threaded environment, with\n"
          << "    " << num_threads << " parallel threads of execution\n";
    }
  catch (...)
    {
      env.terminate(NULL,true);
      codestream.destroy();
      delete[] buffer;
      throw; // Rethrow the exception
    }

  // Cleanup
  env.terminate(NULL,true);
  codestream.destroy();
  delete[] buffer;
}
