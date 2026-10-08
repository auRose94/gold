// The inert shim: used when the system FreeType development headers are
// absent at configure time (or the option is off). Every call is a no-op
// and fontManager falls back to its built-in 5x7 font.

#include "ui/ftShim.h"

namespace gold {
	namespace ftshim {

		bool isOpen() { return false; }

		bool open() { return false; }

		void* openFaceFile(const std::string&) { return nullptr; }

		void* openFaceMemory(const unsigned char*, size_t) {
			return nullptr;
		}

		void closeFace(void*) {}

		bool setSize(void*, float) { return false; }

		faceMetrics metrics(void*, float) { return {}; }

		glyphInfo loadGlyph(void*, uint32_t, bool) { return {}; }

		void translateOutline(void*, int32_t) {}

		bool rasterize(void*) { return false; }

		bitmapView bitmapOf(void*) { return {}; }

		uint32_t charIndex(void*, uint32_t) { return 0; }

		bool hasKerning(void*) { return false; }

		float kerning(void*, uint32_t, uint32_t) { return 0.0f; }

	}  // namespace ftshim
}  // namespace gold