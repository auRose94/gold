#pragma once

/* <Includes> */
#include <set>

#include "component.hpp"
#include "renderable.hpp"
#include "types.hpp"
/* </Includes> */

namespace gold {
	using std::set;
	using std::string;
	struct engine : public object {
	 protected:
		static object& getPrototype();
		friend struct world;

		void registerComponent(component& comp);

		void cleanUp();

		void sortComponents();
		void initComps();
		void callMethod(string m, list args = {});
		list findAll(object proto);
		void drawScene();

	 public:
		string getSettingsDir();
		string getSettingsPath();

		engine();
		engine(string company, string gameName);
		static set<string> allowedConfigNames();
		var start(list args = {});
		var initialize(list args = {});
		var loadSettings(list args = {});
		var saveSettings(list args = {});
		var getPrimaryCamera(list args = {});

		//args: (component, entity), ...
		var addElement(list args);
		//args: (component, entity), ...
		var removeElement(list args);

		//args: (component, entity), ...
		engine& operator+=(list element);
		//args: (component, entity), ...
		engine& operator-=(list element);
	};
}  // namespace gold
