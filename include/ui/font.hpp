#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"

namespace gold {
	namespace UI {

		using std::string;
		using std::vector;

		/** An 8-bit coverage bitmap for one glyph. */
		struct glyphBitmap {
			int width = 0;
			int height = 0;
			/** Pen offset from the glyph origin. */
			int left = 0;
			/** Distance from the baseline up to the top row (positive). */
			int top = 0;
			/** Pen advance in px. */
			float advance = 0.0f;
			vector<uint8_t> alpha;

			bool valid() const {
				return width > 0 && height > 0 && !alpha.empty();
			}
		};

		/** Vertical metrics of a font at one size, in px. */
		struct fontMetrics {
			float ascent = 0.0f;
			float descent = 0.0f;
			float lineGap = 0.0f;
			float xHeight = 0.0f;
		};

		/**
		 * Text measurement and glyph rasterization.
		 *
		 * Font files are registered with `loadFile` / `loadData`; the family
		 * names they answer to include the generic CSS families. When no font
		 * is registered for a family (or FreeType is unavailable) a built-in
		 * 5x7 bitmap font takes over, so the renderer always produces
		 * pixels and tests stay deterministic.
		 *
		 * FreeType headers are deliberately kept out of this header.
		 */
		class fontManager {
		 public:
			fontManager();
			~fontManager();
			fontManager(const fontManager&) = delete;
			fontManager& operator=(const fontManager&) = delete;

			/** Register a font file under `family`. */
			bool loadFile(const string& path, const string& family,
				int weight = 400, bool italic = false);
			/** Register an in-memory font under `family`. */
			bool loadData(binary data, const string& family, int weight = 400,
				bool italic = false);
			/**
			 * Register the files backing the generic families. Missing
			 * entries are skipped; the built-in font covers the rest.
			 */
			void setGenericFonts(const string& sans, const string& serif = "",
				const string& mono = "");

			bool hasFamily(const string& family) const;
			/** Number of registered font files. */
			size_t fontCount() const { return faceCount_; }

			/** Width of `utf8` in px, no wrapping, no kerning. */
			float measureWidth(const string& utf8, const string& family,
				float size, int weight = 400, bool italic = false) const;
			fontMetrics metrics(const string& family, float size,
				int weight = 400, bool italic = false) const;

			/**
			 * Rasterize one codepoint. `subpixel` is a 0..2 horizontal
			 * third-pixel offset used to keep small text from looking mushy.
			 * The result is owned by the manager and stays valid until the
			 * next `clearCache`.
			 */
			const glyphBitmap* glyph(uint32_t codepoint, const string& family,
				float size, int weight = 400, bool italic = false,
				int subpixel = 0);

			/** Glyph run split into codepoints (used for wrapping + hit test). */
			struct shapedGlyph {
				uint32_t codepoint = 0;
				/** Byte offset of the codepoint in the source string. */
				size_t offset = 0;
				float x = 0.0f;
				float advance = 0.0f;
			};
			vector<shapedGlyph> shape(const string& utf8, const string& family,
				float size, int weight = 400, bool italic = false) const;

			/** Drop cached glyph bitmaps. */
			void clearCache();
			size_t glyphCacheSize() const { return cacheSize_; }
			/** True when a real font file backs the family. */
			bool isRealFont(const string& family) const;

			/** UTF-8 helpers, exposed because text arrives as UTF-8 strings. */
			static vector<uint32_t> decodeUTF8(const string& utf8);
			static string encodeUTF8(uint32_t codepoint);

		 private:
			struct impl;
			impl* d;
			size_t faceCount_ = 0;
			size_t cacheSize_ = 0;
		};

	}  // namespace UI
}  // namespace gold
