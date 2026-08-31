#include "parser.hpp"

#include <stdexcept>

namespace gold {
namespace lang {

	using namespace std;

	const Token& Parser::cur() { return toks[i]; }
	const Token& Parser::advance() { return toks[i++]; }
	bool Parser::at(Tok t) const { return toks[i].type == t; }
	bool Parser::atLex(const char* l) const {
		return toks[i].lex == l;
	}
	bool Parser::atKw(const char* k) const {
		return toks[i].type == Tok::Kw && toks[i].lex == k;
	}
	bool Parser::checkPunc(const char* p) {
		if (toks[i].type == Tok::Punc && toks[i].lex == p) {
			i++;
			return true;
		}
		return false;
	}
	bool Parser::checkOp(const char* o) {
		if (toks[i].type == Tok::Op && toks[i].lex == o) {
			i++;
			return true;
		}
		return false;
	}
	bool Parser::checkIdent() {
		if (toks[i].type == Tok::Ident) { i++; return true; }
		return false;
	}
	void Parser::expectPunc(const char* p) {
		if (!checkPunc(p)) throw runtime_error(
			"expected '" + string(p) + "'");
	}
	void Parser::expectIdent(string& out) {
		if (toks[i].type == Tok::Ident) out = advance().lex;
		else throw runtime_error("expected identifier");
	}
	// A type annotation like `: number` or `: string[]` or
	// `: Array<string>`. Parsed (loosely) and returned as a string.
	string Parser::parseType() {
		if (toks[i].type != Tok::Punc || toks[i].lex != ":")
			return "";
		i++;
		string t;
		if (toks[i].type == Tok::Ident) t = advance().lex;
		else if (toks[i].type == Tok::Kw &&
			(toks[i].lex == "null" || toks[i].lex == "undefined"))
			t = advance().lex;
		else throw runtime_error("expected type");
		// array suffix []
		while (toks[i].type == Tok::Punc && toks[i].lex == "[") {
			i++;
			if (toks[i].type == Tok::Punc && toks[i].lex == "]") {
				i++;
				t += "[]";
			} else throw runtime_error("expected ] in type");
		}
		// generic <...>
		if (toks[i].type == Tok::Op && toks[i].lex == "<") {
			int depth = 0;
			string g;
			while (toks[i].type != Tok::Eof) {
				if (toks[i].type == Tok::Op && toks[i].lex == "<") depth++;
				if (toks[i].type == Tok::Op && toks[i].lex == ">") {
					depth--;
					if (depth == 0) { i++; break; }
				}
				g += toks[i].lex;
				i++;
			}
			t += "<" + g + ">";
		}
		return t;
	}

	object Parser::parseProgram() {
		auto stmts = list();
		while (toks[i].type != Tok::Eof)
			stmts.pushObject(parseStatement());
		auto root = object();
		root.setList("body", stmts);
		return root;
	}

	object Parser::parseStatement() {
		if (atKw("let") || atKw("const") || atKw("var"))
			return parseLet();
		if (atKw("if")) return parseIf();
		if (atKw("while")) return parseWhile();
		if (atKw("for")) return parseFor();
		if (atKw("function")) return parseFunction(true);
		if (atKw("async") && toks[i + 1].type == Tok::Kw &&
			toks[i + 1].lex == "function")
			return parseFunction(true);
		if (atKw("class")) return parseClass();
		if (atKw("return")) return parseReturn();
		if (atKw("throw")) return parseThrow();
		if (atKw("try")) return parseTry();
		if (atKw("break")) {
			i++;
			expectPunc(";");
			return object({{"t", "break"}});
		}
		if (atKw("continue")) {
			i++;
			expectPunc(";");
			return object({{"t", "continue"}});
		}
		if (toks[i].type == Tok::Punc && toks[i].lex == "{")
			return parseBlock();
		return parseExprStmt();
	}

