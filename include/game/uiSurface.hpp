#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "graphics.hpp"
#include "renderBackend.hpp"
#include "renderable.hpp"
#include "types.hpp"
#include "ui/renderer.hpp"

namespace gold {

	/**
	 * A UI surface: an HTML/CSS document rendered to a texture and drawn on a
	 * quad in the world - a CRT on a desk, a shop terminal, a wrist display.
	 *
	 * The pixels come from `gold::UI::renderer` (a CPU rasterizer with no GPU
	 * dependency), and are uploaded to the GPU only when the UI actually
	 * changed. A static screen costs one upload at load and nothing per
	 * frame; hover, a click or a game-side `setText` costs one re-render and
	 * one upload.
	 *
	 * ```cpp
	 * auto screen = uiSurface({
	 *     jo("html", markup, jo("css", sheet),
	 *         jo("width", 512.0), jo("height", 384.0),
	 *         jo("size", vec2f(0.32f, 0.24f)))});
	 * screen.on(ja("click", func([](list args) {
	 *     log("pressed", args[0].getObject().getString("cssId"));
	 *     return var();
	 * })));
	 * // per frame, from the engine's draw list
	 * screen.draw(list({view}));
	 * // input: a world-space ray from the player's crosshair
	 * screen.pointer(list({"move", origin, direction}));
	 * ```
	 *
	 * Interaction uses the same box tree the renderer paints, so a ray that
	 * lands on the quad is projected into UI space and dispatched to the
	 * element under it - `:hover`, `:active` and click handlers all work
	 * without the game tracking any UI geometry.
	 */
	struct uiSurface : public renderable {
	 public:
		static object& getPrototype();

		uiSurface();
		uiSurface(object config);
		~uiSurface();

		/**
		 * Build the UI and its texture.
		 *
		 * Config keys: `html`, `css`, `width`, `height` (UI pixels),
		 * `fontSans` / `fontSerif` / `fontMono` (font files),
		 * `background` (page color), `size` (world size of the quad),
		 * `interactive` (route pointer rays to the UI, default true).
		 */
		var initialize(list args = {});
		/** Upload if dirty, then draw the quad for `view`. */
		var draw(list args = {});
		var destroy(list args = {});

		/**
		 * Feed a world-space ray to the UI.
		 *
		 * args: `{type, origin, direction}` where `type` is one of `move`,
		 * `down`, `up` or `leave`. Returns the element descriptor that was
		 * hit, or null when the ray misses the surface.
		 */
		var pointer(list args = {});
		/** Feed UI-space coordinates directly (for a real window/mouse). */
		var pointerAt(list args = {});
		/** The hit element for a world-space ray, without dispatching. */
		var pick(list args = {});

		/** The underlying renderer, for scripts and direct manipulation. */
		var ui(list args = {});
		/** Replace the markup / stylesheet. */
		var setHTML(list args = {});
		var setCSS(list args = {});
		/** Change the UI resolution (re-rasterizes at the new size). */
		var setResolution(list args = {});
		/** Set an element's text by element id. */
		var setText(list args = {});
		/** Set element states (hover/active/focus/checked) from game code. */
		var setState(list args = {});
		/** Register a handler: ("click", func) or ("click", elementId, func). */
		var on(list args = {});
		/** The element's rect in UI pixels, for anchoring 3D widgets. */
		var elementRect(list args = {});
		/** Upload / layout / paint counters, for a debug HUD. */
		var stats(list args = {});
		/** The current document, serialized back to markup (a gold-callable
		 *  view onto the renderer's C++-only markup()). */
		var markup(list args = {});
		/** Query element descriptors by CSS selector (renderer::query).
		 *  An empty selector returns an empty list — use markup() for
		 *  the whole document. */
		var query(list args = {});
		/** Set an element's style declarations by element id
		 *  (renderer::setStyle). */
		var setStyle(list args = {});

	 private:
		/** The renderer instance lives in the OBJECT'S DATA (setPtr),
		 *  not as a C++ facade member: MCP tools and other gold-handle
		 *  dispatch call these methods on reinterpreted copies
		 *  (getObject<uiSurface>), where class members would read
		 *  garbage. The draw path's other members (texture_, quad
		 *  size, counters) are only valid on the instance the app
		 *  holds — facade dispatch of draw() on copies predates this
		 *  and stays instance-only. */
		UI::renderer* renderer() {
			return (UI::renderer*)getPtr("ui");
		}

		/** Create or resize the GPU texture for the current UI size. */
		bool ensureTexture();
		/** Push the renderer's pixels to the GPU. */
		bool uploadTexture();
		/** Project a world ray onto the quad, in UI pixel coordinates. */
		bool projectRay(const var& origin, const var& direction,
			float& outX, float& outY);
		/** Pick the renderer backend from config "renderer" (string or
		 *  list); the built-in software rasterizer is the chain's tail,
		 *  so this never fails. */
		void selectRenderer(object config);

		UI::renderer* ui_ = nullptr;
		string rendererName_ = "software";  // resolved backend name
		renderHandle texture_{};
		uint32_t textureWidth_ = 0;
		uint32_t textureHeight_ = 0;
		float quadWidth_ = 1.0f;
		float quadHeight_ = 1.0f;
		bool interactive_ = true;
		bool hasFrame_ = false;
		uint64_t uploads_ = 0;
		// Per-draw wall-clock delta, fed to the renderer's advance() so
		// CSS animations play without the game loop driving them.
		std::chrono::steady_clock::time_point lastFrame_{};
		bool lastFrameValid_ = false;
	};

	/**
	 * Intersect a ray with the unit quad in the XY plane at `z` (the layout
	 * the UI surface draws in) and return the hit point.
	 *
	 * Exposed for testing: it is the whole pointer-to-UI mapping, with no
	 * engine state involved.
	 */
	bool intersectQuad(
		float originX, float originY, float originZ, float dirX, float dirY,
		float dirZ, float z, float& outX, float& outY);

}  // namespace gold
