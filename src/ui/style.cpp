#include "ui/style.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <unordered_map>

namespace gold {
	namespace UI {

		// ------------------------------------------------------------ utils

		using std::unordered_map;

		namespace {

			bool isSpaceChar(char c) {
				return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
					   c == '\f' || c == '\v';
			}

			string trimStr(const string& s) {
				const size_t start = s.find_first_not_of(" \t\n\r\f\v");
				if (start == string::npos) return "";
				const size_t end = s.find_last_not_of(" \t\n\r\f\v");
				return s.substr(start, end - start + 1);
			}

			string lowerStr(string s) {
				for (auto& c : s) c = (char)tolower((unsigned char)c);
				return s;
			}

			bool eq(const string& a, const char* b) {
				return a.size() == strlen(b) && a == b;
			}

			/** Any gold value as the text a CSS parser should see. */
			string valueText(const var& value) {
				if (value.isBool()) return value.getBool() ? "true" : "false";
				return trimStr(value.getString());
			}

			bool startsWith(const string& s, const char* prefix) {
				const size_t n = strlen(prefix);
				return s.size() >= n && s.compare(0, n, prefix) == 0;
			}

			bool identChar(char c) {
				return isalnum((unsigned char)c) || c == '-' || c == '_' ||
					   (unsigned char)c >= 0x80;
			}

			string readIdent(const string& src, size_t& i) {
				const size_t start = i;
				while (i < src.size() && identChar(src[i])) i++;
				return src.substr(start, i - start);
			}

			/** Read a balanced `( ... )` group starting at `i` (which must be
			 *  on the opening paren). */
			string readParens(const string& src, size_t& i) {
				if (i >= src.size() || src[i] != '(') return "";
				const size_t start = ++i;
				int depth = 1;
				while (i < src.size() && depth > 0) {
					if (src[i] == '(') depth++;
					else if (src[i] == ')') depth--;
					if (depth == 0) break;
					i++;
				}
				const string out = src.substr(start, i - start);
				if (i < src.size() && src[i] == ')') i++;
				return out;
			}

			/** `an+b` for :nth-* selectors. */
			void parseNth(const string& text, int& a, int& b) {
				a = 0;
				b = 0;
				string s = lowerStr(trimStr(text));
				if (s == "odd") {
					a = 2;
					b = 1;
					return;
				}
				if (s == "even") {
					a = 2;
					b = 0;
					return;
				}
				const size_t nPos = s.find('n');
				if (nPos == string::npos) {
					b = atoi(s.c_str());
					return;
				}
				string aPart = trimStr(s.substr(0, nPos));
				string bPart = trimStr(s.substr(nPos + 1));
				if (aPart.empty() || aPart == "+") a = 1;
				else if (aPart == "-") a = -1;
				else a = atoi(aPart.c_str());
				if (!bPart.empty()) {
					if (bPart[0] == '+') b = atoi(bPart.c_str() + 1);
					else if (bPart[0] == '-') b = -atoi(bPart.c_str() + 1);
					else b = atoi(bPart.c_str());
				}
			}

			bool matchesNth(int index, int a, int b) {
				if (index < 1) return false;
				if (a == 0) return index == b;
				if (a > 0) {
					if (index < b) return false;
					return ((index - b) % a) == 0;
				}
				if (index > b) return false;
				return ((b - index) % (-a)) == 0;
			}

			struct namedColor {
				const char* name;
				uint8_t r, g, b;
			};
			// The CSS basic + extended greys; enough for UI stylesheets.
			const namedColor kNamedColors[] = {
				{"black", 0, 0, 0},
				{"silver", 192, 192, 192},
				{"gray", 128, 128, 128},
				{"grey", 128, 128, 128},
				{"darkgray", 169, 169, 169},
				{"darkgrey", 169, 169, 169},
				{"lightgray", 211, 211, 211},
				{"lightgrey", 211, 211, 211},
				{"whitesmoke", 245, 245, 245},
				{"white", 255, 255, 255},
				{"red", 255, 0, 0},
				{"maroon", 128, 0, 0},
				{"orange", 255, 165, 0},
				{"yellow", 255, 255, 0},
				{"olive", 128, 128, 0},
				{"lime", 0, 255, 0},
				{"green", 0, 128, 0},
				{"teal", 0, 128, 128},
				{"aqua", 0, 255, 255},
				{"cyan", 0, 255, 255},
				{"blue", 0, 0, 255},
				{"navy", 0, 0, 128},
				{"fuchsia", 255, 0, 255},
				{"magenta", 255, 0, 255},
				{"purple", 128, 0, 128},
				{"pink", 255, 192, 203},
				{"brown", 165, 42, 42},
				{"gold", 255, 215, 0},
				{"beige", 245, 245, 220},
				{"coral", 255, 127, 80},
				{"crimson", 220, 20, 60},
				{"darkblue", 0, 0, 139},
				{"darkgreen", 0, 100, 0},
				{"darkred", 139, 0, 0},
				{"indigo", 75, 0, 130},
				{"ivory", 255, 255, 240},
				{"khaki", 240, 230, 140},
				{"lavender", 230, 230, 250},
				{"salmon", 250, 128, 114},
				{"tan", 210, 180, 140},
				{"violet", 238, 130, 238},
				{"wheat", 245, 222, 179},
			};

			const char* kPropertyNames[] = {
#define UI_NAME_ENTRY(name, text) text,
				UI_PROPERTIES(UI_NAME_ENTRY)
#undef UI_NAME_ENTRY
			};

			// Properties that inherit from the parent by default.
			bool kInherited[static_cast<size_t>(prop::count)] = {};

			struct inheritedInit {
				inheritedInit() {
					const prop list[] = {
						prop::color,		 prop::fontFamily,
						prop::fontSize,	 prop::fontWeight,
						prop::fontStyle,	 prop::lineHeight,
						prop::letterSpacing,	 prop::textAlign,
						prop::textDecoration, prop::textIndent,
						prop::whiteSpace,	 prop::visibility,
						prop::listStyle,	 prop::cursor,
					};
					for (prop p : list) kInherited[(size_t)p] = true;
				}
			} kInheritedInitInstance;

		}  // namespace

		// ---------------------------------------------------- property ids

		const char* propertyName(prop p) {
			const size_t i = (size_t)p;
			if (i >= static_cast<size_t>(prop::count)) return "";
			return kPropertyNames[i];
		}

		bool lookupProperty(const string& name, prop& out) {
			static const unordered_map<string, prop>* table = []() {
				auto* map = new unordered_map<string, prop>();
				for (size_t i = 0; i < static_cast<size_t>(prop::count); i++)
					(*map)[kPropertyNames[i]] = (prop)i;
				return map;
			}();
			auto found = table->find(lowerStr(trimStr(name)));
			if (found == table->end()) return false;
			out = found->second;
			return true;
		}

		bool isInheritedProperty(prop p) {
			return kInherited[(size_t)p];
		}

		// ---------------------------------------------------- value parsing

		vector<string> splitValues(const string& text, char sep) {
			vector<string> out;
			string current;
			int depth = 0;
			char quote = 0;
			for (size_t i = 0; i < text.size(); i++) {
				const char c = text[i];
				if (quote) {
					current += c;
					if (c == quote) quote = 0;
					continue;
				}
				if (c == '"' || c == '\'') {
					quote = c;
					current += c;
					continue;
				}
				if (c == '(') depth++;
				else if (c == ')') depth = depth > 0 ? depth - 1 : 0;

				const bool isSep = (sep == ' ') ? isSpaceChar(c) : (c == sep);
				if (isSep && depth == 0) {
					const string piece = trimStr(current);
					if (!piece.empty()) out.push_back(piece);
					current.clear();
					continue;
				}
				current += c;
			}
			const string piece = trimStr(current);
			if (!piece.empty()) out.push_back(piece);
			return out;
		}

		bool stripImportant(string& text) {
			string t = lowerStr(text);
			const size_t at = t.rfind("!important");
			if (at == string::npos) return false;
			text = trimStr(text.substr(0, at));
			return true;
		}

		bool parseLength(const string& text, length& out) {
			const string s = lowerStr(trimStr(text));
			if (s.empty()) return false;
			if (eq(s, "auto")) {
				out = length::makeAuto();
				return true;
			}
			if (eq(s, "none") || eq(s, "initial")) {
				out = length::undef();
				return true;
			}

			size_t i = 0;
			if (s[i] == '+' || s[i] == '-') i++;
			bool dot = false;
			while (i < s.size() &&
				   (isdigit((unsigned char)s[i]) ||
					(s[i] == '.' && !dot))) {
				if (s[i] == '.') dot = true;
				i++;
			}
			if (i == 0) return false;  // keyword such as `thin`
			const float v = strtof(s.substr(0, i).c_str(), nullptr);
			const string suffix = trimStr(s.substr(i));

			if (suffix.empty()) {
				out = length::px(v);
				return true;
			}
			if (suffix == "px") {
				out = length::px(v);
				return true;
			}
			if (suffix == "pt") {
				out = length::px(v * (4.0f / 3.0f));
				return true;
			}
			if (suffix == "em") {
				out = {v, unit::em};
				return true;
			}
			if (suffix == "rem") {
				out = {v, unit::rem};
				return true;
			}
			if (suffix == "%") {
				out = length::pct(v);
				return true;
			}
			if (suffix == "vw") {
				out = {v, unit::viewportW};
				return true;
			}
			if (suffix == "vh") {
				out = {v, unit::viewportH};
				return true;
			}
			return false;
		}

