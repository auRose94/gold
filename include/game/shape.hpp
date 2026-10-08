#pragma once

#include "component.hpp"

namespace gold {
	struct shape : public component {
	 public:
		// Public so backend plugins can discover shape components by
		// prototype (getComponentsRecursive({shape::getPrototype()})).
		static object& getPrototype();

	 protected:
		friend struct physicsBody;

	 public:
		shape();
		shape(object config);

		var initialize(list args = {});
		var destroy(list args = {});
	};
}