	object Parser::parseLet() {
		auto kw = advance().lex;
		auto decls = list();
		while (true) {
			string name;
			expectIdent(name);
			string type = parseType();
			var value;
			bool hasValue = false;
			if (checkOp("=")) {
				value = parseExpression();
				hasValue = true;
			}
			auto d = object();
			d.setString("n", name);
			if (!type.empty()) d.setString("type", type);
			if (hasValue) d.setVar("v", value);
			decls.pushObject(d);
			if (toks[i].type == Tok::Punc && toks[i].lex == ",") {
				i++;
				continue;
			}
			break;
		}
		if (toks[i].type == Tok::Punc && toks[i].lex == ";") i++;
		auto n = object();
		n.setString("t", "let");
		n.setString("kw", kw);
		n.setList("decls", decls);
		return n;
	}

	object Parser::parseBlock() {
		expectPunc("{");
		auto body = list();
		while (!(toks[i].type == Tok::Punc && toks[i].lex == "}") &&
			toks[i].type != Tok::Eof)
			body.pushObject(parseStatement());
		expectPunc("}");
		auto n = object();
		n.setList("body", body);
		n.setString("t", "block");
		return n;
	}

	object Parser::parseIf() {
		i++;  // if
		expectPunc("(");
		auto cond = parseExpression();
		expectPunc(")");
		auto then = parseStatement();
		object els;
		if (atKw("else")) {
			i++;
			els = parseStatement();
		}
		auto n = object();
		n.setString("t", "if");
		n.setVar("c", cond);
		n.setObject("then", then);
		if (els) n.setObject("else", els);
		return n;
	}

	object Parser::parseWhile() {
		i++;
		expectPunc("(");
		auto cond = parseExpression();
		expectPunc(")");
		auto body = parseStatement();
		auto n = object();
		n.setString("t", "while");
		n.setVar("c", cond);
		n.setObject("body", body);
		return n;
	}

	object Parser::parseFor() {
		i++;
		expectPunc("(");
		// for (x of iter) / for (x in obj)
		{
			size_t save = i;
			string kw, name;
			if (atKw("let") || atKw("const") || atKw("var")) {
				kw = toks[i].lex;
				i++;
			}
			if (toks[i].type == Tok::Ident) {
				name = toks[i].lex;
				if (toks[i + 1].type == Tok::Kw &&
					(toks[i + 1].lex == "of" || toks[i + 1].lex == "in")) {
					auto mode = toks[i + 1].lex;
					i += 2;
					auto iter = parseExpression();
					expectPunc(")");
					auto body = parseStatement();
					auto n = object();
					n.setString("t", mode == "of" ? "forof" : "forin");
					n.setString("var", name);
					n.setString("kw", kw);
					n.setVar("iter", iter);
					n.setObject("body", body);
					return n;
				}
			}
			i = save;
		}
		object init;
		if (atKw("let") || atKw("const") || atKw("var")) {
			init = parseLet();
		} else {
			var e = var();
			if (!(toks[i].type == Tok::Punc && toks[i].lex == ";")) {
				e = parseExpression();
			}
			init = object();
			init.setString("t", "expr");
			init.setVar("e", e);
			if (toks[i].type == Tok::Punc && toks[i].lex == ";") i++;
		}
		var cond;
		if (!(toks[i].type == Tok::Punc && toks[i].lex == ";")) {
			cond = parseExpression();
		}
		if (toks[i].type == Tok::Punc && toks[i].lex == ";") i++;
		var inc;
		if (!(toks[i].type == Tok::Punc && toks[i].lex == ")")) {
			inc = parseExpression();
		}
		expectPunc(")");
		auto body = parseStatement();
		auto n = object();
		n.setString("t", "for");
		n.setObject("init", init);
		if (cond.getType() != typeNull) n.setVar("cond", cond);
		if (inc.getType() != typeNull) n.setVar("inc", inc);
		n.setObject("body", body);
		return n;
	}

