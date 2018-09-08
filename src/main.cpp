#include "snabl/env.hpp"
#include "tests.hpp"

using namespace snabl;

enum class Mode { Compile, Repl, Run };

int main(int argc, const char *argv[]) {	
	Env env;
	Mode mode(Mode::Repl);
	argc--;
	
	for (const char **ap(argv+1); argc; argc--, ap++) {
		const string a(*ap);
		
		if (a[0] == '-') {
			switch (a[1]) {
			case 'c':
				mode = Mode::Compile;
				break;
			default:
				throw Error(fmt("Invalid flag: %0", {a}));
			}
		} else {
			ifstream f(a);
			if (f.fail()) { throw Error(fmt("File not found: %0", {a})); }
			env.compile(f);
		}
	}
	
	switch (mode) {
	case Mode::Compile: {
		size_t row(0);
		for (const auto &op: env.ops) {
			cout << row++ << '\t';
			op.dump(cout);
		}
		break;
	}
	case Mode::Repl:
		snabl::all_tests();
		//todo: start repl
		break;
	case Mode::Run:
		env.run();
		break;
	}
	
	return 0;
}
