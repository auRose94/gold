#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "tree.hpp"
#include "types.hpp"
#include "web/css.hpp"

namespace gold {
	namespace UI {

		using std::string;
		using std::vector;

		// ------------------------------------------------------------ values

		enum class unit : uint8_t {
			none,     // not specified
			px,
			em,
			rem,
			percent,
			autoLen,  // `auto`
			viewportW,
			viewportH,
			number,   // bare number (line-height, flex-grow, ...)
		};

		/** A CSS length: value + unit, with `auto` and `none` as states. */
		struct length {
			float value = 0.0f;
			unit u = unit::none;

			static length px(float v) { return {v, unit::px}; }
			static length pct(float v) { return {v, unit::percent}; }
			static length makeAuto() { return {0.0f, unit::autoLen}; }
			static length undef() { return {}; }
			/** Treat a percentage as zero (used where % has no basis). */
			length orZero() const {
				return u == unit::none ? length::px(0.0f) : *this;
			}

			bool defined() const { return u != unit::none; }
			bool isAuto() const { return u == unit::autoLen; }
			bool isPercent() const { return u == unit::percent; }
			bool isZero() const {
				return u == unit::px && value == 0.0f;
			}
		};

		/** Non-negative RGBA, 0..1 per channel, straight (not premultiplied). */
		struct color {
			float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
			bool isSet = false;

			static color rgb(float r, float g, float b, float a = 1.0f) {
				return {r, g, b, a, true};
			}
			static color black() { return {0, 0, 0, 1, true}; }
			static color none() { return {}; }
			bool transparent() const { return !isSet || a <= 0.0f; }
		};

		/** Background image description (gradients only in v1). */
		enum class paintKind : uint8_t { none, solid, linearGradient, radialGradient };

		enum class bgSizeType : uint8_t { autoSize, cover, contain, explicitSize };

		struct gradientStop {
			float offset = 0.0f;
			color c;
		};

		struct backgroundPaint {
			paintKind kind = paintKind::none;
			color solid;
			vector<gradientStop> stops;
			// Linear: direction in degrees (CSS convention, 0 = to top).
			float angle = 180.0f;
			// Repeating variants.
			bool repeat = false;
		};

		// ---------------------------------------------------------- enums

		enum class displayType : uint8_t {
			none,
			block,
			inlineType,
			inlineBlock,
			flex,
			inlineFlex,
			grid,
			inlineGrid,
		};
		enum class positionType : uint8_t { staticPos, relative, absolute };
		enum class boxSizingType : uint8_t { contentBox, borderBox };
		enum class overflowType : uint8_t { visible, hidden, scroll, auto_ };
		enum class borderStyleType : uint8_t { none, solid, dashed, dotted, double_ };
		enum class flexDirectionType : uint8_t {
			row,
			rowReverse,
			column,
			columnReverse
		};
		enum class flexWrapType : uint8_t { noWrap, wrap, wrapReverse };
		enum class justifyType : uint8_t {
			start,
			end,
			center,
			spaceBetween,
			spaceAround,
			spaceEvenly,
		};
		enum class alignType : uint8_t {
			auto_,
			start,
			end,
			center,
			baseline,
			stretch,
		};
		enum class gridAutoFlowType : uint8_t { row, column, dense };
		enum class textAlignType : uint8_t { left, right, center, justify };
		enum class whiteSpaceType : uint8_t { normal, pre, nowrap, preWrap };
		enum class visibilityType : uint8_t { visible, hidden };
		enum class listStyleType : uint8_t { none_, disc, circle, square, decimal };

		// -------------------------------------------------- property ids