	list Parser::parseParams() {
		auto params = list();
		if (toks[i].type == Tok::Punc && toks[i].lex == "(") {
			i++;
			if (!(toks[i].type == Tok::Punc && toks[i].lex == ")")) {
				while (true) {
					string name;
					expectIdent(name);
					string type = parseType();
					var def;
					if (toks[i].type == Tok::Op && toks[i].lex == "=") {
						i++;
						def = parseExpression();
					}
					auto p = object();
					p.setString("n", name);
					if (!type.empty()) p.setString("type", type);
					if (def.getType() != typeNull) p.setVar("def", def);
					params.pushObject(p);
					if (toks[i].type == Tok::Punc && toks[i].lex == ",") {
						i++;
						continue;
					}
					break;
				}
			}
			expectPunc(")");
		} else {
			string name;
			expectIdent(name);
			auto p = object();
			p.setString("n", name);
			string type = parseType();
			if (!type.empty()) p.setString("type", type);
			params.pushObject(p);
		}
		return params;
	}

	object Parser::parseFunction([[maybe_unused]] bool named) {
		bool isAsync = false;
		if (atKw("async")) {
			isAsync = true;
			i++;  // async
		}
		i++;  // function
		string name;
		if (toks[i].type == Tok::Ident) name = advance().lex;
		auto params = parseParams();
		string retType = parseType();
		auto body = parseBlock();
		auto n = object();
		n.setString("t", "fn");
		if (!name.empty()) n.setString("n", name);
		n.setList("params", params);
		n.setObject("body", body);
		if (!retType.empty()) n.setString("retType", retType);
		if (isAsync) n.setBool("async", true);
		return n;
	}

	object Parser::parseClass() {
		i++;  // class
		string name;
		expectIdent(name);
		string parent;
		if (atKw("extends")) {
			i++;
			expectIdent(parent);
		}
		expectPunc("{");
		auto methods = list();
		while (!(toks[i].type == Tok::Punc && toks[i].lex == "}") &&
			toks[i].type != Tok::Eof) {
			string mname;
			if (toks[i].type == Tok::Ident ||
				(toks[i].type == Tok::Kw && toks[i].lex == "constructor"))
				mname = advance().lex;
			else throw runtime_error("expected method name");
			auto params = parseParams();
			auto body = parseBlock();
			auto fn = object();
			fn.setString("t", "fn");
			fn.setString("n", mname);
			fn.setList("params", params);
			fn.setObject("body", body);
			methods.pushObject(fn);
		}
		expectPunc("}");
		auto n = object();
		n.setString("t", "class");
		n.setString("n", name);
		if (!parent.empty()) n.setString("parent", parent);
		n.setList("methods", methods);
		return n;
	}

	object Parser::parseReturn() {
		i++;
		var value;
		if (!(toks[i].type == Tok::Punc && toks[i].lex == ";") &&
			toks[i].type != Tok::Eof &&
			!(toks[i].type == Tok::Punc && toks[i].lex == "}"))
			value = parseExpression();
		if (toks[i].type == Tok::Punc && toks[i].lex == ";") i++;
		auto n = object();
		n.setString("t", "return");
		if (value.getType() != typeNull) n.setVar("v", value);
		return n;
	}

	object Parser::parseThrow() {
		i++;  // throw
		auto value = parseExpression();
		if (toks[i].type == Tok::Punc && toks[i].lex == ";") i++;
		auto n = object();
		n.setString("t", "throw");
		n.setVar("v", value);
		return n;
	}

	object Parser::parseTry() {
		i++;  // try
		auto body = parseBlock();
		object n;
		n.setString("t", "try");
		n.setObject("body", body);
		if (atKw("catch")) {
			i++;
			expectPunc("(");
			string param;
			expectIdent(param);
			expectPunc(")");
			auto catchBody = parseBlock();
			n.setString("param", param);
			n.setObject("catch", catchBody);
		}
		if (atKw("finally")) {
			i++;
			n.setObject("finally", parseBlock());
		}
		return n;
	}

	object Parser::parseExprStmt() {
		auto e = parseExpression();
		if (toks[i].type == Tok::Punc && toks[i].lex == ";") i++;
		auto n = object();
		n.setString("t", "expr");
		n.setVar("e", e);
		return n;
	}

