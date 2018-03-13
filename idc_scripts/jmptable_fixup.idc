// fixup jmp tables that don't have all (or any) chunks appended to the function
// Run with the cursor on the "jmp" opcode prior to table
//
// bonus : defines weak names for each chunk

// (c) fenugrec 2018


#include <idc.idc>

#include "helpers.idc"


static main() {
	auto cur, end, next;
	auto pr;
	auto opc;
	auto jtsize;

	cur = ScreenEA();

	if (cur & 1) {
		Message("can't work on odd addr\n");
		return;
	}

	opc = Word(cur);

	//opcode must be "JMP (d8, PC, Xn)"
	if (opc != 0x4EFB) {
		Message("must be on the JMP\n");
		return;
	}

	opc = Word(cur + 2);
	//"brief extension word" : must be (PC + (dx * 1) + 2)
	if ((opc & 0x8FFF) != 0x0002) {
		Message("bad JMP\n");
		return;
	}

	jtsize = get_jtsize(cur);
	if (jtsize == BADADDR) return;

	add_chunks(cur, cur + 4, jtsize);
	
	return;
}
