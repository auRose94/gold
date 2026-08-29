#include <iostream>

#include "bootstrap.hpp"
#include "session.hpp"
#include "template.hpp"
#include "user.hpp"

namespace gg {
	using namespace std;
	using namespace gold;
	using namespace gg::bs;
	using div = HTML::div;

	std::map<string, string> user::nameMap =
		std::map<string, string>({
			{"firstName", "First"},
			{"midName", "Middle"},
			{"lastName", "Last"},
			{"phone", "Phone"},
			{"birthday", "Birthday"},
			{"gender", "Gender"},
			{"country", "Country"},
			{"zone", "Zone"},
			{"city", "City"},
			{"address1", "Address 1"},
			{"address2", "Address 2"},
			{"zip", "ZIP"},
			{"email", "Email"},
			{"confEmail", "Confirm Email"},
			{"password", "Password"},
			{"confPassword", "Confirm Password"},
		});

	gold::list user::userRegister(obj data, obj errs) {
		auto errorBody = tbody();
		if (errs)
			for (auto it = errs.begin(); it != errs.end(); ++it) {
				//<th scope="row">1</th>
				errorBody += {
					tr({
						th({obj{{"scope", "row"}}, nameMap[it->first]}),
						th(list::initList{it->second}),
					}),
				};
			}
		auto errorTable =
			table({obj{{"class", "table"}},
						 thead({
							 tr({
								 th({obj{{"scope", "bs::col"}}, "Field"}),
								 th({obj{{"scope", "bs::col"}}, "Error"}),
							 }),
						 }),
						 errorBody});
		auto gender = data.getString("gender");
		auto content = gold::list({
			div({
				obj{{"class", "card pageCard text-light bg-dark"}},
				div({
					obj{{"class", "card-body"}},
					h5({
						obj{{"class", "card-title"}},
						"Registration",
					}),
					errs.size() > 0 ? div({errorTable}) : div{},
					form({
						obj{{"method", "post"}},
						row({
							bs::col({formInputRow(
								"text", "firstName", "First", true,
								errs.getString("firstName"),
								obj{{"value", data.getString("firstName")}})}),
							bs::col({formInputRow(
								"text", "midName", "Middle", false,
								errs.getString("midName"),
								obj{{"value", data.getString("midName")}})}),
							bs::col({formInputRow(
								"text", "lastName", "Last", true,
								errs.getString("lastName"),
								obj{{"value", data.getString("lastName")}})}),
						}),
						row({
							bs::col({formInputRow(
								"tel", "phone", "Phone", true,
								errs.getString("phone"),
								{
									{"pattern", "\\+[0-9]{8,}"},
									{"placeholder", "+###########"},
									{"autocorrect", "on"},
									{"value", data.getString("phone")},
								})}),
							bs::col({formInputRow(
								"date", "birthday", "Birthday", true,
								errs.getString("birthday"),
								obj{{"value", transformBirthday(data.getString(
																"birthday"))}})}),
							bs::col({formSelect(
								"gender", "Gender", false, {},
								{
									formSelectOption("(Blank)", "", gender == ""),
									formSelectOption(
										"Non-Binary", "n", gender == "n"),
									formSelectOption(
										"Female", "f", gender == "f"),
									formSelectOption("Male", "m", gender == "m"),
								})}),
						}),
						row({
							bs::col({formInputRow(
								"text", "country", "Country", true,
								errs.getString("country"),
								obj{{"value", data.getString("country")}})}),
							bs::col({formInputRow(
								"text", "zone", "State", true,
								errs.getString("zone"),
								obj{{"value", data.getString("zone")}})}),
							bs::col({formInputRow(
								"text", "city", "City", true,
								errs.getString("city"),
								obj{{"value", data.getString("city")}})}),
						}),
						row({
							bs::col({formInputRow(
								"text", "address1", "Address 1", true,
								errs.getString("address1"),
								obj{{"value", data.getString("address1")}})}),
							bs::col({formInputRow(
								"text", "address2", "Address 2", false,
								errs.getString("address2"),
								obj{{"value", data.getString("address2")}})}),
							bs::col({formInputRow(
								"text", "zip", "ZIP", true,
								errs.getString("zip"),
								obj{{"value", data.getString("zip")}})}),
						}),
						row({
							bs::col({formInputRow(
								"email", "email", "Email", true,
								errs.getString("email"),
								obj{{"value", data.getString("email")}})}),
							bs::col({formInputRow(
								"email", "confEmail", "Confirm", true,
								errs.getString("confEmail"),
								obj{{"value", data.getString("confEmail")}})}),
						}),
						row({
							bs::col({formInputRow(
								"password", "password", "Password", true,
								errs.getString("password"))}),
							bs::col({formInputRow(
								"password", "confPassword", "Confirm", true,
								errs.getString("confPassword"))}),
						}),
						button({
							obj{
								{"type", "submit"},
								{"class", "btn btn-primary float-right"},
							},
							"Submit",
						}),
					}),
				}),
			}),
		});
		return content;
	}
}  // namespace gg