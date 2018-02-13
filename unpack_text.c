/*
 * (c) fenugrec 2018
 * GPLv3

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

// these are unlikely to change
#define word_size 0x0C
#define word_mask 0xFFF

#define file_maxsize (8*1024*1024UL)	//arbitrary 8MB limit

struct pack_meta {
	u32 tokbase;
	u32 maxtok;
	u32 tokaddr;
	u32 ptext;
	//these can only be filled after buffering the ROM
	const u8 *text_tok;	//table of tokens
	const u8 *packed_t;	//raw packed text data
};

/* global data for mangle() */
bool flag_getlen;
void *bdest;


/* 99% confidence; returns sane data. */
u32 mangle(const struct pack_meta *pm, u32 val) {
	//val is "d2" in orig code

//	printf("mang: n %X:", val);
	while (val >= pm->tokbase) {
		u32 a1;
		if (val > pm->maxtok) {
			return 0;
		}
		val -= pm->tokbase;
		a1 = reconst_16(&(pm->text_tok[0 + (val * 4)]));
		(void) mangle(pm, a1);	//recurse !

		//update for next loop
		val = reconst_16(&(pm->text_tok[2 + (val * 4)]));
		//printf("L %04X.", val);
	}

	printf("%c", (char) val);

	if (!flag_getlen) {
		*(u8 *) bdest = (val & 0xFF);
	}
	bdest = (u8 *) bdest + 1;
	return val;
}

/* TODO : confirm u16 vs u32 arg pushing
 *
 * if (getlen) : arg8 is u16 / don't care; set to 0.
 * if (!getlen) : arg8 is (u8 *) for destination string
*/
u16 _unpack_text(const struct pack_meta *pm, u32 offs_packed, bool getlen, void *arg8) {
	const u8 *pd;	//packed chunk; a2 in code
	u32 d2 = 0;
	u32 d3 = 0;

	bdest = arg8;
	flag_getlen = getlen;

	pd = &(pm->packed_t[offs_packed]);

	if (!offs_packed) return 0;
	//TODO : bounds check against runtime-calculated _Text_size ?

	printf("upt @ %lX: \"", (unsigned long) offs_packed);

	while (1) {
		u32 d0;

		if (d2 < word_size) {
			d3 <<= 8;
			d3 |= *pd;
			pd++;
			d2 += 8;
			continue;
		}

		d2 -= word_size;
		d0 = (d3 >> d2) & word_mask;
		d0 = mangle(pm, d0);
		if (d0) {
			continue;
		}
		break;
	}

	printf("\n");

	//return string length probably
	return (((u16) bdest & 0xFFFF) - (u16) arg8);
}


void unpack_all(FILE *i_file, struct pack_meta *pm) {
	u32 file_len;
	u16 rv16;
	u16 rec_index = 0;

	rewind(i_file);
	file_len = flen(i_file);
	if ((!file_len) || (file_len > file_maxsize)) {
		printf("huge file (length %lu)\n", (unsigned long) file_len);
	}

	u8 *src = malloc(file_len);
	if (!src) {
		printf("malloc choke\n");
		return;
	}

	/* load whole ROM */
	if (fread(src,1,file_len,i_file) != file_len) {
		printf("trouble reading\n");
		free(src);
		return;
	}

	pm->text_tok = &src[pm->tokaddr];
	pm->packed_t = &src[pm->ptext];

	// test : call first with mflag = 1, bdest = 0
	rv16 = _unpack_text(pm, rec_index + 0x49, 1, 0);

	return;
}




/********* main stuff (options etc) */



static struct option long_options[] = {
//	{ "base", required_argument, 0, 'b' },
	{ "tokbase", required_argument, 0, 't' },
	{ "maxtok", required_argument, 0, 'm' },
	{ "tokaddr", required_argument, 0, 'a' },
	{ "ptext", required_argument, 0, 'p' },
//	{ "debug", no_argument, 0, 'd' },
	{ "help", no_argument, 0, 'h' },
	{ NULL, 0, 0, 0 }
};

static void usage(void)
{
	fprintf(stderr, "usage:\n"
		"--file    \t-f <filename>\tbinary ROM dump\n"
		"--tokbase\t-t <base>  \tvalue of _Token_base\n"
		"--maxtok \t-m <max>   \tvalue of _Max_token\n"
		"--tokaddr\t-a <addr>  \tfile offset of _Text_token table\n"
		"--ptext  \t-p <addr>  \tfile offset of packed text array\n"
		"");
}


int main(int argc, char * argv[]) {
	struct pack_meta pm = {0};

	char c;
	int optidx;
	FILE *file = NULL;

	printf(	"**** %s\n"
		"**** (c) 2018 fenugrec\n", argv[0]);

	while((c = getopt_long(argc, argv, "t:m:a:p:f:h",
			       long_options, &optidx)) != -1) {
		switch(c) {
		case 'h':
			usage();
			return 0;
		case 't':
			if (sscanf(optarg, "%x", &pm.tokbase) != 1) {
				printf("did not understand %s\n", optarg);
				goto bad_exit;
			}
			break;
		case 'm':
			if (sscanf(optarg, "%x", &pm.maxtok) != 1) {
				printf("did not understand %s\n", optarg);
				goto bad_exit;
			}
			break;
		case 'a':
			if (sscanf(optarg, "%x", &pm.tokaddr) != 1) {
				printf("did not understand %s\n", optarg);
				goto bad_exit;
			}
			break;
		case 'p':
			if (sscanf(optarg, "%x", &pm.ptext) != 1) {
				printf("did not understand %s\n", optarg);
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
	if (!pm.tokbase || !pm.maxtok || !pm.tokaddr || !pm.ptext ||
			!file) {
		printf("some missing args.\n");
		usage();
		goto bad_exit;
	}

	if (optind <= 1) {
		usage();
		goto bad_exit;
	}

	unpack_all(file, &pm);
	fclose(file);
	return 0;

bad_exit:
	if (file) {
		fclose(file);
	}
	return 1;
}