		bool parseColor(const string& text, color& out) {
			string s = lowerStr(trimStr(text));
			if (s.empty()) return false;
			if (eq(s, "transparent")) {
				out = {0, 0, 0, 0, true};
				return true;
			}
			if (eq(s, "currentcolor")) return false;  // handled by the caller

			if (s[0] == '#') {
				s = s.substr(1);
				if (s.size() != 3 && s.size() != 4 && s.size() != 6 &&
					s.size() != 8)
					return false;
				uint32_t rgba = 0;
				for (char c : s) {
					int digit;
					if (isdigit((unsigned char)c)) digit = c - '0';
					else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
					else return false;
					rgba = rgba * 16 + (uint32_t)digit;
				}
				if (s.size() == 3 || s.size() == 4) {
					const uint32_t r = (rgba >> (s.size() == 3 ? 8 : 12)) & 0xF;
					const uint32_t g = (rgba >> (s.size() == 3 ? 4 : 8)) & 0xF;
					const uint32_t b = (rgba >> (s.size() == 3 ? 0 : 4)) & 0xF;
					const uint32_t a = (s.size() == 4) ? (rgba & 0xF) : 0xF;
					out = {r * 17.0f / 255.0f, g * 17.0f / 255.0f,
						b * 17.0f / 255.0f, a * 17.0f / 255.0f, true};
				} else if (s.size() == 6) {
					// RRGGBB
					out = {((rgba >> 16) & 0xFF) / 255.0f,
						((rgba >> 8) & 0xFF) / 255.0f, (rgba & 0xFF) / 255.0f,
						1.0f, true};
				} else {
					// RRGGBBAA
					out = {((rgba >> 24) & 0xFF) / 255.0f,
						((rgba >> 16) & 0xFF) / 255.0f,
						((rgba >> 8) & 0xFF) / 255.0f, (rgba & 0xFF) / 255.0f,
						true};
				}
				return true;
			}

			// Functional notation only when there really is a `(`: named
			// colors like `red` and `honeydew` also start with r/h.
			if ((s[0] == 'r' || s[0] == 'h') && s.find('(') != string::npos) {
				const bool hsl = s[0] == 'h';
				size_t open = s.find('(');
				if (open == string::npos) return false;
				const size_t close = s.rfind(')');
				if (close == string::npos || close < open) return false;
				vector<string> parts =
					splitValues(s.substr(open + 1, close - open - 1), ',');
				if (parts.size() < 3) return false;

				auto channel = [](const string& v, float scale) -> float {
					string t = trimStr(v);
					if (!t.empty() && t.back() == '%')
						return strtof(t.c_str(), nullptr) / 100.0f;
					return strtof(t.c_str(), nullptr) / scale;
				};

				float alpha = 1.0f;
				if (parts.size() >= 4) {
					string t = trimStr(parts[3]);
					if (!t.empty() && t.back() == '%')
						alpha = strtof(t.c_str(), nullptr) / 100.0f;
					else
						alpha = strtof(t.c_str(), nullptr);
				}
				if (hsl) {
					const float h =
						(fmodf(strtof(parts[0].c_str(), nullptr), 360.0f) + 360.0f) /
						360.0f;
					const float s2 = strtof(parts[1].c_str(), nullptr) / 100.0f;
					const float l = strtof(parts[2].c_str(), nullptr) / 100.0f;
					const float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s2;
					const float hp = h * 6.0f;
					const float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
					float r1 = 0, g1 = 0, b1 = 0;
					const int sector = (int)hp;
					switch (sector) {
						case 0: r1 = c; g1 = x; break;
						case 1: r1 = x; g1 = c; break;
						case 2: g1 = c; b1 = x; break;
						case 3: g1 = x; b1 = c; break;
						case 4: r1 = x; b1 = c; break;
						default: r1 = c; b1 = x; break;
					}
					const float m = l - c / 2.0f;
					out = {r1 + m, g1 + m, b1 + m, alpha, true};
				} else {
					out = {channel(parts[0], 255.0f), channel(parts[1], 255.0f),
						channel(parts[2], 255.0f), alpha, true};
				}
				return true;
			}

			for (const auto& named : kNamedColors) {
				if (s == named.name) {
					out = {named.r / 255.0f, named.g / 255.0f,
						named.b / 255.0f, 1.0f, true};
					return true;
				}
			}
			return false;
		}

		bool parseBackgroundImage(const string& text, backgroundPaint& out) {
			const string s = lowerStr(trimStr(text));
			out = backgroundPaint();
			if (s.empty() || eq(s, "none")) return true;

			size_t open = s.find('(');
			if (open == string::npos) return false;
			const size_t close = s.rfind(')');
			if (close == string::npos) return false;

			const string fn = trimStr(s.substr(0, open));
			const string args = s.substr(open + 1, close - open - 1);
			vector<string> parts = splitValues(args, ',');

			// Split "45deg, red, blue" into a direction and its stops.
			vector<string> stops;
			string direction;
			if (fn == "linear-gradient" || fn == "repeating-linear-gradient") {
				out.repeat = startsWith(fn, "repeating");
				out.kind = paintKind::linearGradient;
				if (!parts.empty() &&
					(parts[0].find("deg") != string::npos ||
					 startsWith(parts[0], "to "))) {
					direction = parts[0];
					parts.erase(parts.begin());
				}
				out.angle = 180.0f;
				if (direction.find("deg") != string::npos)
					out.angle = strtof(direction.c_str(), nullptr);
				else if (!direction.empty()) {
					// `to right` = 90deg, `to bottom` = 180deg, ...
					if (direction.find("left") != string::npos) out.angle = 270.0f;
					else if (direction.find("right") != string::npos)
						out.angle = 90.0f;
					else if (direction.find("top") != string::npos) out.angle = 0.0f;
					else if (direction.find("bottom") != string::npos)
						out.angle = 180.0f;
				}
			} else if (fn == "radial-gradient" ||
					   fn == "repeating-radial-gradient") {
				out.repeat = startsWith(fn, "repeating");
				out.kind = paintKind::radialGradient;
				// A leading `<size>`/`at ...` prelude is skipped.
				while (!parts.empty() &&
					   (parts[0].find("at ") == 0 ||
						parts[0].find("circle") != string::npos ||
						parts[0].find("ellipse") != string::npos ||
						parts[0].find("closest") != string::npos ||
						parts[0].find("farthest") != string::npos ||
						parts[0].find("px") != string::npos ||
						parts[0].find("%") != string::npos ||
						parts[0].find("em") != string::npos))
					parts.erase(parts.begin());
			} else {
				return false;  // url(...) and friends are not supported yet
			}

			for (size_t i = 0; i < parts.size(); i++) {
				vector<string> piece = splitValues(parts[i], ' ');
				gradientStop stop;
				stop.offset = (parts.size() == 1)
								   ? (i == 0 ? 0.0f : 1.0f)
								   : (float)i / (float)(parts.size() - 1);
				bool haveColor = false;
				for (auto& token : piece) {
					color c;
					if (!haveColor && parseColor(token, c)) {
						stop.c = c;
						haveColor = true;
						continue;
					}
					length pos;
					if (parseLength(token, pos) && pos.isPercent())
						stop.offset = pos.value / 100.0f;
				}
				if (!haveColor) return false;
				out.stops.push_back(stop);
			}
			return out.stops.size() >= 2;
		}

