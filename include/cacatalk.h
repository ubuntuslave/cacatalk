/** \file cacatalk.h
 *  \author Carlos Jaramillo <cjaramillo@gc.cuny.edu>
 *  \brief The \e cacatalk public header.
 *
 Course         : CS 82010 UNIX Application Development (Spring 2013)
 Created on     : May 24, 2013
 Description    : A very simple video and text chat interface (peer-to-peer (non centralized)) based on libcaca
 License        : ​Do What The Fuck You Want To Public License (WTFPL)
 Purpose        : To demonstrate some of the principles used to transfer data using sockets,
 a wrapper around ncurses (via libcaca), the use of the Linux Media Infrasture API (a.k.a. V4L2),
 threads, and the pure awesomeness of CACA itself (Color AsCii Art).

 Usage          : cacatalk  [-p address] [-v video_device_path] [-d caca_driver_style(1-7)]

 Options        :
                 -p address     Sets the reachable peer as a dotted decimal IPv4 address or network resolvable hostname
                 -v video_device_path  Sets the path to the video device (e.g /dev/video0)
                 -d caca_driver_style  The argument value is a number from 1 to 7,
                   and it allows to choose the environment of cacatalk,
                   Possible values represent (1: Default, 2: ncurses terminal, 3: conio, 4: GL, 5: raw, 6: VGA, 7: slang)

 ---- See below for detailed usage instructions ---
 Build with     : make

 Status         : First release (Initial development), so code needs to be cleaned and bugs killed
 The following functionally exists for caca talking:
 1) 'cacatalk' can be run from the command line without arguments at all), and they could be set during runtime from the main menu.
 2) Video stream (if valid device has been open) can be toggle on/off during chat session by pressing 'Ctr+V'
 3) 'cacatalk' uses 2 stream sockets (bidirectional). The text (chat) socket operates on port 25666. The video stream socket uses
 25667. Both peers are listening on 25666, but whoever initiates the connection (by pressing 'c' in the main menu) becomes
 the client who also starts listening on port 25667 for the peer to connect to. All this happens transparently.

 \bugs There maybe issues at quiting a connection when the other peer is still transmitting video data (socket lingers)

 \todo
 - Parametrize dimension values, such as video resolution (for now it's set statically to grab a 640x480 video),
 and the produced caca image uses 16 lines (rows).

 -Scrolling
 The program doesn't scroll (nor saves) ongoing text chats (also after leaving the chatroom).
 Also, there are only 5 entries for send/receive text chat.


 Design
 The program has two canvas:
  1) a presentation canvas on which menus and the chat room are drawn. buffer and a screen. The text buffer.
  2) a background canvas running on a thread that polls video and sends to the peer.

 ******************************************************************************/

#ifndef CACATALK_H_
#define CACATALK_H_

#include <getopt.h>
#include <sys/param.h>
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <pthread.h>
#include "caca.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define BUFFER_SIZE MAX_INPUT
#define TEXT_ENTRIES 5
#define NUM_THREADS  1 // just one for now

/**
 * @brief A buffer for text entry as uint32 characters used for putting characters in caca canvas
 */
typedef struct textentry
{
  uint32_t buffer[BUFFER_SIZE + 1]; ///< The text buffer
  unsigned int size;    ///< The number of characters in the buffer entry
  unsigned int cursor;  ///< The text cursor possition in the entry (relative to the first character)
  unsigned int changed; ///<  To indicate that a change to the buffer has been made.
} textentry;

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
  unsigned short video_lines;
  unsigned short video_cols;
  char *caca_format; ///< preferred libcaca format (e.g. "ansi, utf8, etc.") to export video (for transmission). Arbitrarily set to "caca"
} Window;

/** @brief
 This structure stores the argument options needed across the program. It will be called from a few different places if modification/setting
 of an member argument is needed.
 */
typedef struct options_s
{
  char video_device_name[BUFFER_SIZE]; ///< The video device name
  char peer_name[MAXHOSTNAMELEN]; ///< The IP address or hostname of the peer to connect to
  char host_IPv4[MAXHOSTNAMELEN]; ///< The IP address for the local host
  const char* driver_options[7]; ///< A list of strings for caca display driver options
  unsigned int driver_choice; ///< To choose an index from the array of drivers options
} options;

/** @brief
 This structure stores the video device parameters that are used for streaming video frames with V4L2
 */
typedef struct video_params_s
{
  int is_ok; ///< 1: Indicates when the video device is setup correctly. 0: is not okay
  int is_on; ///< 1: Indicates when the video device is streaming. 0: the video is not streaming. -1: status is undefined
  char dev_name[MAXPATHLEN]; ///< the path to the video device name
  int img_width; ///< Video frame width
  int img_height; ///< Video frame heigth
  int v4l_fd; ///< file despcriptor for the open video device
  struct v4l2_format fmt; ///< v4l stream data format
  struct v4l2_buffer buf; ///< video buffer info
  struct v4l2_requestbuffers req; ///< v4l memory mapping buffers (cacatalk uses a double buffer)
  unsigned int number_of_buffers; ///< The number of memory mapping buffers
  struct buffer *buffers; ///< array of memory buffer structure for video
  enum v4l2_buf_type type; ///< Indicates the type of buffering (e.g. V4L2_BUF_TYPE_VIDEO_CAPTURE)
  enum v4l2_memory memory; ///< Indicates the type of memory for v4l stream (e.g. V4L2_MEMORY_MMAP)
  char *caca_format; ///< preferred libcaca format (e.g. "ansi") to export video (for transmission)
  char *caca_dither; ///< Indicates the algorithm to be used for dithering with libcaca
  float caca_gamma; ///< Gamma value (unset: to be implemented in the future)
  float caca_brightness; ///< Brightness (unset: to be implemented in the future)
  float caca_contrast; ///< Contrast (unset: to be implemented in the future)
  float aspect_ratio; ///< Aspect ratio based to fit video in given number of columns in caca_canvas
  unsigned int cv_rows; ///< Number of rows to resolve video on caca _anvas
  unsigned int cv_cols; ///< Number of columns to resolve video on caca _anvas
} video_params;

