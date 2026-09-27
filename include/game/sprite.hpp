#pragma once

#include "renderable.hpp"

namespace gold {
	/**
	 * Bytecode of the shared "Sprite" program for the active backend, or an
	 * empty buffer when the backend has no compiled variant. Exposed so other
	 * textured quads (see `uiSurface`) can reuse the same program.
	 */
	binary getSpriteShaderData(shaderType stype);

	struct sprite : public renderable {
	 protected:
		void updateVertexBuffer();

	 public:
		static object& getPrototype();
		sprite();
		sprite(object config);

		var setSize(list args);
		var setArea(list args);
		var setOffset(list args);
		var draw(list args = {});
		var initialize(list args = {});
		var destroy(list args = {});
	};
}  // namespace gold