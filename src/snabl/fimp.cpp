#include "snabl/env.hpp"
#include "snabl/fimp.hpp"
#include "snabl/lib.hpp"

namespace snabl {
	Sym Fimp::get_id(const FuncPtr &func, const Args &args) {
		std::stringstream buf;
		buf << func->id.name() << '<';
		char sep = 0;

		for (auto &a: args) {
			if (sep) {
				buf << sep;
			} else {
				sep = ' ';
			}

			if (a.is_undef()) {
				buf << a.type()->id.name();
			} else {
				a.write(buf);
			}
		}

		buf << '>';
		return func->lib.env.sym(buf.str());
	}

	bool Fimp::call(const FimpPtr &fimp, Pos pos) {
		auto &env(fimp->func->lib.env);
		auto &bin(env.bin);
		auto &call(env.push_call(fimp));

		if (fimp->imp) {
			auto stack_offs(env.stack().size());
			(*fimp->imp)(call);
			auto func(fimp->func);
		
			if (env.stack().size() != stack_offs-func->nargs+func->nrets) {
				throw Error("Invalid stack after funcall");
			}
		
			env.pop_call();
			return true;
		}

		fimp->compile(pos);
		env.push_call(fimp, bin.pc+1);
		bin.pc = *fimp->_start_pc;
		return false;
	}

	Fimp::Fimp(const FuncPtr &func, const Args &args, const Rets &rets, Imp imp):
		id(get_id(func, args)), func(func), args(args), rets(rets), imp(imp),
	  _start_pc(stdx::nullopt), _nops(0) { }

	Fimp::Fimp(const FuncPtr &func,
						 const Args &args, const Rets &rets,
						 Forms::const_iterator begin,
						 Forms::const_iterator end):
		id(get_id(func, args)), func(func), args(args), rets(rets), forms(begin, end),
		_start_pc(stdx::nullopt), _nops(0) { }

	bool Fimp::compile(Pos pos) {
		auto &bin(func->lib.env.bin);
		if (_start_pc) { return false; }
		auto &skip(bin.emplace_back(ops::Skip::type, pos, 0).as<ops::Skip>());
		_start_pc = bin.ops.end();
		const auto pc_backup(bin.pc);
		bin.emplace_back(ops::Begin::type, pos);
		bin.compile(forms);
		bin.emplace_back(ops::Return::type, pos);
		bin.pc = pc_backup;
		_nops = skip.nops = bin.ops.end()-*_start_pc;
		return true;
	}

	stdx::optional<std::size_t> Fimp::score(const Stack &stack) const {
		if (!func->nargs) { return 0; }
		if (stack.size() < func->nargs) { return stdx::nullopt; }
		auto &env(func->lib.env);
		auto i(std::next(stack.begin(), stack.size()-func->nargs));
		auto j(args.begin());
		std::size_t score(0);

		for (; j != args.end(); i++, j++) {
			auto &iv(*i), &jv(*j);
			auto it(iv.type()), jt(jv.type());
			if (it == env.no_type) { continue; }

			if (jv.is_undef()) {
				if (!it->isa(jt)) { return stdx::nullopt; }
			} else {
				if (iv.is_undef() || !iv.is_eqval(jv)) { return stdx::nullopt; }
			}
			
			score += std::abs(it->tag-jt->tag);
		}

		return score;
	}

	void Fimp::dump(std::ostream &out) const { out << id.name(); }
}
