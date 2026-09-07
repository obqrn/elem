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
      // The resolved font object is cached: fontconfig lookups happen once
      // per layout, not once per segment per draw.
      struct span_metrics
      {
         font   font_;
         float  ascent;
         float  descent;
      };
      std::vector<span_metrics> metrics;
      metrics.reserve(_spans.size());
      _fonts.clear();
      _fonts.reserve(_spans.size());
      _space_w.clear();
      _space_w.reserve(_spans.size());
      for (auto const& span : _spans)
      {
         font f = span.font_;
         _fonts.push_back(f);
         cnv.font(f, span.font_._size);
         auto fm = cnv.measure_font();
         _space_w.push_back(cnv.measure_text(" ").size.x);
         metrics.push_back({f, fm.ascent, fm.descent});
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

      // Measure a byte range of a span using that span's own font. The
      // resolved font comes from the layout-time cache: no fontconfig
      // lookup happens per measurement.
      auto measure_span_text = [&](std::size_t si, std::size_t first,
         std::size_t last) -> float
      {
         // The canvas is private to this layout call: setting the font
         // directly (no state save/restore) is safe and cheaper.
         cnv.font(_fonts[si], _spans[si].font_._size);
         return cnv.measure_text(std::string(
            _spans[si].text.data() + first,
            _spans[si].text.data() + last).c_str()).size.x;
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
               } while (state != 0 && state != utf8_reject && i < last);

               if (state == utf8_reject)
               {
                  // An invalid byte: skip one byte and resynchronize the
                  // decoder (a reject state is sticky).
                  i = pos + 1;
                  state = 0;
                  pos = i;
                  continue;
               }

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
         unsigned prev_cp = 0;
         while (pos < text.size())
         {
            std::size_t i = pos;
            do
            {
               state = decode_utf8(state, cp, uint8_t(text[i++]));
            } while (state != 0 && state != utf8_reject && i < text.size());

            if (state == utf8_reject)
            {
               // An invalid byte: flush the word prefix, then skip one byte
               // and resynchronize the decoder (a reject state is sticky).
               flush_word(pos);
               i = pos + 1;
               state = 0;
               prev_cp = 0;
               pos = i;
               continue;
            }

            // Drop a truncated trailing sequence (see place_word above).
            // Flush the word prefix first so the valid bytes still lay out.
            if (state != 0)
            {
               flush_word(pos);
               break;
            }

            if (is_newline(cp))
            {
               if (!(cp == '\n' && prev_cp == '\r'))
               {
                  // A hard newline (CRLF counts once: the \r already ended
                  // the line).
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
            prev_cp = cp;
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
            cnv.font(_fonts[seg.span]);
            cnv.fill_style(span.color_);
            cnv.fill_text(
               std::string_view(
                  span.text.data() + seg.first, seg.last - seg.first),
               {x + seg.x, baseline});
         }
         y += l.ascent + l.descent;
      }
   }

   std::size_t rich_text_layout::span_base(std::size_t si) const
   {
      std::size_t base = 0;
      for (std::size_t s = 0; s != si && s != _spans.size(); ++s)
         base += _spans[s].text.size();
      return base;
   }

   std::size_t rich_text_layout::total_size() const
   {
      std::size_t n = 0;
      for (auto const& s : _spans)
         n += s.text.size();
      return n;
   }

   std::size_t rich_text_layout::byte_at(point p) const
   {
      float y = 0;
      for (auto const& l : _lines)
      {
         float h = l.ascent + l.descent;
         if (p.y >= y && p.y < y + h)
         {
            for (std::size_t k = 0; k != l.segments.size(); ++k)
            {
               auto const& seg = l.segments[k];
               float seg_w = (k + 1 < l.segments.size())?
                  l.segments[k + 1].x - seg.x : l.width - seg.x;
               if (p.x >= seg.x && p.x < seg.x + seg_w)
               {
                  auto base = span_base(seg.span) + seg.first;
                  auto len = seg.last - seg.first;
                  if (len == 0)
                     return base;
                  return base + std::size_t((p.x - seg.x) / seg_w * len);
               }
            }
            // Beyond the line end: trailing dropped spaces get a virtual
            // advance, so a click/caret can land after them.
            if (p.x >= l.width && !l.segments.empty())
            {
               auto const& seg = l.segments.back();
               auto base = span_base(seg.span) + seg.last;
               auto sw = (seg.span < _space_w.size())? _space_w[seg.span] : 0;
               auto n = (sw > 0)? std::size_t((p.x - l.width) / sw + 0.5f) : 0;
               auto limit = span_base(seg.span) + _spans[seg.span].text.size();
               return std::min(base + n, limit);
            }
            // Before the line start: the start of the first segment.
            if (!l.segments.empty())
            {
               auto const& seg = l.segments.front();
               return span_base(seg.span) + seg.first;
            }
            return 0;
         }
         y += h;
      }
      // Below all lines: the end of the text.
      return total_size();
   }

   float rich_text_layout::x_at(std::size_t byte) const
   {
      for (auto const& l : _lines)
      {
         for (std::size_t k = 0; k != l.segments.size(); ++k)
         {
            auto const& seg = l.segments[k];
            auto base = span_base(seg.span) + seg.first;
            auto end = base + (seg.last - seg.first);
            if (byte < base || byte > end)
               continue;
            // byte == end interpolates to the segment end, which equals
            // the next segment's start: no ambiguity.
            float seg_w = (k + 1 < l.segments.size())?
               l.segments[k + 1].x - seg.x : l.width - seg.x;
            auto len = seg.last - seg.first;
            if (len == 0)
               return seg.x;
            return seg.x + float(byte - base) / float(len) * seg_w;
         }
      }
      // Not inside any segment: a trailing space run dropped at a wrap
      // boundary (editors keep such spaces in the document). Give them a
      // virtual advance past the end of their line.
      auto li = line_at(byte);
      if (li < _lines.size() && !_lines[li].segments.empty())
      {
         auto const& seg = _lines[li].segments.back();
         auto seg_end = span_base(seg.span) + seg.last;
         if (byte > seg_end)
         {
            auto sw = (seg.span < _space_w.size())? _space_w[seg.span] : 0;
            return _lines[li].width + float(byte - seg_end) * sw;
         }
      }
      return _lines.empty()? 0 : _lines.back().width;
   }

   std::size_t rich_text_layout::line_at(std::size_t byte) const
   {
      for (std::size_t li = 0; li != _lines.size(); ++li)
      {
         for (auto const& seg : _lines[li].segments)
         {
            auto base = span_base(seg.span) + seg.first;
            auto end = base + (seg.last - seg.first);
            if (byte >= base && byte < end)
               return li;
         }
      }
      // At or past the end: the last line.
      return _lines.empty()? 0 : _lines.size() - 1;
   }

   point rich_text_layout::caret_pos(std::size_t byte) const
   {
      // Line y: sum the heights of the lines before the one containing the
      // byte. Uses the same containment test as x_at.
      float y = 0;
      float last_top = 0;
      for (auto const& l : _lines)
      {
         last_top = y;
         bool contains = false;
         for (auto const& seg : l.segments)
         {
            auto base = span_base(seg.span) + seg.first;
            auto end = base + (seg.last - seg.first);
            if (byte >= base && byte <= end)
            {
               contains = true;
               break;
            }
         }
         if (contains)
            return {x_at(byte), y};
         y += l.ascent + l.descent;
      }
      // Past all segments (e.g. a byte after a dropped trailing space):
      // stay on the last line rather than dropping below the block.
      return {x_at(byte), last_top};
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
