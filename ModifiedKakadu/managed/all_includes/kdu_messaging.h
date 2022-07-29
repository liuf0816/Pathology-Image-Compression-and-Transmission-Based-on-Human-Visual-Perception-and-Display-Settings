/*****************************************************************************/
// File: kdu_messaging.h [scope = CORESYS/COMMON]
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
   Defines formatted error and warning message services, which alleviate
other parts of the implementation from concerns regarding formatting of text
messages, appropriate termination methods (e.g. process exit() or exception
throwing) in the event of a fatal error, graphical user interface
considerations and so forth.  These reduce or eliminate the effort required
to port the system to different application environments.
******************************************************************************/

#ifndef KDU_MESSAGING_H
#define KDU_MESSAGING_H

#include <stdlib.h>
#include <stdio.h> // For access to `sprintf'.
#include <assert.h>
#include "kdu_elementary.h"

// Defined here:
  class kdu_message;
  class kdu_thread_safe_message;
  class kdu_message_formatter;
  class kdu_error;
  class kdu_warning;


/* ========================================================================= */
/*                       Messaging Setup Functions                           */
/* ========================================================================= */

extern KDU_EXPORT void
  kdu_customize_warnings(kdu_message *handler);
  /* [SYNOPSIS]
       This function can and usually should be used to customize the behaviour
       of warning messages produced using the `kdu_warning' object.  By
       default, warning messages will simply be discarded.
     [ARG: handler]
       All text delivered to a `kdu_warning' object will be passed to
       `handler->put_text' (unless `handler' is NULL) and all calls to
       `kdu_warning::flush' will be generate calls to `handler->flush',
       with the `end_of_message' argument set to false (regardless of the
       value of this argument in the call to `kdu_warning::flush').  When
       any `kdu_warning' object is destroyed, `handler->flush' will be called
       with an `end_of_message' argument equal to true.
  */
extern KDU_EXPORT void
  kdu_customize_errors(kdu_message *handler);
  /* [SYNOPSIS]
       This function can and usually should be used to customize the behaviour
       of error messages produced using the `kdu_error' object.  By
       default, error message text will be discarded, and the process will
       be terminated through `exit' whenever a `kdu_error' object is destroyed.
     [ARG: handler]
       All text delivered to a `kdu_error' object will be passed to
       `handler->put_text' (unless `handler' is NULL) and all calls to
       `kdu_error::flush' will be generate calls to `handler->flush',
       with the `end_of_message' argument set to false (regardless of the
       value of this argument in the call to `kdu_error::flush').  If
       a `kdu_error' object is destroyed, `handler->flush' will be called
       with an `end_of_message' argument equal to true and the process will
       subsequently be terminated through `exit'.  The termination may be
       avoided, however, by throwing an exception from within the message
       terminating `handler->flush' call.
       [//]
       For consistency, and especially to ensure correct behaviour of
       Kakadu's multi-threaded processing machinery, you should only throw
       integer-valued exceptions.
  */
extern KDU_EXPORT void
  kdu_customize_text(const char *context, kdu_uint32 id,
                     const char *lead_in, const char *text);
  /* [SYNOPSIS]
       This function is used to register text (usually for internalization
       purposes) to be rendered by `kdu_error' or `kdu_warning' when
       constructed with the `context'/`id' pair supplied here.
       [//]
       Note that there is a second (overloaded) form of this function which
       you can use to register unicode text.  If you invoke either of
       these functions multiple times with the same `context' and `id'
       values, the effects of all but the last invocation will be lost.
     [ARG: context]
       String which is matched against a corresponding string which may
       be supplied in the constructor for `kdu_error' or `kdu_warning'.
       Note carefully that the string must be a constant resource.  Only
       its address will be registered; the string itself will not be copied.
     [ARG: id]
       Integer identifier which is matched against the value supplied in
       the constructor for `kdu_error' or `kdu_warning'.
     [ARG: lead_in]
       Holds the lead-in string to be printed first.  All other text is
       printed only in response to interface functions invoked on the
       `kdu_error' or `kdu_warning' object, as appropriate.
     [ARG: text]
       Holds one or more text strings which are to replace instances of
       the special pattern, "<#>", when it is supplied to `kdu_error::put_text'
       or `kdu_warning::put_text', as appropriate.  Most applications of
       `kdu_error' or `kdu_warning' involve only a single string, which
       means that a single instance of the pattern "<#>" will be translated.
       However, multiple translations may be required.  In general, if there
       are N instances of the pattern, `text' is assumed to point to
       N null-terminated strings concatenated together.  If any of these
       strings is found to be empty, no further parsing into the `text'
       array will occur, meaning that no further instances of the "<#>"
       pattern will be replaced.  This means that you can avoid the
       risk of accidentally reading past the end of a `text' array, by
       terminating the last string in the array by two null ('\0') characters.
       However, there is no requirement for you to do this.
  */