/** @brief structure of arguments passed to a thread (also is going to be made global to be able to shut the video on and off)
 */
typedef struct video_out_args_s
{
  int socketfd; ///< The socket file descriptor for streaming video
  int quit; ///< Set to 1 to indicate to quite
  Window *win; ///< Pointer to window object structure
  video_params *vid_params; ///< Pointer to host's video device parameters structure
} video_out_args;

/** @brief  This is the main chatroom environment where text data is sent/received via a single text socket,
 *          and video is received using another socket.
 *
 *          Options, such as turning the video stream on/off can be toggled by pressing "Ctr+V"
 *          Every time the user enters a line of text, it will be sent to the peer.
 * @param cv  A pointer to the caca canvas
 * @param dp  A pointer to the caca display
 * @param text_fd The socket file descriptor for sending/receiving text data
 * @param vid_fd  The socket file descriptor for receiving video data
 * @param win A pointer to our custom Window element to which information will be assigned
 * @param peer_hostname The realized username@hostname of the peer user (However, username is not resolved yet in this version)
 *
 *  @returns 0 if nothing went wrong. This function always return (unless something crashed)
 */
int chat(caca_canvas_t *cv, caca_display_t *dp, int text_fd, int vid_fd, Window *win, char * peer_hostname);

/** @brief  Fills structure with current window dimensions
 *
 * @param fd  A file descriptor number related to the terminal (usually STDIN_FILENO)
 * @param video_lines The number of rows (lines) used for the video area on the canvas
 * @param win A pointer to our custom Window element to which information will be assigned
 * @param cv  A pointer to the caca canvas for which we will identify dimensions of.
 */
void set_window(int fd, unsigned short video_lines, Window *win, caca_canvas_t *cv);

/** @brief It draws the main menu indicating posible options tied to keyboard commands
 *
 * @param cv  A pointer to the caca canvas
 * @param arg_opts  The current argument options structure
 */
void display_menu(caca_canvas_t *cv, options * arg_opts);

/** @brief Sets the video device using the V4L2 driver
 *
 * @param vid_params  A pointer to the parameters structure for the video device to which the request will be made to.
 * @param dev_name   The path to the video device name (e.g. /dev/video0)
 * @param win  A pointer to the window parameters structure to be passed as setting video
 * @param img_width  The video frame width in pixels
 * @param img_height The video frame heigh in pixels
 *
 * @return the video device file descriptor (greater than -1 if video was set/open successfully)
 */
int set_video(video_params *vid_params, char *dev_name, Window *win, int img_width, int img_height);

/** @brief Sets an IP address or hostname to the peer to establish connection with
 *
 * @param cv  A pointer to the caca canvas
 * @param dp  A pointer to the caca display
 * @param opts  The current argument options structure into which the new hostname or IP address will be saved
 */
void set_peer_address(caca_canvas_t *cv, caca_display_t *dp, options *opts);

/** @brief Changes or sets the video device to be used with cacatalk
 *
 * @param cv  A pointer to the caca canvas
 * @param dp  A pointer to the caca display
 * @param opts  The current argument options structure into which the new hostname or IP address will be saved
 * @param vid_params  A pointer to the parameters structure for the video device to which the request will be made to.
 * @param win  A pointer to the window parameters structure to be passed as setting video
 * @param img_width  The video frame width in pixels
 * @param img_height The video frame heigh in pixels
 */
void change_video_device(caca_canvas_t *cv, caca_display_t *dp, options *opts, video_params *vid_params, Window *win,
                         int img_width, int img_height);

/** @brief Stop streaming video and close V4L video device
 *
 * @param a pointer to the parameters structure of the video device
 */
void close_video_stream(video_params *vid_params);

/** @brief Attempts to turn the video stream on and sets the related indicated flag in the passed structure.
 *
 * @param vid_params A pointer to the parameters structure for the video device to which the request will be made to.
 *
 * @retval 1    if the streaming request was set successfully
 * @retval 0    if the streaming request did not succeed, so the stream is off.
 */
int turn_video_stream_on(video_params *vid_params);

/** @brief Attempts to turn the video stream off and sets the related indicated flag in the passed structure.
 *
 * @param vid_params A pointer to the parameters structure for the video device to which the request will be made to.
 *
 * @retval  0    if the turning off streaming request was set successfully
 * @retval -1    if the streaming request did not succeed.
 */
int turn_video_stream_off(video_params *vid_params);

/** @brief It persistently calls the v4l2_ioctl() function to program the V4L2 device.
 *
 * @param fd    An open file descriptor.
 * @param request The encoded request indicating how to program the device (e.g. VIDIOC_STREAMON, VIDIOC_S_FMT, etc.)
 *                 Macros and defines specifying V4L2 ioctl requests are located in the videodev2.h header file
 * @param arg   The appropriate arguments (or pointer to a struct, e.g. v4l2_requestbuffers) related to the request at hand.
 *
 * @retval 0    request set successfully
 * @retval -1   on error. Also, the errno variable is set appropriately
 */
int xioctl(int fd, int request, void *arg);

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
int get_options(int argc, char **argv, options * opt);

// ******** Threads  **********
/** @brief This thread grabs video frames from the local host video device
 * and send it in the caca format to the connected peer
 */
void * send_video_thread(void * arguments);
// ----------------------------

#endif /* CACATALK_H_ */
