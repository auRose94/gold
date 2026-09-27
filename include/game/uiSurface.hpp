#pragma once

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

	 private:
		/** Create or resize the GPU texture for the current UI size. */
		bool ensureTexture();
		/** Push the renderer's pixels to the GPU. */
		bool uploadTexture();
		/** Project a world ray onto the quad, in UI pixel coordinates. */
		bool projectRay(const var& origin, const var& direction,
			float& outX, float& outY) const;

		UI::renderer* ui_ = nullptr;
		renderHandle texture_{};
		uint32_t textureWidth_ = 0;
		uint32_t textureHeight_ = 0;
		float quadWidth_ = 1.0f;
		float quadHeight_ = 1.0f;
		bool interactive_ = true;
		bool hasFrame_ = false;
		uint64_t uploads_ = 0;
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
