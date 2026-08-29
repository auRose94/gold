#pragma once

#include <bx/allocator.h>
#include <inttypes.h>
#include <stdbool.h>

#include <exception>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace gold {
	using std::exception;
	using std::function;
	using std::initializer_list;
	using std::map;
	using std::mutex;
	using std::ostream;
	using std::shared_ptr;
	using std::string;
	using std::string_view;
	using std::vector;
	/* <Types> */
	struct object;
	struct list;
	struct var;
	using method = var (object::*)(list);
	using func = function<var(list)>;
	using binary = vector<uint8_t>;
	using key = string;
	inline bx::DefaultAllocator defaultAllocator = bx::DefaultAllocator();

	typedef enum types_t {
		typeNull = 0,
		typeList,
		typeObject,
		typeMethod,
		typeFunction,
		typePtr,
		typeString,
		typeStringView,
		typeBinary,
		typeInt64,
		typeInt32,
		typeInt16,
		typeInt8,
		typeUInt64,
		typeUInt32,
		typeUInt16,
		typeUInt8,
		typeDouble,
		typeFloat,
		typeBool,
		typeException,
		// Advanced math
		typeVec2Int64,
		typeVec2Int32,
		typeVec2Int16,
		typeVec2Int8,
		typeVec2UInt64,
		typeVec2UInt32,
		typeVec2UInt16,
		typeVec2UInt8,

		typeVec3Int64,
		typeVec3Int32,
		typeVec3Int16,
		typeVec3Int8,
		typeVec3UInt64,
		typeVec3UInt32,
		typeVec3UInt16,
		typeVec3UInt8,

		typeVec4Int64,
		typeVec4Int32,
		typeVec4Int16,
		typeVec4Int8,
		typeVec4UInt64,
		typeVec4UInt32,
		typeVec4UInt16,
		typeVec4UInt8,

		typeVec2Float,
		typeVec2Double,
		typeVec3Float,
		typeVec3Double,
		typeVec4Float,
		typeVec4Double,

		typeQuatFloat,
		typeQuatDouble,

		typeMat3x3Float,
		typeMat3x3Double,
		typeMat4x4Float,
		typeMat4x4Double,
	} types;

	class genericError;

	/**
	 * A type-erased value that can hold any gold type: null, scalar
	 * numbers, strings, binary blobs, lists, objects, methods/functions,
	 * vectors/quaternions/matrices, or errors.
	 *
	 * `var` values are copy-on-write cheap: copies and moves share the
	 * underlying storage through a shared_ptr, so passing var by value is
	 * inexpensive.
	 *
	 * Conversions:
	 * - `var.getString()` returns the string form for every type.
	 * - The `operator string()` cast is implicit; `operator string_view()`
	 *   is explicit and only valid for string/string_view/binary data.
	 */
	struct var {
	 protected:
		struct varContainer {
		 public:
			union {
				void* data;
				object* obj;
				list* li;
				string* str;
				string_view* sv;
				genericError* err;
				method* me;
				func* fu;
				binary* bin;
				double* d;
				float* f;
				uint64_t* u64;
				uint32_t* u32;
				uint16_t* u16;
				uint8_t* u8;
				int64_t* i64;
				int32_t* i32;
				int16_t* i16;
				int8_t* i8;
				bool* b;
			};
			types type;
			~varContainer();

			varContainer();
			// varContainer owns its payload and must not be copied:
			// sharing is done through var's shared_ptr (varPtr).
			varContainer(const varContainer&) = delete;
			varContainer& operator=(const varContainer&) = delete;
		};
		typedef shared_ptr<varContainer> varPtr;
		static varPtr autoNull;
		varPtr sPtr;

	 public:
		var();
		var(const var& copy);
		var(var&& move);
		var(const char* string);
		var(string string);
		var(const binary& bin);
		var(string_view view);
		var(int64_t v);
		var(int32_t v);
		var(int16_t v);
		var(int8_t v);
		var(uint64_t v);
		var(uint32_t v);
		var(uint16_t v);
		var(uint8_t v);
		var(double v);
		var(float v);
		var(const list& v);
		var(const object& v);
		var(void* v, types t);
		var(method v);
		var(func v);
		var(const genericError& v);
		var(bool v);
		var(types t, initializer_list<float>);
		var(types t, initializer_list<double>);
		var(types t, initializer_list<int64_t>);
		var(types t, initializer_list<int32_t>);
		var(types t, initializer_list<int16_t>);
		var(types t, initializer_list<int8_t>);
		var(types t, initializer_list<uint64_t>);
		var(types t, initializer_list<uint32_t>);
		var(types t, initializer_list<uint16_t>);
		var(types t, initializer_list<uint8_t>);
		template <class T> var(T* v, types t) : var((void*)v, t) {}
		~var();

		var& operator=(const var& rhs);
		var& operator=(var&& rhs);
		bool operator==(const var& rhs) const;
		bool operator!=(const var& rhs) const;
		bool operator<=(const var& rhs) const;
		bool operator>=(const var& rhs) const;
		bool operator<(const var& rhs) const;
		bool operator>(const var& rhs) const;

		var operator-() const;
		var operator+(const var& b) const;
		var operator-(const var& b) const;
		var operator*(const var& b) const;
		var operator/(const var& b) const;
		var operator%(const var& b) const;

		types getType() const;
		const char* getTypeString() const;

		bool isString() const;
		bool isView() const;
		bool isNumber() const;
		bool isFloating() const;
		bool isSigned() const;
		bool isBool() const;
		bool isObject() const;
		bool isObject(object& proto) const;
		bool isList() const;
		bool isEmpty() const;
		bool isError() const;
		bool isFunction() const;
		bool isMethod() const;
		bool isBinary() const;
		bool isVec2() const;
		bool isVec3() const;
		bool isVec4() const;
		bool isQuat() const;
		bool isMat3x3() const;
		bool isMat4x4() const;

		void setInt64(size_t i, int64_t v);
		void setInt32(size_t i, int32_t v);
		void setInt16(size_t i, int16_t v);
		void setInt8(size_t i, int8_t v);
		void setUInt64(size_t i, uint64_t v);
		void setUInt32(size_t i, uint32_t v);
		void setUInt16(size_t i, uint16_t v);
		void setUInt8(size_t i, uint8_t v);
		void setDouble(size_t i, double v);
		void setFloat(size_t i, float v);

		string getString() const;
		string_view getStringView() const;
		int64_t getInt64(size_t i = 0) const;
		int32_t getInt32(size_t i = 0) const;
		int16_t getInt16(size_t i = 0) const;
		int8_t getInt8(size_t i = 0) const;
		uint64_t getUInt64(size_t i = 0) const;
		uint32_t getUInt32(size_t i = 0) const;
		uint16_t getUInt16(size_t i = 0) const;
		uint8_t getUInt8(size_t i = 0) const;
		double getDouble(size_t i = 0) const;
		float getFloat(size_t i = 0) const;
		bool getBool(size_t i = 0) const;
		list getList() const;
		object getObject() const;
		void assignList(list& result) const;
		void assignObject(object& result) const;
		method getMethod() const;
		func getFunction() const;
		void* getPtr() const;
		genericError* getError() const;
		binary getBinary() const;
		void assignBinary(binary& result) const;
		template <typename OT = object>
		void assignObject(OT& result) const {
			auto container = sPtr.get();
			if (container && container->type == typeObject)
				result = *reinterpret_cast<OT*>(container->data);
		}
		template <typename OT = object> OT getObject() const {
			auto container = sPtr.get();
			if (container && container->type == typeObject)
				return *reinterpret_cast<OT*>(container->data);
			return OT();
		}

		operator string() const;
		explicit operator string_view() const;
		operator int64_t() const;
		operator int32_t() const;
		operator int16_t() const;
		operator int8_t() const;
		operator uint64_t() const;
		operator uint32_t() const;
		operator uint16_t() const;
		operator uint8_t() const;
		operator double() const;
		operator float() const;
		operator bool() const;
		operator method() const;
		operator func() const;
		operator void*() const;
		operator genericError*() const;
		friend ostream& operator<<(ostream& os, const var& v);
		var operator()(object, list) const;
		var operator()(object) const;
		var operator()(list) const;
	};

	/* <List>
	 * A dynamic array of `var` values with JSON/BSON/CBOR/MsgPack/UBJSON
	 * serialization. Items are addressed by index; out-of-range reads return
	 * the caller-supplied default (or null).
	 *
	 * Thread-safety: every individual method (push/get/set/operator[]) locks
	 * the underlying storage, so concurrent calls are safe. Iteration via
	 * begin()/end() returns raw vector iterators and is NOT synchronized:
	 * you must not mutate the list (or have another thread mutate it) while
	 * iterating. For a safe read, copy items into a local list first.
	 */
	struct list {
	 protected:
		friend struct var;
		typedef std::vector<var> avec;
		struct arrData {
			avec items;
			mutex amutex;
		};
		shared_ptr<arrData> data;

		void pushData(void* dataPtr, uint64_t size, types type);
		void initMemory();

	 public:
		static uint32_t getColor(list args);
		static var getVec2f(list args);

		typedef initializer_list<avec::value_type> initList;
		typedef avec::iterator iterator;

		list();
		list(const list& copy);
		list(list&& move);
		list(initList list);
		list(var value);
		~list();
		list& operator=(const list& rhs);
		list& operator=(list&& rhs);

		uint64_t size();
		void pop();
		types getType(uint64_t index);
		string getJSON(bool pretty = false);
		binary getJSONBin(bool pretty = false);
		binary getBSON();
		binary getCBOR();
		binary getMsgPack();
		binary getUBJSON();

		bool isAllFloating() const;
		bool isAllNumber() const;
		bool isAllObject() const;
		bool isAllObject(object& proto) const;
		void assign(types t, void* target, size_t count) const;

		avec::iterator begin();
		avec::iterator end();
		avec::reverse_iterator rbegin();
		avec::reverse_iterator rend();
		list::avec::iterator erase(avec::iterator i);
		avec::iterator find(object& proto);
		avec::iterator find(object& proto, avec::iterator start);
		avec::iterator find(var item);
		avec::iterator find(var item, avec::iterator start);
		avec::iterator find(types t);
		avec::iterator find(types t, avec::iterator start);
		void resize(size_t newSize);
		void sort(function<bool(var, var)> fn);

		list operator+=(list item);
		list operator+=(var item);
		list operator-=(var item);
		operator bool() const;
		var operator[](uint64_t index) const;

		void pushString(char* value);
		void pushString(const char* value);
		void pushString(string value);
		void pushStringView(string_view value);
		void pushInt64(int64_t value);
		void pushInt32(int32_t value);
		void pushInt16(int16_t value);
		void pushInt8(int8_t value);
		void pushUInt64(uint64_t value);
		void pushUInt32(uint32_t value);
		void pushUInt16(uint16_t value);
		void pushUInt8(uint8_t value);
		void pushDouble(double value);
		void pushFloat(float value);
		void pushBool(bool value);
		void pushList(list value);
		void pushObject(object value);
		void pushMethod(method& value);
		void pushFunc(func value);
		void pushPtr(void* value);
		void pushVar(var value);
		void pushNull();

		void setString(uint64_t index, char* value);
		void setString(uint64_t index, string value);
		void setStringView(uint64_t index, string_view value);
		void setInt64(uint64_t index, int64_t value);
		void setInt32(uint64_t index, int32_t value);
		void setInt16(uint64_t index, int16_t value);
		void setInt8(uint64_t index, int8_t value);
		void setUInt64(uint64_t index, uint64_t value);
		void setUInt32(uint64_t index, uint32_t value);
		void setUInt16(uint64_t index, uint16_t value);
		void setUInt8(uint64_t index, uint8_t value);
		void setDouble(uint64_t index, double value);
		void setFloat(uint64_t index, float value);
		void setBool(uint64_t index, bool value);
		void setList(uint64_t index, list value);
		void setObject(uint64_t index, object value);
		void setFunc(uint64_t index, func& value);
		void setMethod(uint64_t index, method& value);
		void setPtr(uint64_t index, void* value);
		void setVar(uint64_t index, var value);
		void setNull(uint64_t index);

		string getString(uint64_t index, string def = "");
		string_view getStringView(
			uint64_t index, string_view def = string_view());
		int64_t getInt64(uint64_t index, int64_t def = 0);
		int32_t getInt32(uint64_t index, int32_t def = 0);
		int16_t getInt16(uint64_t index, int16_t def = 0);
		int8_t getInt8(uint64_t index, int8_t def = 0);
		uint64_t getUInt64(uint64_t index, uint64_t def = 0);
		uint32_t getUInt32(uint64_t index, uint32_t def = 0);
		uint16_t getUInt16(uint64_t index, uint16_t def = 0);
		uint8_t getUInt8(uint64_t index, uint8_t def = 0);
		double getDouble(uint64_t index, double def = 0);
		float getFloat(uint64_t index, float def = 0);
		bool getBool(uint64_t index, bool def = false);
		void assignList(uint64_t index, list& result);
		list getList(uint64_t index, list def = list());
		object getObject(uint64_t index, object def);
		method getMethod(uint64_t index, method def = 0);
		func getFunction(uint64_t index, func def = 0);
		void assignObject(uint64_t index, object& result);
		void* getPtr(uint64_t index, void* def = 0);
		var getVar(uint64_t index);

		template <typename OT = object>
		void assignObject(uint64_t index, OT& result) {
			assignObject(index, (object&)result);
		}
		template <typename OT = object>
		OT getObject(uint64_t index, OT def = OT());
	};
	using array = list;

	/* </List> */
	/* <Object>
	 * An ordered map of named `var` values with prototype-based inheritance
	 * (setParent/inherits) and JSON/BSON/CBOR/MsgPack/UBJSON serialization.
	 * Values can also be addressed with expression strings such as
	 * "child[0].name".
	 */
	struct object;
	struct objData;
	struct object {
	 protected:
		friend struct var;
		friend struct objData;
		typedef map<key, var> omap;
		typedef shared_ptr<struct objData> ptr;
		ptr data;

		static ptr newObjData(omap m = {}, uint64_t id = 0);
		static ptr newObjData(
			object& p, omap m = {}, uint64_t id = 0);

		void initMemory();
		void findParent();
		template <typename T>
		void setExpression(string name, T value);
		var getExpression(string name);

	 public:
		typedef initializer_list<omap::value_type> initList;
		object(const object& copy);
		object(object&& move);
		object(initList copy);
		object(var value);
		object();
		~object();

		omap::iterator begin();
		omap::iterator end();

		uint64_t refs() const;
		uint64_t size() const;
		types getType(string name);
		var callMethod(string name);
		var callMethod(string name, list args);
		void copy(object& other);
		void empty();
		void setParent(object other);
		object getParent();
		bool inherits(const object other) const;

		void setString(string name, string value);
		void setStringView(string name, string_view value);
		void setInt64(string name, int64_t value);
		void setInt32(string name, int32_t value);
		void setInt16(string name, int16_t value);
		void setInt8(string name, int8_t value);
		void setUInt64(string name, uint64_t value);
		void setUInt32(string name, uint32_t value);
		void setUInt16(string name, uint16_t value);
		void setUInt8(string name, uint8_t value);
		void setDouble(string name, double value);
		void setFloat(string name, float value);
		void setBool(string name, bool value);
		void setList(string name, list value);
		void setObject(string name, object value);
		void setMethod(string name, method& value);
		void setFunc(string name, func& value);
		void setPtr(string name, void* value);
		void setBinary(string name, binary value);
		void setVar(string name, var var);
		void setNull(string name);
		void erase(string name);

		string getString(string name, string def = "");
		string_view getStringView(
			string name, string_view def = string_view());
		int64_t getInt64(string name, int64_t def = 0);
		int32_t getInt32(string name, int32_t def = 0);
		int16_t getInt16(string name, int16_t def = 0);
		int8_t getInt8(string name, int8_t def = 0);
		uint64_t getUInt64(string name, uint64_t def = 0);
		uint32_t getUInt32(string name, uint32_t def = 0);
		uint16_t getUInt16(string name, uint16_t def = 0);
		uint8_t getUInt8(string name, uint8_t def = 0);
		double getDouble(string name, double def = 0);
		float getFloat(string name, float def = 0);
		bool getBool(string name, bool def = false);
		list getList(string name, list def = list());
		void assignList(string name, list& result);
		void assignObject(string name, object& result);
		object getObject(string name, object def = object());
		method getMethod(string name);
		func getFunc(string name);
		void* getPtr(string name, void* def = 0);
		binary getBinary(string name, binary def = binary());
		void assignBinary(string name, binary& result);
		var getVar(string name);
		template <typename OT = object>
		void assignObject(string name, OT& result) {
			assignObject(name, (object&)result);
		}
		template <typename OT = object>
		OT getObject(string name, OT def = OT()) {
			auto obj = getObject(name);
			if (obj) return *reinterpret_cast<OT*>(&obj);
			return def;
		}

		var operator[](string name);
		var operator->*(string name);
		var operator()(string name, list);
		var operator()(string name);
		bool operator==(object&);
		object& operator=(const object&);
		object& operator=(object&&);
		operator bool() const;

		static void parseURLEncoded(string value, object& result);
		static void parseCookie(string value, object& result);

		string getCookieString();

		string getJSON(bool pretty = false);
		binary getJSONBin(bool pretty = false);
		static var loadJSON(string path);
		static var saveJSON(string path, object value);

		binary getBSON();
		static var loadBSON(string path);
		static var saveBSON(string path, object value);

		binary getCBOR();
		static var loadCBOR(string path);
		static var saveCBOR(string path, object value);

		binary getMsgPack();
		static var loadMsgPack(string path);
		static var saveMsgPack(string path, object value);

		binary getUBJSON();
		static var loadUBJSON(string path);
		static var saveUBJSON(string path, object value);

		template <class oT = object>
		oT create(string name, object config = object()) {
			auto o = config ? oT(config) : oT();
			setObject(name, o);
			return o;
		}
		static var generateHash(string value, string salt);
	};
	using obj = object;

	template <typename OT>
	OT list::getObject(uint64_t index, OT def) {
		auto obj = getObject(index, (object)def);
		return OT(*reinterpret_cast<OT*>(&obj));
	}

	/* </Object> */

	/** An error value: carries a message plus file/function/line of origin.
	 * Returned (rather than thrown) by most gold APIs as a `var`; check with
	 * `var::isError()` / `var::getError()`. */
	class genericError : public exception, public object {
	 public:
		genericError(const genericError& copy);
		genericError(
			string_view message,
			const char* _file = __FILE__,
			const char* _func = __FUNCTION__,
			const int _line = __LINE__);
		operator string() const;
		friend ostream& operator<<(ostream& os, genericError& dt);
	};

	bool isCopyable(types type);
	const char* getTypeString(types type);

	/* </Types> */

	var vec2i64(int64_t x, int64_t y);
	var vec3i64(int64_t x, int64_t y, int64_t z);
	var vec4i64(int64_t x, int64_t y, int64_t z, int64_t w);
	var vec2i32(int32_t x, int32_t y);
	var vec3i32(int32_t x, int32_t y, int32_t z);
	var vec4i32(int32_t x, int32_t y, int32_t z, int32_t w);
	var vec2i16(int16_t x, int16_t y);
	var vec3i16(int16_t x, int16_t y, int16_t z);
	var vec4i16(int16_t x, int16_t y, int16_t z, int16_t w);
	var vec2i8(int8_t x, int8_t y);
	var vec3i8(int8_t x, int8_t y, int8_t z);
	var vec4i8(int8_t x, int8_t y, int8_t z, int8_t w);
	var vec2u64(uint64_t x, uint64_t y);
	var vec3u64(uint64_t x, uint64_t y, uint64_t z);
	var vec4u64(uint64_t x, uint64_t y, uint64_t z, uint64_t w);
	var vec2u32(uint32_t x, uint32_t y);
	var vec3u32(uint32_t x, uint32_t y, uint32_t z);
	var vec4u32(uint32_t x, uint32_t y, uint32_t z, uint32_t w);
	var vec2u16(uint16_t x, uint16_t y);
	var vec3u16(uint16_t x, uint16_t y, uint16_t z);
	var vec4u16(uint16_t x, uint16_t y, uint16_t z, uint16_t w);
	var vec2u8(uint8_t x, uint8_t y);
	var vec3u8(uint8_t x, uint8_t y, uint8_t z);
	var vec4u8(uint8_t x, uint8_t y, uint8_t z, uint8_t w);
	var vec2f(float x, float y);
	var vec3f(float x, float y, float z);
	var vec4f(float x, float y, float z, float w);
	var vec2d(double x, double y);
	var vec3d(double x, double y, double z);
	var vec4d(double x, double y, double z, double w);
	var quatf(float x, float y, float z, float w);
	var quatd(double x, double y, double z, double w);
	var mat3x3f(initializer_list<float> list);
	var mat3x3d(initializer_list<double> list);
	var mat4x4f(initializer_list<float> list);
	var mat4x4d(initializer_list<double> list);

	var lookAt(var eye, var at);
	var projection(
		var fov, var ratio, var near, var far, var homo);

	list explode(string v, list chars);

}  // namespace gold
