#pragma once

#include "types.hpp"

namespace gold {
	namespace CSS {

		// CSS Rule represents a selector + declarations
		struct Rule : public object {
			Rule() {}
			Rule(const string& sel, const object& decl) {
				setString("selector", sel);
				setObject("declarations", decl);
			}
		};

		// Parse a CSS string into a list of rules
		list parseCSS(const string& css);

		// Parse a single CSS rule (selector + body)
		Rule parseRule(const string& css, size_t& pos);

		// Parse CSS declarations from a rule body
		object parseDeclarations(const string& body);

		// Get list of common shorthand CSS properties
		vector<string> getShorthandProperties();

	}  // namespace CSS
}  // namespace gold
