#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "font.hpp"
#include "layout.hpp"
#include "raster.hpp"
#include "style.hpp"
#include "tree.hpp"
#include "types.hpp"

namespace gold {
	namespace UI {

		using std::string;
		using std::vector;

		/** Work done by the last `render()`, for tests and in-game debug HUDs. */
		struct frameStats {
			uint64_t stylePasses = 0;
			uint64_t layoutPasses = 0;
			uint64_t paints = 0;
			uint64_t pixelsPainted = 0;
			uint32_t nodeCount = 0;
			uint32_t runCount = 0;
			float layoutMs = 0.0f;
			float paintMs = 0.0f;
		};

		/**
		 * The renderer.
		 *
		 * HTML and CSS go in, an RGBA8 image comes out, and pointer events
		 * go in and gold callbacks come out. It is a gold object, so a
		 * `gold::lang` script, a game component or plain C++ can all drive
		 * the same instance.
		 *
		 * Work is staged and skipped when nothing changed: `advance()`
		 * reports whether a repaint is needed and `render()` only rebuilds
		 * the stages that are dirty. A static UI costs one pass at load and
		 * then nothing per frame, which is what makes it cheap to keep a
		 * CRT screen on a desk up to date.
		 *
		 * ```cpp
		 * gold::UI::renderer ui;
		 * ui.load(list({jo("html", markup), jo("css", sheet),
		 *     jo("width", 512.0), jo("height", 384.0)}));
		 * ui.on(list({"click", func([](list args) { ... })}));
		 * // per frame
		 * if (ui.needsRender()) { ui.render(); upload(ui.surface()); }
		 * // input
		 * ui.dispatch(list({"click", 100.0, 50.0}));
		 * ```
		 */
		struct renderer : public object {
			static object& getPrototype();

			renderer();
			renderer(list args);
			~renderer();

			// ------------------------------------------------- lifecycle
			/** args: {html?, css?, width?, height?, fonts?} */
			var load(list args);
			/** Replace the markup; resets element state. */
			var setHTML(list args);
			/** Replace the stylesheet. */
			var setCSS(list args);
			/** args: {width, height} — triggers a re-layout. */
			var setViewport(list args);
			/** args: {sans?, serif?, mono?} — font files for the CSS generics. */
			var setFonts(list args);
			/** args: {path, family, weight?, italic?} */
			var loadFont(list args);

			/** Advance time by `dt` seconds. Returns true when a repaint is due. */
			var advance(list args);
			/** Run the dirty stages. Returns true when pixels changed. */
			var render(list args);
			/** True when `render()` would change something. */
			var needsRender(list args);
			/** Force every stage to run on the next `render()`. */
			var invalidate(list args);

			// ---------------------------------------------------- reading
			/** {width, height, pixels} - straight-alpha RGBA8. */
			var surface(list args);
			/** Premultiplied RGBA8 pixels, zero-copy. */
			var pixels(list args);
			/** The pixel target itself, for zero-copy GPU uploads. */
			rasterTarget& target() { return target_; }
			const rasterTarget& target() const { return target_; }
			frameStats stats() const { return stats_; }

			// ------------------------------------------------ interaction
			/** Deepest element at (x, y), or -1. */
			int hitTest(float x, float y) const;
			/** args: {x, y} -> element descriptor, or null. */
			var hit(list args);
			/**
			 * args: {type, x, y, data?}. Updates hover/active state, then
			 * calls matching handlers from the target up through its
			 * ancestors. Returns a list of results.
			 */
			var dispatch(list args);
			/** args: {type, func} or {type, elementId, func}. */
			var on(list args);
			/** args: {elementId, "hover"|"active"|"focus", bool}. */
			var setState(list args);
			/** args: {selector} -> list of element descriptors. */
			var query(list args);
			/** args: {elementId} -> element descriptor, or null. */
			var element(list args);
			/** args: {elementId} -> {x, y, width, height} or null. */
			var elementRect(list args);
			/** args: {elementId, text} - replace an element's text content. */
			var setText(list args);
			/** args: {elementId, {prop: value}} - inline style overrides. */
			var setStyle(list args);
			/** Current hover / active / focus element ids. */
			var interactionState(list args);

			// ------------------------------------------- C++ convenience
			void setViewportSize(float w, float h) {
				setViewport(list({(double)w, (double)h}));
			}
			void setMarkup(const string& html) { setHTML(list({html})); }
			void setStyleSheet(const string& css) { setCSS(list({css})); }
			void step(float dt) { advance(list({(double)dt})); }
			bool paint() { return render(list()).getBool(); }
			bool dirty() const { return styleDirty_ || layoutDirty_ || paintDirty_; }
			/** Serialize the current document back to markup. */
			string markup() const { return tree_.toHTML(); }
			const domTree& dom() const { return tree_; }
			const layoutResult& boxes() const { return layout_; }
			/** Look up a node by the stable id handed out in descriptors. */
			int nodeById(uint32_t id) const { return tree_.byId(id); }

		 private:
			void markStyleDirty();
			void markLayoutDirty();
			void markPaintDirty();
			void refreshFingerprint();
			void paintPage();
			/** Paint one box: background, borders, list marker, text runs. */
			void paintBox(rasterizer& raster, int node, float inheritedOpacity);
			/** True when any ancestor clips its content box. */
			bool hasClippedAncestor(int node) const;
			/** Fill in a descriptor object for one node. */
			object describe(int node) const;
			/** Call handlers for `type` on `node` and its ancestors. */
			list fireEvent(const string& type, int node, const var& data,
				const object& target);
			void syncHover(int node);

			domTree tree_;
			stylesheet sheet_;
			styleContext styleCtx_;
			vector<computedStyle> styles_;
			layoutResult layout_;
			layoutContext layoutCtx_;
			rasterTarget target_;
			fontManager fonts_;
			frameStats stats_;

			bool styleDirty_ = true;
			bool layoutDirty_ = true;
			bool paintDirty_ = true;
			uint32_t fingerprint_ = 0;
			string html_;
			string css_;
			color pageBackground_;

			int hoverNode_ = -1;
			int activeNode_ = -1;
			int focusNode_ = -1;

			// Handlers: event type -> (element id or 0 -> callbacks).
			std::map<string, std::map<uint32_t, vector<func>>> handlers_;
		};

	}  // namespace UI
}  // namespace gold