		// X-macro: the enum, the name table and the lookup stay in sync.
#define UI_PROPERTIES(X)                                                  \
	X(display, "display")                                                  \
	X(position, "position")                                                \
	X(top, "top")                                                          \
	X(right, "right")                                                      \
	X(bottom, "bottom")                                                    \
	X(left, "left")                                                        \
	X(zIndex, "z-index")                                                   \
	X(width, "width")                                                      \
	X(height, "height")                                                    \
	X(minWidth, "min-width")                                               \
	X(minHeight, "min-height")                                             \
	X(maxWidth, "max-width")                                               \
	X(maxHeight, "max-height")                                             \
	X(boxSizing, "box-sizing")                                            \
	X(overflow, "overflow")                                                \
	X(visibility, "visibility")                                            \
	X(opacity, "opacity")                                                  \
	X(margin, "margin")                                                    \
	X(marginTop, "margin-top")                                             \
	X(marginRight, "margin-right")                                         \
	X(marginBottom, "margin-bottom")                                       \
	X(marginLeft, "margin-left")                                           \
	X(padding, "padding")                                                  \
	X(paddingTop, "padding-top")                                           \
	X(paddingRight, "padding-right")                                       \
	X(paddingBottom, "padding-bottom")                                     \
	X(paddingLeft, "padding-left")                                         \
	X(borderWidth, "border-width")                                         \
	X(borderStyle, "border-style")                                         \
	X(borderColor, "border-color")                                         \
	X(borderRadius, "border-radius")                                       \
	X(border, "border")                                                    \
	X(borderTop, "border-top")                                             \
	X(borderRight, "border-right")                                         \
	X(borderBottom, "border-bottom")                                       \
	X(borderLeft, "border-left")                                           \
	X(borderTopWidth, "border-top-width")                                   \
	X(borderRightWidth, "border-right-width")                               \
	X(borderBottomWidth, "border-bottom-width")                             \
	X(borderLeftWidth, "border-left-width")                                 \
	X(borderTopColor, "border-top-color")                                   \
	X(borderRightColor, "border-right-color")                               \
	X(borderBottomColor, "border-bottom-color")                             \
	X(borderLeftColor, "border-left-color")                                 \
	X(background, "background")                                            \
	X(backgroundColor, "background-color")                                 \
	X(backgroundImage, "background-image")                                 \
	X(backgroundSize, "background-size")                                   \
	X(backgroundPosition, "background-position")                           \
	X(backgroundRepeat, "background-repeat")                               \
	X(color, "color")                                                      \
	X(fontFamily, "font-family")                                           \
	X(fontSize, "font-size")                                               \
	X(fontWeight, "font-weight")                                           \
	X(fontStyle, "font-style")                                             \
	X(lineHeight, "line-height")                                           \
	X(letterSpacing, "letter-spacing")                                     \
	X(textAlign, "text-align")                                             \
	X(textDecoration, "text-decoration")                                   \
	X(textIndent, "text-indent")                                           \
	X(whiteSpace, "white-space")                                           \
	X(verticalAlign, "vertical-align")                                     \
	X(listStyle, "list-style-type")                                        \
	X(cursor, "cursor")                                                    \
	X(pointerEvents, "pointer-events")                                     \
	X(flexDirection, "flex-direction")                                     \
	X(flexWrap, "flex-wrap")                                               \
	X(flexGrow, "flex-grow")                                               \
	X(flexShrink, "flex-shrink")                                           \
	X(flexBasis, "flex-basis")                                             \
	X(order, "order")                                                      \
	X(gap, "gap")                                                          \
	X(rowGap, "row-gap")                                                   \
	X(columnGap, "column-gap")                                             \
	X(justifyContent, "justify-content")                                   \
	X(justifyItems, "justify-items")                                       \
	X(alignItems, "align-items")                                           \
	X(alignSelf, "align-self")                                             \
	X(alignContent, "align-content")                                       \
	X(gridTemplateColumns, "grid-template-columns")                         \
	X(gridTemplateRows, "grid-template-rows")                               \
	X(gridAutoColumns, "grid-auto-columns")                                 \
	X(gridAutoRows, "grid-auto-rows")                                       \
	X(gridAutoFlow, "grid-auto-flow")                                       \
	X(gridColumn, "grid-column")                                           \
	X(gridRow, "grid-row")                                                 \
	X(animation, "animation")                                              \
	X(transition, "transition")

