#include <bx/math.h>

#include <cmath>
#include <cstring>
#include <string>

#include "types.hpp"
#include "var_ops.hpp"

namespace gold {
	using namespace std;

	struct objData {
		object::omap items;
		gold::object parent;
		uint64_t id;
		mutex omutex;
	};

#define deleteType(typeT, address) \
	case typeT: {                    \
		if (address) delete address;   \
	} break

#define deleteTypeB(typeT, address) \
	case typeT: {                     \
		if (address) delete[] address;  \
	} break

	var::varContainer::~varContainer() {
		switch (type) {
			deleteType(typeException, err);
			deleteType(typeList, li);
			deleteType(typeObject, obj);
			deleteType(typeString, str);
			deleteType(typeBinary, bin);
			deleteType(typeMethod, me);
			deleteType(typeFunction, fu);
			deleteTypeB(typeVec2Float, f);
			deleteTypeB(typeVec2Double, d);
			deleteTypeB(typeVec2Int64, i64);
			deleteTypeB(typeVec2Int32, i32);
			deleteTypeB(typeVec2Int16, i16);
			deleteTypeB(typeVec2Int8, i8);
			deleteTypeB(typeVec2UInt64, u64);
			deleteTypeB(typeVec2UInt32, u32);
			deleteTypeB(typeVec2UInt16, u16);
			deleteTypeB(typeVec2UInt8, u8);
			deleteTypeB(typeVec3Float, f);
			deleteTypeB(typeVec3Double, d);
			deleteTypeB(typeVec3Int64, i64);
			deleteTypeB(typeVec3Int32, i32);
			deleteTypeB(typeVec3Int16, i16);
			deleteTypeB(typeVec3Int8, i8);
			deleteTypeB(typeVec3UInt64, u64);
			deleteTypeB(typeVec3UInt32, u32);
			deleteTypeB(typeVec3UInt16, u16);
			deleteTypeB(typeVec3UInt8, u8);
			deleteTypeB(typeVec4Float, f);
			deleteTypeB(typeVec4Double, d);
			deleteTypeB(typeVec4Int64, i64);
			deleteTypeB(typeVec4Int32, i32);
			deleteTypeB(typeVec4Int16, i16);
			deleteTypeB(typeVec4Int8, i8);
			deleteTypeB(typeVec4UInt64, u64);
			deleteTypeB(typeVec4UInt32, u32);
			deleteTypeB(typeVec4UInt16, u16);
			deleteTypeB(typeVec4UInt8, u8);
			deleteTypeB(typeQuatFloat, f);
			deleteTypeB(typeQuatDouble, d);
			deleteTypeB(typeMat3x3Float, f);
			deleteTypeB(typeMat3x3Double, d);
			deleteTypeB(typeMat4x4Float, f);
			deleteTypeB(typeMat4x4Double, d);
			default:
				break;
		}
		data = nullptr;
		type = typeNull;
	}

	var::varContainer::varContainer() {
		data = nullptr;
		type = typeNull;
	}

	var::varPtr var::autoNull =
		var::varPtr(nullptr, [](void*) {});

	var::var() : sPtr(autoNull) {}

	var::var(const var& copy) : sPtr(copy.sPtr) {}

	var::var(var&& move) : sPtr(std::move(move.sPtr)) {
		move.sPtr = autoNull;
	}

#define initVar(v, t, typeName)  \
	auto ptr = new varContainer(); \
	ptr->data = new typeName(v);   \
	ptr->type = t;                 \
	sPtr = varPtr(ptr)

// Scalar values are stored inline in the container (no second allocation).
// The pointer member is set to point at the inline storage so existing
// accessors (`*con->i64`, etc.) keep working unchanged.
#define initScalar(v, t, member, ptrMember) \
	auto ptr = new varContainer();           \
	ptr->ptrMember = &ptr->member;           \
	ptr->member = v;                         \
	ptr->type = t;                           \
	sPtr = varPtr(ptr)

	var::var(void* v, types t) {
		auto ptr = new varContainer();
		ptr->data = v;
		ptr->type = t;
		sPtr = varPtr(ptr);
	}

	var::var(const char* v) { initVar(v, typeString, string); }

	var::var(string v) { initVar(v, typeString, string); }

	var::var(const binary& bin) {
		initVar((binary&)bin, typeBinary, binary);
	}

	var::var(string_view view) {
		initVar(view, typeStringView, string_view);
	}

	var::var(int64_t v) { initScalar(v, typeInt64, i64s, i64); }

	var::var(int32_t v) { initScalar(v, typeInt32, i32s, i32); }

	var::var(int16_t v) { initScalar(v, typeInt16, i16s, i16); }

	var::var(int8_t v) { initScalar(v, typeInt8, i8s, i8); }

	var::var(uint64_t v) { initScalar(v, typeUInt64, u64s, u64); }

	var::var(uint32_t v) { initScalar(v, typeUInt32, u32s, u32); }

	var::var(uint16_t v) { initScalar(v, typeUInt16, u16s, u16); }

	var::var(uint8_t v) { initScalar(v, typeUInt8, u8s, u8); }

	var::var(double v) { initScalar(v, typeDouble, ds, d); }

	var::var(float v) { initScalar(v, typeFloat, fs, f); }

	var::var(const list& v) { initVar(v, typeList, list); }

	var::var(const obj& v) { initVar(v, typeObject, obj); }

	varRef var::operator[](string_view name) {
		return varRef(*this, string(name));
	}

	var::var(method v) { initVar(v, typeMethod, method); }

	var::var(func v) { initVar(v, typeFunction, func); }

	var::var(bool v) { initScalar(v, typeBool, bs, b); }

	var::var(const genericError& v) {
		initVar(v, typeException, genericError);
	}

#define assignPtr(varType, typeName, target, max, initList) \
	case varType: {                                           \
		ptr->data = new typeName[max];                          \
		for (size_t i = 0; i < size || i < max; ++i)            \
			ptr->target[i] = typeName(initList.begin()[i]);       \
	} break

#define vInit(typeT, initList)                             \
	auto size = initList.size();                             \
	auto ptr = new varContainer();                           \
	switch (typeT) {                                         \
		assignPtr(typeVec2Int64, int64_t, i64, 2, initList);   \
		assignPtr(typeVec2Int32, int32_t, i32, 2, initList);   \
		assignPtr(typeVec2Int16, int16_t, i16, 2, initList);   \
		assignPtr(typeVec2Int8, int8_t, i8, 2, initList);      \
		assignPtr(typeVec2UInt64, uint64_t, u64, 2, initList); \
		assignPtr(typeVec2UInt32, uint32_t, u32, 2, initList); \
		assignPtr(typeVec2UInt16, uint16_t, u16, 2, initList); \
		assignPtr(typeVec2UInt8, uint8_t, u8, 2, initList);    \
		assignPtr(typeVec2Double, double, d, 2, initList);     \
		assignPtr(typeVec2Float, float, f, 2, initList);       \
		assignPtr(typeVec3Int64, int64_t, i64, 3, initList);   \
		assignPtr(typeVec3Int32, int32_t, i32, 3, initList);   \
		assignPtr(typeVec3Int16, int16_t, i16, 3, initList);   \
		assignPtr(typeVec3Int8, int8_t, i8, 3, initList);      \
		assignPtr(typeVec3UInt64, uint64_t, u64, 3, initList); \
		assignPtr(typeVec3UInt32, uint32_t, u32, 3, initList); \
		assignPtr(typeVec3UInt16, uint16_t, u16, 3, initList); \
		assignPtr(typeVec3UInt8, uint8_t, u8, 3, initList);    \
		assignPtr(typeVec3Double, double, d, 3, initList);     \
		assignPtr(typeVec3Float, float, f, 3, initList);       \
		assignPtr(typeVec4Int64, int64_t, i64, 4, initList);   \
		assignPtr(typeVec4Int32, int32_t, i32, 4, initList);   \
		assignPtr(typeVec4Int16, int16_t, i16, 4, initList);   \
		assignPtr(typeVec4Int8, int8_t, i8, 4, initList);      \
		assignPtr(typeVec4UInt64, uint64_t, u64, 4, initList); \
		assignPtr(typeVec4UInt32, uint32_t, u32, 4, initList); \
		assignPtr(typeVec4UInt16, uint16_t, u16, 4, initList); \
		assignPtr(typeVec4UInt8, uint8_t, u8, 4, initList);    \
		assignPtr(typeVec4Double, double, d, 4, initList);     \
		assignPtr(typeVec4Float, float, f, 4, initList);       \
		assignPtr(typeQuatFloat, float, f, 4, initList);       \
		assignPtr(typeQuatDouble, double, d, 4, initList);     \
		assignPtr(typeMat3x3Float, float, f, 9, initList);     \
		assignPtr(typeMat3x3Double, double, d, 9, initList);   \
		assignPtr(typeMat4x4Float, float, f, 16, initList);    \
		assignPtr(typeMat4x4Double, double, d, 16, initList);  \
		default:                                               \
			break;                                               \
	}                                                        \
	ptr->type = typeT;                                       \
	sPtr = varPtr(ptr)

	template <typename T> using initList = initializer_list<T>;
	var::var(types t, initList<float> l) { vInit(t, l); }

	var::var(types t, initList<double> l) { vInit(t, l); }

	var::var(types t, initList<int64_t> l) { vInit(t, l); }

	var::var(types t, initList<int32_t> l) { vInit(t, l); }

	var::var(types t, initList<int16_t> l) { vInit(t, l); }

	var::var(types t, initList<int8_t> l) { vInit(t, l); }

	var::var(types t, initList<uint64_t> l) { vInit(t, l); }

	var::var(types t, initList<uint32_t> l) { vInit(t, l); }

	var::var(types t, initList<uint16_t> l) { vInit(t, l); }

	var::var(types t, initList<uint8_t> l) { vInit(t, l); }

	var::~var() { sPtr = nullptr; }

	var& var::operator=(const var& rhs) {
		sPtr = rhs.sPtr;
		return *this;
	}

	var& var::operator=(var&& rhs) {
		sPtr = std::move(rhs.sPtr);
		rhs.sPtr = autoNull;
		return *this;
	}

	bool var::operator==(const var& rhs) const {
		auto rCon = rhs.sPtr.get();
		if (!rCon) return isEmpty();
		switch (rCon->type) {
			case typeNull: return isEmpty();
			case typeObject: return rCon->obj->data == getObject().data;
			case typeList: return rCon->li->data == getList().data;
			case typeString: return *rCon->str == (string)*this;
			case typeDouble: return *rCon->d == (double)*this;
			case typeFloat: return *rCon->f == (float)*this;
			case typeInt64: return *rCon->i64 == (int64_t)*this;
			case typeInt32: return *rCon->i32 == (int32_t)*this;
			case typeInt16: return *rCon->i16 == (int16_t)*this;
			case typeInt8: return *rCon->i8 == (int8_t)*this;
			case typeUInt64: return *rCon->u64 == (uint64_t)*this;
			case typeUInt32: return *rCon->u32 == (uint32_t)*this;
			case typeUInt16: return *rCon->u16 == (uint16_t)*this;
			case typeUInt8: return *rCon->u8 == (uint8_t)*this;
			case typeBool: return *rCon->b == (bool)*this;
			default:
				// Vector/matrix/quat types use generic helper
				if (var_ops::vecSize(rCon->type) > 0) {
					return var_ops::equalVec(*this, rhs);
				}
				return (void*)rCon->data == (void*)*this;
		}
	}

	bool var::operator!=(const var& rhs) const {
		return !operator==(rhs);
	}

	bool var::operator<=(const var& rhs) const {
		auto rCon = rhs.sPtr.get();
		if (!rCon) return false;
		switch (rCon->type) {
			case typeNull: return isEmpty();
			case typeObject: return getObject().data <= rCon->obj->data;
			case typeList: return getList().data <= rCon->li->data;
			case typeString: return (string)*this <= *rCon->str;
			case typeDouble: return (double)*this <= *rCon->d;
			case typeFloat: return (float)*this <= *rCon->f;
			case typeInt64: return (int64_t)*this <= *rCon->i64;
			case typeInt32: return (int32_t)*this <= *rCon->i32;
			case typeInt16: return (int16_t)*this <= *rCon->i16;
			case typeInt8: return (int8_t)*this <= *rCon->i8;
			case typeUInt64: return (uint64_t)*this <= *rCon->u64;
			case typeUInt32: return (uint32_t)*this <= *rCon->u32;
			case typeUInt16: return (uint16_t)*this <= *rCon->u16;
			case typeUInt8: return (uint8_t)*this <= *rCon->u8;
			case typeBool: return (bool)*this <= *rCon->b;
			default:
				if (var_ops::vecSize(rCon->type) > 0) {
					return var_ops::lessEqualVec(*this, rhs);
				}
				return (void*)*this <= (void*)rCon->data;
		}
	}

	bool var::operator>=(const var& rhs) const {
		auto rCon = rhs.sPtr.get();
		if (!rCon) return false;
		switch (rCon->type) {
			case typeNull: return isEmpty();
			case typeObject: return getObject().data >= rCon->obj->data;
			case typeList: return getList().data >= rCon->li->data;
			case typeString: return (string)*this >= *rCon->str;
			case typeDouble: return (double)*this >= *rCon->d;
			case typeFloat: return (float)*this >= *rCon->f;
			case typeInt64: return (int64_t)*this >= *rCon->i64;
			case typeInt32: return (int32_t)*this >= *rCon->i32;
			case typeInt16: return (int16_t)*this >= *rCon->i16;
			case typeInt8: return (int8_t)*this >= *rCon->i8;
			case typeUInt64: return (uint64_t)*this >= *rCon->u64;
			case typeUInt32: return (uint32_t)*this >= *rCon->u32;
			case typeUInt16: return (uint16_t)*this >= *rCon->u16;
			case typeUInt8: return (uint8_t)*this >= *rCon->u8;
			case typeBool: return (bool)*this >= *rCon->b;
			default:
				if (var_ops::vecSize(rCon->type) > 0) {
					return var_ops::greaterEqualVec(*this, rhs);
				}
				return (void*)*this >= (void*)rCon->data;
		}
	}

	bool var::operator<(const var& rhs) const {
		auto rCon = rhs.sPtr.get();
		if (rCon) {
			switch (rCon->type) {
				case typeNull:
					return isEmpty();
				case typeObject:
					return getObject().data < rCon->obj->data;
				case typeList:
					return getList().data < rCon->li->data;
				case typeString:
					return (string) *this < *rCon->str;
				case typeDouble:
					return (double)*this < *rCon->d;
				case typeFloat:
					return (float)*this < *rCon->f;
				case typeInt64:
					return (int64_t) *this < *rCon->i64;
				case typeInt32:
					return (int32_t) *this < *rCon->i32;
				case typeInt16:
					return (int16_t) *this < *rCon->i16;
				case typeInt8:
					return (int8_t) *this < *rCon->i8;
				case typeUInt64:
					return (uint64_t) *this < *rCon->u64;
				case typeUInt32:
					return (uint32_t) *this < *rCon->u32;
				case typeUInt16:
					return (uint16_t) *this < *rCon->u16;
				case typeUInt8:
					return (uint8_t) *this < *rCon->u8;
				case typeBool:
					return (bool)*this < *rCon->b;
				case typeVec2Float:
					return getFloat(0) < rhs.getFloat(0) &&
								 getFloat(1) < rhs.getFloat(1);
				case typeVec2Double:
					return getDouble(0) < rhs.getDouble(0) &&
								 getDouble(1) < rhs.getDouble(1);
				case typeVec2Int64:
					return getInt64(0) < rhs.getInt64(0) &&
								 getInt64(1) < rhs.getInt64(1);
				case typeVec2Int32:
					return getInt32(0) < rhs.getInt32(0) &&
								 getInt32(1) < rhs.getInt32(1);
				case typeVec2Int16:
					return getInt16(0) < rhs.getInt16(0) &&
								 getInt16(1) < rhs.getInt16(1);
				case typeVec2Int8:
					return getInt8(0) < rhs.getInt8(0) &&
								 getInt8(1) < rhs.getInt8(1);
				case typeVec2UInt64:
					return getUInt64(0) < rhs.getUInt64(0) &&
								 getUInt64(1) < rhs.getUInt64(1);
				case typeVec2UInt32:
					return getUInt32(0) < rhs.getUInt32(0) &&
								 getUInt32(1) < rhs.getUInt32(1);
				case typeVec2UInt16:
					return getUInt16(0) < rhs.getUInt16(0) &&
								 getUInt16(1) < rhs.getUInt16(1);
				case typeVec2UInt8:
					return getUInt8(0) < rhs.getUInt8(0) &&
								 getUInt8(1) < rhs.getUInt8(1);
				case typeVec3Float:
					return getFloat(0) < rhs.getFloat(0) &&
								 getFloat(1) < rhs.getFloat(1) &&
								 getFloat(2) < rhs.getFloat(2);
				case typeVec3Double:
					return getDouble(0) < rhs.getDouble(0) &&
								 getDouble(1) < rhs.getDouble(1) &&
								 getDouble(2) < rhs.getDouble(2);
				case typeVec3Int64:
					return getInt64(0) < rhs.getInt64(0) &&
								 getInt64(1) < rhs.getInt64(1) &&
								 getInt64(2) < rhs.getInt64(2);
				case typeVec3Int32:
					return getInt32(0) < rhs.getInt32(0) &&
								 getInt32(1) < rhs.getInt32(1) &&
								 getInt32(2) < rhs.getInt32(2);
				case typeVec3Int16:
					return getInt16(0) < rhs.getInt16(0) &&
								 getInt16(1) < rhs.getInt16(1) &&
								 getInt16(2) < rhs.getInt16(2);
				case typeVec3Int8:
					return getInt8(0) < rhs.getInt8(0) &&
								 getInt8(1) < rhs.getInt8(1) &&
								 getInt8(2) < rhs.getInt8(2);
				case typeVec3UInt64:
					return getUInt64(0) < rhs.getUInt64(0) &&
								 getUInt64(1) < rhs.getUInt64(1) &&
								 getUInt64(2) < rhs.getUInt64(2);
				case typeVec3UInt32:
					return getUInt32(0) < rhs.getUInt32(0) &&
								 getUInt32(1) < rhs.getUInt32(1) &&
								 getUInt32(2) < rhs.getUInt32(2);
				case typeVec3UInt16:
					return getUInt16(0) < rhs.getUInt16(0) &&
								 getUInt16(1) < rhs.getUInt16(1) &&
								 getUInt16(2) < rhs.getUInt16(2);
				case typeVec3UInt8:
					return getUInt8(0) < rhs.getUInt8(0) &&
								 getUInt8(1) < rhs.getUInt8(1) &&
								 getUInt8(2) < rhs.getUInt8(2);
				case typeVec4Float:
					return getFloat(0) < rhs.getFloat(0) &&
								 getFloat(1) < rhs.getFloat(1) &&
								 getFloat(2) < rhs.getFloat(2) &&
								 getFloat(3) < rhs.getFloat(3);
				case typeVec4Double:
					return getDouble(0) < rhs.getDouble(0) &&
								 getDouble(1) < rhs.getDouble(1) &&
								 getDouble(2) < rhs.getDouble(2) &&
								 getDouble(3) < rhs.getDouble(3);
				case typeVec4Int64:
					return getInt64(0) < rhs.getInt64(0) &&
								 getInt64(1) < rhs.getInt64(1) &&
								 getInt64(2) < rhs.getInt64(2) &&
								 getInt64(3) < rhs.getInt64(3);
				case typeVec4Int32:
					return getInt32(0) < rhs.getInt32(0) &&
								 getInt32(1) < rhs.getInt32(1) &&
								 getInt32(2) < rhs.getInt32(2) &&
								 getInt32(3) < rhs.getInt32(3);
				case typeVec4Int16:
					return getInt16(0) < rhs.getInt16(0) &&
								 getInt16(1) < rhs.getInt16(1) &&
								 getInt16(2) < rhs.getInt16(2) &&
								 getInt16(3) < rhs.getInt16(3);
				case typeVec4Int8:
					return getInt8(0) < rhs.getInt8(0) &&
								 getInt8(1) < rhs.getInt8(1) &&
								 getInt8(2) < rhs.getInt8(2) &&
								 getInt8(3) < rhs.getInt8(3);
				case typeVec4UInt64:
					return getUInt64(0) < rhs.getUInt64(0) &&
								 getUInt64(1) < rhs.getUInt64(1) &&
								 getUInt64(2) < rhs.getUInt64(2) &&
								 getUInt64(3) < rhs.getUInt64(3);
				case typeVec4UInt32:
					return getUInt32(0) < rhs.getUInt32(0) &&
								 getUInt32(1) < rhs.getUInt32(1) &&
								 getUInt32(2) < rhs.getUInt32(2) &&
								 getUInt32(3) < rhs.getUInt32(3);
				case typeVec4UInt16:
					return getUInt16(0) < rhs.getUInt16(0) &&
								 getUInt16(1) < rhs.getUInt16(1) &&
								 getUInt16(2) < rhs.getUInt16(2) &&
								 getUInt16(3) < rhs.getUInt16(3);
				case typeVec4UInt8:
					return getUInt8(0) < rhs.getUInt8(0) &&
								 getUInt8(1) < rhs.getUInt8(1) &&
								 getUInt8(2) < rhs.getUInt8(2) &&
								 getUInt8(3) < rhs.getUInt8(3);
				default:
					return (void*)*this < (void*)rCon->data;
			}
		}
		return false;
	}

