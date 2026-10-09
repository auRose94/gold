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

TEST(data_store_update_operators) {
	const auto root = std::filesystem::temp_directory_path() /
		"gold_data_store_ops_test";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);

	auto store = createDataStore("file");
	EXPECT_TRUE(store != nullptr);
	if (!store) return;
	EXPECT_TRUE(store->open("opsdb", root.string()));
	EXPECT_TRUE(store->createCollection("c"));

	auto doc = store->insert("c", jo("count", 5, "temp", "x"));
	const auto id = doc.getObject().getString("_id");

	// $inc adds to a numeric field.
	store->updateOne("c", jo("_id", id), jo("$inc", jo("count", 3)));
	EXPECT_EQ(store->findOne("c", jo("_id", id)).getInt64("count", -1),
		(int64_t)8);

	// $unset removes fields.
	store->updateOne("c", jo("_id", id), jo("$unset", jo("temp", 1)));
	EXPECT_EQ(store->findOne("c", jo("_id", id)).getType("temp"), typeNull);

	// An update with no supported operator is an error, not a silent
	// success that rewrites nothing.
	EXPECT_TRUE(store->updateOne("c", jo("_id", id),
		jo("$bump", jo("count", 1))).isError());

	delete store;
	std::filesystem::remove_all(root, ec);
}

TEST(data_store_metadata_files_are_not_documents) {
	const auto root = std::filesystem::temp_directory_path() /
		"gold_data_store_meta_test";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);

	auto store = createDataStore("file");
	EXPECT_TRUE(store != nullptr);
	if (!store) return;
	EXPECT_TRUE(store->open("metadb", root.string()));
	EXPECT_TRUE(store->createCollection("c"));
	store->insert("c", jo("n", 1));
	const auto id = store->findOne("c", jo()).getString("_id");

	// The advisory index metadata must never appear as a document...
	EXPECT_TRUE(store->addIndexes("c", jo("n", 1)));
	EXPECT_EQ(store->find("c", jo()).size(), (uint64_t)1);
	// ...and dropIndex clears it through the facade (by collection, not
	// by an index name that would name a different collection).
	EXPECT_TRUE(store->dropIndex("c"));
	EXPECT_TRUE(std::filesystem::exists(root / "metadb" / "c" / ".index.json") ==
		false);

	// Inserting an id that already exists is an error, not a replacement.
	EXPECT_TRUE(store->insert("c", jo("_id", id, "n", 99)).isError());
	EXPECT_EQ(store->findOne("c", jo("_id", id)).getInt64("n", -1), (int64_t)1);

	// replace() keeps the _id of the replaced document.
	auto replaced = store->replace("c", jo("_id", id), jo("n", 9));
	EXPECT_EQ(replaced.getObject().getString("_id"), id);
	EXPECT_EQ(store->findOne("c", jo("_id", id)).getInt64("n", -1), (int64_t)9);

	delete store;
	std::filesystem::remove_all(root, ec);
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

TEST(html_parser_decodes_entities_in_text) {
	auto roots = Parser::parseHTML(
		"<p>Fish &amp; Chips &lt;3 &#65;&#x42; &nbsp;&mdash;&copy;</p>");
	auto p = roots.getObject(0, HTML::iHTML());
	EXPECT_EQ(p.getList("items").getString(0),
		std::string("Fish & Chips <3 AB \xC2\xA0\xE2\x80\x94\xC2\xA9"));
}

TEST(html_parser_decodes_entities_in_attributes) {
	auto roots = Parser::parseHTML(
		"<a title='Fish &amp; Chips' href='/x?a=1&amp;b=2'>go</a>");
	auto a = roots.getObject(0, HTML::iHTML());
	EXPECT_EQ(a.getObject("attr").getString("title"),
		std::string("Fish & Chips"));
	EXPECT_EQ(a.getObject("attr").getString("href"), std::string("/x?a=1&b=2"));
}

TEST(html_parser_unknown_entities_survive) {
	// A named entity outside the set, and a truncated ampersand, are left
	// as written rather than dropped.
	auto roots = Parser::parseHTML("<p>100 &nope; &amp 10 &lt;= 20</p>");
	auto p = roots.getObject(0, HTML::iHTML());
	EXPECT_EQ(p.getList("items").getString(0),
		std::string("100 &nope; &amp 10 <= 20"));
}

