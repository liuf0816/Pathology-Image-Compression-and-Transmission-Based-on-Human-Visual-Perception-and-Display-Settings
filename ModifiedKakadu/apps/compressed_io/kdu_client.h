/*****************************************************************************/
// File: kdu_client.h [scope = APPS/COMPRESSED_IO]
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
  Describes a complete caching compressed data source for use in the client
of an interactive client-server application.
******************************************************************************/

#ifndef KDU_CLIENT_H
#define KDU_CLIENT_H

#include "kdu_cache.h"
#include "kdu_client_window.h"

// Defined here:
class kdu_client_translator;
class kdu_client_notifier;
class kdu_client;

// Defined elsewhere:
class kd_client;
class kd_tcp_channel;

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
      When defining these macros in header files, be sure to undefine
   them at the end of the header.
*/
#ifdef KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("E(kdu_client.h)",_id);
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("W(kdu_client.h)",_id);
#  define KDU_TXT(_string) "<#>" // Special replacement pattern
#else // !KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("Error in Kakadu Client:\n");
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("Warning in Kakadu Client:\n");
#  define KDU_TXT(_string) _string
#endif // !KDU_CUSTOM_TEXT

#define KDU_ERROR_DEV(_name,_id) KDU_ERROR(_name,_id)
 // Use the above version for errors which are of interest only to developers
#define KDU_WARNING_DEV(_name,_id) KDU_WARNING(_name,_id)
 // Use the above version for warnings which are of interest only to developers


/*****************************************************************************/
/*                           kdu_client_translator                           */
/*****************************************************************************/