	bool var::operator>(const var& rhs) const {
		auto rCon = rhs.sPtr.get();
		if (rCon) {
			switch (rCon->type) {
				case typeNull:
					return isEmpty();
				case typeObject:
					return getObject().data > rCon->obj->data;
				case typeList:
					return getList().data > rCon->li->data;
				case typeString:
					return (string) *this > *rCon->str;
				case typeDouble:
					return (double)*this > *rCon->d;
				case typeFloat:
					return (float)*this > *rCon->f;
				case typeInt64:
					return (int64_t) *this > *rCon->i64;
				case typeInt32:
					return (int32_t) *this > *rCon->i32;
				case typeInt16:
					return (int16_t) *this > *rCon->i16;
				case typeInt8:
					return (int8_t) *this > *rCon->i8;
				case typeUInt64:
					return (uint64_t) *this > *rCon->u64;
				case typeUInt32:
					return (uint32_t) *this > *rCon->u32;
				case typeUInt16:
					return (uint16_t) *this > *rCon->u16;
				case typeUInt8:
					return (uint8_t) *this > *rCon->u8;
				case typeBool:
					return (bool)*this > *rCon->b;
				case typeVec2Float:
					return getFloat(0) > rhs.getFloat(0) &&
								 getFloat(1) > rhs.getFloat(1);
				case typeVec2Double:
					return getDouble(0) > rhs.getDouble(0) &&
								 getDouble(1) > rhs.getDouble(1);
				case typeVec2Int64:
					return getInt64(0) > rhs.getInt64(0) &&
								 getInt64(1) > rhs.getInt64(1);
				case typeVec2Int32:
					return getInt32(0) > rhs.getInt32(0) &&
								 getInt32(1) > rhs.getInt32(1);
				case typeVec2Int16:
					return getInt16(0) > rhs.getInt16(0) &&
								 getInt16(1) > rhs.getInt16(1);
				case typeVec2Int8:
					return getInt8(0) > rhs.getInt8(0) &&
								 getInt8(1) > rhs.getInt8(1);
				case typeVec2UInt64:
					return getUInt64(0) > rhs.getUInt64(0) &&
								 getUInt64(1) > rhs.getUInt64(1);
				case typeVec2UInt32:
					return getUInt32(0) > rhs.getUInt32(0) &&
								 getUInt32(1) > rhs.getUInt32(1);
				case typeVec2UInt16:
					return getUInt16(0) > rhs.getUInt16(0) &&
								 getUInt16(1) > rhs.getUInt16(1);
				case typeVec2UInt8:
					return getUInt8(0) > rhs.getUInt8(0) &&
								 getUInt8(1) > rhs.getUInt8(1);
				case typeVec3Float:
					return getFloat(0) > rhs.getFloat(0) &&
								 getFloat(1) > rhs.getFloat(1) &&
								 getFloat(2) > rhs.getFloat(2);
				case typeVec3Double:
					return getDouble(0) > rhs.getDouble(0) &&
								 getDouble(1) > rhs.getDouble(1) &&
								 getDouble(2) > rhs.getDouble(2);
				case typeVec3Int64:
					return getInt64(0) > rhs.getInt64(0) &&
								 getInt64(1) > rhs.getInt64(1) &&
								 getInt64(2) > rhs.getInt64(2);
				case typeVec3Int32:
					return getInt32(0) > rhs.getInt32(0) &&
								 getInt32(1) > rhs.getInt32(1) &&
								 getInt32(2) > rhs.getInt32(2);
				case typeVec3Int16:
					return getInt16(0) > rhs.getInt16(0) &&
								 getInt16(1) > rhs.getInt16(1) &&
								 getInt16(2) > rhs.getInt16(2);
				case typeVec3Int8:
					return getInt8(0) > rhs.getInt8(0) &&
								 getInt8(1) > rhs.getInt8(1) &&
								 getInt8(2) > rhs.getInt8(2);
				case typeVec3UInt64:
					return getUInt64(0) > rhs.getUInt64(0) &&
								 getUInt64(1) > rhs.getUInt64(1) &&
								 getUInt64(2) > rhs.getUInt64(2);
				case typeVec3UInt32:
					return getUInt32(0) > rhs.getUInt32(0) &&
								 getUInt32(1) > rhs.getUInt32(1) &&
								 getUInt32(2) > rhs.getUInt32(2);
				case typeVec3UInt16:
					return getUInt16(0) > rhs.getUInt16(0) &&
								 getUInt16(1) > rhs.getUInt16(1) &&
								 getUInt16(2) > rhs.getUInt16(2);
				case typeVec3UInt8:
					return getUInt8(0) > rhs.getUInt8(0) &&
								 getUInt8(1) > rhs.getUInt8(1) &&
								 getUInt8(2) > rhs.getUInt8(2);
				case typeVec4Float:
					return getFloat(0) > rhs.getFloat(0) &&
								 getFloat(1) > rhs.getFloat(1) &&
								 getFloat(2) > rhs.getFloat(2) &&
								 getFloat(3) > rhs.getFloat(3);
				case typeVec4Double:
					return getDouble(0) > rhs.getDouble(0) &&
								 getDouble(1) > rhs.getDouble(1) &&
								 getDouble(2) > rhs.getDouble(2) &&
								 getDouble(3) > rhs.getDouble(3);
				case typeVec4Int64:
					return getInt64(0) > rhs.getInt64(0) &&
								 getInt64(1) > rhs.getInt64(1) &&
								 getInt64(2) > rhs.getInt64(2) &&
								 getInt64(3) > rhs.getInt64(3);
				case typeVec4Int32:
					return getInt32(0) > rhs.getInt32(0) &&
								 getInt32(1) > rhs.getInt32(1) &&
								 getInt32(2) > rhs.getInt32(2) &&
								 getInt32(3) > rhs.getInt32(3);
				case typeVec4Int16:
					return getInt16(0) > rhs.getInt16(0) &&
								 getInt16(1) > rhs.getInt16(1) &&
								 getInt16(2) > rhs.getInt16(2) &&
								 getInt16(3) > rhs.getInt16(3);
				case typeVec4Int8:
					return getInt8(0) > rhs.getInt8(0) &&
								 getInt8(1) > rhs.getInt8(1) &&
								 getInt8(2) > rhs.getInt8(2) &&
								 getInt8(3) > rhs.getInt8(3);
				case typeVec4UInt64:
					return getUInt64(0) > rhs.getUInt64(0) &&
								 getUInt64(1) > rhs.getUInt64(1) &&
								 getUInt64(2) > rhs.getUInt64(2) &&
								 getUInt64(3) > rhs.getUInt64(3);
				case typeVec4UInt32:
					return getUInt32(0) > rhs.getUInt32(0) &&
								 getUInt32(1) > rhs.getUInt32(1) &&
								 getUInt32(2) > rhs.getUInt32(2) &&
								 getUInt32(3) > rhs.getUInt32(3);
				case typeVec4UInt16:
					return getUInt16(0) > rhs.getUInt16(0) &&
								 getUInt16(1) > rhs.getUInt16(1) &&
								 getUInt16(2) > rhs.getUInt16(2) &&
								 getUInt16(3) > rhs.getUInt16(3);
				case typeVec4UInt8:
					return getUInt8(0) > rhs.getUInt8(0) &&
								 getUInt8(1) > rhs.getUInt8(1) &&
								 getUInt8(2) > rhs.getUInt8(2) &&
								 getUInt8(3) > rhs.getUInt8(3);
				default:
					return (void*)*this > (void*)rCon->data;
			}
		}
		return false;
	}

	var var::operator-() const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeFloat:
					return var(-(*con->f));
				case typeDouble:
					return var(-(*con->d));
				case typePtr:
					return var(-(int64_t)(void*)con->data);
				case typeInt64:
					return var(-(*con->i64));
				case typeInt32:
					return var(-(*con->i32));
				case typeInt16:
					return var(-(*con->i16));
				case typeInt8:
					return var(-(*con->i8));
				case typeUInt64:
					return var(-int64_t(*con->u64));
				case typeUInt32:
					return var(-int32_t(*con->u32));
				case typeUInt16:
					return var(-int16_t(*con->u16));
				case typeUInt8:
					return var(-int8_t(*con->u8));
				case typeBool:
					return var(-int8_t(*con->b));

				case typeVec2Int64:
					return var(
						typeVec2Int64, {-con->i64[0], -con->i64[1]});
				case typeVec2Int32:
					return var(
						typeVec2Int32, {-con->i32[0], -con->i32[1]});
				case typeVec2Int16:
					return var(
						typeVec2Int16, {-con->i16[0], -con->i16[1]});
				case typeVec2Int8:
					return var(typeVec2Int8, {-con->i8[0], -con->i8[1]});
				case typeVec2UInt64:
					return var(
						typeVec2Int64,
						{-int64_t(con->i64[0]), -int64_t(con->i64[1])});
				case typeVec2UInt32:
					return var(
						typeVec2Int32,
						{-int32_t(con->i32[0]), -int32_t(con->i32[1])});
				case typeVec2UInt16:
					return var(
						typeVec2Int16,
						{-int16_t(con->i16[0]), -int16_t(con->i16[1])});
				case typeVec2UInt8:
					return var(
						typeVec2Int8,
						{-int8_t(con->i8[0]), -int8_t(con->i8[1])});

				case typeVec3Int64:
					return var(
						typeVec3Int64,
						{-con->i64[0], -con->i64[1], -con->i64[2]});
				case typeVec3Int32:
					return var(
						typeVec3Int32,
						{-con->i32[0], -con->i32[1], -con->i32[2]});
				case typeVec3Int16:
					return var(
						typeVec3Int16,
						{-con->i16[0], -con->i16[1], -con->i16[2]});
				case typeVec3Int8:
					return var(
						typeVec3Int8,
						{-con->i8[0], -con->i8[1], -con->i8[2]});
				case typeVec3UInt64:
					return var(
						typeVec3Int64,
						{-int64_t(con->i64[0]), -int64_t(con->i64[1]),
						 -int64_t(con->i64[2])});
				case typeVec3UInt32:
					return var(
						typeVec3Int32,
						{-int32_t(con->i32[0]), -int32_t(con->i32[1]),
						 -int32_t(con->i32[2])});
				case typeVec3UInt16:
					return var(
						typeVec3Int16,
						{-int16_t(con->i16[0]), -int16_t(con->i16[1]),
						 -int16_t(con->i16[2])});
				case typeVec3UInt8:
					return var(
						typeVec3Int8,
						{-int8_t(con->i8[0]), -int8_t(con->i8[1]),
						 -int8_t(con->i8[2])});

				case typeVec4Int64:
					return var(
						typeVec4Int64,
						{-con->i64[0], -con->i64[1], -con->i64[2],
						 -con->i64[3]});
				case typeVec4Int32:
					return var(
						typeVec4Int32,
						{-con->i32[0], -con->i32[1], -con->i32[2],
						 -con->i32[3]});
				case typeVec4Int16:
					return var(
						typeVec4Int16,
						{-con->i16[0], -con->i16[1], -con->i16[2],
						 -con->i16[3]});
				case typeVec4Int8:
					return var(
						typeVec4Int8,
						{-con->i8[0], -con->i8[1], -con->i8[2],
						 -con->i8[3]});
				case typeVec4UInt64:
					return var(
						typeVec4Int64,
						{-int64_t(con->i64[0]), -int64_t(con->i64[1]),
						 -int64_t(con->i64[2]), -int64_t(con->i64[3])});
				case typeVec4UInt32:
					return var(
						typeVec4Int32,
						{-int32_t(con->i32[0]), -int32_t(con->i32[1]),
						 -int32_t(con->i32[2]), -int32_t(con->i32[3])});
				case typeVec4UInt16:
					return var(
						typeVec4Int16,
						{-int16_t(con->i16[0]), -int16_t(con->i16[1]),
						 -int16_t(con->i16[2]), -int16_t(con->i16[3])});
				case typeVec4UInt8:
					return var(
						typeVec4Int8,
						{-int8_t(con->i8[0]), -int8_t(con->i8[1]),
						 -int8_t(con->i8[2]), -int8_t(con->i8[3])});

				case typeQuatFloat:
					return var(
						typeQuatFloat,
						{-con->f[0], -con->f[1], -con->f[2], -con->f[3]});
				case typeQuatDouble:
					return var(
						typeQuatDouble,
						{-con->d[0], -con->d[1], -con->d[2], -con->d[3]});

				case typeMat3x3Float:
					return var(
						typeMat3x3Float,
						{-con->f[0], -con->f[1], -con->f[2], -con->f[3],
						 -con->f[4], -con->f[5], -con->f[6], -con->f[7],
						 -con->f[8]});
				case typeMat3x3Double:
					return var(
						typeMat3x3Double,
						{-con->d[0], -con->d[1], -con->d[2], -con->d[3],
						 -con->d[4], -con->d[5], -con->d[6], -con->d[7],
						 -con->d[8]});
				case typeMat4x4Float:
					return var(
						typeMat4x4Float,
						{-con->f[0], -con->f[1], -con->f[2], -con->f[3],
						 -con->f[4], -con->f[5], -con->f[6], -con->f[7],
						 -con->f[8], -con->f[9], -con->f[10], -con->f[11],
						 -con->f[12], -con->f[13], -con->f[14],
						 -con->f[15]});
				case typeMat4x4Double:
					return var(
						typeMat4x4Double,
						{-con->d[0], -con->d[1], -con->d[2], -con->d[3],
						 -con->d[4], -con->d[5], -con->d[6], -con->d[7],
						 -con->d[8], -con->d[9], -con->d[10], -con->d[11],
						 -con->d[12], -con->d[13], -con->d[14],
						 -con->d[15]});

