#include "physicsBody.hpp"

#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btDefaultMotionState.h>

#include "engine.hpp"
#include "entity.hpp"
#include "shape.hpp"
#include "world.hpp"

namespace gold {
	using namespace std;
	object& physicsBody::getPrototype() {
		static auto proto = obj{
			{"priority", priorityEnum::physicsPriority},
			{"mass", float(0)},
			{"initialize", method(&physicsBody::initialize)},
			{"destroy", method(&physicsBody::destroy)},
			{"proto", component::getPrototype()},
		};
		return proto;
	}

	physicsBody::physicsBody() : component() {}

	physicsBody::physicsBody(object config) : component(config) {
		setParent(getPrototype());
	}

	var physicsBody::initialize(list) {
		float mass = getFloat("mass");
		auto parentObject = getObject<entity>("object");
		auto eng = parentObject.getObject<engine>("engine");
		auto phys = eng.getObject<world>("world");
		auto worldBodies = phys.getList("bodies");
		auto dWorld =
			(btDiscreteDynamicsWorld*)phys.getPtr("dynamicsWorld");
		auto parTrans = parentObject.getTransform();
		auto btParTrans = parTrans.getBtTransform().inverse();
		auto shapes =
			parentObject
				.getComponentsRecursive({shape::getPrototype()})
				.getList();
		btCompoundShape* bodyShape = new btCompoundShape();
		setPtr("shape", bodyShape);
		for (auto it = shapes.begin(); it != shapes.end(); ++it) {
			auto shapeComp = it->getObject<shape>();
			auto shapeObj = shapeComp.getObject<entity>("object");
			auto trans = shapeObj.getTransform();
			auto btTrans = trans.getBtTransform() * btParTrans;

			auto shape = (btCollisionShape*)shapeComp.getPtr("shape");
			if (shape) bodyShape->addChildShape(btTrans, shape);
		}
		btAssert(
			(!bodyShape ||
			 bodyShape->getShapeType() != INVALID_SHAPE_PROXYTYPE));

		bool isDynamic = (mass != 0.f);

		btVector3 localInertia(0, 0, 0);
		if (isDynamic)
			bodyShape->calculateLocalInertia(mass, localInertia);

		btDefaultMotionState* motionState =
			new btDefaultMotionState(btParTrans);
		setPtr("motionState", motionState);

		btRigidBody::btRigidBodyConstructionInfo cInfo(
			mass, motionState, bodyShape, localInertia);

		btRigidBody* body = new btRigidBody(cInfo);

		body->setUserIndex(-1);
		setPtr("body", body);

		dWorld->addRigidBody(body);
		worldBodies.pushObject(*this);

		return var();
	}

	var physicsBody::destroy(list) {
		auto body = (btRigidBody*)getPtr("body");
		auto motionState =
			(btDefaultMotionState*)getPtr("motionState");
		auto shape = (btCollisionShape*)getPtr("shape");
		if (body) delete body;
		if (motionState) delete motionState;
		if (shape) delete shape;
		erase("body");
		erase("motionState");
		erase("shape");
		return var();
	}
}  // namespace gold