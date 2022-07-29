/*****************************************************************************/
// File: kdu_server.cpp [scope = APPS/SERVER]
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
   A simple JPEG2000 interactive image server application, built around the
support offered by the Kakadu core system for region-of-interest access,
in-place transcoding, on-demand packet construction, and caching compressed
data sources.
******************************************************************************/

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <direct.h>
#include "client_server_comms.h"
#include "kdu_compressed.h"
#include "kdu_messaging.h"
#include "kdu_args.h"
#include "server_local.h"

static kdu_byte mac_address[6] = {0,0,0,0,0,0};


/* ========================================================================= */
/*                         Set up messaging services                         */
/* ========================================================================= */

static kd_message_lock message_lock;
static kd_message_sink sys_msg(stdout,&message_lock,false);
static kd_message_sink sys_msg_with_exceptions(stdout,&message_lock,true);

static kdu_message_formatter kd_msg_with_exceptions(&sys_msg_with_exceptions);

kdu_message_formatter kd_msg(&sys_msg);
kd_message_log kd_msg_log;


/* ========================================================================= */
/*                             Internal Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                        print_version                               */
/*****************************************************************************/

static void
  print_version()
{
  kdu_message_formatter &out = kd_msg;
  out.start_message();
  out << "This is Kakadu's \"kdu_server\" application.\n";
  out << "\tCompiled against the Kakadu core system, version "
      << KDU_CORE_VERSION << "\n";
  out << "\tCurrent core system version is "
      << kdu_get_core_version() << "\n";
  out.flush(true);
  exit(0);
}

/*****************************************************************************/
/* STATIC                         print_usage                                */
/*****************************************************************************/

static void
  print_usage(char *prog, bool comprehensive=false)
{
  kdu_message_formatter &out = kd_msg;
  out.start_message();

  out << "Usage:\n  \"" << prog << " ...\n";
  out.set_master_indent(3);
  out << "-wd <working directory>\n";
  if (comprehensive)
    out << "\tSpecifies the working directory for the kakadu server; this can "
           "work in conjunction with the -restrict parameter if required.\n";
  out << "-cd <directory in which to store \".cache\" files>\n";
  if (comprehensive)
    out << "\tThe server creates a \".cache\" file for each source file "
           "that it serves, which contains a digest of the metadata "
           "structure and the selected placeholder partitioning (see "
           "`-phld_threshold').  So long as the \".cache\" file persists, "
           "the image will presented in the same way to clients.  This means "
           "that clients can reliably share information with each other "
           "and that a client may reconnect to an image at some later "
           "point (perhaps days or weeks) and fully reuse the information "
           "cached from previous browsing sessions.  Once the \".cache\" "
           "file is created, it will not be changed when clients later "
           "connect to the same image.  By default, the \".cache\" file "
           "is written to the same directory as the original image file.  "
           "The present argument allows you to specify an alternate "
           "directory for the \".cache\" files.  The cache file's path name "
           "is formed by appending the requested image file name, "
           "including all relative path segments, to the supplied directory "
           "name.  If the cache directory is specified with a relative "
           "path, that path is relative to the working directory, which "
           "may be explicitly specified via the `-wd' argument.  This "
           "argument and the `-wd' argument are both most reliably used "
           "in conjunction with the `-restrict' argument. \n";
  out << "-log <log file>\n";
  if (comprehensive) 
    out << "\tRedirects console messages to the specified log file.\n";
  out << "-passwd <administration password>\n";
  if (comprehensive)
    out << "\tIf this argument is used, you will be able to perform various "
           "remote administration tasks by using the \"kdu_server_admin\" "
           "application, with the same password.  The password is any "
           "non-empty character string, although long strings, containing "
           "uncommon characters and sub-strings are best, as usual.\n";
  out << "-port <listen port number>\n";
  if (comprehensive)
    out << "\tBy default, the server listens for HTML connection reqests "
           "on port 80.  To override this, you may specify a different port "
           "number here.\n";
  out << "-address <listen IP address>\n";
  if (comprehensive)
    out << "\tYou should not need to use this command unless you wish to "
           "serve images from a multi-homed host.  By default, the server "
           "uses the first valid IP address on which the machine is "
           "capable of listening.  IP addresses should be supplied in the "
           "familiar period-separated numeric format.\n";
  out << "-delegate <host>[:<port>][*<load share>]\n";
  if (comprehensive)
    out << "\tDelegate the task of actually serving clients to one or "
           "more other servers, usually running on different machines.  This "
           "argument may be supplied multiple times, once for each host to "
           "which service may be delegated.  When multiple delegates are "
           "supplied, the load is distributed on the basis of the optional "
           "load share specifier.  The load share is a positive integer, "
           "which defaults to 4, if not explicitly provided.  Clients are "
           "delegated to the available hosts on a round-robin basis until "
           "each host has received its load share, after which all the "
           "load share counters are initialized to the load share value "
           "and round robin delegation continues from there.\n";
  out << "-time <max connection seconds>\n";
  if (comprehensive)
    out << "\tBy default, clients will be automatically disconnected after "
           "being continuously connected for a period of 5 minutes.  A "
           "different maximum connection time (expressed in seconds) may be "
           "specified here.\n";
  out << "-clients <max client connections>\n";
  if (comprehensive)
    out << "\tBy default, the server permits two client connections to "
           "be serviced at once.  This argument allows the limit to be "
           "adjusted.\n";
  out << "-sources <max open sources>\n";
  if (comprehensive)
    out << "\tBy default, the server permits one open JPEG2000 source file "
           "for each client connection.  This argument may "
           "be used to reduce the number of allowed source files.  Clients "
           "browsing the same image share a single open source file, which "
           "leads to a number of efficiencies.  New client connections will "
           "be refused, even if the total number of clients does not exceed "
           "the limit supplied using `-clients', if the total number of open "
           "files would exceed the limit.  If the `-clients' argument is "
           "missing, the value supplied to a `-sources' argument also becomes "
           "the maximum number of connected clients.\n";
  out << "-max_rate <max bytes/second>\n";
  if (comprehensive)
    out << "\tBy default, transmission of JPEG2000 packet data to the client "
           "is limited to a maximum rate of 4 kBytes/second.  "
           "A different maximum transmission rate (expressed in bytes/second) "
           "may be specified here.  Data is transmitted at the maximum "
           "rate until certain queuing constraints are encountered, if any.\n";
  out << "-max_rtt <max target RTT, in seconds>\n";
  if (comprehensive)
    out << "\tMaximum value to be used as the server's target round trip time "
           "(RTT).  The actual instantaneous RTT may be somewhat larger than "
           "this, depending upon network conditions.  The default value for "
           "this argument is 2 seconds.\n";
  out << "-min_rtt <min target RTT, in seconds>\n";
  if (comprehensive)
    out << "\tMinimum value to be used as the server's target round trip time "
           "(RTT). The actual instantaneous RTT may be smaller than this "
           "value, but the server endeavours to queue sufficient messages "
           "onto the network so as to realize at least this RTT.  The "
           "default value for this argument is 0.5 seconds.  Whenever "
           "the minimum target RTT is lower than the maximum target RTT, the "
           "server will attempt to hunt for the smallest target RTT which is "
           "consistent with these bounds and also maximizes network "
           "utilization.\n";
  out << "-max_chunk <max transfer chunk bytes>\n";
  if (comprehensive)
    out << "\tBy default, the maximum size of the image data chunks shipped "
           "by the server is 1024 bytes.  Flow control at the server or "
           "client is generally performed on chunk boundaries, so smaller "
           "chunks may give you finer granularity for estimating network "
           "conditions, at the expense of higher computational overhead, "
           "and some loss of transport efficiency.  In any event, you may "
           "not specify chunk sizes smaller than 128 bytes here.  "
           "Values smaller than about 32 bytes could cause some fundamental "
           "assumptions in the \"kdu_serve\" object to fail.\n";
  out << "-ignore_relevance\n";
  if (comprehensive)
    out << "\tBy supplying this flag, you force the server to ignore the "
           "degree to which a precinct overlaps with the spatial window "
           "requested by the client, serving up compressed data from all "
           "precincts which have any relevance at all, layer by layer, "
           "with the lowest frequency subbands appearing first within each "
           "layer.  By contrast, the default behaviour is to schedule "
           "precinct data in such a way that more information is provided "
           "for those precincts which have a larger overlap with the "
           "window of interest.  If the source code-stream contains a "
           "COM marker segment which identifies the distortion-length slope "
           "thresholds which were originally used to form the quality layers, "
           "and hence packets, this information is used to schedule precinct "
           "data in a manner which is approximately optimal in the same "
           "rate-distortion sense as that used to form the original "
           "code-stream layers, taking into account the degree to which "
           "each precinct is actually relevant to the window of interest.\n";
  out << "-phld_threshold <JP2 box partitioning threshold>\n";
  if (comprehensive)
    out << "\tThe threshold represents the maximum size for any JP2 box "
           "before that box is replaced by a placeholder in its containing "
           "data-bin, where the placeholder identifies a separate data-bin "
           "which holds the box's contents.  Selecting a large value for the "
           "threshold allows all meta-data to be appear in meta data-bin 0, "
           "with placeholders used only for the contiguous codestream boxes.  "
           "Selecting a small value tends to distribute the meta-data over "
           "numerous data-bins, each of which is delivered to the client "
           "only if its contents are deemed relevant to the client request.  "
           "The default value for the partitioning threshold is 32 bytes.\n"
           "\t   Note carefully that this argument will have no affect on the "
           "partitioning of meta-data into data-bins for target files whose "
           "representation has already been cached in a file having the "
           "suffix, \".cache\".  This is done whenever a target file is first "
           "opened by an instance of the server so as to ensure the delivery "
           "of a consistent image every time the client requests it.  If you "
           "delete the cache file, the server will generate a new target ID "
           "which will prevent the client from re-using any information "
           "it recovered during previous browsing sessions.\n";
  out << "-cache <cache bytes per client>\n";
  if (comprehensive)
    out << "\tWhen serving a client, the JPEG2000 source file is managed by "
           "a persistent Kakadu codestream object.  This object loads "
           "compressed data on demand from the source file.  Data which is "
           "not in use can also be temporarily unloaded from memory, so "
           "long as the JPEG2000 code-stream contains appropriate "
           "PLT (packet length) marker segments and a packet order in which "
           "all packets of any given precinct appear contiguously.  If you "
           "are unsure whether a particular image has an appropriate "
           "structure to permit dynamic unloading and reloading of the "
           "compressed data, try opening it with \"kdu_show\" and monitoring "
           "the compressed data memory using the appropriate status mode.\n"
           "\t   Under these conditions, the system employs a FIFO "
           "(first-in first-out) cachine strategy to unload compressed "
           "data which is not in use once a cache size threshold is "
           "exceeded.  The default cache size "
           "used by this application is 2 Megabytes per client attached to "
           "the same code-stream.  You may specify an alternate per-client "
           "cache size here, which may be as low as 0.  Kakadu applications "
           "should work well even if the cache size is 0, but the server "
           "application may become disk bound if the cache size gets too "
           "small.\n";
  out << "-history <max history records>\n";
  if (comprehensive)
    out << "\tIndicates the maximum number of client records which are "
           "maintained in memory for serving up to the remote administrator "
           "application.  Each record contains simple statistics concerning "
           "the behaviour of a recent client connection.  The default "
           "limit is 100, but there is no harm in increasing this "
           "considerably.\n";
  out << "-initial_timeout <seconds>\n";
  if (comprehensive)
    out << "\tSpecifies the timeout value to use for the handshaking which "
           "is used to establish persistent connection channels.  Each time "
           "a TCP channel is accepted by the server, it allows this amount "
           "of time for the client to pass in the connection message.  In "
           "the case of the initial HTTP connection, the client must send "
           "its HTTP GET request within the timeout period.  In the case of "
           "persistent TCP session channels, the client must send its initial "
           "CONNECT message within the timeout period.  The reason for timing "
           "these events is to guard against malicious behaviour in denial "
           "of service attacks.  The default timeout is 5 seconds, but this "
           "might not be enough when operating over very slow links.\n";
  out << "-completion_timeout <seconds>\n";
  if (comprehensive)
    out << "\tSpecifies the time within which the client must complete all "
           "persistent connections required by the relevant protocol, from "
           "the point at which it is sent the corresponding connection "
           "parameters.  The server will not hold a session open "
           "indefinitely, since the client might terminate leaving the "
           "resources unclaimed.  The default timeout value is 20 seconds.\n";
  out << "-connection_threads <max threads for managing new connections>\n";
  if (comprehensive)
    out << "\tSpecifies the maximum number of threads which can be "
           "dedicated to managing the establishment of new connections.  "
           "The new connections are handed off to dedicated per-client "
           "threads as soon as possible, but connection threads are "
           "responsible for initially opening files and managing all "
           "tasks associated with delegating services to other servers.  By "
           "allowing multiple connection requests to be processed "
           "simultaneously, the performance of the server need not be "
           "compromised by clients with slow channels.  The default "
           "maximum number of connection management threads is 5.\n";
  out << "-restrict -- restrict access to images in the working directory\n";
  out << "-record -- print all HTTP requests and replies to stdout.\n";
  out << "-version -- print core system version I was compiled against.\n";
  out << "-v -- abbreviation of `-version'\n";
  out << "-usage -- print a comprehensive usage statement.\n";
  out << "-u -- print a brief usage statement.\"\n\n";
  out.flush(true);
  exit(0);
}

/*****************************************************************************/
/* STATIC                       parse_arguments                              */
/*****************************************************************************/