				default:
					break;
			}
		}
		return 0;
	}

	var var::operator+(const var& b) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeFloat:
					return var(b.getFloat() + getFloat());
				case typeDouble:
					return var(b.getDouble() + getDouble());
				case typePtr:
					return var((void*)(getInt64() + b.getInt64()), typePtr);
				case typeInt64:
					return var(b.getInt64() + getInt64());
				case typeInt32:
					return var(b.getInt32() + getInt32());
				case typeInt16:
					return var(b.getInt16() + getInt16());
				case typeInt8:
					return var(b.getInt8() + getInt8());
				case typeUInt64:
					return var(b.getUInt64() + getUInt64());
				case typeUInt32:
					return var(b.getUInt32() + getUInt32());
				case typeUInt16:
					return var(b.getUInt16() + getUInt16());
				case typeUInt8:
					return var(b.getUInt8() + getUInt8());
				case typeBool:
					return var(bool(b.getInt8() + getInt8()));
				case typeVec2Int64:
					return var(
						typeVec2Int64,
						{getInt64(0) + b.getInt64(0),
						 getInt64(1) + b.getInt64(1)});
				case typeVec2Int32:
					return var(
						typeVec2Int32,
						{getInt32(0) + b.getInt32(0),
						 getInt32(1) + b.getInt32(1)});
				case typeVec2Int16:
					return var(
						typeVec2Int16,
						{getInt16(0) + b.getInt16(0),
						 getInt16(1) + b.getInt16(1)});
				case typeVec2Int8:
					return var(
						typeVec2Int8,
						{getInt8(0) + b.getInt8(0),
						 getInt8(1) + b.getInt8(1)});
				case typeVec2UInt64:
					return var(
						typeVec2UInt64,
						{getUInt64(0) + b.getUInt64(0),
						 getUInt64(1) + b.getUInt64(1)});
				case typeVec2UInt32:
					return var(
						typeVec2UInt32,
						{getUInt32(0) + b.getUInt32(0),
						 getUInt32(1) + b.getUInt32(1)});
				case typeVec2UInt16:
					return var(
						typeVec2UInt16,
						{getUInt16(0) + b.getUInt16(0),
						 getUInt16(1) + b.getUInt16(1)});
				case typeVec2UInt8:
					return var(
						typeVec2UInt8,
						{getUInt8(0) + b.getUInt8(0),
						 getUInt8(1) + b.getUInt8(1)});
				case typeVec3Int64:
					return var(
						typeVec3Int64,
						{getInt64(0) + b.getInt64(0),
						 getInt64(1) + b.getInt64(1),
						 getInt64(2) + b.getInt64(2)});
				case typeVec3Int32:
					return var(
						typeVec3Int32,
						{getInt32(0) + b.getInt32(0),
						 getInt32(1) + b.getInt32(1),
						 getInt32(2) + b.getInt32(2)});
				case typeVec3Int16:
					return var(
						typeVec3Int16,
						{getInt16(0) + b.getInt16(0),
						 getInt16(1) + b.getInt16(1),
						 getInt16(2) + b.getInt16(2)});
				case typeVec3Int8:
					return var(
						typeVec3Int8,
						{
							getInt8(0) + b.getInt8(0),
							getInt8(1) + b.getInt8(1),
							getInt8(2) + b.getInt8(2),
						});
				case typeVec3UInt64:
					return var(
						typeVec3UInt64,
						{getUInt64(0) + b.getUInt64(0),
						 getUInt64(1) + b.getUInt64(1),
						 getUInt64(2) + b.getUInt64(2)});
				case typeVec3UInt32:
					return var(
						typeVec3UInt32,
						{getUInt32(0) + b.getUInt32(0),
						 getUInt32(1) + b.getUInt32(1),
						 getUInt32(2) + b.getUInt32(2)});
				case typeVec3UInt16:
					return var(
						typeVec3UInt16,
						{getUInt16(0) + b.getUInt16(0),
						 getUInt16(1) + b.getUInt16(1),
						 getUInt16(2) + b.getUInt16(2)});
				case typeVec3UInt8:
					return var(
						typeVec3UInt8,
						{getUInt8(0) + b.getUInt8(0),
						 getUInt8(1) + b.getUInt8(1),
						 getUInt8(2) + b.getUInt8(2)});
				case typeVec4Int64:
					return var(
						typeVec4Int64,
						{getInt64(0) + b.getInt64(0),
						 getInt64(1) + b.getInt64(1),
						 getInt64(2) + b.getInt64(2),
						 getInt64(3) + b.getInt64(3)});
				case typeVec4Int32:
					return var(
						typeVec4Int32,
						{getInt32(0) + b.getInt32(0),
						 getInt32(1) + b.getInt32(1),
						 getInt32(2) + b.getInt32(2),
						 getInt32(3) + b.getInt32(3)});
				case typeVec4Int16:
					return var(
						typeVec4Int16,
						{getInt16(0) + b.getInt16(0),
						 getInt16(1) + b.getInt16(1),
						 getInt16(2) + b.getInt16(2),
						 getInt16(3) + b.getInt16(3)});
				case typeVec4Int8:
					return var(
						typeVec4Int8,
						{
							getInt8(0) + b.getInt8(0),
							getInt8(1) + b.getInt8(1),
							getInt8(2) + b.getInt8(2),
							getInt8(3) + b.getInt8(3),
						});
				case typeVec4UInt64:
					return var(
						typeVec4UInt64,
						{getUInt64(0) + b.getUInt64(0),
						 getUInt64(1) + b.getUInt64(1),
						 getUInt64(2) + b.getUInt64(2),
						 getUInt64(3) + b.getUInt64(3)});
				case typeVec4UInt32:
					return var(
						typeVec4UInt32,
						{getUInt32(0) + b.getUInt32(0),
						 getUInt32(1) + b.getUInt32(1),
						 getUInt32(2) + b.getUInt32(2),
						 getUInt32(3) + b.getUInt32(3)});
				case typeVec4UInt16:
					return var(
						typeVec4UInt16,
						{getUInt16(0) + b.getUInt16(0),
						 getUInt16(1) + b.getUInt16(1),
						 getUInt16(2) + b.getUInt16(2),
						 getUInt16(3) + b.getUInt16(3)});
				case typeVec4UInt8:
					return var(
						typeVec4UInt8,
						{getUInt8(0) + b.getUInt8(0),
						 getUInt8(1) + b.getUInt8(1),
						 getUInt8(2) + b.getUInt8(2),
						 getUInt8(3) + b.getUInt8(3)});
				case typeQuatFloat:
					return var(
						typeQuatFloat,
						{
							getFloat(0) + b.getFloat(0),
							getFloat(1) + b.getFloat(1),
							getFloat(2) + b.getFloat(2),
							getFloat(3) + b.getFloat(3),
						});
				case typeQuatDouble:
					return var(
						typeQuatDouble,
						{
							getDouble(0) + b.getDouble(0),
							getDouble(1) + b.getDouble(1),
							getDouble(2) + b.getDouble(2),
							getDouble(3) + b.getDouble(3),
						});
				case typeMat3x3Float:
					return var(
						typeMat3x3Float,
						{
							getFloat(0) + b.getFloat(0),
							getFloat(1) + b.getFloat(1),
							getFloat(2) + b.getFloat(2),
							getFloat(3) + b.getFloat(3),
							getFloat(4) + b.getFloat(4),
							getFloat(5) + b.getFloat(5),
							getFloat(6) + b.getFloat(6),
							getFloat(7) + b.getFloat(7),
							getFloat(8) + b.getFloat(8),
						});
				case typeMat3x3Double:
					return var(
						typeMat3x3Double,
						{
							getDouble(0) + b.getDouble(0),
							getDouble(1) + b.getDouble(1),
							getDouble(2) + b.getDouble(2),
							getDouble(3) + b.getDouble(3),
							getDouble(4) + b.getDouble(4),
							getDouble(5) + b.getDouble(5),
							getDouble(6) + b.getDouble(6),
							getDouble(7) + b.getDouble(7),
							getDouble(8) + b.getDouble(8),
						});
				case typeMat4x4Float:
					if (b.isVec3()) {
						auto x = mat4x4f({
							getFloat(0),
							getFloat(1),
							getFloat(2),
							getFloat(3),
							getFloat(4),
							getFloat(5),
							getFloat(5),
							getFloat(6),
							getFloat(7),
							getFloat(8),
							getFloat(9),
							getFloat(10),
							getFloat(11),
							getFloat(15),
						});
						bx::mtxTranslate(
							(float*)x.getPtr(),
							b.getFloat(0),
							b.getFloat(1),
							b.getFloat(2));
						return x;
					}
					return var(
						typeMat4x4Float,
						{
							getFloat(0) + b.getFloat(0),
							getFloat(1) + b.getFloat(1),
							getFloat(2) + b.getFloat(2),
							getFloat(3) + b.getFloat(3),
							getFloat(4) + b.getFloat(4),
							getFloat(5) + b.getFloat(5),
							getFloat(6) + b.getFloat(6),
							getFloat(7) + b.getFloat(7),
							getFloat(8) + b.getFloat(8),
							getFloat(9) + b.getFloat(9),
							getFloat(10) + b.getFloat(10),
							getFloat(11) + b.getFloat(11),
							getFloat(12) + b.getFloat(12),
							getFloat(13) + b.getFloat(13),
							getFloat(14) + b.getFloat(14),
							getFloat(15) + b.getFloat(15),
						});
				case typeMat4x4Double:
					if (b.isVec3()) {
						auto x = mat4x4f({
							getFloat(0),
							getFloat(1),
							getFloat(2),
							getFloat(3),
							getFloat(4),
							getFloat(5),
							getFloat(5),
							getFloat(6),
							getFloat(7),
							getFloat(8),
							getFloat(9),
							getFloat(10),
							getFloat(11),
							getFloat(15),
						});
						bx::mtxTranslate(
							(float*)x.getPtr(),
							b.getFloat(0),
							b.getFloat(1),
							b.getFloat(2));
						return x;
					}
					return var(
						typeMat4x4Double,
						{
							getDouble(0) + b.getDouble(0),
							getDouble(1) + b.getDouble(1),
							getDouble(2) + b.getDouble(2),
							getDouble(3) + b.getDouble(3),
							getDouble(4) + b.getDouble(4),
							getDouble(5) + b.getDouble(5),
							getDouble(6) + b.getDouble(6),
							getDouble(7) + b.getDouble(7),
							getDouble(8) + b.getDouble(8),
							getDouble(9) + b.getDouble(9),
							getDouble(10) + b.getDouble(10),
							getDouble(11) + b.getDouble(11),
							getDouble(12) + b.getDouble(12),
							getDouble(13) + b.getDouble(13),
							getDouble(14) + b.getDouble(14),
							getDouble(15) + b.getDouble(15),
						});

				default:
					break;
			}
		}
		return 0;
	}

	var var::operator-(const var& b) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeFloat:
					return var(getFloat() - b.getFloat());
				case typeDouble:
					return var(getDouble() - b.getDouble());
				case typePtr:
					return var((void*)(getInt64() - b.getInt64()), typePtr);
				case typeInt64:
					return var(getInt64() - b.getInt64());
				case typeInt32:
					return var(getInt32() - b.getInt32());
				case typeInt16:
					return var(getInt16() - b.getInt16());
				case typeInt8:
					return var(getInt8() - b.getInt8());
				case typeUInt64:
					return var(getUInt64() - b.getUInt64());
				case typeUInt32:
					return var(getUInt32() - b.getUInt32());
				case typeUInt16:
					return var(getUInt16() - b.getUInt16());
				case typeUInt8:
					return var(getUInt8() - b.getUInt8());
				case typeBool:
					return var(bool(getInt8() - b.getInt8()));

				case typeVec2Int64:
					return var(
						typeVec2Int64,
						{getInt64(0) - b.getInt64(0),
						 getInt64(1) - b.getInt64(1)});
				case typeVec2Int32:
					return var(
						typeVec2Int32,
						{getInt32(0) - b.getInt32(0),
						 getInt32(1) - b.getInt32(1)});
				case typeVec2Int16:
					return var(
						typeVec2Int16,
						{getInt16(0) - b.getInt16(0),
						 getInt16(1) - b.getInt16(1)});
				case typeVec2Int8:
					return var(
						typeVec2Int8,
						{getInt8(0) - b.getInt8(0),
						 getInt8(1) - b.getInt8(1)});
				case typeVec2UInt64:
					return var(
						typeVec2UInt64,
						{getUInt64(0) - b.getUInt64(0),
						 getUInt64(1) - b.getUInt64(1)});
				case typeVec2UInt32:
					return var(
						typeVec2UInt32,
						{getUInt32(0) - b.getUInt32(0),
						 getUInt32(1) - b.getUInt32(1)});
				case typeVec2UInt16:
					return var(
						typeVec2UInt16,
						{getUInt16(0) - b.getUInt16(0),
						 getUInt16(1) - b.getUInt16(1)});
				case typeVec2UInt8:
					return var(
						typeVec2UInt8,
						{getUInt8(0) - b.getUInt8(0),
						 getUInt8(1) - b.getUInt8(1)});
				case typeVec3Int64:
					return var(
						typeVec3Int64,
						{getInt64(0) - b.getInt64(0),
						 getInt64(1) - b.getInt64(1),
						 getInt64(2) - b.getInt64(2)});
				case typeVec3Int32:
					return var(
						typeVec3Int32,
						{getInt32(0) - b.getInt32(0),
						 getInt32(1) - b.getInt32(1),
						 getInt32(2) - b.getInt32(2)});
				case typeVec3Int16:
					return var(
						typeVec3Int16,
						{getInt16(0) - b.getInt16(0),
						 getInt16(1) - b.getInt16(1),
						 getInt16(2) - b.getInt16(2)});
				case typeVec3Int8:
					return var(
						typeVec3Int8,
						{
							getInt8(0) - b.getInt8(0),
							getInt8(1) - b.getInt8(1),
							getInt8(2) - b.getInt8(2),
						});
				case typeVec3UInt64:
					return var(
						typeVec3UInt64,
						{getUInt64(0) - b.getUInt64(0),
						 getUInt64(1) - b.getUInt64(1),
						 getUInt64(2) - b.getUInt64(2)});
				case typeVec3UInt32:
					return var(
						typeVec3UInt32,
						{getUInt32(0) - b.getUInt32(0),
						 getUInt32(1) - b.getUInt32(1),
						 getUInt32(2) - b.getUInt32(2)});
				case typeVec3UInt16:
					return var(
						typeVec3UInt16,
						{getUInt16(0) - b.getUInt16(0),
						 getUInt16(1) - b.getUInt16(1),
						 getUInt16(2) - b.getUInt16(2)});
				case typeVec3UInt8:
					return var(
						typeVec3UInt8,
						{getUInt8(0) - b.getUInt8(0),
						 getUInt8(1) - b.getUInt8(1),
						 getUInt8(2) - b.getUInt8(2)});
				case typeVec4Int64:
					return var(
						typeVec4Int64,
						{getInt64(0) - b.getInt64(0),
						 getInt64(1) - b.getInt64(1),
						 getInt64(2) - b.getInt64(2),
						 getInt64(3) - b.getInt64(3)});
				case typeVec4Int32:
					return var(
						typeVec4Int32,
						{getInt32(0) - b.getInt32(0),
						 getInt32(1) - b.getInt32(1),
						 getInt32(2) - b.getInt32(2),
						 getInt32(3) - b.getInt32(3)});
				case typeVec4Int16:
					return var(
						typeVec4Int16,
						{getInt16(0) - b.getInt16(0),
						 getInt16(1) - b.getInt16(1),
						 getInt16(2) - b.getInt16(2),
						 getInt16(3) - b.getInt16(3)});
				case typeVec4Int8:
					return var(
						typeVec4Int8,
						{
							getInt8(0) - b.getInt8(0),
							getInt8(1) - b.getInt8(1),
							getInt8(2) - b.getInt8(2),
							getInt8(3) - b.getInt8(3),
						});
				case typeVec4UInt64:
					return var(
						typeVec4UInt64,
						{getUInt64(0) - b.getUInt64(0),
						 getUInt64(1) - b.getUInt64(1),
						 getUInt64(2) - b.getUInt64(2),
						 getUInt64(3) - b.getUInt64(3)});
				case typeVec4UInt32:
					return var(
						typeVec4UInt32,
						{getUInt32(0) - b.getUInt32(0),
						 getUInt32(1) - b.getUInt32(1),
						 getUInt32(2) - b.getUInt32(2),
						 getUInt32(3) - b.getUInt32(3)});
				case typeVec4UInt16:
					return var(
						typeVec4UInt16,
						{getUInt16(0) - b.getUInt16(0),
						 getUInt16(1) - b.getUInt16(1),
						 getUInt16(2) - b.getUInt16(2),
						 getUInt16(3) - b.getUInt16(3)});
				case typeVec4UInt8:
					return var(
						typeVec4UInt8,
						{getUInt8(0) - b.getUInt8(0),
						 getUInt8(1) - b.getUInt8(1),
						 getUInt8(2) - b.getUInt8(2),
						 getUInt8(3) - b.getUInt8(3)});

				case typeQuatFloat:
					return var(
						typeQuatFloat,
						{
							getFloat(0) - b.getFloat(0),
							getFloat(1) - b.getFloat(1),
							getFloat(2) - b.getFloat(2),
							getFloat(3) - b.getFloat(3),
						});
				case typeQuatDouble:
					return var(
						typeQuatDouble,
						{
							getDouble(0) - b.getDouble(0),
							getDouble(1) - b.getDouble(1),
							getDouble(2) - b.getDouble(2),
							getDouble(3) - b.getDouble(3),
						});
				case typeMat3x3Float:
					return var(
						typeMat3x3Float,
						{
							getFloat(0) - b.getFloat(0),
							getFloat(1) - b.getFloat(1),
							getFloat(2) - b.getFloat(2),
							getFloat(3) - b.getFloat(3),
							getFloat(4) - b.getFloat(4),
							getFloat(5) - b.getFloat(5),
							getFloat(6) - b.getFloat(6),
							getFloat(7) - b.getFloat(7),
							getFloat(8) - b.getFloat(8),
						});
				case typeMat3x3Double:
					return var(
						typeMat3x3Double,
						{
							getDouble(0) - b.getDouble(0),
							getDouble(1) - b.getDouble(1),
							getDouble(2) - b.getDouble(2),
							getDouble(3) - b.getDouble(3),
							getDouble(4) - b.getDouble(4),
							getDouble(5) - b.getDouble(5),
							getDouble(6) - b.getDouble(6),
							getDouble(7) - b.getDouble(7),
							getDouble(8) - b.getDouble(8),
						});
				case typeMat4x4Float:
					return var(
						typeMat4x4Float,
						{
							getFloat(0) - b.getFloat(0),
							getFloat(1) - b.getFloat(1),
							getFloat(2) - b.getFloat(2),
							getFloat(3) - b.getFloat(3),
							getFloat(4) - b.getFloat(4),
							getFloat(5) - b.getFloat(5),
							getFloat(6) - b.getFloat(6),
							getFloat(7) - b.getFloat(7),
							getFloat(8) - b.getFloat(8),
							getFloat(9) - b.getFloat(9),
							getFloat(10) - b.getFloat(10),
							getFloat(11) - b.getFloat(11),
							getFloat(12) - b.getFloat(12),
							getFloat(13) - b.getFloat(13),
							getFloat(14) - b.getFloat(14),
							getFloat(15) - b.getFloat(15),
						});
				case typeMat4x4Double:
					return var(
						typeMat4x4Double,
						{
							getDouble(0) - b.getDouble(0),
							getDouble(1) - b.getDouble(1),
							getDouble(2) - b.getDouble(2),
							getDouble(3) - b.getDouble(3),
							getDouble(4) - b.getDouble(4),
							getDouble(5) - b.getDouble(5),
							getDouble(6) - b.getDouble(6),
							getDouble(7) - b.getDouble(7),
							getDouble(8) - b.getDouble(8),
							getDouble(9) - b.getDouble(9),
							getDouble(10) - b.getDouble(10),
							getDouble(11) - b.getDouble(11),
							getDouble(12) - b.getDouble(12),
							getDouble(13) - b.getDouble(13),
							getDouble(14) - b.getDouble(14),
							getDouble(15) - b.getDouble(15),
						});

				default:
					break;
			}
		}
		return 0;
	}

	var var::operator*(const var& b) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeFloat:
					return var(b.getFloat() * getFloat());
				case typeDouble:
					return var(b.getDouble() * getDouble());
				case typePtr:
					return var((void*)(getInt64() * b.getInt64()), typePtr);
				case typeInt64:
					return var(b.getInt64() * getInt64());
				case typeInt32:
					return var(b.getInt32() * getInt32());
				case typeInt16:
					return var(b.getInt16() * getInt16());
				case typeInt8:
					return var(b.getInt8() * getInt8());
				case typeUInt64:
					return var(b.getUInt64() * getUInt64());
				case typeUInt32:
					return var(b.getUInt32() * getUInt32());
				case typeUInt16:
					return var(b.getUInt16() * getUInt16());
				case typeUInt8:
					return var(b.getUInt8() * getUInt8());
				case typeBool:
					return var(b.getBool() && getBool());

				case typeVec2Int64:
					return var(
						typeVec2Int64,
						{getInt64(0) * b.getInt64(0),
						 getInt64(1) * b.getInt64(1)});
				case typeVec2Int32:
					return var(
						typeVec2Int32,
						{getInt32(0) * b.getInt32(0),
						 getInt32(1) * b.getInt32(1)});
				case typeVec2Int16:
					return var(
						typeVec2Int16,
						{getInt16(0) * b.getInt16(0),
						 getInt16(1) * b.getInt16(1)});
				case typeVec2Int8:
					return var(
						typeVec2Int8,
						{getInt8(0) * b.getInt8(0),
						 getInt8(1) * b.getInt8(1)});
				case typeVec2UInt64:
					return var(
						typeVec2UInt64,
						{getUInt64(0) * b.getUInt64(0),
						 getUInt64(1) * b.getUInt64(1)});
				case typeVec2UInt32:
					return var(
						typeVec2UInt32,
						{getUInt32(0) * b.getUInt32(0),
						 getUInt32(1) * b.getUInt32(1)});
				case typeVec2UInt16:
					return var(
						typeVec2UInt16,
						{getUInt16(0) * b.getUInt16(0),
						 getUInt16(1) * b.getUInt16(1)});
				case typeVec2UInt8:
					return var(
						typeVec2UInt8,
						{getUInt8(0) * b.getUInt8(0),
						 getUInt8(1) * b.getUInt8(1)});
				case typeVec3Int64:
					return var(
						typeVec3Int64,
						{getInt64(0) * b.getInt64(0),
						 getInt64(1) * b.getInt64(1),
						 getInt64(2) * b.getInt64(2)});
				case typeVec3Int32:
					return var(
						typeVec3Int32,
						{getInt32(0) * b.getInt32(0),
						 getInt32(1) * b.getInt32(1),
						 getInt32(2) * b.getInt32(2)});
				case typeVec3Int16:
					return var(
						typeVec3Int16,
						{getInt16(0) * b.getInt16(0),
						 getInt16(1) * b.getInt16(1),
						 getInt16(2) * b.getInt16(2)});
				case typeVec3Int8:
					return var(
						typeVec3Int8,
						{
							getInt8(0) * b.getInt8(0),
							getInt8(1) * b.getInt8(1),
							getInt8(2) * b.getInt8(2),
						});
				case typeVec3UInt64:
					return var(
						typeVec3UInt64,
						{getUInt64(0) * b.getUInt64(0),
						 getUInt64(1) * b.getUInt64(1),
						 getUInt64(2) * b.getUInt64(2)});
				case typeVec3UInt32:
					return var(
						typeVec3UInt32,
						{getUInt32(0) * b.getUInt32(0),
						 getUInt32(1) * b.getUInt32(1),
						 getUInt32(2) * b.getUInt32(2)});
				case typeVec3UInt16:
					return var(
						typeVec3UInt16,
						{getUInt16(0) * b.getUInt16(0),
						 getUInt16(1) * b.getUInt16(1),
						 getUInt16(2) * b.getUInt16(2)});
				case typeVec3UInt8:
					return var(
						typeVec3UInt8,
						{getUInt8(0) * b.getUInt8(0),
						 getUInt8(1) * b.getUInt8(1),
						 getUInt8(2) * b.getUInt8(2)});
				case typeVec4Int64:
					return var(
						typeVec4Int64,
						{getInt64(0) * b.getInt64(0),
						 getInt64(1) * b.getInt64(1),
						 getInt64(2) * b.getInt64(2),
						 getInt64(3) * b.getInt64(3)});
				case typeVec4Int32:
					return var(
						typeVec4Int32,
						{getInt32(0) * b.getInt32(0),
						 getInt32(1) * b.getInt32(1),
						 getInt32(2) * b.getInt32(2),
						 getInt32(3) * b.getInt32(3)});
				case typeVec4Int16:
					return var(
						typeVec4Int16,
						{getInt16(0) * b.getInt16(0),
						 getInt16(1) * b.getInt16(1),
						 getInt16(2) * b.getInt16(2),
						 getInt16(3) * b.getInt16(3)});
				case typeVec4Int8:
					return var(
						typeVec4Int8,
						{
							getInt8(0) * b.getInt8(0),
							getInt8(1) * b.getInt8(1),
							getInt8(2) * b.getInt8(2),
							getInt8(3) * b.getInt8(3),
						});
				case typeVec4UInt64:
					return var(
						typeVec4UInt64,
						{getUInt64(0) * b.getUInt64(0),
						 getUInt64(1) * b.getUInt64(1),
						 getUInt64(2) * b.getUInt64(2),
						 getUInt64(3) * b.getUInt64(3)});
				case typeVec4UInt32:
					return var(
						typeVec4UInt32,
						{getUInt32(0) * b.getUInt32(0),
						 getUInt32(1) * b.getUInt32(1),
						 getUInt32(2) * b.getUInt32(2),
						 getUInt32(3) * b.getUInt32(3)});
				case typeVec4UInt16:
					return var(
						typeVec4UInt16,
						{getUInt16(0) * b.getUInt16(0),
						 getUInt16(1) * b.getUInt16(1),
						 getUInt16(2) * b.getUInt16(2),
						 getUInt16(3) * b.getUInt16(3)});
				case typeVec4UInt8:
					return var(
						typeVec4UInt8,
						{getUInt8(0) * b.getUInt8(0),
						 getUInt8(1) * b.getUInt8(1),
						 getUInt8(2) * b.getUInt8(2),
						 getUInt8(3) * b.getUInt8(3)});

				case typeQuatFloat:
					if (b.isVec3()) {
						auto x = bx::Vec3(
							b.getFloat(0), b.getFloat(1), b.getFloat(2));
						auto res = bx::mul(
							x,
							bx::Quaternion{
								getFloat(0), getFloat(1), getFloat(2),
								getFloat(3)});
						return var(typeVec3Float, {res.x, res.y, res.z});
					} else if (b.isQuat()) {
						auto res = bx::mul(
							bx::Quaternion{
								b.getFloat(0), b.getFloat(1), b.getFloat(2),
								b.getFloat(3)},
							bx::Quaternion{
								getFloat(0), getFloat(1), getFloat(2),
								getFloat(3)});
						return var(
							typeQuatFloat, {res.x, res.y, res.z, res.w});
					}
					return var(
						typeQuatFloat,
						{
							getFloat(0) * b.getFloat(0),
							getFloat(1) * b.getFloat(1),
							getFloat(2) * b.getFloat(2),
							getFloat(3) * b.getFloat(3),
						});
				case typeQuatDouble:
					if (b.isVec3()) {
						auto vx = b.getDouble(0), vy = b.getDouble(1);
						auto vz = b.getDouble(2), qx = getDouble(0);
						auto qy = getDouble(1), qz = getDouble(2);
						auto qw = getDouble(3);
						auto tx = 2.0 * (qy * vz - qz * vy);
						auto ty = 2.0 * (qz * vx - qx * vz);
						auto tz = 2.0 * (qx * vy - qy * vx);
						return var(typeVec3Double, {
							vx + qw * tx + qy * tz - qz * ty,
							vy + qw * ty + qz * tx - qx * tz,
							vz + qw * tz + qx * ty - qy * tx});
					} else if (b.isQuat()) {
						auto bx = b.getDouble(0), by = b.getDouble(1);
						auto bz = b.getDouble(2), bw = b.getDouble(3);
						auto qx = getDouble(0), qy = getDouble(1);
						auto qz = getDouble(2), qw = getDouble(3);
						return var(typeQuatDouble, {
							bw * qx + bx * qw + by * qz - bz * qy,
							bw * qy - bx * qz + by * qw + bz * qx,
							bw * qz + bx * qy - by * qx + bz * qw,
							bw * qw - bx * qx - by * qy - bz * qz});
					}
					return var(
						typeQuatDouble,
						{
							getDouble(0) * b.getDouble(0),
							getDouble(1) * b.getDouble(1),
							getDouble(2) * b.getDouble(2),
							getDouble(3) * b.getDouble(3),
						});
