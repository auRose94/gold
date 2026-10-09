#include "server.hpp"
#include "session.hpp"
#include "serverTransport.hpp"

#include <filesystem>
#include <iostream>

#include "file.hpp"
#include "html.hpp"

namespace gold {
	using namespace std;
	namespace fs = std::filesystem;

	static auto httpReturnStatusMap = map<uint16_t, string>({
		{100, "100 Continue"},
		{101, "101 Switching Protocols"},
		{102, "102 Processing"},
		{103, "103 Early Hints"},

		{200, "200 OK"},
		{201, "201 Created"},
		{202, "202 Accepted"},
		{203, "203 Non-Authoritative Information"},
		{204, "204 No Content"},
		{205, "205 Reset Content"},
		{206, "206 Partial Content"},
		{207, "207 Multi-Status"},
		{208, "208 Already Reported"},
		{226, "226 IM Used"},

		{300, "300 Multiple Choices"},
		{301, "301 Moved Permanently"},
		{302, "302 Found"},
		{303, "303 See Other"},
		{304, "304 Not Modified"},
		{305, "305 Use Proxy"},
		{306, "306 Switch Proxy"},
		{307, "307 Temporary Redirect"},
		{308, "308 Permanent Redirect"},

		{400, "400 Bad Request"},
		{401, "401 Unauthorized"},
		{402, "402 Payment Required"},
		{403, "403 Forbidden"},
		{404, "404 Not Found"},
		{405, "405 Method Not Allowed"},
		{406, "406 Not Acceptable"},
		{407, "407 Proxy Authentication Required"},
		{408, "408 Request Timeout"},
		{409, "409 Conflict"},
		{410, "410 Gone"},
		{411, "411 Length Required"},
		{412, "412 Precondition Failed"},
		{413, "413 Payload Too Large"},
		{414, "414 URI Too Long"},
		{415, "415 Unsupported Media Type"},
		{416, "416 Range Not Satisfiable"},
		{417, "417 Expectation Failed"},
		{418, "418 I'm a teapot"},
		{421, "421 Misdirected Request"},
		{422, "422 Unprocessable Entity"},
		{423, "423 Locked"},
		{424, "424 Failed Dependency"},
		{425, "425 Too Early"},
		{426, "426 Upgrade Required"},
		{428, "428 Precondition Required"},
		{429, "429 Too Many Requests"},
		{431, "431 Request Header Fields Too Large"},
		{451, "451 Unavailable For Legal Reasons"},

		{500, "500 Internal Server Error"},
		{501, "501 Not Implemented"},
		{502, "502 Bad Gateway"},
		{503, "503 Service Unavailable"},
		{504, "504 Gateway Timeout"},
		{505, "505 HTTP Version Not Supported"},
		{506, "506 Variant Also Negotiates"},
		{507, "507 Insufficient Storage"},
		{508, "508 Loop Detected"},
		{510, "510 Not Extended"},
		{511, "511 Network Authentication Required"},

	});

	/** The status line for a code ("200 OK"), for transports. */
	string httpStatusLine(uint16_t code) {
		auto it = httpReturnStatusMap.find(code);
		return it != httpReturnStatusMap.end() ? it->second : string("200 OK");
	}

	object& server::getPrototype() {
		static auto proto = obj({
			{"host", "127.0.0.1"},
			{"port", 8080},
			{"cacheControl", "max-age=120"},
			{"transport", "lws"},
			{"start", method(&server::start)},
			{"stop", method(&server::stop)},
			{"get", method(&server::get)},
			{"post", method(&server::post)},
			{"put", method(&server::put)},
			{"patch", method(&server::patch)},
			{"del", method(&server::del)},
			{"options", method(&server::options)},
			{"ws", method(&server::ws)},
			{"setMountPoint", method(&server::setMountPoint)},
			{"setErrorHandler", method(&server::setErrorHandler)},
			{"initialize", method(&server::initialize)},
			{"destroy", method(&server::destroy)},
		});
		return proto;
	}

	obj& response::getPrototype() {
		static auto proto = obj({
			{"code", 200},
			{"writeContinue", method(&response::writeContinue)},
			{"writeStatus", method(&response::writeStatus)},
			{"writeHeader", method(&response::writeHeader)},
			{"end", method(&response::end)},
			{"tryEnd", method(&response::tryEnd)},
			{"write", method(&response::write)},
			{"getWriteOffset", method(&response::getWriteOffset)},
			{"hasResponded", method(&response::hasResponded)},
			{"cork", method(&response::cork)},
			{"onWritable", method(&response::onWritable)},
			{"onAborted", method(&response::onAborted)},
			{"onData", method(&response::onData)},
		});
		return proto;
	}

