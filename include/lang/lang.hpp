#pragma once

// lang.hpp — a TypeScript-like scripting language whose runtime IS gold.
//
// Values are gold `var`/`object`/`list`; the AST is built from gold
// objects; scopes reuse gold's prototype chain. Type annotations are
// optional (like TypeScript's `: number` / `: string`) — they are parsed
// and carried on values/functions, and may be enforced at runtime with an
// `enforceTypes` config flag.
//
// Features: let/const/var, arrow & function declarations, classes, object
// and array literals, template literals, member/index access, calls,
// ternary, arithmetic/comparison/logical operators, if/while/for,
// return/break/continue.

#include <iostream>
#include <string>
#include <string_view>

#include "types.hpp"

namespace gold {

	/** Parse a script into a gold-object AST (the statements are a list).
	 * Returns a var holding the list, or genericError on a syntax error. */
	var langParse(std::string_view source);

	/** Run a script; returns the last statement's value (or null). */
	var langRun(std::string_view source,
		object globals = object(), bool enforceTypes = false);

	/** Evaluate a single expression (statement `expr;`) with optional
	 * globals; returns the value or genericError. */
	var langEval(std::string_view source,
		object globals = object(), bool enforceTypes = false);

	/** Interactive REPL: reads lines from `in`, evaluates each (allowing
	 * multi-line blocks and expressions), prints results to `out`. */
	void langRepl(std::istream& in = std::cin, std::ostream& out = std::cout,
		bool enforceTypes = false);

	/**
	 * The `script` object facade: a configurable, embeddable script.
	 *   script({"source", "...", "types", true})   // types: enforce
	 *   s.load({"source", "..."})                  // parse + compile
	 *   s.run()                                    // execute, returns result
	 *   s.setGlobal({"name", value}) / getGlobal
	 *   s.call({"fn", arg0, arg1, ...})            // call a global function
	 * The script shares gold objects with the host, so scripts can call
	 * gold backends and read/write gold data directly.
	 */
	struct script : public object {
	 protected:
		static object& getPrototype();

	 public:
		script();
		script(initList config);

		var load(list args = {});
		var run(list args = {});
		var eval(list args = {});
		var setGlobal(list args);
		var getGlobal(list args);
		var call(list args);
	};

}  // namespace gold