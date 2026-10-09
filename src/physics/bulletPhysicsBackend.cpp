// The bullet physics backend: the loadable libgoldBullet plugin. Every
// bullet-typed line of the physics system lives here, compiled against
// the system bullet package (the double-precision build — gold mirrors
// BT_USE_DOUBLE_PRECISION on both sides so the ABIs agree). The backend
// materializes shapes from gold descriptors, builds rigid bodies, steps
// the world with entity-transform syncing, and flushes bullet's debug
// wireframe through a gold func sink (owned by the world facade).

#include "game/physicsBackend.hpp"

#include <btBulletDynamicsCommon.h>

#include "goldjs.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "mesh.hpp"
#include "physicsBody.hpp"
#include "shape.hpp"
#include "transform.hpp"
#include "world.hpp"

namespace gold {
	using namespace std;

	namespace {

		/** Bullet's debug drawer batches endpoint pairs into one gold
		 *  binary (7 floats per vertex: xyz + rgba) and hands it to the
		 *  render-facing sink the world supplied. */
		class bulletDebugDraw : public btIDebugDraw {
			func sink_;
			vector<float> lines_;
			int mode_ = DBG_DrawWireframe;

			void pushLine(const btVector3& a, const btVector3& b,
				const btVector3& color) {
				const float p[14] = {
					(float)a.x(), (float)a.y(), (float)a.z(),
					(float)color.x(), (float)color.y(), (float)color.z(), 1.0f,
					(float)b.x(), (float)b.y(), (float)b.z(),
					(float)color.x(), (float)color.y(), (float)color.z(), 1.0f,
				};
				lines_.insert(lines_.end(), p, p + 14);
			}

		 public:
			explicit bulletDebugDraw(func sink) : sink_(std::move(sink)) {}

			void drawLine(const btVector3& from, const btVector3& to,
				const btVector3& color) override {
				pushLine(from, to, color);
			}

			void clearLines() override { lines_.clear(); }

			void flushLines() override {
				if (lines_.empty()) return;
				const size_t bytes = lines_.size() * sizeof(float);
				binary batch(bytes);
				memcpy(batch.data(), lines_.data(), bytes);
				sink_({var(batch)});
				clearLines();
			}

			void drawContactPoint(const btVector3&, const btVector3&,
				btScalar, int, const btVector3&) override {}
			void reportErrorWarning(const char*) override {}
			void draw3dText(const btVector3&, const char*) override {}
			void setDebugMode(int debugMode) override { mode_ = debugMode; }
			int getDebugMode() const override { return mode_; }
		};

		/** The entity transform as a bullet transform (gold data in). */
		btTransform toBtTransform(transform& trans) {
			auto rot = trans.getRotation();
			auto pos = trans.getPosition();
			return btTransform(
				btQuaternion((float)rot.getFloat(0), (float)rot.getFloat(1),
					(float)rot.getFloat(2), (float)rot.getFloat(3)),
				btVector3((float)pos.getFloat(0), (float)pos.getFloat(1),
					(float)pos.getFloat(2)));
		}

		class bulletPhysicsBackend : public physicsBackend {
			void destroyShapePayload(object& shapeComp) {
				auto shape = (btCollisionShape*)shapeComp.getPtr("shape");
				if (shape) delete shape;
				shapeComp.erase("shape");
				auto triMesh =
					(btTriangleMesh*)shapeComp.getPtr("btTriMesh");
				if (triMesh) delete triMesh;
				shapeComp.erase("btTriMesh");
			}

		 public:
			const char* name() const override { return "bullet"; }

			bool createWorld(object w, func debugSink) override {
				auto gravity = w.getVar("gravity");
				auto collisionConfiguration = new btDefaultCollisionConfiguration();
				auto dispatcher = new btCollisionDispatcher(collisionConfiguration);
				auto broadphase = new btDbvtBroadphase();
				auto solver = new btSequentialImpulseConstraintSolver;
				auto dynamicsWorld = new btDiscreteDynamicsWorld(
					dispatcher, broadphase, solver, collisionConfiguration);
				w.setPtr("collisionConfiguration", collisionConfiguration);
				w.setPtr("dispatcher", dispatcher);
				w.setPtr("broadphase", broadphase);
				w.setPtr("solver", solver);
				w.setPtr("dynamicsWorld", dynamicsWorld);

				dynamicsWorld->setGravity(btVector3(
					(float)gravity.getFloat(0), (float)gravity.getFloat(1),
					(float)gravity.getFloat(2)));

				auto* debugDrawer = new bulletDebugDraw(std::move(debugSink));
				w.setPtr("debugDrawer", debugDrawer);
				dynamicsWorld->setDebugDrawer(debugDrawer);
				return true;
			}

