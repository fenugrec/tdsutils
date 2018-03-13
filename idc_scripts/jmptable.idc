//jmptable

// find jump tables and mark as data + code ; IDA isn't clever enough
// note, doesn't mark the function. See func_withjmptbl.idc
// (c) fenugrec 2018


#include <idc.idc>

#include "helpers.idc"

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
			//sign extend ? such not !
			Message("fix this ! sign extend or not ?\n");
			return;
			dest = dest | ~0xFFFF;
		}
		MakeCode(cur + dest);
		elems = elems - 1;
	}
	return;
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

		opc = get_jtsize(cur);
		if (opc == BADADDR) {
			continue;
		}

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
