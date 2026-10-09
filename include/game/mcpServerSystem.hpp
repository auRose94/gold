#pragma once

#include "types.hpp"

namespace gold {
	/**
	 * The Model Context Protocol (MCP) server the engine can host: a
	 * client (an IDE, an AI agent, an MCP client) connects over a
	 * network transport and inspects/modifies the running app through
	 * gold data paths, method calls, and gold::lang eval.
	 *
	 * The implementation ships as the loadable libgoldMcp module (HTTP
	 * comes from the web module's serverTransport), so libgoldGame
	 * carries neither MCP nor networking code: the engine loads the
	 * module only when the app's config asks for it
	 * ("mcp": {"enabled", true}) — otherwise this seam costs nothing.
	 * Everything crossing here is gold data.
	 */
	class mcpServerSystem {
	 public:
		virtual ~mcpServerSystem() = default;

		virtual const char* name() const = 0;

		/** Configure and start listening: the engine's "mcp" config
		 *  section (host, port, transport, eval, execute, timeout).
		 *  Returns an error var when the endpoint cannot run (e.g. the
		 *  configured transport is unavailable); the engine keeps running
		 *  either way. */
		virtual var initialize(object config) = 0;

		/** Publish named live objects as the server's addressable roots
		 *  (the same names are the eval globals scripts see). Call before
		 *  clients arrive; root names stay stable while the server runs. */
		virtual void setRoots(object roots) = 0;

		/** Drain queued protocol jobs on the engine frame — the safe
		 *  point after the window pump and before physics/render of the
		 *  next frame. Cheap when the queue is empty. */
		virtual void pump() = 0;

		/** Request a graceful stop of the listener; safe from any
		 *  thread. */
		virtual var shutdown() = 0;
	};

	using createMcpServerFn = mcpServerSystem* (*)();
	void registerMcpServer(const std::string& name,
		createMcpServerFn factory);

	/**
	 * Create an MCP server by name; a miss tries plugin::load first
	 * (the mcp module self-registers), like the other registries.
	 */
	mcpServerSystem* createMcpServer(const std::string& name);
	/** Ordered fallback chain; the first name that produces a server
	 *  wins. */
	mcpServerSystem* createMcpServer(const list& names);
}  // namespace gold