case typeMat3x3Float:
		{
			// Matrix multiplication (row-major)
			float res[9];
			for (int i = 0; i < 3; ++i) {
				int ai = i * 3;
				for (int j = 0; j < 3; ++j) {
					res[ai + j] =
						getFloat(ai + 0) * b.getFloat(0 * 3 + j) +
						getFloat(ai + 1) * b.getFloat(1 * 3 + j) +
						getFloat(ai + 2) * b.getFloat(2 * 3 + j);
				}
			}
			return var(typeMat3x3Float, {res[0], res[1], res[2],
				       res[3], res[4], res[5],
				       res[6], res[7], res[8]});
		}
case typeMat3x3Double:
		{
			// Matrix multiplication (row-major)
			double res[9];
			for (int i = 0; i < 3; ++i) {
				int ai = i * 3;
				for (int j = 0; j < 3; ++j) {
					res[ai + j] =
						getDouble(ai + 0) * b.getDouble(0 * 3 + j) +
						getDouble(ai + 1) * b.getDouble(1 * 3 + j) +
						getDouble(ai + 2) * b.getDouble(2 * 3 + j);
				}
			}
			return var(typeMat3x3Double, {res[0], res[1], res[2],
				       res[3], res[4], res[5],
				       res[6], res[7], res[8]});
		}
				case typeMat4x4Float:
					if (b.isVec3()) {
						auto x = mat4x4f(
							{getFloat(0), getFloat(1), getFloat(2),
							 getFloat(3), getFloat(4), getFloat(5),
							 getFloat(5), getFloat(6), getFloat(7),
							 getFloat(8), getFloat(9), getFloat(10),
							 getFloat(11), getFloat(15)});
						bx::mtxScale(
							(float*)x.getPtr(),
							b.getFloat(0),
							b.getFloat(1),
							b.getFloat(2));
						return x;
					} else if (b.isVec4()) {
						auto x = vec4f(0, 0, 0, 0);
						float y[4] = {
							b.getFloat(0), b.getFloat(1), b.getFloat(2),
							b.getFloat(3)};
						bx::vec4MulMtx(
							(float*)x.getPtr(), y, (float*)getPtr());
						return x;
					} else if (b.isMat4x4()) {
						auto x = mat4x4f({});
						bx::mtxMul(
							(float*)x.getPtr(),
							(float*)getPtr(),
							(float*)b.getPtr());
						return x;
					} else if (b.isQuat()) {
						auto x = mat4x4f(
							{getFloat(0), getFloat(1), getFloat(2),
							 getFloat(3), getFloat(4), getFloat(5),
							 getFloat(5), getFloat(6), getFloat(7),
							 getFloat(8), getFloat(9), getFloat(10),
							 getFloat(11), getFloat(15)});
bx::mtxFromQuaternion(
						(float*)x.getPtr(),
						bx::Quaternion(
							{b.getFloat(0), b.getFloat(1), b.getFloat(2),
							 b.getFloat(3)}));
						return x;
					}
					break;
