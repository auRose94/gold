#pragma once

#include "types.hpp"

namespace gold {
	struct objData {
		object::omap items;
		gold::object parent;
		uint64_t id;
		shared_mutex omutex;
	};
}  // namespace gold
