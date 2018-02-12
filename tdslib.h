/* collection of funcs for working with Tektronix TDS7xx ROMs
 * (c) fenugrec 2018
 * GPLv3
 */

#include <stdio.h>	//just for FILE
#include <stdint.h>

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

/** get file length but restore position */
uint32_t flen(FILE *hf);

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