	// expressions (precedence climbing)
	var Parser::parseExpression() { return parseAssignment(); }

	var Parser::parseAssignment() {
		auto left = parseTernary();
		if (toks[i].type == Tok::Op &&
			(toks[i].lex == "=" || toks[i].lex == "+=" ||
			 toks[i].lex == "-=" || toks[i].lex == "*=" ||
			 toks[i].lex == "/=")) {
			auto op = advance().lex;
			auto value = parseAssignment();
			auto n = object();
			n.setString("t", "assign");
			n.setVar("target", left);
			n.setVar("value", value);
			n.setString("op", op);
			return var(n);
		}
		return left;
	}

	var Parser::parseTernary() {
		auto cond = parseLogicalOr();
		if (toks[i].type == Tok::Op && toks[i].lex == "?") {
			i++;
			auto a = parseExpression();
			if (!(toks[i].type == Tok::Punc && toks[i].lex == ":"))
				throw runtime_error("expected : in ternary");
			i++;
			auto b = parseExpression();
			auto n = object();
			n.setString("t", "tern");
			n.setVar("c", cond);
			n.setVar("a", a);
			n.setVar("b", b);
			return var(n);
		}
		return cond;
	}

	var Parser::parseLogicalOr() {
		auto left = parseLogicalAnd();
		while (toks[i].type == Tok::Op &&
			(toks[i].lex == "||" || toks[i].lex == "??")) {
			auto op = advance().lex;
			auto right = parseLogicalAnd();
			auto n = object();
			n.setString("t", "bin");
			n.setString("op", op);
			n.setVar("l", left);
			n.setVar("r", right);
			left = var(n);
		}
		return left;
	}

	var Parser::parseLogicalAnd() {
		auto left = parseEquality();
		while (toks[i].type == Tok::Op && toks[i].lex == "&&") {
			i++;
			auto right = parseEquality();
			auto n = object();
			n.setString("t", "bin");
			n.setString("op", "&&");
			n.setVar("l", left);
			n.setVar("r", right);
			left = var(n);
		}
		return left;
	}

	var Parser::parseEquality() {
		auto left = parseRelational();
		while (toks[i].type == Tok::Op &&
			(toks[i].lex == "==" || toks[i].lex == "!=" ||
			 toks[i].lex == "===" || toks[i].lex == "!==")) {
			auto op = advance().lex;
			auto right = parseRelational();
			auto n = object();
			n.setString("t", "bin");
			n.setString("op", op);
			n.setVar("l", left);
			n.setVar("r", right);
			left = var(n);
		}
		return left;
	}

	var Parser::parseRelational() {
		auto left = parseAdditive();
		while (toks[i].type == Tok::Op &&
			(toks[i].lex == "<" || toks[i].lex == ">" ||
			 toks[i].lex == "<=" || toks[i].lex == ">=")) {
			auto op = advance().lex;
			auto right = parseAdditive();
			auto n = object();
			n.setString("t", "bin");
			n.setString("op", op);
			n.setVar("l", left);
			n.setVar("r", right);
			left = var(n);
		}
		if (toks[i].type == Tok::Kw && toks[i].lex == "in") {
			i++;
			auto right = parseAdditive();
			auto n = object();
			n.setString("t", "bin");
			n.setString("op", "in");
			n.setVar("l", left);
			n.setVar("r", right);
			left = var(n);
		}
		return left;
	}

	var Parser::parseAdditive() {
		auto left = parseMultiplicative();
		while (toks[i].type == Tok::Op &&
			(toks[i].lex == "+" || toks[i].lex == "-")) {
			auto op = advance().lex;
			auto right = parseMultiplicative();
			auto n = object();
			n.setString("t", "bin");
			n.setString("op", op);
			n.setVar("l", left);
			n.setVar("r", right);
			left = var(n);
		}
		return left;
	}

