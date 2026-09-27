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

			while (pos < end) {
				// Skip whitespace and comments
				while (pos < end && isspace(body[pos])) pos++;
				if (pos >= end) break;

				// Skip CSS comments
				if (pos + 2 < end && body.substr(pos, 2) == "/*") {
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

				// Parse value (handle multiple values and !important)
				size_t valStart = pos;
				bool important = false;

				// Find the end of value (semicolon or closing brace)
				int parenDepth = 0;
				int bracketDepth = 0;

				while (pos < end) {
					char c = body[pos];
					if (c == '(' || c == '[') {
						if (parenDepth == 0 && bracketDepth == 0 &&
						    (pos + 9 > end || body.substr(pos + 1, 8) != "important")) {
							parenDepth += (c == '(');
							bracketDepth += (c == '[');
						}
					} else if (c == ')' || c == ']') {
						if (parenDepth > 0 || bracketDepth > 0) {
							parenDepth -= (c == ')');
							bracketDepth -= (c == ']');
						}
					} else if ((c == ';' || c == '}') && parenDepth == 0 && bracketDepth == 0) {
						break;
					} else if (c == '!' && pos + 9 < end && body.substr(pos + 1, 8) == "important") {
						important = true;
						pos += 9; // skip "!important"
						continue;
					}
					pos++;
				}

				string value = trim(body.substr(valStart, pos - valStart));

				// Handle !important flag (already skipped above)

				// Store the declaration
				// Try to convert value to appropriate type
				if (value == "true" || value == "false") {
					declarations.setBool(name, value == "true");
				} else if (isNumericValue(value)) {
					// Check if it's a float
					size_t dotPos = value.find('.');
					if (dotPos != string::npos) {
						declarations.setDouble(name, stod(value));
					} else {
						declarations.setInt64(name, stoll(value));
					}
				} else {
					// Keep as string - but handle !important flag
					if (important) {
						value += " !important";
					}
					declarations.setString(name, value);
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
