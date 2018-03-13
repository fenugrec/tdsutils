// extra bits
// (c) fenugrec 2018




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


//return # of elements of the jump table (jmploc must point at the jmp opcode)
//return BADADDR if failed
#define JTSIZE_MAXDIST	0x0E	//how far to backtrack
static get_jtsize(jmploc) {
	auto opc;
	auto next;

	// 1) backtrack to find either "moveq; cmp" or "cmp.i"

	//
	next = opsearch_bt(jmploc, JTSIZE_MAXDIST, 0xF038, 0xB000);
	if (next != BADADDR) {
		//CMP found. We need a moveq
		next = opsearch_bt(jmploc, JTSIZE_MAXDIST, 0xF100, 0x7000);
		if (next == BADADDR) {
			Message("not moveq / weird @ %X\n", jmploc);
			return BADADDR;
		}

		//get imm
		opc = Word(next);
		opc = (opc & 0x00FF);
	} else {
		//not CMP, maybe CMPI
		next = opsearch_bt(jmploc, JTSIZE_MAXDIST, 0xFF00, 0x0C00);
		if (next == BADADDR) {
			Message("no CMPI @ %X\n", jmploc);
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



//find main function tail after a jmp table.  (BADADDR if failed)
// Primary technique : iterate over function chunks to find the last.
// FindFuncEnd() doesn't work as expected (it returns the offset of the jmp opcode since
// that's how the func is initially defined)
//
//plan B: return the address of "exit" with this pattern:
//0000:0121AD42 6200 01CC                 bhi.w   exit
//0000:0121AD46 323B 0A06                 move.w  word_121AD4E(pc,d0.l*2),d1
//0000:0121AD4A 4EFB 1002                 jmp     word_121AD4E(pc,d1.w)
//0000:0121AD4E 0010 0096+word_121AD4E:	;jmp table here
//this breaks if bhi jumps before the jmptable !
#define BHI_MAXDIST	0x0E
static find_functail(jmploc) {
	auto bh_loc;
	auto disp;
	auto functail;
	auto cur;

	cur = FirstFuncFchunk(jmploc);	//start at func entry point
	functail = jmploc;

	while (cur != BADADDR) {
		cur = NextFuncFchunk(jmploc, cur);
		if (cur != BADADDR) {
			functail = cur;
		}
	}

	if (functail > jmploc) {
		functail = GetFchunkAttr(functail, FUNCATTR_END);
		Message("functail %X\n", functail);
		return functail;
	}

	//plan B: rely on bhi
	//bhi : 0x62 XX (8-bit disp) ; or 0x62 00 YY YY (16-bit disp)
	bh_loc = opsearch_bt(jmploc, BHI_MAXDIST, 0xFF00, 0x6200);
	if (bh_loc == BADADDR) {
		Message("no bhi\n");
		return BADADDR;
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
//"funcstart" can be any byte within the existing function
//(AppendFchunk requirement)
static add_chunks(funcstart, tblstart, numentries) {
	auto offs;
	auto cur;
	auto functail;
	auto cname;
	auto id;
	auto chunk_start, chunk_end;

	cur = tblstart;
	functail = find_functail(cur - 4);
	if (functail == BADADDR) return;

	for (id = 0; id < numentries; id = id + 1){
		offs = Word(cur);
		
		chunk_start = tblstart + offs;
		chunk_end = NextFchunk(chunk_start);
		//limit Append chunk bounds to smallest of functail or the next chunk.
		if ((chunk_end == BADADDR)  ||
			(chunk_end > functail)) {
			chunk_end = functail;
		}

		//make sure it's marked as code
		MakeCode(chunk_start);

		if (!AppendFchunk(funcstart, chunk_start, chunk_end)) {
			Message("failed to add chunk #%X (%X-%X)\n", id, chunk_start, chunk_end);
		}

		//name pattern : jmptbl_<entry#>
		cname = form("jmptbl_%02X", id);
		MakeNameEx(chunk_start, cname, SN_LOCAL | SN_NOWARN);

		cur = cur + 2;
	}	//while

	Wait();	//make sure analysis is finished
	return;
}