static char *
  parse_arguments(kdu_args &args, kdu_uint32 &ip_address, int &port,
                  int &max_clients, int &max_sources,
                  kd_delegator &delegator,
                  float &max_connection_seconds,
                  float &max_rtt_target, float &min_rtt_target,
                  float &max_bytes_per_second,
                  int &max_chunk_size, bool &ignore_relevance_info,
                  int &phld_threshold, int &per_client_cache,
                  int &max_history_records, int &initial_timeout_milliseconds,
                  int &completion_timeout_milliseconds,
                  int &max_connection_threads,
                  bool &restricted_access, char *&cache_directory)
  /* Note: returns with `ip_address' equal to INADDR_NONE, unless an
     explicit listening address is provided on the command line.  The
     return string, if non-NULL, is a password. */
{
  ip_address = INADDR_NONE;
  port = 80;
  max_clients = max_sources = 0; // Default policy established later.
  max_rtt_target = 2.0F;
  min_rtt_target = 0.5F;
  max_bytes_per_second = 4000.0F;
  max_connection_seconds = 300.0F;
  max_chunk_size = 1024;
  ignore_relevance_info = false;
  phld_threshold = 32;
  per_client_cache = 2000000; // 2 Megabytes per client.
  max_history_records = 100;
  initial_timeout_milliseconds = 5000;
  completion_timeout_milliseconds = 20000;
  max_connection_threads = 5;
  restricted_access = false;
  cache_directory = NULL;
  char *passwd = NULL;

  if (args.find("-u") != NULL)
    print_usage(args.get_prog_name());
  if (args.find("-usage") != NULL)
    print_usage(args.get_prog_name(),true);
  if ((args.find("-version") != NULL) || (args.find("-v") != NULL))
    print_version();

  if(args.find("-wd") != NULL)
    {
      const char *string = args.advance();
      if ((string == NULL) || (*string == '\0'))
        { kdu_error e; e << "Missing or empty working directory supplied "
          "with the \"-wd\" argument."; }
      if (_chdir(string) != 0)
        { kdu_error e; e << "Unable to change working directory to \""
          << string << "\"."; }
      args.advance();
    }

  if (args.find("-cd") != NULL)
    {
      const char *string = args.advance();
      if ((string == NULL) || (*string == '\0'))
        { kdu_error e; e << "Missing or empty cache directory supplied "
          "with the \"-cd\" argument."; }
      size_t len = strlen(string);
      cache_directory = new char[len+1];
      strcpy(cache_directory,string);
      args.advance();
      if ((cache_directory[len-1] == '/') || (cache_directory[len-1] == '\\'))
        cache_directory[len-1] = '\0';
    }

  if (args.find("-passwd") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) || (*string == '\0'))
        { kdu_error e; e << "Missing or empty password string supplied "
          "with the \"-passwd\" argument."; }
      if (passwd != NULL)
        delete[] passwd;
      passwd = new char[strlen(string)+1];
      strcpy(passwd,string);
      args.advance();
    }
  if (args.find("-address") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          ((ip_address = inet_addr(string)) == INADDR_NONE))
        { kdu_error e; e << "Missing or illegal IP address supplied with "
          "the \"-address\" argument."; }
      args.advance();
    }
  if (args.find("-port") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) || (sscanf(string,"%d",&port) != 1) || (port <= 0))
        { kdu_error e; e << "\"-port\" argument requires a positive port "
          "number for the port number on which to listen for connections."; }
      args.advance();
    }
  if (args.find("-clients") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) || (sscanf(string,"%d",&max_clients) != 1) ||
          (max_clients <= 0))
        { kdu_error e; e << "\"-clients\" argument requires a positive "
          "value for the maximum number of supported clients."; }
      args.advance();
    }
  if (args.find("-sources") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) || (sscanf(string,"%d",&max_sources) != 1) ||
          (max_sources <= 0))
        { kdu_error e; e << "\"-sources\" argument requires a positive "
          "value for the maximum number of simultaneously open sources."; }
      args.advance();
    }
  if (max_clients == 0)
    max_clients = 2;
  if (max_clients < max_sources)
    max_clients = max_sources;
  if (max_sources == 0)
    max_sources = max_clients;
  if (args.find("-sources") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) || (sscanf(string,"%d",&max_sources) != 1) ||
          (max_sources <= 0))
        { kdu_error e; e << "\"-sources\" argument requires a positive "
          "value for the maximum number of simultaneously open sources."; }
      args.advance();
    }
  if (args.find("-max_rtt") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          (sscanf(string,"%f",&max_rtt_target) != 1) ||
          (max_rtt_target <= 0.0F))
        { kdu_error e; e << "\"-max_rtt\" argument requires a positive "
          "maximum value for the target datagram RTT."; }
      args.advance();
    }
  if (args.find("-min_rtt") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          (sscanf(string,"%f",&min_rtt_target) != 1) ||
          (min_rtt_target < 0.0F) || (min_rtt_target > max_rtt_target))
        { kdu_error e; e << "\"-min_rtt\" argument requires a non-negative "
          "minimum value for the target datagram RTT.  The value may not "
          "exceed the specified (or default) maximum target RTT value of "
          << max_rtt_target << " seconds."; }
      args.advance();
    }
  if (args.find("-max_rate") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          (sscanf(string,"%f",&max_bytes_per_second) != 1) ||
          (max_bytes_per_second <= 0.0F))
        { kdu_error e; e << "\"-max_rate\" argument requires a positive "
          "maximum transmission rate (bytes/second) for JPEG2000 packet "
          "data."; }
      args.advance();
    }
  if (args.find("-time") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          (sscanf(string,"%f",&max_connection_seconds) != 1) ||
          (max_connection_seconds <= 0.0F))
        { kdu_error e; e << "\"-time\" argument requires a positive time limit "
          "(seconds) for client connections!"; }
      args.advance();
    }
  if (args.find("-max_chunk") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          (sscanf(string,"%d",&max_chunk_size) != 1) ||
          (max_chunk_size < 128))
        { kdu_error e; e << "The `-max_chunk' argument requires an integer "
          "parameter, no smaller than 128."; }
      args.advance();
    }
  if (args.find("-ignore_relevance") != NULL)
    {
      ignore_relevance_info = true;
      args.advance();
    }
  if (args.find("-phld_threshold") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          (sscanf(string,"%d",&phld_threshold) != 1) ||
          (phld_threshold <= 0))
        { kdu_error e; e << "The `-phld_threshold' argument requires a "
          "positive integer parameter."; }
      args.advance();
    }
  if (args.find("-cache") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          (sscanf(string,"%d",&per_client_cache) != 1) ||
          (per_client_cache < 0))
        { kdu_error e; e << "The `-cache' argument requires a non-negative "
          "integer parameter."; }
      args.advance();
    }
  if (args.find("-history") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          (sscanf(string,"%d",&max_history_records) != 1) ||
          (max_history_records < 0))
        { kdu_error e; e << "The `-history' argument requires a non-negative "
          "integer parameter."; }
      args.advance();
    }
  if (args.find("-initial_timeout") != NULL)
    {
      char *string = args.advance();
      float fval;
      if ((string == NULL) || (sscanf(string,"%f",&fval)!=1) || (fval <= 0.0F))
        { kdu_error e; e << "The `-initial_timeout' argument requires a "
          "positive number of seconds."; }
      args.advance();
      initial_timeout_milliseconds = 1 + (int)(fval * 1000.0F);
    }
  if (args.find("-completion_timeout") != NULL)
    {
      char *string = args.advance();
      float fval;
      if ((string == NULL) || (sscanf(string,"%f",&fval)!=1) || (fval <= 0.0F))
        { kdu_error e; e << "The `-completion_timeout' argument requires a "
          "positive number of seconds."; }
      args.advance();
      completion_timeout_milliseconds = 1 + (int)(fval * 1000.0F);
    }
  if (args.find("-connection_threads") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) ||
          (sscanf(string,"%d",&max_connection_threads) != 1) ||
          (max_connection_threads < 1))
        { kdu_error e; e << "The `-connection_threads' argument requires a "
          "positive integer parameter."; }
      args.advance();
    }
  if (args.find("-restrict") != NULL)
    {
      restricted_access = true;
      args.advance();
    }
  if (args.find("-record") != NULL)
    {
      kd_msg_log.init(&kd_msg);
      args.advance();
    }
  while (args.find("-delegate") != NULL)
    {
      const char *string = args.advance();
      if (string == NULL)
        { kdu_error e; e << "The `-delegate' argument requires a string "
          "parameter."; }
      delegator.add_host(string);
      args.advance();
    }
  return passwd;
}

/*****************************************************************************/
/* INLINE                        write_hex_val32                             */
/*****************************************************************************/

static inline void
  write_hex_val32(char *dest, kdu_uint32 val)
{
  int i;
  char v;
  for (i=8; i > 0; i--, val<<=4, dest++)
    {
      v = (char)((val >> 28) & 0x0000000F);
      *dest = (v < 10)?('0'+v):('A'+v-10);
    }
}

/*****************************************************************************/
/* INLINE                         read_hex_val32                             */
/*****************************************************************************/

static inline kdu_uint32
  read_hex_val32(const char *src)
{
  kdu_uint32 val=0;
  int i;
  for (i=8; (i > 0) && (*src != '\0'); i--, src++)
    {
      val <<= 4;
      if ((*src >= '0') && (*src <= '9'))
        val += (kdu_uint32)(*src - '0');
      else if ((*src >= 'A') && (*src <= 'F'))
        val += 10 + (kdu_uint32)(*src - 'A');
    }
  return val;
}

/*****************************************************************************/
/* INLINE                read_val_or_wild (kdu_long)                         */
/*****************************************************************************/

static inline bool
  read_val_or_wild(const char * &string, kdu_long &val)
  /* Reads an unsigned decimal integer from the supplied `string', advancing
     the pointer immediately past the last character in the integer.  If
     `*string' is equal to `*' (wild-card), `val' is set to -1, again
     after advancing the string.  If a parsing error occurs, the function
     returns false. */
{
  if (*string == '*')
    { string++; val = -1; return true;}
  if ((*string >= '0') && (*string <= '9'))
    {
      val = 0;
      do {
          val = val*10 + (int)(*(string++) - '0');
        } while ((*string >= '0') && (*string <= '9'));
      return true;
    }
  return false;
}

/*****************************************************************************/
/* INLINE                   read_val_or_wild (int)                           */
/*****************************************************************************/

static inline bool
  read_val_or_wild(const char * &string, int &val)
  /* Same as above, but for regular integers. */
{
  if (*string == '*')
    { string++; val = -1; return true;}
  if ((*string >= '0') && (*string <= '9'))
    {
      val = 0;
      do {
          val = val*10 + (int)(*(string++) - '0');
        } while ((*string >= '0') && (*string <= '9'));
      return true;
    }
  return false;
}

/*****************************************************************************/
/* INLINE                read_val_range_or_wild (int)                        */
/*****************************************************************************/

static inline bool
  read_val_range_or_wild(const char * &string, int &min, int &max)
  /* Same as above, except that the function can also read a range of two
     values.  If there is only one value, `min' and `max' are both set to
     that value.  If the wildcard character `*' is encountered, both are set
     to -1. */
{
  if (*string == '*')
    { string++; min = max = -1; return true;}
  if ((*string >= '0') && (*string <= '9'))
    {
      min = 0;
      do {
          min = min*10 + (int)(*(string++) - '0');
        } while ((*string >= '0') && (*string <= '9'));
      if (*string == '-') 
        {
          max = 0; string++;
          while ((*string >= '0') && (*string <= '9'))
            max = max*10 + (int)(*(string++) - '0');
          if (string[-1] == '-')
            return false; // Failed to actually read a number
        }
      else
        max = min;
      return true;
    }
  return false;
}

/*****************************************************************************/
/* INLINE                      encode_var_length                             */
/*****************************************************************************/

static inline int
  encode_var_length(kdu_byte *buf, int val)
  /* Encodes the supplied value using a byte-oriented extension code,
     storing the results in the supplied data buffer and returning
     the total number of bytes used to encode the value. */
{
  int shift;
  kdu_byte byte;
  int num_bytes = 0;

  assert(val >= 0);
  for (shift=0; val >= (128<<shift); shift += 7);
  for (; shift >= 0; shift -= 7, num_bytes++)
    {
      byte = (kdu_byte)(val>>shift) & 0x7F;
      if (shift > 0)
        byte |= 0x80;
      *(buf++) = byte;
    }
  return num_bytes;
}

/*****************************************************************************/
/*                               get_mac_address                             */
/*****************************************************************************/

void
  get_mac_address(kdu_byte address[])
  /* Fills in the first 6 bytes of the `address' array with the MAC
     address of our LAN card. */
{
  struct {
      ADAPTER_STATUS adapt;
      NAME_BUFFER name_buff[30];
    } adapter;
  NCB ncb;

  memset(&ncb,0,sizeof(ncb) );
  ncb.ncb_command = NCBRESET;
  ncb.ncb_lana_num = 0;
  Netbios(&ncb);
  memset(&ncb,0,sizeof(ncb));
  ncb.ncb_command = NCBASTAT;
  ncb.ncb_lana_num = 0;
  memset(ncb.ncb_callname,' ',NCBNAMSZ);
  ncb.ncb_callname[0] = '*';
  ncb.ncb_buffer = (unsigned char *) &adapter;
  ncb.ncb_length = sizeof(adapter);
  if (Netbios(&ncb) == 0)
    for (int i=0; i < 6; i++)
      address[i] = adapter.adapt.adapter_address[i];
}


/* ========================================================================= */
/*                               kd_message_log                              */
/* ========================================================================= */

/*****************************************************************************/
/*                            kd_message_log::print                          */
/*****************************************************************************/

