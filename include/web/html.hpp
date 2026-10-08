#pragma once

#include "types.hpp"

#define NewElementType(name)   \
	struct name : public iHTML { \
	 public:                     \
		name();                    \
		name(const name& copy);    \
		name(list args);           \
	}

namespace gold {
	using std::map;
	using std::string;
	using std::vector;

	namespace HTML {
		struct iHTML : public gold::object {
		 public:
			static gold::object& getPrototype();
			iHTML();
			iHTML(const char* tag, list args);

			gold::var setAttributes(list args);
			gold::var getAttribute(list args);

			gold::var addElements(list args);
			gold::var removeElement(list args);

			operator string();
			operator binary();

			iHTML& operator+=(list args);
			iHTML& operator-=(list args);
		};
		using atts = object;

		// Text element for holding text content
		class text : public iHTML {
		public:
			text() : iHTML("text", {}) {}
			text(const text& copy) : iHTML(copy) {}
			text(list args) : iHTML("text", args) {}
		};

		NewElementType(hTemplate);
		NewElementType(hObject);

		NewElementType(html);
		NewElementType(head);
		NewElementType(body);

		NewElementType(meta);
		NewElementType(script);
		NewElementType(style);

		NewElementType(nav);
		NewElementType(base);
		NewElementType(br);
		NewElementType(param);
		NewElementType(link);
		NewElementType(title);
		NewElementType(span);
		NewElementType(p);
		NewElementType(a);
		NewElementType(img);
		NewElementType(area);
		NewElementType(audio);
		NewElementType(canvas);
		NewElementType(embed);
		NewElementType(source);
		NewElementType(track);
		NewElementType(video);
		NewElementType(map);
		NewElementType(input);
		NewElementType(h1);
		NewElementType(h2);
		NewElementType(h3);
		NewElementType(h4);
		NewElementType(h5);
		NewElementType(h6);

		NewElementType(dl);
		NewElementType(dt);
		NewElementType(dd);
		NewElementType(ol);
		NewElementType(ul);
		NewElementType(li);

		NewElementType(adress);
		NewElementType(article);
		NewElementType(aside);
		NewElementType(blockquote);
		NewElementType(del);
		NewElementType(div);
		NewElementType(figure);
		NewElementType(figcaption);
		NewElementType(footer);
		NewElementType(header);
		NewElementType(hr);
		NewElementType(ins);
		NewElementType(main);
		NewElementType(pre);
		NewElementType(section);
		NewElementType(bdi);
		NewElementType(bdo);
		NewElementType(cite);
		NewElementType(data);

		NewElementType(b);
		NewElementType(abbr);
		NewElementType(dfn);
		NewElementType(q);
		NewElementType(i);
		NewElementType(u);
		NewElementType(s);
		NewElementType(hSmall);
		NewElementType(strong);
		NewElementType(em);
		NewElementType(mark);
		NewElementType(rp);
		NewElementType(sub);
		NewElementType(rb);
		NewElementType(rt);
		NewElementType(ruby);
		NewElementType(time);
		NewElementType(wbr);

		NewElementType(code);
		NewElementType(kbd);
		NewElementType(samp);
		NewElementType(var);

		NewElementType(form);
		NewElementType(button);
		NewElementType(datalist);
		NewElementType(fieldlist);
		NewElementType(label);
		NewElementType(legend);
		NewElementType(meter);
		NewElementType(option);
		NewElementType(optgroup);
		NewElementType(output);
		NewElementType(progress);
		NewElementType(select);
		NewElementType(textarea);

		NewElementType(table);
		NewElementType(tr);
		NewElementType(th);
		NewElementType(td);
		NewElementType(colgroup);
		NewElementType(col);
		NewElementType(caption);
		NewElementType(thead);
		NewElementType(tbody);
		NewElementType(tfoot);

		NewElementType(iframe);

	}  // namespace HTML

	namespace Parser {
		list parseHTML(const string& html);
		const vector<string>& voidTags();
		bool isVoidTag(const string& tag);

		/** Decode character references (`&amp;`, `&#65;`, `&#x42;`) in text
		 *  and attribute values; unknown or truncated entities survive. */
		string decodeEntities(const string& text);
		/** Escape text (or, attribute=true, a value about to be quoted) so a
		 *  string can be embedded in markup. */
		string escapeHTML(const string& text, bool attribute = false);

		string trim(const string& s);
		bool isNumericValue(const string& s);
	}

}  // namespace gold