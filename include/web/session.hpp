#pragma once

#include "types.hpp"
#include <functional>
#include <string>

namespace gold {
	/**
	 * The per-connection transport state behind gold's request/response
	 * facades. Everything HTTP-shaped that is not gold data travels
	 * through this interface; the web module's facade code (status maps,
	 * JSON/HTML bodies, header staging, body buffering) is transport-
	 * independent and implemented once in server.cpp.
	 *
	 * A transport implementation (libwebsockets, loadable as the libgoldLws
	 * plugin) creates a session per request, fills the gold object's
	 * data fields (headers/params/query/method/path), and hands both
	 * facades to the dispatch helpers.
	 */
	struct session {
		virtual ~session() = default;

		// -------------------------------------------------- request side
		virtual void setYield(bool value) = 0;
		virtual bool yielded() const = 0;

		// ------------------------------------------------- response side
		virtual bool writeContinue() = 0;
		virtual bool writeStatusRaw(const string& status) = 0;
		virtual bool writeHeaderRaw(const string& key,
			const string& value) = 0;
		virtual bool end(const string& body) = 0;
		/** [sentFully, sentBytes]. */
		virtual list tryEnd(const string& body, size_t totalSize) = 0;
		virtual size_t write(const string& partial) = 0;
		virtual size_t writeOffset() const = 0;
		virtual bool hasResponded() const = 0;
		/** Run the handler now on this connection's own context. */
		virtual void cork(func handler) = 0;
		virtual void onWritable(func handler) = 0;
		virtual void onAborted(func handler) = 0;
		/** Body-data arrival: handler(string raw, bool final). */
		virtual void onDataRaw(func handler) = 0;
	};

	/**
	 * The per-connection WebSocket state behind gold's wsSocket facade
	 * (server.ws(route, {open, message, close})). Sends stage and are
	 * flushed on the connection's own next writable moment; close
	 * requests a graceful close. Transports implement both interfaces in
	 * one per-connection object.
	 */
	struct wsConn {
		virtual ~wsConn() = default;

		/** Stage a message for the peer: text frames by default, one
		 *  binary frame when `binary` is set. */
		virtual bool sendRaw(const string& data, bool binary) = 0;
		/** Request a graceful close. */
		virtual void closeConn() = 0;
	};
}  // namespace gold