#include <iostream>

#include "bootstrap.hpp"
#include "session.hpp"
#include "template.hpp"
#include "user.hpp"

using namespace gold;

namespace gg {
	using namespace std;
	using namespace gold;
	using namespace gg::bs;
	using div = HTML::div;
	gold::list user::userHome(session sesh) {
		auto u = sesh.getUser().getObject<user>();
		auto greetings =
			string("Hello, ") + u.getString("firstName") + "!";
		auto content = gold::list{
			div({
				obj{{"class", "card pageCard text-light bg-dark"}},
				div({
					obj{{"class", "card-body"}},
					h5({
						obj{{"class", "card-title"}},
						greetings,
					}),
				}),

			}),
		};
		return content;
	}
}  // namespace gg