#include <iostream>

#include "../../../include/bootstrap.hpp"
#include "session.hpp"
#include "template.hpp"
#include "upload.hpp"
#include "user.hpp"

using namespace gold;

namespace gg {
	using namespace std;
	using namespace gold;
	using namespace gg::bs;
	using link = HTML::link;
	using div = HTML::div;

	gold::list user::userDelete(session s, user u, list data) {
		auto greetings =
			string("Warning, ") + u.getString("firstName") + "!";
		auto q = s.getQuery().getString();
		auto items = list();
		auto inputs = list();
		for (auto it = data.begin(); it != data.end(); ++it) {
			auto item = user();
			if (it->isString()) {
				item = user::findOne({obj{{"_id", it->getString()}}})
								 .getObject<user>();
			} else if (it->isObject()) {
				item = it->getObject<user>();
			}
			if (item) {
				auto iid = item.getID();
				if (iid == u.getID() || u.isAdmin()) {
					inputs.pushObject(input({
						atts{
							{"type", "hidden"},
							{"id", "items"},
							{"name", "items[]"},
							{"value", item.getID()},
						},
					}));
					items.pushObject(user::userMediaItem(s, u, item));
				}
			}
		}
		auto paraContent =
			data.size() == 1
				? p({"Just to confirm things, you're about to delete "
						 "a user from the system. You will not be "
						 "able to recover them after doing this. You "
						 "should back up what you want."})
				: p(
						{"Just to confirm things, you're about to delete "
						 "several users (" +
						 std::to_string(data.size()) +
						 ") from the system. You will not be "
						 "able to recover them after doing this. You "
						 "should back up what you want."});
		auto content = gold::list{
			div({
				atts{{"class", "card pageCard text-light bg-dark"}},
				div({
					atts{{"class", "card-body"}},
					h5({
						atts{{"class", "card-title"}},
						greetings,
					}),
					paraContent,
					items,
					form({
						atts{
							{"method", "post"},
							{"action", "/delete/user" + q}},
						inputs,
						button({
							obj{
								{"type", "submit"},
								{"class", "btn btn-primary float-right"},
							},
							"Confirm",
						}),
					}),
				}),
			}),
		};
		return content;
	}
}  // namespace gg