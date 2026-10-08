#pragma once

#include <database.hpp>
#include <server.hpp>

namespace gg {
	using string = std::string;
	using list = gold::list;
	using var = gold::var;
	using namespace gold;
	// Forward gg's session first: a class declared in this namespace
	// shadows the using-directive's gold::web session in every later
	// use (gold's session is a transport interface, not this model).
	struct session;

	struct user : public model {
	 public:
		static std::map<string, string> nameMap;
		static object& getPrototype();
		user();
		user(object data);

		var checkPassword(list args);
		var changePassword(list args);
		var changeEmail(list args);
		var getIcon(list args);
		var addFriend(list args);
		var removeFriend(list args);
		var blockUser(list args);
		var deleteUser(list args);

		bool isAdmin();

		static void setRoutes(database, server);
		static var create(object data);
		static var login(
			string email, string password, string agent);

		// Utilities
		static string transformBirthday(string value);

		// Site rendering
		static var userCard(struct gg::session sesh, user u, user data);
		static var userMediaItem(
			struct gg::session sesh, user u, user data);
		static list userHome(struct gg::session sesh);
		static list userLogin(
			string response = "",
			string emailError = "",
			string passwordError = "");
		static list userRegister(obj data, object errs);
		static list userOptions(obj data, object errs);
		static list userDelete(session s, user u, list data);

		// Validation
		static bool invalidEmail(string email, string& error);
		static bool invalidPassword(string password, string& error);
		static bool invalidFirstName(string fName, string& error);
		static bool invalidMidName(string mName, string& error);
		static bool invalidLastName(string lName, string& error);
		static bool invalidCompany(string com, string& error);
		static bool invalidPhone(string b, string& error);
		static bool invalidGender(string b, string& error);
		static bool invalidBirthday(string b, string& error);
		static bool invalidAddress1(string a1, string& error);
		static bool invalidAddress2(string a2, string& error);
		static bool invalidCountry(string c, string& error);
		static bool invalidCity(string c, string& error);
		static bool invalidZone(string z, string& error);
		static bool invalidZip(string z, string& error);
		static bool invalidIcon(string id, string& error);
		static bool invalidUserType(string t, string& error);

		static var findOne(list args);
		static var findMany(list args);
	};

}  // namespace gg