	var Parser::parseMultiplicative() {
		auto left = parseUnary();
		while (toks[i].type == Tok::Op &&
			(toks[i].lex == "*" || toks[i].lex == "/" ||
			 toks[i].lex == "%" || toks[i].lex == "**")) {
			auto op = advance().lex;
			auto right = parseUnary();
			auto n = object();
			n.setString("t", "bin");
			n.setString("op", op);
			n.setVar("l", left);
			n.setVar("r", right);
			left = var(n);
		}
		return left;
	}

	var Parser::parseUnary() {
		if (toks[i].type == Tok::Op &&
			(toks[i].lex == "!" || toks[i].lex == "-" ||
			 toks[i].lex == "+")) {
			auto op = advance().lex;
			auto a = parseUnary();
			auto n = object();
			n.setString("t", "un");
			n.setString("op", op);
			n.setVar("a", a);
			return var(n);
		}
		if (toks[i].type == Tok::Kw && toks[i].lex == "typeof") {
			i++;
			auto a = parseUnary();
			auto n = object();
			n.setString("t", "typeof");
			n.setVar("a", a);
			return var(n);
		}
		if (toks[i].type == Tok::Kw && toks[i].lex == "await") {
			i++;  // await
			auto a = parseUnary();
			auto n = object();
			n.setString("t", "await");
			n.setVar("a", a);
			return var(n);
		}
		return parsePostfix();
	}

	var Parser::parsePostfix() {
		auto expr = parsePrimary();
		while (true) {
			if (toks[i].type == Tok::Punc && toks[i].lex == ".") {
				i++;
				string prop;
				expectIdent(prop);
				auto n = object();
				n.setString("t", "member");
				n.setVar("o", expr);
				n.setString("p", prop);
				expr = var(n);
			} else if (toks[i].type == Tok::Punc &&
				toks[i].lex == "[") {
				i++;
				auto idx = parseExpression();
				expectPunc("]");
				auto n = object();
				n.setString("t", "index");
				n.setVar("o", expr);
				n.setVar("i", idx);
				expr = var(n);
			} else if (toks[i].type == Tok::Op &&
				(toks[i].lex == "++" || toks[i].lex == "--")) {
				auto op = advance().lex;
				auto n = object();
				n.setString("t", "postfix");
				n.setString("op", op);
				n.setVar("a", expr);
				expr = var(n);
			} else if (toks[i].type == Tok::Punc &&
				toks[i].lex == "(") {
				i++;
				auto args = list();
				if (!(toks[i].type == Tok::Punc && toks[i].lex == ")")) {
					while (true) {
						args.pushVar(parseExpression());
						if (toks[i].type == Tok::Punc && toks[i].lex == ",") {
							i++;
							continue;
						}
						break;
					}
				}
				expectPunc(")");
				auto n = object();
				n.setString("t", "call");
				n.setVar("f", expr);
				n.setList("args", args);
				expr = var(n);
			} else {
				break;
			}
		}
		return expr;
	}

	// arrow function: `(a, b) => ...` or `a => ...`
	var Parser::tryArrow() {
		// pattern: Ident => ... | ( params ) => ...
		size_t save = i;
		list params;
		if (toks[i].type == Tok::Ident) {
			auto name = toks[i].lex;
			// a single ident param only if followed by =>
			if (toks[i + 1].type == Tok::Op && toks[i + 1].lex == "=>") {
				auto p = object();
				p.setString("n", name);
				params.pushObject(p);
				i += 2;
			} else {
				return var();
			}
		} else if (toks[i].type == Tok::Punc && toks[i].lex == "(") {
			// Only treat as arrow params when the first token is an
			// identifier or it's empty; otherwise it's a grouping.
			auto inner = toks[i + 1];
			bool paramsLike = (inner.type == Tok::Ident) ||
				(inner.type == Tok::Punc && inner.lex == ")");
			if (!paramsLike) return var();
			params = parseParams();
			if (!(toks[i].type == Tok::Op && toks[i].lex == "=>")) {
				i = save;  // restore: not an arrow, it's grouping
				return var();
			}
			i++;
		} else {
			return var();
		}
		string retType = parseType();
		var body;
		if (toks[i].type == Tok::Punc && toks[i].lex == "{") {
			body = var(parseBlock());
		} else {
			body = parseExpression();
		}
		auto n = object();
		n.setString("t", "arrow");
		n.setList("params", params);
		n.setVar("body", body);
		if (!retType.empty()) n.setString("retType", retType);
		return var(n);
	}

