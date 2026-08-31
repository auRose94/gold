#include "lang/lang.hpp"

#include <iostream>
#include <string>
#include <string_view>

#include "goldjs.hpp"
#include "interpreter.hpp"

namespace gold {
	using namespace std;

	// ----------------------------------------------------------------------
	// public API
	// ----------------------------------------------------------------------

	var langParse(string_view source) {
		string err;
		auto ast = lang::compile(source, err);
		if (ast) return var(ast);
		return genericError(err);
	}

	var langRun(string_view source, object globals, bool enforceTypes) {
		string err;
		auto ast = lang::compile(source, err);
		if (!ast) return genericError(err);
		auto g = lang::makeGlobals(globals);
		lang::Interpreter interp(g, enforceTypes);
		return interp.run(ast);
	}

	var langEval(string_view source, object globals, bool enforceTypes) {
		string err;
		auto ast = lang::compile(source, err);
		if (!ast) return genericError(err);
		auto g = lang::makeGlobals(globals);
		lang::Interpreter interp(g, enforceTypes);
		return interp.eval(ast);
	}

	// ----------------------------------------------------------------------
	// REPL
	// ----------------------------------------------------------------------

	void langRepl(std::istream& in, std::ostream& out, bool enforceTypes) {
		auto globals = lang::makeGlobals(object());
		out << "gold::lang REPL — type statements, ^D to exit\n";
		string buf;
		string line;
		bool multiline = false;
		while (true) {
			out << (multiline ? "...> " : "gold> ") << std::flush;
			if (!std::getline(in, line)) break;
			buf += line;
			buf += "\n";
			// continue collecting while braces/parens are unbalanced
			int depth = 0;
			bool inStr = false;
			for (size_t i = 0; i < buf.size(); ++i) {
				char c = buf[i];
				if (inStr) {
					if (c == '\\') i++;
					else if (c == '"' || c == '\'') inStr = false;
					continue;
				}
				if (c == '"' || c == '\'') inStr = true;
				else if (c == '{' || c == '(' || c == '[') depth++;
				else if (c == '}' || c == ')' || c == ']') depth--;
			}
			if (depth > 0) {
				multiline = true;
				continue;
			}
			multiline = false;
			string err;
			auto ast = lang::compile(buf, err);
			if (!ast) {
				out << "Error: " << err << "\n";
			} else {
				lang::Interpreter replInterp(globals, enforceTypes);
				auto v = replInterp.run(ast);
				if (v.isError()) out << "Error: " << (string)*v.getError() << "\n";
				else if (v.getType() != typeNull) out << (string)v << "\n";
			}
			buf.clear();
		}
	}

	// ----------------------------------------------------------------------
	// script facade
	// ----------------------------------------------------------------------

	obj& script::getPrototype() {
		static auto proto = obj({
			{"source", ""},
			{"types", false},
			{"load", method(&script::load)},
			{"run", method(&script::run)},
			{"eval", method(&script::eval)},
			{"setGlobal", method(&script::setGlobal)},
			{"getGlobal", method(&script::getGlobal)},
			{"call", method(&script::call)},
		});
		return proto;
	}

	script::script() : obj() {}

	script::script(initList config) : obj(config) {
		setParent(getPrototype());
	}

	var script::load(list) {
		auto source = getString("source");
		string err;
		auto ast = lang::compile(source, err);
		if (!ast) return genericError(err);
		setObject("ast", ast);
		setString("error", "");
		return var();
	}

	var script::run(list) {
		auto ast = getObject("ast");
		if (!ast) return genericError("no script loaded");
		auto globals = getObject("globals");
		if (!globals) {
			globals = lang::ensureData(object());
			setObject("globals", globals);
		}
		lang::Interpreter interp(lang::ensureData(globals), getBool("types"));
		auto r = interp.run(ast);
		if (r.isError()) setString("error", (string)*r.getError());
		else setString("error", "");
		return r;
	}

	var script::eval(list args) {
		auto source = args[0].getString();
		string err;
		auto ast = lang::compile(source, err);
		if (!ast) return genericError(err);
		auto globals = getObject("globals");
		if (!globals) {
			globals = lang::ensureData(object());
			setObject("globals", globals);
		}
		lang::Interpreter interp(lang::ensureData(globals), getBool("types"));
		return interp.eval(ast);
	}

	var script::setGlobal(list args) {
		auto name = args[0].getString();
		auto value = args[1];
		auto globals = getObject("globals");
		if (!globals) {
			globals = lang::ensureData(object());
			setObject("globals", globals);
		}
		globals.setVar(name, value);
		return value;
	}

	var script::getGlobal(list args) {
		auto globals = getObject("globals");
		auto name = args[0].getString();
		if (globals) return globals.getVar(name);
		return var();
	}

	var script::call(list args) {
		auto name = args[0].getString();
		auto globals = getObject("globals");
		if (!globals) return genericError("no globals");
		auto fn = globals.getVar(name);
		if (!lang::isFn(fn))
			return genericError("'" + name + "' is not a function");
		list callArgs;
		for (uint64_t i = 1; i < args.size(); ++i)
			callArgs.pushVar(args.getVar(i));
		lang::Interpreter interp(globals, getBool("types"));
		try {
			return interp.callFn(fn, callArgs);
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

}  // namespace gold