TEST(html_parser_title_and_textarea_decode_entities) {
	auto roots = Parser::parseHTML(
		"<title>a &amp; b</title><textarea>1 &lt; 2</textarea>");
	EXPECT_EQ(roots.getObject(0, HTML::iHTML()).getList("items").getString(0),
		std::string("a & b"));
	EXPECT_EQ(roots.getObject(1, HTML::iHTML()).getList("items").getString(0),
		std::string("1 < 2"));
}

TEST(html_parser_keeps_script_and_style_content_raw) {
	const string markup =
		"<script>if (a < b && c > d) { x(); }</script>"
		"<style>p > li:not(.x) { color: red }</style><p>ok</p>";
	auto roots = Parser::parseHTML(markup);
	EXPECT_EQ(roots.size(), (uint64_t)3);
	EXPECT_EQ(roots.getObject(0, HTML::iHTML()).getList("items").getString(0),
		std::string("if (a < b && c > d) { x(); }"));
	EXPECT_EQ(roots.getObject(1, HTML::iHTML()).getList("items").getString(0),
		std::string("p > li:not(.x) { color: red }"));
	// And they serialize verbatim, so parse(serialize) is stable.
	EXPECT_EQ((string)roots.getObject(0, HTML::iHTML()),
		std::string("<script>if (a < b && c > d) { x(); }</script>"));
}

TEST(html_parser_escapes_special_text_on_serialize) {
	// The builder puts markup-significant characters into a text node.
	auto element = HTML::p(list({"Fish & <Chips> \"quoted\""}));
	const string markup = (string)element;
	EXPECT_EQ(markup, std::string("<p>Fish &amp; &lt;Chips&gt; \"quoted\"</p>"));
	// Parse -> serialize -> parse is stable at this form.
	auto back = Parser::parseHTML(markup);
	EXPECT_EQ((string)back.getObject(0, HTML::iHTML()), markup);
	EXPECT_EQ(back.getObject(0, HTML::iHTML()).getList("items").getString(0),
		std::string("Fish & <Chips> \"quoted\""));
}

// ------------------------------------------------------------- HTML builder

TEST(html_builder_attributes_and_items) {
	// Element ctors take one `list` of: child elements (first), attribute
	// bundles (merged), strings and lists. An `items` key inside a bundle
	// routes to children instead of becoming an attribute.
	auto el = HTML::div(list({
		HTML::h2(list({"Title"})),
		HTML::p(list({"Body"})),
		jo("class", "card", "id", "main"),
		jo("items", ja("tail")),
	}));
	EXPECT_EQ(el.getString("tag"), std::string("div"));
	EXPECT_EQ(el.getObject("attr").getString("class"), std::string("card"));
	EXPECT_EQ(el.getObject("attr").getString("id"), std::string("main"));
	auto items = el.getList("items");
	EXPECT_EQ(items.size(), (uint64_t)3);
	if (items.size() == (uint64_t)3) {
		EXPECT_EQ(items.getObject(0, HTML::iHTML()).getString("tag"),
			std::string("h2"));
		EXPECT_EQ(items.getObject(1, HTML::iHTML()).getString("tag"),
			std::string("p"));
		EXPECT_EQ(items.getString(2), std::string("tail"));
	}
}

TEST(html_builder_operators_and_methods) {
	HTML::div box(list({"inner"}));
	box += list({HTML::span(list({"more"})), jo("data-k", "v")});
	EXPECT_EQ(box.getObject("attr").getString("data-k"), std::string("v"));
	EXPECT_EQ(box.getList("items").size(), (uint64_t)2);

	// setAttributes merges one attribute bundle into the element.
	box.setAttributes(list({jo("lang", "en")}));
	EXPECT_EQ(box.getObject("attr").getString("lang"), std::string("en"));
	EXPECT_EQ(box.getObject("attr").getString("data-k"), std::string("v"));
	// getAttribute hands back the live attribute object.
	box.getAttribute(list()).getObject().setString("lang", "sv");
	EXPECT_EQ(box.getObject("attr").getString("lang"), std::string("sv"));

	// addElements / removeElement manage the children list.
	box.addElements(list({"tail text"}));
	EXPECT_EQ(box.getList("items").size(), (uint64_t)3);
	auto span = HTML::span(list({"gone"}));
	box.addElements(list({span}));
	EXPECT_EQ(box.getList("items").size(), (uint64_t)4);
	box.removeElement(list({span}));
	EXPECT_EQ(box.getList("items").size(), (uint64_t)3);
	EXPECT_EQ(box.getList("items").getString(2), std::string("tail text"));
}

