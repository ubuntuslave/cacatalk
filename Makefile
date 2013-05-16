# This is a very simple Makefile, not taking much advantage of 
# built-in rules other than C/C++ source to C object.. 
# Type: 
#       make          to compile all the programs in the chapter 
#       make clean    to remove objects files and executables
#       make progname to make just progname

CC         = gcc
EXECS      =  cacatalk caca_sock_server
# List only those who will be created as objects:
OBJS       = src/common_image src/caca_sock_client
SRCS       = $(patsubst %, %.c, $(EXECS))
OBJS_NAMES = $(patsubst %, %.o, $(OBJS))
CFLAGS     =   -Wall -g
INCLUDEFLAGS = -I../include -I./include
ENVVARS    = # -DSHOWHOST -DDEVELOP 
ALLFLAGS   = $(CFLAGS) $(INCLUDEFLAGS) # $(ENVVARS)
LDFLAGS    = -lv4l2 -lcaca -lncurses

all: $(OBJS) $(EXECS)

.PHONY: all clean  cleanall

clean:
	\rm -f $(OBJS) 

cleanall:
	\rm -rf core *.dSYM $(OBJS) $(EXECS)

install: $(EXECS)
	cp $(EXECS) ~/bin
	
$(OBJS): 
	$(CC) -c $(ALLFLAGS) $@.c $(LDFLAGS) -o $@.o
	
cacatalk: include/cacatalk_common.h src/grabber.c $(OBJS_NAMES)
	$(CC) $(ALLFLAGS) $(OBJS_NAMES) src/grabber.c $(LDFLAGS) -o $@

caca_sock_server: src/caca_sock_server.c
	$(CC) $(ALLFLAGS) src/caca_sock_server.c -o $@
