// 
// (c) fenugrec 2018
//
// find  typical function prologue "link a6, #0" (opcode 4E 56)
// skips unaligned hits, and already-defined stuff


#include <idc.idc>

static findfuncprol(cur, end) {
	auto tflags;
	auto pr;
	auto occ;

	occ = 0;

	while (cur < end) {
		cur = FindBinary(cur, SEARCH_DOWN | SEARCH_NEXT | SEARCH_CASE, "4E 56");
		if (cur == BADADDR) {
			break;
		}
		Jump(cur);

		tflags = GetFlags(cur);
		if ( !isUnknown(tflags)) {
			//Message("not unknown @ %X\n", cur);
			//already explored
			continue;
		}

		if (cur & 0x01) {
			Message("odd addr @ %X\n", cur);
			//odd address : not code
			continue;
		}

		pr=AskYN(1, form("mark probable func @ %X?", cur));
		if (pr == -1) {
			//Cancel
			break;
		}
		if (pr == 1) {
			occ = occ + 1;
			MakeFunction(cur, BADADDR);
		}
	}	//while

	return occ;
}


static main() {
	auto cur, end;

	cur = ScreenEA();
	
	end = SegEnd(cur);
	if (end == BADADDR) {
		Message("outside of a seg ?\n");
		return;
	}

	findfuncprol(cur, end);
	return;
}
