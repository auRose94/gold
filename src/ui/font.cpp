#include "ui/font.hpp"

#include <cmath>
#include <cstring>
#include <unordered_map>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

namespace gold {
	namespace UI {

		namespace {

			// Classic 5x7 cell font, ASCII 0x20..0x7E. Each glyph is five
			// columns; bit N of a column is row N, counted from the top.
			const uint8_t kBuiltin5x7[95][5] = {
				{0x00, 0x00, 0x00, 0x00, 0x00},  // space
				{0x00, 0x00, 0x5F, 0x00, 0x00},  // !
				{0x00, 0x07, 0x00, 0x07, 0x00},  // "
				{0x14, 0x7F, 0x14, 0x7F, 0x14},  // #
				{0x24, 0x2A, 0x7F, 0x2A, 0x12},  // $
				{0x23, 0x13, 0x08, 0x64, 0x62},  // %
				{0x36, 0x49, 0x55, 0x22, 0x50},  // &
				{0x00, 0x05, 0x03, 0x00, 0x00},  // '
				{0x00, 0x1C, 0x22, 0x41, 0x00},  // (
				{0x00, 0x41, 0x22, 0x1C, 0x00},  // )
				{0x14, 0x08, 0x3E, 0x08, 0x14},  // *
				{0x08, 0x08, 0x3E, 0x08, 0x08},  // +
				{0x00, 0x50, 0x30, 0x00, 0x00},  // ,
				{0x08, 0x08, 0x08, 0x08, 0x08},  // -
				{0x00, 0x60, 0x60, 0x00, 0x00},  // .
				{0x20, 0x10, 0x08, 0x04, 0x02},  // /
				{0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0
				{0x00, 0x42, 0x7F, 0x40, 0x00},  // 1
				{0x42, 0x61, 0x51, 0x49, 0x46},  // 2
				{0x21, 0x41, 0x45, 0x4B, 0x31},  // 3
				{0x18, 0x14, 0x12, 0x7F, 0x10},  // 4
				{0x27, 0x45, 0x45, 0x45, 0x39},  // 5
				{0x3C, 0x4A, 0x49, 0x49, 0x30},  // 6
				{0x01, 0x71, 0x09, 0x05, 0x03},  // 7
				{0x36, 0x49, 0x49, 0x49, 0x36},  // 8
				{0x06, 0x49, 0x49, 0x29, 0x1E},  // 9
				{0x00, 0x36, 0x36, 0x00, 0x00},  // :
				{0x00, 0x56, 0x36, 0x00, 0x00},  // ;
				{0x08, 0x14, 0x22, 0x41, 0x00},  // <
				{0x14, 0x14, 0x14, 0x14, 0x14},  // =
				{0x00, 0x41, 0x22, 0x14, 0x08},  // >
				{0x02, 0x01, 0x51, 0x09, 0x06},  // ?
				{0x32, 0x49, 0x79, 0x41, 0x3E},  // @
				{0x7E, 0x11, 0x11, 0x11, 0x7E},  // A
				{0x7F, 0x49, 0x49, 0x49, 0x36},  // B
				{0x3E, 0x41, 0x41, 0x41, 0x22},  // C
				{0x7F, 0x41, 0x41, 0x22, 0x1C},  // D
				{0x7F, 0x49, 0x49, 0x49, 0x41},  // E
				{0x7F, 0x09, 0x09, 0x09, 0x01},  // F
				{0x3E, 0x41, 0x49, 0x49, 0x7A},  // G
				{0x7F, 0x08, 0x08, 0x08, 0x7F},  // H
				{0x00, 0x41, 0x7F, 0x41, 0x00},  // I
				{0x20, 0x40, 0x41, 0x3F, 0x01},  // J
				{0x7F, 0x08, 0x14, 0x22, 0x41},  // K
				{0x7F, 0x40, 0x40, 0x40, 0x40},  // L
				{0x7F, 0x02, 0x0C, 0x02, 0x7F},  // M
				{0x7F, 0x04, 0x08, 0x10, 0x7F},  // N
				{0x3E, 0x41, 0x41, 0x41, 0x3E},  // O
				{0x7F, 0x09, 0x09, 0x09, 0x06},  // P
				{0x3E, 0x41, 0x51, 0x21, 0x5E},  // Q
				{0x7F, 0x09, 0x19, 0x29, 0x46},  // R
				{0x46, 0x49, 0x49, 0x49, 0x31},  // S
				{0x01, 0x01, 0x7F, 0x01, 0x01},  // T
				{0x3F, 0x40, 0x40, 0x40, 0x3F},  // U
				{0x1F, 0x20, 0x40, 0x20, 0x1F},  // V
				{0x3F, 0x40, 0x38, 0x40, 0x3F},  // W
				{0x63, 0x14, 0x08, 0x14, 0x63},  // X
				{0x07, 0x08, 0x70, 0x08, 0x07},  // Y
				{0x61, 0x51, 0x49, 0x45, 0x43},  // Z
				{0x00, 0x7F, 0x41, 0x41, 0x00},  // [
				{0x02, 0x04, 0x08, 0x10, 0x20},  // backslash
				{0x00, 0x41, 0x41, 0x7F, 0x00},  // ]
				{0x04, 0x02, 0x01, 0x02, 0x04},  // ^
				{0x40, 0x40, 0x40, 0x40, 0x40},  // _
				{0x00, 0x01, 0x02, 0x04, 0x00},  // `
				{0x20, 0x54, 0x54, 0x54, 0x78},  // a
				{0x7F, 0x48, 0x44, 0x44, 0x38},  // b
				{0x38, 0x44, 0x44, 0x44, 0x20},  // c
				{0x38, 0x44, 0x44, 0x48, 0x7F},  // d
				{0x38, 0x54, 0x54, 0x54, 0x18},  // e
				{0x08, 0x7E, 0x09, 0x01, 0x02},  // f
				{0x0C, 0x52, 0x52, 0x52, 0x3E},  // g
				{0x7F, 0x08, 0x04, 0x04, 0x78},  // h
				{0x00, 0x44, 0x7D, 0x40, 0x00},  // i
				{0x20, 0x40, 0x44, 0x3D, 0x00},  // j
				{0x7F, 0x10, 0x28, 0x44, 0x00},  // k
				{0x00, 0x41, 0x7F, 0x40, 0x00},  // l
				{0x7C, 0x04, 0x18, 0x04, 0x78},  // m
				{0x7C, 0x08, 0x04, 0x04, 0x78},  // n
				{0x38, 0x44, 0x44, 0x44, 0x38},  // o
				{0x7C, 0x14, 0x14, 0x14, 0x08},  // p
				{0x08, 0x14, 0x14, 0x18, 0x7C},  // q
				{0x7C, 0x08, 0x04, 0x04, 0x08},  // r
				{0x48, 0x54, 0x54, 0x54, 0x20},  // s
				{0x04, 0x3F, 0x44, 0x40, 0x20},  // t
				{0x3C, 0x40, 0x40, 0x20, 0x7C},  // u
				{0x1C, 0x20, 0x40, 0x20, 0x1C},  // v
				{0x3C, 0x40, 0x30, 0x40, 0x3C},  // w
				{0x44, 0x28, 0x10, 0x28, 0x44},  // x
				{0x0C, 0x50, 0x50, 0x50, 0x3C},  // y
				{0x44, 0x64, 0x54, 0x4C, 0x44},  // z
				{0x00, 0x08, 0x36, 0x41, 0x00},  // {
				{0x00, 0x00, 0x7F, 0x00, 0x00},  // |
				{0x00, 0x41, 0x36, 0x08, 0x00},  // }
				{0x08, 0x08, 0x2A, 0x1C, 0x08},  // ~
			};

			const int kCellWidth = 5;
			const int kCellHeight = 7;
			const int kCellAdvance = 6;

			string lowerStr(string s) {
				for (auto& c : s) c = (char)tolower((unsigned char)c);
				return s;
			}

			string trimStr(const string& s) {
				const size_t start = s.find_first_not_of(" \t\"'");
				if (start == string::npos) return "";
				const size_t end = s.find_last_not_of(" \t\"'");
				return s.substr(start, end - start + 1);
			}

			/** Snap a size to a stable key so the glyph cache hits. */
			uint32_t sizeKey(float size) {
				return (uint32_t)(size * 16.0f + 0.5f);
			}

		}  // namespace

