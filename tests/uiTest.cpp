#include <cmath>
#include <filesystem>

#include "goldjs.hpp"
#include "goldtest.hpp"
#include "ui/renderer.hpp"
#include "ui/software_renderer.hpp"
#include "ui/style.hpp"
#include "ui/tree.hpp"
#include "web/html.hpp"

using namespace gold;
using namespace gold::UI;

namespace {

	/** Point a renderer at a fixed viewport and paint it once. */
	void setupUI(renderer& ui, const string& html, const string& css = "",
		float width = 200.0f, float height = 120.0f) {
		ui.setViewport(list({(double)width, (double)height}));
		ui.setMarkup(html);
		ui.setStyleSheet(css);
		ui.paint();
	}

	/** The rect of an element descriptor returned by `hit()`. */
	object rectOf(object element) { return element.getObject("rect"); }

	/** Alpha of a pixel in a rendered surface (0 = transparent). */
	int alphaAt(const rasterTarget& target, int x, int y) {
		if (x < 0 || y < 0 || (uint32_t)x >= target.width ||
			(uint32_t)y >= target.height)
			return -1;
		return target.pixels[((size_t)y * target.width + (size_t)x) * 4 + 3];
	}

	/** True when a pixel is close to `rgb` (ignoring alpha). */
	bool pixelNear(const rasterTarget& target, int x, int y, int r, int g,
		int b) {
		if (x < 0 || y < 0 || (uint32_t)x >= target.width ||
			(uint32_t)y >= target.height)
			return false;
		const size_t i = ((size_t)y * target.width + (size_t)x) * 4;
		return std::abs((int)target.pixels[i] - r) <= 12 &&
			std::abs((int)target.pixels[i + 1] - g) <= 12 &&
			std::abs((int)target.pixels[i + 2] - b) <= 12;
	}

	/** Count pixels close to `rgb` inside a rect. */
	int countPixelsNear(const rasterTarget& target, const rect& area, int r,
		int g, int b) {
		int count = 0;
		for (int y = (int)area.y; y < (int)area.bottom(); y++) {
			for (int x = (int)area.x; x < (int)area.right(); x++) {
				if (pixelNear(target, x, y, r, g, b)) count++;
			}
		}
		return count;
	}

	bool isSpaceChar(char c) {
		return c == ' ' || c == '\n' || c == '\t' || c == '\r';
	}

	/** Strip whitespace so markup can be written across several lines. */
	string compact(const string& s) {
		string out;
		for (size_t i = 0; i < s.size(); i++) {
			if (isSpaceChar(s[i])) {
				if (!out.empty() && !isSpaceChar(out.back())) out += ' ';
				continue;
			}
			out += s[i];
		}
		while (!out.empty() && out.back() == ' ') out.pop_back();
		return out;
	}

}  // namespace

// --------------------------------------------------------------- DOM tree

TEST(ui_tree_flattens_elements_and_text) {
	domTree tree = buildTree(compact("<div id='a'><p>hi</p>tail</div>"));
	// div, p, "hi", "tail"
	EXPECT_EQ(tree.nodes.size(), (size_t)4);
	EXPECT_EQ(tree.nodes[0].tag, std::string("div"));
	EXPECT_EQ(tree.nodes[0].attrString("id"), std::string("a"));
	EXPECT_EQ(tree.nodes[1].tag, std::string("p"));
	EXPECT_TRUE(tree.nodes[2].isText);
	EXPECT_EQ(tree.nodes[2].text, std::string("hi"));
	EXPECT_EQ(tree.nodes[1].parent, 0);
	EXPECT_EQ(tree.nodes[2].parent, 1);
	EXPECT_EQ(tree.byCSSId("a"), 0);
}

TEST(ui_tree_class_and_attribute_queries) {
	domTree tree = buildTree("<div class='a b' data-x='1'></div>");
	EXPECT_TRUE(tree.nodes[0].hasClass("a"));
	EXPECT_TRUE(tree.nodes[0].hasClass("b"));
	EXPECT_FALSE(tree.nodes[0].hasClass("c"));
	EXPECT_TRUE(tree.nodes[0].hasAttr("data-x"));
	EXPECT_FALSE(tree.nodes[0].hasAttr("data-y"));
	EXPECT_EQ(tree.nodes[0].attrString("data-x", "none"), std::string("1"));
}

TEST(ui_tree_round_trips_to_html) {
	domTree tree = buildTree("<div class='x'><p>hi</p></div>");
	EXPECT_EQ(compact(tree.toHTML()),
		std::string("<div class=\"x\"><p>hi</p></div>"));
}

// ----------------------------------------------------------- style engine

TEST(ui_style_parses_lengths) {
	length value;
	EXPECT_TRUE(parseLength("12px", value));
	EXPECT_NEAR(value.value, 12.0, 0.001);
	EXPECT_TRUE(parseLength("50%", value));
	EXPECT_TRUE(value.isPercent());
	EXPECT_NEAR(value.value, 50.0, 0.001);
	EXPECT_TRUE(parseLength("1.5em", value));
	EXPECT_TRUE(parseLength("auto", value));
	EXPECT_TRUE(value.isAuto());
	EXPECT_TRUE(parseLength("2rem", value));
	EXPECT_FALSE(parseLength("thick", value));
}

TEST(ui_style_parses_colors) {
	color c;
	EXPECT_TRUE(parseColor("#f00", c));
	EXPECT_NEAR(c.r, 1.0, 0.01);
	EXPECT_NEAR(c.g, 0.0, 0.01);
	EXPECT_TRUE(parseColor("#00ff00", c));
	EXPECT_NEAR(c.g, 1.0, 0.01);
	EXPECT_TRUE(parseColor("rgba(0,0,0,0.5)", c));
	EXPECT_NEAR(c.a, 0.5, 0.01);
	EXPECT_TRUE(parseColor("blue", c));
	EXPECT_NEAR(c.b, 1.0, 0.01);
	EXPECT_TRUE(parseColor("transparent", c));
	EXPECT_TRUE(c.transparent());
	EXPECT_FALSE(parseColor("notacolor", c));
}

TEST(ui_style_parses_gradients) {
	backgroundPaint paint;
	EXPECT_TRUE(parseBackgroundImage(
		"linear-gradient(90deg, #000, #fff)", paint));
	EXPECT_EQ((int)paint.kind, (int)paintKind::linearGradient);
	EXPECT_EQ(paint.stops.size(), (size_t)2);
	EXPECT_NEAR(paint.stops[0].offset, 0.0, 0.001);
	EXPECT_NEAR(paint.stops[1].offset, 1.0, 0.001);
	EXPECT_NEAR(paint.angle, 90.0, 0.001);
	EXPECT_TRUE(parseBackgroundImage("none", paint));
	EXPECT_EQ((int)paint.kind, (int)paintKind::none);
}

