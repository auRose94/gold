#include <cstring>
#include "world.hpp"

#include <bgfx/bgfx.h>

#include "camera.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "graphics.hpp"
#include "physicsBackend.hpp"
#include "physicsBody.hpp"
#include "shaderWireframe.hpp"
#include "transform.hpp"

namespace gold {
	using namespace std;
	using namespace bgfx;

	binary getWireframeShaderData(shaderType stype);

	object& world::getPrototype() {
		static object proto = obj({
			{"bodies", list()},
			{"gravity", vec3f(0, -9.8, 0)},
			{"maxSubSteps", 1},
			{"fixedTimeStep", 1.0 / 60.0},
		});
		return proto;
	}

	world::world() : object() {}

	world::world(object config) : object(config) {
		setParent(getPrototype());
	}

	var world::step(list args) {
		auto backend = (physicsBackend*)getPtr("physicsBackend");
		if (!backend) return var();
		auto fixedTimeStep = getFloat("fixedTimeStep");
		auto timeStep = args.getFloat(0, fixedTimeStep);
		auto maxSubSteps = getInt32("maxSubSteps");
		// The backend advances the world and syncs each body's entity
		// transform (including pruning bodies gone from the list).
		backend->step(*this, timeStep, maxSubSteps, fixedTimeStep);
		return var();
	}

	var world::setGravity(list args) {
		float g[3] = {0, 0, 0};
		if (args.isAllNumber() && args.size() >= 3)
			args.assign(typeFloat, g, 3);
		else if (args.size() >= 1 && args[0].isVec3()) {
			auto v = args[0];
			g[0] = v.getFloat(0);
			g[1] = v.getFloat(1);
			g[2] = v.getFloat(2);
		}
		auto ret = vec3f(g[0], g[1], g[2]);
		setVar("gravity", ret);
		if (auto backend = (physicsBackend*)getPtr("physicsBackend"))
			backend->setGravity(*this, g);
		return ret;
	}

	var world::raytrace(list args) {
		float from[3];
		float to[3];
		args[0].getList().assign(typeFloat, from, 3);
		args[1].getList().assign(typeFloat, to, 3);
		return var();
	}

	var world::initialize(list args) {
		setList("bodies", list({}));
		auto engIt = args.find(engine::getPrototype());
		if (engIt != args.end()) {
			auto eng = engIt->getObject<engine>();
			setObject("engine", eng);
		} else {
			return genericError("Expected engine to be first arg");
		}

		// Physics backend: "bullet" ships as the libgoldBullet plugin;
		// "none" is the built-in no-op fallback, so the world facade
		// still works on machines with no physics engine.
		auto names = list();
		const auto physics = getString("physics", "bullet");
		if (physics != "none") names.pushString(physics);
		names.pushString("none");
		auto backend = createPhysicsBackend(names);
		if (!backend) return genericError("No physics backend available");
		setPtr("physicsBackend", backend);

		// The wireframe debug program + uniform are created lazily on the
		// first flush (bgfx must be initialized; the engine boot order
		// guarantees graphics before the world, but a bare-bootstrap world
		// may not have run yet). The backend's drawer only batches into
		// this sink.
		auto debugSink = func([](list args) -> var {
			auto lines = args[0].getBinary();
			auto layout = vertexLayout::findInCache("Wireframe");
			if (!layout)
				layout = vertexLayout({{"name", "Wireframe"}})
							 .begin()
							 .add(vertexLayout::attrib::Position,
								 vertexLayout::attribType::Float, 3)
							 .add(vertexLayout::attrib::Color0,
								 vertexLayout::attribType::Float, 4)
							 .end();
			if (!layout || lines.size() == 0) return var();
			auto program = shaderProgram::findInCache("Wireframe");
			if (!program)
				program = shaderProgram(
					{{"name", "Wireframe"},
						{"vert",
							shaderObject({{"data",
								getWireframeShaderData(
									VertexShaderType)}})},
						{"frag",
							shaderObject({{"data",
								getWireframeShaderData(
									FragmentShaderType)}})}});
			if (!program) return var();
			shaderProgram::createUniform("u_thickness", uniformType::Vec4);
			auto vbh = vertexBuffer({
				{"count", (int64_t)(lines.size() / 28)},
				{"type", transientBufferType},
				{"layout", layout},
			});
			vbh.update(lines);
			vbh.set(0);
			float u_thickness[4] = {2, 0, 0, 0};
			shaderProgram::setUniform("u_thickness", u_thickness);
			program.setState({
				{"type", "lines"},
				{"MSAA", true},
				{"lineAA", true},
			});
			program.submit(uint16_t(0));
			return var();
		});

		if (!backend->createWorld(*this, debugSink)) {
			setPtr("physicsBackend", nullptr);
			delete backend;
			return genericError("Physics backend failed to initialize");
		}
		return var();
	}

	var world::destroy() {
		auto backend = (physicsBackend*)getPtr("physicsBackend");
		if (backend) {
			backend->destroyWorld(*this);
			delete backend;
			setPtr("physicsBackend", nullptr);
		}
		empty();
		return var();
	}

	var world::debugDraw() {
		auto backend = (physicsBackend*)getPtr("physicsBackend");
		if (!backend) return genericError("World is not initialized");
		backend->debugDraw(*this);
		return var();
	}

	// The wireframe shader blob for the running bgfx renderer type; kept
	// game-side with the embedded shader arrays until the render facade
	// is confined behind its backend interface.
	binary getWireframeShaderData(shaderType stype) {
		auto renderType = getRendererType();
		switch (renderType) {
			case bgfx::RendererType::Direct3D11:
			case bgfx::RendererType::Direct3D12: {
				switch (stype) {
					case VertexShaderType:
						return dx11_vs_wireframe;
					case FragmentShaderType:
						return dx11_fs_wireframe;
					default:
						break;
				}
				break;
			}
			case bgfx::RendererType::Metal: {
				switch (stype) {
					case VertexShaderType:
						return metal_vs_wireframe;
					case FragmentShaderType:
						return metal_fs_wireframe;
					default:
						break;
				}
				break;
			}
			case bgfx::RendererType::OpenGLES: {
				switch (stype) {
					case VertexShaderType:
						return essl_vs_wireframe;
					case FragmentShaderType:
						return essl_fs_wireframe;
					default:
						break;
				}
				break;
			}
			case bgfx::RendererType::OpenGL: {
				switch (stype) {
					case VertexShaderType:
						return glsl_vs_wireframe;
					case FragmentShaderType:
						return glsl_fs_wireframe;
					default:
						break;
				}
				break;
			}
			case bgfx::RendererType::Vulkan: {
				switch (stype) {
					case VertexShaderType:
						return spirv_vs_wireframe;
					case FragmentShaderType:
						return spirv_fs_wireframe;
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