	obj& request::getPrototype() {
		static auto proto = obj({
			{"getAllHeaders", method(&request::getAllHeaders)},
			{"getHeader", method(&request::getHeader)},
			{"getMethod", method(&request::getMethod)},
			{"getParameter", method(&request::getParameter)},
			{"getQuery", method(&request::getQuery)},
			{"getUrl", method(&request::getUrl)},
			// Yield control so other matching routes keep running.
			{"setYield", method(&request::setYield)},
			{"getYield", method(&request::getYield)},
		});
		return proto;
	}

	obj& wsSocket::getPrototype() {
		static auto proto = obj({
			{"send", method(&wsSocket::send)},
			{"close", method(&wsSocket::close)},
		});
		return proto;
	}

	// ------------------------------------------------------------- server

	var server::start(list) {
		// Not initialized: the pre-transport contract returned false
		// without entering a loop — keep it.
		if (!getBool("initialized")) return var(false);
		// The transport (the loadable libgoldLws plugin — libwebsockets)
		// consumes the buffered routes + config and blocks on its own
		// loop. A failed bind reports an error instead of looping.
		auto names = list();
		const auto transport = getString("transport", "lws");
		if (!transport.empty()) names.pushString(transport);
		auto resolved = createServerTransport(names);
		if (!resolved)
			return genericError(
				"No server transport available (" + transport + ")");
		// Keep the instance reachable for stop() from another thread;
		// dropped again before the delete below (and by destroy()).
		setPtr("handle", (void*)resolved);
		auto run = resolved->run(*this);
		setPtr("handle", nullptr);
		delete resolved;
		return run;
	}

	var server::stop(list) {
		// A running transport answers a stop REQUEST (its loop exits and
		// start() returns on its own thread); this never touches its
		// state after that, and never frees it.
		auto running = (serverTransport*)getPtr("handle");
		if (!running) return var(false);
		running->stop();
		return var(true);
	}

	/** Buffer a WebSocket route: "ws" rides in the routes object like a
	 *  verb, but maps pattern -> {open, message, close} handlers object. */
	var server::ws(list args) {
		auto pattern = args[0].getString();
		auto handlers = args[1].getObject();
		if (!handlers) return genericError("missing handlers object");
		auto routes = getObject("routes");
		if (!routes) {
			routes = obj({});
			setObject("routes", routes);
		}
		auto wsRoutes = routes.getObject("ws");
		if (!wsRoutes) {
			wsRoutes = obj({});
			routes.setObject("ws", wsRoutes);
		}
		wsRoutes.setObject(pattern, handlers);
		return var();
	}

	namespace {
		/** Buffer one (pattern, func) onto the verb's route list. */
		void bufferRoute(object& self, const string& verb,
			const string& pattern, func handler) {
			auto routes = self.getObject("routes");
			if (!routes) {
				routes = obj({});
				self.setObject("routes", routes);
			}
			auto byVerb = routes.getObject(verb);
			if (!byVerb) {
				byVerb = obj({});
				routes.setObject(verb, byVerb);
			}
			byVerb.setFunc(pattern, handler);
		}
	}  // namespace

	var server::get(list args) {
		auto pattern = args[0].getString();
		auto handler = args[1].getFunction();
		if (!handler) return genericError("missing callback");
		bufferRoute(*this, "get", pattern, handler);
		return var();
	}

	var server::post(list args) {
		auto pattern = args[0].getString();
		auto handler = args[1].getFunction();
		if (!handler) return genericError("missing callback");
		bufferRoute(*this, "post", pattern, handler);
		return var();
	}

	var server::put(list args) {
		auto pattern = args[0].getString();
		auto handler = args[1].getFunction();
		if (!handler) return genericError("missing callback");
		bufferRoute(*this, "put", pattern, handler);
		return var();
	}

	var server::patch(list args) {
		auto pattern = args[0].getString();
		auto handler = args[1].getFunction();
		if (!handler) return genericError("missing callback");
		bufferRoute(*this, "patch", pattern, handler);
		return var();
	}

	var server::del(list args) {
		auto pattern = args[0].getString();
		auto handler = args[1].getFunction();
		if (!handler) return genericError("missing callback");
		bufferRoute(*this, "del", pattern, handler);
		return var();
	}

	var server::options(list args) {
		auto pattern = args[0].getString();
		auto handler = args[1].getFunction();
		if (!handler) return genericError("missing callback");
		bufferRoute(*this, "options", pattern, handler);
		return var();
	}

	var server::setMountPoint(list args) {
		auto mounts = getObject("mounts");
		for (auto it = args.begin(); it != args.end(); ++it) {
			try {
				// Mount points may be writable data directories (e.g.
				// uploads) that do not exist yet; create them so
				// canonical()/recursiveReadDirectory succeed.
				fs::create_directories(it->getString());
				auto url = fs::canonical(it->getString());
				file::recursiveReadDirectory(url, mounts);
			} catch (fs::filesystem_error e) {
				auto msg = string("Unable to find folder (") +
									 string(e.what()) + string(")");
				return genericError(msg);
			}
		}
		return var();
	}