		enum class prop : uint16_t {
#define UI_ENUM_ENTRY(name, text) name,
			UI_PROPERTIES(UI_ENUM_ENTRY)
#undef UI_ENUM_ENTRY
			count
		};

		/** Canonical CSS name for a property id. */
		const char* propertyName(prop p);
		/** Look up a property id by (lowercased) CSS name. */
		bool lookupProperty(const string& name, prop& out);
		/** True when the property inherits to children by default. */
		bool isInheritedProperty(prop p);

		// ------------------------------------------------- computed style

		/** One `grid-template-*` track. */
		struct trackSpec {
			enum class kind : uint8_t { fixed, flexible, autoTrack, minmax };
			kind k = kind::autoTrack;
			float fixed = 0.0f;   // px
			float flex = 0.0f;    // fr
			length minSize;       // minmax()/auto minimum
			length maxSize;       // minmax() maximum
		};

		/** Everything the box tree and the rasterizer need for one element. */
		struct computedStyle {
			displayType display = displayType::inlineType;
			positionType position = positionType::staticPos;
			boxSizingType boxSizing = boxSizingType::contentBox;
			overflowType overflow = overflowType::visible;
			visibilityType visibility = visibilityType::visible;
			float opacity = 1.0f;
			int zIndex = 0;

			length width, height;
			length minWidth, minHeight, maxWidth, maxHeight;
			length top, right, bottom, left;

			// Box edges, indexed [top, right, bottom, left].
			length margin[4], padding[4], borderWidth[4];
			color borderColor[4];
			borderStyleType borderStyle[4];
			length borderRadius[4];
			color backgroundColor;
			backgroundPaint backgroundImage;
			length backgroundSizeW, backgroundSizeH;
			bgSizeType backgroundSizeMode = bgSizeType::autoSize;
			length backgroundPosX, backgroundPosY;
			bool backgroundRepeat = false;

			color textColor = {0.0f, 0.0f, 0.0f, 1.0f, true};
			string fontFamily = "sans-serif";
			int fontWeight = 400;
			bool italic = false;
			length fontSize = length::px(16.0f);
			length lineHeight;  // number (unit::none) or length
			length letterSpacing;
			length textIndent;
			textAlignType textAlign = textAlignType::left;
			whiteSpaceType whiteSpace = whiteSpaceType::normal;
			alignType verticalAlign = alignType::baseline;
			bool underline = false;
			bool lineThrough = false;
			listStyleType listStyle = listStyleType::none_;
			string cursor = "default";
			bool pointerEvents = true;

			// Flex container / item.
			flexDirectionType flexDirection = flexDirectionType::row;
			flexWrapType flexWrap = flexWrapType::noWrap;
			float flexGrow = 0.0f;
			float flexShrink = 1.0f;
			length flexBasis;
			int order = 0;
			length rowGap, columnGap;
			justifyType justifyContent = justifyType::start;
			justifyType justifyItems = justifyType::start;
			alignType alignItems = alignType::stretch;
			alignType alignSelf = alignType::auto_;
			alignType alignContent = alignType::stretch;

			// Grid container / item.
			vector<trackSpec> gridColumns, gridRows;
			trackSpec gridAutoColumns, gridAutoRows;
			gridAutoFlowType gridAutoFlow = gridAutoFlowType::row;
			string gridColumn, gridRow;

			string animationShorthand;
			string transitionShorthand;

			/** Inherit everything that inherits, from `parent`. */
			void inheritFrom(const computedStyle& parent);
			/** The document defaults for the root element. */
			static computedStyle initial();
			/** True when a transition on this property can interpolate. */
			bool animatable(prop p) const;
		};

		// ------------------------------------------------------ selectors

		enum class pseudoClass : uint8_t {
			none,
			root,
			empty,
			firstChild,
			lastChild,
			onlyChild,
			firstOfType,
			lastOfType,
			onlyOfType,
			nthChild,
			nthLastChild,
			nthOfType,
			not_,
			hover,
			active,
			focus,
			checked,
			disabled,
			enabled,
			link,
		};

