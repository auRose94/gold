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
	 public:
		// Public: plugins, tests, and scripts need the prototype to
		// bootstrap or detect engines (same rationale as shape's).
		static object& getPrototype();

	 protected:
		friend struct world;

		void boot(string company, string gameName);
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
		// Same as above, with the console arguments overriding the
		// backend selections the settings file made (flags: window-backend,
		// render-backend, renderer — see backendOverrides).
		engine(string company, string gameName, int argc, char* argv[]);
		static set<string> allowedConfigNames();

		/** Console-argument backend selection: takes the raw argument
		 *  tokens (argv contents; a leading program name is fine) and
		 *  returns a settings-shaped overrides object — "window"/
		 *  "graphics" sections — or an empty object when nothing matched.
		 *  Recognizes --window-backend=NAME[,FALLBACK...],
		 *  --render-backend=NAME and --renderer=NAME, each also as
		 *  "--flag value". */
		static object backendOverrides(list args);
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