		bool parseTracks(const string& text, vector<trackSpec>& out) {
			out.clear();
			for (const string& token : splitValues(text, ' ')) {
				if (startsWith(token, "repeat(")) {
					size_t i = 6;  // the '(' of "repeat("
					const string args = readParens(token, i);
					const size_t comma = splitValues(args, ',').size() > 1
											 ? args.find(',')
											 : string::npos;
					if (comma == string::npos) return false;
					int count = atoi(trimStr(args.substr(0, comma)).c_str());
					vector<trackSpec> inner;
					if (!parseTracks(args.substr(comma + 1), inner)) return false;
					for (int r = 0; r < count; r++)
						for (const auto& t : inner) out.push_back(t);
					continue;
				}
				if (startsWith(token, "minmax(")) {
					size_t i = 6;  // the '(' of "minmax("
					const string args = readParens(token, i);
					vector<string> parts = splitValues(args, ',');
					if (parts.size() != 2) return false;
					trackSpec spec;
					spec.k = trackSpec::kind::minmax;
					parseLength(parts[0], spec.minSize);
					length maxLen;
					if (parseLength(parts[1], maxLen)) {
						if (maxLen.u == unit::number)
							spec.flex = maxLen.value;
						else
							spec.maxSize = maxLen;
					}
					out.push_back(spec);
					continue;
				}
				trackSpec spec;
				if (token == "auto" || token == "min-content" ||
					token == "max-content") {
					spec.k = trackSpec::kind::autoTrack;
					out.push_back(spec);
					continue;
				}
				// `fr` is grid/flex specific, so it is handled here rather
				// than in the generic length parser.
				if (token.size() > 2 && token.compare(token.size() - 2, 2, "fr") == 0) {
					spec.k = trackSpec::kind::flexible;
					spec.flex = strtof(token.c_str(), nullptr);
					if (spec.flex < 0.0f) return false;
					out.push_back(spec);
					continue;
				}
				length value;
				if (!parseLength(token, value)) return false;
				if (value.u == unit::number) {
					spec.k = trackSpec::kind::flexible;
					spec.flex = value.value;
				} else if (value.isAuto()) {
					spec.k = trackSpec::kind::autoTrack;
				} else {
					spec.k = trackSpec::kind::fixed;
					spec.fixed = value.value;
				}
				out.push_back(spec);
			}
			return !out.empty();
		}

		float resolveLength(length l, float basis, float fontSize,
			float rootFontSize, const styleContext& ctx) {
			switch (l.u) {
				case unit::px: return l.value;
				case unit::em: return l.value * fontSize;
				case unit::rem: return l.value * rootFontSize;
				case unit::percent: return l.value * 0.01f * basis;
				case unit::viewportW: return l.value * 0.01f * ctx.viewportWidth;
				case unit::viewportH: return l.value * 0.01f * ctx.viewportHeight;
				case unit::number: return l.value;
				case unit::autoLen:
				case unit::none:
				default: return 0.0f;
			}
		}

		float lineHeightPx(const computedStyle& style, float fontSize) {
			if (style.lineHeight.u == unit::number)
				return style.lineHeight.value * fontSize;
			if (style.lineHeight.defined()) return style.lineHeight.value;
			return fontSize * 1.2f;
		}

		// ------------------------------------------------------- selectors

		namespace {

			bool matchPseudo(pseudoClass pc, int nthA, int nthB,
				const domTree& tree, int node) {
				const domNode& n = tree.nodes[(size_t)node];
				switch (pc) {
					case pseudoClass::root: return n.parent < 0;
					case pseudoClass::empty: return n.isEmpty();
					case pseudoClass::firstChild: return n.prev < 0;
					case pseudoClass::lastChild: return n.next < 0;
					case pseudoClass::onlyChild:
						return n.prev < 0 && n.next < 0;
					case pseudoClass::firstOfType: return n.typeIndex == 0;
					case pseudoClass::lastOfType: {
						// No following element sibling with the same tag.
						for (int s = n.next; s >= 0; s = tree.nodes[(size_t)s].next)
							if (tree.nodes[(size_t)s].tag == n.tag) return false;
						return true;
					}
					case pseudoClass::onlyOfType: {
						if (n.typeIndex != 0) return false;
						for (int s = n.next; s >= 0; s = tree.nodes[(size_t)s].next)
							if (tree.nodes[(size_t)s].tag == n.tag) return false;
						return true;
					}
					case pseudoClass::nthChild:
						return matchesNth(n.index + 1, nthA, nthB);
					case pseudoClass::nthLastChild: {
						// Count the elements that follow this one.
						int following = 0;
						for (int s = n.next; s >= 0; s = tree.nodes[(size_t)s].next)
							following++;
						return matchesNth(following + 1, nthA, nthB);
					}
					case pseudoClass::nthOfType:
						// `typeIndex` is 0-based among same-tag element
						// siblings, which is exactly the nth-of-type index.
						return matchesNth(n.typeIndex + 1, nthA, nthB);
					case pseudoClass::hover: return n.state.hover;
					case pseudoClass::active: return n.state.active;
					case pseudoClass::focus: return n.state.focus;
					case pseudoClass::checked:
						return n.state.checked || n.hasAttr("checked") ||
							n.hasAttr("selected");
					case pseudoClass::disabled:
						return n.state.disabled || n.hasAttr("disabled");
					case pseudoClass::enabled:
						return !n.state.disabled && !n.hasAttr("disabled");
					case pseudoClass::link:
						return (n.tag == "a" || n.tag == "area" ||
								   n.tag == "link") &&
							n.hasAttr("href");
					default: return false;
				}
			}

			bool matchAttr(const domNode& n, const string& name, attrOp op,
				const string& value) {
				if (op == attrOp::exists) return n.hasAttr(name);
				const string actual = n.attrString(name);
				switch (op) {
					case attrOp::equals: return actual == value;
					case attrOp::includes:
						for (const string& piece : splitValues(actual, ' '))
							if (piece == value) return true;
						return false;
					case attrOp::dashMatch:
						return actual == value ||
							(actual.size() > value.size() &&
								actual.compare(0, value.size(), value) == 0 &&
								actual[value.size()] == '-');
					case attrOp::prefix:
						return actual.size() >= value.size() &&
							actual.compare(0, value.size(), value) == 0;
					case attrOp::suffix:
						return actual.size() >= value.size() &&
							actual.compare(actual.size() - value.size(),
								value.size(), value) == 0;
					case attrOp::substring:
						return actual.find(value) != string::npos;
					default: return false;
				}
			}

			bool parseSimple(const string& src, size_t& i, simpleSelector& out);

			/** Parse the argument list of one simple selector. */
			bool parseCompoundText(const string& src, compoundSelector& out) {
				size_t i = 0;
				while (i < src.size()) {
					if (isSpaceChar(src[i])) {
						i++;
						continue;
					}
					simpleSelector simple;
					if (!parseSimple(src, i, simple)) return false;
					out.simples.push_back(simple);
				}
				return !out.simples.empty();
			}

			bool parseSimple(const string& src, size_t& i, simpleSelector& out) {
				const char c = src[i];
				if (c == '*') {
					i++;
					return true;
				}
				if (c == '#') {
					i++;
					out.id = readIdent(src, i);
					return !out.id.empty();
				}
				if (c == '.') {
					i++;
					const string cls = readIdent(src, i);
					if (cls.empty()) return false;
					out.classes.push_back(cls);
					return true;
				}
				if (c == '[') {
					const size_t close = src.find(']', i);
					if (close == string::npos) return false;
					const string body = src.substr(i + 1, close - i - 1);
					i = close + 1;
					const size_t op = body.find_first_of("~^$*|=");
					string name = trimStr(body);
					attrOp attr = attrOp::exists;
					string value;
					if (op != string::npos) {
						name = trimStr(body.substr(0, op));
						// Operator tokens: `=`, `~=`, `^=`, `$=`, `*=`, `|=`.
						// Compound ones carry their `=` right after the
						// operator character; every form ends on the value.
						size_t valueStart = op + 1;
						switch (body[op]) {
							case '~': attr = attrOp::includes; break;
							case '^': attr = attrOp::prefix; break;
							case '$': attr = attrOp::suffix; break;
							case '*': attr = attrOp::substring; break;
							case '|': attr = attrOp::dashMatch; break;
							case '=':
								attr = attrOp::equals;
								valueStart = op;
								break;
							default: return false;
						}
						if (valueStart < body.size() &&
							body[valueStart] == '=')
							valueStart++;
						string raw = trimStr(body.substr(valueStart));
						if (raw.size() >= 2 &&
							((raw.front() == '"' && raw.back() == '"') ||
							 (raw.front() == '\'' && raw.back() == '\'')))
							raw = raw.substr(1, raw.size() - 2);
						value = raw;
					}
					if (name.empty()) return false;
					out.attrs.push_back({lowerStr(name), attr});
					out.attrValues.push_back(value);
					return true;
				}
				if (c == ':') {
					i++;
					if (i < src.size() && src[i] == ':') {
						// Pseudo-element: recorded, not rendered in v1.
						i++;
						out.pseudoElement = lowerStr(readIdent(src, i));
						return !out.pseudoElement.empty();
					}
					const string name = lowerStr(readIdent(src, i));
					if (name.empty()) return false;
					string args;
					if (i < src.size() && src[i] == '(') args = readParens(src, i);

					struct alias {
						const char* from;
						pseudoClass to;
					};
					static const alias kAliases[] = {
						{"hover", pseudoClass::hover},
						{"active", pseudoClass::active},
						{"focus", pseudoClass::focus},
						{"focus-visible", pseudoClass::focus},
						{"focus-within", pseudoClass::focus},
						{"checked", pseudoClass::checked},
						{"disabled", pseudoClass::disabled},
						{"enabled", pseudoClass::enabled},
						{"link", pseudoClass::link},
						{"any-link", pseudoClass::link},
						{"visited", pseudoClass::link},
						{"root", pseudoClass::root},
						{"empty", pseudoClass::empty},
						{"first-child", pseudoClass::firstChild},
						{"last-child", pseudoClass::lastChild},
						{"only-child", pseudoClass::onlyChild},
						{"first-of-type", pseudoClass::firstOfType},
						{"last-of-type", pseudoClass::lastOfType},
						{"only-of-type", pseudoClass::onlyOfType},
						{"nth-child", pseudoClass::nthChild},
						{"nth-last-child", pseudoClass::nthLastChild},
						{"nth-of-type", pseudoClass::nthOfType},
					};
					for (const auto& a : kAliases) {
						if (name == a.from) {
							out.pseudos.push_back(a.to);
							if (!args.empty()) {
								int na = 0, nb = 0;
								parseNth(args, na, nb);
								out.pseudoArgs.push_back(na);
								out.pseudoArgs.push_back(nb);
							} else {
								out.pseudoArgs.push_back(0);
								out.pseudoArgs.push_back(0);
							}
							return true;
						}
					}
					if (name == "not") {
						compoundSelector inner;
						if (!parseCompoundText(args, inner)) return false;
						for (const auto& item : inner.simples)
							out.notSelectors.push_back(item);
						return true;
					}
					return false;  // unknown pseudo invalidates the selector
				}
				if (identChar(c)) {
					out.tag = lowerStr(readIdent(src, i));
					return !out.tag.empty();
				}
				return false;
			}

