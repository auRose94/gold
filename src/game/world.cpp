#include <cstring>
#include "world.hpp"

#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <bgfx/bgfx.h>
#include <btBulletCollisionCommon.h>

#include "camera.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "graphics.hpp"
#include "physicsBody.hpp"
#include "shaderWireframe.hpp"
#include "transform.hpp"

namespace gold {
	using namespace std;
	using namespace bgfx;

	class btDebugDraw;

	binary getWireframeShaderData(shaderType stype);

	struct PosColorVertex {
		float x, y, z;
		float r, g, b, a;
	};

	struct DebugLine {
		PosColorVertex p1;
		PosColorVertex p2;
	};

	class btDebugDraw : public btIDebugDraw {
	 protected:
		world w;
		float lineWidth = 2;
		shaderProgram program;
		vertexLayout layout;
		vertexBuffer vbh;
		vector<DebugLine> lines;
		int mode = DBG_DrawWireframe;

	 public:
		btDebugDraw(world w) {
			this->w = w;
			program = shaderProgram::findInCache("Wireframe");
			if (!program)
				program = shaderProgram({
					{"name", "Wireframe"},
					{"vert",
					 shaderObject({
						 {"data", getWireframeShaderData(VertexShaderType)},
					 })},
					{"frag", shaderObject({
										 {"data", getWireframeShaderData(
																FragmentShaderType)},
									 })},
				});
			using a = vertexLayout::attrib;
			using aT = vertexLayout::attribType;
			layout = vertexLayout::findInCache("Wireframe");
			if (!layout)
				layout = vertexLayout({{"name", "Wireframe"}})
									 .begin()
									 .add(a::Position, aT::Float, 3)
									 .add(a::Color0, aT::Float, 4)
									 .end();

			lines = vector<DebugLine>();
			using uT = uniformType;
			shaderProgram::createUniform("u_thickness", uT::Vec4);
		}

		void drawLine(
			const btVector3& from,
			const btVector3& to,
			const btVector3& color) {
			lines.push_back(DebugLine{
				{
					from.x(),
					from.y(),
					from.z(),
					color.x(),
					color.y(),
					color.z(),
					1.0f,
				},
				{
					to.x(),
					to.y(),
					to.z(),
					color.x(),
					color.y(),
					color.z(),
					1.0f,
				},
			});
		}

		void clearLines() { lines.clear(); }

		void flushLines() {
			auto eng = w.getObject<engine>("engine");
			auto cam = eng.getPrimaryCamera().getObject<camera>();
			auto trans = cam.getTransform();
			auto pos = trans.relative({});
			const auto viewId = uint16_t(0);
			auto vBinSize = sizeof(DebugLine) * lines.size() * 2;
			auto vBin = binary();
			vBin.resize(vBinSize, 0);
			memcpy(vBin.data(), lines.data(), vBinSize);

			vbh = vertexBuffer({
				{"count", lines.size() * 2},
				{"type", transientBufferType},
				{"layout", layout},
			});
			vbh.update(vBin);
			vbh.set(0);
			float u_thickness[4] = {lineWidth, 0.0, 0.0, 0.0};
			shaderProgram::setUniform("u_thickness", u_thickness);
			program.setState({
				{"type", "lines"},
				{"MSAA", true},
				{"lineAA", true},
			});
			program.submit(viewId);
		}

		void drawContactPoint(
			const btVector3& PointOnB,
			const btVector3& normalOnB,
			btScalar distance,
			int lifeTime,
			const btVector3& color) {}

		void reportErrorWarning(const char* warningString) {}

		void draw3dText(
			const btVector3& location, const char* textString) {}

		void setDebugMode(int debugMode) { mode = debugMode; }

		int getDebugMode() const { return mode; }
	};

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
		auto fixedTimeStep = getFloat("fixedTimeStep");
		auto timeStep = args.getFloat(0, fixedTimeStep);
		auto maxSubSteps = getInt32("maxSubSteps");
		auto bodies = getList("bodies");
		auto dWorld =
			(btDiscreteDynamicsWorld*)getPtr("dynamicsWorld");
		dWorld->stepSimulation(
			timeStep, maxSubSteps, fixedTimeStep);
		auto comp = physicsBody();
		auto body = (btRigidBody*)nullptr;
		auto ent = entity();
		auto trans = transform();
		for (auto it = bodies.begin(); it != bodies.end(); ++it) {
			if (
				(comp = it->getObject<physicsBody>()) &&
				(body = (btRigidBody*)comp.getPtr("body")) &&
				(ent = comp.getObject<entity>("object")) &&
				(trans = ent.getComponent({transform::getPrototype()})
									 .getObject<transform>())) {
				auto wTrans = body->getWorldTransform();
				auto rot = wTrans.getRotation();
				auto pos = wTrans.getOrigin();
				trans.setPosition({vec3f(pos.x(), pos.y(), pos.z())});
				trans.setRotation(
					{quatf(rot.x(), rot.y(), rot.z(), rot.w())});
			} else {
				it = bodies.erase(it);
			}
		}

		return var();
	}

	var world::setGravity(list args) {
		auto dWorld =
			(btDiscreteDynamicsWorld*)getPtr("dynamicsWorld");
		float g[3];
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
		dWorld->setGravity(btVector3(g[0], g[1], g[2]));
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
		auto gravity = getVar("gravity");
		auto collisionConfiguration =
			new btDefaultCollisionConfiguration();
		setPtr("collisionConfiguration", collisionConfiguration);

		auto dispatcher =
			new btCollisionDispatcher(collisionConfiguration);
		setPtr("dispatcher", dispatcher);

		auto broadphase = new btDbvtBroadphase();
		setPtr("broadphase", broadphase);

		auto solver = new btSequentialImpulseConstraintSolver;
		setPtr("solver", solver);

		auto dynamicsWorld = new btDiscreteDynamicsWorld(
			dispatcher, broadphase, solver, collisionConfiguration);
		setPtr("dynamicsWorld", dynamicsWorld);

		dynamicsWorld->setGravity(btVector3(
			gravity.getFloat(0),
			gravity.getFloat(1),
			gravity.getFloat(2)));

		auto debugDrawer = new btDebugDraw(*this);
		setPtr("debugDrawer", debugDrawer);
		dynamicsWorld->setDebugDrawer(debugDrawer);

		return var();
	}

	var world::destroy() {
		auto dynamicsWorld =
			(btDiscreteDynamicsWorld*)getPtr("dynamicsWorld");
		if (dynamicsWorld) delete dynamicsWorld;

		auto solver =
			(btSequentialImpulseConstraintSolver*)getPtr("solver");
		if (solver) delete solver;

		auto broadphase = (btDbvtBroadphase*)getPtr("broadphase");
		if (broadphase) delete broadphase;

		auto dispatcher =
			(btCollisionDispatcher*)getPtr("dispatcher");
		if (dispatcher) delete dispatcher;

		auto collisionConfiguration =
			(btDefaultCollisionConfiguration*)getPtr(
				"collisionConfiguration");
		if (collisionConfiguration) delete collisionConfiguration;

		auto debugDraw = (btDebugDraw*)getPtr("debugDrawer");
		if (debugDraw) delete debugDraw;

		empty();
		return var();
	}

	var world::debugDraw() {
		auto dynamicsWorld =
			(btDiscreteDynamicsWorld*)getPtr("dynamicsWorld");
		if (!dynamicsWorld) return genericError("World is not initialized");
		dynamicsWorld->debugDrawWorld();
		return var();
	}

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
