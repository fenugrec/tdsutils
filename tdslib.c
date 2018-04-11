/* collection of funcs for working with Tektronix TDS7xx ROMs (5xx/6xx probably also)
 * (c) fenugrec 2018
 * GPLv3
 */

#include <stddef.h>	//for offsetof
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>	//malloc

#include "stypes.h"
#include "tdslib.h"

#define file_maxsize (8*1024*1024UL)	//arbitrary 8MB limit

uint32_t reconst_32(const uint8_t *buf) {
	// ret 4 bytes at *buf with SH endianness
	// " reconst_4B"
	return buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
}

uint16_t reconst_16(const uint8_t *buf) {
	return buf[0] << 8 | buf[1];
}

void write_32b(uint32_t val, uint8_t *buf) {
	//write 4 bytes at *buf with SH endianness
	// "decomp_4B"
	buf += 3;	//start at end
	*(buf--) = val & 0xFF;
	val >>= 8;
	*(buf--) = val & 0xFF;
	val >>= 8;
	*(buf--) = val & 0xFF;
	val >>= 8;
	*(buf--) = val & 0xFF;

	return;
}


//Painfully unoptimized, because it's easy to get it wrong
const uint8_t *u8memstr(const uint8_t *buf, uint32_t buflen, const uint8_t *needle, unsigned nlen) {
	uint32_t hcur;
	if (!buf || !needle || (nlen > buflen)) return NULL;

	for (hcur=0; hcur < (buflen - nlen); hcur++) {
		if (memcmp(buf + hcur, needle, nlen)==0) {
			return &buf[hcur];
		}
	}

	return NULL;
}

/* manual, aligned search for u16 val */
const uint8_t *u16memstr(const uint8_t *buf, uint32_t buflen, const uint16_t needle) {
	buflen &= ~1;
	uint32_t cur;
	for (cur = 0; cur < buflen; cur += 2) {
		if (reconst_16(&buf[cur]) == needle) return &buf[cur];
	}
	return NULL;
}

/* manual, aligned search instead of calling u8memstr; should be faster */
const uint8_t *u32memstr(const uint8_t *buf, uint32_t buflen, const uint32_t needle) {
	buflen &= ~3;
	uint32_t cur;
	for (cur = 0; cur < buflen; cur += 4) {
		if (reconst_32(&buf[cur]) == needle) return &buf[cur];
	}
	return NULL;
}


// hax, get file length but restore position
static u32 _flen(FILE *hf) {
	long siz;
	long orig;

	if (!hf) return 0;
	orig = ftell(hf);
	if (orig < 0) return 0;

	if (fseek(hf, 0, SEEK_END)) return 0;

	siz = ftell(hf);
	if (siz < 0) siz=0;
		//the rest of the code just won't work if siz = UINT32_MAX
	#if (LONG_MAX >= UINT32_MAX)
		if ((long long) siz == (long long) UINT32_MAX) siz = 0;
	#endif

	if (fseek(hf, orig, SEEK_SET)) return 0;
	return (u32) siz;
}




/** Find opcode pattern... bleh
 * "patlen" is # of opcodes
 */
uint32_t find_pattern(const uint8_t *buf, uint32_t siz, unsigned patlen,
			const uint16_t *pat, const uint16_t *mask) {
	uint32_t bcur = 0;	//base cursor for start of pattern
	uint32_t hcur = 0;	//iterating cursor
	unsigned patcur = 0;	//cursor within pattern

	while (hcur < (siz - patlen * 2)) {
		uint16_t val;
		val = reconst_16(&buf[hcur + patcur * 2]);
		if ((val & mask[patcur]) == pat[patcur]) {
			if (patcur == 0) bcur = hcur;
			patcur += 1;
			if (patcur == patlen) {
				//complete match !
				return bcur;
			}
			continue;
		}
		//no match : continue where we started
		patcur = 0;
		//hcur = bcur;
		hcur += 2;
	}
	//no match : ret -1 (illegal val)
	return -1;
}


#define SYMTBL_MAXSDIST	8
#define SYMSIZE_LOBOUND 0x1000
#define SYMSIZE_HIBOUND 0x2000
/** Attempt to guess the start and size of the sym table.
 *
 * Probably won't work as-is on many ROMs since it depends
 * on the ROM header (which varies, i.e. 7xxC and 7xxD are
 * are not quite like 7xxA). The strategy is
 * to start at the end of the idata section and work backwards
 * to find the "symtbl_size" value which is conveniently stored
 * right there.
 */