			bool parseComplex(const string& src, complexSelector& out) {
				compoundSelector current;
				combinator pending = combinator::descendant;
				bool havePending = false;
				size_t i = 0;

				auto flush = [&]() -> bool {
					if (current.simples.empty()) return false;
					if (!out.parts.empty()) out.combinators.push_back(pending);
					out.parts.push_back(current);
					current = compoundSelector();
					havePending = false;
					return true;
				};

				while (i < src.size()) {
					const char c = src[i];
					if (isSpaceChar(c)) {
						size_t j = i;
						while (j < src.size() && isSpaceChar(src[j])) j++;
						// Whitespace between two compounds is a descendant
						// combinator, so the current compound ends here.
						// Whitespace *around* an explicit combinator (`a > b`)
						// is only formatting, so it is skipped instead.
						if (!current.simples.empty() && j < src.size() &&
							src[j] != '>' && src[j] != '+' && src[j] != '~') {
							if (!flush()) return false;
							pending = combinator::descendant;
							havePending = true;
						}
						i = j;
						continue;
					}
					if (c == '>' || c == '+' || c == '~') {
						if (!flush()) return false;
						pending = c == '>'	 ? combinator::child
								 : c == '+' ? combinator::adjacentSibling
											: combinator::generalSibling;
						havePending = true;
						i++;
						while (i < src.size() && isSpaceChar(src[i])) i++;
						continue;
					}
					simpleSelector simple;
					if (!parseSimple(src, i, simple)) return false;
					current.simples.push_back(simple);
				}
				if (!flush()) return false;
				(void)havePending;
				return out.parts.size() >= 1;
			}

		}  // namespace

		vector<complexSelector> parseSelectorList(const string& text) {
			vector<complexSelector> out;
			for (const string& part : splitValues(text, ',')) {
				complexSelector sel;
				if (parseComplex(trimStr(part), sel)) out.push_back(sel);
			}
			return out;
		}

		specificity selectorSpecificity(const complexSelector& sel) {
			specificity spec;
			for (const auto& compound : sel.parts) {
				for (const auto& simple : compound.simples) {
					if (!simple.tag.empty()) spec.tags++;
					if (!simple.id.empty()) spec.ids++;
					spec.classes += (int)simple.classes.size();
					spec.classes += (int)simple.attrs.size();
					spec.classes += (int)simple.pseudos.size();
					if (!simple.notSelectors.empty()) spec.ids++;
				}
			}
			return spec;
		}

		bool simpleMatches(const simpleSelector& simple, const domTree& tree,
			int node) {
			if (node < 0) return false;
			const domNode& n = tree.nodes[(size_t)node];
			if (n.isText) return false;
			if (!simple.tag.empty() && n.tag != simple.tag) return false;
			if (!simple.id.empty() && n.attrString("id") != simple.id) return false;
			for (const string& cls : simple.classes)
				if (!n.hasClass(cls)) return false;
			for (size_t i = 0; i < simple.attrs.size(); i++) {
				if (!matchAttr(n, simple.attrs[i].first, simple.attrs[i].second,
						simple.attrValues[i]))
					return false;
			}
			for (size_t i = 0; i < simple.pseudos.size(); i++) {
				const size_t arg = i * 2;
				if (!matchPseudo(simple.pseudos[i],
						arg < simple.pseudoArgs.size() ? simple.pseudoArgs[arg] : 0,
						arg + 1 < simple.pseudoArgs.size()
							? simple.pseudoArgs[arg + 1]
							: 0,
						tree, node))
					return false;
			}
			for (const simpleSelector& notSimple : simple.notSelectors)
				if (simpleMatches(notSimple, tree, node)) return false;
			// ::before / ::after are parsed but not generated in v1.
			if (!simple.pseudoElement.empty()) return false;
			return true;
		}

		bool compoundMatches(const compoundSelector& compound,
			const domTree& tree, int node) {
			for (const auto& simple : compound.simples)
				if (!simpleMatches(simple, tree, node)) return false;
			return !compound.simples.empty();
		}

		namespace {
			bool matchFromRight(const complexSelector& sel, size_t part,
				const domTree& tree, int node) {
				if (!compoundMatches(sel.parts[part], tree, node)) return false;
				if (part == 0) return true;
				const combinator comb = sel.combinators[part - 1];
				const auto& nodes = tree.nodes;
				switch (comb) {
					case combinator::child: {
						const int p = nodes[(size_t)node].parent;
						return p >= 0 && matchFromRight(sel, part - 1, tree, p);
					}
					case combinator::descendant: {
						for (int a = nodes[(size_t)node].parent; a >= 0;
							 a = nodes[(size_t)a].parent)
							if (matchFromRight(sel, part - 1, tree, a)) return true;
						return false;
					}
					case combinator::adjacentSibling: {
						const int s = nodes[(size_t)node].prev;
						return s >= 0 && matchFromRight(sel, part - 1, tree, s);
					}
					case combinator::generalSibling: {
						for (int s = nodes[(size_t)node].prev; s >= 0;
							 s = nodes[(size_t)s].prev)
							if (matchFromRight(sel, part - 1, tree, s)) return true;
						return false;
					}
				}
				return false;
			}
		}  // namespace

		bool selectorMatches(const complexSelector& sel, const domTree& tree,
			int node) {
			if (sel.parts.empty() || node < 0) return false;
			return matchFromRight(sel, sel.parts.size() - 1, tree, node);
		}

		// ------------------------------------------------------ stylesheet

		namespace {
			/**
			 * Walk `text` and hand every depth-0 construct to `fn` as
			 * (prelude, body). Handles nesting, so it works for both plain
			 * rules and `@media { ... }` groups.
			 */
			void scanBlocks(const string& text, size_t& i,
				const function<void(const string&, const string&)>& fn) {
				while (i < text.size()) {
					while (i < text.size() && isSpaceChar(text[i])) i++;
					if (i >= text.size()) break;
					if (text.compare(i, 2, "/*") == 0) {
						const size_t end = text.find("*/", i + 2);
						i = (end == string::npos) ? text.size() : end + 2;
						continue;
					}

					const size_t preludeStart = i;
					int depth = 0;
					size_t bodyStart = string::npos;
					size_t bodyEnd = string::npos;
					while (i < text.size()) {
						const char c = text[i];
						if (c == '/' && text.compare(i, 2, "/*") == 0) {
							const size_t end = text.find("*/", i + 2);
							i = (end == string::npos) ? text.size() : end + 2;
							continue;
						}
						if (c == '"' || c == '\'') {
							const char quote = c;
							i++;
							while (i < text.size() && text[i] != quote) i++;
							if (i < text.size()) i++;
							continue;
						}
						if (c == '{') {
							if (depth == 0) bodyStart = i + 1;
							depth++;
						} else if (c == '}') {
							depth--;
							if (depth == 0) {
								bodyEnd = i;
								i++;
								break;
							}
							if (depth < 0) {
								// Stray close brace at depth 0.
								depth = 0;
								i++;
								break;
							}
						} else if (c == ';' && depth == 0) {
							i++;
							break;
						}
						i++;
					}
					// `bodyStart` points just past the '{', which is not part
					// of the prelude.
					const size_t preludeEnd = (bodyStart == string::npos)
												  ? i
												  : bodyStart - 1;
					const string prelude =
						trimStr(text.substr(preludeStart,
							preludeEnd > preludeStart ? preludeEnd - preludeStart
														: 0));
					// A body exists as soon as a '{' was seen; `depth` is back
					// to 0 by the time a complete block ends.
					const string body = (bodyStart == string::npos)
											? string()
											: text.substr(bodyStart,
												  bodyEnd - bodyStart);
					if (!prelude.empty()) fn(prelude, body);
					if (bodyStart == string::npos) {
						// Statement at-rule (e.g. `@import url(...);`).
						continue;
					}
				}
			}
		}  // namespace