TEST(ui_style_parses_track_lists) {
	vector<trackSpec> tracks;
	EXPECT_TRUE(parseTracks("100px 1fr auto", tracks));
	EXPECT_EQ(tracks.size(), (size_t)3);
	EXPECT_EQ((int)tracks[0].k, (int)trackSpec::kind::fixed);
	EXPECT_EQ((int)tracks[1].k, (int)trackSpec::kind::flexible);
	EXPECT_EQ((int)tracks[2].k, (int)trackSpec::kind::autoTrack);
	EXPECT_TRUE(parseTracks("repeat(3, 1fr)", tracks));
	EXPECT_EQ(tracks.size(), (size_t)3);
	if (tracks.size() == 3)
		EXPECT_EQ((int)tracks[2].k, (int)trackSpec::kind::flexible);
}

TEST(ui_selector_matching) {
	domTree tree = buildTree(compact(
		"<div class='wrap'><p class='lead'>a</p><p>b</p></div>"));
	stylesheet sheet = parseStylesheet(
		"p { color: red } .lead { color: lime } "
		".wrap > p { font-size: 20px } div p { font-weight: 700 }");

	styleContext ctx;
	vector<computedStyle> styles = resolveStyles(tree, sheet, ctx);

	const int firstP = 1;   // div, p.lead, "a", p, "b"
	const int secondP = 3;
	// `.lead` (0,1,0) outranks `p` (0,0,1).
	EXPECT_NEAR(styles[(size_t)firstP].textColor.g, 1.0, 0.01);
	// The plain `p` rule applies to the second paragraph.
	EXPECT_NEAR(styles[(size_t)secondP].textColor.r, 1.0, 0.01);
	EXPECT_NEAR(styles[(size_t)secondP].textColor.g, 0.0, 0.01);
	// `.wrap > p` is a child combinator and matches both.
	EXPECT_NEAR(fontSizeOf(styles[(size_t)firstP], 16.0f, 16.0f,
				 layoutContext()),
		20.0, 0.01);
	// `div p` is a descendant combinator.
	EXPECT_EQ(styles[(size_t)secondP].fontWeight, 700);
	EXPECT_EQ(styles[0].fontWeight, 400);
}

TEST(ui_specificity_and_important) {
	domTree tree = buildTree("<p class='x'>t</p>");
	stylesheet sheet = parseStylesheet(
		"p { color: #ff0000 } .x { color: #00ff00 } "
		"p { color: #0000ff !important }");
	styleContext ctx;
	vector<computedStyle> styles = resolveStyles(tree, sheet, ctx);
	EXPECT_NEAR(styles[0].textColor.b, 1.0, 0.01);
	EXPECT_NEAR(styles[0].textColor.r, 0.0, 0.01);
}

TEST(ui_inline_style_attribute_wins) {
	domTree tree = buildTree("<p style='color: #ff0000'>t</p>");
	stylesheet sheet = parseStylesheet("p { color: #00ff00 }");
	styleContext ctx;
	vector<computedStyle> styles = resolveStyles(tree, sheet, ctx);
	EXPECT_NEAR(styles[0].textColor.r, 1.0, 0.01);
}

TEST(ui_inheritance) {
	domTree tree = buildTree("<div style='color: #ff0000'><p>t</p></div>");
	stylesheet sheet = parseStylesheet("p { font-weight: 700 }");
	styleContext ctx;
	vector<computedStyle> styles = resolveStyles(tree, sheet, ctx);
	EXPECT_NEAR(styles[1].textColor.r, 1.0, 0.01);
	// `font-weight` set on the <p> does not leak back to the parent.
	EXPECT_EQ(styles[0].fontWeight, 400);
	EXPECT_EQ(styles[1].fontWeight, 700);
}

TEST(ui_pseudo_class_from_node_state) {
	domTree tree = buildTree("<p class='btn'>t</p>");
	stylesheet sheet = parseStylesheet(
		".btn { color: #ff0000 } .btn:hover { color: #00ff00 }");
	styleContext ctx;
	tree.nodes[0].state.hover = true;
	vector<computedStyle> styles = resolveStyles(tree, sheet, ctx);
	EXPECT_NEAR(styles[0].textColor.g, 1.0, 0.01);
}

TEST(ui_keyframes_are_collected) {
	stylesheet sheet = parseStylesheet(
		"@keyframes blink { 0% { opacity: 1 } 100% { opacity: 0 } } "
		"p { color: red }");
	const keyframesBlock* block = sheet.findKeyframes("blink");
	EXPECT_TRUE(block != nullptr);
	if (!block) return;
	EXPECT_EQ(block->steps.size(), (size_t)2);
	EXPECT_NEAR(block->steps[0].offset, 0.0, 0.001);
	EXPECT_NEAR(block->steps[1].offset, 1.0, 0.001);
	EXPECT_EQ(sheet.rules.size(), (size_t)1);
}

TEST(ui_media_queries_match_viewport) {
	styleContext ctx;
	ctx.viewportWidth = 800.0f;
	ctx.viewportHeight = 600.0f;
	EXPECT_TRUE(mediaMatches("(min-width: 700px)", ctx));
	EXPECT_FALSE(mediaMatches("(min-width: 900px)", ctx));
	EXPECT_TRUE(mediaMatches("(max-width: 900px)", ctx));
	EXPECT_TRUE(mediaMatches("screen and (min-height: 500px)", ctx));
	EXPECT_FALSE(mediaMatches("print", ctx));

	domTree tree = buildTree("<p>t</p>");
	stylesheet sheet = parseStylesheet(
		"p { color: #ff0000 } "
		"@media (min-width: 700px) { p { color: #00ff00 } }");
	vector<computedStyle> styles = resolveStyles(tree, sheet, ctx);
	EXPECT_NEAR(styles[0].textColor.g, 1.0, 0.01);
}

