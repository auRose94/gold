#include "ui/raster.hpp"

#include <algorithm>
#include <cmath>

namespace gold {
	namespace UI {

		namespace {
			uint8_t toByte(float v) {
				const float scaled = v * 255.0f + 0.5f;
				if (scaled <= 0.0f) return 0;
				if (scaled >= 255.0f) return 255;
				return (uint8_t)scaled;
			}

			float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
		}  // namespace

		// ---------------------------------------------------- rasterTarget

		bool rasterTarget::resize(uint32_t w, uint32_t h) {
			if (w == 0 || h == 0) {
				width = height = 0;
				pixels.clear();
				return false;
			}
			if (w == width && h == height && !pixels.empty()) return true;
			width = w;
			height = h;
			pixels.assign((size_t)w * (size_t)h * 4, 0);
			return true;
		}

		void rasterTarget::clear(const color& c) {
			const float a = clamp01(c.a);
			const uint8_t r = toByte(c.r * a);
			const uint8_t g = toByte(c.g * a);
			const uint8_t b = toByte(c.b * a);
			const uint8_t a8 = toByte(a);
			for (size_t i = 0; i < pixels.size(); i += 4) {
				pixels[i] = r;
				pixels[i + 1] = g;
				pixels[i + 2] = b;
				pixels[i + 3] = a8;
			}
		}

		binary rasterTarget::unpremultiply() const {
			binary out(pixels.size());
			for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
				const uint8_t a = pixels[i + 3];
				if (a == 0) {
					out[i] = out[i + 1] = out[i + 2] = 0;
					continue;
				}
				if (a == 255) {
					out[i] = pixels[i];
					out[i + 1] = pixels[i + 1];
					out[i + 2] = pixels[i + 2];
				} else {
					const float inv = 255.0f / (float)a;
					out[i] = toByte((float)pixels[i] * inv / 255.0f);
					out[i + 1] = toByte((float)pixels[i + 1] * inv / 255.0f);
					out[i + 2] = toByte((float)pixels[i + 2] * inv / 255.0f);
				}
				out[i + 3] = a;
			}
			return out;
		}

		float resolveCornerRadius(const length radii[4], const rect& r) {
			// Per-corner radii collapse to the largest one: the rounded-box
			// SDF has a single radius, and taking the max never clips a corner
			// the author asked to be rounded.
			float value = 0.0f;
			for (int i = 0; i < 4; i++) {
				if (!radii[i].defined()) continue;
				if (radii[i].isPercent())
					value = std::max(value,
						radii[i].value * 0.01f * std::min(r.w, r.h));
				else
					value = std::max(value, radii[i].value);
			}
			return std::min(value, std::min(r.w, r.h) * 0.5f);
		}

		// ------------------------------------------------------- rasterizer

		float rasterizer::sdRoundBox(float px, float py, float halfW,
			float halfH, float radius) {
			// Standard iq rounded-box distance, clamped to the radius.
			const float r = std::min(radius, std::min(halfW, halfH));
			const float qx = std::fabs(px) - halfW + r;
			const float qy = std::fabs(py) - halfH + r;
			const float outside =
				std::sqrt(std::max(qx, 0.0f) * std::max(qx, 0.0f) +
						  std::max(qy, 0.0f) * std::max(qy, 0.0f));
			const float inside = std::min(std::max(qx, qy), 0.0f);
			return outside + inside - r;
		}

		void rasterizer::clear(const color& c) {
			target_.clear(c);
		}

