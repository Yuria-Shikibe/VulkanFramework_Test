export module ext.stack_trace;

import std;

//TODO shits
export namespace ext{
	void print_stack_trace(std::ostream& ss, unsigned skip = 2);


	[[deprecated]] void getStackTraceBrief(std::ostream& ss, const bool jumpUnSource = true, const bool showExec = false, const int skipNative = 2);
}
