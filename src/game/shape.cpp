#include "shape.hpp"

namespace gold {
	using namespace std;

	object& shape::getPrototype() {
		static auto proto = obj{
			{"priority", priorityEnum::dataPriority},
			{"proto", component::getPrototype()},
		};
		return proto;
	}

	shape::shape() : component() {

	}

	shape::shape(object config) : component(config) {
		setParent(getPrototype());
	}

	var shape::initialize(list) {
		return var();
	}

	var shape::destroy(list) {
		return var();
	}
}  // namespace gold