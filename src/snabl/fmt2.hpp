#ifndef SNABL_FMT2_HPP
#define SNABL_FMT2_HPP

#include "snabl/error.hpp"
#include "snabl/std.hpp"

namespace snabl {	
	string fmt2_arg(const char* x);
	string fmt2_arg(const string &x);

	template <typename T>
	string fmt2_arg(const T &x) { return to_string(x); }

	struct fmt2_conv {
    template<typename T>
    fmt2_conv(T&& val);

		const string as_str;
	};

	template<typename T>
	fmt2_conv::fmt2_conv(T&& val): as_str(fmt2_arg(val)) { }
	
	string fmt2(string_view spec, initializer_list<fmt2_conv> args);
}

#endif
