#include "lang/lang.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "goldjs.hpp"

namespace gold {
	using namespace std;

	namespace {

		// ------------------------------------------------------------------
		// tokens & lexer
		// ------------------------------------------------------------------

		enum class Tok {
			Num, Str, Tpl, Ident, Kw, Punc, Op, Eof,
		};

		struct Token {
			Tok type;
			string lex;
			double num = 0;
			int line = 1;
		};

		bool isIdentStart(char c) {
			return isalpha((unsigned char)c) || c == '_' || c == '$';
		}
		bool isIdentChar(char c) {
			return isalnum((unsigned char)c) || c == '_' || c == '$';
		}

		class Lexer {
			string_view src;
			size_t pos = 0;
			int line = 1;
			string err;

			char peek(size_t off = 0) const {
				return pos + off < src.size() ? src[pos + off] : '\0';
			}
			char next() {
				if (pos < src.size()) return src[pos++];
				return '\0';
			}
			bool eof() const { return pos >= src.size(); }

			void scanNumber() {
				size_t start = pos;
				bool isFloat = false;
				while (isdigit((unsigned char)peek())) next();
				if (peek() == '.') {
					isFloat = true;
					next();
					while (isdigit((unsigned char)peek())) next();
				}
				if (peek() == 'e' || peek() == 'E') {
					isFloat = true;
					next();
					if (peek() == '+' || peek() == '-') next();
					while (isdigit((unsigned char)peek())) next();
				}
				string lex(src.substr(start, pos - start));
				Token t;
				t.type = Tok::Num;
				t.lex = lex;
				t.num = strtod(lex.c_str(), nullptr);
				t.line = line;
				tokens.push_back(t);
			}

			void scanString(char quote) {
				next();  // opening quote
				string out;
				while (!eof() && peek() != quote) {
					char c = next();
					if (c == '\\') {
						char e = next();
						switch (e) {
							case 'n': out += '\n'; break;
							case 't': out += '\t'; break;
							case 'r': out += '\r'; break;
							case 'b': out += '\b'; break;
							case 'f': out += '\f'; break;
							case '0': out += '\0'; break;
							case '\\': out += '\\'; break;
							case '\'': out += '\''; break;
							case '"': out += '"'; break;
							case '`': out += '`'; break;
							case 'u': {
								unsigned v = 0;
								for (int i = 0; i < 4; ++i) {
									char h = next();
									v <<= 4;
									if (h >= '0' && h <= '9') v |= h - '0';
									else if (h >= 'a' && h <= 'f') v |= h - 'a' + 10;
									else if (h >= 'A' && h <= 'F') v |= h - 'A' + 10;
								}
								if (v < 0x80) out += char(v);
								else if (v < 0x800) {
									out += char(0xC0 | (v >> 6));
									out += char(0x80 | (v & 0x3F));
								} else {
									out += char(0xE0 | (v >> 12));
									out += char(0x80 | ((v >> 6) & 0x3F));
									out += char(0x80 | (v & 0x3F));
								}
								break;
							}
							default: out += e; break;
						}
					} else {
						out += c;
					}
				}
				if (peek() != quote) {
					err = "unterminated string";
					return;
				}
				next();  // closing quote
				Token t;
				t.type = Tok::Str;
				t.lex = out;
				t.line = line;
				tokens.push_back(t);
			}

			void scanTemplate() {
				next();  // opening backtick
				string raw;
				while (!eof() && peek() != '`') {
					if (peek() == '\\') {
						raw += next();
						raw += next();
					} else {
						raw += next();
					}
				}
				if (peek() != '`') {
					err = "unterminated template literal";
					return;
				}
				next();  // closing backtick
				Token t;
				t.type = Tok::Tpl;
				t.lex = raw;
				t.line = line;
				tokens.push_back(t);
			}

