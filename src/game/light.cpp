#include "light.hpp"

namespace gold {
	using namespace std;
	obj& light::getPrototype() {
		static auto proto = obj({
			{"priority", priorityEnum::genericPriority},
			{"color", 0xffffffff},
			{"intensity", 1.0f},
			{"cone", vec2f(1, 1)},
			{"type", "point"},
			{"setColor", method(&light::setColor)},
			{"getColor", method(&light::getColor)},
			{"setIntensity", method(&light::setIntensity)},
			{"getIntensity", method(&light::getIntensity)},
			{"setCone", method(&light::setCone)},
			{"getCone", method(&light::getCone)},
			{"setType", method(&light::setType)},
			{"getType", method(&light::getType)},
			{"getArea", method(&light::getArea)},
			{"drawScene", method(&light::drawScene)},
			{"proto", component::getPrototype()},
		});
		return proto;
	}

	var light::setColor(list args) {
		auto color = list::getColor(args);
		setUInt32("color", color);
		return color;
	}

	var light::getColor(list) { return getUInt32("color"); }

	var light::setIntensity(list args) {
		if (args.size() == 0) return genericError("setIntensity requires a value");
		float in = args[0].getFloat();
		setFloat("intensity", in);
		return in;
	}

	var light::getIntensity(list) {
		return getFloat("intensity");
	}

	var light::setCone(list args) {
		auto val = list::getVec2f(args);
		setVar("cone", val);
		return val;
	}

	var light::getCone(list) { return getVar("cone"); }

	var light::setType(list args) {
		if (args.size() == 0) return genericError("setType requires a value");
		auto val = args.getString(0);
		setString("type", val);
		return val;
	}

	var light::getType(list) { return getString("type"); }

	var light::getArea(list) { return var(); }

	var light::drawScene(list args) {
		auto comps = args.getList(0);
		auto scene = args.getUInt16(1);
		return var();
	}

	light::light() : component() { setParent(getPrototype()); }

	light::light(obj config) : component() {
		copy(config);
		setParent(getPrototype());
	}
}  // namespace gold
