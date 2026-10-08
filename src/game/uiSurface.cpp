#include "uiSurface.hpp"

#include <cmath>
#include <cstring>

#include "entity.hpp"
#include "goldjs.hpp"
#include "ui/software_renderer.hpp"
#include "shaderSprite.hpp"
#include "sprite.hpp"
#include "transform.hpp"

namespace gold {

	using namespace gold::UI;

	namespace {
		// A textured quad in the XY plane, matching the sprite vertex layout
		// so the shared "Sprite" program can draw it.
		struct quadVertex {
			float x, y, z;
			float u, v;
		};

		constexpr uint16_t kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
	}  // namespace

	bool intersectQuad(float originX, float originY, float originZ,
		float dirX, float dirY, float dirZ, float z, float& outX,
		float& outY) {
		// A ray parallel to the quad never hits it.
		if (std::fabs(dirZ) < 1e-6f) return false;
		const float t = (z - originZ) / dirZ;
		if (t < 0.0f) return false;  // behind the camera
		outX = originX + dirX * t;
		outY = originY + dirY * t;
		return true;
	}

	object& uiSurface::getPrototype() {
		static auto proto = obj({
			{"initialize", method(&uiSurface::initialize)},
			{"draw", method(&uiSurface::draw)},
			{"destroy", method(&uiSurface::destroy)},
			{"pointer", method(&uiSurface::pointer)},
			{"pointerAt", method(&uiSurface::pointerAt)},
			{"pick", method(&uiSurface::pick)},
			{"ui", method(&uiSurface::ui)},
			{"setHTML", method(&uiSurface::setHTML)},
			{"setCSS", method(&uiSurface::setCSS)},
			{"setResolution", method(&uiSurface::setResolution)},
			{"setText", method(&uiSurface::setText)},
			{"setState", method(&uiSurface::setState)},
			{"on", method(&uiSurface::on)},
			{"elementRect", method(&uiSurface::elementRect)},
			{"stats", method(&uiSurface::stats)},
			{"priority", priorityEnum::drawPriority},
			{"proto", renderable::getPrototype()},
		});
		return proto;
	}

	uiSurface::uiSurface() {
		setParent(getPrototype());
		selectRenderer(object());
	}

	uiSurface::uiSurface(object config) : uiSurface() {
		initialize(list({config}));
	}

	uiSurface::~uiSurface() {
		destroy(list());
		delete ui_;
		ui_ = nullptr;
	}

	void uiSurface::selectRenderer(object config) {
		// Chain: any config names first (string or list; "auto" entries
		// skipped — nothing to probe by default), then the guaranteed
		// software tail. A missing optional backend (e.g. an ultralight
		// that was never built) is simply absent from the chain's outcome.
		auto names = list();
		const auto cfg = config.getVar("renderer");
		if (cfg.getType() == typeList) names = cfg.getList();
		else if (cfg.isString() && string(cfg).size() > 0 &&
				 string(cfg) != "auto")
			names.pushString(string(cfg));

		UI::renderer* chosen = UI::createRenderer(names);
		const string name = chosen ? chosen->name() : "software";
		// Reinitialization with the same backend keeps the live renderer
		// (and its loaded document); a new backend replaces it.
		if (ui_ && name == rendererName_ && chosen && ui_ != chosen) {
			delete chosen;
			return;
		}
		if (ui_ != chosen) {
			delete ui_;
			ui_ = chosen;
		}
		if (!ui_) ui_ = new UI::software_renderer();
		rendererName_ = string(ui_->name());
		setString("renderer", rendererName_);
	}

