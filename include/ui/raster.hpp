#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "font.hpp"
#include "layout.hpp"
#include "style.hpp"
#include "types.hpp"

namespace gold {
	namespace UI {

		using std::string;
		using std::vector;

		/**
		 * An RGBA8 pixel buffer.
		 *
		 * Pixels are stored premultiplied, which makes "over" compositing a
		 * three-term multiply-add per channel. `unpremultiply()` produces
		 * straight alpha for callers that need it (PNG export, for example).
		 */
		struct rasterTarget {
			uint32_t width = 0;
			uint32_t height = 0;
			vector<uint8_t> pixels;  // RGBA8, premultiplied, top-down

			bool resize(uint32_t w, uint32_t h);
			void clear(const color& c);
			uint8_t* row(int y) {
				return pixels.data() + (size_t)y * (size_t)width * 4;
			}
			/** Straight-alpha RGBA8 copy, for texture upload or export. */
			binary unpremultiply() const;
		};

		/** Decoded image pixels an `imageProvider` hands back. */
		struct imageData {
			uint32_t width = 0;
			uint32_t height = 0;
			vector<uint8_t> rgba;  // RGBA8, straight alpha
		};

		/**
		 * Supplies decoded images for `url(...)` backgrounds and `<img>`.
		 * The renderer owns one; a default that decodes nothing is used when
		 * the host does not provide one.
		 */
		class imageProvider {
		 public:
			virtual ~imageProvider() = default;
			/** Decoded pixels for `url`, or nullptr when unavailable. */
			virtual const imageData* get(const string& url) = 0;
		};

		/**
		 * Resolve CSS corner radii against a box. Per-corner radii collapse to
		 * the largest one, which the rounded-box distance field can express.
		 */
		float resolveCornerRadius(const length radii[4], const rect& r);

		/**
		 * Anti-aliased software rasterizer.
		 *
		 * Coverage comes from signed distance fields of rounded rectangles,
		 * so one code path handles plain rects, rounded corners, borders and
		 * gradients, and edges stay smooth at any radius.
		 */
		class rasterizer {
			 public:
			explicit rasterizer(rasterTarget& target) : target_(target) {}

			void clear(const color& c);
			/** Intersect the clip with `r`. */
			void pushClip(const rect& r);
			void popClip();
			const rect& clip() const { return clip_; }

			void fillRect(const rect& r, const color& c);
			/** `radius` is a single value; per-corner radii collapse to max. */
			void fillRoundedRect(const rect& r, float radius, const color& c);
			/** Paint a background: solid color and/or a gradient. */
			void fillBackground(const rect& r, float radius,
				const computedStyle& style, const rect& paintArea);
			/** Draw the border band, one color and width per side. */
			void strokeBorders(const rect& r, float radius,
				const float widths[4], const color colors[4]);
			/** Draw `text` with its baseline at (x, baselineY). */
			void drawText(const textRun& run, fontManager& fonts);
			/** Blit an image, honouring background-size cover/contain. */
			void drawImage(const rect& r, float radius, const imageData& image,
				bgSizeType sizeMode, const color& tint);
			/** Debug hook: paint the box tree's outlines. */
			void strokeRect(const rect& r, const color& c, float thickness = 1.0f);

			/** Coverage-weighted blend of one pixel. */
			void blend(int x, int y, float coverage, float r, float g, float b,
				float a);
			/** Pixels touched since the last `resetStats()`. */
			uint64_t pixelsPainted() const { return painted_; }
			void resetStats() { painted_ = 0; }

			 private:
			/** Signed distance to a rounded box centred on the origin. */
			static float sdRoundBox(float px, float py, float halfW,
				float halfH, float radius);
			void blendCoverage(int x, int y, float coverage, const color& c);
			/** Color at `t` (0..1) along a gradient. */
			color gradientColor(const backgroundPaint& paint, float t) const;

			rasterTarget& target_;
			vector<rect> clips_;
			rect clip_ = {0, 0, 1e9f, 1e9f};
			uint64_t painted_ = 0;
		};

	}  // namespace UI
}  // namespace gold