class kdu_client_translator {
  /* [BIND: reference]
     [SYNOPSIS]
       Base class for objects which can be supplied to
       `kdu_client::install_context_translator' for use in translating
       codestream contexts into their constituent codestreams, and
       translating the view window into one which is appropriate for each
       of the codestreams derived from a codestream context.  The interface
       functions provided by this object are designed to match the
       translating functions provided by the `kdu_serve_target' object, from
       which file-format-specific server components are derived.
       [//]
       This class was first defined in Kakadu v4.2 to address the need for
       translating JPX compositing layers and compositing instructions so
       that cache model statements could be delivered correctly to a server
       (for stateless services and for efficient re-use of data cached from
       a previous session).  The translator for JPX files is embodied by the
       `kdu_clientx' object, which is, in some sense, the client version of
       `kdu_servex'.  In the future, however, translators for other file
       formats such as MJ2 and JPM could be implemented on top of this
       interface and then used as-is, without modifying the `kdu_client'
       implementation itself.
  */
  public: // Member functions
    KDU_AUX_EXPORT kdu_client_translator();
    virtual ~kdu_client_translator() { return; }
    virtual void init(kdu_cache *main_cache)
      { close(); aux_cache.attach_to(main_cache); }
      /* [SYNOPSIS]
           This function first calls `close' and then attaches the
           internal auxiliary `kdu_cache' object to the `main_cache' so
           that meta data-bins can be asynchronously read by the
           implementation.  You may need to override this function to
           perform additional initialization steps.
      */
    virtual void close() { aux_cache.close(); }
      /* [SYNOPSIS]
           Destroys all internal resources, detaching from the main
           cache installed by `init'.  This function will be called from
           `kdu_client' when the client-server connection becomes
           disconnected.
      */
    virtual bool update() { return false; }
      /* [SYNOPSIS]
           This function is called from inside `kdu_client' when the
           member functions below are about to be used to translate
           codestream contexts.  It parses as much of the file format
           as possible, returning true if any new information was
           parsed.  If sufficient information has already been parsed
           to answer all relevant requests via the member functions below,
           the function returns false immediately, without doing any work.
      */
    virtual int get_num_context_members(int context_type, int context_idx,
                                        int remapping_ids[])
      { return 0; }
      /* [SYNOPSIS]
           Designed to mirror `kdu_serve_target::get_num_context_members'.
           [//]
           This is one of four functions which work together to translate
           codestream contexts into codestreams, and view windows expressed
           relative to a codestream context into view windows expressed
           relative to the individual codestreams associated with those
           contexts.  A codestream context might be a JPX compositing layer
           or an MJ2 video track, for instance.  The context is generally
           a higher level imagery object which may be represented by one
           or more codestreams.  The context is identified by the
           `context_type' and `context_idx' arguments, but may also be
           modified by the `remapping_ids' array, as explained below.
         [RETURNS]
           The number of codestreams which belong to the context.  If the
           object is unable to translate this context, it should return 0.
           This situation may change in the future if more data becomes
           available in the cache, allowing the context to be translated.
           On the other hand, the context might simply not exist.  The
           return value does not distinguish between these two types of
           failure.
         [ARG: context_type]
           Identifies the type of context.  Currently defined context type
           identifiers (in "kdu_client_window.h") are
           `KDU_JPIP_CONTEXT_JPXL' (for JPX compositing layer contexts) and
           `KDU_JPIP_CONTEXT_MJ2T' (for MJ2 video tracks).  For more
           information on these, see `kdu_sampled_range::context_type'.
         [ARG: context_idx]
           Identifies the specific index of the JPX compositing layer,
           MJ2 video track or other context.  Note that JPX compositing
           layers are numbered from 0, while MJ2 video track numbers start
           from 1.
         [ARG: remapping_ids]
           Array containing two integers, whose values may alter the
           context membership and/or the way in which view windows are to
           be mapped onto each member codestream.  For more information on
           the interpretation of remapping id values, see the comments
           appearing with `kdu_sampled_range::remapping_ids' and
           `kdu_sampled_range::context_type'.
           [//]
           If the supplied `remapping_ids' values cannot be processed as-is,
           they may be modified by this function, but all of the other
           functions which accept a `remapping_ids' argument (i.e.,
           `get_context_codestream', `get_context_components' and
           `perform_context_remapping') must be prepared to operate
           correctly with the modified remapping values.
      */
    virtual int get_context_codestream(int context_type,
                                       int context_idx, int remapping_ids[],
                                       int member_idx)
      { return -1; }
      /* [SYNOPSIS]
           Designed to mirror `kdu_serve_target::get_context_codestream'.
           [//]
           See `get_num_context_members' for a general introduction.  The
           `context_type', `context_idx' and `remapping_ids' arguments have
           the same interpretation as in that function.  The `member_idx'
           argument is used to enumerate the individual codestreams which
           belong to the context.  It runs from 0 to the value returned by
           `get_num_context_members' less 1.
         [RETURNS]
           The index of the codestream associated with the indicated member
           of the codestream context.  If the object is unable to translate
           this context, it should return -1.  As for
           `get_num_context_members', this situation may change in the
           future, if more data is added to the cache.
      */
    virtual const int *get_context_components(int context_type,
                                              int context_idx,
                                              int remapping_ids[],
                                              int member_idx,
                                              int &num_components)
      { return NULL; }
      /* [SYNOPSIS]
           Designed to mirror `kdu_serve_target::get_context_components'.
           [//]
           See `get_num_context_members' for a general introduction.  The
           `context_type', `context_idx', `remapping_ids' and `member_idx'
           arguments have the same interpretation as in
           `get_context_codestream', except that this function returns a
           list of all image components from the codestream which are used
           by this context.
         [RETURNS]
           An array with one entry for each image component used by the
           indicated codestream context, from the specific codestream
           associated with the indicated context member.  This is the
           same codestream whose index is returned `get_context_codestream'.
           Each element in the array holds the index (starting from 0) of
           the image component.  If the object is unable to translate
           the indicated context, it should return NULL.  As for
           the other functions, however, a NULL return value may change in
           the future, if more data is added to the cache.
         [ARG: num_components]
           Used to return the number of elements in the returned array.
      */
    virtual bool perform_context_remapping(int context_type,
                                           int context_idx,
                                           int remapping_ids[],
                                           int member_idx,
                                           kdu_coords &resolution,
                                           kdu_dims &region)
      { return false; }
      /* [SYNOPSIS]
           Designed to mirror `kdu_serve_target::perform_context_remapping'.
           [//]
           See `get_num_context_members' for a general introduction.  The
           `context_type', `context_idx', `remapping_ids' and `member_idx'
           arguments have the same interpretation as in
           `get_context_codestream', except that this function serves to
           translate view window coordinates.
         [RETURNS]
           False if translation of the view window coordinates is not
           possible for any reason.  As for the other functions, a
           false return value may become true in the future, if more
           data is added to the cache.
         [ARG: resolution]
           On entry, this argument represents the full size of the image, as
           it is to be rendered within the indicated codestream context.  Upon
           return, the values of this argument will be converted to represent
           the full size of the codestream, required to render the context at
           the full size indicated on entry.
         [ARG: region]
           On entry, this argument represents the location and dimensions of
           the view window, expressed relative to the full size image
           at the resolution given by `resolution'.  Upon return, the
           values of this argument will be converted to represent the
           location and dimensions of the view window within the codestream.
      */
    protected: // Data members, shared by all file-format implementations
      kdu_cache aux_cache; // Attached to the main client cache; allows
                           // asynchronous reading of meta data-bins.
  };