TEST(html_builder_numeric_attributes_stay_typed) {
	auto el =
		HTML::div(list({jo("width", 300, "checked", true, "note", "plain")}));
	EXPECT_EQ(el.getObject("attr").getInt64("width", 0), (int64_t)300);
	EXPECT_TRUE(el.getObject("attr").getBool("checked", false));
	EXPECT_EQ(el.getObject("attr").getString("note"), std::string("plain"));
}

// ------------------------------------------------- malformed / hostile input

TEST(html_parser_survives_unterminated_input) {
	// No closing '>' on the tag, unterminated comment, unterminated raw
	// text, truncated entity: nothing may hang or throw.
	auto roots = Parser::parseHTML("<div class='x");
	EXPECT_EQ(roots.size(), (uint64_t)1);

	roots = Parser::parseHTML("<!-- no end ever");
	EXPECT_EQ(roots.size(), (uint64_t)0);

	roots = Parser::parseHTML("<script>never closed");
	EXPECT_EQ(roots.size(), (uint64_t)1);
	EXPECT_EQ(roots.getObject(0, HTML::iHTML()).getList("items").getString(0),
		std::string("never closed"));

	roots = Parser::parseHTML("<p>oops &am");
	EXPECT_EQ(roots.getObject(0, HTML::iHTML()).getList("items").getString(0),
		std::string("oops &am"));
}

TEST(html_parser_close_tag_crosses_unclosed_children) {
	// `</div>` pops everything still open inside it, and stray close tags
	// for already-closed elements are ignored.
	auto roots = Parser::parseHTML(
		"<div><span><p>x</span></p></div><p>after</p>");
	EXPECT_EQ(roots.size(), (uint64_t)2);
	EXPECT_EQ(roots.getObject(0, HTML::iHTML()).getString("tag"),
		std::string("div"));
	EXPECT_EQ(roots.getObject(1, HTML::iHTML()).getString("tag"),
		std::string("p"));
}

TEST(html_parser_links_do_not_nest) {
	// `<a>` implicitly closes an open `<a>`, like real browsers.
	auto roots = Parser::parseHTML("<a href='1'>first<a href='2'>second</a>");
	EXPECT_EQ(roots.size(), (uint64_t)2);
	EXPECT_EQ(roots.getObject(0, HTML::iHTML()).getObject("attr")
				  .getString("href"),
		std::string("1"));
	EXPECT_EQ(roots.getObject(1, HTML::iHTML()).getObject("attr")
				  .getString("href"),
		std::string("2"));
}

TEST(html_parser_top_level_text_is_kept) {
	auto roots = Parser::parseHTML("hello <b>x</b> tail");
	EXPECT_EQ(roots.size(), (uint64_t)3);
	EXPECT_EQ(roots.getString(0), std::string("hello "));
	EXPECT_EQ(roots.getString(2), std::string(" tail"));
}