	var uiSurface::initialize(list args) {
		object config;
		if (args.size() == 1 && args[0].isObject()) config = args[0].getObject();
		else if (args.size() == 1 && args[0].isList()) config = object();

		selectRenderer(config);

		// Everything the renderer needs travels as one config object, so the
		// gold façade and C++ callers configure it the same way.

		// Everything the renderer needs travels as one config object, so the
		// gold façade and C++ callers configure it the same way.
		object rendererConfig = obj({
			{"html", config.getString("html")},
			{"css", config.getString("css")},
			{"width", config.getDouble("width", 256.0)},
			{"height", config.getDouble("height", 192.0)},
			{"sans", config.getString("fontSans")},
			{"serif", config.getString("fontSerif")},
			{"mono", config.getString("fontMono")},
		});
		if (config.getType("background") != typeNull)
			rendererConfig.setString("background",
				config.getString("background"));
		ui_->load(list({rendererConfig}));

		const var size = config.getVar("size");
		if (size.isVec2() || size.isFloating() || size.isNumber()) {
			quadWidth_ = size.isVec2() ? size.getFloat(0) : (float)size.getDouble();
			quadHeight_ = size.isVec2() ? size.getFloat(1) : quadWidth_;
		} else {
			// Default to a 4:3 screen a little wider than it is tall, so the
			// quad reads as a monitor rather than a sticker.
			quadWidth_ = 0.32f;
			quadHeight_ = 0.24f;
		}
		interactive_ = config.getBool("interactive", true);
		ensureTexture();
		ui_->paint();
		uploadTexture();

		// Index buffer for the quad, shared shape with `sprite`.
		const size_t indexBytes = sizeof(kQuadIndices);
		binary indices(indexBytes);
		std::memcpy(indices.data(), kQuadIndices, indexBytes);
		setObject("ibh",
			indexBuffer({
				{"type", standardBufferType},
				{"data", indices},
			}));
		setBool("rebuild", true);
		return var(this);
	}

	bool uiSurface::ensureTexture() {
		const uint32_t width = ui_->target().width;
		const uint32_t height = ui_->target().height;
		if (width == 0 || height == 0) return false;
		if (texture_.valid() && width == textureWidth_ &&
			height == textureHeight_)
			return true;

		renderBackend* backend = gfxBackend::backend();
		if (!backend || !backend->isValid()) return false;
		if (texture_.valid()) backend->destroyTexture(texture_);

		// Premultiplied RGBA8: the UI blends its own layers, and the shader
		// treats the result as straight alpha, so ask for the straight copy.
		const binary pixels = ui_->target().unpremultiply();
		texture_ = backend->createTexture2D((uint16_t)width, (uint16_t)height,
			false, 1, texFormat::RGBA8,
			BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
				BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
			pixels.data(), (uint32_t)pixels.size());
		textureWidth_ = width;
		textureHeight_ = height;
		return texture_.valid();
	}

	bool uiSurface::uploadTexture() {
		renderBackend* backend = gfxBackend::backend();
		if (!backend || !texture_.valid()) return false;
		if (!ensureTexture()) return false;
		const binary pixels = ui_->target().unpremultiply();
		backend->updateTexture(texture_, 0, 0, pixels.data(),
			(uint32_t)pixels.size());
		uploads_++;
		return true;
	}

