#pragma once

// interpreter.hpp — tree-walking evaluator for the gold::lang scripting
// language. The runtime IS gold: values are var/object/list, scopes use
// gold's prototype chain, and control flow is signalled via exceptions.
// Internal to the lang module (not installed).

#include <string>

#include "types.hpp"

namespace gold {
namespace lang {

	// A function/class stored in the runtime as a gold object.
	bool isFn(const var& v);
	bool isClass(const var& v);

	// Compile (parse) source into a program AST. On failure returns an
	// empty object and sets `err`.
	object compile(std::string_view src, std::string& err);

	// Build the global scope: copies `g` and predefines the Math builtin
	// namespace.
	object makeGlobals(object g);

	// Normalize a gold object for use as a data scope (strips the internal
	// init marker).
	object ensureData(object o);

	class Interpreter {
		object globals;
		bool enforceTypes;
		std::string err;

		std::string typeName(const var& v);
		bool setInt(var& out, int64_t v);
		std::string strToUpper(std::string s);
		std::string strToLower(std::string s);
		std::string strTrim(std::string s);
		bool callBuiltin(var obj, const std::string& method, list args, var& out);
		bool truthy(const var& v);
		bool typeMatches(const std::string& type, const var& v);
		void checkType(const std::string& type, const var& v);
		var lookup(const std::string& name, object env);
		void assignIdent(const std::string& name, const var& value, object env);

	 public:
		var callFn(const var& fn, list args, var thisArg = var());

	 private:
		var execBlock(object node, object env);
		var makeFn(object node, object env);
		var instantiateClass(object cls, object env, list args);
		var evalNode(var node, object env);
		var execNode(object node, object env);

	 public:
		Interpreter(object g, bool t);

		var run(object program);
		var eval(object program);
		object& globalsRef();
		const std::string& error() const;
	};

}  // namespace lang
}  // namespace gold
