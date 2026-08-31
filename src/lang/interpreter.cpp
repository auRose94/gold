#include "interpreter.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "lexer.hpp"
#include "parser.hpp"

namespace gold {
namespace lang {

	using namespace std;

	// ------------------------------------------------------------------
	// runtime helpers
	// ------------------------------------------------------------------

	struct Signal {
		enum Kind { Return, Break, Continue, Throw } kind;
		var value;
	};

	bool isFn(const var& v) {
		return v.isObject() &&
			v.getObject().getBool("__goldFn");
	}
	bool isClass(const var& v) {
		return v.isObject() &&
			v.getObject().getBool("__goldClass");
	}
	bool isBuiltinFn(const var& v) {
		return v.isObject() &&
			v.getObject().getString("__goldBuiltin").size() > 0;
	}

	bool callMathBuiltin(const string& name, list args, var& out) {
		auto n = args.size() > 0 ? args.getVar(0).getDouble() : 0.0;
		if (name == "round") { out = var(std::round(n)); return true; }
		if (name == "floor") { out = var(std::floor(n)); return true; }
		if (name == "ceil") { out = var(std::ceil(n)); return true; }
		if (name == "abs") { out = var(std::fabs(n)); return true; }
		if (name == "sqrt") { out = var(std::sqrt(n)); return true; }
		if (name == "pow") {
			auto b = args.size() > 1 ? args.getVar(1).getDouble() : 1.0;
			out = var(std::pow(n, b)); return true;
		}
		if (name == "max") {
			double m = n;
			for (uint64_t i = 1; i < args.size(); ++i)
				m = std::max(m, args.getVar(i).getDouble());
			out = var(m); return true;
		}
		if (name == "min") {
			double m = n;
			for (uint64_t i = 1; i < args.size(); ++i)
				m = std::min(m, args.getVar(i).getDouble());
			out = var(m); return true;
		}
		return false;
	}

	list initList() {
		list l;
		l.setVar(0, var());
		l.erase(l.begin());
		return l;
	}

	// ------------------------------------------------------------------
	// Interpreter
	// ------------------------------------------------------------------

	string Interpreter::typeName(const var& v) {
		switch (v.getType()) {
			case typeNull: return "null";
			case typeBool: return "boolean";
			case typeString: case typeStringView: return "string";
			case typeInt64: case typeInt32: case typeInt16: case typeInt8:
			case typeUInt64: case typeUInt32: case typeUInt16: case typeUInt8:
			case typeDouble: case typeFloat: return "number";
			case typeList: return "array";
			case typeObject:
				if (isFn(v)) return "function";
				if (isClass(v)) return "class";
				return "object";
			default: return "unknown";
		}
	}

	bool Interpreter::setInt(var& out, int64_t v) { out = var(v); return true; }
	string Interpreter::strToUpper(string s) { for (auto& c : s) c = toupper((unsigned char)c); return s; }
	string Interpreter::strToLower(string s) { for (auto& c : s) c = tolower((unsigned char)c); return s; }
	string Interpreter::strTrim(string s) {
		size_t a = s.find_first_not_of(" \t\n\r");
		size_t b = s.find_last_not_of(" \t\n\r");
		if (a == string::npos) return "";
		return s.substr(a, b - a + 1);
	}

