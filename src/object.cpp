
#include <stdarg.h>
#include <string.h>

#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "file.hpp"
#include "types.hpp"
#include "object_impl.hpp"

namespace gold {
	using namespace std;
	static mutex cryptoMutex;

	object::ptr object::newObjData(omap m, uint64_t id) {
		auto obj = object();
		auto p = new objData{m, obj, id, shared_mutex()};
		p->id = id != 0 ? id : (uint64_t)p;
		return object::ptr(p);
	}

	void object::initMemory() {
		// Lazy allocation is not thread-safe on its own: two threads writing
		// to a default-constructed object could both see !data and both write
		// the shared_ptr. Use double-checked locking with atomic load/store
		// on the shared_ptr (ThreadSanitizer-clean).
		if (!std::atomic_load(&data)) {
			static mutex initMtx;
			lock_guard<mutex> guard(initMtx);
			if (!std::atomic_load(&data))
				std::atomic_store(&data, newObjData());
		}
	}

	void object::findParent() {
		auto parentIt = data->items.find("proto");
		if (parentIt != data->items.end()) {
			auto parent = parentIt->second.getObject();
			if (parent) {
				setParent(parent);
				data->items.erase(parentIt);
			}
		}
	}

	object::object(var value) {
		auto obj = value.getObject();
		if (bool(obj.data))
			data = obj.data;
		else
			data = newObjData();
	}

	object::object(const obj& copy) : data(copy.data) {}

	object::object(obj&& move) : data(std::move(move.data)) {
		move.data = nullptr;
	}

	object::object(initList list)
		: data(newObjData(object::omap(list), 0)) {
		findParent();
	}

	object::object() : data(nullptr) {}

	object::~object() {
		if (data && data.use_count() <= 1) {
			data->items.clear();
			data->parent = object();
		}
		data = nullptr;
	}

	object::omap::iterator object::begin() {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		return std::begin(data->items);
	}

	object::omap::iterator object::end() {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		return std::end(data->items);
	}

	uint64_t object::refs() const {
		if (data) return data.use_count();
		return 0;
	}

	uint64_t object::size() const {
		if (data) return data->items.size();
		return 0;
	}

