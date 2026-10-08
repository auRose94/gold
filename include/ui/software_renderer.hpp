#pragma once

#include "renderer.hpp"
#include "tree.hpp"
#include "style.hpp"
#include "layout.hpp"
#include "raster.hpp"
#include "font.hpp"

namespace gold {
	namespace UI {

		struct software_renderer : public renderer {
			software_renderer();
			virtual ~software_renderer() override;

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

			virtual var surface(list args) override;
			virtual var pixels(list args) override;
			virtual rasterTarget& target() override { return target_; }
			virtual const rasterTarget& target() const override { return target_; }
			virtual frameStats stats() const override { return stats_; }

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

			// Dirty includes an external DOM mutation: the fingerprint of
			// the shared gold tree is checked here too, so a game loop
			// calling `needsRender()` sees its own data edits.
			virtual bool dirty() const override {
				return styleDirty_ || layoutDirty_ || paintDirty_ ||
					   tree_.fingerprint() != fingerprint_;
			}
			virtual string markup() const override { return tree_.toHTML(); }
			virtual const domTree& dom() const override { return tree_; }
			virtual const layoutResult& boxes() const override { return layout_; }
			virtual int nodeById(uint32_t id) const override { return tree_.byId(id); }

		private:
			void markStyleDirty();
			void markLayoutDirty();
			void markPaintDirty();
			void refreshFingerprint();
			void paintPage();
			void paintBox(rasterizer& raster, int node, float inheritedOpacity);
			bool hasClippedAncestor(int node) const;
			object describe(int node) const;
			list fireEvent(const string& type, int node, const var& data, const object& target);
			void syncHover(int node);

			domTree tree_;
			stylesheet sheet_;
			styleContext styleCtx_;
			vector<computedStyle> styles_;
			layoutResult layout_;
			layoutContext layoutCtx_;
			rasterTarget target_;
			fontManager fonts_;
			frameStats stats_;

			bool styleDirty_ = true;
			bool layoutDirty_ = true;
			bool paintDirty_ = true;
			uint32_t fingerprint_ = 0;
			string html_;
			string css_;
			color pageBackground_;

			int hoverNode_ = -1;
			int activeNode_ = -1;
			int focusNode_ = -1;

			std::map<string, std::map<uint32_t, vector<func>>> handlers_;
		};

	}
}
