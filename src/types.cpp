#include "types.hpp"

#include <sstream>

namespace gold {

	const var nullVar = var();

	bool isCopyable(types type) {
		if (type == typeString) return true;
		if (sizeof(uint64_t) > sizeof(void*) && type == typeUInt64)
			return true;
		if (sizeof(int64_t) > sizeof(void*) && type == typeInt64)
			return true;
		if (sizeof(double) > sizeof(void*) && type == typeDouble)
			return true;
		return false;
	}

	const char* getTypeString(types type) {
		switch (type) {
			case typeList:
				return "List";
			case typeObject:
				return "Object";
			case typeMethod:
				return "Method";
			case typeFunction:
				return "Function";
			case typePtr:
				return "Pointer";
			case typeString:
				return "String";
			case typeStringView:
				return "StringView";
			case typeBinary:
				return "Binary";
			case typeInt64:
				return "Int64";
			case typeInt32:
				return "Int32";
			case typeInt16:
				return "Int16";
			case typeInt8:
				return "Int8";
			case typeUInt64:
				return "UInt64";
			case typeUInt32:
				return "UInt32";
			case typeUInt16:
				return "UInt16";
			case typeUInt8:
				return "UInt8";
			case typeDouble:
				return "Double";
			case typeFloat:
				return "Float";
			case typeBool:
				return "Bool";
			case typeNull:
				return "Null";
			case typeException:
				return "Exception";
			case typeVec2Int64:
				return "Vec2Int64";
			case typeVec2Int32:
				return "Vec2Int32";
			case typeVec2Int16:
				return "Vec2Int16";
			case typeVec2Int8:
				return "Vec2Int8";
			case typeVec2UInt64:
				return "Vec2UInt64";
			case typeVec2UInt32:
				return "Vec2UInt32";
			case typeVec2UInt16:
				return "Vec2UInt16";
			case typeVec2UInt8:
				return "Vec2UInt8";
			case typeVec3Int64:
				return "Vec3Int64";
			case typeVec3Int32:
				return "Vec3Int32";
			case typeVec3Int16:
				return "Vec3Int16";
			case typeVec3Int8:
				return "Vec3Int8";
			case typeVec3UInt64:
				return "Vec3UInt64";
			case typeVec3UInt32:
				return "Vec3UInt32";
			case typeVec3UInt16:
				return "Vec3UInt16";
			case typeVec3UInt8:
				return "Vec3UInt8";
			case typeVec4Int64:
				return "Vec4Int64";
			case typeVec4Int32:
				return "Vec4Int32";
			case typeVec4Int16:
				return "Vec4Int16";
			case typeVec4Int8:
				return "Vec4Int8";
			case typeVec4UInt64:
				return "Vec4UInt64";
			case typeVec4UInt32:
				return "Vec4UInt32";
			case typeVec4UInt16:
				return "Vec4UInt16";
			case typeVec4UInt8:
				return "Vec4UInt8";
			case typeVec2Float:
				return "Vec2Float";
			case typeVec2Double:
				return "Vec2Double";
			case typeVec3Float:
				return "Vec3Float";
			case typeVec3Double:
				return "Vec3Double";
			case typeVec4Float:
				return "Vec4Float";
			case typeVec4Double:
				return "Vec4Double";
			case typeQuatFloat:
				return "QuatFloat";
			case typeQuatDouble:
				return "QuatDouble";
			case typeMat3x3Float:
				return "Mat3x3Float";
			case typeMat3x3Double:
				return "Mat3x3Double";
			case typeMat4x4Float:
				return "Mat4x4Float";
			case typeMat4x4Double:
				return "Mat4x4Double";
		}
		return "Undefined";
	}

	genericError::genericError(
		const exception& copy, const sourceLocation& l)
		: exception(copy), msg(), loc(l) {}

	genericError::genericError(const genericError& copy)
		: exception(copy), msg(copy.msg), loc(copy.loc) {}

	genericError::genericError(
		string_view message, const sourceLocation& l)
		: exception(), msg(message), loc(l) {}

	genericError::operator string() const {
		auto ss = stringstream();
		ss << (*this);
		return ss.str();
	}

	ostream& operator<<(ostream& os, const genericError& dt) {
		return os << dt.loc.file_name() << ":" << dt.loc.line()
							<< ":" << dt.loc.column() << ":" << dt.msg;
	}

}  // namespace gold