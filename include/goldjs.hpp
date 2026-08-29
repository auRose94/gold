#pragma once

// goldjs.hpp — JavaScript/TypeScript-style ergonomics for gold.
//
// Property access (read+write through objects/lists) comes from types.hpp
// (varRef, via v["key"] / v[i]). This header adds small helpers that make
// gold objects read like JS literals and pipelines:
//
//   auto user = o(
//     "name", "bob",
//     "tags", a("admin", "dev"),
//     "stats", o("age", 30, "score", 9.5)
//   );
//   user["score"] = 10;                 // write through
//   t("Hello $0, you scored $1", user["name"], user["score"]);  // "Hello bob, ..."
//   each(user["tags"].getList(), func([&](list e){ ... }));

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "goldjson.hpp"
#include "types.hpp"

namespace gold {

	// ------------------------------------------------------------------
	// object/array literals
	// ------------------------------------------------------------------

	/** `o("a", 1, "b", "x", ...)` -> object {a:1, b:"x"} (nestable). */
	inline object o() { return object(); }
	template <typename V, typename... R>
	object o(const char* key, V&& value, R&&... rest) {
		object r;
		r.setVar(key, var(std::forward<V>(value)));
		if constexpr (sizeof...(rest) > 0) {
			auto more = o(std::forward<R>(rest)...);
			for (auto it = more.begin(); it != more.end(); ++it)
				r.setVar(it->first, it->second);
		}
		return r;
	}

	/** `a(1, "x", 2.5, ...)` -> list [1, "x", 2.5] (nestable). */
	inline list a() { return list(); }
	template <typename V, typename... R>
	list a(V&& value, R&&... rest) {
		list l;
		l.pushVar(var(std::forward<V>(value)));
		if constexpr (sizeof...(rest) > 0) {
			auto more = a(std::forward<R>(rest)...);
			for (auto it = more.begin(); it != more.end(); ++it)
				l.pushVar(*it);
		}
		return l;
	}

	// ------------------------------------------------------------------
	// template strings
	// ------------------------------------------------------------------

	namespace detail {
		inline string fmtOne(var v) { return (string)v; }
	}  // namespace detail

	/** `t("Hello $0, you have $1", name, count)` — JS template literals. */
	template <typename... A>
	string t(const char* format, A&&... args) {
		std::vector<string> vals = {
			detail::fmtOne(var(std::forward<A>(args)))...};
		string out;
		string f = format;
		for (size_t i = 0; i < f.size(); ++i) {
			if (f[i] == '$' && i + 1 < f.size() &&
				isdigit((unsigned char)f[i + 1])) {
				size_t j = i + 1;
				size_t n = 0;
				while (j < f.size() && isdigit((unsigned char)f[j])) {
					n = n * 10 + size_t(f[j] - '0');
					++j;
				}
				if (n < vals.size()) out += vals[n];
				else { out += f.substr(i, j - i); }
				i = j - 1;
			} else {
				out += f[i];
			}
		}
		return out;
	}

	// ------------------------------------------------------------------
	// array helpers (JS-style)
	// ------------------------------------------------------------------

	/** Call func(element) for each element. */
	inline void each(list li, const func& fn) {
		for (auto it = li.begin(); it != li.end(); ++it)
			fn({*it});
	}

	/** Return a new list of func(element) results. */
	inline list mapArr(list li, const func& fn) {
		list out;
		for (auto it = li.begin(); it != li.end(); ++it)
			out.pushVar(fn({*it}));
		return out;
	}

	/** Return elements for which func(element) is truthy. */
	inline list filter(list li, const func& fn) {
		list out;
		for (auto it = li.begin(); it != li.end(); ++it)
			if (fn({*it}).getBool()) out.pushVar(*it);
		return out;
	}

	/** First element for which func(element) is truthy, else empty var. */
	inline var findArr(list li, const func& fn) {
		for (auto it = li.begin(); it != li.end(); ++it)
			if (fn({*it}).getBool()) return *it;
		return var();
	}

	/** Concatenate elements as strings with `sep`. */
	inline string join(list li, string_view sep = ",") {
		string out;
		bool first = true;
		for (auto it = li.begin(); it != li.end(); ++it) {
			if (!first) out += sep;
			out += (string)(*it);
			first = false;
		}
		return out;
	}

	// ------------------------------------------------------------------
	// JSON shorthand
	// ------------------------------------------------------------------

	/** JSON.stringify */
	inline string toJSON(var value, bool pretty = false) {
		return jsonStringify(value, pretty);
	}
	/** JSON.parse */
	inline var fromJSON(string_view data) { return jsonParse(data); }

}  // namespace gold