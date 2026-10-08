#include "ui/ultralight_renderer.hpp"
#include <AppCore/AppCore.h>
#include <algorithm>
#include <vector>

#include "goldjs.hpp"
#include "plugin.hpp"

namespace gold {
	namespace UI {

		ultralight_renderer::ultralight_renderer() {
			setParent(renderer::getPrototype());

			// Initialize Ultralight
			renderer_ = ultralight::Renderer::Create();
			view_ = renderer_->CreateView(640, 480, ultralight::ViewConfig(), nullptr);
		}

		ultralight_renderer::~ultralight_renderer() {
			// RefPtr handles cleanup
		}

		var ultralight_renderer::load(list args) {
			object config;
			if (args.size() == 1 && args[0].isObject()) config = args[0].getObject();
			else if (args.size() > 0) config = object(jo("html", args[0]));

			if (config) {
				if (config.getType("html") != typeNull)
					setHTML(list({config.getString("html")}));
				if (config.getType("css") != typeNull)
					setCSS(list({config.getString("css")}));
				if (config.getType("width") != typeNull) {
					const float w = (float)config.getDouble("width");
					updateViewport(w, 480.0f);
				}
				if (config.getType("height") != typeNull) {
					const float h = (float)config.getDouble("height");
					updateViewport(640.0f, h);
				}
			}
			return var(this);
		}

		var ultralight_renderer::setHTML(list args) {
			string html = args.size() == 0 ? "" : args[0].getString();
			view_->LoadHTML(html.c_str());
			return var(this);
		}

		var ultralight_renderer::setCSS(list args) {
			string css = args.size() == 0 ? "" : args[0].getString();
			string styleTag = tpl("<style>", css, "</style>");
			view_->EvaluateScript(tpl("document.head.innerHTML += '", styleTag, "';").c_str());
			return var(this);
		}

		var ultralight_renderer::setViewport(list args) {
			float w = 640.0f, h = 480.0f;
			if (args.size() >= 2) {
				w = (float)args[0].getDouble();
				h = (float)args[1].getDouble();
			}
			updateViewport(w, h);
			return var(this);
		}

		// Font management is a no-op here: the WebKit backend resolves its own
		// fonts. Kept so the interface is satisfied.
		var ultralight_renderer::setFonts(list args) {
			return var(this);
		}

		var ultralight_renderer::loadFont(list args) {
			return var(this);
		}

		void ultralight_renderer::updateViewport(float w, float h) {
			width_ = w;
			height_ = h;
			view_->Resize(uint32_t(w), uint32_t(h));
			dirty_ = true;
		}

		var ultralight_renderer::advance(list args) {
			renderer_->Update();
			renderer_->Render();
			return dirty_;
		}

		var ultralight_renderer::render(list args) {
			dirty_ = false;
			return true;
		}

		var ultralight_renderer::needsRender(list args) {
			return dirty_;
		}

		var ultralight_renderer::invalidate(list args) {
			dirty_ = true;
			return var(true);
		}

		var ultralight_renderer::surface(list args) {
			ultralight::BitmapSurface* surface = (ultralight::BitmapSurface*)view_->surface();
			if (!surface) return var();

			ultralight::RefPtr<ultralight::Bitmap> bitmap = surface->bitmap();
			if (!bitmap) return var();
			auto locked = bitmap->LockPixelsSafe();
			if (!locked) return var();

			// The surface contract is premultiplied RGBA8; Ultralight's
			// BitmapSurface is premultiplied BGRA — swap red/blue once
			// here so consumers (and uiSurface's unpremultiply()) see the
			// documented format.
			std::vector<unsigned char> pixels(
				(unsigned char*)locked.data(), (unsigned char*)locked.data() + locked.size());
			for (size_t i = 0; i + 3 < pixels.size(); i += 4)
				std::swap(pixels[i], pixels[i + 2]);
			return jo(
				"width", (int64_t)width_,
				"height", (int64_t)height_,
				"pixels", binary(pixels.data(), pixels.data() + pixels.size())
			);
		}

		var ultralight_renderer::pixels(list args) {
			ultralight::BitmapSurface* surface = (ultralight::BitmapSurface*)view_->surface();
			if (!surface) return var();
			ultralight::RefPtr<ultralight::Bitmap> bitmap = surface->bitmap();
			if (!bitmap) return var();
			auto locked = bitmap->LockPixelsSafe();
			if (!locked) return var();
			std::vector<unsigned char> pixels(
				(unsigned char*)locked.data(), (unsigned char*)locked.data() + locked.size());
			for (size_t i = 0; i + 3 < pixels.size(); i += 4)
				std::swap(pixels[i], pixels[i + 2]);
			return var(binary(pixels.data(), pixels.data() + pixels.size()));
		}

		bool ultralight_renderer::dirty() const {
			return dirty_;
		}

		frameStats ultralight_renderer::stats() const {
			return {};
		}

		int ultralight_renderer::hitTest(float x, float y) const {
			return -1;
		}

		var ultralight_renderer::hit(list args) {
			return var();
		}

		var ultralight_renderer::dispatch(list args) {
			string type = args[0].getString();
			double x = args[1].getDouble();
			double y = args[2].getDouble();

			ultralight::MouseEvent evt;
			evt.x = (int)x;
			evt.y = (int)y;
			evt.button = ultralight::MouseEvent::kButton_None;
			if (type == "move") {
				evt.type = ultralight::MouseEvent::kType_MouseMoved;
			} else if (type == "down") {
				evt.type = ultralight::MouseEvent::kType_MouseDown;
				evt.button = ultralight::MouseEvent::kButton_Left;
			} else if (type == "up") {
				evt.type = ultralight::MouseEvent::kType_MouseUp;
				evt.button = ultralight::MouseEvent::kButton_Left;
			} else {
				return var();
			}
			view_->FireMouseEvent(evt);
			return var();
		}

		var ultralight_renderer::on(list args) {
			return var(true);
		}

		var ultralight_renderer::setState(list args) {
			return var();
		}

		var ultralight_renderer::query(list args) {
			return var();
		}

		var ultralight_renderer::element(list args) {
			return var();
		}

		var ultralight_renderer::elementRect(list args) {
			return var();
		}

		var ultralight_renderer::setText(list args) {
			string id = args[0].getString();
			string text = args[1].getString();
			string js = tpl("document.getElementById('", id, "').innerText = '", text, "';");
			view_->EvaluateScript(js.c_str());
			return var(true);
		}

		var ultralight_renderer::setStyle(list args) {
			return var();
		}

		var ultralight_renderer::interactionState(list args) {
			return var();
		}

		// Plugin registrar: loading libgoldUltralight (the plugin naming
		// for backend "ultralight") makes this renderer reachable through
		// UI::createRenderer("ultralight") and the uiSurface "renderer"
		// chain.
		namespace {
			struct ultralightRegistrar {
				ultralightRegistrar() {
					registerRenderer("ultralight",
						[]() -> renderer* {
							return new ultralight_renderer();
						});
				}
			};
			ultralightRegistrar ultralightReg;
		}  // namespace

	} // namespace UI
} // namespace gold
