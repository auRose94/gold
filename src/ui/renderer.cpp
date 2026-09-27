#include "ui/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>

#include "goldjs.hpp"

namespace gold {
	namespace UI {

		namespace {

			float nowMs() {
				return (float)std::clock() / (float)CLOCKS_PER_SEC * 1000.0f;
			}

			string lowerStr(string s) {
				for (auto& c : s) c = (char)tolower((unsigned char)c);
				return s;
			}

			float argFloat(list args, size_t index, float def = 0.0f) {
				if (index >= args.size()) return def;
				return (float)args[index].getDouble();
			}

			string argString(list args, size_t index,
				const string& def = "") {
				if (index >= args.size()) return def;
				return args[index].getString();
			}

			bool isEventState(const string& name) {
				return name == "hover" || name == "active" || name == "focus" ||
					   name == "checked" || name == "disabled" || name == "open";
			}

		}  // namespace

		// ------------------------------------------------------- prototype

		object& renderer::getPrototype() {
			static auto proto = obj({
				{"load", method(&renderer::load)},
				{"setHTML", method(&renderer::setHTML)},
				{"setCSS", method(&renderer::setCSS)},
				{"setViewport", method(&renderer::setViewport)},
				{"setFonts", method(&renderer::setFonts)},
				{"loadFont", method(&renderer::loadFont)},
				{"advance", method(&renderer::advance)},
				{"render", method(&renderer::render)},
				{"needsRender", method(&renderer::needsRender)},
				{"invalidate", method(&renderer::invalidate)},
				{"surface", method(&renderer::surface)},
				{"pixels", method(&renderer::pixels)},
				{"hit", method(&renderer::hit)},
				{"dispatch", method(&renderer::dispatch)},
				{"on", method(&renderer::on)},
				{"setState", method(&renderer::setState)},
				{"query", method(&renderer::query)},
				{"element", method(&renderer::element)},
				{"elementRect", method(&renderer::elementRect)},
				{"setText", method(&renderer::setText)},
				{"setStyle", method(&renderer::setStyle)},
				{"interactionState", method(&renderer::interactionState)},
			});
			return proto;
		}

		renderer::renderer() {
			setParent(getPrototype());
			// Transparent by default, like a browser canvas: a host that wants
			// an opaque panel sets `background` in `load`.
			pageBackground_ = {0.0f, 0.0f, 0.0f, 0.0f, false};
			styleCtx_.defaultFontSize = 16.0f;
			styleCtx_.viewportWidth = 640.0f;
			styleCtx_.viewportHeight = 480.0f;
			layoutCtx_.viewportWidth = 640.0f;
			layoutCtx_.viewportHeight = 480.0f;
			layoutCtx_.fonts = &fonts_;
			target_.resize(640, 480);
		}

		renderer::renderer(list args) : renderer() {
			load(args);
		}

		renderer::~renderer() = default;

		// ------------------------------------------------------- lifecycle

		void renderer::markStyleDirty() {			styleDirty_ = true;
			markLayoutDirty();
		}

		void renderer::markLayoutDirty() {
			layoutDirty_ = true;
			markPaintDirty();
		}

		void renderer::markPaintDirty() {
			paintDirty_ = true;
		}

		void renderer::refreshFingerprint() {
			fingerprint_ = tree_.fingerprint();
		}

		var renderer::load(list args) {
			object config;
			if (args.size() == 1 && args[0].isObject()) config = args[0].getObject();
			else if (args.size() > 0) config = object({{"html", args[0]}});

			if (config) {
				if (config.getType("html") != typeNull)
					html_ = config.getString("html");
				if (config.getType("css") != typeNull)
					css_ = config.getString("css");
				if (config.getType("width") != typeNull) {
					const float w = (float)config.getDouble("width");
					styleCtx_.viewportWidth = w;
					layoutCtx_.viewportWidth = w;
					target_.resize((uint32_t)std::max(1.0f, w),
						target_.height ? target_.height : 480);
				}
				if (config.getType("height") != typeNull) {
					const float h = (float)config.getDouble("height");
					styleCtx_.viewportHeight = h;
					layoutCtx_.viewportHeight = h;
					target_.resize(target_.width ? target_.width : 640,
						(uint32_t)std::max(1.0f, h));
				}
				if (config.getType("background") != typeNull)
					parseColor(config.getString("background"), pageBackground_);
			}
			setFonts(list({}));
			setHTML(list({html_}));
			setCSS(list({css_}));
			return var(this);
		}

