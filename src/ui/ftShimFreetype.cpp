// The real FreeType shim: compiled against the system freetype headers
// for the C ABI types, but every entry point is resolved from a dlopen'd
// handle — nothing here links libfreetype.

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "ui/ftShim.h"
#include "plugin.hpp"

namespace gold {
	namespace ftshim {

		namespace {
			void* dl = nullptr;       // dlopen handle
			FT_Library ft = nullptr;  // FreeType library instance

			using errFn = FT_Error (*)();

			// The entry points font.cpp needs, typed straight from the
			// system headers.
			struct api {
				FT_Error (*Init_FreeType)(FT_Library*){};
				FT_Error (*Done_FreeType)(FT_Library){};
				FT_Error (*New_Face)(FT_Library, const char*, FT_Long,
					FT_Face*){};
				FT_Error (*New_Memory_Face)(FT_Library, const FT_Byte*,
					FT_Long, FT_Long, FT_Face*){};
				FT_Error (*Done_Face)(FT_Face){};
				FT_Error (*Set_Char_Size)(FT_Face, FT_F26Dot6, FT_F26Dot6,
					FT_UInt, FT_UInt){};
				FT_UInt (*Get_Char_Index)(FT_Face, FT_ULong){};
				FT_Error (*Get_Kerning)(FT_Face, FT_UInt, FT_UInt, FT_UInt,
					FT_Vector*){};
				FT_Error (*Outline_Translate)(const FT_Outline*, FT_Pos,
					FT_Pos){};
				FT_Error (*Load_Glyph)(FT_Face, FT_UInt, FT_Int32){};
				FT_Error (*Render_Glyph)(FT_GlyphSlot, FT_Render_Mode){};
			};
			api fns{};

