/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements.hpp>
#include <cmath>
#include <cstdio>

using namespace cycfi::elements;

namespace
{
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

   auto make_document() -> std::shared_ptr<text_document>;

   int run_selftest()
   {
      using pos = text_document::position;

      // 1. An empty document has one empty paragraph block
      {
         text_document doc;
         CHECK(doc.size() == 1 && doc.blocks()[0].plain_text().empty(),
            "empty document has one empty block");
      }

      // 2. Single-block insertion
      {
         text_document doc;
         doc.insert({0, 0}, "hello");
         CHECK(doc.blocks()[0].plain_text() == "hello", "single block insertion");
         doc.insert({0, 5}, " world");
         CHECK(doc.blocks()[0].plain_text() == "hello world", "insert at block end");
         doc.insert({0, 0}, "> ");
         CHECK(doc.blocks()[0].plain_text() == "> hello world", "insert at block start");
      }

      // 3. Multi-line insertion splits blocks
      {
         text_document doc;
         doc.insert({0, 0}, "one\ntwo\nthree");
         CHECK(doc.size() == 3, "multiline insert splits into three blocks");
         CHECK(doc.blocks()[0].plain_text() == "one", "first block");
         CHECK(doc.blocks()[1].plain_text() == "two", "second block");
         CHECK(doc.blocks()[2].plain_text() == "three", "third block");
      }

      // 4. Split inside existing text merges the tail
      {
         text_document doc;
         doc.insert({0, 0}, "abcd");
         doc.insert({0, 2}, "\n");
         CHECK(doc.size() == 2, "split produces two blocks");
         CHECK(doc.blocks()[0].plain_text() == "ab", "head block");
         CHECK(doc.blocks()[1].plain_text() == "cd", "tail block");
      }

      // 5. Cross-block erase joins blocks
      {
         text_document doc;
         doc.insert({0, 0}, "ab\ncd");
         doc.erase({0, 1}, {1, 1});   // "b" + "\n" + "c"
         CHECK(doc.size() == 1, "cross-block erase joins blocks");
         CHECK(doc.blocks()[0].plain_text() == "ad", "joined text");
      }

      // 6. Undo/redo across split and join
      {
         text_document doc;
         doc.insert({0, 0}, "ab\ncd");
         CHECK(doc.size() == 2, "initial two blocks");
         doc.undo();
         CHECK(doc.size() == 1 && doc.blocks()[0].plain_text().empty(),
            "undo restores the empty document");
         doc.redo();
         CHECK(doc.size() == 2 && doc.blocks()[0].plain_text() == "ab" &&
            doc.blocks()[1].plain_text() == "cd", "redo restores the split");

         doc.erase({0, 1}, {1, 1});
         CHECK(doc.size() == 1 && doc.blocks()[0].plain_text() == "ad",
            "join erases the boundary");
         doc.undo();
         CHECK(doc.size() == 2 && doc.blocks()[0].plain_text() == "ab" &&
            doc.blocks()[1].plain_text() == "cd", "undo restores the join");
         doc.redo();
         CHECK(doc.size() == 1 && doc.blocks()[0].plain_text() == "ad",
            "redo re-applies the join");
      }

      // 7. Offset mapping round-trip
      {
         text_document doc;
         doc.insert({0, 0}, "abc\nde\nf");
         // Blocks: "abc"(3) "de"(2) "f"(1)
         CHECK(doc.block_at(0) == 0, "offset 0 in block 0");
         CHECK(doc.block_at(3) == 1, "offset 3 in block 1");
         CHECK(doc.block_at(5) == 2, "offset 5 in block 2");
         CHECK(doc.doc_offset({1, 1}) == 4, "doc offset of {1,1}");
         CHECK(doc.doc_offset({2, 0}) == 5, "doc offset of {2,0}");
      }

      // 8. Clamp
      {
         text_document doc;
         doc.insert({0, 0}, "abc");
         auto p = doc.clamp({5, 100});
         CHECK(p.block == 0 && p.offset == 3, "clamp pins to the last block");
      }

      // 9. set_type
      {
         text_document doc;
         doc.insert({0, 0}, "title");
         doc.set_type(0, block_type::heading1);
         CHECK(doc.blocks()[0].type == block_type::heading1,
            "set_type changes the block type");
         doc.undo();
         CHECK(doc.blocks()[0].type == block_type::paragraph,
            "set_type is undoable");
      }

      // 10. layout query round-trip: x_at(byte_at(p)) lands near p
      {
         auto theme_ = get_theme();
         auto base = theme_.text_box_font;
         rich_text_layout rl({text_span{
            "the quick brown fox jumps over the lazy dog",
            base, theme_.label_font_color}});
         rl.layout(200);
         bool ok = true;
         auto& lines = rl.lines();
         if (lines.empty())
            ok = false;
         auto h = lines[0].ascent + lines[0].descent;
         float y = 0;
         for (auto const& l : lines)
         {
            auto py = y + h * 0.4f;
            auto b = rl.byte_at({py * 3, py});
            auto x = rl.x_at(b);
            if (std::abs(x - py * 3) > 5.0f)
               ok = false;
            y += l.ascent + l.descent;
         }
         CHECK(ok, "byte_at/x_at round-trip");
      }

      // 10b. caret_pos/byte_at inverse check on the sample document
      {
         auto doc = make_document();
         auto const& thm = get_theme();
         bool ok = true;
         std::size_t bi = 0;
         for (auto const& b : doc->blocks())
         {
            std::vector<text_span> spans;
            spans.push_back({b.plain_text(), font_descr{"Microsoft YaHei", 13.5}, thm.label_font_color});
            rich_text_layout rl(std::move(spans));
            rl.layout(420);
            for (std::size_t off = 0; off <= b.size(); ++off)
            {
               auto pos = rl.caret_pos(off);
               auto back = rl.byte_at(pos);
               if (back > off? back - off > 6 : off - back > 6)
               {
                  printf("INV block=%zu off=%zu pos=(%.1f,%.1f) back=%zu width=%.1f\n",
                     bi, off, pos.x, pos.y, back, rl.lines().empty()? 0 : rl.lines()[0].width);
                  ok = false;
               }
            }
            ++bi;
         }
         CHECK(ok, "caret_pos/byte_at inverse on sample doc");
      }

      // 11. Empty edits: no-op, no undo entry, valid return range
      {
         text_document doc;
         doc.insert({0, 0}, "abc");
         auto r = doc.insert({100, 0}, "");
         CHECK(r.first < doc.size(), "empty insert clamps its range");
         doc.undo();   // must undo the real edit, not the empty one
         CHECK(doc.blocks()[0].plain_text().empty(), "empty edit adds no undo entry");
         doc.insert({0, 0}, "abc");
         auto r2 = doc.erase({100, 0}, {100, 0});
         CHECK(r2.first < doc.size(), "empty erase clamps its range");
         doc.undo();
         CHECK(doc.blocks()[0].plain_text().empty(), "empty erase adds no undo entry");
      }

      // 12. Newline edge shapes: blank blocks, leading/trailing newlines
      {
         text_document doc;
         doc.insert({0, 0}, "a\n\nb");
         CHECK(doc.size() == 3, "blank block preserved");
         CHECK(doc.blocks()[1].plain_text().empty(), "middle block is empty");

         text_document doc2;
         doc2.insert({0, 0}, "x\n");
         CHECK(doc2.size() == 2 && doc2.blocks()[1].plain_text().empty(),
            "trailing newline makes an empty tail block");

         text_document doc3;
         doc3.insert({0, 0}, "\nx");
         CHECK(doc3.size() == 2 && doc3.blocks()[0].plain_text().empty(),
            "leading newline makes an empty head block");
      }

      // 13. Split inherits the block type (first piece merges into the
      //     current block, the last piece absorbs the tail)
      {
         text_document doc;
         doc.insert({0, 0}, "ab");
         doc.set_type(0, block_type::heading1);
         doc.insert({0, 1}, "X\nY");
         CHECK(doc.size() == 2, "insert with newline splits into two blocks");
         CHECK(doc.blocks()[0].plain_text() == "aX", "head merges prefix");
         CHECK(doc.blocks()[1].plain_text() == "Yb", "tail absorbs suffix");
         CHECK(doc.blocks()[0].type == block_type::heading1 &&
            doc.blocks()[1].type == block_type::heading1,
            "all split blocks inherit the type");
      }

      // 14. Erase across 3+ blocks and whole-block boundaries
      {
         text_document doc;
         doc.insert({0, 0}, "aaa\nbbb\nccc\nddd");
         // first={0,1}: keep "a"; last={2,2}: keep "c" (byte offset 2 of
         // "ccc"). Blocks 1 is removed, block 3 survives.
         doc.erase({0, 1}, {2, 2});
         CHECK(doc.size() == 2, "three-block erase joins to two blocks");
         CHECK(doc.blocks()[0].plain_text() == "ac", "middle block removed");
         CHECK(doc.blocks()[1].plain_text() == "ddd", "trailing block survives");
         doc.undo();
         CHECK(doc.size() == 4, "undo restores four blocks");
      }

      // 14b. Code block keeps newlines: insert with '\n' does not split
      {
         text_document doc;
         doc.insert({0, 0}, "abc");
         doc.set_type(0, block_type::code_block);
         doc.insert({0, 3}, "\ndef");
         CHECK(doc.size() == 1, "newline insert inside a code block stays one block");
         CHECK(doc.blocks()[0].plain_text() == "abc\ndef", "code block holds the newline");
         CHECK(doc.blocks()[0].type == block_type::code_block,
            "code block type preserved");
      }

      // 14c. Erasing across a code block boundary joins into a code block
      {
         text_document doc;
         doc.insert({0, 0}, "P\naa");
         doc.set_type(1, block_type::code_block);
         doc.insert({1, 2}, "\nbb");      // code block keeps the newline
         // Erase from the start through the first byte of the code block:
         // the tail "a\nbb" carries a newline into the joined block.
         doc.erase({0, 0}, {1, 1});
         CHECK(doc.size() == 1, "boundary erase joins the blocks");
         CHECK(doc.blocks()[0].plain_text() == "a\nbb",
            "joined text keeps the code newline");
         CHECK(doc.blocks()[0].type == block_type::code_block,
            "joined block becomes a code block (it holds newlines)");
         doc.undo();
         CHECK(doc.size() == 2 && doc.blocks()[0].plain_text() == "P" &&
            doc.blocks()[1].plain_text() == "aa\nbb" &&
            doc.blocks()[1].type == block_type::code_block,
            "undo restores the boundary, text and type");
      }

      // 14d. Erase inside a code block across a newline is a plain edit
      {
         text_document doc;
         doc.insert({0, 0}, "ab");
         doc.set_type(0, block_type::code_block);
         doc.insert({0, 2}, "\ncd");
         doc.erase({0, 1}, {0, 4});  // "b\nc" crosses the newline
         CHECK(doc.size() == 1, "in-code erase stays one block");
         CHECK(doc.blocks()[0].plain_text() == "ad", "lines joined inside the block");
      }

      // 14e. set_type to a non-code block flattens the newlines
      {
         text_document doc;
         doc.insert({0, 0}, "ab");
         doc.set_type(0, block_type::code_block);
         doc.insert({0, 2}, "\ncd");
         doc.set_type(0, block_type::paragraph);
         CHECK(doc.blocks()[0].plain_text() == "ab cd",
            "paragraph cannot hold newlines: flattened to spaces");
      }

      // 15. Reversed erase arguments
      {
         text_document doc;
         doc.insert({0, 0}, "abc");
         doc.erase({0, 3}, {0, 0});
         CHECK(doc.blocks()[0].plain_text().empty(), "reversed erase clears the block");
      }

      // 16. Multi-step undo/redo chain; redo cleared by a new edit
      {
         text_document doc;
         doc.insert({0, 0}, "one");
         doc.insert({0, 3}, "\ntwo");
         doc.set_type(0, block_type::quote);
         doc.undo(); doc.undo();
         CHECK(doc.size() == 1 && doc.blocks()[0].plain_text() == "one",
            "two undos walk back");
         doc.redo();
         CHECK(doc.size() == 2, "redo re-splits");
         doc.insert({0, 3}, "X");
         CHECK(!doc.can_redo(), "new edit clears the redo stack");
      }

      // 17. Select-all erase leaves one empty block
      {
         text_document doc;
         doc.insert({0, 0}, "a\nb\nc");
         auto n = doc.size();
         doc.erase({0, 0}, {n - 1, doc.blocks()[n - 1].size()});
         CHECK(doc.size() == 1 && doc.blocks()[0].plain_text().empty(),
            "full erase leaves one empty block");
         doc.undo();
         CHECK(doc.size() == 3, "undo restores everything");
      }

      // 18. Out-of-range positions clamp before editing
      {
         text_document doc;
         doc.insert({0, 0}, "abc");
         doc.insert({100, 0}, "X");
         // {100,0} clamps to block 0 offset 0: the block start.
         CHECK(doc.blocks()[0].plain_text() == "Xabc", "out-of-range insert clamps");
      }

      // 18b. Clamp before ordering prevents an invalid reversed range from
      // underflowing the single-block erase length.
      {
         text_document doc;
         doc.insert({0, 0}, "abc");
         doc.erase({100, 0}, {0, 100});
         CHECK(doc.blocks()[0].plain_text().empty(),
            "reversed out-of-range erase clamps before swapping");

         text_document replaced;
         replaced.insert({0, 0}, "abc");
         replaced.replace({100, 0}, {0, 100}, "x");
         CHECK(replaced.blocks()[0].plain_text() == "x",
            "reversed out-of-range replacement clamps before swapping");
         replaced.undo();
         CHECK(replaced.blocks()[0].plain_text() == "abc",
            "out-of-range replacement remains undoable");
      }

      // 19. Clamp lands on a UTF-8 codepoint boundary
      {
         text_document doc;
         doc.insert({0, 0}, "中");      // 3-byte CJK character
         auto p = doc.clamp({0, 1});     // inside the character
         CHECK(p.offset == 0, "clamp steps back to the codepoint boundary");
         auto end = doc.clamp({0, doc.blocks()[0].size()});
         CHECK(end.offset == doc.blocks()[0].size(),
            "clamp keeps the exact text end");
      }

      // 19b. Imported blocks obey the same newline invariant as edits.
      {
         text_document imported({
            {block_type::paragraph, "a\r\nb"},
            {block_type::code_block, "x\r\ny"}
         });
         CHECK(imported.blocks()[0].plain_text() == "a b" &&
            imported.blocks()[1].plain_text() == "x\ny",
            "initial blocks normalize newlines");
      }

      // 20. Trailing spaces dropped at wrap get a virtual advance: the
      //     caret walks past them and clicks land after them
      {
         auto theme_ = get_theme();
         auto base = theme_.text_box_font;
         rich_text_layout rl({text_span{
            "ab  ", base, theme_.label_font_color}});
         rl.layout(full_extent);
         auto x_end_word = rl.x_at(2);   // after "ab"
         auto x_end_text = rl.x_at(4);   // after the two trailing spaces
         CHECK(x_end_text > x_end_word, "trailing spaces advance the caret");
         auto b = rl.byte_at({x_end_text, 2.0f});
          CHECK(b == 4, "click past trailing spaces lands at the text end");
      }

      // 21. Clipboard-style CRLF input is normalized before it reaches the
      // block model; a code block keeps the newline but uses LF as well.
      {
         text_document doc;
         doc.insert({0, 0}, "a\r\nb\rc");
         CHECK(doc.size() == 3 && doc.blocks()[0].plain_text() == "a" &&
            doc.blocks()[1].plain_text() == "b" && doc.blocks()[2].plain_text() == "c",
            "CRLF and CR insert as normalized block breaks");

         text_document code({text_block{block_type::code_block,
            std::vector<text_span>{}}});
         code.insert({0, 0}, "a\r\nb");
         CHECK(code.size() == 1 && code.blocks()[0].plain_text() == "a\nb",
            "code block normalizes CRLF without splitting");
      }

      // 22. Replacing a selection is one undoable operation.
      {
         text_document doc;
         doc.insert({0, 0}, "hello world");
         doc.replace({0, 0}, {0, 5}, "hi");
         CHECK(doc.blocks()[0].plain_text() == "hi world", "selection replacement");
         doc.undo();
         CHECK(doc.blocks()[0].plain_text() == "hello world",
            "replacement undo restores the selection");
         doc.undo();
         CHECK(doc.blocks()[0].plain_text().empty(),
            "replacement uses one undo entry");
         doc.redo();
         doc.redo();
         CHECK(doc.blocks()[0].plain_text() == "hi world",
            "replacement redo restores the final text");
      }

      // 22b. A replacement may use storage owned by the selected block.
      // The document must copy the view before erasing that block.
      {
         text_document doc;
         doc.insert({0, 0}, "abcdef");
         auto const& source = doc.blocks()[0].spans[0].text;
         doc.replace({0, 1}, {0, 5},
            cycfi::string_view(source.data() + 1, source.size() - 1));
         CHECK(doc.blocks()[0].plain_text() == "abcdeff",
            "replacement copies aliased input before erase");
      }

      // 23. Setting the current type is a no-op and does not destroy redo.
      {
         text_document doc;
         doc.insert({0, 0}, "x");
         doc.undo();
         doc.set_type(0, block_type::paragraph);
         CHECK(!doc.can_undo() && doc.can_redo() &&
            doc.blocks()[0].type == block_type::paragraph,
            "unchanged block type has no undo side effect");
         doc.redo();
         CHECK(doc.blocks()[0].plain_text() == "x", "redo survives unchanged type");
      }

      // 24. Inline runs survive insertion, inheritance, deletion and undo.
      {
         auto thm = get_theme();
         auto base = thm.text_box_font;
         auto red = colors::red;
         text_document doc(std::vector<text_block>{
            text_block{block_type::paragraph, std::vector<text_span>{
               {"ab", base, colors::black},
               {"CD", base.bold(), red},
               {"ef", base, colors::black}
            }}
         });
         CHECK(doc.blocks()[0].spans.size() == 3,
            "rich block keeps distinct initial styles");
         doc.insert({0, 2}, "!", {base.bold(), red});
         doc.insert({0, 5}, "?");
         CHECK(doc.blocks()[0].plain_text() == "ab!CD?ef" &&
            doc.blocks()[0].spans.size() == 3 &&
            doc.blocks()[0].spans[1].text == "!CD?",
            "insert preserves and inherits the active style");
         doc.erase({0, 1}, {0, 6});
         CHECK(doc.blocks()[0].plain_text() == "aef" &&
            doc.blocks()[0].spans.size() == 1,
            "delete merges equal styles across a removed run");
         doc.undo();
         CHECK(doc.blocks()[0].plain_text() == "ab!CD?ef" &&
            doc.blocks()[0].spans[1].text == "!CD?",
            "rich delete undo restores runs");
         doc.undo();
         CHECK(doc.blocks()[0].plain_text() == "ab!CDef" &&
            doc.blocks()[0].spans.size() == 3,
            "rich insert undo restores original runs");
         doc.redo();
         doc.redo();
         CHECK(doc.blocks()[0].plain_text() == "aef" &&
            doc.blocks()[0].spans.size() == 1,
            "rich redo restores the final runs");
      }

      // 25. Formatting a range splits the boundary runs and is undoable.
      {
         auto thm = get_theme();
         auto base = thm.text_box_font;
         text_document doc(std::vector<text_block>{
            text_block{block_type::paragraph, std::vector<text_span>{
               {"abcdef", base, colors::black}
            }}
         });
         auto style = text_style{base.italic(), colors::blue};
         doc.set_style({0, 1}, {0, 5}, style);
         CHECK(doc.blocks()[0].spans.size() == 3 &&
            doc.blocks()[0].spans[0].text == "a" &&
            doc.blocks()[0].spans[1].text == "bcde" &&
            doc.blocks()[0].spans[2].text == "f" &&
            doc.blocks()[0].spans[1].style() == style,
            "format range creates exact inline boundaries");
         doc.undo();
         CHECK(doc.blocks()[0].spans.size() == 1 &&
            doc.blocks()[0].plain_text() == "abcdef",
            "format undo restores the original style");
         doc.set_style({0, 0}, {0, 6}, {base, colors::black});
         CHECK(!doc.can_undo() && doc.can_redo(),
            "no-op format preserves redo");
         doc.redo();
         CHECK(doc.blocks()[0].spans.size() == 3 &&
            doc.blocks()[0].spans[1].style() == style,
            "format redo restores the styled range");
      }

      printf("%s\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
      return failures == 0 ? 0 : 1;
   }

   std::shared_ptr<text_document> make_document()
   {
      auto thm = get_theme();
      auto base = thm.text_box_font;
      auto heading = thm.heading_font.size(24).bold();
      auto fg = thm.label_font_color;
      auto blue = rgba(96, 205, 255, 255);
      auto red = rgba(239, 154, 154, 255);
      return std::make_shared<text_document>(
         std::vector<text_block>{
            text_block{block_type::heading1, std::vector<text_span>{
               {"Rich ", heading, fg},
               {"Text Editor", heading.bold(), blue}
            }},
            text_block{block_type::paragraph, std::vector<text_span>{
               {"Inline styles are editable: ", base, fg},
               {"bold", base.bold(), blue},
               {", ", base, fg},
               {"italic", base.italic(), red},
               {", and text inserted inside a run inherits its style.",
                  base, fg}
            }},
            text_block{block_type::quote, std::vector<text_span>{
               {"Select across runs, then call apply_style to format the "
                "range. Ctrl+Z/Y also restores inline styles.", base.italic(), fg}
            }},
            text_block{block_type::paragraph, std::vector<text_span>{
               {"中文输入测试：这段文本也可以和英文、", base, fg},
               {"加粗中文", base.bold(), blue},
               {"混合编辑。", base, fg}
            }},
            text_block{block_type::code_block, std::vector<text_span>{
               {"int main() {\n   return 0;\n}", base, fg}
            }},
            text_block{block_type::list_item, std::vector<text_span>{
               {"list item one", base, fg}
            }},
            text_block{block_type::list_item, std::vector<text_span>{
               {"list item two", base, fg}
            }},
         });
   }
}

int main(int argc, char* argv[])
{
   if (argc > 1 && std::string(argv[1]) == "--selftest")
      return run_selftest();

   // The document contains explicit inline styles and the block fonts come
   // from the theme for inherited runs.
   // The library's font match picks the first existing family without
   // glyph-level fallback, so give the body font CJK glyphs directly
   // (Microsoft YaHei carries Latin + CJK + bold). The platform-specific
   // family string stays here in the example.
   {
      auto thm = get_theme();
      thm.text_box_font = font_descr{"Microsoft YaHei", 14.0};
      thm.heading_font = font_descr{"Microsoft YaHei", 15.0};
      set_theme(thm);
   }

   app _app("Text Editor");
   window _win(_app.name());
   _win.on_close = [&_app]() { _app.stop(); };

   view view_(_win);
   view_.content(
      margin({20, 20, 20, 20},
         htile(
            vtile(text_editor(make_document(), 420), vstretch(1.0f, empty())),
            hstretch(1.0f, empty())))
   );

   _app.run();
   return 0;
}
