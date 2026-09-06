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
      auto black = colors::black;
      auto blue = rgb(0x1565C0);
      auto red = rgb(0xC62828);

      return
         margin({20, 20, 20, 20},
            vtile(
               rich_text({text_span{
                  "Rich Text Demo", base.size(24).bold(), black}}),
               rich_text({text_span{
                  "Mixed styles on one line: ", base, black},
                  {text_span{"bold", base.bold(), blue}},
                  {text_span{" and ", base, black}},
                  {text_span{"italic", base.italic(), red}},
                  {text_span{" and ", base, black}},
                  {text_span{"small caps", base.size(10), black}}}),
               rich_text({text_span{
                  "Wrapped text: one two three four five six seven eight "
                  "nine ten eleven twelve thirteen fourteen fifteen",
                  base, black}}, 200),
               rich_text({text_span{
                  "right aligned", base.italic(), black}}, 200,
                  canvas::right),
               rich_text({text_span{
                  "center aligned", base.bold(), black}}, 200,
                  canvas::center),
               rich_text({
                  text_span{"中文 ", base.size(16), black},
                  text_span{"加粗中文", base.size(16).bold(), blue},
                  text_span{" 与英文 mixed together 自动换行测试文本内容示例",
                     base.size(16), black}}, 200)
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
   view_.content(
      scroller(make_content())
   );

   _app.run();
   return 0;
}