		// ------------------------------------------------------------- impl

		struct fontManager::impl {
			FT_Library library = nullptr;
			struct faceEntry {
				FT_Face face = nullptr;
				binary data;  // keeps in-memory faces alive
				string family;
				int weight = 400;
				bool italic = false;
			};
			vector<faceEntry> faces;
			std::unordered_map<string, size_t> byFamily;
			std::unordered_map<uint64_t, glyphBitmap> cache;
			bool freetypeReady = false;

			~impl() {
				for (auto& entry : faces)
					if (entry.face) FT_Done_Face(entry.face);
				if (library) FT_Done_FreeType(library);
			}

			/** Find the face for a family, falling back to any weight. */
			faceEntry* find(const string& family, int weight, bool italic) {
				lowerStr(const_cast<string&>(family));
				string key = family;
				auto found = byFamily.find(key);
				if (found == byFamily.end()) {
					// Case-insensitive retry.
					for (const auto& pair : byFamily)
						if (lowerStr(pair.first) == key)
							return &faces[pair.second];
					return nullptr;
				}
				faceEntry* entry = &faces[found->second];
				if (entry->weight == weight && entry->italic == italic)
					return entry;
				// Same family, different weight/style: reuse it rather than
				// synthesizing (FreeType cannot fake bold here).
				return entry;
			}

