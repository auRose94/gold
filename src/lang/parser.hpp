#pragma once

// parser.hpp — recursive-descent parser for the gold::lang scripting
// language. Produces a gold-object AST (statements are a list under
// "body"). Internal to the lang module (not installed).

#include <string>
#include <vector>

#include "types.hpp"
#include "lexer.hpp"

namespace gold {
namespace lang {

	class Parser {
		const std::vector<Token>& toks;
		size_t i = 0;
		std::string err;

		const Token& cur();
		const Token& advance();
		bool at(Tok t) const;
		bool atLex(const char* l) const;
		bool atKw(const char* k) const;
		bool checkPunc(const char* p);
		bool checkOp(const char* o);
		bool checkIdent();
		void expectPunc(const char* p);
		void expectIdent(std::string& out);
		std::string parseType();

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
		object parseThrow();
		object parseTry();
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
		explicit Parser(const std::vector<Token>& t);

		object parse();
		const std::string& error() const;
	};

}  // namespace lang
}  // namespace gold
