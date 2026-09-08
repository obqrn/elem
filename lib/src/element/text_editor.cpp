/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements/element/text_editor.hpp>
#include <elements/support/context.hpp>
#include <elements/support/pixmap.hpp>
#include <elements/support/theme.hpp>
#include <elements/support/text_utils.hpp>
#include <elements/view.hpp>
#include <infra/assert.hpp>
#include <algorithm>
#include <cmath>

namespace cycfi::elements
{
   namespace
   {
      // Block spacing in logical pixels: a larger gap before headings.
      constexpr float block_gap = 8.0f;
      constexpr float heading_gap = 12.0f;

      // The font for a block type.
      font_descr block_font(block_type t)
      {
         auto const& thm = get_theme();
         switch (t)
         {
            case block_type::heading1: return thm.heading_font.size(24).bold();
            case block_type::heading2: return thm.heading_font.size(20).bold();
            case block_type::heading3: return thm.heading_font.size(17).bold();
            case block_type::heading4: return thm.heading_font.size(15).bold();
            case block_type::heading5: return thm.heading_font.size(13).bold();
            case block_type::heading6: return thm.heading_font.size(12).bold();
            default:                   return thm.text_box_font;
         }
      }

      bool is_heading(block_type t)
      {
         return t >= block_type::heading1 && t <= block_type::heading6;
      }

      float gap_before(block_type t)
      {
         return is_heading(t)? heading_gap : block_gap;
      }

      bool same_font_descr(font_descr const& a, font_descr const& b)
      {
         return a._families == b._families
            && a._size == b._size
            && a._weight == b._weight
            && a._slant == b._slant
            && a._stretch == b._stretch;
      }

      bool is_default_font_descr(font_descr const& f)
      {
         return f._families.empty()
            && f._size == 12
            && f._weight == font_constants::weight_normal
            && f._slant == font_constants::slant_normal
            && f._stretch == font_constants::stretch_normal;
      }

      font_descr effective_font(font_descr const& run_font,
         font_descr block_font_)
      {
         if (!run_font._families.empty() || is_default_font_descr(run_font))
            return run_font._families.empty()? block_font_ : run_font;

         // An empty family is an inherited family with inline overrides.
         // Keep the block's family and apply the run's explicit attributes.
         return block_font_.size(run_font._size)
            .weight(static_cast<font_constants::weight_enum>(run_font._weight))
            .style(static_cast<font_constants::slant_enum>(run_font._slant))
            .stretch(static_cast<font_constants::stretch_enum>(run_font._stretch));
      }

      std::vector<text_span> layout_spans(text_block const& block,
         font_descr block_font_, color default_color)
      {
         std::vector<text_span> spans;
         spans.reserve(std::max<std::size_t>(block.spans.size(), 1));
         for (auto const& span : block.spans)
         {
            auto font = effective_font(span.font_, block_font_);
            auto color_ = span.color_.alpha == 0?
               default_color : span.color_;
            spans.push_back({span.text, font, color_});
         }
         if (spans.empty())
            spans.push_back({{}, block_font_, default_color});
         return spans;
      }

      std::string text_range(text_block const& block, std::size_t first,
         std::size_t last)
      {
         std::string result;
         result.reserve(last - first);
         std::size_t base = 0;
         for (auto const& span : block.spans)
         {
            auto span_last = base + span.text.size();
            auto begin = std::max(first, base);
            auto end = std::min(last, span_last);
            if (begin < end)
               result.append(span.text, begin - base, end - begin);
            base = span_last;
         }
         return result;
      }

      bool style_differs(text_block const& block, std::size_t first,
         std::size_t last, text_style const& style)
      {
         if (first == last)
            return false;
         std::size_t base = 0;
         for (auto const& span : block.spans)
         {
            auto span_last = base + span.text.size();
            auto overlap_first = std::max(first, base);
            auto overlap_last = std::min(last, span_last);
            if (overlap_first < overlap_last && span.style() != style)
               return true;
            base = span_last;
         }
         return false;
      }

      // Union of several rects; empty rects are skipped.
      rect union_of(rect a, rect b)
      {
         if (a.is_empty())
            return b;
         if (b.is_empty())
            return a;
         return rect{
            std::min(a.left, b.left), std::min(a.top, b.top),
            std::max(a.right, b.right), std::max(a.bottom, b.bottom)};
      }

      rect union_of(rect a, rect b, rect c)
      {
         return union_of(union_of(a, b), c);
      }

      rect union_of(rect a, rect b, rect c, rect d)
      {
         return union_of(union_of(a, b, c), d);
      }