	// Built-in string/array method dispatch. Returns true if handled.
	bool Interpreter::callBuiltin(var obj, const string& method, list args, var& out) {
		if (obj.isString()) {
			auto str = obj.getString();
			if (method == "toUpperCase") { out = var(strToUpper(str)); return true; }
			if (method == "toLowerCase") { out = var(strToLower(str)); return true; }
			if (method == "trim") { out = var(strTrim(str)); return true; }
			if (method == "indexOf") {
				auto needle = args.size() > 0 ? (string)args.getVar(0) : string();
				return setInt(out, (int64_t)str.find(needle));
			}
			if (method == "includes") {
				auto needle = args.size() > 0 ? (string)args.getVar(0) : string();
				out = var(str.find(needle) != string::npos); return true;
			}
			if (method == "substr" || method == "slice") {
				auto start = args.size() > 0 ? args.getVar(0).getInt64() : int64_t(0);
				auto n = args.size() > 1 ? args.getVar(1).getUInt64() : string::npos;
				out = var(str.substr((size_t)start, n)); return true;
			}
			if (method == "split") {
				auto sep = args.size() > 0 ? (string)args.getVar(0) : string(",");
				list res;
				size_t p = 0, q;
				while ((q = str.find(sep, p)) != string::npos) {
					res.pushString(str.substr(p, q - p)); p = q + sep.size();
				}
				res.pushString(str.substr(p));
				out = var(res); return true;
			}
			if (method == "startsWith") {
				auto needle = args.size() > 0 ? (string)args.getVar(0) : string();
				out = var(str.rfind(needle, 0) == 0); return true;
			}
			return false;
		}
		if (obj.isList()) {
			auto li = obj.getList();
			if (method == "push") {
				for (auto it = args.begin(); it != args.end(); ++it) li.pushVar(*it);
				out = var((uint64_t)li.size()); return true;
			}
			if (method == "pop") {
				if (li.size() > 0) { out = li.getVar(li.size() - 1); return true; }
				out = var(); return true;
			}
			if (method == "join") {
				auto sep = args.size() > 0 ? (string)args.getVar(0) : string(",");
				string r; bool first = true;
				for (auto it = li.begin(); it != li.end(); ++it) {
					if (!first) r += sep;
					r += (string)(*it); first = false;
				}
				out = var(r); return true;
			}
			if (method == "indexOf") {
				auto needle = args.size() > 0 ? args.getVar(0) : var();
				uint64_t i = 0;
				for (auto it = li.begin(); it != li.end(); ++it, ++i)
					if (*it == needle) { out = var((int64_t)i); return true; }
				out = var(int64_t(-1)); return true;
			}
			if (method == "includes") {
				auto needle = args.size() > 0 ? args.getVar(0) : var();
				for (auto it = li.begin(); it != li.end(); ++it)
					if (*it == needle) { out = var(true); return true; }
				out = var(false); return true;
			}
			if (method == "shift") {
				if (li.size() > 0) { out = li.getVar(0); return true; }
				out = var(); return true;
			}
			return false;
		}
		return false;
	}

	bool Interpreter::truthy(const var& v) {
		switch (v.getType()) {
			case typeNull: return false;
			case typeBool: return v.getBool();
			case typeInt64: return v.getInt64() != 0;
			case typeInt32: return v.getInt32() != 0;
			case typeInt16: return v.getInt16() != 0;
			case typeInt8: return v.getInt8() != 0;
			case typeUInt64: return v.getUInt64() != 0;
			case typeUInt32: return v.getUInt32() != 0;
			case typeUInt16: return v.getUInt16() != 0;
			case typeUInt8: return v.getUInt8() != 0;
			case typeDouble: return v.getDouble() != 0;
			case typeFloat: return v.getFloat() != 0;
			case typeString: return v.getString().size() > 0;
			case typeList: return v.getList().size() > 0;
			case typeObject: return true;
			default: return false;
		}
	}

	bool Interpreter::typeMatches(const string& type, const var& v) {
		if (type.empty() || type == "any" || type == "unknown")
			return true;
		if (type == "number")
			return v.isNumber();
		if (type == "string")
			return v.isString();
		if (type == "boolean")
			return v.isBool();
		if (type == "object")
			return v.isObject();
		if (type == "array")
			return v.isList();
		if (type == "null" || type == "undefined")
			return v.getType() == typeNull;
		if (type == "function")
			return isFn(v);
		// named type: object that "inherits" the class proto
		if (v.isObject()) {
			auto o = v.getObject();
			auto proto = o.getVar("__protoName");
			return proto.isString() && proto.getString() == type;
		}
		return true;
	}

