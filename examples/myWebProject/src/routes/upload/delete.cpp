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

	gold::list upload::uploadDelete(
		session s, user u, list data) {
		auto greetings =
			string("Warning, ") + u.getString("firstName") + "!";
		auto q = s.getQuery().getString();
		auto items = list();
		auto inputs = list();
		for (auto it = data.begin(); it != data.end(); ++it) {
			auto item = upload();
			if (it->isString()) {
				item = upload::findOne({obj{{"_id", it->getString()}}})
								 .getObject<upload>();
			} else if (it->isObject()) {
				item = it->getObject<upload>();
			}
			if (item && (item.isOwner({u}) || u.isAdmin())) {
				inputs.pushObject(input({
					atts{
						{"type", "hidden"},
						{"id", "items"},
						{"name", "items[]"},
						{"value", item.getID()},
					},
				}));
				items.pushObject(upload::uploadMediaItem(s, u, item));
			}
		}
		auto paraContent =
			data.size() == 1
				? p({"Just to confirm things, you're about to delete "
						 "an upload from the system. You will not be "
						 "able to recover them after doing this. You "
						 "should back up what you want."})
				: p(
						{"Just to confirm things, you're about to delete "
						 "several uploads (" +
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
							{"action", "/delete/upload" + q}},
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