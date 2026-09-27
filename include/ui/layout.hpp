#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "font.hpp"
#include "style.hpp"
#include "tree.hpp"

namespace gold {
	namespace UI {

		using std::string;
		using std::vector;

		/** An axis-aligned rectangle in viewport pixels. */
		struct rect {
			float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

			float right() const { return x + w; }
			float bottom() const { return y + h; }
			bool empty() const { return w <= 0.0f || h <= 0.0f; }
			bool contains(float px, float py) const {
				return px >= x && px < x + w && py >= y && py < y + h;
			}
			bool intersects(const rect& o) const {
				return !(o.x >= right() || o.right() <= x || o.y >= bottom() ||
						 o.bottom() <= y);
			}
			rect inset(float top, float right, float bottom,
				float left) const {
				return {x + left, y + top, w - left - right,
					h - top - bottom};
			}
		};

		/** One laid-out run of text; `y` is the baseline. */
		struct textRun {
			int node = -1;  // element that owns the text (for events)
			float x = 0.0f;
			float y = 0.0f;  // baseline
			float width = 0.0f;
			string text;
			color c;
			string family;
			float size = 16.0f;
			int weight = 400;
			bool italic = false;
			float letterSpacing = 0.0f;
			bool underline = false;
			bool lineThrough = false;
		};

		/** A line box produced by an inline formatting context. */
		struct lineBox {
			float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
			float baseline = 0.0f;
		};

		/**
		 * One laid-out element.
		 *
		 * Box indices match DOM node indices one-to-one, so painting,
		 * hit testing and event dispatch never need a node -> box lookup.
		 */
		struct layoutBox {
			int node = -1;
			int parent = -1;
			vector<int> children;

			rect border;   // border box, absolute in the viewport
			rect content;  // content box (inside padding)

			bool visible = true;
			bool isTextNode = false;
			int zIndex = 0;
			float opacity = 1.0f;

			/** Runs and line boxes owned by this block container. */
			vector<textRun> runs;
			vector<lineBox> lines;
			/** `<li>` marker, painted at the content edge. */
			bool hasMarker = false;
			textRun marker;
			/** Set when a block child overflowed its container. */
			bool clipped = false;
		};

		/** Everything one layout pass produced. */
		struct layoutResult {
			vector<layoutBox> boxes;
			float width = 0.0f;
			float height = 0.0f;
			/** Bumped on every pass so callers can detect a real change. */
			uint64_t generation = 0;
		};

		/** Inputs a layout pass needs beyond the tree and the styles. */
		struct layoutContext {
			float viewportWidth = 0.0f;
			float viewportHeight = 0.0f;
			float rootFontSize = 16.0f;
			/** Font size of a root element, backing `em` on <html>/<body>. */
			float defaultFontSize = 16.0f;
			/** Not owned; must outlive the pass. */
			fontManager* fonts = nullptr;
		};

		/**
		 * Lay the whole document out into `out`.
		 *
		 * One pass builds the box tree. `display: flex` and `display: grid`
		 * containers lay their own children out (see `layoutEngine`), so
		 * nested flex-in-grid and grid-in-flex work without a second pass.
		 */
		void layoutTree(const domTree& tree, const vector<computedStyle>& styles,
			const layoutContext& ctx, layoutResult& out);

		// --------------------------------------------------------- helpers

		/** Resolve a length against a percentage basis and font size. */
		float resolve(length l, float basis, float fontSize,
			float rootFontSize, const layoutContext& ctx);
		/** Resolved font size in px for one node. */
		float fontSizeOf(const computedStyle& style, float parentSize,
			float rootFontSize, const layoutContext& ctx);

	}  // namespace UI
}  // namespace gold
