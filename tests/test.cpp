#include <iostream>

#include "file.hpp"
#include "goldjs.hpp"
#include "module.hpp"
#include "promise.hpp"
#include "types.hpp"
#include "goldtest.hpp"

using namespace gold;

static bool binaryEqual(const binary& a, const binary& b) {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i] != b[i]) return false;
	return true;
}

TEST(var_basic_types) {
	var nul;
	EXPECT_EQ(nul.getType(), typeNull);
	EXPECT_TRUE(nul.isEmpty());

	var i64 = var(int64_t(42));
	EXPECT_EQ(i64.getType(), typeInt64);
	EXPECT_EQ(i64.getInt64(), 42);
	EXPECT_TRUE(i64.isNumber());

	var str = var(string("hello"));
	EXPECT_EQ(str.getType(), typeString);
	EXPECT_TRUE(str.isString());
	EXPECT_EQ(str.getString(), "hello");

	var boo = var(true);
	EXPECT_EQ(boo.getType(), typeBool);
	EXPECT_EQ(boo.getBool(), true);

	var dbl = var(3.5);
	EXPECT_EQ(dbl.getType(), typeDouble);
	EXPECT_NEAR(dbl.getDouble(), 3.5, 1e-9);
}

TEST(var_conversions) {
	EXPECT_EQ(var(int64_t(7)).getInt64(), 7);
	EXPECT_EQ(var(int32_t(7)).getInt64(), 7);
	EXPECT_EQ(var(uint8_t(7)).getInt64(), 7);
	EXPECT_EQ(var(7.5).getDouble(), 7.5);
	EXPECT_EQ(var(7.5).getInt64(), 7);
	EXPECT_EQ(var(uint64_t(300)).getUInt64(), (uint64_t)300);
	EXPECT_EQ(var(string("123")).getInt64(), 123);
	EXPECT_EQ(var(string("456")).getUInt64(), (uint64_t)456);
	EXPECT_EQ(var(string("true")).getBool(), true);
	EXPECT_EQ(var(string("false")).getBool(), false);
}

TEST(var_string_conversion) {
	EXPECT_EQ(var(string("abc")).getString(), "abc");
	EXPECT_EQ(var(int64_t(123)).getString(), "123");
	EXPECT_EQ(var(double(1.5)).getString(), std::to_string(1.5));
	EXPECT_EQ(var(true).getString(), "true");
	EXPECT_EQ(var(false).getString(), "false");
	EXPECT_TRUE(var().isEmpty());
}

TEST(var_arithmetic_uint16) {
	// Regression: operator+ used getUInt8() for typeUInt16
	auto a = var(uint16_t(400));
	auto b = var(uint16_t(50));
	EXPECT_EQ((a + b).getUInt16(), (uint16_t)450);
	EXPECT_EQ((a - b).getUInt16(), (uint16_t)350);
	EXPECT_EQ((a * b).getUInt16(), (uint16_t)20000);
	EXPECT_EQ((a / b).getUInt16(), (uint16_t)8);
	EXPECT_EQ((a % b).getUInt16(), (uint16_t)0);
}

TEST(var_arithmetic_int) {
	auto a = var(int64_t(20));
	auto b = var(int64_t(3));
	EXPECT_EQ((a + b).getInt64(), 23);
	EXPECT_EQ((a - b).getInt64(), 17);
	EXPECT_EQ((a * b).getInt64(), 60);
	EXPECT_EQ((a / b).getInt64(), 6);
	EXPECT_EQ((a % b).getInt64(), 2);
	EXPECT_EQ((-a).getInt64(), -20);
}

TEST(var_arithmetic_bool) {
	auto a = var(true);
	auto b = var(true);
	EXPECT_EQ((a + b).getBool(), true);
	EXPECT_EQ((a - b).getBool(), false);
	EXPECT_EQ((b - a).getBool(), false);
	EXPECT_EQ((a * b).getBool(), true);
	EXPECT_EQ((a / b).getBool(), true);
}

TEST(var_arithmetic_pointer_order) {
	// 0x10 - 0x08 == 0x08
	var hi = var((void*)(uintptr_t)0x10, typePtr);
	var lo = var((void*)(uintptr_t)0x08, typePtr);
	EXPECT_EQ((hi - lo).getPtr(), (void*)(uintptr_t)0x08);
	EXPECT_EQ((lo - hi).getPtr(), (void*)(uintptr_t)-0x08);
}

