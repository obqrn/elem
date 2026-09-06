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

      // A fresh scratch context per layout call: a malformed byte sequence
      // (truncated UTF-8) can poison a cairo context with a sticky error
      // status, so sharing a context across calls would silently zero out
      // all later measurements.
      detail::scratch_context scratch;
      canvas cnv{*scratch.context()};

      // Per-span font metrics, needed for line height and the baseline.
      struct span_metrics
      {
         float  ascent;
         float  descent;
      };
      std::vector<span_metrics> metrics;
      metrics.reserve(_spans.size());
      for (auto const& span : _spans)
      {
         cnv.font(span.font_, span.font_._size);
         auto fm = cnv.measure_font();
         metrics.push_back({fm.ascent, fm.descent});
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

      // Measure a byte range of a span using that span's own font. Uses the
      // string_view overload so no temporary std::string is constructed.
      auto measure_span_text = [&](std::size_t si, std::size_t first,
         std::size_t last) -> float
      {
         return measure_text(cnv,
            std::string_view(_spans[si].text.data() + first, last - first),
            _spans[si].font_).x;
      };

      // Append a segment for span `si` covering bytes [first, last) at the
      // current horizontal position, advancing the cursor by the given
      // (already measured) width.
      auto put_segment = [&](std::size_t si, std::size_t first,
         std::size_t last, float w)
      {
         if (first == last)
            return;
         current.segments.push_back({si, first, last, x});
         current.ascent = std::max(current.ascent, metrics[si].ascent);
         current.descent = std::max(current.descent, metrics[si].descent);
         x += w;
      };

      // Pending whitespace: a run of space characters, tracked as one entry
      // per span it crosses. Each entry carries its own measured width
      // (measured in the font of the span it belongs to).
      struct space_seg
      {
         std::size_t  si;
         std::size_t  first;
         std::size_t  last;
         float        w;
      };
      std::vector<space_seg> pending_spaces;

      // Place a word from span `si`, bytes [first, last). The word is put on
      // the current line when it fits (carrying its pending leading spaces);
      // if the word fits a full line but not the remainder of the current
      // one, the line ends first and the pending spaces are dropped. An
      // over-wide word is hard-broken character by character.
      auto place_word = [&](std::size_t si, std::size_t first, std::size_t last)
      {
         float w = measure_span_text(si, first, last);
         float space = 0;
         for (auto const& sg : pending_spaces)
            space += sg.w;

         if (x + space + w <= width)
         {
            // Fits on the current line, including pending leading spaces.
            for (auto const& sg : pending_spaces)
               put_segment(sg.si, sg.first, sg.last, sg.w);
            pending_spaces.clear();
            put_segment(si, first, last, w);
         }
         else if (w <= width)
         {
            // The word does not fit the remainder: end the line, strip
            // pending spaces.
            end_line();
            pending_spaces.clear();
            put_segment(si, first, last, w);
         }
         else
         {
            // Hard break: the word is wider than a full line. Split it
            // character by character (UTF-8 aware).
            if (!current.segments.empty())
               end_line();
            pending_spaces.clear();

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
               } while (state != 0 && i < last);

               // A truncated sequence ends the loop with a non-zero
               // decoder state. Drop the malformed tail: passing it to
               // cairo would poison the context with a sticky error.
               if (state != 0)
                  break;

               // `i` is the byte after the current character [pos, i)
               float cw = measure_span_text(si, pos, i);
               if (x > 0 && x + cw > width)
                  end_line();
               put_segment(si, pos, i, cw);
               pos = i;
            }
         }
      };

      // Greedy token scan across all spans. Words are sequences of
      // non-whitespace characters; whitespace is kept pending and attached
      // to the word that follows (and dropped at wrap-induced line ends).
      // Pending spaces survive span boundaries; each span contributes its
      // own whitespace segment. A hard newline ends the line immediately
      // and drops pending spaces; spaces after the newline are preserved.
      for (std::size_t si = 0; si != _spans.size(); ++si)
      {
         auto const& text = _spans[si].text;
         std::size_t word_start = text.size();

         auto flush_word = [&](std::size_t word_end)
         {
            if (word_start == text.size())
               return;
            place_word(si, word_start, word_end);
            word_start = text.size();
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
            } while (state != 0 && i < text.size());

            // Drop a truncated trailing sequence (see place_word above).
            // Flush the word prefix first so the valid bytes still lay out.
            if (state != 0)
            {
               flush_word(pos);
               break;
            }

            if (is_newline(cp))
            {
               flush_word(pos);
               pending_spaces.clear();
               if (current.segments.empty() && !_lines.empty())
               {
                  // An explicit empty line: consecutive newlines. Give it
                  // the height of the current span's font.
                  line blank;
                  blank.ascent = metrics[si].ascent;
                  blank.descent = metrics[si].descent;
                  _lines.push_back(std::move(blank));
               }
               end_line();
            }
            else if (is_space(cp))
            {
               flush_word(pos);
               if (!pending_spaces.empty() &&
                   pending_spaces.back().si == si &&
                   pending_spaces.back().last == pos)
               {
                  // Extend the whitespace run within the same span.
                  auto& sg = pending_spaces.back();
                  sg.last = i;
                  sg.w += measure_span_text(si, pos, i);
               }
               else
               {
                  pending_spaces.push_back(
                     {si, pos, i, measure_span_text(si, pos, i)});
               }
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
