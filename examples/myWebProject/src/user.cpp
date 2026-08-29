#include "user.hpp"

#include <cstddef>
#include <iostream>
#include <locale>
#include <random>
#include <regex>

#include "bootstrap.hpp"
#include "error.hpp"
#include "file.hpp"
#include "session.hpp"
#include "template.hpp"
#include "user.hpp"
#include "utility.hpp"
#include <goldjs.hpp>

namespace gg {
	using namespace std;

	using div = gold::HTML::div;

	obj& user::getPrototype() {
		static auto proto = obj({
			{"email", ""},
			{"hash", ""},
			{"salt", ""},
			{"firstName", ""},
			{"midName", ""},
			{"lastName", ""},
			{"company", ""},
			{"birthday", ""},
			{"gender", ""},
			{"phone", ""},
			{"address1", ""},
			{"address2", ""},
			{"country", ""},
			{"city", ""},
			{"zone", ""},
			{"zip", ""},
			{"icon", ""},
			{"admin", false},
			{"proto", gold::model::getPrototype()},
		});
		return proto;
	}

	var user::userCard(session s, user u, user data) {
		auto q = s.getQuery().getString();
		auto iconSrc = "/user/" + data.getString("_id") + q;
		auto iconPage =
			"/upload/" + data.getString("icon") + "/image" + q;
		auto fullName = data.getString("firstName") + " " +
										data.getString("midName") + " " +
										data.getString("lastName");
		auto ret = div({
			atts{{"class", "card"}},
			a({
				atts{{"href", iconSrc}},
				img({
					atts{
						{"class", "mr-3"},
						{"src", iconPage},
						{"width", 64},
						{"height", 64},
					},
				}),
			}),
			div(
				{atts{{"class", "card-body"}},
				 h5({atts{{"class", "mt-0"}}, fullName}),
				 p({data.getString("shortDesc")})}),
		});
		return ret;
	}

	var user::userMediaItem(session s, user u, user data) {
		auto q = s.getQuery().getString();
		auto iconSrc = "/user/" + data.getString("_id") + q;
		auto iconPage =
			"/upload/" + data.getString("icon") + "/image" + q;
		auto fullName = data.getString("firstName") + " " +
										data.getString("midName") + " " +
										data.getString("lastName");
		auto ret = div({
			atts{{"class", "media d-inline-flex m-3"}},
			a({
				atts{{"href", iconSrc}},
				img({
					atts{
						{"class", "mr-3"},
						{"src", iconPage},
						{"width", 64},
						{"height", 64},
					},
				}),
			}),
			div(
				{atts{{"class", "media-body"}},
				 h5({atts{{"class", "mt-0"}}, fullName}),
				 p({data.getString("shortDesc")})}),
		});
		return ret;
	}

