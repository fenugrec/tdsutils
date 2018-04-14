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
	librd-> ptype = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, ptype)]) - ROM_BASE;
	librd-> psize = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, psize)]) - ROM_BASE;
	librd-> poffs = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, poffs)]) - ROM_BASE;
	librd-> pfactory = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, pfactory)]) - ROM_BASE;
	librd-> membase = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, membase)]);
	librd-> size1 = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, size1)]);
	librd-> size2 = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, size2)]);
	librd-> psavefunc = reconst_32(&flrom->rom[fileoffs + offsetof(struct libr_descr, psavefunc)]) - ROM_BASE;
	return;
}

/** calculate checksum for nvram log area
 *
 * @param log_pos offset within nvdata
 */
static u16 _getLogChksum(const u8 *nvdata, unsigned log_pos) {
	unsigned idx;
	u16 cks = 0;
	for (idx=2; idx < 0x10; idx += 2) {
		cks += reconst_16(&nvdata[log_pos + idx]);
	}
	return ~cks;
}

/** print info about a log area header
 *
 * @param log_pos offset within nvdata
 */
static void dump_loghdr(const u8 *nvdata, unsigned log_pos) {
	struct log_hdr lh;
	u16 cks_calc;

	lh.log_cks = reconst_16(&nvdata[log_pos + offsetof(struct log_hdr, log_cks)]);
	lh.firstnum = reconst_16(&nvdata[log_pos + offsetof(struct log_hdr, firstnum)]);
	lh.nextw = reconst_16(&nvdata[log_pos + offsetof(struct log_hdr, nextw)]);
	lh.type = reconst_16(&nvdata[log_pos + offsetof(struct log_hdr, type)]);
	lh.maxnum = reconst_16(&nvdata[log_pos + offsetof(struct log_hdr, maxnum)]);

	cks_calc = _getLogChksum(nvdata, log_pos);
	printf("log cks=%04X, want %04X; firstnum=%04X, nextw=%04X, type=%04X, maxnum?=%04X\n",
			(unsigned) cks_calc, (unsigned) lh.log_cks, (unsigned) lh.firstnum,
			(unsigned) lh.nextw, (unsigned) lh.type, (unsigned) lh.maxnum);
	return;
}


/** check librarian checksum
 *
 * ret 1 if ok */
static bool verify_checksum(struct libr_descr *librd, const u8 *nvdata, u32 nvsiz) {
	u32 nvram_offs;
	u32 size;
	u16 cks = 0;
	u16 want;
	u32 cur;

	// 1- check if "guard" bytes match declared size

	nvram_offs = librd->membase - NVRAM_BASE;
	if (((nvram_offs + librd->size1) >= nvsiz) ||
		(nvram_offs <= 2)) {
		printf("librd out of bounds\n");
		return 0;
	}
	size = reconst_32(&nvdata[nvram_offs + librd->size1 - 4]);
	if (size != librd->size1) {
		printf("size mismatch\n");
		return 0;
	}

	// 2- _libCheckSumMem
	for (cur = 0; cur < librd->size1; cur += 2) {
		cks += reconst_16(&nvdata[nvram_offs + cur]);
	}
	want = reconst_16(&nvdata[nvram_offs - 2]);
	if (cks != want) {
		printf("bad cks : wanted %02X, got %02X\n", (unsigned) want, (unsigned) cks);
		return 0;
	}
	printf("checksum OK\n");
	return 1;
}


static void dump_u8(const u8 *buf, unsigned bytes) {
	unsigned cur = 0;
	while (bytes) {
		printf("%02X ", (unsigned) buf[cur]);
		cur++;
		bytes -= 1;
	}
}
static void dump_u16(const u8 *buf, unsigned bytes) {
	u16 val;
	unsigned cur = 0;
	while (bytes) {
		val = reconst_16(&buf[cur]);
		printf("%04X ", (unsigned) val);
		cur += 2;
		bytes -= 1;
	}
}
static void dump_u32(const u8 *buf, unsigned bytes) {
	u32 val;
	unsigned cur = 0;
	while (bytes) {
		val = reconst_32(&buf[cur]);
		printf("%08X ", (unsigned) val);
		cur += 4;
		bytes -= 4;
	}
}

