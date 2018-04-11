CC = gcc
BASICFLAGS = -std=gnu11 -Wall -Wextra -Wpedantic
OPTFLAGS = -g
#extra flags for profiling
#EXFLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = $(BASICFLAGS) $(OPTFLAGS) $(EXFLAGS)

TGTLIST = unpack_text test extract_idata nvram_tool

all: $(TGTLIST)


extract_idata:	extract_idata.c tdslib.c

test:	test.c tdslib.c

unpack_text: unpack_text.c tdslib.c

nvram_tool: nvram_tool.c tdslib.c

clean:
	rm -f *.o
	rm -f $(TGTLIST)
