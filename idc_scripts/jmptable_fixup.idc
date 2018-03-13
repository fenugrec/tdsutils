// fixup jmp tables that don't have all (or any) chunks appended to the function
// Run with the cursor on the "jmp" opcode prior to table
//
// bonus : defines weak names for each chunk

// (c) fenugrec 2018


#include <idc.idc>

#include "helpers.idc"


//parse jmp table entries to add each chunk to the given func.
//tblstart is the first byte of the array of offsets
//"funcstart" can be any byte within the existing function
//(AppendFchunk requirement)
static add_chunks(funcstart, tblstart, numentries) {
	auto offs;
	auto cur;
	auto functail;
	auto cname;
	auto id;

	cur = tblstart;
	functail = find_functail(cur);

	for (id = 0; id < numentries; id = id + 1){
		offs = Word(cur);
		
		Message("adding chunk #%X (%X-%X)\n", id, tblstart + offs, functail);
		AppendFchunk(funcstart, tblstart + offs, functail);

		//name pattern : jmptbl_<entry#>
		cname = form("jmptbl_%02X", id);
		MakeNameEx(tblstart + offs, cname, SN_LOCAL);

		cur = cur + 2;
	}	//while

	return;
}


static main() {
	auto cur, end, next;
	auto pr;
	auto opc;
	auto jtsize;
	auto functail;

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