			bool resolve(void* handle) {
// dlsym each name into its typed slot; early-out on the first miss.
#define FT_SYMBOL(field, name)                                      \
	if (!(fns.field =                                              \
			(decltype(fns.field))plugin::symbol(handle, #name)))   \
		return false;
				FT_SYMBOL(Init_FreeType, FT_Init_FreeType);
				FT_SYMBOL(Done_FreeType, FT_Done_FreeType);
				FT_SYMBOL(New_Face, FT_New_Face);
				FT_SYMBOL(New_Memory_Face, FT_New_Memory_Face);
				FT_SYMBOL(Done_Face, FT_Done_Face);
				FT_SYMBOL(Set_Char_Size, FT_Set_Char_Size);
				FT_SYMBOL(Get_Char_Index, FT_Get_Char_Index);
				FT_SYMBOL(Get_Kerning, FT_Get_Kerning);
				FT_SYMBOL(Outline_Translate, FT_Outline_Translate);
				FT_SYMBOL(Load_Glyph, FT_Load_Glyph);
				FT_SYMBOL(Render_Glyph, FT_Render_Glyph);
#undef FT_SYMBOL
				return true;
			}
		}  // namespace

		bool isOpen() {
			return ft != nullptr;
		}

		bool open() {
			if (ft) return true;
			if (!dl) dl = plugin::openSystemLibrary(plugin::sonames("freetype"));
			if (!dl) return false;
			if (!resolve(dl)) return false;
			return fns.Init_FreeType(&ft) == 0 && ft != nullptr;
		}

		void* openFaceFile(const std::string& path) {
			if (!ft) return nullptr;
			FT_Face face = nullptr;
			if (fns.New_Face(ft, path.c_str(), 0, &face) != 0) return nullptr;
			return face;
		}

		void* openFaceMemory(const unsigned char* data, size_t size) {
			if (!ft || !data || size == 0) return nullptr;
			FT_Face face = nullptr;
			if (fns.New_Memory_Face(ft, data, (FT_Long)size, 0, &face) != 0)
				return nullptr;
			return face;
		}

		void closeFace(void* face) {
			if (face) fns.Done_Face((FT_Face)face);
		}

		bool setSize(void* faceV, float size) {
			if (!faceV || size <= 0.0f) return false;
			return fns.Set_Char_Size((FT_Face)faceV, 0,
					   (FT_F26Dot6)(size * 64.0), 72, 72) == 0;
		}

		faceMetrics metrics(void* faceV, float size) {
			faceMetrics out;
			if (!faceV || !setSize(faceV, size)) return out;
			const FT_Size_Metrics& m = ((FT_Face)faceV)->size->metrics;
			out.ascent = (float)m.ascender / 64.0f;
			out.descent = (float)(-m.descender) / 64.0f;
			const float gap =
				(float)m.height / 64.0f - out.ascent - out.descent;
			out.lineGap = gap > 0.0f ? gap : 0.0f;
			// Real x-height from the 'x' glyph at this size; fall back to
			// half the ascent when the font has no 'x'.
			const FT_UInt xIndex = fns.Get_Char_Index((FT_Face)faceV, 'x');
			if (xIndex != 0 &&
				fns.Load_Glyph((FT_Face)faceV, xIndex, FT_LOAD_NO_BITMAP) ==
					0)
				out.xHeight =
					(float)((FT_Face)faceV)->glyph->metrics.height / 64.0f;
			else
				out.xHeight = out.ascent * 0.5f;
			return out;
		}

		glyphInfo loadGlyph(void* faceV, uint32_t glyphIndex, bool noBitmap) {
			glyphInfo out;
			if (!faceV || glyphIndex == 0) return out;
			FT_Int32 load = FT_LOAD_DEFAULT;
			if (noBitmap) load |= FT_LOAD_NO_BITMAP;
			if (fns.Load_Glyph((FT_Face)faceV, glyphIndex, load) != 0)
				return out;
			out.advance =
				(float)((FT_Face)faceV)->glyph->advance.x / 64.0f;
			out.hasOutline =
				((FT_Face)faceV)->glyph->format == FT_GLYPH_FORMAT_OUTLINE;
			out.ok = true;
			return out;
		}

		void translateOutline(void* faceV, int32_t dx26) {
			if (!faceV) return;
			auto* glyph = ((FT_Face)faceV)->glyph;
			if (glyph && glyph->format == FT_GLYPH_FORMAT_OUTLINE)
				fns.Outline_Translate(&glyph->outline, (FT_Pos)dx26, 0);
		}

		bool rasterize(void* faceV) {
			if (!faceV) return false;
			return fns.Render_Glyph(((FT_Face)faceV)->glyph,
					   FT_RENDER_MODE_NORMAL) == 0;
		}

		bitmapView bitmapOf(void* faceV) {
			bitmapView out;
			if (!faceV) return out;
			auto* glyph = ((FT_Face)faceV)->glyph;
			if (!glyph) return out;
			const FT_Bitmap& src = glyph->bitmap;
			out.width = (int32_t)src.width;
			out.height = (int32_t)src.rows;
			out.pitch = (int32_t)src.pitch;
			out.buffer = src.buffer;
			out.left = glyph->bitmap_left;
			out.top = glyph->bitmap_top;
			return out;
		}

		uint32_t charIndex(void* faceV, uint32_t codepoint) {
			if (!faceV) return 0;
			return (uint32_t)fns.Get_Char_Index((FT_Face)faceV,
				(FT_ULong)codepoint);
		}

		bool hasKerning(void* faceV) {
			if (!faceV) return false;
			return FT_HAS_KERNING((FT_Face)faceV) != 0;
		}

		float kerning(void* faceV, uint32_t previous, uint32_t current) {
			if (!faceV || previous == 0 || current == 0) return 0.0f;
			FT_Vector delta;
			if (fns.Get_Kerning((FT_Face)faceV, previous, current,
					FT_KERNING_DEFAULT, &delta) != 0)
				return 0.0f;
			return (float)delta.x / 64.0f;
		}

	}  // namespace ftshim
}  // namespace gold