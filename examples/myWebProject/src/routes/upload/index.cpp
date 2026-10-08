#include <iostream>

#include "bootstrap.hpp"
#include "session.hpp"
#include "template.hpp"
#include "upload.hpp"
#include "user.hpp"

using namespace gold;

namespace gg {
	using namespace std;
	using namespace gold;
	using namespace gg::bs;
	using div = HTML::div;
	using link = HTML::link;
	gold::list upload::uploadIndex(gg::session sesh, user u, upload item) {
		auto q = string();
		if (sesh && !sesh.getBool("useCookies"))
			q = "?s=" + sesh.getString("_id");
		auto iconSrc =
			"/upload/" + item.getString("_id") + "/image"+q;
		auto content = gold::list{
			div({
				obj{{"class", "card pageCard text-light bg-dark"}},
				div({
					obj{{"class", "card-body"}},
					div({
						img({atts{
							{"src", iconSrc},
							{"class", "w-100 shadow-lg mr-3"},
						}}),
						div({
							atts{{"class", "descContainer"}},
							p({
								item.getString("desc"),
							}),
						}),
					}),

				}),
			}),
		};
		return content;
	}
}  // namespace gg