			static uint64_t cacheKey(uint32_t codepoint, size_t faceIndex,
				uint32_t size, int subpixel) {
				return (uint64_t)faceIndex << 48 | (uint64_t)size << 16 |
					   ((uint64_t)subpixel << 8) | codepoint;
			}
		};

		fontManager::fontManager() : d(new impl()) {
			d->freetypeReady = FT_Init_FreeType(&d->library) == 0;
		}

		fontManager::~fontManager() {
			delete d;
		}

		vector<uint32_t> fontManager::decodeUTF8(const string& utf8) {
			vector<uint32_t> out;
			size_t i = 0;
			while (i < utf8.size()) {
				const unsigned char c = (unsigned char)utf8[i];
				uint32_t cp = 0;
				size_t extra = 0;
				if (c < 0x80) {
					cp = c;
					extra = 0;
				} else if ((c & 0xE0) == 0xC0) {
					cp = c & 0x1F;
					extra = 1;
				} else if ((c & 0xF0) == 0xE0) {
					cp = c & 0x0F;
					extra = 2;
				} else if ((c & 0xF8) == 0xF0) {
					cp = c & 0x07;
					extra = 3;
				} else {
					cp = 0xFFFD;
					extra = 0;
				}
				if (i + extra >= utf8.size()) {
					cp = 0xFFFD;
					extra = 0;
				} else {
					for (size_t k = 1; k <= extra; k++)
						cp = (cp << 6) |
							 ((unsigned char)utf8[i + k] & 0x3F);
				}
				out.push_back(cp);
				i += extra + 1;
			}
			return out;
		}

		string fontManager::encodeUTF8(uint32_t cp) {
			string out;
			if (cp < 0x80) {
				out += (char)cp;
			} else if (cp < 0x800) {
				out += (char)(0xC0 | (cp >> 6));
				out += (char)(0x80 | (cp & 0x3F));
			} else if (cp < 0x10000) {
				out += (char)(0xE0 | (cp >> 12));
				out += (char)(0x80 | ((cp >> 6) & 0x3F));
				out += (char)(0x80 | (cp & 0x3F));
			} else {
				out += (char)(0xF0 | (cp >> 18));
				out += (char)(0x80 | ((cp >> 12) & 0x3F));
				out += (char)(0x80 | ((cp >> 6) & 0x3F));
				out += (char)(0x80 | (cp & 0x3F));
			}
			return out;
		}

		bool fontManager::loadFile(const string& path, const string& family,
			int weight, bool italic) {
			if (!d->freetypeReady) return false;
			FT_Face face = nullptr;
			if (FT_New_Face(d->library, path.c_str(), 0, &face) != 0)
				return false;
			impl::faceEntry entry;
			entry.face = face;
			entry.family = family;
			entry.weight = weight;
			entry.italic = italic;
			const size_t index = d->faces.size();
			d->faces.push_back(entry);
			d->byFamily[family] = index;
			faceCount_ = d->faces.size();
			return true;
		}

		bool fontManager::loadData(binary data, const string& family,
			int weight, bool italic) {
			if (!d->freetypeReady || data.size() == 0) return false;
			FT_Face face = nullptr;
			if (FT_New_Memory_Face(d->library, (const FT_Byte*)data.data(),
					(FT_Long)data.size(), 0, &face) != 0)
				return false;
			impl::faceEntry entry;
			entry.face = face;
			entry.data = data;  // FreeType reads it lazily
			entry.family = family;
			entry.weight = weight;
			entry.italic = italic;
			const size_t index = d->faces.size();
			d->faces.push_back(entry);
			d->byFamily[family] = index;
			faceCount_ = d->faces.size();
			return true;
		}

