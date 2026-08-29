#include "error.hpp"

#include <html.hpp>

namespace gg {
	using namespace std;
	using namespace HTML;
	using div = HTML::div;
	gold::list errorPage(gold::list args) {
		auto err = args[0].getError();
		auto content = gold::list{
			div({
				obj{{"class", "card pageCard text-light bg-dark"}},
				div({
					obj{{"class", "card-body"}},
					h5({
						obj{{"class", "card-title"}},
						"Error",
					}),
					p({string(*err)}),
				}),
			}),
		};
		return content;
	}
}  // namespace gg