extern KDU_EXPORT void
  kdu_customize_text(const char *context, kdu_uint32 id,
                     const kdu_uint16 *lead_in, const kdu_uint16 *text);
  /* [SYNOPSIS]
       Same as the first form of the overloaded `kdu_customize_text'
       function, except that this version registers unicode `lead_in'
       and `text' strings.  Strings must be terminated with the 16-bit
       unicode null character (0x0000).  Where multiple strings are
       embedded in the `text' array, they must each be separated by the
       unicode null character.  Reading into the `text' array will be
       safely terminated once two consecutive unicode null characters have
       been encountered.
  */


/* ========================================================================= */
/*                           Messaging Classes                               */
/* ========================================================================= */

/*****************************************************************************/
/*                               kdu_message                                 */
/*****************************************************************************/

class kdu_message {
  /* [BIND: reference]
     [SYNOPSIS]
       Objects of this class form a generic platform on which text messaging
       services are built in Kakadu.  These services allow the Kakadu core
       system to avoid any dependence on C++ I/O streams, which
       are not always available on some platforms (e.g., WinCE).
       For meaningful messaging, this object must be derived.  The
       architecture is carefully devised to allow advanced messaging
       extensions to be implemented in other language bindings (e.g., Java),
       and to then be available for rendering all Kakadu core system
       messages, along with other application-specific messages.
       [//]
       If you need to ensure that a single message service will behave
       correctly when used from multiple threads, consider the
       `kdu_thread_safe_message' object as a base for your derived
       message classes.
  */
  public: // Member functions
    kdu_message() { mode_hex = false; }
    virtual ~kdu_message() { return; };
    virtual void put_text(const char *string) { return; }
      /* [BIND: callback]
         [SYNOPSIS]
           This function should be overridden in any meaningful messaging
           class, providing an implementation of the text output function.
           All text is delivered through this function.
           [//]
           For foreign language bindings, e.g., Java, this function is regarded
           as a callback.  When the native function is called, it will
           automatically invoke the corresponding Java (or other foreign
           language) function, supplying the text in the most natural form.
        */
    virtual void put_text(const kdu_uint16 *string) { return; }
      /* [SYNOPSIS]
           This form of the `put_text' function is called if the text to
           be printed is a unicode string.  To support unicode, you have
           only to override this function with a meaningful implementation.
           [//]
           In this case, you need to be prepared to accept a mixture of
           calls to the ASCII and Unicode versions of `put_text'.
      */
    virtual void flush(bool end_of_message=false) { return; }
      /* [BIND: callback]
         [SYNOPSIS]
           This function should be overridden by any derived messaging
           class, whose implementation requires `flush' events to be captured.
         [ARG: end_of_message]
           Has special meaning to the messaging objects passed in to
           `kdu_customize_errors' and `kdu_customize_warnings', as explained
           in connection with those functions.  As a general rule,
           applications should provide a value of false for this argument
           if they need to flush text which may be temporarily buffered in
           a `kdu_message'-derived object.
      */
    virtual void start_message() { return; }
      /* [BIND: callback]
         [SYNOPSIS]
           This function is invoked on the `kdu_message'-derived objects
           passed in to `kdu_customize_errors' and `kdu_customize_warnings',
           whenever a `kdu_error' or `kdu_warning' object (as appropriate)
           is constructed.  In many cases, there may be no need to override
           this function when deriving new classes from `kdu_message'.
           An important exception, however, arises when a single messaging
           object is to be shared by multiple threads of execution.  To
           avoid message mangling or nasty race conditions, multi-threaded
           applications should lock a mutex resource when `start_message'
           is called, releasing it when `flush' is called with `end_of_message'
           true.
      */
    bool set_hex_mode(bool new_mode)
      { bool old_mode = mode_hex; mode_hex = new_mode; return old_mode; }
      /* [SYNOPSIS]
           Changes the behaviour of integer output operators.  A false
           argument means that integer values will be displayed as decimal
           quantities, while a true argument means that  they will be displayed
           as hexadecimal quantities.  The function returns the previous
           value of the hexadecimal mode, so that it can be restored by the
           caller.
      */
    kdu_message &operator<<(const char *string)
      { put_text(string); return *this; }
    kdu_message &operator<<(char ch)
      { char text[2]; text[0]=ch; text[1]='\0'; put_text(text); return *this; }
    kdu_message &operator<<(int val)
      { char text[80];
        sprintf(text,(mode_hex)?"%x":"%d",val);
        put_text(text); return *this; }
    kdu_message &operator<<(unsigned int val)
      { char text[80];
        sprintf(text,(mode_hex)?"%x":"%u",val);
        put_text(text); return *this; }
    kdu_message &operator<<(long val)
      { return (*this)<<(int) val; }
    kdu_message &operator<<(unsigned long val)
      { return (*this)<<(unsigned int) val; }
    kdu_message &operator<<(short int val)
      { return (*this)<<(int) val; }
    kdu_message &operator<<(unsigned short int val)
      { return (*this)<<(unsigned int) val; }
#ifdef KDU_LONG64
    kdu_message &operator<<(kdu_long val)
      { // Conveniently prints in decimal with thousands separated by commas
        if (val < 0)
          { (*this)<<'-'; val = -val; }
        kdu_long base = 1;
        while ((1000*base) <= val) base *= 1000;
        int digits = (int)(val/base);   (*this) << digits;
        while (base > 1)
          { val -= base*digits;  base /= 1000;  digits = (int)(val/base);
            char text[4]; sprintf(text,"%03d",digits); (*this)<<','<<text; }
        return (*this);
      }
#endif
    kdu_message &operator<<(float val)
      { char text[80]; sprintf(text,"%f",val); put_text(text); return *this; }
    kdu_message &operator<<(double val)
      { char text[80]; sprintf(text,"%f",val); put_text(text); return *this; }
  private: // Data
    bool mode_hex;
  };