			void scanIdent() {
				size_t start = pos;
				while (isIdentChar(peek())) next();
				string lex(src.substr(start, pos - start));
				static const char* kws[] = {
					"let", "const", "var", "function", "return", "if",
					"else", "while", "for", "break", "continue", "class",
					"constructor", "new", "this", "true", "false", "null",
					"undefined",
				};
				Token t;
				t.type = Tok::Ident;
				t.lex = lex;
				t.line = line;
				for (auto k : kws)
					if (lex == k) { t.type = Tok::Kw; break; }
				tokens.push_back(t);
			}

			bool scanOp() {
				// multi-char operators first
				static const char* ops[] = {
					"===", "!==", "=>", "==", "!=", "<=", ">=", "&&",
					"||", "+=", "-=", "*=", "/=", "++", "--",
				};
				for (auto o : ops) {
					size_t n = strlen(o);
					if (src.compare(pos, n, o) == 0) {
						Token t;
						t.type = Tok::Op;
						t.lex = o;
						t.line = line;
						tokens.push_back(t);
						pos += n;
						return true;
					}
				}
				if (string("+-*/%=<>!?.:,;()[]{}").find(peek()) != string::npos) {
					Token t;
					t.type = peek() == '(' || peek() == ')' ||
							peek() == '[' || peek() == ']' ||
							peek() == '{' || peek() == '}' ||
							peek() == ',' || peek() == ';' ||
							peek() == '.' || peek() == ':'
						? Tok::Punc
						: Tok::Op;
					t.lex = string(1, peek());
					t.line = line;
					tokens.push_back(t);
					next();
					return true;
				}
				return false;
			}

		 public:
			vector<Token> tokens;

			Lexer(string_view s) : src(s) {}

			bool scan() {
				while (!eof()) {
					char c = peek();
					if (c == '\n') { line++; next(); continue; }
					if (isspace((unsigned char)c)) { next(); continue; }
					if (c == '/' && peek(1) == '/') {
						while (!eof() && peek() != '\n') next();
						continue;
					}
					if (c == '/' && peek(1) == '*') {
						next(); next();
						while (!eof() && !(peek() == '*' && peek(1) == '/')) {
							if (peek() == '\n') line++;
							next();
						}
						if (eof()) { err = "unterminated comment"; break; }
						next(); next();
						continue;
					}
					if (isdigit((unsigned char)c) ||
						(c == '.' && isdigit((unsigned char)peek(1)))) {
						scanNumber();
						continue;
					}
					if (c == '"' || c == '\'') { scanString(c); continue; }
					if (c == '`') { scanTemplate(); continue; }
					if (isIdentStart(c)) { scanIdent(); continue; }
					if (scanOp()) continue;
					err = "unexpected character '" + string(1, c) + "'";
					break;
				}
				Token t;
				t.type = Tok::Eof;
				t.lex = "";
				t.line = line;
				tokens.push_back(t);
				return err.empty();
			}
			const string& error() const { return err; }
		};

		// ------------------------------------------------------------------
		// parser (produces a gold-object AST)
		// ------------------------------------------------------------------

		class Parser {
			const vector<Token>& toks;
			size_t i = 0;
			string err;