		void fontManager::setGenericFonts(const string& sans, const string& serif,
			const string& mono) {
			if (!sans.empty()) {
				loadFile(sans, "sans-serif");
				loadFile(sans, "sans");
			}
			if (!serif.empty()) {
				loadFile(serif, "serif");
				loadFile(serif, "Times");
			}
			if (!mono.empty()) {
				loadFile(mono, "monospace");
				loadFile(mono, "Courier");
			}
		}

		bool fontManager::hasFamily(const string& family) const {
			return d->byFamily.find(family) != d->byFamily.end();
		}

		bool fontManager::isRealFont(const string& family) const {
			return hasFamily(family);
		}

		namespace {
			/** Built-in font metrics: the cell is 7 units tall. */
			float builtinScale(float size) { return size / (float)kCellHeight; }
		}  // namespace

		fontMetrics fontManager::metrics(const string& family, float size,
			int weight, bool italic) const {
			fontMetrics out;
			(void)weight;
			(void)italic;
			if (size <= 0.0f) size = 1.0f;
			auto* entry = d->find(family, weight, italic);
			if (entry && entry->face) {
				// The size metrics are 26.6 fixed point; scale them to the
				// requested pixel size (the face was set at 72 dpi).
				const FT_Size_Metrics& m = entry->face->size->metrics;
				const float units = (float)(size * 64.0) / 64.0f;
				out.ascent = (float)m.ascender / 64.0f * units;
				out.descent = (float)(-m.descender) / 64.0f * units;
				out.lineGap = (float)m.height / 64.0f * units;
				out.xHeight = (float)m.height / 64.0f * units;
				if (out.ascent > 0.0f) return out;
			}
			const float s = builtinScale(size);
			out.ascent = (float)kCellHeight * s;
			out.descent = 0.0f;
			out.lineGap = 0.0f;
			out.xHeight = 5.0f * s;
			return out;
		}

		float fontManager::measureWidth(const string& utf8, const string& family,
			float size, int weight, bool italic) const {
			if (utf8.empty() || size <= 0.0f) return 0.0f;
			auto* entry = d->find(family, weight, italic);
			if (!entry || !entry->face) {
				const float s = builtinScale(size);
				float w = 0.0f;
				for (size_t i = 0; i < utf8.size();) {
					// Skip the continuation bytes of a UTF-8 sequence.
					if (((unsigned char)utf8[i] & 0xC0) == 0x80) {
						i++;
						continue;
					}
					i++;
					w += (float)kCellAdvance * s;
				}
				return w;
			}

			FT_Face face = entry->face;
			const FT_F26Dot6 charSize = (FT_F26Dot6)(size * 64.0);
			FT_Set_Char_Size(face, 0, charSize, 72, 72);

			float width = 0.0f;
			FT_UInt previous = 0;
			for (uint32_t cp : decodeUTF8(utf8)) {
				const FT_UInt index = FT_Get_Char_Index(face, cp);
				if (previous && index &&
					FT_HAS_KERNING(face)) {
					FT_Vector delta;
					if (FT_Get_Kerning(face, previous, index,
							FT_KERNING_DEFAULT, &delta) == 0)
						width += (float)delta.x / 64.0f;
				}
				if (FT_Load_Glyph(face, index, FT_LOAD_DEFAULT) == 0)
					width += (float)face->glyph->advance.x / 64.0f;
				previous = index;
			}
			return width;
		}

		vector<fontManager::shapedGlyph> fontManager::shape(
			const string& utf8, const string& family, float size, int weight,
			bool italic) const {
			vector<shapedGlyph> out;
			float x = 0.0f;
			if (auto* entry = d->find(family, weight, italic);
				entry && entry->face) {
				FT_Face face = entry->face;
				FT_Set_Char_Size(face, 0, (FT_F26Dot6)(size * 64.0), 72, 72);
				for (uint32_t cp : decodeUTF8(utf8)) {
					shapedGlyph glyph;
					glyph.codepoint = cp;
					glyph.x = x;
					if (FT_Load_Glyph(face, FT_Get_Char_Index(face, cp),
							FT_LOAD_DEFAULT) == 0)
						glyph.advance = (float)face->glyph->advance.x / 64.0f;
					x += glyph.advance;
					out.push_back(glyph);
				}
				return out;
			}
			const float advance = (float)kCellAdvance * builtinScale(size);
			for (uint32_t cp : decodeUTF8(utf8)) {
				shapedGlyph glyph;
				glyph.codepoint = cp;
				glyph.x = x;
				glyph.advance = advance;
				x += advance;
				out.push_back(glyph);
			}
			return out;
		}