TEST(ui_selector_nth_child_variants) {
	domTree tree = buildTree(
		"<ul><li>1</li><li>2</li><li>3</li><li>4</li><li>5</li></ul>");
	// Node indices: ul, li, t, li, t, ... every li at 1, 3, 5, 7, 9.
	stylesheet odd = parseStylesheet("li:nth-child(odd) { color: #ff0000 }");
	stylesheet even = parseStylesheet("li:nth-child(even) { color: #00ff00 }");
	stylesheet third = parseStylesheet("li:nth-child(3) { color: #0000ff }");
	stylesheet lastTwo = parseStylesheet("li:nth-last-child(-n+2) { color: #ff00ff }");
	styleContext ctx;

	auto colors = resolveStyles(tree, odd, ctx);
	EXPECT_NEAR(colors[1].textColor.r, 1.0, 0.01);   // li 1
	EXPECT_NEAR(colors[3].textColor.r, 0.0, 0.01);   // li 2
	EXPECT_NEAR(colors[5].textColor.r, 1.0, 0.01);   // li 3

	colors = resolveStyles(tree, even, ctx);
	EXPECT_NEAR(colors[3].textColor.g, 1.0, 0.01);
	EXPECT_NEAR(colors[7].textColor.g, 1.0, 0.01);
	EXPECT_NEAR(colors[1].textColor.g, 0.0, 0.01);

	colors = resolveStyles(tree, third, ctx);
	EXPECT_NEAR(colors[5].textColor.b, 1.0, 0.01);
	EXPECT_NEAR(colors[1].textColor.b, 0.0, 0.01);

	// The last two lis are indexes 4 and 5.
	colors = resolveStyles(tree, lastTwo, ctx);
	EXPECT_NEAR(colors[7].textColor.r, 1.0, 0.01);
	EXPECT_NEAR(colors[9].textColor.r, 1.0, 0.01);
	EXPECT_NEAR(colors[5].textColor.r, 0.0, 0.01);
}

TEST(ui_selector_of_type_matches_sibling_position) {
	domTree tree = buildTree(compact(
		"<div><span>a</span><p>b</p><span>c</span><span>d</span>"
		"<b>bold</b></div>"));
	// Node indices: div 0, span 1, text 2, p 3, text 4, span 5, text 6,
	// span 7, text 8, b 9, text 10.
	styleContext ctx;

	// The second span is node 5, the third is node 7.
	stylesheet sheet = parseStylesheet("span:nth-of-type(2) { color: #ff0000 }");
	auto colors = resolveStyles(tree, sheet, ctx);
	EXPECT_NEAR(colors[5].textColor.r, 1.0, 0.01);
	EXPECT_NEAR(colors[1].textColor.r, 0.0, 0.01);
	EXPECT_NEAR(colors[7].textColor.r, 0.0, 0.01);

	// The paragraph is the first <p> among its siblings.
	stylesheet firstP = parseStylesheet("p:first-of-type { color: #00ff00 }");
	colors = resolveStyles(tree, firstP, ctx);
	EXPECT_NEAR(colors[3].textColor.g, 1.0, 0.01);

	// The last span is node 7; the first one is not.
	stylesheet lastSpan = parseStylesheet("span:last-of-type { color: #0000ff }");
	colors = resolveStyles(tree, lastSpan, ctx);
	EXPECT_NEAR(colors[7].textColor.b, 1.0, 0.01);
	EXPECT_NEAR(colors[1].textColor.b, 0.0, 0.01);

	// The only <b>.
	stylesheet onlyB = parseStylesheet("b:only-of-type { color: #ff00ff }");
	colors = resolveStyles(tree, onlyB, ctx);
	EXPECT_NEAR(colors[9].textColor.r, 1.0, 0.01);
	EXPECT_NEAR(colors[9].textColor.b, 1.0, 0.01);
}

TEST(ui_selector_sibling_combinators) {
	domTree tree = buildTree(compact(
		"<div><p class='a'>1</p><p class='b'>2</p><span>m</span>"
		"<p class='c'>3</p></div>"));
	// Node indices: div 0, p.a 1, t 2, p.b 3, t 4, span 5, t 6, p.c 7,
	// t 8.
	styleContext ctx;

	// `+` matches the immediately following element.
	stylesheet sheet = parseStylesheet("p.a + p { color: #ff0000 }");
	auto colors = resolveStyles(tree, sheet, ctx);
	EXPECT_NEAR(colors[3].textColor.r, 1.0, 0.01);
	EXPECT_NEAR(colors[7].textColor.r, 0.0, 0.01);

	// `~` matches any following element.
	stylesheet any = parseStylesheet("p.a ~ p { color: #00ff00 }");
	colors = resolveStyles(tree, any, ctx);
	EXPECT_NEAR(colors[3].textColor.g, 1.0, 0.01);
	EXPECT_NEAR(colors[7].textColor.g, 1.0, 0.01);

	// The span sits between p.b and p.c, so `p.b + p` reaches nothing:
	// the adjacency is positional and p.c's previous sibling is the span.
	stylesheet afterSpan = parseStylesheet("p.b + p { color: #0000ff }");
	colors = resolveStyles(tree, afterSpan, ctx);
	EXPECT_NEAR(colors[7].textColor.b, 0.0, 0.01);
	EXPECT_NEAR(colors[3].textColor.b, 0.0, 0.01);
}

TEST(ui_renderer_registry_selects_software) {
	// The built-in software rasterizer self-registers; unknown names are
	// a plugin-load attempt then a miss (registry semantics, not a chain).
	auto* ui = UI::createRenderer("software");
	EXPECT_TRUE(ui != nullptr);
	if (ui) {
		EXPECT_EQ(string(ui->name()), string("software"));
		delete ui;
	}

	// Chains pick the first available, falling through absent plugins to
	// the registered built-in.
	auto* chained = UI::createRenderer(
		list({var(string("no_such_renderer")), var(string("software"))}));
	EXPECT_TRUE(chained != nullptr);
	if (chained) {
		EXPECT_EQ(string(chained->name()), string("software"));
		delete chained;
	}

	// An unknown-only chain is a miss, with the loader able to explain it.
	EXPECT_TRUE(UI::createRenderer("no_renderer_anywhere") == nullptr);
}

TEST(ui_selector_attribute_operators) {
	domTree tree = buildTree(compact(
		"<a role='btn primary' lang='en-US' href='/docs/page.html'>x</a>"
		"<a role='other'>y</a>"));
	styleContext ctx;
	auto matches = [&](const string& selector) {
		const auto sels = parseSelectorList(selector);
		for (const auto& sel : sels)
			if (selectorMatches(sel, tree, 0)) return true;
		return false;
	};
	EXPECT_TRUE(matches("a[role~='primary']"));
	EXPECT_FALSE(matches("a[role~='prim']"));
	EXPECT_TRUE(matches("a[lang|='en']"));
	EXPECT_TRUE(matches("a[href^='/docs']"));
	EXPECT_TRUE(matches("a[href$='.html']"));
	EXPECT_TRUE(matches("a[href*='page']"));
	EXPECT_TRUE(matches("[role]"));
	EXPECT_FALSE(matches("[title]"));
}

