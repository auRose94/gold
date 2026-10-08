#include "sphereShape.hpp"

#include "shape.hpp"

namespace gold {
	using namespace std;
	object& sphereShape::getPrototype() {
		static auto proto = obj{
			{"priority", priorityEnum::dataPriority},
			// Descriptor data only: materialized by the physics backend
			// when its entity gets a body.
			{"shapeKind", "sphere"},
			{"size", 1.0},
			{"proto", shape::getPrototype()},
		};
		return proto;
	}

	sphereShape::sphereShape() : shape() {}

	sphereShape::sphereShape(object config) : shape(config) {
		setParent(getPrototype());
	}
}  // namespace gold