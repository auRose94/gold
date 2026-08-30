#pragma once
#include "types.hpp"

namespace gold {
namespace var_ops {

// Element count for vector/matrix types
inline size_t vecSize(types_t t) {
    switch (t) {
        case typeVec2Float: case typeVec2Double: case typeVec2Int64: case typeVec2Int32:
        case typeVec2Int16: case typeVec2Int8: case typeVec2UInt64: case typeVec2UInt32:
        case typeVec2UInt16: case typeVec2UInt8:
            return 2;
        case typeVec3Float: case typeVec3Double: case typeVec3Int64: case typeVec3Int32:
        case typeVec3Int16: case typeVec3Int8: case typeVec3UInt64: case typeVec3UInt32:
        case typeVec3UInt16: case typeVec3UInt8:
            return 3;
        case typeVec4Float: case typeVec4Double: case typeVec4Int64: case typeVec4Int32:
        case typeVec4Int16: case typeVec4Int8: case typeVec4UInt64: case typeVec4UInt32:
        case typeVec4UInt16: case typeVec4UInt8:
            return 4;
        case typeQuatFloat: case typeQuatDouble:
            return 4;
        case typeMat3x3Float: case typeMat3x3Double:
            return 9;
        case typeMat4x4Float: case typeMat4x4Double:
            return 16;
        default:
            return 0;
    }
}

// Generic element-wise equality
inline bool equalVec(const var& a, const var& b) {
    if (a.getType() != b.getType()) return false;
    size_t n = vecSize(a.getType());
    if (n == 0) return false;
    for (size_t i = 0; i < n; ++i) {
        // Use generic getters; they convert as needed
        if (a.getType() == typeVec2Float || a.getType() == typeVec3Float || a.getType() == typeVec4Float ||
            a.getType() == typeQuatFloat || a.getType() == typeMat3x3Float || a.getType() == typeMat4x4Float) {
            if (a.getFloat(i) != b.getFloat(i)) return false;
        } else if (a.getType() == typeVec2Double || a.getType() == typeVec3Double || a.getType() == typeVec4Double ||
                   a.getType() == typeQuatDouble || a.getType() == typeMat3x3Double || a.getType() == typeMat4x4Double) {
            if (a.getDouble(i) != b.getDouble(i)) return false;
        } else {
            if (a.getInt64(i) != b.getInt64(i)) return false;
        }
    }
    return true;
}

} // namespace var_ops
} // namespace gold
