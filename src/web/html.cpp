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
				else if (it->isObject())
					attr = it->getObject();
				else if (it->isList())
					items += it->getList();
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
				} else
					buffer +=
						it->first + "=\"" + it->second.getString() + "\" ";
			}
			// Each attribute is emitted with a trailing space; drop the last
			// one so round-tripping a parse produces the original markup.
			if (buffer.size() > 1 && buffer.back() == ' ') buffer.pop_back();
			buffer += ">";
			auto defHTML = iHTML();
			for (auto it = items.begin(); it != items.end(); ++it) {
				if (it->isObject(getPrototype())) {
					auto obj = it->getObject<iHTML>();
					buffer += (string)(obj);
				} else if (it->isString())
					buffer += (string)(*it);
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
					if (isNumericValue(value))
						attr.setInt64(name, stoll(value));
					else
						attr.setString(name, value);
				}
			}
		}  // namespace

		list parseHTML(const string& html) {
			list roots;
			vector<HTML::iHTML> stack;
			size_t pos = 0;
			string pending;

			// Append text to the innermost open element (or to the roots).
			auto flushText = [&]() {
				if (pending.empty()) return;
				if (!stack.empty())
					stack.back().getList("items").pushString(pending);
				else
					roots.pushString(pending);
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
						const string content =
							html.substr(pos, contentEnd - pos);
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