	var Parser::parsePrimary() {
		auto t = toks[i];
		if (t.type == Tok::Num) {
			i++;
			auto n = object();
			n.setString("t", "num");
			n.setDouble("v", t.num);
			return var(n);
		}
		if (t.type == Tok::Str) {
			i++;
			auto n = object();
			n.setString("t", "str");
			n.setString("v", t.lex);
			return var(n);
		}
		if (t.type == Tok::Tpl) {
			i++;
			// split raw into literal / ${expr} parts
			auto parts = list();
			string lit;
			size_t p = 0;
			string raw = t.lex;
			while (p < raw.size()) {
				if (p + 2 < raw.size() && raw[p] == '$' && raw[p + 1] == '{') {
					if (!lit.empty()) {
						parts.pushString(lit);
						lit.clear();
					}
					// find matching }
					int depth = 1;
					size_t q = p + 2;
					string exprSrc;
					while (q < raw.size() && depth > 0) {
						if (raw[q] == '{') depth++;
						else if (raw[q] == '}') {
							depth--;
							if (depth == 0) break;
						}
						exprSrc += raw[q];
						q++;
					}
					if (depth != 0)
						throw runtime_error("unterminated ${ in template");
					// parse the inner expression
					Lexer sub(exprSrc);
					if (!sub.scan())
						throw runtime_error("bad ${ expression: " + sub.error());
					Parser subParser(sub.tokens);
					auto subAst = subParser.parse();
					if (subParser.error().size() > 0)
						throw runtime_error("bad ${ expression");
					auto body = subAst.getList("body");
					if (body.size() != 1)
						throw runtime_error("expected expression in ${");
					parts.pushObject(body.getObject(0).getVar("e").getObject());
					p = q + 1;
				} else {
					lit += raw[p];
					p++;
				}
			}
			if (!lit.empty()) parts.pushString(lit);
			auto n = object();
			n.setString("t", "tpl");
			n.setList("parts", parts);
			return var(n);
		}
		if (t.type == Tok::Ident) {
			i++;
			// arrow function shorthand
			if (toks[i].type == Tok::Op && toks[i].lex == "=>") {
				auto p = object();
				p.setString("n", t.lex);
				auto params = list();
				params.pushObject(p);
				i++;
				var body;
				if (toks[i].type == Tok::Punc && toks[i].lex == "{")
					body = var(parseBlock());
				else body = parseExpression();
				auto n = object();
				n.setString("t", "arrow");
				n.setList("params", params);
				n.setVar("body", body);
				return var(n);
			}
			auto n = object();
			n.setString("t", "ident");
			n.setString("n", t.lex);
			return var(n);
		}
		if (t.type == Tok::Kw) {
			if (t.lex == "true" || t.lex == "false") {
				i++;
				auto n = object();
				n.setString("t", "bool");
				n.setBool("v", t.lex == "true");
				return var(n);
			}
			if (t.lex == "null" || t.lex == "undefined") {
				i++;
				auto n = object();
				n.setString("t", "null");
				return var(n);
			}
			if (t.lex == "this") {
				i++;
				auto n = object();
				n.setString("t", "this");
				return var(n);
			}
			if (t.lex == "async") {
				// async arrow: `async (a, b) => ...` or `async a => ...`
				size_t save = i;
				i++;  // async
				list params;
				if (toks[i].type == Tok::Ident) {
					auto p = object();
					p.setString("n", toks[i].lex);
					params.pushObject(p);
					i++;
				} else if (toks[i].type == Tok::Punc && toks[i].lex == "(") {
					params = parseParams();
				} else {
					i = save;
					throw runtime_error("unexpected 'async'");
				}
				if (!(toks[i].type == Tok::Op && toks[i].lex == "=>")) {
					i = save;
					throw runtime_error("expected '=>' after async params");
				}
				i++;  // =>
				string retType = parseType();
				var body;
				if (toks[i].type == Tok::Punc && toks[i].lex == "{")
					body = var(parseBlock());
				else body = parseExpression();
				auto n = object();
				n.setString("t", "arrow");
				n.setList("params", params);
				n.setVar("body", body);
				if (!retType.empty()) n.setString("retType", retType);
				n.setBool("async", true);
				return var(n);
			}
			if (t.lex == "new") {
				i++;
				string cls;
				expectIdent(cls);
				auto args = list();
				if (toks[i].type == Tok::Punc && toks[i].lex == "(") {
					i++;
					if (!(toks[i].type == Tok::Punc && toks[i].lex == ")")) {
						while (true) {
							args.pushVar(parseExpression());
							if (toks[i].type == Tok::Punc && toks[i].lex == ",") {
								i++;
								continue;
							}
							break;
						}
					}
					expectPunc(")");
				}
				auto n = object();
				n.setString("t", "new");
				n.setString("n", cls);
				n.setList("args", args);
				return var(n);
			}
			throw runtime_error("unexpected keyword '" + t.lex + "'");
		}
		if (t.type == Tok::Punc) {
			if (t.lex == "(") {
				// arrow fn or grouping
				auto arrow = tryArrow();
				if (arrow.getType() != typeNull) return arrow;
				i++;
				auto e = parseExpression();
				expectPunc(")");
				return e;
			}
			if (t.lex == "{") {
				// object literal or block? In expression context, object.
				return var(parseObjectLit());
			}
			if (t.lex == "[") {
				i++;
				auto items = list();
				if (!(toks[i].type == Tok::Punc && toks[i].lex == "]")) {
					while (true) {
						items.pushVar(parseExpression());
						if (toks[i].type == Tok::Punc && toks[i].lex == ",") {
							i++;
							if (toks[i].type == Tok::Punc && toks[i].lex == "]")
								break;  // trailing comma
							continue;
						}
						break;
					}
				}
				expectPunc("]");
				auto n = object();
				n.setString("t", "arr");
				n.setList("items", items);
				return var(n);
			}
		}
		throw runtime_error("unexpected token '" + t.lex + "'");
	}