TEST(ui_selector_not_root_empty) {
	domTree tree = buildTree("<div class='on'><p>t</p></div><div></div>");
	// Node indices: div.on(0), p(1), text t(2), empty div(3).
	stylesheet sheet = parseStylesheet(
		"div:not(.on) { color: #ff0000 } div:empty { font-weight: 700 }");
	styleContext ctx;
	auto colors = resolveStyles(tree, sheet, ctx);
	// :not excludes .on...
	EXPECT_NEAR(colors[0].textColor.r, 0.0, 0.01);
	EXPECT_EQ(colors[0].fontWeight, 400);
	// ...and the empty div matches both rules.
	EXPECT_NEAR(colors[3].textColor.r, 1.0, 0.01);
	EXPECT_EQ(colors[3].fontWeight, 700);
}

TEST(ui_important_numeric_declaration_wins) {
	domTree tree = buildTree("<div>t</div>");
	// `!important` on a number-valued declaration (stored typed by the CSS
	// parser) must still outrank the later normal rule.
	stylesheet sheet = parseStylesheet(
		"div { margin: 0 !important } div { margin: 5px }");
	styleContext ctx;
	auto styles = resolveStyles(tree, sheet, ctx);
	EXPECT_NEAR(styles[0].margin[0].value, 0.0, 0.01);
	EXPECT_EQ(styles[0].margin[0].u, unit::px);

	stylesheet zIndex = parseStylesheet(
		"div { z-index: 7 } div { z-index: 2 !important }");
	auto zIndexed = resolveStyles(tree, zIndex, ctx);
	EXPECT_EQ(zIndexed[0].zIndex, 2);

	// The flag also works inline.
	stylesheet inlineStyle = parseStylesheet("div { margin: 9px }");
	tree.nodes[0].attr.setString("style", "margin: 0 !important");
	auto inlined = resolveStyles(tree, inlineStyle, ctx);
	EXPECT_NEAR(inlined[0].margin[0].value, 0.0, 0.01);
	EXPECT_EQ(inlined[0].margin[0].u, unit::px);
}

TEST(ui_parse_stylesheet_at_rules) {
	stylesheet sheet = parseStylesheet(
		"@import url(theme.css); "
		"@font-face { font-family: Custom; src: url(c.ttf) } "
		"p { color: #ff0000 } "
		"@media (min-width: 700px) { "
		"  @media (min-height: 400px) { p { color: #00ff00 } } "
		"} "
		"@keyframes slide { from { opacity: 0 } to { opacity: 1 } }");

	// @import ignored; the base rule plus the (nested) media rule are both
	// collected, with the media query combined via `and`.
	EXPECT_EQ(sheet.rules.size(), (size_t)2);
	EXPECT_EQ(sheet.rules[1].media,
		std::string("(min-width: 700px) and (min-height: 400px)"));
	EXPECT_EQ(sheet.fontFaces.size(), (size_t)1);
	const keyframesBlock* block = sheet.findKeyframes("slide");
	EXPECT_TRUE(block != nullptr);
	if (block) EXPECT_EQ(block->steps.size(), (size_t)2);

	styleContext small;
	small.viewportWidth = 600.0f;
	small.viewportHeight = 300.0f;
	domTree tree = buildTree("<p>t</p>");
	auto styles = resolveStyles(tree, sheet, small);
	EXPECT_NEAR(styles[0].textColor.r, 1.0, 0.01);

	styleContext big;
	big.viewportWidth = 800.0f;
	big.viewportHeight = 600.0f;
	styles = resolveStyles(tree, sheet, big);
	EXPECT_NEAR(styles[0].textColor.g, 1.0, 0.01);
}

// ------------------------------------------------------------------ builder

TEST(ui_set_html_accepts_builder_elements) {
	software_renderer ui;
	ui.setViewport(list({60.0, 30.0}));
	// Built with the element ctors; nested items via an "items" bundle.
	auto panel = HTML::div(
		list({jo("style",
				 "width: 60px; height: 30px; background: #ff0000",
				 "items", ja(HTML::span(list({"hi"}))))}));
	ui.setHTML(list({var(panel)}));
	ui.setStyleSheet("");
	EXPECT_TRUE(ui.dom().nodes.size() >= 2);
	EXPECT_TRUE(ui.paint());
	EXPECT_TRUE(pixelNear(ui.target(), 30, 15, 255, 0, 0));
	// markup() round-trips the built element.
	EXPECT_EQ(compact(ui.markup()),
		std::string("<div style=\"width: 60px; height: 30px; "
			"background: #ff0000\"><span>hi</span></div>"));
}

TEST(ui_set_css_accepts_rule_list) {
	software_renderer ui;
	ui.setViewport(list({40.0, 20.0}));
	ui.setMarkup("<div class='box'>x</div>");
	// Data-driven stylesheet: CSS::parseCSS output, or a hand-built list.
	list rules({var(CSS::Rule(".box",
		jo("width", "40px", "height", "20px", "background", "#00ff00")))});
	ui.setCSS(list({var(rules)}));
	EXPECT_TRUE(ui.paint());
	EXPECT_TRUE(pixelNear(ui.target(), 20, 10, 0, 255, 0));
}

TEST(ui_markup_escapes_text) {
	software_renderer ui; setupUI(ui, "<p>1 &lt; 2 &amp; 3</p>", "");
	// setupUI already painted: the entity text round-trips.
	EXPECT_EQ(ui.markup(), std::string("<p>1 &lt; 2 &amp; 3</p>"));
	// The text nodes hold the decoded characters.
	EXPECT_EQ(ui.dom().nodes[1].text, std::string("1 < 2 & 3"));
}

TEST(ui_top_level_text_reaches_the_renderer) {
	software_renderer ui; setupUI(ui, "hello world", "", 80, 20);
	uint64_t lit = 0;
	const rasterTarget& target = ui.target();
	for (uint32_t i = 3; i < target.pixels.size(); i += 4)
		if (target.pixels[i] > 40) lit++;
	EXPECT_TRUE(lit > 4);
}

// ------------------------------------------------------------------ layout

TEST(ui_layout_block_flow_positions_siblings) {
	software_renderer ui; setupUI(ui, "<div id='a'>A</div><div id='b'>B</div>",
		"div { height: 20px }");
	object a = ui.hit(list({5.0, 5.0}));
	object b = ui.hit(list({5.0, 30.0}));
	EXPECT_EQ(a.getString("cssId"), std::string("a"));
	EXPECT_EQ(b.getString("cssId"), std::string("b"));
	EXPECT_NEAR(rectOf(a).getDouble("y"), 0.0, 0.5);
	EXPECT_NEAR(rectOf(b).getDouble("y"), 20.0, 0.5);
}

