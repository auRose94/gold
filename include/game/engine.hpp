#pragma once

/* <Includes> */
#include <set>

#include "types.hpp"
#include "renderable.hpp"
#include "component.hpp"
/* </Includes> */

namespace gold {
	struct engine : public object {
	 protected:
		static object& getPrototype();
		friend struct world;

		void registerComponent(component& comp);

	 public:
		string getSettingsDir();
		string getSettingsPath();

		engine();
		engine(string company, string gameName);
		static set<string> allowedConfigNames();

		var destroy();
		var start();
		var initialize();
		var loadSettings();
		var saveSettings();
		var handleEntity(list args);
		var addElement(list args);
		var getPrimaryCamera();

		engine& operator+=(list element);
		engine& operator-=(list element);
	};
}  // namespace gold