      // UTF-8 encode a single codepoint; returns the byte count.
      int encode_utf8(unsigned cp, char out[4])
      {
         if (cp < 0x80)
         {
            out[0] = char(cp);
            return 1;
         }
         if (cp < 0x800)
         {
            out[0] = char(0xC0 | (cp >> 6));
            out[1] = char(0x80 | (cp & 0x3F));
            return 2;
         }
         if (cp < 0x10000)
         {
            out[0] = char(0xE0 | (cp >> 12));
            out[1] = char(0x80 | ((cp >> 6) & 0x3F));
            out[2] = char(0x80 | (cp & 0x3F));
            return 3;
         }
         out[0] = char(0xF0 | (cp >> 18));
         out[1] = char(0x80 | ((cp >> 12) & 0x3F));
         out[2] = char(0x80 | ((cp >> 6) & 0x3F));
         out[3] = char(0x80 | (cp & 0x3F));
         return 4;
      }

   }

   text_editor_element::text_editor_element(
      std::shared_ptr<text_document> doc, float width)
    : _doc(std::move(doc))
    , _width(width)
   {
      CYCFI_ASSERT(_doc, "text_editor_element requires a document");
   }

   ////////////////////////////////////////////////////////////////////////////
   // Layout
   ////////////////////////////////////////////////////////////////////////////
   void text_editor_element::ensure_layouts() const
   {
      if (_layouts_valid)
      {
         auto const& thm = get_theme();
         if (_layout_theme_valid &&
             same_font_descr(thm.text_box_font, _layout_text_font) &&
             same_font_descr(thm.heading_font, _layout_heading_font))
            return;
         _layouts_valid = false;
      }

      auto width = _width;
      auto const& blocks = _doc->blocks();
      auto const& thm = get_theme();

      _layouts.clear();
      _layouts.reserve(blocks.size());
      for (auto const& b : blocks)
      {
         auto font = block_font(b.type);
         rich_text_layout rl(layout_spans(b, font, thm.label_font_color));
         rl.layout(width);
         _layouts.push_back(std::move(rl));
      }

      recompute_block_y();
      _cache_valid.assign(_layouts.size(), false);
      _block_cache.clear();
      _block_cache.resize(_layouts.size());
      _layout_text_font = thm.text_box_font;
      _layout_heading_font = thm.heading_font;
      _layout_theme_valid = true;
      _cache_scale = {};
      _cache_text_color = {};
      _cache_theme_valid = false;
      _layouts_valid = true;
   }

   void text_editor_element::relayout_blocks(std::size_t first, std::size_t last) const
   {
      // Structural edits (split/join) change the block count: the cached
      // layouts no longer line up with the document. Fall back to a full
      // relayout (structural edits are not keystroke-hot).
      if (_layouts.size() != _doc->size())
      {
         _layouts_valid = false;
         ensure_layouts();
         return;
      }
      CYCFI_ASSERT(first < _layouts.size(), "Block index out of range");
      last = std::min(last, _layouts.size());
      auto width = _width;
      auto const& blocks = _doc->blocks();
      auto const& thm = get_theme();

      for (auto i = first; i != last; ++i)
      {
         auto font = block_font(blocks[i].type);
         rich_text_layout rl(layout_spans(
            blocks[i], font, thm.label_font_color));
         rl.layout(width);
         _layouts[i] = std::move(rl);
         _cache_valid[i] = false;
      }
      recompute_block_y();
   }

   void text_editor_element::recompute_block_y() const
   {
      _block_y.clear();
      _block_y.reserve(_layouts.size() + 1);
      _content_width = 0;
      float y = 0;
      auto const& blocks = _doc->blocks();
      for (std::size_t i = 0; i != _layouts.size(); ++i)
      {
         _content_width = std::max(_content_width, _layouts[i].size().x);
         for (auto const& line : _layouts[i].lines())
            _content_width = std::max(_content_width, line.caret_width);
         _block_y.push_back(y + gap_before(blocks[i].type));
         y = _block_y.back() + _layouts[i].size().y;
      }
      _block_y.push_back(y); // total height sentinel
   }

   view_limits text_editor_element::limits(basic_context const& /* ctx */) const
   {
      ensure_layouts();
      // A fixed wrap width is also the editor's usable minimum width. Using
      // only the current longest line makes short documents collapse inside
      // an htile, so clicks in the editor's blank area never reach it.
      float min_x = (_width == full_extent)? 0 : _width;
      min_x = std::max(min_x, _content_width);
      return {{min_x, _block_y.back()}, {full_extent, _block_y.back()}};
   }

   void text_editor_element::layout(context const& ctx)
   {
      // Wrap width stays at the constructor value. Following the assigned
      // width here made a stretched container (e.g. htile without a
      // scroller) re-wrap the whole document to the container width,
      // pushing the content and the caret far outside the window.
      (void)ctx;
      ensure_layouts();
   }