/*****************************************************************************/
/*                         kdu_thread_safe_message                           */
/*****************************************************************************/

class kdu_thread_safe_message : public kdu_message {
  /* [BIND: reference]
     [SYNOPSIS]
       Provides thread-safe handling of messages, for applications in
       which multiple threads may need to write messages to the same
       object.  This is done by locking a mutex inside the
       `start_message' member function and unlocking it again in the
       `flush' function (when called with `end_of_message'=true).  Between
       these two calls, you may be sure that only one thread is
       writing text to the object.
       [//]
       If your platform does not support multi-threading, this object
       will have identical behaviour to `kdu_message'.
  */
  public: // Member functions
    kdu_thread_safe_message()
      { is_locked=false; mutex.create(); }
    virtual ~kdu_thread_safe_message()
      { mutex.destroy(); }
    virtual void flush(bool end_of_message=false)
      {
        if (end_of_message && is_locked)
          { is_locked = false; mutex.unlock(); }
      }
      /* [SYNOPSIS]
           Unlocks the mutex locked by `start_message', so long as
           `end_of_message' is true and the mutex was locked by a
           previous call to `start_message'.
      */
    virtual void start_message()
      { mutex.lock(); is_locked = true; }
      /* [SYNOPSIS]
           Locks a mutex so that no other thread can complete a call to
           `start_message' until the present thread calls `flush' with
           an `end_of_message' argument equal to true.
      */
  private: // Data
    bool is_locked;
    kdu_mutex mutex;
  };
  
/*****************************************************************************/
/*                            kdu_message_formatter                          */
/*****************************************************************************/

