/*
 * (c) fenugrec 2018
 * GPLv3
 *
 * extrat initialized data chunk
 */


#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "stypes.h"
#include "tdslib.h"

#define ROM_BASE 0x01000000UL	//flash ROM loaded address


void extract_idata(FILE *ifile, FILE *ofile) {
	u32 idata_siz;
	struct flashrom *flrom;
	struct flashrom_hdr *fh;	//shorthand

	flrom = loadrom(ifile);
	if (!flrom) return;

	fh = &flrom->fh;

	idata_siz = fh->bss_start - fh->sdata;
		/* sanity checks */
	if (	(fh->idata_start < ROM_BASE) ||
			((fh->idata_start - ROM_BASE + idata_siz) >= flrom->siz)) {
		printf("idata location out of bounds \n");
	}

	printf("Extracting idata @ %08lX (size=%08lX)\n",
			(unsigned long) fh->idata_start, (unsigned long) idata_siz);
	if (fwrite(&flrom->rom[fh->idata_start - ROM_BASE], 1, idata_siz, ofile) != idata_siz) {
		printf("bad fwrite : %s\n", strerror(errno));
	}

	closerom(flrom);
	return;
}




/********* main stuff (options etc) */



static struct option long_options[] = {
	{ "file", required_argument, 0, 'f' },
	{ "out", required_argument, 0, 'o' },
	{ "help", no_argument, 0, 'h' },
	{ NULL, 0, 0, 0 }
};

static void usage(void)
{
	fprintf(stderr, "usage:\n"
		"--file   \t-f <filename>\tbinary ROM dump\n"
		"--out    \t-o <filename>\toutput file name\n"
		"");
}


int main(int argc, char * argv[]) {
	char c;
	int optidx;
	FILE *ifile = NULL;
	FILE *ofile = NULL;

	printf(	"**** %s\n"
		"**** (c) 2018 fenugrec\n", argv[0]);

	while((c = getopt_long(argc, argv, "f:o:h",
			       long_options, &optidx)) != -1) {
		switch(c) {
		case 'h':
			usage();
			return 0;
		case 'f':
			if (ifile) {
				fprintf(stderr, "-f given twice");
				goto bad_exit;
			}
			ifile = fopen(optarg, "rb");
			if (!ifile) {
				fprintf(stderr, "fopen() failed: %s\n", strerror(errno));
				goto bad_exit;
			}
			break;
		case 'o':
			if (ofile) {
				fprintf(stderr, "-o given twice");
				goto bad_exit;
			}
			ofile = fopen(optarg, "wb");
			if (!ofile) {
				fprintf(stderr, "fopen() failed: %s\n", strerror(errno));
				goto bad_exit;
			}
			break;
		default:
			usage();
			goto bad_exit;
		}
	}
	if (!ifile || !ofile) {
		printf("some missing args.\n");
		usage();
		goto bad_exit;
	}

	if (optind <= 1) {
		usage();
		goto bad_exit;
	}

	extract_idata(ifile, ofile);
	fclose(ifile);
	fclose(ofile);
	return 0;

bad_exit:
	if (ifile) {
		fclose(ifile);
	}
	if (ofile) {
		fclose(ofile);
	}
	return 1;
}