		const keyframesBlock* stylesheet::findKeyframes(
			const string& name) const {
			for (const auto& block : keyframes)
				if (block.name == name) return &block;
			return nullptr;
		}

		bool mediaMatches(const string& query, const styleContext& ctx) {
			if (query.empty()) return true;
			for (const string& part : splitValues(query, ',')) {
				bool all = true;
				for (const string& raw : splitValues(part, ' ')) {
					// Each term is wrapped in parentheses: `(min-width: 700px)`.
					string term = lowerStr(raw);
					if (term.size() >= 2 && term.front() == '(' &&
						term.back() == ')')
						term = term.substr(1, term.size() - 2);
					const string t = term;
					if (t.empty()) continue;
					if (t == "and" || t == "only" || t == "not") continue;
					if (t == "screen" || t == "all" || t == "ui") continue;
					if (t == "hover") {
						if (!ctx.hoverCapable) all = false;
						continue;
					}
					if (t == "none" || t == "no-hover" || t == "pointer:coarse" ||
						t == "no-preference") {
						all = false;
						continue;
					}
					const size_t colon = t.find(':');
					if (colon == string::npos) {
						all = false;
						continue;
					}
					const string name = t.substr(0, colon);
					length value;
					if (!parseLength(t.substr(colon + 1), value)) {
						all = false;
						continue;
					}
					const bool width = name == "min-width" || name == "max-width";
					const float actual = width ? ctx.viewportWidth
											   : ctx.viewportHeight;
					const float px = resolveLength(value, actual, 16.0f, 16.0f, ctx);
					if (name == "min-width" || name == "min-height") {
						if (actual < px) all = false;
					} else if (name == "max-width" || name == "max-height") {
						if (actual > px) all = false;
					} else {
						all = false;
					}
				}
				if (all) return true;
			}
			return false;
		}

		namespace {
			float keyframeOffset(const string& prelude) {
				const string t = lowerStr(trimStr(prelude));
				if (t == "from") return 0.0f;
				if (t == "to") return 1.0f;
				if (!t.empty() && t.back() == '%')
					return strtof(t.c_str(), nullptr) / 100.0f;
				return strtof(t.c_str(), nullptr);
			}

			void collectRules(stylesheet& sheet, const string& css,
				const string& media, int& order) {
				size_t i = 0;
				scanBlocks(css, i, [&](const string& prelude,
									   const string& body) {
					if (!prelude.empty() && prelude[0] == '@') {
						const size_t space = prelude.find_first_of(" \t");
						const string name =
							lowerStr(space == string::npos ? prelude
														  : prelude.substr(0, space));
						if (name == "@media" || name == "@supports") {
							const string query =
								space == string::npos ? "" : prelude.substr(space + 1);
							// Nested media queries combine with '&'.
							const string combined = media.empty()
														? query
														: media + " and " + query;
							collectRules(sheet, body, combined, order);
							return;
						}
						if (name == "@keyframes" ||
							name == "@-webkit-keyframes") {
							if (space == string::npos) return;
							keyframesBlock block;
							block.name = trimStr(prelude.substr(space + 1));
							size_t k = 0;
							scanBlocks(body, k, [&](const string& step,
													   const string& decls) {
								keyframeStep entry;
								entry.offset = keyframeOffset(step);
								entry.declarations = CSS::parseDeclarations(decls);
								block.steps.push_back(entry);
							});
							std::stable_sort(block.steps.begin(),
								block.steps.end(), [](const keyframeStep& a,
													 const keyframeStep& b) {
									return a.offset < b.offset;
								});
							sheet.keyframes.push_back(block);
							return;
						}
						if (name == "@font-face") {
							sheet.fontFaces.push_back(body);
							return;
						}
						return;  // @import, @page, @charset: ignored
					}

					if (body.empty()) return;
					object declarations = CSS::parseDeclarations(body);
					if (!declarations) return;
					for (const complexSelector& sel : parseSelectorList(prelude)) {
						styleRule rule;
						rule.selector = sel;
						rule.declarations = declarations;
						rule.spec = selectorSpecificity(sel);
						rule.order = order++;
						rule.media = media;
						sheet.rules.push_back(rule);
					}
				});
			}
		}  // namespace

		stylesheet parseStylesheet(const string& css) {
			stylesheet sheet;
			int order = 0;
			collectRules(sheet, css, "", order);
			// Ascending cascade priority: later rules and higher specificity
			// are applied last, so they win.
			std::stable_sort(sheet.rules.begin(), sheet.rules.end(),
				[](const styleRule& a, const styleRule& b) {
					if (a.spec < b.spec) return true;
					if (b.spec < a.spec) return false;
					return a.order < b.order;
				});
			return sheet;
		}

		// ------------------------------------------------------- cascade

		void computedStyle::inheritFrom(const computedStyle& parent) {
			textColor = parent.textColor;
			fontFamily = parent.fontFamily;
			fontSize = parent.fontSize;
			fontWeight = parent.fontWeight;
			italic = parent.italic;
			lineHeight = parent.lineHeight;
			letterSpacing = parent.letterSpacing;
			textIndent = parent.textIndent;
			textAlign = parent.textAlign;
			underline = parent.underline;
			lineThrough = parent.lineThrough;
			whiteSpace = parent.whiteSpace;
			visibility = parent.visibility;
			listStyle = parent.listStyle;
			cursor = parent.cursor;
		}

		namespace {
			/** Expand a 1..4 value shorthand across the four box sides. */
			bool expandSides(const string& value, length* dest) {
				vector<string> parts = splitValues(value, ' ');
				if (parts.empty()) return false;
				length v[4];
				const size_t count = (size_t)std::min<size_t>(4, parts.size());
				for (size_t i = 0; i < count; i++)
					if (!parseLength(parts[i], v[i])) return false;
				switch (parts.size()) {
					case 1:
						dest[0] = dest[1] = dest[2] = dest[3] = v[0];
						break;
					case 2:
						dest[0] = dest[2] = v[0];
						dest[1] = dest[3] = v[1];
						break;
					case 3:
						dest[0] = v[0];
						dest[1] = dest[3] = v[1];
						dest[2] = v[2];
						break;
					default:
						for (int i = 0; i < 4; i++) dest[i] = v[i];
						break;
				}
				return true;
			}

			bool expandSides(const string& value, color* dest) {
				vector<string> parts = splitValues(value, ' ');
				if (parts.empty()) return false;
				color v[4];
				const size_t count = (size_t)std::min<size_t>(4, parts.size());
				for (size_t i = 0; i < count; i++)
					if (!parseColor(parts[i], v[i])) return false;
				switch (parts.size()) {
					case 1:
						for (int i = 0; i < 4; i++) dest[i] = v[0];
						break;
					case 2:
						dest[0] = dest[2] = v[0];
						dest[1] = dest[3] = v[1];
						break;
					case 3:
						dest[0] = v[0];
						dest[1] = dest[3] = v[1];
						dest[2] = v[2];
						break;
					default:
						for (int i = 0; i < 4; i++) dest[i] = v[i];
						break;
				}
				return true;
			}

			/** Border-radius corner order is TL TR BR BL. */
			bool expandCorners(const string& value, length* dest) {
				vector<string> parts = splitValues(value, ' ');
				if (parts.empty()) return false;
				length v[4];
				const size_t count = (size_t)std::min<size_t>(4, parts.size());
				for (size_t i = 0; i < count; i++)
					if (!parseLength(parts[i], v[i])) return false;
				switch (parts.size()) {
					case 1:
						for (int i = 0; i < 4; i++) dest[i] = v[0];
						break;
					case 2:
						dest[0] = dest[2] = v[0];
						dest[1] = dest[3] = v[1];
						break;
					case 3:
						dest[0] = v[0];
						dest[1] = v[1];
						dest[2] = v[2];
						dest[3] = v[0];
						break;
					default:
						for (int i = 0; i < 4; i++) dest[i] = v[i];
						break;
				}
				return true;
			}

			bool isBorderWidthKeyword(const string& t) {
				return eq(t, "thin") || eq(t, "medium") || eq(t, "thick");
			}

			float borderWidthKeyword(const string& t) {
				if (eq(t, "thin")) return 1.0f;
				if (eq(t, "thick")) return 3.0f;
				return 2.0f;  // medium
			}

