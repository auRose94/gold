#include "web/css.hpp"
#include <cctype>

namespace gold {
	namespace CSS {

		// Helper functions
		static string trim(const string& s) {
			size_t start = s.find_first_not_of(" \t\n\r");
			if (start == string::npos) return "";
			size_t end = s.find_last_not_of(" \t\n\r");
			return s.substr(start, end - start + 1);
		}

		static bool isNumericValue(const string& s) {
			if (s.empty()) return false;
			size_t i = 0;
			if (s[0] == '-' || s[0] == '+') i = 1;
			if (i >= s.size()) return false;
			bool hasDot = false;
			for (; i < s.size(); i++) {
				if (s[i] == '.') {
					if (hasDot) return false;
					hasDot = true;
				} else if (!isdigit(s[i])) {
					return false;
				}
			}
			return true;
		}

		list parseCSS(const string& css) {
			list rules;
			size_t pos = 0;

			while (pos < css.size()) {
				// Skip whitespace and comments
				while (pos < css.size() && isspace(css[pos])) pos++;
				if (pos >= css.size()) break;

				// Skip CSS comments
				if (pos + 3 < css.size() && css.substr(pos, 2) == "/*") {
					size_t end = css.find("*/", pos + 2);
					if (end != string::npos) {
						pos = end + 2;
					} else {
						pos = css.size();
					}
					continue;
				}

				Rule rule = parseRule(css, pos);
				if (rule.getString("selector") != "") {
					rules.pushVar(var(rule));
				}
			}

			return rules;
		}

		Rule parseRule(const string& css, size_t& pos) {
			Rule rule;

			// Skip whitespace
			while (pos < css.size() && isspace(css[pos])) pos++;
			if (pos >= css.size()) return rule;

			// Find the opening brace for selector
			size_t braceStart = css.find('{', pos);
			if (braceStart == string::npos) {
				pos = css.size();
				return rule;
			}

			rule.setString("selector", trim(css.substr(pos, braceStart - pos)));
			pos = braceStart + 1;

			// Find the end of declarations (closing brace)
			size_t declEnd = css.find('}', pos);
			if (declEnd == string::npos) declEnd = css.size();
			
			string declBody = css.substr(pos, declEnd - pos);
			rule.setObject("declarations", parseDeclarations(declBody));
			pos = declEnd + 1;

			return rule;
		}

	object parseDeclarations(const string& body) {
		object declarations;
		size_t pos = 0;
		size_t end = body.size();

		// `!important` (case-insensitive) must end the value: followed by
		// whitespace, `;`, `}` or nothing at all.
		auto importantAt = [&body, end](size_t at) -> bool {
			if (at + 10 > end) return false;
			for (size_t k = 0; k < 10; k++) {
				const char expected = (k == 0) ? '!' : "important"[k - 1];
				if (tolower((unsigned char)body[at + k]) != expected)
					return false;
			}
			if (at + 10 >= end) return true;
			const char after = body[at + 10];
			return after == ';' || after == '}' || isspace((unsigned char)after);
		};

		while (pos < end) {
			// Skip whitespace and comments
			while (pos < end && isspace(body[pos])) pos++;
			if (pos >= end) break;

			// Skip CSS comments
			if (pos + 2 <= end && body[pos] == '/' && body[pos + 1] == '*') {
				size_t endComment = body.find("*/", pos + 2);
				if (endComment != string::npos) {
					pos = endComment + 2;
				} else {
					break;
				}
				continue;
			}

			// Check for closing brace
			if (body[pos] == '}') {
				pos++;
				break;
			}

			// Find colon separator
			size_t colonPos = body.find(':', pos);
			if (colonPos == string::npos) break;

			string name = trim(body.substr(pos, colonPos - pos));
			pos = colonPos + 1;

			// Skip whitespace after colon
			while (pos < end && isspace(body[pos])) pos++;

			// Parse the value. Quotes and nested parens/brackets are
			// honored, so a `;` inside `rgb(...)` or a quoted string does
			// not end the declaration. `!important` is recorded as a flag
			// under `name + "!"` rather than glued onto the value, so it
			// survives for numeric values too.
			size_t valStart = pos;
			size_t valueEnd = pos;
			bool important = false;
			int depth = 0;
			char quote = 0;

			while (pos < end) {
				char c = body[pos];
				if (quote) {
					if (c == quote) quote = 0;
				} else if (c == '"' || c == '\'') {
					quote = c;
				} else if (c == '(' || c == '[') {
					depth++;
				} else if (c == ')' || c == ']') {
					if (depth > 0) depth--;
				} else if (depth == 0 && (c == ';' || c == '}')) {
					if (!important) valueEnd = pos;  // flag case set it earlier
					break;
				} else if (depth == 0 && c == '!' && importantAt(pos)) {
					important = true;
					valueEnd = pos;  // the flag is not part of the value
					pos += 10;       // skip "!important"
					continue;
				}
				pos++;
			}
			if (!important) valueEnd = pos;  // ran to the end of the body

			const string value =
				trim(body.substr(valStart, valueEnd - valStart));

			// Store the declaration, converting plain numbers/booleans so
			// host code reads typed data back.
			if (!name.empty() && !value.empty()) {
				if (value == "true" || value == "false") {
					declarations.setBool(name, value == "true");
				} else if (isNumericValue(value)) {
					size_t dotPos = value.find('.');
					if (dotPos != string::npos) {
						declarations.setDouble(name, stod(value));
					} else {
						declarations.setInt64(name, stoll(value));
					}
				} else {
					declarations.setString(name, value);
				}
				if (important) declarations.setBool(name + "!", true);
			}

			// Skip to next declaration (past ; or })
			if (pos < end && (body[pos] == ';' || body[pos] == '}')) pos++;
		}

		return declarations;
	}

		vector<string> getShorthandProperties() {
			static const vector<string> shorthand = {
				"background", "border", "border-top",
				"border-right", "border-bottom", "border-left",
				"border-width", "border-style", "border-color",
				"font", "margin", "padding",
				"list-style", "outline", "overflow"
			};
			return shorthand;
		}

	}  // namespace CSS
}  // namespace gold
