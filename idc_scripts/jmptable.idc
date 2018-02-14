//jmptable

// find jump tables ; IDA isn't clever enough
// (c) fenugrec 2018


#include <idc.idc>

//parse jmp table, marking code for every index.
//cur must be the first offset of the table (i.e. right after JMP ...)
static parse_table(cur, elems) {
	auto dest;

	if (!MakeWord(cur)) {
		Message("can't define table @ %X\n", cur);
	}

	if (!MakeArray(cur, elems)) {
		Message("can't create array @ %X\n", cur);
	}

	elems = elems - 1;
	while (elems >= 0) {
		dest = Word(cur + (elems * 2));
		if (dest & 0x8000) {
			//sign extend
			dest = dest | ~0xFFFF;
		}
		MakeCode(cur + dest);
		elems = elems - 1;
	}
	return;
}

//search backwards to find an opcode that verifies ((opc & mask) == val).
//Look back within "maxdist"; return position if found, BADADDR otherwise
static opsearch_bt(cur, maxdist, mask, val) {
	auto opc, end;
	end = cur - maxdist;

	for (; cur >= end; cur = cur - 2) {
		opc = Word(cur);
		if ((opc & mask) == val) {
			return cur;
		}
	}
	return BADADDR;
}

static main() {
	auto cur, end, next;
	auto pr;
	auto opc, ext;

	cur = ScreenEA();

	if (cur & 1) {
		Message("can't work on odd addr\n");
	}
	
	end = SegEnd(cur);
	if (end == BADADDR) {
		Message("outside of a seg ?");
		return;
	}

	for (; cur < end; cur = cur + 2) {

		opc = Word(cur);

		//opcode must be "JMP (d8, PC, Xn)"
		if (opc != 0x4EFB) {
			continue;
		}

		ext = Word(cur + 2);
		//"brief extension word" : must be (PC + (dx * 1) + 2)
		if ((ext & 0x8FFF) != 0x0002) {
			continue;
		}

		//two cases : either "moveq; cmp" or "cmp.i"


		next = opsearch_bt(cur, 0x0C, 0xF038, 0xB000);
		if (next != BADADDR) {
			//CMP found. We need a moveq
			next = opsearch_bt(cur, 0x0E, 0xF100, 0x7000);
			if (next == BADADDR) {
				Message("not moveq / weird @ %X\n", cur);
				continue;
			}

			//get imm
			opc = Word(next);
			opc = (opc & 0x00FF);

		} else {
			//not CMP, maybe CMPI
			next = opsearch_bt(cur, 0x0E, 0xFF00, 0x0C00);
			if (next == BADADDR) {
				Message("no CMPI @ %X\n", cur);
				continue;
			}

			//it is a cmp.i; get immediate according to size
			opc = (Word(next) & 0xC0) >> 6;
			if (opc == 0) {
				opc = Byte(next + 3);
			} else if (opc == 1) {
				opc = Word(next + 2);
			} else if (opc == 2) {
				opc = ((Word(next + 2) << 16) | Word(next + 4));
			} else {
				Message("bad cmpi lit\n");
				continue;
			}
		}

		opc = opc + 1;	//code uses bhi, so table size is + 1
		Jump(cur);
		pr=AskYN(1, form("tbl[%X] @ %X", opc, cur));
		if (pr == -1) {
			//Cancel
			return;
		}
		if (pr == 1) {
			parse_table(cur + 4, opc);
		}
	}
}
