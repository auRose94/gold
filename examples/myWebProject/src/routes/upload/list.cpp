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
	gold::list upload::uploadList(
		session sesh, user u, obj filter, list results) {
		auto q = string();
		if (sesh && !sesh.getBool("useCookies"))
			q = "?s=" + sesh.getString("_id");
		const auto tableHeight = to_string(94 * 6);
		auto content = gold::list{
			div({
				obj{{"class", "card pageCard text-light bg-dark"}},
				div({
					obj{{"class", "card-body"}},
					h5({
						obj{{"class", "card-title"}},
						"List Uploads",
					}),
				}),
				div({
					table({
						atts{
							{"class", "table text-light bg-dark"},
							{"data-toggle", "table"},
							{"data-pagination", "true"},
							{"data-search", "true"},
							{"data-show-toggle", "true"},
							{"data-smart-display", "true"},
							{"data-click-to-select", "true"},
							{"data-multiple-select-row", "true"},
							{"data-unique-id", "_id"},
							{"data-height", tableHeight},
							{"data-data-type", "json"},
							{"data-show-refresh", "true"},
							{"data-show-columns", "true"},
							{"data-show-fullscreen", "true"},
							{"data-buttons-align", "left"},
							{"data-trim-on-search", "true"},
							{"data-url", "/list/upload.json" + q},
						},
						thead({
							tr({
								th({
									atts{
										{"scope", "col"},
										{"class", "text-light"},
										{"data-field", "state"},
										{"data-checkbox", "true"},
									},
								}),
								th(
									{atts{
										 {"scope", "col"},
										 {"class", "text-light"},
										 {"data-field", "desc"},
										 {"data-sortable", "true"},
										 {"data-width", "40"},
										 {"data-width-unit", "%"},
										 {"data-formatter", "window.descFormatter"},
									 },
									 "Description"}),
								th({atts{
											{"scope", "col"},
											{"class", "text-light"},
											{"data-field", "verified"},
											{"data-sortable", "true"},
										},
										"Verified"}),
								th({atts{
											{"scope", "col"},
											{"class", "text-light"},
											{"data-field", "hide"},
											{"data-sortable", "true"},
										},
										"Hide"}),
								th({atts{
											{"scope", "col"},
											{"class", "text-light"},
											{"data-field", "nsfw"},
											{"data-sortable", "true"},
										},
										"NSFW"}),
								th({atts{
											{"scope", "col"},
											{"class", "text-light"},
											{"data-field", "mime"},
											{"data-sortable", "true"},
										},
										"MIME"}),
								th({atts{
											{"scope", "col"},
											{"class", "text-light"},
											{"data-field", "license"},
											{"data-sortable", "true"},
										},
										"License"}),
								th({atts{
											{"scope", "col"},
											{"class", "text-light"},
											{"data-field", "attr"},
											{"data-sortable", "true"},
										},
										"Attribution"}),
								th({atts{
											{"scope", "col"},
											{"class", "text-light"},
											{"data-field", "size"},
											{"data-sortable", "true"},
										},
										"Size"}),
								th({atts{
											{"scope", "col"},
											{"class", "text-light"},
											{"data-field", "width"},
											{"data-sortable", "true"},
										},
										"Width"}),
								th({atts{
											{"scope", "col"},
											{"class", "text-light"},
											{"data-field", "height"},
											{"data-sortable", "true"},
										},
										"Height"}),
								th(
									{atts{
										 {"scope", "col"},
										 {"class", "text-light"},
										 {"data-field", "created"},
										 {"data-sortable", "true"},
										 {"data-width", "10"},
										 {"data-width-unit", "%"},
										 {"data-formatter", "window.dateFormatter"},
									 },
									 "Created"}),
								th(
									{atts{
										 {"scope", "col"},
										 {"class", "text-light"},
										 {"data-field", "updated"},
										 {"data-sortable", "true"},
										 {"data-width", "10"},
										 {"data-width-unit", "%"},
										 {"data-formatter", "window.dateFormatter"},
									 },
									 "Modified"}),
							}),
						}),
					}),
				}),
				script({
					atts{
						{"src", "/js/main/unixTime.js"},
					},
				}),
				script({
					atts{
						{"src", "/js/main/uploadList.js"},
					},
				}),
				script({
					atts{
						{"defer", true},
						{"src",
						 "/js/bootstrap-table/dist/bootstrap-table.min.js"},
					},
				}),
				link({obj{
					{"rel", "stylesheet"},
					{"href",
					 "/css/bootstrap-table/dist/bootstrap-table.min.css"},
				}}),
			}),
		};
		return content;
	}
}  // namespace gg