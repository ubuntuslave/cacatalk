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
#include <linux/videodev2.h>
#include <libv4l2.h>
#include "caca.h"

#define BUFFER_SIZE MAX_INPUT
#define TEXT_ENTRIES 5

typedef struct textentry
{
  uint32_t buffer[BUFFER_SIZE + 1];
  unsigned int size, cursor, changed;
} textentry;

struct thread_arg_struct {
    int socketfd;
    int text_buffer_size;
    unsigned int row_offset;
    unsigned int col_offset;
    char label[MAX_INPUT]; ///< title string
    caca_canvas_t *cv;  ///< caca canvas
    caca_display_t *dp; ///< caca display
};

typedef struct video_params_s {
  char dev_name[MAXPATHLEN]; ///< the path to the video device name
  int img_width;        ///< Video frame width
  int img_height;       ///< Video frame heigth
  int v4l_fd;           ///< file despcriptor for the open video device
  struct v4l2_format fmt;  ///< v4l stream data format
  struct v4l2_buffer buf;  ///< video buffer info
  struct v4l2_requestbuffers req; ///< v4l memory mapping buffers (cacatalk uses a double buffer)
  unsigned int number_of_buffers;///< The number of memory mapping buffers
  struct buffer *buffers;       ///< array of memory buffer structure for video
  enum v4l2_buf_type type;      ///< Indicates the type of buffering (e.g. V4L2_BUF_TYPE_VIDEO_CAPTURE)
  enum v4l2_memory   memory;    ///< Indicates the type of memory for v4l stream (e.g. V4L2_MEMORY_MMAP)
  char *caca_format;      ///< preferred libcaca format (e.g. "ansi") to export video (for transmission)
  char *caca_dither;      ///< Indicates the algorithm to be used for dithering with libcaca
  float caca_gamma;       ///< Gamma value (unset: to be implemented in the future)
  float caca_brightness;  ///< Brightness (unset: to be implemented in the future)
  float caca_contrast;    ///< Contrast (unset: to be implemented in the future)
  float aspect_ratio;   ///< Aspect ratio based to fit video in given number of columns in caca_canvas
  unsigned int cv_rows; ///< Number of rows to resolve video on caca _anvas
  unsigned int cv_cols; ///< Number of columns to resolve video on caca _anvas
} video_params;


/** @brief TODO
 *
 * @return the video device file descriptor (greater than -1 if video was set/open successfully)
 */
int set_video(video_params *vid_params, char *dev_name, unsigned int cv_height_for_video, int img_width, int img_height);

/** @brief Stop streaming video and close V4L video device
 *
 * @return the video device file descriptor (greater than -1 if video was set/open successfully)
 */
void close_video_stream(video_params *vid_params);

/** @brief
 A Window structure encapsulates the dimensions of the window and the
 amount by which the text within it has been scrolled. The  line_at_top
 member is the lowest index of all lines in the text buffer that is currently
 visible in the window, i.e., the index of the top line on the screen.
 Only complete lines are displayed, so line_at_top is the index of the
 line whose entire contents are visible.
 */
typedef struct _window
{
  unsigned short rows;
  unsigned short cols;
} Window;

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

    while ((c = getopt(argc, argv, "sv:p:")) != -1) // The ":" next to a flag indicates to expect a value
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
