/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements/element/rich_text.hpp>
#include <elements/support/context.hpp>
#include <elements/support/text_utils.hpp>
#include <elements/support/detail/scratch_context.hpp>
#include <infra/assert.hpp>
#include <algorithm>
#include <cairo.h>

namespace cycfi::elements
{
   namespace
   {
      detail::scratch_context scratch_context_;
   }

   void rich_text_layout::layout(float width)
   {
      CYCFI_ASSERT(width > 0 || width == full_extent,
         "Precondition failure: width must be positive or full_extent");

      _lines.clear();
      if (_spans.empty())
      {
         _size = {0, 0};
         return;
      }

      canvas cnv{*scratch_context_.context()};

      // Pre-compute per-span metrics: text width is measured on demand,
      // but the font metrics are needed per span for line height and the
      // baseline.
      struct span_metrics
      {
         float  ascent;
         float  descent;
         float  space_w;
      };
      std::vector<span_metrics> metrics;
      metrics.reserve(_spans.size());
      for (auto const& span : _spans)
      {
         cnv.font(span.font_, span.font_._size);
         auto fm = cnv.measure_font();
         metrics.push_back({fm.ascent, fm.descent, cnv.measure_text(" ").size.x});
      }

      line current;
      float x = 0;
      float max_width = 0;

      auto end_line = [&]
      {
         if (current.segments.empty())
         {
            x = 0;
            return;
         }
         current.width = x;
         max_width = std::max(max_width, x);
         _lines.push_back(std::move(current));
         current = {};
         x = 0;
      };

      // Append a segment for span `si` covering bytes [first, last) at the
      // current horizontal position, advancing the cursor.
      auto put_segment = [&](std::size_t si, std::size_t first, std::size_t last)
      {
         if (first == last)
            return;
         current.segments.push_back({si, first, last, x});
         current.ascent = std::max(current.ascent, metrics[si].ascent);
         current.descent = std::max(current.descent, metrics[si].descent);
         x += cnv.measure_text(std::string(
            _spans[si].text.data() + first, _spans[si].text.data() + last).c_str()).size.x;
      };

      // Place a word from span `si`, bytes [first, last). The word is put on
      // the current line when it fits (carrying its pending leading spaces);
      // if the word fits a full line but not the remainder of the current
      // one, the line ends first. An over-wide word is hard-broken character
      // by character.
      auto place_word = [&](std::size_t si, std::size_t first, std::size_t last,
         std::size_t space_span_, std::size_t space_start, bool pending_space)
      {
         auto word = std::string(
            _spans[si].text.data() + first, _spans[si].text.data() + last);
         float w = cnv.measure_text(word.c_str()).size.x;
         float space = pending_space ? metrics[space_span_].space_w : 0;

         if (x + space + w <= width)
         {
            // Fits on the current line, including pending leading spaces.
            // A space run inside the same span ends where the word begins;
            // one that survives a span boundary runs to the end of its own
            // span's text.
            if (pending_space)
            {
               std::size_t space_end = (space_span_ == si)
                  ? first : _spans[space_span_].text.size();
               put_segment(space_span_, space_start, space_end);
            }
            put_segment(si, first, last);
         }
         else if (w <= width)
         {
            // The word does not fit the remainder: end the line, strip
            // pending spaces.
            end_line();
            put_segment(si, first, last);
         }
         else
         {
            // Hard break: the word is wider than a full line. Split it
            // character by character (UTF-8 aware).
            if (!current.segments.empty())
               end_line();

            auto const& text = _spans[si].text;
            std::size_t pos = first;
            unsigned state = 0;
            unsigned cp = 0;
            while (pos < last)
            {
               std::size_t i = pos;
               do
               {
                  state = decode_utf8(state, cp, uint8_t(text[i++]));
               } while (state != 0);

               // `i` is the byte after the current character [pos, i)
               float cw = cnv.measure_text(std::string(
                  text.data() + pos, text.data() + i).c_str()).size.x;
               if (x > 0 && x + cw > width)
                  end_line();
               put_segment(si, pos, i);
               pos = i;
            }
         }
      };

      // Greedy token scan across all spans. Words are sequences of
      // non-whitespace characters; whitespace is kept pending and attached
      // to the word that follows (and dropped at line ends). Pending spaces
      // survive span boundaries: a trailing space of a span still joins the
      // next span's word. A hard newline ends the line immediately.
      std::size_t space_span = 0;
      std::size_t space_start = 0;
      bool pending_space = false;

      for (std::size_t si = 0; si != _spans.size(); ++si)
      {
         cnv.font(_spans[si].font_, _spans[si].font_._size);
         auto const& text = _spans[si].text;

         std::size_t word_start = text.size();

         auto flush_word = [&](std::size_t word_end)
         {
            if (word_start == text.size())
               return;
            place_word(si, word_start, word_end, space_span, space_start,
               pending_space);
            word_start = text.size();
            pending_space = false;
         };

         std::size_t pos = 0;
         unsigned state = 0;
         unsigned cp = 0;
         while (pos < text.size())
         {
            std::size_t i = pos;
            do
            {
               state = decode_utf8(state, cp, uint8_t(text[i++]));
            } while (state != 0);

            if (is_newline(cp))
            {
               flush_word(pos);
               end_line();
            }
            else if (is_space(cp))
            {
               flush_word(pos);
               if (!pending_space)
               {
                  space_span = si;
                  space_start = pos;
               }
               pending_space = true;
            }
            else if (word_start == text.size())
            {
               word_start = pos;
            }
            pos = i;
         }
         flush_word(text.size());
      }

      end_line();
      _size = {max_width, 0};
      for (auto const& l : _lines)
         _size.y += l.ascent + l.descent;
   }

