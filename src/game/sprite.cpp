#include <cstring>
#include "sprite.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bx/timer.h>

#include <iostream>

#include "entity.hpp"
#include "graphics.hpp"
#include "shaderSprite.hpp"
#include "transform.hpp"

namespace gold {
	using namespace std;
	using namespace bgfx;

	binary getSpriteShaderData(shaderType stype);

	struct PosColorVertex {
		float x, y, z;
		float u, v;
	};

	obj& sprite::getPrototype() {
		static auto proto = obj({
			{"priority", priorityEnum::drawPriority},
			{"offset", vec2f(0.5, 0.5)},
			{"size", vec2f(1.0, 1.0)},
			{"area", vec4f(0.0, 0.0, 1.0, 1.0)},
			{"draw", method(&sprite::draw)},
			{"color", vec4f(1, 1, 1, 0)},
			{"opacity", vec4f(1, 1, 1, 1)},
			{"initialize", method(&sprite::initialize)},
			{"destroy", method(&sprite::destroy)},
			{"proto", renderable::getPrototype()},
		});
		return proto;
	}

	void sprite::updateVertexBuffer() {
		auto current = getObject<vertexBuffer>("vbh");
		auto rebuild = getBool("rebuild");
		if (current && rebuild) {
			current.destroy();
			current = vertexBuffer();
			erase("vbh");
		} else if (current)
			return;
		auto tex = getObject<gpuTexture>("texture");
		if (!tex) return;
		auto offset = getVar("offset");
		auto size = getVar("size");
		auto area = getVar("area");
		auto texWidth = tex.getFloat("width");
		auto texHeight = tex.getFloat("height");
		if (!area.isFloating() && area.isNumber()) {
			auto x = area.getFloat(0) / texWidth;
			auto y = area.getFloat(1) / texHeight;
			auto w = area.getFloat(2) / texWidth;
			auto h = area.getFloat(3) / texHeight;
			area = vec4f(x, y, w, h);
		}
		if (!size.isFloating() && size.isNumber()) {
			auto w = (texWidth * area.getFloat(2)) / size.getFloat(0);
			auto h =
				(texHeight * area.getFloat(3)) / size.getFloat(1);
			size = vec2f(w, h);
		}
		if (!offset.isFloating() && offset.isNumber()) {
			auto x = offset.getFloat(0) /
							 (texWidth * area.getFloat(2)) * size.getFloat(0);
			auto y =
				-(offset.getFloat(1) / (texHeight * area.getFloat(3))) *
				size.getFloat(1);
			offset = vec2f(x, y);
		}
		auto width = size.getFloat(0);
		auto height = size.getFloat(1);

		auto tx0 = area.getFloat(0);
		auto ty0 = area.getFloat(1);
		auto tx1 = area.getFloat(0) + area.getFloat(2);
		auto ty1 = -area.getFloat(3) + area.getFloat(1);

		auto px0 = -offset.getFloat(0);
		auto py0 = -offset.getFloat(1);
		auto px1 = -offset.getFloat(0) + width;
		auto py1 = -offset.getFloat(1) + height;
		PosColorVertex verts[4] = {
			{px0, py1, 1, tx0, ty1},
			{px1, py1, 1, tx1, ty1},
			{px0, py0, 1, tx0, ty0},
			{px1, py0, 1, tx1, ty0},
		};
		auto vSize = sizeof(PosColorVertex) * 4;
		auto vBin = binary(vSize);
		vBin.resize(vSize, 0);
		memcpy(vBin.data(), verts, vSize);

		using a = vertexLayout::attrib;
		using aT = vertexLayout::attribType;
		auto layout = vertexLayout::findInCache("Sprite");
		if (!layout) {
			layout = vertexLayout({{"name", "Sprite"}})
								 .begin()
								 .add(a::Position, aT::Float, 3)
								 .add(a::TexCoord0, aT::Float, 2)
								 .end();
		}

		auto vbh = vertexBuffer({
			{"data", vBin},
			{"type", standardBufferType},
			{"layout", layout},
		});
		setObject("vbh", vbh);
		erase("rebuild");
	}

	var sprite::setSize(list args) {
		auto size = list::getVec2f(args);
		setVar("size", size);
		setBool("rebuild", true);
		return var();
	}

	var sprite::setArea(list args) {
		if (args.size() == 0) return genericError("setArea requires a value");
		if (args[0].isVec4()) {
			setVar("area", args[0]);
		} else if (args.size() == 4) {
			if (args.isAllFloating())
				setVar(
					"area",
					vec4f(
						args[0].getFloat(),
						args[1].getFloat(),
						args[2].getFloat(),
						args[3].getFloat()));
			else if (args.isAllNumber())
				setVar(
					"area",
					vec4i32(
						args[0].getInt32(),
						args[1].getInt32(),
						args[2].getInt32(),
						args[3].getInt32()));
		}
		setBool("rebuild", true);
		return var();
	}

