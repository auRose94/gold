#pragma once

#include "file.hpp"

namespace gold {
	using std::string;
	using std::vector;
	struct mesh : public file {
	 protected:
		static object& getPrototype();
		static void parseGLTFBuffer(
			string type,
			uint32_t scalarType,
			uint64_t offset,
			uint64_t length,
			void* data,
			list& parsed);

	 public:
		mesh();
		mesh(path meshFilePath);
		mesh(file meshFile);

		var getNode(list);            // string name
		var getPrimitiveCount(list);  // string nName
		var getVertexLayoutHandle(
			list);  // string nName, uint64_t primitive = 0
		var getVertexBufferHandle(
			list);  // string nName, uint64_t primitive = 0
		var getIndexBufferHandle(
			list);  // string nName, uint64_t primitive = 0
		var getTrianglesFromMesh(list);  // string nName
		var getMaterial(list);           // string/uint64 nName
		var getMaterialFromPrimitive(
			list);  // string nName, uint64_t primitive

		var destroy(list args = {});
	};
}  // namespace gold
