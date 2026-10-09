// The MCP tools: the universal core (gold data paths, method calls,
// gold::lang eval) plus the module-surface tools (entities, UI, window
// input, screenshots) as they land. Everything a tool touches is gold
// data reachable from the context's globals (the roots the host
// attached), so the surface grows with the app rather than with this
// file.

#include "mcpTools.hpp"

#include "goldjs.hpp"
#include "goldjson.hpp"

#if defined(GOLD_MCP_LANG)
#include "lang/lang.hpp"
#endif

namespace gold {
	using namespace std;

	namespace {

		// ------------------------------------------------------------------
		// paths
		// ------------------------------------------------------------------

		/** "engine.entities.2.name" -> {engine, entities, 2, name}. */
		vector<string> pathSegments(const string& path) {
			vector<string> out;
			size_t start = 0;
			while (start <= path.size()) {
				size_t cut = path.find('.', start);
				if (cut == string::npos) {
					out.push_back(path.substr(start));
					break;
				}
				out.push_back(path.substr(start, cut - start));
				start = cut + 1;
			}
			return out;
		}

		/** The final reference along a path: the parent container plus
		 *  the last segment (an object key or a list index). Reads and
		 *  writes go through the container's gold data (objects/lists
		 *  are handles, so a read here is the live value). */
		struct pathRef {
			bool ok{false};
			string error;
			var parent;          // the container the last segment lives in
			string key;
			uint64_t index{0};
			bool isIndex{false};
		};

		/** The context's root names, for error messages that make agent
		 *  recovery cheap. */
		string rootsHint(mcpServerContext& ctx) {
			string names;
			for (auto it = ctx.globals.begin(); it != ctx.globals.end();
					 ++it) {
				if (!names.empty()) names += ", ";
				names += "'" + it->first + "'";
			}
			return names.empty() ? string("(none attached)") : names;
		}

		/** The members of the container being descended, for the same
		 *  reason. */
		string membersHint(const var& container) {
			if (container.isList()) {
				auto li = container.getList();
				return "(a list, size " + to_string(li.size()) + ")";
			}
			if (container.isObject()) {
				string names;
				auto src = container.getObject();
				for (auto it = src.begin(); it != src.end(); ++it) {
					if (!names.empty()) names += ", ";
					names += "'" + it->first + "'";
				}
				return names.empty() ? string("(no members)") : names;
			}
			return "(a scalar)";
		}

		bool isNumeric(const string& s) {
			if (s.empty()) return false;
			for (char c : s)
				if (!isdigit((unsigned char)c)) return false;
			return true;
		}

		pathRef failRef(string error) {
			pathRef fail;
			fail.error = error;
			return fail;
		}

		pathRef resolvePath(mcpServerContext& ctx, const string& path) {
			auto segs = pathSegments(path);
			if (segs.empty() || segs[0].empty())
				return failRef(
					"empty path: a path starts with a root name (" +
					rootsHint(ctx) + ")");
			auto rootVar = ctx.globals.getVar(segs[0]);
			if (rootVar.getType() == typeNull)
				return failRef("no root '" + segs[0] + "'; roots: " +
					rootsHint(ctx));
			pathRef ref;
			ref.ok = true;
			if (segs.size() > 1) {
				var cur = rootVar;
				for (size_t i = 1; i < segs.size() - 1; ++i) {
					auto const& seg = segs[i];
					var next;
					if (cur.isList()) {
						if (!isNumeric(seg))
							return failRef("'" + seg +
								"' is not a list index; the value at '" +
								path + "' is a list there");
						auto li = cur.getList();
						auto idx = (uint64_t)atoll(seg.c_str());
						if (idx >= li.size())
							return failRef("list index " + seg +
								" is out of range (size " +
								to_string(li.size()) + ") at '" + path +
								"'");
						next = li.getVar(idx);
						if (next.getType() == typeNull)
							return failRef("the list element at '" + path +
								"' is missing or null");
					} else if (cur.isObject()) {
						auto o = cur.getObject();
						next = o.getVar(seg);
						if (next.getType() == typeNull)
							return failRef("no member '" + seg + "' at '" +
								path + "':" + membersHint(cur));
					} else
						return failRef("the value at '" + path +
							"' is a scalar and cannot be descended past '" +
							seg + "'");
					cur = next;
				}
				ref.parent = cur;
				auto const& last = segs[segs.size() - 1];
				if (ref.parent.isList()) {
					if (!isNumeric(last))
						return failRef("'" + last +
							"' is not a list index at '" + path + "'");
					ref.index = (uint64_t)atoll(last.c_str());
					ref.isIndex = true;
				} else if (ref.parent.isObject())
					ref.key = last;
				else
					return failRef("the value at '" + path +
						"' is a scalar and cannot carry a member '" +
						last + "'");
			} else {
				// The root itself: the container it lives in is the
				// globals object.
				ref.parent = ctx.globals;
				ref.key = segs[0];
			}
			return ref;
		}