static void _find_symtbl(struct flashrom *flrom) {
	u32 idata_end = flrom->fh.idata_start + (flrom->fh.bss_start - flrom->fh.sdata);
	u32 cur = idata_end - ROM_BASE;	//conv to file offs
	u32 stop = cur - SYMTBL_MAXSDIST;

	for (; cur >= stop; cur -= 2) {
		u32 test = reconst_32(&flrom->rom[cur]);
		if ((test <= SYMSIZE_HIBOUND) && (test >= SYMSIZE_LOBOUND)) {
			flrom->sym_num = test;
			flrom->symloc = cur - (test * sizeof(struct sym_entry));
			return;
		}
	}
	/* if not found : fill bogus value that should at least not crash */
	flrom->sym_num = 1;
	flrom->symloc = 0;
	return;

}

void parse_romhdr(const uint8_t *buf, struct flashrom_hdr *fh) {
	fh->rominit = reconst_32(&buf[offsetof(struct flashrom_hdr, rominit)]);
	fh->bodyck_start = reconst_32(&buf[offsetof(struct flashrom_hdr, bodyck_start)]);
	fh->idata_start = reconst_32(&buf[offsetof(struct flashrom_hdr, idata_start)]);
	fh->sdata = reconst_32(&buf[offsetof(struct flashrom_hdr, sdata)]);
	fh->bss_start = reconst_32(&buf[offsetof(struct flashrom_hdr, bss_start)]);
	fh->body_cks = reconst_32(&buf[offsetof(struct flashrom_hdr, body_cks)]);
	fh->hdr_cks = reconst_16(&buf[offsetof(struct flashrom_hdr, hdr_cks)]);
	return;
}


void parse_sym(const u8 *buf, struct sym_entry *se) {
	se->p_name = reconst_32(&buf[offsetof(struct sym_entry, p_name)]);
	se->p_obj = reconst_32(&buf[offsetof(struct sym_entry, p_obj)]);
	return;
}


uint32_t find_sym(struct flashrom *flrom, uint32_t sympos, const uint8_t *name, size_t nlen) {
	const u8 *buf = flrom->rom;
	u32 siz = flrom->siz;
	u32 cur;

	for (cur = sympos; cur < siz; cur += sizeof(struct sym_entry)) {
		// cheat : don't parse whole entry

		u32 pname = reconst_32(&buf[cur + offsetof(struct sym_entry, p_name)]);
		pname -= ROM_BASE;	//now a file offset
		if ((pname + nlen) > siz) {
			//invalid; continue anyway
			continue;
		}
		if (memcmp(&buf[pname], name, nlen) == 0) {
			//found !
			return cur;
		}
	}
	return 0;
}

struct flashrom *loadrom(FILE *romfil) {
	u32 file_len;
	struct flashrom * flrom;

	rewind(romfil);
	file_len = _flen(romfil);
	if ((!file_len) || (file_len > file_maxsize)) {
		printf("huge file (length %lu)\n", (unsigned long) file_len);
		return NULL;
	}

	flrom = malloc(sizeof(struct flashrom));
	if (!flrom) {
		printf("malloc choke\n");
		return NULL;
	}

	u8 *src = malloc(file_len);
	if (!src) {
		free(flrom);
		printf("malloc choke\n");
		return NULL;
	}

	/* load whole ROM */
	if (fread(src,1,file_len,romfil) != file_len) {
		printf("trouble reading\n");
		free(flrom);
		free(src);
		return NULL;
	}

	flrom->rom = src;
	flrom->siz = file_len;

	parse_romhdr(src, &flrom->fh);
	_find_symtbl(flrom);

	return flrom;
}


void closerom(struct flashrom *flrom) {
	free(flrom->rom);
	free(flrom);
	return;
}


void print_rominfo(struct flashrom *flrom) {
	u32 idata_siz;

	printf("bodyck_start:\t%lX\n", (unsigned long) flrom->fh.bodyck_start);
	printf("idata_start:\t%lX\n", (unsigned long) flrom->fh.idata_start);
	printf("sdata:\t%lX\n", (unsigned long) flrom->fh.sdata);
	printf("bss_start:\t%lX\n", (unsigned long) flrom->fh.bss_start);

	idata_siz = flrom->fh.bss_start - flrom->fh.sdata;
	printf("idata siz:\t%lX\n", (unsigned long) idata_siz);
	printf("idata end:\t%lX\n", (unsigned long) flrom->fh.idata_start + idata_siz);

	printf("symtbl @\t%lX\n", (unsigned long) flrom->symloc);
	printf("sym entries:\t%lX\n", (unsigned long) flrom->sym_num);
	return;
}
