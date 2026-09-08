/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements.hpp>
#include <elements/support/detail/scratch_context.hpp>
#include <cmath>
#include <cstdio>

using namespace cycfi::elements;

namespace
{
   ////////////////////////////////////////////////////////////////////////////
   // Self test: pure layout assertions, no window required. Run with:
   //    rich_text --selftest
   ////////////////////////////////////////////////////////////////////////////
   int failures = 0;

   #define CHECK(cond, name)                                                  \
      do                                                                      \
      {                                                                       \
         if (cond)                                                            \
            printf("PASS %s\n", name);                                        \
         else                                                                 \
         {                                                                    \
            printf("FAIL %s (%s:%d)\n", name, __FILE__, __LINE__);            \
            ++failures;                                                       \
         }                                                                    \
      } while (0)

   int run_selftest()
   {
      auto theme_ = get_theme();
      auto base = theme_.label_font.size(12);

      // A scratch canvas for independent measurement of expected values.
      detail::scratch_context sctx;
      canvas cnv{*sctx.context()};

      // 1. Empty layout
      {
         rich_text_layout lt;
         lt.layout(100);
         CHECK(lt.lines().empty() && lt.size().x == 0 && lt.size().y == 0,
            "empty layout");

         rich_text_layout empty_block({text_span{"", base, colors::black}});
         empty_block.layout(100);
         auto empty_range = empty_block.line_range(0);
         CHECK(empty_block.lines().size() == 1 &&
            empty_block.lines()[0].ascent > 0 &&
            empty_range.first == 0 && empty_range.second == 0,
            "empty span keeps an editable line");
      }

      // 2. Single span, no wrap (full_extent): one line, width = sum
      {
         rich_text_layout lt({text_span{"hello world", base, colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 1, "single line when width is full_extent");
         CHECK(lt.lines()[0].segments.size() == 3,
            "one segment per word plus the space run");
         CHECK(lt.lines()[0].width > 0, "width positive");
      }

      // 3. Word wrap: words do not exceed the constraint width
      {
         rich_text_layout lt({text_span{
            "one two three four five six seven", base, colors::black}});
         lt.layout(60);
         CHECK(lt.lines().size() > 1, "text wraps into multiple lines");
         bool all_fit = true;
         for (auto const& l : lt.lines())
            if (l.width > 60.001f)
               all_fit = false;
         CHECK(all_fit, "no line exceeds the constraint width");
      }

      // 4. Explicit newline ends the line
      {
         rich_text_layout lt({text_span{"a\nb", base, colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 2, "hard newline produces two lines");
      }

      // 5. Leading spaces are stripped on wrapped lines
      {
         rich_text_layout lt({text_span{
            "aaaa bbbb cccc dddd", base, colors::black}});
         lt.layout(30);
         bool no_leading_space = true;
         for (auto const& l : lt.lines())
            for (auto const& seg : l.segments)
            {
               char first = lt.spans()[seg.span].text[seg.first];
               if (first == ' ' && seg.x == 0)
                  no_leading_space = false;
            }
         CHECK(no_leading_space, "wrapped lines have no leading spaces");
      }

      // 6. Long word hard-breaks character by character
      {
         rich_text_layout lt({text_span{
            "abcdefghij", base, colors::black}});
         lt.layout(30);
         CHECK(lt.lines().size() >= 2, "long word is hard broken");
         bool all_fit = true;
         for (auto const& l : lt.lines())
            if (l.width > 30.001f)
               all_fit = false;
         CHECK(all_fit, "hard broken lines respect the width");
      }

      // 7. Mixed font sizes: line height = max, spans share a baseline
      {
         rich_text_layout lt({
            text_span{"big ", base.size(24), colors::black},
            text_span{"small", base.size(12), colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 1, "mixed spans share one line");
         auto const& l = lt.lines()[0];
         CHECK(l.segments.size() == 3,
            "two text segments plus the space run");
         CHECK(l.ascent > 0 && l.descent > 0, "line metrics valid");
         // The line ascent must be at least the small font ascent; the big
         // font dominates: height must exceed the small font height.
         rich_text_layout small_only({text_span{"x", base.size(12),
            colors::black}});
         small_only.layout(full_extent);
         auto small_h = small_only.lines()[0].ascent +
            small_only.lines()[0].descent;
         CHECK(l.ascent + l.descent > small_h, "line height follows the max");
      }

      // 8. Cross-span wrap: a later span continues on the same line and
      //    the trailing space of the first span joins the next word
      {
         rich_text_layout lt({
            text_span{"aa ", base, colors::black},
            text_span{"bb cc", base, colors::black}});
         lt.layout(35);
         CHECK(lt.lines().size() >= 2, "cross-span text wraps");
         bool has_span0 = false;
         bool has_span1 = false;
         for (auto const& seg : lt.lines()[0].segments)
         {
            has_span0 = has_span0 || seg.span == 0;
            has_span1 = has_span1 || seg.span == 1;
         }
         CHECK(has_span0 && has_span1, "first line carries both spans");
         // The space segment must survive the span boundary: the first line
         // must contain three segments (aa, space, bb).
         CHECK(lt.lines()[0].segments.size() == 3,
            "trailing space segment survives the span boundary");
      }

      // 9. Hard break across spans: word wider than the line
      {
         rich_text_layout lt({
            text_span{"aa ", base, colors::black},
            text_span{"bbbbbbbbbb", base, colors::black}});
         lt.layout(30);
         bool all_fit = true;
         for (auto const& l : lt.lines())
            if (l.width > 30.001f)
               all_fit = false;
         CHECK(all_fit, "cross-span hard break respects the width");
      }

      // 10. CJK: character-by-character wrapping
      {
         rich_text_layout lt({text_span{
            "中文字符自动换行测试文本内容", base, colors::black}});
         lt.layout(60);
         bool all_fit = true;
         for (auto const& l : lt.lines())
            if (l.width > 60.001f)
               all_fit = false;
         CHECK(all_fit, "CJK text wraps within the width");
         CHECK(lt.lines().size() >= 2, "CJK text produces multiple lines");
      }

      // 11. Space before a newline is dropped; the newline byte must never
      //     appear in a segment
      {
         rich_text_layout lt({text_span{"a \nb", base, colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 2, "newline after space still wraps");
         bool clean = true;
         for (auto const& l : lt.lines())
            for (auto const& seg : l.segments)
            {
               auto const& t = lt.spans()[seg.span].text;
               for (std::size_t k = seg.first; k != seg.last; ++k)
                  if (t[k] == '\n')
                     clean = false;
               if (seg.x == 0 && t[seg.first] == ' ')
                  clean = false;
            }
         CHECK(clean, "no newline byte or leading space leaks into lines");
      }

      // 12. Consecutive newlines produce an explicit empty line
      {
         rich_text_layout lt({text_span{"a\n\nb", base, colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 3, "blank line preserved");
         CHECK(lt.lines()[1].segments.empty() && lt.lines()[1].width == 0,
            "blank line has no segments");
         CHECK(lt.lines()[1].ascent > 0, "blank line has the span height");
      }

      // 12b. Leading and trailing hard newlines also preserve their empty
      // lines.
      {
         rich_text_layout leading({text_span{"\na", base, colors::black}});
         leading.layout(full_extent);
         CHECK(leading.lines().size() == 2 &&
            leading.lines()[0].segments.empty(),
            "leading newline preserves an empty line");

         rich_text_layout trailing({text_span{"a\n", base, colors::black}});
         trailing.layout(full_extent);
         CHECK(trailing.lines().size() == 2 &&
            trailing.lines()[1].segments.empty(),
            "trailing newline preserves an empty line");

         auto leading_h = leading.lines()[0].ascent + leading.lines()[0].descent;
         auto trailing_h = trailing.lines()[0].ascent + trailing.lines()[0].descent;
         CHECK(leading.line_at(0) == 0 && leading.line_at(1) == 1 &&
            leading.caret_pos(0).y == 0 &&
            leading.byte_at({0, leading_h * 0.5f}) == 0,
            "leading newline caret stays on the empty line");
         CHECK(trailing.line_at(1) == 0 && trailing.line_at(2) == 1 &&
            trailing.caret_pos(2).y > 0 &&
            trailing.byte_at({0, trailing_h * 1.5f}) == 2,
            "trailing newline caret stays after the newline");
      }

      // 13. Multi-space runs are measured at their full width
      {
         rich_text_layout lt({text_span{
            "aaaa  bbbb cccc", base, colors::black}});
         lt.layout(40);
         bool all_fit = true;
         for (auto const& l : lt.lines())
            if (l.width > 40.001f)
               all_fit = false;
         CHECK(all_fit, "double-space runs respect the width");
      }

      // 13b. Spaces omitted at a wrap boundary remain part of the line's
      // caret range, while the next word starts on the next line.
      {
         rich_text_layout lt({text_span{"aa   b", base, colors::black}});
         auto word_w = measure_text(cnv, "aa", base).x;
         lt.layout(word_w + 1);
         CHECK(lt.lines().size() == 2 &&
            lt.line_range(0).first == 0 && lt.line_range(0).second == 5 &&
            lt.line_range(1).first == 5,
            "wrapped spaces stay in the preceding line range");
         CHECK(lt.lines()[0].caret_width > lt.lines()[0].width,
            "wrapped spaces advance the line caret");
      }

      // 13c. Wrapped spaces crossing style spans retain each span's width.
      {
         auto base_space = measure_text(cnv, " ", base).x;
         auto large = base.size(24);
         auto large_space = measure_text(cnv, " ", large).x;
         auto first_width = measure_text(cnv, "a", base).x + 1;
         rich_text_layout lt({
            text_span{"a  ", base, colors::black},
            text_span{"  b", large, colors::black}});
         lt.layout(first_width);
         auto const& l = lt.lines()[0];
         CHECK(lt.line_range(0).second == 5,
            "cross-span wrapped spaces stay in the line range");
         CHECK(std::abs(lt.x_at(3) - (l.width + 2 * base_space)) < 0.5f &&
            std::abs(lt.x_at(4) - (l.width + 2 * base_space +
               large_space)) < 0.5f &&
            std::abs(l.caret_width - (l.width + 2 * base_space +
               2 * large_space)) < 0.5f,
            "cross-span wrapped spaces use their own widths");
      }

      // 14. Spaces after a hard newline are preserved (markdown indented
      //     code block semantics)
      {
         rich_text_layout lt({text_span{"a\n  b", base, colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 2, "text after newline on its own line");
         bool leading_space_present = false;
         for (auto const& seg : lt.lines()[1].segments)
         {
            auto const& t = lt.spans()[seg.span].text;
            if (seg.x == 0 && t[seg.first] == ' ')
               leading_space_present = true;
         }
         CHECK(leading_space_present, "spaces after newline are preserved");

         rich_text_layout narrow({text_span{"a\n    b", base, colors::black}});
         narrow.layout(measure_text(cnv, "b", base).x + 1);
         bool narrow_indent = narrow.lines().size() >= 2 &&
            !narrow.lines()[1].segments.empty() &&
            narrow.lines()[1].segments.front().x == 0 &&
            narrow.lines()[1].segments.front().first == 2 &&
            narrow.spans()[0].text[narrow.lines()[1].segments.front().first] == ' ';
         CHECK(narrow_indent,
            "spaces after newline are kept when indentation wraps");
      }

      // 15. Cross-span space runs: every span contributes its own segment
      //     and the full run width is accounted for
      {
         rich_text_layout lt({
            text_span{"a ", base, colors::black},
            text_span{"  b", base, colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 1, "all on one line");
         // segments: a, space, space, b
         CHECK(lt.lines()[0].segments.size() == 4,
            "each span's spaces become their own segments");
         // total width = a + 3 spaces + b, measured segment by segment
         // (per-segment shaping has no cross-segment kerning)
         auto expected =
            measure_text(cnv, "a", base).x +
            measure_text(cnv, " ", base).x +
            measure_text(cnv, " ", base).x +
            measure_text(cnv, " ", base).x +
            measure_text(cnv, "b", base).x;
         CHECK(std::abs(lt.lines()[0].width - expected) < 0.5f,
            "full space run width accounted for");
      }

      // 16. Mixed font sizes with a cross-span space: the space is measured
      //     in its own span's font
      {
         rich_text_layout lt({
            text_span{"big ", base.size(24), colors::black},
            text_span{" b", base.size(12), colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 1, "mixed sizes on one line");
         // Expected: big("big") + big(" ") + small(" ") + small("b")
         auto big_f = base.size(24);
         auto small_f = base.size(12);
         auto expected =
            measure_text(cnv, "big", big_f).x +
            measure_text(cnv, " ", big_f).x +
            measure_text(cnv, " ", small_f).x +
            measure_text(cnv, "b", small_f).x;
         CHECK(std::abs(lt.lines()[0].width - expected) < 0.5f,
            "cross-span space measured in its own font");
      }

      // 17. A truncated UTF-8 sequence: the valid prefix is laid out, the
      //     malformed tail is dropped, and later layouts keep measuring
      //     correctly (the malformed bytes must never reach cairo)
      {
         rich_text_layout lt({text_span{
            std::string("ab\xE4\xB8", 4), base, colors::black}});
         lt.layout(40);
         CHECK(lt.lines().size() >= 1, "truncated utf8 does not crash");
         bool ranges_ok = true;
         for (auto const& l : lt.lines())
            for (auto const& seg : l.segments)
               if (seg.first > seg.last ||
                   seg.last > lt.spans()[seg.span].text.size())
                  ranges_ok = false;
         CHECK(ranges_ok, "truncated utf8 byte ranges stay in bounds");
         // The valid prefix "ab" must still be laid out with its real width.
         CHECK(lt.lines()[0].width > 0, "valid prefix still measured");
      }

      // 18. A character wider than the line is placed anyway (no hang)
      {
         rich_text_layout lt({text_span{
            "wide", base.size(24), colors::black}});
         lt.layout(1);
         CHECK(lt.lines().size() == 4, "every character lands on its own line");
      }

      // 18b. Indentation after a newline must not disable hard breaking of a
      //     word that is wider than the available line width.
      {
         rich_text_layout lt({text_span{
            "a\n    widewide", base, colors::black}});
         lt.layout(30);
         bool all_fit = true;
         for (auto const& l : lt.lines())
            if (l.width > 30.001f)
               all_fit = false;
         CHECK(all_fit, "indented over-wide word still hard breaks");
      }

      // 19. Four-byte emoji survives a hard break without splitting
      //     a code point
      {
         rich_text_layout lt({text_span{
            "\xF0\x9F\x98\x80x", base, colors::black}});
         lt.layout(12);
         bool all_fit = true;
         for (auto const& l : lt.lines())
            if (l.width > 12.001f)
               all_fit = false;
         CHECK(all_fit, "emoji hard break respects the width");
         CHECK(lt.lines().size() == 2, "emoji and x land on separate lines");
         if (!lt.lines().empty() && !lt.lines()[0].segments.empty())
         {
            auto const& seg = lt.lines()[0].segments[0];
            CHECK(seg.first == 0 && seg.last == 4,
               "emoji occupies one unsplit segment [0,4)");
         }
      }

      // 19b. Hit testing never returns an offset inside a UTF-8 codepoint.
      {
         rich_text_layout lt({text_span{
            "\xF0\x9F\x98\x80x", base, colors::black}});
         lt.layout(full_extent);
         auto const& l = lt.lines()[0];
         auto h = l.ascent + l.descent;
         bool boundaries = true;
         for (int i = 0; i != 10; ++i)
         {
            auto x = l.width * (float(i) + 0.5f) / 10.0f;
            auto byte = lt.byte_at({x, h * 0.5f});
            if (byte > 0 && byte < 4)
               boundaries = false;
         }
         CHECK(boundaries, "utf8 hit testing keeps codepoint boundaries");

         auto emoji_width = measure_text(cnv, "\xF0\x9F\x98\x80", base).x;
         auto x_width = measure_text(cnv, "x", base).x;
         CHECK(std::abs(lt.x_at(4) - emoji_width) < 0.5f &&
            std::abs(lt.x_at(5) - (emoji_width + x_width)) < 0.5f,
            "utf8 caret positions use glyph advances");
      }

      // 20. CRLF counts as a single newline (no phantom blank line)
      {
         rich_text_layout lt({text_span{
            "a\r\nb", base, colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 2, "CRLF produces two lines");
         rich_text_layout lt2({text_span{
            "a\r\n\r\nb", base, colors::black}});
         lt2.layout(full_extent);
         CHECK(lt2.lines().size() == 3, "CRLF CRLF produces one blank line");

         rich_text_layout split_crlf({
            text_span{"a\r", base, colors::black},
            text_span{"\nb", base.bold(), colors::black}});
         split_crlf.layout(full_extent);
         auto first_range = split_crlf.line_range(0);
         auto second_range = split_crlf.line_range(1);
         CHECK(split_crlf.lines().size() == 2 && first_range.second == 3 &&
            second_range.first == 3 && split_crlf.line_at(2) == 0 &&
            split_crlf.line_at(3) == 1,
            "CRLF across spans stays on one logical line break");
      }

      // 21. An invalid byte in the middle resynchronizes: the text after
      //     it is not discarded
      {
         rich_text_layout lt({text_span{
            std::string("abc\xFF" "def", 7), base, colors::black}});
         lt.layout(full_extent);
         CHECK(lt.lines().size() == 1, "one line");
         // The invalid byte is skipped; "abc" and "def" both survive.
         std::size_t covered = 0;
         for (auto const& seg : lt.lines()[0].segments)
            covered += seg.last - seg.first;
         CHECK(covered == 6, "all valid bytes are laid out (invalid skipped)");
      }

      // 22. A word split only by a style boundary still wraps as one word.
      {
         auto first_three = measure_text(cnv, "a", base).x +
            measure_text(cnv, "b", base).x +
            measure_text(cnv, "c", base.bold()).x;
         rich_text_layout lt({
            text_span{"ab", base, colors::black},
            text_span{"cd", base.bold(), colors::black}});
         lt.layout(first_three + 0.5f);
         CHECK(lt.lines().size() == 2, "cross-span word hard breaks");
         bool first_line_crosses_span = lt.lines()[0].segments.size() == 3 &&
            lt.lines()[0].segments[0].span == 0 &&
            lt.lines()[0].segments[0].first == 0 &&
            lt.lines()[0].segments[0].last == 1 &&
            lt.lines()[0].segments[1].span == 0 &&
            lt.lines()[0].segments[1].first == 1 &&
            lt.lines()[0].segments[1].last == 2 &&
            lt.lines()[0].segments[2].span == 1 &&
            lt.lines()[0].segments[2].first == 0 &&
            lt.lines()[0].segments[2].last == 1;
         CHECK(first_line_crosses_span,
            "cross-span hard break keeps style boundaries");
      }

      printf("%s\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
      return failures == 0 ? 0 : 1;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Visual demo
   ////////////////////////////////////////////////////////////////////////////
   auto make_content()
   {
      auto theme_ = get_theme();
      auto base = theme_.label_font.size(14);
      // The default theme is dark: use the theme's foreground color
      // instead of a hard-coded black. Accent colors are picked for
      // contrast on dark surfaces.
      auto fg = theme_.label_font_color;
      auto blue = rgba(96, 205, 255, 255);
      auto red = rgba(239, 154, 154, 255);
      // CJK: Open Sans has no CJK glyphs. fontconfig falls back to an
      // available CJK font, but for intended rendering pick one
      // explicitly (Microsoft YaHei on Windows).
      auto cjk = font_descr{"Microsoft YaHei", 16.0};

      return
         margin({20, 20, 20, 20},
            vtile(
               rich_text({text_span{
                  "Rich Text Demo", base.size(24).bold(), fg}}),
               rich_text({text_span{
                  "Mixed styles on one line: ", base, fg},
                  {text_span{"bold", base.bold(), blue}},
                  {text_span{" and ", base, fg}},
                  {text_span{"italic", base.italic(), red}},
                  {text_span{" and ", base, fg}},
                  {text_span{"small caps", base.size(10), fg}}}),
               rich_text({text_span{
                  "Wrapped text: one two three four five six seven eight "
                  "nine ten eleven twelve thirteen fourteen fifteen",
                  base, fg}}, 200),
               rich_text({text_span{
                  "right aligned", base.italic(), fg}}, 200,
                  canvas::right),
               rich_text({text_span{
                  "center aligned", base.bold(), fg}}, 200,
                  canvas::center),
               rich_text({
                  text_span{"中文 ", cjk, fg},
                  text_span{"加粗中文", cjk.bold(), blue},
                  text_span{" 与英文 mixed together 自动换行测试文本内容示例",
                     cjk, fg}}, 200)
         )
      );
   }
}

int main(int argc, char* argv[])
{
   if (argc > 1 && std::string(argv[1]) == "--selftest")
      return run_selftest();

   app _app("Rich Text");
   window _win(_app.name());
   _win.on_close = [&_app]() { _app.stop(); };

   view view_(_win);
   // No scroller: the content fits without scrolling, and a scroller's
   // scrollbar hover would refresh (and visibly redraw) the text. The
   // stretches let the window resize freely in both directions; without
   // them the window max size is clamped to the content's max extent.
   view_.content(
      htile(
         vtile(make_content(), vstretch(1.0f, empty())),
         hstretch(1.0f, empty())
      )
   );

   _app.run();
   return 0;
}