case typeMat4x4Double:
		{
			// Matrix multiplication and scaling for double matrices
			if (b.isVec3()) {
				// Scaling matrix (row‑major)
				double res[16] = {1.0, 0.0, 0.0, 0.0,
					    0.0, 1.0, 0.0, 0.0,
					    0.0, 0.0, 1.0, 0.0,
					    0.0, 0.0, 0.0, 1.0};
				res[0] *= b.getDouble(0);
				res[5] *= b.getDouble(1);
				res[10] *= b.getDouble(2);
				return var(typeMat4x4Double, {res[0], res[1], res[2], res[3],
				       res[4], res[5], res[6], res[7],
				       res[8], res[9], res[10], res[11],
				       res[12], res[13], res[14], res[15]});
			}
			else if (b.isVec4()) {
				// Vector multiplication
				const double y[4] = { b.getDouble(0), b.getDouble(1), b.getDouble(2), b.getDouble(3) };
				double res[4] = {0,0,0,0};
				for (int i = 0; i < 4; ++i) {
					res[i] = 0;
					for (int j = 0; j < 4; ++j)
						res[i] += getDouble(i*4 + j) * y[j];
				}
				return var(typeVec4Double, {res[0], res[1], res[2], res[3]});
			}
			else if (b.isMat4x4()) {
				// Matrix multiplication
				double res[16];
				for (int i = 0; i < 4; ++i) {
					for (int j = 0; j < 4; ++j) {
						double sum = 0;
						for (int k = 0; k < 4; ++k)
							sum += getDouble(i*4 + k) * b.getDouble(k*4 + j);
						res[i*4 + j] = sum;
					}
				}
				return var(typeMat4x4Double, {res[0], res[1], res[2], res[3],
				       res[4], res[5], res[6], res[7],
				       res[8], res[9], res[10], res[11],
				       res[12], res[13], res[14], res[15]});
			}
			else if (b.isQuat()) {
				// Quaternion to rotation matrix
				const double x = b.getDouble(0), y = b.getDouble(1), z = b.getDouble(2), w = b.getDouble(3);
				double xx = x * x, yy = y * y, zz = z * z;
				double xy = x * y, xz = x * z, yz = y * z;
				double wx = w * x, wy = w * y, wz = w * z;
				double m[16] = {
				   1.0 - 2.0*(yy + zz),  2.0*(xy - wz),      2.0*(xz + wy),      0.0,
				   2.0*(xy + wz),      1.0 - 2.0*(xx + zz), 2.0*(yz - wx),      0.0,
				   2.0*(xz - wy),      2.0*(yz + wx),      1.0 - 2.0*(xx + yy), 0.0,
				   0.0,                0.0,                0.0,                1.0
				};
				return var(typeMat4x4Double, {m[0], m[1], m[2], m[3],
				       m[4], m[5], m[6], m[7],
				       m[8], m[9], m[10], m[11],
				       m[12], m[13], m[14], m[15]});
		}
		break;
	}


				default:
					break;
			}
		}
		return 0;
	}

	var var::operator/(const var& b) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeFloat:
					return var(getFloat() / b.getFloat());
				case typeDouble:
					return var(getDouble() / b.getDouble());
				case typePtr:
					return var((void*)(getInt64() / b.getInt64()), typePtr);
				case typeInt64:
					return var(getInt64() / b.getInt64());
				case typeInt32:
					return var(getInt32() / b.getInt32());
				case typeInt16:
					return var(getInt16() / b.getInt16());
				case typeInt8:
					return var(getInt8() / b.getInt8());
				case typeUInt64:
					return var(getUInt64() / b.getUInt64());
				case typeUInt32:
					return var(getUInt32() / b.getUInt32());
				case typeUInt16:
					return var(getUInt16() / b.getUInt16());
				case typeUInt8:
					return var(getUInt8() / b.getUInt8());
				case typeBool:
					return var(bool(getInt8() / b.getInt8()));

				case typeVec2Int64:
					return var(
						typeVec2Int64,
						{getInt64(0) / b.getInt64(0),
						 getInt64(1) / b.getInt64(1)});
				case typeVec2Int32:
					return var(
						typeVec2Int32,
						{getInt32(0) / b.getInt32(0),
						 getInt32(1) / b.getInt32(1)});
				case typeVec2Int16:
					return var(
						typeVec2Int16,
						{getInt16(0) / b.getInt16(0),
						 getInt16(1) / b.getInt16(1)});
				case typeVec2Int8:
					return var(
						typeVec2Int8,
						{getInt8(0) / b.getInt8(0),
						 getInt8(1) / b.getInt8(1)});
				case typeVec2UInt64:
					return var(
						typeVec2UInt64,
						{getUInt64(0) / b.getUInt64(0),
						 getUInt64(1) / b.getUInt64(1)});
				case typeVec2UInt32:
					return var(
						typeVec2UInt32,
						{getUInt32(0) / b.getUInt32(0),
						 getUInt32(1) / b.getUInt32(1)});
				case typeVec2UInt16:
					return var(
						typeVec2UInt16,
						{getUInt16(0) / b.getUInt16(0),
						 getUInt16(1) / b.getUInt16(1)});
				case typeVec2UInt8:
					return var(
						typeVec2UInt8,
						{getUInt8(0) / b.getUInt8(0),
						 getUInt8(1) / b.getUInt8(1)});
				case typeVec3Int64:
					return var(
						typeVec3Int64,
						{getInt64(0) / b.getInt64(0),
						 getInt64(1) / b.getInt64(1),
						 getInt64(2) / b.getInt64(2)});
				case typeVec3Int32:
					return var(
						typeVec3Int32,
						{getInt32(0) / b.getInt32(0),
						 getInt32(1) / b.getInt32(1),
						 getInt32(2) / b.getInt32(2)});
				case typeVec3Int16:
					return var(
						typeVec3Int16,
						{getInt16(0) / b.getInt16(0),
						 getInt16(1) / b.getInt16(1),
						 getInt16(2) / b.getInt16(2)});
				case typeVec3Int8:
					return var(
						typeVec3Int8,
						{
							getInt8(0) / b.getInt8(0),
							getInt8(1) / b.getInt8(1),
							getInt8(2) / b.getInt8(2),
						});
				case typeVec3UInt64:
					return var(
						typeVec3UInt64,
						{getUInt64(0) / b.getUInt64(0),
						 getUInt64(1) / b.getUInt64(1),
						 getUInt64(2) / b.getUInt64(2)});
				case typeVec3UInt32:
					return var(
						typeVec3UInt32,
						{getUInt32(0) / b.getUInt32(0),
						 getUInt32(1) / b.getUInt32(1),
						 getUInt32(2) / b.getUInt32(2)});
				case typeVec3UInt16:
					return var(
						typeVec3UInt16,
						{getUInt16(0) / b.getUInt16(0),
						 getUInt16(1) / b.getUInt16(1),
						 getUInt16(2) / b.getUInt16(2)});
				case typeVec3UInt8:
					return var(
						typeVec3UInt8,
						{getUInt8(0) / b.getUInt8(0),
						 getUInt8(1) / b.getUInt8(1),
						 getUInt8(2) / b.getUInt8(2)});
				case typeVec4Int64:
					return var(
						typeVec4Int64,
						{getInt64(0) / b.getInt64(0),
						 getInt64(1) / b.getInt64(1),
						 getInt64(2) / b.getInt64(2),
						 getInt64(3) / b.getInt64(3)});
				case typeVec4Int32:
					return var(
						typeVec4Int32,
						{getInt32(0) / b.getInt32(0),
						 getInt32(1) / b.getInt32(1),
						 getInt32(2) / b.getInt32(2),
						 getInt32(3) / b.getInt32(3)});
				case typeVec4Int16:
					return var(
						typeVec4Int16,
						{getInt16(0) / b.getInt16(0),
						 getInt16(1) / b.getInt16(1),
						 getInt16(2) / b.getInt16(2),
						 getInt16(3) / b.getInt16(3)});
				case typeVec4Int8:
					return var(
						typeVec4Int8,
						{
							getInt8(0) / b.getInt8(0),
							getInt8(1) / b.getInt8(1),
							getInt8(2) / b.getInt8(2),
							getInt8(3) / b.getInt8(3),
						});
				case typeVec4UInt64:
					return var(
						typeVec4UInt64,
						{getUInt64(0) / b.getUInt64(0),
						 getUInt64(1) / b.getUInt64(1),
						 getUInt64(2) / b.getUInt64(2),
						 getUInt64(3) / b.getUInt64(3)});
				case typeVec4UInt32:
					return var(
						typeVec4UInt32,
						{getUInt32(0) / b.getUInt32(0),
						 getUInt32(1) / b.getUInt32(1),
						 getUInt32(2) / b.getUInt32(2),
						 getUInt32(3) / b.getUInt32(3)});
				case typeVec4UInt16:
					return var(
						typeVec4UInt16,
						{getUInt16(0) / b.getUInt16(0),
						 getUInt16(1) / b.getUInt16(1),
						 getUInt16(2) / b.getUInt16(2),
						 getUInt16(3) / b.getUInt16(3)});
				case typeVec4UInt8:
					return var(
						typeVec4UInt8,
						{getUInt8(0) / b.getUInt8(0),
						 getUInt8(1) / b.getUInt8(1),
						 getUInt8(2) / b.getUInt8(2),
						 getUInt8(3) / b.getUInt8(3)});

				case typeQuatFloat:
					return var(
						typeQuatFloat,
						{
							getFloat(0) / b.getFloat(0),
							getFloat(1) / b.getFloat(1),
							getFloat(2) / b.getFloat(2),
							getFloat(3) / b.getFloat(3),
						});
				case typeQuatDouble:
					return var(
						typeQuatDouble,
						{
							getDouble(0) / b.getDouble(0),
							getDouble(1) / b.getDouble(1),
							getDouble(2) / b.getDouble(2),
							getDouble(3) / b.getDouble(3),
						});
				case typeMat3x3Float:
					return var(
						typeMat3x3Float,
						{
							getFloat(0) / b.getFloat(0),
							getFloat(1) / b.getFloat(1),
							getFloat(2) / b.getFloat(2),
							getFloat(3) / b.getFloat(3),
							getFloat(4) / b.getFloat(4),
							getFloat(5) / b.getFloat(5),
							getFloat(6) / b.getFloat(6),
							getFloat(7) / b.getFloat(7),
							getFloat(8) / b.getFloat(8),
						});
				case typeMat3x3Double:
					return var(
						typeMat3x3Double,
						{
							getDouble(0) / b.getDouble(0),
							getDouble(1) / b.getDouble(1),
							getDouble(2) / b.getDouble(2),
							getDouble(3) / b.getDouble(3),
							getDouble(4) / b.getDouble(4),
							getDouble(5) / b.getDouble(5),
							getDouble(6) / b.getDouble(6),
							getDouble(7) / b.getDouble(7),
							getDouble(8) / b.getDouble(8),
						});
				case typeMat4x4Float:
					return var(
						typeMat4x4Float,
						{
							getFloat(0) / b.getFloat(0),
							getFloat(1) / b.getFloat(1),
							getFloat(2) / b.getFloat(2),
							getFloat(3) / b.getFloat(3),
							getFloat(4) / b.getFloat(4),
							getFloat(5) / b.getFloat(5),
							getFloat(6) / b.getFloat(6),
							getFloat(7) / b.getFloat(7),
							getFloat(8) / b.getFloat(8),
							getFloat(9) / b.getFloat(9),
							getFloat(10) / b.getFloat(10),
							getFloat(11) / b.getFloat(11),
							getFloat(12) / b.getFloat(12),
							getFloat(13) / b.getFloat(13),
							getFloat(14) / b.getFloat(14),
							getFloat(15) / b.getFloat(15),
						});
				case typeMat4x4Double:
					return var(
						typeMat4x4Double,
						{
							getDouble(0) / b.getDouble(0),
							getDouble(1) / b.getDouble(1),
							getDouble(2) / b.getDouble(2),
							getDouble(3) / b.getDouble(3),
							getDouble(4) / b.getDouble(4),
							getDouble(5) / b.getDouble(5),
							getDouble(6) / b.getDouble(6),
							getDouble(7) / b.getDouble(7),
							getDouble(8) / b.getDouble(8),
							getDouble(9) / b.getDouble(9),
							getDouble(10) / b.getDouble(10),
							getDouble(11) / b.getDouble(11),
							getDouble(12) / b.getDouble(12),
							getDouble(13) / b.getDouble(13),
							getDouble(14) / b.getDouble(14),
							getDouble(15) / b.getDouble(15),
						});

				default:
					break;
			}
		}
		return 0;
	}

	var var::operator%(const var& b) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeFloat:
					return var(bx::mod(getFloat(), b.getFloat()));
				case typeDouble:
					return var(
						std::fmod(getDouble(), b.getDouble()));
				case typePtr:
					return var((void*)(getInt64() % b.getInt64()), typePtr);
				case typeInt64:
					return var(getInt64() % b.getInt64());
				case typeInt32:
					return var(getInt32() % b.getInt32());
				case typeInt16:
					return var(getInt16() % b.getInt16());
				case typeInt8:
					return var(getInt8() % b.getInt8());
				case typeUInt64:
					return var(getUInt64() % b.getUInt64());
				case typeUInt32:
					return var(getUInt32() % b.getUInt32());
				case typeUInt16:
					return var(getUInt16() % b.getUInt16());
				case typeUInt8:
					return var(getUInt8() % b.getUInt8());
				case typeBool:
					return var(bool(getInt8() % b.getInt8()));

				case typeVec2Int64:
					return var(
						typeVec2Int64,
						{getInt64(0) % b.getInt64(0),
						 getInt64(1) % b.getInt64(1)});
				case typeVec2Int32:
					return var(
						typeVec2Int32,
						{getInt32(0) % b.getInt32(0),
						 getInt32(1) % b.getInt32(1)});
				case typeVec2Int16:
					return var(
						typeVec2Int16,
						{getInt16(0) % b.getInt16(0),
						 getInt16(1) % b.getInt16(1)});
				case typeVec2Int8:
					return var(
						typeVec2Int8,
						{getInt8(0) % b.getInt8(0),
						 getInt8(1) % b.getInt8(1)});
				case typeVec2UInt64:
					return var(
						typeVec2UInt64,
						{getUInt64(0) % b.getUInt64(0),
						 getUInt64(1) % b.getUInt64(1)});
				case typeVec2UInt32:
					return var(
						typeVec2UInt32,
						{getUInt32(0) % b.getUInt32(0),
						 getUInt32(1) % b.getUInt32(1)});
				case typeVec2UInt16:
					return var(
						typeVec2UInt16,
						{getUInt16(0) % b.getUInt16(0),
						 getUInt16(1) % b.getUInt16(1)});
				case typeVec2UInt8:
					return var(
						typeVec2UInt8,
						{getUInt8(0) % b.getUInt8(0),
						 getUInt8(1) % b.getUInt8(1)});
				case typeVec3Int64:
					return var(
						typeVec3Int64,
						{getInt64(0) % b.getInt64(0),
						 getInt64(1) % b.getInt64(1),
						 getInt64(2) % b.getInt64(2)});
				case typeVec3Int32:
					return var(
						typeVec3Int32,
						{getInt32(0) % b.getInt32(0),
						 getInt32(1) % b.getInt32(1),
						 getInt32(2) % b.getInt32(2)});
				case typeVec3Int16:
					return var(
						typeVec3Int16,
						{getInt16(0) % b.getInt16(0),
						 getInt16(1) % b.getInt16(1),
						 getInt16(2) % b.getInt16(2)});
				case typeVec3Int8:
					return var(
						typeVec3Int8,
						{
							getInt8(0) % b.getInt8(0),
							getInt8(1) % b.getInt8(1),
							getInt8(2) % b.getInt8(2),
						});
				case typeVec3UInt64:
					return var(
						typeVec3UInt64,
						{getUInt64(0) % b.getUInt64(0),
						 getUInt64(1) % b.getUInt64(1),
						 getUInt64(2) % b.getUInt64(2)});
				case typeVec3UInt32:
					return var(
						typeVec3UInt32,
						{getUInt32(0) % b.getUInt32(0),
						 getUInt32(1) % b.getUInt32(1),
						 getUInt32(2) % b.getUInt32(2)});
				case typeVec3UInt16:
					return var(
						typeVec3UInt16,
						{getUInt16(0) % b.getUInt16(0),
						 getUInt16(1) % b.getUInt16(1),
						 getUInt16(2) % b.getUInt16(2)});
				case typeVec3UInt8:
					return var(
						typeVec3UInt8,
						{getUInt8(0) % b.getUInt8(0),
						 getUInt8(1) % b.getUInt8(1),
						 getUInt8(2) % b.getUInt8(2)});
				case typeVec4Int64:
					return var(
						typeVec4Int64,
						{getInt64(0) % b.getInt64(0),
						 getInt64(1) % b.getInt64(1),
						 getInt64(2) % b.getInt64(2),
						 getInt64(3) % b.getInt64(3)});
				case typeVec4Int32:
					return var(
						typeVec4Int32,
						{getInt32(0) % b.getInt32(0),
						 getInt32(1) % b.getInt32(1),
						 getInt32(2) % b.getInt32(2),
						 getInt32(3) % b.getInt32(3)});
				case typeVec4Int16:
					return var(
						typeVec4Int16,
						{getInt16(0) % b.getInt16(0),
						 getInt16(1) % b.getInt16(1),
						 getInt16(2) % b.getInt16(2),
						 getInt16(3) % b.getInt16(3)});
				case typeVec4Int8:
					return var(
						typeVec4Int8,
						{
							getInt8(0) % b.getInt8(0),
							getInt8(1) % b.getInt8(1),
							getInt8(2) % b.getInt8(2),
							getInt8(3) % b.getInt8(3),
						});
				case typeVec4UInt64:
					return var(
						typeVec4UInt64,
						{getUInt64(0) % b.getUInt64(0),
						 getUInt64(1) % b.getUInt64(1),
						 getUInt64(2) % b.getUInt64(2),
						 getUInt64(3) % b.getUInt64(3)});
				case typeVec4UInt32:
					return var(
						typeVec4UInt32,
						{getUInt32(0) % b.getUInt32(0),
						 getUInt32(1) % b.getUInt32(1),
						 getUInt32(2) % b.getUInt32(2),
						 getUInt32(3) % b.getUInt32(3)});
				case typeVec4UInt16:
					return var(
						typeVec4UInt16,
						{getUInt16(0) % b.getUInt16(0),
						 getUInt16(1) % b.getUInt16(1),
						 getUInt16(2) % b.getUInt16(2),
						 getUInt16(3) % b.getUInt16(3)});
				case typeVec4UInt8:
					return var(
						typeVec4UInt8,
						{getUInt8(0) % b.getUInt8(0),
						 getUInt8(1) % b.getUInt8(1),
						 getUInt8(2) % b.getUInt8(2),
						 getUInt8(3) % b.getUInt8(3)});

				case typeQuatFloat:
					return var(
						typeQuatFloat,
						{
							bx::mod(getFloat(0), b.getFloat(0)),
							bx::mod(getFloat(1), b.getFloat(1)),
							bx::mod(getFloat(2), b.getFloat(2)),
							bx::mod(getFloat(3), b.getFloat(3)),
						});
				case typeQuatDouble:
					return var(typeQuatDouble, {
							bx::mod(getFloat(0), b.getFloat(0)),
							bx::mod(getFloat(1), b.getFloat(1)),
							bx::mod(getFloat(2), b.getFloat(2)),
							bx::mod(getFloat(3), b.getFloat(3)),
						});

				case typeMat3x3Float:
					return var(
						typeMat3x3Float,
						{
							bx::mod(getFloat(0), b.getFloat(0)),
							bx::mod(getFloat(1), b.getFloat(1)),
							bx::mod(getFloat(2), b.getFloat(2)),
							bx::mod(getFloat(3), b.getFloat(3)),
							bx::mod(getFloat(4), b.getFloat(4)),
							bx::mod(getFloat(5), b.getFloat(5)),
							bx::mod(getFloat(6), b.getFloat(6)),
							bx::mod(getFloat(7), b.getFloat(7)),
							bx::mod(getFloat(8), b.getFloat(8)),
						});
				case typeMat3x3Double:
					return var(
						typeMat3x3Double,
						{
							bx::mod(
								float(getDouble(0)), float(b.getDouble(0))),
							bx::mod(
								float(getDouble(1)), float(b.getDouble(1))),
							bx::mod(
								float(getDouble(2)), float(b.getDouble(2))),
							bx::mod(
								float(getDouble(3)), float(b.getDouble(3))),
							bx::mod(
								float(getDouble(4)), float(b.getDouble(4))),
							bx::mod(
								float(getDouble(5)), float(b.getDouble(5))),
							bx::mod(
								float(getDouble(6)), float(b.getDouble(6))),
							bx::mod(
								float(getDouble(7)), float(b.getDouble(7))),
							bx::mod(
								float(getDouble(8)), float(b.getDouble(8))),
						});
				case typeMat4x4Float:
					return var(
						typeMat4x4Float,
						{
							bx::mod(getFloat(0), b.getFloat(0)),
							bx::mod(getFloat(1), b.getFloat(1)),
							bx::mod(getFloat(2), b.getFloat(2)),
							bx::mod(getFloat(3), b.getFloat(3)),
							bx::mod(getFloat(4), b.getFloat(4)),
							bx::mod(getFloat(5), b.getFloat(5)),
							bx::mod(getFloat(6), b.getFloat(6)),
							bx::mod(getFloat(7), b.getFloat(7)),
							bx::mod(getFloat(8), b.getFloat(8)),
							bx::mod(getFloat(9), b.getFloat(9)),
							bx::mod(getFloat(10), b.getFloat(10)),
							bx::mod(getFloat(11), b.getFloat(11)),
							bx::mod(getFloat(12), b.getFloat(12)),
							bx::mod(getFloat(13), b.getFloat(13)),
							bx::mod(getFloat(14), b.getFloat(14)),
							bx::mod(getFloat(15), b.getFloat(15)),
						});
				case typeMat4x4Double:
					return var(
						typeMat4x4Double,
						{
							bx::mod((getFloat(0)), (b.getFloat(0))),
							bx::mod((getFloat(1)), (b.getFloat(1))),
							bx::mod((getFloat(2)), (b.getFloat(2))),
							bx::mod((getFloat(3)), (b.getFloat(3))),
							bx::mod((getFloat(4)), (b.getFloat(4))),
							bx::mod((getFloat(5)), (b.getFloat(5))),
							bx::mod((getFloat(6)), (b.getFloat(6))),
							bx::mod((getFloat(7)), (b.getFloat(7))),
							bx::mod((getFloat(8)), (b.getFloat(8))),
							bx::mod((getFloat(9)), (b.getFloat(9))),
							bx::mod((getFloat(10)), (b.getFloat(10))),
							bx::mod((getFloat(11)), (b.getFloat(11))),
							bx::mod((getFloat(12)), (b.getFloat(12))),
							bx::mod((getFloat(13)), (b.getFloat(13))),
							bx::mod((getFloat(14)), (b.getFloat(14))),
							bx::mod((getFloat(15)), (b.getFloat(15))),
						});

				default:
					break;
			}
		}
		return 0;
	}

	types var::getType() const {
		auto con = sPtr.get();
		if (con) return con->type;
		return typeNull;
	}

	const char* var::getTypeString() const {
		types type = typeNull;
		auto con = sPtr.get();
		if (con) type = con->type;
		return gold::getTypeString(type);
	}

	bool var::isString() const {
		auto con = sPtr.get();
		if (con && con->type == typeString) return true;
		return false;
	}

	bool var::isView() const {
		auto con = sPtr.get();
		if (con) {
			if (con->type == typeStringView)
				return true;
			else if (con->type == typeString)
				return true;
			else if (con->type == typeBinary)
				return true;
		}
		return false;
	}

	bool var::isNumber() const {
		auto con = sPtr.get();
		if (
			con &&
			(con->type == typeDouble || con->type == typeFloat ||
			 con->type == typeInt64 || con->type == typeInt32 ||
			 con->type == typeInt16 || con->type == typeInt8 ||
			 con->type == typeUInt64 || con->type == typeUInt32 ||
			 con->type == typeUInt16 || con->type == typeUInt8 ||
			 con->type == typePtr || con->type == typeVec2Int64 ||
			 con->type == typeVec2Int32 ||
			 con->type == typeVec2Int16 ||
			 con->type == typeVec2Int8 ||
			 con->type == typeVec3Int64 ||
			 con->type == typeVec3Int32 ||
			 con->type == typeVec3Int16 ||
			 con->type == typeVec3Int8 ||
			 con->type == typeVec4Int64 ||
			 con->type == typeVec4Int32 ||
			 con->type == typeVec4Int16 ||
			 con->type == typeVec4Int8 ||
			 con->type == typeVec2UInt64 ||
			 con->type == typeVec2UInt32 ||
			 con->type == typeVec2UInt16 ||
			 con->type == typeVec2UInt8 ||
			 con->type == typeVec3UInt64 ||
			 con->type == typeVec3UInt32 ||
			 con->type == typeVec3UInt16 ||
			 con->type == typeVec3UInt8 ||
			 con->type == typeVec4UInt64 ||
			 con->type == typeVec4UInt32 ||
			 con->type == typeVec4UInt16 ||
			 con->type == typeVec4UInt8 ||
			 con->type == typeVec2Float ||
			 con->type == typeVec2Double ||
			 con->type == typeVec3Float ||
			 con->type == typeVec3Double ||
			 con->type == typeVec4Float ||
			 con->type == typeVec4Double ||
			 con->type == typeQuatFloat ||
			 con->type == typeQuatDouble ||
			 con->type == typeMat3x3Float ||
			 con->type == typeMat3x3Double ||
			 con->type == typeMat4x4Float ||
			 con->type == typeMat4x4Double))
			return true;
		return false;
	}

	bool var::isFloating() const {
		auto con = sPtr.get();
		if (
			con &&
			(con->type == typeDouble || con->type == typeFloat ||
			 con->type == typeVec2Float ||
			 con->type == typeVec2Double ||
			 con->type == typeVec3Float ||
			 con->type == typeVec3Double ||
			 con->type == typeVec4Float ||
			 con->type == typeVec4Double ||
			 con->type == typeQuatFloat ||
			 con->type == typeQuatDouble ||
			 con->type == typeMat3x3Float ||
			 con->type == typeMat3x3Double ||
			 con->type == typeMat4x4Float ||
			 con->type == typeMat4x4Double))
			return true;
		return false;
	}

	bool var::isSigned() const {
		auto con = sPtr.get();
		if (
			con &&
			(con->type == typeDouble || con->type == typeFloat ||
			 con->type == typeInt64 || con->type == typeInt32 ||
			 con->type == typeInt16 || con->type == typeInt8 ||
			 con->type == typeVec2Int64 ||
			 con->type == typeVec2Int32 ||
			 con->type == typeVec2Int16 ||
			 con->type == typeVec2Int8 ||
			 con->type == typeVec3Int64 ||
			 con->type == typeVec3Int32 ||
			 con->type == typeVec3Int16 ||
			 con->type == typeVec3Int8 ||
			 con->type == typeVec4Int64 ||
			 con->type == typeVec4Int32 ||
			 con->type == typeVec4Int16 ||
			 con->type == typeVec4Int8 ||
			 con->type == typeVec2Float ||
			 con->type == typeVec2Double ||
			 con->type == typeVec3Float ||
			 con->type == typeVec3Double ||
			 con->type == typeVec4Float ||
			 con->type == typeVec4Double ||
			 con->type == typeQuatFloat ||
			 con->type == typeQuatDouble ||
			 con->type == typeMat3x3Float ||
			 con->type == typeMat3x3Double ||
			 con->type == typeMat4x4Float ||
			 con->type == typeMat4x4Double))
			return true;
		return false;
	}

	bool var::isBool() const {
		auto con = sPtr.get();
		if (con && con->type == typeBool) return true;
		return false;
	}

	bool var::isObject() const {
		auto con = sPtr.get();
		if (con && con->type == typeObject) return true;
		return false;
	}

	bool var::isObject(obj& proto) const {
		if (sPtr && sPtr->type == typeObject) {
			auto p = *sPtr->obj;
			while (p) {
				if (p == proto) return true;
				p = p.data->parent;
			}
		}
		return false;
	}

	bool var::isList() const {
		auto con = sPtr.get();
		if (con && con->type == typeList) return true;
		return false;
	}

	bool var::isEmpty() const {
		auto con = sPtr.get();
		if (con) {
			auto t = con->type;
			if (t == typeList)
				return con->li->size() == 0;
			else if (t == typeObject)
				return con->obj->size() == 0;
			else if (t == typeString)
				return con->str->length() == 0;
			else if (t == typeFunction)
				return *con->fu == 0;
			else if (t == typeMethod)
				return *con->me == 0;
			else if (t == typeDouble)
				return *con->d == 0.0;
			else if (t == typeFloat)
				return *con->f == 0.0;
			else if (t == typeUInt64)
				return *con->u64 == 0;
			else if (t == typeUInt32)
				return *con->u32 == 0;
			else if (t == typeUInt16)
				return *con->u16 == 0;
			else if (t == typeUInt8)
				return *con->u8 == 0;
			else if (t == typeInt64)
				return *con->i64 == 0;
			else if (t == typeInt32)
				return *con->i32 == 0;
			else if (t == typeInt16)
				return *con->i16 == 0;
			else if (t == typeInt8)
				return *con->i8 == 0;
			else if (t == typeBool)
				return *con->b == false;
			else if (t == typePtr)
				return con->data == nullptr;
			else if (t == typeVec2Int64)
				return getInt64(0) == 0 && getInt64(1) == 0;
			else if (t == typeVec2Int32)
				return getInt32(0) == 0 && getInt32(1) == 0;
			else if (t == typeVec2Int16)
				return getInt16(0) == 0 && getInt16(1) == 0;
			else if (t == typeVec2Int8)
				return getInt8(0) == 0 && getInt8(1) == 0;
			else if (t == typeVec2UInt64)
				return getUInt64(0) == 0 && getUInt64(1) == 0;
			else if (t == typeVec2UInt32)
				return getUInt32(0) == 0 && getUInt32(1) == 0;
			else if (t == typeVec2UInt16)
				return getUInt16(0) == 0 && getUInt16(1) == 0;
			else if (t == typeVec2UInt8)
				return getUInt8(0) == 0 && getUInt8(1) == 0;

			else if (t == typeVec3Int64)
				return getInt64(0) == 0 && getInt64(1) == 0 &&
							 getInt64(2) == 0;
			else if (t == typeVec3Int32)
				return getInt32(0) == 0 && getInt32(1) == 0 &&
							 getInt32(2) == 0;
			else if (t == typeVec3Int16)
				return getInt16(0) == 0 && getInt16(1) == 0 &&
							 getInt16(2) == 0;
			else if (t == typeVec3Int8)
				return getInt8(0) == 0 && getInt8(1) == 0 &&
							 getInt8(2) == 0;
			else if (t == typeVec3UInt64)
				return getUInt64(0) == 0 && getUInt64(1) == 0 &&
							 getUInt64(2) == 0;
			else if (t == typeVec3UInt32)
				return getUInt32(0) == 0 && getUInt32(1) == 0 &&
							 getUInt32(2) == 0;
			else if (t == typeVec3UInt16)
				return getUInt16(0) == 0 && getUInt16(1) == 0 &&
							 getUInt16(2) == 0;
			else if (t == typeVec3UInt8)
				return getUInt8(0) == 0 && getUInt8(1) == 0 &&
							 getUInt8(2) == 0;

			else if (t == typeVec4Int64)
				return getInt64(0) == 0 && getInt64(1) == 0 &&
							 getInt64(2) == 0 && getInt64(3) == 0;
			else if (t == typeVec4Int32)
				return getInt32(0) == 0 && getInt32(1) == 0 &&
							 getInt32(2) == 0 && getInt32(3) == 0;
			else if (t == typeVec4Int16)
				return getInt16(0) == 0 && getInt16(1) == 0 &&
							 getInt16(2) == 0 && getInt16(3) == 0;
			else if (t == typeVec4Int8)
				return getInt8(0) == 0 && getInt8(1) == 0 &&
							 getInt8(2) == 0 && getInt8(3) == 0;
			else if (t == typeVec4UInt64)
				return getUInt64(0) == 0 && getUInt64(1) == 0 &&
							 getUInt64(2) == 0 && getUInt64(3) == 0;
			else if (t == typeVec4UInt32)
				return getUInt32(0) == 0 && getUInt32(1) == 0 &&
							 getUInt32(2) == 0 && getUInt32(3) == 0;
			else if (t == typeVec4UInt16)
				return getUInt16(0) == 0 && getUInt16(1) == 0 &&
							 getUInt16(2) == 0 && getUInt16(3) == 0;
			else if (t == typeVec4UInt8)
				return getUInt8(0) == 0 && getUInt8(1) == 0 &&
							 getUInt8(2) == 0 && getUInt8(3) == 0;

			else if (t == typeVec2Float)
				return getFloat(0) == 0 && getFloat(1) == 0;
			else if (t == typeVec2Double)
				return getDouble(0) == 0 && getDouble(1) == 0;
			else if (t == typeVec3Float)
				return getFloat(0) == 0 && getFloat(1) == 0 &&
							 getFloat(2) == 0;
			else if (t == typeVec3Double)
				return getDouble(0) == 0 && getDouble(1) == 0 &&
							 getDouble(2) == 0;
			else if (t == typeVec4Float)
				return getFloat(0) == 0 && getFloat(1) == 0 &&
							 getFloat(2) == 0 && getFloat(3) == 0;
			else if (t == typeVec4Double)
				return getDouble(0) == 0 && getDouble(1) == 0 &&
							 getDouble(2) == 0 && getDouble(3) == 0;

			else if (t == typeQuatFloat)
				return getFloat(0) == 0 && getFloat(1) == 0 &&
							 getFloat(2) == 0 && getFloat(3) == 0;
			else if (t == typeQuatDouble)
				return getDouble(0) == 0 && getDouble(1) == 0 &&
							 getDouble(2) == 0 && getDouble(3) == 0;

			else if (t == typeMat3x3Float)
				return getFloat(0) == 0 && getFloat(1) == 0 &&
							 getFloat(2) == 0 && getFloat(3) == 0 &&
							 getFloat(4) == 0 && getFloat(5) == 0 &&
							 getFloat(6) == 0 && getFloat(7) == 0 &&
							 getFloat(8) == 0;
			else if (t == typeMat3x3Double)
				return getDouble(0) == 0 && getDouble(1) == 0 &&
							 getDouble(2) == 0 && getDouble(3) == 0 &&
							 getDouble(4) == 0 && getDouble(5) == 0 &&
							 getDouble(6) == 0 && getDouble(7) == 0 &&
							 getDouble(8) == 0;
			else if (t == typeMat4x4Float)
				return getFloat(0) == 0 && getFloat(1) == 0 &&
							 getFloat(2) == 0 && getFloat(3) == 0 &&
							 getFloat(4) == 0 && getFloat(5) == 0 &&
							 getFloat(6) == 0 && getFloat(7) == 0 &&
							 getFloat(8) == 0 && getFloat(9) == 0 &&
							 getFloat(10) == 0 && getFloat(11) == 0 &&
							 getFloat(12) == 0 && getFloat(13) == 0 &&
							 getFloat(14) == 0 && getFloat(15) == 0;
			else if (t == typeMat4x4Double)
				return getDouble(0) == 0 && getDouble(1) == 0 &&
							 getDouble(2) == 0 && getDouble(3) == 0 &&
							 getDouble(4) == 0 && getDouble(5) == 0 &&
							 getDouble(6) == 0 && getDouble(7) == 0 &&
							 getDouble(8) == 0 && getDouble(9) == 0 &&
							 getDouble(10) == 0 && getDouble(11) == 0 &&
							 getDouble(12) == 0 && getDouble(13) == 0 &&
							 getDouble(14) == 0 && getDouble(15) == 0;

			else
				return false;
		}
		return true;
	}

	bool var::isError() const {
		auto con = sPtr.get();
		return con && con->type == typeException;
	}

	bool var::isFunction() const {
		auto con = sPtr.get();
		return con && con->type == typeFunction;
	}

	bool var::isMethod() const {
		auto con = sPtr.get();
		return con && con->type == typeMethod;
	}

	bool var::isBinary() const {
		auto con = sPtr.get();
		return con && con->type == typeBinary;
	}

	bool var::isVec2() const {
		auto con = sPtr.get();
		return con && (con->type == typeVec2Double ||
									 con->type == typeVec2Float ||
									 con->type == typeVec2Int16 ||
									 con->type == typeVec2Int32 ||
									 con->type == typeVec2Int64 ||
									 con->type == typeVec2Int8 ||
									 con->type == typeVec2UInt16 ||
									 con->type == typeVec2UInt32 ||
									 con->type == typeVec2UInt64 ||
									 con->type == typeVec2UInt8);
	}

	bool var::isVec3() const {
		auto con = sPtr.get();
		return con && (con->type == typeVec3Double ||
									 con->type == typeVec3Float ||
									 con->type == typeVec3Int16 ||
									 con->type == typeVec3Int32 ||
									 con->type == typeVec3Int64 ||
									 con->type == typeVec3Int8 ||
									 con->type == typeVec3UInt16 ||
									 con->type == typeVec3UInt32 ||
									 con->type == typeVec3UInt64 ||
									 con->type == typeVec3UInt8);
	}

	bool var::isVec4() const {
		auto con = sPtr.get();
		return con && (con->type == typeVec4Double ||
									 con->type == typeVec4Float ||
									 con->type == typeVec4Int16 ||
									 con->type == typeVec4Int32 ||
									 con->type == typeVec4Int64 ||
									 con->type == typeVec4Int8 ||
									 con->type == typeVec4UInt16 ||
									 con->type == typeVec4UInt32 ||
									 con->type == typeVec4UInt64 ||
									 con->type == typeVec4UInt8);
	}

	bool var::isQuat() const {
		auto con = sPtr.get();
		return con && (con->type == typeQuatFloat ||
									 con->type == typeQuatDouble);
	}

	bool var::isMat3x3() const {
		auto con = sPtr.get();
		return con && (con->type == typeMat3x3Float ||
									 con->type == typeMat3x3Double);
	}

	bool var::isMat4x4() const {
		auto con = sPtr.get();
		return con && (con->type == typeMat4x4Float ||
									 con->type == typeMat4x4Double);
	}

	void var::setInt64(size_t i, int64_t v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2Int64:
				con->i64[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3Int64:
				con->i64[min((unsigned long)i, 2ul)] = v;
				break;
			case typeVec4Int64:
				con->i64[min((unsigned long)i, 3ul)] = v;
				break;
			default:
				break;
		}
	}

	void var::setInt32(size_t i, int32_t v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2Int32:
				con->i32[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3Int32:
				con->i32[min((unsigned long)i, 2ul)] = v;
				break;
			case typeVec4Int32:
				con->i32[min((unsigned long)i, 3ul)] = v;
				break;
			default:
				break;
		}
	}

	void var::setInt16(size_t i, int16_t v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2Int16:
				con->i16[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3Int16:
				con->i16[min((unsigned long)i, 2ul)] = v;
				break;
			case typeVec4Int16:
				con->i16[min((unsigned long)i, 3ul)] = v;
				break;
			default:
				break;
		}
	}

	void var::setInt8(size_t i, int8_t v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2Int8:
				con->i8[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3Int8:
				con->i8[min((unsigned long)i, 2ul)] = v;
				break;
			case typeVec4Int8:
				con->i8[min((unsigned long)i, 3ul)] = v;
				break;
			default:
				break;
		}
	}

	void var::setUInt64(size_t i, uint64_t v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2UInt64:
				con->u64[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3UInt64:
				con->u64[min((unsigned long)i, 2ul)] = v;
				break;
			case typeVec4UInt64:
				con->u64[min((unsigned long)i, 3ul)] = v;
				break;
			default:
				break;
		}
	}

	void var::setUInt32(size_t i, uint32_t v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2UInt32:
				con->u32[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3UInt32:
				con->u32[min((unsigned long)i, 2ul)] = v;
				break;
			case typeVec4UInt32:
				con->u32[min((unsigned long)i, 3ul)] = v;
				break;
			default:
				break;
		}
	}

	void var::setUInt16(size_t i, uint16_t v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2UInt16:
				con->u16[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3UInt16:
				con->u16[min((unsigned long)i, 2ul)] = v;
				break;
			case typeVec4UInt16:
				con->u16[min((unsigned long)i, 3ul)] = v;
				break;
			default:
				break;
		}
	}

	void var::setUInt8(size_t i, uint8_t v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2UInt8:
				con->u8[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3UInt8:
				con->u8[min((unsigned long)i, 2ul)] = v;
				break;
			case typeVec4UInt8:
				con->u8[min((unsigned long)i, 3ul)] = v;
				break;
			default:
				break;
		}
	}

	void var::setDouble(size_t i, double v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2Double:
				con->d[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3Double:
				con->d[min((unsigned long)i, 2ul)] = v;
				break;
			case typeQuatDouble:
			case typeVec4Double:
				con->d[min((unsigned long)i, 3ul)] = v;
				break;
			case typeMat3x3Double:
				con->d[min((unsigned long)i, 8ul)] = v;
				break;
			case typeMat4x4Double:
				con->d[min((unsigned long)i, 15ul)] = v;
				break;
			default:
				break;
		}
	}

	void var::setFloat(size_t i, float v) {
		auto con = sPtr.get();
		switch (con->type) {
			case typeVec2Float:
				con->f[min((unsigned long)i, 1ul)] = v;
				break;
			case typeVec3Float:
				con->f[min((unsigned long)i, 2ul)] = v;
				break;
			case typeQuatFloat:
			case typeVec4Float:
				con->f[min((unsigned long)i, 3ul)] = v;
				break;
			case typeMat3x3Float:
				con->f[min((unsigned long)i, 8ul)] = v;
				break;
			case typeMat4x4Float:
				con->f[min((unsigned long)i, 15ul)] = v;
				break;
			default:
				break;
		}
	}

	string var::getString() const { return this->operator std::string(); }

	string_view var::getStringView() const {
		auto con = sPtr.get();
		if (!con) return string_view();
		if (con->type == typeStringView)
			return *con->sv;
		else if (con->type == typeBinary)
			return string_view(
				(char*)con->bin->data(), con->bin->size());
		else if (con->type == typeString)
			return string_view(con->str->data(), con->str->size());
		return string_view();
	}

	int64_t var::getInt64(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2Int64:
					return (con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return (con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return (con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return (*con->i64);
				case typePtr:
					return (int64_t)(void*)con->data;
				case typeVec2Int32:
					return int64_t(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return int64_t(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return int64_t(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return int64_t(*con->i32);
				case typeVec2Int16:
					return int64_t(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return int64_t(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return int64_t(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return int64_t(*con->i16);
				case typeVec2Int8:
					return int64_t(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return int64_t(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return int64_t(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return int64_t(*con->i8);
				case typeVec2UInt64:
					return int64_t(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return int64_t(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return int64_t(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return int64_t(*con->u64);
				case typeVec2UInt32:
					return int64_t(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return int64_t(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return int64_t(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return int64_t(*con->u32);
				case typeVec2UInt16:
					return int64_t(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return int64_t(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return int64_t(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return int64_t(*con->u16);
				case typeVec2UInt8:
					return int64_t(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return int64_t(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return int64_t(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return int64_t(*con->u8);
				case typeVec2Double:
					return int64_t(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return int64_t(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return int64_t(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return int64_t(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return int64_t(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return int64_t(*con->d);
				case typeVec2Float:
					return int64_t(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return int64_t(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return int64_t(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return int64_t(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return int64_t(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return int64_t(*con->f);
				case typeBool:
					return int64_t(*con->b);
				case typeString:
					return atol((*con->str).c_str());
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	int32_t var::getInt32(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2Int32:
					return (con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return (con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return (con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return (*con->i32);
				case typeVec2Int64:
					return int32_t(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return int32_t(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return int32_t(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return int32_t(*con->i64);
				case typeVec2Int16:
					return int32_t(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return int32_t(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return int32_t(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return int32_t(*con->i16);
				case typeVec2Int8:
					return int32_t(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return int32_t(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return int32_t(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return int32_t(*con->i8);
				case typeVec2UInt64:
					return int32_t(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return int32_t(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return int32_t(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return int32_t(*con->u64);
				case typeVec2UInt32:
					return int32_t(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return int32_t(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return int32_t(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return int32_t(*con->u32);
				case typeVec2UInt16:
					return int32_t(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return int32_t(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return int32_t(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return int32_t(*con->u16);
				case typeVec2UInt8:
					return int32_t(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return int32_t(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return int32_t(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return int32_t(*con->u8);
				case typeVec2Double:
					return int32_t(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return int32_t(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return int32_t(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return int32_t(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return int32_t(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return int32_t(*con->d);
				case typeVec2Float:
					return int32_t(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return int32_t(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return int32_t(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return int32_t(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return int32_t(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return int32_t(*con->f);
				case typeBool:
					return int32_t(*con->b);
				case typeString:
					return atoi((*con->str).c_str());
				case typePtr:
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	int16_t var::getInt16(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2Int16:
					return (con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return (con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return (con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return (*con->i16);
				case typeVec2Int64:
					return int16_t(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return int16_t(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return int16_t(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return int16_t(*con->i64);
				case typeVec2Int32:
					return int16_t(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return int16_t(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return int16_t(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return int16_t(*con->i32);
				case typeVec2Int8:
					return int16_t(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return int16_t(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return int16_t(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return int16_t(*con->i8);
				case typeVec2UInt64:
					return int16_t(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return int16_t(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return int16_t(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return int16_t(*con->u64);
				case typeVec2UInt32:
					return int16_t(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return int16_t(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return int16_t(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return int16_t(*con->u32);
				case typeVec2UInt16:
					return int16_t(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return int16_t(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return int16_t(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return int16_t(*con->u16);
				case typeVec2UInt8:
					return int16_t(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return int16_t(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return int16_t(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return int16_t(*con->u8);
				case typeVec2Double:
					return int16_t(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return int16_t(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return int16_t(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return int16_t(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return int16_t(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return int16_t(*con->d);
				case typeVec2Float:
					return int16_t(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return int16_t(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return int16_t(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return int16_t(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return int16_t(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return int16_t(*con->f);
				case typeBool:
					return int16_t(*con->b);
				case typeString:
					return (int16_t)atoi((*con->str).c_str());
				case typePtr:
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	int8_t var::getInt8(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2Int8:
					return (con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return (con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return (con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return (*con->i8);
				case typeVec2Int64:
					return int8_t(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return int8_t(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return int8_t(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return int8_t(*con->i64);
				case typeVec2Int32:
					return int8_t(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return int8_t(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return int8_t(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return int8_t(*con->i32);
				case typeVec2Int16:
					return int8_t(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return int8_t(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return int8_t(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return int8_t(*con->i16);
				case typeVec2UInt64:
					return int8_t(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return int8_t(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return int8_t(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return int8_t(*con->u64);
				case typeVec2UInt32:
					return int8_t(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return int8_t(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return int8_t(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return int8_t(*con->u32);
				case typeVec2UInt16:
					return int8_t(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return int8_t(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return int8_t(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return int8_t(*con->u16);
				case typeVec2UInt8:
					return int8_t(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return int8_t(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return int8_t(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return int8_t(*con->u8);
				case typeVec2Double:
					return int8_t(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return int8_t(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return int8_t(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return int8_t(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return int8_t(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return int8_t(*con->d);
				case typeVec2Float:
					return int8_t(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return int8_t(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return int8_t(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return int8_t(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return int8_t(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return int8_t(*con->f);
				case typeBool:
					return int8_t(*con->b);
				case typeString:
					return (int8_t)atoi((*con->str).c_str());
				case typePtr:
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	uint64_t var::getUInt64(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2UInt64:
					return (con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return (con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return (con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return (*con->u64);
				case typeVec2Int64:
					return uint64_t(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return uint64_t(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return uint64_t(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return uint64_t(*con->i64);
				case typePtr:
					return (uint64_t)(void*)con->data;
				case typeVec2Int32:
					return uint64_t(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return uint64_t(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return uint64_t(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return uint64_t(*con->i32);
				case typeVec2Int16:
					return uint64_t(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return uint64_t(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return uint64_t(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return uint64_t(*con->i16);
				case typeVec2Int8:
					return uint64_t(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return uint64_t(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return uint64_t(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return uint64_t(*con->i8);
				case typeVec2UInt32:
					return uint64_t(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return uint64_t(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return uint64_t(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return uint64_t(*con->u32);
				case typeVec2UInt16:
					return uint64_t(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return uint64_t(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return uint64_t(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return uint64_t(*con->u16);
				case typeVec2UInt8:
					return uint64_t(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return uint64_t(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return uint64_t(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return uint64_t(*con->u8);
				case typeVec2Double:
					return uint64_t(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return uint64_t(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return uint64_t(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return uint64_t(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return uint64_t(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return uint64_t(*con->d);
				case typeVec2Float:
					return uint64_t(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return uint64_t(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return uint64_t(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return uint64_t(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return uint64_t(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return uint64_t(*con->f);
				case typeBool:
					return uint64_t(*con->b);
				case typeString:
					return (uint64_t)strtoul(
						(*con->str).c_str(), NULL, 0);
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	uint32_t var::getUInt32(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2UInt32:
					return (con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return (con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return (con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return (*con->u32);
				case typeVec2Int64:
					return uint32_t(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return uint32_t(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return uint32_t(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return uint32_t(*con->i64);
				case typeVec2Int32:
					return uint32_t(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return uint32_t(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return uint32_t(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return uint32_t(*con->i32);
				case typeVec2Int16:
					return uint32_t(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return uint32_t(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return uint32_t(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return uint32_t(*con->i16);
				case typeVec2Int8:
					return uint32_t(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return uint32_t(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return uint32_t(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return uint32_t(*con->i8);
				case typeVec2UInt64:
					return uint32_t(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return uint32_t(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return uint32_t(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return uint32_t(*con->u64);
				case typeVec2UInt16:
					return uint32_t(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return uint32_t(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return uint32_t(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return uint32_t(*con->u16);
				case typeVec2UInt8:
					return uint32_t(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return uint32_t(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return uint32_t(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return uint32_t(*con->u8);
				case typeVec2Double:
					return uint32_t(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return uint32_t(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return uint32_t(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return uint32_t(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return uint32_t(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return uint32_t(*con->d);
				case typeVec2Float:
					return uint32_t(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return uint32_t(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return uint32_t(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return uint32_t(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return uint32_t(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return uint32_t(*con->f);
				case typeBool:
					return uint32_t(*con->b);
				case typeString:
					return (uint32_t)strtoul(
						(*con->str).c_str(), NULL, 0);
				case typePtr:
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	uint16_t var::getUInt16(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2UInt16:
					return (con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return (con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return (con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return (*con->u16);
				case typeVec2Int64:
					return uint16_t(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return uint16_t(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return uint16_t(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return uint16_t(*con->i64);
				case typeVec2Int32:
					return uint16_t(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return uint16_t(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return uint16_t(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return uint16_t(*con->i32);
				case typeVec2Int16:
					return uint16_t(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return uint16_t(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return uint16_t(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return uint16_t(*con->i16);
				case typeVec2Int8:
					return uint16_t(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return uint16_t(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return uint16_t(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return uint16_t(*con->i8);
				case typeVec2UInt64:
					return uint16_t(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return uint16_t(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return uint16_t(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return uint16_t(*con->u64);
				case typeVec2UInt32:
					return uint16_t(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return uint16_t(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return uint16_t(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return uint16_t(*con->u32);
				case typeVec2UInt8:
					return uint16_t(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return uint16_t(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return uint16_t(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return uint16_t(*con->u8);
				case typeVec2Double:
					return uint16_t(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return uint16_t(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return uint16_t(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return uint16_t(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return uint16_t(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return uint16_t(*con->d);
				case typeVec2Float:
					return uint16_t(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return uint16_t(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return uint16_t(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return uint16_t(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return uint16_t(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return uint16_t(*con->f);
				case typeBool:
					return uint16_t(*con->b);
				case typeString:
					return (uint16_t)strtoul(
						(*con->str).c_str(), NULL, 0);
				case typePtr:
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	uint8_t var::getUInt8(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2UInt8:
					return (con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return (con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return (con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return (*con->u8);
				case typeVec2Int64:
					return uint8_t(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return uint8_t(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return uint8_t(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return uint8_t(*con->i64);
				case typeVec2Int32:
					return uint8_t(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return uint8_t(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return uint8_t(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return uint8_t(*con->i32);
				case typeVec2Int16:
					return uint8_t(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return uint8_t(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return uint8_t(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return uint8_t(*con->i16);
				case typeVec2Int8:
					return uint8_t(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return uint8_t(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return uint8_t(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return uint8_t(*con->i8);
				case typeVec2UInt64:
					return uint8_t(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return uint8_t(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return uint8_t(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return uint8_t(*con->u64);
				case typeVec2UInt32:
					return uint8_t(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return uint8_t(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return uint8_t(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return uint8_t(*con->u32);
				case typeVec2UInt16:
					return uint8_t(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return uint8_t(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return uint8_t(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return uint8_t(*con->u16);
				case typeVec2Double:
					return uint8_t(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return uint8_t(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return uint8_t(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return uint8_t(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return uint8_t(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return uint8_t(*con->d);
				case typeVec2Float:
					return uint8_t(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return uint8_t(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return uint8_t(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return uint8_t(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return uint8_t(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return uint8_t(*con->f);
				case typeBool:
					return uint8_t(*con->b);
				case typeString:
					return (uint8_t)strtoul((*con->str).c_str(), NULL, 0);
				case typePtr:
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	double var::getDouble(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2Double:
					return (con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return (con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return (con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return (con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return (con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return (*con->d);
				case typeVec2Int64:
					return double(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return double(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return double(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return double(*con->i64);
				case typePtr:
					return (double)(uint64_t)(void*)con->data;
				case typeVec2Int32:
					return double(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return double(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return double(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return double(*con->i32);
				case typeVec2Int16:
					return double(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return double(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return double(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return double(*con->i16);
				case typeVec2Int8:
					return double(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return double(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return double(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return double(*con->i8);
				case typeVec2UInt64:
					return double(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return double(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return double(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return double(*con->u64);
				case typeVec2UInt32:
					return double(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return double(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return double(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return double(*con->u32);
				case typeVec2UInt16:
					return double(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return double(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return double(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return double(*con->u16);
				case typeVec2UInt8:
					return double(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return double(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return double(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return double(*con->u8);
				case typeVec2Float:
					return double(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return double(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return double(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return double(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return double(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return double(*con->f);
				case typeBool:
					return double(*con->b);
				case typeString:
					return stod((*con->str).c_str());
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	float var::getFloat(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeVec2Float:
					return (con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return (con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return (con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return (con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return (con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return (*con->f);
				case typeVec2Double:
					return float(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return float(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return float(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return float(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return float(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return float(*con->d);
				case typeVec2Int64:
					return float(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return float(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return float(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return float(*con->i64);
				case typePtr:
					return (float)(uint64_t)(void*)con->data;
				case typeVec2Int32:
					return float(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return float(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return float(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return float(*con->i32);
				case typeVec2Int16:
					return float(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return float(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return float(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return float(*con->i16);
				case typeVec2Int8:
					return float(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return float(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return float(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return float(*con->i8);
				case typeVec2UInt64:
					return float(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return float(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return float(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return float(*con->u64);
				case typeVec2UInt32:
					return float(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return float(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return float(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return float(*con->u32);
				case typeVec2UInt16:
					return float(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return float(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return float(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return float(*con->u16);
				case typeVec2UInt8:
					return float(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return float(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return float(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return float(*con->u8);
				case typeBool:
					return float(*con->b);
				case typeString:
					return stof((*con->str).c_str());
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return 0;
	}

	bool var::getBool(size_t i) const {
		auto con = sPtr.get();
		if (con) {
			switch (con->type) {
				case typeBool:
					return bool(*con->b);
				case typeVec2Int64:
					return bool(con->i64[min((unsigned long)i, 1ul)]);
				case typeVec3Int64:
					return bool(con->i64[min((unsigned long)i, 2ul)]);
				case typeVec4Int64:
					return bool(con->i64[min((unsigned long)i, 3ul)]);
				case typeInt64:
					return bool(*con->i64);
				case typePtr:
					return (bool)(void*)con->data;
				case typeVec2Int32:
					return bool(con->i32[min((unsigned long)i, 1ul)]);
				case typeVec3Int32:
					return bool(con->i32[min((unsigned long)i, 2ul)]);
				case typeVec4Int32:
					return bool(con->i32[min((unsigned long)i, 3ul)]);
				case typeInt32:
					return bool(*con->i32);
				case typeVec2Int16:
					return bool(con->i16[min((unsigned long)i, 1ul)]);
				case typeVec3Int16:
					return bool(con->i16[min((unsigned long)i, 2ul)]);
				case typeVec4Int16:
					return bool(con->i16[min((unsigned long)i, 3ul)]);
				case typeInt16:
					return bool(*con->i16);
				case typeVec2Int8:
					return bool(con->i8[min((unsigned long)i, 1ul)]);
				case typeVec3Int8:
					return bool(con->i8[min((unsigned long)i, 2ul)]);
				case typeVec4Int8:
					return bool(con->i8[min((unsigned long)i, 3ul)]);
				case typeInt8:
					return bool(*con->i8);
				case typeVec2UInt64:
					return bool(con->u64[min((unsigned long)i, 1ul)]);
				case typeVec3UInt64:
					return bool(con->u64[min((unsigned long)i, 2ul)]);
				case typeVec4UInt64:
					return bool(con->u64[min((unsigned long)i, 3ul)]);
				case typeUInt64:
					return bool(*con->u64);
				case typeVec2UInt32:
					return bool(con->u32[min((unsigned long)i, 1ul)]);
				case typeVec3UInt32:
					return bool(con->u32[min((unsigned long)i, 2ul)]);
				case typeVec4UInt32:
					return bool(con->u32[min((unsigned long)i, 3ul)]);
				case typeUInt32:
					return bool(*con->u32);
				case typeVec2UInt16:
					return bool(con->u16[min((unsigned long)i, 1ul)]);
				case typeVec3UInt16:
					return bool(con->u16[min((unsigned long)i, 2ul)]);
				case typeVec4UInt16:
					return bool(con->u16[min((unsigned long)i, 3ul)]);
				case typeUInt16:
					return bool(*con->u16);
				case typeVec2UInt8:
					return bool(con->u8[min((unsigned long)i, 1ul)]);
				case typeVec3UInt8:
					return bool(con->u8[min((unsigned long)i, 2ul)]);
				case typeVec4UInt8:
					return bool(con->u8[min((unsigned long)i, 3ul)]);
				case typeUInt8:
					return bool(*con->u8);
				case typeVec2Float:
					return bool(con->f[min((unsigned long)i, 1ul)]);
				case typeVec3Float:
					return bool(con->f[min((unsigned long)i, 2ul)]);
				case typeVec4Float:
				case typeQuatFloat:
					return bool(con->f[min((unsigned long)i, 3ul)]);
				case typeMat3x3Float:
					return bool(con->f[min((unsigned long)i, 8ul)]);
				case typeMat4x4Float:
					return bool(con->f[min((unsigned long)i, 15ul)]);
				case typeFloat:
					return bool(*con->f);
				case typeVec2Double:
					return bool(con->d[min((unsigned long)i, 1ul)]);
				case typeVec3Double:
					return bool(con->d[min((unsigned long)i, 2ul)]);
				case typeVec4Double:
				case typeQuatDouble:
					return bool(con->d[min((unsigned long)i, 3ul)]);
				case typeMat3x3Double:
					return bool(con->d[min((unsigned long)i, 8ul)]);
				case typeMat4x4Double:
					return bool(con->d[min((unsigned long)i, 15ul)]);
				case typeDouble:
					return bool(*con->d);
				case typeBinary:
					return (*con->bin).size() > 0;
				case typeString:
					if ((*con->str) == "true" || (*con->str) == "1")
						return true;
					if ((*con->str) == "false" || (*con->str) == "0")
						return false;
				case typeNull:
				case typeList:
				case typeObject:
				case typeMethod:
				default:
					break;
			}
		}
		return false;
	}

	list var::getList() const {
		auto con = sPtr.get();
		if (con && con->type == typeList) return *con->li;
		return list();
	}

	object var::getObject() const {
		auto con = sPtr.get();
		if (con && con->type == typeObject) return *con->obj;
		return object();
	}

	void var::assignList(list& result) const {
		auto con = sPtr.get();
		if (con && con->type == typeList) result = *con->li;
	}

	void var::assignObject(obj& result) const {
		auto con = sPtr.get();
		if (con && con->type == typeObject) result = *con->obj;
	}

	method var::getMethod() const { return method(*this); }

	func var::getFunction() const { return func(*this); }

	void* var::getPtr() const { return (void*)*this; }

	genericError* var::getError() const {
		return (genericError*)*this;
	}

	binary var::getBinary() const {
		auto con = sPtr.get();
		if (con) {
			if (con->type == typeBinary)
				return *con->bin;
			else if (con->type == typeString)
				return binary(con->str->begin(), con->str->end());
			else if (con->type == typeStringView)
				return binary(con->sv->begin(), con->sv->end());
		}
		return binary();
	}

	void var::assignBinary(binary& result) const {
		auto con = sPtr.get();
		if (con) {
			if (con->type == typeBinary)
				result = *con->bin;
			else if (con->type == typeString) {
				auto str = *con->str;
				size_t size = str.size();
				result = binary(size);
				memcpy(result.data(), str.data(), size);
			} else if (con->type == typeStringView)
				result = binary(con->sv->begin(), con->sv->end());
		}
	}

	var::operator string_view() const { return getStringView(); }

	var::operator string() const {
		auto c = sPtr.get();
#define S std::to_string
		if (c) {
			switch (c->type) {
				case typeStringView:
					return string(*c->sv);
				case typeInt64:
					return S(*c->i64);
				case typePtr:
				case typeFunction:
				case typeMethod:
					return "0x" + S((uint64_t)c->data);
				case typeInt32:
					return S(*c->i32);
				case typeInt16:
					return S(*c->i16);
				case typeInt8:
					return S(*c->i8);
				case typeUInt64:
					return S(*c->u64);
				case typeUInt32:
					return S(*c->u32);
				case typeUInt16:
					return S(*c->u16);
				case typeUInt8:
					return S(*c->u8);
				case typeDouble:
					return S(*c->d);
				case typeFloat:
					return S(*c->f);
				case typeBool:
					return *c->b ? "true" : "false";
				case typeString:
					return *c->str;
				case typeNull:
					return "null";
				case typeList:
					return (c->li)->getJSON();
				case typeObject:
					return (c->obj)->getJSON();
				case typeException:
					return (c->err->operator std::string());
				case typeBinary:
					return string(c->bin->begin(), c->bin->end());

				case typeVec2Int64:
					return "[" + S(c->i64[0]) + ", " + S(c->i64[1]) + "]";
				case typeVec2Int32:
					return "[" + S(c->i32[0]) + ", " + S(c->i32[1]) + "]";
				case typeVec2Int16:
					return "[" + S(c->i16[0]) + ", " + S(c->i16[1]) + "]";
				case typeVec2Int8:
					return "[" + S(c->i8[0]) + ", " + S(c->i8[1]) + "]";
				case typeVec2UInt64:
					return "[" + S(c->u64[0]) + ", " + S(c->u64[1]) + "]";
				case typeVec2UInt32:
					return "[" + S(c->u32[0]) + ", " + S(c->u32[1]) + "]";
				case typeVec2UInt16:
					return "[" + S(c->u16[0]) + ", " + S(c->u16[1]) + "]";
				case typeVec2UInt8:
					return "[" + S(c->u8[0]) + ", " + S(c->u8[1]) + "]";

				case typeVec3Int64:
					return "[" + S(c->i64[0]) + ", " + S(c->i64[1]) +
								 ", " + S(c->i64[2]) + "]";
				case typeVec3Int32:
					return "[" + S(c->i32[0]) + ", " + S(c->i32[1]) +
								 ", " + S(c->i32[2]) + "]";
				case typeVec3Int16:
					return "[" + S(c->i16[0]) + ", " + S(c->i16[1]) +
								 ", " + S(c->i16[2]) + "]";
				case typeVec3Int8:
					return "[" + S(c->i8[0]) + ", " + S(c->i8[1]) + ", " +
								 S(c->i8[2]) + "]";
				case typeVec3UInt64:
					return "[" + S(c->u64[0]) + ", " + S(c->u64[1]) +
								 ", " + S(c->u64[2]) + "]";
				case typeVec3UInt32:
					return "[" + S(c->u32[0]) + ", " + S(c->u32[1]) +
								 ", " + S(c->u32[2]) + "]";
				case typeVec3UInt16:
					return "[" + S(c->u16[0]) + ", " + S(c->u16[1]) +
								 ", " + S(c->u16[2]) + "]";
				case typeVec3UInt8:
					return "[" + S(c->u8[0]) + ", " + S(c->u8[1]) + ", " +
								 S(c->u8[2]) + "]";

				case typeVec4Int64:
					return "[" + S(c->i64[0]) + ", " + S(c->i64[1]) +
								 ", " + S(c->i64[2]) + ", " + S(c->i64[3]) +
								 "]";
				case typeVec4Int32:
					return "[" + S(c->i32[0]) + ", " + S(c->i32[1]) +
								 ", " + S(c->i32[2]) + ", " + S(c->i32[3]) +
								 "]";
				case typeVec4Int16:
					return "[" + S(c->i16[0]) + ", " + S(c->i16[1]) +
								 ", " + S(c->i16[2]) + ", " + S(c->i16[3]) +
								 "]";
				case typeVec4Int8:
					return "[" + S(c->i8[0]) + ", " + S(c->i8[1]) + ", " +
								 S(c->i8[2]) + ", " + S(c->i8[3]) + "]";
				case typeVec4UInt64:
					return "[" + S(c->u64[0]) + ", " + S(c->u64[1]) +
								 ", " + S(c->u64[2]) + ", " + S(c->u64[3]) +
								 "]";
				case typeVec4UInt32:
					return "[" + S(c->u32[0]) + ", " + S(c->u32[1]) +
								 ", " + S(c->u32[2]) + ", " + S(c->u32[3]) +
								 "]";
				case typeVec4UInt16:
					return "[" + S(c->u16[0]) + ", " + S(c->u16[1]) +
								 ", " + S(c->u16[2]) + ", " + S(c->u16[3]) +
								 "]";
				case typeVec4UInt8:
					return "[" + S(c->u8[0]) + ", " + S(c->u8[1]) + ", " +
								 S(c->u8[2]) + ", " + S(c->u8[3]) + "]";

				case typeVec2Float:
					return "[" + S(c->f[0]) + ", " + S(c->f[1]) + "]";
				case typeVec2Double:
					return "[" + S(c->d[0]) + ", " + S(c->d[1]) + "]";
				case typeVec3Float:
					return "[" + S(c->f[0]) + ", " + S(c->f[1]) + ", " +
								 S(c->f[2]) + "]";
				case typeVec3Double:
					return "[" + S(c->d[0]) + ", " + S(c->d[1]) + ", " +
								 S(c->d[2]) + "]";
				case typeQuatFloat:
				case typeVec4Float:
					return "[" + S(c->f[0]) + ", " + S(c->f[1]) + ", " +
								 S(c->f[2]) + ", " + S(c->f[3]) + "]";
				case typeQuatDouble:
				case typeVec4Double:
					return "[" + S(c->d[0]) + ", " + S(c->d[1]) + ", " +
								 S(c->d[2]) + ", " + S(c->d[3]) + "]";

				case typeMat3x3Float:
					return "[" + S(c->f[0]) + ", " + S(c->f[1]) + ", " +
								 S(c->f[2]) + ", " + S(c->f[3]) + ", " +
								 S(c->f[4]) + ", " + S(c->f[5]) + ", " +
								 S(c->f[6]) + ", " + S(c->f[7]) + ", " +
								 S(c->f[8]) + "]";
				case typeMat3x3Double:
					return "[" + S(c->d[0]) + ", " + S(c->d[1]) + ", " +
								 S(c->d[2]) + ", " + S(c->d[3]) + ", " +
								 S(c->d[4]) + ", " + S(c->d[5]) + ", " +
								 S(c->d[6]) + ", " + S(c->d[7]) + ", " +
								 S(c->d[8]) + "]";

				case typeMat4x4Float:
					return "[" + S(c->f[0]) + ", " + S(c->f[1]) + ", " +
								 S(c->f[2]) + ", " + S(c->f[3]) + ", " +
								 S(c->f[4]) + ", " + S(c->f[5]) + ", " +
								 S(c->f[6]) + ", " + S(c->f[7]) + ", " +
								 S(c->f[8]) + ", " + S(c->f[9]) + ", " +
								 S(c->f[10]) + ", " + S(c->f[11]) + ", " +
								 S(c->f[12]) + ", " + S(c->f[13]) + ", " +
								 S(c->f[14]) + ", " + S(c->f[15]) + "]";
				case typeMat4x4Double:
					return "[" + S(c->d[0]) + ", " + S(c->d[1]) + ", " +
								 S(c->d[2]) + ", " + S(c->d[3]) + ", " +
								 S(c->d[4]) + ", " + S(c->d[5]) + ", " +
								 S(c->d[6]) + ", " + S(c->d[7]) + ", " +
								 S(c->d[8]) + ", " + S(c->d[9]) + ", " +
								 S(c->d[10]) + ", " + S(c->d[11]) + ", " +
								 S(c->d[12]) + ", " + S(c->d[13]) + ", " +
								 S(c->d[14]) + ", " + S(c->d[15]) + "]";
				default:
					break;
			}
		}
#undef S

		return "";
	}

	var::operator int64_t() const { return getInt64(); }

	var::operator int32_t() const { return getInt32(); }

	var::operator int16_t() const { return getInt16(); }

	var::operator int8_t() const { return getInt8(); }

	var::operator uint64_t() const { return getUInt64(); }

	var::operator uint32_t() const { return getUInt32(); }

	var::operator uint16_t() const { return getUInt16(); }

	var::operator uint8_t() const { return getUInt8(); }

	var::operator double() const { return getDouble(); }

	var::operator float() const { return getFloat(); }

	var::operator bool() const { return getBool(); }

	var::operator void*() const {
		auto con = sPtr.get();
		if (con) return (void*)con->data;
		return nullptr;
	}

	var::operator method() const {
		auto con = sPtr.get();
		if (con && con->type == typeMethod) return (*con->me);
		return nullptr;
	}

	var::operator func() const {
		auto con = sPtr.get();
		if (con && con->type == typeFunction) return (*con->fu);
		return nullptr;
	}

	var::operator genericError*() const {
		auto con = sPtr.get();
		if (con && con->type == typeException) return (con->err);
		return nullptr;
	}

	ostream& operator<<(ostream& os, const var& v) {
		os << v.operator std::string();
		return os;
	}

	var var::operator()(obj self, list args) const {
		auto func = getMethod();
		if (func != nullptr) return (self.*func)(args);
		return var();
	}

	var var::operator()(obj self) const {
		auto func = getMethod();
		if (func != nullptr) {
			auto empty = list();
			return (self.*func)(empty);
		}
		return var();
	}

	var var::operator()(list args) const {
		auto con = sPtr.get();
		if (con && con->type == typeFunction)
			return (*con->fu)(args);
		return var();
	}

	var vec2i64(int64_t x, int64_t y) {
		return var(typeVec2Int64, {x, y});
	}

	var vec3i64(int64_t x, int64_t y, int64_t z) {
		return var(typeVec3Int64, {x, y, z});
	}

	var vec4i64(int64_t x, int64_t y, int64_t z, int64_t w) {
		return var(typeVec4Int64, {x, y, z, w});
	}

	var vec2i32(int32_t x, int32_t y) {
		return var(typeVec2Int32, {x, y});
	}

	var vec3i32(int32_t x, int32_t y, int32_t z) {
		return var(typeVec3Int32, {x, y, z});
	}

	var vec4i32(int32_t x, int32_t y, int32_t z, int32_t w) {
		return var(typeVec4Int32, {x, y, z, w});
	}

	var vec2i16(int16_t x, int16_t y) {
		return var(typeVec2Int16, {x, y});
	}

	var vec3i16(int16_t x, int16_t y, int16_t z) {
		return var(typeVec3Int16, {x, y, z});
	}

	var vec4i16(int16_t x, int16_t y, int16_t z, int16_t w) {
		return var(typeVec4Int16, {x, y, z, w});
	}

	var vec2i8(int8_t x, int8_t y) {
		return var(typeVec2Int8, {x, y});
	}

	var vec3i8(int8_t x, int8_t y, int8_t z) {
		return var(typeVec3Int8, {x, y, z});
	}

	var vec4i8(int8_t x, int8_t y, int8_t z, int8_t w) {
		return var(typeVec4Int8, {x, y, z, w});
	}

	var vec2u64(uint64_t x, uint64_t y) {
		return var(typeVec2UInt64, {x, y});
	}

	var vec3u64(uint64_t x, uint64_t y, uint64_t z) {
		return var(typeVec3UInt64, {x, y, z});
	}

	var vec4u64(uint64_t x, uint64_t y, uint64_t z, uint64_t w) {
		return var(typeVec4UInt64, {x, y, z, w});
	}

	var vec2u32(uint32_t x, uint32_t y) {
		return var(typeVec2UInt32, {x, y});
	}

	var vec3u32(uint32_t x, uint32_t y, uint32_t z) {
		return var(typeVec3UInt32, {x, y, z});
	}

	var vec4u32(uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
		return var(typeVec4UInt32, {x, y, z, w});
	}

	var vec2u16(uint16_t x, uint16_t y) {
		return var(typeVec2UInt16, {x, y});
	}

	var vec3u16(uint16_t x, uint16_t y, uint16_t z) {
		return var(typeVec3UInt16, {x, y, z});
	}

	var vec4u16(uint16_t x, uint16_t y, uint16_t z, uint16_t w) {
		return var(typeVec4UInt16, {x, y, z, w});
	}

	var vec2u8(uint8_t x, uint8_t y) {
		return var(typeVec2UInt8, {x, y});
	}

	var vec3u8(uint8_t x, uint8_t y, uint8_t z) {
		return var(typeVec3UInt8, {x, y, z});
	}

	var vec4u8(uint8_t x, uint8_t y, uint8_t z, uint8_t w) {
		return var(typeVec4UInt8, {x, y, z, w});
	}

	var vec2f(float x, float y) {
		return var(typeVec2Float, {x, y});
	}

	var vec3f(float x, float y, float z) {
		return var(typeVec3Float, {x, y, z});
	}

	var vec4f(float x, float y, float z, float w) {
		return var(typeVec4Float, {x, y, z, w});
	}

	var vec2d(double x, double y) {
		return var(typeVec2Double, {x, y});
	}

	var vec3d(double x, double y, double z) {
		return var(typeVec3Double, {x, y, z});
	}

	var vec4d(double x, double y, double z, double w) {
		return var(typeVec4Double, {x, y, z, w});
	}

	var quatf(float x, float y, float z, float w) {
		return var(typeQuatFloat, {x, y, z, w});
	}

	var quatd(double x, double y, double z, double w) {
		return var(typeQuatDouble, {x, y, z, w});
	}

	var mat3x3f(initializer_list<float> list) {
		if (list.size() != 9)
			return var(
				typeMat3x3Float,
				{
					1.f, 0.f, 0.f,  // Row 1
					0.f, 1.f, 0.f,  // Row 2
					0.f, 0.f, 1.f,  // Row 3
				});
		return var(typeMat3x3Float, list);
	}

	var mat3x3d(initializer_list<double> list) {
		if (list.size() != 9)
			return var(
				typeMat3x3Double,
				{
					1., 0., 0.,  // Row 1
					0., 1., 0.,  // Row 2
					0., 0., 1.,  // Row 3
				});
		return var(typeMat3x3Double, list);
	}

	var mat4x4f(initializer_list<float> list) {
		if (list.size() != 16)
			return var(
				typeMat4x4Float,
				{
					1.f, 0.f, 0.f, 0.f,  // Row 1
					0.f, 1.f, 0.f, 0.f,  // Row 2
					0.f, 0.f, 1.f, 0.f,  // Row 3
					0.f, 0.f, 0.f, 1.f,  // Row 3
				});
		return var(typeMat4x4Float, list);
	}

	var mat4x4d(initializer_list<double> list) {
		if (list.size() != 16)
			return var(
				typeMat4x4Double,
				{
					1., 0., 0., 0.,  // Row 1
					0., 1., 0., 0.,  // Row 2
					0., 0., 1., 0.,  // Row 3
					0., 0., 0., 1.,  // Row 3
				});
		return var(typeMat4x4Double, list);
	}

	var lookAt(var eye, var at) {
		auto x = mat4x4f({});
		bx::mtxLookAt(
			(float*)x.getPtr(),
			bx::Vec3(
				eye.getFloat(0), eye.getFloat(1), eye.getFloat(2)),
			bx::Vec3(at.getFloat(0), at.getFloat(1), at.getFloat(2)));
		return x;
	}

	var projection(
		var fov, var ratio, var near, var far, var homo) {
		auto x = mat4x4f({});
		bx::mtxProj(
			(float*)x.getPtr(),
			fov.getFloat(),
			ratio.getFloat(),
			near.getFloat(),
			far.getFloat(),
			homo.getBool());
		return x;
	}

	list explode(string v, list chars) {
		auto numList = list({});
		auto numBuff = string("");

		auto isDelimiter = [&](char c) -> bool {
			for (auto bi = chars.begin(); bi != chars.end(); ++bi) {
				if (uint8_t(*bi) == uint8_t(c)) return true;
			}
			return false;
		};

		size_t i = 0;
		while (i < v.size()) {
			if (isDelimiter(v[i])) {
				numList.pushString(numBuff);
				numBuff = "";
				// Skip all consecutive delimiters.
				while (i < v.size() && isDelimiter(v[i])) ++i;
			} else {
				numBuff.push_back(v[i]);
				++i;
			}
		}
		// Drop a trailing empty token produced by a trailing delimiter,
		// but keep a single empty token for empty input.
		if (numBuff.size() > 0 || numList.size() == 0)
			numList.pushString(numBuff);
		return numList;
	}

}  // namespace gold