	types object::getType(string name) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		return val.getType();
	}

	string object::getCookieString() {
		auto buffer = string();
		auto count = size();
		uint64_t i = 0;
		for (auto it = begin(); it != end(); ++it, ++i) {
			auto key = it->first;
			auto value = it->second.getString();
			auto isLast = count == 0 || i == count - 1;
			buffer += key + "=" + value + (isLast ? "" : "; ");
		}
		return buffer;
	}

	string object::getJSON(bool pretty) {
		return file::serializeJSON(*this, pretty);
	}

	binary object::getJSONBin(bool pretty) {
		auto str = file::serializeJSON(*this, pretty);
		return binary(str.begin(), str.end());
	}

	binary object::getBSON() {
		return file::serializeBSON(*this);
	}

	binary object::getCBOR() {
		return file::serializeCBOR(*this);
	}

	binary object::getMsgPack() {
		return file::serializeMsgPack(*this);
	}

	binary object::getUBJSON() {
		return file::serializeUBJSON(*this);
	}

	var object::callMethod(string name) {
		initMemory();
		auto method = this->getMethod(name);
		var resp;
		if (method)
			resp = (this->*method)({});
		else {
			auto func = this->getFunc(name);
			if (func) resp = func({*this});
		}
		return resp;
	}

	var object::callMethod(string name, list args) {
		initMemory();
		auto method = this->getMethod(name);
		var resp;
		if (method != nullptr)
			resp = (this->*method)(args);
		else {
			auto func = this->getFunc(name);
			auto thisArgs = list({*this});
			thisArgs += args;
			if (func) resp = func(thisArgs);
		}
		return resp;
	}

	void object::copy(object& other) {
		initMemory();
		if (other) {
			shared_lock<shared_mutex> srcGuard(other.data->omutex);
			unique_lock<shared_mutex> dstGuard(data->omutex);
			auto end = other.data->items.end();
			for (auto it = other.data->items.begin(); it != end; ++it)
				data->items[it->first] = it->second;
			findParent();
		}
	}

	void object::empty() {
		if (data) {
			unique_lock<shared_mutex> guard(data->omutex);
			data->items.clear();
		}
	}

	void object::setParent(object other) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		if (other.data) {
			data->parent.data = other.data;
		} else
			data->parent = object();
	}

	object object::getParent() {
		initMemory();
		return data->parent;
	}

	bool object::inherits(const object other) const {
		if (!data) return false;
		if (!other.data) return false;
		auto cur = data->parent;
		while (cur) {
			if (cur.data->id == other.data->id) return true;
			cur = cur.data->parent;
		}
		return false;
	}

	template <typename T>
	void object::setExpression(string name, T value) {
		// This allows you to set values of objects and lists
		// from inside objects, recursively.
		auto varVal = var(value);
		auto sqIndex = name.find('[');
		if (sqIndex == string::npos) {
			// Fast path: plain key, no expression parsing or allocation.
			data->items[name] = varVal;
			return;
		}
		auto aName = name.substr(0, sqIndex);
		auto end = name.find(']', sqIndex);
		if (sqIndex + 1 == end) {
			auto liVal = getExpression(aName);
			auto li = liVal.getList();
			if (liVal != varVal) li.pushVar(varVal);
			varVal = li;
		} else if (end != string::npos) {
			auto subKey =
				name.substr(sqIndex + 1, end - sqIndex - 1) +
				name.substr(end + 1);
			auto con = getObject(aName, obj({}));
			con.setExpression(subKey, varVal);
			varVal = con;
		}
		data->items[aName] = varVal;
	}

	var object::getExpression(string name) {
		auto sqIndex = name.find('[');
		if (sqIndex == string::npos) {
			// Fast path: plain key, no expression parsing or allocation.
			auto it = data->items.find(name);
			if (it != data->items.end())
				return it->second;
			if (data->parent.data)
				return data->parent.getExpression(name);
			// Data-driven prototype: a "proto" key pointing at an
			// object behaves as a fallback scope even when it has not
			// been promoted into data->parent by findParent().
			auto proto = data->items.find("proto");
			if (proto != data->items.end() &&
				proto->second.isObject())
				return proto->second.getObject().getExpression(name);
			return var();
		}
		auto varVal = var();
		auto aName = name.substr(0, sqIndex);
		auto end = name.find(']', sqIndex);
		if (sqIndex + 1 == end) {
			auto con = list();
			auto it = data->items.find(aName);
			if (it != data->items.end())
				con = it->second.getList();
			else if (!con && data->parent.data)
				con = data->parent.getExpression(aName).getList();
			if (con) varVal = con;
		} else if (end != string::npos) {
			auto subKey =
				name.substr(sqIndex + 1, end - sqIndex - 1) +
				name.substr(end + 1);
			auto con = obj();
			auto it = data->items.find(aName);
			if (it != data->items.end())
				con = it->second.getObject();
			else if (!con && data->parent.data)
				con = data->parent.getExpression(aName).getObject();
			varVal = con.getExpression(subKey);
		}
		return varVal;
	}

	void object::setString(string name, string value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setStringView(string name, string_view value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setInt64(string name, int64_t value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setInt32(string name, int32_t value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setInt16(string name, int16_t value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setInt8(string name, int8_t value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setUInt64(string name, uint64_t value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setUInt32(string name, uint32_t value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setUInt16(string name, uint16_t value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setUInt8(string name, uint8_t value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setDouble(string name, double value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setFloat(string name, float value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setBool(string name, bool value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setList(string name, list value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setObject(string name, object value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setMethod(string name, method& value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setFunc(string name, func& value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setPtr(string name, void* value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, var(value, typePtr));
	}

	void object::setBinary(string name, binary value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setVar(string name, var value) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, value);
	}

	void object::setNull(string name) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		setExpression(name, var());
	}

	void object::erase(string name) {
		initMemory();
		unique_lock<shared_mutex> guard(data->omutex);
		data->items.erase(name);
	}

	bool object::owns(string name) {
		if (!data) return false;
		shared_lock<shared_mutex> guard(data->omutex);
		return data->items.find(name) != data->items.end();
	}

	string object::getString(string name, string def) {
		try {
			initMemory();
			shared_lock<shared_mutex> guard(data->omutex);
			auto val = getExpression(name);
			if (val.isString()) return val.getString();
			return def;
		} catch (exception&) {
			return def;
		}
	}

	gold::var object::generateHash(string value, string salt) {
		try {
			unique_lock<mutex> guard(cryptoMutex);
			// OWASP recommends 600k iterations for PBKDF2-HMAC-SHA256.
			unsigned char derived[SHA256_DIGEST_LENGTH];
			if (PKCS5_PBKDF2_HMAC(
					value.data(),
					int(value.size()),
					(const unsigned char*)salt.data(),
					int(salt.size()),
					600000,
					EVP_sha256(),
					SHA256_DIGEST_LENGTH,
					derived) != 1)
				return genericError("PBKDF2 key derivation failed");

			std::string result;
			result.resize(SHA256_DIGEST_LENGTH * 2);
			for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
				static const char hex[] = "0123456789abcdef";
				result[i * 2] = hex[derived[i] >> 4];
				result[i * 2 + 1] = hex[derived[i] & 0x0F];
			}
			return result;
		} catch (exception& e) {
			return genericError(e.what());
		}
	}

	string_view object::getStringView(
		string name, string_view def) {
		try {
			initMemory();
			shared_lock<shared_mutex> guard(data->omutex);
			auto val = getExpression(name);
			if (val.isView()) return val.getStringView();
			return def;
		} catch (exception&) {
			return def;
		}
	}

	int64_t object::getInt64(string name, int64_t def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getInt64();
		return def;
	}

	int32_t object::getInt32(string name, int32_t def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getInt32();
		return def;
	}

	int16_t object::getInt16(string name, int16_t def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getInt16();
		return def;
	}

	int8_t object::getInt8(string name, int8_t def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getInt8();
		return def;
	}

	uint64_t object::getUInt64(string name, uint64_t def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getUInt64();
		return def;
	}

	uint32_t object::getUInt32(string name, uint32_t def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getUInt32();
		return def;
	}

	uint16_t object::getUInt16(string name, uint16_t def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getUInt16();
		return def;
	}

	uint8_t object::getUInt8(string name, uint8_t def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getUInt8();
		return def;
	}

	double object::getDouble(string name, double def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getDouble();
		return def;
	}

	float object::getFloat(string name, float def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isNumber()) return val.getFloat();
		return def;
	}

	bool object::getBool(string name, bool def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isBool()) return val.getBool();
		return def;
	}

	list object::getList(string name, list def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isList()) return val.getList();
		return def;
	}

	void object::assignList(string name, list& result) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isList()) result = val.getList();
	}

	void object::assignObject(string name, object& result) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isObject()) result = val.getObject();
	}

	object object::getObject(string name, object def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isObject()) return val.getObject();
		return def;
	}

	method object::getMethod(string name) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isMethod()) return val.getMethod();
		return nullptr;
	}

	func object::getFunc(string name) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isFunction()) return val.getFunction();
		return nullptr;
	}

	void* object::getPtr(string name, void* def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val) return val.getPtr();
		return def;
	}

	binary object::getBinary(string name, binary def) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isBinary()) return val.getBinary();
		return def;
	}

	void object::assignBinary(string name, binary& result) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		auto val = getExpression(name);
		if (val.isBinary()) result = val.getBinary();
	}

	var object::getVar(string name) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		return getExpression(name);
	}

	varRef object::operator[](string_view name) {
		initMemory();
		return varRef(var(*this), string(name));
	}

	var object::operator->*(string name) {
		initMemory();
		shared_lock<shared_mutex> guard(data->omutex);
		return getExpression(name);
	}

	var object::operator()(string name, list args) {
		return this->callMethod(name, args);
	}

	var object::operator()(string name) {
		return this->callMethod(name);
	}

	bool object::operator==(object& other) {
		if (!data || !other.data) return data == other.data;
		if (data->id == other.data->id) return true;
		return false;
	}

	object& object::operator=(const object& o) {
		if (data && data.use_count() <= 1) {
			data->items.clear();
			data->parent = object();
		}
		data = o.data;
		return *this;
	}

	object& object::operator=(object&& o) {
		data = std::move(o.data);
		o.data = nullptr;
		return *this;
	}

	object::operator bool() const { return bool(this->data); }

	void object::parseURLEncoded(string value, object& result) {
		auto it = value.begin();
		string buffer = "";
		string key = "";

		auto decodePercent = [&](string::iterator& it) {
			string h = "0x";
			if (it + 1 < value.end()) h += *(++it);
			if (it + 1 < value.end()) h += *(++it);
			auto c = (char)strtoul(h.c_str(), nullptr, 16);
			buffer += string(&c, 1);
		};

		auto pushVar = [&]() {
			if (key.size() > 0 && buffer.size() > 0) {
				auto vType = result.getType(key);
				if (vType == typeString) {
					auto copy = result.getString(key);
					auto arr = list({copy, buffer});
					result.setList(key, arr);
				} else if (vType == typeList) {
					auto arr = result.getList(key, {});
					arr.pushString(buffer);
					if (result.getType(key) == typeNull)
						result.setList(key, arr);
				} else {
					result.setString(key, buffer);
				}
				buffer = "";
				key = "";
			}
		};
		while (it != value.end()) {
			if (isalnum((unsigned char)*it))
				buffer += *it;
			else if (*it == '=') {
				key = buffer;
				buffer = "";
			} else if (*it == '&') {
				pushVar();
			} else if (*it == '+') {
				buffer += " ";
			} else if (*it == '%') {
				decodePercent(it);
			} else {
				buffer += *it;
			}
			++it;
		}
		pushVar();
	}

	void object::parseCookie(string value, object& result) {
		auto it = value.begin();
		string buffer = "";
		string key = "";

		auto decodePercent = [&](string::iterator& it) {
			string h = "0x";
			if (it + 1 < value.end()) h += *(++it);
			if (it + 1 < value.end()) h += *(++it);
			auto c = (char)strtoul(h.c_str(), nullptr, 16);
			buffer += string(&c, 1);
		};

		while (it != value.end()) {
			if (isalnum((unsigned char)*it))
				buffer += *it;
			else if (*it == '=') {
				key = buffer;
				buffer = "";
			} else if (*it == ';') {
				auto vType = result.getType(key);
				if (vType == typeString) {
					auto copy = result.getString(key);
					auto arr = list({copy, buffer});
					result.setList(key, arr);
				} else if (vType == typeList) {
					auto arr = list();
					result.assignList(key, arr);
					arr.pushString(buffer);
				} else {
					result.setString(key, buffer);
				}
				buffer = "";
				key = "";
				it++;
			} else if (*it == '+') {
				buffer += " ";
			} else if (*it == '%') {
				decodePercent(it);
			} else {
				buffer += *it;
			}
			++it;
		}
		if (key.size() > 0 && buffer.size() > 0) {
			auto vType = result.getType(key);
			if (vType == typeString) {
				auto copy = result.getString(key);
				auto arr = list({copy, buffer});
				result.setList(key, arr);
			} else if (vType == typeList) {
				auto arr = list();
				result.assignList(key, arr);
				arr.pushString(buffer);
			} else {
				result.setString(key, buffer);
			}
		}
	}

	var object::loadJSON(string path) {
		return file::readFile(path).getObject<file>().asJSON();
	}

	var object::saveJSON(string path, object value) {
		auto d = value.getJSONBin(true);
		auto v = string_view((char*)d.data(), d.size());
		return file::saveFile(path, v);
	}

	var object::loadBSON(string path) {
		return file::readFile(path).getObject<file>().asBSON();
	}

	var object::saveBSON(string path, object value) {
		auto d = value.getBSON();
		auto v = string_view((char*)d.data(), d.size());
		return file::saveFile(path, v);
	}

	var object::loadCBOR(string path) {
		return file::readFile(path).getObject<file>().asCBOR();
	}

	var object::saveCBOR(string path, object value) {
		auto d = value.getCBOR();
		auto v = string_view((char*)d.data(), d.size());
		return file::saveFile(path, v);
	}

	var object::loadMsgPack(string path) {
		return file::readFile(path).getObject<file>().asMsgPack();
	}

	var object::saveMsgPack(string path, object value) {
		auto d = value.getMsgPack();
		auto v = string_view((char*)d.data(), d.size());
		return file::saveFile(path, v);
	}

	var object::loadUBJSON(string path) {
		return file::readFile(path).getObject<file>().asUBJSON();
	}

	var object::saveUBJSON(string path, object value) {
		auto d = value.getUBJSON();
		auto v = string_view((char*)d.data(), d.size());
		return file::saveFile(path, v);
	}

}  // namespace gold