			const Token& cur() { return toks[i]; }
			const Token& advance() { return toks[i++]; }
			bool at(Tok t) const { return toks[i].type == t; }
			bool atLex(const char* l) const {
				return toks[i].lex == l;
			}
			bool atKw(const char* k) const {
				return toks[i].type == Tok::Kw && toks[i].lex == k;
			}
			bool checkPunc(const char* p) {
				if (toks[i].type == Tok::Punc && toks[i].lex == p) {
					i++;
					return true;
				}
				return false;
			}
			bool checkOp(const char* o) {
				if (toks[i].type == Tok::Op && toks[i].lex == o) {
					i++;
					return true;
				}
				return false;
			}
			bool checkIdent() {
				if (toks[i].type == Tok::Ident) { i++; return true; }
				return false;
			}
			void expectPunc(const char* p) {
				if (!checkPunc(p)) throw runtime_error(
					"expected '" + string(p) + "'");
			}
			void expectIdent(string& out) {
				if (toks[i].type == Tok::Ident) out = advance().lex;
				else throw runtime_error("expected identifier");
			}
			// A type annotation like `: number` or `: string[]` or
			// `: Array<string>`. Parsed (loosely) and returned as a string.
			string parseType() {
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

			// statements
			object parseProgram();
			object parseStatement();
			object parseLet();
			object parseBlock();
			object parseIf();
			object parseWhile();
			object parseFor();
			object parseFunction(bool named);
			object parseClass();
			object parseReturn();
			object parseExprStmt();
			var parseExpression();
			var parseAssignment();
			var parseTernary();
			var parseLogicalOr();
			var parseLogicalAnd();
			var parseEquality();
			var parseRelational();
			var parseAdditive();
			var parseMultiplicative();
			var parseUnary();
			var parsePostfix();
			var parsePrimary();
			object parseObjectLit();
			list parseParams();
			var tryArrow();

		 public:
			Parser(const vector<Token>& t) : toks(t) {}

			object parse() {
				try {
					return parseProgram();
				} catch (const exception& e) {
					err = e.what();
					return object();
				}
			}
			const string& error() const { return err; }
		};

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
			if (atKw("class")) return parseClass();
			if (atKw("return")) return parseReturn();
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
			string name;
			expectIdent(name);
			string type = parseType();
			var value;
			bool hasValue = false;
			if (checkOp("=")) {
				value = parseExpression();
				hasValue = true;
			}
			// allow semicolon or newline (ASI for statements)
			if (toks[i].type == Tok::Punc && toks[i].lex == ";") i++;
			auto n = object();
			n.setString("t", "let");
			n.setString("n", name);
			n.setString("kw", kw);
			if (!type.empty()) n.setString("type", type);
			if (hasValue) n.setVar("v", value);
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
						auto p = object();
						p.setString("n", name);
						if (!type.empty()) p.setString("type", type);
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