void
  kd_message_log::print(const char *buffer, int num_chars, const char *prefix,
                        bool leave_blank_line)
{
  if (output == NULL)
    return;
  output->start_message();
  if (leave_blank_line)
    output->put_text("\n");
  while (num_chars > 0)
    {
      int xfer_len = 0;
      for (xfer_len=0; xfer_len < num_chars; xfer_len++)
        if (buffer[xfer_len] == '\n')
          break;
      if (buf_len < (xfer_len+2))
        { // Need to reallocate buffer.
          if (buf != NULL)
            delete[] buf;
          buf_len = xfer_len*2 + 20;
          buf = new char[buf_len];
        }
      memcpy(buf,buffer,(size_t) xfer_len);
      buf[xfer_len++] = '\n';
      buf[xfer_len] = '\0';
      (*output) << prefix << buf;
      buffer += xfer_len;
      num_chars -= xfer_len;
    }
  output->flush(true);
}


/* ========================================================================= */
/*                          kd_target_description                            */
/* ========================================================================= */

/*****************************************************************************/
/*                  kd_target_description::parse_byte_range                  */
/*****************************************************************************/

bool
  kd_target_description::parse_byte_range()
{
  range_start = 0;
  range_lim = KDU_LONG_MAX;
  const char *cp=byte_range;
  while (*cp == ' ') cp++;
  if (*cp == '\0')
    return true;

  while ((*cp >= '0') && (*cp <= '9'))
    range_start = (range_start*10) + (*(cp++) - '0');
  if (*(cp++) != '-')
    { range_start=0; return false; }
  range_lim = 0;
  while ((*cp >= '0') && (*cp <= '9'))
    range_lim = (range_lim*10) + (*(cp++) - '0');
  if ((range_lim <= range_start) || ((*cp != '\0') && (*cp != ' ')))
    { range_lim = KDU_LONG_MAX; return false; }
  range_lim++; // Since the upper end of the supplied range string is inclusive
  return true;
}

/* ========================================================================= */
/*                                 kd_source                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                              kd_source::unlink                            */
/*****************************************************************************/

void
  kd_source::unlink()
{
  if (manager == NULL)
    return;
  if (prev == NULL)
    {
      assert(manager->sources == this);
      manager->sources = next;
    }
  else
    prev->next = next;
  if (next != NULL)
    next->prev = prev;
  manager->num_sources--;
  manager = NULL;
  next = prev = NULL;
}

/*****************************************************************************/
/*                              kd_source::open                              */
/*****************************************************************************/