TEST(ui_layout_box_model_widths) {
	software_renderer ui; setupUI(ui, "<div id='a'>A</div>",
		"div { width: 100px; height: 40px; padding: 5px; "
		"border: 2px solid #000; margin: 10px }");
	object rect = rectOf(ui.hit(list({50.0, 30.0})));
	EXPECT_NEAR(rect.getDouble("width"), 114.0, 0.5);   // 100 + 2*5 + 2*2
	EXPECT_NEAR(rect.getDouble("height"), 54.0, 0.5);
	EXPECT_NEAR(rect.getDouble("x"), 10.0, 0.5);
	EXPECT_NEAR(rect.getDouble("y"), 10.0, 0.5);
}

TEST(ui_layout_border_box_sizing) {
	software_renderer ui; setupUI(ui, "<div id='a'>A</div>",
		"div { box-sizing: border-box; width: 100px; height: 40px; "
		"padding: 5px; border: 2px solid #000 }");
	object rect = rectOf(ui.hit(list({50.0, 20.0})));
	EXPECT_NEAR(rect.getDouble("width"), 100.0, 0.5);
	EXPECT_NEAR(rect.getDouble("height"), 40.0, 0.5);
}

TEST(ui_layout_percentage_width) {
	software_renderer ui; setupUI(ui, "<div id='a'>A</div>", "div { width: 50%; }");
	object rect = rectOf(ui.hit(list({50.0, 5.0})));
	EXPECT_NEAR(rect.getDouble("width"), 100.0, 0.5);
}

TEST(ui_layout_display_none_hides_box) {
	software_renderer ui; setupUI(ui, "<div id='a'>A</div><div id='b'>B</div>",
		"#a { display: none } #b { height: 10px }");
	// The hidden box must not be hit, and the visible one moves up to y=0.
	EXPECT_EQ(ui.hit(list({5.0, 5.0})).getString("cssId"), std::string("b"));
}

TEST(ui_layout_flex_row_distributes_space) {
	software_renderer ui; setupUI(ui, "<div id='row'><div id='l'>L</div>"
						 "<div id='r'>R</div></div>",
		"#row { display: flex; width: 200px; height: 20px } "
		"#l, #r { flex-grow: 1 }");
	object l = ui.hit(list({25.0, 10.0}));
	object r = ui.hit(list({175.0, 10.0}));
	EXPECT_EQ(l.getString("cssId"), std::string("l"));
	EXPECT_EQ(r.getString("cssId"), std::string("r"));
	EXPECT_NEAR(rectOf(l).getDouble("width"), 100.0, 1.0);
}

TEST(ui_layout_flex_justify_and_gap) {
	software_renderer ui; setupUI(ui, "<div id='row'><div id='l'>L</div>"
						 "<div id='r'>R</div></div>",
		"#row { display: flex; width: 200px; height: 20px; gap: 10px; "
		"justify-content: center } "
		"#l, #r { width: 40px; flex-grow: 0 }");
	object l = ui.hit(list({70.0, 10.0}));
	EXPECT_EQ(l.getString("cssId"), std::string("l"));
	// 40 + 10 + 40 = 90, centered in 200 -> starts at 55.
	EXPECT_NEAR(rectOf(l).getDouble("x"), 55.0, 1.0);
}

TEST(ui_layout_grid_places_items_in_cells) {
	software_renderer ui; setupUI(ui, "<div id='g'><div id='a'>A</div>"
						 "<div id='b'>B</div></div>",
		"#g { display: grid; grid-template-columns: 100px 100px; "
		"height: 40px } #a, #b { height: 40px }");
	object a = ui.hit(list({50.0, 20.0}));
	object b = ui.hit(list({150.0, 20.0}));
	EXPECT_EQ(a.getString("cssId"), std::string("a"));
	EXPECT_EQ(b.getString("cssId"), std::string("b"));
	EXPECT_NEAR(rectOf(b).getDouble("x"), 100.0, 1.0);
}

TEST(ui_layout_text_wraps_to_lines) {
	software_renderer ui; setupUI(ui, "<div id='p'>one two three four five</div>",
		"div { width: 60px; font-size: 8px }");
	const layoutResult& result = ui.boxes();
	const layoutBox& box = result.boxes[0];
	EXPECT_TRUE(box.lines.size() >= 2);
	EXPECT_TRUE(box.runs.size() >= 2);
	// Every run must stay inside the content box.
	for (const textRun& run : box.runs)
		EXPECT_TRUE(run.x >= box.content.x - 0.5f);
	EXPECT_TRUE(box.content.h > 0.0f);
}

TEST(ui_layout_absolute_positioning) {
	software_renderer ui; setupUI(ui, "<div id='c'><div id='a'>A</div></div>",
		"#c { position: relative; width: 200px; height: 200px } "
		"#a { position: absolute; left: 30px; top: 40px; width: 20px; "
		"height: 20px }");
	object a = ui.hit(list({40.0, 50.0}));
	EXPECT_EQ(a.getString("cssId"), std::string("a"));
	object rect = ui.elementRect(list({(double)a.getInt64("id")}));
	EXPECT_NEAR(rect.getDouble("x"), 30.0, 0.5);
	EXPECT_NEAR(rect.getDouble("y"), 40.0, 0.5);
}

TEST(ui_layout_relative_offset_moves_box) {
	software_renderer ui; setupUI(ui, "<div id='c'><div id='a'>A</div></div>",
		"#c { width: 200px; height: 200px } "
		"#a { position: relative; left: 15px; top: 5px; width: 20px; "
		"height: 20px }");
	object rect = ui.elementRect(list({(double)ui.hit(list({25.0, 15.0}))
										   .getObject().getInt64("id")}));
	EXPECT_NEAR(rect.getDouble("x"), 15.0, 0.5);
	EXPECT_NEAR(rect.getDouble("y"), 5.0, 0.5);
}

// ------------------------------------------------------------- rasterizer

TEST(ui_paint_fills_background_color) {
	software_renderer ui; setupUI(ui, "<div style='width: 40px; height: 20px; "
						 "background: #ff0000'></div>");
	EXPECT_TRUE(pixelNear(ui.target(), 20, 10, 255, 0, 0));
	EXPECT_TRUE(alphaAt(ui.target(), 20, 10) > 200);
}

TEST(ui_paint_antialiases_rounded_corners) {
	software_renderer ui; setupUI(ui, "<div style='width: 40px; height: 40px; "
						 "background: #ff0000; border-radius: 10px'></div>");
	// The very corner is outside the rounded shape...
	EXPECT_TRUE(alphaAt(ui.target(), 0, 0) < 40);
	// ...while the middle of an edge is fully covered.
	EXPECT_TRUE(alphaAt(ui.target(), 20, 1) > 200);
	EXPECT_TRUE(alphaAt(ui.target(), 20, 20) > 200);
}

