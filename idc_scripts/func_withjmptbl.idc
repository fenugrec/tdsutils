//mark a function containing a jmp table
// (normally fails due to the jmp table breaking code flow)

// jumps to 
// (c) fenugrec 2018


#include <idc.idc>

//mark the func @ cur manually by finding problematic jmp table.
//Advances cursor to end of new func
#define MAX_TBLDIST	0x180	//jmptable must be close to func header
static markfunc(start) {
	auto end;
	auto opc;
	auto cur;

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
		
		//found a table. The jmp is @ "cur"; we use this as a function end.
		MakeFunction(start, cur + 2);
		return;
	}	//for
	Message("couldn't find jmptable @ %X\n", start);
	return;
}

static findfuncs(cur, end) {
	auto tflags;
	auto pr;
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

		//so, probably a func that failed due to a jmp table.
		Jump(cur);
		pr=AskYN(1, form("Mark func @ %X ?", cur));
		if (pr == -1) {
			//Cancel
			return;
		}
		if (pr == 1) {
			markfunc(cur);
			//debug : just one at a time
			return;
		}
	}
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
