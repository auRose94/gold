#include "server.hpp"

#include <App.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

#include "file.hpp"
#include "html.hpp"

namespace gold {
	using namespace std;
	using namespace uWS;
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

	static auto mimeMap = map<string, string>({
		{".bin", "application/octet-stream"},
		{".zip", "application/zip"},
		{".rar", "application/x-rar-compressed"},
		{".json", "application/json"},
		{".bson", "application/bson"},
		{".js", "application/javascript"},
		{".xml", "application/xml"},
		{".gz", "application/gzip"},
		{".bz", "application/x-bzip"},
		{".bz2", "application/x-bzip2"},
		{".azw", "application/vnd.amazon.ebook"},
		{".doc", "application/msword"},
		{".ogx", "application/ogg"},
		{".pdf", "application/pdf"},
		{".tar", "application/x-tar"},
		{".xhtml", "application/xhtml+xml"},
		{".xls", "application/vnd.ms-excel"},
		{".7z", "application/x-7z-compressed"},
		{".abw", "application/x-abiword"},
		{".arc", "application/x-freearc"},
		{".html", "text/html"},
		{".csv", "text/csv"},
		{".css", "text/css"},
		{".rtf", "text/rtf"},
		{".txt", "text/plain"},
		{".ics", "text/calendar"},
		{".apng", "image/apng"},
		{".bmp", "image/bmp"},
		{".gif", "image/gif"},
		{".ico", "image/x-icon"},
		{".jpeg", "image/jpeg"},
		{".jpg", "image/jpeg"},
		{".jfif", "image/jpeg"},
		{".pjpeg", "image/jpeg"},
		{".pjp", "image/jpeg"},
		{".png", "image/png"},
		{".svg", "image/svg+xml"},
		{".tif", "image/tiff"},
		{".tiff", "image/tiff"},
		{".webp", "image/webp"},
		{".wav", "audio/wave"},
		{".aac", "audio/aac"},
		{".mp3", "audio/mpeg"},
		{".oga", "audio/ogg"},
		{".opus", "audio/opus"},
		{".weba", "audio/webm"},
		{".mid", "audio/midi"},
		{".midi", "audio/x-midi"},
		{".webm", "video/webm"},
		{".ogv", "video/ogg"},
		{".mpeg", "video/mpeg"},
		{".avi", "video/x-msvideo"},
		{".ttf", "font/ttf"},
		{".otf", "font/otf"},
		{".woff2", "font/woff2"},
		{".docx",
		 "application/"
		 "vnd.openxmlformats-officedocument.wordprocessingml."
		 "document"},
		{".ppt", "application/vnd.ms-powerpoint"},
		{".pptx",
		 "application/"
		 "vnd.openxmlformats-officedocument.presentationml."
		 "presentation"},
		{".xlsx",
		 "application/"
		 "vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
	});

	obj& server::getPrototype() {
		static auto proto = obj({
			{"host", "127.0.0.1"},
			{"port", 8080},
			{"cacheControl", "max-age=120"},
			{"start", method(&server::start)},
			{"get", method(&server::get)},
			{"post", method(&server::post)},
			{"put", method(&server::put)},
			{"patch", method(&server::patch)},
			{"del", method(&server::del)},
			{"options", method(&server::options)},
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
		});
		return proto;
	}

	var server::start(list) {
		auto host = getString("host");
		auto port = getInt32("port");
		auto handle = (App*)getPtr("handle");
		auto def = obj({});
		auto mounts = getObject("mounts");
		if (handle != nullptr) {
			auto handler = [&](auto* res, auto* req) {
				try {
					auto p = string(req->getUrl());
					auto f = mounts.getObject<file>(p);
					if (!f) {
						auto assetIndex = p.find("/assets/");
						if (assetIndex != string::npos) {
							p = p.substr(0, assetIndex) +
									p.substr(assetIndex + 8);
							f = mounts.getObject<file>(p);
						}
						if (!f) {
							auto indexIndex = p.find("/index.");
							if (indexIndex != string::npos) {
								p = p.substr(0, indexIndex);
								f = mounts.getObject<file>(p);
							}
						}
					}
					auto chash = req->getHeader("if-none-match");
					auto control = getString("cacheControl");
					if (f) {
						auto loaded = f.load();
						if (loaded.isView()) {
							auto hash = f.hash().getString();
							if (hash.compare(chash) == 0) {
								res->writeStatus("304 Not Changed");
								res->writeHeader("Cache-Control", control);
								res->end();
							} else {
								auto bin = loaded.getStringView();
								res->writeStatus(HTTP_200_OK);
								auto ext = fs::path(p).extension().string();
								auto ct = mimeMap[ext];
								res->writeHeader("Content-Type", ct);
								res->writeHeader("Cache-Control", control);
								res->writeHeader("ETag", hash);
								res->end(bin);
							}
						} else {
							res->writeStatus("404 Not Found");
							res->end();
						}
					} else {
						res->writeStatus("404 Not Found");
						res->end();
					}
				} catch (genericError& e) {
					cerr << e << endl;
				}
			};

			for (auto it = mounts.begin(); it != mounts.end(); ++it) {
				auto url = it->first;
				handle->get(url, handler);
			}

			handle
				->listen(
					port,
					[port, host](auto* token) {
						if (token) {
							std::cout << "Serving " << host << " over "
												<< port << std::endl;
						}
					})
				.run();
			return var(true);
		}
		return var(false);
	}

	var server::get(list args) {
		auto handle = (App*)getPtr("handle");
		if (!handle) return genericError("server handle null");
		auto pattern = args[0].getString();
		auto func = args[1].getFunction();

		if (!func)
			return genericError("missing callback");
		else
			handle->get(pattern.c_str(), [=](auto* res, auto* req) {
				try {
					func({request(req), response(res)});
				} catch (genericError& e) {
					cerr << e << endl;
				}
			});
		return var();
	}

	var server::post(list args) {
		auto handle = (App*)getPtr("handle");
		if (!handle) return genericError("server handle null");
		auto pattern = args[0].getString();
		auto func = args[1].getFunction();
		if (!func)
			return genericError("missing callback");
		else
			handle->post(pattern.c_str(), [=](auto* res, auto* req) {
				try {
					func({request(req), response(res)});
				} catch (genericError& e) {
					cerr << e << endl;
				}
			});
		return var();
	}

	var server::put(list args) {
		auto handle = (App*)getPtr("handle");
		if (!handle) return genericError("server handle null");
		auto pattern = args[0].getString();
		auto func = args[1].getFunction();
		if (!func)
			return genericError("missing callback");
		else
			handle->put(pattern.c_str(), [=](auto* res, auto* req) {
				try {
					func({request(req), response(res)});
				} catch (genericError& e) {
					cerr << e << endl;
				}
			});
		return var();
	}

	var server::patch(list args) {
		auto handle = (App*)getPtr("handle");
		if (!handle) return genericError("server handle null");
		auto pattern = args[0].getString();
		auto func = args[1].getFunction();
		if (!func)
			return genericError("missing callback");
		else
			handle->patch(pattern.c_str(), [=](auto* res, auto* req) {
				try {
					func({request(req), response(res)});
				} catch (genericError& e) {
					cerr << e << endl;
				}
			});
		return var();
	}

	var server::del(list args) {
		auto handle = (App*)getPtr("handle");
		if (!handle) return genericError("server handle null");
		auto pattern = args[0].getString();
		auto func = args[1].getFunction();
		if (!func)
			return genericError("missing callback");
		else
			handle->del(pattern.c_str(), [=](auto* res, auto* req) {
				try {
					func({request(req), response(res)});
				} catch (genericError& e) {
					cerr << e << endl;
				}
			});
		return var();
	}

	var server::options(list args) {
		auto handle = (App*)getPtr("handle");
		if (!handle) return genericError("server handle null");
		auto pattern = args[0].getString();
		auto func = args[1].getFunction();
		if (!func)
			return genericError("missing callback");
		else
			handle->options(
				pattern.c_str(), [=](auto* res, auto* req) {
					try {
						func({request(req), response(res)});
					} catch (genericError& e) {
						cerr << e << endl;
					}
				});
		return var();
	}

	var server::setMountPoint(list args) {
		auto handle = (App*)getPtr("handle");
		if (!handle) return genericError("server handle null");
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
		auto handle = (App*)getPtr("handle");
		if (!handle) return genericError("server handle null");
		auto func = args[0].getFunction();

		if (!func)
			return genericError("missing callback");
		else
			handle->get("/*", [=](auto* res, auto* req) {
				try {
					func({request(req), response(res)});
				} catch (genericError& e) {
					cerr << e << endl;
				}
			});
		return var();
	}

	var server::initialize(list) {
		auto kFN = getString("keyFileName");
		auto cFN = getString("certFileName");
		auto p = getString("passphrase");
		auto dh = getString("dhParamsFileName");
		auto ca = getString("caFileName");
		auto sslPreferLowMemoryUsage =
			getBool("sslPreferLowMemoryUsage", false);
		auto settings = us_socket_context_options_t{
			.key_file_name = (kFN.size() > 0) ? kFN.c_str() : nullptr,
			.cert_file_name =
				(cFN.size() > 0) ? cFN.c_str() : nullptr,
			.passphrase = (p.size() > 0) ? p.c_str() : nullptr,
			.dh_params_file_name =
				(dh.size() > 0) ? dh.c_str() : nullptr,
			.ca_file_name = (ca.size() > 0) ? ca.c_str() : nullptr,
			.ssl_prefer_low_memory_usage = sslPreferLowMemoryUsage,
		};
		setPtr("handle", new App(settings));
		auto mounts = obj({});
		setObject("mounts", mounts);
		return var();
	}

	var server::destroy(list) {
		auto handle = (App*)getPtr("handle");
		if (handle) delete handle;
		return var();
	}

	server::server() : obj() {}

	server::server(obj config) : obj() {
		copy(config);
		setParent(getPrototype());
		initialize({});
	}

	response::response() : obj() {}

	response::response(HttpResponse<true>* res) : obj() {
		setParent(getPrototype());
		setPtr("handle", res);
		setBool("ssl", true);
		setObject("headers", obj({}));
	}

	response::response(HttpResponse<false>* res) : obj() {
		setParent(getPrototype());
		setPtr("handle", res);
		setBool("ssl", false);
		setObject("headers", obj({}));
	}

	var response::writeContinue(list) {
		auto ssl = getBool("ssl");
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			res->writeContinue();
		} else {
			auto res = (HttpResponse<false>*)getPtr("handle");
			res->writeContinue();
		}
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

	var response::end(list args) {
		auto bin = binary();
		if (args[0].isObject(HTML::iHTML::getPrototype())) {
			string htmlStr = string(args[0]);
			bin.insert(bin.end(), htmlStr.begin(), htmlStr.end());
			writeHeader({"Content-Type", "text/html"});
		} else if (args[0].isObject()) {
			bin = args[0].getObject().getJSONBin();
			writeHeader({"Content-Type", "application/json"});
		} else if (args[0].isList()) {
			bin = args[0].getList().getJSONBin();
			writeHeader({"Content-Type", "application/json"});
		} else if (args[0].isList()) {
			bin = args[0].getList().getJSONBin();
			writeHeader({"Content-Type", "application/json"});
		} else if (args[0].isView())
			bin = args[0].getBinary();

		auto headers = getObject("headers");
		auto code = getUInt16("code");
		auto status = httpReturnStatusMap[code];

		auto strV = string_view((char*)bin.data(), bin.size());
		auto ssl = getBool("ssl");
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			res->writeStatus(status);
			for (auto it = headers.begin(); it != headers.end();
					 ++it) {
				auto key = it->first;
				if (it->second.isString()) {
					auto strValue = it->second.getString();
					res->writeHeader(key, strValue);
				} else if (it->second.isNumber()) {
					auto uValue = it->second.getUInt32();
					res->writeHeader(key, uValue);
				}
			}
			res->end(strV);
		} else {
			auto res = (HttpResponse<false>*)getPtr("handle");
			res->writeStatus(status);
			for (auto it = headers.begin(); it != headers.end();
					 ++it) {
				auto key = it->first;
				if (it->second.isString()) {
					auto strValue = it->second.getString();
					res->writeHeader(key, strValue);
				} else if (it->second.isNumber()) {
					auto uValue = it->second.getUInt32();
					res->writeHeader(key, uValue);
				}
			}
			res->end(strV);
		}
		return var();
	}