	void Interpreter::checkType(const string& type, const var& v) {
		if (enforceTypes && !typeMatches(type, v)) {
			err = "type error: expected '" + type + "', got '" +
				string(v.getTypeString()) + "'";
			throw runtime_error(err);
		}
	}

	var Interpreter::lookup(const string& name, object env) {
		if (env.getVar(name).getType() != typeNull)
			return env.getVar(name);
		return var();
	}

	void Interpreter::assignIdent(const string& name, const var& value,
		object env) {
		// find the scope that declares it; else set in env
		auto scope = env;
		while (scope) {
			if (scope.owns(name)) {
				scope[name] = value;
				return;
			}
			auto parent = scope.getParent();
			if (!parent) break;
			scope = parent;
		}
		env[name] = value;
	}

	var Interpreter::callFn(const var& fn, list args, var thisArg) {
		if (!isFn(fn))
			throw runtime_error("attempted to call a non-function");
		auto f = fn.getObject();
		auto params = f.getList("params");
		auto body = f.getObject("body");
		auto closure = f.getObject("env");
		// new scope
		auto scope = object();
		scope.setParent(closure);
		uint64_t n = params.size();
		for (uint64_t i = 0; i < n; ++i) {
			auto p = params.getObject(i);
			var v = i < args.size() ? args.getVar(i) : var();
			if (v.getType() == typeNull && p.getVar("def").getType() != typeNull)
				v = evalNode(p.getVar("def"), scope);
			auto t = p.getString("type");
			checkType(t, v);
			scope[p.getString("n")] = v;
		}
		if (thisArg.getType() != typeNull)
			scope["this"] = thisArg;
		try {
			var r = execBlock(body, scope);
			auto ret = f.getString("retType");
			if (!ret.empty()) checkType(ret, r);
			return r;
		} catch (Signal& s) {
			if (s.kind == Signal::Return) {
				auto ret = f.getString("retType");
				if (!ret.empty()) checkType(ret, s.value);
				return s.value;
			}
			throw runtime_error("break/continue outside loop");
		}
	}

	// executes a block (or arrow body); returns last value
	var Interpreter::execBlock(object node, object env) {
		if (node.getVar("t").getString() == "block") {
			var result;
			auto body = node.getList("body");
			for (auto it = body.begin(); it != body.end(); ++it)
				result = execNode(it->getObject(), env);
			return result;
		}
		// arrow function body expression
		return evalNode(var(node), env);
	}

	var Interpreter::makeFn(object node, object env) {
		auto f = object();
		f.setBool("__goldFn", true);
		f.setList("params", node.getList("params"));
		f.setObject("body", node.getObject("body"));
		f.setObject("env", env);
		if (node.getVar("t").getString() == "fn" &&
			node.getString("n").size() > 0)
			f.setString("__name", node.getString("n"));
		f.setString("retType", node.getString("retType"));
		return f;
	}

	var Interpreter::instantiateClass(object cls, object env, list args) {
		auto methods = cls.getList("methods");
		// instance is a plain object; methods stored as its parent
		// prototype with __protoName for named-type checks.
		auto proto = object();
		auto name = cls.getString("n");
		proto.setString("__protoName", name);
		for (auto it = methods.begin(); it != methods.end(); ++it) {
			auto m = it->getObject();
			auto fn = makeFn(m, env);
			proto.setObject(m.getString("n"), fn);
		}
		auto instance = object();
		instance.setParent(proto);
		// constructor (if any)
		auto ctor = proto.getVar("constructor");
		if (isFn(ctor)) {
			callFn(ctor, args, var(instance));
		}
		return var(instance);
	}

