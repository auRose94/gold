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
	gold::list user::userLogin(
		string response, string emailError, string passwordError) {
		string eText, eClass, pText, pClass;
		if (emailError.empty()) {
			eText = "Your address is in good hands";
			eClass = "form-text text-muted";
		} else {
			eText = emailError;
			eClass = "form-text text-danger";
		}
		if (passwordError.empty()) {
			pText = "We securely store everything.";
			pClass = "form-text text-muted";
		} else {
			pText = passwordError;
			pClass = "form-text text-danger";
		}
		auto em = formInputRow("email", "email", "Email", true);
		em += {hSmall({
			atts{{"id", "emailHelp"}, {"class", eClass}},
			eText,
		})};
		auto ps =
			formInputRow("password", "password", "Password", true);
		ps += {hSmall({
			atts{{"id", "passwordHelp"}, {"class", pClass}},
			pText,
		})};
		auto content = gold::list{
			div({
				atts{{"class", "card loginCard text-light bg-dark"}},
				div({
					atts{{"class", "card-body"}},
					h5({
						atts{{"class", "card-title"}},
						"Login",
					}),
				}),
				h3({atts{
							{"class", response.empty() ? "" : "text-danger"}},
						response}),
				form({
					atts{{"id", "login"}, {"method", "post"}},
					em,
					ps,
					formCheck("rememberMe", "Remember Me!"),
					button({
						atts{
							{"id", "formSubmit"},
							{"type", "submit"},
							{"class", "btn btn-primary float-right"},
						},
						"Submit",
					}),
				}),
			}),
			script({
				atts{
					{"src", "/js/main/loginCallback.js"},
				},
			}),
		};
		return content;
	}
}  // namespace gg