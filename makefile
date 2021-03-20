CC =		gcc
CLIBS =		$(shell sdl-config --libs) -lSDL_image -lpthread -lmraa -lm #-lefence
CFLAGS =	-O3 -W $(shell sdl-config --cflags) -g -W

HEADERS =	src/include/tracker.h 
SOURCES =	src/lib/tracker.c

all:		tracker

clean:
		rm -f *.o
		rm tracker

tracker:		${SOURCES} ${HEADERS}
		${CC} ${CFLAGS} -o tracker ${SOURCES} ${CLIBS}
