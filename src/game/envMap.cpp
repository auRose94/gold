#include "envMap.hpp"

#include <functional>

#include "component.hpp"
#include "types.hpp"

namespace gold {
	using namespace std;
	obj& envMap::getPrototype() {
		static auto proto = obj({
			{"priority", priorityEnum::genericPriority},
			{"size", 512},
			{"getSize", method(&envMap::getSize)},
			{"proto", component::getPrototype()},
		});
		return proto;
	}

	var envMap::getSize(list) { return getVar("size"); }

	envMap::envMap() : component() { setParent(getPrototype()); }

	envMap::envMap(obj config) : component() {
		copy(config);
		setParent(getPrototype());
	}
}  // namespace gold