	var response::tryEnd(list args) {
		auto bin = binary();
		if (args[0].isObject(HTML::iHTML::getPrototype())) {
			string htmlStr = string(args[0]);
			bin.insert(bin.end(), htmlStr.begin(), htmlStr.end());
			writeHeader({"Content-Type", "text/html"});
		} else if (args[0].isObject()) {
			bin = args[0].getObject().getJSONBin();
			writeHeader({"Content-Type", "application/json"});
		} else if (args[0].isList()) {
			bin = args[0].getList().getJSONBin();
			writeHeader({"Content-Type", "application/json"});
		} else if (args[0].isList()) {
			bin = args[0].getList().getJSONBin();
			writeHeader({"Content-Type", "application/json"});
		} else if (args[0].isView())
			bin = args[0].getBinary();

		auto headers = getObject("headers");
		auto code = getUInt16("code");
		auto status = httpReturnStatusMap[code];

		auto strV = string_view((char*)bin.data(), bin.size());
		auto size = args.size() >= 2 ? args[1].getInt32() : 0;
		auto ssl = getBool("ssl");
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			res->writeStatus(status);
			for (auto it = headers.begin(); it != headers.end();
					 ++it) {
				auto key = it->first;
				if (it->second.isString()) {
					auto strValue = it->second.getString();
					res->writeHeader(key, strValue);
				} else if (it->second.isNumber()) {
					auto uValue = it->second.getUInt32();
					res->writeHeader(key, uValue);
				}
			}
			auto p = res->tryEnd(strV, size);
			return var(gold::list({p.first, p.second}));
		}
		auto res = (HttpResponse<false>*)getPtr("handle");
		res->writeStatus(status);
		for (auto it = headers.begin(); it != headers.end(); ++it) {
			auto key = it->first;
			if (it->second.isString()) {
				auto strValue = it->second.getString();
				res->writeHeader(key, strValue);
			} else if (it->second.isNumber()) {
				auto uValue = it->second.getUInt32();
				res->writeHeader(key, uValue);
			}
		}
		auto p = res->tryEnd(strV, size);
		return var(gold::list({p.first, p.second}));
	}

	var response::write(list args) {
		auto bin = binary();
		if (args[0].isObject(HTML::iHTML::getPrototype())) {
			string htmlStr = string(args[0]);
			bin.insert(bin.end(), htmlStr.begin(), htmlStr.end());
			writeHeader({"Content-Type", "text/html"});
		} else if (args[0].isView())
			bin = args[0].getBinary();
		auto strV = string_view((char*)bin.data(), bin.size());

		auto ssl = getBool("ssl");
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			return res->write(strV);
		}
		auto res = (HttpResponse<false>*)getPtr("handle");
		return res->write(strV);
	}

	var response::getWriteOffset(list) {
		auto ssl = getBool("ssl");
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			return res->getWriteOffset();
		}
		auto res = (HttpResponse<false>*)getPtr("handle");
		return res->getWriteOffset();
	}

	var response::hasResponded(list) {
		auto ssl = getBool("ssl");
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			return res->hasResponded();
		}
		auto res = (HttpResponse<false>*)getPtr("handle");
		return res->hasResponded();
	}

	var response::cork(list args) {
		auto handler = args[0].getFunction();
		auto ssl = getBool("ssl");
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			res->cork([handler]() { handler({}); });
			return var();
		}
		auto res = (HttpResponse<false>*)getPtr("handle");
		res->cork([handler]() { handler({}); });
		return var();
	}

	var response::onWritable(list args) {
		auto handler = args[0].getFunction();
		auto ssl = getBool("ssl");
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			res->onWritable([=](int v) { return handler({v}); });
			return var();
		}
		auto res = (HttpResponse<false>*)getPtr("handle");
		res->onWritable([=](int v) { return handler({v}); });
		return var();
	}

	var response::onAborted(list args) {
		auto handler = args[0].getFunction();
		auto reqObj = args[1].getObject<request>();
		auto ssl = getBool("ssl");
		auto callback = [=, *this]() {
			handler({var(reqObj), var(*this)});
		};
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			res->onAborted(callback);
			return var();
		}
		auto res = (HttpResponse<false>*)getPtr("handle");
		res->onAborted(callback);
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
		auto ssl = getBool("ssl");
		auto c = make_shared<dataContext>(
			dataContext{handler, reqObj, *this, string()});
		auto buffer = make_shared<string>();
		auto callback = [=](string_view v, bool b) {
			(c->buffer) = (c->buffer) + string(v);
			if (b) c->handler({c->buffer, c->req, c->res});
		};
		if (ssl) {
			auto res = (HttpResponse<true>*)getPtr("handle");
			res->onData(callback);
			return var();
		}
		auto res = (HttpResponse<false>*)getPtr("handle");
		res->onData(callback);
		return var();
	}

	request::request() : obj() {}

	request::request(uWS::HttpRequest* req) : obj() {
		setParent(getPrototype());
		auto headers = obj({});
		auto params = list({});
		auto it = req->begin();
		for (; it != req->end(); ++it) {
			if (it.ptr) {
				auto k = string(it.ptr->key);
				auto v = string(it.ptr->value);
				auto exist = headers.getVar(k);
				if (exist.isString()) {
					auto arr = list({exist.getString(), v});
					headers.setList(k, arr);
				} else if (exist.isList()) {
					auto arr = exist.getList();
					arr.pushString(v);
				} else
					headers.setString(k, v);
			}
		}
		uint32_t i = 0;
		auto currentParm = req->getParameter(i);
		while (currentParm.data() && currentParm.size() > 0) {
			params.pushString(string(currentParm));
			currentParm = req->getParameter(++i);
		}
		setObject("headers", headers);
		setList("params", params);
		setString("query", string(req->getQuery()));
		setString("method", string(req->getMethod()));
		setString("path", string(req->getUrl()));
		setPtr("handle", req);
	}

	var request::getAllHeaders(list args) {
		return getObject("headers");
	}

	var request::getHeader(list args) {
		auto headers = getObject("headers");
		auto lch = args[0].getString();
		transform(lch.begin(), lch.end(), lch.begin(), [](auto c) {
			return std::tolower(c);
		});
		return headers[lch];
	}

	var request::getMethod(list args) {
		return getString("method");
	}

	var request::getParameter(list args) {
		auto in = args[0].getUInt32();
		auto params = getList("params");
		return params[in];
	}

	var request::getQuery(list args) {
		return getString("query");
	}

	var request::getUrl(list args) { return getString("path"); }

	var request::getYield(list args) {
		auto handle = (uWS::HttpRequest*)getPtr("handle");
		return handle->getYield();
	}

	var request::setYield(list args) {
		auto handle = (uWS::HttpRequest*)getPtr("handle");
		auto value = args[0].getBool();
		handle->setYield(value);
		return value;
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