#include <filesystem>

#include "goldjs.hpp"
#include "goldtest.hpp"
#include "web/database.hpp"
#include "web/dataStore.hpp"
#include "web/css.hpp"
#include "web/html.hpp"

using namespace gold;

TEST(file_data_store_crud) {
	const auto root = std::filesystem::temp_directory_path() /
		"gold_data_store_test";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);

	auto store = createDataStore("file");
	EXPECT_TRUE(store != nullptr);
	if (!store) return;
	EXPECT_TRUE(store->open("testdb", root.string()));
	EXPECT_TRUE(store->createCollection("users"));

	auto inserted = store->insert("users",
		jo("name", "alice", "tags", list({"admin", "user"})));
	EXPECT_TRUE(inserted.isObject());
	if (inserted.isObject()) {
		auto id = inserted.getObject().getString("_id");
		EXPECT_EQ(id.size(), (size_t)24);
		auto found = store->findOne("users", jo("name", "alice"));
		EXPECT_EQ(found.getString("_id"), id);

		auto updated = store->updateOne("users", jo("_id", id),
			jo("$set", jo("name", "bob")));
		EXPECT_EQ(updated.getObject().getString("name"), "bob");

		auto in = store->find("users",
			jo("tags", jo("$in", list({"admin"}))));
		EXPECT_EQ(in.size(), (uint64_t)1);
		auto deleted = store->deleteOne("users", jo("_id", id));
		EXPECT_EQ(deleted.getObject().getUInt64("deletedCount"), (uint64_t)1);
		EXPECT_FALSE(store->findOne("users", jo("_id", id)));
	}

	delete store;
	std::filesystem::remove_all(root, ec);
}

TEST(database_facade_rejects_missing_arguments) {
	database db({{"backend", "file"}, {"name", "test"},
		{"path", std::filesystem::temp_directory_path().string()}});
	EXPECT_TRUE(db.createCollection(list()).isError());
	EXPECT_TRUE(db.getCollection(list()).isError());
}

// ---------------------------------------------------------------- HTML parser

TEST(html_parser_nested_tree) {
	auto roots = Parser::parseHTML("<div class='foo'><p>Hello</p></div>");
	EXPECT_EQ(roots.size(), (uint64_t)1);

	auto div = roots.getObject(0, HTML::iHTML());
	EXPECT_EQ(div.getString("tag"), std::string("div"));
	EXPECT_EQ(div.getObject("attr").getString("class"), std::string("foo"));

	auto items = div.getList("items");
	EXPECT_EQ(items.size(), (uint64_t)1);
	auto p = items.getObject(0, HTML::iHTML());
	EXPECT_EQ(p.getString("tag"), std::string("p"));
	EXPECT_EQ(p.getList("items").getString(0), std::string("Hello"));
}

TEST(html_parser_round_trips_to_string) {
	auto roots = Parser::parseHTML("<div class='foo'><p>Hello</p></div>");
	EXPECT_EQ((string)roots.getObject(0, HTML::iHTML()),
		std::string("<div class=\"foo\"><p>Hello</p></div>"));
}

TEST(html_parser_attribute_forms) {
	auto roots = Parser::parseHTML(
		"<input type=\"text\" name=q disabled data-x='1 2'>");
	auto attr = roots.getObject(0, HTML::iHTML()).getObject("attr");
	EXPECT_EQ(attr.getString("type"), std::string("text"));
	EXPECT_EQ(attr.getString("name"), std::string("q"));
	EXPECT_TRUE(attr.getBool("disabled", false));
	EXPECT_EQ(attr.getString("data-x"), std::string("1 2"));
}

TEST(html_parser_void_and_self_closing) {
	auto roots = Parser::parseHTML("<div><br><img src='a.png'><hr/></div>");
	auto div = roots.getObject(0, HTML::iHTML());
	// Void tags never nest, so all three stay direct children of <div>.
	EXPECT_EQ(div.getList("items").size(), (uint64_t)3);
	EXPECT_EQ(div.getList("items").getObject(1, HTML::iHTML()).getString("tag"),
		std::string("img"));
}

TEST(html_parser_skips_comments_and_doctype) {
	auto roots = Parser::parseHTML(
		"<!DOCTYPE html><!-- hidden --><p>shown</p>");
	EXPECT_EQ(roots.size(), (uint64_t)1);
	EXPECT_EQ(roots.getObject(0, HTML::iHTML()).getList("items").getString(0),
		std::string("shown"));
}

TEST(html_parser_implicit_close) {
	auto roots = Parser::parseHTML("<ul><li>a<li>b</ul>");
	auto ul = roots.getObject(0, HTML::iHTML());
	EXPECT_EQ(ul.getList("items").size(), (uint64_t)2);
	EXPECT_EQ(ul.getList("items").getObject(1, HTML::iHTML()).getString("tag"),
		std::string("li"));
}

TEST(html_parser_raw_text_elements) {
	auto roots = Parser::parseHTML("<style>p{color:red}</style><p>after</p>");
	EXPECT_EQ(roots.size(), (uint64_t)2);
	EXPECT_EQ(roots.getObject(0, HTML::iHTML()).getList("items").getString(0),
		std::string("p{color:red}"));
	EXPECT_EQ(roots.getObject(1, HTML::iHTML()).getString("tag"),
		std::string("p"));
}

TEST(html_parser_unbalanced_close_tag_is_ignored) {
	auto roots = Parser::parseHTML("</span><p>ok</p>");
	EXPECT_EQ(roots.size(), (uint64_t)1);
	EXPECT_EQ(roots.getObject(0, HTML::iHTML()).getString("tag"),
		std::string("p"));
}

TEST(html_void_tag_table) {
	EXPECT_TRUE(Parser::isVoidTag("BR"));
	EXPECT_FALSE(Parser::isVoidTag("div"));
	EXPECT_TRUE(Parser::voidTags().size() > 10);
}

// ----------------------------------------------------------------- CSS parser

TEST(css_parser_rules_and_declarations) {
	auto rules = CSS::parseCSS(
		"/* c */ p, .x { color: red; margin : 0 } #id { width: 50%; }");
	EXPECT_EQ(rules.size(), (uint64_t)2);
	EXPECT_EQ(rules.getObject(0, CSS::Rule()).getString("selector"),
		std::string("p, .x"));
	auto decl = rules.getObject(0, CSS::Rule()).getObject("declarations");
	EXPECT_EQ(decl.getString("color"), std::string("red"));
	EXPECT_EQ(decl.getInt64("margin", -1), (int64_t)0);
	EXPECT_EQ(rules.getObject(1, CSS::Rule()).getObject("declarations")
				  .getString("width"),
		std::string("50%"));
}

TEST(css_parser_function_values_with_semicolons_inside) {
	auto decl = CSS::parseDeclarations("background: rgba(1,2,3,0.5); x: 1");
	EXPECT_EQ(decl.size(), (uint64_t)2);
	EXPECT_EQ(decl.getString("background"), std::string("rgba(1,2,3,0.5)"));
}

int main() {
	return goldtest::runAll();
}