		void rasterizer::pushClip(const rect& r) {
			clips_.push_back(clip_);
			const float x0 = std::max(clip_.x, r.x);
			const float y0 = std::max(clip_.y, r.y);
			const float x1 = std::min(clip_.right(), r.right());
			const float y1 = std::min(clip_.bottom(), r.bottom());
			clip_ = {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
		}

		void rasterizer::popClip() {
			if (clips_.empty()) return;
			clip_ = clips_.back();
			clips_.pop_back();
		}

		void rasterizer::blend(int x, int y, float coverage, float r, float g,
			float b, float a) {
			if (x < 0 || y < 0 || (uint32_t)x >= target_.width ||
				(uint32_t)y >= target_.height)
				return;
			const float alpha = clamp01(coverage) * clamp01(a);
			if (alpha <= 0.0f) return;
			uint8_t* px = target_.row(y) + (size_t)x * 4;
			const float inv = 1.0f - alpha;
			// Source is premultiplied, so "over" is src + dst * (1 - a).
			px[0] = toByte(r * alpha + (px[0] / 255.0f) * inv);
			px[1] = toByte(g * alpha + (px[1] / 255.0f) * inv);
			px[2] = toByte(b * alpha + (px[2] / 255.0f) * inv);
			px[3] = toByte(alpha + (px[3] / 255.0f) * inv);
			painted_++;
		}

		void rasterizer::blendCoverage(int x, int y, float coverage,
			const color& c) {
			if (coverage <= 0.0f || c.transparent()) return;
			blend(x, y, coverage, c.r, c.g, c.b, c.a);
		}

		void rasterizer::fillRect(const rect& r, const color& c) {
			fillRoundedRect(r, 0.0f, c);
		}

		void rasterizer::fillRoundedRect(const rect& r, float radius,
			const color& c) {
			if (c.transparent() || r.empty()) return;
			const int x0 = (int)std::floor(std::max(r.x, clip_.x)) - 1;
			const int y0 = (int)std::floor(std::max(r.y, clip_.y)) - 1;
			const int x1 = (int)std::ceil(std::min(r.right(), clip_.right())) + 1;
			const int y1 = (int)std::ceil(std::min(r.bottom(), clip_.bottom())) + 1;
			const float cx = r.x + r.w * 0.5f;
			const float cy = r.y + r.h * 0.5f;
			const float hw = r.w * 0.5f;
			const float hh = r.h * 0.5f;
			const float rad = std::max(0.0f, std::min(radius, std::min(hw, hh)));
			// Skip the SDF when there is no rounding: cheaper per pixel.
			const bool square = rad <= 0.0f;

			for (int y = y0; y < y1; y++) {
				if ((float)y < clip_.y || (float)y >= clip_.bottom()) continue;
				for (int x = x0; x < x1; x++) {
					if ((float)x < clip_.x || (float)x >= clip_.right()) continue;
					float coverage;
					if (square) {
						// Analytic edge coverage for axis-aligned edges.
						const float dx = std::min((float)x + 1.0f, r.right()) -
										 std::max((float)x, r.x);
						const float dy = std::min((float)y + 1.0f, r.bottom()) -
										 std::max((float)y, r.y);
						coverage = clamp01(dx) * clamp01(dy);
					} else {
						const float sd = sdRoundBox((float)x + 0.5f - cx,
							(float)y + 0.5f - cy, hw, hh, rad);
						coverage = clamp01(0.5f - sd);
					}
					if (coverage > 0.0f) blendCoverage(x, y, coverage, c);
				}
			}
		}

		color rasterizer::gradientColor(const backgroundPaint& paint,
			float t) const {
			if (paint.stops.empty()) return color::black();
			if (paint.stops.size() == 1) return paint.stops[0].c;
			t = clamp01(t);
			size_t i = 0;
			while (i + 1 < paint.stops.size() && paint.stops[i + 1].offset < t)
				i++;
			const gradientStop& a = paint.stops[i];
			const gradientStop& b = paint.stops[std::min(i + 1,
				paint.stops.size() - 1)];
			const float span = b.offset - a.offset;
			const float f = span > 0.0f ? clamp01((t - a.offset) / span) : 0.0f;
			color out;
			out.r = a.c.r + (b.c.r - a.c.r) * f;
			out.g = a.c.g + (b.c.g - a.c.g) * f;
			out.b = a.c.b + (b.c.b - a.c.b) * f;
			out.a = a.c.a + (b.c.a - a.c.a) * f;
			out.isSet = true;
			return out;
		}

		void rasterizer::fillBackground(const rect& r, float radius,
			const computedStyle& style, const rect& paintArea) {
			if (r.empty()) return;
			if (!style.backgroundColor.transparent())
				fillRoundedRect(r, radius, style.backgroundColor);

			const backgroundPaint& paint = style.backgroundImage;
			if (paint.kind == paintKind::none) return;
			if (paint.kind == paintKind::solid) {
				fillRoundedRect(r, radius, paint.solid);
				return;
			}

			const int x0 = (int)std::floor(std::max(r.x, clip_.x)) - 1;
			const int y0 = (int)std::floor(std::max(r.y, clip_.y)) - 1;
			const int x1 = (int)std::ceil(std::min(r.right(), clip_.right())) + 1;
			const int y1 = (int)std::ceil(std::min(r.bottom(), clip_.bottom())) + 1;
			const float cx = r.x + r.w * 0.5f;
			const float cy = r.y + r.h * 0.5f;
			const float hw = r.w * 0.5f;
			const float hh = r.h * 0.5f;
			const float rad = std::max(0.0f, std::min(radius, std::min(hw, hh)));

			// Gradient line: CSS angles start at "to top" and go clockwise.
			const float radians = (paint.angle - 90.0f) * 3.14159265f / 180.0f;
			const float dx = std::cos(radians);
			const float dy = std::sin(radians);
			const float extent = std::fabs(dx) * hw + std::fabs(dy) * hh;
			if (extent <= 0.0f) return;
			const float originX = paintArea.x + paintArea.w * 0.5f;
			const float originY = paintArea.y + paintArea.h * 0.5f;

			for (int y = y0; y < y1; y++) {
				if ((float)y < clip_.y || (float)y >= clip_.bottom()) continue;
				for (int x = x0; x < x1; x++) {
					if ((float)x < clip_.x || (float)x >= clip_.right()) continue;
					float coverage;
					if (rad <= 0.0f) {
						const float dxp = std::min((float)x + 1.0f, r.right()) -
										  std::max((float)x, r.x);
						const float dyp = std::min((float)y + 1.0f, r.bottom()) -
										  std::max((float)y, r.y);
						coverage = clamp01(dxp) * clamp01(dyp);
					} else {
						coverage = clamp01(0.5f - sdRoundBox(
											(float)x + 0.5f - cx,
											(float)y + 0.5f - cy, hw, hh,
											rad));
					}
					if (coverage <= 0.0f) continue;
					const float px = (float)x + 0.5f - originX;
					const float py = (float)y + 0.5f - originY;
					const float t = 0.5f + (px * dx + py * dy) / (2.0f * extent);
					blendCoverage(x, y, coverage,
						gradientColor(paint, t));
				}
			}
		}

		void rasterizer::strokeBorders(const rect& r, float radius,
			const float widths[4], const color colors[4]) {
			bool any = false;
			for (int i = 0; i < 4; i++)
				if (widths[i] > 0.0f && !colors[i].transparent()) any = true;
			if (!any || r.empty()) return;

			const int x0 = (int)std::floor(std::max(r.x, clip_.x)) - 1;
			const int y0 = (int)std::floor(std::max(r.y, clip_.y)) - 1;
			const int x1 = (int)std::ceil(std::min(r.right(), clip_.right())) + 1;
			const int y1 = (int)std::ceil(std::min(r.bottom(), clip_.bottom())) + 1;
			const float cx = r.x + r.w * 0.5f;
			const float cy = r.y + r.h * 0.5f;
			const float hw = r.w * 0.5f;
			const float hh = r.h * 0.5f;
			const float rad = std::max(0.0f, std::min(radius, std::min(hw, hh)));

			for (int y = y0; y < y1; y++) {
				if ((float)y < clip_.y || (float)y >= clip_.bottom()) continue;
				for (int x = x0; x < x1; x++) {
					if ((float)x < clip_.x || (float)x >= clip_.right()) continue;
					const float px = (float)x + 0.5f;
					const float py = (float)y + 0.5f;
					float sd = sdRoundBox(px - cx, py - cy, hw, hh, rad);
					if (sd > 0.5f) continue;  // outside the box entirely

					// Which side owns this pixel: the dominant axis, matching
					// how CSS splits corner pixels between two borders.
					const float ddx = std::fabs(px - cx) - (hw - rad);
					const float ddy = std::fabs(py - cy) - (hh - rad);
					const bool horizontalSide = ddy >= ddx;
					int side;
					if (horizontalSide)
						side = (py < cy) ? 0 : 2;  // top / bottom
					else
						side = (px < cx) ? 3 : 1;  // left / right

					const float width = widths[side];
					if (width <= 0.0f || colors[side].transparent()) continue;
					// The band runs from the outer edge (sd == 0) inward by
					// `width`, so coverage is the product of the two edges.
					const float coverage = clamp01(0.5f - sd) *
										   clamp01(sd + width + 0.5f);
					if (coverage <= 0.0f) continue;
					blendCoverage(x, y, coverage, colors[side]);
				}
			}
		}

		void rasterizer::drawText(const textRun& run, fontManager& fonts) {
			if (run.text.empty() || run.c.transparent()) return;
			const vector<fontManager::shapedGlyph> glyphs =
				fonts.shape(run.text, run.family, run.size, run.weight,
					run.italic);
			float penX = run.x;
			for (const auto& shaped : glyphs) {
				const int whole = (int)std::floor(penX);
				const int subpixel = (int)((penX - (float)whole) * 3.0f + 0.5f);
				const glyphBitmap* glyph = fonts.glyph(shaped.codepoint,
					run.family, run.size, run.weight, run.italic,
					subpixel > 2 ? 2 : (subpixel < 0 ? 0 : subpixel));
				if (glyph && glyph->valid()) {
					const int gx = whole + glyph->left;
					const int gy = (int)std::lround(run.y) - glyph->top;
					for (int y = 0; y < glyph->height; y++) {
						const int py = gy + y;
						if ((float)py < clip_.y || (float)py >= clip_.bottom())
							continue;
						const uint8_t* row = glyph->alpha.data() +
											 (size_t)y * (size_t)glyph->width;
						for (int x = 0; x < glyph->width; x++) {
							const int pxx = gx + x;
							if ((float)pxx < clip_.x ||
								(float)pxx >= clip_.right())
								continue;
							blendCoverage(pxx, py, row[x] / 255.0f, run.c);
						}
					}
				}
				penX += shaped.advance + run.letterSpacing;
			}

			// Underline / strike-through, thickness proportional to the size.
			const float thickness = std::max(1.0f, run.size / 12.0f);
			if (run.underline)
				fillRect({run.x, run.y + run.size * 0.12f, run.width,
							 thickness},
					run.c);
			if (run.lineThrough)
				fillRect({run.x, run.y - run.size * 0.28f, run.width, thickness},
					run.c);
		}

		void rasterizer::drawImage(const rect& r, float radius,
			const imageData& image, bgSizeType sizeMode, const color& tint) {
			if (r.empty() || image.width == 0 || image.height == 0) return;
			float scaleX = (float)image.width / r.w;
			float scaleY = (float)image.height / r.h;
			if (sizeMode == bgSizeType::cover) {
				const float s = std::max(scaleX, scaleY);
				scaleX = scaleY = s;
			} else if (sizeMode == bgSizeType::contain) {
				const float s = std::min(scaleX, scaleY);
				scaleX = scaleY = s;
			}
			const float drawW = (float)image.width / scaleX;
			const float drawH = (float)image.height / scaleY;
			const float offsetX = r.x + (r.w - drawW) * 0.5f;
			const float offsetY = r.y + (r.h - drawH) * 0.5f;
			const rect area = {offsetX, offsetY, drawW, drawH};
			const bool repeat = sizeMode == bgSizeType::autoSize;

			const int x0 = (int)std::floor(std::max(area.x, clip_.x));
			const int y0 = (int)std::floor(std::max(area.y, clip_.y));
			const int x1 = (int)std::ceil(std::min(area.right(), clip_.right()));
			const int y1 = (int)std::ceil(std::min(area.bottom(), clip_.bottom()));
			for (int y = y0; y < y1; y++) {
				for (int x = x0; x < x1; x++) {
					if ((float)x < clip_.x || (float)x >= clip_.right()) continue;
					if ((float)y < clip_.y || (float)y >= clip_.bottom()) continue;
					float u = ((float)x + 0.5f - area.x) * scaleX;
					float v = ((float)y + 0.5f - area.y) * scaleY;
					if (repeat) {
						u = std::fmod(u, (float)image.width);
						v = std::fmod(v, (float)image.height);
						if (u < 0.0f) u += (float)image.width;
						if (v < 0.0f) v += (float)image.height;
					}
					const int iu = (int)u;
					const int iv = (int)v;
					if (iu < 0 || iv < 0 || (uint32_t)iu >= image.width ||
						(uint32_t)iv >= image.height)
						continue;
					const size_t offset =
						((size_t)iv * image.width + (size_t)iu) * 4;
					color c;
					c.r = image.rgba[offset] / 255.0f;
					c.g = image.rgba[offset + 1] / 255.0f;
					c.b = image.rgba[offset + 2] / 255.0f;
					c.a = image.rgba[offset + 3] / 255.0f;
					if (!tint.transparent()) {
						// `background-blend-mode`-ish tint.
						c.r = c.r * (1.0f - tint.a) + tint.r;
						c.g = c.g * (1.0f - tint.a) + tint.g;
						c.b = c.b * (1.0f - tint.a) + tint.b;
						c.a = c.a + tint.a * (1.0f - c.a);
					}
					const float coverage = radius > 0.0f
						? clamp01(0.5f - sdRoundBox((float)x + 0.5f -
													  (r.x + r.w * 0.5f),
												  (float)y + 0.5f -
													  (r.y + r.h * 0.5f),
								  r.w * 0.5f, r.h * 0.5f, radius))
						: 1.0f;
					blendCoverage(x, y, coverage, c);
				}
			}
		}

		void rasterizer::strokeRect(const rect& r, const color& c,
			float thickness) {
			fillRect({r.x, r.y, r.w, thickness}, c);
			fillRect({r.x, r.bottom() - thickness, r.w, thickness}, c);
			fillRect({r.x, r.y, thickness, r.h}, c);
			fillRect({r.right() - thickness, r.y, thickness, r.h}, c);
		}

	}  // namespace UI
}  // namespace gold
