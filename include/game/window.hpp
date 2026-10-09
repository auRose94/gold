#pragma once

#include <memory>

#include "types.hpp"

namespace gold {
	class windowSystem;

	/** Window position constant: center on the current display. */
	constexpr int32_t WindowCentered = 0x2FFF;

	struct window : public object {
	 protected:
		static object& getPrototype();
		static std::shared_ptr<windowSystem>& backend();

	 public:
		window();
		window(object config);

		/** The active window system backend (may be null before create). */
		windowSystem* getBackend();

		var setSize(list args);
		var setPos(list args);
		var setTitle(list args);
		var setFullscreen(list args);
		var setBorderless(list args);
		var create(list args = {});
		var destroy(list args = {});
		var handleEvent(list args);
		var getConfig(list args = {});

		// Event handlers. Defaults on the prototype update window state;
		// apps override by setFunc/setMethod("onQuit", ...) etc.
		var onQuit(list);
		var onResized(list);
		var onMoved(list);
		var onShown(list);
		var onHidden(list);
		var onMinimized(list);
		var onMaximized(list);
		var onRestored(list);
		var onFocusGained(list);
		var onFocusLost(list);
		var onKeyDown(list);
		var onKeyUp(list);
		var onTextInput(list);
		var onMouseDown(list);
		var onMouseUp(list);
		var onMouseMove(list);
		var onMouseWheel(list);
	};
}  // namespace gold
