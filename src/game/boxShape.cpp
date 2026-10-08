#include "boxShape.hpp"

#include "shape.hpp"

namespace gold {
	using namespace std;
	object& boxShape::getPrototype() {
		static auto proto = obj{
			{"priority", priorityEnum::dataPriority},
			// Descriptor data only: the physics backend (the bullet
			// plugin) materializes the collision shape when a body is
			// created; without one these are plain data.
			{"shapeKind", "box"},
			{"size", vec3f(1, 1, 1)},
			{"proto", shape::getPrototype()},
		};
		return proto;
	}

	boxShape::boxShape() : shape() {}

	boxShape::boxShape(object config) : shape(config) {
		setParent(getPrototype());
	}
}  // namespace gold