TEST(var_arithmetic_double_mod) {
	auto a = var(7.5);
	auto b = var(2.0);
	EXPECT_NEAR((a % b).getDouble(), 1.5, 1e-9);
}

TEST(var_arithmetic_mat3x3_double) {
	// Regression: mat3x3 double ops returned typeMat3x3Float
	auto a = mat3x3d({1., 2., 3., 4., 5., 6., 7., 8., 9.});
	auto b = mat3x3d({9., 8., 7., 6., 5., 4., 3., 2., 1.});
	auto sum = a + b;
	EXPECT_EQ(sum.getType(), typeMat3x3Double);
	EXPECT_NEAR(sum.getDouble(0), 10.0, 1e-9);
	EXPECT_NEAR(sum.getDouble(8), 10.0, 1e-9);
	auto neg = -a;
	EXPECT_EQ(neg.getType(), typeMat3x3Double);
	EXPECT_NEAR(neg.getDouble(0), -1.0, 1e-9);
}

TEST(var_arithmetic_quat_double) {
	auto q = quatd(0.0, 0.0, 0.0, 1.0);
	auto v = vec3d(2.0, 3.0, 4.0);
	auto rotated = q * v;
	EXPECT_EQ(rotated.getType(), typeVec3Double);
	EXPECT_NEAR(rotated.getDouble(0), 2.0, 1e-9);
	EXPECT_NEAR(rotated.getDouble(1), 3.0, 1e-9);
	EXPECT_NEAR(rotated.getDouble(2), 4.0, 1e-9);
	auto product = q * q;
	EXPECT_EQ(product.getType(), typeQuatDouble);
	EXPECT_NEAR(product.getDouble(3), 1.0, 1e-9);
}

TEST(var_mat3x3_float_string) {
	// Regression: typeMat3x3Float string read the double union member
	auto m = mat3x3f({1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f});
	auto s = m.getString();
	EXPECT_NE(s, "");
	// First element must read the float 1.f (not the double union's junk).
	// Formatting varies by libstdc++ version (1.0 vs 1.000000), so only
	// check that it starts with the correct value.
	EXPECT_EQ(s[0], '[');
	EXPECT_EQ(s[1], '1');
}

TEST(var_vec_set_uint8) {
	// Regression: setUInt8 switched on UInt16 vec types
	auto v = vec2u8(1, 2);
	v.setUInt8(0, 9);
	EXPECT_EQ(v.getUInt8(0), 9);
	EXPECT_EQ(v.getUInt8(1), 2);
}

