#include "component.hpp"

#include <functional>

#include "types.hpp"

namespace gold {
	using namespace std;
	obj& component::getPrototype() {
		static auto proto = obj({
			{"priority", priorityEnum::genericPriority},
			// The lifecycle defaults; concrete components bind their own
			// overrides on their own prototypes (sprites do, shapes do).
			{"initialize", method(&component::initialize)},
			{"draw", method(&component::draw)},
			{"update", method(&component::update)},
		});
		return proto;
	}

	var component::draw(list) { return var(); }

	var component::update(list) { return var(); }

	var component::initialize(list) { return var(); }

	component::component() : obj() { setParent(getPrototype()); }

	component::component(obj config) : obj() {
		copy(config);
		setParent(getPrototype());
	}
}  // namespace gold