		enum class attrOp : uint8_t {
			exists,
			equals,
			includes,	  // ~=
			dashMatch,	  // |=
			prefix,	  // ^=
			suffix,	  // $=
			substring,  // *=
		};

		/** One simple selector inside a compound, e.g. `a.foo[href^="/"]:hover`. */
		struct simpleSelector {
			string tag;  // "" means universal
			string id;
			vector<string> classes;
			vector<std::pair<string, attrOp>> attrs;
			vector<string> attrValues;
			vector<pseudoClass> pseudos;
			vector<int> pseudoArgs;  // an+b for :nth-*
			/**
			 * `:not()` targets. A vector (rather than a nested
			 * simpleSelector) keeps simpleSelector usable inside its own
			 * definition.
			 */
			vector<simpleSelector> notSelectors;
			/** Pseudo-element (::before / ::after) — parsed, not rendered. */
			string pseudoElement;
		};

		enum class combinator : uint8_t {
			descendant,
			child,
			adjacentSibling,
			generalSibling,
		};

		/** A compound selector: every simple selector must match. */
		struct compoundSelector {
			vector<simpleSelector> simples;
		};

		/** A full complex selector, left to right. `combinators[i]` joins
		 *  `parts[i-1]` to `parts[i]`. */
		struct complexSelector {
			vector<compoundSelector> parts;
			vector<combinator> combinators;
		};

		struct specificity {
			int ids = 0, classes = 0, tags = 0;
			bool operator<(const specificity& o) const {
				if (ids != o.ids) return ids < o.ids;
				if (classes != o.classes) return classes < o.classes;
				return tags < o.tags;
			}
			bool operator==(const specificity& o) const {
				return ids == o.ids && classes == o.classes && tags == o.tags;
			}
		};

		/** Parse a selector list (`"a, .b > c"`). Bad parts are skipped. */
		vector<complexSelector> parseSelectorList(const string& text);
		specificity selectorSpecificity(const complexSelector& sel);
		/** Right-to-left match of a complex selector against a node. */
		bool selectorMatches(const complexSelector& sel, const domTree& tree,
			int node);
		bool compoundMatches(const compoundSelector& compound,
			const domTree& tree, int node);
		bool simpleMatches(const simpleSelector& simple, const domTree& tree,
			int node);

		// ----------------------------------------------------- stylesheet

		struct styleRule {
			complexSelector selector;
			object declarations;
			specificity spec;
			int order = 0;
			/** Media query text; empty matches every viewport. */
			string media;
		};

		struct keyframeStep {
			float offset = 0.0f;
			object declarations;
		};

		struct keyframesBlock {
			string name;
			vector<keyframeStep> steps;  // sorted by offset
		};

		/** A parsed stylesheet: at-rules hoisted, rules in cascade order. */
		struct stylesheet {
			vector<styleRule> rules;  // ascending cascade priority
			vector<keyframesBlock> keyframes;
			vector<string> fontFaces;
			/** Rule count, for change detection. */
			size_t size() const { return rules.size(); }
			const keyframesBlock* findKeyframes(const string& name) const;
		};

		/**
		 * Parse a full stylesheet, including `@media`, `@keyframes` and
		 * `@font-face`. `CSS::parseCSS` only understands plain rules, so this
		 * scans at-rules itself and reuses the declaration parser.
		 */
		stylesheet parseStylesheet(const string& css);

		// ------------------------------------------------------- cascade

		/** Inputs that the cascade needs beyond the DOM. */
		struct styleContext {
			float viewportWidth = 0.0f;
			float viewportHeight = 0.0f;
			string defaultFontFamily = "sans-serif";
			float defaultFontSize = 16.0f;
			/** False for touch-driven surfaces, where `:hover` never sticks. */
			bool hoverCapable = true;
		};

		/**
		 * Compute one style per DOM node (same indexing as `tree.nodes`).
		 * Inherited properties come from the parent, then the cascade runs in
		 * ascending priority, then `!important`, then the inline `style`
		 * attribute.
		 */
		vector<computedStyle> resolveStyles(const domTree& tree,
			const stylesheet& sheet, const styleContext& ctx);

