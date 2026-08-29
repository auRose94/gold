#include <iostream>
#include <sstream>

#include "goldjs.hpp"
#include "goldtest.hpp"
#include "lang/lang.hpp"

using namespace gold;

TEST(lang_arithmetic) {
	auto r = langRun("let a = 5; let b = 7; a + b * 2;", object(), false);
	EXPECT_TRUE(r.isNumber());
	EXPECT_EQ(r.getInt64(), 19);
}

TEST(lang_strings_templates) {
	auto r = langRun(
		"const name = \"bob\"; const n = 3; `Hello ${name}, you have ${n} items`;",
		object(), false);
	EXPECT_EQ(r.getString(), "Hello bob, you have 3 items");
}

TEST(lang_objects_arrays) {
	auto r = langRun(
		"const u = { name: \"alice\", age: 30, tags: [\"a\", \"b\"] }; "
		"u.age = 31; u.tags[1];",
		object(), false);
	EXPECT_EQ(r.getString(), "b");
}

TEST(lang_functions_arrow) {
	auto r = langRun(
		"function add(a, b) { return a + b; } "
		"const inc = (x) => x + 1; add(2, 3) + inc(10);",
		object(), false);
	EXPECT_EQ(r.getInt64(), 16);
}

TEST(lang_control_flow) {
	auto r = langRun(
		"let sum = 0; for (let i = 0; i < 5; i++) { sum += i; } "
		"let w = 0; let j = 0; while (j < 3) { w += j; j++; } "
		"sum + w;",
		object(), false);
	EXPECT_EQ(r.getInt64(), 13);  // 10 + 3
}

TEST(lang_classes) {
	auto r = langRun(
		"class Point { constructor(x, y) { this.x = x; this.y = y; } "
		"len() { return this.x * this.x + this.y * this.y; } } "
		"const p = new Point(3, 4); p.len();",
		object(), false);
	EXPECT_EQ(r.getInt64(), 25);
}

TEST(lang_optional_types) {
	// Annotations parsed; enforcement off.
	auto ok = langRun("let n: number = 42; n;", object(), false);
	EXPECT_EQ(ok.getInt64(), 42);
	// Enforcement on: correct type passes.
	auto pass = langRun("let n: number = 42; n;", object(), true);
	EXPECT_EQ(pass.getInt64(), 42);
	// Enforcement on: wrong type is an error.
	auto bad = langRun("let n: number = \"hi\";", object(), true);
	EXPECT_TRUE(bad.isError());
}

TEST(lang_errors) {
	auto r = langRun("let = ;", object(), false);
	EXPECT_TRUE(r.isError());
	auto r2 = langRun("foo(1);", object(), false);  // unknown fn
	EXPECT_TRUE(r2.isError());
}

TEST(lang_try_catch) {
	auto r = langRun(
		"let h; try { throw \"boom\"; } catch (e) { h = `caught: ${e}`; } "
		"try { 5 + 5; } catch (e) { h = \"no\"; } h;",
		object(), false);
	EXPECT_EQ(r.getString(), "caught: boom");
	auto un = langRun("throw \"oops\";", object(), false);
	EXPECT_TRUE(un.isError());
}

TEST(lang_iteration) {
	auto r = langRun(
		"let s = 0; for (const x of [1,2,3,4]) { s += x; } s;",
		object(), false);
	EXPECT_EQ(r.getInt64(), 10);
	auto keys = langRun(
		"const u = { a: 1, b: 2 }; let k = []; for (const name in u) { k.push(name); } k.join(\",\");",
		object(), false);
	EXPECT_EQ(keys.getString(), "a,b");
}

TEST(lang_builtins) {
	auto str = langRun(
		"const s = \"Hello World\"; s.toUpperCase() + \"/\" + s.length + \"/\" + s.indexOf(\"World\");",
		object(), false);
	EXPECT_EQ(str.getString(), "HELLO WORLD/11/6");
	auto arr = langRun(
		"const a = [1,2,3]; a.push(4); a.join(\"-\") + \"/\" + a.length;",
		object(), false);
	EXPECT_EQ(arr.getString(), "1-2-3-4/4");
	auto map = langRun(
		"const a = [1,2,3]; a.map((x) => x * 2).join(\",\");",
		object(), false);
	EXPECT_EQ(map.getString(), "2,4,6");
	auto math = langRun("Math.round(7.7) + Math.floor(7.7);", object(), false);
	EXPECT_EQ(math.getInt64(), 15);
}

TEST(lang_misc) {
	EXPECT_EQ(langRun("typeof 42 + \" \" + typeof \"x\" + \" \" + typeof [1] + \" \" + typeof {};",
		object(), false).getString(), "number string array object");
	EXPECT_TRUE(langRun("\"a\" in { a: 1 };", object(), false).getBool());
	EXPECT_EQ(langRun("function f(a, b = 10) { return a + b; } f(5);",
		object(), false).getInt64(), 15);
	// closures over multiple scopes
	EXPECT_EQ(langRun("const make = (n) => (x) => x + n; const add5 = make(5); add5(10);",
		object(), false).getInt64(), 15);
	// multi-declarators
	EXPECT_EQ(langRun("const a = 1, b = 2; a + b;", object(), false).getInt64(), 3);
}

TEST(lang_repl) {
	std::stringstream in;
	in << "1 + 2;\n";
	in << "const s = \"hi\"; s.toUpperCase();\n";
	in << "try { throw 7; } catch (e) { `got ${e}`; }\n";
	std::stringstream out;
	langRepl(in, out, false);
	auto text = out.str();
	EXPECT_NE(text.find("3"), string::npos);
	EXPECT_NE(text.find("HI"), string::npos);
	EXPECT_NE(text.find("got 7"), string::npos);
}

TEST(lang_script_facade) {
	auto s = script({{"source", var(string(
		"function greet(name) { return `Hello ${name}!`; } "
		"function add(a, b) { return a + b; } greet(\"gold\");"))}});
	EXPECT_TRUE(s.load().isEmpty());
	auto r = s.run();
	EXPECT_EQ(r.getString(), "Hello gold!");
	// Shared gold global.
	s.setGlobal({var(string("base")), var(int64_t(100))});
	auto e = s.eval({var(string("base + add(1, 2)"))});
	EXPECT_EQ(e.getInt64(), 103);
	// Call a defined function with gold objects.
	auto g = s.call({var(string("greet")), var(string("world"))});
	EXPECT_EQ(g.getString(), "Hello world!");
	// Pass a gold object in and read it back.
	auto host = jo("name", "gold", "count", 3);
	s.setGlobal({var(string("host")), var(host)});
	auto access = s.eval({var(string("`${host.name}-${host.count}`"))});
	EXPECT_EQ(access.getString(), "gold-3");
}

int main() {
	return goldtest::runAll();
}