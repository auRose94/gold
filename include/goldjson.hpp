#pragma once

#include <string>
#include <string_view>

#include "types.hpp"

namespace gold {

	/**
	 * Self-contained JSON and binary data codecs operating directly on
	 * gold's var/list/object types. Replaces the nlohmann/json dependency.
	 *
	 * JSON numbers: positive integers parse as unsigned, negative as signed,
	 * decimals/exponents as double. Arrays of uniform numbers are
	 * reconstructed into vec/mat types (2/3/4/9/16 elements), mirroring
	 * the previous behavior.
	 */

	/** Parse JSON text into a var (object/list/scalar). Returns
	 * genericError on malformed input. */
	var jsonParse(std::string_view data);

	/** Serialize a var as JSON text. */
	std::string jsonStringify(var data, bool pretty = false);

	// Binary codecs (BSON, CBOR, MessagePack, UBJSON).
	var bsonParse(std::string_view data);
	binary bsonStringify(var data);
	var cborParse(std::string_view data);
	binary cborStringify(var data);
	var msgpackParse(std::string_view data);
	binary msgpackStringify(var data);
	var ubjsonParse(std::string_view data);
	binary ubjsonStringify(var data);

}  // namespace gold