	object Parser::parseObjectLit() {
		expectPunc("{");
		auto props = list();
		while (!(toks[i].type == Tok::Punc && toks[i].lex == "}") &&
			toks[i].type != Tok::Eof) {
			string key;
			if (toks[i].type == Tok::Ident) key = advance().lex;
			else if (toks[i].type == Tok::Str) key = advance().lex;
			else throw runtime_error("expected property name");
			if (toks[i].type == Tok::Punc && toks[i].lex == ":") {
				i++;
				auto value = parseExpression();
				auto p = object();
				p.setString("k", key);
				p.setVar("v", value);
				props.pushObject(p);
			} else {
				// shorthand { name }
				auto p = object();
				p.setString("k", key);
				auto n = object();
				n.setString("t", "ident");
				n.setString("n", key);
				p.setVar("v", var(n));
				props.pushObject(p);
			}
			if (toks[i].type == Tok::Punc && toks[i].lex == ",") {
				i++;
				if (toks[i].type == Tok::Punc && toks[i].lex == "}")
					break;  // trailing comma
				continue;
			}
			break;
		}
		expectPunc("}");
		auto n = object();
		n.setString("t", "obj");
		n.setList("props", props);
		return n;
	}

	Parser::Parser(const std::vector<Token>& t) : toks(t) {}

	object Parser::parse() {
		try {
			return parseProgram();
		} catch (const exception& e) {
			err = e.what();
			return object();
		}
	}
	const std::string& Parser::error() const { return err; }

}  // namespace lang
}  // namespace gold