class kdu_message_formatter : public kdu_message {
  /* [BIND: reference]
     [SYNOPSIS]
     Derived `kdu_message' object which formats the text it receives
     into lines of a given maximum length (as supplied to the constructor)
     and passes the lines to a separate `kdu_message'-derived object.  Every
     attempt is made to avoid breaking words (space-delimited tokens) across
     lines.  Tab characters are converted to a single space, unless they
     appear immediately after the last new-line (or immediately after the
     object is created).  In the latter case, each tab increments indentation
     for the paragraph (currently by 4 spaces).  Indentation state is
     cancelled at the next new-line character.
  */
  public: // Member functions
    kdu_message_formatter(kdu_message *output, int max_line=79)
      {
      /* [SYNOPSIS]
           Identifies the `kdu_message'-derived object which is to receive
           the formatted text, and the maximum line length to be used during
           formatting.
         [ARG: output]
           If NULL, all text will be discarded.  If you supply a
           `kdu_thread_safe_message' object (or derived object) here,
           the entire combination will be thread safe, in the sense that
           message text handling is protected against interference by
           other threads, so long as the `start_message' function is used
           to start writing a message and the `flush' function with
           `end_of_message'=true is used to finish writing a message.
         [ARG: max_line]
           Max characters in a formatted line of text.  The bound does not
           include null terminators or terminal new-line characters, but it
           does include any prevailing indentation, established by the use
           of tab characters or through calls to `set_master_indent'.  Note
           that the actual maximum line length may be smaller than the
           bound supplied here, depending on the way in which internal
           resources are allocated.
      */
        if (max_line > 200) max_line = 200;
        dest = output; line_chars = max_line; num_chars = 0;
        max_indent = 40; indent = 0; master_indent = 0;
        no_output_since_newline = true;
      }
    ~kdu_message_formatter() { if (dest != NULL) dest->flush(); }
      /* [SYNOPSIS]
           Flushes any outstanding text to the output object.
      */
    KDU_EXPORT void set_master_indent(int val);
      /* [SYNOPSIS]
           This function alters the amount by which every line will be
           indented from this point forward, in addition to any
           paragraph-specific indents inserted by sending tab (`\t')
           characters to the `put_text' function.
           [//]
           You should try to avoid calling this function except at the
           beginning of a paragraph (i.e., immediately after a new-line
           character, a call to `flush', or object construction).  If
           you call the function in the middle of a paragraph, the `flush'
           function will be invoked first.
         [ARG: val]
           Minimal indent (number of spaces) at the beginning of each
           formatted line of text.  Additional indentation may be
           introduced by tab characters in text supplied to the base
           `streambuf' object. */
    KDU_EXPORT void put_text(const char *string);
      /* [SYNOPSIS]
           Implements the `kdu_message::put_text' function, collecting
           text as it appears and formatting it into lines with appropriate
           indentation and line breaks.
           [//]
           Note that the current implementation supports only ASCII text
           formatting.  If you want to support Unicode, you are best off
           implementing your own `kdu_message'-derived object from
           scratch.
      */
    KDU_EXPORT void put_text(const kdu_uint16 *string)
      { kdu_message::put_text(string); }
      /* [SYNOPSIS]
           We do not currently provide a text formatting service for
           unicode strings -- they will get lost.  this function is
           included only to avoid warning messages about the underlying
           virtual function being hidden by the previous `put_text'
           function.  In the future, we might give it a useful
           implementation.
      */
    KDU_EXPORT void flush(bool end_of_message=false);
      /* [SYNOPSIS]
           Flushes the current line, as though the line length had been
           exceeded, and invokes the output object's `kdu_message::flush'
           function.
      */
    void start_message()
      {
        if (dest == NULL) return;
        dest->start_message();
        flush();
      }
      /* [SYNOPSIS]
           Passes the `start_message' call along to the output object,
           which may wish to lock a mutex to guard multi-threaded access
           to the text formatting facilities provided here.  The `flush'
           function is then called to flush any outstanding text from
           previous formatting operations.
      */
  private: // Data
    char line_buf[201];
    int line_chars; // Maximum number of characters per line. Must be <= 200.
    int num_chars; // Number of characters written to current line.
    int max_indent;
    int indent; // Indent to be applied until next new-line character.
    int master_indent; // Indent to be applied to all lines henceforth.
    bool no_output_since_newline;
    kdu_message *dest;
  };

/*****************************************************************************/
/*                                kdu_error                                  */
/*****************************************************************************/