		/** The value a pathRef points at; null when missing/out of
		 *  range. */
		var readRef(const pathRef& ref) {
			if (!ref.ok) return var();
			if (ref.isIndex) {
				auto li = ref.parent.getList();
				if (ref.index >= li.size()) return var();
				return li.getVar(ref.index);
			}
			return ref.parent.getObject().getVar(ref.key);
		}

		// ------------------------------------------------------------------
		// serialization safety
		// ------------------------------------------------------------------

		/** Deep-clone with a depth cap: containers past the cap collapse
		 *  to {size, "__truncated"}. Binaries/functions serialize as
		 *  null in JSON (goldjson's default), and non-finite numbers
		 *  throw at stringify — the tools catch that and report it. */
		var cloneCapped(const var& v, int64_t depth) {
			if (depth <= 0 && (v.isObject() || v.isList())) {
				auto size = v.isObject() ? v.getObject().size()
										 : v.getList().size();
				return jo("size", size, "__truncated", true);
			}
			if (v.isObject()) {
				auto src = v.getObject();
				object out;
				for (auto it = src.begin(); it != src.end(); ++it)
					out.setVar(it->first, cloneCapped(it->second, depth - 1));
				return out;
			}
			if (v.isList()) {
				auto src = v.getList();
				list out;
				for (auto it = src.begin(); it != src.end(); ++it)
					out.pushVar(cloneCapped(*it, depth - 1));
				return out;
			}
			return v;
		}

		// ------------------------------------------------------------------
		// the tools
		// ------------------------------------------------------------------

		/** state read shared body: path + optional depth (full subtree)
		 *  or the type/size listing (listOnly). */
		var stateReadImpl(object args, mcpServerContext& ctx,
			bool listOnly) {
			auto path = args["path"].getString();
			if (path.empty()) {
				// No path: the roots themselves.
				object out;
				for (auto it = ctx.globals.begin(); it != ctx.globals.end();
						 ++it) {
					auto v = it->second;
					auto entry = jo("type", getTypeString(v.getType()));
					if (v.isObject() || v.isList())
						entry.setInt64(
							"size", (int64_t)(v.isObject()
										 ? v.getObject().size()
										 : v.getList().size()));
					out.setObject(it->first, entry);
				}
				return out;
			}
			auto ref = resolvePath(ctx, path);
			if (!ref.ok) throw genericError("state read: " + ref.error);
			auto value = readRef(ref);
			if (value.getType() == typeNull)
				throw genericError("the value at '" + path +
					"' is missing or null");
			if (listOnly) {
				auto entry = jo("type", getTypeString(value.getType()));
				if (value.isObject()) {
					auto o = value.getObject();
					auto members = jo();
					for (auto it = o.begin(); it != o.end(); ++it)
						members.setString(it->first,
							getTypeString(it->second.getType()));
					entry.setObject("members", members);
				} else if (value.isList()) {
					auto li = value.getList();
					entry.setInt64("size", (int64_t)li.size());
					if (li.size() > 0)
						entry.setString("elementType",
							getTypeString(li[0].getType()));
				}
				return entry;
			}
			auto depth = args["depth"].getInt64();
			if (depth > 0) value = cloneCapped(value, depth);
			string json;
			try {
				json = jsonStringify(value);
			} catch (const exception& e) {
				throw genericError(
					"the value at '" + path +
					"' does not serialize to JSON: " + e.what());
			}
			if (json.size() > 4 * 1024 * 1024)
				throw genericError("the value at '" + path +
					"' serializes to " + to_string(json.size()) +
					" bytes; narrow the path or cap it with \"depth\"");
			return jo("path", path, "json", json);
		}

		var stateGetImpl(object args, mcpServerContext& ctx) {
			return stateReadImpl(args, ctx, false);
		}

		var stateListImpl(object args, mcpServerContext& ctx) {
			return stateReadImpl(args, ctx, true);
		}

		var stateSetImpl(object args, mcpServerContext& ctx) {
			auto path = args["path"].getString();
			if (args.getVar("value").getType() == typeNull)
				throw genericError(
					"state_set needs a \"value\" to write (nulls are not "
					"writable; use eval for null)");
			auto ref = resolvePath(ctx, path);
			if (!ref.ok) throw genericError("state_set: " + ref.error);
			auto now = readRef(ref);
			if (now.getType() == typeNull)
				throw genericError(
					"nothing to write at '" + path +
					"' (state_set only rewrites existing members; "
					"create new members with eval)");
			// A var copy, not the arguments' varRef: assigning a varRef
			// source through a varRef target silently does nothing in
			// gold's write proxy.
			auto value = args.getVar("value");
			if (ref.isIndex) {
				auto li = ref.parent.getList();
				li[ref.index] = value;
			} else {
				auto o = ref.parent.getObject();
				o[ref.key] = value;
			}
			return jo("path", path, "wrote", cloneCapped(value, 2));
		}

