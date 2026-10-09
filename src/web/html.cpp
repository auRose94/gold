#include "html.hpp"

#include <algorithm>
#include <cctype>

#define DefineElementType(name)                 \
	name::name() : iHTML(#name, {}) {}            \
	name::name(const name& copy) : iHTML(copy) {} \
	name::name(list args) : iHTML(#name, args) {}

#define DefineElementTypeTag(name, tag)         \
	name::name() : iHTML(tag, {}) {}              \
	name::name(const name& copy) : iHTML(copy) {} \
	name::name(list args) : iHTML(tag, args) {}

namespace gold {
	using namespace std;
	namespace HTML {
		gold::obj& iHTML::getPrototype() {
			static auto proto = obj({
				{"setAttributes", method(&iHTML::setAttributes)},
				{"getAttribute", method(&iHTML::getAttribute)},
				{"addElements", method(&iHTML::addElements)},
				{"removeElement", method(&iHTML::removeElement)},
			});
			return proto;
		}

		iHTML::iHTML() : obj() {}

		iHTML::iHTML(const char* tag, list args) : obj() {
			setParent(getPrototype());
			setString("tag", tag);
			auto items = list({});
			auto attr = obj({});
			for (auto it = args.begin(); it != args.end(); ++it) {
				if (it->isObject(getPrototype()))
					items.pushVar(*it);
				else if (it->isString())
					items.pushVar(*it);
				else if (it->isList())
					items += it->getList();
				else if (it->isObject()) {
					// An attribute bundle. Several may appear; their keys
					// merge. `items` inside a bundle routes to children
					// instead of becoming an attribute.
					object bundle = it->getObject();
					for (auto kv = bundle.begin(); kv != bundle.end(); ++kv) {
						if (kv->first == "items") {
							if (kv->second.isList())
								items += kv->second.getList();
							else
								items.pushVar(kv->second);
							continue;
						}
						if (kv->second.isString())
							attr.setString(kv->first, kv->second.getString());
						else if (kv->second.isBool())
							attr.setBool(kv->first, kv->second.getBool());
						else
							attr.setVar(kv->first, kv->second);
					}
				}
			}
			setList("items", items);
			setObject("attr", attr);
		}

		gold::var iHTML::setAttributes(list args) {
			auto attr = getObject("attr");
			auto o = args[0].getObject();
			if (o && attr) {
				attr.copy(o);
				return gold::var(attr);
			}
			return gold::var();
		}

		gold::var iHTML::getAttribute(list) {
			auto attr = getObject("attr");
			if (attr) return gold::var(attr);
			return gold::var();
		}

		gold::var iHTML::addElements(list args) {
			auto items = getList("items");
			for (auto it = args.begin(); it != args.end(); ++it) {
				if (it->isObject(getPrototype()))
					items.pushVar(*it);
				else if (it->isString())
					items.pushVar(*it);
			}
			return var();
		}

		gold::var iHTML::removeElement(list args) {
			auto items = getList("items");
			for (auto it = args.begin(); it != args.end(); ++it) {
				if (it->isObject(getPrototype())) {
					auto in = items.find(*it);
					if (in != items.end()) items.erase(in);
				}
			}
			return var();
		}

		iHTML::operator string() {
			auto buffer = string();
			auto tag = getString("tag");
			auto attr = getObject("attr");
			auto items = getList("items");
			buffer += "<" + tag;
			if (attr.size() > 0) buffer += " ";
			for (auto it = attr.begin(); it != attr.end(); ++it) {
				if (it->second.isBool()) {
					if (it->second.getBool()) buffer += it->first + " ";
				} else if (it->second.isFloating()) {
					buffer += it->first + "=\"" +
									  to_string(it->second.getDouble()) + "\" ";
				} else if (it->second.isNumber()) {
					buffer += it->first + "=\"" +
									  to_string(it->second.getInt64()) + "\" ";
				} else if (it->second.isString()) {
					buffer += it->first + "=\"" +
							  Parser::escapeHTML(it->second.getString(), true) +
							  "\" ";
				} else
					buffer +=
						it->first + "=\"" + it->second.getString() + "\" ";
			}
			// Each attribute is emitted with a trailing space; drop the last
			// one so round-tripping a parse produces the original markup.
			if (buffer.size() > 1 && buffer.back() == ' ') buffer.pop_back();
			buffer += ">";
			// Void elements have no children and no end tag: "</br>"-style
			// output would round-trip as TWO breaks.
			const bool voidTag = Parser::isVoidTag(tag);
			auto defHTML = iHTML();
			// Script/style bodies are source text, not character data, so
			// they round-trip verbatim (their parser branch reads raw).
			const bool escapeText = tag != "script" && tag != "style";
			if (voidTag) return buffer;
			for (auto it = items.begin(); it != items.end(); ++it) {
				if (it->isObject(getPrototype())) {
					auto obj = it->getObject<iHTML>();
					buffer += (string)(obj);
				} else if (it->isString())
					buffer += escapeText ? Parser::escapeHTML((string)(*it), false)
										 : (string)(*it);
			}
			buffer += "</" + tag + ">";
			return buffer;
		}

		iHTML::operator binary() {
			auto data = (string) * this;
			return binary(data.begin(), data.end());
		}

		iHTML& iHTML::operator+=(list args) {
			auto items = getList("items");
			auto attr = getObject("attr");
			for (auto it = args.begin(); it != args.end(); ++it) {
				if (it->isObject(getPrototype()))
					items.pushVar(*it);
				else if (it->isObject()) {
					auto o = it->getObject();
					attr.copy(o);
				}
			}
			return *this;
		}

		iHTML& iHTML::operator-=(list args) {
			auto items = getList("items");
			for (auto it = args.begin(); it != args.end(); ++it) {
				if (it->isObject(getPrototype())) {
					auto in = items.find(*it);
					if (in != items.end()) items.erase(in);
				}
			}
			return *this;
		}

		DefineElementTypeTag(hTemplate, "template");
		DefineElementTypeTag(hObject, "object");
		DefineElementTypeTag(hSmall, "small");

		DefineElementType(html);
		DefineElementType(head);
		DefineElementType(body);

		DefineElementType(meta);
		DefineElementType(script);
		DefineElementType(style);

		DefineElementType(nav);
		DefineElementType(base);
		DefineElementType(br);
		DefineElementType(param);
		DefineElementType(link);
		DefineElementType(title);
		DefineElementType(span);
		DefineElementType(p);
		DefineElementType(a);
		DefineElementType(img);
		DefineElementType(area);
		DefineElementType(audio);
		DefineElementType(canvas);
		DefineElementType(embed);
		DefineElementType(source);
		DefineElementType(track);
		DefineElementType(video);
		DefineElementType(map);
		DefineElementType(input);
		DefineElementType(h1);
		DefineElementType(h2);
		DefineElementType(h3);
		DefineElementType(h4);
		DefineElementType(h5);
		DefineElementType(h6);

		DefineElementType(dl);
		DefineElementType(dt);
		DefineElementType(dd);
		DefineElementType(ol);
		DefineElementType(ul);
		DefineElementType(li);

		DefineElementType(adress);
		DefineElementType(article);
		DefineElementType(aside);
		DefineElementType(blockquote);
		DefineElementType(del);
		DefineElementType(div);
		DefineElementType(figure);
		DefineElementType(figcaption);
		DefineElementType(footer);
		DefineElementType(header);
		DefineElementType(hr);
		DefineElementType(ins);
		DefineElementType(main);
		DefineElementType(pre);
		DefineElementType(section);
		DefineElementType(bdi);
		DefineElementType(bdo);
		DefineElementType(cite);
		DefineElementType(data);

		DefineElementType(b);

		DefineElementType(abbr);
		DefineElementType(dfn);
		DefineElementType(q);

		DefineElementType(i);
		DefineElementType(u);

		DefineElementType(s);

		
		DefineElementType(strong);
		DefineElementType(em);
		DefineElementType(mark);
		DefineElementType(rp);
		DefineElementType(sub);
		DefineElementType(rb);
		DefineElementType(rt);
		DefineElementType(ruby);
		DefineElementType(time);
		DefineElementType(wbr);

		DefineElementType(code);
		DefineElementType(kbd);
		DefineElementType(samp);
		DefineElementType(var);

		DefineElementType(form);
		DefineElementType(button);
		DefineElementType(datalist);
		DefineElementType(fieldlist);
		DefineElementType(label);
		DefineElementType(legend);
		DefineElementType(meter);
		DefineElementType(option);
		DefineElementType(optgroup);
		DefineElementType(output);
		DefineElementType(progress);
		DefineElementType(select);
		DefineElementType(textarea);

		DefineElementType(table);
		DefineElementType(tr);
		DefineElementType(th);
		DefineElementType(td);
		DefineElementType(colgroup);
		DefineElementType(col);
		DefineElementType(caption);
		DefineElementType(thead);
		DefineElementType(tbody);
		DefineElementType(tfoot);

		DefineElementType(iframe);

	}  // namespace HTML

	namespace Parser {

		namespace {
			bool isSpace(char c) {
				return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
					   c == '\f' || c == '\v';
			}

			string toLower(string s) {
				for (auto& c : s) c = (char)tolower((unsigned char)c);
				return s;
			}

			// A practical entity set: the five ASCII ones the spec requires
			// plus a few that show up in real text.
			const struct entityDef {
				const char* name;
				const char* utf8;
			} kEntities[] = {
				{"amp", "&"},     {"lt", "<"},     {"gt", ">"},
				{"quot", "\""},   {"apos", "'"},   {"nbsp", "\xC2\xA0"},
				{"shy", "\xC2\xAD"}, {"copy", "\xC2\xA9"}, {"reg", "\xC2\xAE"},
				{"trade", "\xE2\x84\xA2"}, {"hellip", "\xE2\x80\xA6"},
				{"mdash", "\xE2\x80\x94"}, {"ndash", "\xE2\x80\x93"},
				{"lsquo", "\xE2\x80\x98"}, {"rsquo", "\xE2\x80\x99"},
				{"ldquo", "\xE2\x80\x9C"}, {"rdquo", "\xE2\x80\x9D"},
				{"bull", "\xE2\x80\xA2"}, {"middot", "\xC2\xB7"},
				{"times", "\xC3\x97"}, {"divide", "\xC3\xB7"},
				{"deg", "\xC2\xB0"}, {"plusmn", "\xC2\xB1"},
				{"frac12", "\xC2\xBD"}, {"frac14", "\xC2\xBC"},
			};

			void appendCodepoint(string& out, uint32_t cp) {
				if (cp < 0x80)
					out += (char)cp;
				else if (cp < 0x800) {
					out += (char)(0xC0 | (cp >> 6));
					out += (char)(0x80 | (cp & 0x3F));
				} else if (cp < 0x10000) {
					out += (char)(0xE0 | (cp >> 12));
					out += (char)(0x80 | ((cp >> 6) & 0x3F));
					out += (char)(0x80 | (cp & 0x3F));
				} else {
					out += (char)(0xF0 | (cp >> 18));
					out += (char)(0x80 | ((cp >> 12) & 0x3F));
					out += (char)(0x80 | ((cp >> 6) & 0x3F));
					out += (char)(0x80 | (cp & 0x3F));
				}
			}

			// Decode one `&...;` starting at `at`; returns the replacement
			// (empty keeps the source characters as-is).
			string decodeEntity(const string& src, size_t at, size_t& next) {
				const size_t semi = src.find(';', at);
				if (semi == string::npos || semi == at + 1 || semi - at > 12) {
					next = at + 1;
					return "";
				}
				const string body = src.substr(at + 1, semi - at - 1);
				next = semi + 1;
				if (!body.empty() && body[0] == '#') {
					uint32_t cp = 0;
					if (body.size() > 1 && (body[1] == 'x' || body[1] == 'X')) {
						for (size_t k = 2; k < body.size(); k++) {
							const char c = body[k];
							const int d = isdigit((unsigned char)c)	 ? c - '0'
										  : (c >= 'a' && c <= 'f')	 ? c - 'a' + 10
										  : (c >= 'A' && c <= 'F')	 ? c - 'A' + 10
																	 : -1;
							if (d < 0) return "";
							cp = cp * 16 + (uint32_t)d;
						}
					} else {
						for (size_t k = 1; k < body.size(); k++) {
							if (!isdigit((unsigned char)body[k])) return "";
							cp = cp * 10 + (uint32_t)(body[k] - '0');
						}
					}
					if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
						return "";
					string out;
					appendCodepoint(out, cp);
					return out;
				}
				for (const auto& def : kEntities)
					if (body == def.name) return def.utf8;
				return "";
			}
		}  // namespace

		string trim(const string& s) {
			size_t start = s.find_first_not_of(" \t\n\r\f\v");
			if (start == string::npos) return "";
			size_t end = s.find_last_not_of(" \t\n\r\f\v");
			return s.substr(start, end - start + 1);
		}

		bool isNumericValue(const string& s) {
			if (s.empty()) return false;
			size_t i = 0;
			if (s[0] == '-' || s[0] == '+') i = 1;
			if (i >= s.size()) return false;
			bool hasDot = false;
			for (; i < s.size(); i++) {
				if (s[i] == '.') {
					if (hasDot) return false;
					hasDot = true;
				} else if (!isdigit((unsigned char)s[i])) {
					return false;
				}
			}
			return true;
		}

		string decodeEntities(const string& s) {
			// Fast path: nothing to decode.
			if (s.find('&') == string::npos) return s;
			string out;
			size_t i = 0;
			while (i < s.size()) {
				if (s[i] != '&') {
					out += s[i++];
					continue;
				}
				size_t next = i;
				const string decoded = decodeEntity(s, i, next);
				if (decoded.empty())
					out += s[i++];
				else {
					out += decoded;
					i = next;
				}
			}
			return out;
		}

		string escapeHTML(const string& s, bool attribute) {
			string out;
			out.reserve(s.size() + 8);
			for (char c : s) {
				switch (c) {
					case '&': out += "&amp;"; break;
					case '<': out += "&lt;"; break;
					case '>': out += "&gt;"; break;
					case '"':
						if (attribute) {
							out += "&quot;";
							break;
						}
						out += c;
						break;
					default: out += c;
				}
			}
			return out;
		}

		const vector<string>& voidTags() {
			static const vector<string> tags = {
				"area", "base", "br",   "col",	 "embed", "hr",  "img",
				"input", "link", "meta", "param", "source", "track", "wbr"};
			return tags;
		}

		bool isVoidTag(const string& tag) {
			const string t = toLower(trim(tag));
			const vector<string>& tags = voidTags();
			return find(tags.begin(), tags.end(), t) != tags.end();
		}

		namespace {
			// Elements that never have children: their content is raw text up
			// to the matching close tag.
			bool isRawTextTag(const string& tag) {
				return tag == "script" || tag == "style" || tag == "textarea" ||
					   tag == "title";
			}

			// Block-level elements implicitly close an open <p>.
			bool isBlockTag(const string& tag) {
				static const char* blocks[] = {
					"address", "article", "aside",	 "blockquote", "details",
					"div",		"dl",		"fieldset", "figcaption", "figure",
					"footer",	"form",		"hgroup",	 "main",		 "menu",
					"nav",		"ol",		"pre",	 "section",	 "table",
					"ul"};
				for (auto b : blocks)
					if (tag == b) return true;
				if (tag.size() == 2 && tag[0] == 'h' && tag[1] >= '1' &&
					tag[1] <= '6')
					return true;
				return false;
			}

			// An open tag that implicitly closes `open` (walking the stack).
			bool closesTag(const string& open, const string& existing) {
				if (existing == "p") return isBlockTag(open) || open == "p";
				if (existing == "li") return open == "li";
				if (existing == "dt" || existing == "dd")
					return open == "dt" || open == "dd";
				if (existing == "option")
					return open == "option" || open == "optgroup";
				if (existing == "optgroup") return open == "optgroup";
				if (existing == "a") return open == "a";
				if (existing == "tr")
					return open == "tr" || open == "td" || open == "th" ||
						   open == "thead" || open == "tbody" || open == "tfoot";
				if (existing == "td" || existing == "th")
					return open == "td" || open == "th" || open == "tr" ||
						   open == "thead" || open == "tbody" || open == "tfoot";
				if (existing == "thead" || existing == "tbody" ||
					existing == "tfoot")
					return open == "thead" || open == "tbody" || open == "tfoot";
				return false;
			}

			// Parse the attribute section of a start tag into `attr`.
			void parseAttributes(const string& src, object& attr) {
				size_t i = 0;
				while (i < src.size()) {
					while (i < src.size() && isSpace(src[i])) i++;
					if (i >= src.size()) break;

					size_t nameStart = i;
					while (i < src.size() && !isSpace(src[i]) && src[i] != '=' &&
						   src[i] != '/')
						i++;
					if (i == nameStart) {
						i++;
						continue;
					}
					const string name = toLower(src.substr(nameStart, i - nameStart));

					size_t save = i;
					while (i < src.size() && isSpace(src[i])) i++;
					if (i >= src.size() || src[i] != '=') {
						// Boolean attribute (e.g. `disabled`).
						attr.setBool(name, true);
						i = save;
						continue;
					}
					i++;  // consume '='
					while (i < src.size() && isSpace(src[i])) i++;

					string value;
					if (i < src.size() && (src[i] == '"' || src[i] == '\'')) {
						const char quote = src[i++];
						const size_t valStart = i;
						while (i < src.size() && src[i] != quote) i++;
						value = src.substr(valStart, i - valStart);
						if (i < src.size()) i++;  // consume closing quote
					} else {
						const size_t valStart = i;
						while (i < src.size() && !isSpace(src[i]) && src[i] != '>')
							i++;
						value = src.substr(valStart, i - valStart);
					}
					const string decoded = decodeEntities(value);
					// Attribute values are strings (HTML semantics): plain
					// numbers stay text, so `stoll` never silently
					// truncates a decimal like "12.5".
					attr.setString(name, decoded);
				}
			}
		}  // namespace

		list parseHTML(const string& html) {
			list roots;
			vector<HTML::iHTML> stack;
			size_t pos = 0;
			string pending;

			// Append text to the innermost open element (or to the roots).
			// Character references (`&amp;`,...) are decoded here, per the
			// "escapable raw text" rule; script/style bodies are raw and
			// are attached separately below.
			auto flushText = [&]() {
				if (pending.empty()) return;
				const string text = decodeEntities(pending);
				if (!stack.empty())
					stack.back().getList("items").pushString(text);
				else
					roots.pushString(text);
				pending.clear();
			};

			while (pos < html.size()) {
				if (html[pos] != '<') {
					const size_t next = html.find('<', pos);
					const size_t end = (next == string::npos) ? html.size() : next;
					pending += html.substr(pos, end - pos);
					pos = end;
					continue;
				}

				// Comment / doctype / CDATA: skipped entirely.
				if (html.compare(pos, 4, "<!--") == 0) {
					const size_t end = html.find("-->", pos + 4);
					pos = (end == string::npos) ? html.size() : end + 3;
					continue;
				}
				if (html.compare(pos, 2, "<!") == 0 || html.compare(pos, 2, "<?") == 0) {
					const size_t end = html.find('>', pos);
					pos = (end == string::npos) ? html.size() : end + 1;
					continue;
				}

				// Close tag.
				if (html.compare(pos, 2, "</") == 0) {
					flushText();
					const size_t end = html.find('>', pos);
					if (end == string::npos) break;
					const string tag =
						toLower(trim(html.substr(pos + 2, end - pos - 2)));
					pos = end + 1;
					// Pop to the nearest matching open element; ignore a
					// stray close tag that has no open counterpart.
					for (int i = (int)stack.size() - 1; i >= 0; i--) {
						if (stack[(size_t)i].getString("tag") == tag) {
							stack.resize((size_t)i);
							break;
						}
					}
					continue;
				}

				// Start tag.
				{
					size_t i = pos + 1;
					const size_t nameStart = i;
					while (i < html.size() && !isSpace(html[i]) && html[i] != '>' &&
						   html[i] != '/')
						i++;
					const string tag = toLower(html.substr(nameStart, i - nameStart));
					if (tag.empty()) {
						pos++;
						continue;
					}

					size_t gt = html.find('>', i);
					size_t attrEnd = gt;
					bool selfClose = false;
					if (gt == string::npos) {
						attrEnd = html.size();
					} else {
						// Detect a trailing '/'.
						size_t k = gt;
						while (k > i && isSpace(html[k - 1])) k--;
						if (k > i && html[k - 1] == '/') {
							selfClose = true;
							attrEnd = k - 1;
						}
					}
					const string attrSrc = html.substr(i, attrEnd - i);
					pos = (gt == string::npos) ? html.size() : gt + 1;

					flushText();

					// Implicit close: <li> inside <li>, <p> before a block, ...
					while (!stack.empty() &&
						   closesTag(tag, stack.back().getString("tag")))
						stack.pop_back();

					HTML::iHTML el(tag.c_str(), list({}));
					object attr;
					parseAttributes(attrSrc, attr);
					if (attr.size() > 0) el.setObject("attr", attr);

					if (stack.empty()) roots.pushVar(var(el));
					else stack.back().getList("items").pushVar(var(el));

					const bool voidEl = isVoidTag(tag) || selfClose;
					if (voidEl) continue;

					if (isRawTextTag(tag)) {
						// Consume raw content up to the matching close tag.
						const string closeTag = "</" + tag;
						size_t scan = pos;
						size_t contentEnd = html.size();
						size_t nextPos = html.size();
						while (scan < html.size()) {
							const size_t at = html.find('<', scan);
							if (at == string::npos) break;
							if (html.compare(at, closeTag.size(), closeTag) == 0) {
								contentEnd = at;
								const size_t gt = html.find('>', at);
								nextPos = (gt == string::npos) ? html.size() : gt + 1;
								break;
							}
							scan = at + 1;
						}
						string content = html.substr(pos, contentEnd - pos);
						// `title` / `textarea` still decode character
						// references; `script` / `style` are raw source.
						if (tag != "script" && tag != "style")
							content = decodeEntities(content);
						if (!content.empty())
							el.getList("items").pushString(content);
						pos = nextPos;
						continue;
					}

					stack.push_back(el);
				}
			}

			flushText();
			return roots;
		}

	}  // namespace Parser
}  // namespace gold