#include "ui/tree.hpp"

#include <algorithm>
#include <cctype>

namespace gold {
	namespace UI {

		namespace {
			bool isSpaceChar(char c) {
				return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
					   c == '\f' || c == '\v';
			}

			string lower(string s) {
				for (auto& c : s) c = (char)tolower((unsigned char)c);
				return s;
			}

			string trimStr(const string& s) {
				size_t start = s.find_first_not_of(" \t\n\r\f\v");
				if (start == string::npos) return "";
				size_t end = s.find_last_not_of(" \t\n\r\f\v");
				return s.substr(start, end - start + 1);
			}
		}  // namespace

		bool domNode::hasClass(const string& cls) const {
			const string classes = classList();
			size_t pos = 0;
			while (pos < classes.size()) {
				size_t end = classes.find_first_of(" \t\n\r", pos);
				if (end == string::npos) end = classes.size();
				if (classes.compare(pos, end - pos, cls) == 0) return true;
				pos = end + 1;
			}
			return false;
		}

		bool domNode::isEmpty() const {
			if (!children.empty()) return false;
			for (char c : text)
				if (!isSpaceChar(c)) return false;
			return true;
		}

		int domTree::byId(uint32_t id) const {
			for (size_t i = 0; i < nodes.size(); i++)
				if (nodes[i].id == id) return (int)i;
			return -1;
		}

		int domTree::byCSSId(const string& cssId) const {
			for (size_t i = 0; i < nodes.size(); i++) {
				if (nodes[i].isText) continue;
				if (nodes[i].attrString("id") == cssId) return (int)i;
			}
			return -1;
		}

		int domTree::byTag(const string& tag) const {
			for (size_t i = 0; i < nodes.size(); i++) {
				if (nodes[i].isText) continue;
				if (nodes[i].tag == tag) return (int)i;
			}
			return -1;
		}

		vector<int> domTree::roots() const {
			vector<int> out;
			for (size_t i = 0; i < nodes.size(); i++)
				if (nodes[i].parent < 0) out.push_back((int)i);
			return out;
		}

		void domTree::reindex() {
			for (auto& node : nodes) {
				node.index = 0;
				node.typeIndex = 0;
				node.prev = -1;
				node.next = -1;
			}
			// Sibling indices skip text nodes: `li:nth-child(2)` counts
			// elements, not whitespace between them.
			for (auto& node : nodes) {
				int elementIndex = 0;
				int typeIndex = 0;
				int last = -1;
				for (int c : node.children) {
					auto& child = nodes[(size_t)c];
					if (child.isText) continue;
					child.index = elementIndex++;
					if (child.tag == node.tag) child.typeIndex = typeIndex++;
					child.prev = last;
					if (last >= 0) nodes[(size_t)last].next = c;
					last = c;
				}
			}
		}

		namespace {
			/** Serialize one node (and its subtree) back to markup. */
			void writeHTML(const vector<domNode>& nodes, int index,
				string& out) {
				if (index < 0) return;
				const auto& node = nodes[(size_t)index];
				if (node.isText) {
					out += node.text;
					return;
				}
				const bool voidTag = Parser::isVoidTag(node.tag);
				out += "<" + node.tag;
				object attr = node.attr;
				for (auto it = attr.begin(); it != attr.end(); ++it) {
					if (it->second.isBool()) {
						if (it->second.getBool()) out += " " + it->first;
						continue;
					}
					out += " " + it->first + "=\"" + it->second.getString() +
						   "\"";
				}
				out += ">";
				if (voidTag) return;
				for (int c : node.children) writeHTML(nodes, c, out);
				out += "</" + node.tag + ">";
			}
		}  // namespace

		string domTree::toHTML() const {
			string out;
			for (int r : roots()) writeHTML(nodes, r, out);
			return out;
		}

		uint32_t domTree::fingerprint() const {
			// Cheap structural hash: shape + ids, used to notice that a
			// game-side DOM mutation invalidates style and layout.
			uint32_t h = 2166136261u;
			auto mix = [&h](uint32_t v) {
				h ^= v;
				h *= 16777619u;
			};
			mix((uint32_t)nodes.size());
			for (const auto& node : nodes) {
				mix(node.id);
				mix((uint32_t)node.children.size());
				for (char c : node.tag) mix((uint32_t)(uint8_t)c);
				for (char c : node.text) mix((uint32_t)(uint8_t)c);
			}
			return h;
		}

		domTree buildTree(const list& elements) {
			domTree tree;
			list elems = elements;

			// Register one element, append it to `parent`, return its index.
			auto addElement = [&tree](object el, int parent) {
				domNode node;
				node.id = tree.nextId++;
				node.el = el;
				node.tag = lower(el.getString("tag"));
				node.attr = el.getObject("attr");
				node.parent = parent;
				const int index = (int)tree.nodes.size();
				tree.nodes.push_back(node);
				if (parent >= 0)
					tree.nodes[(size_t)parent].children.push_back(index);
				return index;
			};

			// Depth-first walk. Each frame keeps its own `items` list, because
			// a nested element's children live in a different list than its
			// parent's.
			struct frame {
				int node;
				uint64_t next;
				uint64_t count;
				list items;
			};
			vector<frame> open;
			for (uint64_t i = 0; i < elems.size(); i++) {
				var item = elems.getVar(i);
				if (!item.isObject()) continue;
				const int self = addElement(item.getObject(), -1);
				list items = item.getObject().getList("items");
				open.push_back({self, 0, items.size(), items});
				while (!open.empty()) {
					frame& f = open.back();
					if (f.next >= f.count) {
						open.pop_back();
						continue;
					}
					var child = f.items.getVar(f.next++);
					if (child.isString()) {
						domNode textNode;
						textNode.id = tree.nextId++;
						textNode.isText = true;
						textNode.text = child.getString();
						textNode.parent = f.node;
						const int index = (int)tree.nodes.size();
						tree.nodes.push_back(textNode);
						tree.nodes[(size_t)f.node].children.push_back(index);
						continue;
					}
					if (!child.isObject()) continue;
					const int index = addElement(child.getObject(), f.node);
					if (Parser::isVoidTag(tree.nodes[(size_t)index].tag))
						continue;
					if (open.size() > 256) continue;  // runaway guard
					list childItems = child.getObject().getList("items");
					open.push_back(
						{index, 0, childItems.size(), childItems});
				}
			}

			tree.reindex();
			return tree;
		}

		domTree buildTree(const string& html) {
			return buildTree(Parser::parseHTML(html));
		}

	}  // namespace UI
}  // namespace gold
