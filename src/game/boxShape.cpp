#include "boxShape.hpp"

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btAlignedObjectArray.h>
#include <LinearMath/btDefaultMotionState.h>
#include <LinearMath/btVector3.h>
#include <btBulletDynamicsCommon.h>

#include "entity.hpp"
#include "shape.hpp"

namespace gold {
	using namespace std;
	object& boxShape::getPrototype() {
		static auto proto = obj{
			{"priority", priorityEnum::dataPriority},
			{"size", vec3f(1, 1, 1)},
			{"initialize", method(&boxShape::initialize)},
			{"destroy", method(&boxShape::destroy)},
			{"proto", shape::getPrototype()},
		};
		return proto;
	}

	boxShape::boxShape() : shape() {}

	boxShape::boxShape(object config) : shape(config) {
		setParent(getPrototype());
	}

	var boxShape::initialize(list) {
		auto size = getVar("size");
		auto shape = new btBoxShape(btVector3(
			size.getFloat(0), size.getFloat(1), size.getFloat(2)));
		shape->setUserPointer(this);
		setPtr("shape", shape);
		return var();
	}

	var boxShape::destroy(list) {
		auto shape = (btBoxShape*)getPtr("shape");
		if (shape) delete shape;
		erase("shape");
		return var();
	}
}  // namespace gold