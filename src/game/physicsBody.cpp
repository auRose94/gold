#include "physicsBody.hpp"

#include "engine.hpp"
#include "entity.hpp"
#include "physicsBackend.hpp"
#include "shape.hpp"
#include "world.hpp"

namespace gold {
	using namespace std;
	object& physicsBody::getPrototype() {
		static auto proto = obj{
			{"priority", priorityEnum::physicsPriority},
			{"mass", float(0)},
			{"initialize", method(&physicsBody::initialize)},
			{"destroy", method(&physicsBody::destroy)},
			{"proto", component::getPrototype()},
		};
		return proto;
	}

	physicsBody::physicsBody() : component() {}

	physicsBody::physicsBody(object config) : component(config) {
		setParent(getPrototype());
	}

	var physicsBody::initialize(list) {
		// The physics backend (bullet plugin, or "none" on machines
		// without it) materializes the compound body from the entity's
		// shape components and registers it with the world.
		auto parentObject = getObject<entity>("object");
		if (!parentObject) return var();
		auto eng = parentObject.getObject<engine>("engine");
		if (!eng) return var();
		auto phys = eng.getObject<world>("world");
		if (!phys) return var();
		auto backend = (physicsBackend*)phys.getPtr("physicsBackend");
		if (!backend) return var();
		backend->createBody(*this, phys);
		return var();
	}

	var physicsBody::destroy(list) {
		auto parentObject = getObject<entity>("object");
		if (!parentObject) return var();
		auto eng = parentObject.getObject<engine>("engine");
		if (!eng) return var();
		auto phys = eng.getObject<world>("world");
		if (!phys) return var();
		auto backend = (physicsBackend*)phys.getPtr("physicsBackend");
		if (backend) backend->destroyObject(*this);
		erase("body");
		erase("motionState");
		erase("shape");
		return var();
	}
}  // namespace gold