/* collection of funcs for working with Tektronix TDS7xx ROMs (5xx/6xx probably also)
 * (c) fenugrec 2018
 * GPLv3
 */

#include <stdint.h>
#include <string.h>
#include "stypes.h"

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
u32 flen(FILE *hf) {
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
