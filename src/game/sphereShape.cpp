#include "sphereShape.hpp"

#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btAlignedObjectArray.h>
#include <LinearMath/btDefaultMotionState.h>
#include <LinearMath/btVector3.h>
#include <btBulletDynamicsCommon.h>

#include "entity.hpp"
#include "shape.hpp"

namespace gold {
	using namespace std;
	object& sphereShape::getPrototype() {
		static auto proto = obj{
			{"priority", priorityEnum::dataPriority},
			{"size", 1.0},
			{"initialize", method(&sphereShape::initialize)},
			{"destroy", method(&sphereShape::destroy)},
			{"proto", shape::getPrototype()},
		};
		return proto;
	}

	sphereShape::sphereShape() : shape() {}

	sphereShape::sphereShape(object config) : shape(config) {
		setParent(getPrototype());
	}

	var sphereShape::initialize(list) {
		auto size = getFloat("size");
		auto shape = new btSphereShape(size);
		shape->setUserPointer(this);
		setPtr("shape", shape);
		return var();
	}

	var sphereShape::destroy(list) {
		auto shape = (btSphereShape*)getPtr("shape");
		if (shape) delete shape;
		erase("shape");
		return var();
	}
}  // namespace gold