		const glyphBitmap* fontManager::glyph(uint32_t codepoint,
			const string& family, float size, int weight, bool italic,
			int subpixel) {
			if (size <= 0.0f) size = 1.0f;
			if (subpixel < 0) subpixel = 0;
			if (subpixel > 2) subpixel = 2;

			auto* entry = d->find(family, weight, italic);
			if (!entry || !entry->face) {
				// Built-in 5x7 fallback, scaled to the requested size.
				const float scale = builtinScale(size);
				const int w = (int)ceilf((float)kCellWidth * scale);
				const int h = (int)ceilf((float)kCellHeight * scale);
				const uint64_t key = impl::cacheKey(codepoint, 0,
					sizeKey(size), subpixel);
				auto cached = d->cache.find(key);
				if (cached != d->cache.end()) return &cached->second;

				glyphBitmap bitmap;
				bitmap.width = w;
				bitmap.height = h;
				bitmap.left = 0;
				bitmap.top = h;  // sits on the baseline
				bitmap.advance = (float)kCellAdvance * scale;
				bitmap.alpha.assign((size_t)(w * h), 0);
				if (codepoint >= 0x20 && codepoint <= 0x7E) {
					const uint8_t* glyph5x7 = kBuiltin5x7[codepoint - 0x20];
					for (int y = 0; y < h; y++) {
						for (int x = 0; x < w; x++) {
							// Nearest-neighbour sample of the 5x7 cell.
							const int sx = (int)((float)x * kCellWidth / w);
							const int sy = (int)((float)y * kCellHeight / h);
							const bool on = (glyph5x7[sx] >> sy) & 1;
							bitmap.alpha[(size_t)(y * w + x)] = on ? 255 : 0;
						}
					}
				} else {
					// Unmapped codepoint: draw the usual "tofu" box so that
					// markers and symbols still occupy space.
					for (int y = 0; y < h; y++) {
						for (int x = 0; x < w; x++) {
							const bool border = x == 0 || y == 0 || x == w - 1 ||
												y == h - 1;
							bitmap.alpha[(size_t)(y * w + x)] = border ? 255 : 0;
						}
					}
				}
				d->cache[key] = std::move(bitmap);
				cacheSize_ = d->cache.size();
				if (cacheSize_ > 8192) clearCache();
				return &d->cache[key];
			}

			const size_t faceIndex =
				(size_t)(entry - d->faces.data());
			const uint64_t key =
				impl::cacheKey(codepoint, faceIndex, sizeKey(size), subpixel);
			auto cached = d->cache.find(key);
			if (cached != d->cache.end()) return &cached->second;

			FT_Face face = entry->face;
			FT_Set_Char_Size(face, 0, (FT_F26Dot6)(size * 64.0), 72, 72);
			const FT_UInt index = FT_Get_Char_Index(face, codepoint);

			glyphBitmap bitmap;
			bitmap.advance = 0.0f;
			if (index && FT_Load_Glyph(face, index,
					FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP) == 0) {
				bitmap.advance = (float)face->glyph->advance.x / 64.0f;
				if (subpixel && face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
					FT_Outline_Translate(&face->glyph->outline,
						(FT_Pos)(subpixel * 21), 0);
				}
				if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) == 0) {
					const FT_Bitmap& src = face->glyph->bitmap;
					bitmap.width = (int)src.width;
					bitmap.height = (int)src.rows;
					bitmap.left = face->glyph->bitmap_left;
					bitmap.top = face->glyph->bitmap_top;
					bitmap.alpha.assign((size_t)(bitmap.width * bitmap.height),
						0);
					for (int y = 0; y < bitmap.height; y++) {
						const unsigned char* row = src.buffer +
													(size_t)y * (size_t)src.pitch;
						uint8_t* dst = bitmap.alpha.data() +
									   (size_t)y * (size_t)bitmap.width;
						for (int x = 0; x < bitmap.width; x++) dst[x] = row[x];
					}
				}
			}

			d->cache[key] = std::move(bitmap);
			cacheSize_ = d->cache.size();
			if (cacheSize_ > 8192) clearCache();
			return &d->cache[key];
		}

		void fontManager::clearCache() {
			d->cache.clear();
			cacheSize_ = 0;
		}

	}  // namespace UI
}  // namespace gold
