#include "html.hpp"

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
}  // namespace gold