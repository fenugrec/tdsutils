//align2

// level 1 - clean up 0x00 padding, i.e. two "dc.b 0" in a row surrounded by defined stuff
// (c) fenugrec 2018


#include <idc.idc>

// check if single 00 byte, surrounded by valid data
// ret  -1:cancel,0-no,1-ok
static check_align2(cur) {
	auto tflags, pr;

	// check previous and after for code/data
	tflags = GetFlags(cur - 1);
//	Message("tf prev @%X =%X\n", cur, tflags);
	if (isUnknown(tflags))return;

	tflags = GetFlags(cur + 1);
//	Message("tf next=%X\n", tflags);
	if (isUnknown(tflags)) return;

//	Message("got 1 @ %X\n", cur);

	//prompt only if there's xrefs to it
	pr = 1;
	if ((RfirstB0(cur) != BADADDR) || (DfirstB(cur) != BADADDR)) {
		pr=AskYN(1, form("got 1 shared @ %X", cur));
	}
	if (pr == 1) {
		//pr = 
		MakeAlign(cur, 1, 0);
		//if (!pr) return -1;
	}
	return pr;
}

// check if pair of 00 bytes, surrounded by valid data
static check_align4(cur) {
	auto tflags, pr;

	// check previous and after for code/data
	tflags = GetFlags(cur - 2);
//	Message("tf prev @%X =%X\n", cur, tflags);
	if (isUnknown(tflags))return;

	tflags = GetFlags(cur + 2);
//	Message("tf next=%X\n", tflags);
	if (isUnknown(tflags)) return;

	//prompt only if there's xrefs to it
	pr = 1;
	if ((RfirstB0(cur) != BADADDR) || (DfirstB(cur) != BADADDR)) {
		pr=AskYN(1, form("got 2 shared @ %X", cur));
	}

	if (pr == 1) {
		//pr = 
		MakeAlign(cur, 2, 0);
		//if (!pr) return -1;
	}
	return pr;
}



static main() {
	auto occ;
	auto cur, end;
	auto tflags;
	auto pr;

	cur = ScreenEA();
	
	end = SegEnd(cur);
	if (end == BADADDR) {
		Message("outside of a seg ?");
		return;
	}

	while (cur < end) {
		// jump to next transition from defined -> undefined
		cur = FindExplored(ScreenEA(), SEARCH_DOWN);
		if (cur == BADADDR) {
			return;
		}
		Jump(cur);
		
		cur = FindUnexplored(ScreenEA(), SEARCH_DOWN);
		if (cur == BADADDR) {
			return;
		}
		Jump(cur);

		// must be a 0x00
		if (Byte(cur) != 0) {
			continue;
		}

		 // odd address ?
		if (cur & 1) {
			if (check_align2(cur) == -1) return;
			continue;
		}

		// even address, must have a second 0x00
		if (Byte(cur + 1) != 0) {
			continue;
		}
		if (check_align4(cur) == -1) return;

	}
}
