#include "ui/layout.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace gold {
	namespace UI {

		namespace {

			bool isSpaceChar(char c) {
				return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
					   c == '\f' || c == '\v';
			}

			bool isInlineLevel(displayType d) {
				return d == displayType::inlineType;
			}

			bool isAtomicLevel(displayType d) {
				return d == displayType::inlineBlock ||
					   d == displayType::inlineFlex ||
					   d == displayType::inlineGrid;
			}

			bool isBlockLevel(displayType d) {
				return d == displayType::block || d == displayType::flex ||
					   d == displayType::grid;
			}

		}  // namespace

		float resolve(length l, float basis, float fontSize,
			float rootFontSize, const layoutContext& ctx) {
			switch (l.u) {
				case unit::px: return l.value;
				case unit::em: return l.value * fontSize;
				case unit::rem: return l.value * rootFontSize;
				case unit::percent: return l.value * 0.01f * basis;
				case unit::viewportW: return l.value * 0.01f * ctx.viewportWidth;
				case unit::viewportH: return l.value * 0.01f * ctx.viewportHeight;
				case unit::number: return l.value;
				case unit::autoLen:
				case unit::none:
				default: return 0.0f;
			}
		}

		float fontSizeOf(const computedStyle& style, float parentSize,
			float rootFontSize, const layoutContext& ctx) {
			if (!style.fontSize.defined()) return parentSize;
			switch (style.fontSize.u) {
				case unit::em: return style.fontSize.value * parentSize;
				case unit::rem: return style.fontSize.value * rootFontSize;
				case unit::percent:
					return style.fontSize.value * 0.01f * parentSize;
				default:
					return resolve(style.fontSize, parentSize, parentSize,
						rootFontSize, ctx);
			}
		}

		namespace {

			/** One entry in an inline formatting context. */
			struct inlineItem {
				enum class kind : uint8_t { word, atomic, block };
				kind k = kind::word;
				int node = -1;  // owning element
				string text;
				float width = 0.0f;
				bool isSpace = false;
			};

			/** Resolved box edges, indexed [top, right, bottom, left]. */
			struct edges {
				float margin[4] = {0, 0, 0, 0};
				float border[4] = {0, 0, 0, 0};
				float padding[4] = {0, 0, 0, 0};
				float horizontalExtra() const {
					return margin[1] + margin[3] + border[1] + border[3] +
						   padding[1] + padding[3];
				}
				float verticalExtra() const {
					return border[0] + border[2] + padding[0] + padding[2];
				}
			};

			/**
			 * One layout pass. Boxes are stored parallel to the DOM, so any
			 * helper can address a box by DOM node index and painting,
			 * hit testing and event dispatch need no mapping.
			 */
			class layoutEngine {
			 public:
				layoutEngine(const domTree& tree,
					const vector<computedStyle>& styles,
					const layoutContext& ctx, layoutResult& out)
					: tree_(tree), styles_(styles), ctx_(ctx), out_(out) {
					out_.boxes.resize(tree.nodes.size());
						fontSize_.assign(tree.nodes.size(), ctx.defaultFontSize);
					for (size_t i = 0; i < tree.nodes.size(); i++) {
						layoutBox& b = out_.boxes[i];
						b.node = (int)i;
						b.isTextNode = tree.nodes[i].isText;
						b.parent = tree.nodes[i].parent;
						b.children = tree.nodes[i].children;
					}
				}

				void run() {
					// Font sizes first: em lengths, line heights and text
					// measurement all depend on them. Parents come first in
					// build order, so a single forward pass is enough.
					for (size_t i = 0; i < tree_.nodes.size(); i++) {
						const int p = tree_.nodes[i].parent;
						const float parentSize =
							p >= 0 ? fontSize_[(size_t)p] : ctx_.defaultFontSize;
						fontSize_[i] = fontSizeOf(styles_[i], parentSize,
							ctx_.rootFontSize, ctx_);
					}

					float maxBottom = 0.0f;
					// Top-level elements stack like block siblings, so a
					// fragment with several roots fills the page downward.
					float cursor = 0.0f;
					for (int root : tree_.roots()) {
						if (styles_[(size_t)root].display == displayType::none)
							continue;
						const rect r = layoutBlockLevel(root,
							ctx_.viewportWidth, 0.0f, cursor, false, 0.0f,
							false);
						cursor = r.bottom();
						maxBottom = std::max(maxBottom, cursor);
					}
					out_.width = ctx_.viewportWidth;
					out_.height = std::max(ctx_.viewportHeight, maxBottom);
					applyRelativeOffsets();
					out_.generation++;
				}

				// ---------------------------------------------------- sizing

				float fontOf(int node) const { return fontSize_[(size_t)node]; }
				const computedStyle& style(int node) const {
					return styles_[(size_t)node];
				}
				layoutBox& box(int node) { return out_.boxes[(size_t)node]; }
				const layoutBox& box(int node) const {
					return out_.boxes[(size_t)node];
				}
				float len(int node, length l, float basis) const {
					return resolve(l, basis, fontOf(node), ctx_.rootFontSize, ctx_);
				}

				edges edgesOf(int node, float availableWidth) const {
					const computedStyle& s = style(node);
					edges e;
					for (int i = 0; i < 4; i++) {
						const float basis = (i == 1 || i == 3) ? availableWidth
															  : 0.0f;
						e.margin[i] = resolve(s.margin[i], basis, fontOf(node),
							ctx_.rootFontSize, ctx_);
						e.padding[i] = resolve(s.padding[i], basis, fontOf(node),
							ctx_.rootFontSize, ctx_);
						// `border-style: none` means no border box at all.
						e.border[i] = s.borderStyle[i] == borderStyleType::none
										  ? 0.0f
										  : resolve(s.borderWidth[i], basis,
												fontOf(node), ctx_.rootFontSize,
												ctx_);
					}
					return e;
				}

				/**
				 * Lay out a block-level box at (x, y) and return its border
				 * box. `forcedWidth` is how flex and grid place items.
				 */
				rect layoutBlockLevel(int node, float availableWidth, float x,
					float y, bool shrinkToFit = false, float forcedWidth = 0.0f,
					bool hasForcedWidth = false) {
					const computedStyle& s = style(node);
					layoutBox& b = box(node);
					b.runs.clear();
					b.lines.clear();
					b.hasMarker = false;
					b.visible = s.display != displayType::none &&
								s.visibility != visibilityType::hidden;
					b.opacity = s.opacity;
					b.zIndex = s.zIndex;

					const edges e = edgesOf(node, availableWidth);
					const float extraW = e.border[1] + e.border[3] +
										 e.padding[1] + e.padding[3];

					float contentWidth = 0.0f;
					if (hasForcedWidth) {
						contentWidth = forcedWidth;
					} else if (s.width.defined() && !s.width.isAuto()) {
						contentWidth = len(node, s.width, availableWidth);
						if (s.boxSizing == boxSizingType::borderBox)
							contentWidth -= extraW;
					} else if (shrinkToFit) {
						contentWidth =
							shrinkToFitWidth(node, availableWidth - extraW);
					} else {
						contentWidth = availableWidth - extraW - e.margin[1] -
									   e.margin[3];
					}
					if (s.minWidth.defined() && !s.minWidth.isAuto()) {
						float minContent =
							len(node, s.minWidth, availableWidth);
						if (s.boxSizing == boxSizingType::borderBox)
							minContent -= extraW;
						contentWidth = std::max(contentWidth, minContent);
					}
					if (s.maxWidth.defined() && !s.maxWidth.isAuto()) {
						float maxContent =
							len(node, s.maxWidth, availableWidth);
						if (s.boxSizing == boxSizingType::borderBox)
							maxContent -= extraW;
						if (maxContent > 0.0f)
							contentWidth = std::min(contentWidth, maxContent);
					}
					contentWidth = std::max(contentWidth, 0.0f);

					rect border;
					border.x = x + e.margin[3];
					border.y = y + e.margin[0];
					border.w = contentWidth + extraW;

					rect content = border.inset(e.border[0] + e.padding[0],
						e.border[1] + e.padding[1], e.border[2] + e.padding[2],
						e.border[3] + e.padding[3]);

					const bool fixedHeight = s.height.defined() &&
											  !s.height.isAuto();
					const float extraV = e.border[0] + e.border[2] + e.padding[0] +
										 e.padding[2];
					float contentHeight = 0.0f;
					if (fixedHeight) {
						contentHeight = len(node, s.height, ctx_.viewportHeight);
						if (s.boxSizing == boxSizingType::borderBox)
							contentHeight -= extraV;
						contentHeight = std::max(contentHeight, 0.0f);
					}
					content.h = contentHeight;

					b.border = border;
					b.content = content;
					b.clipped = s.overflow == overflowType::hidden ||
								s.overflow == overflowType::scroll ||
								s.overflow == overflowType::auto_;

					// Children report the height they used; for an auto-height
					// box that becomes the box's own height.
					const float used = layoutChildren(node, content, content.w);
					if (!fixedHeight) content.h = std::max(content.h, used);

					// min-height / max-height resolve against the content box.
					if (s.minHeight.defined() && !s.minHeight.isAuto()) {
						float minH = len(node, s.minHeight, ctx_.viewportHeight);
						if (s.boxSizing == boxSizingType::borderBox) minH -= extraV;
						content.h = std::max(content.h, minH);
					}
					if (s.maxHeight.defined() && !s.maxHeight.isAuto()) {
						float maxH = len(node, s.maxHeight, ctx_.viewportHeight);
						if (s.boxSizing == boxSizingType::borderBox) maxH -= extraV;
						if (maxH > 0.0f) content.h = std::min(content.h, maxH);
					}
					content.h = std::max(content.h, 0.0f);
					border.h = content.h + extraV;
					b.border = border;
					b.content = content;
					layoutMarker(node, content);
					return border;
				}

				/** min(max-content, available) with a min-content floor. */
				float shrinkToFitWidth(int node, float available) {
					if (available <= 0.0f) return maxContentWidth(node);
					const float maxContent = maxContentWidth(node);
					const float minContent = minContentWidth(node);
					return std::min(std::max(minContent, available), maxContent);
				}

				void measureIntrinsic(int node, float& minContent,
					float& maxContent) {
					vector<inlineItem> items;
					collectInline(node, items);
					float widestLine = 0.0f;
					float lineWidth = 0.0f;
					minContent = 0.0f;
					for (const inlineItem& item : items) {
						if (item.isSpace) continue;
						minContent = std::max(minContent, item.width);
						if (item.k != inlineItem::kind::word) {
							widestLine = std::max(widestLine, lineWidth);
							lineWidth = item.width;
							continue;
						}
						lineWidth += item.width;
					}
					maxContent = std::max(widestLine, lineWidth);

					// Explicitly sized block children count too.
					for (int child : tree_.nodes[(size_t)node].children) {
						if (tree_.nodes[(size_t)child].isText) continue;
						const computedStyle& cs = style(child);
						if (!isBlockLevel(cs.display)) continue;
						if (!cs.width.defined() || cs.width.isAuto()) continue;
						const float w = resolve(cs.width, available_, fontOf(child),
							ctx_.rootFontSize, ctx_);
						maxContent = std::max(maxContent, w);
						minContent = std::max(minContent, w);
					}
				}

				float maxContentWidth(int node) {
					float minContent = 0.0f, maxContent = 0.0f;
					measureIntrinsic(node, minContent, maxContent);
					return maxContent;
				}

				float minContentWidth(int node) {
					float minContent = 0.0f, maxContent = 0.0f;
					measureIntrinsic(node, minContent, maxContent);
					return minContent;
				}

				// ---------------------------------------------- block flow

				/** Lay out a block container's children; returns used height. */
				float layoutChildren(int node, const rect& content,
					float innerWidth) {
					const computedStyle& s = style(node);
					available_ = innerWidth;

					if (s.display == displayType::flex ||
						s.display == displayType::inlineFlex)
						return std::max(content.h,
							layoutFlexChildren(node, content, innerWidth));
					if (s.display == displayType::grid ||
						s.display == displayType::inlineGrid)
						return std::max(content.h,
							layoutGridChildren(node, content, innerWidth));

					float y = content.y;
					vector<inlineItem> lineItems;
					bool pendingSpace = false;

					auto flushInline = [&]() {
						if (lineItems.empty()) return;
						y = layoutInlineGroup(node, lineItems, content.x, y,
							innerWidth);
						lineItems.clear();
						pendingSpace = false;
					};

					for (int child : tree_.nodes[(size_t)node].children) {
						const domNode& cn = tree_.nodes[(size_t)child];
						const computedStyle& cs = style(child);
						if (cs.display == displayType::none) {
							box(child).visible = false;
							continue;
						}
						if (cn.isText) {
							collectTextNode(child, lineItems, pendingSpace);
							pendingSpace = true;
							continue;
						}
						if (cs.position == positionType::absolute) {
							// Out of flow: laid out, but no height added.
							layoutOutOfFlow(child, content, innerWidth);
							continue;
						}
						if (isInlineLevel(cs.display) ||
							isAtomicLevel(cs.display)) {
							collectAtomic(child, lineItems);
							pendingSpace = false;
							continue;
						}
						flushInline();
						const rect childBox = layoutBlockLevel(child, innerWidth,
							content.x, y);
						y = childBox.bottom();
					}
					flushInline();
					return y - content.y;
				}

				// ---------------------------------------------- flex layout

				/** One resolved grid/flex track on an axis. */
				struct track {
					float start = 0.0f;
					float size = 0.0f;
				};

				/**
				 * Lay out `display: flex` children.
				 *
				 * Covers what a game UI needs: direction, wrap, gap,
				 * justify-content, align-items/self, order, and
				 * grow/shrink/basis. `flex-basis: auto` falls back to the
				 * item's own width, then to its max-content size.
				 */
				float layoutFlexChildren(int node, const rect& content,
					float innerWidth) {
					const computedStyle& cs = style(node);
					const bool row = cs.flexDirection == flexDirectionType::row ||
									 cs.flexDirection == flexDirectionType::rowReverse;
					const bool reverse =
						cs.flexDirection == flexDirectionType::rowReverse ||
						cs.flexDirection == flexDirectionType::columnReverse;
					const float mainGap =
						len(node, row ? cs.columnGap : cs.rowGap, innerWidth);
					const float crossGap =
						len(node, row ? cs.rowGap : cs.columnGap, innerWidth);
					const float mainSize = row ? innerWidth : content.h;
					const float crossSize = row ? content.h : innerWidth;

					struct entry {
						int node;
						int order;
					};
					vector<entry> items;
					for (int child : tree_.nodes[(size_t)node].children) {
						if (tree_.nodes[(size_t)child].isText) continue;
						const computedStyle& s = style(child);
						if (s.display == displayType::none) continue;
						if (s.position == positionType::absolute) {
							layoutOutOfFlow(child, content, innerWidth);
							continue;
						}
						items.push_back({child, s.order});
					}
					std::stable_sort(items.begin(), items.end(),
						[](const entry& a, const entry& b) {
							return a.order < b.order;
						});

					// Hypothetical main size per item.
					vector<float> size;
					for (const entry& e : items) {
						const computedStyle& s = style(e.node);
						const edges ed = edgesOf(e.node, innerWidth);
						float value = 0.0f;
						if (s.flexBasis.defined() && !s.flexBasis.isAuto()) {
							value = len(e.node, s.flexBasis, innerWidth);
						} else if (s.width.defined() && !s.width.isAuto()) {
							value = len(e.node, s.width, innerWidth);
							if (s.boxSizing == boxSizingType::contentBox)
								value += ed.horizontalExtra();
						} else if (row) {
							value = maxContentWidth(e.node);
						}
						size.push_back(std::max(value, 0.0f));
					}

					// Wrap into lines.
					const bool wrap = cs.flexWrap != flexWrapType::noWrap;
					vector<vector<size_t>> lines;
					vector<size_t> current;
					float usedMain = 0.0f;
					for (size_t i = 0; i < items.size(); i++) {
						const float gap = current.empty() ? 0.0f : mainGap;
						if (wrap && !current.empty() &&
							usedMain + gap + size[i] > mainSize + 0.01f) {
							lines.push_back(current);
							current.clear();
							usedMain = 0.0f;
						}
						usedMain += (current.empty() ? 0.0f : mainGap) + size[i];
						current.push_back(i);
					}
					lines.push_back(current);

					// Resolve grow/shrink inside each line.
					vector<float> lineMain(lines.size(), 0.0f);
					for (size_t li = 0; li < lines.size(); li++) {
						const vector<size_t>& lineItems = lines[li];
						float line = 0.0f;
						for (size_t index : lineItems) line += size[index];
						line += mainGap * (float)(lineItems.size() - 1);
						const float free = mainSize - line;
						float growSum = 0.0f, shrinkSum = 0.0f;
						for (size_t index : lineItems) {
							const computedStyle& s = style(items[index].node);
							if (free > 0.0f) growSum += s.flexGrow;
							else shrinkSum += s.flexShrink * size[index];
						}
						if (free > 0.0f && growSum > 0.0f) {
							for (size_t index : lineItems) {
								const float grow = style(items[index].node).flexGrow;
								if (grow <= 0.0f) continue;
								size[index] += free * (grow / growSum);
							}
						} else if (free < 0.0f && shrinkSum > 0.0f) {
							for (size_t index : lineItems) {
								const computedStyle& s = style(items[index].node);
								if (s.flexShrink <= 0.0f) continue;
								size[index] = std::max(0.0f,
									size[index] + free *
													 (s.flexShrink * size[index] /
													  shrinkSum));
							}
						}
						line = 0.0f;
						for (size_t index : lineItems) line += size[index];
						lineMain[li] = line + mainGap * (float)(lineItems.size() - 1);
					}

					// Lay each item out at its resolved main size to measure
					// it, remembering where it landed so the placement pass
					// below can translate by the delta.
					vector<float> lineCross(lines.size(), 0.0f);
					vector<float> itemX(items.size(), 0.0f);
					vector<float> itemY(items.size(), 0.0f);
					for (size_t li = 0; li < lines.size(); li++) {
						float mainCursor = 0.0f;
						const vector<size_t>& lineItems = lines[li];
						for (size_t index : lineItems) {
							const int child = items[index].node;
							const float x = row ? content.x + mainCursor
												: content.x;
							const float y = row ? content.y : content.y + mainCursor;
							if (row) {
								layoutBlockLevel(child, innerWidth, x, y, true,
									size[index], true);
							} else {
								layoutBlockLevel(child, innerWidth, x, y, true,
									0.0f, false);
								// The main size is the item's block height.
								box(child).content.h = size[index];
								layoutBlockLevel(child, innerWidth, x, y, true,
									innerWidth, true);
							}
							itemX[index] = box(child).border.x;
							itemY[index] = box(child).border.y;
							const rect r = box(child).border;
							lineCross[li] = std::max(lineCross[li],
								row ? r.h : r.w);
							mainCursor += size[index] + mainGap;
						}
					}
					if (cs.alignContent == alignType::stretch &&
						lines.size() == 1)
						lineCross[0] = std::max(lineCross[0], crossSize);

					// Cross alignment + main-axis justification.
					float mainCursor = 0.0f;
					for (size_t li = 0; li < lines.size(); li++) {
						const vector<size_t>& lineItems = lines[li];
						const size_t count = lineItems.size();
						const float slack = mainSize - lineMain[li];
						float spacing = mainGap;
						float offset = 0.0f;
						if (count > 0) {
							switch (cs.justifyContent) {
								case justifyType::center:
									offset = slack * 0.5f;
									break;
								case justifyType::end:
									offset = slack;
									break;
								case justifyType::spaceBetween:
									if (count > 1)
										spacing = mainGap +
												  slack / (float)(count - 1);
									break;
								case justifyType::spaceAround:
									offset = slack / (float)(2 * count);
									if (count > 1)
										spacing = mainGap + slack / (float)count;
									break;
								case justifyType::spaceEvenly:
									offset = slack / (float)(count + 1);
									spacing = mainGap + slack / (float)(count + 1);
									break;
								default: break;
							}
						}
						if (reverse) {
							offset = slack - offset;
							spacing = mainGap;
						}

						float cursor = offset;
						for (size_t k = 0; k < count; k++) {
							const size_t index =
								reverse ? lineItems[count - 1 - k] : lineItems[k];
							const int child = items[index].node;
							alignType align = style(child).alignSelf;
							if (align == alignType::auto_) align = cs.alignItems;
							const rect before = box(child).border;
							const float itemCross = row ? before.h : before.w;
							float crossOffset = 0.0f;
							switch (align) {
								case alignType::end:
									crossOffset = lineCross[li] - itemCross;
									break;
								case alignType::center:
									crossOffset =
										(lineCross[li] - itemCross) * 0.5f;
									break;
								case alignType::stretch:
									if (row) {
										const float extra =
											lineCross[li] - itemCross;
										box(child).border.h += extra;
										box(child).content.h += extra;
									} else {
										const float extra =
											lineCross[li] - itemCross;
										box(child).border.w += extra;
										box(child).content.w += extra;
									}
									break;
								default: break;
							}
							// Move from where the measuring pass put the item
							// to where justification and alignment want it.
							const float targetX = row
													  ? content.x + mainCursor +
															cursor
													  : content.x + crossOffset;
							const float targetY = row
													  ? content.y + mainCursor +
															crossOffset
													  : content.y + mainCursor +
															cursor;
							translateSubtree(child, targetX - itemX[index],
								targetY - itemY[index]);
							cursor += size[index] + spacing;
						}
						mainCursor += lineMain[li] +
									  (li + 1 < lines.size() ? crossGap : 0.0f);
					}
					return mainCursor;
				}

				// ---------------------------------------------- grid layout

				/** A resolved placement of one grid item. */
				struct gridPlacement {
					int node = 0;
					int column = 1;	   // 1-based
					int row = 1;	   // 1-based
					int columnSpan = 1;
					int rowSpan = 1;
				};

				static bool isSpanToken(const string& s) {
					return s.size() > 5 && s.compare(0, 5, "span ") == 0;
				}

				/** Size one axis: fixed tracks, then `fr`, then `auto`. */
				vector<track> buildTracks(const vector<trackSpec>& specs,
					size_t count, float available, float gap, bool isRow,
					const vector<gridPlacement>& placed) {
					vector<trackSpec> defs;
					defs.reserve(count);
					for (size_t i = 0; i < count; i++)
						defs.push_back(i < specs.size() ? specs[i] : trackSpec());

					float gaps = gap * (float)(count > 0 ? count - 1 : 0);
					float free = available - gaps;
					float fixedTotal = 0.0f;
					float flexTotal = 0.0f;
					for (const trackSpec& spec : defs) {
						switch (spec.k) {
							case trackSpec::kind::fixed:
								fixedTotal += spec.fixed;
								break;
							case trackSpec::kind::minmax:
								if (spec.flex > 0.0f) flexTotal += spec.flex;
								else
									fixedTotal += spec.minSize.defined()
													  ? spec.minSize.value
													  : 0.0f;
								break;
							case trackSpec::kind::flexible: flexTotal += spec.flex; break;
							case trackSpec::kind::autoTrack:
							default: break;
						}
					}

					vector<track> out;
					out.reserve(count);
					float cursor = 0.0f;
					float leftover = free - fixedTotal;
					vector<float> autoSize(count, 0.0f);
					for (const gridPlacement& p : placed) {
						const size_t index = isRow ? (size_t)(p.row - 1)
												  : (size_t)(p.column - 1);
						if (index >= count) continue;
						const float need = isRow
							? (p.node >= 0 ? style(p.node).display ==
										displayType::none
											? 0.0f
											: fontOf(p.node) * 1.2f
									  : 0.0f)
							: maxContentWidth(p.node);
						autoSize[index] = std::max(autoSize[index], need);
					}

					for (size_t i = 0; i < count; i++) {
						const trackSpec& spec = defs[i];
						track t;
						t.start = cursor;
						switch (spec.k) {
							case trackSpec::kind::fixed:
								t.size = spec.fixed;
								break;
							case trackSpec::kind::flexible:
								t.size = flexTotal > 0.0f && leftover > 0.0f
											 ? leftover * (spec.flex / flexTotal)
											 : 0.0f;
								break;
							case trackSpec::kind::minmax:
								if (spec.flex > 0.0f)
									t.size = flexTotal > 0.0f && leftover > 0.0f
												 ? leftover * (spec.flex / flexTotal)
												 : 0.0f;
								else
									t.size = spec.minSize.defined()
												 ? spec.minSize.value
												 : 0.0f;
								break;
							case trackSpec::kind::autoTrack:
							default:
								t.size = autoSize[i];
								break;
						}
						t.size = std::max(t.size, 0.0f);
						out.push_back(t);
						cursor += t.size + gap;
					}
					return out;
				}

				/**
				 * Lay out `display: grid` children: explicit tracks, `fr`
				 * distribution, `auto` tracks sized to their content, sparse
				 * auto-placement, and `grid-column` / `grid-row` spans.
				 */
				float layoutGridChildren(int node, const rect& content,
					float innerWidth) {
					const computedStyle& cs = style(node);
					const float colGap = len(node, cs.columnGap, innerWidth);
					const float rowGap = len(node, cs.rowGap, innerWidth);
					const bool columnFlow =
						cs.gridAutoFlow == gridAutoFlowType::column;

					vector<gridPlacement> placed;
					size_t columnCount = cs.gridColumns.size();
					size_t rowCount = cs.gridRows.size();
					int autoRow = 1, autoColumn = 1;

					for (int child : tree_.nodes[(size_t)node].children) {
						if (tree_.nodes[(size_t)child].isText) continue;
						const computedStyle& s = style(child);
						if (s.display == displayType::none) continue;
						if (s.position == positionType::absolute) {
							layoutOutOfFlow(child, content, innerWidth);
							continue;
						}
						gridPlacement p;
						p.node = child;
						p.column = 0;
						p.row = 0;
						const vector<string> colParts =
							splitValues(s.gridColumn, ' ');
						if (!colParts.empty() && !colParts[0].empty())
							p.column = atoi(colParts[0].c_str());
						if (colParts.size() > 1 && isSpanToken(colParts[1]))
							p.columnSpan = atoi(colParts[1].c_str() + 5);
						const vector<string> rowParts =
							splitValues(s.gridRow, ' ');
						if (!rowParts.empty() && !rowParts[0].empty())
							p.row = atoi(rowParts[0].c_str());
						if (rowParts.size() > 1 && isSpanToken(rowParts[1]))
							p.rowSpan = atoi(rowParts[1].c_str() + 5);

						if (p.column <= 0 && p.row <= 0) {
							// Sparse auto-placement along the flow axis.
							if (columnFlow) {
								p.column = autoColumn++;
								p.row = autoRow;
								if (autoColumn > (int)columnCount) {
									autoColumn = 1;
									autoRow++;
								}
							} else {
								p.row = autoRow++;
								p.column = autoColumn;
								if (autoRow > (int)rowCount) {
									autoRow = 1;
									autoColumn++;
								}
							}
						} else {
							if (p.column > 0) autoColumn = p.column;
							if (p.row > 0) autoRow = p.row;
						}
						if (p.column <= 0) p.column = 1;
						if (p.row <= 0) p.row = 1;
						columnCount = std::max(columnCount,
							(size_t)(p.column - 1 + p.columnSpan));
						rowCount = std::max(rowCount,
							(size_t)(p.row - 1 + p.rowSpan));
						placed.push_back(p);
					}
					if (columnCount == 0) columnCount = 1;
					if (rowCount == 0) rowCount = 1;

					const vector<track> columnTracks = buildTracks(cs.gridColumns,
						columnCount, innerWidth, colGap, false, placed);
					const vector<track> rowTracks = buildTracks(cs.gridRows,
						rowCount, content.h, rowGap, true, placed);

					for (const gridPlacement& p : placed) {
						const size_t ci = (size_t)(p.column - 1);
						const size_t ri = (size_t)(p.row - 1);
						if (ci >= columnTracks.size() || ri >= rowTracks.size())
							continue;
						const float x = content.x + columnTracks[ci].start;
						const float y = content.y + rowTracks[ri].start;
						float w = 0.0f, h = 0.0f;
						for (int k = 0; k < p.columnSpan; k++) {
							const size_t index = ci + (size_t)k;
							if (index < columnTracks.size())
								w += columnTracks[index].size +
									 (k ? colGap : 0.0f);
						}
						for (int k = 0; k < p.rowSpan; k++) {
							const size_t index = ri + (size_t)k;
							if (index < rowTracks.size())
								h += rowTracks[index].size + (k ? rowGap : 0.0f);
						}

						// justify-items / align-self inside the area.
						alignType align = style(p.node).alignSelf;
						if (align == alignType::auto_) align = cs.alignItems;
						float alignOffsetY = 0.0f;
						if (align == alignType::end) alignOffsetY = 0.0f;
						else if (align == alignType::center)
							alignOffsetY = (h - fontOf(p.node) * 1.2f) * 0.5f;

						layoutBlockLevel(p.node, w, x, y, true, w, true);
						// The row track owns the height of a spanning item.
						box(p.node).content.h = std::max(h, box(p.node).content.h);
						box(p.node).border.h =
							box(p.node).content.h +
							(box(p.node).border.h - box(p.node).content.h);
						if (alignOffsetY != 0.0f)
							translateSubtree(p.node, 0.0f, alignOffsetY);
					}

					float usedHeight = 0.0f;
					for (size_t i = 0; i < rowTracks.size(); i++) {
						usedHeight += rowTracks[i].size;
						if (i + 1 < rowTracks.size()) usedHeight += rowGap;
					}
					return usedHeight;
				}

				// -------------------------------------------- inline flow

				void collectTextNode(int node, vector<inlineItem>& items,
					bool pendingSpace) {
					const domNode& dn = tree_.nodes[(size_t)node];
					const int owner = dn.parent;
					if (owner < 0 || dn.text.empty()) return;
					const computedStyle& os = style(owner);
					if (os.whiteSpace == whiteSpaceType::pre ||
						os.whiteSpace == whiteSpaceType::preWrap) {
						inlineItem item;
						item.node = owner;
						item.text = dn.text;
						item.width = measure(dn.text, owner);
						items.push_back(item);
						return;
					}

					// Collapse runs of whitespace into a single space item.
					string word;
					bool spacePending = pendingSpace;
					auto flushWord = [&]() {
						if (word.empty()) return;
						if (spacePending) {
							inlineItem space;
							space.node = owner;
							space.text = " ";
							space.isSpace = true;
							space.width = measure(" ", owner);
							items.push_back(space);
							spacePending = false;
						}
						inlineItem item;
						item.node = owner;
						item.text = word;
						item.width = measure(word, owner);
						items.push_back(item);
						word.clear();
					};
					for (char c : dn.text) {
						if (isSpaceChar(c)) {
							flushWord();
							spacePending = true;
							continue;
						}
						word += c;
					}
					flushWord();
				}

				void collectAtomic(int node, vector<inlineItem>& items) {
					inlineItem item;
					item.k = inlineItem::kind::atomic;
					item.node = node;
					const rect r = layoutBlockLevel(node, available_, 0.0f, 0.0f,
						true, 0.0f, false);
					item.width = r.w;
					items.push_back(item);
				}

				/** Flatten inline descendants into items for wrapping. */
				void collectInline(int node, vector<inlineItem>& items,
					int depth = 0) {
					if (depth > 32) return;  // pathological nesting guard
					for (int child : tree_.nodes[(size_t)node].children) {
						const domNode& cn = tree_.nodes[(size_t)child];
						const computedStyle& cs = style(child);
						if (cs.display == displayType::none) continue;
						if (cn.isText) {
							collectTextNode(child, items,
								!items.empty() && items.back().isSpace);
							continue;
						}
						if (isAtomicLevel(cs.display)) {
							collectAtomic(child, items);
							continue;
						}
						if (isBlockLevel(cs.display)) {
							// Block-in-inline: place it as an atomic item so
							// the line box can still position it.
							inlineItem item;
							item.k = inlineItem::kind::block;
							item.node = child;
							const rect r = layoutBlockLevel(child, available_,
								0.0f, 0.0f, true, 0.0f, false);
							item.width = r.w;
							items.push_back(item);
							continue;
						}
						collectInline(child, items, depth + 1);
					}
				}

				/** Break `items` into lines; returns the y after the last one. */
				float layoutInlineGroup(int node, vector<inlineItem>& items,
					float x, float y, float availableWidth) {
					layoutBox& b = box(node);
					const computedStyle& s = style(node);
					const bool wrap = s.whiteSpace != whiteSpaceType::nowrap;

					struct placed {
						int item = 0;  // index into `items`
						float x = 0.0f;
					};
					vector<placed> line;
					float lineWidth = 0.0f;
					float lineTop = y;
					bool lineHasContent = false;

					auto commitLine = [&]() {
						if (line.empty()) return;
						float ascent = 0.0f, descent = 0.0f, lineHeight = 0.0f;
						for (const placed& p : line) {
							const inlineItem& item = items[(size_t)p.item];
							if (item.isSpace) continue;
							lineHeight = std::max(lineHeight,
								lineHeightPx(style(item.node), fontOf(item.node)));
							if (ctx_.fonts) {
								const fontMetrics fm = ctx_.fonts->metrics(
									style(item.node).fontFamily,
									fontOf(item.node),
									style(item.node).fontWeight,
									style(item.node).italic);
								ascent = std::max(ascent, fm.ascent);
								descent = std::max(descent, fm.descent);
							}
						}
						if (lineHeight <= 0.0f) lineHeight = ascent + descent;
						const float baseline = lineTop +
												   (lineHeight - (ascent + descent)) *
													   0.5f +
												   ascent;

						lineBox lb;
						lb.x = x;
						lb.y = lineTop;
						lb.w = lineWidth;
						lb.h = lineHeight;
						lb.baseline = baseline;
						b.lines.push_back(lb);

						// text-align
						const float slack = availableWidth - lineWidth;
						if (slack > 0.5f) {
							float offset = 0.0f;
							if (s.textAlign == textAlignType::center)
								offset = slack * 0.5f;
							else if (s.textAlign == textAlignType::right)
								offset = slack;
							for (placed& p : line) p.x += offset;
							if (s.textAlign == textAlignType::justify &&
								line.size() > 1) {
								int gaps = 0;
								for (const placed& p : line)
									if (items[(size_t)p.item].isSpace) gaps++;
								if (gaps > 0) {
									const float extra = slack / (float)gaps;
									for (placed& p : line)
										if (items[(size_t)p.item].isSpace)
											p.x += extra;
								}
							}
						}

						for (const placed& p : line) {
							inlineItem& item = items[(size_t)p.item];
							if (item.isSpace) continue;
							if (item.k == inlineItem::kind::word) {
								emitRun(item.node, item.text, p.x, baseline,
									item.width, node);
							} else {
								placeAtomic(item.node, p.x, baseline);
							}
						}
						line.clear();
						lineWidth = 0.0f;
						lineTop = y;
						lineHasContent = false;
					};

					for (size_t i = 0; i < items.size(); i++) {
						const inlineItem& item = items[i];
						const bool overflows = wrap && lineHasContent &&
											   !item.isSpace &&
											   lineWidth + item.width >
													   availableWidth + 0.01f;
						if (overflows) {
							// Trailing spaces stay on the finished line.
							while (!line.empty() &&
								   items[(size_t)line.back().item].isSpace) {
								lineWidth -= items[(size_t)line.back().item].width;
								line.pop_back();
							}
							commitLine();
						}
						placed p;
						p.item = (int)i;
						p.x = x + lineWidth;
						line.push_back(p);
						lineWidth += item.width;
						if (!item.isSpace) lineHasContent = true;
					}
					commitLine();

					if (!b.lines.empty()) {
						const lineBox& last = b.lines.back();
						return last.y + last.h;
					}
					return y;
				}

				/** Shift a laid-out atomic box onto its line box. */
				void placeAtomic(int node, float x, float baseline) {
					rect before = box(node).border;
					const computedStyle& as = style(node);
					float top = baseline - before.h;  // bottom on the baseline
					switch (as.verticalAlign) {
						case alignType::center: top = baseline - before.h * 0.5f; break;
						case alignType::start: top = baseline - before.h; break;
						case alignType::end: top = baseline; break;
						default: break;
					}
					translateSubtree(node, x - before.x, top - before.y);
				}

				void translateSubtree(int node, float dx, float dy) {
					if (dx == 0.0f && dy == 0.0f) return;
					layoutBox& b = box(node);
					b.border.x += dx;
					b.border.y += dy;
					b.content.x += dx;
					b.content.y += dy;
					for (textRun& run : b.runs) {
						run.x += dx;
						run.y += dy;
					}
					for (lineBox& line : b.lines) {
						line.x += dx;
						line.y += dy;
					}
					if (b.hasMarker) {
						b.marker.x += dx;
						b.marker.y += dy;
					}
					for (int child : b.children) translateSubtree(child, dx, dy);
				}

				void emitRun(int owner, const string& text, float x,
					float baseline, float width, int container) {
					const computedStyle& s = style(owner);
					textRun run;
					run.node = owner;
					run.x = x;
					run.y = baseline;
					run.width = width;
					run.text = text;
					run.c = s.textColor;
					run.family = s.fontFamily;
					run.size = fontOf(owner);
					run.weight = s.fontWeight;
					run.italic = s.italic;
					run.letterSpacing =
						resolve(s.letterSpacing, 0.0f, fontOf(owner),
							ctx_.rootFontSize, ctx_);
					run.underline = s.underline;
					run.lineThrough = s.lineThrough;
					box(container).runs.push_back(run);
				}

				float measure(const string& text, int node) {
					if (!ctx_.fonts || text.empty()) return 0.0f;
					const computedStyle& s = style(node);
					return ctx_.fonts->measureWidth(text, s.fontFamily,
						fontOf(node), s.fontWeight, s.italic);
				}

				/**
				 * `list-style-type` markers are painted outside the content
				 * box, aligned to the first line's baseline.
				 */
				void layoutMarker(int node, const rect& content) {
					const computedStyle& s = style(node);
					if (s.listStyle == listStyleType::none_) return;
					layoutBox& b = box(node);
					string text;
					switch (s.listStyle) {
						case listStyleType::disc: text = "\xE2\x80\xA2"; break;
						case listStyleType::circle: text = "\xE2\x80\xA3"; break;
						case listStyleType::square: text = "\xE2\x96\xA0"; break;
						case listStyleType::decimal: {
							// Count preceding siblings with the same marker.
							int ordinal = 1;
							const int parent = tree_.nodes[(size_t)node].parent;
							for (int sibling :
								 tree_.nodes[(size_t)parent].children) {
								if (sibling == node) break;
								if (tree_.nodes[(size_t)sibling].isText) continue;
								if (style(sibling).listStyle ==
									listStyleType::decimal)
									ordinal++;
							}
							text = std::to_string(ordinal) + ".";
							break;
						}
						default: return;
					}
					const float width = measure(text, node);
					const float baseline = b.lines.empty()
											   ? content.y + fontOf(node)
												 : b.lines[0].baseline;
					// Markers hang to the left of the content box; when there is
					// no room (a list flush against the viewport edge) they are
					// drawn inside it instead.
					const float hang = content.x - width - fontOf(node) * 0.4f;
					b.hasMarker = true;
					b.marker = textRun();
					b.marker.node = node;
					b.marker.text = text;
					b.marker.x = std::max(0.0f, hang);
					b.marker.y = baseline;
					b.marker.width = width;
					b.marker.c = s.textColor;
					b.marker.family = s.fontFamily;
					b.marker.size = fontOf(node);
					b.marker.weight = s.fontWeight;
					b.marker.italic = s.italic;
				}

				// ------------------------------------------- position/offset

				void applyRelativeOffsets() {
					for (size_t i = 0; i < tree_.nodes.size(); i++) {
						if (tree_.nodes[i].isText) continue;
						const int node = (int)i;
						const computedStyle& s = style(node);
						if (s.position != positionType::relative) continue;
						// `left`/`right` and `top`/`bottom` offset from the
						// static position; the used one wins.
						float dx = 0.0f, dy = 0.0f;
						if (s.left.defined() && !s.left.isAuto())
							dx = len(node, s.left, box(node).border.w);
						else if (s.right.defined() && !s.right.isAuto())
							dx = -len(node, s.right, box(node).border.w);
						if (s.top.defined() && !s.top.isAuto())
							dy = len(node, s.top, box(node).border.h);
						else if (s.bottom.defined() && !s.bottom.isAuto())
							dy = -len(node, s.bottom, box(node).border.h);
						translateSubtree(node, dx, dy);
					}
				}

				void layoutOutOfFlow(int node, const rect& content,
					float innerWidth) {
					const rect before = box(node).border;
					rect r = layoutBlockLevel(node, innerWidth, content.x,
						content.y, true, 0.0f, false);
					const computedStyle& s = style(node);
					if (s.left.defined() && !s.left.isAuto())
						r.x = content.x + len(node, s.left, content.w);
					else if (s.right.defined() && !s.right.isAuto())
						r.x = content.x + content.w - r.w -
							  len(node, s.right, content.w);
					if (s.top.defined() && !s.top.isAuto())
						r.y = content.y + len(node, s.top, content.h);
					else if (s.bottom.defined() && !s.bottom.isAuto())
						r.y = content.y + content.h - r.h -
							  len(node, s.bottom, content.h);
					translateSubtree(node, r.x - before.x, r.y - before.y);
				}

				float available_ = 0.0f;
				const domTree& tree_;
				const vector<computedStyle>& styles_;
				const layoutContext& ctx_;
				layoutResult& out_;
				vector<float> fontSize_;
			};

		}  // namespace

		void layoutTree(const domTree& tree,
			const vector<computedStyle>& styles, const layoutContext& ctx,
			layoutResult& out) {
			layoutEngine engine(tree, styles, ctx, out);
			engine.run();
		}

	}  // namespace UI
}  // namespace gold
