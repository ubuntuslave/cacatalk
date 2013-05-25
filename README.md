cacatalk
========

cacatalk is a simple, distributed (peer-to-peer) video chat interface for Linux terminals. It's based on libcaca and v4l2 

Carlos Jaramillo
<cjaramillo@gc.cuny.edu>

Copyright (C) 2013 under License: ​Do What The Fuck You Want To Public License (WTFPL)
 
Overview
-----------------------------------

'cacatalk' demonstrates basic socket programming principles used to transfer ASCII Art data using 
[libcaca | http://http://caca.zoy.org/wiki/libcaca], and its display drivers around ncurses, GL, conio, X11, etc.
Video grabbing from a supported device uses the Linux Media Infrasture API (a.k.a. V4L2). This is a prove of
concept on the power of asynchronous polling with the select() system call for file descriptors (sockets in our case),
the use of threads, and the pure awesomeness of CACA itself (Color AsCii Art).

This code is at an experimental stage, and licensed under the WTFPL license.

Installing
-----------------------------------

### From source ###

First, satisfy dependencies:

1) libcaca
2) libv4l2 (probably there already)
3) ncurses (mostlikely there already)

If you don't have git installed, do so:

    sudo apt-get install git-core

Download the package from this repository:

    git clone https://github.com/ubuntuslave/cacatalk.git

Build cacatalk with:

    make

Quick usage
-----------------------------------

Connect your v4l2-supported video camera and launch cacatalk: 

    ./cacatalk

Detailed current functionality
-----------------------------------

Usage          : cacatalk  [-p address] [-v video_device_path] [-d caca_driver_style(1-7)]

Options        :
                 -p address     Sets the reachable peer as a dotted decimal IPv4 address or network resolvable hostname
                 
                 -v video_device_path  Sets the path to the video device (e.g /dev/video0)
                 
                 -d caca_driver_style  The argument value is a number from 1 to 7,
                   and it allows to choose the environment of cacatalk,
                   Possible values represent (1: Default, 2: ncurses terminal, 3: conio, 4: GL, 5: raw, 6: VGA, 7: slang)

Current status 
---------------------------
This is a first release (initial development), so the source code needs to be cleaned up and debugged.
The following functionally exists for caca talking:

1) 'cacatalk' can be run from the command line without arguments at all), and they could be set during runtime from the main menu.

2) Video stream (if valid device has been open) can be toggle on/off during chat session by pressing 'Ctr+V'

3) 'cacatalk' uses 2 stream sockets (bidirectional). The text (chat) socket operates on port 25666. The video stream socket uses
25667. Both peers are listening on 25666, but whoever initiates the connection (by pressing 'c' in the main menu) becomes
the client who also starts listening on port 25667 for the peer to connect to. All this happens transparently.

* bugs 

There maybe issues at quiting a connection when the other peer is still transmitting video data (socket lingers)

* TODO

- Parametrize dimension values, such as video resolution (for now it's set statically to grab a 640x480 video),
 and the produced caca image uses 16 lines (rows).

- Add scrolling: The program doesn't scroll (nor saves) ongoing text chats (also after leaving the chatroom).
 Also, there are only 5 entries for send/receive text chat.

* Design

The program has two canvas:
1) a presentation canvas on which menus and the chat room are drawn. buffer and a screen. The text buffer.
2) a background canvas running on a thread that polls video and sends to the peer.

More info
-----------------------------------

Documentation:  can be found in the 'docs' folder




