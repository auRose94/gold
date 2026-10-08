#include "meshShape.hpp"

#include "shape.hpp"

namespace gold {
	using namespace std;
	object& meshShape::getPrototype() {
		static auto proto = obj{
			{"priority", priorityEnum::dataPriority},
			// Descriptor data only: the physics backend reads the mesh +
			// node and materializes a triangle-mesh shape at body time.
			{"shapeKind", "mesh"},
			{"mesh", var()},
			{"node", ""},
			{"proto", shape::getPrototype()},
		};
		return proto;
	}

	meshShape::meshShape() : shape() {}

	meshShape::meshShape(object config) : shape(config) {
		setParent(getPrototype());
	}
}  // namespace gold