/*
 * (c) fenugrec 2018
 * GPLv3
 *
 * Just NVRAM dump data visualization.
 * To work, this also needs the ROM to determine
 * data layout within the NVRAM. Not sure how much the
 * layout varies between firmware versions.
 *
 * This tool assumes the NVRAM dump is mapped at 0x0400 0000.
 *
 * Uses the built-in symbol table to find everything, so
 * only -f and -n need be specified. ex.:
 *
 * ./nvram_tool -f TDS744A_ROM.bin -n nvdump.bin
 *
 */


#include <errno.h>
#include <stdbool.h>
#include <stddef.h>	//for offsetof
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "stypes.h"
#include "tdslib.h"

#define file_maxsize (8*1024*1024UL)	//arbitrary 8MB limit
#define NVRAM_BASE 0x04000000UL
#define NVRAM_MAXSIZ	((512 + 128) * 1024UL)

/*** symbol names to retrieve required metadata */
#define SYM_LIBRCOLL "_libraryCollection"


/** parse librarian descriptor
 *
 * fills caller-provided *librd
 */
static void parse_librdescr(struct flashrom *flrom, u32 fileoffs, struct libr_descr *librd) {
	librd-> ptype = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, ptype)]);
	librd-> psize = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, psize)]);
	librd-> poffs = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, poffs)]);
	librd-> pfactory = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, pfactory)]);
	librd-> membase = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, membase)]);
	librd-> size1 = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, size1)]);
	librd-> size2 = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, size2)]);
	librd-> psavefunc = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, psavefunc)]);
	return;
}


static void dump_onelibr(struct flashrom *flrom, u32 fileoffs, const u8 *nvdata, u32 nvsiz) {
	struct libr_descr librd;

	parse_librdescr(flrom, fileoffs, &librd);

	printf("librarian @ %lX:\tmembase=%lX, size1=%lX, size2=%lX\n",
			(unsigned long) fileoffs, (unsigned long) librd.membase,
			(unsigned long) librd.size1, (unsigned long) librd.size2);
	if ((librd.membase >= NVRAM_BASE) && (librd.membase <= (NVRAM_BASE + NVRAM_MAXSIZ))) {
		printf("outside NVRAM, skipping\n");
		return;
	}

	return;
}



static void dump_libs(struct flashrom *flrom, const u8 *nvdata, u32 nvsiz) {
	u32 librcoll_pos;
	unsigned lib_index = 0;
	u32 cur;

	if (!pm_parse32(flrom, &librcoll_pos, SYM_LIBRCOLL)) {
		printf("couldn't find collection\n");
		return;
	}

	if (librcoll_pos >= flrom->siz) {
		printf("bad collection pos\n");
		return;
	}

	for (cur = librcoll_pos; 1; cur += 4) {
		u32 libr_pos = reconst_32(&flrom->rom[cur]);
		if (libr_pos == 0) {
			if (lib_index == 0) {
				printf("skipping null first entry\n");
				continue;
			}
			return;
		}
		libr_pos -= ROM_BASE;
		if (libr_pos >= flrom->siz) {
			printf("bad libr pos\n");
			return;
		}
		dump_onelibr(flrom, libr_pos, nvdata, nvsiz);
	}
	return;
}




/********* main stuff (options etc) */



static struct option long_options[] = {
	{ "file", required_argument, 0, 'f' },
	{ "nvdump", required_argument, 0, 'n' },
	{ "help", no_argument, 0, 'h' },
	{ NULL, 0, 0, 0 }
};

static void usage(void)
{
	fprintf(stderr, "usage:\n"
		"--file    \t-f <filename>\tfirmware ROM dump\n"
		"--nvdump  \t-n <filename>\tNVRAM dump\n"
		"");
}


int main(int argc, char * argv[]) {
	struct flashrom *flrom;
	u8 nvdata[NVRAM_MAXSIZ];
	u32 nvsiz;

	char c;
	int optidx;
	FILE *file = NULL;
	FILE *nvfile = NULL;

	printf(	"**** %s\n"
		"**** (c) 2018 fenugrec\n", argv[0]);

	while((c = getopt_long(argc, argv, "f:n:h",
			       long_options, &optidx)) != -1) {
		switch(c) {
		case 'h':
			usage();
			return 0;
		case 'n':
			if (nvfile) {
				fprintf(stderr, "-n given twice");
				goto bad_exit;
			}
			nvfile = fopen(optarg, "rb");
			if (!nvfile) {
				fprintf(stderr, "fopen() failed: %s\n", strerror(errno));
				goto bad_exit;
			}
			break;
		case 'f':
			if (file) {
				fprintf(stderr, "-f given twice");
				goto bad_exit;
			}
			file = fopen(optarg, "rb");
			if (!file) {
				fprintf(stderr, "fopen() failed: %s\n", strerror(errno));
				goto bad_exit;
			}
			break;
		default:
			usage();
			goto bad_exit;
		}
	}
	if (!file || !nvfile) {
		printf("some missing args.\n");
		usage();
		goto bad_exit;
	}

	flrom = loadrom(file);
	if (!flrom) return 1;

	rewind(nvfile);
	nvsiz = fread(nvdata,1,NVRAM_MAXSIZ,nvfile);	//read maximum

	fclose(file);
	fclose(nvfile);

	print_rominfo(flrom);

	dump_libs(flrom, nvdata, nvsiz);
	return 0;

bad_exit:
	if (file) {
		fclose(file);
	}
	return 1;
}

