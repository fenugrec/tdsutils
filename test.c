/* simple self-test rig for tdslib.
 * individual tests ret 1 if ok
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stypes.h"
#include "tdslib.h"


/* sample flash ROM header, taken from TDS744A 1.1e */
static const u8 sample_flash_hdr[0x1aa] = {
  0x4E, 0x71, 0x4E, 0xF9, 0x01, 0x00, 0x17, 0x4E, 0x01, 0x00, 0x01, 0xAA, 0x01, 0x27, 0x50, 0xAA,
  0x05, 0x00, 0x10, 0x00, 0x05, 0x03, 0x73, 0xD0, 0x9A, 0x8A, 0x01, 0x00, 0x00, 0x2C, 0x01, 0x00,
  0x01, 0xB0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x36, 0x6E, 0x43, 0x4F, 0x50, 0x59,
  0x52, 0x49, 0x47, 0x48, 0x54, 0x20, 0x54, 0x45, 0x4B, 0x54, 0x52, 0x4F, 0x4E, 0x49, 0x58, 0x20,
  0x49, 0x4E, 0x43, 0x2C, 0x20, 0x31, 0x39, 0x38, 0x39, 0x2C, 0x31, 0x39, 0x39, 0x30, 0x2C, 0x31,
  0x39, 0x39, 0x31, 0x2C, 0x31, 0x39, 0x39, 0x32, 0x2C, 0x31, 0x39, 0x39, 0x33, 0x2C, 0x31, 0x39,
  0x39, 0x34, 0x00, 0x00, 0x43, 0x43, 0x4F, 0x4F, 0x50, 0x50, 0x59, 0x59, 0x52, 0x52, 0x49, 0x49,
  0x47, 0x47, 0x48, 0x48, 0x54, 0x54, 0x20, 0x20, 0x54, 0x54, 0x45, 0x45, 0x4B, 0x4B, 0x54, 0x54,
  0x52, 0x52, 0x4F, 0x4F, 0x4E, 0x4E, 0x49, 0x49, 0x58, 0x58, 0x20, 0x20, 0x49, 0x49, 0x4E, 0x4E,
  0x43, 0x43, 0x2C, 0x2C, 0x31, 0x31, 0x39, 0x39, 0x38, 0x38, 0x39, 0x39, 0x2C, 0x2C, 0x31, 0x31,
  0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x2C, 0x2C, 0x31, 0x31, 0x39, 0x39, 0x39, 0x39, 0x31, 0x31,
  0x2C, 0x2C, 0x31, 0x31, 0x39, 0x39, 0x39, 0x39, 0x32, 0x32, 0x2C, 0x2C, 0x31, 0x31, 0x39, 0x39,
  0x39, 0x39, 0x33, 0x33, 0x2C, 0x2C, 0x31, 0x31, 0x39, 0x39, 0x39, 0x39, 0x34, 0x34, 0x00, 0x00,
  0x00, 0x43, 0x43, 0x43, 0x43, 0x4F, 0x4F, 0x4F, 0x4F, 0x50, 0x50, 0x50, 0x50, 0x59, 0x59, 0x59,
  0x59, 0x52, 0x52, 0x52, 0x52, 0x49, 0x49, 0x49, 0x49, 0x47, 0x47, 0x47, 0x47, 0x48, 0x48, 0x48,
  0x48, 0x54, 0x54, 0x54, 0x54, 0x20, 0x20, 0x20, 0x20, 0x54, 0x54, 0x54, 0x54, 0x45, 0x45, 0x45,
  0x45, 0x4B, 0x4B, 0x4B, 0x4B, 0x54, 0x54, 0x54, 0x54, 0x52, 0x52, 0x52, 0x52, 0x4F, 0x4F, 0x4F,
  0x4F, 0x4E, 0x4E, 0x4E, 0x4E, 0x49, 0x49, 0x49, 0x49, 0x58, 0x58, 0x58, 0x58, 0x20, 0x20, 0x20,
  0x20, 0x49, 0x49, 0x49, 0x49, 0x4E, 0x4E, 0x4E, 0x4E, 0x43, 0x43, 0x43, 0x43, 0x2C, 0x2C, 0x2C,
  0x2C, 0x31, 0x31, 0x31, 0x31, 0x39, 0x39, 0x39, 0x39, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39,
  0x39, 0x2C, 0x2C, 0x2C, 0x2C, 0x31, 0x31, 0x31, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
  0x39, 0x30, 0x30, 0x30, 0x30, 0x2C, 0x2C, 0x2C, 0x2C, 0x31, 0x31, 0x31, 0x31, 0x39, 0x39, 0x39,
  0x39, 0x39, 0x39, 0x39, 0x39, 0x31, 0x31, 0x31, 0x31, 0x2C, 0x2C, 0x2C, 0x2C, 0x31, 0x31, 0x31,
  0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x32, 0x32, 0x32, 0x32, 0x2C, 0x2C, 0x2C,
  0x2C, 0x31, 0x31, 0x31, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x33, 0x33, 0x33,
  0x33, 0x2C, 0x2C, 0x2C, 0x2C, 0x31, 0x31, 0x31, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
  0x39, 0x34, 0x34, 0x34, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00 };

bool test_parse_romhdr(void) {
	struct flashrom_hdr fh = {0};
	struct flashrom_hdr want = {
		.rominit = 0x100174e,
		.bodyck_start = 0x10001aa,
		.idata_start = 0x12750aa,
		.sdata = 0x5001000,
		.bss_start = 0x50373d0,
		.body_cks = 0x9a8a0100,
		.hdr_cks = 0x366e
		// in theory, unlisted initializers here should be initted to 0 ? C99 6.7.8 #19
	};

	parse_romhdr(sample_flash_hdr, &fh);

	if (memcmp(&fh, &want, sizeof(struct flashrom_hdr)) != 0) {
		printf("parse_romhdr mismatch !\n");
		return 0;
	}
	return 1;
}



/* sample symlist entry, taken from TDS744A 1.1e */
static const u8 sample_sym[] = {
  0x00, 0x00, 0x00, 0x00, 0x01, 0x27, 0x47, 0x72, 0x01, 0x25, 0x72, 0xEE, 0x05, 0x00 };

bool test_parse_sym(void) {
	struct sym_entry se = {0};
	struct sym_entry want = {
		.p_name = 0x1274772,
		.p_obj = 0x12572ee
	};

	parse_sym(sample_sym, &se);

	if (memcmp(&se, &want, sizeof(struct sym_entry)) != 0) {
		printf("parse_sym mismatch !\n");
		return 0;
	}
	return 1;

}

int main(int argc, char **argv) {
	(void) argc;
	(void) argv;

	test_parse_romhdr();
	test_parse_sym();
	return 0;
}