	var Interpreter::evalNode(var node, object env) {
		auto o = node.getObject();
		auto t = o.getString("t");
		if (t == "num") return var(o.getDouble("v"));
		if (t == "str") return var(o.getString("v"));
		if (t == "bool") return var(o.getBool("v"));
		if (t == "null") return var();
		if (t == "ident") return lookup(o.getString("n"), env);
		if (t == "this") return lookup("this", env);
		if (t == "obj") {
			auto out = object();
			auto props = o.getList("props");
			for (auto it = props.begin(); it != props.end(); ++it) {
				auto p = it->getObject();
				out[p.getString("k")] = evalNode(p.getVar("v"), env);
			}
			return var(out);
		}
		if (t == "arr") {
			auto out = initList();
			auto items = o.getList("items");
			for (auto it = items.begin(); it != items.end(); ++it)
				out.pushVar(evalNode(*it, env));
			return var(out);
		}
		if (t == "tpl") {
			string out;
			auto parts = o.getList("parts");
			for (auto it = parts.begin(); it != parts.end(); ++it) {
				auto part = *it;
				if (part.isString()) {
					out += part.getString();
				} else {
					auto v = evalNode(part, env);
					out += (string)v;
				}
			}
			return var(out);
		}
		if (t == "arrow" || t == "fn") {
			return var(makeFn(o, env));
		}
		if (t == "class") {
			// store the class definition; instantiation via new
			auto c = object();
			c.setBool("__goldClass", true);
			c.setString("n", o.getString("n"));
			c.setList("methods", o.getList("methods"));
			c.setObject("env", env);
			if (!o.getString("parent").empty())
				c.setString("parent", o.getString("parent"));
			env[o.getString("n")] = var(c);
			return var();
		}
		if (t == "new") {
			auto c = lookup(o.getString("n"), env);
			if (isClass(c)) {
				auto cls = c.getObject();
				auto args = list();
				auto argNodes = o.getList("args");
				for (auto it = argNodes.begin(); it != argNodes.end(); ++it)
					args.pushVar(evalNode(*it, env));
				auto parent = cls.getString("parent");
				if (!parent.empty()) {
					auto pc = lookup(parent, env);
					if (isClass(pc)) {
						// merge inherited methods
						auto inst = instantiateClass(pc.getObject(), cls.getObject("env"), args);
						auto instObj = inst.getObject();
						auto methods = cls.getList("methods");
						for (auto it = methods.begin(); it != methods.end(); ++it)
							instObj[it->getObject().getString("n")] =
								var(makeFn(it->getObject(), cls.getObject("env")));
						return inst;
					}
				}
				return var(instantiateClass(cls, cls.getObject("env"), args));
			}
			throw runtime_error("new: '" + o.getString("n") + "' is not a class");
		}
		if (t == "member") {
			auto obj = evalNode(o.getVar("o"), env);
			auto prop = o.getString("p");
			if (obj.isString() && prop == "length")
				return var((int64_t)obj.getString().size());
			if (obj.isList() && prop == "length")
				return var((int64_t)obj.getList().size());
			if (obj.isObject()) {
				// return the property (including builtin markers)
				auto oo = obj.getObject();
				if (oo.owns(prop)) return oo.getVar(prop);
				return var();
			}
			return var();
		}
		if (t == "index") {
			auto obj = evalNode(o.getVar("o"), env);
			auto idx = evalNode(o.getVar("i"), env);
			if (obj.isList()) return obj.getList().getVar(idx.getUInt64());
			if (obj.isObject()) return obj.getObject().getVar((string)idx);
			return var();
		}
		if (t == "call") {
			auto callee = o.getVar("f").getObject();
			auto args = list();
			auto argNodes = o.getList("args");
			for (auto it = argNodes.begin(); it != argNodes.end(); ++it)
				args.pushVar(evalNode(*it, env));
			if (callee.getString("t") == "member") {
				auto obj = evalNode(callee.getVar("o"), env);
				auto method = callee.getString("p");
				// built-in string/array method?
				var built;
				if (callBuiltin(obj, method, args, built))
					return built;
				// array map/filter with a callback function
				if (obj.isList() &&
					(method == "map" || method == "filter") &&
					args.size() > 0 && isFn(args.getVar(0))) {
					auto fn = args.getVar(0);
					auto li = obj.getList();
					auto res = list();
					for (auto it = li.begin(); it != li.end(); ++it) {
						if (method == "map") {
							res.pushVar(callFn(fn, list{*it}, var()));
						} else if (truthy(callFn(fn, list{*it}, var()))) {
							res.pushVar(*it);
						}
					}
					return var(res);
				}
				// Math builtins
				if (obj.isObject() &&
					callMathBuiltin(method, args, built))
					return built;
				// gold object method (this = obj)
				auto fn = obj.getObject().getVar(method);
				if (isBuiltinFn(fn)) {
					var mathOut;
					if (callMathBuiltin(
							fn.getObject().getString("__goldBuiltin"),
							args, mathOut))
						return mathOut;
				}
				return callFn(fn, args, obj);
			}
			auto fn = evalNode(o.getVar("f"), env);
			if (isBuiltinFn(fn)) {
				var mathOut;
				if (callMathBuiltin(
						fn.getObject().getString("__goldBuiltin"),
						args, mathOut))
					return mathOut;
			}
			return callFn(fn, args, var());
		}
		if (t == "assign") {
			auto target = o.getVar("target").getObject();
			auto tt = target.getString("t");
			if (tt == "ident") {
				var value = evalNode(o.getVar("value"), env);
				auto op = o.getString("op");
				if (op != "=") {
					auto cur = lookup(target.getString("n"), env);
					if (op == "+=") value = cur + value;
					else if (op == "-=") value = cur - value;
					else if (op == "*=") value = cur * value;
					else if (op == "/=") value = cur / value;
				}
				assignIdent(target.getString("n"), value, env);
				return value;
			}
			if (tt == "member") {
				auto obj = evalNode(target.getVar("o"), env);
				auto value = evalNode(o.getVar("value"), env);
				obj.getObject()[target.getString("p")] = value;
				return value;
			}
			if (tt == "index") {
				auto obj = evalNode(target.getVar("o"), env);
				auto idx = evalNode(target.getVar("i"), env);
				auto value = evalNode(o.getVar("value"), env);
				if (obj.isList()) {
					auto l = obj.getList();
					l[idx.getUInt64()] = value;
				} else if (obj.isObject()) {
					obj.getObject()[(string)idx] = value;
				}
				return value;
			}
			throw runtime_error("invalid assignment target");
		}
		if (t == "typeof") {
			auto a = evalNode(o.getVar("a"), env);
			return var(typeName(a));
		}
		if (t == "bin") {
			auto op = o.getString("op");
			auto l = evalNode(o.getVar("l"), env);
			auto r = evalNode(o.getVar("r"), env);
			if (op == "in") {
				if (r.isObject())
					return var(r.getObject().owns((string)l));
				if (r.isList())
					return var(l.getUInt64() < r.getList().size());
				return var(false);
			}
			if (op == "&&") return var(truthy(l) ? r : l);
			if (op == "||") return var(truthy(l) ? l : r);
			if (op == "==" || op == "===")
				return var((l == r));
			if (op == "!=" || op == "!==")
				return var(!(l == r));
			if (op == "<") return var(l < r);
			if (op == ">") return var(l > r);
			if (op == "<=") return var(l <= r);
			if (op == ">=") return var(l >= r);
			if (op == "+") {
				if (l.isString() || r.isString())
					return var((string)l + (string)r);
				return var(l + r);
			}
			if (op == "-") return var(l - r);
			if (op == "*") return var(l * r);
			if (op == "/") return var(l / r);
			if (op == "%") return var(l % r);
			throw runtime_error("unknown operator '" + op + "'");
		}
		if (t == "un") {
			auto op = o.getString("op");
			auto a = evalNode(o.getVar("a"), env);
			if (op == "!") return var(!truthy(a));
			if (op == "-") return var(-a);
			if (op == "+") return var(a);
		}
		if (t == "postfix") {
			auto a = evalNode(o.getVar("a"), env);
			auto op = o.getString("op");
			auto v = op == "++" ? a + var(int64_t(1))
				: a - var(int64_t(1));
			// assign back to the target
			auto target = o.getVar("a").getObject();
			if (target.getString("t") == "ident")
				assignIdent(target.getString("n"), v, env);
			// Postfix semantics: return the OLD value (the update
			// still happened). `i++` yields i, then i becomes i+1.
			return a;
		}
		if (t == "tern") {
			auto c = evalNode(o.getVar("c"), env);
			return truthy(c) ? evalNode(o.getVar("a"), env)
				: evalNode(o.getVar("b"), env);
		}
		throw runtime_error("unknown AST node '" + t + "'");
	}

