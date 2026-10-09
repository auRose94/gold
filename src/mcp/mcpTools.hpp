#pragma once

// The MCP tool implementations (module-internal): each tool is a
// schema-carrying entry with an impl(arguments, ctx) -> content var.
// Tools throw genericError for tool-level failures — the dispatcher
// turns those into {content, isError: true} results, not protocol
// errors.

#include <functional>

#include "mcp/mcpDispatcher.hpp"

namespace gold {
	struct mcpTool {
		string name;
		string description;
		var inputSchema;
		std::function<var(object args, mcpServerContext& ctx)> impl;
	};

	/** The full tool registry (filtered by the context's settings when
	 *  the catalog is served). */
	const std::vector<mcpTool>& mcpTools();

	/** The catalog "tools/list" serves, as gold data, filtered by the
	 *  context's settings. */
	var mcpToolCatalog(mcpServerContext& ctx);
}  // namespace gold