TEST(ui_paint_border_sides_have_independent_colors) {
	software_renderer ui; setupUI(ui, "<div style='width: 40px; height: 40px; "
						 "border-top: 4px solid #ff0000; "
						 "border-bottom: 4px solid #0000ff'></div>");
	// Content-box sizing: the border box is 40 x (40 + 4 + 4).
	EXPECT_TRUE(pixelNear(ui.target(), 20, 1, 255, 0, 0));
	EXPECT_TRUE(pixelNear(ui.target(), 20, 45, 0, 0, 255));
	// The middle is content, not border.
	EXPECT_TRUE(alphaAt(ui.target(), 20, 22) < 20);
}

TEST(ui_paint_gradient_varies_across_the_box) {
	software_renderer ui; setupUI(ui, "<div style='width: 40px; height: 10px; "
						 "background: linear-gradient(90deg, #000000, "
						 "#ffffff)'></div>");
	const int left = (int)ui.target().pixels[(10 * 40 + 5) * 4];
	const int right = (int)ui.target().pixels[(10 * 40 + 34) * 4];
	EXPECT_TRUE(right > left + 60);
}

TEST(ui_paint_text_marks_pixels) {
	software_renderer ui; setupUI(ui, "<div style='color: #ffffff; font-size: 16px'>III</div>",
		"", 100, 40);
	uint64_t lit = 0;
	for (uint32_t i = 3; i < ui.target().pixels.size(); i += 4)
		if (ui.target().pixels[i] > 40) lit++;
	EXPECT_TRUE(lit > 6);
}

TEST(ui_paint_respects_overflow_clip) {
	software_renderer ui; setupUI(ui, 
		"<div style='width: 20px; height: 10px; overflow: hidden'>"
		"<span style='display: block; width: 200px; height: 50px; "
		"background: #ff0000'></span></div>",
		"", 100, 60);
	// Inside the clip: painted.
	EXPECT_TRUE(alphaAt(ui.target(), 10, 5) > 200);
	// Outside: the child must not bleed out.
	EXPECT_TRUE(alphaAt(ui.target(), 30, 5) < 20);
}

TEST(ui_paint_visibility_hidden_is_not_drawn) {
	software_renderer ui; setupUI(ui, "<div style='width: 40px; height: 20px; "
						 "background: #ff0000; visibility: hidden'></div>");
	EXPECT_TRUE(alphaAt(ui.target(), 20, 10) < 20);
}

TEST(ui_paint_opacity_fades_pixels) {
	software_renderer ui; setupUI(ui, "<div style='width: 40px; height: 20px; "
						 "background: #ff0000; opacity: 0.25'></div>");
	const int a = alphaAt(ui.target(), 20, 10);
	EXPECT_TRUE(a > 30);
	EXPECT_TRUE(a < 120);
}

// -------------------------------------------------- dirty tracking / frames

TEST(ui_only_repaints_when_something_changed) {
	software_renderer ui;
	ui.setViewport(list({200.0, 100.0}));
	ui.setMarkup("<div style='height: 10px'></div>");
	ui.setStyleSheet("");
	EXPECT_TRUE(ui.dirty());
	EXPECT_TRUE(ui.paint());
	// A static UI costs nothing per frame.
	EXPECT_FALSE(ui.dirty());
	EXPECT_FALSE(ui.paint());
	EXPECT_EQ(ui.stats().stylePasses, (uint64_t)1);
	EXPECT_EQ(ui.stats().layoutPasses, (uint64_t)1);
	EXPECT_EQ(ui.stats().paints, (uint64_t)1);
}

TEST(ui_viewport_change_relayouts) {
	software_renderer ui;
	ui.setViewport(list({200.0, 100.0}));
	ui.setMarkup("<div style='width: 50%'></div>");
	ui.setStyleSheet("");
	ui.paint();
	EXPECT_NEAR(ui.elementRect(list({1.0})).getObject().getDouble("width"), 100.0, 0.5);
	ui.setViewport(list({400.0, 100.0}));
	ui.paint();
	EXPECT_NEAR(ui.elementRect(list({1.0})).getObject().getDouble("width"), 200.0, 0.5);
}

TEST(ui_set_text_marks_dirty_and_replaces_content) {
	software_renderer ui; setupUI(ui, "<div id='a'>i</div>", "div { width: 80px }");
	const int64_t id = ui.hit(list({5.0, 5.0})).getObject().getInt64("id");
	ui.paint();
	EXPECT_FALSE(ui.dirty());
	ui.setText(list({(double)id, "much longer text"}));
	EXPECT_TRUE(ui.dirty());
	ui.paint();
	// The new text produced more runs than the single character did.
	EXPECT_TRUE(ui.boxes().boxes[0].runs.size() >= 2);
	EXPECT_EQ(ui.query(list({"#a"})).getList().getObject(0).getString("text"),
		std::string("much longer text"));
}

TEST(ui_set_style_overrides) {
	software_renderer ui; setupUI(ui, "<div id='a'>A</div>", "div { height: 10px }");
	const int64_t id = ui.hit(list({5.0, 5.0})).getObject().getInt64("id");
	ui.setStyle(list({(double)id, jo("height", 40.0)}));
	ui.paint();
	EXPECT_NEAR(ui.elementRect(list({(double)id})).getObject().getDouble("height"), 40.0,
		0.5);
}

TEST(ui_external_dom_mutation_repaints) {
	software_renderer ui;
	ui.setViewport(list({100.0, 60.0}));
	ui.setMarkup("<div style='width: 100px; height: 60px; "
				 "background: #ff0000'>t</div>");
	ui.setStyleSheet("");
	ui.paint();
	EXPECT_TRUE(pixelNear(ui.target(), 50, 30, 255, 0, 0));
	EXPECT_FALSE(ui.dirty());

	// Mutate the shared parsed element behind the renderer's back: object
	// copies share storage, so this write reaches the renderer's tree.
	object el = ui.dom().nodes[0].el;
	el.getObject("attr").setString("style",
		"width: 100px; height: 60px; background: #0000ff");

	// An otherwise quiet surface still notices and repaints.
	EXPECT_TRUE(ui.dirty());
	EXPECT_TRUE(ui.paint());
	EXPECT_TRUE(pixelNear(ui.target(), 50, 30, 0, 0, 255));
}

// ------------------------------------------------------------ interaction

TEST(ui_hit_test_finds_deepest_element) {
	software_renderer ui; setupUI(ui, 
		"<div id='outer' style='width: 100px; height: 100px'>"
		"<div id='inner' style='width: 20px; height: 20px'></div></div>");
	object hit = ui.hit(list({5.0, 5.0}));
	EXPECT_EQ(hit.getString("cssId"), std::string("inner"));
	object miss = ui.hit(list({50.0, 50.0}));
	EXPECT_EQ(miss.getString("cssId"), std::string("outer"));
	// Outside everything.
	EXPECT_TRUE(ui.hit(list({500.0, 500.0})).isEmpty());
}