		object Parser::parseFunction(bool named) {
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
			while (toks[i].type == Tok::Op && toks[i].lex == "||") {
				i++;
				auto right = parseLogicalAnd();
				auto n = object();
				n.setString("t", "bin");
				n.setString("op", "||");
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
				 toks[i].lex == "%")) {
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
				auto save2 = i;
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

		// ------------------------------------------------------------------
		// interpreter
		// ------------------------------------------------------------------

		struct Signal {
			enum Kind { Return, Break, Continue } kind;
			var value;
		};

		// A function/class stored in the runtime as a gold object.
		bool isFn(const var& v) {
			return v.isObject() &&
				v.getObject().getBool("__goldFn");
		}
		bool isClass(const var& v) {
			return v.isObject() &&
				v.getObject().getBool("__goldClass");
		}

		class Interpreter {
			object globals;
			bool enforceTypes;
			string err;

			bool truthy(const var& v) {
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

			bool typeMatches(const string& type, const var& v) {
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

			void checkType(const string& type, const var& v) {
				if (enforceTypes && !typeMatches(type, v)) {
					err = "type error: expected '" + type + "', got '" +
						string(v.getTypeString()) + "'";
					throw runtime_error(err);
				}
			}

			var lookup(const string& name, object env) {
				if (env.getVar(name).getType() != typeNull)
					return env.getVar(name);
				return var();
			}

			void assignIdent(const string& name, const var& value,
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

		 public:
			var callFn(const var& fn, list args, var thisArg = var()) {
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

		 private:

			// executes a block (or arrow body); returns last value
			var execBlock(object node, object env) {
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

			var makeFn(object node, object env) {
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

			var instantiateClass(object cls, object env, list args) {
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

			var evalNode(var node, object env) {
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
					auto out = list();
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
					return obj.getObject().getVar(o.getString("p"));
				}
				if (t == "index") {
					auto obj = evalNode(o.getVar("o"), env);
					auto idx = evalNode(o.getVar("i"), env);
					if (obj.isList()) return obj.getList().getVar(idx.getUInt64());
					if (obj.isObject()) return obj.getObject().getVar((string)idx);
					return var();
				}
				if (t == "call") {
					auto fn = evalNode(o.getVar("f"), env);
					auto args = list();
					auto argNodes = o.getList("args");
					for (auto it = argNodes.begin(); it != argNodes.end(); ++it)
						args.pushVar(evalNode(*it, env));
					// this-binding for method calls (a.b(...) where b is a fn)
					var thisArg;
					auto callee = o.getVar("f").getObject();
					if (callee.getString("t") == "member") {
						thisArg = evalNode(callee.getVar("o"), env);
					}
					return callFn(fn, args, thisArg);
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
				if (t == "bin") {
					auto op = o.getString("op");
					auto l = evalNode(o.getVar("l"), env);
					auto r = evalNode(o.getVar("r"), env);
					if (op == "&&") return var(truthy(l) && truthy(r));
					if (op == "||") return var(truthy(l) || truthy(r));
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
					return v;
				}
				if (t == "tern") {
					auto c = evalNode(o.getVar("c"), env);
					return truthy(c) ? evalNode(o.getVar("a"), env)
						: evalNode(o.getVar("b"), env);
				}
				throw runtime_error("unknown AST node '" + t + "'");
			}

			var execNode(object node, object env) {
				auto t = node.getString("t");
				if (t == "let") {
					var value;
					if (node.getVar("v").getType() != typeNull)
						value = evalNode(node.getVar("v"), env);
					checkType(node.getString("type"), value);
					env[node.getString("n")] = value;
					return value;
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

		 public:
			Interpreter(object g, bool t) : globals(std::move(g)), enforceTypes(t) {}

			var run(object program) {
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
			var eval(object program) {
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
			object& globalsRef() { return globals; }
			const string& error() const { return err; }
		};

		object ensureData(object o) {
			o.setNull("__gold_init");
			o.erase("__gold_init");
			return o;
		}

		object makeGlobals(object g) {
			object globals;
			if (g) globals.copy(g);
			return ensureData(globals);
		}

		// Compile (parse) source into a program AST.
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

	}  // namespace

	// ----------------------------------------------------------------------
	// public API
	// ----------------------------------------------------------------------

	var langParse(string_view source) {
		string err;
		auto ast = compile(source, err);
		if (ast) return var(ast);
		return genericError(err);
	}

	var langRun(string_view source, object globals, bool enforceTypes) {
		string err;
		auto ast = compile(source, err);
		if (!ast) return genericError(err);
		auto g = makeGlobals(globals);
		Interpreter interp(g, enforceTypes);
		return interp.run(ast);
	}

	var langEval(string_view source, object globals, bool enforceTypes) {
		string err;
		auto ast = compile(source, err);
		if (!ast) return genericError(err);
		auto g = makeGlobals(globals);
		Interpreter interp(g, enforceTypes);
		return interp.eval(ast);
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
		auto ast = compile(source, err);
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
			globals = ensureData(object());
			setObject("globals", globals);
		}
		Interpreter interp(ensureData(globals), getBool("types"));
		auto r = interp.run(ast);
		if (r.isError()) setString("error", (string)*r.getError());
		else setString("error", "");
		return r;
	}

	var script::eval(list args) {
		auto source = args[0].getString();
		string err;
		auto ast = compile(source, err);
		if (!ast) return genericError(err);
		auto globals = getObject("globals");
		if (!globals) {
			globals = ensureData(object());
			setObject("globals", globals);
		}
		Interpreter interp(ensureData(globals), getBool("types"));
		return interp.eval(ast);
	}

	var script::setGlobal(list args) {
		auto name = args[0].getString();
		auto value = args[1];
		auto globals = getObject("globals");
		if (!globals) {
			globals = ensureData(object());
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
		if (!isFn(fn))
			return genericError("'" + name + "' is not a function");
		list callArgs;
		for (uint64_t i = 1; i < args.size(); ++i)
			callArgs.pushVar(args.getVar(i));
		Interpreter interp(globals, getBool("types"));
		try {
			return interp.callFn(fn, callArgs);
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

}  // namespace gold