// The MCP protocol layer: JSON-RPC 2.0 envelopes, the initialize
// handshake, tools/list/tools/call — transport-free, so tests drive it
// with plain gold vars.

#include "mcp/mcpDispatcher.hpp"

#include "goldjs.hpp"
#include "goldjson.hpp"
#include "mcpTools.hpp"

namespace gold {
	using namespace std;

	namespace {

		var envelope(var id, var result) {
			return jo("jsonrpc", "2.0", "id", id, "result", result);
		}

		var textContent(string text) {
			return jo("type", "text", "text", text);
		}

		/** Tool results are gold data; the content shape is what the
		 *  protocol defines: plain values become a single text item of
		 *  JSON, tools that compose richer content (images) build their
		 *  own content item list starting with {"type": ...}. */
		list asContentList(var result) {
			if (result.isList() && result.getList().size() > 0) {
				auto first = result.getList()[0];
				if (first.isObject() &&
					first.getObject().getVar("type").getType() !=
						typeNull)
					return result.getList();
			}
			return ja(textContent(jsonStringify(result)));
		}

	}  // namespace

	var mcpErrorEnvelope(var id, int64_t code, string message) {
		return jo("jsonrpc", "2.0", "id", id,
			"error", jo("code", code, "message", message));
	}

	var mcpDispatch(var message, mcpServerContext& ctx) {
		if (!message.isObject())
			return mcpErrorEnvelope(
				var(), -32600, "requests are JSON-RPC 2.0 objects");

		auto req = message.getObject();
		auto jsonrpc = req.getString("jsonrpc");
		if (jsonrpc != "2.0")
			return mcpErrorEnvelope(var(), -32600,
				"requests are JSON-RPC 2.0 (jsonrpc: \"2.0\")");
		auto id = req.getVar("id");
		// No id (or an explicit null) is a notification: no response at
		// all. Unknown notification names are ignored.
		const bool notification = id.getType() == typeNull;
		auto method = req.getString("method");

		if (method == "initialize") {
			auto wanted = req.getObject("params").getString("protocolVersion");
			// Echo the client's version when we speak it; otherwise
			// the server's latest (the client decides — the protocol
			// allows this either way).
			auto version =
				wanted == "2025-03-26" || wanted == "2025-06-18"
					? wanted
					: string(mcpServerContext::protocolVersion);
			return envelope(id,
				jo("protocolVersion", version,
					"capabilities", jo("tools", jo("listChanged", false)),
					"serverInfo",
					jo("name", "gold", "version", "0.1.0"),
					"instructions",
					"Debug server for a running gold app. Browse app "
					"state with state_list/state_get (gold data paths), "
					"mutate with state_set, invoke methods with call, "
					"and run gold::lang with eval. For the module "
					"surfaces: entities_list, ui_query/ui_set/ui_event, "
					"screenshot, and window_input."));
		}

		if (method == "ping") return envelope(id, jo());

		if (method == "tools/list")
			return envelope(id, jo("tools", mcpToolCatalog(ctx)));

		if (method == "tools/call") {
			auto params = req.getObject("params");
			auto name = params.getString("name");
			const mcpTool* tool = nullptr;
			for (auto it = mcpTools().begin(); it != mcpTools().end();
					 ++it)
				if (it->name == name) tool = &(*it);
			if (!tool)
				return mcpErrorEnvelope(
					id, -32602, "unknown tool: " + name);
			if (name == "eval" && !ctx.evalEnabled)
				return mcpErrorEnvelope(id, -32602,
					"tool disabled (config \"eval\": false)");
			try {
				auto content = tool->impl(
					params.getObject("arguments", obj({})), ctx);
				return envelope(id,
					jo("content", asContentList(content)));
			} catch (const genericError& e) {
				return envelope(id,
					jo("content",
						ja(textContent("tool failed: " + (string)e)),
						"isError", true));
			} catch (const exception& e) {
				return envelope(id,
					jo("content",
						ja(textContent(string("tool failed: ") + e.what())),
						"isError", true));
			}
		}

		if (method.starts_with("notifications/")) return var();
		if (!notification)
			return mcpErrorEnvelope(
				id, -32601, "method not found: " + method);
		return var();
	}

}  // namespace gold