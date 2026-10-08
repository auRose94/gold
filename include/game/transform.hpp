#pragma once

#include "component.hpp"

namespace gold {
	struct transform : public component {
	 public:
		static object& getPrototype();
		transform();
		transform(object config);

		var getMatrix(list args = {});
		var getWorldMatrix(list args = {});
		var relative(list args);
		var setPosition(list args);
		var setRotation(list args);
		var setAxisRotation(list args);
		var setScale(list args);
		var getPosition(list args = {});
		var getRotation(list args = {});
		var getEuler(list args = {});
		var getScale(list args = {});
		var reset(list args = {});
	};
}  // namespace gold