		var callImpl(object args, mcpServerContext& ctx) {
			auto path = args["path"].getString();
			auto methodName = args["method"].getString();
			auto ref = resolvePath(ctx, path);
			if (!ref.ok) throw genericError("call: " + ref.error);
			auto target = readRef(ref);
			auto targetObj = target.getObject();
			if (!targetObj)
				throw genericError("call: '" + path + "' is not an object (a " +
					string(getTypeString(target.getType())) + ")");
			auto fn = targetObj.getVar(methodName);
			if (!fn.isFunction())
				throw genericError("call: no method '" + methodName +
					"' on '" + path + "'");
			list argsList = args.getList("args", list());
			var result;
			try {
				result = targetObj.callMethod(methodName, argsList);
			} catch (const exception& e) {
				throw genericError("call: '" + path + "." + methodName +
					"' threw: " + e.what());
			}
			return jo("path", path, "method", methodName, "result", result);
		}

#if defined(GOLD_MCP_LANG)
		var evalImpl(object args, mcpServerContext& ctx) {
			auto source = args["source"].getString();
			auto result = langRun(source, ctx.globals, false);
			if (result.isError())
				throw genericError("eval: " + (string)*result.getError());
			// The globals object is gold data shared with the host (see
			// setRoots): assignments in scripts land in live app state.
			return result;
		}
#endif
	}  // namespace

	var mcpToolCatalog(mcpServerContext& ctx) {
		list out;
		for (auto it = mcpTools().begin(); it != mcpTools().end(); ++it) {
			if (it->name == "eval" && !ctx.evalEnabled) continue;
			out.pushObject(jo("name", it->name, "description",
				it->description, "inputSchema", it->inputSchema));
		}
		return out;
	}

	const std::vector<mcpTool>& mcpTools() {
		static vector<mcpTool> tools = {
			mcpTool{
				"state_get",
				"Read gold data at a path. Paths start at an attached root "
				"name and descend object members (\"engine.window.title\", "
				"\"engine.world.gravity\") and list indices "
				"(\"engine.entities.0.name\"). An empty path lists the "
				"roots. Binaries/functions read as null; very large values "
				"(full buffers) are refused with guidance.",
				jo("type", "object",
					"properties",
					jo("path",
						jo("type", "string",
							"description",
							"A gold data path, e.g. "
							"\"engine.entities.2.enabled\"; empty lists the "
							"roots."),
						"depth",
						jo("type", "integer",
							"description",
							"Optional depth cap for nested lists/objects.")),
					"required", ja("path")),
				[](object args, mcpServerContext& ctx) -> var {
					return stateGetImpl(args, ctx);
				},
			},
			mcpTool{
				"state_list",
				"List the members of a gold value: key names with their "
				"types for objects, size for lists — a cheap way to browse "
				"before state_get.",
				jo("type", "object",
					"properties",
					jo("path",
						jo("type", "string",
							"description",
							"The gold data path to list; empty lists the "
							"roots.")),
					"required", ja("path")),
				[](object args, mcpServerContext& ctx) -> var {
					return stateListImpl(args, ctx);
				},
			},
			mcpTool{
				"state_set",
				"Write gold data at an existing path "
				"(\"engine.entities.2.enabled\": false). Uniform number "
				"arrays land as gold vectors (\"value\": [1, 2, 3] -> "
				"vec3f), matching how the app reads them.",
				jo("type", "object",
					"properties",
					jo("path",
						jo("type", "string",
							"description", "The gold data path to write."),
						"value",
						jo("description", "The JSON value to write.")),
					"required", ja("path", "value")),
				[](object args, mcpServerContext& ctx) -> var {
					return stateSetImpl(args, ctx);
				},
			},
			mcpTool{
				"call",
				"Invoke a method on the gold object at a path "
				"(\"call engine.graphics setDebug\", physics raytrace, "
				"dataStore operations...). Runs on the engine frame when "
				"the engine pumps, so it mutates at a safe point between "
				"frames.",
				jo("type", "object",
					"properties",
					jo("path",
						jo("type", "string",
							"description",
							"The gold data path of the object whose method "
							"to call, e.g. \"engine.world\"."),
						"method",
						jo("type", "string", "description",
							"The method name."),
						"args",
						jo("type", "array",
							"description",
							"Positional arguments as a JSON array.")),
					"required", ja("path", "method")),
				[](object args, mcpServerContext& ctx) -> var {
					return callImpl(args, ctx);
				},
			},
#if defined(GOLD_MCP_LANG)
			mcpTool{
				"eval",
				"Run gold::lang (a TypeScript-like language) against the "
				"running app: globals are the attached roots (the same "
				"live objects), so scripts call methods, spawn entities, "
				"and read/write app state in place. Values assigned to "
				"root-object members persist; local lets do not. Disable "
				"with config \"eval\": false.",
				jo("type", "object",
					"properties",
					jo("source",
						jo("type", "string",
							"description",
							"The script source — expressions and statements "
							"both run; the last value is returned.")),
					"required", ja("source")),
				[](object args, mcpServerContext& ctx) -> var {
					return evalImpl(args, ctx);
				},
			},
#endif
		};
		return tools;
	}

}  // namespace gold