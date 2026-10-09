#pragma once

#include "renderable.hpp"
#include "camera.hpp"

namespace gold {
	using std::string;
	struct meshRenderer : public renderable {
	 public:
		// Public: scripts/tests need the prototype to detect the
		// component (same rationale as engine's).
		static object& getPrototype();

	 protected:
		void setMaterial(camera cam, object primitive, object mesh);
		string gatherDefines(object primitive, object meshEntry);
		object configureVertex(
			object primitive, object meshEntry, const string& defines);
		object configureFragment(
			object primitive, object meshEntry, const string& defines);

	 public:
		meshRenderer();
		meshRenderer(object config);

		var draw(list args = {});
		var initialize(list args = {});
		var destroy(list args = {});
	};
}  // namespace gold