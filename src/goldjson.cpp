#include "goldjson.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace gold {
	using namespace std;

	namespace {

		// ------------------------------------------------------------------
		// shared helpers
		// ------------------------------------------------------------------

		static string fmtDouble(double d) {
			if (!std::isfinite(d))
				throw runtime_error("JSON: non-finite number");
			char buf[64];
			auto r = to_chars(buf, buf + sizeof(buf), d);
			string s(buf, r.ptr);
			// Keep integral-valued floats as JSON floats so they parse back
			// as doubles, not integers.
			if (s.find_first_of(".eE") == string::npos) s += ".0";
			return s;
		}

		static string fmtFloat(float f) {
			if (!std::isfinite(f))
				throw runtime_error("JSON: non-finite number");
			char buf[32];
			auto r = to_chars(buf, buf + sizeof(buf), f);
			string s(buf, r.ptr);
			if (s.find_first_of(".eE") == string::npos) s += ".0";
			return s;
		}

		static void appendJSONString(string& out, string_view s) {
			out += '"';
			for (unsigned char c : s) {
				switch (c) {
					case '"': out += "\\\""; break;
					case '\\': out += "\\\\"; break;
					case '\b': out += "\\b"; break;
					case '\f': out += "\\f"; break;
					case '\n': out += "\\n"; break;
					case '\r': out += "\\r"; break;
					case '\t': out += "\\t"; break;
					default:
						if (c < 0x20) {
							char buf[7];
							snprintf(buf, sizeof(buf), "\\u%04x", c);
							out += buf;
						} else {
							out += char(c);
						}
				}
			}
			out += '"';
		}

		// Reconstruct a list of uniform numbers into a gold vec/mat type,
		// mirroring the typed-array behavior of the old json2List path.
		static var reconstructList(list li) {
			auto s = li.size();
			if (s == 2 || s == 3 || s == 4 || s == 9 || s == 16) {
				bool hasF = false, allI = true, allU = true;
				for (uint64_t i = 0; i < s; ++i) {
					auto t = li.getType(i);
					bool isF = t == typeDouble || t == typeFloat;
					bool isI = t == typeInt64 || t == typeInt32 ||
						t == typeInt16 || t == typeInt8;
					bool isU = t == typeUInt64 || t == typeUInt32 ||
						t == typeUInt16 || t == typeUInt8;
					if (isF) hasF = true;
					if (!isI) allI = false;
					if (!isU) allU = false;
					if (!hasF && !allI && !allU) return li;
				}
				if (hasF) {
					switch (s) {
						case 2: return var(vec2f(li.getFloat(0), li.getFloat(1)));
						case 3: return var(vec3f(li.getFloat(0), li.getFloat(1), li.getFloat(2)));
						case 4: return var(vec4f(li.getFloat(0), li.getFloat(1), li.getFloat(2), li.getFloat(3)));
						case 9: return var(mat3x3f({li.getFloat(0), li.getFloat(1), li.getFloat(2), li.getFloat(3), li.getFloat(4), li.getFloat(5), li.getFloat(6), li.getFloat(7), li.getFloat(8)}));
						case 16: return var(mat4x4f({li.getFloat(0), li.getFloat(1), li.getFloat(2), li.getFloat(3), li.getFloat(4), li.getFloat(5), li.getFloat(6), li.getFloat(7), li.getFloat(8), li.getFloat(9), li.getFloat(10), li.getFloat(11), li.getFloat(12), li.getFloat(13), li.getFloat(14), li.getFloat(15)}));
					}
				} else if (allU) {
					switch (s) {
						case 2: return var(vec2u32(li.getUInt32(0), li.getUInt32(1)));
						case 3: return var(vec3u32(li.getUInt32(0), li.getUInt32(1), li.getUInt32(2)));
						case 4: return var(vec4u32(li.getUInt32(0), li.getUInt32(1), li.getUInt32(2), li.getUInt32(3)));
					}
				} else if (allI) {
					switch (s) {
						case 2: return var(vec2i32(li.getInt32(0), li.getInt32(1)));
						case 3: return var(vec3i32(li.getInt32(0), li.getInt32(1), li.getInt32(2)));
						case 4: return var(vec4i32(li.getInt32(0), li.getInt32(1), li.getInt32(2), li.getInt32(3)));
					}
				}
			}
			return var(li);
		}

		// ------------------------------------------------------------------
		// JSON
		// ------------------------------------------------------------------

		class jsonReader {
			string_view s;
			size_t pos = 0;

			char peek() const { return pos < s.size() ? s[pos] : '\0'; }
			void skipWs() {
				while (pos < s.size() &&
					isspace((unsigned char)s[pos]))
					pos++;
			}
			void expect(char c) {
				if (peek() != c)
					throw runtime_error("JSON: unexpected character");
				pos++;
			}
			uint32_t hex4() {
				if (pos + 4 > s.size())
					throw runtime_error("JSON: bad \\u escape");
				uint32_t v = 0;
				for (int i = 0; i < 4; ++i) {
					char c = s[pos++];
					v <<= 4;
					if (c >= '0' && c <= '9') v |= c - '0';
					else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
					else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
					else throw runtime_error("JSON: bad \\u escape");
				}
				return v;
			}

		 public:
			jsonReader(string_view d) : s(d) {}

			string parseString() {
				expect('"');
				string out;
				while (true) {
					if (pos >= s.size())
						throw runtime_error("JSON: unterminated string");
					char c = s[pos++];
					if (c == '"') break;
					if (c == '\\') {
						char e = s[pos++];
						switch (e) {
							case '"': out += '"'; break;
							case '\\': out += '\\'; break;
							case '/': out += '/'; break;
							case 'b': out += '\b'; break;
							case 'f': out += '\f'; break;
							case 'n': out += '\n'; break;
							case 'r': out += '\r'; break;
							case 't': out += '\t'; break;
							case 'u': {
								uint32_t cp = hex4();
								if (cp >= 0xD800 && cp <= 0xDBFF) {
									// expect a low surrogate
									if (pos + 1 < s.size() && s[pos] == '\\' &&
										s[pos + 1] == 'u') {
										pos += 2;
										uint32_t lo = hex4();
										if (lo >= 0xDC00 && lo <= 0xDFFF) {
											cp = 0x10000 +
												((cp - 0xD800) << 10) +
												(lo - 0xDC00);
										} else {
											throw runtime_error(
												"JSON: bad surrogate pair");
										}
									} else {
										throw runtime_error(
											"JSON: bad surrogate pair");
									}
								}
								// UTF-8 encode cp
								if (cp < 0x80) out += char(cp);
								else if (cp < 0x800) {
									out += char(0xC0 | (cp >> 6));
									out += char(0x80 | (cp & 0x3F));
								} else if (cp < 0x10000) {
									out += char(0xE0 | (cp >> 12));
									out += char(0x80 | ((cp >> 6) & 0x3F));
									out += char(0x80 | (cp & 0x3F));
								} else {
									out += char(0xF0 | (cp >> 18));
									out += char(0x80 | ((cp >> 12) & 0x3F));
									out += char(0x80 | ((cp >> 6) & 0x3F));
									out += char(0x80 | (cp & 0x3F));
								}
								break;
							}
							default:
								throw runtime_error("JSON: bad escape");
						}
					} else {
						out += c;
					}
				}
				return out;
			}

			var parseNumber() {
				size_t start = pos;
				bool isFloat = false;
				if (peek() == '-') pos++;
				while (isdigit((unsigned char)peek())) pos++;
				if (peek() == '.') {
					isFloat = true;
					pos++;
					while (isdigit((unsigned char)peek())) pos++;
				}
				if (peek() == 'e' || peek() == 'E') {
					isFloat = true;
					pos++;
					if (peek() == '+' || peek() == '-') pos++;
					while (isdigit((unsigned char)peek())) pos++;
				}
				if (pos == start || (pos == start + 1 && s[start] == '-'))
					throw runtime_error("JSON: bad number");
				auto text = s.substr(start, pos - start);
				if (isFloat)
					return var(strtod(string(text).c_str(), nullptr));
				// JSON integer: negative -> signed, positive -> unsigned.
				if (text[0] == '-')
					return var((int64_t)strtoll(string(text).c_str(), nullptr, 10));
				return var((uint64_t)strtoull(string(text).c_str(), nullptr, 10));
			}

			bool literal(const char* lit) {
				size_t n = strlen(lit);
				if (pos + n > s.size() ||
					s.substr(pos, n) != string_view(lit, n))
					return false;
				pos += n;
				return true;
			}

			var parseValue() {
				skipWs();
				char c = peek();
				if (c == '{') return var(parseObject());
				if (c == '[') return parseArray();
				if (c == '"') return var(parseString());
				if (c == 't') { if (literal("true")) return var(true); throw runtime_error("JSON: bad literal"); }
				if (c == 'f') { if (literal("false")) return var(false); throw runtime_error("JSON: bad literal"); }
				if (c == 'n') { if (literal("null")) return var(); throw runtime_error("JSON: bad literal"); }
				if (c == '-' || isdigit((unsigned char)c)) return parseNumber();
				throw runtime_error("JSON: unexpected token");
			}

			object parseObject() {
				expect('{');
				skipWs();
				object o;
				if (peek() == '}') { pos++; return o; }
				while (true) {
					skipWs();
					auto name = parseString();
					skipWs();
					expect(':');
					auto value = parseValue();
					o.setVar(name, value);
					skipWs();
					if (peek() == ',') { pos++; continue; }
					if (peek() == '}') { pos++; break; }
					throw runtime_error("JSON: expected , or }");
				}
				return o;
			}

			var parseArray() {
				expect('[');
				skipWs();
				list li;
				if (peek() == ']') { pos++; return reconstructList(li); }
				while (true) {
					skipWs();
					li.pushVar(parseValue());
					skipWs();
					char c = peek();
					if (c == ',') { pos++; continue; }
					if (c == ']') { pos++; break; }
					throw runtime_error("JSON: expected , or ]");
				}
				return reconstructList(li);
			}

			void finish() {
				skipWs();
				if (pos != s.size())
					throw runtime_error("JSON: trailing data");
			}
		};

		// append a gold var as a JSON value.
		void appendJSONValue(var value, string& out, bool pretty, int indent);

		void appendJSONArray(list li, string& out, bool pretty, int indent) {
			out += '[';
			auto end = li.end();
			for (auto i = li.begin(); i != end; ++i) {
				if (i != li.begin()) out += ',';
				if (pretty) { out += '\n'; out += string(indent + 1, '\t'); }
				appendJSONValue(*i, out, pretty, indent + 1);
			}
			if (pretty && li.size() > 0) { out += '\n'; out += string(indent, '\t'); }
			out += ']';
		}

		void appendJSONObject(object o, string& out, bool pretty, int indent) {
			out += '{';
			auto end = o.end();
			for (auto i = o.begin(); i != end; ++i) {
				if (i != o.begin()) out += ',';
				if (pretty) { out += '\n'; out += string(indent + 1, '\t'); }
				appendJSONString(out, i->first);
				out += pretty ? ": " : ":";
				appendJSONValue(i->second, out, pretty, indent + 1);
			}
			if (pretty && o.size() > 0) { out += '\n'; out += string(indent, '\t'); }
			out += '}';
		}

		template <typename F>  // F: uint64_t -> string
		void appendNumberArray(var value, uint64_t n, F f, string& out,
			bool pretty, int indent) {
			(void)value;
			out += '[';
			for (uint64_t i = 0; i < n; ++i) {
				if (i) out += ',';
				if (pretty) { out += '\n'; out += string(indent + 1, '\t'); }
				out += f(i);
			}
			if (pretty && n) { out += '\n'; out += string(indent, '\t'); }
			out += ']';
		}

		void appendJSONValue(var value, string& out, bool pretty, int indent) {
			switch (value.getType()) {
				case typeNull: out += "null"; break;
				case typeBool: out += value.getBool() ? "true" : "false"; break;
				case typeString: appendJSONString(out, value.getString()); break;
				case typeStringView: appendJSONString(out, string(value.getStringView())); break;
				case typeInt64: out += to_string(value.getInt64()); break;
				case typeInt32: out += to_string(value.getInt32()); break;
				case typeInt16: out += to_string(value.getInt16()); break;
				case typeInt8: out += to_string(value.getInt8()); break;
				case typeUInt64: out += to_string(value.getUInt64()); break;
				case typeUInt32: out += to_string(value.getUInt32()); break;
				case typeUInt16: out += to_string(value.getUInt16()); break;
				case typeUInt8: out += to_string(value.getUInt8()); break;
				case typeDouble: out += fmtDouble(value.getDouble()); break;
				case typeFloat: out += fmtFloat(value.getFloat()); break;
				case typeList: appendJSONArray(value.getList(), out, pretty, indent); break;
				case typeObject: appendJSONObject(value.getObject(), out, pretty, indent); break;

				// vec/mat types expand to number arrays.
				case typeVec2Int64: case typeVec3Int64: case typeVec4Int64:
					appendNumberArray(value, value.getType() == typeVec2Int64 ? 2 : value.getType() == typeVec3Int64 ? 3 : 4,
						[&](uint64_t i){ return to_string(value.getInt64(i)); }, out, pretty, indent);
					break;
				case typeVec2Int32: case typeVec3Int32: case typeVec4Int32:
					appendNumberArray(value, value.getType() == typeVec2Int32 ? 2 : value.getType() == typeVec3Int32 ? 3 : 4,
						[&](uint64_t i){ return to_string(value.getInt32(i)); }, out, pretty, indent);
					break;
				case typeVec2Int16: case typeVec3Int16: case typeVec4Int16:
					appendNumberArray(value, value.getType() == typeVec2Int16 ? 2 : value.getType() == typeVec3Int16 ? 3 : 4,
						[&](uint64_t i){ return to_string(value.getInt16(i)); }, out, pretty, indent);
					break;
				case typeVec2Int8: case typeVec3Int8: case typeVec4Int8:
					appendNumberArray(value, value.getType() == typeVec2Int8 ? 2 : value.getType() == typeVec3Int8 ? 3 : 4,
						[&](uint64_t i){ return to_string(value.getInt8(i)); }, out, pretty, indent);
					break;
				case typeVec2UInt64: case typeVec3UInt64: case typeVec4UInt64:
					appendNumberArray(value, value.getType() == typeVec2UInt64 ? 2 : value.getType() == typeVec3UInt64 ? 3 : 4,
						[&](uint64_t i){ return to_string(value.getUInt64(i)); }, out, pretty, indent);
					break;
				case typeVec2UInt32: case typeVec3UInt32: case typeVec4UInt32:
					appendNumberArray(value, value.getType() == typeVec2UInt32 ? 2 : value.getType() == typeVec3UInt32 ? 3 : 4,
						[&](uint64_t i){ return to_string(value.getUInt32(i)); }, out, pretty, indent);
					break;
				case typeVec2UInt16: case typeVec3UInt16: case typeVec4UInt16:
					appendNumberArray(value, value.getType() == typeVec2UInt16 ? 2 : value.getType() == typeVec3UInt16 ? 3 : 4,
						[&](uint64_t i){ return to_string(value.getUInt16(i)); }, out, pretty, indent);
					break;
				case typeVec2UInt8: case typeVec3UInt8: case typeVec4UInt8:
					appendNumberArray(value, value.getType() == typeVec2UInt8 ? 2 : value.getType() == typeVec3UInt8 ? 3 : 4,
						[&](uint64_t i){ return to_string(value.getUInt8(i)); }, out, pretty, indent);
					break;
				case typeVec2Float: case typeVec3Float: case typeVec4Float: case typeQuatFloat:
					appendNumberArray(value, value.getType() == typeVec2Float ? 2 : value.getType() == typeVec3Float ? 3 : 4,
						[&](uint64_t i){ return fmtFloat(value.getFloat(i)); }, out, pretty, indent);
					break;
				case typeVec2Double: case typeVec3Double: case typeVec4Double: case typeQuatDouble:
					appendNumberArray(value, value.getType() == typeVec2Double ? 2 : value.getType() == typeVec3Double ? 3 : 4,
						[&](uint64_t i){ return fmtDouble(value.getDouble(i)); }, out, pretty, indent);
					break;
				case typeMat3x3Float: case typeMat3x3Double:
					appendNumberArray(value, 9,
						[&](uint64_t i){ return value.getType() == typeMat3x3Float ? fmtFloat(value.getFloat(i)) : fmtDouble(value.getDouble(i)); },
						out, pretty, indent);
					break;
				case typeMat4x4Float: case typeMat4x4Double:
					appendNumberArray(value, 16,
						[&](uint64_t i){ return value.getType() == typeMat4x4Float ? fmtFloat(value.getFloat(i)) : fmtDouble(value.getDouble(i)); },
						out, pretty, indent);
					break;
				default:
					out += "null";
					break;
			}
		}

		// ------------------------------------------------------------------
		// binary reader/writer
		// ------------------------------------------------------------------

		struct binReader {
			const uint8_t* p;
			size_t len;
			size_t pos = 0;

			void need(size_t n) {
				if (pos + n > len) throw runtime_error("truncated data");
			}
			uint8_t u8() { need(1); return p[pos++]; }
			uint16_t be16() { need(2); uint16_t v = (uint16_t(p[pos]) << 8) | p[pos + 1]; pos += 2; return v; }
			uint32_t be32() { need(4); uint32_t v = 0; for (int i = 0; i < 4; ++i) v = (v << 8) | p[pos + i]; pos += 4; return v; }
			uint64_t be64() { need(8); uint64_t v = 0; for (int i = 0; i < 8; ++i) v = (v << 8) | p[pos + i]; pos += 8; return v; }
			uint32_t le32() { need(4); uint32_t v = 0; for (int i = 3; i >= 0; --i) v = (v << 8) | p[pos + i]; pos += 4; return v; }
			uint64_t le64() { need(8); uint64_t v = 0; for (int i = 7; i >= 0; --i) v = (v << 8) | p[pos + i]; pos += 8; return v; }
			double le64d() { uint64_t v = le64(); double d; memcpy(&d, &v, 8); return d; }
			void skip(size_t n) { need(n); pos += n; }
			string_view take(size_t n) { need(n); auto v = string_view((const char*)p + pos, n); pos += n; return v; }
			bool eof() const { return pos >= len; }
		};

		struct binWriter {
			binary buf;
			void u8(uint8_t v) { buf.push_back(v); }
			void be16(uint16_t v) { buf.push_back(v >> 8); buf.push_back(v); }
			void be32(uint32_t v) { buf.push_back(v >> 24); buf.push_back(v >> 16); buf.push_back(v >> 8); buf.push_back(v); }
			void be64(uint64_t v) { for (int i = 7; i >= 0; --i) buf.push_back(v >> (i * 8)); }
			void le32(uint32_t v) { for (int i = 0; i < 4; ++i) buf.push_back(v >> (i * 8)); }
			void le64(uint64_t v) { for (int i = 0; i < 8; ++i) buf.push_back(v >> (i * 8)); }
			void le64d(double d) { uint64_t v; memcpy(&v, &d, 8); le64(v); }
			void bytes(const void* d, size_t n) { buf.insert(buf.end(), (const uint8_t*)d, (const uint8_t*)d + n); }
			void str(string_view s) { bytes(s.data(), s.size()); }
		};

		// ------------------------------------------------------------------
		// BSON
		// ------------------------------------------------------------------

		void bsonEncodeValue(binary& out, var value);

		void bsonEncodeKeyValue(binary& out, const string& name, var value) {
			switch (value.getType()) {
				case typeObject: {
					out.push_back(0x03);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					bsonEncodeValue(out, value);
					break;
				}
				case typeList: {
					out.push_back(0x04);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					bsonEncodeValue(out, value);
					break;
				}
				case typeString: case typeStringView: {
					out.push_back(0x02);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					string s = value.getString();
					uint32_t n = uint32_t(s.size()) + 1;
					out.push_back(n & 0xFF); out.push_back((n >> 8) & 0xFF);
					out.push_back((n >> 16) & 0xFF); out.push_back((n >> 24) & 0xFF);
					out.insert(out.end(), s.begin(), s.end());
					out.push_back(0);
					break;
				}
				case typeBool: {
					out.push_back(0x08);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					out.push_back(value.getBool() ? 1 : 0);
					break;
				}
				case typeNull: {
					out.push_back(0x0A);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					break;
				}
				case typeDouble: {
					out.push_back(0x01);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					double dv = value.getDouble(); uint64_t v; memcpy(&v, &dv, 8);
					// little-endian double
					for (int i = 0; i < 8; ++i) out.push_back(v >> (i * 8));
					break;
				}
				case typeFloat: {
					out.push_back(0x01);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					double d = value.getFloat();
					uint64_t v; memcpy(&v, &d, 8);
					for (int i = 0; i < 8; ++i) out.push_back(v >> (i * 8));
					break;
				}
				case typeInt64: {
					out.push_back(0x12);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					int64_t v = value.getInt64();
					for (int i = 0; i < 8; ++i) out.push_back((uint64_t(v) >> (i * 8)) & 0xFF);
					break;
				}
				case typeUInt64: {
					out.push_back(0x12);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					uint64_t v = value.getUInt64();
					for (int i = 0; i < 8; ++i) out.push_back((v >> (i * 8)) & 0xFF);
					break;
				}
				case typeInt32: case typeInt16: case typeInt8:
				case typeUInt32: case typeUInt16: case typeUInt8: {
					out.push_back(0x10);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					int32_t v = value.getInt32();
					for (int i = 0; i < 4; ++i) out.push_back((uint32_t(v) >> (i * 8)) & 0xFF);
					break;
				}
				default: {
					// vec/mat and anything else encode as an array.
					list li;
					auto t = value.getType();
					uint64_t n = 0;
					switch (t) {
						case typeVec2Int64: case typeVec2Int32: case typeVec2Int16: case typeVec2Int8:
						case typeVec2UInt64: case typeVec2UInt32: case typeVec2UInt16: case typeVec2UInt8:
						case typeVec2Float: case typeVec2Double: n = 2; break;
						case typeVec3Int64: case typeVec3Int32: case typeVec3Int16: case typeVec3Int8:
						case typeVec3UInt64: case typeVec3UInt32: case typeVec3UInt16: case typeVec3UInt8:
						case typeVec3Float: case typeVec3Double: n = 3; break;
						case typeVec4Int64: case typeVec4Int32: case typeVec4Int16: case typeVec4Int8:
						case typeVec4UInt64: case typeVec4UInt32: case typeVec4UInt16: case typeVec4UInt8:
						case typeVec4Float: case typeVec4Double: case typeQuatFloat: case typeQuatDouble: n = 4; break;
						case typeMat3x3Float: case typeMat3x3Double: n = 9; break;
						case typeMat4x4Float: case typeMat4x4Double: n = 16; break;
						default: break;
					}
					bool isF = t == typeVec2Float || t == typeVec3Float || t == typeVec4Float ||
						t == typeQuatFloat || t == typeMat3x3Float || t == typeMat4x4Float;
					bool isU = t == typeVec2UInt64 || t == typeVec3UInt64 || t == typeVec4UInt64 ||
						t == typeVec2UInt32 || t == typeVec3UInt32 || t == typeVec4UInt32 ||
						t == typeVec2UInt16 || t == typeVec3UInt16 || t == typeVec4UInt16 ||
						t == typeVec2UInt8 || t == typeVec3UInt8 || t == typeVec4UInt8;
					bool isD = t == typeVec2Double || t == typeVec3Double || t == typeVec4Double ||
						t == typeQuatDouble || t == typeMat3x3Double || t == typeMat4x4Double;
					for (uint64_t i = 0; i < n; ++i) {
						if (isU) li.pushUInt64(value.getUInt64(i));
						else if (isF) li.pushFloat(value.getFloat(i));
						else if (isD) li.pushDouble(value.getDouble(i));
						else li.pushInt64(value.getInt64(i));
					}
					out.push_back(0x04);
					out.insert(out.end(), name.begin(), name.end());
					out.push_back(0);
					bsonEncodeValue(out, var(li));
					break;
				}
			}
		}

		// Encode a document/array (the "value" payload of a 0x03/0x04 element,
		// i.e. a length-prefixed BSON document).
		void bsonEncodeValue(binary& out, var value) {
			binary body;
			if (value.getType() == typeList) {
				auto li = value.getList();
				uint64_t i = 0;
				for (auto it = li.begin(); it != li.end(); ++it, ++i)
					bsonEncodeKeyValue(body, to_string(i), *it);
			} else if (value.getType() == typeObject) {
				auto o = value.getObject();
				for (auto it = o.begin(); it != o.end(); ++it)
					bsonEncodeKeyValue(body, it->first, it->second);
			} else {
				// scalar as a single-element document
				bsonEncodeKeyValue(body, "0", value);
			}
			uint32_t len = uint32_t(body.size()) + 5;
			out.push_back(len & 0xFF); out.push_back((len >> 8) & 0xFF);
			out.push_back((len >> 16) & 0xFF); out.push_back((len >> 24) & 0xFF);
			out.insert(out.end(), body.begin(), body.end());
			out.push_back(0);
		}

		var bsonDecodeValue(binReader& r, uint8_t type, bool& ok);

		var bsonDecodeDoc(binReader& r) {
			uint32_t len = r.le32();
			if (len < 5) throw runtime_error("BSON: bad document length");
			size_t end = r.pos + len - 4;  // length includes itself
			object o;
			while (r.pos < end) {
				uint8_t type = r.u8();
				if (type == 0) break;  // terminator
				// cstring name
				string name;
				while (true) {
					char c = char(r.u8());
					if (c == 0) break;
					name += c;
				}
				bool ok = true;
				auto v = bsonDecodeValue(r, type, ok);
				if (!ok) throw runtime_error("BSON: unsupported type");
				o.setVar(name, v);
			}
			r.pos = end > r.pos ? end : r.pos;
			return var(o);
		}

		var bsonDecodeValue(binReader& r, uint8_t type, bool& ok) {
			ok = true;
			switch (type) {
				case 0x01: return var(r.le64d());
				case 0x02: {
					uint32_t n = r.le32();
					auto s = r.take(n > 0 ? n - 1 : 0);
					if (n > 0) r.u8();  // trailing NUL
					return var(string(s));
				}
				case 0x03: return bsonDecodeDoc(r);
				case 0x04: {
					uint32_t len = r.le32();
					size_t end = r.pos + len - 4;
					list li;
					uint64_t idx = 0;
					while (r.pos < end) {
						uint8_t t = r.u8();
						if (t == 0) break;
						while (r.u8() != 0) {}  // skip name
						bool ok2 = true;
						li.setVar(idx, bsonDecodeValue(r, t, ok2));
						++idx;
					}
					r.pos = end > r.pos ? end : r.pos;
					return reconstructList(li);
				}
				case 0x05: {
					uint32_t n = r.le32();
					uint8_t subtype = r.u8();
					auto b = r.take(n);
					if (subtype == 0x02 || subtype == 0x03) {
						// old binary/uuid: length includes a 4-byte inner length
						uint32_t inner = uint32_t(uint8_t(b[0])) | (uint32_t(uint8_t(b[1])) << 8) | (uint32_t(uint8_t(b[2])) << 16) | (uint32_t(uint8_t(b[3])) << 24);
						if (inner + 4 <= n)
							return var(binary(b.begin() + 4, b.begin() + 4 + inner));
					}
					return var(binary(b.begin(), b.end()));
				}
				case 0x07: { auto b = r.take(12); return var(binary(b.begin(), b.end())); }
				case 0x08: return var(r.u8() != 0);
				case 0x09: case 0x11: return var((int64_t)r.le64());
				case 0x0A: return var();
				case 0x10: return var((int32_t)r.le32());
				case 0x12: return var((int64_t)r.le64());
				case 0x13: { auto b = r.take(16); return var(binary(b.begin(), b.end())); }
				default:
					ok = false;
					return var();
			}
		}

		// ------------------------------------------------------------------
		// CBOR
		// ------------------------------------------------------------------

		void cborHeader(binWriter& w, uint8_t major, uint64_t v) {
			if (v < 24) {
				w.u8((major << 5) | uint8_t(v));
			} else if (v <= 0xFF) {
				w.u8((major << 5) | 24);
				w.u8(uint8_t(v));
			} else if (v <= 0xFFFF) {
				w.u8((major << 5) | 25);
				w.be16(uint16_t(v));
			} else if (v <= 0xFFFFFFFFull) {
				w.u8((major << 5) | 26);
				w.be32(uint32_t(v));
			} else {
				w.u8((major << 5) | 27);
				w.be64(v);
			}
		}

		void cborEncode(binWriter& w, var value);

		void cborEncode(binWriter& w, var value) {
			switch (value.getType()) {
				case typeNull: w.u8(0xF6); break;
				case typeBool: w.u8(value.getBool() ? 0xF5 : 0xF4); break;
				case typeUInt64: cborHeader(w, 0, value.getUInt64()); break;
				case typeUInt32: cborHeader(w, 0, value.getUInt32()); break;
				case typeUInt16: cborHeader(w, 0, value.getUInt16()); break;
				case typeUInt8: cborHeader(w, 0, value.getUInt8()); break;
				case typeInt64: {
					auto v = value.getInt64();
					if (v >= 0) cborHeader(w, 0, uint64_t(v));
					else cborHeader(w, 1, uint64_t(-(v + 1)));
					break;
				}
				case typeInt32: case typeInt16: case typeInt8: {
					auto v = value.getInt32();
					if (v >= 0) cborHeader(w, 0, uint64_t(v));
					else cborHeader(w, 1, uint64_t(-(v + 1)));
					break;
				}
				case typeString: case typeStringView: {
					auto s = value.getString();
					cborHeader(w, 3, s.size());
					w.str(s);
					break;
				}
				case typeDouble: {
					w.u8(0xFB);
					double dv = value.getDouble(); uint64_t du; memcpy(&du, &dv, 8); w.be64(du);
					break;
				}
				case typeFloat: {
					w.u8(0xFA);
					float f = value.getFloat();
					uint32_t u;
					memcpy(&u, &f, 4);
					w.be32(u);
					break;
				}
				case typeList: {
					auto li = value.getList();
					cborHeader(w, 4, li.size());
					for (auto it = li.begin(); it != li.end(); ++it)
						cborEncode(w, *it);
					break;
				}
				case typeObject: {
					auto o = value.getObject();
					cborHeader(w, 5, o.size());
					for (auto it = o.begin(); it != o.end(); ++it) {
						cborHeader(w, 3, it->first.size());
						w.str(it->first);
						cborEncode(w, it->second);
					}
					break;
				}
				default: {
					// vec/mat expand to arrays.
					list li;
					auto t = value.getType();
					uint64_t n = 0;
					bool isU = false, isF = false, isD = false;
					switch (t) {
						case typeVec2Int64: case typeVec2Int32: case typeVec2Int16: case typeVec2Int8: n = 2; break;
						case typeVec2UInt64: case typeVec2UInt32: case typeVec2UInt16: case typeVec2UInt8: n = 2; isU = true; break;
						case typeVec2Float: case typeVec2Double: n = 2; isF = true; break;
						case typeVec3Int64: case typeVec3Int32: case typeVec3Int16: case typeVec3Int8: n = 3; break;
						case typeVec3UInt64: case typeVec3UInt32: case typeVec3UInt16: case typeVec3UInt8: n = 3; isU = true; break;
						case typeVec3Float: case typeVec3Double: n = 3; isF = true; break;
						case typeVec4Int64: case typeVec4Int32: case typeVec4Int16: case typeVec4Int8: n = 4; break;
						case typeVec4UInt64: case typeVec4UInt32: case typeVec4UInt16: case typeVec4UInt8: n = 4; isU = true; break;
						case typeVec4Float: case typeVec4Double: case typeQuatFloat: case typeQuatDouble: n = 4; isF = true; break;
						case typeMat3x3Float: case typeMat3x3Double: n = 9; isF = true; break;
						case typeMat4x4Float: case typeMat4x4Double: n = 16; isF = true; break;
						default: break;
					}
					if (value.getType() == typeVec2Double || value.getType() == typeVec3Double ||
						value.getType() == typeVec4Double || value.getType() == typeQuatDouble ||
						value.getType() == typeMat3x3Double || value.getType() == typeMat4x4Double)
						isD = true;
					for (uint64_t i = 0; i < n; ++i) {
						if (isU) li.pushUInt64(value.getUInt64(i));
						else if (isD) li.pushDouble(value.getDouble(i));
						else if (isF) li.pushFloat(value.getFloat(i));
						else li.pushInt64(value.getInt64(i));
					}
					cborEncode(w, var(li));
					break;
				}
			}
		}

		uint64_t cborLength(binReader& r, uint8_t addl) {
			switch (addl) {
				case 24: return r.u8();
				case 25: return r.be16();
				case 26: return r.be32();
				case 27: return r.be64();
				default: return addl;
			}
		}

		double halfToDouble(uint16_t h) {
			int sign = (h >> 15) & 1;
			int exp = (h >> 10) & 0x1F;
			int mant = h & 0x3FF;
			double v;
			if (exp == 0) v = mant * pow(2.0, -24);
			else if (exp == 31) v = mant ? nan("") : INFINITY;
			else v = (mant + 1024.0) * pow(2.0, exp - 25);
			return sign ? -v : v;
		}

		var cborDecode(binReader& r);

		var cborDecode(binReader& r) {
			uint8_t b = r.u8();
			uint8_t major = b >> 5;
			uint8_t addl = b & 0x1F;
			switch (major) {
				case 0: return var(cborLength(r, addl));
				case 1: return var((int64_t)(-(int64_t)cborLength(r, addl) - 1));
				case 2: {
					uint64_t n = cborLength(r, addl);
					auto s = r.take(n);
					return var(binary(s.begin(), s.end()));
				}
				case 3: {
					uint64_t n = cborLength(r, addl);
					auto s = r.take(n);
					return var(string(s));
				}
				case 4: {
					if (addl == 31) {
						list li;
						while (true) {
							uint8_t nb = r.u8();
							if (nb == 0xFF) break;
							// we consumed a byte; re-decode it
							r.pos--;
							li.pushVar(cborDecode(r));
						}
						return reconstructList(li);
					}
					uint64_t n = cborLength(r, addl);
					list li;
					for (uint64_t i = 0; i < n; ++i)
						li.pushVar(cborDecode(r));
					return reconstructList(li);
				}
				case 5: {
					uint64_t n = addl == 31 ? 0 : cborLength(r, addl);
					object o;
					uint64_t i = 0;
					while (addl == 31 || i < n) {
						if (addl == 31) {
							uint8_t nb = r.u8();
							if (nb == 0xFF) break;
							r.pos--;
						}
						auto k = cborDecode(r);
						auto v = cborDecode(r);
						o.setVar(k.getString(), v);
						++i;
					}
					return var(o);
				}
				case 6: {
					// tag: unwrap the following value
					cborLength(r, addl);
					return cborDecode(r);
				}
				case 7:
				default:
					switch (addl) {
						case 20: return var(false);
						case 21: return var(true);
						case 22: case 23: return var();
						case 25: return var(halfToDouble(r.be16()));
						case 26: { uint32_t u = r.be32(); float f; memcpy(&f, &u, 4); return var((double)f); }
						case 27: { uint64_t u = r.be64(); double d; memcpy(&d, &u, 8); return var(d); }
						default: throw runtime_error("CBOR: unsupported simple value");
					}
			}
		}

		// ------------------------------------------------------------------
		// MessagePack
		// ------------------------------------------------------------------

		void mpEncode(binWriter& w, var value);

		void mpEncodeStr(binWriter& w, const string& s) {
			auto n = s.size();
			if (n < 32) w.u8(uint8_t(0xA0 | n));
			else if (n <= 0xFF) { w.u8(0xD9); w.u8(uint8_t(n)); }
			else if (n <= 0xFFFF) { w.u8(0xDA); w.be16(uint16_t(n)); }
			else { w.u8(0xDB); w.be32(uint32_t(n)); }
			w.str(s);
		}

		void mpEncode(binWriter& w, var value) {
			switch (value.getType()) {
				case typeNull: w.u8(0xC0); break;
				case typeBool: w.u8(value.getBool() ? 0xC3 : 0xC2); break;
				case typeUInt8: {
					auto v = value.getUInt8();
					if (v < 128) w.u8(v);
					else { w.u8(0xCC); w.u8(v); }
					break;
				}
				case typeUInt16: {
					auto v = value.getUInt16();
					if (v < 128) w.u8(uint8_t(v));
					else if (v <= 0xFF) { w.u8(0xCC); w.u8(uint8_t(v)); }
					else { w.u8(0xCD); w.be16(v); }
					break;
				}
				case typeUInt32: {
					auto v = value.getUInt32();
					if (v < 128) w.u8(uint8_t(v));
					else if (v <= 0xFF) { w.u8(0xCC); w.u8(uint8_t(v)); }
					else if (v <= 0xFFFF) { w.u8(0xCD); w.be16(uint16_t(v)); }
					else { w.u8(0xCE); w.be32(v); }
					break;
				}
				case typeUInt64: {
					auto v = value.getUInt64();
					if (v < 128) w.u8(uint8_t(v));
					else if (v <= 0xFF) { w.u8(0xCC); w.u8(uint8_t(v)); }
					else if (v <= 0xFFFF) { w.u8(0xCD); w.be16(uint16_t(v)); }
					else if (v <= 0xFFFFFFFFull) { w.u8(0xCE); w.be32(uint32_t(v)); }
					else { w.u8(0xCF); w.be64(v); }
					break;
				}
				case typeInt64: {
					auto v = value.getInt64();
					if (v >= 0 && v < 128) w.u8(uint8_t(v));
					else if (v < 0 && v >= -32) w.u8(uint8_t(int8_t(v)));
					else if (v >= INT8_MIN && v <= INT8_MAX) { w.u8(0xD0); w.u8(uint8_t(int8_t(v))); }
					else if (v >= INT16_MIN && v <= INT16_MAX) { w.u8(0xD1); w.be16(uint16_t(int16_t(v))); }
					else if (v >= INT32_MIN && v <= INT32_MAX) { w.u8(0xD2); w.be32(uint32_t(int32_t(v))); }
					else { w.u8(0xD3); w.be64(uint64_t(v)); }
					break;
				}
				case typeInt32: case typeInt16: case typeInt8: {
					auto v = (int64_t)value.getInt32();
					if (v >= 0 && v < 128) w.u8(uint8_t(v));
					else if (v < 0 && v >= -32) w.u8(uint8_t(int8_t(v)));
					else if (v >= INT8_MIN && v <= INT8_MAX) { w.u8(0xD0); w.u8(uint8_t(int8_t(v))); }
					else if (v >= INT16_MIN && v <= INT16_MAX) { w.u8(0xD1); w.be16(uint16_t(int16_t(v))); }
					else if (v >= INT32_MIN && v <= INT32_MAX) { w.u8(0xD2); w.be32(uint32_t(int32_t(v))); }
					else { w.u8(0xD3); w.be64(uint64_t(v)); }
					break;
				}
				case typeDouble: { w.u8(0xCB); uint64_t u; double dv = value.getDouble(); memcpy(&u, &dv, 8); w.be64(u); break; }
				case typeFloat: { w.u8(0xCA); uint32_t u; float f = value.getFloat(); memcpy(&u, &f, 4); w.be32(u); break; }
				case typeString: case typeStringView: mpEncodeStr(w, value.getString()); break;
				case typeList: {
					auto li = value.getList();
					auto n = li.size();
					if (n < 16) w.u8(uint8_t(0x90 | n));
					else if (n <= 0xFFFF) { w.u8(0xDC); w.be16(uint16_t(n)); }
					else { w.u8(0xDD); w.be32(uint32_t(n)); }
					for (auto it = li.begin(); it != li.end(); ++it)
						mpEncode(w, *it);
					break;
				}
				case typeObject: {
					auto o = value.getObject();
					auto n = o.size();
					if (n < 16) w.u8(uint8_t(0x80 | n));
					else if (n <= 0xFFFF) { w.u8(0xDE); w.be16(uint16_t(n)); }
					else { w.u8(0xDF); w.be32(uint32_t(n)); }
					for (auto it = o.begin(); it != o.end(); ++it) {
						mpEncodeStr(w, it->first);
						mpEncode(w, it->second);
					}
					break;
				}
				default: {
					// vec/mat expand to arrays.
					list li;
					auto t = value.getType();
					uint64_t n = 0;
					bool isU = false, isF = false, isD = false;
					switch (t) {
						case typeVec2Int64: case typeVec2Int32: case typeVec2Int16: case typeVec2Int8: n = 2; break;
						case typeVec2UInt64: case typeVec2UInt32: case typeVec2UInt16: case typeVec2UInt8: n = 2; isU = true; break;
						case typeVec2Float: case typeVec2Double: n = 2; isF = true; break;
						case typeVec3Int64: case typeVec3Int32: case typeVec3Int16: case typeVec3Int8: n = 3; break;
						case typeVec3UInt64: case typeVec3UInt32: case typeVec3UInt16: case typeVec3UInt8: n = 3; isU = true; break;
						case typeVec3Float: case typeVec3Double: n = 3; isF = true; break;
						case typeVec4Int64: case typeVec4Int32: case typeVec4Int16: case typeVec4Int8: n = 4; break;
						case typeVec4UInt64: case typeVec4UInt32: case typeVec4UInt16: case typeVec4UInt8: n = 4; isU = true; break;
						case typeVec4Float: case typeVec4Double: case typeQuatFloat: case typeQuatDouble: n = 4; isF = true; break;
						case typeMat3x3Float: case typeMat3x3Double: n = 9; isF = true; break;
						case typeMat4x4Float: case typeMat4x4Double: n = 16; isF = true; break;
						default: break;
					}
					if (value.getType() == typeVec2Double || value.getType() == typeVec3Double ||
						value.getType() == typeVec4Double || value.getType() == typeQuatDouble ||
						value.getType() == typeMat3x3Double || value.getType() == typeMat4x4Double)
						isD = true;
					for (uint64_t i = 0; i < n; ++i) {
						if (isU) li.pushUInt64(value.getUInt64(i));
						else if (isD) li.pushDouble(value.getDouble(i));
						else if (isF) li.pushFloat(value.getFloat(i));
						else li.pushInt64(value.getInt64(i));
					}
					mpEncode(w, var(li));
					break;
				}
			}
		}

		var mpDecode(binReader& r);

		var mpDecode(binReader& r) {
			uint8_t b = r.u8();
			if (b <= 0x7F) return var((uint64_t)b);
			if (b >= 0xE0) return var((int64_t)(int8_t)b);
			if (b >= 0xA0 && b <= 0xBF) { auto s = r.take(b & 0x1F); return var(string(s)); }
			if (b >= 0x90 && b <= 0x9F) {
				list li;
				uint64_t n = b & 0x0F;
				for (uint64_t i = 0; i < n; ++i) li.pushVar(mpDecode(r));
				return reconstructList(li);
			}
			if (b >= 0x80 && b <= 0x8F) {
				object o;
				uint64_t n = b & 0x0F;
				for (uint64_t i = 0; i < n; ++i) {
					auto k = mpDecode(r);
					o.setVar(k.getString(), mpDecode(r));
				}
				return var(o);
			}
			switch (b) {
				case 0xC0: return var();
				case 0xC2: return var(false);
				case 0xC3: return var(true);
				case 0xCA: { uint32_t u = r.be32(); float f; memcpy(&f, &u, 4); return var((double)f); }
				case 0xCB: { uint64_t u = r.be64(); double d; memcpy(&d, &u, 8); return var(d); }
				case 0xCC: return var((uint64_t)r.u8());
				case 0xCD: return var((uint64_t)r.be16());
				case 0xCE: return var((uint64_t)r.be32());
				case 0xCF: return var(r.be64());
				case 0xD0: return var((int64_t)(int8_t)r.u8());
				case 0xD1: return var((int64_t)(int16_t)r.be16());
				case 0xD2: return var((int64_t)(int32_t)r.be32());
				case 0xD3: return var((int64_t)r.be64());
				case 0xD9: { auto s = r.take(r.u8()); return var(string(s)); }
				case 0xDA: { auto s = r.take(r.be16()); return var(string(s)); }
				case 0xDB: { auto s = r.take(r.be32()); return var(string(s)); }
				case 0xDC: {
					list li;
					uint64_t n = r.be16();
					for (uint64_t i = 0; i < n; ++i) li.pushVar(mpDecode(r));
					return reconstructList(li);
				}
				case 0xDD: {
					list li;
					uint64_t n = r.be32();
					for (uint64_t i = 0; i < n; ++i) li.pushVar(mpDecode(r));
					return reconstructList(li);
				}
				case 0xDE: {
					object o;
					uint64_t n = r.be16();
					for (uint64_t i = 0; i < n; ++i) {
						auto k = mpDecode(r);
						o.setVar(k.getString(), mpDecode(r));
					}
					return var(o);
				}
				case 0xDF: {
					object o;
					uint64_t n = r.be32();
					for (uint64_t i = 0; i < n; ++i) {
						auto k = mpDecode(r);
						o.setVar(k.getString(), mpDecode(r));
					}
					return var(o);
				}
				default: throw runtime_error("MessagePack: unsupported marker");
			}
		}

		// ------------------------------------------------------------------
		// UBJSON
		// ------------------------------------------------------------------

		void ubEncode(binWriter& w, var value);

		void ubEncodeString(binWriter& w, const string& s) {
			w.u8('S');
			// length as int64
			w.u8('L');
			w.be64(s.size());
			w.str(s);
		}

		void ubEncode(binWriter& w, var value) {
			switch (value.getType()) {
				case typeNull: w.u8('Z'); break;
				case typeBool: w.u8(value.getBool() ? 'T' : 'F'); break;
				case typeUInt8: w.u8('U'); w.u8(value.getUInt8()); break;
				case typeInt8: w.u8('i'); w.u8(uint8_t(value.getInt8())); break;
				case typeInt16: w.u8('I'); w.be16(uint16_t(value.getInt16())); break;
				case typeInt32: w.u8('l'); w.be32(uint32_t(value.getInt32())); break;
				case typeInt64: w.u8('L'); w.be64(uint64_t(value.getInt64())); break;
				case typeUInt16: w.u8('I'); w.be16(value.getUInt16()); break;
				case typeUInt32: w.u8('l'); w.be32(value.getUInt32()); break;
				case typeUInt64: w.u8('L'); w.be64(value.getUInt64()); break;
				case typeDouble: { w.u8('D'); uint64_t u; double dv = value.getDouble(); memcpy(&u, &dv, 8); w.be64(u); break; }
				case typeFloat: { w.u8('d'); uint32_t u; float f = value.getFloat(); memcpy(&u, &f, 4); w.be32(u); break; }
				case typeString: case typeStringView: ubEncodeString(w, value.getString()); break;
				case typeList: {
					auto li = value.getList();
					w.u8('[');
					for (auto it = li.begin(); it != li.end(); ++it)
						ubEncode(w, *it);
					w.u8(']');
					break;
				}
				case typeObject: {
					auto o = value.getObject();
					w.u8('{');
					for (auto it = o.begin(); it != o.end(); ++it) {
						ubEncodeString(w, it->first);
						ubEncode(w, it->second);
					}
					w.u8('}');
					break;
				}
				default: {
					list li;
					auto t = value.getType();
					uint64_t n = 0;
					bool isU = false, isF = false, isD = false;
					switch (t) {
						case typeVec2Int64: case typeVec2Int32: case typeVec2Int16: case typeVec2Int8: n = 2; break;
						case typeVec2UInt64: case typeVec2UInt32: case typeVec2UInt16: case typeVec2UInt8: n = 2; isU = true; break;
						case typeVec2Float: case typeVec2Double: n = 2; isF = true; break;
						case typeVec3Int64: case typeVec3Int32: case typeVec3Int16: case typeVec3Int8: n = 3; break;
						case typeVec3UInt64: case typeVec3UInt32: case typeVec3UInt16: case typeVec3UInt8: n = 3; isU = true; break;
						case typeVec3Float: case typeVec3Double: n = 3; isF = true; break;
						case typeVec4Int64: case typeVec4Int32: case typeVec4Int16: case typeVec4Int8: n = 4; break;
						case typeVec4UInt64: case typeVec4UInt32: case typeVec4UInt16: case typeVec4UInt8: n = 4; isU = true; break;
						case typeVec4Float: case typeVec4Double: case typeQuatFloat: case typeQuatDouble: n = 4; isF = true; break;
						case typeMat3x3Float: case typeMat3x3Double: n = 9; isF = true; break;
						case typeMat4x4Float: case typeMat4x4Double: n = 16; isF = true; break;
						default: break;
					}
					if (value.getType() == typeVec2Double || value.getType() == typeVec3Double ||
						value.getType() == typeVec4Double || value.getType() == typeQuatDouble ||
						value.getType() == typeMat3x3Double || value.getType() == typeMat4x4Double)
						isD = true;
					for (uint64_t i = 0; i < n; ++i) {
						if (isU) li.pushUInt64(value.getUInt64(i));
						else if (isD) li.pushDouble(value.getDouble(i));
						else if (isF) li.pushFloat(value.getFloat(i));
						else li.pushInt64(value.getInt64(i));
					}
					w.u8('[');
					for (auto it = li.begin(); it != li.end(); ++it)
						ubEncode(w, *it);
					w.u8(']');
					break;
				}
			}
		}

		// Read a UBJSON string ('S' + length + bytes).
		string ubReadString(binReader& r) {
			char t = char(r.u8());
			uint64_t n = 0;
			switch (t) {
				case 'U': n = r.u8(); break;
				case 'i': n = (uint64_t)(int64_t)(int8_t)r.u8(); break;
				case 'I': n = (uint64_t)(int64_t)(int16_t)r.be16(); break;
				case 'l': n = (uint64_t)(int64_t)(int32_t)r.be32(); break;
				case 'L': n = r.be64(); break;
				default: throw runtime_error("UBJSON: bad string length");
			}
			auto s = r.take(n);
			return string(s);
		}

		var ubDecode(binReader& r);

		var ubDecodeValueWithType(binReader& r, char t);

		var ubDecode(binReader& r) {
			return ubDecodeValueWithType(r, char(r.u8()));
		}

		var ubDecodeValueWithType(binReader& r, char t) {
			switch (t) {
				case 'Z': return var();
				case 'N': return ubDecode(r);  // noop: next value
				case 'T': return var(true);
				case 'F': return var(false);
				case 'U': return var((uint64_t)r.u8());
				case 'i': return var((int64_t)(int8_t)r.u8());
				case 'I': return var((int64_t)(int16_t)r.be16());
				case 'l': return var((int64_t)(int32_t)r.be32());
				case 'L': return var((int64_t)r.be64());
				case 'd': { uint32_t u = r.be32(); float f; memcpy(&f, &u, 4); return var((double)f); }
				case 'D': { uint64_t u = r.be64(); double d; memcpy(&d, &u, 8); return var(d); }
				case 'C': return var(string(1, char(r.u8())));
				case 'S': { auto s = ubReadString(r); return var(s); }
				case 'H': {
					// high-precision number: a string, parse as double
					auto s = ubReadString(r);
					try {
						return var(strtod(s.c_str(), nullptr));
					} catch (...) {
						return var(s);
					}
				}
				case '[': {
					list li;
					uint64_t idx = 0;
					// optional optimized form: '$' <type> '#' <count>
					if (!r.eof() && r.p[r.pos] == '$') {
						r.pos++;
						char vt = char(r.u8());
						uint64_t count = 0;
						if (r.p[r.pos] == '#') {
							r.pos++;
							auto c = ubDecode(r);
							count = c.getUInt64();
						}
						for (uint64_t i = 0; i < count; ++i)
							li.pushVar(ubDecodeValueWithType(r, vt));
					} else {
						while (!r.eof() && r.p[r.pos] != ']')
							li.pushVar(ubDecode(r));
						if (r.eof()) throw runtime_error("UBJSON: unterminated array");
						r.pos++;
					}
					return reconstructList(li);
				}
				case '{': {
					object o;
					if (!r.eof() && r.p[r.pos] == '#') {
						r.pos++;
						auto c = ubDecode(r);
						uint64_t count = c.getUInt64();
						for (uint64_t i = 0; i < count; ++i) {
							auto k = ubDecode(r);
							o.setVar(k.getString(), ubDecode(r));
						}
					} else {
						while (!r.eof() && r.p[r.pos] != '}') {
							auto k = ubDecode(r);
							if (r.eof() || r.p[r.pos] == '}')
								throw runtime_error("UBJSON: unterminated object");
							o.setVar(k.getString(), ubDecode(r));
						}
						if (r.eof()) throw runtime_error("UBJSON: unterminated object");
						r.pos++;
					}
					return var(o);
				}
				default: throw runtime_error("UBJSON: unknown type");
			}
		}

	}  // namespace

	// ----------------------------------------------------------------------
	// public API
	// ----------------------------------------------------------------------

	var jsonParse(string_view data) {
		try {
			jsonReader r(data);
			auto v = r.parseValue();
			r.finish();
			return v;
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

	string jsonStringify(var data, bool pretty) {
		string out;
		appendJSONValue(data, out, pretty, 0);
		return out;
	}

	var bsonParse(string_view data) {
		try {
			binReader r((const uint8_t*)data.data(), data.size());
			return bsonDecodeDoc(r);
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

	binary bsonStringify(var data) {
		binary out;
		bsonEncodeValue(out, data);
		return out;
	}

	var cborParse(string_view data) {
		try {
			binReader r((const uint8_t*)data.data(), data.size());
			auto v = cborDecode(r);
			if (!r.eof()) throw runtime_error("CBOR: trailing data");
			return v;
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

	binary cborStringify(var data) {
		binWriter w;
		cborEncode(w, data);
		return w.buf;
	}

	var msgpackParse(string_view data) {
		try {
			binReader r((const uint8_t*)data.data(), data.size());
			auto v = mpDecode(r);
			if (!r.eof()) throw runtime_error("MessagePack: trailing data");
			return v;
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

	binary msgpackStringify(var data) {
		binWriter w;
		mpEncode(w, data);
		return w.buf;
	}

	var ubjsonParse(string_view data) {
		try {
			binReader r((const uint8_t*)data.data(), data.size());
			auto v = ubDecode(r);
			if (!r.eof()) throw runtime_error("UBJSON: trailing data");
			return v;
		} catch (const exception& e) {
			return genericError(e.what());
		}
	}

	binary ubjsonStringify(var data) {
		binWriter w;
		ubEncode(w, data);
		return w.buf;
	}

}  // namespace gold