#include "lexer.hpp"

#include <cctype>
#include <cstring>

namespace gold {
namespace lang {

	bool isIdentStart(char c) {
		return isalpha((unsigned char)c) || c == '_' || c == '$';
	}
	bool isIdentChar(char c) {
		return isalnum((unsigned char)c) || c == '_' || c == '$';
	}

	char Lexer::peek(size_t off) const {
		return pos + off < src.size() ? src[pos + off] : '\0';
	}
	char Lexer::next() {
		if (pos < src.size()) return src[pos++];
		return '\0';
	}
	bool Lexer::eof() const { return pos >= src.size(); }

	void Lexer::scanNumber() {
		size_t start = pos;
		while (isdigit((unsigned char)peek())) next();
		if (peek() == '.') {
			next();
			while (isdigit((unsigned char)peek())) next();
		}
		if (peek() == 'e' || peek() == 'E') {
			next();
			if (peek() == '+' || peek() == '-') next();
			while (isdigit((unsigned char)peek())) next();
		}
		std::string lex(src.substr(start, pos - start));
		Token t;
		t.type = Tok::Num;
		t.lex = lex;
		t.num = strtod(lex.c_str(), nullptr);
		t.line = line;
		tokens.push_back(t);
	}

	void Lexer::scanString(char quote) {
		next();  // opening quote
		std::string out;
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

	void Lexer::scanTemplate() {
		next();  // opening backtick
		std::string raw;
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

	void Lexer::scanIdent() {
		size_t start = pos;
		while (isIdentChar(peek())) next();
		std::string lex(src.substr(start, pos - start));
		static const char* kws[] = {
			"let", "const", "var", "function", "return", "if",
			"else", "while", "for", "break", "continue", "class",
			"constructor", "new", "this", "true", "false", "null",
			"undefined", "throw", "try", "catch", "finally", "of",
			"in", "typeof", "async", "await",
		};
		Token t;
		t.type = Tok::Ident;
		t.lex = lex;
		t.line = line;
		for (auto k : kws)
			if (lex == k) { t.type = Tok::Kw; break; }
		tokens.push_back(t);
	}

	bool Lexer::scanOp() {
		// multi-char operators first
		static const char* ops[] = {
			"===", "!==", "=>", "==", "!=", "<=", ">=", "&&",
			"||", "+=", "-=", "*=", "/=", "++", "--", "**",
			"??", "?.",
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
		if (std::string("+-*/%=<>!?.:,;()[]{}").find(peek()) != std::string::npos) {
			Token t;
			t.type = peek() == '(' || peek() == ')' ||
					peek() == '[' || peek() == ']' ||
					peek() == '{' || peek() == '}' ||
					peek() == ',' || peek() == ';' ||
					peek() == '.' || peek() == ':'
				? Tok::Punc
				: Tok::Op;
			t.lex = std::string(1, peek());
			t.line = line;
			tokens.push_back(t);
			next();
			return true;
		}
		return false;
	}

	Lexer::Lexer(std::string_view s) : src(s) {}

	bool Lexer::scan() {
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
			err = "unexpected character '" + std::string(1, c) + "'";
			break;
		}
		Token t;
		t.type = Tok::Eof;
		t.lex = "";
		t.line = line;
		tokens.push_back(t);
		return err.empty();
	}
	const std::string& Lexer::error() const { return err; }

}  // namespace lang
}  // namespace gold
