#include "index.hpp"

#include <html.hpp>
#include <iostream>

#include "template.hpp"

namespace gg {
	using div = HTML::div;
	void setIndexRoute(database, server serv) {
		func getIndex = [](const list& args) -> gold::var {
			using namespace HTML;
			auto req = args[0].getObject<request>();
			auto res = args[1].getObject<response>();
			return res.end({getTemplate(
				req,
				{
					div({
						obj{{"class", "card pageCard text-light bg-dark"}},
						div(
							{obj{{"class", "card-body"}},
							 h5({
								 obj{{"class", "card-title"}},
								 "Welcome!",
							 }),
							 p({"This is one of the gold framework examples. "
									"This project will allow you to get started "
									"developing full-stack web applications with "
									"C++. A lot of the design philosophies used "
									"in this project are inspired by frameworks "
									"from NodeJS. Trying to make an organic, "
									"flexible, and powerful framework that will "
									"create great apps."}),
							 p({"This project makes REST requests over HTTP "
									"and serves HTML rendered from gold "
									"data: routes are functions, pages are "
									"built with the HTML element builders, "
									"and documents persist through the "
									"file-backed dataStore."}),
							 p({"Browse the example: register a user, edit "
									"the session, and look at the route "
									"sources in `src/routes`."})}),
					}),
				})});
		};

		serv.get({"/", getIndex});
	}
}  // namespace gg