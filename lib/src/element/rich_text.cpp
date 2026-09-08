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
      _line_ranges.clear();
      _line_carets.clear();
      _span_bases.clear();
      if (_spans.empty())
      {
         _size = {0, 0};
         return;
      }

      _span_bases.resize(_spans.size() + 1);
      for (std::size_t i = 0; i != _spans.size(); ++i)
         _span_bases[i + 1] = _span_bases[i] + _spans[i].text.size();

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
      for (auto const& span : _spans)
      {
         font f = span.font_;
         _fonts.push_back(f);
         cnv.font(f, span.font_._size);
         auto fm = cnv.measure_font();
         metrics.push_back({f, fm.ascent, fm.descent});
      }

      line current;
      std::vector<caret_point> current_carets;
      float x = 0;
      float max_width = 0;
      std::size_t line_start = 0;

      // Pending whitespace is kept separate until the following word is
      // placed. This lets a wrap drop leading spaces without losing their
      // byte range for caret and hit-test queries.
      struct space_seg
      {
         std::size_t  si;
         std::size_t  first;
         std::size_t  last;
         float        w;
      };
      std::vector<space_seg> pending_spaces;
      bool preserve_pending_spaces = true;

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

      auto add_caret = [&](std::size_t byte, float caret_x)
      {
         if (!current_carets.empty() && current_carets.back().byte == byte)
            current_carets.back().x = caret_x;
         else
            current_carets.push_back({byte, caret_x});
      };

      // Record measured advances at UTF-8 boundaries. Keep this linear in the
      // number of codepoints: measuring every prefix would make long words
      // quadratic during editing.
      auto append_segment_carets = [&](std::size_t si, std::size_t first,
         std::size_t last, float segment_x, float w)
      {
         auto const& text = _spans[si].text;
         auto base = span_base(si);
         add_caret(base + first, segment_x);
         auto pos = first;
         float advance_total = 0;
         while (pos < last)
         {
            auto i = pos;
            unsigned state = 0;
            unsigned cp = 0;
            do
            {
               state = decode_utf8(state, cp, uint8_t(text[i++]));
            } while (state != 0 && state != utf8_reject && i < last);

            if (state != 0)
            {
               ++pos;
               continue;
            }

            advance_total += measure_span_text(si, pos, i);
            add_caret(base + i, segment_x + advance_total);
            pos = i;
         }
         if (current_carets.empty() ||
             current_carets.back().byte != base + last)
            add_caret(base + last, segment_x + w);
         else
            current_carets.back().x = segment_x + w;
      };

      // Spaces omitted at a wrap boundary are still valid caret positions.
      // Keep their per-codepoint advances so clicks do not jump by one byte
      // at a time through a multibyte whitespace character.
      auto append_pending_space_carets = [&]
      {
         auto tail_x = x;
         for (auto const& sg : pending_spaces)
         {
            auto segment_x = tail_x;
            auto const& text = _spans[sg.si].text;
            auto base = span_base(sg.si);
            add_caret(base + sg.first, segment_x);
            auto pos = sg.first;
            float advance_total = 0;
            while (pos < sg.last)
            {
               auto i = pos;
               unsigned state = 0;
               unsigned cp = 0;
               do
               {
                  state = decode_utf8(state, cp, uint8_t(text[i++]));
               } while (state != 0 && state != utf8_reject && i < sg.last);

               if (state != 0)
               {
                  ++pos;
                  continue;
               }

               advance_total += measure_span_text(sg.si, pos, i);
               add_caret(base + i, segment_x + advance_total);
               pos = i;
            }
            if (!current_carets.empty() &&
                current_carets.back().byte == base + sg.last)
               current_carets.back().x = segment_x + sg.w;
            tail_x = segment_x + sg.w;
         }
      };

      auto end_line = [&](bool force_empty, std::size_t blank_span,
         std::size_t next_line_start, float trailing_width = 0)
      {
         if (current.segments.empty())
         {
            if (force_empty)
            {
               current.ascent = metrics[blank_span].ascent;
               current.descent = metrics[blank_span].descent;
               current.caret_width = 0;
               _lines.push_back(std::move(current));
               current = {};
               _line_carets.push_back(std::move(current_carets));
               current_carets = {};
               _line_ranges.push_back({line_start, next_line_start});
               line_start = next_line_start;
            }
            x = 0;
            return;
         }
         append_pending_space_carets();
         current.width = x;
         current.caret_width = x + trailing_width;
         max_width = std::max(max_width, x);
         _lines.push_back(std::move(current));
         current = {};
         _line_carets.push_back(std::move(current_carets));
         current_carets = {};
         x = 0;
         _line_ranges.push_back({line_start, next_line_start});
         line_start = next_line_start;
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
         append_segment_carets(si, first, last, x, w);
         x += w;
      };

      // A word can cross style spans. Keep its pieces together until a
      // whitespace or hard newline tells us where the word ends.
      struct word_seg
      {
         std::size_t  si;
         std::size_t  base;
         std::size_t  first;
         std::size_t  last;
         float        w;
      };
      std::vector<word_seg> pending_word;

      // Place all pieces of one word. The word is put on the current line
      // when it fits (carrying its pending leading spaces); if it fits a full
      // line but not the remainder of the current one, the line ends first
      // and the pending spaces are dropped. An over-wide word is hard-broken
      // character by character while retaining each piece's style.
      auto place_word = [&](std::vector<word_seg> const& word)
      {
         float w = 0;
         for (auto const& seg : word)
            w += seg.w;
         float space = 0;
         for (auto const& sg : pending_spaces)
            space += sg.w;
         auto preserve_spaces = preserve_pending_spaces;

         if (x + space + w <= width ||
             (x == 0 && preserve_spaces && !pending_spaces.empty() &&
                w <= width))
         {
            // Fits on the current line, including pending leading spaces.
            // Spaces at the beginning of the document and after an explicit
            // newline are significant, even if keeping them makes the line
            // wider than the wrap width.
            for (auto const& sg : pending_spaces)
               put_segment(sg.si, sg.first, sg.last, sg.w);
            pending_spaces.clear();
            preserve_pending_spaces = false;
            for (auto const& seg : word)
               put_segment(seg.si, seg.first, seg.last, seg.w);
         }
         else if (w <= width)
         {
            // The word does not fit the remainder: end the line, strip
            // pending spaces.
            auto break_at = word.front().base + word.front().first;
            end_line(false, 0, break_at, space);
            pending_spaces.clear();
            preserve_pending_spaces = false;
            for (auto const& seg : word)
               put_segment(seg.si, seg.first, seg.last, seg.w);
         }
         else
         {
            // Hard break: the word is wider than a full line. Split it
            // character by character (UTF-8 aware), including across spans.
            if (!current.segments.empty())
            {
               auto break_at = word.front().base + word.front().first;
               end_line(false, 0, break_at, space);
            }
            if (x == 0 && preserve_spaces)
            {
               for (auto const& sg : pending_spaces)
                  put_segment(sg.si, sg.first, sg.last, sg.w);
            }
            pending_spaces.clear();
            preserve_pending_spaces = false;

            for (auto const& seg : word)
            {
               auto const& text = _spans[seg.si].text;
               std::size_t pos = seg.first;
               unsigned state = 0;
               unsigned cp = 0;
               while (pos < seg.last)
               {
                  std::size_t i = pos;
                  do
                  {
                     state = decode_utf8(state, cp, uint8_t(text[i++]));
                  } while (state != 0 && state != utf8_reject && i < seg.last);

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
                  float cw = measure_span_text(seg.si, pos, i);
                  if (x > 0 && x + cw > width)
                  {
                     auto break_at = seg.base + pos;
                     end_line(false, 0, break_at);
                  }
                  put_segment(seg.si, pos, i, cw);
                  pos = i;
               }
            }
         }
      };

      auto place_pending_word = [&]
      {
         if (!pending_word.empty())
         {
            place_word(pending_word);
            pending_word.clear();
         }
      };

      auto place_pending_spaces = [&]
      {
         for (auto const& sg : pending_spaces)
            put_segment(sg.si, sg.first, sg.last, sg.w);
         pending_spaces.clear();
      };

      // Greedy token scan across all spans. Words are sequences of
      // non-whitespace characters; whitespace is kept pending and attached
      // to the word that follows (and dropped at wrap-induced line ends).
      // Pending spaces survive span boundaries; each span contributes its
      // own whitespace segment. A hard newline ends the line immediately
      // and drops pending spaces; spaces after the newline are preserved.
      unsigned prev_cp = 0;
      std::size_t span_base = 0;
      for (std::size_t si = 0; si != _spans.size(); ++si)
      {
         auto const& text = _spans[si].text;
         std::size_t word_start = text.size();

         auto flush_word = [&](std::size_t word_end)
         {
            if (word_start == text.size())
               return;
            pending_word.push_back({si, span_base, word_start, word_end,
               measure_span_text(si, word_start, word_end)});
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
            } while (state != 0 && state != utf8_reject && i < text.size());

            if (state == utf8_reject)
            {
               // An invalid byte: flush the word prefix, then skip one byte
               // and resynchronize the decoder (a reject state is sticky).
               flush_word(pos);
               place_pending_word();
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
               place_pending_word();
               prev_cp = 0;
               break;
            }

            if (is_newline(cp))
            {
               if (!(cp == '\n' && prev_cp == '\r'))
               {
                  // A hard newline (CRLF counts once: the \r already ended
                  // the line).
                  flush_word(pos);
                  place_pending_word();
                  pending_spaces.clear();
                  preserve_pending_spaces = true;
                  auto next_line_start = span_base + i;
                  end_line(true, si, next_line_start);
               }
               else if (!_line_ranges.empty())
               {
                  // A CRLF may cross a style-span boundary. The CR already
                  // ended the line; extend that range over the LF as well so
                  // the pair remains one logical newline.
                  auto crlf_end = span_base + i;
                  _line_ranges.back().second = crlf_end;
                  line_start = crlf_end;
               }
            }
            else if (is_space(cp))
            {
               flush_word(pos);
               place_pending_word();
               if (!current.segments.empty())
                  preserve_pending_spaces = false;
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
         span_base += text.size();
      }

      place_pending_word();
      // A trailing whitespace run has no following word to attach to. Keep
      // it on the current line so an editor can show and hit-test its caret;
      // whitespace before an explicit newline is still discarded above.
      place_pending_spaces();
      preserve_pending_spaces = false;
      // A non-empty span list represents a real text source, including an
      // empty block. Keep one metrics-bearing line so an editor can place a
      // visible caret in an empty document/block.
      end_line(current.segments.empty(), _spans.size() - 1, span_base);
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
      if (si < _span_bases.size())
         return _span_bases[si];
      return total_size();
   }

   std::size_t rich_text_layout::total_size() const
   {
      return _span_bases.empty()? 0 : _span_bases.back();
   }

   std::size_t rich_text_layout::snap_byte(std::size_t byte) const
   {
      std::size_t base = 0;
      for (auto const& span : _spans)
      {
         auto end = base + span.text.size();
         if (byte < end)
         {
            auto local = byte - base;
            while (local > 0 &&
               (uint8_t(span.text[local]) & 0xC0) == 0x80)
               --local;
            return base + local;
         }
         base = end;
      }
      return base;
   }

   std::size_t rich_text_layout::byte_at(point p) const
   {
      float y = 0;
      for (std::size_t li = 0; li != _lines.size(); ++li)
      {
         auto const& l = _lines[li];
         float h = l.ascent + l.descent;
         if (p.y < y)
            return 0;
         if (p.y < y + h)
         {
            if (li >= _line_carets.size() || _line_carets[li].empty())
               return (li < _line_ranges.size())? _line_ranges[li].first : 0;

            auto const& carets = _line_carets[li];
            if (p.x <= carets.front().x)
               return carets.front().byte;
            for (std::size_t k = 1; k != carets.size(); ++k)
            {
               auto const& previous = carets[k - 1];
               auto const& current = carets[k];
               if (p.x < (previous.x + current.x) / 2)
                  return previous.byte;
            }
            return carets.back().byte;
         }
         y += h;
      }
      // Below all lines: the end of the text.
      return total_size();
   }

   float rich_text_layout::x_at(std::size_t byte) const
   {
      if (_lines.empty())
         return 0;

      byte = snap_byte(byte);

      auto li = line_at(byte);
      if (li >= _lines.size())
         li = _lines.size() - 1;
      if (li >= _line_carets.size() || _line_carets[li].empty())
         return 0;

      auto const& carets = _line_carets[li];
      auto it = std::lower_bound(carets.begin(), carets.end(), byte,
         [](caret_point const& caret, std::size_t value)
         {
            return caret.byte < value;
         });
      if (it != carets.end() && it->byte == byte)
         return it->x;
      if (it == carets.begin())
         return it->x;
      return (it == carets.end()? carets.back() : *(it - 1)).x;
   }

   std::size_t rich_text_layout::line_at(std::size_t byte) const
   {
      if (_lines.empty())
         return 0;

      byte = snap_byte(byte);

      if (_line_ranges.size() == _lines.size())
      {
         for (std::size_t li = 0; li != _line_ranges.size(); ++li)
         {
            auto const& range = _line_ranges[li];
            if (byte < range.second ||
                (li + 1 == _line_ranges.size() && byte == range.second))
               return li;
            if (byte < range.first)
               return li;
         }
         return _line_ranges.size() - 1;
      }

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
      return _lines.size() - 1;
   }

   std::pair<std::size_t, std::size_t>
   rich_text_layout::line_range(std::size_t line) const
   {
      if (line >= _line_ranges.size())
         return {};
      return _line_ranges[line];
   }

   point rich_text_layout::caret_pos(std::size_t byte) const
   {
      if (_lines.empty())
         return {};

      auto li = line_at(byte);
      float y = 0;
      for (std::size_t i = 0; i != li; ++i)
         y += _lines[i].ascent + _lines[i].descent;
      return {x_at(byte), y};
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
