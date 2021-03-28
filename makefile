CC =		gcc
CLIBS =		$(shell sdl2-config --libs) -lSDL2_image -lpthread -lmraa -lOpenGL -lm #-lefence
CFLAGS =	-O3 -W $(shell sdl2-config --cflags) -g -W

HEADERS =	src/include/tracker.h
SOURCES =	src/lib/tracker.c

all:		tracker

clean:
		rm -f *.o
		rm tracker

tracker:		${SOURCES} ${HEADERS}
		${CC} ${CFLAGS} -o tracker ${SOURCES} ${CLIBS}
