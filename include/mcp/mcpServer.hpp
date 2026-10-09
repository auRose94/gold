#pragma once

#include "types.hpp"

namespace gold {
	/**
	 * The MCP server facade — gold's embeddable Model Context Protocol
	 * development endpoint. It owns a web server facade over the
	 * "/mcp" JSON-RPC route and runs it on its own thread:
	 *
	 *   mcpServer devServer(jo("host", "127.0.0.1", "port", 8090));
	 *   devServer.setRoots(jo("engine", app));
	 *   devServer.start();        // listens; clients may arrive now
	 *   ...
	 *   devServer.pump();         // the engine loop calls this every frame
	 *   devServer.stop();         // from anywhere
	 *
	 * The engine only reaches this through the mcpServerSystem seam
	 * (include/game/mcpServerSystem.hpp) and loads the code on demand;
	 * apps and tests may link gold::mcp and use the facade directly.
	 */
	struct mcpServer : public object {
	 public:
		static object& getPrototype();

		mcpServer();
		mcpServer(initList config);
		/** Runtime-configured: the object form (jo/ja-built config). */
		mcpServer(object config);

		/** Apply the runtime config keys (host/port/transport/eval/
		 *  execute/timeout) and build the endpoint. Does not listen. */
		var initialize(list args = {});
		/** Start listening on the facade's own thread (non-blocking;
		 *  poll the port or check the route to know it came up). */
		var start(list args = {});
		/** Graceful stop + thread join; safe from any thread. */
		var stop(list args = {});
		/** Merge args[0] (name → live objects) into the roots/eval
		 *  globals. */
		var setRoots(list args);
		/** One more root: attach(name, object). */
		var attach(list args);
		/** Drain queued jobs on the calling thread (the engine calls
		 *  this every frame in "frame" execution mode). Cheap when the
		 *  queue is empty. */
		var pump(list args = {});
		var destroy(list args = {});
	};
}  // namespace gold