			/** Font-size keywords, resolved against the parent size. */
			bool fontSizeKeyword(const string& t, float parentSize,
				length& out) {
				if (eq(t, "xx-small")) { out = length::px(parentSize * 0.578f); return true; }
				if (eq(t, "x-small")) { out = length::px(parentSize * 0.694f); return true; }
				if (eq(t, "small")) { out = length::px(parentSize * 0.833f); return true; }
				if (eq(t, "medium")) { out = length::px(parentSize); return true; }
				if (eq(t, "large")) { out = length::px(parentSize * 1.2f); return true; }
				if (eq(t, "x-large")) { out = length::px(parentSize * 1.5f); return true; }
				if (eq(t, "xx-large")) { out = length::px(parentSize * 2.0f); return true; }
				if (eq(t, "smaller")) { out = length::px(parentSize * 0.833f); return true; }
				if (eq(t, "larger")) { out = length::px(parentSize * 1.2f); return true; }
				return false;
			}
		}  // namespace

		bool applyDeclaration(computedStyle& s, prop p, const var& value,
			const styleContext& ctx, float parentFontSize, float rootFontSize) {
			string text = valueText(value);
			if (text.empty()) return false;
			const string t = lowerStr(text);

			// em and percentages resolve against the element's own font size,
			// which `font-size` itself resolves against the parent's.
			const float fontSize =
				s.fontSize.defined()
					? resolveLength(s.fontSize, parentFontSize, parentFontSize,
						  rootFontSize, ctx)
					: ctx.defaultFontSize;

			auto setLength = [&](length& dest) {
				return parseLength(t, dest);
			};
			// `currentColor` means "the element's own color value".
			auto setColor = [&](color& dest) {
				if (t == "currentcolor") {
					dest = s.textColor;
					return true;
				}
				return parseColor(t, dest);
			};
			auto setSides = [&](length* dest) { return expandSides(t, dest); };

			switch (p) {
				case prop::display: {
					if (t == "none") s.display = displayType::none;
					else if (t == "block") s.display = displayType::block;
					else if (t == "inline") s.display = displayType::inlineType;
					else if (t == "inline-block")
						s.display = displayType::inlineBlock;
					else if (t == "flex") s.display = displayType::flex;
					else if (t == "inline-flex") s.display = displayType::inlineFlex;
					else if (t == "grid") s.display = displayType::grid;
					else if (t == "inline-grid") s.display = displayType::inlineGrid;
					else if (t == "flow-root" || t == "block flow")
						s.display = displayType::block;
					else return false;
					return true;
				}
				case prop::position: {
					if (t == "static") s.position = positionType::staticPos;
					else if (t == "relative") s.position = positionType::relative;
					else if (t == "absolute") s.position = positionType::absolute;
					else return false;
					return true;
				}
				case prop::top: return setLength(s.top);
				case prop::right: return setLength(s.right);
				case prop::bottom: return setLength(s.bottom);
				case prop::left: return setLength(s.left);

				case prop::zIndex: {
					if (t == "auto") s.zIndex = 0;
					else s.zIndex = atoi(t.c_str());
					return true;
				}
				case prop::width: return setLength(s.width);
				case prop::height: return setLength(s.height);
				case prop::minWidth: return setLength(s.minWidth);
				case prop::minHeight: return setLength(s.minHeight);
				case prop::maxWidth: return setLength(s.maxWidth);
				case prop::maxHeight: return setLength(s.maxHeight);

				case prop::boxSizing: {
					if (t == "content-box") s.boxSizing = boxSizingType::contentBox;
					else if (t == "border-box") s.boxSizing = boxSizingType::borderBox;
					else return false;
					return true;
				}
				case prop::overflow: {
					if (t == "visible") s.overflow = overflowType::visible;
					else if (t == "hidden") s.overflow = overflowType::hidden;
					else if (t == "scroll") s.overflow = overflowType::scroll;
					else if (t == "auto") s.overflow = overflowType::auto_;
					else if (t == "clip") s.overflow = overflowType::hidden;
					else return false;
					return true;
				}
				case prop::visibility: {
					if (t == "visible") s.visibility = visibilityType::visible;
					else if (t == "hidden" || t == "collapse")
						s.visibility = visibilityType::hidden;
					else return false;
					return true;
				}
				case prop::opacity: {
					const float v = strtof(t.c_str(), nullptr);
					if (t == "normal") {
						s.opacity = 1.0f;
						return true;
					}
					s.opacity = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
					return true;
				}

				case prop::margin: return setSides(s.margin);
				case prop::marginTop: return setLength(s.margin[0]);
				case prop::marginRight: return setLength(s.margin[1]);
				case prop::marginBottom: return setLength(s.margin[2]);
				case prop::marginLeft: return setLength(s.margin[3]);
				case prop::padding: return setSides(s.padding);
				case prop::paddingTop: return setLength(s.padding[0]);
				case prop::paddingRight: return setLength(s.padding[1]);
				case prop::paddingBottom: return setLength(s.padding[2]);
				case prop::paddingLeft: return setLength(s.padding[3]);

				case prop::borderWidth:
					return setSides(s.borderWidth);
				case prop::borderStyle: {
					borderStyleType st = borderStyleType::solid;
					if (t == "none" || t == "hidden") st = borderStyleType::none;
					else if (t == "dashed") st = borderStyleType::dashed;
					else if (t == "dotted") st = borderStyleType::dotted;
					else if (t == "double") st = borderStyleType::double_;
					else if (t != "solid" && t != "groove" && t != "ridge" &&
							 t != "inset" && t != "outset")
						return false;
					for (int i = 0; i < 4; i++) s.borderStyle[i] = st;
					return true;
				}
				case prop::borderColor: {
					// `border-color: red` is a color shorthand; a bare keyword
					// like `solid` is not a color and must be rejected.
					color c;
					if (parseColor(t, c)) {
						for (int i = 0; i < 4; i++) s.borderColor[i] = c;
						return true;
					}
					return expandSides(t, s.borderColor);
				}
				case prop::borderRadius: return expandCorners(t, s.borderRadius);

				// `border-top: <width> <style> <color>` and friends.
				case prop::border:
				case prop::borderTop:
				case prop::borderRight:
				case prop::borderBottom:
				case prop::borderLeft: {
					// Index of the first side this shorthand covers.
					const int first = p == prop::border	 ? 0
									  : p == prop::borderTop	 ? 0
									  : p == prop::borderRight ? 1
									  : p == prop::borderBottom ? 2
																: 3;
					const int count = p == prop::border ? 4 : 1;
					float width = 0.0f;
					bool haveWidth = false;
					color sideColor;
					bool haveColor = false;
					borderStyleType sideStyle = borderStyleType::solid;
					for (const string& token : splitValues(t, ' ')) {
						length w;
						if (!haveWidth && parseLength(token, w)) {
							width = w.value;
							haveWidth = true;
							continue;
						}
						if (isBorderWidthKeyword(token)) {
							width = borderWidthKeyword(token);
							haveWidth = true;
							continue;
						}
						borderStyleType st;
						if (token == "none" || token == "hidden")
							st = borderStyleType::none;
						else if (token == "dashed")
							st = borderStyleType::dashed;
						else if (token == "dotted")
							st = borderStyleType::dotted;
						else if (token == "double")
							st = borderStyleType::double_;
						else if (token == "solid" || token == "groove" ||
								 token == "ridge" || token == "inset" ||
								 token == "outset")
							st = borderStyleType::solid;
						else {
							color c;
							if (!haveColor && parseColor(token, c)) {
								sideColor = c;
								haveColor = true;
								continue;
							}
							return false;
						}
						sideStyle = st;
					}
					for (int i = 0; i < count; i++) {
						const int side = first + i;
						// The `border` shorthand resets the side to solid.
						s.borderStyle[side] = sideStyle;
						if (haveWidth) s.borderWidth[side] = length::px(width);
						if (haveColor) s.borderColor[side] = sideColor;
					}
					return true;
				}
				case prop::borderTopWidth:
					if (isBorderWidthKeyword(t)) {
						s.borderWidth[0] = length::px(borderWidthKeyword(t));
						return true;
					}
					return setLength(s.borderWidth[0]);
				case prop::borderRightWidth:
					if (isBorderWidthKeyword(t)) {
						s.borderWidth[1] = length::px(borderWidthKeyword(t));
						return true;
					}
					return setLength(s.borderWidth[1]);
				case prop::borderBottomWidth:
					if (isBorderWidthKeyword(t)) {
						s.borderWidth[2] = length::px(borderWidthKeyword(t));
						return true;
					}
					return setLength(s.borderWidth[2]);
				case prop::borderLeftWidth:
					if (isBorderWidthKeyword(t)) {
						s.borderWidth[3] = length::px(borderWidthKeyword(t));
						return true;
					}
					return setLength(s.borderWidth[3]);
				case prop::borderTopColor: return setColor(s.borderColor[0]);
				case prop::borderRightColor: return setColor(s.borderColor[1]);
				case prop::borderBottomColor: return setColor(s.borderColor[2]);
				case prop::borderLeftColor: return setColor(s.borderColor[3]);

				case prop::backgroundColor: {
					if (t == "transparent") {
						s.backgroundColor = {0, 0, 0, 0, true};
						return true;
					}
					return setColor(s.backgroundColor);
				}
				case prop::backgroundImage:
					return parseBackgroundImage(t, s.backgroundImage);
				case prop::backgroundSize: {
					if (t == "cover") {
						s.backgroundSizeMode = bgSizeType::cover;
						return true;
					}
					if (t == "contain") {
						s.backgroundSizeMode = bgSizeType::contain;
						return true;
					}
					vector<string> parts = splitValues(t, ' ');
					if (parts.size() == 1) {
						if (!parseLength(parts[0], s.backgroundSizeW)) return false;
						s.backgroundSizeH = s.backgroundSizeW;
					} else if (parts.size() == 2) {
						if (!parseLength(parts[0], s.backgroundSizeW)) return false;
						if (!parseLength(parts[1], s.backgroundSizeH)) return false;
					} else {
						return false;
					}
					s.backgroundSizeMode = bgSizeType::explicitSize;
					return true;
				}
				case prop::backgroundPosition: {
					vector<string> parts = splitValues(t, ' ');
					if (parts.empty()) return false;
					if (!parseLength(parts[0], s.backgroundPosX)) return false;
					s.backgroundPosY = s.backgroundPosX;
					if (parts.size() >= 2 && !parseLength(parts[1], s.backgroundPosY))
						return false;
					return true;
				}
				case prop::backgroundRepeat: {
					if (t == "repeat") s.backgroundRepeat = true;
					else if (t == "no-repeat" || t == "repeat-x" ||
							 t == "repeat-y" || t == "space" || t == "round")
						s.backgroundRepeat = false;
					else return false;
					return true;
				}
				case prop::background: {
					// `background: <color> <image> <position/size> <repeat>`
					bool any = false;
					for (const string& token : splitValues(t, ' ')) {
						const string tk = lowerStr(token);
						if (tk == "no-repeat" || tk == "repeat-x" ||
							tk == "repeat-y") {
							s.backgroundRepeat = (tk == "repeat-x" ||
												  tk == "repeat-y");
							any = true;
							continue;
						}
						if (tk == "none") {
							s.backgroundImage = backgroundPaint();
							any = true;
							continue;
						}
						if (tk == "center" || tk == "left" || tk == "right" ||
							tk == "top" || tk == "bottom") {
							any = true;
							continue;
						}
						if (parseColor(tk, s.backgroundColor)) {
							any = true;
							continue;
						}
						backgroundPaint paint;
						if (parseBackgroundImage(tk, paint)) {
							s.backgroundImage = paint;
							any = true;
							continue;
						}
					}
					return any;
				}

				case prop::color: return setColor(s.textColor);
				case prop::fontFamily: {
					vector<string> parts = splitValues(t, ',');
					if (parts.empty()) return false;
					// Quoted names are common; strip the quotes.
					string first = parts[0];
					if (first.size() >= 2 &&
						((first.front() == '"' && first.back() == '"') ||
						 (first.front() == '\'' && first.back() == '\'')))
						first = first.substr(1, first.size() - 2);
					s.fontFamily = first;
					return true;
				}
				case prop::fontSize: {
					length out;
					if (fontSizeKeyword(t, parentFontSize, out)) {
						s.fontSize = out;
						return true;
					}
					return setLength(s.fontSize);
				}
				case prop::fontWeight: {
					if (t == "normal") s.fontWeight = 400;
					else if (t == "bold") s.fontWeight = 700;
					else if (t == "lighter") s.fontWeight = 300;
					else if (t == "bolder") s.fontWeight = 700;
					else {
						const int w = atoi(t.c_str());
						if (w <= 0) return false;
						s.fontWeight = w;
					}
					return true;
				}
				case prop::fontStyle: {
					if (t == "normal") s.italic = false;
					else if (t == "italic" || t == "oblique") s.italic = true;
					else return false;
					return true;
				}
				case prop::lineHeight: {
					if (t == "normal") {
						s.lineHeight = length::undef();
						return true;
					}
					// A bare number is a multiplier, not a pixel length.
					bool numeric = true;
					for (char c : t)
						if (!isdigit((unsigned char)c) && c != '.' && c != '-' &&
							c != '+')
							numeric = false;
					if (numeric) {
						s.lineHeight = {strtof(t.c_str(), nullptr), unit::number};
						return true;
					}
					return setLength(s.lineHeight);
				}
				case prop::letterSpacing:
					if (t == "normal") {
						s.letterSpacing = length::px(0.0f);
						return true;
					}
					return setLength(s.letterSpacing);
				case prop::textIndent: return setLength(s.textIndent);
				case prop::textAlign: {
					if (t == "left" || t == "start") s.textAlign = textAlignType::left;
					else if (t == "right" || t == "end") s.textAlign = textAlignType::right;
					else if (t == "center") s.textAlign = textAlignType::center;
					else if (t == "justify") s.textAlign = textAlignType::justify;
					else return false;
					return true;
				}
				case prop::textDecoration: {
					if (t == "none") {
						s.underline = false;
						s.lineThrough = false;
					} else {
						if (t.find("underline") != string::npos) s.underline = true;
						if (t.find("line-through") != string::npos) s.lineThrough = true;
						if (!s.underline && !s.lineThrough) return false;
					}
					return true;
				}
				case prop::whiteSpace: {
					if (t == "normal") s.whiteSpace = whiteSpaceType::normal;
					else if (t == "nowrap") s.whiteSpace = whiteSpaceType::nowrap;
					else if (t == "pre") s.whiteSpace = whiteSpaceType::pre;
					else if (t == "pre-wrap" || t == "pre-line")
						s.whiteSpace = whiteSpaceType::preWrap;
					else return false;
					return true;
				}
				case prop::verticalAlign: {
					if (t == "baseline") s.verticalAlign = alignType::baseline;
					else if (t == "top") s.verticalAlign = alignType::start;
					else if (t == "bottom") s.verticalAlign = alignType::end;
					else if (t == "middle") s.verticalAlign = alignType::center;
					else if (t == "sub") s.verticalAlign = alignType::end;
					else if (t == "super") s.verticalAlign = alignType::start;
					else return false;
					return true;
				}
				case prop::listStyle: {
					if (t == "none") s.listStyle = listStyleType::none_;
					else if (t == "disc") s.listStyle = listStyleType::disc;
					else if (t == "circle") s.listStyle = listStyleType::circle;
					else if (t == "square") s.listStyle = listStyleType::square;
					else if (t == "decimal") s.listStyle = listStyleType::decimal;
					else return false;
					return true;
				}
				case prop::cursor: {
					s.cursor = t;
					return true;
				}
				case prop::pointerEvents: {
					if (t == "none") s.pointerEvents = false;
					else if (t == "auto" || t == "all") s.pointerEvents = true;
					else return false;
					return true;
				}

				case prop::flexDirection: {
					if (t == "row") s.flexDirection = flexDirectionType::row;
					else if (t == "row-reverse")
						s.flexDirection = flexDirectionType::rowReverse;
					else if (t == "column")
						s.flexDirection = flexDirectionType::column;
					else if (t == "column-reverse")
						s.flexDirection = flexDirectionType::columnReverse;
					else return false;
					return true;
				}
				case prop::flexWrap: {
					if (t == "nowrap") s.flexWrap = flexWrapType::noWrap;
					else if (t == "wrap") s.flexWrap = flexWrapType::wrap;
					else if (t == "wrap-reverse")
						s.flexWrap = flexWrapType::wrapReverse;
					else return false;
					return true;
				}
				case prop::flexGrow: {
					s.flexGrow = t == "0" ? 0.0f : strtof(t.c_str(), nullptr);
					return true;
				}
				case prop::flexShrink: {
					s.flexShrink = t == "0" ? 0.0f : strtof(t.c_str(), nullptr);
					return true;
				}
				case prop::flexBasis: {
					if (t == "content" || t == "auto") {
						s.flexBasis = length::makeAuto();
						return true;
					}
					if (t == "none") {
						s.flexBasis = length::px(0.0f);
						return true;
					}
					return setLength(s.flexBasis);
				}
				case prop::order: {
					s.order = atoi(t.c_str());
					return true;
				}
				case prop::gap: {
					vector<string> parts = splitValues(t, ' ');
					if (parts.empty()) return false;
					if (!parseLength(parts[0], s.rowGap)) return false;
					s.columnGap = s.rowGap;
					if (parts.size() >= 2 && !parseLength(parts[1], s.columnGap))
						return false;
					return true;
				}
				case prop::rowGap: return setLength(s.rowGap);
				case prop::columnGap: return setLength(s.columnGap);
				case prop::justifyContent:
				case prop::justifyItems: {
					justifyType j;
					if (t == "flex-start" || t == "start" || t == "left" ||
						t == "normal")
						j = justifyType::start;
					else if (t == "flex-end" || t == "end" || t == "right")
						j = justifyType::end;
					else if (t == "center") j = justifyType::center;
					else if (t == "space-between") j = justifyType::spaceBetween;
					else if (t == "space-around") j = justifyType::spaceAround;
					else if (t == "space-evenly") j = justifyType::spaceEvenly;
					else if (t == "stretch") j = justifyType::start;
					else return false;
					if (p == prop::justifyContent) s.justifyContent = j;
					else s.justifyItems = j;
					return true;
				}
				case prop::alignItems:
				case prop::alignSelf:
				case prop::alignContent: {
					alignType a;
					if (t == "flex-start" || t == "start" || t == "left" ||
						t == "normal")
						a = alignType::start;
					else if (t == "flex-end" || t == "end" || t == "right")
						a = alignType::end;
					else if (t == "center") a = alignType::center;
					else if (t == "baseline") a = alignType::baseline;
					else if (t == "stretch") a = alignType::stretch;
					else if (t == "auto" && p == prop::alignSelf)
						a = alignType::auto_;
					else return false;
					if (p == prop::alignItems) s.alignItems = a;
					else if (p == prop::alignSelf) s.alignSelf = a;
					else s.alignContent = a;
					return true;
				}

				case prop::gridTemplateColumns:
					return parseTracks(t, s.gridColumns);
				case prop::gridTemplateRows:
					return parseTracks(t, s.gridRows);
				case prop::gridAutoColumns: {
					vector<trackSpec> tracks;
					if (!parseTracks(t, tracks)) return false;
					if (!tracks.empty()) s.gridAutoColumns = tracks[0];
					return true;
				}
				case prop::gridAutoRows: {
					vector<trackSpec> tracks;
					if (!parseTracks(t, tracks)) return false;
					if (!tracks.empty()) s.gridAutoRows = tracks[0];
					return true;
				}
				case prop::gridAutoFlow: {
					if (t == "row") s.gridAutoFlow = gridAutoFlowType::row;
					else if (t == "column") s.gridAutoFlow = gridAutoFlowType::column;
					else if (t == "row dense")
						s.gridAutoFlow = gridAutoFlowType::row;
					else if (t == "column dense")
						s.gridAutoFlow = gridAutoFlowType::column;
					else if (t == "dense") s.gridAutoFlow = gridAutoFlowType::dense;
					else return false;
					return true;
				}
				case prop::gridColumn: {
					s.gridColumn = t;
					return true;
				}
				case prop::gridRow: {
					s.gridRow = t;
					return true;
				}

				case prop::animation: {
					s.animationShorthand = t;
					return true;
				}
				case prop::transition: {
					s.transitionShorthand = t;
					return true;
				}
				default: return false;
			}
		}