	var uiSurface::draw(list args) {
		if (args.size() == 0) return genericError("uiSurface draw requires a view");
		const auto view = args[0].getUInt16();

		// The renderer only does work when something changed, so a static
		// screen costs one comparison per frame.
		if (ui_->dirty()) {
			ui_->paint();
			uploadTexture();
		}

		auto program = getObject<shaderProgram>("program");
		if (!program) {
			program = shaderProgram::findInCache("Sprite");
			if (!program) {
				binary vertexData(getSpriteShaderData(VertexShaderType));
				binary fragmentData(getSpriteShaderData(FragmentShaderType));
				program = shaderProgram({
					{"name", "Sprite"},
					{"vert", shaderObject({{"data", vertexData}})},
					{"frag", shaderObject({{"data", fragmentData}})},
				});
			}
			shaderProgram::createUniform("s_texColor", uniformType::Sampler);
			shaderProgram::createUniform("u_opacity", uniformType::Vec4);
			shaderProgram::createUniform("u_color0", uniformType::Vec4);
			setObject("program", program);
		}
		auto tex = getObject<gpuTexture>("texture");
		if (!tex) return genericError("uiSurface has no texture");

		// (Re)build the quad when the size or resolution changed.
		if (getBool("rebuild", false) || ui_->target().width != textureWidth_ ||
			ui_->target().height != textureHeight_) {
			quadVertex verts[4] = {
				{0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
				{quadWidth_, 0.0f, 0.0f, 1.0f, 1.0f},
				{0.0f, quadHeight_, 0.0f, 0.0f, 0.0f},
				{quadWidth_, quadHeight_, 0.0f, 1.0f, 0.0f},
			};
			const size_t vertexBytes = sizeof(quadVertex) * 4;
			binary vertexData(vertexBytes);
			std::memcpy(vertexData.data(), verts, vertexBytes);

			vertexLayout layout = vertexLayout::findInCache("Sprite");
			if (!layout)
				layout = vertexLayout({{"name", "Sprite"}})
							 .begin()
							 .add(vertexLayout::attrib::Position,
								 vertexLayout::attribType::Float, 3)
							 .add(vertexLayout::attrib::TexCoord0,
								 vertexLayout::attribType::Float, 2)
							 .end();
			setObject("vbh",
				vertexBuffer({
					{"data", vertexData},
					{"type", standardBufferType},
					{"layout", layout},
				}));
			erase("rebuild");
		}

		auto parentObject = getObject<entity>("object");
		if (!parentObject) return genericError("uiSurface has no entity");
		auto mtx = parentObject.getTransform().getWorldMatrix();
		program.setTransform(mtx);
		shaderProgram::bindTexture("s_texColor", 0, tex);
		var opaque = vec4f(1.0f, 1.0f, 1.0f, 1.0f);
		shaderProgram::setUniform("u_opacity", opaque.getPtr(), 1);
		shaderProgram::setUniform("u_color0", opaque.getPtr(), 1);		getObject<vertexBuffer>("vbh").set(0);
		getObject<indexBuffer>("ibh").set();
		program.defaultState();
		program.submit(view);
		return var(this);
	}
	bool uiSurface::projectRay(const var& origin, const var& direction,
		float& outX, float& outY) const {
		// Project the ray onto the quad's plane, then into 0..1 UV, then into
		// UI pixels. Anything outside the quad is not a hit.
		float x = 0.0f, y = 0.0f;
		if (!intersectQuad(origin.getFloat(0), origin.getFloat(1),
				origin.getFloat(2), direction.getFloat(0), direction.getFloat(1),
				direction.getFloat(2), 0.0f, x, y))
			return false;
		if (quadWidth_ <= 0.0f || quadHeight_ <= 0.0f) return false;
		const float u = x / quadWidth_;
		const float v = y / quadHeight_;
		if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return false;
		outX = u * (float)ui_->target().width;
		// The texture's V axis runs bottom-up, the renderer's top-down.
		outY = (1.0f - v) * (float)ui_->target().height;
		return true;
	}

	var uiSurface::pointer(list args) {
		if (!interactive_ || args.size() < 3) return var();
		const string type = args[0].getString();
		if (type == "leave") return ui_->dispatch(list({"leave", -1.0, -1.0}));
		float x = 0.0f, y = 0.0f;
		if (!projectRay(args[1], args[2], x, y))
			return ui_->dispatch(list({"leave", -1.0, -1.0}));
		return ui_->dispatch(list({type, (double)x, (double)y}));
	}

	var uiSurface::pointerAt(list args) {
		if (!interactive_ || args.size() < 3) return var();
		list forwarded;
		forwarded.pushVar(args[0]);
		forwarded.pushVar(args[1]);
		forwarded.pushVar(args[2]);
		if (args.size() > 3) forwarded.pushVar(args[3]);
		return ui_->dispatch(forwarded);
	}

	var uiSurface::pick(list args) {
		if (args.size() < 2) return var();
		float x = 0.0f, y = 0.0f;
		if (!projectRay(args[0], args[1], x, y)) return var();
		return ui_->hit(list({(double)x, (double)y}));
	}

	var uiSurface::ui(list) {
		return var(*ui_);
	}

	var uiSurface::setHTML(list args) {
		ui_->setHTML(args);
		return var(this);
	}

	var uiSurface::setCSS(list args) {
		ui_->setCSS(args);
		return var(this);
	}

	var uiSurface::setResolution(list args) {
		ui_->setViewport(args);
		erase("rebuild");
		return var(this);
	}

	var uiSurface::setText(list args) {
		return ui_->setText(args);
	}

	var uiSurface::setState(list args) {
		return ui_->setState(args);
	}

	var uiSurface::on(list args) {
		ui_->on(args);
		return var(this);
	}

	var uiSurface::elementRect(list args) {
		return ui_->elementRect(args);
	}

	var uiSurface::stats(list) {
		const UI::frameStats& s = ui_->stats();
		return jo("uploads", (int64_t)uploads_, "paints", (int64_t)s.paints,
			"layoutPasses", (int64_t)s.layoutPasses, "stylePasses",
			(int64_t)s.stylePasses, "width", (int64_t)textureWidth_, "height",
			(int64_t)textureHeight_, "nodes", (int64_t)s.nodeCount);
	}

	var uiSurface::destroy(list) {
		if (texture_.valid()) {
			if (renderBackend* backend = gfxBackend::backend())
				backend->destroyTexture(texture_);
			texture_ = renderHandle();
		}
		textureWidth_ = textureHeight_ = 0;
		return var(this);
	}

}  // namespace gold
