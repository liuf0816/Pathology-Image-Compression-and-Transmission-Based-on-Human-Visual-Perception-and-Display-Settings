/******************************************************************************/
// File: kdu_region_decompressor.h [scope = APPS/SUPPORT]
// Version: Kakadu, V6.1
// Author: David Taubman
// Last Revised: 28 November, 2008
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
   Defines incremental, robust, region-based decompression services through
the `kdu_region_decompressor' object.  These services should prove useful
to many interactive applications which require JPEG2000 rendering capabilities.
*******************************************************************************/

#ifndef KDU_REGION_DECOMPRESSOR_H
#define KDU_REGION_DECOMPRESSOR_H

#include "kdu_compressed.h"
#include "kdu_sample_processing.h"
#include "jp2.h"

// Objects declared here
struct kdu_channel_mapping;
class kdu_region_decompressor;

// Objects declared elsewhere
struct kd_tile_bank;
struct kd_channel;
struct kd_component;


/******************************************************************************/
/*                             kdu_channel_mapping                            */
/******************************************************************************/

struct kdu_channel_mapping {
  /* [BIND: reference]
     [SYNOPSIS]
     This structure provides all information required to express the
     relationship between code-stream image components and the colour
     channels to be produced during rendering.  In the simplest case, each
     colour channel (red, green and blue, or luminance) is directly
     assigned to a single code-stream component.  More generally, component
     samples may need to be mapped through a pallete lookup table, or a
     colour space transformation might be required (e.g., the components or
     palette outputs might describe the image in terms of a custom colour
     space defined through an ICC profile).
     [//]
     The purpose of this structure is to capture the reproduction functions
     required for correct colour reproduction, so that they can be passed to
     the `kdr_region_decompressor::start' function.
     [//]
     This structure also serves to capture any information concerning opacity
     (alpha) channels.
  */
  //----------------------------------------------------------------------------
  public: // Member functions
    KDU_AUX_EXPORT kdu_channel_mapping();
      /* [SYNOPSIS]
           Constructs an empty mapping object.  Use the `configure' function
           or else fill out the data members of this structure manually before
           passing the object into `kdu_region_decompressor'.  To return to the
           empty state, use the `clear' function.
      */
    ~kdu_channel_mapping() { clear(); }
      /* [SYNOPSIS]
           Calls `clear', which will delete any lookup tables
           referenced from the `palette' array.
      */
    KDU_AUX_EXPORT void clear();
      /* [SYNOPSIS]
           Returns all data members to the empty state created by the
           constructor, deleting any lookup tables which may have been
           installed in the `palette' array.
      */
    KDU_AUX_EXPORT bool configure(kdu_codestream codestream);
      /* [SYNOPSIS]
           Configures the channel mapping information based upon the
           dimensions of the supplied raw code-stream.  Since no explicit
           channel mapping information is available from a wrapping file
           format, the function assumes that the first 3 output image
           components represent red, green and blue colour channels, unless
           they have different dimensions or there are fewer than 3 components,
           in which case the function treats the first component as a
           luminance channel and ignores the others.
         [RETURNS]
           All versions of this overloaded function return true if successful.
           This version always returns true.
      */
    KDU_AUX_EXPORT bool
      configure(jp2_colour colour, jp2_channels channels,
                int codestream_idx, jp2_palette palette,
                jp2_dimensions codestream_dimensions);
      /* [SYNOPSIS]
           Configures the channel mapping information based on the supplied
           colour, palette and channel binding information.  The object is
           configured only for colour rendering, regardless of whether the
           `channels' object identifies the existence of opacity channels.
           However, you may augment the configuration with alpha information
           at a later time by calling `add_alpha_to_configuration'.
         [ARG: codestream_idx]
           A JPX source may have multiple codestreams associated with a given
           compositing layer.  This argument identifies the index of the
           codestream which is to be used with the present configuration when
           it is supplied to `kdu_region_decompressor'.  The identifier is
           compared against the codestream identifiers returned via
           `channels.get_colour_mapping' and `channels.get_opacity_mapping'.
         [ARG: palette]
           Used to supply the `jp2_palette' interface associated with the
           codestream which is identified by `codestream_idx'.  For JP2 files,
           there is only one recognized palette and one recognized codestream
           having index 0.  For JPX files, each codestream is associated with
           its own palette.
         [ARG: codestream_dimensions]
           Used to supply the `jp2_dimensions' interface associated with the
           codestream which is identified by `codestream_idx'.  For JP2 files,
           there is only one recognized codestream having index 0.  For JPX
           files, each codestream has its own set of dimensions.  The
           present object uses the `codestream_dimensions' interface only
           to obtain default rendering precision and signed/unsigned
           information to use in the event that
           `kdu_region_decompressor::process' is invoked using a
           `precision_bits' argument of 0.
         [RETURNS]
           All versions of this overloaded function return true if successful.
           This version returns false only if the colour space cannot be
           rendered.  This gives the caller an opportunity to provide an
           alternate colour space for rendering.  JPX data sources, for example,
           may provide several related colour descriptions for a single
           compositing layer.  If any other error occurs, the function
           may invoke `kdu_error' rather than returninig -- this in turn
           may throw an exception which the caller can catch.
      */
    KDU_AUX_EXPORT bool configure(jp2_source *jp2_in, bool ignore_alpha);
      /* [SYNOPSIS]
           Simplified interface to the second form of the `configure' function
           above, based on a simple JP2 data source.  Automatically invokes
           `add_alpha_to_configuration' if `ignore_alpha' is false.  Note
           that `add_alpha_to_configuration' is invoked with the
           `ignore_premultiplied_alpha' argument set to true.  If you want
           to add both regular opacity and premultiplied opacity channels,
           you should call the `add_alpha_to_configuration' function
           explicitly.
         [RETURNS]
           All versions of this overloaded function return true if successful.
           This version always returns true.  If an error occurs, or the
           colour space cannot be converted, it generates an error through
           `kdu_error' rather than returning false.  If you would like the
           function to return false when the colour space cannot be rendered,
           use the second form of the `configure' function instead.
      */
    KDU_AUX_EXPORT bool
      add_alpha_to_configuration(jp2_channels channels, int codestream_idx,
                                 jp2_palette palette,
                                 jp2_dimensions codestream_dimensions,
                                 bool ignore_premultiplied_alpha=true);
      /* [SYNOPSIS]
           Unlike the `configure' functions, this function does not call
           `clear' automatically.  Instead, it tries to add an alpha channel
           to whatever configuration already exists.  It is legal (although
           not useful) to call this function even if an alpha channel has
           already been configured, since the function strips back the current
           configuration to include only the colour channels before looking
           to add an alpha channel.  Note, however, that it is perfectly
           acceptable (and quite useful) to add an alpha channel to a
           configuration which has just been cleared, so that only alpha
           information is recovered, without any colour information.
           [//]
           If a single alpha component cannot be found in the `channels'
           object, the function returns false.  This happens if there is no
           alpha (opacity) information, or if multiple distinct alpha channels
           are identified by `channels', or if the single alpha channel does
           not use the indicated codestream.  The `palette' must be provided
           since an alpha channel may be formed by passing the relevant image
           component samples through one of the palette lookup tables.
           [//]
           JP2/JPX files may identify alpha (opacity) information as
           premultiplied or not premultiplied.  These are sometimes known
           as associated and unassociated alpha.  The
           `ignore_premultiplied_alpha' argument controls which form of
           alpha information you are interested in adding, as indicated
           in the argument description below.  For compatibility with
           earlier versions of the function, this argument defaults to true.
         [RETURNS]
           True only if there is exactly one opacity channel and it is based
           on the indicated codestream.  Otherwise, the function returns
           false, adding no channels to the configuration.
         [ARG: ignore_premultiplied_alpha]
           If true, only unassociated alpha channels are examined, as
           reported by the `jp2_channels::get_opacity_mapping' function.
           Otherwise, both regular opacity and premultiplied opacity are
           considered, as reported by the `jp2_channels::get_premult_mapping'
           function.  To discover which type of alpha is being installed,
           you can call the function first with `ignore_premultiplied_alpha'
           set to true and then, if this call returns false, again with
           `ignore_premultiplied_alpha' set to false.
      */
  //----------------------------------------------------------------------------
  public: // Data
    int num_channels;
      /* [SYNOPSIS]
           Total number of channels to render, including colour channels and
           opacity channels.  The `configure' function will set this member
           to 1 or 3 if there is no alpha channel, and 2 or 4 if there is
           an alpha channel, depending on whether the source is monochrome
           or colour.  However, you may manually configure any number of
           channels you like.
      */
    int get_num_channels() { return num_channels; }
      /* [SYNOPSIS] Returns the value of the public member variable,
         `num_channels'.  Useful for function-only language bindings. */
    KDU_AUX_EXPORT void set_num_channels(int num);
      /* [SYNOPSIS]
           Convenience function for allocating the `source_components' and
           `palette' arrays required to hold the channel properties.  Also
           sets the `num_channels' member.  Copies any existing channel
           data, so that you can build up a channel description progressively
           by calling this function multiple times.  Initializes all new
           `source_components' entries to -1, all new `palette' entries
           to NULL, all new `default_rendering_precision' entries to 8 and
           all new `default_rendering_signed' entries to false.
      */
    int num_colour_channels;
      /* [SYNOPSIS]
           Indicates the number of initial channels which are used to describe
           pixel colour.  This might be smaller than `num_channels' if, for
           example, opacity channels are to be rendered.  All channels are
           processed in the same way, except in the event that colour space
           conversion is required.
      */
    int get_num_colour_channels() { return num_colour_channels; }
      /* [SYNOPSIS] Returns the value of the public member variable,
         `num_colour_channels'.  Useful for function-only language bindings. */
    int *source_components;
      /* [SYNOPSIS]
           Array holding the indices of the codestream's output image
           components which are used to form each of the colour channels.
           There must be one entry for each channel, although multiple channels
           may reference the same component.  Also, the mapping between source
           component samples and channel sample values need not be direct.
      */
    int get_source_component(int n)
      { return ((n>=0) && (n<num_channels))?source_components[n]:-1; }
      /* [SYNOPSIS]
           Returns entry `n' of the public `source_components' array,
           or -1 if `n' lies outside the range 0 to `num_channels'-1.
           This function is useful for function-only language bindings. */
    int *default_rendering_precision;
      /* [SYNOPSIS]
           Indicates the default precision with which to represent the sample
           values returned by the `kdu_region_decompressor::process' function
           in the event that it is invoked with a `precision_bits' argument
           of 0.  A separate precision is provided for each channel.  The
           `configure' functions set these entries up with the bit-depth
           values available from the file format or code-stream headers, as
           appropriate, but you can change them to suit the needs of your
           application. */
    bool *default_rendering_signed;
      /* [SYNOPSIS]
           Similar to `default_rendering_precision', but used to identify
           whether or not each channel's samples should be rendered as signed
           quantities when the `kdu_region_decompressor::process' function
           is invoked with a zero-valued `precision_bits' argument.  The
           `configure' functions set these entries up with values recovered
           from the file format or code-stream headers, as appropriate, but
           you can change them to suit the needs of your application.
      */
    int get_default_rendering_precision(int n)
      { return ((n>=0) && (n<num_channels))?default_rendering_precision[n]:0; }
      /* [SYNOPSIS]
           Returns entry `n' of the public `default_rendering_precision'
           array, or 0 if `n' lies outside the range 0 to `num_channels'-1.
           This function is useful for function-only language bindings. */
    bool get_default_rendering_signed(int n)
      { return ((n>=0) && (n<num_channels))?default_rendering_signed[n]:false; }
      /* [SYNOPSIS]
           Returns entry `n' of the public `default_rendering_signed'
           array, or false if `n' lies outside the range 0 to `num_channels'-1.
           This function is useful for function-only language bindings. */
    int palette_bits;
      /* [SYNOPSIS]
           Number of index bits used with any palette lookup tables found in
           the `palette' array.
      */
    kdu_sample16 **palette;
      /* [SYNOPSIS]
           Array with `num_samples' entries, each of which is either NULL or
           else a pointer to a lookup table with 2^{`palette_bits'} entries.
           [//]
           Note carefully that each lookup table must have a unique buffer,
           even if all lookup tables hold identical contents.  The buffer must
           be allocated using `new', since it will automatically be de-allocated
           using `delete' when the present object is destroyed, or its `clear'
           function is called.
           [//]
           If `palette_bits' is non-zero and one or more entries in the
           `palette' array are non-NULL, the corresponding colour channels
           are recovered by scaling the relevant code-stream image component
           samples (see `source_components') to the range 0 through
           2^{`palette_bits'}-1 and applying them (as indices) to the relevant
           lookup tables.  If the code-stream image component has an unsigned
           representation (this is common), the signed samples recovered from
           `kdu_synthesis' or `kdu_decoder' will be level adjusted to unsigned
           values before applying them as indices to a palette lookup table.
           [//]
           The entries in any palette lookup table are 16-bit fixed point
           values, having KDU_FIX_POINT fraction bits and representing
           normalized quantities having the nominal range of -0.5 to +0.5.
      */
    jp2_colour_converter colour_converter;
      /* [SYNOPSIS]
           Initialized to an empty interface (`colour_converter.exists' returns
           false), you may call `colour_converter.init' to provide colour
           transformation capabilities.  This object is used by reference
           within `kdu_region_decompressor', so you should not alter its
           state while still engaged in region processing.
           [//]
           This object is initialized by the 2'nd and 3'rd forms of the
           `configure' function to convert the JP2 or JPX colour representation
           to sRGB, if possible.
      */
    jp2_colour_converter *get_colour_converter() { return &colour_converter; }
      /* [SYNOPSIS]
            Returns a pointer to the public member variable `colour_converter';
            useful for function-only language bindings.
      */
  };