TEST(var_vector_types) {
	auto v2 = vec2f(1.f, 2.f);
	EXPECT_TRUE(v2.isVec2());
	EXPECT_NEAR(v2.getFloat(0), 1.f, 1e-6);

	auto v3 = vec3i32(1, 2, 3);
	EXPECT_TRUE(v3.isVec3());
	EXPECT_EQ(v3.getInt32(2), 3);

	auto q = quatf(0.f, 0.f, 0.f, 1.f);
	EXPECT_TRUE(q.isQuat());
	EXPECT_NEAR(q.getFloat(3), 1.f, 1e-6);

	auto m4 = mat4x4f({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
	EXPECT_TRUE(m4.isMat4x4());
	EXPECT_NEAR(m4.getFloat(15), 1.f, 1e-6);
}

TEST(var_comparison) {
	EXPECT_TRUE(var(int64_t(1)) == var(int64_t(1)));
	EXPECT_TRUE(var(int64_t(1)) != var(int64_t(2)));
	EXPECT_TRUE(var(int64_t(1)) < var(int64_t(2)));
	EXPECT_TRUE(var(1.5) < var(2.5));
	EXPECT_TRUE(var(string("a")) == var(string("a")));
}

TEST(list_basics) {
	list li;
	EXPECT_EQ(li.size(), (uint64_t)0);
	li.pushInt64(1);
	li.pushInt64(2);
	li.pushInt64(3);
	EXPECT_EQ(li.size(), (uint64_t)3);
	EXPECT_EQ(li.getInt64(0), 1);
	EXPECT_EQ(li.getInt64(2), 3);
	EXPECT_EQ(li.getInt64(9, 42), 42);
	li.pop();
	EXPECT_EQ(li.size(), (uint64_t)2);
	EXPECT_EQ(li.getVar(1).getInt64(), 2);
}

TEST(list_set_get) {
	list li;
	li.setString(2, string("foo"));
	EXPECT_EQ(li.size(), (uint64_t)3);
	EXPECT_EQ(li.getString(2), "foo");
	EXPECT_EQ(li.getType(0), typeNull);
	li.setBool(0, true);
	EXPECT_EQ(li.getBool(0), true);
}

TEST(list_find_and_remove) {
	list li;
	li.pushString("apple");
	li.pushString("banana");
	li.pushString("cherry");
	auto it = li.find(var(string("banana")));
	EXPECT_NE(it, li.end());
	li -= var(string("banana"));
	EXPECT_EQ(li.size(), (uint64_t)2);
	EXPECT_EQ(li.getString(0), "apple");
	EXPECT_EQ(li.getString(1), "cherry");
}

TEST(list_json_roundtrip) {
	list li;
	li.pushString("hello");
	li.pushInt64(42);
	li.pushBool(true);
	li.pushDouble(3.14);
	auto jsonStr = li.getJSON();
	EXPECT_NE(jsonStr.find("hello"), string::npos);

	auto parsed = file::parseJSON(jsonStr);
	EXPECT_TRUE(parsed.isList());
	auto li2 = parsed.getList();
	EXPECT_EQ(li2.size(), (uint64_t)4);
	EXPECT_EQ(li2.getString(0), "hello");
	// JSON positive integers round-trip as unsigned
	EXPECT_EQ(li2.getVar(1).getInt64(), 42);
	EXPECT_EQ(li2.getBool(2), true);
	EXPECT_NEAR(li2.getVar(3).getDouble(), 3.14, 1e-6);
}

TEST(object_basics) {
	object obj;
	obj.setString("name", "gold");
	obj.setInt64("count", 5);
	obj.setBool("active", true);
	EXPECT_EQ(obj.size(), (uint64_t)3);
	EXPECT_EQ(obj.getString("name"), "gold");
	EXPECT_EQ(obj.getInt64("count"), 5);
	EXPECT_EQ(obj.getBool("active"), true);
	EXPECT_EQ(obj.getString("missing", "def"), "def");
	obj.erase("name");
	EXPECT_EQ(obj.getString("name", "gone"), "gone");
}

TEST(object_expression_nested) {
	object obj;
	auto sub = object({});
	sub.setInt64("x", 10);
	obj.setObject("child", sub);
	EXPECT_EQ(obj.getObject("child").getInt64("x"), 10);
}

TEST(object_json_roundtrip) {
	object obj;
	obj.setString("greeting", "hi");
	obj.setInt64("n", 7);
	auto jsonStr = obj.getJSON();
	EXPECT_NE(jsonStr.find("greeting"), string::npos);

	auto parsed = file::parseJSON(jsonStr);
	EXPECT_TRUE(parsed.isObject());
	auto o2 = parsed.getObject();
	EXPECT_EQ(o2.getString("greeting"), "hi");
	EXPECT_EQ(o2.getInt64("n"), 7);
}

TEST(object_serialization_formats) {
	object obj;
	obj.setString("name", "gold");
	obj.setInt64("value", 1234);

	auto bson = obj.getBSON();
	EXPECT_TRUE(bson.size() > 0);
	auto fromBSON = file::parseBSON(string_view((char*)bson.data(), bson.size()));
	EXPECT_EQ(fromBSON.getObject().getString("name"), "gold");
	EXPECT_EQ(fromBSON.getObject().getInt64("value"), 1234);

	auto cbor = obj.getCBOR();
	auto fromCBOR = file::parseCBOR(string_view((char*)cbor.data(), cbor.size()));
	EXPECT_EQ(fromCBOR.getObject().getString("name"), "gold");

	auto msgpack = obj.getMsgPack();
	auto fromMsgPack = file::parseMsgPack(string_view((char*)msgpack.data(), msgpack.size()));
	EXPECT_EQ(fromMsgPack.getObject().getString("name"), "gold");

	auto ubjson = obj.getUBJSON();
	auto fromUBJSON = file::parseUBJSON(string_view((char*)ubjson.data(), ubjson.size()));
	EXPECT_EQ(fromUBJSON.getObject().getString("name"), "gold");
}

TEST(object_get_cookie_string) {
	object obj;
	obj.setString("a", "1");
	obj.setString("b", "2");
	auto cookie = obj.getCookieString();
	// separator should be between items, not after the last
	EXPECT_NE(cookie.find("; "), string::npos);
	EXPECT_EQ(cookie.substr(cookie.size() - 1), "2");
	EXPECT_EQ(cookie, "a=1; b=2");
}

TEST(object_prototype_inheritance) {
	auto proto = object({
		{"greet", func([](list) -> var {
			 return string("hello");
		 })}
	});
	auto child = object({});
	child.setParent(proto);
	EXPECT_TRUE(child.inherits(proto));
	EXPECT_EQ(child.getVar("greet").getType(), typeFunction);
}

TEST(file_save_load) {
	auto path = file::currentWorkingDir() + "/goldtest_tmp.bin";
	auto data = string("some test data");
	auto err = file::saveFile(path, data);
	EXPECT_FALSE(err.isError());
	auto f = file::readFile(path);
	EXPECT_FALSE(f.isError());
	auto fo = f.getObject<file>();
	EXPECT_EQ(fo.getStringView("data"), string_view("some test data"));
	remove(path.c_str());
}

TEST(file_base64) {
	auto data = binary({1, 2, 3, 4, 5});
	auto encoded = file::encodeBase64(data);
	EXPECT_TRUE(encoded.size() > 0);
	auto decoded = file::decodeBase64(encoded);
	EXPECT_TRUE(binaryEqual(decoded, data));
}

TEST(file_asset_pack_roundtrip) {
	auto packed = file::pack(list({
		obj({{"path", "z.txt"}, {"data", binary({'z'})}}),
		obj({{"path", "a.bin"}, {"data", binary({1, 2, 3})}}),
	}));
	EXPECT_FALSE(packed.isError());
	auto unpacked = file::unpack(packed.getBinary());
	EXPECT_FALSE(unpacked.isError());
	EXPECT_EQ(unpacked.getList().size(), (uint64_t)2);
	EXPECT_EQ(unpacked.getList().getObject(0).getString("path"), "a.bin");
	EXPECT_EQ(unpacked.getList().getObject(1).getString("path"), "z.txt");
}

TEST(file_asset_pack_rejects_unsafe_input) {
	EXPECT_TRUE(file::pack(list({obj({{"path", "../secret"}})})).isError());
	auto bad = binary({'G', 'O', 'L', 'D', 'P', 'A', 'K', '1', 1, 0, 0, 0});
	EXPECT_TRUE(file::unpack(bad).isError());
}

TEST(object_generate_hash) {
	auto h1 = object::generateHash("password", "salt");
	auto h2 = object::generateHash("password", "salt");
	auto h3 = object::generateHash("password", "other");
	EXPECT_FALSE(h1.isError());
	EXPECT_EQ((string)h1, (string)h2);
	EXPECT_NE((string)h1, (string)h3);
}

TEST(generic_error) {
	var e = genericError("boom");
	EXPECT_TRUE(e.isError());
	auto err = e.getError();
	EXPECT_NE(err, (genericError*)nullptr);
	EXPECT_EQ(err->getString("msg"), "boom");
}

TEST(promise_synchronous_execution) {
	auto callback = func([](list args) -> var {
		return var(args.getInt64(1) * 2);
	});
	promise task(object(), callback, list({int64_t(21)}));
	EXPECT_EQ(task.await().getInt64(), 42);
	EXPECT_TRUE((bool)task);
}

TEST(explode_string) {
	auto parts = explode("a,b,c", list({var(int8_t(','))}));
	EXPECT_EQ(parts.size(), (uint64_t)3);
	EXPECT_EQ(parts.getString(0), "a");
	EXPECT_EQ(parts.getString(1), "b");
	EXPECT_EQ(parts.getString(2), "c");
}

TEST(explode_edge_cases) {
	auto comma = list({var(int8_t(','))});
	// trailing delimiter
	auto trailing = explode("a,b,", comma);
	EXPECT_EQ(trailing.size(), (uint64_t)2);
	EXPECT_EQ(trailing.getString(0), "a");
	EXPECT_EQ(trailing.getString(1), "b");
	// consecutive delimiters collapse
	auto collapsed = explode("a,,b", comma);
	EXPECT_EQ(collapsed.size(), (uint64_t)2);
	EXPECT_EQ(collapsed.getString(0), "a");
	EXPECT_EQ(collapsed.getString(1), "b");
	// empty input yields one empty token
	auto empty = explode("", comma);
	EXPECT_EQ(empty.size(), (uint64_t)1);
	EXPECT_EQ(empty.getString(0), "");
	// no delimiters
	auto none = explode("hello", comma);
	EXPECT_EQ(none.size(), (uint64_t)1);
	EXPECT_EQ(none.getString(0), "hello");
	// multiple distinct delimiters
	auto multi = explode("a;b,c", list({var(int8_t(';')), var(int8_t(','))}));
	EXPECT_EQ(multi.size(), (uint64_t)3);
	EXPECT_EQ(multi.getString(2), "c");
}

TEST(malformed_json_is_error) {
	auto bad = file::parseJSON("this is not json{");
	EXPECT_TRUE(bad.isError());
	auto bad2 = file::parseJSON("{\"unterminated\": ");
	EXPECT_TRUE(bad2.isError());
}

TEST(malformed_binary_formats_are_error) {
	binary junk = {0xFF, 0xFF, 0xFF, 0xFF};
	auto v = string_view((char*)junk.data(), junk.size());
	EXPECT_TRUE(file::parseBSON(v).isError());
	EXPECT_TRUE(file::parseCBOR(v).isError());
	EXPECT_TRUE(file::parseMsgPack(v).isError());
	EXPECT_TRUE(file::parseUBJSON(v).isError());
}

TEST(parse_url_encoded) {
	object result;
	object::parseURLEncoded("a=1&b=hello%20world&c=3", result);
	EXPECT_EQ(result.getString("a"), "1");
	EXPECT_EQ(result.getString("b"), "hello world");
	EXPECT_EQ(result.getString("c"), "3");
	// trailing '%' (incomplete escape) must not read out of bounds
	object result2;
	object::parseURLEncoded("x=abc%", result2);
	EXPECT_NE(result2.getString("x"), "");
}

TEST(parse_cookie) {
	object result;
	object::parseCookie("sid=abc123; theme=dark", result);
	EXPECT_EQ(result.getString("sid"), "abc123");
	EXPECT_EQ(result.getString("theme"), "dark");
	// trailing '%' must not read out of bounds
	object result2;
	object::parseCookie("x=abc%", result2);
	EXPECT_NE(result2.getString("x"), "");
}

TEST(object_default_iteration) {
	// begin()/end() must not crash on a default-constructed object
	object empty;
	size_t count = 0;
	for (auto it = empty.begin(); it != empty.end(); ++it) count++;
	EXPECT_EQ(count, (size_t)0);
}

TEST(object_default_equality) {
	object a;
	object b;
	EXPECT_TRUE(a == b);
	a.setString("k", "v");
	EXPECT_FALSE(a == b);
	EXPECT_TRUE(a == a);
}

TEST(var_string_view_cast_is_explicit) {
	// explicit cast still works for actual string data
	var s = var(string("hello"));
	auto sv = static_cast<string_view>(s);
	EXPECT_EQ(sv, string_view("hello"));
	// numeric var: string conversion must use operator string(), not view
	EXPECT_EQ(var(int64_t(42)).getString(), "42");
}

TEST(module_loader) {
	// module::load is lazy and safe: a missing module just returns false.
	EXPECT_FALSE(module::isLoaded("does_not_exist"));
	EXPECT_FALSE(module::load("does_not_exist"));
	EXPECT_FALSE(module::isLoaded("does_not_exist"));
}

TEST(move_semantics) {
	// var move
	var v = var(string("hello"));
	var v2 = std::move(v);
	EXPECT_EQ(v2.getString(), "hello");
	EXPECT_TRUE(v.isEmpty());
	// list move
	list l;
	l.pushInt64(7);
	list l2 = std::move(l);
	EXPECT_EQ(l2.getInt64(0), 7);
	EXPECT_EQ(l.size(), (uint64_t)0);
	// object move
	object o;
	o.setString("k", "v");
	object o2 = std::move(o);
	EXPECT_EQ(o2.getString("k"), "v");
	EXPECT_FALSE(o);
}

TEST(list_get_color) {
	// 3-channel input should not read a 4th element
	auto c3 = list::getColor(list({var(1.0f), var(0.5f), var(0.25f)}));
	EXPECT_TRUE(c3 != 0);
	auto c4 = list::getColor(list({var(1.0f), var(0.5f), var(0.25f), var(1.0f)}));
	EXPECT_TRUE(c4 != 0);
}

TEST(list_safe_empty_operations) {
	list empty;
	empty.pop();
	EXPECT_EQ(empty.getType(0), typeNull);
	EXPECT_EQ(list::getColor(empty), (uint32_t)0);
	list target;
	empty.assignList(0, target);
	EXPECT_EQ(target.size(), (uint64_t)0);
}

TEST(list_get_vec2f_short_input) {
	EXPECT_TRUE(list::getVec2f(list()).isEmpty());
	EXPECT_TRUE(list::getVec2f(list({var(1.0f)})).isEmpty());
}

TEST(file_missing_write_time_is_error) {
	file missing(path("/tmp/gold-file-that-does-not-exist"));
	EXPECT_TRUE(missing.getWriteTime().isError());
}

TEST(list_functions) {
		auto fn = func([](list) -> var { return var(int64_t(99)); });
	list li;
	li.pushFunc(fn);
	EXPECT_EQ(li.getFunction(0)({}).getInt64(), 99);
}

TEST(js_sugar) {
	// Object/array literals, JS-style.
	auto user = jo(
		"name", "bob",
		"age", 30,
		"score", 9.5,
		"admin", true,
		"tags", ja("admin", "dev"),
		"meta", jo("joined", 2020, "active", true));

	EXPECT_EQ(user.getString("name"), "bob");
	EXPECT_EQ(user.getInt64("age"), 30);

	// Property get/set through the varRef proxy (writes through).
	user["age"] = 31;
	user["score"] = 10.0;
	user["nickname"] = "bobby";
	user["meta"]["active"] = false;
	user["tags"][0] = "staff";
	EXPECT_EQ(user.getInt64("age"), 31);
	EXPECT_NEAR(user.getDouble("score"), 10.0, 1e-9);
	EXPECT_EQ(user.getString("nickname"), "bobby");
	EXPECT_FALSE(user.getObject("meta").getBool("active"));
	EXPECT_EQ(user.getList("tags").getString(0), "staff");

	// Implicit conversions read the value.
	string name = user["name"];
	int64_t age = user["age"];
	EXPECT_EQ(name, "bob");
	EXPECT_EQ(age, 31);
	EXPECT_TRUE((bool)user["admin"]);
	EXPECT_FALSE((bool)user["meta"]["active"]);

	// var indexing (object wrapped in a var).
	var v = jo("a", 1, "b", "x");
	v["b"] = "y";
	EXPECT_EQ(v["b"].getString(), "y");
	EXPECT_EQ(v["a"].getInt64(), 1);

	// list element write-through.
	auto li = ja(1, 2, 3);
	li[1] = 99;
	EXPECT_EQ(li.getInt64(1), 99);

	// Template strings.
	EXPECT_EQ(tpl("Hello $0, $1", "bob", 31), "Hello bob, 31");
	EXPECT_EQ(tpl("no placeholders"), "no placeholders");

	// Array helpers.
	auto nums = ja(1, 2, 3, 4, 5);
	auto doubled = mapArr(nums, func([](list a) -> var {
		return var(a[0].getInt64() * 2);
	}));
	EXPECT_EQ(join(doubled), "2,4,6,8,10");
	auto evens = filter(nums, func([](list a) -> var {
		return var(a[0].getInt64() % 2 == 0);
	}));
	EXPECT_EQ(join(evens), "2,4");
	auto first = findArr(nums, func([](list a) -> var {
		return var(a[0].getInt64() > 3);
	}));
	EXPECT_EQ(first.getInt64(), 4);
	EXPECT_EQ(join(ja("x", "y", "z"), "-"), "x-y-z");

	// JSON shorthand round-trip.
	auto js = toJSON(user);
	auto parsed = fromJSON(js);
	EXPECT_EQ(parsed["name"].getString(), "bob");
	EXPECT_EQ(parsed["meta"]["joined"].getInt64(), 2020);
}

TEST(object_proto_key_fallback) {
	// A "proto" key pointing at an object behaves as a data-driven
	// prototype scope on the read path (object::getExpression), even when
	// it has not been promoted into data->parent by findParent().
	auto base = jo("aliveColor", vec4f(0, 1, 0, 1));
	auto wrapper = jo("proto", base);
	EXPECT_EQ(wrapper.getVar("aliveColor").getType(), typeVec4Float);
	auto nested = jo("proto", wrapper);
	EXPECT_EQ(nested.getVar("aliveColor").getType(), typeVec4Float);
	EXPECT_EQ(nested.getVar("missing").getType(), typeNull);
}

int main() {
	return goldtest::runAll();
}
