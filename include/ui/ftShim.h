#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace gold {
	/**
	 * The FreeType shim: font rasterization through the system FreeType
	 * without linking it (no `libfreetype` in the dynamic table, so a
	 * machine without it falls back to the built-in font).
	 *
	 * The shim is a thin pass-through over FreeType's stable C ABI: every
	 * entry point is dlsym'd (typed by the system freetype headers at
	 * build time) from a dlopen'd handle; the plugin::sonames("freetype")
	 * probe list carries the candidate sonames. This header exposes only
	 * gold-native types — no FreeType headers are part of gold's API.
	 *
	 * When FreeType is unavailable (ftShimNone build or a machine without
	 * the library) every call is an inert no-op and `open()` is false; the
	 * fontManager falls back to its built-in 5x7 font.
	 */
	namespace ftshim {

		/** Open the system FreeType and resolve its entry points. Safe to
		 *  call repeatedly; returns false when unavailable. */
		bool open();
		/** Whether the system FreeType is open. */
		bool isOpen();

		void* openFaceFile(const std::string& path);
		void* openFaceMemory(const unsigned char* data, size_t size);
		void closeFace(void* face);

		/** Set the pixel size (72 dpi) that subsequent metrics and glyph
		 *  reads on `face` use. */
		bool setSize(void* face, float size);

		/** Vertical metrics in px, computed at `size` (sets it first). */
		struct faceMetrics {
			float ascent = 0.0f;
			float descent = 0.0f;
			float lineGap = 0.0f;
			float xHeight = 0.0f;
		};
		faceMetrics metrics(void* face, float size);

		/** A loaded glyph: advance/outline state after `loadGlyph`, the
		 *  bitmap view after a successful `rasterize`. */
		struct glyphInfo {
			float advance = 0.0f;
			bool hasOutline = false;
			bool ok = false;
		};
		/** Load a glyph by index. `noBitmap` filters bitmap strikes (the
		 *  renderer re-rasterizes outlines at exact sizes). */
		glyphInfo loadGlyph(void* face, uint32_t glyphIndex, bool noBitmap);

		/** Horizontally translate the loaded glyph's outline (subpixel
		 *  positioning), fixed-26.6 x offset. */
		void translateOutline(void* face, int32_t dx26);

		/** Rasterize the loaded glyph (8-bit alpha). */
		bool rasterize(void* face);

		/** The rasterized bitmap view; buffer stays valid until the next
		 *  glyph load/free on this face. */
		struct bitmapView {
			int32_t left = 0;
			int32_t top = 0;
			int32_t width = 0;
			int32_t height = 0;
			int32_t pitch = 0;
			const unsigned char* buffer = nullptr;
		};
		bitmapView bitmapOf(void* face);

		/** 0 when the codepoint is unmapped. */
		uint32_t charIndex(void* face, uint32_t codepoint);
		bool hasKerning(void* face);
		/** Kerning between two glyph indices in px at the current size. */
		float kerning(void* face, uint32_t previous, uint32_t current);

	}  // namespace ftshim
}  // namespace gold