class kdu_error : public kdu_message {
  /* [SYNOPSIS]
       Objects of this class may be created, written to and then
       destroyed as a compact and powerful mechanism for generating error
       messages and jumping out of the immediate execution context.  All
       text is delivered to the `kdu_message'-derived object  supplied in
       the most recent call to the `kdu_customize_errors' function.  The
       `kdu_error' object's destructor plays a key role in completing
       errors.  It first invokes the output object's `kdu_message::flush'
       function with an `end_of_message' argument equal to true, and then
       terminates the process through the system `exit' function.
       [//]
       To avoid process termination, the caller may supply a special
       `kdu_message'-derived object to `kdu_customize_errors' which
       overrides the `flush' function so as to throw an exception in the
       event that its `flush' member is called with `end_of_message' equal
       to true.  This condition is guaranteed not to occur until the
       `kdu_error' object is destroyed.  For consistency, and especially
       to ensure correct behaviour of Kakadu's multi-threaded processing
       machinery, you should only throw integer-valued exceptions.
       [//]
       The following code-fragment illustrates typical usage:
       [>>]
       { kdu_error e; if (error) { kdu_error e; e << "Oops! Don't do that."; }}
       [//]
       In view of the preceding discussion, it should be clear that
       `kdu_error' objects should never be created on the heap.
       [//]
       As of Kakadu Version 4.5, the `kdu_error' object provides additional
       constructors which may be used to customize the way messages are
       displayed, providing for internationalizable text and/or allowing
       compiled code to be stripped of numerous messages which are
       meaningful only to the developer.
  */
  public:
    KDU_EXPORT kdu_error();
      /* [SYNOPSIS]
           This original form of the constructor for the `kdu_error' object
           should be used if you have no special requirements such as
           custom lead-in's or internationalization.
      */
    KDU_EXPORT kdu_error(const char *lead_in);
      /* [SYNOPSIS]
           This form of the constructor modifies the lead-in message
           which is automatically generated whenever a `kdu_error' object
           is constructed.
           [//]
           The default constructor uses the lead-in string "Kakadu Error:\n".
           If you use this form of the constructor, the supplied `lead_in'
           will be used.  If you wish the lead-in text to appear on its
           own separate line, you must explicitly supply the end-of-line
           character.
      */
    KDU_EXPORT kdu_error(const char *context, kdu_uint32 id);
      /* [SYNOPSIS]
           This form of the constructor allows all for the creation of
           internationalizable or otherwise customized messages.  The
           `context' string and `id' are used together to form a unique
           index against which customizable aspects of the message text
           are registered using the `kdu_customize_text' function.  That
           function can be used to register both the lead-in text and
           a string to replace each instance of the special pattern "<#>"
           which is supplied to `put_text'.
           [//]
           If nothing is registered against this `context'/`id' pair, a
           special lead-in string is generated which commences with the
           text: "Untranslated error -- Consult vendor for more information".
           This lead-in is followed by details of the `context' string and
           `id' value, followed by the untranslated text of the error message.
           This information is sufficient for the full error message to
           be reconstructed by the vendor who compiled the application.
           [//]
           All calls to `kdu_error' from within the Kakadu core system
           invoke the constructor via a macro replacement strategy which
           allows either simple messaging or full registered messaging
           for internationalization.  See any of the source files to
           understand how this is achieved.
      */
    KDU_EXPORT ~kdu_error();
      /* [SYNOPSIS]
           The destructor terminates the process through `exit', unless a
           custom error handling `kdu_message'-derived object has been
           installed using `kdu_customize_errors'.  In the latter case,
           the error can be caught immediately before the call to `exit'
           by overriding `kdu_message::flush' and checking for the
           `end_of_message'=true condition.  The override may throw an
           exception in interactive applications where processing must
           be able to continue.  Because this behaviour is to be expected,
           you should be careful only to destroy `kdu_error' objects from a
           context in which both process exit and exception throwing are
           reasonable behaviours. */
    KDU_EXPORT void put_text(const char *string);
      /* [SYNOPSIS]
           Passes all text directly through to the `kdu_message'-derived
           error message handler supplied to `kdu_customize_errors', if
           not NULL.
      */
    KDU_EXPORT void put_text(const kdu_uint16 *string)
      { if (handler != NULL) handler->put_text(string); }
      /* [SYNOPSIS]
           Passes all unicode text directly through to the
           `kdu_message'-derived error message handler supplied to
           `kdu_customize_errors', if not NULL.
      */
    void flush(bool end_of_message)
      { if (handler != NULL) handler->flush(false); }
      /* [SYNOPSIS]
           Invokes the `kdu_message::flush' function associated with the
           error message handler supplied to `kdu_customize_errors', if not
           NULL, being careful to always set `end_of_message' to false in this
           forwarded call.  This ensures that an `end_of_message' value of
           true will uniquely identify the fact that the `kdu_error' object
           is being destroyed.
      */
  private:
    kdu_message *handler;
    const char *ascii_text; // Non-NULL if ASCII replacement text exists
    const kdu_uint16 *unicode_text; // Non-NULL if Unicode replacements exist
  };

/*****************************************************************************/
/*                               kdu_warning                                 */
/*****************************************************************************/

