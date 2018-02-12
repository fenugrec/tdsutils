// mstring.idc
// (c) fenugrec 2018
//
//mark N consecutive items as strings


#include <idc.idc>
static main() {
	auto x,next;
		
	auto nbelems;
	nbelems=AskLong(0,"max # of str");
	
	if (nbelems<=0) {
		Warning("invalid");
		return;
	}
	for (x=1; x<=nbelems; x=x+1) {
		if (MakeStr(ScreenEA(), BADADDR) != 1) {
			return;
		}

		next = FindUnexplored(ScreenEA(), SEARCH_DOWN);
		if (next == BADADDR) {
			return;
		}
		Jump(next);
	}
}