static void dump_onelibr(struct flashrom *flrom, u32 fileoffs, const u8 *nvdata, u32 nvsiz) {
	struct libr_descr librd;
	u32 nvram_offs;	//pos within nvram

	parse_librdescr(flrom, fileoffs, &librd);

	printf("librarian @ %lX:\tmembase=%lX, size1=%lX, size2=%lX\n",
			(unsigned long) fileoffs, (unsigned long) librd.membase,
			(unsigned long) librd.size1, (unsigned long) librd.size2);
	if ((librd.membase < NVRAM_BASE) || (librd.membase >= (NVRAM_BASE + NVRAM_MAXSIZ))) {
		printf("outside NVRAM, skipping\n");
		return;
	}

	verify_checksum(&librd, nvdata, nvsiz);

	/* To parse all librarian entries, iterate through offsets array until we reach
	 * "size1".
	 */
	printf("#item\tpos\t\ttype\tsize(B)\tdata\n");

	nvram_offs = librd.membase - NVRAM_BASE;
	unsigned idx;

	for (idx = 0; ; idx += 1) {
		u8 type = flrom->rom[librd.ptype + idx];
		u16 size = reconst_16(&flrom->rom[librd.psize + 2 * idx]);
		u32 offs = reconst_32(&flrom->rom[librd.poffs + 4 * idx]);

		if (offs >= librd.size1) {
			//done
			break;
		}
		if ((offs + size) >= nvsiz) {
			printf("%X\t%06X\t%X\t%X\t<outside NVRAM!?>\n",
					idx, (unsigned) nvram_offs + offs, (unsigned) type, (unsigned) size);
			continue;
		}
		printf("%X\t%06X\t%X\t%X\t",
					idx, (unsigned) nvram_offs + offs, (unsigned) type, (unsigned) size);
		switch (type) {
		case LIBR_TYPE_U8:
			dump_u8(&nvdata[nvram_offs + offs], size);
			break;
		case LIBR_TYPE_U16:
			dump_u16(&nvdata[nvram_offs + offs], size);
			break;
		case LIBR_TYPE_U32:
			dump_u32(&nvdata[nvram_offs + offs], size);
			break;
		case LIBR_TYPE_4:
			dump_u32(&nvdata[nvram_offs + offs], size);
			break;
		default:
			dump_u8(&nvdata[nvram_offs + offs], size);
			break;
		}
		printf("\n");
	}
	return;
}



static void dump_libs(struct flashrom *flrom, const u8 *nvdata, u32 nvsiz) {
	u32 librcoll_pos;
	unsigned lib_index = 0;
	u32 cur;

	if (!pm_parse32(flrom, &librcoll_pos, ROM_BASE, SYM_LIBRCOLL)) {
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
		"--logpos  \t-l <offs>\toffset within NVRAM of log header (optional)\n"
		"");
}


int main(int argc, char * argv[]) {
	struct flashrom *flrom;
	u8 nvdata[NVRAM_MAXSIZ];
	u32 nvsiz;
	u32 logpos = 0;

	char c;
	int optidx;
	FILE *file = NULL;
	FILE *nvfile = NULL;

	printf(	"**** %s\n"
		"**** (c) 2018 fenugrec\n", argv[0]);

	while((c = getopt_long(argc, argv, "f:n:l:h",
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
		case 'l':
			if (sscanf(optarg, "%x", &logpos) != 1) {
				printf("did not understand %s\n", optarg);
				logpos = 0;
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
	if ((logpos) && ((logpos + sizeof(struct log_hdr)) < nvsiz)) {
		dump_loghdr(nvdata, logpos);
	}

	dump_libs(flrom, nvdata, nvsiz);
	return 0;

bad_exit:
	if (file) {
		fclose(file);
	}
	return 1;
}