			void destroyWorld(object w) override {
				// Free the world's bodies (each keeps motion state, compound
				// and its children's payloads), then the stack. Bodies were
				// never removed from the world before deletion (parity was a
				// missing call); removing them now keeps bullet's internals
				// from touching freed memory on the last step.
				auto bodies = w.getList("bodies");
				for (auto it = bodies.begin(); it != bodies.end(); ++it) {
					auto comp = it->getObject<physicsBody>();
					if (!comp) continue;
					destroyObject(comp);
				}
				w.setList("bodies", list());

				auto debugDrawer = (bulletDebugDraw*)w.getPtr("debugDrawer");
				if (debugDrawer) delete debugDrawer;
				w.erase("debugDrawer");

				auto dynamicsWorld =
					(btDiscreteDynamicsWorld*)w.getPtr("dynamicsWorld");
				if (dynamicsWorld) delete dynamicsWorld;
				w.erase("dynamicsWorld");

				auto solver = (btSequentialImpulseConstraintSolver*)w.getPtr("solver");
				if (solver) delete solver;
				w.erase("solver");

				auto broadphase = (btDbvtBroadphase*)w.getPtr("broadphase");
				if (broadphase) delete broadphase;
				w.erase("broadphase");

				auto dispatcher = (btCollisionDispatcher*)w.getPtr("dispatcher");
				if (dispatcher) delete dispatcher;
				w.erase("dispatcher");

				auto config = (btDefaultCollisionConfiguration*)w.getPtr(
					"collisionConfiguration");
				if (config) delete config;
				w.erase("collisionConfiguration");
			}

			void setGravity(object w, float g[3]) override {
				if (auto* world = (btDiscreteDynamicsWorld*)w.getPtr("dynamicsWorld"))
					world->setGravity(btVector3(g[0], g[1], g[2]));
			}

			void step(object w, float timeStep, int maxSubSteps,
				float fixedTimeStep) override {
				auto* world = (btDiscreteDynamicsWorld*)w.getPtr("dynamicsWorld");
				if (!world) return;
				world->stepSimulation(timeStep, maxSubSteps, fixedTimeStep);

				// Sync every registered body's entity transform.
				auto bodies = w.getList("bodies");
				for (auto it = bodies.begin(); it != bodies.end();) {
					auto comp = it->getObject<physicsBody>();
					auto body = (btRigidBody*)comp.getPtr("body");
					auto ent = comp.getObject<entity>("object");
					if (!(bool)comp || !body || !(bool)ent) {
						it = bodies.erase(it);
						continue;
					}
					auto trans = ent.getComponent({transform::getPrototype()})
									 .getObject<transform>();
					if (!(bool)trans) {
						it = bodies.erase(it);
						continue;
					}
					const auto& wTrans = body->getWorldTransform();
					const auto& rot = wTrans.getRotation();
					const auto& pos = wTrans.getOrigin();
					trans.setPosition({vec3f(pos.x(), pos.y(), pos.z())});
					trans.setRotation(
						{quatf(rot.x(), rot.y(), rot.z(), rot.w())});
					++it;
				}
				w.setList("bodies", bodies);
			}

			void debugDraw(object w) override {
				auto* world = (btDiscreteDynamicsWorld*)w.getPtr("dynamicsWorld");
				if (!world) return;
				world->debugDrawWorld();
			}

			list raycast(object w, float from[3], float to[3]) override {
				auto* world =
					(btDiscreteDynamicsWorld*)w.getPtr("dynamicsWorld");
				if (!world) return list();
				btVector3 rayFrom(from[0], from[1], from[2]);
				btVector3 rayTo(to[0], to[1], to[2]);
				btCollisionWorld::ClosestRayResultCallback ray(rayFrom, rayTo);
				world->rayTest(rayFrom, rayTo, ray);
				if (!ray.hasHit()) return list();

				// The struck body's gold component, found by pointer
				// identity against the world's registered bodies list.
				object bodyComp;
				auto bodies = w.getList("bodies");
				for (auto it = bodies.begin(); it != bodies.end(); ++it) {
					auto comp = it->getObject<physicsBody>();
					if ((btRigidBody*)comp.getPtr("body") ==
						ray.m_collisionObject) {
						bodyComp = comp;
						break;
					}
				}

				const auto& point = ray.m_hitPointWorld;
				const auto& normal = ray.m_hitNormalWorld;
				const float spanX = to[0] - from[0];
				const float spanY = to[1] - from[1];
				const float spanZ = to[2] - from[2];
				const float span = sqrtf(
					spanX * spanX + spanY * spanY + spanZ * spanZ);
				auto hit = jo(
					"position",
					var(vec3f((float)point.x(), (float)point.y(),
						(float)point.z())),
					"normal",
					var(vec3f((float)normal.x(), (float)normal.y(),
						(float)normal.z())),
					"distance",
					ray.m_closestHitFraction * span);
				if (bodyComp) hit["body"] = var(bodyComp);
				return list({hit});
			}

