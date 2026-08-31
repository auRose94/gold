#pragma once

// lexer.hpp — tokenizer for the gold::lang scripting language.
//
// Internal to the lang module (not installed). Produces a flat token
// stream consumed by the parser.

#include <string>
#include <string_view>
#include <vector>

namespace gold {
namespace lang {

	enum class Tok {
		Num, Str, Tpl, Ident, Kw, Punc, Op, Eof,
	};

	struct Token {
		Tok type;
		std::string lex;
		double num = 0;
		int line = 1;
	};

	bool isIdentStart(char c);
	bool isIdentChar(char c);

	class Lexer {
		std::string_view src;
		size_t pos = 0;
		int line = 1;
		std::string err;

		char peek(size_t off = 0) const;
		char next();
		bool eof() const;

		void scanNumber();
		void scanString(char quote);
		void scanTemplate();
		void scanIdent();
		bool scanOp();

	 public:
		std::vector<Token> tokens;

		explicit Lexer(std::string_view s);

		bool scan();
		const std::string& error() const;
	};

}  // namespace lang
}  // namespace gold