/******************************************************************************/
/*                            kdu_region_decompressor                         */
/******************************************************************************/

class kdu_region_decompressor {
  /* [BIND: reference]
     [SYNOPSIS]
       Objects of this class provide a powerful mechanism for interacting with
       JPEG2000 compressed imagery.  They are particularly suitable for
       applications requiring interactive decompression, such as browsers
       and image editors, although there may be many other applications for
       the object.  The object abstracts all details associated with opening
       and closing tiles, colour transformations, interpolation (possibly
       by different amounts for each code-stream component) and determining
       the elements which are required to recover a given region of interest
       within the image.
       [//]
       The object also manages all state information required
       to process any selected image region (at any given scale) incrementally
       through multiple invocations of the `process' function.  This allows
       the CPU-intensive decompression operations to be interleaved with other
       tasks (e.g., user event processing) to maintain the responsiveness
       of an interactive application.
       [//]
       The implementation here is entirely platform independent, even though
       it may often be embedded inside applications which contain platform
       dependent code to manage graphical user interfaces.  If you are looking
       to build interfaces to Kakadu services from other languages, such as
       Java or Delphi, this class would be the best place to start, since
       it wraps up so much useful functionality, with a relatively small number
       of function calls.
       [//]
       From Kakadu version 5.1, this object offers multi-threaded processing
       capabilities for enhanced throughput.  These capabilities are based
       upon the options for multi-threaded processing offered by the
       underlying `kdu_multi_synthesis' object and the `kdu_synthesis' and
       `kdu_decoder' objects which it, in turn, uses.  Multi-threaded
       processing provides the greatest benefit on platforms with multiple
       physical CPU's, or where CPU's offer hyperthreading capabilities.
       Interestingly, although hyper-threading is often reported as offering
       relatively little gain, Kakadu's multi-threading model is typically
       able to squeeze 30-50% speedup out of processors which offer
       hyperthreading, in addition to the benefits which can be reaped from
       true multi-processor (or multi-core) architectures.  Even on platforms
       which do not offer either multiple CPU's or a single hyperthreading
       CPU, multi-threaded processing could be beneficial, depending on other
       bottlenecks which your decompressed imagery might encounter -- this is
       because underlying decompression tasks can proceed while other steps
       might be blocked on I/O conditions, for example.
       [//]
       To take advantage of multi-threading, you need to create a
       `kdu_thread_env' object, add a suitable number of working threads to
       it (see comments appearing with the definition of `kdu_thread_env') and
       pass it into the `start' function.  You can re-use this `kdu_thread_env'
       object as often as you like -- that is, you need not tear down and
       recreate the collaborating multi-threaded environment between calls to
       `finish' and `start'.  Multi-threading could not be much simpler.  The
       only thing you do need to remember is that all calls to `start',
       `process' and `finish' should be executed from the same thread -- the
       one identified by the `kdu_thread_env' reference passed to `start'.
       This constraint represents a slight loss of flexibility with respect
       to the core processing objects such as `kdu_multi_synthesis', which
       allow calls from any thread.  In exchange, however, you get simplicity.
       In particular, you only need to pass the `kdu_thread_env' object into
       the `start' function, after which the object remembers the thread
       reference for you.
  */
  public: // Member functions
    KDU_AUX_EXPORT kdu_region_decompressor();
    KDU_AUX_EXPORT virtual ~kdu_region_decompressor();
      /* [SYNOPSIS]
           Deallocates any resources which might have been left behind if
           a `finish' call was still pending when the object was destroyed.
      */
    KDU_AUX_EXPORT kdu_dims
      get_rendered_image_dims(
           kdu_codestream codestream, kdu_channel_mapping *mapping,
           int single_component, int discard_levels,
           kdu_coords expand_numerator,
           kdu_coords expand_denominator=kdu_coords(1,1),
           kdu_component_access_mode access_mode=KDU_WANT_OUTPUT_COMPONENTS);
      /* [SYNOPSIS]
           This function may be used to determine the size of the complete
           image on the rendering canvas, based upon the supplied component
           mapping rules, number of discarded resolution levels and rational
           expansion factors.  For further explanation of the rendering
           canvas and these various parameters, see the `start' function.
           The present function is provided to assist implementators in
           ensuring that the `region' they pass to `start' will indeed lie
           within the span of the image after all appropriate level
           discarding and expansion has taken place.
           [//]
           The function may not be called between `start' and `finish' -- i.e.
           while the object is open for processing.
      */
    kdu_dims get_rendered_image_dims() { return full_render_dims; }
      /* [SYNOPSIS]
           This version of the `get_rendered_image_dims' function returns the
           location and dimensions of the complete image on the rendering
           canvas, based on the parameters which were supplied to `start'.
           If the function is called prior to `start' or after `finish', it
           will return an empty region.
      */
    void set_white_stretch(int white_stretch_precision)
      { this->white_stretch_precision = white_stretch_precision; }
      /* [SYNOPSIS]
           For predictable behaviour, you should call this function only while
           the object is inactive -- i.e., before the first call to `start',
           or after the most recent call to `finish' and before any subsequent
           call to `start'.
           [//]
           The function sets up the "white stretch" mode which will become
           active when `start' is next called.  The same "white stretch" mode
           will be used in each subsequent call to `start' unless the present
           function is used to change it.  We could have added the information
           supplied by this function to the argument list accepted by `start',
           but the number of optional arguments offered by that function has
           already become large enough.
           [//]
           So, what is this "white stretch" mode, and what does it have to
           do with precision?  To explain this, we begin by explaining what
           happens if you set `white_stretch_precision' to 0 -- this is the
           default, which also corresponds to the object's behaviour prior
           to Kakadu Version 5.2 for backward compatibility.  Suppose the
           original image samples which were compressed had a precision of
           P bits/sample, as recorded in the codestream and/or reported by
           the `jp2_dimensions' interface to a JP2/JPX source.  The actual
           precision may differ from component to component, of course, but
           let's keep things simple for the moment).
           [//]
           For the purpose of colour transformations and conversion to
           different rendering precisions (as requested in the relevant call
           to `process'), Kakadu applies the uniform interpretation that
           unsigned quantities range from 0 (black) to 2^P (max intensity);
           signed quantities are assumed to lie in the nominal range of
           -2^{P-1} to 2^{P-1}.  Thus, for example:
           [>>] to render P=12 bit samples into a B=8 bit buffer, the object
                simply divides by 16 (downshifts by 4=P-B);
           [>>] to render P=4 bit samples into a B=8 bit buffer, the object
                multiplies by 16 (upshifts by 4=B-P); and
           [>>] to render P=1 bit samples into a B=8 bit buffer, the object
                multiplies by 128 (upshifts by 7=B-P).
           [//]
           This last example, reveals the weakness of a pure shifting scheme
           particularly well, since the maximum attainable value in the 8-bit
           rendering buffer from a 1-bit source will be 128, as opposed to the
           expected 255, leading to images which are darker than would be
           suspected.  Nevertheless, this policy has merits.  One important
           merit is that the original sample values are preserved exactly,
           so long as the rendering buffer has precision B >= P.  The
           policy also has minimal computational complexity and produces
           visually excellent results except for very low bit-depth images.
           Moreover, very low bit-depth images are often used to index
           a colour palette, which performs the conversion from low to high
           precision in exactly the manner prescribed by the image content
           creator.
           [//]
           Nevertheless, it is possible that low bit-depth image components
           are used without a colour palette and that applications will want
           to render them to higher bit-depth displays.  The most obvious
           example of this is palette-less bi-level images, but another
           example is presented by images which have a low precision associted
           alpha (opacity) channel.  To render such images in the most
           natural way, unsigned sample values with P < B should ideally be
           stretched by (1-2^{-B})/(1-2^{-P}), prior to rendering, in
           recognition of the fact that the maximum display output value is
           (1-2^{-B}) times the assumed range of 2^B, while the maximum
           intended component sample value (during content creation) was
           probably (1-2^{-P}) times the assumed range of 2^P.
           It is not entirely clear whether the same interpretations are
           reasonable for signed sample values, but the function extends the
           policy to both signed and unsigned samples by fixing the lower
           bound of the nominal range and stretching the length of the range
           by (1-2^{-B})/(1-2^{-P}).  In fact, the internal representation of
           all component samples is signed so as to optimally exploit the
           dynamic range of the available word sizes.
           [//]
           To facilitate the stretching procedure described above, the
           present function allows you to specify the value of B that you
           would like to be applied in the stretching process.  This is the
           value of the `white_stretch_precision' argument.  Normally, B
           will coincide with the value of the `precision_bits' argument
           you intend to supply to the `process' function (often B=8).
           However, you can use a different value.  In particular, using a
           smaller value for B here will reduce the likelihood that white
           stretching is applied (also reducing the accuracy of the stretch,
           of course), which may represent a useful computational saving
           for your application.  Also, when the target rendering precision is
           greater than 8 bits, it is unclear whether your application will
           want stretching or not -- if so, stretching to B=8 bits might be
           quite accurate enough for you, differing from the optimal stretch
           value by at most 1 part in 256.  The important thing to note is
           that stretching will occur only when the component sample precision
           P is less than B, so 8-bit original samples will be completely
           unchanged if you specify B <= 8.  In particular, the default
           value of B=0 means that no stretching will ever be applied.
         [ARG: white_stretch_precision]
           This argument holds the target stretching precision, B, explained
           in the discussion above.  White stretching will be applied only
           to image components (or JP2/JPX/MJ2 palette outputs) whose
           nominated precision P is less than B.
      */
    KDU_AUX_EXPORT bool
      start(kdu_codestream codestream, kdu_channel_mapping *mapping,
           int single_component, int discard_levels, int max_layers,
           kdu_dims region, kdu_coords expand_numerator,
           kdu_coords expand_denominator=kdu_coords(1,1), bool precise=false,
           kdu_component_access_mode access_mode=KDU_WANT_OUTPUT_COMPONENTS,
           bool fastest=false, kdu_thread_env *env=NULL,
           kdu_thread_queue *env_queue=NULL);
      /* [SYNOPSIS]
           Prepares to decompress a new region of interest.  The actual
           decompression work is performed incrementally through successive
           calls to `process' and terminated by a call to `finish'.  This
           incremental processing strategy allows the decompression work to
           be interleaved with other tasks, e.g. to preserve the
           responsiveness of an interactive application.  There is no need
           to process the entire region of interest established by the
           present function call; `finish' may be called at any time, and
           processing restarted with a new region of interest.  This is
           particularly helpful in interactive applications, where an
           impatient user's interests may change before processing of an
           outstanding region is complete.
           [//]
           If `mapping' is NULL, a single image component is to be
           decompressed, whose index is identified by `single_component'.
           Otherwise, one or more image components must be decompressed and
           subjected to potentially quite complex mapping rules to generate
           channels for display; the relevant components and mapping
           rules are identified by the `mapping' object.
           [//]
           The `region' argument identifies the image region which is to be
           decompressed.  This region is defined with respect to a
           `rendering canvas'.  The rendering canvas might be identical to the
           high resolution canvas associated with the code-stream, but it is
           often different.
           [//]
           The `expand_numerator' and `expand_denominator' arguments identify
           the amount by which the first channel described by `mapping' (or
           the single image component if `mapping' is NULL) should be
           expanded to obtain its representation on the rendering canvas.
           To be more precise, let Cr be the index of the reference image
           component (the first one identified by `mapping' or the
           single image component if `mapping' is NULL).  Also, let (Px,Py)
           and (Sx,Sy) denote the upper left hand corner and the dimensions,
           respectively, of the region returned by `kdu_codestream::get_dims',
           for component Cr.  Note that these dimensions take into account
           the effects of the `discard_levels' argument, as well as any
           orientation adjustments created by calls to
           `kdu_codestream::change_appearance'.  The location and dimensions
           of the image on the rendering canvas are then given by
                    ( ceil(Px*Nx/Dx-Hx), ceil(Py*Ny/Dy-Hy) )
           and
                    ( ceil((Px+Sx)*Nx/Dx-Hx)-ceil(Px*Nx/Dx-Hx),
                      ceil((Py+Sy)*Ny/Dy-Hy)-ceil(Py*Ny/Dy-Hy) )
           respectively, where (Nx,Ny) are found in `expand_numerator',
           (Dx,Dy) are found in `expand_denominator', and (Hx,Hy) are
           intended to represent approximately "half pixel" offsets.
           Specifically, Hx=floor((Nx-1)/2)/Dx and Hy=floor((Ny-1)/2)/Dy.
           Since the above formulae can be tricky to reproduce precisely,
           the `get_rendered_image_dims' function is provided to learn the
           exact location and dimensions of the image on the rendering canvas.
           [//]
           You are required to ensure that the `expand_numerator'
           and `expand_denominator' coordinates are strictly positive and
           that they do not require downsampling of any of the image
           components involved.  If the codestream subsampling factors for
           all of the image components involved are at least as large as
           those associated with the reference component (this should almost
           always be the case in practical applications), this simply means
           that you must select Nx >= Dx > 1 and Ny >= Dy > 1.
           [//]
           In the current implementation, expansion by integer factors is
           implemented by pixel replication, while expansion by other factors
           is implemented using bi-linear interpolation.
           [//]
           Note carefully that this function calls the
           `kdu_codestream::apply_input_restrictions' function, which will
           destroy any restrictions you may previously have imposed.  This
           may also alter the current component access mode interpretation.
           For this reason, the function provides you with a separate
           `access_mode' argument which you can set to one of
           `KDU_WANT_CODESTREAM_COMPONENTS' or `KDU_WANT_OUTPUT_COMPONENTS',
           to control the way in which image component indices should be
           interpreted and whether or not multi-component transformations
           defined at the level of the code-stream should be performed.
         [RETURNS]
           False if a fatal error occurred and an exception (usually generated
           from within the error handler associated with `kdu_error') was
           caught.  In this case, you need not call `finish', but you should
           generally destroy the codestream interface passed in here.
         [ARG: codestream]
           Interface to the internal code-stream management machinery.  Must
           have been created (see `codestream.create') for input.
         [ARG: mapping]
           Points to a structure whose contents identify the relationship
           between image components and the channels to be rendered.  The
           interpretation of these image components depends upon the
           `access_mode' argument.  Any or all of the image
           components may be involved in rendering channels; these might be
           subjected to a palette lookup operation and/or specific colour space
           transformations.  The channels need not all represent colour, but
           the initial `mapping->num_colour_channels' channels do represent
           the colour of a pixel.
           [//]
           If the `mapping' pointer is NULL, only one image component is to
           be rendered, as a monochrome image; the relevant component's
           index is supplied via the `single_component' argument.
         [ARG: single_component]
           Ignored, unless `mapping' is NULL, in which case this argument
           identifies the image component (starting from 0) which
           is to be rendered as a monochrome image.  The interpretation of
           this image component index depends upon the `access_mode'
           argument.
         [ARG: discard_levels]
           Indicates the number of highest resolution levels to be discarded
           from each image component's DWT.  Each discarded level typically
           halves the dimensions of each image component, although note that
           JPEG2000 Part-2 supports downsampling factor styles in which
           only one of the two dimensions might be halved between levels.
           Recall that the rendering canvas is determined by applying the
           expansion factors represented by `expand_numerator' and
           `expand_denominator' to the dimensions of the reference image
           component, as it appears after taking the number of discarded
           resolution levels (and any appearance changes) into account.  Thus,
           each additional discarded resolution level serves to reduce the
           dimensions of the entire image as it would appear on the rendering
           canvas.
         [ARG: max_layers]
           Maximum number of quality layers to use when decompressing
           code-stream image components for rendering.  The actual number of
           layers which are available might be smaller or larger than this
           limit and may vary from tile to tile.
         [ARG: region]
           Location and dimensions of the new region of interest, expressed
           on the rendering canvas.  This region should be located
           within the region returned by `get_rendered_image_dims'.
         [ARG: expand_numerator]
           Numerator of the rational expansion factors which are applied to
           the reference image component.
         [ARG: expand_denominator]
           Denominator of the rational expansion factors which are applied to
           the reference image component.
         [ARG: precise]
           Setting this argument to true encourages the implementation to
           use higher precision internal representations when decompressing
           image components.  The precision of the internal representation
           is not directly coupled to the particular version of the overloaded
           `process' function which is to be used.  The lower precision
           `process' function may be used with higher precision internal
           computations, or vice-versa, although it is natural to couple
           higher precision computations with calls to a higher precision
           `process' function.
           [//]
           Note that a higher precision internal representation may be adopted
           even if `precise' is false, if it is deemed to be necessary for
           correct decompression of some particular image component.  Note
           also that higher precision can be maintained throughout the entire
           process, only for channels whose contents are taken directly from
           decompressed image components.  If any palette lookup, or colour
           space conversion operations are required, the internal precision
           for those channels will be reduced (at the point of conversion) to
           16-bit fixed-point with `KDU_FIX_POINT' fraction bits -- due to
           current limitations in the `jp2_colour_converter' object.
           [//]           
           Of course, most developers may remain blissfully ignorant of such
           subtleties, since they relate only to internal representations and
           approximations.
         [ARG: fastest]
           Setting this argument to true encourages the implementation to use
           lower precision internal representations when decompressing image
           components, even if this results in the loss of image quality.
           The argument is ignored unless `precise' is false.  The argument
           is essentially passed directly to `kdu_multi_synthesis::create',
           along with the `precises' argument, as that function's
           `want_fastest' and `force_precise' arguments, respectively.
         [ARG: access_mode]
           This argument is passed directly through to the
           `kdu_codestream::apply_input_restrictions' function.  It thus affects
           the way in which image components are interpreted, as found in the
           `single_component_idx' and `mapping' arguments.  For a detailed
           discussion of image component interpretations, consult the second
           form of the `kdu_codestream::apply_input_restrictions' function.
           It suffices here to mention that this argument must take one of
           the values `KDU_WANT_CODESTREAM_COMPONENTS' or
           `KDU_WANT_OUTPUT_COMPONENTS' -- for more applications the most
           appropriate value is `KDU_WANT_OUTPUT_COMPONENTS' since these
           correspond to the declared intentions of the original codestream
           creator.
         [ARG: env]
           This argument is used to establish multi-threaded processing.  For
           a discussion of the multi-threaded processing features offered
           by the present object, see the introductory comments to
           `kdu_region_decompressor'.  We remind you here, however, that
           all calls to `start', `process' and `finish' must be executed
           from the same thread, which is identified only in this function.
           For the single-threaded processing model used prior to Kakadu
           version 5.1, set this argument to NULL.
         [ARG: env_queue]
           This argument is ignored unless `env' is non-NULL, in which case
           a non-NULL `env_queue' means that all multi-threaded processing
           queues created inside the present object, by calls to `process',
           should be created as sub-queues of the identified `env_queue'.
           [//]
           One application for a non-NULL `env_queue' might be one which
           processes two frames of a video sequence in parallel.  There
           can be some benefit to doing this, since it can avoid the small
           amount of thread idle time which often appears at the end of
           the last call to the `process' function prior to `finish'.  In
           this case, each concurrent frame would have its own `env_queue', and
           its own `kdu_region_decompressor' object.  Moreover, the
           `env_queue' associated with a given `kdu_region_decompressor'
           object can be used to run a job which invokes the `start',
           `process' and `finish' member functions.  In this case, however,
           it is particularly important that the `start', `process' and
           `finish' functions all be called from within the execution of a
           single job, since otherwise there is no guarantee that they would
           all be executed from the same thread, whose importance has
           already been stated above.
      */
    KDU_AUX_EXPORT bool
      process(kdu_byte **channel_bufs, bool expand_monochrome,
              int pixel_gap, kdu_coords buffer_origin, int row_gap,
              int suggested_increment, int max_region_pixels,
              kdu_dims &incomplete_region, kdu_dims &new_region,
              int precision_bits=8, bool measure_row_gap_in_pixels=true);
      /* [SYNOPSIS]
           This powerful function is the workhorse of a typical interactive
           image rendering application.  It is used to incrementally decompress
           an active region into identified portions of one or more
           application-supplied buffers.  Each call to the function always
           decompresses some whole number of lines of one or more horizontally
           adjacent tiles, aiming to produce roughly the number of samples
           suggested by the `suggested_increment' argument, unless that value
           is smaller than a single line of the current tile, or larger than
           the number of samples in a row of horizontally adjacent tiles.
           The newly rendered samples are guaranteed to belong to a rectangular
           region -- the function returns this region via the
           `new_region' argument.  This, and all other regions manipulated
           by the function are expressed relative to the `rendering canvas'
           (the coordinate system associated with the `region' argument
           supplied to the `start' member function).
           [//]
           Decompressed samples are automatically colour transformed,
           clipped, level adjusted, interpolated and colour appearance
           transformed, as necessary.  The result is a collection of
           rendered image pixels, each of which has the number of channels
           described by the `kdu_channel_mapping::num_channels' member of
           the `kdu_channel_mapping' object passed to `start' (or just one
           channel, if no `kdu_channel_mapping' object was passed to `start').
           The initial `kdu_channel_mapping::num_colour_channels' of these
           describe the pixel colour, while any remaining channels describe
           auxiliary properties such as opacity.  Other than these few
           constraints, the properties of the channels are entirely determined
           by the way in which the application configures the
           `kdu_channel_mapping' object.
           [//]
           The rendered channel samples are written to the buffers supplied
           via the `channel_bufs' array.  If `expand_monochrome' is false,
           this array must have exactly one entry for each of the channels
           described by the `kdu_channel_mapping' object supplied to `start'.
           The entries may all point to offset locations within a single
           channel-interleaved rendering buffer, or else they may point to
           distinct buffers for each channel; this allows the application to
           render to buffers with a variety of interleaving conventions.
           [//]
           If `expand_monochrome' is true and the number of colour channels
           (see `kdu_channel_mapping::num_colour_channels') is exactly equal
           to 1, the function automatically copies the single (monochrome)
           colour channel into 3 identical colour channels whose buffers
           appear as the first three entries in the `channel_bufs' array.
           This is a convenience feature to support direct rendering of
           monochrome images into 24- or 32-bpp display buffers, with the
           same ease as full colour images.  Your application is not obliged
           to use this feature, of course.
           [//]
           Each buffer referenced by the `channel_bufs' array has horizontally
           adjacent pixels separated by `pixel_gap'.  Regarding vertical
           organization, however, two distinct configurations are supported.
           [//]
           If `row_gap' is 0, successive rows of the region written into
           `new_region' are concatenated within each channe buffer, so that
           the row gap is effectively equal to `new_region.size.x' -- it is
           determined by the particular dimensions of the region processed
           by the function.  In this case, the `buffer_origin' argument is
           ignored.
           [//]
           If `row_gap' is non-zero, each channel buffer points to the location
           identified by `buffer_origin' (on the rendering canvas), and each
           successive row of the buffer is separated by the amount determined
           by `row_gap'.  In this case, it is the application's responsibility
           to ensure that the buffers will not be overwritten if any samples
           from the `incomplete_region' are written onto the buffer, taking
           the `buffer_origin' into account.  In particular, the
           `buffer_origin' must not lie beyond the first row or first column
           of the `incomplete_region'.  Note that the interpretation of
           `row_gap' depends upon the `measure_row_gap_in_pixels' argument.
           [//]
           On entry, the `incomplete_region' structure identifies the subset
           of the original region supplied to `start', which has not yet been
           decompressed and is still relevant to the application.  The function
           uses this information to avoid unnecessary processing of tiles
           which are no longer relevant, unless these tiles are already opened
           and being processed.
           [//]
           On exit, the upper boundary of the `incomplete_region' is updated
           to reflect any completely decompressed rows.  Once the region
           becomes empty, all processing is complete and future calls will
           return false.
           [//]
           The function may return true, with `new_region' empty.  This can
           happen, for example, when skipping over unnecessary tile or
           group of tiles.  The intent is to avoid the possibility that the
           caller might be forced to wait for an unbounded number of tiles to
           be loaded (possibly from disk) while hunting for one which has a
           non-empty intersection with the `incomplete_region'.  In general,
           the current implementation limits the number of new tiles which
           will be opened to one row of horizontally adjacent tiles.  In this
           way, a number of calls may be required before the function will
           return with `new_region' non-empty.
           [//]
           If the underlying code-stream is found to be sufficiently corrupt
           that the decompression process must be aborted, the current function
           will catch any exceptions thrown from an application supplied
           `kdu_error' handler, returning false prematurely.  This condition
           will be evident from the fact that `incomplete_region' is non-empty.
           You should still call `finish' and watch the return value from that
           function.
           [//]
           Java note: if you are invoking this function using the Java native
           API, the function may not return directly after an internal error
           in the code-stream, if the `kdu_customize_errors' function was
           supplied an object which ultimately throws a Java exception.  In
           this case, the error condition must be caught by a catch statement
           -- i.e., catch(KduException e) -- surrounding the call.  The
           catch statement should then call `finish' and watch its return
           value, in the same manner described above for direct native callers.
         [RETURNS]
           False if the `incomplete_region' becomes empty as a result of this
           call, or if an internal error occurs during code-stream data
           processing and an exception was thrown by the application-supplied
           `kdu_error' handler.  In either case, the correct response to a
           false return value is to call `finish' and check its return value.
           Note, however, that invocations from Java may cause a KduException
           Java exception to be thrown, as explained above.
           [//]
           If `finish' returns false, an internal error has occurred and
           you must close the `kdu_codestream' object (possibly re-opening it
           for subsequent processing, perhaps in a more resilient mode -- see
           `kdu_codestream::set_resilient').
           [//]
           If `finish' returns true, the incomplete region might not have
           been completed if the present function found that you were
           attempting to discard too many resolution levels or to flip an
           image which cannot be flipped, due to the use of certain packet
           wavelet decomposition structures.  To check for the former
           possibility, you should always check the value returned by
           `kdu_codestream::get_min_dwt_levels' after `finish' returns true,
           possibly adjusting the current display resolution before further
           processing.  To check for the second possibility, you should also
           test the value returned by `kdu_codestream::can_flip', possibly
           adjusting the appearance of the codestream (via
           `kdu_codestream::change_appearance') before further processing.
           Note that the values returned by `kdu_codestream::get_min_dwt_levels'
           and `kdu_codestream::can_flip' can become progressively more
           restrictive over time, as more tiles are visited.
           [//]
           Note carefully that this function may return true, even though it has
           decompressed no new data of interest to the application (`new_region'
           empty).  This is because a limited number of new tiles are opened
           during each call, and these tiles might not have any intersection
           with the current `incomplete_region' -- the application might have
           reduced the incomplete region to reflect changing interests.
         [ARG: channel_bufs]
           Array with `kdu_channel_mapping::num_channels' entries, or
           `kdu_channel_mapping::num_channels'+2 entries.  The latter applies
           only if `expand_monochrome' is true and
           `kdu_channel_mapping::num_colour_channels' is equal to 1.  If
           no `kdu_channel_mapping' object was passed to `start', the number
           of channel buffers is equal to 1 (if `expand_monochrome' is false)
           or 3 (if `expand_monochrome' is true).  The entries in the
           `channel_bufs' array are not modified by this function.
           [//]
           If any entry in the array is NULL, that channel will effectively
           be skipped.  This can be useful, for example, if the value of
           `kdu_channel_mapping::num_colour_channels' is larger than the
           number of channels produced by a colour transform supplied by
           `kdu_channel_mapping::colour_transform' -- for example, a CMYK
           colour space (`kdu_channel_mapping::num_colour_channels'=4)
           might be converted to an sRGB space, so that the 4'th colour
           channel, after conversion, becomes meaningless.
         [ARG: expand_monochrome]
           If true and the number of colour channels identified by
           `kdu_channel_mapping::num_colour_channels' is 1, or if no
           `kdu_channel_mapping' object was passed to `start', the function
           automatically copies the colour channel data into the first
           three buffers supplied via the `channel_bufs' array, and the second
           real channel, if any, corresponds to the fourth entry in the
           `channel_bufs' array.
         [ARG: pixel_gap]
           Separation between consecutive pixels, in each of the channel
           buffers supplied via the `channel_bufs' argument.
         [ARG: buffer_origin]
           Location, in rendering canvas coordinates, of the upper left hand
           corner pixel in each channel buffer supplied via the `channel_bufs'
           argument, unless `row_gap' is 0, in which case, this argument is
           ignored.
         [ARG: row_gap]
           If zero, rendered image lines will simply be concatenated into the
           channel buffers supplied via `channel_bufs', so that the line
           spacing is given by the value written into `new_region.size.x'
           upon return.  In this case, `buffer_origin' is ignored.  Otherwise,
           this argument identifies the separation between vertically adjacent
           pixels within each of the channel buffers.  If
           `measure_row_gap_in_pixels' is true, the number of samples
           between consecutive buffer lines is `row_gap'*`pixel_gap'.
           Otherwise, it is just `row_gap'.
         [ARG: suggested_increment]
           Suggested number of samples to decompress from the code-stream
           component associated with the first channel (or the single
           image component) before returning.  Of course, there will often
           be other image components which must be decompressed as well, in
           order to reconstruct colour imagery; the number of these samples
           which will be decompressed is adjusted in a commensurate fashion.
           Note that the decompressed samples may be subject to interpolation
           (if the `expand_numerator' and `expand_denominator' arguments
           supplied to the `start' member function represent expansion
           factors which are larger than 1); the `suggested_increment' value
           refers to the number of decompressed samples prior to any such
           interpolation.  Note also that the function is free to make up its
           own mind exactly how many samples it will process in the current
           call, which may vary from 0 to the entire `incomplete_region'.
           [//]
           For interactive applications, typical values for the
           `suggested_increment' argument might range from tens of thousands,
           to hundreds of thousands.
           [//]
           If `row_gap' is 0, and the present argument is 0, the only
           constraint will be that imposed by `max_region_pixels'.
         [ARG: max_region_pixels]
           Maximum number of pixels which can be written to any channel buffer
           provided via the `channel_bufs' argument.  This argument is
           ignored unless `row_gap' is 0, since only in that case is the
           number of pixels which can be written governed solely by the size
           of the buffers.  An error will be generated (through `kdu_error') if
           the supplied limit is too small to accommodate even a single line
           from the new region.  For this reason, you should be careful to
           ensure that `max_region_pixels' is at least as large as
           `incomplete_region.size.x'.
         [ARG: incomplete_region]
           Region on the rendering canvas which is still required by the
           application.  This region should be a subset of the region of
           interest originally supplied to `start'.  However, it may be much
           smaller.  The function works internally with a bank of horizontally
           adjacent tiles, which may range from a single tile to an entire
           row of tiles (or part of a row of tiles).  From within the current
           tile bank, the function decompresses lines as required
           to fill out the incomplete region, discarding any rows from already
           open tiles which do not intersect with the incomplete region.  On
           exit, the first row in the incomplete region will be moved down
           to reflect any completely decompressed image rows.  Note, however,
           that the function may decompress image data and return with
           `new_region' non-empty, without actually adjusting the incomplete
           region.  This happens when the function decompresses tile data
           which intersects with the incomplete region, but one or more tiles
           remain to the right of that region.  Generally speaking, the function
           advances the top row of the incomplete region only when it
           decompresses data from right-most tiles which intersect with that
           region, or when it detects that the identity of the right-most
           tile has changed (due to the width of the incomplete region
           shrinking) and that the new right-most tile has already been
           decompressed.
         [ARG: new_region]
           Used to return the location and dimensions of the region of the
           image which was actually decompressed and written into the channel
           buffers supplied via the `channel_bufs' argument.  The region's
           size and location are all expressed relative to the same rendering
           canvas as the `incomplete_region' and `buffer_origin' arguments.
           Note that the region might be empty, even though processing is not
           yet complete.
         [ARG: precision_bits]
           Indicates the precision of the unsigned integers represented by
           each sample in each buffer supplied via the `channel_bufs' argument.
           This value need not bear any special relationship to the original
           bit-depth of the compressed image samples.  The rendered sample
           values are written into the buffer as B-bit unsigned integers,
           where B is the value of `precision_bits' and the most significant
           bits of the B-bit integer correspond to the most significant bits
           of the original image samples.  Normally, the value of this argument
           will be 8 so that the rendered data is always normalized for display
           on an 8-bit/sample device.  There may be more interest in selecting
           different precisions when using the second form of the overloaded
           `process' function.
           [//]
           From Kakadu v4.3, it is possible to supply a special value of 0
           for this argument.  In this case, a "default" set of precisions
           will be used (one for each channel).  If a `kdu_channel_mapping'
           object was supplied to `start', that object's
           `kdu_channel_mapping::default_rendering_precision' and
           `kdu_channel_mapping::default_rendering_signed' arrays are used
           to derive the default channel precisions, as well as per-channel
           information about whether the samples should be rendered as
           unsigned quantities or as 2's complement 8-bit integers.  That
           information, in turn, is typically initialized by one of the
           `kdu_channel_mapping::configure' functions to represent the
           native bit-depths and signed/unsigned properties of the original
           image samples (or palette indices); however, it may be explicitly
           overridden by the application.  This gives you enormous flexibility
           in choosing the way you want rendered sample bits to be
           represented.  If no `kdu_channel_mapping' object was supplied to
           `start', the default rendering precision and signed/unsigned
           characteristics are derived from the original properties of the
           image samples represented by the code-stream.
           [//]
           It is worth noting that the rendering precision, B, can actually
           exceed 8, either because `precision_bits' > 8, or because
           `precision_bits'=0 and the default rendering precisions, derived
           in the above-mentioned manner, exceed 8.  In this case, the
           function automatically truncates the rendered values to fit
           within the 8-bit representation associated with the `channel_bufs'
           array(s).  If B <= 8, the rendered values are truncated to fit
           within the B bits.  Where 2's complement output samples are
           written, they are truncated in the natural way and sign extended.
         [ARG: measure_row_gap_in_pixels]
           If true, `row_gap' is interpreted as the number of whole pixels
           between consecutive rows in the buffer.  Otherwise, it is interpreted
           as the number of samples only.  The latter form can be useful when
           working with image buffers having alignment constraints which are
           not based on whole pixels (e.g., Windows bitmap buffers).
      */
    KDU_AUX_EXPORT bool
      process(kdu_uint16 **channel_bufs, bool expand_monochrome,
              int pixel_gap, kdu_coords buffer_origin, int row_gap,
              int suggested_increment, int max_region_pixels,
              kdu_dims &incomplete_region, kdu_dims &new_region,
              int precision_bits=16, bool measure_row_gap_in_pixels=true);
      /* [SYNOPSIS]
           Same as the first form of the overloaded `process' function, except
           that the channel buffers each hold 16-bit unsigned quantities.  As
           before, the `precision_bits' argument is used to control the
           representation written into each output sample.  If it is 0, default
           precisions are obtained, either from the `kdu_channel_mapping'
           object or from the codestream, as appropriate, and these defaults
           might also cause the 16-bit output samples to hold 2's complement
           signed quantities.  Also, as before, the precision requested
           explicitly or implicitly (via `precision_bits'=0) may exceed 16.
           In each case, the most natural truncation procedures are
           employed for out-of-range values, following the general
           strategy outlined in the comments accompanying the
           `precision_bits' argument in the first form of the `process'
           function.
      */
    KDU_AUX_EXPORT bool
      process(kdu_int32 *buffer, kdu_coords buffer_origin,
              int row_gap, int suggested_increment, int max_region_pixels,
              kdu_dims &incomplete_region, kdu_dims &new_region);
      /* [SYNOPSIS]
           This function is equivalent to invoking the first form of the
           overloaded `process' function, with four 8-bit channel buffers.
           [>>] The 1st channel buffer (nominally RED) corresponds to the 2nd
                most significant byte of each integer in the `buffer'.
           [>>] The 2nd channel buffer (nominally GREEN) corresponds to the 2nd
                least significant byte of each integer in the `buffer'.
           [>>] The 3rd channel buffer (nominally BLUE) corresponds to the least
                significant byte of each integer in the `buffer'.
           [>>] The 4th channel buffer (nominally ALPHA) corresponds to the most
                significant byte of each integer in the `buffer'.
           [//]
           If the source has only one colour channel, the second and third
           channel buffers are copied from the first channel buffer (same as
           the `expand_monochrome' feature found in other forms of the `process'
           function).
           [//]
           If there are more than 3 colour channels, only the first 3 will
           be transferred to the supplied `buffer'.
           [//]
           If there is no alpha information, the fourth channel buffer (most
           significant byte of each integer) is set to 0xFF.
           [//]
           Apart from convenience, one reason for providing this function
           in addition to the other, more general forms of the `process'
           function, is that it is readily mapped to alternate language
           bindings, such as Java.
           [//]
           Another important reason for providing this form of the `process'
           function is that missing alpha data is always synthesized (with
           0xFF), so that a full 32-bit word is aways written for each pixel.
           This can significantly improve memory access efficiency on some
           common platforms.  It also allows for efficient internal
           implementations in which the rendered channel data is written
           in aligned 16-byte chunks wherever possible, without any need
           to mask off unused intermediate values.  To fully exploit this
           capability, you are strongly recommended to suppy a 16-byte
           aligned `buffer'.
      */
    KDU_AUX_EXPORT bool
      process(kdu_byte *buffer, int *channel_offsets,
              int pixel_gap, kdu_coords buffer_origin, int row_gap,
              int suggested_increment, int max_region_pixels,
              kdu_dims &incomplete_region, kdu_dims &new_region,
              int precision_bits=8, bool measure_row_gap_in_pixels=true);
      /* [SYNOPSIS]
           Same as the first form of the overloaded `process' function, except
           that the channel buffers are interleaved into a single buffer that
           is max_region_pixels*num_channels in size.
           [//]
           The main reason for providing this function in addition to the first
           form of the `process' function, is that this version is readily
           mapped to alternate language bindings, such as Java.
           [//]
           Unfortunately, with this function, the ability to specifically
           declare that you are not interested in one or more of the
           channels (by setting the relevant buffer pointer to NULL) must
           be sacrificed.
         [ARG: buffer]
           Array max_region_pixels*kdu_channel_mapping::num_channels in size.
         [ARG: channel_offsets]
           Array with `kdu_channel_mapping::num_channels' entries. Each entry
           specifies the offset in bytes to the first byte of the associated
           channel data.
      */
  KDU_AUX_EXPORT bool
    process(kdu_uint16 *buffer, int *channel_offsets,
            int pixel_gap, kdu_coords buffer_origin, int row_gap,
            int suggested_increment, int max_region_pixels,
            kdu_dims &incomplete_region, kdu_dims &new_region,
            int precision_bits=16, bool measure_row_gap_in_pixels=true);
      /* [SYNOPSIS]
           Same as the second form of the overloaded `process' function, except
           that the channel buffers are interleaved into a single buffer that
           is max_region_pixels*num_channels in size.
           [//]
           The main reason for providing this function in addition to the second
           form of the `process' function, is that this version is readily
           mapped to alternate language bindings, such as Java.
           [//]
           Unfortunately, with this function, the ability to specifically
           declare that you are not interested in one or more of the
           channels (by setting the relevant buffer pointer to NULL) must
           be sacrificed.
         [ARG: buffer]
           Array max_region_pixels*kdu_channel_mapping::num_channels in size.
         [ARG: channel_offsets]
           Array with `kdu_channel_mapping::num_channels' entries. Each entry
           specifies the offset in 16-bit units to the first byte of the
           associated channel data.
      */
    KDU_AUX_EXPORT bool finish();
      /* [SYNOPSIS]
           Every call to `start' must be matched by a call to `finish';
           however, you may call `finish' prematurely.  This allows processing
           to be terminated on a region whose intersection with a display
           window has become too small to justify the effort.
         [RETURNS]
           If the function returns false, a fatal error has occurred in the
           underlying code-stream and you must destroy the codestream object
           (use `kdu_codestream::destroy').  You will probably also have
           to close the relevant compressed data source (e.g., using
           `kdu_compressed_source::close').  This should clean up all the
           resources correctly, in preparation for subsequently opening a new
           code-stream for further decompression and rendering work.
      */
  protected: // Implementation helpers which might be useful to extended classes
    bool process_generic(int sample_bytes, int pixel_gap,
                         kdu_coords buffer_origin, int row_gap,
                         int suggested_increment, int max_region_pixels,
                         kdu_dims &incomplete_region, kdu_dims &new_region,
                         int precision_bits, bool fill_alpha=false);
      /* Implements both the 8-bit and 16-bit `process' functions, where the
         buffer pointers are stored in the internal `channel_bufs' array.
         `sample_bytes' must be equal to 1 (for 8-bit samples) or 2 (for 16-bit
         samples.  Note that `row_gap' here is measured in samples, not
         pixels.  If `fill_alpha' is true, the last channel buffer is to be
         filled with 0xFF, rather than deriving it from rendered imagery.
         This option will only be used if the alpha channel is the most
         significant byte in each 32-bit word of the buffer supplied to the
         second form of the `process' function. */
  private: // Implementation helpers
    void set_num_channels(int num);
      /* Convenience function to allocate and initialize the `channels' array
         as required to manage a total of `num' channels.  Sets `num_channels'
         and `num_colour_channels' both equal to `num' before returning.
         Also, initializes the `kd_channel::native_precision' value to 0
         and `kd_channel::native_signed' to false for each channel. */
    kd_component *add_component(int comp_idx);
      /* Adds a new component to the `components' array, if necessary, returning
         a pointer to that component. */
    bool start_tile_bank(kd_tile_bank *bank, kdu_long suggested_tile_area,
                         kdu_dims incomplete_region);
      /* This function uses `suggested_tile_area' to determine the number of
         new tiles which should be opened as part of the new tile bank.  The
         value of `suggested_tile_area' represents a suggested number of
         samples to decompress for the image component which describes
         channel 0.  The function opens horizontally adjacent tiles until
         the area of the component used by channel 0 (prior to any
         interpolation) exceeds the value of `suggested_tile_area'.
         [//]
         On entry, the supplied `bank' must have its `num_tiles' member set
         to 0, meaning that it is closed.  Tiles are opened starting from
         the tile identified by the `next_tile_idx' member, which is
         adjusted as appropriate.
         [//]
         Note that this function does not make any adjustments to the
         `render_dims' member or any of the members in the `components' or
         `channels' arrays.  To prepare the state of those objects, the
         `make_tile_bank_current' function must be invoked on a tile-bank
         which has already been started.
         [//]
         If the function is unable to open new tiles due to restrictions on
         the number of DWT levels or decomposition-structure-imposed
         restrictions on the allowable flipping modes, the function returns
         false.
         [//]
         The last argument is used to identify tiles which have no overlap
         with the current `incomplete_region'.  The supplied region is
         identical to that passed into the `process' functions on entry.  If
         some tiles that would normally be opened are found to lie outside
         this region, they are closed immediately and not included in the
         count recorded in `kd_tile_bank::num_tiles'.  As a result, the
         function may return with `bank->num_tiles'=0, which is not a failure
         condition, but simply means that there is nothing to decompress right
         now, at least until the application calls `process' again. */
    void close_tile_bank(kd_tile_bank *bank);
      /* Use this function to close all tiles and tile-processing engines
         associated with the tile-bank.  This is normally the current tile
         bank, unless processing is being terminated prematurely. */
    void make_tile_bank_current(kd_tile_bank *bank);
      /* Call this function any time after starting a tile-bank, to make it
         the current tile-bank and appropriately configure the `render_dims'
         and various tile-specific members of the `components' and `channels'
         arrays. */
  private: // Data
    bool precise;
    bool fastest;
    int white_stretch_precision; // Persistent mode setting
    kdu_thread_env *env; // NULL for single-threaded processing
    kdu_thread_queue *env_queue; // Passed to `start'
    kdu_long next_queue_bank_idx; // Used by `start_tile_bank'
    kd_tile_bank *tile_banks; // Array of 2 tile banks
    kd_tile_bank *current_bank; // Points to one of the `tile_banks'
    kd_tile_bank *background_bank; // Non-NULL if the next bank has been started
    kdu_codestream codestream;
    bool codestream_failure; // True if an exception generated in `process'.
    int discard_levels; // Value supplied in last call to `start'
    kdu_dims valid_tiles;
    kdu_coords next_tile_idx; // Index of next tile, not yet opened in any bank
    kdu_sample_allocator aux_allocator; // For palette indices, interp bufs, etc.
    kdu_dims full_render_dims; // Dimensions of whole image on rendering canvas
    kdu_dims render_dims; // Dimensions of current tile bank on rendering canvas
    int max_channels; // So we can tell if `channels' array needs reallocating
    int num_channels;
    int num_colour_channels;
    kd_channel *channels;
    jp2_colour_converter *colour_converter; // For colour conversion ops
    bool pre_convert_colour;  // To understand what these mean, see comments in
    bool post_convert_colour; // `process_generic' where they are used.
    int max_components; // So we can tell if `components' needs re-allocating
    int num_components; // Num valid elements in each of the next two arrays
    kd_component *components;
    int *component_indices; // Used with `codestream.apply_input_restrictions'
    int max_channel_bufs; // Size of `channel_bufs' array
    int num_channel_bufs; // Set from within `process'
    kdu_byte **channel_bufs; // Holds buffers passed to `process'
  };
  /* Implementation related notes:
        The object is capable of working with a single tile at a time, or
     a larger bank of concurrent tiles.  The choice between these options,
     depends on the size of the tiles, compared to the surface being rendered
     and the suggested processing increment supplied in calls to `process'.
     The `kd_tile_bank' structure is used to maintain a single bank of
     simultaneously open (active) tiles.  These must all be horizontally
     adjacent.  It is possible to start one bank of tiles in the background
     while another is being processed, which can be useful when a
     multi-threading environment is available, to minimize the likelihood
     that processor resources become idle.  Most of the rendering operations
     performed outside of decompression itself work on the current tile bank
     as a whole, rather than individual tiles.  For this reason, the dynamic
     state information found in `render_dims' and the various members of the
     `components' and `channels' arrays reflect the dimensions and other
     properties of the current tile bank, rather than just one tile (of
     course a tile bank may consist of only 1 tile, but it may consist of
     an entire row of tiles, or any collection of horizontally adjacent tiles).
        The `render_dims' member identifies the dimensions and location of the
     region associated with the `current_tile_bank', expressed on the
     rendering canvas coordinate system (not the code-stream canvas coordinate
     system). Whenever a new line of channel data is produced, the
     `render_dims.pos.y' field is incremented and `render_dims.size.y' is
     decremented.
        If the 16-bit version of the `process' function is invoked,
     `channel_bufs' actually holds pointers to 16-bit buffers, cast to byte
     pointers.  This allows a single internal implementation of the various
     externally visible `process' functions.
  */

#endif // KDU_REGION_DECOMPRESSOR_H