		var renderer::setHTML(list args) {
			html_ = argString(args, 0, html_);
			tree_ = buildTree(html_);
			// The first element acts as the page root: a bare fragment should
			// still fill the viewport.
			hoverNode_ = activeNode_ = focusNode_ = -1;
			refreshFingerprint();
			markStyleDirty();
			return var(this);
		}

		var renderer::setCSS(list args) {
			css_ = argString(args, 0, css_);
			sheet_ = parseStylesheet(css_);
			markStyleDirty();
			return var(this);
		}

		var renderer::setViewport(list args) {
			float w = argFloat(args, 0, styleCtx_.viewportWidth);
			float h = argFloat(args, 1, styleCtx_.viewportHeight);
			if (args.size() == 1 && args[0].isObject()) {
				object config = args[0].getObject();
				if (config.getType("width") != typeNull)
					w = (float)config.getDouble("width");
				if (config.getType("height") != typeNull)
					h = (float)config.getDouble("height");
			}
			styleCtx_.viewportWidth = w;
			styleCtx_.viewportHeight = h;
			layoutCtx_.viewportWidth = w;
			layoutCtx_.viewportHeight = h;
			target_.resize((uint32_t)std::max(1.0f, std::floor(w)),
				(uint32_t)std::max(1.0f, std::floor(h)));
			markStyleDirty();
			return var(this);
		}

		var renderer::setFonts(list args) {
			string sans, serif, mono;
			if (args.size() == 1 && args[0].isObject()) {
				object config = args[0].getObject();
				sans = config.getString("sans");
				serif = config.getString("serif");
				mono = config.getString("mono");
			} else {
				sans = argString(args, 0);
				serif = argString(args, 1);
				mono = argString(args, 2);
			}
			if (!sans.empty() || !serif.empty() || !mono.empty())
				fonts_.setGenericFonts(sans, serif, mono);
			markStyleDirty();
			return var(this);
		}

		var renderer::loadFont(list args) {
			const string path = argString(args, 0);
			const string family = argString(args, 1, "sans-serif");
			const int weight = (int)argFloat(args, 2, 400.0f);
			const bool italic = args.size() > 3 && args[3].getBool();
			if (path.empty()) return false;
			return fonts_.loadFile(path, family, weight, italic);
		}

		var renderer::advance(list args) {
			// No animations run yet, so time alone never dirties the tree; the
			// hook is here so a game loop can call it unconditionally.
			(void)args;
			return dirty();
		}

		var renderer::invalidate(list args) {
			const string stage = argString(args, 0, "all");
			if (stage == "style" || stage == "all") styleDirty_ = true;
			if (stage == "layout" || stage == "all") layoutDirty_ = true;
			if (stage == "paint" || stage == "all") paintDirty_ = true;
			return dirty();
		}

		var renderer::needsRender(list args) {
			(void)args;
			return dirty();
		}

