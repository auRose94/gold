#include "meshShape.hpp"

#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/CollisionShapes/btTriangleMeshShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btAlignedObjectArray.h>
#include <LinearMath/btDefaultMotionState.h>
#include <LinearMath/btVector3.h>
#include <btBulletDynamicsCommon.h>

#include "entity.hpp"
#include "mesh.hpp"
#include "shape.hpp"

namespace gold {
	using namespace std;
	object& meshShape::getPrototype() {
		static auto proto = obj{
			{"priority", priorityEnum::dataPriority},
			{"mesh", var()},
			{"node", ""},
			{"initialize", method(&meshShape::initialize)},
			{"destroy", method(&meshShape::destroy)},
			{"proto", shape::getPrototype()},
		};
		return proto;
	}

	meshShape::meshShape() : shape() {}

	meshShape::meshShape(object config) : shape(config) {
		setParent(getPrototype());
	}

	var meshShape::initialize(list) {
		auto m = getObject<mesh>("mesh");
		auto node = getString("node");
		if (m && node != "") {
			auto triMesh = new btTriangleMesh();
			setPtr("btTriMesh", triMesh);
			auto triList = m.getTrianglesFromMesh({node}).getList();
			if (triList) {
				for (auto it = triList.begin(); it != triList.end();) {
					btVector3 tri[3];
					size_t v;
					for (v = 0; v < 3; v++) {
						if (it->isNumber()) {
							tri[v] = btVector3(
								it->getFloat(0), it->getFloat(1),
								it->getFloat(2));
							it++;
						}
					}
					if (v >= 3)
						triMesh->addTriangle(tri[0], tri[1], tri[2]);
				}
				auto shape = new btBvhTriangleMeshShape(triMesh, true);
				shape->setUserPointer(this);
				setPtr("shape", shape);
			}
		}
		return var();
	}

	var meshShape::destroy(list) {
		auto shape = (btTriangleMeshShape*)getPtr("shape");
		if (shape) delete shape;
		erase("shape");
		auto triMesh = (btTriangleMesh*)getPtr("btTriMesh");
		if (triMesh) delete triMesh;
		erase("btTriMesh");
		return var();
	}
}  // namespace gold