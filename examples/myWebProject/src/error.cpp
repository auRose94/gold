#include "error.hpp"

#include <html.hpp>
#include <goldjs.hpp>

namespace gg {
	using namespace std;
	using namespace HTML;
	using div = HTML::div;
	gold::list errorPage(gold::list args) {
		auto err = args[0].getError();
		auto content = gold::list{
			div({
				jo("class", "card pageCard text-light bg-dark"),
				div({
					jo("class", "card-body"),
					h5({
						jo("class", "card-title"),
						"Error",
					}),
					p({string(*err)}),
				}),
			}),
		};
		return content;
	}
}  // namespace gg