		var renderer::render(list args) {
			(void)args;
			if (!dirty()) return false;

			// A game may have mutated the gold DOM behind our back.
			const uint32_t current = tree_.fingerprint();
			if (current != fingerprint_) {
				refreshFingerprint();
				styleDirty_ = true;
			}
			if (styleDirty_) {
				styleDirty_ = false;
				layoutDirty_ = true;
				styles_ = resolveStyles(tree_, sheet_, styleCtx_);
				stats_.stylePasses++;
			}
			if (layoutDirty_) {
				layoutDirty_ = false;
				paintDirty_ = true;
				layoutCtx_.rootFontSize = styles_.empty()
					? styleCtx_.defaultFontSize
					: fontSizeOf(styles_[0], styleCtx_.defaultFontSize,
						  styleCtx_.defaultFontSize, layoutCtx_);
				const float t0 = nowMs();
				layoutTree(tree_, styles_, layoutCtx_, layout_);
				stats_.layoutMs = nowMs() - t0;
				stats_.layoutPasses++;
			}
			if (paintDirty_) {
				paintDirty_ = false;
				const float t0 = nowMs();
				paintPage();
				stats_.paintMs = nowMs() - t0;
				stats_.paints++;
			}
			stats_.nodeCount = (uint32_t)tree_.nodes.size();
			stats_.runCount = 0;
			for (const layoutBox& b : layout_.boxes)
				stats_.runCount += (uint32_t)b.runs.size();
			return true;
		}

		// ------------------------------------------------------- painting

		void renderer::paintBox(rasterizer& raster, int node,
			float inheritedOpacity) {
			const layoutBox& b = layout_.boxes[(size_t)node];
			if (!b.visible) return;
			const computedStyle& s = styles_[(size_t)node];
			// Element opacity multiplies into the colors it paints; nested
			// groups are not isolated into a separate buffer.
			const float alpha = inheritedOpacity *
								std::max(0.0f, std::min(1.0f, s.opacity));
			if (alpha <= 0.0f) return;

			const float radius = resolveCornerRadius(s.borderRadius, b.border);

			// Boxes are painted flat, so the clip of every clipping ancestor
			// is applied here rather than by a recursive walk.
			const bool clipped = b.clipped || hasClippedAncestor(node);
			if (clipped) {
				rect clip = {0.0f, 0.0f, (float)target_.width,
					(float)target_.height};
				for (int p = node; p >= 0; p = tree_.nodes[(size_t)p].parent) {
					if (!layout_.boxes[(size_t)p].clipped) continue;
					const rect& c = layout_.boxes[(size_t)p].content;
					clip.x = std::max(clip.x, c.x);
					clip.y = std::max(clip.y, c.y);
					clip.w = std::min(clip.right(), c.right()) - clip.x;
					clip.h = std::min(clip.bottom(), c.bottom()) - clip.y;
					clip.w = std::max(clip.w, 0.0f);
					clip.h = std::max(clip.h, 0.0f);
				}
				raster.pushClip(clip);
			}
			if (b.border.w > 0.0f && b.border.h > 0.0f) {
				computedStyle tinted = s;
				if (alpha < 1.0f) {
					tinted.backgroundColor.a *= alpha;
					for (int i = 0; i < 4; i++) tinted.borderColor[i].a *= alpha;
					for (auto& stop : tinted.backgroundImage.stops)
						stop.c.a *= alpha;
				}
				raster.fillBackground(b.border, radius, tinted, b.border);
				// `background-image: url(...)` is not parsed into a paint, so
				// only the gradient/solid path runs here.
			}

			float widths[4] = {0, 0, 0, 0};
			color colors[4];
			for (int i = 0; i < 4; i++) {
				widths[i] = resolve(s.borderWidth[i], b.border.w,
					layoutCtx_.defaultFontSize, layoutCtx_.rootFontSize,
					layoutCtx_);
				if (s.borderStyle[i] == borderStyleType::none) widths[i] = 0.0f;
				colors[i] = s.borderColor[i];
				colors[i].a *= alpha;
			}
			raster.strokeBorders(b.border, radius, widths, colors);

			if (b.hasMarker) {
				textRun marker = b.marker;
				marker.c.a *= alpha;
				raster.drawText(marker, fonts_);
			}
			for (const textRun& run : b.runs) {
				textRun tintedRun = run;
				tintedRun.c.a *= alpha;
				raster.drawText(tintedRun, fonts_);
			}
			if (clipped) raster.popClip();
		}

		bool renderer::hasClippedAncestor(int node) const {
			for (int p = tree_.nodes[(size_t)node].parent; p >= 0;
				 p = tree_.nodes[(size_t)p].parent)
				if (layout_.boxes[(size_t)p].clipped) return true;
			return false;
		}

