/* collection of funcs for working with Tektronix TDS7xx ROMs
 * (c) fenugrec 2018
 * GPLv3
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>	//just for FILE
#include <stdint.h>

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

/********** ROM structures ******/

#define ROM_BASE 0x01000000UL	//flash ROM loaded address


/** first 0x2C bytes of a flash ROM.
 * the sizeof() of this struct may be incorrect
 */
struct flashrom_hdr {
	uint16_t	nop;	/* dummy */
	uint16_t	jmp;	/* dummy */
	uint32_t rominit;	/* points to _romInit entry point */
	uint32_t bodyck_start;	/* startCheckSumming */
	uint32_t	idata_start;	/* initialized data in ROM */
	uint32_t	sdata;		/* destination for idata in RAM */
	uint32_t	bss_start;	/* in RAM */
	uint32_t	body_cks;
	uint8_t	unk[14];	/* some padding & garbage */
	uint16_t	hdr_cks;
	/* arguably, the 3 copyright strings are probably part of the hdr */
};


/** general flash ROM metadata.
 * Includes the header and symtable info
 */
struct flashrom {
	u8 *rom;
	uint32_t siz;

	struct flashrom_hdr fh;

	u32 symloc;	//file offset of symbol table
	u32 sym_num;	//number of symbol entries
};


/** struct SYMBOL in vxworks "symbol.h"
 * it was user-customizable and seems to have changed from vx 5.0 to vx 5.4.
 *
 * in  TDS744 1.1e, sizeof == 0x0E.
 */

struct sym_entry {
	uint32_t unk_0;	//always 0?
	uint32_t p_name;	/* string */
	uint32_t p_obj;	/* addr of item */
	uint8_t type;	/* not sure; 5 = code, 7 = idata, 9=bss ? */
	uint8_t unk_1;	/* ?? */
} __attribute__ ((packed));

_Static_assert(sizeof(struct sym_entry) == 0x0E, "bad sym_entry size\n");


/** librarian descriptor
 *
 * For convenience, "p" values are saved in here as file offsets
 */
struct libr_descr {
	uint32_t ptype;	//array of "types" for each item. 3 = uint32 ? 2 = uint16?
	uint32_t psize;	//array of "sizes" for each item, in bytes. An item can be an array, e.g. type=2 and size=1A
	uint32_t poffs;	//array of "offsets" of each item (added to membase)
	uint32_t pfactory;	//array of default values (factory init)
	uint32_t membase;	//such as 0x4000100
	uint32_t size1;		//seems used more often: checksum, factory initialization,
	uint32_t size2;	//always == size1 ?
	uint32_t flags;	//not sure
	uint32_t psavefunc;	//not sure
} __attribute__ ((packed));

_Static_assert(sizeof(struct libr_descr) == 0x24, "bad libr_descr size\n");

/* tentative defs for the ptype array */
#define LIBR_TYPE_U8 1
#define LIBR_TYPE_U16 2
#define LIBR_TYPE_U32 3
//#define LIBR_TYPE_FLOAT ?




/********** useful funcs ********/

/** load ROM, parse header and return new fh struct.
 *
 * caller must call closerom()
 */
struct flashrom *loadrom(FILE *romfil);

/** close ROM and free resources */
void closerom(struct flashrom *flrom);

/** parse flash ROM header into fh struct */
void parse_romhdr(const uint8_t *buf, struct flashrom_hdr *fh);

/** print ROM info and metadata */
void print_rominfo(struct flashrom *flrom);

/** parse single symbol table entry */
void parse_sym(const uint8_t *buf, struct sym_entry *se);

/** Read uint32 at *buf bigendian
*/
uint32_t reconst_32(const uint8_t *buf);

/** Read uint16 at *buf bigendian
*/
uint16_t reconst_16(const uint8_t *buf);

/** write uint32 at *buf bigendian
*/
void write_32b(uint32_t val, uint8_t *buf);

/** search a <buflen> u8 buffer for a <len>-byte long sequence.
 *
 * @param buflen size in bytes of *buf
 * @param needle pattern to search
 * @param nlen size of "needle" pattern
 * @return NULL if not found
 *
 * Painfully unoptimized, because it's easy to get it wrong
 */
const uint8_t *u8memstr(const uint8_t *buf, uint32_t buflen, const uint8_t *needle, unsigned nlen);

const uint8_t *u16memstr(const uint8_t *buf, uint32_t buflen, const uint16_t needle);

/** search a <buflen> u8 buffer for a 32-bit aligned uint32_t value, in SH endianness
 *
 */
const uint8_t *u32memstr(const uint8_t *buf, uint32_t buflen, const uint32_t needle);


/** Find opcode pattern... bleh
 * "patlen" is # of opcodes
 *
 * @return (u32) -1 if failed
 */
uint32_t find_pattern(const uint8_t *buf, uint32_t siz, unsigned patlen,
			const uint16_t *pat, const uint16_t *mask);

/** iterate through symbol table trying to match name.
 *
 * @param siz in bytes
 * @param nlen strlen(name)
 * @return file offset of entry; 0 if not found
 */
uint32_t find_sym(struct flashrom *flrom, const uint8_t *name, size_t nlen);



/** helper for u16 symbol parsage
 *
 * 1) find symbol entry with name *sym
 * 2) get u16 value at p_obj
 *
 * @return 1 if ok
 */
bool pm_parse16v(struct flashrom *flrom, uint16_t *dest, const char *sym);

/** helper for u32 symbol parsage
 *
 * 1) find symbol entry with name *sym
 * 2) get file ofs of obj, since caller may not need the value at *pobj
 */
bool pm_parse32(struct flashrom *flrom, uint32_t *dest, const char *sym);