	var sprite::setOffset(list args) {
		auto offset = list::getVec2f(args);
		setVar("offset", offset);
		setBool("rebuild", true);
		return var();
	}

	var sprite::draw(list args) {
		auto view = args[0].getUInt16();
		updateVertexBuffer();
		auto vbh = getObject<vertexBuffer>("vbh");
		auto ibh = getObject<indexBuffer>("ibh");
		auto program = getObject<shaderProgram>("program");
		auto tex = getObject<gpuTexture>("texture");
		if (!tex) return genericError("Missing texture on sprite.");

		auto parentObject = getObject<entity>("object");
		auto parentTrans = parentObject.getTransform();
		auto mtx = parentTrans.getWorldMatrix();

		auto color = getVar("color");
		auto opacity = getVar("opacity");

		program.setTransform(mtx);
		shaderProgram::bindTexture("s_texColor", 0, tex);
		shaderProgram::setUniform("u_opacity", opacity.getPtr());
		shaderProgram::setUniform("u_color0", color.getPtr());
		vbh.set(0);
		ibh.set();
		program.defaultState();
		program.submit(view);
		return var();
	}

	var sprite::initialize(list) {
		updateVertexBuffer();
		uint16_t indicies[4 * 3] = {
			0, 1, 2,  // 1
			1, 3, 2,  // 2
			1, 0, 2,  // Backside 1
			1, 2, 3,  // Backside 2
		};
		auto iSize = sizeof(uint16_t) * 4 * 3;
		auto iBin = binary(iSize);
		iBin.resize(iSize, 0);
		memcpy(iBin.data(), indicies, iSize);
		setObject(
			"ibh",
			indexBuffer({
				{"type", standardBufferType},
				{"data", iBin},
			}));

		using uT = uniformType;
		shaderProgram::createUniform("s_texColor", uT::Sampler);
		shaderProgram::createUniform("u_color0", uT::Vec4);
		shaderProgram::createUniform("u_opacity", uT::Vec4);

		auto program = shaderProgram::findInCache("Sprite");
		if (!program)
			program = shaderProgram({
				{"name", "Sprite"},
				{"vert",
				 shaderObject({
					 {"data", getSpriteShaderData(VertexShaderType)},
				 })},
				{"frag",
				 shaderObject({
					 {"data", getSpriteShaderData(FragmentShaderType)},
				 })},
			});
		setObject("program", program);

		return var();
	}

	var sprite::destroy(list) {
		auto ibh = getObject<indexBuffer>("ibh");
		ibh.destroy();
		auto vbh = getObject<vertexBuffer>("vbh");
		vbh.destroy();
		empty();
		return var();
	}

	sprite::sprite() : renderable() {}

	sprite::sprite(obj config) : renderable() {
		setParent(sprite::getPrototype());
		copy(config);
	}

	binary getSpriteShaderData(shaderType stype) {
		auto renderType = getRendererType();
		switch (renderType) {
			case bgfx::RendererType::Direct3D11:
			case bgfx::RendererType::Direct3D12: {
				switch (stype) {
					case VertexShaderType:
						return dx11_vs_sprite;
					case FragmentShaderType:
						return dx11_fs_sprite;
					default:
						break;
				}
			}
			case bgfx::RendererType::Metal: {
				switch (stype) {
					case VertexShaderType:
						return metal_vs_sprite;
					case FragmentShaderType:
						return metal_fs_sprite;
					default:
						break;
				}
			}
			case bgfx::RendererType::OpenGLES: {
				switch (stype) {
					case VertexShaderType:
						return essl_vs_sprite;
					case FragmentShaderType:
						return essl_fs_sprite;
					default:
						break;
				}
			}
			case bgfx::RendererType::OpenGL: {
				switch (stype) {
					case VertexShaderType:
						return glsl_vs_sprite;
					case FragmentShaderType:
						return glsl_fs_sprite;
					default:
						break;
				}
			}
			case bgfx::RendererType::Vulkan: {
				switch (stype) {
					case VertexShaderType:
						return spirv_vs_sprite;
					case FragmentShaderType:
						return spirv_fs_sprite;
					default:
						break;
				}
			}
			case bgfx::RendererType::Gnm:
			case bgfx::RendererType::Nvn:
			case bgfx::RendererType::Noop:
			default:
				break;
		}
		return binary();
	}
}  // namespace gold