TEST(ui_pointer_events_none_is_skipped) {
	software_renderer ui; setupUI(ui, 
		"<div id='outer' style='width: 100px; height: 100px'>"
		"<div id='inner' style='width: 20px; height: 20px; "
		"pointer-events: none'></div></div>");
	EXPECT_EQ(ui.hit(list({5.0, 5.0})).getString("cssId"),
		std::string("outer"));
}

TEST(ui_click_handler_fires_with_element_data) {
	software_renderer ui; setupUI(ui, "<div id='btn' style='width: 50px; height: 20px'>"
						 "OK</div>");
	int64_t captured = -1;
	string capturedTag;
	ui.on(ja("click", func([&](list args) {
		 captured = args[0].getObject().getInt64("id");
		 capturedTag = args[0].getObject().getString("tag");
		 return var(42);
	 })));
	ui.dispatch(list({"click", 10.0, 5.0}));
	EXPECT_EQ(capturedTag, std::string("div"));
	EXPECT_TRUE(captured > 0);
}

TEST(ui_click_handler_bubbles_to_ancestors) {
	software_renderer ui; setupUI(ui, 
		"<div id='panel' style='width: 100px; height: 100px'>"
		"<button id='btn' style='width: 20px; height: 20px'>x</button>"
		"</div>");
	vector<string> seen;
	ui.on(ja("click", func([&](list args) {
		 seen.push_back(args[0].getObject().getString("cssId"));
		 return var();
	 })));
	ui.dispatch(list({"click", 5.0, 5.0}));
	EXPECT_EQ(seen.size(), (size_t)1);
	if (!seen.empty()) EXPECT_EQ(seen[0], std::string("btn"));
}

TEST(ui_element_scoped_handler_only_fires_for_that_element) {
	software_renderer ui; setupUI(ui, 
		"<div id='a' style='width: 20px; height: 20px'></div>"
		"<div id='b' style='width: 20px; height: 20px'></div>");
	int aClicks = 0, bClicks = 0;
	const int64_t idA = ui.hit(list({5.0, 5.0})).getObject().getInt64("id");
	const int64_t idB = ui.hit(list({5.0, 30.0})).getObject().getInt64("id");
	ui.on(ja("click", (int64_t)idA, func([&](list) {
		 aClicks++;
		 return var();
	 })));
	ui.on(ja("click", (int64_t)idB, func([&](list) {
		 bClicks++;
		 return var();
	 })));
	ui.dispatch(list({"click", 5.0, 5.0}));
	ui.dispatch(list({"click", 5.0, 30.0}));
	EXPECT_EQ(aClicks, 1);
	EXPECT_EQ(bClicks, 1);
}

TEST(ui_hover_state_changes_style_and_dirties) {
	// The background lives in the stylesheet, not inline: an inline
	// declaration would outrank any `:hover` rule.
	software_renderer ui; setupUI(ui, "<div id='btn'>hover me</div>",
		"#btn { width: 50px; height: 20px; background: #ff0000 } "
		"#btn:hover { background: #0000ff }");
	EXPECT_TRUE(pixelNear(ui.target(), 25, 10, 255, 0, 0));

	ui.dispatch(list({"move", 25.0, 10.0}));
	EXPECT_TRUE(ui.dirty());
	ui.paint();
	EXPECT_TRUE(pixelNear(ui.target(), 25, 10, 0, 0, 255));

	ui.dispatch(list({"move", 500.0, 500.0}));
	ui.paint();
	EXPECT_TRUE(pixelNear(ui.target(), 25, 10, 255, 0, 0));
}

TEST(ui_press_then_release_outside_cancels_click) {
	software_renderer ui; setupUI(ui, "<div id='btn' style='width: 50px; "
						 "height: 20px'></div>");
	int clicks = 0;
	ui.on(ja("click", func([&](list) {
		 clicks++;
		 return var();
	 })));
	ui.dispatch(list({"down", 10.0, 5.0}));
	ui.dispatch(list({"up", 500.0, 500.0}));
	EXPECT_EQ(clicks, 0);
	ui.dispatch(list({"down", 10.0, 5.0}));
	ui.dispatch(list({"up", 10.0, 5.0}));
	EXPECT_EQ(clicks, 1);
}

TEST(ui_game_can_drive_hover_state_directly) {
	software_renderer ui; setupUI(ui, "<div id='btn'>B</div>",
		"#btn { width: 50px; height: 20px; background: #ff0000 } "
		"#btn:hover { background: #00ff00 }");
	const int64_t id = ui.hit(list({5.0, 5.0})).getObject().getInt64("id");
	ui.paint();
	ui.setState(list({(double)id, "hover", true}));
	ui.paint();
	EXPECT_TRUE(pixelNear(ui.target(), 25, 10, 0, 255, 0));
	object state = ui.interactionState(list());
	EXPECT_EQ(state.getInt64("hover"), id);
}

TEST(ui_query_selects_elements) {
	software_renderer ui; setupUI(ui, 
		"<ul><li class='item'>a</li><li class='item'>b</li></ul>");
	list found = ui.query(list({"li.item"}));
	EXPECT_EQ(found.size(), (uint64_t)2);
	list byId = ui.query(list({"#nope"}));
	EXPECT_EQ(byId.size(), (uint64_t)0);
}

TEST(ui_element_descriptor_reports_geometry) {
	software_renderer ui; setupUI(ui, "<div id='box' style='width: 30px; height: 10px'>"
						 "text</div>");
	list found = ui.query(list({"#box"}));
	EXPECT_EQ(found.size(), (uint64_t)1);
	if (found.size() == 0) return;
	object element = found.getObject(0);
	EXPECT_EQ(element.getString("tag"), std::string("div"));
	EXPECT_EQ(element.getString("cssId"), std::string("box"));
	EXPECT_NEAR(element.getObject("rect").getDouble("width"), 30.0, 0.5);
	EXPECT_EQ(element.getString("text"), std::string("text"));
}

// ------------------------------------------------------------------- fonts

TEST(ui_font_builtin_metrics_are_deterministic) {
	fontManager fonts;
	EXPECT_FALSE(fonts.hasFamily("sans-serif"));
	const float one = fonts.measureWidth("iiii", "sans-serif", 14.0f);
	const float two = fonts.measureWidth("iiii", "sans-serif", 28.0f);
	EXPECT_TRUE(one > 0.0f);
	// Doubling the size doubles the advance.
	EXPECT_NEAR(two, one * 2.0, 0.01);
	// A wider string measures wider.
	EXPECT_TRUE(fonts.measureWidth("wwww", "sans-serif", 14.0f) >=
		fonts.measureWidth("iiii", "sans-serif", 14.0f));
	fontMetrics metrics = fonts.metrics("sans-serif", 14.0f);
	EXPECT_NEAR(metrics.ascent, 14.0, 0.01);
}