		void renderer::paintPage() {
			rasterizer raster(target_);
			raster.clear(pageBackground_);
			raster.resetStats();

			// Painting order: static content in document order, then anything
			// positioned or z-indexed, lowest z first.
			vector<int> deferred;
			for (size_t i = 0; i < tree_.nodes.size(); i++) {
				if (tree_.nodes[i].isText) continue;
				if (styles_[i].display == displayType::none) continue;
				const computedStyle& s = styles_[i];
				if (s.position != positionType::staticPos || s.zIndex != 0) {
					deferred.push_back((int)i);
					continue;
				}
				paintBox(raster, (int)i, 1.0f);
			}
			std::stable_sort(deferred.begin(), deferred.end(),
				[this](int a, int b) {
					return styles_[(size_t)a].zIndex < styles_[(size_t)b].zIndex;
				});
			for (int node : deferred) paintBox(raster, node, 1.0f);

			stats_.pixelsPainted = raster.pixelsPainted();
		}

		// ------------------------------------------------------- reading

		var renderer::pixels(list args) {
			(void)args;
			return var(target_.pixels);
		}

		var renderer::surface(list args) {
			(void)args;
			return jo("width", (int64_t)target_.width, "height",
				(int64_t)target_.height, "pixels", target_.unpremultiply());
		}

		// ----------------------------------------------------- interaction

		int renderer::hitTest(float x, float y) const {
			int best = -1;
			int bestDepth = -1;
			// Deepest match wins, so children paint over parents and pick the
			// same way.
			for (size_t i = 0; i < tree_.nodes.size(); i++) {
				if (tree_.nodes[i].isText) continue;
				if (!layout_.boxes[i].visible) continue;
				const computedStyle& s = styles_[i];
				if (s.display == displayType::none || !s.pointerEvents) continue;
				if (!layout_.boxes[i].border.contains(x, y)) continue;
				// Skip elements clipped away by an overflow:hidden ancestor.
				bool clipped = false;
				for (int p = tree_.nodes[i].parent; p >= 0;
					 p = tree_.nodes[p].parent) {
					if (!layout_.boxes[(size_t)p].clipped) continue;
					if (!layout_.boxes[(size_t)p].content.contains(x, y))
						clipped = true;
				}
				if (clipped) continue;
				int depth = 0;
				for (int p = tree_.nodes[i].parent; p >= 0;
					 p = tree_.nodes[p].parent)
					depth++;
				if (depth >= bestDepth) {
					bestDepth = depth;
					best = (int)i;
				}
			}
			return best;
		}

		object renderer::describe(int node) const {
			if (node < 0) return object();
			const domNode& dn = tree_.nodes[(size_t)node];
			object attr = dn.attr;
			string text;
			for (int child : dn.children)
				if (tree_.nodes[(size_t)child].isText)
					text += tree_.nodes[(size_t)child].text;
			const layoutBox& b = layout_.boxes[(size_t)node];
			return jo("id", (int64_t)dn.id, "tag", dn.tag,
				"cssId", dn.attrString("id"), "class", dn.classList(),
				"text", text, "attrs", attr, "visible", b.visible,
				"rect", jo("x", b.border.x, "y", b.border.y, "width", b.border.w,
					"height", b.border.h),
				"content", jo("x", b.content.x, "y", b.content.y, "width",
					b.content.w, "height", b.content.h));
		}

		var renderer::hit(list args) {
			const float x = argFloat(args, 0);
			const float y = argFloat(args, 1);
			return describe(hitTest(x, y));
		}

