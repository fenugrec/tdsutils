//mark a function containing a jmp table
// (normally fails due to the jmp table breaking code flow)

// searches next non-function code
// (c) fenugrec 2018


#include <idc.idc>

#include "helpers.idc"



//parse jmp table entries to add each chunk to the given func.
//tblstart is the first byte of the array of offsets
static add_chunks(funcstart, tblstart, numentries) {
	auto offs;
	auto cur;
	auto functail;

	cur = tblstart;
	functail = find_functail(cur);

	while (numentries > 0) {
		offs = Word(cur);
		
		Message("adding chunks %X-%X\n", tblstart + offs, functail);
		AppendFchunk(funcstart, tblstart + offs, functail);

		numentries = numentries - 1;
		cur = cur + 2;
	}	//while

	return;
}




//mark the func @ cur manually by finding problematic jmp table.
//Advances cursor to end of new func; ret 1 to continue, 0 to cancel
#define MAX_TBLDIST	0x180	//jmptable must be close to func entry
static markfunc(start) {
	auto end;
	auto opc;
	auto cur;
	auto pr;
	auto tblsiz;

	//look for "jmp tbl(pc, dX.w)" sequence
	cur = start;
	end = start + MAX_TBLDIST;
	for (; cur < end; cur = cur + 2) {

		opc = Word(cur);

		//opcode must be "JMP (d8, PC, Xn)"
		if (opc != 0x4EFB) {
			continue;
		}

		opc = Word(cur + 2);
		//"brief extension word" : must be (PC + (dx * 1) + 2)
		if ((opc & 0x8FFF) != 0x0002) {
			continue;
		}
		//found a table. check # of elems
		tblsiz = get_jtsize(cur);
		if (tblsiz == BADADDR) {
			return 1;
		}
		// The jmp was @ "cur"; we'll use this as a function end.
		pr=AskYN(1, form("Mark func %X-%X[%X]?", start, cur, tblsiz));
		if (pr == -1) {
			//Cancel.
			return 0;
		}
		if (pr == 1) {
			MakeFunction(start, cur + 2);
			//also add chunks for all the tails
			add_chunks(start, cur + 4, tblsiz);
			
			return 1;
		}
		return 1;
	}	//for
	Message("couldn't find jmptable @ %X\n", start);
	return 1;
}

static findfuncs(cur, end) {
	auto tflags;

	auto name;
 
	while (cur < end) {

		// JumpNotFunction AAAAAAA can't be called from IDC !

		cur = FindCode(cur, SEARCH_DOWN); 
		if (cur == BADADDR) {
			return;
		}

		name = GetFunctionName(cur);
		if (name != "") {
			//within a func : jump to end - 2
			cur = FindFuncEnd(cur);
			if (cur == BADADDR) {
				Message("problem?\n");
				return;
			}
			cur = cur - 2;	//ugliness so that FindCode lands on the next func start
			continue;
		}

		//so, probably a func that IDA can't parse due to a jmp table.
		Jump(cur);
		if (markfunc(cur) == 0) {
			return;
		}
	}	//while
}

static main() {
	auto cur, end, next;
	auto pr;
	auto opc, ext;

	cur = ScreenEA() - 2;

	if (cur & 1) {
		Message("can't work on odd addr\n");
	}
	
	end = SegEnd(cur);
	if (end == BADADDR) {
		Message("outside of a seg ?");
		return;
	}

	findfuncs(cur, end);
	return;
}