	void user::setRoutes(database db, server serv) {
		auto colRes = db.getCollection({"user"});
		if (!colRes.isObject()) cerr << colRes << endl;
		auto def = collection();
		auto col = colRes.getObject<collection>();
		getPrototype()["collection"] = col;

		func getUserHome = [](list args) -> gold::var {
			serveArgs(args, req, res);
			auto sesh =
				req.callMethod("getSession").getObject<session>();
			auto d = html();
			if (sesh) {
				auto u = req.callMethod("getUser").getObject<user>();
				auto url = "/home";
				return res.end(
					{getTemplateEx(u, sesh, url, user::userHome(sesh))});
			}

			return req.setYield({true});
		};

		func getLogin = [](list args) -> gold::var {
			serveArgs(args, req, res);
			auto content = user::userLogin();

			return res.end({getTemplate(req, content)});
		};

		func postLogin = [](list args) -> gold::var {
			serveArgs(args, req, res);
			func onDataCallback = [](const list& dargs) -> gold::var {
				auto data = dargs[0].getString();
				auto req = dargs[1].getObject<request>();
				auto res = dargs[2].getObject<response>();
				auto agent = req.getHeader({"user-agent"}).getString();
				string userEmail;
				string userPassword;

				auto p = obj();
				if (req.isWWWFormURLEncoded()) {
					obj::parseURLEncoded(data, p);
					userEmail = p.getString("email");
					userPassword = p.getString("password");
				} else if (req.isJSON()) {
					p = file::parseJSON(data);
					userEmail = p.getString("email");
					userPassword = p.getString("password");
				}

				auto authHeader =
					req.getHeader({"authorization"}).getString();
				if (authHeader.find("Basic") != string::npos) {
					auto encoded = authHeader.substr(6);
					auto decoded = file::decodeBase64(encoded);
					auto str = string(decoded.begin(), decoded.end());
					auto colonIn = str.find(':');
					if (colonIn != string::npos) {
						userEmail = str.substr(0, colonIn);
						userPassword = str.substr(colonIn + 1);
					}
				}

				auto acceptHeader = req.getHeader({"accept"});
				auto allHeaders = req.getAllHeaders();
				auto acceptValue = !acceptHeader.isEmpty()
														 ? acceptHeader.getString()
														 : string("application/json");
				if (acceptValue.find("text/html") != string::npos) {
					auto error = string("");
					if (user::invalidEmail(userEmail, error))
						return res.end(
							{getTemplate(req, user::userLogin("", error))});
					else if (user::invalidPassword(userPassword, error))
						return res.end({getTemplate(
							req, user::userLogin("", "", error))});
					else {
						auto loggedIn =
							user::login(userEmail, userPassword, agent);
						if (loggedIn.isError())
							return res.end({getTemplate(
								req, user::userLogin(*loggedIn.getError()))});
						else if (loggedIn.isList()) {
							auto ret = loggedIn.getList();
							auto sesh = ret[1].getObject<session>();
							sesh.writeSession({obj{
								{"res", res},
								{"redirect", "/home"},
							}});
							return res.end(
								{getTemplate(req, user::userLogin())});
						} else {
							return res.end({getTemplate(
								req, user::userLogin("Invalid credentials."))});
						}
					}
				} else if (
					acceptValue.find("application/json") !=
					string::npos) {
					string error;
					if (user::invalidEmail(userEmail, error)) {
						res.writeStatus({401});
						return res.end({jo("error", error)});
					} else if (user::invalidPassword(
											 userPassword, error)) {
						res.writeStatus({401});
						return res.end({jo("error", error)});
					} else {
						auto loggedIn =
							user::login(userEmail, userPassword, agent);
						if (loggedIn.isError()) {
							res.writeStatus({401});
							return res.end({jo("error", loggedIn)});
						} else if (loggedIn.isList()) {
							auto ret = loggedIn.getList();
							auto sesh = ret[1].getObject<session>();
							sesh.writeSession({jo("res", res)});
							return res.end({jo("session", sesh)});
						} else {
							res.writeStatus({401});
							return res.end({jo("error", "invalid login")});
						}
					}
				} else {
					// Unknown Accept header (e.g. curl's "*/*"): default to
					// the HTML login page instead of never responding.
					return res.end({getTemplate(req, user::userLogin())});
				}
				return gold::var();
			};

			func onAbort = [](list dargs) -> gold::var {
				auto req = dargs[0].getObject<request>();
				auto res = dargs[1].getObject<response>();
				return res.end({getTemplate(
					req, errorPage({genericError("Aborted.")}))});
			};

			res.onAborted({onAbort, req});
			res.onData({onDataCallback, req});

			return gold::var();
		};

		func getRegister = [](list args) -> gold::var {
			serveArgs(args, req, res);
			auto errors = obj();
			return res.end(
				{getTemplate(req, user::userRegister(obj(), errors))});
		};

		func postRegister = [](list pArgs) -> gold::var {
			serveArgs(pArgs, req, res);

			func onDataCallback = [](list args) -> gold::var {
				auto data = args[0].getString();
				auto req = args[1].getObject<request>();
				auto res = args[2].getObject<response>();
				if (req.isWWWFormURLEncoded()) {
					auto params = obj();
					obj::parseURLEncoded(data, params);
					auto u = user::create(params);
					if (u.isObject(user::getPrototype())) {
						// Created
						auto userO = u.getObject<user>();
						auto sesh = session(userO);
						auto agent =
							req.getHeader({"user-agent"}).getString();
						sesh["agent"] = agent;
						auto succ = sesh.save();
						if (succ.isError()) {
							return res.end(
								{getTemplate(req, errorPage({succ}))});
						} else {
							sesh.writeSession({obj{
								{"res", res},
								{"redirect", "/home"},
							}});
							return res.end({getTemplate(
								req, user::userRegister(params, u))});
						}
					} else if (u.isObject()) {
						return res.end({getTemplate(
							req, user::userRegister(params, u))});
					} else if (u.isError()) {
						return res.end({getTemplate(req, errorPage({u}))});
					}
				}

				return res.end(
					{getTemplate(req, user::userRegister({}, {}))});
			};

			func onAbort = [](list args) -> gold::var {
				auto req = args[0].getObject<request>();
				auto res = args[1].getObject<response>();
				res.end({getTemplate(
					req, errorPage({genericError("Aborted.")}))});
				return gold::var();
			};

			res.onAborted({onAbort, req});
			res.onData({onDataCallback, req});

			return gold::var();
		};

		func getOptions = [](list args) -> gold::var {
			serveArgs(args, req, res);
			auto errors = obj();
			auto sesh =
				req.callMethod("getSession").getObject<session>();
			if (sesh) {
				auto u = req.callMethod("getUser").getObject<user>();
				if (u)
					return res.end(
						{getTemplate(req, user::userOptions(u, errors))});
			}

			return req.setYield({true});
		};

		func postOptions = [](list pArgs) -> gold::var {
			using namespace HTML;
			serveArgs(pArgs, req, res);

			func onDataCallback = [](list args) -> gold::var {
				auto data = args[0].getString();
				auto req = args[1].getObject<request>();
				auto res = args[2].getObject<response>();
				auto sesh =
					req.callMethod("getSession").getObject<session>();
				auto u = req.callMethod("getUser").getObject<user>();
				if (req.isWWWFormURLEncoded()) {
					auto params = obj();
					obj::parseURLEncoded(data, params);

					auto seshId = req.getParameter({0}).getString();
					auto pass = params.getString("password");
					auto confPass = params.getString("confPassword");
					auto email = params.getString("email");
					auto confEmail = params.getString("confEmail");
					if (email != "" && confEmail == email)
						u.changeEmail({email});
					if (pass != "" && confPass == pass)
						u.changePassword({pass});
					params.erase("confEmail");
					params.erase("password");
					params.erase("confPassword");
					u.copy(params);
					auto resp = u.save();
					if (resp.isError())
						return res.end(
							{getTemplate(req, errorPage({resp}))});
					else
						return res.end(
							{getTemplate(req, user::userOptions(u, obj()))});
				} else
					return res.end({getTemplate(
						req,
						errorPage({genericError(
							"Could not parse encoded form.")}))});

				return gold::var();
			};

			func onAbort = [](list args) -> gold::var {
				auto req = args[0].getObject<request>();
				auto res = args[1].getObject<response>();
				res.end({getTemplate(
					req, errorPage({genericError("Aborted")}))});
				return gold::var();
			};
			auto sesh =
				req.callMethod("getSession").getObject<session>();
			if (sesh) {
				auto u = req.callMethod("getUser").getObject<user>();
				if (u) {
					res.onAborted({onAbort, req});
					return res.onData({onDataCallback, req});
				}
			}

			return req.setYield({true});
		};

		// Session
		serv.get({"/options", getOptions});
		serv.post({"/options", postOptions});
		serv.get({"/home", getUserHome});

		// Without
		serv.get({"/register", getRegister});
		serv.post({"/register", postRegister});
		serv.get({"/login", getLogin});
		serv.post({"/login", postLogin});
		serv.post({"/auth", postLogin});
	}