		/** Evaluate a media query list against the viewport/capabilities. */
		bool mediaMatches(const string& query, const styleContext& ctx);

		// -------------------------------------------------- value parsing

		/** Parse a CSS `<length>`; `auto`/`none` map to their states. */
		bool parseLength(const string& text, length& out);
		/** Parse a CSS color: #hex, rgb(), rgba(), hsl(), or a named color. */
		bool parseColor(const string& text, color& out);
		/** Parse a `background-image` value (gradients). */
		bool parseBackgroundImage(const string& text, backgroundPaint& out);
		/** Parse a `grid-template-*` value into tracks. */
		bool parseTracks(const string& text, vector<trackSpec>& out);
		/** Split a whitespace-separated list, honoring quotes and parens. */
		vector<string> splitValues(const string& text, char sep = ' ');
		/** Strip a trailing `!important`; returns true when it was present. */
		bool stripImportant(string& text);

		// ------------------------------------------------------ animations

		/** A parsed `animation` shorthand. */
		struct animationSpec {
			string name;
			float duration = 0.0f;   // seconds
			string easing = "ease";  // timing-function name
			float delay = 0.0f;      // seconds
			float iterations = 1.0f;
			bool infinite = false;
			enum class direction : uint8_t {
				normal,
				reverse,
				alternate,
				alternateReverse,
			};
			enum class fill : uint8_t { none, forwards, backwards, both };
			direction dir = direction::normal;
			fill fillMode = fill::none;
		};

		/** Parse the `animation` shorthand: name duration timing delay
		 *  iteration-count direction fill-mode. Times are `2s`/`800ms`, the
		 *  bare-number token is the iteration count, and the first token
		 *  that matches nothing else is the name; unrecognized tokens after
		 *  that are ignored. */
		animationSpec parseAnimationShorthand(const string& value);

		/** The keyframe timeline position in [0,1] at `elapsed` seconds,
		 *  with delay, iteration count, direction and fill applied (0 or 1
		 *  for a holding fill). A negative return means the animation is
		 *  not playing. */
		float animationProgress(const animationSpec& spec, double elapsed);

		/** The named timing function's eased value of `f` in [0,1]
		 *  (linear, ease, ease-in, ease-out, ease-in-out as the standard
		 *  cubic beziers). */
		float easeValue(const string& easing, float f);

		/**
		 * The declarations overlay for a playing animation: every property
		 * in the bracketing keyframe steps, interpolated (numbers, colors,
		 * and non-percentage lengths resolved to px; pairs that cannot
		 * interpolate take the far step's value). Empty when the animation
		 * is not playing or the sheet has no such keyframes. Percentages
		 * and non-numeric values stay out of v1.
		 */
		object keyframeDeclarations(const animationSpec& spec,
			const stylesheet& sheet, const styleContext& ctx,
			float elementFontSize, float rootFontSize, double elapsed);

		/** Which declarations `applyDeclarations` should touch. */
		enum class importantMode : uint8_t { normal, only, skip };

		/**
		 * Apply one declaration (`"color" -> "red"`) to a style. Returns
		 * false for unknown properties or unparseable values, so callers can
		 * count what they ignored.
		 */
		bool applyDeclaration(computedStyle& style, prop p, const var& value,
			const styleContext& ctx, float parentFontSize,
			float rootFontSize);
		/** Apply a whole `declarations` object. */
		void applyDeclarations(computedStyle& style, const object& declarations,
			const styleContext& ctx, float parentFontSize, float rootFontSize,
			importantMode mode = importantMode::normal);
		/** Resolve a length against a percentage basis and font size. */
		float resolveLength(length l, float basis, float fontSize,
			float rootFontSize, const styleContext& ctx);
		/** Default line height in px for a font size (1.2 when unspecified). */
		float lineHeightPx(const computedStyle& style, float fontSize);

	}  // namespace UI
}  // namespace gold