	var Interpreter::execNode(object node, object env) {
		auto t = node.getString("t");
		if (t == "let") {
			var result;
			auto decls = node.getList("decls");
			for (auto it = decls.begin(); it != decls.end(); ++it) {
				auto d = it->getObject();
				var value;
				if (d.getVar("v").getType() != typeNull)
					value = evalNode(d.getVar("v"), env);
				checkType(d.getString("type"), value);
				env[d.getString("n")] = value;
				result = value;
			}
			return result;
		}
		if (t == "block") {
			// a nested block creates a child scope
			auto scope = object();
			scope.setParent(env);
			var result;
			auto body = node.getList("body");
			for (auto it = body.begin(); it != body.end(); ++it)
				result = execNode(it->getObject(), scope);
			return result;
		}
		if (t == "expr") return evalNode(node.getVar("e"), env);
		if (t == "if") {
			auto c = evalNode(node.getVar("c"), env);
			if (truthy(c)) return execNode(node.getObject("then"), env);
			auto els = node.getVar("else");
			if (els.getType() != typeNull)
				return execNode(els.getObject(), env);
			return var();
		}
		if (t == "while") {
			var result;
			while (truthy(evalNode(node.getVar("c"), env))) {
				try {
					result = execNode(node.getObject("body"), env);
				} catch (Signal& s) {
					if (s.kind == Signal::Break) break;
					if (s.kind == Signal::Continue) continue;
					throw;
				}
			}
			return result;
		}
		if (t == "for") {
			// new scope for init
			auto scope = object();
			scope.setParent(env);
			execNode(node.getObject("init"), scope);
			var result;
			while (node.getVar("cond").getType() == typeNull ||
				truthy(evalNode(node.getVar("cond"), scope))) {
				try {
					result = execNode(node.getObject("body"), scope);
				} catch (Signal& s) {
					if (s.kind == Signal::Break) break;
					if (s.kind == Signal::Continue) {
						if (node.getVar("inc").getType() != typeNull)
							evalNode(node.getVar("inc"), scope);
						continue;
					}
					throw;
				}
				if (node.getVar("inc").getType() != typeNull)
					evalNode(node.getVar("inc"), scope);
			}
			return result;
		}
		if (t == "throw") {
			var value;
			if (node.getVar("v").getType() != typeNull)
				value = evalNode(node.getVar("v"), env);
			throw Signal{Signal::Throw, value};
		}
		if (t == "try") {
			var result;
			try {
				result = execNode(node.getObject("body"), env);
			} catch (Signal& sig) {
				if (sig.kind == Signal::Throw) {
					auto cb = node.getVar("catch");
					if (cb.getType() != typeNull) {
						auto catchObj = cb.getObject();
						auto scope = object();
						scope.setParent(env);
						scope[node.getString("param")] = sig.value;
						result = execNode(catchObj, scope);
					}
				} else throw;
			}
			auto fin = node.getVar("finally");
			if (fin.getType() != typeNull)
				execNode(fin.getObject(), env);
			return result;
		}
		if (t == "forof") {
			auto iter = evalNode(node.getVar("iter"), env);
			var result;
			auto scope = object();
			scope.setParent(env);
			if (iter.isList()) {
				auto li = iter.getList();
				for (auto it = li.begin(); it != li.end(); ++it) {
					scope[node.getString("var")] = *it;
					try { result = execNode(node.getObject("body"), scope); }
					catch (Signal& s) {
						if (s.kind == Signal::Break) break;
						if (s.kind == Signal::Continue) continue;
						throw;
					}
				}
			} else if (iter.isObject()) {
				auto o = iter.getObject();
				for (auto it = o.begin(); it != o.end(); ++it) {
					scope[node.getString("var")] = var(it->first);
					try { result = execNode(node.getObject("body"), scope); }
					catch (Signal& s) {
						if (s.kind == Signal::Break) break;
						if (s.kind == Signal::Continue) continue;
						throw;
					}
				}
			}
			return result;
		}
		if (t == "forin") {
			auto objVar = evalNode(node.getVar("iter"), env);
			var result;
			auto scope = object();
			scope.setParent(env);
			if (objVar.isObject()) {
				auto o = objVar.getObject();
				for (auto it = o.begin(); it != o.end(); ++it) {
					scope[node.getString("var")] = var(it->first);
					try { result = execNode(node.getObject("body"), scope); }
					catch (Signal& s) {
						if (s.kind == Signal::Break) break;
						if (s.kind == Signal::Continue) continue;
						throw;
					}
				}
			}
			return result;
		}
		if (t == "return") {
			var value;
			if (node.getVar("v").getType() != typeNull)
				value = evalNode(node.getVar("v"), env);
			throw Signal{Signal::Return, value};
		}
		if (t == "break") throw Signal{Signal::Break, var()};
		if (t == "continue") throw Signal{Signal::Continue, var()};
		if (t == "fn") {
			env[node.getString("n")] = var(makeFn(node, env));
			return var();
		}
		if (t == "class") {
			auto c = object();
			c.setBool("__goldClass", true);
			c.setString("n", node.getString("n"));
			c.setList("methods", node.getList("methods"));
			c.setObject("env", env);
			if (!node.getString("parent").empty())
				c.setString("parent", node.getString("parent"));
			env[node.getString("n")] = var(c);
			return var();
		}
		return evalNode(var(node), env);
	}

