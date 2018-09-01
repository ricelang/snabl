#include "snabl/env.hpp"
#include "snabl/libs/home.hpp"
#include "snabl/std.hpp"
#include "snabl/timer.hpp"

namespace snabl {
	namespace libs {
		Home::Home(Env &env): Lib(env, env.sym("home")) {
			env.maybe_type = add_type<Trait>(env.sym("Maybe"));
			env.nil_type = add_type<NilType>(env.sym("Nil"), {env.maybe_type});
			env.a_type = add_type<Trait>(env.sym("A"), {env.maybe_type});
			env.no_type = add_type<Trait>(env.sym("_"));
			env.num_type = add_type<Trait>(env.sym("Num"), {env.a_type});
			env.bool_type = add_type<BoolType>(env.sym("Bool"), {env.a_type});
			env.float_type = add_type<FloatType>(env.sym("Float"), {env.num_type});
			env.int_type = add_type<IntType>(env.sym("Int"), {env.num_type});
			env.time_type = add_type<TimeType>(env.sym("Time"), {env.a_type});
			env.lambda_type = add_type<LambdaType>(env.sym("Lambda"), {env.a_type});
			
			add_macro(env.sym("t"), env.bool_type, true);			
			add_macro(env.sym("f"), env.bool_type, false);			
			add_macro(env.sym("nil"), env.nil_type, nullptr);			

			add_macro(env.sym("_"),
								[](Forms::const_iterator &in,
									 Forms::const_iterator end,
									 FuncPtr &func, FimpPtr &fimp,
									 Env &env) {
									in++;
								});
			
			add_macro(env.sym("call"), ops::Call::type);
			add_macro(env.sym("drop"), ops::Drop::type);
			add_macro(env.sym("dup"), ops::Dup::type);
			add_macro(env.sym("swap"), ops::Swap::type);

			add_macro(env.sym("let:"),
								[](Forms::const_iterator &in,
									 Forms::const_iterator end,
									 FuncPtr &func, FimpPtr &fimp,
									 Env &env) {
									auto &form(*in++);
									auto &p(*in++);

									if (&p.type != &forms::Id::type) {
										throw SyntaxError(p.pos, "Invalid let: place");
									}
									
									if (in != end) { env.compile(in++, in+1); }
									env.emit(ops::PutVar::type, form.pos, p.as<forms::Id>().id);
								});

			add_macro(env.sym("if:"),
								[](Forms::const_iterator &in,
									 Forms::const_iterator end,
									 FuncPtr &func, FimpPtr &fimp,
									 Env &env) {
									auto &form(*in++);
									env.compile(in++, in+1);
									auto &else_skip(env.emit(ops::Else::type, form.pos, 0));
									size_t start_pc(env.ops.size());								
									env.compile(in++, in+1);
									auto &if_skip(env.emit(ops::Skip::type, form.pos, 0));
									else_skip.as<ops::Else>().nops = env.ops.size()-start_pc;
									start_pc = env.ops.size();
									env.compile(in++, in+1);
									if_skip.as<ops::Skip>().nops = env.ops.size()-start_pc;
								});	

			add_macro(env.sym("func:"),
								[](Forms::const_iterator &in,
									 Forms::const_iterator end,
									 FuncPtr &func, FimpPtr &fimp,
									 Env &env) {
									auto &lib(env.lib());
									const auto &form(*in++);
									const auto &id_form((in++)->as<forms::Id>());

									auto &args_form(*in++);
									vector<Box> args;
									
									if (&args_form.type == &forms::TypeList::type) {
										auto &ids(args_form.as<forms::TypeList>().ids);

										for (const auto id: ids) {
											const auto t(lib.get_type(id));
											if (!t) { throw Error(fmt("Unknown type: %0", {id})); }
											args.emplace_back(t);
										}
									} else {
										throw SyntaxError(args_form.pos,
																			fmt("Invalid func args: %0",
																					{args_form.type.id}));
									}

									auto &rets_form(*in++);
									vector<ATypePtr> rets;
									
									if (&rets_form.type == &forms::Id::type) {
										const auto id(rets_form.as<forms::Id>().id);
										const auto t(lib.get_type(id));
										if (!t) { throw Error(fmt("Unknown type: %0", {id})); }
										rets.push_back(t);
									} else {
										throw SyntaxError(rets_form.pos,
																			fmt("Invalid func rets: %0",
																					{rets_form.type.id}));
									}

									auto fi = lib.add_fimp(id_form.id, args, rets, in++, in+1);
									Fimp::compile(fi, form.pos);
								});

			add_fimp(env.sym("="),
							 {Box(env.maybe_type), Box(env.maybe_type)},
							 {env.bool_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 Box y(env.pop()), x(env.pop());
								 env.push(env.bool_type, x.is_eqval(y));
							 });

			add_fimp(env.sym("=="),
							 {Box(env.maybe_type), Box(env.maybe_type)},
							 {env.bool_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 Box y(env.pop()), x(env.pop());
								 env.push(env.bool_type, x.is_equid(y));
							 });

			add_fimp(env.sym("<"),
							 {Box(env.a_type), Box(env.a_type)},
							 {env.bool_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 Box y(env.pop()), x(env.pop());
								 env.push(env.bool_type, x.cmp(y) == Cmp::LT);
							 });
	
			add_fimp(env.sym("int"),
							 {Box(env.float_type)},
							 {env.int_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 const Float v(env.pop().as<Float>());
								 env.push(env.int_type, Int(v));
							 });

			add_fimp(env.sym("float"),
							 {Box(env.int_type)},
							 {env.float_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 const Int v(env.pop().as<Int>());
								 env.push(env.float_type, Float(v));
							 });

			add_fimp(env.sym("+"),
							 {Box(env.int_type), Box(env.int_type)},
							 {env.int_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 Int y(env.pop().as<Int>()), x(env.pop().as<Int>());
								 env.push(env.int_type, x+y);
							 });

			add_fimp(env.sym("-"),
							 {Box(env.int_type), Box(env.int_type)},
							 {env.int_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 Int y(env.pop().as<Int>()), x(env.pop().as<Int>());
								 env.push(env.int_type, x-y);
							 });
			
			add_fimp(env.sym("*"),
							 {Box(env.int_type), Box(env.int_type)},
							 {env.int_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 Int y(env.pop().as<Int>()), x(env.pop().as<Int>());
								 env.push(env.int_type, x*y);
							 });

			add_fimp(env.sym("bool"),
							 {Box(env.maybe_type)},
							 {env.bool_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 env.push(env.bool_type, env.pop().is_true());
							 });

			add_fimp(env.sym("dump"),
							 {Box(env.maybe_type)}, {},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 env.pop().dump(cerr);
								 cerr << endl;
							 });