		list renderer::fireEvent(const string& type, int node, const var& data,
			const object& target) {
			list results;
			if (node < 0) return results;
			// Bubble from the target up through its ancestors.
			for (int current = node; current >= 0;
				 current = tree_.nodes[(size_t)current].parent) {
				auto typeIt = handlers_.find(type);
				if (typeIt == handlers_.end()) break;
				const uint32_t id = tree_.nodes[(size_t)current].id;
				auto nodeIt = typeIt->second.find(id);
				if (nodeIt == typeIt->second.end()) continue;
				for (func& callback : nodeIt->second) {
					if (!callback) continue;
					list args;
					args.pushVar(var(target));
					if (data) args.pushVar(data);
					results.pushVar(callback(args));
				}
			}
			// Document-level handlers see every event.
			auto typeIt = handlers_.find(type);
			if (typeIt != handlers_.end()) {
				auto globalIt = typeIt->second.find(0);
				if (globalIt != typeIt->second.end()) {
					for (func& callback : globalIt->second) {
						if (!callback) continue;
						list args;
						args.pushVar(var(target));
						if (data) args.pushVar(data);
						results.pushVar(callback(args));
					}
				}
			}
			return results;
		}

		void renderer::syncHover(int node) {
			if (node == hoverNode_) return;
			if (hoverNode_ >= 0)
				tree_.nodes[(size_t)hoverNode_].state.hover = false;
			hoverNode_ = node;
			if (hoverNode_ >= 0)
				tree_.nodes[(size_t)hoverNode_].state.hover = true;
			if (node >= 0) markStyleDirty();
			else markStyleDirty();
		}

		var renderer::dispatch(list args) {
			const string type = argString(args, 0, "click");
			const float x = argFloat(args, 1, -1.0f);
			const float y = argFloat(args, 2, -1.0f);
			var data;
			if (args.size() > 3) data = args[3];

			const int node = (x < 0.0f || y < 0.0f) ? hoverNode_ : hitTest(x, y);
			if (node < 0) {
				// Leaving the surface clears hover and cancels a press.
				if (hoverNode_ >= 0 || activeNode_ >= 0) {
					syncHover(-1);
					if (activeNode_ >= 0) {
						tree_.nodes[(size_t)activeNode_].state.active = false;
						activeNode_ = -1;
					}
				}
				return list();
			}

			if (type == "move" || type == "hover") {
				syncHover(node);
			} else if (type == "down" || type == "press") {
				syncHover(node);
				if (activeNode_ >= 0)
					tree_.nodes[(size_t)activeNode_].state.active = false;
				activeNode_ = node;
				tree_.nodes[(size_t)node].state.active = true;
				markStyleDirty();
			} else if (type == "up" || type == "release") {
				// Releasing over the pressed element is a click; releasing
				// anywhere else cancels it, like a real pointer.
				bool sameTarget = false;
				if (activeNode_ >= 0) {
					sameTarget = activeNode_ == node;
					tree_.nodes[(size_t)activeNode_].state.active = false;
					activeNode_ = -1;
					markStyleDirty();
				}
				if (!sameTarget) return list();
			} else if (type == "leave" || type == "cancel") {
				syncHover(-1);
				if (activeNode_ >= 0) {
					tree_.nodes[(size_t)activeNode_].state.active = false;
					activeNode_ = -1;
					markStyleDirty();
				}
				return list();
			}
			// "up" completes a press, so it reports a "click".
			const string fired = (type == "up" || type == "release") ? "click"
																	 : type;
			return fireEvent(fired, node, data, describe(node));
		}

		var renderer::on(list args) {
			if (args.size() == 0) return var(this);
			const string type = lowerStr(argString(args, 0));
			uint32_t id = 0;
			size_t funcIndex = 1;
			// ("click", func) listens everywhere; ("click", id, func) only
			// fires for that element.
			if (args.size() >= 3 && !args[1].isFunction()) {
				id = (uint32_t)args[1].getInt64();
				funcIndex = 2;
			}
			if (funcIndex >= args.size()) return var(this);
			func callback = args[funcIndex].getFunction();
			if (!callback) return var(this);
			handlers_[type][id].push_back(callback);
			return var(this);
		}