	Interpreter::Interpreter(object g, bool t) : globals(std::move(g)), enforceTypes(t) {}

	var Interpreter::run(object program) {
		try {
			var result;
			auto body = program.getList("body");
			for (auto it = body.begin(); it != body.end(); ++it)
				result = execNode(it->getObject(), globals);
			return result;
		} catch (Signal& s) {
			if (s.kind == Signal::Throw)
				return genericError((string)s.value);
			err = "unexpected control-flow signal";
			return genericError(err);
		} catch (const exception& e) {
			err = e.what();
			return genericError(err);
		}
	}
	var Interpreter::eval(object program) {
		try {
			var result;
			auto body = program.getList("body");
			for (auto it = body.begin(); it != body.end(); ++it)
				result = execNode(it->getObject(), globals);
			return result;
		} catch (const exception& e) {
			err = e.what();
			return genericError(err);
		}
	}
	object& Interpreter::globalsRef() { return globals; }
	const string& Interpreter::error() const { return err; }

	// ------------------------------------------------------------------
	// compile & globals
	// ------------------------------------------------------------------

	object ensureData(object o) {
		o.setNull("__gold_init");
		o.erase("__gold_init");
		return o;
	}

	object makeGlobals(object g) {
		object globals;
		if (g) globals.copy(g);
		// predefine the Math builtin namespace
		auto math = object();
		static const char* names[] = {
			"round", "floor", "ceil", "abs", "sqrt", "pow", "max", "min",
		};
		for (auto nm : names) {
			object b;
			b.setString("__goldBuiltin", nm);
			math.setObject(nm, b);
		}
		globals.setObject("Math", math);
		return ensureData(globals);
	}

	object compile(string_view src, string& err) {
		Lexer l(src);
		if (!l.scan()) {
			err = "lex error: " + l.error();
			return object();
		}
		Parser p(l.tokens);
		auto ast = p.parse();
		if (p.error().size() > 0) {
			err = "parse error: " + p.error();
			return object();
		}
		return ast;
	}

}  // namespace lang
}  // namespace gold
