#pragma once

#include "types.hpp"
#include "component.hpp"

namespace gold {
	struct physicsBody : public component {
	 public:
		// Public: scripts/tests need the prototype to detect the
		// component (same rationale as engine's).
		static object& getPrototype();

	 protected:
		friend struct world;

	 public:
		physicsBody();
		physicsBody(object config);

		var initialize(list args = {});
		var destroy(list args = {});
	};
}  // namespace gold