   void text_editor_element::refresh_layouts()
   {
      _layouts_valid = false;
      _layout_theme_valid = false;
      _cache_theme_valid = false;
      // The document may have changed outside this editor; its document
      // undo stack can no longer be paired reliably with local caret states.
      _edit_history.clear();
      _redo_history.clear();
      clamp_caret();
   }

   void text_editor_element::apply_style(context const& ctx, text_style style)
   {
      if (!_has_selection)
         return;

      ensure_layouts();
      clamp_caret();
      auto first = sel_min();
      auto last = sel_max();
      bool changed = false;
      auto const& blocks = _doc->blocks();
      for (auto i = first.block; i <= last.block; ++i)
      {
         auto begin = i == first.block? first.offset : 0;
         auto end = i == last.block? last.offset : blocks[i].size();
         if (style_differs(blocks[i], begin, end, style))
         {
            changed = true;
            break;
         }
      }
      if (!changed)
         return;

      auto before = capture_caret();
      auto old_caret = caret_rect();
      auto old_sel = selection_bounds();
      auto anchor_before = _block_y[last.block + 1];
      auto [affected_first, affected_last] =
         _doc->set_style(first, last, style);
      remember_edit(before);
      if (_layouts_valid)
         relayout_blocks(affected_first, affected_last);
      repaint_edit(ctx, old_caret, old_sel,
         affected_first, affected_last, anchor_before);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Drawing
   ////////////////////////////////////////////////////////////////////////////
   void text_editor_element::draw(context const& ctx)
   {
      ensure_layouts();
      auto& cnv = ctx.canvas;

      auto text_color = get_theme().label_font_color;
      if (_cache_theme_valid && _cache_text_color != text_color)
      {
         // The color is stored in each layout span. Rebuilding only the
         // pixmap would keep drawing the old color into the new cache.
         _layouts_valid = false;
         ensure_layouts();
      }
      if (!_cache_theme_valid || _cache_text_color != text_color)
      {
         std::fill(_cache_valid.begin(), _cache_valid.end(), false);
         _cache_text_color = text_color;
         _cache_theme_valid = true;
      }

      // Local (content) coordinates; the bounds top-left is the origin.
      float view_h = ctx.bounds.height();
      float origin_x = ctx.bounds.left;
      float origin_y = ctx.bounds.top;

      // Blocks, culled to the visible range. Each block is rendered into
      // a cached pixmap on first draw (and after relayout); subsequent
      // repaints just blit the pixmap.
      auto device_scale = cnv.device_scale();
      if (std::abs(device_scale.x - _cache_scale.x) > 0.001f ||
          std::abs(device_scale.y - _cache_scale.y) > 0.001f)
      {
         std::fill(_cache_valid.begin(), _cache_valid.end(), false);
         _cache_scale = device_scale;
      }
      for (std::size_t i = 0; i != _doc->size(); ++i)
      {
         float y0 = _block_y[i];
         float y1 = y0 + _layouts[i].size().y;
         if (y0 > view_h)
            break;
         if (y1 < 0)
            continue;
         if (i >= _cache_valid.size() || !_cache_valid[i])
         {
            auto& rl = _layouts[i];
            auto sz = rl.size();
            if (sz.x > 0 && sz.y > 0)
            {
               if (i >= _block_cache.size())
                  _block_cache.resize(i + 1);
               // Physical pixels must be size * device_scale, with
               // device_scale 1: the older form (logical size + scale)
               // rasterized text at 1/scale the density and the blit
               // magnified it, blurring the glyphs.
               _block_cache[i] = std::make_unique<pixmap>(
                  point{
                     std::ceil(sz.x * device_scale.x),
                     std::ceil(sz.y * device_scale.y)
                  }, 1.0f);
               pixmap_context pc(*_block_cache[i]);
               canvas c2{*pc.context()};
               c2.scale(device_scale);
               rl.draw(c2, {0, 0});
               if (i >= _cache_valid.size())
                  _cache_valid.resize(i + 1, false);
               _cache_valid[i] = true;
            }
         }
         if (i < _cache_valid.size() && _cache_valid[i])
         {
            auto sz = _layouts[i].size();
            cnv.draw(*_block_cache[i],
               rect{0, 0,
                    sz.x * device_scale.x, sz.y * device_scale.y},
               rect{origin_x, origin_y + y0,
                    origin_x + sz.x, origin_y + y0 + sz.y}, false);
         }
         else
            _layouts[i].draw(cnv, {origin_x, origin_y + y0});
      }

      // Selection highlight, per block (line-precise within the block).
      // Drawn after the block pixmaps: the pixmaps are opaque, so a
      // highlight painted before them would be invisible. The highlight
      // blends over the glyphs (like editor selections do).
      if (_has_selection)
      {
         auto first = sel_min();
         auto last = sel_max();

         for (std::size_t i = first.block; i <= last.block && i < _doc->size(); ++i)
         {
            float y0 = _block_y[i];
            float y1 = y0 + _layouts[i].size().y;
            if (y0 > view_h)
               break;
            if (y1 < 0)
               continue;

            // Byte range of this block within the selection.
            std::size_t b0 = (i == first.block)? first.offset : 0;
            std::size_t b1 = (i == last.block)? last.offset :
               _doc->blocks()[i].size();
            if (b0 == b1)
               continue;

            auto& rl = _layouts[i];
            float y = y0;
            for (std::size_t li = 0; li != rl.lines().size(); ++li)
            {
               auto const& l = rl.lines()[li];
               float h = l.ascent + l.descent;
               auto range = rl.line_range(li);
               auto sel0 = std::max(b0, range.first);
               auto sel1 = std::min(b1, range.second);
               if (sel0 < sel1)
               {
                  auto x0 = (sel0 == range.first)? 0.0f : rl.x_at(sel0);
                  auto x1 = (sel1 == range.second)? l.caret_width :
                     rl.x_at(sel1);
                  if (l.segments.empty())
                     x1 = std::max(x1, x0 + 2.0f);
                  if (x1 < x0)
                     std::swap(x0, x1);
                  cnv.fill_style(get_theme().indicator_color.opacity(0.3));
                  cnv.fill_rect({origin_x + x0, origin_y + y,
                     origin_x + x1, origin_y + y + h});
               }
               y += h;
            }
         }
      }

      // Caret
      if (_is_focus)
      {
         auto pos = _layouts[_caret.block].caret_pos(_caret.offset);
         float x = origin_x + pos.x;
         float y0 = _block_y[_caret.block] + pos.y;
         float h = caret_line_height(_caret);
         cnv.fill_style(get_theme().indicator_color);
         cnv.fill_rect({x - 0.5f, origin_y + y0, x + 1.5f, origin_y + y0 + h});
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // Hit testing and caret movement
   ////////////////////////////////////////////////////////////////////////////
   text_editor_element::position
   text_editor_element::position_at(point p) const
   {
      ensure_layouts();
      if (_doc->empty())
         return {};

      // Find the block: binary search on _block_y.
      auto it = std::upper_bound(_block_y.begin(), _block_y.end(), p.y);
      std::size_t bi = (it == _block_y.begin())? 0 : std::size_t(it - _block_y.begin()) - 1;
      bi = std::min(bi, _doc->size() - 1);

      auto off = _layouts[bi].byte_at({p.x, p.y - _block_y[bi]});
      return _doc->clamp({bi, off});
   }

   text_editor_element::position
   text_editor_element::move_left(position p) const
   {
      auto const& blocks = _doc->blocks();
      if (p.offset > 0)
      {
         // Step back one UTF-8 character.
         auto const& t = blocks[p.block].spans;
         auto off = p.offset - 1;
         while (off > 0)
         {
            std::size_t base = 0;
            bool continuation = false;
            for (auto const& span : t)
            {
               if (off < base + span.text.size())
               {
                  continuation =
                     (uint8_t(span.text[off - base]) & 0xC0) == 0x80;
                  break;
               }
               base += span.text.size();
            }
            if (!continuation)
               break;
            --off;
         }
         return {p.block, off};
      }
      if (p.block > 0)
         return {p.block - 1, blocks[p.block - 1].size()};
      return p;
   }

   text_editor_element::position
   text_editor_element::move_right(position p) const
   {
      auto const& blocks = _doc->blocks();
      auto const& t = blocks[p.block].spans;
      auto length = blocks[p.block].size();
      if (p.offset < length)
      {
         // Step forward one UTF-8 character.
         auto off = p.offset + 1;
         while (off < length)
         {
            std::size_t base = 0;
            bool continuation = false;
            for (auto const& span : t)
            {
               if (off < base + span.text.size())
               {
                  continuation =
                     (uint8_t(span.text[off - base]) & 0xC0) == 0x80;
                  break;
               }
               base += span.text.size();
            }
            if (!continuation)
               break;
            ++off;
         }
         return {p.block, off};
      }
      if (p.block + 1 < blocks.size())
         return {p.block + 1, 0};
      return p;
   }

   text_editor_element::position
   text_editor_element::move_vertical(context const& ctx, position p, int dir) const
   {
      (void)ctx;
      ensure_layouts();
      auto& rl = _layouts[p.block];
      auto li = rl.line_at(p.offset);
      auto li_count = rl.lines().size();
      auto target = int(li) + dir;

      if (target >= 0 && target < int(li_count))
      {
         // Same block, adjacent line.
         auto pos = rl.caret_pos(p.offset);
         if (_desired_x < 0)
            _desired_x = pos.x;
         auto y = [&]() {
            float acc = 0;
            for (std::size_t k = 0; k != std::size_t(target); ++k)
               acc += rl.lines()[k].ascent + rl.lines()[k].descent;
            return acc + rl.lines()[std::size_t(target)].ascent * 0.5f;
         }();
         auto off = rl.byte_at({_desired_x, y});
         return _doc->clamp({p.block, off});
      }

      // Cross-block vertical move.
      if (dir < 0 && p.block > 0)
      {
         auto& rl2 = _layouts[p.block - 1];
         if (_desired_x < 0)
            _desired_x = rl.caret_pos(p.offset).x;
         auto off = rl2.byte_at({_desired_x, rl2.size().y - 1});
         return _doc->clamp({p.block - 1, off});
      }
      if (dir > 0 && p.block + 1 < _doc->size())
      {
         auto& rl2 = _layouts[p.block + 1];
         if (_desired_x < 0)
            _desired_x = rl.caret_pos(p.offset).x;
         auto off = rl2.byte_at({_desired_x, 0});
         return _doc->clamp({p.block + 1, off});
      }
      return p;
   }

   void text_editor_element::clamp_caret()
   {
      _caret = _doc->clamp(_caret);
      _anchor = _doc->clamp(_anchor);
      if (_caret == _anchor)
         _has_selection = false;
   }

   text_editor_element::caret_state
   text_editor_element::capture_caret() const
   {
      return {_caret, _anchor, _has_selection};
   }

   void text_editor_element::restore_caret(caret_state state)
   {
      _caret = state.caret;
      _anchor = state.anchor;
      _has_selection = state.has_selection;
      _desired_x = -1;
      clamp_caret();
   }

   void text_editor_element::remember_edit(caret_state before)
   {
      _edit_history.push_back({before, capture_caret()});
      _redo_history.clear();
      if (_edit_history.size() > 1000)
         _edit_history.erase(_edit_history.begin());
   }

   text_editor_element::position text_editor_element::sel_min() const
   {
      return (_anchor < _caret)? _anchor : _caret;
   }

   text_editor_element::position text_editor_element::sel_max() const
   {
      return (_anchor < _caret)? _caret : _anchor;
   }

   float text_editor_element::caret_line_height(position p) const
   {
      auto const& rl = _layouts[p.block];
      auto li = rl.line_at(p.offset);
      if (li < rl.lines().size())
      {
         auto const& l = rl.lines()[li];
         if (l.ascent + l.descent > 0)
            return l.ascent + l.descent;
      }
      // Empty block (or past the last line): no layout line carries the
      // metrics, approximate with the block font size. The refresh rect is
      // only ever slightly over-sized.
      auto f = block_font(_doc->blocks()[p.block].type);
      return f._size * 1.4f;
   }

   rect text_editor_element::caret_rect() const
   {
      if (_layouts.empty() || _block_y.empty())
         return {};
      auto pos = _layouts[_caret.block].caret_pos(_caret.offset);
      float y = _block_y[_caret.block] + pos.y;
      float h = caret_line_height(_caret);
      return {pos.x - 1, y, pos.x + 2, y + h};
   }

   rect text_editor_element::selection_bounds() const
   {
      if (!_has_selection || _layouts.empty() || _block_y.empty())
         return {};
      auto a = _layouts[sel_min().block].caret_pos(sel_min().offset);
      auto b = _layouts[sel_max().block].caret_pos(sel_max().offset);
      float ya = _block_y[sel_min().block] + a.y;
      float yb = _block_y[sel_max().block] + b.y;
      auto first = sel_min();
      auto last = sel_max();
      auto first_line = _layouts[first.block].line_at(first.offset);
      auto last_line = _layouts[last.block].line_at(last.offset);
      if (first.block != last.block || first_line != last_line)
      {
         // Selection drawing can cover every line between the endpoints. A
         // narrow endpoint rectangle would leave stale highlights when the
         // caret moves across a wrapped line or another block.
         return {0, std::min(ya, yb), _content_width,
            std::max(ya + caret_line_height(first),
               yb + caret_line_height(last))};
      }
      return {
         std::min(a.x, b.x), std::min(ya, yb),
         std::max(a.x, b.x),
         std::max(ya + caret_line_height(sel_min()),
                  yb + caret_line_height(sel_max()))};
   }

   rect text_editor_element::block_rect(std::size_t i) const
   {
      if (i >= _layouts.size() || i >= _block_y.size() - 1)
         return {};
      auto sz = _layouts[i].size();
      return {0, _block_y[i], std::max(sz.x, 0.0f), _block_y[i] + sz.y};
   }

   void text_editor_element::repaint_caret(
      context const& ctx, rect old_caret, rect old_sel) const
   {
      auto u = union_of(old_caret, old_sel, caret_rect(), selection_bounds());
      if (u.is_empty())
         return;
      u = u.move_to(u.left + ctx.bounds.left, u.top + ctx.bounds.top);
      ctx.view.refresh(ctx, u);
   }

   void text_editor_element::repaint_edit(
      context const& ctx, rect old_caret, rect old_sel
    , std::size_t first, std::size_t last, float anchor_before) const
   {
      auto u = union_of(old_caret, old_sel, caret_rect(), selection_bounds());
      for (auto i = first; i != last; ++i)
         u = union_of(u, block_rect(i));
      // If the bottom of the affected range moved, every block below it
      // shifted: extend the repaint to the content bottom.
      float bottom = (last < _block_y.size())? _block_y[last] : _block_y.back();
      if (bottom != anchor_before)
      {
         u = union_of(u, rect{0, std::min(anchor_before, bottom),
            _content_width, _block_y.back()});
      }
      // A shorter edit can leave old glyphs to the right of the new line,
      // even when the block height is unchanged. Repaint the complete
      // affected vertical band, including the caret's one-pixel left edge.
      if (!u.is_empty())
         u = union_of(u, rect{-2, u.top, ctx.bounds.width() + 2, u.bottom});
      if (u.is_empty())
         return;
      u = u.move_to(u.left + ctx.bounds.left, u.top + ctx.bounds.top);
      ctx.view.refresh(ctx, u);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Editing
   ////////////////////////////////////////////////////////////////////////////
   void text_editor_element::insert_text(context const& ctx, string_view s)
   {
      if (s.empty())
         return;

      ensure_layouts();
      clamp_caret();

      auto before = capture_caret();
      auto old_caret = caret_rect();
      auto old_sel = selection_bounds();
      auto mn = sel_min();
      auto mx = sel_max();

      // The repaint must cover everything from the top of the first
      // touched block to the pre-edit bottom of the last touched one.
      float anchor_before = _block_y[mx.block + 1];

      // Keep the untouched suffix length. The document normalizes CRLF while
      // inserting, so deriving the caret from the input byte count is wrong.
      auto tail_size = _doc->blocks()[mx.block].size() - mx.offset;

      // The insert lands where the selection started (or the old caret).
      position at = _doc->clamp(mn);
      _desired_x = -1;

      auto [first, last] = _has_selection?
         _doc->replace(mn, mx, s) : _doc->insert(at, s);

      // The caret lands at the end of the inserted text: after the last
      // byte of a single-block insert, or at the end of the last inserted
      // line (which is the start of the next line when the text ends with
      // a newline, e.g. Enter).
      auto caret_block = last - 1;
      auto caret_offset = _doc->blocks()[caret_block].size() - tail_size;
      _caret = _doc->clamp({caret_block, caret_offset});
      _anchor = _caret;
      _has_selection = false;
      remember_edit(before);

      if (_layouts_valid)
         relayout_blocks(first, last);
      repaint_edit(ctx, old_caret, old_sel, first, last, anchor_before);
   }

   void text_editor_element::erase_selection(context const& ctx)
   {
      if (!_has_selection)
         return;

      ensure_layouts();
      clamp_caret();

      auto before = capture_caret();
      auto old_caret = caret_rect();
      auto old_sel = selection_bounds();
      auto mn = sel_min();
      auto mx = sel_max();
      float anchor_before = _block_y[mx.block + 1];

      auto [first, last] = _doc->erase(mn, mx);
      _caret = _doc->clamp(mn);
      _anchor = _caret;
      _has_selection = false;
      _desired_x = -1;
      remember_edit(before);

      if (_layouts_valid)
         relayout_blocks(first, last);
      repaint_edit(ctx, old_caret, old_sel, first, last, anchor_before);
   }

   std::string text_editor_element::selection_text() const
   {
      if (!_has_selection)
         return {};
      auto first = sel_min();
      auto last = sel_max();
      auto const& blocks = _doc->blocks();
      if (first.block == last.block)
         return text_range(blocks[first.block], first.offset, last.offset);
      std::string s = text_range(blocks[first.block], first.offset,
         blocks[first.block].size());
      for (auto i = first.block + 1; i != last.block; ++i)
      {
         s += '\n';
         s += blocks[i].plain_text();
      }
      s += '\n';
      s += text_range(blocks[last.block], 0, last.offset);
      return s;
   }

   void text_editor_element::select_all()
   {
      _anchor = {0, 0};
      _caret = {_doc->size() - 1, _doc->blocks().back().size()};
      _has_selection = (_caret != _anchor);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Events
   ////////////////////////////////////////////////////////////////////////////
   bool text_editor_element::click(context const& ctx, mouse_button btn)
   {
      if (btn.state != mouse_button::left)
         return false;

      // Button-up can arrive outside the editor after a drag because the
      // host captures the mouse. Always clear the pointer state first.
      if (!btn.down)
         return ctx.bounds.includes(btn.pos);

      if (!ctx.bounds.includes(btn.pos))
         return false;

      ensure_layouts();
      clamp_caret();

      auto old_caret = caret_rect();
      auto old_sel = selection_bounds();
      auto local = point{
         btn.pos.x - ctx.bounds.left, btn.pos.y - ctx.bounds.top};
      _caret = position_at(local);
      _anchor = _caret;
      _has_selection = false;
      _desired_x = -1;
      // Repaint the caret areas plus the old selection extent: a click
      // clears the selection, and without repainting it the highlight
      // lingers (the reported 'multiple carets').
      repaint_caret(ctx, old_caret, old_sel);
      return true;
   }

   void text_editor_element::drag(context const& ctx, mouse_button btn)
   {
      if (btn.state != mouse_button::left)
         return;

      ensure_layouts();
      clamp_caret();

      auto local = point{
         btn.pos.x - ctx.bounds.left, btn.pos.y - ctx.bounds.top};

      auto old_caret = caret_rect();
      auto old_sel = selection_bounds();
      _caret = position_at(local);
      _has_selection = (_caret != _anchor);
      _desired_x = -1;
      // Repaint only the caret and selection extents (old + new). A
      // click's microscopic mouse jitter used to trigger a drag with a
      // full-view repaint here - one of the click flicker sources.
      repaint_caret(ctx, old_caret, old_sel);
   }

   bool text_editor_element::cursor(context const&, point, cursor_tracking status)
   {
      if (status == cursor_tracking::leaving)
         return false;
      set_cursor(cursor_type::ibeam);
      return true;
   }

   bool text_editor_element::key(context const& ctx, key_info k)
   {
      if (k.action == key_action::release || k.action == key_action::unknown)
         return false;

      ensure_layouts();
      clamp_caret();

      auto ctrl = (k.modifiers & (mod_control | mod_action)) != 0;
      auto shift = (k.modifiers & mod_shift) != 0;

      if (ctrl)
      {
         switch (int(k.key))
         {
            case 'z':
            case 'Z':
               if (_doc->can_undo())
               {
                  _doc->undo();
                  _layouts_valid = false;
                  if (!_edit_history.empty())
                  {
                     auto edit = std::move(_edit_history.back());
                     _edit_history.pop_back();
                     restore_caret(edit.before);
                     _redo_history.push_back(std::move(edit));
                  }
                  else
                     clamp_caret();
                  // Undo can change the block count anywhere in the
                  // document; relayout everything and repaint the element.
                  ensure_layouts();
                  ctx.view.refresh(ctx);
               }
               return true;
            case 'y':
            case 'Y':
               if (_doc->can_redo())
               {
                  _doc->redo();
                  _layouts_valid = false;
                  if (!_redo_history.empty())
                  {
                     auto edit = std::move(_redo_history.back());
                     _redo_history.pop_back();
                     restore_caret(edit.after);
                     _edit_history.push_back(std::move(edit));
                  }
                  else
                     clamp_caret();
                  ensure_layouts();
                  ctx.view.refresh(ctx);
               }
               return true;
            case 'a':
            case 'A':
            {
               auto old_caret = caret_rect();
               auto old_sel = selection_bounds();
               select_all();
               repaint_caret(ctx, old_caret, old_sel);
               return true;
            }
            case 'c':
            case 'C':
               if (_has_selection)
                  clipboard(selection_text());
               return true;
            case 'x':
            case 'X':
               if (_has_selection)
               {
                  clipboard(selection_text());
                  erase_selection(ctx);
               }
               return true;
            case 'v':
            case 'V':
            {
               auto text = clipboard();
               insert_text(ctx, text);
               return true;
            }
            default:
               return false;
         }
      }

      switch (k.key)
      {
         case key_code::left:
         {
            auto old_caret = caret_rect();
            auto old_sel = selection_bounds();
            _caret = (!shift && _has_selection)? sel_min() : move_left(_caret);
            if (!shift)
            {
               _anchor = _caret;
               _has_selection = false;
            }
            else
               _has_selection = (_caret != _anchor);
            _desired_x = -1;
            repaint_caret(ctx, old_caret, old_sel);
            return true;
         }

         case key_code::right:
         {
            auto old_caret = caret_rect();
            auto old_sel = selection_bounds();
            _caret = (!shift && _has_selection)? sel_max() : move_right(_caret);
            if (!shift)
            {
               _anchor = _caret;
               _has_selection = false;
            }
            else
               _has_selection = (_caret != _anchor);
            _desired_x = -1;
            repaint_caret(ctx, old_caret, old_sel);
            return true;
         }

         case key_code::up:
         {
            auto old_caret = caret_rect();
            auto old_sel = selection_bounds();
            _caret = move_vertical(ctx, _caret, -1);
            if (!shift)
            {
               _anchor = _caret;
               _has_selection = false;
            }
            else
               _has_selection = (_caret != _anchor);
            repaint_caret(ctx, old_caret, old_sel);
            return true;
         }

         case key_code::down:
         {
            auto old_caret = caret_rect();
            auto old_sel = selection_bounds();
            _caret = move_vertical(ctx, _caret, 1);
            if (!shift)
            {
               _anchor = _caret;
               _has_selection = false;
            }
            else
               _has_selection = (_caret != _anchor);
            repaint_caret(ctx, old_caret, old_sel);
            return true;
         }

         case key_code::home:
         {
            auto old_caret = caret_rect();
            auto old_sel = selection_bounds();
            _caret = {_caret.block, 0};
            if (!shift)
            {
               _anchor = _caret;
               _has_selection = false;
            }
            else
               _has_selection = (_caret != _anchor);
            _desired_x = -1;
            repaint_caret(ctx, old_caret, old_sel);
            return true;
         }

         case key_code::end:
         {
            auto old_caret = caret_rect();
            auto old_sel = selection_bounds();
            _caret = {_caret.block, _doc->blocks()[_caret.block].size()};
            if (!shift)
            {
               _anchor = _caret;
               _has_selection = false;
            }
            else
               _has_selection = (_caret != _anchor);
            _desired_x = -1;
            repaint_caret(ctx, old_caret, old_sel);
            return true;
         }

         case key_code::backspace:
         {
            if (_has_selection)
            {
               erase_selection(ctx);
               return true;
            }
            auto p = move_left(_caret);
            if (p != _caret)
            {
               auto before = capture_caret();
               auto old_caret = caret_rect();
               auto old_sel = selection_bounds();
               float anchor_before = _block_y[_caret.block + 1];
               auto [first, last] = _doc->erase(p, _caret);
               _caret = _doc->clamp(p);
               _anchor = _caret;
               _desired_x = -1;
               remember_edit(before);
               if (_layouts_valid)
                  relayout_blocks(first, last);
               repaint_edit(ctx, old_caret, old_sel, first, last, anchor_before);
            }
            return true;
         }

         case key_code::_delete:
         {
            if (_has_selection)
            {
               erase_selection(ctx);
               return true;
            }
            auto p = move_right(_caret);
            if (p != _caret)
            {
               auto before = capture_caret();
               auto old_caret = caret_rect();
               auto old_sel = selection_bounds();
               float anchor_before = _block_y[_caret.block + 1];
               auto [first, last] = _doc->erase(_caret, p);
               _caret = _doc->clamp(_caret);
               _anchor = _caret;
               _desired_x = -1;
               remember_edit(before);
               if (_layouts_valid)
                  relayout_blocks(first, last);
               repaint_edit(ctx, old_caret, old_sel, first, last, anchor_before);
            }
            return true;
         }

         case key_code::enter:
         {
            // Inside a code block the document keeps the newline as a
            // literal character (no split); everywhere else it splits the
            // block. Either way this is a plain text insert.
            insert_text(ctx, "\n");
            return true;
         }

         case key_code::tab:
            return false;

         default:
            // Character input arrives through the text() event (the host
            // translates WM_CHAR/IME there). Keeping a second printable
            // path here would double-insert and mis-handle case/shift.
            return false;
         }
   }

   bool text_editor_element::text(context const& ctx, text_info info)
   {
      if (!_is_focus)
         return false;

      // Filter control codepoints: they must not enter block text (a
      // carriage return would render as a newline and break the block
      // invariant; tab has no semantics in this editor yet).
      //
      // IME input arrives here as committed characters (WM_CHAR): the
      // composition itself is rendered by the system's default IME window,
      // so typed CJK text appears after the commit. Inline composition
      // (preedit under the caret) is a stage 2b host-side extension.
      if (info.codepoint < 32 ||
          (info.codepoint >= 0x7F && info.codepoint <= 0x9F) ||
          info.codepoint > 0x10FFFF ||
          (info.codepoint >= 0xD800 && info.codepoint <= 0xDFFF))
         return false;

      char buf[4];
      auto n = encode_utf8(info.codepoint, buf);
      insert_text(ctx, string_view(buf, n));
      return true;
   }

   void text_editor_element::begin_focus(focus_request /* req */)
   {
      _is_focus = true;
   }

   bool text_editor_element::end_focus()
   {
      _is_focus = false;
      return true;
   }

   text_editor_element text_editor(
      std::shared_ptr<text_document> doc, float width)
   {
      return text_editor_element{std::move(doc), width};
   }
}