	var server::setErrorHandler(list args) {
		auto handler = args[0].getFunction();
		if (!handler) return genericError("missing callback");
		setFunc("errorHandler", handler);
		return var();
	}

	var server::initialize(list) {
		auto mounts = obj({});
		setObject("mounts", mounts);
		auto routes = obj({});
		setObject("routes", routes);
		setBool("initialized", true);
		return var();
	}

	var server::destroy(list) {
		// Transports own their own handles and free them when their run
		// ends; the server's pointer fields are just data and safe to
		// clear twice.
		erase("handle");
		setBool("initialized", false);
		return var();
	}

	server::server() : obj() {}

	server::server(obj config) : obj() {
		copy(config);
		setParent(getPrototype());
		initialize({});
	}

	// ----------------------------------------------------------- response

	response::response() : obj() {}

	response::response(session* s) : obj() {
		setParent(getPrototype());
		setPtr("session", s);
		setObject("headers", obj({}));
	}

	var response::writeContinue(list) {
		if (auto s = (session*)getPtr("session")) s->writeContinue();
		return var();
	}

	var response::writeStatus(list args) {
		auto code = args[0].getUInt16();
		setUInt16("code", code);
		return var();
	}

	var response::writeHeader(list args) {
		auto key = args[0].getString();
		auto value = args[1];
		auto headers = getObject("headers");
		headers.setVar(key, value);
		return var();
	}

	/** Serialize a staged body arg into bytes + content type. */
	static binary stagedBody(const var& arg, response& self) {
		auto bin = binary();
		if (arg.isObject(HTML::iHTML::getPrototype())) {
			string htmlStr = string(arg);
			bin.insert(bin.end(), htmlStr.begin(), htmlStr.end());
			self.writeHeader({"Content-Type", "text/html"});
		} else if (arg.isObject()) {
			bin = arg.getObject().getJSONBin();
			self.writeHeader({"Content-Type", "application/json"});
		} else if (arg.isList()) {
			bin = arg.getList().getJSONBin();
			self.writeHeader({"Content-Type", "application/json"});
		} else if (arg.isView())
			bin = arg.getBinary();
		return bin;
	}

	var response::end(list args) {
		auto s = (session*)getPtr("session");
		if (!s) return genericError("response has no connection");
		// Ending twice in one handler trips some transports' asserts; keep
		// the first end.
		if (hasResponded(list()).getBool()) return var();
		auto bin = binary();
		if (args.size() > 0) bin = stagedBody(args[0], *this);

		auto headers = getObject("headers");
		const auto status = httpStatusLine(getUInt16("code"));
		s->writeStatusRaw(status);
		for (auto it = headers.begin(); it != headers.end(); ++it) {
			auto key = it->first;
			s->writeHeaderRaw(key, string(it->second));
		}
		s->end(string(string_view((char*)bin.data(), bin.size())));
		return var();
	}

	var response::tryEnd(list args) {
		auto s = (session*)getPtr("session");
		if (!s) return genericError("response has no connection");
		auto bin = binary();
		if (args.size() > 0) bin = stagedBody(args[0], *this);
		auto headers = getObject("headers");
		const auto status = httpStatusLine(getUInt16("code"));
		s->writeStatusRaw(status);
		for (auto it = headers.begin(); it != headers.end(); ++it)
			s->writeHeaderRaw(it->first, string(it->second));
	const size_t total =
		args.size() >= 2 ? (size_t)args[1].getInt32() : 0;
	auto p = s->tryEnd(
		string(string_view((char*)bin.data(), bin.size())), total);
	return var(p);
	}

	var response::write(list args) {
		auto s = (session*)getPtr("session");
		if (!s) return genericError("response has no connection");
		auto bin = binary();
		if (args.size() > 0 && args[0].isObject(HTML::iHTML::getPrototype())) {
			string htmlStr = string(args[0]);
			bin.insert(bin.end(), htmlStr.begin(), htmlStr.end());
			writeHeader({"Content-Type", "text/html"});
		} else if (args.size() > 0 && args[0].isView())
			bin = args[0].getBinary();
		return var((int64_t)s->write(
			string(string_view((char*)bin.data(), bin.size()))));
	}

	var response::getWriteOffset(list) {
		if (auto s = (session*)getPtr("session"))
			return var((int64_t)s->writeOffset());
		return var(0);
	}

	var response::hasResponded(list) {
		if (auto s = (session*)getPtr("session"))
			return s->hasResponded();
		return var(false);
	}

