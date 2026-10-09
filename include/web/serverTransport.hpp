#pragma once

#include "types.hpp"

namespace gold {
	/**
	 * A server transport owns a concrete HTTP engine. gold's server
	 * facade keeps the API (routes, mounts, error handler, config) as
	 * gold data; `run()` consumes it and blocks on the transport's own
	 * event loop. Transports register into a registry with the usual
	 * chain + plugin-load hook — the uWS stopgap ships as libgoldUws,
	 * the libwebsockets port as libgoldLws.
	 *
	 * Dispatch policy is shared: transports route a request to the gold
	 * handler func from the server's buffered route data, then hand the
	 * (request, response) pair to `dispatchRoute` in serverTransport.cpp
	 * for the 500-on-exception policy; unmatched verbs/paths are answered
	 * 404 by the transport.
	 */
	struct serverTransport {
		virtual ~serverTransport() = default;

		virtual const char* name() const = 0;

		/** Blocking. Reads routes/mounts/errorHandler/cacheControl/
		 *  host/port (+SSL settings) off `server`. Returns var(true)
		 *  after the loop yields, or an error. */
		virtual var run(object server) = 0;

		/** Request a graceful loop exit so a blocking run() returns.
		 *  Callable from any thread; never frees the transport. */
		virtual void stop() {}
	};

	using createServerTransportFn = serverTransport* (*)();
	void registerServerTransport(const std::string& name,
		createServerTransportFn factory);
	/** Ordered fallback chain with the plugin-load retry. */
	serverTransport* createServerTransport(const list& names);

	/** The shared dispatch policy: run a gold route handler, turning any
	 *  exception into a 500 reply on that connection. */
	var dispatchRoute(func handler, var req, var res);

	/** The mount/static-file dispatch shared by transports: resolve the
	 *  request path against the server's buffered mounts (gold file
	 *  objects) under the ETag/304/mime/cache-control policy, 404 when
	 *  miss. Transports route unmatched mounts here in their fallback
	 *  chain. */
	func makeMountHandler(object host);
}  // namespace gold