			add_fimp(env.sym("say"),
							 {Box(env.maybe_type)}, {},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 env.pop().print(cout);
								 cout << endl;
							 });
			
			add_fimp(env.sym("ns"),
							 {Box(env.int_type)}, {env.time_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 env.push(env.time_type, env.pop().as<Int>());
							 });			

			add_fimp(env.sym("ms"),
							 {Box(env.int_type)}, {env.time_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 env.push(env.time_type, Time::ms(env.pop().as<Int>()));
							 });			

			add_fimp(env.sym("ms"),
							 {Box(env.time_type)}, {env.int_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 env.push(env.int_type, env.pop().as<Time>().as_ms());
							 });
			
			add_fimp(env.sym("sleep"),
							 {Box(env.time_type)}, {},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 const Time time(env.pop().as<Time>());
								 this_thread::sleep_for(nanoseconds(time.ns));
							 });

			add_fimp(env.sym("bench"),
							 {Box(env.int_type), Box(env.a_type)}, {env.time_type},
							 [](Call &call) {
								 Env &env(call.scope->env);
								 const Box target(env.pop());
								 const Int reps(env.pop().as<Int>());
								 for (int i(0); i < reps/2; i++) { target.call(true); }
								 
								 Timer t;
								 for (Int i(0); i < reps; i++) { target.call(true); }
								 env.push(env.time_type, t.ns());
							 });
		}
	}
}
