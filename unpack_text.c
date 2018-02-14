/*
 * (c) fenugrec 2018
 * GPLv3

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

/*** symbol names to retrieve required metadata */
#define SYM_WORDSIZE "_Word_size"
#define SYM_WORDMASK "_Word_mask"
#define SYM_TOKBASE "_Token_base"
#define SYM_MAXTOK "_Max_token"
#define SYM_TOKADDR "_Text_token"
#define SYM_PTEXT "_Packed_text"
#define SYM_RECISIZ "_recIndexSize"
#define SYM_RECINDEX "_RecIndex"

/** static init to 0 = safe */
struct pack_meta {
	u32 symloc;	//file offset of symbol table

	//these can be filled after buffering the ROM
	u32 a_recindex;	//fileofs of record index
	u32 a_recisiz; //fileofs of rec index size
	u32 recisiz;

	u32 a_tokbase;	//fileofs of tokbase
	u32 tokbase;
	u32 a_maxtok;	//fileofs of maxtok
	u32 maxtok;
	u32 a_tok;	//file ofs of text_token
	u32 a_ptext;	//file ofs of packed_text

	u16 word_size;
	u16 word_mask;


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

/** unpack selected chunk
 *
 * @param offs_packed file offs of chunk within _packed_text
 *
 * @return strlen
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
	//TODO : bounds check against _Text_size ?

	while (1) {
		u32 d0;

		if (d2 < (pm->word_size)) {
			d3 <<= 8;
			d3 |= *pd;
			pd++;
			d2 += 8;
			continue;
		}

		d2 -= (pm->word_size);
		d0 = (d3 >> d2) & (pm->word_mask);
		d0 = mangle(pm, d0);
		if (d0) {
			continue;
		}
		break;
	}

	printf("\n");

	//return string length
	return (u16) ((u8 *) bdest - (u8 *) arg8);
}



/** helper for u16 meta parsage
 *
 * 1) find symbol entry with name *sym
 * 2) get u16 value at p_obj
 *
 * @return 1 if ok
 */

static bool pm_parse16v(const u8 *buf, u32 siz, u16 *dest, struct pack_meta *pmstruc, const char *sym) {

	if (*dest) return 1;

	u32 psymoffs = find_sym(buf, pmstruc->symloc, siz, (const u8 *) sym, strlen(sym));
	if (!(psymoffs)) {
		printf("Couldn't find %s !\n", sym);
		return 0;
	}

	u32 pobj = reconst_32(&buf[psymoffs + offsetof(struct sym_entry, p_obj)]);

	if (!pobj || (pobj < ROM_BASE) ||
		((pobj - ROM_BASE) > siz)) {
		printf("bad pobj\n");
		return 0;
	}
	pobj -= ROM_BASE;
	*dest = (u16) reconst_32(&buf[pobj]);
	if (!*dest) {
		printf("bad value for %s\n", sym);
		return 0;
	}

	return 1;
}

/** helper for u32 meta parsage
 *
 * get file ofs of obj, since caller may not need the value at *pobj
 */
static bool pm_parse32(const u8 *buf, u32 siz, u32 *dest, struct pack_meta *pmstruc, const char *sym) {

	if (*dest) return 1;

	u32 psymoffs = find_sym(buf, pmstruc->symloc, siz, (const u8 *) sym, strlen(sym));
	if (!(psymoffs)) {
		printf("Couldn't find %s !\n", sym);
		return 0;
	}

	u32 pobj = reconst_32(&buf[psymoffs + offsetof(struct sym_entry, p_obj)]);

	if (!pobj || (pobj < ROM_BASE) ||
		((pobj - ROM_BASE) > siz)) {
		printf("bad pobj\n");
		return 0;
	}
	pobj -= ROM_BASE;
	*dest = pobj;
	if (!*dest) {
		printf("bad value for %s\n", sym);
		return 0;
	}

	return 1;
}

/** parse symbol table to fill in unspecified metadata in *pm.
 * ret 1 if ok
 */
static bool parse_meta(const u8 *buf, u32 siz, struct pack_meta *pm) {

	int rv = 1;

	rv &= pm_parse32(buf, siz, &pm->a_tokbase, pm, SYM_TOKBASE);
	rv &= pm_parse32(buf, siz, &pm->a_maxtok, pm, SYM_MAXTOK);
	rv &= pm_parse32(buf, siz, &pm->a_tok, pm, SYM_TOKADDR);
	rv &= pm_parse32(buf, siz, &pm->a_ptext, pm, SYM_PTEXT);
	rv &= pm_parse16v(buf, siz, &pm->word_size, pm, SYM_WORDSIZE);
	rv &= pm_parse16v(buf, siz, &pm->word_mask, pm, SYM_WORDMASK);
	rv &= pm_parse32(buf, siz, &pm->a_recindex, pm, SYM_RECINDEX);
	rv &= pm_parse32(buf, siz, &pm->a_recisiz, pm, SYM_RECISIZ);

	if (!rv) {
		return 0;
	}

	pm->tokbase = reconst_32(&buf[pm->a_tokbase]);
	pm->maxtok = reconst_32(&buf[pm->a_maxtok]);
	pm->recisiz = reconst_32(&buf[pm->a_recisiz]);
	pm->text_tok = &buf[pm->a_tok];
	pm->packed_t = &buf[pm->a_ptext];
	return 1;
}

static void unpack_all(FILE *i_file, struct pack_meta *pm) {
	u32 file_len;

	u16 rec_index = 0;

	rewind(i_file);
	file_len = flen(i_file);
	if ((!file_len) || (file_len > file_maxsize)) {
		printf("huge file (length %lu)\n", (unsigned long) file_len);
		return;
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

	if (!parse_meta(src, file_len, pm)) {
		printf("missing metadata\n");
		return;
	}

	// to work properly, index 0 is empty and points 2 bytes before recindex
	for (rec_index = 1; rec_index <= pm->recisiz; rec_index++) {
		u16 recpos;

		recpos = reconst_16(&src[pm->a_recindex + (rec_index * 2) - 2]);
		printf("\n\n****** record %u @ packed[%X] ******\n",
				rec_index, (unsigned) recpos);
		(void) _unpack_text(pm, recpos, 1, 0);
		printf("\n");
	}

	return;
}




/********* main stuff (options etc) */



static struct option long_options[] = {
//	{ "base", required_argument, 0, 'b' },
	{ "symloc", required_argument, 0, 's' },
//	{ "debug", no_argument, 0, 'd' },
	{ "help", no_argument, 0, 'h' },
	{ NULL, 0, 0, 0 }
};

static void usage(void)
{
	fprintf(stderr, "usage:\n"
		"--file    \t-f <filename>\tbinary ROM dump\n"
		"--symloc  \t-s <file_offs> start of symbol table in file\n"
		"");
}


int main(int argc, char * argv[]) {
	struct pack_meta pm = {0};

	char c;
	int optidx;
	FILE *file = NULL;

	printf(	"**** %s\n"
		"**** (c) 2018 fenugrec\n", argv[0]);

	while((c = getopt_long(argc, argv, "t:m:a:p:f:s:h",
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
			if (sscanf(optarg, "%x", &pm.a_tok) != 1) {
				printf("did not understand %s\n", optarg);
				goto bad_exit;
			}
			break;
		case 'p':
			if (sscanf(optarg, "%x", &pm.a_ptext) != 1) {
				printf("did not understand %s\n", optarg);
				goto bad_exit;
			}
			break;
		case 's':
			if (sscanf(optarg, "%x", &pm.symloc) != 1) {
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
	if (!pm.symloc || !file) {
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

