#include "renderable.hpp"

#include "component.hpp"

namespace gold {
	using namespace std;

	obj& renderable::getPrototype() {
		static auto proto = obj({
			{"priority", priorityEnum::drawPriority},
			{"view", list({uint16_t(0)})},
			{"draw", method(&renderable::draw)},
			{"proto", component::getPrototype()},
		});
		return proto;
	}

	var renderable::draw(list) {
		return genericError("Needs to overriden");
	}

}  // namespace gold