	user::user() : model() {}

	user::user(obj data)
		: model(
				getPrototype().getObject<collection>("collection"),
				data) {
		setParent(getPrototype());
	}

	gold::var user::checkPassword(list args) {
		auto p = args[0].getString();
		auto s = getString("salt");
		auto ch = getString("hash");
		auto rph = generateHash(p, s);
		if (rph.isError()) return rph;
		auto ph = rph.getString();
		return gold::var(ch == ph);
	}

	gold::var user::changePassword(list args) {
		auto pass = args[0].getString();
		auto nSalt = randomString(16);
		auto nHash = generateHash(pass, nSalt);
		if (nHash.isError()) return nHash;
		setString("hash", nHash.getString());
		setString("salt", nSalt);
		return gold::var();
	}

	gold::var user::changeEmail(list args) {
		auto email = args[0].getString();
		auto conf = randomString(16);
		setString("emailChangeConf", conf);
		setString("emailChangeValue", email);
		// TODO: Send email confirmation two both addresses; new (to
		// confirm) and old (to verify address)
		return gold::var();
	}

	gold::var user::getIcon(list) {
		auto id = getString("icon");
		return gold::var();
	}

	bool user::isAdmin() { return getBool("admin"); }

	gold::var user::create(obj data) {
		auto cemail = data.getString("confEmail");
		auto cp = data.getString("confPassword");
		auto email = data.getString("email");
		auto p = data.getString("password");
		auto fn = data.getString("firstName");
		auto mn = data.getString("midName");
		auto ln = data.getString("lastName");
		auto com = data.getString("company");
		auto ph = data.getString("phone");
		auto ge = data.getString("gender");
		auto bi = data.getString("birthday");
		auto a1 = data.getString("address1");
		auto a2 = data.getString("address2");
		auto con = data.getString("country");
		auto city = data.getString("city");
		auto zone = data.getString("zone");
		auto zip = data.getString("zip");
		auto icon = data.getString("icon");
		auto ut = data.getString("userType");

		string e;
		auto err = obj({});
		if (email != cemail)
			err.setString(
				"confEmail", "Confirm email doesn't match.");
		if (p != cp)
			err.setString(
				"confPassword", "Confirm password doesn't match.");
		if (invalidEmail(email, e)) err["email"] = e;
		if (invalidPassword(p, e)) err["password"] = e;
		if (invalidFirstName(fn, e)) err["firstName"] = e;
		if (invalidMidName(mn, e)) err["midName"] = e;
		if (invalidLastName(ln, e)) err["lastName"] = e;
		if (invalidCompany(com, e)) err["company"] = e;
		if (invalidPhone(ph, e)) err["phone"] = e;
		if (invalidGender(ge, e)) err["gender"] = e;
		if (invalidBirthday(bi, e)) err["birthday"] = e;
		if (invalidAddress1(a1, e)) err["address1"] = e;
		if (invalidAddress2(a2, e)) err["address2"] = e;
		if (invalidCountry(con, e)) err["country"] = e;
		if (invalidCity(city, e)) err["city"] = e;
		if (invalidZone(zone, e)) err["zone"] = e;
		if (invalidZip(zip, e)) err["zip"] = e;
		if (invalidIcon(icon, e)) err["icon"] = e;
		if (invalidUserType(ut, e)) err["userType"] = e;

		if (err.size() > 0) return err;

		auto col =
			getPrototype().getObject<collection>("collection");
		auto existU = col.findOne({jo("email", email)});
		if (existU.isError())
			return existU;
		else if (existU.isObject())
			return genericError("User with email exists.");

		auto u = user(data);
		auto salt = randomString(16);
		auto hash = generateHash(p, salt);
		u["salt"] = salt;
		u["hash"] = hash;
		// We have to remove sensitive data
		u.erase("password");
		u.erase("confPassword");
		u.erase("confEmail");
		return u.save();
	}

