#pragma once

#include <AppCore/AppCore.h>

#include "ui/renderer.hpp"
#include "ui/raster.hpp"
#include "ui/tree.hpp"
#include "ui/layout.hpp"

namespace gold {
	namespace UI {

		/**
		 * An Ultralight-powered implementation of the UI renderer.
		 * This replaces the software rasterizer with a
		 * high-performance WebKit engine.
		 */
		struct ultralight_renderer : public renderer {
			ultralight_renderer();
			virtual ~ultralight_renderer() override;

			virtual const char* name() const override { return "ultralight"; }

			// -------------------------------------------------
			// lifecycle
			virtual var load(list args) override;
			virtual var setHTML(list args) override;
			virtual var setCSS(list args) override;
			virtual var setViewport(list args) override;
			virtual var setFonts(list args) override;
			virtual var loadFont(list args) override;

			virtual var advance(list args) override;
			virtual var render(list args) override;
			virtual var needsRender(list args) override;
			virtual var invalidate(list args) override;

			// ----------------------------------------------------
			// reading
			virtual var surface(list args) override;
			virtual var pixels(list args) override;
			// The WebKit backend renders to its own surface; these expose an
			// (empty) software-shaped view so the interface is satisfied.
			virtual rasterTarget& target() override { return target_; }
			virtual const rasterTarget& target() const override { return target_; }
			virtual string markup() const override { return markup_; }
			virtual const domTree& dom() const override { return tree_; }
			virtual const layoutResult& boxes() const override { return layout_; }
			virtual int nodeById(uint32_t id) const override { return -1; }
			virtual bool dirty() const override;
			virtual frameStats stats() const override;

			// ------------------------------------------------
			// interaction
			virtual int hitTest(float x, float y) const override;
			virtual var hit(list args) override;
			virtual var dispatch(list args) override;
			virtual var on(list args) override;
			virtual var setState(list args) override;
			virtual var query(list args) override;
			virtual var element(list args) override;
			virtual var elementRect(list args) override;
			virtual var setText(list args) override;
			virtual var setStyle(list args) override;
			virtual var interactionState(list args) override;

		 private:
			ultralight::RefPtr<ultralight::Renderer> renderer_;
			ultralight::RefPtr<ultralight::View> view_;

			float width_ = 640.0f;
			float height_ = 480.0f;
			bool dirty_ = true;

			// Interface-shaped views of the WebKit document (kept empty; the
			// pixels live in the Ultralight surface, see `surface()`/`pixels()`).
			rasterTarget target_;
			domTree tree_;
			layoutResult layout_;
			string markup_;

			void updateViewport(float w, float h);
		};

	}  // namespace UI
}  // namespace gold