TEST(ui_font_glyph_bitmap_is_cached) {
	fontManager fonts;
	const glyphBitmap* first = fonts.glyph('A', "sans-serif", 12.0f);
	EXPECT_TRUE(first != nullptr);
	if (!first) return;
	EXPECT_EQ(fonts.glyphCacheSize(), (size_t)1);
	const glyphBitmap* second = fonts.glyph('A', "sans-serif", 12.0f);
	EXPECT_TRUE(first == second);
	EXPECT_TRUE(first->width > 0);
	EXPECT_TRUE(first->height > 0);
}

TEST(ui_font_cache_eviction_returns_the_new_glyph) {
	fontManager fonts;
	// Push past the 8192-entry eviction with distinct tofu codepoints.
	// The old order (insert, then clear, then `return &cache[key]`) wiped
	// the insertion and handed back AND cached an empty bitmap.
	bool sawEmpty = false;
	for (uint32_t i = 0; i < 9000; ++i) {
		const auto* g = fonts.glyph(0x3000 + i, "sans-serif", 16.0f);
		if (!g || !g->valid()) {
			sawEmpty = true;
			break;
		}
	}
	EXPECT_FALSE(sawEmpty);
	EXPECT_TRUE(fonts.glyph('A', "sans-serif", 12.0f)->valid());
}

TEST(ui_font_utf8_round_trip) {
	vector<uint32_t> decoded = fontManager::decodeUTF8("a\xC3\xA9\xE2\x82\xAC");
	EXPECT_EQ(decoded.size(), (size_t)3);
	EXPECT_EQ(decoded[0], (uint32_t)'a');
	EXPECT_EQ(decoded[1], (uint32_t)0xE9);
	EXPECT_EQ(decoded[2], (uint32_t)0x20AC);
	EXPECT_EQ(fontManager::encodeUTF8(0x20AC), std::string("\xE2\x82\xAC"));
}

TEST(ui_font_real_metrics_are_sane) {
	// Uses a system-installed TTF when one exists. The ranges hold for any
	// working real-font path; the old double-scaled metrics read ~12x the
	// em size and fail these.
	string fontFile;
	if (std::filesystem::exists("/usr/share/fonts")) {
		for (const auto& entry :
			std::filesystem::recursive_directory_iterator(
				"/usr/share/fonts")) {
			if (entry.path().extension() == ".ttf") {
				fontFile = entry.path().string();
				break;
			}
		}
	}
	if (fontFile.empty()) return;  // no font on this system to test against

	fontManager fonts;
	EXPECT_TRUE(fonts.loadFile(fontFile, "sans-serif"));
	const float size = 16.0f;
	const fontMetrics fm = fonts.metrics("sans-serif", size);
	EXPECT_TRUE(fm.ascent > 0.0f);
	EXPECT_TRUE(fm.ascent < 1.5f * size);
	EXPECT_TRUE(fm.descent >= 0.0f && fm.descent < 1.0f * size);
	EXPECT_TRUE(fm.lineGap >= 0.0f && fm.lineGap < 0.5f * size);
	EXPECT_TRUE(fm.xHeight > 0.0f && fm.xHeight < fm.ascent + fm.descent);
	// A single glyph advance stays sub-em at this size.
	const float w = fonts.measureWidth("x", "sans-serif", size);
	EXPECT_TRUE(w > 0.0f && w < size);
	// And glyphs rasterize at this size too.
	EXPECT_TRUE(fonts.glyph('A', "sans-serif", size) != nullptr);
}

// -------------------------------------------------------------- end to end

TEST(ui_crt_panel_renders_and_reacts) {
	// A small in-world "screen": markup + CSS in, pixels out, clickable.
	software_renderer ui;
	ui.setViewport(list({128.0, 64.0}));
	ui.setMarkup(compact(
		"<div id='crt' class='screen'>"
		"  <div id='title'>SYSTEM READY</div>"
		"  <button id='go'>LAUNCH</button>"
		"</div>"));
	ui.setStyleSheet(
		".screen { width: 128px; height: 64px; background: #001018; "
		"padding: 4px }"
		"#title { color: #33ff66; font-size: 8px }"
		"#go { margin-top: 6px; padding: 4px; color: #ffffff; "
		"background: #004400; border: 1px solid #33ff66; width: 60px }"
		"#go:hover { background: #00aa00 }");

	EXPECT_TRUE(ui.paint());
	EXPECT_TRUE(alphaAt(ui.target(), 60, 30) > 100);
	// The panel background painted.
	EXPECT_TRUE(pixelNear(ui.target(), 2, 2, 0, 16, 24));

	// The launch button is a real hit target.
	object button = ui.hit(list({30.0, 30.0}));
	EXPECT_EQ(button.getString("cssId"), std::string("go"));
	int clicks = 0;
	ui.on(ja("click", func([&](list) {
		 clicks++;
		 return var();
	 })));
	ui.dispatch(list({"down", 30.0, 30.0}));
	ui.dispatch(list({"up", 30.0, 30.0}));
	EXPECT_EQ(clicks, 1);

	// The button hover style repaints the whole button, not one pixel.
	object buttonRect = rectOf(button);
	const rect pad{(float)(buttonRect.getDouble("x") + 2.0f),
		(float)(buttonRect.getDouble("y") + 2.0f), 6.0f, 6.0f};
	const int before =
		countPixelsNear(ui.target(), pad, 0, 170, 0);
	EXPECT_EQ(before, 0);
	ui.dispatch(list({"move", 30.0, 30.0}));
	ui.paint();
	EXPECT_TRUE(countPixelsNear(ui.target(), pad, 0, 170, 0) > 20);
}

TEST(ui_list_marker_is_painted) {
	software_renderer ui; setupUI(ui, "<ul style='width: 60px'>"
						 "<li>one</li><li>two</li></ul>",
		"ul { width: 60px } li { height: 12px }", 80, 60);
	// Markers sit left of the content; something must be drawn there.
	uint64_t litLeft = 0;
	for (int y = 0; y < 30; y++)
		for (int x = 0; x < 12; x++)
			if (alphaAt(ui.target(), x, y) > 40) litLeft++;
	EXPECT_TRUE(litLeft > 0);
}

int main() {
	// Unbuffered so a hang points at the test group that caused it.
	std::cout << std::unitbuf;
	return goldtest::runAll();
}
