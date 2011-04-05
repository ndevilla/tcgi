#
# Makefile for libtcgi.a
# 

# Compiler settings
CC		= gcc
IFLAGS  = -I.
CFLAGS	= -g -Wall
LDFLAGS = -L. -ltcgi

# Other variables
AR		= ar
ARFLAGS = rcv
RM 		= rm -rf

default:	check libtcgi.a

check:
	@(if ! [ -f dict.c ]; then echo "dict.c missing: download it from github"; exit 1 ; fi)
	@(if ! [ -f dict.h ]; then echo "dict.h missing: download it from github"; exit 1 ; fi)

test:		test.cgi

clean:
	$(RM) libtcgi.a *.o test.cgi lighttpd.conf tmp


COMPILE.c=$(CC) $(CFLAGS) -c
.c.o:
	@(echo "compiling $< ...")
	@($(COMPILE.c) -o $@ $<)


SRCS =	tcgi.c dict.c
OBJS = $(SRCS:.c=.o)

libtcgi.a: $(OBJS)
	@(echo "building $< ...")
	@($(AR) $(ARFLAGS) libtcgi.a $(OBJS))

test.cgi: tcgi.c dict.c
	$(CC) $(CFLAGS) $(IFLAGS) -o test.cgi tcgi.c dict.c -DTCGI_MAIN
	sed -e "s#PWD#`pwd`#" < lighttpd.conf.in > lighttpd.conf
	mkdir tmp

