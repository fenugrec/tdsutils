//mark a function containing a jmp table
// (normally fails due to the jmp table breaking code flow)

// jumps to 
// (c) fenugrec 2018


#include <idc.idc>

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


//return # of elements of the jump table that starts @ cur
//return BADADDR if failed
static get_jtsize(cur) {
	auto opc;
	auto next;

	// 1) backtrack to find either "moveq; cmp" or "cmp.i"
	next = opsearch_bt(cur, 0x0C, 0xF038, 0xB000);
	if (next != BADADDR) {
		//CMP found. We need a moveq
		next = opsearch_bt(cur, 0x0E, 0xF100, 0x7000);
		if (next == BADADDR) {
			Message("not moveq / weird @ %X\n", cur);
			return BADADDR;
		}

		//get imm
		opc = Word(next);
		opc = (opc & 0x00FF);
	} else {
		//not CMP, maybe CMPI
		next = opsearch_bt(cur, 0x0E, 0xFF00, 0x0C00);
		if (next == BADADDR) {
			Message("no CMPI @ %X\n", cur);
			return BADADDR;
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
			return BADADDR;
		}
	}
	opc = opc + 1;	//code uses bhi, so table size is + 1
	return opc;
}





//find main function tail.  
//i.e. return the address of "exit" (BADADDR if failed)
//0000:0121AD42 6200 01CC                 bhi.w   exit
//0000:0121AD46 323B 0A06                 move.w  word_121AD4E(pc,d0.l*2),d1
//0000:0121AD4A 4EFB 1002                 jmp     word_121AD4E(pc,d1.w)
//0000:0121AD4E 0010 0096+word_121AD4E:	;jmp table here
#define BHI_MAXDIST	0x0E
static find_functail(jmploc) {
	auto bh_loc;
	auto disp;

	//bhi : 0x62 XX (8-bit disp) ; or 0x62 00 YY YY (16-bit disp)
	bh_loc = opsearch_bt(jmploc, BHI_MAXDIST, 0xFF00, 0x6200);
	if (bh_loc == BADADDR) {
		Message("no bhi\n");
		return bh_loc;
	}

	// calculate displacement
	disp = Byte(bh_loc + 1);
	if (disp == 0) {
		disp = Word(bh_loc + 2);
	}
	return (bh_loc + disp + 2);
}




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