bool
  kd_source::open(kd_target_description &target, kd_message_block &explanation,
                  int phld_threshold, int per_client_cache,
                  const char *cache_directory)
{
  assert(this->target.filename[0] == '\0'); // Otherwise source is already open
  strcpy(this->target.filename,target.filename);
  strcpy(this->target.byte_range,target.byte_range);
  if (!this->target.parse_byte_range())
    assert(0); // Should already have been verified by the caller

  // Find the time at which the file was last modified
  HANDLE fhandle = CreateFile(this->target.filename,GENERIC_READ,
                              FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
  if (fhandle == INVALID_HANDLE_VALUE)
    {
      explanation << "404 Image does not exist on server";
      return false;
    }
  FILETIME last_write_time;
  if (!GetFileTime(fhandle,NULL,NULL,&last_write_time))
    last_write_time.dwHighDateTime = last_write_time.dwLowDateTime = 0;
  CloseHandle(fhandle);
  kdu_byte last_modify_buf[8];
  for (int n=0; n < 4; n++)
    {
      last_modify_buf[n] =
        (kdu_byte)(last_write_time.dwHighDateTime>>(24-n*8));
      last_modify_buf[4+n] =
        (kdu_byte)(last_write_time.dwLowDateTime>>(24-n*8));
    }

  // Find the name for the cache file
  char *cp, cache_filename[320];
  if (cache_directory == NULL)
    strcpy(cache_filename,this->target.filename);
  else
    {
      strcpy(cache_filename,cache_directory);
      char *rel_path = this->target.filename;
      if ((strchr(rel_path,':') != NULL) ||
          (rel_path[0] == '/') || (rel_path[0] == '\\'))
        { // Strip back to the actual file name
          if ((cp = strrchr(rel_path,'/')) != NULL)
            rel_path = cp+1;
          if ((cp = strrchr(rel_path,'\\')) != NULL)
            rel_path = cp+1;
        }
      while ((rel_path[0]=='.') && (rel_path[1]=='.') &&
             ((rel_path[2]=='/') || (rel_path[2]=='\\')))
        rel_path += 3;
      strcat(cache_filename,"/");
      strcat(cache_filename,rel_path);
    }
  cp = strrchr(cache_filename,'.');
  if (cp != NULL)
    *cp = '_';
  if (this->target.byte_range[0] != '\0')
    {
      strcat(cache_filename,"(");
      strcat(cache_filename,this->target.byte_range);
      strcat(cache_filename,")");
    }
  strcat(cache_filename,".cache");

  bool cache_exists = false;
  target_id[0] = '\0';

  // See if an existing compatible cache file exists
  FILE *cache_fp = fopen(cache_filename,"rb");
  if (cache_fp != NULL)
    { // Read the target-ID
      cache_exists = true;
      target_id[32] = '\0';
      if (fread(target_id,1,32,cache_fp) != 32)
        cache_exists = false;
      for (int i=0; (i < 32) && cache_exists; i++)
        if (!(((target_id[i] >= '0') && (target_id[i] <= '9')) ||
              ((target_id[i] >= 'a') && (target_id[i] <= 'f')) ||
              ((target_id[i] >= 'A') && (target_id[i] <= 'F'))))
          cache_exists = false;
      char test_modify_buf[8];
      if ((fread(test_modify_buf,1,8,cache_fp) != 8) ||
          (memcmp(test_modify_buf,last_modify_buf,8) != 0))
        cache_exists = false;
      if (!cache_exists)
        { fclose(cache_fp); cache_fp = NULL; target_id[0] = '\0'; }
    }

  bool open_succeeded = false;
  do { // Loop so we can catch a bad cache file, and rewrite it
      try {
          if ((!cache_exists) && generate_target_id())
            { // Try to create the cache file
              cache_fp = fopen(cache_filename,"wb");
              if ((cache_fp != NULL) &&
                  ((fwrite(target_id,1,32,cache_fp) != 32) ||
                   (fwrite(last_modify_buf,1,8,cache_fp) != 8)))
                { // Write failure (e.g., disk full)
                  fclose(cache_fp); cache_fp = NULL; target_id[0] = '\0';
                }
            }
          serve_target.open(this->target.filename,phld_threshold,
                            per_client_cache,cache_fp,cache_exists,
                            this->target.range_start,this->target.range_lim);
          open_succeeded = true;
        }
      catch (...)
        {
          if (cache_fp != NULL)
            { // Delete the cache file
              fclose(cache_fp); cache_fp = NULL;
              remove(cache_filename);
            }
          if (cache_exists)
            { // Try regenerating the cache file
              cache_exists = false;
              target_id[0] = '\0';
            }
          else
            break; // Nothing more we can do
        }
    } while (!open_succeeded);

  if (cache_fp != NULL)
    fclose(cache_fp);
  if (!open_succeeded)
    explanation << "404 Cannot open target file on server";
  return open_succeeded;
}

/*****************************************************************************/
/*                      kd_source::generate_target_id                        */
/*****************************************************************************/

bool
  kd_source::generate_target_id()
{
  memset(target_id,0,33);

  // Start by creating an initial 128-bit number from the last modification
  // date and the file name.
  HANDLE fhandle =
    CreateFile(target.filename,GENERIC_READ,FILE_SHARE_READ,NULL,
               OPEN_EXISTING,0,NULL);
  if (fhandle == INVALID_HANDLE_VALUE)
    return false;
  FILETIME last_write_time;
  if (!GetFileTime(fhandle,NULL,NULL,&last_write_time))
    last_write_time.dwHighDateTime = last_write_time.dwLowDateTime = 0;
  CloseHandle(fhandle);
  union {
      kdu_byte id_data[16];
      kdu_uint32 id_data32[4];
    };
  id_data32[0] = last_write_time.dwHighDateTime;
  id_data32[1] = last_write_time.dwLowDateTime;
  id_data32[2] = id_data32[3] = 0;

  int i;
  char *tmp, full_pathname[400]; full_pathname[0]=full_pathname[399]='\0';
  if (!GetFullPathName(target.filename,399,full_pathname,&tmp))
    strcpy(full_pathname,target.filename);
  for (i=0; full_pathname[i] != '\0'; i++)
    {
      id_data[8+(i&7)] ^= (kdu_byte) target.filename[i];
      id_data[15-(i&7)] ^= ((kdu_byte) target.filename[i]) << 4;
    }
  for (i=0; target.byte_range[i] != '\0'; i++)
    {
      id_data[8+(i&7)] ^= (kdu_byte) target.byte_range[i];
      id_data[15-(i&7)] ^= ((kdu_byte) target.byte_range[i]) << 4;
    }

  // Now see if we can encrypt the 128-bit number using our own MAC address
  kdu_byte mac_key[16];
  for (i=0; i < 16; i++)
    mac_key[i] = mac_address[i % 6];
  kdu_rijndael cipher;
  cipher.set_key(mac_key);
  cipher.encrypt(id_data);

  for (i=0; i < 32; i++)
    {
      kdu_byte val = id_data[i>>1];
      if ((i & 1) == 0)
        val >>= 4;
      val &= 0x0F;
      if (val < 10)
        target_id[i] = (char)('0'+val);
      else
        target_id[i] = (char)('A'+val-10);
    }
  return true;
}





/* ========================================================================= */
/*                              kd_source_thread                             */
/* ========================================================================= */

static unsigned int __stdcall
  kd_source_thread_start_proc(void *param)
{
  kd_source_thread *obj = (kd_source_thread *) param;
  try {
      obj->thread_entry();
    } catch(int) {};
  obj->closing();
  return 0;
}

/*****************************************************************************/
/*                      kd_source_thread::kd_source_thread                   */
/*****************************************************************************/

kd_source_thread::kd_source_thread(kd_source *source,
                                   kd_source_manager *manager,
                                   kd_delivery_manager *delivery_manager,
                                   const char *channel_id,
                                   kd_jpip_channel_type channel_type,
                                   int listen_port, int max_chunk_bytes,
                                   bool ignore_relevance_info,
                                   int completion_timeout)
{
  h_thread = NULL;
  h_event = CreateEvent(NULL,FALSE,FALSE,NULL); // Auto-reset
  this->max_chunk_bytes = max_chunk_bytes;
  this->ignore_relevance_info = ignore_relevance_info;
  this->completion_timeout = completion_timeout;
  this->source = source;
  this->manager = manager;
  this->delivery_manager = delivery_manager;
  this->cserver = NULL;
  tcp_transmitter = NULL;
  http_transmitter = NULL;
  this->extra_data_buf = new kdu_byte[max_chunk_bytes];
  jpip_cnew_header_required = false;
  destruction_scheduled = false;
  waiting_for_primary_channel = true;
  waiting_for_auxiliary_channel = (channel_type == KD_CHANNEL_HTTP_TCP);
  request_channel = primary_channel = auxiliary_channel = NULL;
  num_connections = 0;
  total_received_requests = total_serviced_requests = 0;
  num_serviced_with_byte_limits = cumulative_byte_limit = 0;
  idle_start = 0.0F;
  this->listen_port = listen_port;
  jpip_channel_type = channel_type;
  is_stateless = (channel_type == KD_CHANNEL_STATELESS);
  strcpy(this->channel_id,channel_id);
  prev = next = NULL;
}

/*****************************************************************************/
/*                     kd_source_thread::~kd_source_thread                   */
/*****************************************************************************/

kd_source_thread::~kd_source_thread()
{
  assert((tcp_transmitter == NULL) && (http_transmitter == NULL));
  if (h_thread != NULL)
    {
      WaitForSingleObject(h_thread,INFINITE);
      CloseHandle(h_thread);
    }
  CloseHandle(h_event);
  if (request_channel != NULL)
    delete request_channel;
  if (primary_channel != NULL)
    delete primary_channel;
  if (auxiliary_channel != NULL)
    delete auxiliary_channel;
  if (extra_data_buf != NULL)
    delete[] extra_data_buf;
}

/*****************************************************************************/
/*                           kd_source_thread::start                         */
/*****************************************************************************/

void
  kd_source_thread::start()
{
  unsigned int thread_id;
  h_thread = (HANDLE)
    _beginthreadex(NULL,0,kd_source_thread_start_proc,this,0,&thread_id);
  if (h_thread == NULL)
    closing();
}

/*****************************************************************************/
/*                         kd_source_thread::closing                         */
/*****************************************************************************/

void
  kd_source_thread::closing()
{
  waiting_for_primary_channel = waiting_for_auxiliary_channel = false;
  if (request_channel != NULL)
    request_channel->close(); // Always safe to close, even if not open.
  if (primary_channel != NULL)
    primary_channel->close(); // Always safe to close, even if not open.
  if (auxiliary_channel != NULL)
    auxiliary_channel->close();

  // Print transmission statistics and close transmitter
  bool session_complete = false;
  float connected_seconds = 0.0F;
  int transmitted_bytes = 0;
  int rtt_events = 0;
  float average_rtt = 0.0F;
  float approximate_rate = 0.0F;
  float average_serviced_byte_limit = 0.0F;
  float average_service_bytes = 0.0F;

  kd_msg.start_message();
  time_t binary_time; time(&binary_time);
  struct tm *local_time = localtime(&binary_time);
  kd_msg << "\n\tDisconnecting client\n";
  kd_msg << "\t\tFile = \"" << source->target.filename << "\"\n";
  if (is_stateless)
    kd_msg << "\t\tChannel transport = <none> (stateless)\n";
  else if (jpip_channel_type == KD_CHANNEL_HTTP_TCP)
    kd_msg << "\t\tChannel transport = \"http-tcp\"\n";
  else
    kd_msg << "\t\tChannel transport = \"http\"\n";
  kd_msg << "\t\tChannel ID = \"" << channel_id << "\"\n";
  kd_msg << "\t\tDisconnect time = " << asctime(local_time);
  if (tcp_transmitter != NULL)
    {
      session_complete = true;
      connected_seconds = tcp_transmitter->get_connected_seconds();
      transmitted_bytes = tcp_transmitter->get_total_transmitted_bytes();
      kd_msg << "\t\tConnected for " << connected_seconds << " seconds\n";
      kd_msg << "\t\tTotal bytes transmitted = " << transmitted_bytes
             << ", acknowledged = "
             << tcp_transmitter->get_total_acknowledged_bytes() << "\n";
      rtt_events = tcp_transmitter->get_total_rtt_events();
      if (rtt_events == 0)
        kd_msg << "\t\tAverage RTT = ------\n";
      else
        {
          average_rtt = tcp_transmitter->get_average_rtt();
          approximate_rate = tcp_transmitter->get_total_acknowledged_bytes() /
            (tcp_transmitter->get_active_seconds()+0.01F);
          kd_msg << "\t\tAverage RTT = " << average_rtt << " seconds\n";
          kd_msg << "\t\tApproximate transmission data rate = "
                 << approximate_rate << " bytes/second\n";
        }
    }
  else if (http_transmitter != NULL)
    {
      connected_seconds = http_transmitter->get_connected_seconds();
      transmitted_bytes = http_transmitter->get_total_transmitted_bytes();
      kd_msg << "\t\tTotal bytes transmitted = "
             << transmitted_bytes << "\n";
      if (num_serviced_with_byte_limits > 0)
        average_serviced_byte_limit = ((float) cumulative_byte_limit) /
          ((float) num_serviced_with_byte_limits);
      if ((!is_stateless) || (total_received_requests > 1))
        {
          session_complete = true;
          kd_msg << "\t\tConnected for "
                 << connected_seconds << " seconds\n";
          kd_msg << "\t\tAverage requested byte limit = "
                 << average_serviced_byte_limit << "\n";
        }
    }

  bool connection_established =
   (http_transmitter != NULL) || (tcp_transmitter != NULL);
  if (!connection_established)
    kd_msg << "\t\tClient failed to establish request and return channels.\n";
  else if (session_complete)
    {
      if (total_serviced_requests > 0)
        average_service_bytes = ((float) transmitted_bytes) /
          ((float) total_serviced_requests);
      kd_msg << "\t\tNumber of requests = "
             << total_received_requests << "\n";
      kd_msg << "\t\tNumber of requests not pre-empted = "
             << total_serviced_requests << "\n";
      kd_msg << "\t\tAverage bytes served per request not "
                "pre-empted = " << average_service_bytes << "\n";
      kd_msg << "\t\tNumber of primary channel attachment events = "
             << num_connections << "\n";
    }
  kd_msg.flush(true);
  if (tcp_transmitter != NULL)
    delivery_manager->release_transmitter(tcp_transmitter);
  if (http_transmitter != NULL)
    delivery_manager->release_transmitter(http_transmitter);
  tcp_transmitter = NULL;
  http_transmitter = NULL;
  this->destroy(); // Destroys the `kdu_serve' object's internal state

  // This is a good place to clean up any dead source threads.
  manager->delete_dead_threads();

  // Lock manager and update statistics
  manager->acquire_lock();
  kd_client_history *history = NULL;
  if (connection_established)
    {
      manager->total_transmitted_bytes += transmitted_bytes;
      manager->completed_connections++;
      history = manager->get_new_history_entry();
    }
  else
    manager->terminated_connections++;
  if (history != NULL)
    {
      history->jpip_channel_type = jpip_channel_type;
      strcpy(history->target.filename,source->target.filename);
      strcpy(history->target.byte_range,source->target.byte_range);
      history->num_connections = num_connections;
      history->transmitted_bytes = transmitted_bytes;
      history->connected_seconds = connected_seconds;
      history->num_requests = total_received_requests;
      history->rtt_events = rtt_events;
      history->average_rtt = average_rtt;
      history->approximate_rate = approximate_rate;
      history->average_serviced_byte_limit = average_serviced_byte_limit;
      history->average_service_bytes = average_service_bytes;
    }

  // Remove from list of threads for this source
  assert(source != NULL);
  if (prev == NULL)
    {
      assert(source->threads == this);
      source->threads = next;
    }
  else
    prev->next = next;
  if (next != NULL)
    next->prev = prev;

  // Add to list of dead threads in manager
  assert(manager->num_threads > 0);
  manager->num_threads--;
  next = manager->dead_threads;
  manager->dead_threads = this;
  prev = NULL;

  // See if we should remove the source from the manager's list of sources
  if (source->threads == NULL)
    {
      source->unlink();
      delete source;
    }
  source = NULL;

  // Signal and release manager
  manager->signal_event(); // Might be waiting for one or more threads to die
  manager->release_lock();
}

/*****************************************************************************/
/*                     kd_source_thread::install_channel                     */
/*****************************************************************************/

bool
  kd_source_thread::install_channel(bool auxiliary, kd_tcp_channel *channel,
                                    kd_request_queue *transfer_queue,
                                    kd_connection_server *cserver)
{
  if (auxiliary)
    {
      assert(transfer_queue == NULL);
      if ((!waiting_for_auxiliary_channel) || (auxiliary_channel != NULL))
        return false;
      channel->change_event(NULL); // Break any previous dependencies
      auxiliary_channel = channel;
    }
  else
    {
      if ((!waiting_for_primary_channel) || (primary_channel != NULL))
        return false;
      if (transfer_queue != NULL)
        primary_holding_queue.transfer_state(transfer_queue);
      channel->change_event(NULL); // Break any previous dependencies
      assert((this->cserver == NULL) || (this->cserver == cserver));
      this->cserver = cserver;
      primary_channel = channel;
    }
  channel->start_timer(0,false,NULL); // Disable any timeout or closure flag
  signal_event();
  return true;
}

/*****************************************************************************/
/*                       kd_source_thread::thread_entry                      */
/*****************************************************************************/

void
  kd_source_thread::thread_entry()
{
  bool ht_variant = false;
  if (jpip_channel_type == KD_CHANNEL_HTTP_TCP)
    ht_variant = true;

  // Initialize the `kdu_serve' base object
  prefix_bytes = (ht_variant)?8:0;
  try {
      initialize(&source->serve_target,max_chunk_bytes,prefix_bytes,
                 ignore_relevance_info);
    }
  catch (int exc) {
      source->failed = destruction_scheduled = true;
      throw exc;
    }

  // Now enter the service loop
  if (ht_variant)
    http_tcp_service_loop();
  else
    http_service_loop();

  // Now recycle channels if possible
  if (cserver != NULL)
    {
      if ((http_transmitter != NULL) && (request_channel == NULL))
        request_channel = http_transmitter->close(&pending_requests,h_event);
      if (request_channel != NULL)
        {
          cserver->push_active_channel(request_channel,pending_requests);
          request_channel = NULL;
        }
      if ((primary_channel != NULL) && (primary_channel->is_open()))
        {
          cserver->push_active_channel(primary_channel,primary_holding_queue);
          primary_channel = NULL;
        }
    }
}

/*****************************************************************************/
/*                   kd_source_thread::http_tcp_service_loop                 */
/*****************************************************************************/

void
  kd_source_thread::http_tcp_service_loop()
{
  kds_chunk *pending_head=NULL, *pending_tail=NULL, *new_chunks=NULL, *chnk;
  kdu_window new_window, current_window;
  int new_max_bytes=INT_MAX, current_max_bytes=INT_MAX;
  int remaining_max_bytes;
  int num_unsent_terminators = 0;
  bool need_target_id=false, align=false, extended_headers=false;
  int roi_index = 0;
  const char *content_type = NULL;
  bool window_done = true, window_changed = false;
  bool jpip_cclose = false; // True if the JPIP session is to be closed after
                            // responding to the current request.
  bool close_after_response = false; // True if channel to be closed after
                                     // responding to the current request.

  while (1)
    {
      if (waiting_for_primary_channel && (primary_channel != NULL))
        {
          primary_channel->change_event(h_event);
          pending_requests.init();
          pending_requests.transfer_state(&primary_holding_queue);
          close_after_response = false;
          waiting_for_primary_channel = false;
          request_channel = primary_channel;
          primary_channel = NULL;
          num_connections++;
        }
      if (waiting_for_auxiliary_channel && (auxiliary_channel != NULL))
        {
          auxiliary_channel->change_event(NULL);
          waiting_for_auxiliary_channel = false;
          tcp_transmitter =
            delivery_manager->get_tcp_transmitter(auxiliary_channel);
          auxiliary_channel = NULL;
        }

      if ((new_chunks == NULL) && (!jpip_cclose) &&
          pending_requests.have_request(!window_done))
        { // Process window requests all at once now
          window_changed =
            process_window_requests(new_window,new_max_bytes,
                                    need_target_id,align,extended_headers,
                                    jpip_cclose,num_unsent_terminators,
                                    roi_index,&close_after_response);
          if (close_after_response && (!is_stateless) && (!jpip_cclose) &&
              waiting_for_auxiliary_channel)
            waiting_for_primary_channel = true; // Allow new channel to be
              // connected while we are still sending data on old channel, if
              // client is capable of doing this.
          content_type =
            (extended_headers)?"image/jpp-stream;ptype=ext":"image/jpp-stream";
        }

      if ((new_chunks == NULL) && (window_changed || !window_done))
        { // Generate a new list of chunks for transmission
          int suggested_bytes = 100; // No need to send much until the TCP
                                     // transmitter gets up and running
          if (tcp_transmitter != NULL)
            suggested_bytes = tcp_transmitter->get_suggested_bytes();

          if (window_changed)
            {
              current_max_bytes = new_max_bytes;
              if (current_max_bytes < (max_chunk_bytes - 2))
                current_max_bytes = max_chunk_bytes - 2;
              remaining_max_bytes = current_max_bytes;
              set_window(new_window);
              total_serviced_requests++;
              if (!window_done)
                num_unsent_terminators++;
            }
          for (; num_unsent_terminators > 0; num_unsent_terminators--)
            { // Include terminators at front of data
              extra_data_buf[0] = 0; // Termination code
              extra_data_buf[1] = JPIP_EOR_WINDOW_CHANGE;
              extra_data_buf[2] = 0; // No extra bytes
              push_extra_data(extra_data_buf,3);
            }
          try {
              new_chunks =
                generate_increments(suggested_bytes,remaining_max_bytes,
                                    align,extended_headers);
              window_done = false;
              if ((window_done = !get_window(current_window)) ||
                  (remaining_max_bytes < 32)) // Don't process if byte limit
                                              // gets ridiculously small
                { // Append a null terminator
                  extra_data_buf[0] = 0; // Termination code
                  if (get_image_done())
                    extra_data_buf[1] = JPIP_EOR_IMAGE_DONE;
                  else if (window_done)
                    extra_data_buf[1] = JPIP_EOR_WINDOW_DONE;
                  else
                    extra_data_buf[1] = JPIP_EOR_BYTE_LIMIT_REACHED;
                  extra_data_buf[2] = 0; // No extra bytes
                  push_extra_data(extra_data_buf,3,new_chunks);
                  window_done = true;
                }
              if (window_changed)
                {
                  generate_window_reply(new_window,current_window,
                                        new_max_bytes,current_max_bytes,
                                        need_target_id,roi_index,
                                        close_after_response);
                    // Augments text in `reply_block' from processing requests
                  kd_msg_log.print(reply_block,"\t>> ");
                  request_channel->write_block(true,reply_block);
                  reply_block.restart();
                  window_changed = false;
                }
            }
          catch (int) {
              source->failed = destruction_scheduled = true;
              reply_block << "HTTP/1.1 500 Server error condition generated "
                             "while processing target image.\r\n"
                          << "Connection: close\r\n\r\n";
              kd_msg_log.print(reply_block,"\t>> ");
              window_changed = false;
              window_done = true;
              close_after_response = true;
              jpip_cclose = true;
                // Make sure we don't try to process any more requests
            }
        }

      if (reply_block.get_remaining_bytes() > 0)
        { // Send any built up error messages immediately
          kd_msg_log.print(reply_block,"\t>> ");
          request_channel->write_block(true,reply_block);
          reply_block.restart();
        }

      while ((tcp_transmitter != NULL) &&
             ((chnk = tcp_transmitter->retrieve_completed_chunks()) != NULL))
        {
          assert(chnk == pending_head);
          pending_head = pending_head->next;
          if (pending_head == NULL)
            pending_tail = NULL;
          chnk->next = NULL;
          release_chunks(chnk);
        }

      if (close_after_response && (request_channel != NULL))
        {
          request_channel->close();
          request_channel = NULL;
          close_after_response = false;
          if (!jpip_cclose)
            waiting_for_primary_channel = true;
              // Allow reconnection of primary channel, subject to timeout.
        }

      while ((request_channel != NULL) && (!jpip_cclose) &&
             pending_requests.read_request(false,request_channel));
        /* Making the above calls immediately before pushing chunks
           ensures that the `h_event' object will be signalled if a
           new request becomes available, waking up a blocked `push_chunk'
           call if necessary. */

      bool push_chunk_called = false;
      if (((chnk=new_chunks) != NULL) && (tcp_transmitter != NULL))
        {
          if (tcp_transmitter->push_chunk(chnk,h_event,content_type))
            {
              assert(chnk->num_bytes > 0);
              new_chunks = chnk->next;
              chnk->next = NULL;
              if (pending_tail == NULL)
                pending_head = pending_tail = chnk;
              else
                pending_tail = pending_tail->next = chnk;
            }
          push_chunk_called = true;
        }

      if ((new_chunks == NULL) && window_done && jpip_cclose)
        return; // Will finish the service and recycle channels if possible

      if (push_chunk_called)
        { // There may be more chunks to push in, or else we may have
          // had to wait inside `tcp_transmitter->push_chunk' for the
          // `h_event' object to be signalled.  In either case, we cannot
          // safely wait for `h_event' to be signalled again until after
          // we have made another call to `pending_requests.read_request'.
          reset_event();
          continue;
        }

      kdu_uint32 milliseconds_left = 1;
      if (tcp_transmitter != NULL)
        {
          if (!tcp_transmitter->is_open())
            return; // Auxiliary TCP channel has died; cannot be reconnected
          milliseconds_left = tcp_transmitter->get_time_left();
          if (milliseconds_left == 0)
            return; // Connection has timed out.
          if ((new_chunks != NULL) || (!window_done) ||
              pending_requests.have_request(false))
            continue; // Still more work to do; no need to wait for anything
        }
      if (!((waiting_for_primary_channel && (primary_channel!=NULL)) ||
            (waiting_for_auxiliary_channel && (auxiliary_channel!=NULL))))
        { // No connections are ready to be installed yet and no data is
          // left to be sent, so wait until something happens
          if (waiting_for_primary_channel || waiting_for_auxiliary_channel)
            { // Limit timeout when waiting for channel connection
              if (WaitForSingleObject(h_event,
                                      completion_timeout) != WAIT_OBJECT_0)
                return; 
            }
          else
            wait_event(milliseconds_left); // Don't wait beyond session timeout
        }
    }
}

/*****************************************************************************/
/*                     kd_source_thread::http_service_loop                   */
/*****************************************************************************/

void
  kd_source_thread::http_service_loop()
{
  kds_chunk *pending_head=NULL, *pending_tail=NULL, *new_chunks=NULL, *chnk;
  kdu_window new_window, current_window;
  int new_max_bytes=INT_MAX, current_max_bytes=INT_MAX;
  int remaining_max_bytes;
  bool need_target_id=false, align=false, extended_headers=false;
  int roi_index = 0;
  const char *content_type=NULL;
  bool window_done = true, window_changed = false;
  bool jpip_cclose = false; // True if JPIP session is to be closed after
                            // responding to the current request.
  bool close_after_response = false; // True if channel to be closed after
                                     // responding to the current request.
  bool release_channel = false; // True if channel contains an unrelated
      // request, meaning that it should be returned to the connection server
      // for re-assignment, once the current request has been served.

  while (1)
    {
      if (waiting_for_primary_channel && (primary_channel != NULL) &&
          ((http_transmitter == NULL) || (!http_transmitter->is_open())))
        {
          primary_channel->change_event(h_event);
          close_after_response = release_channel = false;
          pending_requests.init();
          const kd_request *top_req = primary_holding_queue.pop_head(NULL);
          if (top_req != NULL)
            pending_requests.push_copy(top_req);
          if (http_transmitter == NULL)
            http_transmitter =
              delivery_manager->get_http_transmitter(primary_channel,
                                                     &primary_holding_queue);
          else
            http_transmitter->reopen(primary_channel,&primary_holding_queue);
          assert(!primary_holding_queue.have_request(false));
          waiting_for_primary_channel = false;
          primary_channel = NULL;
          num_connections++;
        }

      if ((new_chunks == NULL) && (!close_after_response) &&
          (!release_channel) && (!jpip_cclose) &&
          (window_done || !is_stateless) &&
          pending_requests.have_request(!window_done))
        { // Process window requests all at once now.  Note that in the
          // stateless mode, we do not process a new request until the
          // previous request has been completely serviced.
          int unsent_terminators; // Won't use this with the "http" transport
          window_changed =
            process_window_requests(new_window,new_max_bytes,
                                    need_target_id,align,extended_headers,
                                    jpip_cclose,unsent_terminators,
                                    roi_index,&close_after_response,
                                    (is_stateless)?(&release_channel):NULL);
          if ((close_after_response || release_channel) &&
              (!is_stateless) && (!jpip_cclose))
            waiting_for_primary_channel = true; // Allow new channel to be
              // connected while we are still sending data on old channel, if
              // client is capable of doing this.
          content_type =
            (extended_headers)?"image/jpp-stream;ptype=ext":"image/jpp-stream";
        }

      if ((new_chunks == NULL) && (http_transmitter != NULL) &&
          (window_changed || !window_done))
        { // Generate a new list of chunks for transmission
          int suggested_bytes = http_transmitter->get_suggested_bytes();
          if (window_changed)
            {
              current_max_bytes = new_max_bytes;
              if (current_max_bytes < max_chunk_bytes)
                current_max_bytes = max_chunk_bytes;
              remaining_max_bytes = current_max_bytes;
              set_window(new_window);
              total_serviced_requests++;
              if (new_max_bytes < (1<<24)) // Ridiculously large limit
                {
                  num_serviced_with_byte_limits++;
                  cumulative_byte_limit += new_max_bytes;
                }
            }
          try {
              new_chunks =
                generate_increments(suggested_bytes,remaining_max_bytes,
                                    align,extended_headers);
              window_done = false;
              if ((window_done = !get_window(current_window)) ||
                  (remaining_max_bytes < 32))
                      // Don't process if byte limit gets ridiculously small
                { // Append a null terminator
                  extra_data_buf[0] = 0; // Termination code
                  if (get_image_done())
                    extra_data_buf[1] = JPIP_EOR_IMAGE_DONE;
                  else if (window_done)
                    extra_data_buf[1] = JPIP_EOR_WINDOW_DONE;
                  else
                    extra_data_buf[1] = JPIP_EOR_BYTE_LIMIT_REACHED;
                  extra_data_buf[2] = 0; // No extra bytes
                  push_extra_data(extra_data_buf,3,new_chunks);
                  window_done = true;
                }
              if (window_changed)
                {
                  generate_window_reply(new_window,current_window,
                                        new_max_bytes,current_max_bytes,
                                        need_target_id,roi_index,
                                        close_after_response);
                   // Augments text in `reply_block' from processing requests
                  window_changed = false;
                }
            }
          catch (int) {
              source->failed = destruction_scheduled = true;
              reply_block << "HTTP/1.1 500 Server error condition generated "
                             "while processing target image.\r\n"
                          << "Connection: close\r\n\r\n";
              kd_msg_log.print(reply_block,"\t>> ");
              jpip_cclose = true;
              window_done = true;
              window_changed = false;
              close_after_response = true; // Make sure we don't try to process
                                           // any further requests.
            }
        }

      while ((http_transmitter != NULL) &&
             ((chnk = http_transmitter->retrieve_completed_chunks()) != NULL))
        {
          assert(chnk == pending_head);
          pending_head = pending_head->next;
          if (pending_head == NULL)
            pending_tail = NULL;
          chnk->next = NULL;
          release_chunks(chnk);
        }

      if (reply_block.get_remaining_bytes() > 0)
        { // Send any accumulated replies immediately
          if (http_transmitter->push_replies(reply_block,h_event,
                                             (new_chunks != NULL)))
            reply_block.restart();
        }
      else
        { // Push chunks in only after pushing in replies
          while (((chnk=new_chunks) != NULL) &&
                 http_transmitter->push_chunk(chnk,h_event,content_type))
            {
              assert(chnk->num_bytes > 0);
              new_chunks = chnk->next;
              chnk->next = NULL;
              if (pending_tail == NULL)
                pending_head = pending_tail = chnk;
              else
                pending_tail = pending_tail->next = chnk;
              if ((new_chunks == NULL) && window_done)
                http_transmitter->terminate_chunked_data();
            }
        }

      if ((new_chunks == NULL) && window_done)
        {
          assert(request_channel == NULL);
          if (close_after_response || jpip_cclose || release_channel)
            {
              request_channel =
                http_transmitter->close(&pending_requests,h_event);
              if (close_after_response && (request_channel != NULL) &&
                  (cserver != NULL))
                {
                  cserver->push_active_channel(request_channel,
                                               pending_requests);
                  request_channel = NULL;
                }
              if (jpip_cclose)
                return; // Will finish the service and recycle channels if any
              if (release_channel)
                {
                  if ((cserver == NULL) || (request_channel == NULL))
                    return;
                  cserver->push_active_channel(request_channel,
                                               pending_requests);
                  request_channel = NULL;
                }
            }
        }

      if ((http_transmitter != NULL) && http_transmitter->is_open())
        {
          http_transmitter->retrieve_requests(&pending_requests,h_event);
            // Calling the above function right before we enter a wait ensures
            // that we leave behind our wake-up event for the delivery thread
            // to signal if a new message arrives (or the connection is closed)

          kdu_uint32 milliseconds_left = http_transmitter->get_time_left();
          if (milliseconds_left == 0)
            return; // Will automatically close down the connection
          if ((new_chunks == NULL) && window_done &&
              (reply_block.get_remaining_bytes() == 0) &&
              !pending_requests.have_request(false))
            wait_event(milliseconds_left);
        }
      else if (waiting_for_primary_channel && (primary_channel == NULL))
        { // Still waiting for channels to be connected
          if (WaitForSingleObject(h_event,completion_timeout) != WAIT_OBJECT_0)
            return;
        }
      else if (primary_channel == NULL)
        return; // Channel closed and cannot be re-opened
    }
}

/*****************************************************************************/
/*                  kd_source_thread::process_window_requests                */
/*****************************************************************************/

bool
  kd_source_thread::process_window_requests(kdu_window &window, int &max_bytes,
                    bool &need_target_id, bool &align, bool &extended_headers,
                    bool &jpip_cclose, int &num_preempted_requests,
                    int &roi_index, bool *close_after_response,
                    bool *release_channel)
{
  const kd_request *req=NULL;
  bool have_valid_window = false;
  bool is_overridden = false;

  if (close_after_response != NULL)
    *close_after_response = false;
  roi_index = 0;
  reply_block.restart();
  while (((close_after_response == NULL) || !(*close_after_response)) &&
         ((release_channel == NULL) || !(*release_channel)) &&
         ((req=pending_requests.pop_head(&is_overridden)) != NULL))
    {
      if (req->resource == NULL)
        {
          reply_block << "HTTP/1.1 405 Unacceptable request method, \""
                      << req->method << "\"\r\n\r\n";
          continue;
        }

      if ((req->fields.admin_key!=NULL) || (req->fields.admin_command!=NULL))
        {
          if (release_channel != NULL)
            {
              *release_channel = true;
              pending_requests.replace_head(req);
              break;
            }
          reply_block << "HTTP/1.1 400 Unacceptable use of admin request "
                         "fields over same TCP channel as ongoing interaction "
                         "with an image\r\n\r\n";
          continue;
        }

      bool incompatible = false;
      if (is_stateless || (req->fields.channel_new != NULL))
        { // Stateless request: target must agree
          if ((req->fields.target == NULL) ||
              (strcmp(req->fields.target,source->target.filename) != 0))
            incompatible = true;
          else if (req->fields.sub_target == NULL)
            incompatible = (source->target.byte_range[0] != '\0');
          else
            incompatible =
              (strcmp(req->fields.sub_target,source->target.byte_range) != 0);
        }
      else
        { // Stateful request: channel must agree
          if (req->fields.channel_id != NULL)
            incompatible = (strcmp(req->fields.channel_id,channel_id) != 0);
          else if (req->fields.channel_close != NULL)
            incompatible = (strcmp(req->fields.channel_close,channel_id) != 0);
          else
            incompatible = true;
        }

      if (incompatible)
        {
          if (release_channel != NULL)
            {
              *release_channel = true;
              pending_requests.replace_head(req);
              break;
            }
          reply_block << "HTTP/1.1 409 Proxy or client appears to be "
            "re-using a single persistent TCP channel to send unrelated "
            "requests.  This is legal, but not supported.\r\n"
                      << "Server: Kakadu JPIP Server "
                      << KDU_CORE_VERSION << "\r\n\r\n";
          continue;
        }

      // If we get here, we are committed to not releasing the channel
      
      if (req->fields.channel_new != NULL)
        jpip_cnew_header_required = true;
  
      if (have_valid_window)
        { // Previous request being pre-empted.
          num_preempted_requests++;
          have_valid_window = false;
          reply_block << "HTTP/1.1 202 pre-empted\r\n";
          if (need_target_id)
            reply_block << "JPIP-" JPIP_FIELD_TARGET_ID ": "
                        << source->target_id << "\r\n";
          if (jpip_cnew_header_required)
            {
              assert(!is_stateless);
              reply_block << "JPIP-" JPIP_FIELD_CHANNEL_NEW ": "
                << "cid=" << channel_id
                << ",path=" KD_MY_RESOURCE_NAME;
              if (jpip_channel_type == KD_CHANNEL_HTTP)
                reply_block << ",transport=http";
              else
                reply_block << ",transport=http-tcp,auxport=" << listen_port;
              reply_block << "\r\n";
              jpip_cnew_header_required = false;
            }
          reply_block << "\r\n";
        }
      window.init();

      if (close_after_response != NULL)
        *close_after_response = req->close_connection;
      
      // Perform preliminary checks to make sure request can be processed
      if (req->fields.unrecognized != NULL)
        {
          reply_block << "HTTP/1.1 400 Unrecognized request field, \""
                      << req->fields.unrecognized << "\"\r\n\r\n";
          continue;
        }

      if (req->fields.upload != NULL)
        {
          reply_block << "HTTP/1.1 501 Upload functionality not "
                         "implemented.\r\n"
                      << "Server: Kakadu JPIP Server "
                      << KDU_CORE_VERSION << "\r\n\r\n";
          continue;
        }

      if ((req->fields.xpath_bin != NULL) ||
          (req->fields.xpath_box != NULL) ||
          (req->fields.xpath_query != NULL))
        {
          reply_block << "HTTP/1.1 501 Xpath functionality not "
                         "implemented.\r\n"
                      << "Server: Kakadu JPIP Server "
                      << KDU_CORE_VERSION << "\r\n\r\n";
          continue;
        }

      if (req->fields.channel_close != NULL)
        {
          if ((strcmp(req->fields.channel_close,channel_id) == 0) ||
              (strcmp(req->fields.channel_close,"*") == 0))
            jpip_cclose = true;
        }

      align = (req->fields.align != NULL) &&
        (strcmp(req->fields.align,"yes") == 0);
      need_target_id = (req->fields.target_id != NULL) &&
        (strcmp(req->fields.target_id,source->target_id) != 0);
      max_bytes = INT_MAX;
      if ((req->fields.max_length != NULL) &&
          ((sscanf(req->fields.max_length,"%d",&max_bytes) == 0) ||
           (max_bytes < 0)))
        {
          reply_block << "HTTP/1.1 400 Malformed \"" JPIP_FIELD_MAX_LENGTH
            << "=\" request body.  Need non-negative integer.\r\n\r\n.";
          continue;
        }

      int val1, val2;
      const char *scan, *delim;
      if (req->fields.type != NULL)
        {
          for (scan=req->fields.type; *scan != '\0'; scan=delim)
            {
              while (*scan == ',') scan++;
              for (delim=scan;
                   (*delim != ',') && (*delim != ';') && (*delim != '\0');
                   delim++);
              if ((((delim-scan) == 1)  && (*scan == '*')) ||
                  (((delim-scan) == 10) &&
                   (strncmp(scan,"jpp-stream",10) == 0)) ||
                  (((delim-scan) == 16) &&
                   (strncmp(scan,"image/jpp-stream",16)==0)))
                break;
              if (*delim == ';')
                while ((*delim != ',') && (*delim != '\0'))
                  delim++;
            }
          if (*scan == '\0')
            {
              reply_block << "HTTP/1.1 406 Requested image return types "
                "are not acceptable; can only return \"jpp-stream\".\r\n\r\n.";
              continue;
            }
          extended_headers = false;
          if (*delim == ';')
            {
              for (scan=delim+1; (*delim != ',') && (*delim != '\0'); delim++);
              if (((delim-scan) == 9) && (strncmp(scan,"ptype=ext",9) == 0))
                extended_headers = true;
            }
        }
      if (req->fields.full_size != NULL)
        {
          if ((sscanf(req->fields.full_size,"%d,%d",&val1,&val2) != 2) ||
              (val1 <= 0) || (val2 <= 0))
            {
              reply_block << "HTTP/1.1 400 Malformed \"" JPIP_FIELD_FULL_SIZE
                "=\" request body.  Need two positive integers.\r\n\r\n";
              continue;
            }
          window.resolution.x = val1;
          window.resolution.y = val2;
          for (scan=req->fields.full_size; *scan != ','; scan++);
          for (scan++; (*scan != ',') && (*scan != '\0'); scan++);
          window.round_direction = -1; // Default is round-down
          if (strcmp(scan,",round-up") == 0)
            window.round_direction = 1;
          else if (strcmp(scan,",closest") == 0)
            window.round_direction = 0;
          else if (strcmp(scan,",round-down") == 0)
            window.round_direction = -1;
          else if (*scan != '\0')
            {
              reply_block << "HTTP/1.1 400 Malformed \"" JPIP_FIELD_FULL_SIZE
                "=\" request body.  Unrecognizable round-direction.\r\n\r\n";
              continue;
            }
        }
      else if ((req->fields.region_offset != NULL) ||
               (req->fields.region_size != NULL))
        {
          reply_block << "HTTP/1.1 400 Malformed JPIP request.  Neither the "
                         "\"" JPIP_FIELD_REGION_OFFSET "\", nor the "
                         "\"" JPIP_FIELD_REGION_SIZE "\" request field may "
                         "appear in a request which does not contain a "
                         "\"" JPIP_FIELD_FULL_SIZE "\" request field.\r\n\r\n";
          continue;
        }
      if (req->fields.region_size != NULL)
        {
          if ((sscanf(req->fields.region_size,"%d,%d",&val1,&val2) != 2) ||
              (val1 <= 0) || (val2 <= 0))
            {
              reply_block << "HTTP/1.1 400 Malformed \"" JPIP_FIELD_REGION_SIZE
                "=\" request body.  Need two positive integers.\r\n\r\n";
              continue;
            }
          window.region.size.x = val1;
          window.region.size.y = val2;
        }
      else
        window.region.size = window.resolution;
      if (req->fields.region_offset != NULL)
        {
          if ((sscanf(req->fields.region_offset,"%d,%d",&val1,&val2) != 2) ||
              (val1 < 0) || (val2 < 0))
            {
              reply_block<<"HTTP/1.1 400 Malformed \"" JPIP_FIELD_REGION_OFFSET
                "=\" request body.  Need two non-negative integers.\r\n\r\n";
              continue;
            }
          window.region.pos.x = val1;
          window.region.pos.y = val2;
        }
      if (req->fields.layers != NULL)
        {
          if ((sscanf(req->fields.layers,"%d",&val1) != 1) || (val1 < 0))
            {
              reply_block<<"HTTP/1.1 400 Malformed \"" JPIP_FIELD_LAYERS
                "=\" request body.  Need non-negative integer.\r\n\r\n";
              continue;
            }
          window.max_layers = val1;
        }
      if (req->fields.components != NULL)
        {
          bool failure = false;
          char *end_cp;
          int from, to;
          for (scan=req->fields.components; (*scan!='&') && (*scan!='\0'); )
            {
              while (*scan == ',')
                scan++;
              from = to = strtol(scan,&end_cp,10);
              if (end_cp == scan)
                { failure = true; break; }
              scan = end_cp;
              if (*scan == '-')
                {
                  scan++;
                  to = strtol(scan,&end_cp,10);
                  if (end_cp == scan)
                    to = INT_MAX;
                  scan = end_cp;
                }
              if (((*scan != ',') && (*scan != '&') && (*scan != '\0')) ||
                  (from < 0) || (from > to))
                { failure = true; break; }
              window.components.add(from,to);
            }
          if (failure)
            {
              reply_block << "HTTP/1.1 400 Malformed \"" JPIP_FIELD_COMPONENTS
                "=\" request body.\r\n\r\n";
              continue;
            }
        }
      if (req->fields.codestreams != NULL)
        {
          bool failure = false;
          char *end_cp;
          kdu_sampled_range range;
          for (scan=req->fields.codestreams; (*scan!='&') && (*scan!='\0'); )
            {
              while (*scan == ',')
                scan++;
              range.step = 1;
              range.from = range.to = strtol(scan,&end_cp,10);
              if (end_cp == scan)
                { failure = true; break; }
              scan = end_cp;
              if (*scan == '-')
                {
                  scan++;
                  range.to = strtol(scan,&end_cp,10);
                  if (end_cp == scan)
                    range.to = INT_MAX;
                  scan = end_cp;
                }
              if (*scan == ':')
                {
                  scan++;
                  range.step = strtol(scan,&end_cp,10);
                  if (end_cp == scan)
                    { failure = true; break; }
                  scan = end_cp;
                }
              if (((*scan != ',') && (*scan != '&') && (*scan != '\0')) ||
                  (range.from < 0) || (range.from > range.to) ||
                  (range.step < 1))
                { failure = true; break; }
              window.codestreams.add(range);
            }
          if (failure)
            {
              reply_block << "HTTP/1.1 400 Malformed \"" JPIP_FIELD_CODESTREAMS
                "=\" request body.\r\n\r\n";
              continue;
            }
        }

      int default_codestream_id =
        (window.codestreams.is_empty())?0:window.codestreams.get_range(0).from;

      if (req->fields.contexts != NULL)
        {
          for (scan=req->fields.contexts; (*scan!='&') && (*scan!='\0'); )
            {
              while (*scan == ',') scan++;
              scan = window.parse_context(scan);
              if ((*scan != ',') && (*scan != '&') && (*scan != '\0'))
                {
                  reply_block <<"HTTP/1.1 400 Malformed \"" JPIP_FIELD_CONTEXTS
                    "=\" request body.\r\n\r\n";
                  continue;
                }
            }
        }

      roi_index = 0;
      if (req->fields.roi != NULL)
        {
          roi_index = source->serve_target.find_roi(default_codestream_id,
                                                    req->fields.roi);
          if (roi_index == 0)
            roi_index = -1; // Indicate need for a "JPIP-roi:" response header
          else
            source->serve_target.get_roi_details(roi_index,window.resolution,
                                                 window.region);
        }

      if (req->fields.meta_request != NULL)
        {
          const char *failure = window.parse_metareq(req->fields.meta_request);
          if (failure != NULL)
            {
              reply_block <<"HTTP/1.1 400 Malformed \"" JPIP_FIELD_META_REQUEST
                "=\" request body.\r\n\r\n";
              continue;
            }
        }

      // At this point we are committed to processing the window request
      total_received_requests++;
      set_window(window); // Set window to base `kdu_serve' object

      if (is_stateless)
        { // Stateless communications may not use any prior cache knowledge
          for (int cls=0; cls < KDU_NUM_DATABIN_CLASSES; cls++)
            truncate_cache_model(cls,0,INT_MAX,-1,0);
        }

      if ((req->fields.need != NULL) && is_stateless && !need_target_id)
        {
          for (int cls=0; cls < KDU_NUM_DATABIN_CLASSES; cls++)
            augment_cache_model(cls,0,INT_MAX,-1,-1);
          if (!process_cache_specifiers(req->fields.need,
                                        default_codestream_id,true))
            {
              reply_block << "HTTP/1.1 400 Malformed \"" JPIP_FIELD_NEED
                "=\" request body.\r\n\r\n";
              continue;
            }
        }
      else if ((req->fields.model != NULL) && !need_target_id)
        {
          if (!process_cache_specifiers(req->fields.model,
                                        default_codestream_id))
            {
              reply_block << "HTTP/1.1 400 Malformed \"" JPIP_FIELD_MODEL
                "=\" request body.\r\n\r\n";
              continue;
            }
        }

      have_valid_window = true;

      // We have successfully parsed a request; are we done?
      if (jpip_cclose || !is_overridden)
        break;
    }
  return have_valid_window;
}

/*****************************************************************************/
/*                 kd_source_thread::process_cache_specifiers                */
/*****************************************************************************/

bool
  kd_source_thread::process_cache_specifiers(const char *string,
                                             int default_codestream_id,
                                             bool need)
{
  const char *qualifier=NULL; // Points to the last code-stream qualifier

  while (*string != '\0')
    {
      while (*string == ',') string++;
      if (*string == '[')
        {
          qualifier = string+1;
          while ((*string != '\0') && (*string != ','))
            string++;
          if (string[-1] != ']')
            return false;
          continue;
        }

      bool positive = !need;
      if (*string == '-')
        {
          if (need)
            return false; // Can't have negative descriptors in "need="
          string++;
          positive = false;
        }

      int cls=-1; // Data-bin class code
      int tmin, tmax, cmin, cmax, rmin, rmax, pmin, pmax;
      kdu_long bin_id=-1;

      if (*string == 'H')
        {
          string++;
          if (*string == 'm')
            {
              string++;
              cls = KDU_MAIN_HEADER_DATABIN;
              bin_id = 0;
            }
          else
            {
              cls = KDU_TILE_HEADER_DATABIN;
              if (!read_val_or_wild(string,bin_id))
                return false;
            }
        }
      else if (*string == 'P')
        {
          string++;
          cls = KDU_PRECINCT_DATABIN;
          if (!read_val_or_wild(string,bin_id))
            return false;
        }
      else if (*string == 'M')
        {
          string++;
          cls = KDU_META_DATABIN;
          if (!read_val_or_wild(string,bin_id))
            return false;
        }
      else if (*string == 'T')
        {
          string++;
          cls = KDU_TILE_DATABIN;
          if (!read_val_or_wild(string,bin_id))
            return false;
        }
      else if ((*string=='t')||(*string=='c')||(*string=='r')||(*string=='p'))
        { // Scan for an implicit descriptor
          tmin=tmax=cmin=cmax=rmin=rmax=pmin=pmax=-1;
          while (1)
            {
              if (*string=='t')
                {
                  string++;
                  if (!read_val_range_or_wild(string,tmin,tmax))
                    return false;
                }
              else if (*string=='c')
                {
                  string++;
                  if (!read_val_range_or_wild(string,cmin,cmax))
                    return false;
                }
              else if (*string=='r')
                {
                  string++;
                  if (!read_val_range_or_wild(string,rmin,rmax))
                    return false;
                }
              else if (*string=='p')
                {
                  string++;
                  if (!read_val_range_or_wild(string,pmin,pmax))
                    return false;
                }
              else
                break;
            }
          if ((tmin > tmax) || (cmin > cmax) || (rmin > rmax) || (pmin > pmax))
            return false;
        }
      else
        return false; // Unrecognized cache descriptor
      
      int available_bytes=-1, available_packets=0;
      if (*string == ':')
        {
          available_bytes = 0;
          string++;
          if (*string == 'L')
            {
              string++;
              if (!read_val_or_wild(string,available_packets))
                return false;
              if (available_packets < 0)
                available_bytes = -1; // Found wild-card
            }
          else if (!read_val_or_wild(string,available_bytes))
            return false;
        }
          
      const char *qp=qualifier;
      do {
          int smin, smax; // Code-stream ID range
          if (qp == NULL)
            smin = smax = default_codestream_id;
          else
            {
              if (!read_val_range_or_wild(qp,smin,smax))
                return false;
              if (*qp == ';')
                qp++;
              else if (*qp != ']')
                return false;
              if (smin > smax)
                return false;
            }
          if (positive)
            {
              if (cls < 0)
                augment_cache_model(smin,smax,
                                    tmin,tmax,cmin,cmax,rmin,rmax,pmin,pmax,
                                    available_bytes,available_packets);
              else
                augment_cache_model(cls,smin,smax,bin_id,
                                    available_bytes,available_packets);
            }
          else
            {
              if (cls < 0)
                truncate_cache_model(smin,smax,
                                     tmin,tmax,cmin,cmax,rmin,rmax,pmin,pmax,
                                     available_bytes,available_packets);
              else
                truncate_cache_model(cls,smin,smax,bin_id,
                                     available_bytes,available_packets);
            }
        } while ((qp != NULL) && (*qp != ']'));
    }

  return true;
}

/*****************************************************************************/
/*                   kd_source_thread::generate_window_reply                 */
/*****************************************************************************/

void
  kd_source_thread::generate_window_reply(kdu_window &requested,
                                          kdu_window &actual,
                                          int requested_max_bytes,
                                          int actual_max_bytes,
                                          bool need_target_id, int roi_index,
                                          bool close_after_response)
{
  reply_block << "HTTP/1.1 200 ";
  if (requested.equals(actual))
    reply_block << "OK\r\n";
  else
    reply_block << "OK, with modifications\r\n";
  
  if (need_target_id)
    reply_block << "JPIP-" JPIP_FIELD_TARGET_ID ": "
                << source->target_id << "\r\n";
  
  if (jpip_cnew_header_required)
    {
      assert(!is_stateless);
      reply_block << "JPIP-" JPIP_FIELD_CHANNEL_NEW ": "
                  << "cid=" << channel_id
                  << ",path=" KD_MY_RESOURCE_NAME;
      if (jpip_channel_type == KD_CHANNEL_HTTP)
        reply_block << ",transport=http";
      else
        reply_block << ",transport=http-tcp,auxport=" << listen_port;
      reply_block << "\r\n";
      jpip_cnew_header_required = false;
    }

  if (requested_max_bytes != actual_max_bytes)
    reply_block << "JPIP-" JPIP_FIELD_MAX_LENGTH ": "
                << actual_max_bytes << "\r\n";
  
  if (requested.max_layers != actual.max_layers)
    reply_block << "JPIP-" JPIP_FIELD_LAYERS ": "
                << actual.max_layers << "\r\n";

  if (((requested.resolution != actual.resolution) &&
       (requested.resolution.x != 0) &&
       (requested.resolution.y != 0)) ||
      (roi_index > 0))
    reply_block << "JPIP-" JPIP_FIELD_FULL_SIZE ": "
                << actual.resolution.x << "," << actual.resolution.y << "\r\n";
  if (((requested.region.pos != actual.region.pos) &&
       (requested.resolution.x != 0) &&
       (requested.resolution.y != 0)) ||
      (roi_index > 0))
    reply_block << "JPIP-" JPIP_FIELD_REGION_OFFSET ": "
                << actual.region.pos.x << "," << actual.region.pos.y << "\r\n";
  if (((requested.region.size != actual.region.size) &&
       !requested.region.is_empty()) ||
      (roi_index > 0))
    reply_block << "JPIP-" JPIP_FIELD_REGION_SIZE ": "
              << actual.region.size.x << "," << actual.region.size.y << "\r\n";
  if (roi_index < 0)
    reply_block << "JPIP-" JPIP_FIELD_ROI ": roi=no-roi\r\n";
  else if (roi_index > 0)
    {
      kdu_coords res;
      kdu_dims reg;
      const char *roi_name =
        source->serve_target.get_roi_details(roi_index,res,reg);
      assert(roi_name != NULL);
      reply_block << "JPIP-" JPIP_FIELD_ROI ": roi=" << roi_name
                  << ";fsiz=" << res.x << "," << res.y
                  << ";rsiz=" << reg.size.x << "," << reg.size.y
                  << ";roff=" << reg.pos.x << "," << reg.pos.y << "\r\n";
    }

  if ((requested.components != actual.components) &&
      !requested.components.is_empty())
    {
      kdu_sampled_range *rg;
      reply_block << "JPIP-" JPIP_FIELD_COMPONENTS ": ";
      for (int cn=0; (rg = actual.components.access_range(cn)) != NULL; cn++)
        {
          if (cn > 0)
            reply_block << ",";
          reply_block << rg->from;
          if (rg->to > rg->from)
            reply_block << "-" << rg->to;
        }
      reply_block << "\r\n";
    }

  bool need_codestream_header =
    (requested.codestreams != actual.codestreams);
  if (!(need_codestream_header || actual.contexts.is_empty()))
    { // See if any translated codestreams exist
      kdu_sampled_range *rg;
      for (int cn=0; (rg = actual.codestreams.access_range(cn)) != NULL; cn++)
        if (rg->context_type == KDU_JPIP_CONTEXT_TRANSLATED)
          { need_codestream_header = true; break; }
    }
  if (need_codestream_header)
    {
      kdu_sampled_range *rg;
      reply_block << "JPIP-" JPIP_FIELD_CODESTREAMS ": ";
      for (int cn=0; (rg = actual.codestreams.access_range(cn)) != NULL; cn++)
        {
          if (cn > 0)
            reply_block << ",";
          if (rg->context_type == KDU_JPIP_CONTEXT_TRANSLATED)
            reply_block << "<" << rg->remapping_ids[0]
                        << ":" << rg->remapping_ids[1] << ">";
          reply_block << rg->from;
          if (rg->to > rg->from)
            reply_block << "-" << rg->to;
          if ((rg->step > 1) && (rg->to > rg->from))
            reply_block << ":" << rg->step;
        }
      reply_block << "\r\n";
    }

  if (!actual.contexts.is_empty())
    {
      kdu_sampled_range *rg;
      bool range_written= false;
      for (int cn=0; (rg = actual.contexts.access_range(cn)) != NULL; cn++)
        {
          if ((rg->context_type != KDU_JPIP_CONTEXT_JPXL) &&
              (rg->context_type != KDU_JPIP_CONTEXT_MJ2T))
            continue;
          if (rg->expansion == NULL)
            continue;
          if (!range_written)
            reply_block << "JPIP-" JPIP_FIELD_CONTEXTS ": ";
          else
            reply_block << ";";
          range_written = true;
          if (rg->context_type == KDU_JPIP_CONTEXT_JPXL)
            reply_block << "jpxl";
          else if (rg->context_type == KDU_JPIP_CONTEXT_MJ2T)
            reply_block << "mj2t";
          else
            assert(0);
          reply_block << "<" << rg->from;
          if (rg->to > rg->from)
            reply_block << "-" << rg->to;
          if ((rg->step > 1) && (rg->to > rg->from))
            reply_block << ":" << rg->step;
          if ((rg->context_type == KDU_JPIP_CONTEXT_MJ2T) &&
              (rg->remapping_ids[1] == 0))
            reply_block << "+now";
          reply_block << ">";
          if (rg->context_type == KDU_JPIP_CONTEXT_JPXL)
            {
              if ((rg->remapping_ids[0] >= 0) && (rg->remapping_ids[1] >= 0))
                reply_block << "[s" << rg->remapping_ids[0]
                            << "i" << rg->remapping_ids[1] << "]";
            }
          else if (rg->context_type == KDU_JPIP_CONTEXT_MJ2T)
            {
              if (rg->remapping_ids[0] == 0)
                reply_block << "[track]";
              else if (rg->remapping_ids[0] == 1)
                reply_block << "[movie]";
            }
          reply_block << "=";
          kdu_range_set *expansion = rg->expansion;
          for (int en=0; (rg = expansion->access_range(en)) != NULL; en++)
            {
              if (en > 0)
                reply_block << ",";
              reply_block << rg->from;
              if (rg->to > rg->from)
                reply_block << "-" << rg->to;
              if ((rg->step > 1) && (rg->to > rg->from))
                reply_block << ":" << rg->step;
            }
        }
      if (range_written)
        reply_block << "\r\n";
    }
  
  if (!is_stateless)
    reply_block << "Cache-Control: no-cache\r\n";
  if (close_after_response)
    reply_block << "Connection: close\r\n";
  reply_block << "\r\n";
}


/* ========================================================================= */
/*                             kd_source_manager                             */
/* ========================================================================= */

/*****************************************************************************/
/*                    kd_source_manager::kd_source_manager                   */
/*****************************************************************************/

kd_source_manager::kd_source_manager(kd_delivery_manager *delivery_manager,
                                     int listen_port, int max_sources,
                                     int max_threads, int max_chunk_bytes,
                                     bool ignore_relevance_info,
                                     int phld_threshold,
                                     int per_client_cache,
                                     int max_history_records,
                                     int completion_timeout,
                                     const char *password,
                                     HANDLE shutdown_event,
                                     const char *cache_directory)
{
  this->delivery_manager = delivery_manager;
  this->listen_port = listen_port;
  this->max_sources = max_sources; this->max_threads = max_threads;
  this->max_chunk_bytes = max_chunk_bytes;
  this->ignore_relevance_info = ignore_relevance_info;
  this->phld_threshold = phld_threshold;
  this->per_client_cache = per_client_cache;
  this->max_history_records = max_history_records;
  this->completion_timeout = completion_timeout;
  num_sources = num_threads = 0; sources = NULL; dead_threads = NULL;
  total_transmitted_bytes = 0;
  num_history_records = total_clients = 0;
  completed_connections = terminated_connections = 0;
  history_head = history_tail = NULL;
  memset(admin_id,0,16);
  if (password != NULL)
    admin_cipher.set_key(password);
  h_mutex = CreateMutex(NULL,FALSE,NULL);
  h_event = CreateEvent(NULL,FALSE,FALSE,NULL); // Auto-reset
  this->shutdown_event = shutdown_event;
  shutdown_requested = false;
  this->cache_directory = cache_directory;
}

/*****************************************************************************/
/*                    kd_source_manager::~kd_source_manager                  */
/*****************************************************************************/

kd_source_manager::~kd_source_manager()
{
  acquire_lock();
  max_threads = max_sources = 0; // Prevent new threads being started
  release_lock();
  while (num_threads > 0)
    wait_event();
  assert(num_sources == 0);
  delete_dead_threads();
  CloseHandle(h_mutex);
  CloseHandle(h_event);
  while ((history_tail = history_head) != NULL)
    {
      history_head = history_tail->next;
      delete history_tail;
    }
}

/*****************************************************************************/
/*                   kd_source_manager::create_source_thread                 */
/*****************************************************************************/

bool
  kd_source_manager::create_source_thread(kd_target_description &target,
                                          const char *channel_type,
                                          char channel_id[],
                                          kd_message_block &explanation)
{
  kd_jpip_channel_type jpip_channel_type = KD_CHANNEL_STATELESS;
  if ((channel_type != NULL) && (*channel_type != '\0'))
    {
      const char *scan, *delim;
      for (scan=channel_type; *scan != '\0'; scan=delim)
        {
          while (*scan == ',') scan++;
          for (delim=scan; (*delim != '\0') && (*delim != ','); delim++);
          if (((delim-scan)==4) && kd_has_caseless_prefix(scan,"http"))
            { jpip_channel_type = KD_CHANNEL_HTTP; break; }
          if (((delim-scan)==8) && kd_has_caseless_prefix(scan,"http-tcp"))
            { jpip_channel_type = KD_CHANNEL_HTTP_TCP; break; }
        }
    }

  kd_source *src;
  acquire_lock();
  if (num_threads >= max_threads)
    {
      explanation << "503 Too many clients currently connected to server.";
      release_lock();
      return false;
    }
  for (src=sources; src != NULL; src=src->next)
    if ((strcmp(src->target.filename,target.filename) == 0) &&
        (src->target.range_start == target.range_start) &&
        (src->target.range_lim == target.range_lim))
      break;
  if (src == NULL)
    { // Need to open the image
      if (num_sources >= max_sources)
        {
          explanation << "503 Too many sources currently open on server.";
          release_lock();
          return false;
        }
      src = new kd_source;
      if (!src->open(target,explanation,phld_threshold,per_client_cache,
                     cache_directory))
        {
          delete src;
          release_lock();
          return false;
        }

      // Create the unique source identifier
      src->source_id =
        ((kdu_uint32 *)(&src))[0]; // Start with 32 LSB's of address
      kd_source *sscan;
      do {
          src->source_id ^= (kdu_uint32) rand();
          src->source_id ^= (kdu_uint32)(rand() << 16);
          for (sscan=sources; sscan != NULL; sscan=sscan->next)
            if (sscan->source_id == src->source_id)
              break;
        } while (sscan != NULL);

      // Link into the manager's list of active sources
      src->manager = this; // Link the new source in.
      src->next = sources;
      if (sources != NULL)
        sources->prev = src;
      sources = src;
      num_sources++;
    }
  
  // Now create the source thread, assigning a unique channel ID first
  kd_source_thread *thrd;
  do {
      kdu_uint32 client_id = (kdu_uint32)(num_threads + (num_threads<<16));
      client_id +=
        (kdu_uint32)(total_transmitted_bytes + (total_transmitted_bytes<<12));
      client_id ^= (kdu_uint32) rand();
      client_id ^= (kdu_uint32)(rand() << 11);
      channel_id[0] = 'J'; channel_id[1] = 'P'; channel_id[2] = 'H';
      channel_id[3] = (jpip_channel_type == KD_CHANNEL_HTTP)?'_':'T';
      write_hex_val32(channel_id+4,client_id);
      write_hex_val32(channel_id+12,src->source_id);
      channel_id[20] = '\0';
      for (thrd=src->threads; thrd != NULL; thrd=thrd->next)
        if (strcmp(thrd->channel_id,channel_id) == 0)
          break;
    } while (thrd != NULL); // There is a random component which ensures
                            // that we keep generating new service ID's.

  thrd = new kd_source_thread(src,this,delivery_manager,channel_id,
                              jpip_channel_type,listen_port,max_chunk_bytes,
                              ignore_relevance_info,completion_timeout);
  thrd->next = src->threads;
  if (src->threads != NULL)
    src->threads->prev = thrd;
  src->threads = thrd;
  num_threads++;
  total_clients++;

  release_lock();

  thrd->start();
  return true;
}

/*****************************************************************************/
/*                    kd_source_manager::install_channel                     */
/*****************************************************************************/

bool
  kd_source_manager::install_channel(const char *channel_id, bool auxiliary,
                                     kd_tcp_channel *channel,
                                     kd_request_queue *transfer_queue,
                                     kd_connection_server *cserver,
                                     kd_message_block &explanation)
{
  int length = (int) strlen(channel_id);
  if (length <= 8)
    {
      explanation << "404 Request involves an ivalid channel ID\r\n";
      return false;
    }
  kdu_uint32 source_id = read_hex_val32(channel_id+length-8);
  acquire_lock();
  kd_source *src;
  for (src=sources; src != NULL; src=src->next)
    if (src->source_id == source_id)
      break;
  if (src == NULL)
    {
      release_lock();
      explanation << "404 Requested channel ID no longer exists\r\n";
      return false;
    }
  kd_source_thread *thrd;
  for (thrd=src->threads; thrd != NULL; thrd=thrd->next)
    if (strcmp(thrd->channel_id,channel_id) == 0)
      break;
  if (thrd == NULL)
    {
      release_lock();
      explanation << "404 Requested channel ID no longer exists\r\n";
      return false;
    }
  bool success =
    thrd->install_channel(auxiliary,channel,transfer_queue,cserver);
  release_lock();
  if (!success)
    explanation << "409 Requested channel ID belongs to a session which "
                   "is already connected to a different TCP channel.\r\n";
  return success;
}

/*****************************************************************************/
/*                    kd_source_manager::write_admin_id                      */
/*****************************************************************************/

bool
  kd_source_manager::write_admin_id(kd_message_block &block)
{
  if (!admin_cipher)
    return false;
  acquire_lock();
  admin_cipher.encrypt(admin_id);
  for (int i=0; i < 32; i++)
    {
      kdu_byte val = admin_id[i>>1];
      if ((i & 1) == 0)
        val >>= 4;
      val &= 0x0F;
      if (val < 10)
        block << (char)('0'+val);
      else
        block << (char)('A'+val-10);
    }
  release_lock();
  return true;
}

/*****************************************************************************/
/*                   kd_source_manager::compare_admin_id                     */
/*****************************************************************************/

bool
  kd_source_manager::compare_admin_id(const char *string)
{
  if (!admin_cipher)
    return false;
  bool matches = true;
  acquire_lock();
  admin_cipher.encrypt(admin_id);
  for (int i=0; (i < 32) && matches; i++, string++)
    {
      kdu_byte val = admin_id[i>>1];
      if ((i & 1) == 0)
        val >>= 4;
      val &= 0x0F;
      if (((val < 10) && (*string != ('0'+val))) ||
          ((val >= 10) &&
           (*string != ('A'+val-10)) && (*string!= ('a'+val-10))))
        matches = false;
    }
  release_lock();
  return matches;
}

/*****************************************************************************/
/*                   kd_source_manager::advance_admin_id                     */
/*****************************************************************************/

bool
  kd_source_manager::advance_admin_id(kdu_byte buf[])
{
  if (!admin_cipher)
    return false;
  acquire_lock();
  admin_cipher.encrypt(admin_id);
  memcpy(buf,admin_id,16);
  release_lock();
  return true;
}

/*****************************************************************************/
/*                      kd_source_manager::write_stats                       */
/*****************************************************************************/

void
  kd_source_manager::write_stats(kd_message_block &block)
{
  acquire_lock();
  block << "\nCURRENT STATS:\n";
  block << "    Current active sources = " << num_sources << "\n";
  block << "    Current active clients = " << num_threads << "\n";
  block << "    Total clients started = " << total_clients << "\n";
  block << "    Total clients successfully completed = "
        << completed_connections << "\n";
  block << "    Total clients terminated (channels not connected in time) = "
        << terminated_connections << "\n";
  block << "    Total bytes transmitted to completed clients = "
        << (int) total_transmitted_bytes << "\n";
  release_lock();
}

/*****************************************************************************/
/*                     kd_source_manager::write_history                      */
/*****************************************************************************/

void
  kd_source_manager::write_history(kd_message_block &block, int max_records)
{
  acquire_lock();
  int index = completed_connections;
  block << "\nHISTORY:\n";
  kd_client_history *scan;
  for (scan=history_head; (scan != NULL) && (max_records > 0);
       scan=scan->next, max_records--, index--)
    {
      block << "* " << index << ":\n";
      block << "    Target = \"" << scan->target.filename << "\"";
      if (scan->target.byte_range[0] != '\0')
        block << "(" << scan->target.byte_range << ")";
      block << "\n";
      if (scan->jpip_channel_type == KD_CHANNEL_STATELESS)
        block << "    Channel = <none> (stateless)\n";
      else if (scan->jpip_channel_type == KD_CHANNEL_HTTP)
        block << "    Channel = \"http\"\n";
      else if (scan->jpip_channel_type == KD_CHANNEL_HTTP_TCP)
        block << "    Channel = \"http-tcp\"\n";
      block << "    Connected for " << scan->connected_seconds << " seconds\n";
      block << "    Transmitted bytes = " << scan->transmitted_bytes << "\n";
      block << "    Number of requests = " << scan->num_requests << "\n";
      block << "    Number of primary channel attachment events = "
            << scan->num_connections << "\n";
      if (scan->jpip_channel_type == KD_CHANNEL_HTTP_TCP)
        {
          block << "    Average RTT = " << scan->average_rtt
                << " seconds (over " << scan->rtt_events << " RTT events)\n";
          block << "    Approximate transmission rate = "
                << scan->approximate_rate << " bytes/sec\n";
        }
      else if (scan->jpip_channel_type == KD_CHANNEL_HTTP)
        {
          block << "    Average byte limit in requests which were not "
                "pre-empted = " << scan->average_serviced_byte_limit << "\n";
          block << "    Average bytes delivered per request which was not "
                "pre-empted = " << scan->average_service_bytes << "\n";
          block << "    Average rate (not compensating for idle clients) = "
                << scan->transmitted_bytes / (scan->connected_seconds+0.01F)
                << " bytes/sec\n";
        }
    }
  release_lock();
}

/*****************************************************************************/
/*                  kd_source_manager::get_new_history_entry                 */
/*****************************************************************************/

kd_client_history *
  kd_source_manager::get_new_history_entry()
{
  kd_client_history *result;
  if (max_history_records <= 0)
    return NULL;
  if (num_history_records < max_history_records)
    result = new kd_client_history;
  else
    {
      result = history_tail;
      history_tail = result->prev;
    }
  memset(result,0,sizeof(kd_client_history));
  if ((result->next = history_head) != NULL)
    result->next->prev = result;
  history_head = result;
  if (history_tail == NULL)
    history_tail = history_head;
  history_tail->next = NULL;
  num_history_records++;
  return result;
}


/* ========================================================================= */
/*                             External Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* EXTERN                            main                                    */
/*****************************************************************************/

int
  main(int argc, char *argv[])
{
#ifdef _DEBUG
  _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
#endif
  // _CrtSetBreakAlloc(50); // Example of how to find memory leaks

  // Prepare console I/O and argument parsing.
  kdu_customize_warnings(&kd_msg);
  kdu_customize_errors(&kd_msg_with_exceptions);
  kdu_args args(argc,argv,"-s");

  FILE *log_file = NULL;
  if (args.find("-log") != NULL)
    {
      char *string = args.advance();
      if ((string == NULL) || (*string == '\0'))
        { kdu_error e; e << "Missing or empty file name supplied with "
          "the \"-log\" argument."; }
      log_file = fopen(string,"w");
      if (log_file == NULL)
        { kdu_error e;
          e << "Unable to open supplied log file, \"" << string << "\"."; }
      sys_msg.redirect(log_file);
      sys_msg_with_exceptions.redirect(log_file);
      args.advance();
    }

  get_mac_address(mac_address);

  kd_source_manager *source_manager = NULL;
  kd_connection_server *connection_server = NULL;
  kd_delivery_manager *delivery_manager = NULL;
  kd_delegator delegator;
  HANDLE listen_event = CreateEvent(NULL,TRUE,FALSE,NULL); // Manual-reset
  char *cache_directory=NULL;
  try { // Catch otherwise uncaught exceptions thrown by `error_callback'.

      // Get command-line configurable parameters
      kdu_uint32 listen_ip_addr;
      int listen_port, max_clients, max_sources;
      float max_connection_seconds, max_rtt_target, min_rtt_target;
      float max_bytes_per_second;
      int max_chunk_bytes, phld_threshold, per_client_cache;
      int initial_timeout_milliseconds, completion_timeout_milliseconds;
      int max_history_records, max_connection_threads;
      bool ignore_relevance_info, restricted_access;
      char *passwd =
        parse_arguments(args,listen_ip_addr,listen_port,max_clients,
                        max_sources,delegator,max_connection_seconds,
                        max_rtt_target,min_rtt_target,max_bytes_per_second,
                        max_chunk_bytes,ignore_relevance_info,phld_threshold,
                        per_client_cache,max_history_records,
                        initial_timeout_milliseconds,
                        completion_timeout_milliseconds,
                        max_connection_threads,
                        restricted_access,cache_directory);
      if (args.show_unrecognized(kd_msg) != 0)
        { kdu_error e;
          e << "There were unrecognized command line arguments!"; }

      // Start up network
      WORD wsa_version = MAKEWORD(2,2);
      WSADATA wsa_data;
      WSAStartup(wsa_version,&wsa_data);

      // Start up service managers
      delivery_manager =
        new kd_delivery_manager(max_connection_seconds,max_rtt_target,
                                min_rtt_target,max_bytes_per_second);
      delivery_manager->start();
      source_manager =
        new kd_source_manager(delivery_manager,listen_port,max_sources,
                              max_clients,max_chunk_bytes,
                              ignore_relevance_info,phld_threshold,
                              per_client_cache,max_history_records,
                              completion_timeout_milliseconds,
                              passwd,listen_event,cache_directory);
      connection_server =
        new kd_connection_server(source_manager,restricted_access,
                                 max_connection_threads,&delegator,
                                 initial_timeout_milliseconds);
      if (passwd != NULL)
        delete[] passwd;

      // Start up listening socket.
      SOCKET listen_socket = socket(AF_INET,SOCK_STREAM,0);
      if (listen_socket == INVALID_SOCKET)
        { kdu_error e; e << "Unable to create a listening socket!"; }
      SOCKADDR_IN listen_addr;
      memset(&listen_addr,0,sizeof(listen_addr));
      if (listen_ip_addr != INADDR_NONE)
        listen_addr.sin_addr.S_un.S_addr = listen_ip_addr;
      else
        {
          char local_hostname[256]; local_hostname[255] = '\0';
          gethostname(local_hostname,255);
          HOSTENT *hostent = gethostbyname(local_hostname);
          if (hostent == NULL)
            { kdu_error e;
              e << "Cannot obtain local host address for server to use!";}
          memcpy(&(listen_addr.sin_addr),hostent->h_addr_list[0],4);
        }
      listen_addr.sin_family = AF_INET;
      listen_addr.sin_port = htons((kdu_uint16) listen_port); // Byte order
      if (bind(listen_socket,(SOCKADDR *)(&listen_addr),sizeof(listen_addr)))
        { kdu_error e;
          e << "Unable to bind listening socket to supplied port!"; }
      listen(listen_socket,5);

      // Print starting details for log file.
      time_t binary_time; time(&binary_time);
      struct tm *local_time = localtime(&binary_time);
      kd_msg << "\nKakadu Experimental JPIP Server "
             << KDU_CORE_VERSION << " started:\n"
             << "\tTime = " << asctime(local_time)
             << "\tListening address:port = "
             << inet_ntoa(listen_addr.sin_addr)
             << ":" << ntohs(listen_addr.sin_port) << "\n";
      kd_msg.flush();

      // Enter service loop.
      SOCKET connected_socket;
      while ((WSAEventSelect(listen_socket,listen_event,FD_ACCEPT) == 0) &&
             (WSAWaitForMultipleEvents(1,&listen_event,TRUE,INFINITE,
                                       FALSE) == WSA_WAIT_EVENT_0) &&
             !source_manager->check_shutdown_requested())
        {
          if ((connected_socket =
               accept(listen_socket,NULL,NULL)) != INVALID_SOCKET)
            {
              connection_server->service_request(connected_socket);
              connection_server->wait_for_resources();
            }
          ResetEvent(listen_event);
        }

      // Do the following if the listening loop terminates normally
      shutdown(listen_socket,SD_BOTH);
      closesocket(listen_socket);
    }
  catch (int)
    {
      if (log_file != NULL)
        { fclose(log_file); log_file = NULL; }
      return -1;
    } // Fatal error occurred in server application.

  if (connection_server != NULL)
    delete connection_server; // Waits until pending connection requests served
  if (source_manager != NULL)
    delete source_manager; // Waits until current clients disconnect
  if (delivery_manager != NULL)
    delete delivery_manager;
  if (cache_directory != NULL)
    delete[] cache_directory;

  // Shut down services
  CloseHandle(listen_event);
  WSACleanup();
  if (log_file != NULL)
    { fclose(log_file); log_file = NULL; }
  return 0;
}