		void applyDeclarations(computedStyle& style,
			const object& declarations, const styleContext& ctx,
			float parentFontSize, float rootFontSize, importantMode mode) {
			if (!declarations) return;
			object decls = declarations;
			for (auto it = decls.begin(); it != decls.end(); ++it) {
				// `CSS::parseDeclarations` records `!important` as a flag
				// under `name + "!"`; those keys are skipped, not applied.
				if (!it->first.empty() && it->first.back() == '!') continue;
				if (mode == importantMode::only &&
					!decls.getBool(it->first + "!", false))
					continue;
				if (mode == importantMode::skip &&
					decls.getBool(it->first + "!", false))
					continue;
				const string raw = valueText(it->second);
				if (raw.empty()) continue;
				prop p;
				if (!lookupProperty(it->first, p)) continue;
				applyDeclaration(style, p, var(raw), ctx, parentFontSize,
					rootFontSize);
			}
		}

		vector<computedStyle> resolveStyles(const domTree& tree,
			const stylesheet& sheet, const styleContext& ctx) {
			vector<computedStyle> out(tree.nodes.size());
			if (tree.nodes.empty()) return out;

			// The root font size backs `rem`, so resolve the <html> element's
			// own font-size first.
			float rootFontSize = ctx.defaultFontSize;
			{
				int html = tree.byTag("html");
				if (html < 0) {
					vector<int> tops = tree.roots();
					if (!tops.empty()) html = tops[0];
				}
				if (html >= 0) {
					computedStyle probe;
					for (const styleRule& rule : sheet.rules) {
						if (!mediaMatches(rule.media, ctx)) continue;
						if (!selectorMatches(rule.selector, tree, html)) continue;
						applyDeclarations(probe, rule.declarations, ctx,
							ctx.defaultFontSize, ctx.defaultFontSize);
					}
					if (probe.fontSize.defined())
						rootFontSize = resolveLength(probe.fontSize,
							ctx.defaultFontSize, ctx.defaultFontSize,
							ctx.defaultFontSize, ctx);
				}
			}

			// A single forward pass works: buildTree emits parents before
			// children, so a parent's style is always ready.
			vector<const styleRule*> matched;
			for (size_t i = 0; i < tree.nodes.size(); i++) {
				const domNode& node = tree.nodes[i];
				computedStyle style;
				float parentFontSize = ctx.defaultFontSize;

				if (node.parent >= 0) {
					style.inheritFrom(out[(size_t)node.parent]);
					parentFontSize = resolveLength(
						out[(size_t)node.parent].fontSize, ctx.defaultFontSize,
						ctx.defaultFontSize, rootFontSize, ctx);
				} else {
					style.fontFamily = ctx.defaultFontFamily;
					style.fontSize = length::px(ctx.defaultFontSize);
				}

				// UA defaults for the elements that need them.
				if (!node.isText) {
					const string& tag = node.tag;
					if (tag == "b" || tag == "strong") style.fontWeight = 700;
					else if (tag == "i" || tag == "em" || tag == "cite" ||
							 tag == "var" || tag == "dfn")
						style.italic = true;
					else if (tag == "code" || tag == "kbd" || tag == "samp" ||
							 tag == "tt")
						style.fontFamily = "monospace";
					else if (tag == "small") {
						style.fontSize = length::px(parentFontSize * 0.833f);
					} else if (tag == "big") {
						style.fontSize = length::px(parentFontSize * 1.2f);
					} else if (tag == "u" || tag == "ins") {
						style.underline = true;
					} else if (tag == "s" || tag == "del" ||
							 tag == "strike") {
						style.lineThrough = true;
					} else if (tag == "a" && node.hasAttr("href")) {
						style.textColor = {0.22f, 0.45f, 0.9f, 1.0f, true};
						style.underline = true;
					}
					// Blocks by default; `li` gets a marker.
					const bool isBlock =
						tag == "html" || tag == "body" || tag == "div" ||
						tag == "p" || tag == "h1" || tag == "h2" || tag == "h3" ||
						tag == "h4" || tag == "h5" || tag == "h6" || tag == "ul" ||
						tag == "ol" || tag == "li" || tag == "table" ||
						tag == "tr" || tag == "td" || tag == "th" || tag == "hr" ||
						tag == "header" || tag == "footer" || tag == "main" ||
						tag == "section" || tag == "article" || tag == "nav" ||
						tag == "aside" || tag == "form" || tag == "fieldset" ||
						tag == "figure" || tag == "figcaption" ||
						tag == "blockquote" || tag == "pre" || tag == "dl" ||
						tag == "dt" || tag == "dd" || tag == "address";
					if (isBlock) style.display = displayType::block;
					if (tag == "li" && node.parent >= 0) {
						const string& parentTag = tree.nodes[(size_t)node.parent].tag;
						if (parentTag == "ol")
							style.listStyle = listStyleType::decimal;
						else if (parentTag == "ul")
							style.listStyle = listStyleType::disc;
					}
					if (tag == "br" || tag == "wbr") {
						style.display = displayType::inlineType;
					}
				}

				// Collect matching rules once, then run the cascade in two
				// passes so `!important` wins over later normal declarations.
				matched.clear();
				for (const styleRule& rule : sheet.rules) {
					if (!mediaMatches(rule.media, ctx)) continue;
					if (!selectorMatches(rule.selector, tree, (int)i)) continue;
					matched.push_back(&rule);
				}

				object inlineDecls;
				if (!node.isText && node.hasAttr("style"))
					inlineDecls = CSS::parseDeclarations(node.attrString("style"));

				for (int pass = 0; pass < 2; pass++) {
					const importantMode mode = pass == 0 ? importantMode::skip
														: importantMode::only;
					for (const styleRule* rule : matched)
						applyDeclarations(style, rule->declarations, ctx,
							parentFontSize, rootFontSize, mode);
					applyDeclarations(style, inlineDecls, ctx, parentFontSize,
						rootFontSize, mode);
				}

				out[i] = style;
			}
			return out;
		}

	}  // namespace UI
}  // namespace gold