	var response::cork(list args) {
		auto handler = args[0].getFunction();
		if (auto s = (session*)getPtr("session")) s->cork(handler);
		return var();
	}

	var response::onWritable(list args) {
		auto handler = args[0].getFunction();
		if (auto s = (session*)getPtr("session")) s->onWritable(handler);
		return var();
	}

	var response::onAborted(list args) {
		auto handler = args[0].getFunction();
		auto reqObj = args[1].getObject<request>();
		if (auto s = (session*)getPtr("session")) {
			// The connection is dying: keep the gold values by value.
			auto callback = [=, *this](list) -> var {
				handler({var(reqObj), var(*this)});
				return var();
			};
			s->onAborted(callback);
		}
		return var();
	}

	var response::onData(list args) {
		struct dataContext {
			func handler;
			request req;
			response res;
			string buffer;
		};
		auto handler = args[0].getFunction();
		auto reqObj = args[1].getObject<request>();
		auto c = make_shared<dataContext>(
			dataContext{handler, reqObj, *this, string()});
		auto s = (session*)getPtr("session");
		if (!s) return var();
		s->onDataRaw([=](list raw) -> var {
			c->buffer = c->buffer + string(raw[0].getString());
			if (raw[1].getBool())
				c->handler({c->buffer, c->req, c->res});
			return var();
		});
		return var();
	}

	// ------------------------------------------------------------ request

	request::request() : obj() {}

	request::request(session* s) : obj() {
		setParent(getPrototype());
		setPtr("session", s);
		setObject("headers", obj({}));
		setList("params", list({}));
		setString("query", "");
		setString("method", "");
		setString("path", "");
	}

	var request::getAllHeaders(list args) {
		(void)args;
		return getObject("headers");
	}

	var request::getHeader(list args) {
		auto headers = getObject("headers");
		auto lch = args[0].getString();
		transform(lch.begin(), lch.end(), lch.begin(), [](auto c) {
			return std::tolower(static_cast<unsigned char>(c));
		});
		return headers[lch];
	}

	var request::getMethod(list args) {
		(void)args;
		return getString("method");
	}

	var request::getParameter(list args) {
		auto in = args[0].getUInt32();
		auto params = getList("params");
		return params[in];
	}

	var request::getQuery(list args) {
		(void)args;
		return getString("query");
	}

	var request::getUrl(list) { return getString("path"); }

	var request::getYield(list args) {
		(void)args;
		if (auto s = (session*)getPtr("session")) return s->yielded();
		return var(false);
	}

	var request::setYield(list args) {
		auto value = args[0].getBool();
		if (auto s = (session*)getPtr("session")) s->setYield(value);
		return value;
	}

	// ----------------------------------------------------------- wsSocket

	wsSocket::wsSocket() : obj() {}

	wsSocket::wsSocket(wsConn* c, const string& path) : obj() {
		setParent(getPrototype());
		setPtr("conn", (void*)c);
		setString("path", path);
	}

	var wsSocket::send(list args) {
		auto conn = (wsConn*)getPtr("conn");
		if (!conn) return genericError("socket is not connected");
		auto value = args[0];
		// Text for anything stringish; binary frames only for real
		// byte payloads (gold stores strings as byte views too, so a
		// bare isView() would misroute every message).
		if (value.isString() || value.getType() == typeStringView)
			conn->sendRaw(value.getString(), false);
		else if (value.isView()) {
			auto bin = value.getBinary();
			conn->sendRaw(
				string(string_view((char*)bin.data(), bin.size())), true);
		} else
			conn->sendRaw((string)value, false);
		return var();
	}

	var wsSocket::close(list) {
		auto conn = (wsConn*)getPtr("conn");
		if (conn) conn->closeConn();
		return var();
	}

	bool request::isWWWFormURLEncoded() {
		auto type = "application/x-www-form-urlencoded";
		auto contentType = getHeader({"Content-Type"}).getString();
		return contentType.find(type) != string::npos;
	}

	bool request::isJSON() {
		auto type0 = "application/json";
		auto type1 = "text/json";
		auto contentType = getHeader({"Content-Type"}).getString();
		return contentType.find(type0) != string::npos ||
					 contentType.find(type1) != string::npos;
	}

	bool request::acceptingJSON() {
		auto type0 = "application/json";
		auto type1 = "text/json";
		auto contentType = getHeader({"accept"}).getString();
		return contentType.find(type0) != string::npos ||
					 contentType.find(type1) != string::npos;
	}
	bool request::acceptingHTML() {
		auto type0 = "text/html";
		auto type1 = "application/xhtml+xml";
		auto contentType = getHeader({"accept"}).getString();
		return contentType.find(type0) != string::npos ||
					 contentType.find(type1) != string::npos;
	}

}  // namespace gold