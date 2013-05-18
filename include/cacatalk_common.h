/****************************************************************************************
  Title          : cacatalk_common.h
  Author         : Carlos Jaramillo
  Course         : CS 82010 UNIX Application Development (Spring 2013)
  Instructor     : Prof. Stewart Weiss
  Created on     : May  16, 2013
  License        : ​Do What The Fuck You Want To Public License (WTFPL)

  Description    : common structures and functions for cacatalk
****************************************************************************************/

#ifndef CACATALK_COMMON_H_
#define CACATALK_COMMON_H_

#include <getopt.h>
#include <sys/param.h>
#include "caca.h"

#define BUFFER_SIZE 75
#define TEXT_ENTRIES 5

typedef struct textentry
{
  uint32_t buffer[BUFFER_SIZE + 1];
  unsigned int size, cursor, changed;
} textentry;

typedef struct options_s
{
  char video_device_name[BUFFER_SIZE]; ///< The video device name
  char peer_name[MAXHOSTNAMELEN]; ///< The IP address or hostname of the peer to connect to
  int is_server; ///< To indicate socket behavior (either as server=1, or client=0 (default))
} options;

/** @brief Parses command-line options and corresponding arguments using the getopt() function
 * found in the unistd library.
 *
 * @param argc The command line argument count
 * @param argv The command line arguments list
 * @param opt  A pointer to the options structure pertaining to corresponding argument values
 *
 * @retval 0 if successfully parsed all options
 * @retval -1 if there was an error parsing options
 */
int get_options(int argc, char **argv, options * opt)
  {
    opterr = 0;
    int c;

    // Initialize defaults
    strcpy(opt->video_device_name, "/dev/video0\0");
    memset(opt->peer_name, '\0', MAXHOSTNAMELEN);
    opt->is_server = 0; // Socket client is the default behavior

    while ((c = getopt(argc, argv, "sv:p:")) != -1)
    {
      switch (c)
      {
        case 's':
          opt->is_server = 1;
          break;
        case 'v':
          strcpy(opt->video_device_name, optarg);
          break;
        case 'p':
          strcpy(opt->peer_name, optarg);
          break;
        case '?':
        {
          if (optopt == 'v' || optopt == 'p')
            fprintf(stderr, "Option -%c requires an argument.\n", optopt);
          else if (isprint (optopt))
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
          else
            fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
          return -1;
        }
        default:
          abort();
          break;
      }
    }

    // Get non-option arguments (usernames, in this case)
    /* None so far
     for (int index = optind; index < argc; index++)
     {
     argv[index];
     }
     */

    return 0;
  };
#endif /* CACATALK_COMMON_H_ */
