#pragma once

#include "types.hpp"

namespace gold {
	/** What a shape descriptor materializes as, chosen by the shape
	 *  component's "shapeKind" ("box" | "sphere" | "mesh"). */
	enum class physicsShapeKind { box = 0, sphere, mesh };

	/**
	 * The physics engine behind the world/shape/physicsBody facades.
	 *
	 * Everything bullet-typed lives in backend implementations (the
	 * loadable libgoldBullet plugin compiles against the system bullet
	 * package); this interface deals only in gold objects and floats. The
	 * backend stores its opaque payloads in the facades' pointer fields
	 * ("shape"/"body"/"motionState"/"btTriMesh"/"dynamicsWorld" via
	 * setPtr) — the same fields the pre-plugin physics code used — so a
	 * machine without bullet runs the built-in "none" backend instead:
	 * worlds initialize, bodies no-op, and transforms stay under script
	 * control.
	 */
	class physicsBackend {
	 public:
		virtual ~physicsBackend() = default;

		virtual const char* name() const = 0;

		/** Build the world stack (dynamicsWorld etc.) and remember the
		 *  debug sink: collect(line floats, func sink) style — the
		 *  sink receives a binary of 7 floats per vertex, endpoint
		 *  pairs, whenever the world's wireframe flushes. */
		virtual bool createWorld(object world, func debugSink) = 0;
		/** Free everything createWorld and this backend's created
		 *  shapes/bodies still own. */
		virtual void destroyWorld(object world) = 0;

		virtual void setGravity(object world, float g[3]) = 0;
		/** Advance the simulation and sync each registered body's entity
		 *  transform to the backend's body state (pruning stale ones). */
		virtual void step(object world, float timeStep, int maxSubSteps,
			float fixedTimeStep) = 0;
		/** Batch the wireframe of every body through the debug sink. */
		virtual void debugDraw(object world) = 0;

		/** Ray-cast from world-space `from` to `to`; an empty list means
		 *  no hit, a hit is one object {position, normal, distance, body}
		 *  (position/normal vec3s, distance in world units, body the
		 *  physicsBody component that was struck). The built-in backends
		 *  default to no hit. */
		virtual list raycast(object world, float from[3], float to[3]) {
			(void)world;
			(void)from;
			(void)to;
			return list();
		}

		/** Materialize the shape component's payload ("shape" pointer)
		 *  from its descriptor ("shapeKind" + size/mesh/node fields). */
		virtual bool createShape(object shape) = 0;
		/** Free a shape/body's payload. For shapes that were
		 *  materialized for a body, ownership stays with the backend
		 *  until world destruction; this drops it early. */
		virtual void destroyObject(object physicsObject) = 0;
		/** Assemble `body`'s compound shape from its entity's shape
		 *  components (materializing them), build the rigid body, and
		 *  register it with `world`'s dynamic world + "bodies" list. */
		virtual bool createBody(object body, object world) = 0;
	};

	using createPhysicsBackendFn = physicsBackend* (*)();
	void registerPhysicsBackend(const std::string& name,
		createPhysicsBackendFn factory);

	/**
	 * Create a physics backend by name; a miss tries plugin::load first
	 * (the bullet plugin self-registers), like the other registries.
	 */
	physicsBackend* createPhysicsBackend(const std::string& name);
	/** Ordered fallback chain; the first name that produces a backend
	 *  wins ("none" should be a chain's guaranteed tail). */
	physicsBackend* createPhysicsBackend(const list& names);
}  // namespace gold