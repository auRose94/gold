#include "file.hpp"

#include <openssl/evp.h>

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>

namespace gold {
	using namespace std;
	using value_t = nlohmann::detail::value_t;
	namespace fs = std::filesystem;
	const auto preferred_separator =
		fs::path::preferred_separator;

	obj& file::getPrototype() {
		static auto proto = obj({
			{"save", method(&file::save)},
			{"load", method(&file::load)},
			{"trash", method(&file::trash)},
			{"getWriteTime", method(&file::getWriteTime)},
			{"hash", method(&file::hash)},
			{"extension", method(&file::extension)},
		});
		return proto;
	}

	path file::forwardPath(path p) {
		auto str = p.native();
		for (size_t i = 0; i < str.length(); ++i) {
			auto c = str[i];
			if (c == '\\') str[i] = '/';
		}
		p = path(str);
		return p;
	}

	file::file() : obj() {}

	file::file(path p) : obj() {
		setParent(getPrototype());
		setString("path", p.string());
	}

	file::file(binary data) : obj() {
		setParent(getPrototype());
		setBinary("data", data);
	}

	file::file(string_view data) {
		setParent(getPrototype());
		setStringView("data", data);
	}

	var file::save(list args) {
		auto path = getString("path");
		auto bin = getStringView("data");
		for (auto it = args.begin(); it != args.end(); ++it)
			if (it->isString() && path == "")
				path = it->getString();
			else if (
				(it->isBinary() || it->isString() || it->isView()) &&
				bin.size() == 0)
				bin = it->getStringView();
			else
				break;
		if (path.size() == 0)
			return genericError(
				"path is empty, supply as argument or set path on "
				"object");
		if (bin.size() == 0)
			return genericError(
				"data is empty, supply as argument or set data on "
				"object");
		auto str = ofstream(path);
		if (str.is_open()) {
			str.write((char*)bin.data(), bin.size());
			str.close();
			auto writeTime = fs::last_write_time(path);
			auto wtms =
				(uint64_t)
					std::chrono::duration_cast<std::chrono::nanoseconds>(
						writeTime.time_since_epoch())
						.count();
			setUInt64("writeTime", wtms);
			return true;
		}
		return false;
	}

