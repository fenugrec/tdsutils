CC = gcc
BASICFLAGS = -std=gnu99 -Wall -Wextra -Wpedantic 
OPTFLAGS = -g
#extra flags for profiling
#EXFLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = $(BASICFLAGS) $(OPTFLAGS) $(EXFLAGS)

TGTLIST = unpack_text

all: $(TGTLIST)

unpack_text: unpack_text.c tdslib.c

clean:
	rm -f *.o
	rm -f $(TGTLIST)