	gold::var user::login(
		string email, string password, string agent) {
		auto col =
			getPrototype().getObject<collection>("collection");
		auto exist = col.findOne({jo("email", email)});
		collection::setParentModel(exist, getPrototype());
		if (exist.isError())
			return exist;
		else if (exist.isObject()) {
			auto u = exist.getObject<user>();
			auto valid = u.checkPassword({password});
			if (valid.isError())
				return valid;
			else if (valid.getBool()) {
				auto sesh = session(u);
				sesh["agent"] = agent;
				auto saved = sesh.save();
				if (saved.isError()) return saved;
				return gold::var(gold::list{u, sesh});
			}
		}
		return gold::var();
	}

	string user::transformBirthday(string value) {
		if (value.size() != 8) {
			return value;
		}
		auto year = value.substr(0, 4);
		auto month = value.substr(4, 2);
		auto day = value.substr(6, 2);
		return year + "-" + month + "-" + day;
	}

	bool user::invalidEmail(string email, string& error) {
		error = "";
		if (email.find("@") == string::npos) {
			error = "Missing @ symbol from address.";
			return true;
		}
		if (email.find(".") == string::npos) {
			error = "Missing . symbol from address.";
			return true;
		}
		return false;
	}

	bool user::invalidPassword(string password, string& error) {
		error = "";
		if (password.size() < 6) {
			error = "Password needs to be larger than 6.";
			return true;
		}
		bool symbol = false;
		bool number = false;
		for (auto it = password.begin(); it != password.end();
				 ++it) {
			if (isdigit(*it))
				number = true;
			else if (!isalnum(*it))
				symbol = true;
			if (number && symbol) break;
		}
		if (!(number && symbol)) {
			if (!number && !symbol)
				error = "Password missing number & symbol.";
			else if (!number)
				error = "Password missing number.";
			else if (!symbol)
				error = "Password missing symbol.";
			return true;
		}
		return false;
	}