	var file::load(list args) {
		try {
			auto path = args.size() > 0 ? args[0].getString()
																	: getString("path");
			if (path.size() == 0)
				return genericError(
					"path is empty, supply as argument or set path on "
					"object");
			if (fs::exists(path)) {
				auto bin = getStringView("data");
				auto writeTime = fs::last_write_time(path);
				auto wtms = (uint64_t)std::chrono::duration_cast<
											std::chrono::nanoseconds>(
											writeTime.time_since_epoch())
											.count();
				auto lastWriteTime = getUInt64("writeTime");
				if (bin.size() > 0 && wtms == lastWriteTime) return bin;
				auto str = ifstream(path);
				if (str.is_open()) {
					str.seekg(0, str.end);
					auto size = size_t(str.tellg());
					auto read = binary(size);
					str.seekg(0, str.beg);
					str.read((char*)read.data(), read.size());
					str.close();
					setUInt64("writeTime", wtms);
					setBinary("data", read);
					return getStringView("data");
				}
			}
			return gold::var();
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

	var file::trash(list args) {
		auto path =
			args.size() > 0 ? args[0].getString() : getString("path");
		if (path.size() == 0)
			return genericError(
				"path is empty, supply as argument or set path on "
				"object");
		setNull("writeTime");
		return remove(fs::path(path));
	}

	var file::getWriteTime(list args) {
		auto path =
			args.size() > 0 ? args[0].getString() : getString("path");
		if (path.size() == 0)
			return genericError(
				"path is empty, supply as argument or set path on "
				"object");
		auto cur = getUInt64("writeTime");
		if (cur != 0) return cur;
		auto writeTime = fs::last_write_time(path);
		auto wtms =
			(uint64_t)
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					writeTime.time_since_epoch())
					.count();
		return wtms;
	}

	var file::hash(list args) {
		auto bin = binary();
		if (args.size() > 0)
			args[0].assignBinary(bin);
		else
			assignBinary("data", bin);
		auto strData = string_view((char*)bin.data(), bin.size());
		auto h = std::hash<string_view>();
		return to_string((uint64_t)h(strData));
	}

	var file::extension(list args) {
		return fs::path(getString("path")).extension().string();
	}

	var file::asJSON(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseJSON(d);
		return var();
	}

	var file::asBSON(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseBSON(d);
		return var();
	}

	var file::asCBOR(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseCBOR(d);
		return var();
	}

	var file::asMsgPack(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseMsgPack(d);
		return var();
	}

	var file::asUBJSON(list args) {
		auto d = getStringView("data");
		if (d.size() > 0) return file::parseUBJSON(d);
		return var();
	}

	file::operator binary() { return load().getBinary(); }

	file::operator string() { return load().getString(); }

	file::operator string_view() {
		return load().getStringView();
	}

	string file::currentWorkingDir() {
		return fs::current_path().string();
	}

	var file::readFile(path p) {
		auto f = file(p);
		auto err = f.load();
		if (err.isError()) return err;
		return f;
	}

	var file::saveFile(path p, string_view data) {
		auto f = file(data);
		auto err = f.save({p.string()});
		if (err.isError()) return err;
		return f;
	}

	obj& file::recursiveReadDirectory(path p, obj& results) {
		try {
			auto point = fs::canonical(p);
			auto cwd = fs::current_path();
			auto pName = point.string();
			pName =
				pName.substr(pName.rfind(preferred_separator) + 1);
			auto absPoint = fs::canonical(point);
			if (fs::exists(absPoint) && fs::is_directory(absPoint)) {
				for (auto& ip : fs::recursive_directory_iterator(
							 absPoint,
							 fs::directory_options::
								 follow_directory_symlink)) {
					auto chop = ip.path().string().find(pName);
					auto relURL = fs::relative(ip, cwd);
					auto url = forwardPath(
						fs::path(ip.path().string().substr(chop - 1)));

					if (!fs::is_directory(ip))
						results.setObject(url.string(), file(ip));
				}
			} else {
				auto e =
					"Directory doesn't exist. (" + point.string() + ")";
				results.setString("error", e);
			}
			return results;
		} catch (const exception& e) {
			return results;
		}
	}

	bool isTypedArray(
		json value, bool& hasF, bool& allI, bool& allU) {
		bool rightSize = false;
		auto s = value.size();
		if (s == 2)
			rightSize = true;
		else if (s == 3)
			rightSize = true;
		else if (s == 4)
			rightSize = true;
		else if (s == 9)
			rightSize = true;
		else if (s == 16)
			rightSize = true;
		if (rightSize) {
			hasF = false;
			allI = true;
			allU = true;
			for (auto it = value.begin(); it != value.end(); ++it) {
				if (!hasF) hasF = it->is_number_float();
				if (allI) allI = it->is_number_integer();
				if (allU) allU = it->is_number_unsigned();
				if (!hasF && !allI && !allU) return false;
			}
			return true;
		}
		return false;
	}

	list json2List(json& value);
	var evaluateList(json& value) {
		var toSet;
		bool hasF, allI, AllU;
		if (isTypedArray(value, hasF, allI, AllU)) {
			if (hasF) {
				switch (value.size()) {
					case 2:
						toSet = vec2f(value[0], value[1]);
						break;
					case 3:
						toSet = vec3f(value[0], value[1], value[2]);
						break;
					case 4:
						toSet =
							vec4f(value[0], value[1], value[2], value[3]);
						break;
					case 9:
						toSet = mat3x3f(
							{value[0], value[1], value[2], value[3], value[4],
							 value[5], value[6], value[7], value[8]});
						break;
					case 16:
						toSet = mat4x4f(
							{value[0], value[1], value[2], value[3], value[4],
							 value[5], value[6], value[7], value[8], value[9],
							 value[10], value[11], value[12], value[13],
							 value[14], value[15]});
						break;
					default:
						break;
				}
			} else if (AllU) {
				switch (value.size()) {
					case 2:
						toSet = vec2u32(value[0], value[1]);
						break;
					case 3:
						toSet = vec3u32(value[0], value[1], value[2]);
						break;
					case 4:
						toSet =
							vec4u32(value[0], value[1], value[2], value[3]);
						break;
					default:
						break;
				}
			} else if (allI) {
				switch (value.size()) {
					case 2:
						toSet = vec2i32(value[0], value[1]);
						break;
					case 3:
						toSet = vec3i32(value[0], value[1], value[2]);
						break;
					case 4:
						toSet =
							vec4i32(value[0], value[1], value[2], value[3]);
						break;
					default:
						break;
				}
			} else {
				toSet = json2List(value);
			}
		} else {
			toSet = json2List(value);
		}
		return toSet;
	}

	object json2Object(json& j) {
		auto ret = obj();
		for (auto it = j.begin(); it != j.end(); ++it) {
			auto name = it.key();
			auto value = it.value();
			var toSet = var();
			switch (value.type()) {
				case value_t::object:
					toSet = json2Object(value);
					break;
				case value_t::array:
					toSet = evaluateList(value);
					break;
				case value_t::string:
					toSet = var((string)value);
					break;
				case value_t::boolean:
					toSet = var((bool)value);
					break;
				case value_t::number_integer:
					toSet = var((int64_t)value);
					break;
				case value_t::number_unsigned:
					toSet = var((uint64_t)value);
					break;
				case value_t::number_float:
					toSet = var((double)value);
					break;
				default:
				case value_t::null:
					break;
			}
			ret.setVar(name, toSet);
		}
		return ret;
	}

	list json2List(json& j) {
		auto ret = list();
		size_t i = 0;
		for (auto it = j.begin(); it != j.end(); ++it) {
			auto value = it.value();
			var toSet = var();
			switch (value.type()) {
				case value_t::object:
					toSet = json2Object(value);
					break;
				case value_t::array:
					toSet = evaluateList(value);
					break;
				case value_t::string:
					toSet = (string)value;
					break;
				case value_t::boolean:
					toSet = (bool)value;
					break;
				case value_t::number_integer:
					toSet = (int64_t)value;
					break;
				case value_t::number_unsigned:
					toSet = (uint64_t)value;
					break;
				case value_t::number_float:
					toSet = (double)value;
					break;
				case value_t::null:
				default:
					break;
			}
			ret.setVar(i, toSet);
			i++;
		}
		return ret;
	}

	var file::parseJSON(string_view data) {
		try {
			json j = json::parse(data);
			if (j.is_object())
				return json2Object(j);
			else if (j.is_array())
				return json2List(j);
		} catch (const exception& e) {
			return genericError(e.what());
		}
		return var();
	}

	var file::parseBSON(string_view data) {
		try {
			json j = json::from_bson(data);
			if (j.is_object())
				return json2Object(j);
			else if (j.is_array())
				return json2List(j);
		} catch (const exception& e) {
			return genericError(e.what());
		}
		return var();
	}

	var file::parseCBOR(string_view data) {
		try {
			json j = json::from_cbor(data);
			if (j.is_object())
				return json2Object(j);
			else if (j.is_array())
				return json2List(j);
		} catch (const exception& e) {
			return genericError(e.what());
		}
		return var();
	}

	var file::parseMsgPack(string_view data) {
		try {
			json j = json::from_msgpack(data);
			if (j.is_object())
				return json2Object(j);
			else if (j.is_array())
				return json2List(j);
		} catch (const exception& e) {
			return genericError(e.what());
		}
		return var();
	}

	var file::parseUBJSON(string_view data) {
		try {
			json j = json::from_ubjson(data);
			if (j.is_object())
				return json2Object(j);
			else if (j.is_array())
				return json2List(j);
		} catch (const exception& e) {
			return genericError(e.what());
		}
		return var();
	}

	json basicToJSON(var value);

	json list2JSON(list li) {
		json j = json::array();
		auto end = li.end();
		for (auto i = li.begin(); i != end; ++i) {
			j.push_back(basicToJSON(*i));
		}
		return j;
	}

	json object2JSON(object obj) {
		json j = json::object();
		auto end = obj.end();
		for (auto i = obj.begin(); i != end; ++i) {
			auto name = i->first;
			auto value = i->second;
			j[name] = basicToJSON(value);
		}
		return j;
	}

	json basicToJSON(var value) {
		switch (value.getType()) {
			case typeNull:
				return nullptr;
			case typeList:
				return list2JSON(value.getList());
			case typeObject:
				return object2JSON(value.getObject());
			case typeString:
				return value.getString();
			case typeInt64:
				return value.getInt64();
			case typeInt32:
				return value.getInt32();
			case typeInt16:
				return value.getInt16();
			case typeInt8:
				return value.getInt8();
			case typeUInt64:
				return value.getUInt64();
			case typeUInt32:
				return value.getUInt32();
			case typeUInt16:
				return value.getUInt16();
			case typeUInt8:
				return value.getUInt8();
			case typeDouble:
				return value.getDouble();
			case typeFloat:
				return value.getFloat();
			case typeBool:
				return value.getBool();
			case typeVec2Int64: {
				auto a = json();
				a.push_back(value.getInt64(0));
				a.push_back(value.getInt64(1));
				return a;
			}
			case typeVec2Int32: {
				auto a = json();
				a.push_back(value.getInt32(0));
				a.push_back(value.getInt32(1));
				return a;
			}
			case typeVec2Int16: {
				auto a = json();
				a.push_back(value.getInt16(0));
				a.push_back(value.getInt16(1));
				return a;
			}
			case typeVec2Int8: {
				auto a = json();
				a.push_back(value.getInt8(0));
				a.push_back(value.getInt8(1));
				return a;
			}
			case typeVec2UInt64: {
				auto a = json();
				a.push_back(value.getUInt64(0));
				a.push_back(value.getUInt64(1));
				return a;
			}
			case typeVec2UInt32: {
				auto a = json();
				a.push_back(value.getUInt32(0));
				a.push_back(value.getUInt32(1));
				return a;
			}
			case typeVec2UInt16: {
				auto a = json();
				a.push_back(value.getUInt16(0));
				a.push_back(value.getUInt16(1));
				return a;
			}
			case typeVec2UInt8: {
				auto a = json();
				a.push_back(value.getUInt8(0));
				a.push_back(value.getUInt8(1));
				return a;
			}
			case typeVec3Int64: {
				auto a = json();
				a.push_back(value.getInt64(0));
				a.push_back(value.getInt64(1));
				a.push_back(value.getInt64(2));
				return a;
			}
			case typeVec3Int32: {
				auto a = json();
				a.push_back(value.getInt32(0));
				a.push_back(value.getInt32(1));
				a.push_back(value.getInt32(2));
				return a;
			}
			case typeVec3Int16: {
				auto a = json();
				a.push_back(value.getInt16(0));
				a.push_back(value.getInt16(1));
				a.push_back(value.getInt16(2));
				return a;
			}
			case typeVec3Int8: {
				auto a = json();
				a.push_back(value.getInt8(0));
				a.push_back(value.getInt8(1));
				a.push_back(value.getInt8(2));
				return a;
			}
			case typeVec3UInt64: {
				auto a = json();
				a.push_back(value.getUInt64(0));
				a.push_back(value.getUInt64(1));
				a.push_back(value.getUInt64(2));
				return a;
			}
			case typeVec3UInt32: {
				auto a = json();
				a.push_back(value.getUInt32(0));
				a.push_back(value.getUInt32(1));
				a.push_back(value.getUInt32(2));
				return a;
			}
			case typeVec3UInt16: {
				auto a = json();
				a.push_back(value.getUInt16(0));
				a.push_back(value.getUInt16(1));
				a.push_back(value.getUInt16(2));
				return a;
			}
			case typeVec3UInt8: {
				auto a = json();
				a.push_back(value.getUInt8(0));
				a.push_back(value.getUInt8(1));
				a.push_back(value.getUInt8(2));
				return a;
			}
			case typeVec4Int64: {
				auto a = json();
				a.push_back(value.getInt64(0));
				a.push_back(value.getInt64(1));
				a.push_back(value.getInt64(2));
				a.push_back(value.getInt64(3));
				return a;
			}
			case typeVec4Int32: {
				auto a = json();
				a.push_back(value.getInt32(0));
				a.push_back(value.getInt32(1));
				a.push_back(value.getInt32(2));
				a.push_back(value.getInt32(3));
				return a;
			}
			case typeVec4Int16: {
				auto a = json();
				a.push_back(value.getInt16(0));
				a.push_back(value.getInt16(1));
				a.push_back(value.getInt16(2));
				a.push_back(value.getInt16(3));
				return a;
			}
			case typeVec4Int8: {
				auto a = json();
				a.push_back(value.getInt8(0));
				a.push_back(value.getInt8(1));
				a.push_back(value.getInt8(2));
				a.push_back(value.getInt8(3));
				return a;
			}
			case typeVec4UInt64: {
				auto a = json();
				a.push_back(value.getUInt64(0));
				a.push_back(value.getUInt64(1));
				a.push_back(value.getUInt64(2));
				a.push_back(value.getUInt64(3));
				return a;
			}
			case typeVec4UInt32: {
				auto a = json();
				a.push_back(value.getUInt32(0));
				a.push_back(value.getUInt32(1));
				a.push_back(value.getUInt32(2));
				a.push_back(value.getUInt32(3));
				return a;
			}
			case typeVec4UInt16: {
				auto a = json();
				a.push_back(value.getUInt16(0));
				a.push_back(value.getUInt16(1));
				a.push_back(value.getUInt16(2));
				a.push_back(value.getUInt16(3));
				return a;
			}
			case typeVec4UInt8: {
				auto a = json();
				a.push_back(value.getUInt8(0));
				a.push_back(value.getUInt8(1));
				a.push_back(value.getUInt8(2));
				a.push_back(value.getUInt8(3));
				return a;
			}
			case typeVec2Float: {
				auto a = json();
				a.push_back(value.getFloat(0));
				a.push_back(value.getFloat(1));
				return a;
			}
			case typeVec3Float: {
				auto a = json();
				a.push_back(value.getFloat(0));
				a.push_back(value.getFloat(1));
				a.push_back(value.getFloat(2));
				return a;
			}
			case typeVec4Float: {
				auto a = json();
				a.push_back(value.getFloat(0));
				a.push_back(value.getFloat(1));
				a.push_back(value.getFloat(2));
				a.push_back(value.getFloat(3));
				return a;
			}
			case typeVec2Double: {
				auto a = json();
				a.push_back(value.getDouble(0));
				a.push_back(value.getDouble(1));
				return a;
			}
			case typeVec3Double: {
				auto a = json();
				a.push_back(value.getDouble(0));
				a.push_back(value.getDouble(1));
				a.push_back(value.getDouble(2));
				return a;
			}
			case typeVec4Double: {
				auto a = json();
				a.push_back(value.getDouble(0));
				a.push_back(value.getDouble(1));
				a.push_back(value.getDouble(2));
				a.push_back(value.getDouble(3));
				return a;
			}
			case typeQuatFloat: {
				auto a = json();
				a.push_back(value.getFloat(0));
				a.push_back(value.getFloat(1));
				a.push_back(value.getFloat(2));
				a.push_back(value.getFloat(3));
				return a;
			}
			case typeQuatDouble: {
				auto a = json();
				a.push_back(value.getDouble(0));
				a.push_back(value.getDouble(1));
				a.push_back(value.getDouble(2));
				a.push_back(value.getDouble(3));
				return a;
			}
			case typeMat3x3Float: {
				auto a = json();
				a.push_back(value.getFloat(0));
				a.push_back(value.getFloat(1));
				a.push_back(value.getFloat(2));

				a.push_back(value.getFloat(3));
				a.push_back(value.getFloat(4));
				a.push_back(value.getFloat(5));

				a.push_back(value.getFloat(6));
				a.push_back(value.getFloat(7));
				a.push_back(value.getFloat(8));
				return a;
			}
			case typeMat3x3Double: {
				auto a = json();
				a.push_back(value.getDouble(0));
				a.push_back(value.getDouble(1));
				a.push_back(value.getDouble(2));

				a.push_back(value.getDouble(3));
				a.push_back(value.getDouble(4));
				a.push_back(value.getDouble(5));

				a.push_back(value.getDouble(6));
				a.push_back(value.getDouble(7));
				a.push_back(value.getDouble(8));
				return a;
			}
			case typeMat4x4Float: {
				auto a = json();
				a.push_back(value.getFloat(0));
				a.push_back(value.getFloat(1));
				a.push_back(value.getFloat(2));
				a.push_back(value.getFloat(3));

				a.push_back(value.getFloat(4));
				a.push_back(value.getFloat(5));
				a.push_back(value.getFloat(6));
				a.push_back(value.getFloat(7));

				a.push_back(value.getFloat(8));
				a.push_back(value.getFloat(9));
				a.push_back(value.getFloat(10));
				a.push_back(value.getFloat(11));

				a.push_back(value.getFloat(12));
				a.push_back(value.getFloat(13));
				a.push_back(value.getFloat(14));
				a.push_back(value.getFloat(15));
				return a;
			}
			case typeMat4x4Double: {
				auto a = json();
				a.push_back(value.getDouble(0));
				a.push_back(value.getDouble(1));
				a.push_back(value.getDouble(2));
				a.push_back(value.getDouble(3));

				a.push_back(value.getDouble(4));
				a.push_back(value.getDouble(5));
				a.push_back(value.getDouble(6));
				a.push_back(value.getDouble(7));

				a.push_back(value.getDouble(8));
				a.push_back(value.getDouble(9));
				a.push_back(value.getDouble(10));
				a.push_back(value.getDouble(11));

				a.push_back(value.getDouble(12));
				a.push_back(value.getDouble(13));
				a.push_back(value.getDouble(14));
				a.push_back(value.getDouble(15));
				return a;
			}
			default:
				break;
		}
		return nullptr;
	}

	string file::serializeJSON(var data, bool pretty) {
		auto j = basicToJSON(data);
		auto str = pretty ? j.dump(1, '\t') : j.dump();
		auto out = string();
		out.resize(str.size(), 0);
		memcpy(out.data(), str.data(), str.size());
		return out;
	}

	binary file::serializeBSON(var data) {
		return json::to_bson(basicToJSON(data));
	}

	binary file::serializeCBOR(var data) {
		return json::to_cbor(basicToJSON(data));
	}

	binary file::serializeMsgPack(var data) {
		return json::to_msgpack(basicToJSON(data));
	}

	binary file::serializeUBJSON(var data) {
		return json::to_ubjson(basicToJSON(data));
	}

	binary file::decodeDataURL(string_view v, string& mimeType) {
		binary out;
		auto s = v.size();
		if (s == 0) return out;
		if (v.substr(0, 5).compare("data:") != 0) return out;
		auto commaIndex = v.find(',');
		auto semiIndex = v.find(';');
		if (
			semiIndex == string::npos && commaIndex != string::npos) {
			mimeType = v.substr(5, commaIndex);
		} else if (
			semiIndex != string::npos && semiIndex < commaIndex) {
			mimeType = v.substr(5, semiIndex - 5);
		}
		if (commaIndex == string::npos) return out;
		auto args = v.substr(5, commaIndex - 5);
		auto data = v.substr(commaIndex + 1);
		if (args.find(";base64") != string::npos) {
			out = decodeBase64(data);
		} else
			// TODO: Swap out escaped chars
			out = binary(data.begin(), data.end());
		return out;
	}

	binary file::decodeBase64(string_view v) {
		if (v.empty()) return binary();

		// Accept both the standard (+ /) and URL-safe (- _) alphabets.
		auto b64 = string(v);
		for (auto& c : b64) {
			if (c == '-') c = '+';
			else if (c == '_') c = '/';
		}

		// EVP_DecodeBlock requires padding to a multiple of 4.
		auto padded = b64;
		while (padded.size() % 4 != 0) padded += '=';

		size_t padCount = 0;
		for (auto it = padded.rbegin(); it != padded.rend() && *it == '=';
				 ++it)
			padCount++;

		binary out((padded.size() / 4) * 3);
		if (out.empty()) return out;

		auto n = EVP_DecodeBlock(
			out.data(), (const unsigned char*)padded.data(),
			int(padded.size()));
		if (n < 0) return binary();
		// Each '=' accounts for one zero byte in EVP_DecodeBlock's output.
		size_t decoded = size_t(n) - padCount;
		if (decoded > out.size()) decoded = out.size();
		out.resize(decoded);
		return out;
	}

	string file::encodeBase64(binary b) {
		if (b.empty()) return "";
		binary out((b.size() / 3 + 1) * 4);
		auto n = EVP_EncodeBlock(out.data(), b.data(), int(b.size()));
		if (n < 0) return "";
		auto str = string((char*)out.data(), size_t(n));
		// URL-safe alphabet and no padding.
		for (auto& c : str) {
			if (c == '+') c = '-';
			else if (c == '/') c = '_';
			else if (c == '=') c = '\0';
		}
		auto end = str.find('\0');
		if (end != string::npos) str.resize(end);
		return str;
	}

}  // namespace gold