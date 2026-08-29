#include "session.hpp"

#include <database.hpp>
#include <iostream>

#include "user.hpp"
#include "utility.hpp"
#include <goldjs.hpp>

namespace gg {
	using namespace std;

	collection session::col;
	const auto expireOffset = 86400000;

	gold::var session::getSession(list args) {
		auto req = args[0].getObject<request>();
		auto seshId = string();
		if (seshId.size() == 0) {
			auto query = object();
			parseURLEncoded(req.getQuery(), query);
			seshId = query.getString("s");
		}
		if (seshId.size() == 0) {
			auto cookie = req.getHeader({"Cookie"});
			auto query = object();
			parseCookie(cookie, query);
			seshId = query.getString("session");
		}
		auto agent = req.getHeader({"user-agent"}).getString();
		return session::findOne(
			{jo("_id", seshId, "agent", agent)});
	}

	gold::var session::writeSession(list args) {
		auto options = args.getObject(0);
		auto res = options.getObject<response>("res");
		auto redirect = options.getString("redirect");
		auto url = string(redirect);
		auto seshId = getString("_id");
		if (!getBool("useCookies")) {
			auto q = "?s=" + seshId;
			url += q;
		} else {
			auto maxAge =
				to_string((getUInt64("expire") - getMonoTime()) / 1000);
			auto domain = getString("domain");
			auto cookie = obj{
				{"session", seshId}, {"Domain", domain},
				{"Max-Age", maxAge}, {"SameSite", "Strict"},
				{"Path", "/"},
			};
			res.writeHeader(
				{"Set-Cookie", (cookie.getCookieString())});
		}
		if (redirect.size() != 0) {
			res.writeHeader({"Location", url});
			res.writeStatus({303});
		}
		return var();
	}

	var session::getQuery(list args) {
		auto q = string();
		if (!getBool("useCookies")) q = "?s=" + getString("_id");
		return q;
	}

	gold::var session::getSeshUser(list args) {
		auto req = args[0].getObject<request>();
		auto sesh =
			req.callMethod("getSession").getObject<session>();
		return sesh.getUser();
	}

	obj& session::getPrototype() {
		static auto proto = obj({
			{"user", ""},
			{"app", ""},
			{"expire", 0},
			{"agent", ""},
			{"useCookies", false},
			{"domain", ""},
			{"isExpired", method(&session::isExpired)},
			{"getUser", method(&session::getUser)},
			{"reset", method(&session::reset)},
			{"save", method(&session::save)},
			{"proto", model::getPrototype()},
		});
		auto getSession = func(session::getSession);
		auto getSeshUser = func(session::getSeshUser);
		auto requestProto = request::getPrototype();
		requestProto.setFunc("getSession", getSession);
		requestProto.setFunc("getUser", getSeshUser);
		return proto;
	}

	session::session() : model() {}

	session::session(user u) : model(col) {
		setParent(getPrototype());
		setString("user", u.getID());
		setUInt64("expire", getMonoTime() + expireOffset);
	}

	session::session(obj data) : model(col, data) {
		setParent(getPrototype());
	}

	var session::isExpired(list) {
		auto current = getMonoTime();
		auto expire = getUInt64("expire");
		return current > expire;
	}

	var session::getUser(list) {
		auto id = getString("user");
		return user::findOne({obj({
			{"_id", id},
		})});
	}

	var session::reset(list) {
		auto now = getMonoTime();
		auto expire = now + expireOffset;
		setUInt64("expire", expire);
		return expire;
	}

	var session::save(list args) {
		auto ret = model::save(args);
		if (ret.isObject()) {
			auto o = ret.getObject();
			copy(o);
		}
		auto indRes = col.addIndexes({
			"session",
			jo("expire", 1),
			jo("expireAfterSeconds", 0),
		});
		if (indRes.isError()) cerr << indRes << endl;
		return ret;
	}

	void session::setRoutes(database db, server) {
		auto colRes = db.getCollection({"session"});
		if (colRes.isError()) cerr << colRes << endl;
		col = colRes.getObject<collection>();
		getPrototype()["col"] = col;
	}

	gold::var session::findOne(list args) {
		auto value = session::col.findOne(args);
		return collection::setParentModel(
			value, session::getPrototype());
	}

}  // namespace gg