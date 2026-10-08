#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"

namespace gold {
	namespace UI {

		using std::string;
		using std::vector;

		// Forward declarations for the reading API below; concrete renderers
		// include the real headers.
		struct rasterTarget;
		struct domTree;
		struct layoutResult;

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
		 * Base interface for UI renderers.
		 *
		 * Pure virtual: a host picks a concrete backend (`software_renderer`
		 * for the CPU rasterizer, `ultralight_renderer` for WebKit) and drives
		 * it through this interface. The reading API (`target()`, `boxes()`,
		 * …) returns the backend's pixel/box state so a game or test can pull
		 * it out without knowing which engine is underneath.
		 */
		struct renderer : public object {
			static object& getPrototype();

			virtual ~renderer() = default;

			/** Backend name ("software", "ultralight", ...). */
			virtual const char* name() const = 0;

			// ------------------------------------------------- lifecycle
			virtual var load(list args) { return var(); }
			virtual var setHTML(list args) { return var(); }
			virtual var setCSS(list args) { return var(); }
			virtual var setViewport(list args) { return var(); }
			virtual var setFonts(list args) { return var(); }
			virtual var loadFont(list args) { return var(); }

			virtual var advance(list args) { return var(); }
			virtual var render(list args) { return var(); }
			virtual var needsRender(list args) { return var(); }
			virtual var invalidate(list args) { return var(); }

			// ---------------------------------------------------- reading
			virtual var surface(list args) { return var(); }
			virtual var pixels(list args) { return var(); }
			/** The pixel target, for zero-copy GPU uploads. */
			virtual rasterTarget& target() = 0;
			virtual const rasterTarget& target() const = 0;
			/** Serialize the current document back to markup. */
			virtual string markup() const = 0;
			/** The parsed DOM tree. */
			virtual const domTree& dom() const = 0;
			/** The laid-out box tree. */
			virtual const layoutResult& boxes() const = 0;
			/** Look up a node by the stable id handed out in descriptors. */
			virtual int nodeById(uint32_t id) const = 0;
			virtual bool dirty() const { return false; }
			virtual frameStats stats() const { return {}; }

			// ------------------------------------------------ interaction
			virtual int hitTest(float x, float y) const { return -1; }
			virtual var hit(list args) { return var(); }
			virtual var dispatch(list args) { return var(); }
			virtual var on(list args) { return var(); }
			virtual var setState(list args) { return var(); }
			virtual var query(list args) { return var(); }
			virtual var element(list args) { return var(); }
			virtual var elementRect(list args) { return var(); }
			virtual var setText(list args) { return var(); }
			virtual var setStyle(list args) { return var(); }
			virtual var interactionState(list args) { return var(); }

			// ------------------------------------------- C++ convenience
			void setViewportSize(float w, float h) {
				setViewport(list({(double)w, (double)h}));
			}
			void setMarkup(const string& html) { setHTML(list({html})); }
			void setStyleSheet(const string& css) { setCSS(list({css})); }
			void step(float dt) { advance(list({(double)dt})); }
			bool paint() { return render(list()).getBool(); }
		};

		using createRendererFn = renderer* (*)();

		/**
		 * Register a renderer backend factory ("software", "ultralight",
		 * ...). Static registrars (see renderer.cpp) keep built-ins
		 * available; plugins register the same way when loaded.
		 */
		void registerRenderer(const std::string& name,
			createRendererFn factory);
		/** Create a renderer by backend name; a miss tries plugin::load
		 *  first (plugins self-register), then stays null. */
		renderer* createRenderer(const std::string& name);
		/** Ordered fallback chain; the first name that produces a renderer
		 *  wins ("software" should be a chain's guaranteed tail). */
		renderer* createRenderer(const list& names);
	}
}
