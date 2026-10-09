#pragma once

#include <string>
#include <vector>

#include "types.hpp"

namespace gold {
	/**
	 * The MCP protocol layer: JSON-RPC 2.0 messages over HTTP POST
	 * ("/mcp", the "streamable HTTP" flavor of Model Context Protocol —
	 * single JSON responses, no SSE), plus the tool catalog. This layer
	 * is transport-free and testable without a network.
	 *
	 * Sessions: the server does not mint Mcp-Session-Id values (the
	 * protocol leaves that to the server, and libwebsockets exposes
	 * arbitrary request headers only through its token table). Requests
	 * are treated as one implicit session; DELETE /mcp stays legal.
	 */
	struct mcpServerContext {
		/** The live gold objects addressable as paths ("engine.world…")
		 *  — the same names are the globals gold::lang eval scripts
		 *  see; root values are object handles, so scripts read/write
		 *  the same storage the host reads. */
		object globals;
		/** The "eval" config flag: without it the eval tool leaves the
		 *  catalog. */
		bool evalEnabled{true};

		/** The protocol versions this server negotiates. */
		static constexpr const char* protocolVersion = "2025-06-18";
	};

	/** Handle one parsed request/notification object. Returns the
	 *  response message var to serialize (a null var for notifications
	 *  — send nothing), applying the shared 500-on-exception policy to
	 *  the tools: a tool that fails yields {content, isError: true},
	 *  not a protocol error. */
	var mcpDispatch(var message, mcpServerContext& ctx);

	/** A JSON-RPC error envelope ("jsonrpc", "id",
	 *  {"code": code, "message": message}); both the dispatcher's
	 *  protocol errors and the endpoint's transport-level errors use
	 *  it. */
	var mcpErrorEnvelope(var id, int64_t code, string message);

	/** The "tools/list" catalog (filtered by the context's settings). */
	var mcpToolCatalog(mcpServerContext& ctx);
}  // namespace gold