/*****************************************************************************/
/*                            kdu_client_notifier                            */
/*****************************************************************************/

class kdu_client_notifier {
  /* [BIND: reference]
     [SYNOPSIS]
       Base class, derived by an application and delivered to
       `kdu_client' for the purpose of receiving notification messages when
       information is transferred from the server.
  */
  public:
    kdu_client_notifier() { return; }
    virtual ~kdu_client_notifier() { return; }
    virtual void notify() { return; }
    /* [BIND: callback]
       [SYNOPSIS]
         Called whenever an event occurs, such as a change in the status
         message (see `kdu_client::get_status') or a change in the contents
         of the cache associated with the `kdu_client' object.
    */
  };

/*****************************************************************************/
/*                                kdu_client                                 */
/*****************************************************************************/

class kdu_client : public kdu_cache {
  /* [BIND: reference]
     [SYNOPSIS]
     Implements a full JPIP network client, building on the services offered
     by `kdu_cache'.  The following is a brief summary of the way in which
     this object is expected to be used:
     [>>] The client application uses the `connect' function to start a
          new thread of execution which will communicate with the remote
          server.  The cache may be used immediately, if desired, although
          its contents will remain empty until the server connection has
          been established.
     [>>] The client application determines whether or not the remote
          object is a JP2-family file, by invoking
          `kdu_cache::get_databin_length' on meta data-bin 0, using its
          `is_complete' member to determine whether a returned length
          of 0 means that meta data-bin 0 is empty, or simply that nothing
          is yet known about the data-bin, based on communication with the
          server so far (the server might not even have been connected yet).
     [>>] If the `kdu_cache::get_databin_length' function returns a length of
          0 for meta data-bin 0, and the data-bin is complete, the image
          corresponds to a single raw JPEG2000 code-stream and the client
          application must wait until the main header data-bin is complete,
          calling `kdu_cache::get_databin_length' again (this time querying
          the state of the code-stream main header data-bin) to determine
          when this has occurred.  At that time, the present object may be
          passed directly to `kdu_codestream::create'.
     [>>] If the `kdu_cache::get_databin_length' function identifies meta
          data-bin 0 as non-empty, the image source is a JP2-family file.  The
          client application uses the present object to open a `jp2_family_src'
          object, which is then used to open a `jp2_source' object (or
          any other suitable file format parser derived from `jp2_input_box').
          The application then calls `jp2_source::read_header' until it returns
          true, meaning that the cache has sufficient information to read
          both the JP2 file header and the main header of the embedded
          JPEG2000 code-stream.  At that point, the `jp2_source' object is
          passed to `kdu_codestream::create'.
     [>>] At any point, the client application may use the `disconnect'
          function to disconnect from the server, while continuing to use
          the cache, or the `close' function to both disconnect from the
          server (if still connected) and discard the contents of the cache.
     [//]
     The `kdu_client' object also provides a number of status functions for
     monitoring the state of the network connection.  These are
     [>>] `is_active' -- returns true from the point at which `connect'
          is called, remaining true until `close' is called, even if the
          connection with the server has been dropped, or was never completed.
     [>>] `is_alive' -- returns true from the point at which `connect' is
          called, remaining true until the network management thread
          terminates.  This happens if `close' is called, if the network
          connection is explicitly terminated by a call to `disconnect', if
          the server terminates the connection, or if the original connection
          attempt failed to complete.
     [>>] `is_idle' -- returns true if the server has finished responding
          to all pending requests, but the connection is still alive.
  */
  public: // Member functions
    KDU_AUX_EXPORT kdu_client();
    KDU_AUX_EXPORT virtual ~kdu_client();
      /* [SYNOPSIS]
           Invokes `close' before destroying the object.
      */
    void install_context_translator(kdu_client_translator *translator)
      {
        if (this->translator == translator)
          return;
        if ((this->translator != NULL) && active_state)
          { KDU_ERROR_DEV(e,0); e <<
              KDU_TXT("You may not install a new client context "
              "translator, over the top of an existing one, while the "
              "`kdu_client' object is active (from `connect' to `close').");
          }
        if (this->translator != NULL)
          this->translator->close();
        if (translator != NULL)
          translator->init(this);
        this->translator = translator;
      }
      /* [SYNOPSIS]
           You may call this function at any time, to install a translator
           for context requests.  Translators are required only if you
           wish to issue requests which involve a codestream context
           (see `kdu_window::contexts'), and the client-server communications
           are stateless, or information from a previous cached session is
           to be reused.  In these cases, the context translator serves to
           help the client determine what codestreams are being (implicitly)
           requested, and what each of their effective request windows are.
           This information, in turn, allows the client to inform the server
           of relevant data-bins which it already has in its cache, either
           fully or in part.  If you do not install a translator,
           communications involving codestream contexts may become
           unnecessarily inefficient under the circumstances described above.
           [//]
           Conceivably, a different translator might be created to handle
           each different type of image file format which can be accessed
           using a JPIP client.  For the moment, a single translator serves
           the needs of all JP2/JPX files, while a separate translator may
           be required for MJ2 (motion JPEG2000) files, and another one might
           be required for JPM (compound document) files.
           [//]
           You can install the translator prior to or after the call to
           `connect'.  If you have several possible translators available,
           you might want to wait until after `connect' has been called
           and sufficient information has been received to determine the
           file format, before installing a translator.
           [//]
           You may replace an existing translator, which has already been,
           installed, but you may only do this when the client is not
           active, as identified by the `is_active' function.  To
           remove an existing translator, without replacing it, supply
           NULL for the `translator' argument.
      */
    KDU_AUX_EXPORT virtual void
      connect(const char *server, const char *proxy,
              const char *request, const char *channel_transport,
              const char *cache_dir);
      /* [SYNOPSIS]
           Creates a new thread of execution to manage network communications
           and initiate a connection with the appropriate server.  The
           function returns immediately, without blocking on the success of
           the connection.
           [//]
           The application may then monitor the value returned by
           `is_active' and `is_alive' to determine when the connection is
           actually established, if at all.  The application may also monitor
           transfer statistics using `kdu_cache::get_transferred_bytes' and it
           may receive notification of network activities by supplying a
           `kdu_client_notifier'-derived object to the `install_notifier'
           function.
           [//]
           The network management thread will be terminated when the
           `close' or `disconnect' function is called, when the server
           disconnects, or when a terminal error condition arises, whichever
           occurs first.
           [//]
           It is important to realize that the internal network management
           thread may itself issue an error message through `kdu_error'.
           For this to be robust, the `kdu_message'-derived object supplied to
           `kdu_customize_errors' must throw an exception when it receives
           the call to `kdu_message::flush' with `end_of_message' true.
           The implementation of the `kdu_message'-derived object
           should be thread-safe and should be able to successfully deliver
           its messages, regardless of the thread from which it is invoked.
           These conditions should not be onerous on application developers.
           [//]
           The present function may itself issue a terminal error message
           through `kdu_error' if there is something wrong with any of the
           supplied arguments.  For this reason, interactive applications
           should generally provide try/catch protection when calling
           this function.
         [ARG: server]
           Holds the host name or IP address of the server to be contacted.
           The `server' string may contain an optional port number suffix,
           in which case it has the form <host name or IP address>:<port>.
           The default HTTP port number of 80 is otherwise assumed.
           Communication with this machine proceeds over HTTP, and may involve
           intermediate proxies, as described in connection with the `proxy'
           argument.
         [ARG: proxy]
           Same syntax as `server', but gives the host (and optionally the
           port number) of the machine with which the initial TCP connection
           should be established.  This may either be the server itself, or
           an HTTP proxy server.  May be NULL, or point to an empty string,
           if no proxy is to be used.
         [ARG: request]
           This pointer may not be NULL or reference an empty string.  The
           string has one of the following forms: a) <resource>; or
           b) <resource>?<query>.  In the former case, <resource> is simply
           the name of the target file on the server.  In the latter case,
           the target file name may be held in the <resource> component, or
           as a query field of the form "target=<name>", in which case the
           <resource> string may refer to a CGI script or some other
           identifier having meaning to the server.  In any event, the
           complete `request' string may consist only of URI-legal characters
           and, in particular, may contain no white space characters.  If
           necessary, non-conforming characters should be hex-hex encoded,
           as described in RFC2396 (Uniform Resource Identifiers (URI):
           Generic Syntax).
           [//]
           The <query> string may contain multiple fields, each of the form
           <name>=<value>, separated by the usual "&" character.  If a <query>
           string contains anything other than the "target" or "subtarget"
           fields, it is interpreted as a one-time request.
           In this case, the request will be issued as-is to the server, and
           once the server's response is fully received, the connection will
           be closed.  This type of request might be embedded in HTML
           documents as a simple URL.  In this mode, any calls to
           `post_window' will be ignored, since the application is not
           permitted to interactively control the communications.  Also, in
           this mode, the `channel_transport' argument is ignored and a
           session-less transaction is used.
         [ARG: channel_transport]
           If NULL or a pointer to an empty string or the string "none", no
           attempt will be made to establish a JPIP channel.  In this case,
           no attempt will be made to establish a stateful communication
           session; each request is delivered to the server (possibly through
           the proxy) in a self-contained manner, with all relevant cache
           contents identified using appropriate JPIP-defined header lines.
           When used interactively, (nothing other than "target" or "subtarget"
           query fields in the `request' string), this mode may involve
           somewhat lengthy requests, and may cause the server to go to
           quite a bit of additional work, re-creating the context for each
           and every request.
           [//]
           If `channel_transport' holds the string "http" and the
           `request' string does not contain any query request fields other
           than "target" or "subtarget", the client's first request asks
           for a channel with HTTP as the transport.  If the server refuses
           to grant a channel, communication will continue as if the
           `channel_transport' argument had been NULL.
           [//]
           If `channel_transport' holds the string "http-tcp", the behaviour
           is the same as if it were "http", except that a second TCP
           channel is established for the return data.  This transport
           variant is somewhat more efficient for both client and server,
           but requires an additional TCP channel and cannot be used from
           within organizations which mandate that all external communication
           proceed through HTTP proxies.  If the server does not support
           the "http-tcp" transport, it may fall back to an HTTP transported
           channel.  This is because the client's request to the server
           includes a request field of the form "cnew=http-tcp,http", giving
           the server both options.
           [//]
           It is worth noting that wherever a channel is requested, the
           server may respond with the address of a different host to be
           used in all future requests.
         [ARG: cache_dir]
           If non-NULL, this argument provides the path name for a directory
           in which cached data may be saved at the end of an interactive
           communication session.  The directory is also searched at the
           beginning of an interactive session, to see if information is
           already available for the image in question.  If the argument
           is NULL, or points to an empty string, the cache contents will
           not be saved and no previously cached information may be re-used
           here.  Files written to or read from the cache directory have the
           ".kjc" suffix, which stands for (Kakadu JPIP Cache).  These
           files commence with some details which may be used to re-establish
           connection with the server, if necessary (not currently implemented)
           followed by the cached data itself, stored as a concatenated list
           of data-bins.  The format of these files is private to the current
           implementation and subject to change in subsequent releases of
           the Kakadu software, although the files are at least guaranteed to
           have an initial header which can be used for version-validation
           purposes.
      */
    bool is_active() { return active_state; }
      /* [SYNOPSIS]
           Returns true from the point at which `connect' is called until the
           `close' function is called.  This means that the function may
           return true before a network connection with the server has been
           established, and it may return true after the network connection
           has been closed.
      */
    bool is_one_time_request() { return one_time_request; }
      /* [SYNOPSIS]
           Returns true if the `request' string passed to `connect'
           represented a one-time request, as defined there.  In
           particular, a one-time request is one which contains a query
           string with anything other than a target or sub-target
           request field.  In the event that a one-time request was found
           in the request string passed to `connect', this function continues
           to return true until `close' is called, i.e., for the same
           interval over which `is_active' returns true.
      */
    KDU_AUX_EXPORT bool is_alive();
      /* [SYNOPSIS]
           Returns true so long as the network management thread is alive.
           This condition is established by a call to `connect' and terminated
           by calls to one of `close' or `disconnect' or by the server itself
           terminating the connection.
           [//]
           This enables you to check whether or not a connection has been
           dropped (or the original connection attempt failed).  In either
           case, `is_active' returns true and `is_alive' returns false.
      */
    bool is_idle() { return (idle_state && (state != NULL)); }
      /* [SYNOPSIS]
           Returns true if the server has finished responding to all
           pending window requests, and the network management thread is
           still alive.  The state of this function reverts to false
           immediately after any call to `post_window' which will cause a
           request to be sent to the server.  Any request for a window
           which is known to have been completely serviced already, however,
           will not result in a message being sent to the server, so in that
           case `is_idle' will remain true if it was previously true.
      */
    KDU_AUX_EXPORT virtual void disconnect(bool keep_transport_open=false,
                                           int timeout_milliseconds=2000);
      /* [SYNOPSIS]
           Severs the connection with the server, but leaves the object open
           for reading data from the cache.  This function may safely be called
           at any time.  If the `close' function has not been called since
           the last call to `connect', `is_active' remains true, while
           `is_alive' goes false.
         [ARG: keep_transport_open]
           If true, an attempt will be made to keep the underlying transport
           channel (typically one or more TCP channels) alive, so that
           a subsequent call to `connect' can try to re-use it, rather than
           instantiating a new transport channel from scratch.
           [//]
           Whether or not this is successful depends on a number of factors,
           but the most critical step is waiting for the connection to become
           idle (i.e., `is_idle' should return true) before calling this
           function.  You can encourage the connection to become idle as
           soon as possible by issuing a `post_window' call which specifies
           an empty window.  If the connection is not idle when the present
           function is invoked, it is most likely that the transport channel
           will need to be destroyed.
           [//]
           For HTTP connections, if the connection is idle when this function
           is called, the "Connection: close" header will not be issued by
           the client, and the TCP transport channel will be saved for
           later re-use, so long as the server does not issue with a
           "Connection: close" header with its response.
           [//]
           If you have kept the transport channel open and later wish to
           close it, you have only to call this function again with
           `keep_transport_open'=false.  There is no need for an intervening
           call to `connect' or `close'.
         [ARG: timeout_milliseconds]
           Ignored unless `keep_transport_open' is true.  In that case, this
           argument identifies the maximum amount of time for which the
           function might be blocked while the internal disconnection
           mechanism waits to ensure that the server has released the
           transport channel from its use in the current session.  This
           should not be long, because it will not be waiting unless
           the connection was already idle.  Nevertheless, if the server
           does not released the transport within that time, or if an
           earlier determination can be made that it will not do so,
           the function closes the existing transport channel anyway, so
           that it cannot be re-used.
      */
    KDU_AUX_EXPORT virtual bool close();
      /* [SYNOPSIS]
           Same as `disconnect', but also discards the entire contents of the
           cache, causing future calls to `is_alive' and `is_active' both to
           return false.  It is safe to call this function at any time,
           regardless of whether or not it has been successfully opened.
           If `disconnect' has already been called, and the underlying
           transport channel has been saved, the present function will not
           destroy it -- that will happen only if a subsequent call to
           `connect' cannot use it, if `disconnect' is called explicitly with
           `keep_transport_open'=false, or if the object is destroyed.
      */
    void install_notifier(kdu_client_notifier *notifier)
      { acquire_lock(); this->notifier = notifier; release_lock(); }
      /* [SYNOPSIS]
           Provides a generic mechanism for the network management thread
           to notify the application when the connection state changes or
           when new data is placed in the cache.  Specifically, the application
           provides an appropriate object, derived from the abstract base
           class, `kdu_client_notifier'.  The object's virtual `notify'
           function is called from within the network management thread
           whenever any of the following conditions occur:
           [>>] Connection with the server is completed;
           [>>] The server has acknowledged a request for a new window into
                the image, possibly modifying the requested window to suit
                its needs (call `get_window_in_progress' to learn about any
                changes to the window which is actually being served);
           [>>] One or more new messages from the server have been received and
                used to augment the cache; or
           [>>] The server has disconnected -- in this case, `notifier->notify'
                is called immediately before the network management thread is
                terminated, but `is_alive' is guaranteed to be false from
                immediately before the point at which the notification was
                raised, so as to avoid possible race conditions in the
                client application.
           [//]
           A typical notifier would post a message on an interactive client's
           message queue so as to wake the application up if necessary.  A more
           intelligent notifier may choose to wake the application up in this
           way only if a substantial change has occurred in the cache --
           something which may be determined with the aid of the
           `kdu_cache::get_transferred_bytes' function.
           [//]
           The following functions should never be called from within the
           `notifier->notify' call, since that call is generated from within
           the network management thread: `connect', `close', `disconnect' and
           `post_window'.
           [//]
           We note that the notifier remains installed after its `notify'
           function has been called, so there is no need to re-install the
           notifier.  If you wish to remove the notifier, the
           `install_notifier' function may be called with a NULL argument.
      */
    virtual const char *get_status() { return status; }
      /* [SYNOPSIS]
           Returns a pointer to a null-terminated character string which
           best describes the current state of the network connection.  Even
           if the object has never been opened, or `connect' has never been
           called, this function always returns a pointer to some valid
           string -- e.g., "ready".
      */
    KDU_AUX_EXPORT virtual bool post_window(kdu_window *window);
      /* [SYNOPSIS]
           Requests that a message be delivered to the server identifying
           a change in the user's window of interest into the compressed
           image.  The message may not be delivered immediately; it may not
           even be delivered at all, if the function is called again
           specifying a different access window, before the function has a
           chance to deliver a request for the current window to the server.
           [//]
           It is important to note that the server may not be able to
           serve some windows in the exact form they are requested.  When
           this happens, the server's response will indicate modified
           attributes of the window which it is able to service.  The
           client may learn about the actual window for which data is being
           served by calling `get_window_in_progress'.
           [//]
           This function may be called at any time after `connect', without
           necessarily waiting for `open' to succeed.  In fact, this can
           be useful, since it allows an initial window to be
           requested while the initial transfer of mandatory headers is
           in progress, or even before it starts, thereby avoiding the
           latency associated with extra round-trip-times.
         [RETURNS]
           False if the specified window is known to have been fully served
           already, so that it will not result in any request being delivered
           to the server.  If the function returns true, the window is
           held until a suitable request message can be sent to the server.
           This message will eventually be sent, unless a new call to
           `post_window' arrives in the mean time.
      */
    KDU_AUX_EXPORT virtual bool get_window_in_progress(kdu_window *window);
      /* [SYNOPSIS]
           This function may be used to learn about the window
           which the server is currently servicing.
           [//]
           If the server is idle, meaning that it has finished serving all
           outstanding requests, an appropriate status message will
           appear, but the present function will continue to identify the most
           recently serviced window as the one which is in progress.
           [//]
           If there is no active connection with the server (`is_alive'
           returns false), the present function returns false, with
           `window->num_components' equal to 0.
         [RETURNS]
           True if the window which the server is currently serving or about
           to start serving corresponds to the window which was most recently
           requested via a call to `post_window' which returned  true.
           Otherwise, the function returns false, meaning that the server
           has not yet finished serving a previous window request, or no
           request message has yet been sent to the server.
         [ARG: window]
           This argument may be NULL, in which case only the function's
           return value has any meaning.  Otherwise, the function modifies
           the various members of this object to indicate the current window
           which is being serviced by the server.
           [//]
           Note that the `window->resolution' member will be set to reflect
           the dimensions of the image resolution which is currently being
           served.  These can, and probably should, be used in posting new
           window requests at the same image resolution.
           [//]
           If the server has not yet responded to any window
           request, the `window' object will be set to its initialized
           state (i.e., the function automatically invokes `window->init'). */
    int get_received_bytes() { return received_bytes; }
      /* [SYNOPSIS]
         Returns the total number of bytes which have been received from
         the server, including all headers and other overhead information.
         This function differs from `kdu_cache::get_transferred_bytes'
         in two respects: firstly, it returns only the amount of data
         transferred by the server, not including additional bytes which
         may have been retrieved from a local cache; secondly, it includes
         all the transfer overhead. */
  protected: // Virtual synchronization functions implemented for "kdu_cache"
    void acquire_lock(); // Implements `kdu_cache::acquire_lock'
    void release_lock(); // Implements `kdu_cache::release_lock'
  private: // Data
    friend class kd_client;
    bool active_state; // True from `connect' until `close'.
    bool one_time_request; // True from `connect' to `close' if appropriate.
    bool idle_state; // True if all pending requests have been satisfied.
    const char *status;
    kdu_client_notifier *notifier;
    kdu_client_translator *translator;
    void *h_mutex; // Used to guard access to the cache.
    kd_client *state; // Implements the network management machinery.
    int received_bytes; // Total bytes received from server
  private: // TCP channel preservation
    bool keep_transport_open; // Sends a message to `kd_client::~kd_client'
    int timeout_milliseconds; // Part of above message
    kd_tcp_channel *saved_request_channel; // Used to keep the TCP channel
       // used in a previous JPIP session open for communication in future
       // sessions if `disconnect' was called with `keep_transport_open'=true.
  };

#undef KDU_ERROR
#undef KDU_ERROR_DEV
#undef KDU_WARNING
#undef KDU_WARNING_DEV
#undef KDU_TXT

#endif // KDU_CLIENT_H
