#include "camera.hpp"

#include <bx/math.h>

#include "graphics.hpp"
#include "renderBackend.hpp"

#include "transform.hpp"

namespace gold {
	using namespace std;

	object& camera::getPrototype() {
		static auto proto = obj({
			{"offset", vec2f(0, 0)},
			{"fov", 60},
			{"depth", 1.0f},
			{"near", 0.1f},
			{"far", 100.0f},
			{"flags", uint16_t(BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH)},
			{"rgba", uint32_t(0x6ab0deff)},
			{"stencil", uint8_t(0)},
			{"proto", entity::getPrototype()},
		});
		return proto;
	}

	var camera::setViewClear(list args) {
		if (args.isAllNumber())
			for (auto it = args.begin(); it != args.end(); ++it) {
				auto t = it->getType();
				if (t == typeUInt16)
					setVar("flags", *it);
				else if (t == typeUInt32)
					setVar("rgba", *it);
				else if (t == typeUInt8)
					setVar("stencil", *it);
				else if (t == typeFloat || t == typeDouble)
					setVar("depth", *it);
			}
		else if (args.size() > 0 && args[0].isObject()) {
			auto c = args[0].getObject();
			auto flags = c.getVar("flags");
			auto rgba = c.getVar("rgba");
			auto stencil = c.getVar("stencil");
			auto depth = c.getVar("depth");
			if (flags.getType() == typeUInt16) setVar("flags", flags);
			if (rgba.getType() == typeUInt32) setVar("rgba", rgba);
			if (stencil.getType() == typeUInt8)
				setVar("stencil", stencil);
			auto dt = depth.getType();
			if (dt == typeFloat || dt == typeDouble)
				setVar("depth", depth);
		}
		return var();
	}

	var camera::setViewSize(list args) {
		if (args.size() == 0) return genericError("setViewSize requires a value");
		if (args[0].isList()) {
			setViewSize(args[0].getList());
		} else if (args[0].isVec2()) {
			setVar("size", args[0]);
		} else if (args.size() >= 2 && args.isAllNumber())
			setVar(
				"size", vec2f(args[0].getFloat(), args[1].getFloat()));
		return var();
	}
	var camera::setViewOffset(list args) {
		if (args.size() == 0) return genericError("setViewOffset requires a value");
		if (args[0].isList()) {
			setViewOffset(args[0].getList());
		} else if (args[0].isVec2()) {
			setVar("offset", args[0]);
		} else if (args.size() >= 2 && args.isAllNumber())
			setVar(
				"offset",
				vec2f(args[0].getFloat(), args[1].getFloat()));
		return var();
	}

	var camera::setViewTransform(list args) {
		auto viewId =
			args.size() > 0 ? args[0].getUInt16() : uint16_t(0);
		auto parentTrans = getTransform();
		auto mtx = parentTrans.getWorldMatrix();

		auto zero = vec4f(0, 0, 0, 1);
		auto forward = vec4f(0, 0, 1, 1);
		auto at = mtx * forward;
		auto eye = mtx * zero;

		auto view = lookAt(eye, at);

		auto fov = getFloat("fov");
		auto near = getFloat("near");
		auto far = getFloat("far");
		auto size = getVar("size");
		auto width = size.getFloat(0);
		auto height = size.getFloat(1);
		auto ratio = height != 0.0f ? width / height : 1.0f;
		// Depth range comes from the active backend (GL vs D3D conventions).
		auto backend = gfxBackend::backend();
		const bool homo =
			backend ? backend->homogeneousDepth() : true;
		auto proj = projection(fov, ratio, near, far, homo);
		if (backend)
			backend->viewTransform(
				(uint8_t)viewId, view.getPtr(), proj.getPtr());
		return var();
	}

	var camera::setView(list args) {
		auto viewId =
			args.size() > 0 ? args[0].getUInt16() : uint16_t(0);
		auto size = getVar("size");
		auto offset = getVar("offset");
		auto width = size.getUInt16(0);
		auto height = size.getUInt16(1);
		auto x = offset.getUInt16(0);
		auto y = offset.getUInt16(1);
		auto flags = getUInt16("flags");
		auto rgba = getUInt32("rgba");
		auto stencil = getUInt8("stencil");
		auto depth = getFloat("depth");
		if (auto back = gfxBackend::backend()) {
			back->viewClear(
				(uint8_t)viewId, flags, rgba, depth, stencil);
			back->viewRect((uint8_t)viewId, x, y, width, height);
		}
		setViewTransform({viewId});
		return var();
	}

	camera::camera() : entity() {}

	camera::camera(object config) : entity(config) {
		setParent(getPrototype());
	}
}  // namespace gold