   void rich_text_layout::draw(canvas& cnv, point pos, int align)
   {
      if (_lines.empty())
         return;

      float block_width = 0;
      for (auto const& l : _lines)
         block_width = std::max(block_width, l.width);

      auto state = cnv.new_state();
      cnv.text_align(canvas::left);

      float y = pos.y;
      for (auto const& l : _lines)
      {
         float x = pos.x;
         switch (align & 0x3)
         {
            case canvas::center:
               x += (block_width - l.width) / 2;
               break;
            case canvas::right:
               x += block_width - l.width;
               break;
            default:
               break;
         }

         float baseline = y + l.ascent;
         for (auto const& seg : l.segments)
         {
            auto const& span = _spans[seg.span];
            cnv.font(span.font_, span.font_._size);
            cnv.fill_style(span.color_);
            cnv.fill_text(
               std::string_view(
                  span.text.data() + seg.first, seg.last - seg.first),
               {x + seg.x, baseline});
         }
         y += l.ascent + l.descent;
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // rich_text_element
   ////////////////////////////////////////////////////////////////////////////
   rich_text_element::rich_text_element(
      std::vector<text_span> spans
    , float width
    , int align
   )
    : _layout(std::move(spans))
    , _width(width)
    , _align(align)
   {}

   view_limits rich_text_element::limits(basic_context const& /* ctx */) const
   {
      layout_(_width);
      auto s = _layout.size();
      return {{s.x, s.y}, {s.x, s.y}};
   }

   void rich_text_element::draw(context const& ctx)
   {
      layout_(_width);
      auto s = _layout.size();
      float y = ctx.bounds.top;
      switch (_align & 0x1C)
      {
         case canvas::middle:
            y += (ctx.bounds.height() - s.y) / 2;
            break;
         case canvas::bottom:
            y += ctx.bounds.height() - s.y;
            break;
         default:
            break;
      }
      _layout.draw(ctx.canvas, {ctx.bounds.left, y}, _align);
   }

   void rich_text_element::layout_(float width) const
   {
      if (_laid_width != width)
      {
         _layout.layout(width);
         _laid_width = width;
      }
   }

   rich_text_element rich_text(std::vector<text_span> spans, float width, int align)
   {
      return rich_text_element{std::move(spans), width, align};
   }
}