TEST(html_parser_attribute_values_are_strings) {
	// Attribute values are always strings, per HTML: nothing truncates
	// "12.5" through a numeric conversion, and getString just works.
	auto roots = Parser::parseHTML("<div width=300 height='12.5'>x</div>");
	auto attr = roots.getObject(0, HTML::iHTML()).getObject("attr");
	EXPECT_EQ(attr.getString("width"), std::string("300"));
	EXPECT_EQ(attr.getString("height"), std::string("12.5"));
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

TEST(css_parser_rule_streaming) {
	const string css = "a { x: 1 } b { y: 2 } trailing text";
	size_t pos = 0;
	auto first = CSS::parseRule(css, pos);
	EXPECT_EQ(first.getString("selector"), std::string("a"));
	EXPECT_EQ(first.getObject("declarations").getInt64("x", 0), (int64_t)1);
	auto second = CSS::parseRule(css, pos);
	EXPECT_EQ(second.getString("selector"), std::string("b"));
	EXPECT_EQ(second.getObject("declarations").getInt64("y", 0), (int64_t)2);
	// pos moved past both rules, into the trailing text.
	EXPECT_EQ(pos, (size_t)21);
}

TEST(css_parser_important_flag_on_any_value_type) {
	auto decl = CSS::parseDeclarations(
		"margin: 0 !important; width: 100px !important; "
		"color: red !IMPORTANT; z-index: 3");
	EXPECT_EQ(decl.getInt64("margin", -1), (int64_t)0);
	EXPECT_TRUE(decl.getBool("margin!", false));
	EXPECT_EQ(decl.getString("width"), std::string("100px"));
	EXPECT_TRUE(decl.getBool("width!", false));
	EXPECT_EQ(decl.getString("color"), std::string("red"));
	EXPECT_TRUE(decl.getBool("color!", false));
	// No flag on the value without !important.
	EXPECT_TRUE(decl.getType("z-index!") == typeNull);
}

TEST(css_parser_quoted_values_with_semicolons_and_braces) {
	auto decl = CSS::parseDeclarations(
		"font-family: \"Foo; Bar\"; content: 'x}y'; color: rgb(1,2,3)");
	EXPECT_EQ(decl.getString("font-family"), std::string("\"Foo; Bar\""));
	EXPECT_EQ(decl.getString("content"), std::string("'x}y'"));
	EXPECT_EQ(decl.getString("color"), std::string("rgb(1,2,3)"));
}

int main() {
	return goldtest::runAll();
}

TEST(html_void_tags_serialize_without_end_tags) {
	// Parses → serializes: void elements must not emit "</br>"-style end
	// tags (double renders when a browser re-parses them).
	auto roots = Parser::parseHTML(
		"<div><br><img src='a.png'><hr/></div>");
	auto br = roots.getObject(0, HTML::iHTML())
				  .getList("items")
				  .getObject(0, HTML::iHTML());
	auto rendered = (string)br;
	EXPECT_EQ(rendered, std::string("<br>"));

	auto img = roots.getObject(0, HTML::iHTML())
				   .getList("items")
				   .getObject(1, HTML::iHTML());
	EXPECT_EQ((string)img, std::string("<img src=\"a.png\">"));

	// A whole fragment keeps its structure: only the void children lose
	// their end tags.
	auto div = roots.getObject(0, HTML::iHTML());
	auto html = (string)div;
	EXPECT_TRUE(html.find("</div>") != string::npos);
	EXPECT_TRUE(html.find("</br>") == string::npos);
	EXPECT_TRUE(html.find("</img>") == string::npos);
	EXPECT_TRUE(html.find("</hr>") == string::npos);

	// Building a page that mixes void and normal elements round-trips
	// through the same path.
	auto page = HTML::body(list({HTML::img(list({jo("src", "b.png")})),
		HTML::p(list({"text"}) )}));
	auto out = (string)page;
	EXPECT_TRUE(out.find("<img src=\"b.png\">") != string::npos);
	EXPECT_TRUE(out.find("</img>") == string::npos);
	EXPECT_TRUE(out.find("</p>") != string::npos);
}

TEST(css_at_rules_do_not_leak_into_rules) {
	// The plain parser skips at-rules whole: block forms balance their
	// braces, statement forms end at ';'. Pre-fix, "@media (...) { p { x }
	// }" split at the FIRST '}', misparsing the inner selectors.
	auto rules = CSS::parseCSS(
		"@charset \"utf-8\";\n"
		"@media (max-width: 600px) { p { color: red } }\n"
		"@import url(\"x.css\");\n"
		"@keyframes spin { to { left: 1px } }\n"
		".kept { margin: 0 }\n"
		"div.kept { color: blue }\n");
	EXPECT_EQ(rules.size(), (uint64_t)2);
	EXPECT_EQ(rules.getObject(0, CSS::Rule()).getString("selector"),
		string(".kept"));
	EXPECT_EQ(rules.getObject(0, CSS::Rule())
				  .getObject("declarations")
				  .getInt64("margin", -1),
		(int64_t)0);
	EXPECT_EQ(rules.getObject(1, CSS::Rule()).getString("selector"),
		string("div.kept"));
	EXPECT_EQ(
		rules.getObject(1, CSS::Rule()).getObject("declarations").getString(
			"color"),
		string("blue"));

	// parseRule called directly with an at-rule consumes it and returns
	// nothing, leaving pos past the block.
	size_t pos = 0;
	CSS::Rule atRule = CSS::parseRule("@media screen { a { b: c } }", pos);
	EXPECT_EQ(atRule.getString("selector"), string(""));
	EXPECT_EQ(pos, (size_t)string("@media screen { a { b: c } }").size());
}
