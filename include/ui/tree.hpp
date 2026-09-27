#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"
#include "web/html.hpp"

namespace gold {
	namespace UI {

		using std::string;
		using std::vector;

		/**
		 * Interactive state for one element. Driven by the host (pointer
		 * events, game code) rather than by the stylesheet, and read by the
		 * style matcher for `:hover` / `:active` / `:focus` / `:checked`.
		 */
		struct nodeState {
			bool hover = false;
			bool active = false;
			bool focus = false;
			bool checked = false;
			bool disabled = false;
			bool open = false;
		};

		/**
		 * One entry of the flattened DOM.
		 *
		 * The renderer flattens the gold object tree (`HTML::iHTML`) into a
		 * vector once, then reuses it for style resolution, layout, painting
		 * and hit testing. `id` is stable across frames so game code and
		 * event handlers can name an element without holding pointers.
		 */
		struct domNode {
			uint32_t id = 0;
			/**
			 * The source element, held by value. Gold objects are
			 * reference-counted, so this shares storage with the parsed DOM:
			 * reading `el` sees later mutations, and writing through it
			 * updates the tree the caller passed in.
			 */
			object el;
			string tag;      // lowercase; empty on text nodes
			object attr;     // attribute object
			string text;     // text-node content
			bool isText = false;
			int parent = -1;
			vector<int> children;

			// Sibling bookkeeping counts element children only, so text
			// nodes never break `:first-child` or `+` / `~` combinators.
			int index = 0;
			int typeIndex = 0;
			int prev = -1;
			int next = -1;

			nodeState state;

			// gold's object accessors are non-const, so read through a shared
			// copy (an `object` copy shares storage, it is not a deep copy).
			string attrString(const string& name,
				const string& def = "") const {
				object copy = attr;
				var value = copy.getVar(name);
				// Numeric and boolean attributes come back as their var form:
				// `object::getString` returns the default for those.
				if (value.getType() == typeNull) return def;
				return value.getString();
			}
			bool hasAttr(const string& name) const {
				object copy = attr;
				return copy.getType(name) != typeNull;
			}
			string classList() const { return attrString("class"); }
			bool hasClass(const string& cls) const;
			/** True when the node has no element children and no text. */
			bool isEmpty() const;
		};

		/** The flattened document plus lookup helpers. */
		struct domTree {
			vector<domNode> nodes;
			uint32_t nextId = 1;

			int byId(uint32_t id) const;
			int byCSSId(const string& cssId) const;
			int byTag(const string& tag) const;
			/** Top-level nodes, in document order. */
			vector<int> roots() const;
			/** Depth-first walk; `fn` receives each element node index. */
			template <typename F>
			void walk(F fn) const {
				for (int r : roots()) walkNode(r, fn);
			}
			/** Re-serialize the tree back to an HTML string. */
			string toHTML() const;
			/** Recompute sibling indices after the DOM was mutated. */
			void reindex();
			/** True when any node (or an ancestor) changed shape. */
			uint32_t fingerprint() const;

		 private:
			template <typename F>
			void walkNode(int index, F& fn) const {
				if (index < 0) return;
				fn(index);
				for (int c : nodes[(size_t)index].children) walkNode(c, fn);
			}
		};

		/** Build a tree from parsed elements (`Parser::parseHTML` output). */
		domTree buildTree(const list& elements);
		/** Build a tree straight from an HTML string. */
		domTree buildTree(const string& html);

	}  // namespace UI
}  // namespace gold