	bool user::invalidFirstName(string fName, string& error) {
		error = "";
		auto s = fName.size();
		if (s == 0) {
			error = "First name is empty.";
			return true;
		} else if (s > 255) {
			error = "First name is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidMidName(string mName, string& error) {
		error = "";
		auto s = mName.size();
		if (s > 255) {
			error = "Middle name is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidLastName(string lName, string& error) {
		error = "";
		auto s = lName.size();
		if (s == 0) {
			error = "Last name is empty.";
			return true;
		} else if (s > 255) {
			error = "Last name is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidCompany(string com, string& error) {
		error = "";
		auto s = com.size();
		if (s > 255) {
			error = "Company is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidPhone(string b, string& error) {
		error = "";
		std::regex e("\\+[0-9]{8,}");
		if (!regex_match(b, e)) {
			error = "Invalid phone number.";
			return true;
		}
		return false;
	}

	bool user::invalidGender(string g, string& error) {
		error = "";
		if (g.size() == 0) return false;
		auto fc = tolower(g[0]);
		if (!(fc == 'n' || fc == 'f' || fc == 'm')) {
			error =
				"Expected gender; non-binary, female, male, or "
				"blank";
			return true;
		}
		return false;
	}

	bool user::invalidBirthday(string b, string& error) {
		error = "";
		auto s = b.size();
		if (s == 0) {
			error = "Birthday is empty.";
			return true;
		} else if (s > 16) {
			error = "Birthday is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidAddress1(string a1, string& error) {
		error = "";
		auto s = a1.size();
		if (s == 0) {
			error = "Address 1 is empty.";
			return true;
		} else if (s > 255) {
			error = "Address 1 is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidAddress2(string a2, string& error) {
		error = "";
		auto s = a2.size();
		if (s > 255) {
			error = "Address 2 is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidCountry(string c, string& error) {
		error = "";
		auto s = c.size();
		if (s == 0) {
			error = "Country is empty.";
			return true;
		} else if (s > 128) {
			error = "Country is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidCity(string c, string& error) {
		error = "";
		auto s = c.size();
		if (s == 0) {
			error = "City is empty.";
			return true;
		} else if (s > 128) {
			error = "City is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidZone(string z, string& error) {
		error = "";
		auto s = z.size();
		if (s == 0) {
			error = "State/Prov/Zone is empty.";
			return true;
		} else if (s > 128) {
			error = "State/Prov/Zone is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidZip(string z, string& error) {
		error = "";
		auto s = z.size();
		if (s == 0) {
			error = "ZIP is empty.";
			return true;
		} else if (s > 32) {
			error = "ZIP is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidIcon(string id, string& error) {
		error = "";
		auto s = id.size();
		if (s > 32) {
			error = "Icon id is too big.";
			return true;
		}
		return false;
	}

	bool user::invalidUserType(string t, string& error) {
		error = "";
		if (t == "patron") return false;
		if (t == "manager") return false;
		if (t == "all") return false;
		error = "Expected userType; patron, manager, or all.";
		return true;
	}

	gold::var user::findOne(list args) {
		auto col =
			getPrototype().getObject<collection>("collection");
		auto value = col.findOne(args);
		return collection::setParentModel(
			value, user::getPrototype());
	}

	gold::var user::findMany(list args) {
		auto col =
			getPrototype().getObject<collection>("collection");
		auto value = col.findMany(args);
		return collection::setParentModel(
			value, user::getPrototype());
	}

}  // namespace gg