		var renderer::setState(list args) {
			if (args.size() < 2) return var(this);
			const int id = (int)args[0].getInt64();
			const int node = tree_.byId((uint32_t)id);
			if (node < 0) return var(this);
			const string state = lowerStr(argString(args, 1));
			const bool value = args.size() < 3 ? true : args[2].getBool();
			if (!isEventState(state)) return var(this);
			if (state == "hover") {
				hoverNode_ = value ? node : -1;
				tree_.nodes[(size_t)node].state.hover = value;
			} else if (state == "active") {
				activeNode_ = value ? node : -1;
				tree_.nodes[(size_t)node].state.active = value;
			} else if (state == "focus") {
				if (focusNode_ >= 0)
					tree_.nodes[(size_t)focusNode_].state.focus = false;
				focusNode_ = value ? node : -1;
				tree_.nodes[(size_t)node].state.focus = value;
			} else if (state == "checked") {
				tree_.nodes[(size_t)node].state.checked = value;
			} else if (state == "disabled") {
				tree_.nodes[(size_t)node].state.disabled = value;
			} else if (state == "open") {
				tree_.nodes[(size_t)node].state.open = value;
			}
			markStyleDirty();
			return var(this);
		}

		var renderer::query(list args) {
			const string selectorText = argString(args, 0);
			list out;
			if (selectorText.empty()) return out;
			for (const complexSelector& sel : parseSelectorList(selectorText)) {
				for (size_t i = 0; i < tree_.nodes.size(); i++) {
					if (tree_.nodes[i].isText) continue;
					if (!selectorMatches(sel, tree_, (int)i)) continue;
					out.pushVar(var(describe((int)i)));
				}
			}
			return out;
		}

		var renderer::element(list args) {
			const int node = tree_.byId((uint32_t)argFloat(args, 0));
			return describe(node);
		}

		var renderer::elementRect(list args) {
			const int node = tree_.byId((uint32_t)argFloat(args, 0));
			if (node < 0) return var();
			const layoutBox& b = layout_.boxes[(size_t)node];
			return jo("x", b.border.x, "y", b.border.y, "width", b.border.w,
				"height", b.border.h);
		}

		var renderer::setText(list args) {
			const int node = tree_.byId((uint32_t)argFloat(args, 0));
			if (node < 0) return var(this);
			const string text = argString(args, 1);
			// Update the gold element and the flattened tree together, so the
			// stable element ids survive a text update.
			object el = tree_.nodes[(size_t)node].el;
			if (el) el.setList("items", list({var(text)}));
			auto& kids = tree_.nodes[(size_t)node].children;
			for (int child : kids) {
				if (!tree_.nodes[(size_t)child].isText) continue;
				tree_.nodes[(size_t)child].text = text;
			}
			if (kids.empty() && !text.empty()) {
				domNode textNode;
				textNode.id = tree_.nextId++;
				textNode.isText = true;
				textNode.text = text;
				textNode.parent = node;
				tree_.nodes.push_back(textNode);
				tree_.nodes[(size_t)node].children.push_back(
					(int)tree_.nodes.size() - 1);
			}
			refreshFingerprint();
			markStyleDirty();
			return var(this);
		}

		var renderer::setStyle(list args) {
			const int node = tree_.byId((uint32_t)argFloat(args, 0));
			if (node < 0) return var(this);
			if (args.size() < 2 || !args[1].isObject()) return var(this);
			object declarations = args[1].getObject();
			// `style` lives in the attribute object, which the element shares.
			object attr = tree_.nodes[(size_t)node].attr;
			string styleAttr = attr.getString("style");
			for (auto it = declarations.begin(); it != declarations.end(); ++it) {
				if (!styleAttr.empty()) styleAttr += ";";
				styleAttr += it->first + ":" + it->second.getString();
			}
			attr.setString("style", styleAttr);
			refreshFingerprint();
			markStyleDirty();
			return var(this);
		}

		var renderer::interactionState(list args) {
			(void)args;
			auto idOf = [this](int node) -> int64_t {
				return node < 0 ? (int64_t)-1
								: (int64_t)tree_.nodes[(size_t)node].id;
			};
			return jo("hover", idOf(hoverNode_), "active", idOf(activeNode_),
				"focus", idOf(focusNode_));
		}

	}  // namespace UI
}  // namespace gold