			bool createShape(object shapeComp) override {
				if (shapeComp.getPtr("shape")) return true;  // already live
				const auto kind = shapeComp.getString("shapeKind");
				if (kind == "box") {
					auto size = shapeComp.getVar("size");
					auto* shape = new btBoxShape(btVector3(
						(float)size.getFloat(0), (float)size.getFloat(1),
						(float)size.getFloat(2)));
					shapeComp.setPtr("shape", shape);
					return true;
				}
				if (kind == "sphere") {
					auto* shape = new btSphereShape(
						(float)shapeComp.getFloat("size"));
					shapeComp.setPtr("shape", shape);
					return true;
				}
				if (kind == "mesh") {
					auto m = shapeComp.getObject<mesh>("mesh");
					auto node = shapeComp.getString("node");
					if (!(bool)m || node.empty()) return false;
					auto* triMesh = new btTriangleMesh();
					shapeComp.setPtr("btTriMesh", triMesh);
					auto triList = m.getTrianglesFromMesh({node}).getList();
					if (!triList) return false;
					for (auto it = triList.begin(); it != triList.end();) {
						btVector3 tri[3];
						size_t v;
						for (v = 0; v < 3; v++) {
							if (it->isNumber()) {
								tri[v] = btVector3(
									(float)it->getFloat(0),
									(float)it->getFloat(1),
									(float)it->getFloat(2));
								it++;
							}
						}
						if (v >= 3)
							triMesh->addTriangle(tri[0], tri[1], tri[2]);
					}
					auto* shape = new btBvhTriangleMeshShape(triMesh, true);
					shapeComp.setPtr("shape", shape);
					return true;
				}
				return false;
			}

			void destroyObject(object physicsObject) override {
				// A body: drop its rigid body + compound + motion state,
				// and take it out of the world first.
				auto body = (btRigidBody*)physicsObject.getPtr("body");
				if (body) {
					auto parentObject =
						physicsObject.getObject<object>("object");
					auto eng = parentObject.getObject<engine>("engine");
					auto w = eng.getObject<world>("world");
					if (auto* world =
							(btDiscreteDynamicsWorld*)w.getPtr("dynamicsWorld"))
						world->removeRigidBody(body);
					delete body;
					physicsObject.erase("body");
				}
				auto motionState =
					(btDefaultMotionState*)physicsObject.getPtr("motionState");
				if (motionState) delete motionState;
				physicsObject.erase("motionState");

				auto shape = (btCollisionShape*)physicsObject.getPtr("shape");
				if (shape) delete shape;
				physicsObject.erase("shape");
				auto triMesh = (btTriangleMesh*)physicsObject.getPtr("btTriMesh");
				if (triMesh) delete triMesh;
				physicsObject.erase("btTriMesh");
			}

			bool createBody(object comp, object w) override {
				auto* dynamicsWorld =
					(btDiscreteDynamicsWorld*)w.getPtr("dynamicsWorld");
				if (!dynamicsWorld) return false;
				if (comp.getPtr("body")) return true;  // already registered

				const float mass = (float)comp.getFloat("mass");
				auto parentObject =
					comp.getObject<entity>("object");
				auto parTrans = parentObject.getTransform();
				// Child shapes are materialized here (body time), after
				// whatever lazy descriptor state they carry is set.
				auto shapes = parentObject
					.getComponentsRecursive({shape::getPrototype()})
					.getList();
				auto bodyShape = new btCompoundShape();
				comp.setPtr("shape", bodyShape);
				for (auto it = shapes.begin(); it != shapes.end(); ++it) {
					auto shapeComp = it->getObject<shape>();
					if (!(bool)shapeComp) continue;
					createShape(shapeComp);
					auto shape = (btCollisionShape*)shapeComp.getPtr("shape");
					if (!shape) continue;
					auto shapeObj = shapeComp.getObject<entity>("object");
					auto trans = shapeObj.getTransform();
					auto btTrans =
						toBtTransform(trans) * toBtTransform(parTrans).inverse();
					bodyShape->addChildShape(btTrans, shape);
				}
				btAssert((!bodyShape ||
					bodyShape->getShapeType() != INVALID_SHAPE_PROXYTYPE));

				const bool isDynamic = (mass != 0.f);
				btVector3 localInertia(0, 0, 0);
				if (isDynamic)
					bodyShape->calculateLocalInertia(mass, localInertia);

				auto motionState = new btDefaultMotionState(
					toBtTransform(parTrans).inverse());
				comp.setPtr("motionState", motionState);

				btRigidBody::btRigidBodyConstructionInfo cInfo(
					mass, motionState, bodyShape, localInertia);
				auto* body = new btRigidBody(cInfo);
				body->setUserIndex(-1);
				comp.setPtr("body", body);
				dynamicsWorld->addRigidBody(body);

				auto worldBodies = w.getList("bodies");
				worldBodies.pushObject(comp);
				w.setList("bodies", worldBodies);
				return true;
			}
		};

		struct bulletRegistrar {
			bulletRegistrar() {
				registerPhysicsBackend("bullet",
					[]() -> physicsBackend* { return new bulletPhysicsBackend(); });
			}
		};
		bulletRegistrar bulletReg;
	}  // namespace

}  // namespace gold