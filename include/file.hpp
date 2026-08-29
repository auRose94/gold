#pragma once

#include <filesystem>

#include "types.hpp"

namespace gold {
	using std::string;
	using std::string_view;
	using std::vector;
	using path = std::filesystem::path;
	/** A filesystem/file object. Reads/writes data with write-time caching,
	 * plus parsing/serializing JSON, BSON, CBOR, MsgPack and UBJSON.
	 * Malformed input is returned as a `genericError` var, never thrown. */
	struct file : public object {
	 protected:
		static object& getPrototype();
		static path forwardPath(path);
	 public:
		file();
		file(path p);
		file(binary data);
		file(string_view data);

		var save(list args = {});
		var load(list args = {});
		var trash(list args = {});
		var getWriteTime(list args = {});
		var hash(list args = {});
		var extension(list args = {});
		var asJSON(list args = {});
		var asBSON(list args = {});
		var asCBOR(list args = {});
		var asMsgPack(list args = {});
		var asUBJSON(list args = {});

		operator binary();
		operator string();
		operator string_view();

		static string currentWorkingDir();
		static var readFile(path p);
		static var saveFile(path p, string_view data);
		static object& recursiveReadDirectory(
			path p, object& results);
		static var parseJSON(string_view data);
		static var parseBSON(string_view data);
		static var parseCBOR(string_view data);
		static var parseMsgPack(string_view data);
		static var parseUBJSON(string_view data);
		static string serializeJSON(var data, bool pretty = false);
		static binary serializeBSON(var data);
		static binary serializeCBOR(var data);
		static binary serializeMsgPack(var data);
		static binary serializeUBJSON(var data);
		static binary decodeDataURL(string_view v, string& mimeType);
		static binary decodeBase64(string_view v);
		static string encodeBase64(binary b);
		static var pack(list entries);
		static var unpack(binary data);
	};
}  // namespace gold
