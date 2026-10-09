// The MCP tools: the universal core (gold data paths, method calls,
// gold::lang eval) plus the module-surface tools (entities, UI, window
// input, screenshots) as they land. Everything a tool touches is gold
// data reachable from the context's globals (the roots the host
// attached), so the surface grows with the app rather than with this
// file.

#include "mcpTools.hpp"

#include <filesystem>

#include "component.hpp"
#include "entity.hpp"
#include "file.hpp"
#include "goldjs.hpp"
#include "goldjson.hpp"
#include "light.hpp"
#include "meshRenderer.hpp"
#include "physicsBody.hpp"
#include "shape.hpp"
#include "sprite.hpp"
#include "transform.hpp"
#include "uiSurface.hpp"

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

		// ------------------------------------------------------------------
		// module-surface tools
		// ------------------------------------------------------------------
		// These all assume the engine root the engine attaches itself
		// as ("engine") — the surfaces live on its members (window,
		// graphics, entities/components). Apps can always attach more
		// roots and address them through state tools; these exist to
		// keep common debug flows to a single round trip.

		/** The engine root, or a tool error naming the roots. */
		object engineRoot(mcpServerContext& ctx, const char* what) {
			auto engine = ctx.globals.getObject("engine");
			if (!engine)
				throw genericError(string(what) + ": needs the 'engine' root (attached by the engine itself; attached roots: " +
					rootsHint(ctx) + ")");
			return engine;
		}

		/** The requested uiSurface: an explicit gold "path" argument, or
		 *  the first uiSurface among the engine's components. */
		object findUiSurface(mcpServerContext& ctx, object args) {
			auto surfacePath = args.getString("path", string());
			if (!surfacePath.empty()) {
				auto ref = resolvePath(ctx, surfacePath);
				if (!ref.ok) throw genericError("ui: " + ref.error);
				auto surface = readRef(ref);
				auto s = surface.getObject();
				if (!s || !surface.isObject(uiSurface::getPrototype()))
					throw genericError("ui: '" + surfacePath +
						"' is not a uiSurface");
				return s;
			}
			auto engine = engineRoot(ctx, "ui");
			auto comps = engine.getList("components");
			for (auto it = comps.begin(); it != comps.end(); ++it)
				if (it->isObject(uiSurface::getPrototype())) return *it;
			throw genericError(
				"ui: no uiSurface in the engine components");
		}

		/** A component's prototype name (the best-effort known set;
		 *  app classes fall back to "component"). */
		string componentProto(const var& comp) {
			if (comp.isObject(meshRenderer::getPrototype()))
				return "meshRenderer";
			if (comp.isObject(transform::getPrototype()))
				return "transform";
			if (comp.isObject(light::getPrototype())) return "light";
			if (comp.isObject(sprite::getPrototype())) return "sprite";
			if (comp.isObject(uiSurface::getPrototype()))
				return "uiSurface";
			if (comp.isObject(shape::getPrototype())) return "shape";
			if (comp.isObject(physicsBody::getPrototype()))
				return "physicsBody";
			return "component";
		}

		var entitiesListImpl(object, mcpServerContext& ctx) {
			auto engine = engineRoot(ctx, "entities_list");
			list out;
			std::function<var(var, uint64_t)> walk =
				[&walk](var entVar, uint64_t index) -> var {
				auto ent = entVar.getObject();
				if (!ent)
					return jo("index", index, "error", "not an object");
				list comps;
				auto compList = ent.getList("components");
				for (auto it = compList.begin(); it != compList.end();
						 ++it)
					comps.pushObject(jo("index",
						(uint64_t)(it - compList.begin()), "proto",
						componentProto(*it), "enabled",
						it->getObject().getBool("enabled", true)));
				list children;
				auto kids = ent.getList("children");
				for (auto it = kids.begin(); it != kids.end(); ++it)
					children.pushVar(
						walk(*it, (uint64_t)(it - kids.begin())));
				return jo("index", index, "name",
					ent.getString("name", string()), "enabled",
					ent.getBool("enabled", true), "components", comps,
					"children", children);
			};
			auto entities = engine.getList("entities");
			for (auto it = entities.begin(); it != entities.end(); ++it)
				out.pushVar(walk(*it, (uint64_t)(it - entities.begin())));
			return jo("entities", out);
		}

		var uiQueryImpl(object args, mcpServerContext& ctx) {
			auto surface = findUiSurface(ctx, args);
			auto selector = args.getString("selector", string());
			if (selector.empty())
				// The whole document: markup + the frame counters.
				return jo("markup", surface.callMethod("markup"),
					"stats", surface.callMethod("stats"));
			return surface.callMethod("query", ja(selector));
		}

		var uiSetImpl(object args, mcpServerContext& ctx) {
			auto surface = findUiSurface(ctx, args);
			auto id = args.getFloat("id");
			list applied;
			if (args.getVar("text").getType() != typeNull) {
				surface.callMethod("setText", ja(id, args.getString("text")));
				applied.pushString("text");
			}
			if (args.getVar("style").getType() != typeNull) {
				surface.callMethod("setStyle",
					ja(id, args.getObject("style")));
				applied.pushString("style");
			}
			if (args.getVar("state").getType() != typeNull) {
				surface.callMethod("setState",
					ja(id, args.getString("state"),
						args.getBool("stateValue", true)));
				applied.pushString("state");
			}
			return jo("id", (int64_t)id, "applied", applied);
		}

		var uiEventImpl(object args, mcpServerContext& ctx) {
			auto surface = findUiSurface(ctx, args);
			auto type = args.getString("type", "click");
			// pointerAt is the surface's UI-space dispatch (the same
			// pipeline real input arrives through); x/y < 0 dispatches
			// to the current hover node.
			list eventArgs(ja(type,
				(double)args.getDouble("x", -1.0),
				(double)args.getDouble("y", -1.0)));
			if (args.getVar("data").getType() != typeNull)
				eventArgs.pushVar(args["data"]);
			auto hit = surface.callMethod("pointerAt", eventArgs);
			return jo("type", type, "x", (double)args.getDouble("x", -1.0),
				"y", (double)args.getDouble("y", -1.0), "hit", hit);
		}

		var screenshotImpl(object args, mcpServerContext& ctx) {
			auto engine = engineRoot(ctx, "screenshot");
			auto gfx = engine.getObject("graphics");
			if (!gfx)
				throw genericError("screenshot: the engine has no "
					"'graphics' member");
			auto path = args.getString("path", string("/tmp/gold-mcp.png"));
			// Screenshots are asynchronous (the backends read back
			// post-submit), so the flow is request → poll: if the PNG
			// from a previous call is on disk, serve it and remove it
			// so the next call captures fresh.
			if (std::filesystem::exists(path)) {
				auto loaded = file::readFile(path);
				binary png;
				if (loaded.isError())
					throw genericError(
						"screenshot: the file exists but is unreadable: " +
						(string)loaded);
				file image;
				loaded.assignObject(image);
				png = (binary)image;
				std::filesystem::remove(path.c_str());
				return ja(
					jo("type", "image", "mime", "image/png",
						"data", file::encodeBase64(png)),
					jo("type", "text",
						"text", tpl("png captured at $0 ($1 bytes)",
							path, (int64_t)png.size())));
			}
			auto requested = gfx.callMethod("screenshot", ja(path));
			if (requested.isError())
				throw genericError(
					"screenshot: " + (string)*requested.getError());
			return jo("status", "requested", "path", path,
				"hint",
				"the backend writes the PNG after the next frames; "
				"call again to receive it as image content (the file is "
				"served and removed)");
		}

		var windowInputImpl(object args, mcpServerContext& ctx) {
			auto engine = engineRoot(ctx, "window_input");
			auto win = engine.getObject("window");
			if (!win)
				throw genericError(
					"window_input: the engine has no 'window' member");
			auto type = args.getString("type");
			if (type.empty())
				throw genericError(
					"window_input: needs an event \"type\" (key_down, "
					"key_up, text_input, mouse_down, mouse_up, "
					"mouse_move, mouse_wheel...)");
			// The event dictionary the windowSystem backends emit —
			// identical downstream from a real device event.
			object ev;
			for (auto it = args.begin(); it != args.end(); ++it)
				ev.setVar(it->first, it->second);
			auto result = win.callMethod("handleEvent", ja(ev));
			return jo("dispatched", ev, "result", result);
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
			mcpTool{
				"entities_list",
				"A compact walk of the engine's entities: name, enabled, "
				"components (with the best-effort prototype name and "
				"index), and children recursively. The indices are the "
				"same \"engine.entities.N\"/\"engine.components.N\" "
				"paths the other tools address.",
				jo("type", "object", "properties", jo()),
				[](object args, mcpServerContext& ctx) -> var {
					return entitiesListImpl(args, ctx);
				},
			},
			mcpTool{
				"ui_query",
				"Query an HTML/CSS uiSurface through its renderer: a real "
				"CSS selector (\".hud .score\", \"#settings\") returns "
				"element descriptors (id, tag, class, text, rect); an "
				"empty selector returns the whole document as markup plus "
				"the frame counters. \"path\" addresses a specific "
				"surface; the default is the first uiSurface among the "
				"engine components.",
				jo("type", "object",
					"properties",
					jo("selector",
						jo("type", "string",
							"description",
							"A CSS selector; empty for the full markup + "
							"stats."),
						"path",
						jo("type", "string",
							"description",
							"Optional gold path to a specific uiSurface."))),
				[](object args, mcpServerContext& ctx) -> var {
					return uiQueryImpl(args, ctx);
				},
			},
			mcpTool{
				"ui_set",
				"Mutate an element on a uiSurface: set text, a style "
				"declaration object (\"style\": {\"color\": \"red\"}), "
				"or a pseudo-class state (\"state\": \"checked\", "
				"\"stateValue\": true). Applies and forces a re-render.",
				jo("type", "object",
					"properties",
					jo("id",
						jo("type", "integer",
							"description",
							"The element id (from ui_query descriptors)."),
						"text", jo("type", "string"),
						"style", jo("type", "object"),
						"state", jo("type", "string",
							"description",
							"hover | active | focus | checked | disabled "
							"| open"),
						"stateValue", jo("type", "boolean"),
						"path", jo("type", "string")),
					"required", ja("id")),
				[](object args, mcpServerContext& ctx) -> var {
					return uiSetImpl(args, ctx);
				},
			},
			mcpTool{
				"ui_event",
				"Dispatch a synthetic UI pointer event (\"move\", "
				"\"down\", \"up\", \"click\"...) on a uiSurface — the "
				"same pipeline real input arrives through, so :hover and "
				"handlers fire.",
				jo("type", "object",
					"properties",
					jo("type",
						jo("type", "string",
							"description",
							"move | hover | down | press | up | release "
							"| leave | cancel"),
						"x", jo("type", "number",
							"description",
							"UI-space x (negative dispatches to the "
							"hovered element)"),
						"y", jo("type", "number", "description",
							"UI-space y"),
						"data", jo("description",
							"Optional event payload."),
						"path", jo("type", "string"))),
				[](object args, mcpServerContext& ctx) -> var {
					return uiEventImpl(args, ctx);
				},
			},
			mcpTool{
				"screenshot",
				"Capture the game's rendered back buffer: requests the "
				"shot (backends write the PNG post-submit) and, on the "
				"next call once the file is written, returns it as MCP "
				"image content (and cleans the file up). \"path\" may "
				"pick the location; default /tmp/gold-mcp.png.",
				jo("type", "object",
					"properties",
					jo("path", jo("type", "string",
						"description",
						"The PNG path; default /tmp/gold-mcp.png."))),
				[](object args, mcpServerContext& ctx) -> var {
					return screenshotImpl(args, ctx);
				},
			},
			mcpTool{
				"window_input",
				"Inject a window event exactly as a real device would "
				"emit it (its dictionary goes through window.handleEvent "
				"unchanged: \"type\": \"key_down\"/\"text_input\"/"
				"\"mouse_move\"..., with \"keyCode\"/\"text\"/"
				"\"button\"/\"x\"/\"y\" fields as needed).",
				jo("type", "object",
					"properties",
					jo("type",
						jo("type", "string",
							"description",
							"The event type (key_down, key_up, "
							"text_input, mouse_down, mouse_up, "
							"mouse_move, mouse_wheel...)"),
						"keyCode", jo("type", "integer"),
						"text", jo("type", "string"),
						"button", jo("type", "integer"),
						"x", jo("type", "number"),
						"y", jo("type", "number"),
						"scrollX", jo("type", "number"),
						"scrollY", jo("type", "number")),
					"required", ja("type")),
				[](object args, mcpServerContext& ctx) -> var {
					return windowInputImpl(args, ctx);
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