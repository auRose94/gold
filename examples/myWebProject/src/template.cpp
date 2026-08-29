#include "template.hpp"

#include "navbar.hpp"
#include "session.hpp"
#include "user.hpp"

namespace gg {
	using namespace std;
	using link = HTML::link;
	using div = HTML::div;
	using span = HTML::span;

	html getTemplateEx(
		user u, session s, string path, gold::list content,
		gold::list header) {
		using namespace HTML;
		auto sessionID = s ? s.getID() : "";
		auto q = string();
		if (s && !s.getBool("useCookies")) q = "?s=" + sessionID;
		auto headEl = head({
			obj{{"lang", "en"}},
			title({"MyWebProject"}),
			meta{{obj{{"charset", "utf-8"}}}},
			meta({obj{
				{"name", "viewport"},
				{"content",
				 "width=device-width, initial-scale=1, "
				 "shrink-to-fit=no"},
			}}),
			link({obj{
				{"rel", "shortcut icon"},
				{"href", "/assets/favicon.ico"},
			}}),
			link({obj{
				{"rel", "stylesheet"},
				{"href", "/css/bootstrap/bootstrap.min.css"},
			}}),
			link({obj{
				{"rel", "stylesheet"},
				{"href", "/css/bootstrap/bootstrap-utilities.min.css"},
			}}),
			link({obj{
				{"rel", "stylesheet"},
				{"href", "/css/bootstrap/bootstrap-reboot.min.css"},
			}}),
			link({obj{
				{"rel", "stylesheet"},
				{"href", "/css/bootstrap/bootstrap-grid.min.css"},
			}}),
			link({obj{
				{"rel", "stylesheet"},
				{"href", "/css/main/baseStyle.css"},
			}}),
			script({obj{{"src", "/js/main/jquery-3.5.1.js"}}}),
			script({obj{{"src", "/js/main/luxon.min.js"}}}),
			script({obj{{"src", "/js/fontawesome/js/all.min.js"}}}),
			script(
				{obj{{"src", "/js/bootstrap/bootstrap.bundle.min.js"},
						 {"defer", true}}}),
		});
		headEl += header;
		auto bodyEl = body({
			nav({
				obj{
					{"class",
					 "navbar navbar-expand-lg navbar-dark bg-dark p-1 "
					 "fixed-top"},
				},
				a({
					obj{
						{"class", "navbar-brand"},
						{"href", "/" + q},
					},
					"MyWebProject",
				}),
				button({
					obj{
						{"class", "navbar-toggler"},
						{"type", "button"},
						{"data-toggle", "collapse"},
						{"data-target", "#navbarSupportedContent"},
						{"aria-controls", "navbarSupportedContent"},
						{"aria-expanded", "false"},
						{"aria-label", "Toggle navigation"},
					},
					span({
						obj{
							{"class", "navbar-toggler-icon"},
						},
					}),
				}),
				div({
					obj{
						{"class", "collapse navbar-collapse"},
						{"id", "navbarSupportedContent"},
					},
					navbar(u, s, path),
					form({
						obj{
							{"class", "form-inline my-2 my-lg-0 d-flex"},
						},
						input({
							obj{
								{"class", "form-control mr-sm-2"},
								{"type", "search"},
								{"placeholder", "Search"},
								{"aria-label", "Search"},
							},
						}),
						button({
							obj{
								{"class",
								 "btn btn-outline-success my-2 my-sm-0"},
								{"type", "submit"},
							},
							"Search",
						}),
					}),
				}),
			}),
		});
		bodyEl += content;
		return html({
			headEl,
			bodyEl,
		});
	}

	html getTemplate(
		request req, gold::list content, gold::list header) {
		using namespace HTML;
		auto path = req.getString("path");
		auto sesh =
			req.callMethod("getSession").getObject<session>();
		user u;
		if (!sesh.isExpired()) {
			u = req.callMethod("getUser").getObject<user>();
		}
		return getTemplateEx(u, sesh, path, content, header);
	}  // namespace gg
}  // namespace gg