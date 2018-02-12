/*
 * (c) fenugrec 2018
 * GPLv3

 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>

#include "tdslib.h"

// these are unlikely to change
#define word_size 0x0C
#define word_mask 0xFFF

#define file_maxsize (8*1024*1024UL)	//arbitrary 8MB limit

struct pack_meta {
	u32 tokbase;
	u32 maxtok;
	u32 tokaddr;
}

void unpack_all(file *i_file, struct pack_meta *pm) {
	u32 file_len;

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

	
	return;
}


static struct option long_options[] = {
//	{ "base", required_argument, 0, 'b' },
	{ "tokbase", required_argument, 0, 't' },
	{ "maxtok", required_argument, 0, 'm' },
	{ "tokaddr", required_argument, 0, 'a' },
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
		"");
}


int main(int argc, char * argv[]) {
	struct pack_meta pm = {0};

	char c;
	int optidx;
	FILE *file = NULL;

	printf(	"**** %s\n"
		"**** (c) 2018 fenugrec\n", argv[0]);

	while((c = getopt_long(argc, argv, "t:m:a:f:h",
			       long_options, &optidx)) != -1) {
		switch(c) {
		case 'h':
			usage();
			return 0;
		case 't':
			if (sscanf(optarg, "%x", &tok_base) != 1) {
				printf("did not understand %s\n", optarg);
				goto bad_exit;
			}
			break;
		case 'm':
			if (sscanf(optarg, "%x", &max_tok) != 1) {
				printf("did not understand %s\n", optarg);
				goto bad_exit;
			}
			break;
		case 'a':
			if (sscanf(optarg, "%x", &tok_addr) != 1) {
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
	if (!tok_addr || !max_tok || !tok_base || !file) {
		printf("some missing args\n");
		goto bad_exit;
	}

	if (optind <= 1) {
		usage();
		goto bad_exit;
	}

	unpack_all(file);
	fclose(file);
	return 0;

bad_exit:
	if (file) {
		fclose(file);
	}
	return 1;
}