class kdu_warning : public kdu_message {
  /* [SYNOPSIS]
       Objects of this class are used in a similar manner to `kdu_error'
       objects, except that the destructor does not terminate the currently
       executing process.  Note, however, that even warning handlers may
       conceivable throw exceptions or terminate the process themselves.
       The warning handler is configured using `kdu_customize_warnings'.
       For this reason, `kdu_warning' objects should never be created on
       the heap.
  */
  public:
    KDU_EXPORT kdu_warning();
      /* [SYNOPSIS]
           This original form of the constructor for the `kdu_warnikng' object
           should be used if you have no special requirements such as
           custom lead-in's or internationalization.
      */
    KDU_EXPORT kdu_warning(const char *lead_in);
      /* [SYNOPSIS]
           This form of the constructor modifies the lead-in message
           which is automatically generated whenever a `kdu_warning' object
           is constructed.
           [//]
           The default constructor uses the lead-in string "Kakadu Warning:\n".
           If you use this form of the constructor, the supplied `lead_in'
           will be used.  If you wish the lead-in text to appear on its
           own separate line, you must explicitly supply the end-of-line
           character.
      */
    KDU_EXPORT kdu_warning(const char *context, kdu_uint32 id);
      /* [SYNOPSIS]
           This form of the constructor allows all for the creation of
           internationalizable or otherwise customized messages.  The
           `context' string and `id' are used together to form a unique
           index against which customizable aspects of the message text
           are registered using the `kdu_customize_text' function.  That
           function can be used to register both the lead-in text and
           a string to replace each instance of the special pattern "<#>"
           which is supplied to `put_text'.
           [//]
           If nothing is registered against this `context'/`id' pair,
           no warning will be printed at all.  This may be used to hide
           warnings which are meaningful only to developers from the
           end users of an application.
           [//]
           All calls to `kdu_warning' from within the Kakadu core system
           invoke the constructor via a macro replacement strategy which
           allows either simple messaging or full registered messaging
           for internationalization.  See any of the source files to
           understand how this is achieved.
      */
    KDU_EXPORT ~kdu_warning();
      /* [SYNOPSIS]
           The destructor will invoke the `kdu_message::flush' function of
           any warning handler installed using `kdu_customize_warnings',
           supplying an `end_of_message' argument equal to true.  This
           condition will not occur in any context other than the destruction
           of the `kdu_warning' object.
      */
    KDU_EXPORT void put_text(const char *string);
      /* [SYNOPSIS]
           Passes all text directly through to the `kdu_message'-derived
           warning message handler supplied to `kdu_customize_warnings', if
           not NULL.
      */
    KDU_EXPORT void put_text(const kdu_uint16 *string)
      { if (handler != NULL) handler->put_text(string); }
      /* [SYNOPSIS]
           Passes all unicode text directly through to the
           `kdu_message'-derived warning message handler supplied to
           `kdu_customize_warnings', if not NULL.
      */
    void flush(bool end_of_message)
      { if (handler != NULL) handler->flush(false); }
      /* [SYNOPSIS]
           Invokes the `kdu_message::flush' function associated with the
           warning message handler supplied to `kdu_customize_warnings', if not
           NULL, being careful to always set `end_of_message' to false in this
           forwarded call.  This ensures that an `end_of_message' value of
           true will uniquely identify the fact that the `kdu_warning' object
           is being destroyed.
      */
  private:
    kdu_message *handler;
    const char *ascii_text; // Non-NULL if ASCII replacement text exists
    const kdu_uint16 *unicode_text; // Non-NULL if Unicode replacements exist
  };

/* ========================================================================= */
/*                     Messaging Short-Cut Functions                         */
/* ========================================================================= */

inline void
  kdu_print_error(const char *message)
{ kdu_error e; e << message; }
  /* [SYNOPSIS]
       This function provides a compact means of generating simple
       error messages through `kdu_error'.  It is particularly useful
       for foreign language bindings which require all objects to reside
       on the heap, since `kdu_error' objects should never be created
       on the heap.
  */

inline void
  kdu_print_warning(const char *message)
{ kdu_warning w; w << message; }
  /* [SYNOPSIS]
       This function provides a compact means of generating a simple
       warning message through `kdu_warning'.  It is particularly useful
       for foreign language bindings which require all objects to reside
       on the heap, since `kdu_warning' objects should never be created
       on the heap.
  */

#endif // KDU_UTILS_H
