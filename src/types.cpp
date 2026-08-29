#include "types.hpp"

#include <sstream>
#include <bx/platform.h>

namespace gold {
	using namespace std;

	const var nullVar = var();
	const auto voidPSize = sizeof(void*);

	bool isCopyable(types type) {
		if (type == typeString) return true;
#ifdef BX_ARCH_64BIT
		if (type == typeUInt64)
			return true;
		if (type == typeInt64)
			return true;
		if (type == typeDouble)
			return true;
#else
		if (type == typeUInt32) return true;
		if (type == typeInt32) return true;
		if (type == typeFloat) return true;
#endif
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

	genericError::genericError(const genericError& copy)
		: exception(copy), object(copy) {}

	genericError::genericError(
		string_view message,
		const char* _file,
		const char* _func,
		const int _line)
		: exception(), object() {
		setString("msg", string(message));
		setString("file", _file);
		setString("func", _func);
		setInt32("line", _line);
	}

	genericError::operator string() const {
		auto ss = stringstream();
		ss.operator<<(*this);
		return ss.str();
	}

	ostream& operator<<(ostream& os, genericError& dt) {
		return os << dt.getString("file") << ":"
							<< dt.getInt32("line") << " ("
							<< dt.getString("func")
							<< "):" << dt.getString("msg");
	}

}  // namespace gold