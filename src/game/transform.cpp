#include "transform.hpp"

#include <bx/math.h>

#include "entity.hpp"

namespace gold {
	using namespace std;
	object& transform::getPrototype() {
		static auto proto = obj(initList{
			{"priority", priorityEnum::dataPriority},
			{"pos", vec3f(0, 0, 0)},
			{"rot", quatf(0, 0, 0, 1)},
			{"scl", vec3f(1, 1, 1)},
			{"getMatrix", method(&transform::getMatrix)},
			{"getWorldMatrix", method(&transform::getWorldMatrix)},
			{"relative", method(&transform::relative)},
			{"setPosition", method(&transform::setPosition)},
			{"setRotation", method(&transform::setRotation)},
			{"setScale", method(&transform::setScale)},
			{"getPosition", method(&transform::getPosition)},
			{"getRotation", method(&transform::getRotation)},
			{"getScale", method(&transform::getScale)},
			{"reset", method(&transform::reset)},
			{"proto", component::getPrototype()},
		});
		return proto;
	}

	transform::transform() : component() {}

	transform::transform(object config) : component() {
		setParent(getPrototype());
		copy(config);
	}

	var transform::relative(list args) {
		auto mtx = getMatrix();
		if (args.size() > 1) {
			auto ret = list({});
			for (auto it = args.begin(); it != args.end(); ++it)
				ret.pushVar(mtx * (*it));
			return ret;
		} else if (args.size() == 1)
			return mtx * args[0];
		return mtx * vec4f(0, 0, 0, 1);
	}

	var transform::setPosition(list args) {
		if (args.size() == 0) return genericError("setPosition requires a value");
		if (args.getType(0) == typeList) {
			setPosition(args);
		} else if (args[0].isVec3()) {
			setVar("pos", args[0]);
			setBool("rebuild", true);
		} else if (args.size() >= 3 && args.isAllNumber()) {
			setVar(
				"pos",
				vec3f(
					args[0].getFloat(),
					args[1].getFloat(),
					args[2].getFloat()));
			setBool("rebuild", true);
		}
		return var();
	}

	var transform::setRotation(list args) {
		if (args.size() == 0) return genericError("setRotation requires a value");
		if (args.getType(0) == typeList) {
			setRotation(args);
		} else if (args[0].isQuat()) {
			setVar("rot", args[0]);
			setBool("rebuild", true);
		} else if (args.size() >= 4 && args.isAllNumber()) {
			setVar(
				"rot",
				quatf(
					args[0].getFloat(),
					args[1].getFloat(),
					args[2].getFloat(),
					args[3].getFloat()));
			setBool("rebuild", true);
		}
		return var();
	}

	var transform::setAxisRotation(list args) {
		auto axis = bx::Vec3(0.0f, 0.0f, 0.0f);
		auto value = float(0);
		for (auto it = args.begin(); it != args.end(); ++it) {
			if (it->isVec3())
				axis = bx::Vec3(
					it->getFloat(0), it->getFloat(1), it->getFloat(2));
			else if (it->isNumber())
				value = it->getFloat(0);
		}
		auto qua = bx::fromAxisAngle(axis, value);
		auto rot = quatf(qua.x, qua.y, qua.z, qua.w);
		setVar("rot", rot);
		setBool("rebuild", true);
		return (rot);
	}

	var transform::setScale(list args) {
		if (args.size() == 0) return genericError("setScale requires a value");
		if (args.getType(0) == typeList) {
			setScale(args);
		} else if (args[0].isVec3()) {
			setVar("scl", args[0]);
			setBool("rebuild", true);
		} else if (args.size() >= 3 && args.isAllNumber()) {
			setVar(
				"scl",
				vec3f(
					args[0].getFloat(),
					args[1].getFloat(),
					args[2].getFloat()));
			setBool("rebuild", true);
		}
		return var();
	}

	var transform::getEuler(list) {
		auto rot = getVar("rot");
		auto qua = bx::Quaternion{rot.getFloat(0), rot.getFloat(1),
															rot.getFloat(2), rot.getFloat(3)};
		auto v3 = bx::toEuler(qua);
		return vec3f(v3.x, v3.y, v3.z);
	}

	var transform::getPosition(list) { return getVar("pos"); }

	var transform::getRotation(list) { return getVar("rot"); }

	var transform::getScale(list) { return getVar("scl"); }

	var transform::getMatrix(list) {
		if (getBool("rebuild", false) == false) {
			if (getType("mtx") != typeNull) return getVar("mtx");
		}

		auto pos = getVar("pos");
		auto rot = getVar("rot");
		auto scl = getVar("scl");

		// Compose T*R*S with bx directly: the var operators used here
		// before (mat * scale, mat * quat, mat + pos) each REPLACE the
		// matrix with a fresh translate/scale/rotate instead of
		// composing, so the last step (translation) was the whole
		// matrix and rotations silently vanished.
		float t[16], r[16], s[16], sr[16], trs[16];
		bx::mtxTranslate(
			t, pos.getFloat(0), pos.getFloat(1), pos.getFloat(2));
		bx::mtxFromQuaternion(
			r,
			bx::Quaternion(
				rot.getFloat(0), rot.getFloat(1), rot.getFloat(2),
				rot.getFloat(3)));
		bx::mtxScale(
			s, scl.getFloat(0), scl.getFloat(1), scl.getFloat(2));
		// Row-major chain: scale, then rotate, then translate.
		bx::mtxMul(sr, s, r);
		bx::mtxMul(trs, sr, t);

		auto results = mat4x4f({
			trs[0], trs[1], trs[2], trs[3],
			trs[4], trs[5], trs[6], trs[7],
			trs[8], trs[9], trs[10], trs[11],
			trs[12], trs[13], trs[14], trs[15]});
		setVar("mtx", results);
		erase("rebuild");
		return results;
	}

	var transform::getWorldMatrix(list) {
		auto parentObj = entity();
		assignObject<entity>("object", parentObj);
		auto parentChain = list();
		while (parentObj) {
			parentChain.pushObject(parentObj);
			parentObj = parentObj.getObject<entity>("parent");
		}
		auto results = mat4x4f({});
		for (auto it = parentChain.rbegin();
				 it != parentChain.rend();
				 ++it) {
			it->assignObject<entity>(parentObj);
			auto comps = parentObj.getList("components");
			auto parentTrans = comps.find(transform::getPrototype());
			if (parentTrans != comps.end()) {
				auto trans = parentTrans->getObject<transform>();
				results = results * trans.getMatrix();
			}
		}
		return results;
	}

	var transform::reset(list) {
		setVar("pos", vec3f(0, 0, 0));
		setVar("rot", quatf(0, 0, 0, 1));
		setVar("scl", vec3f(1, 1, 1));
		setVar("mtx", mat4x4f({}));
		erase("rebuild");
		return var();
	}
}  // namespace gold
