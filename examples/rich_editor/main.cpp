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
         CHECK(doc.size() == 1 && doc.blocks()[0].text.empty(),
            "empty document has one empty block");
      }

      // 2. Single-block insertion
      {
         text_document doc;
         doc.insert({0, 0}, "hello");
         CHECK(doc.blocks()[0].text == "hello", "single block insertion");
         doc.insert({0, 5}, " world");
         CHECK(doc.blocks()[0].text == "hello world", "insert at block end");
         doc.insert({0, 0}, "> ");
         CHECK(doc.blocks()[0].text == "> hello world", "insert at block start");
      }

      // 3. Multi-line insertion splits blocks
      {
         text_document doc;
         doc.insert({0, 0}, "one\ntwo\nthree");
         CHECK(doc.size() == 3, "multiline insert splits into three blocks");
         CHECK(doc.blocks()[0].text == "one", "first block");
         CHECK(doc.blocks()[1].text == "two", "second block");
         CHECK(doc.blocks()[2].text == "three", "third block");
      }

      // 4. Split inside existing text merges the tail
      {
         text_document doc;
         doc.insert({0, 0}, "abcd");
         doc.insert({0, 2}, "\n");
         CHECK(doc.size() == 2, "split produces two blocks");
         CHECK(doc.blocks()[0].text == "ab", "head block");
         CHECK(doc.blocks()[1].text == "cd", "tail block");
      }

      // 5. Cross-block erase joins blocks
      {
         text_document doc;
         doc.insert({0, 0}, "ab\ncd");
         doc.erase({0, 1}, {1, 1});   // "b" + "\n" + "c"
         CHECK(doc.size() == 1, "cross-block erase joins blocks");
         CHECK(doc.blocks()[0].text == "ad", "joined text");
      }

      // 6. Undo/redo across split and join
      {
         text_document doc;
         doc.insert({0, 0}, "ab\ncd");
         CHECK(doc.size() == 2, "initial two blocks");
         doc.undo();
         CHECK(doc.size() == 1 && doc.blocks()[0].text.empty(),
            "undo restores the empty document");
         doc.redo();
         CHECK(doc.size() == 2 && doc.blocks()[0].text == "ab" &&
            doc.blocks()[1].text == "cd", "redo restores the split");

         doc.erase({0, 1}, {1, 1});
         CHECK(doc.size() == 1 && doc.blocks()[0].text == "ad",
            "join erases the boundary");
         doc.undo();
         CHECK(doc.size() == 2 && doc.blocks()[0].text == "ab" &&
            doc.blocks()[1].text == "cd", "undo restores the join");
         doc.redo();
         CHECK(doc.size() == 1 && doc.blocks()[0].text == "ad",
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
            spans.push_back({b.text, font_descr{"Microsoft YaHei", 13.5}, thm.label_font_color});
            rich_text_layout rl(std::move(spans));
            rl.layout(420);
            for (std::size_t off = 0; off <= b.text.size(); ++off)
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
         CHECK(doc.blocks()[0].text.empty(), "empty edit adds no undo entry");
         doc.insert({0, 0}, "abc");
         auto r2 = doc.erase({100, 0}, {100, 0});
         CHECK(r2.first < doc.size(), "empty erase clamps its range");
         doc.undo();
         CHECK(doc.blocks()[0].text.empty(), "empty erase adds no undo entry");
      }

      // 12. Newline edge shapes: blank blocks, leading/trailing newlines
      {
         text_document doc;
         doc.insert({0, 0}, "a\n\nb");
         CHECK(doc.size() == 3, "blank block preserved");
         CHECK(doc.blocks()[1].text.empty(), "middle block is empty");

         text_document doc2;
         doc2.insert({0, 0}, "x\n");
         CHECK(doc2.size() == 2 && doc2.blocks()[1].text.empty(),
            "trailing newline makes an empty tail block");

         text_document doc3;
         doc3.insert({0, 0}, "\nx");
         CHECK(doc3.size() == 2 && doc3.blocks()[0].text.empty(),
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
         CHECK(doc.blocks()[0].text == "aX", "head merges prefix");
         CHECK(doc.blocks()[1].text == "Yb", "tail absorbs suffix");
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
         CHECK(doc.blocks()[0].text == "ac", "middle block removed");
         CHECK(doc.blocks()[1].text == "ddd", "trailing block survives");
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
         CHECK(doc.blocks()[0].text == "abc\ndef", "code block holds the newline");
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
         CHECK(doc.blocks()[0].text == "a\nbb",
            "joined text keeps the code newline");
         CHECK(doc.blocks()[0].type == block_type::code_block,
            "joined block becomes a code block (it holds newlines)");
         doc.undo();
         CHECK(doc.size() == 2 && doc.blocks()[0].text == "P" &&
            doc.blocks()[1].text == "aa\nbb" &&
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
         CHECK(doc.blocks()[0].text == "ad", "lines joined inside the block");
      }

      // 14e. set_type to a non-code block flattens the newlines
      {
         text_document doc;
         doc.insert({0, 0}, "ab");
         doc.set_type(0, block_type::code_block);
         doc.insert({0, 2}, "\ncd");
         doc.set_type(0, block_type::paragraph);
         CHECK(doc.blocks()[0].text == "ab cd",
            "paragraph cannot hold newlines: flattened to spaces");
      }

      // 15. Reversed erase arguments
      {
         text_document doc;
         doc.insert({0, 0}, "abc");
         doc.erase({0, 3}, {0, 0});
         CHECK(doc.blocks()[0].text.empty(), "reversed erase clears the block");
      }

      // 16. Multi-step undo/redo chain; redo cleared by a new edit
      {
         text_document doc;
         doc.insert({0, 0}, "one");
         doc.insert({0, 3}, "\ntwo");
         doc.set_type(0, block_type::quote);
         doc.undo(); doc.undo();
         CHECK(doc.size() == 1 && doc.blocks()[0].text == "one",
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
         doc.erase({0, 0}, {n - 1, doc.blocks()[n - 1].text.size()});
         CHECK(doc.size() == 1 && doc.blocks()[0].text.empty(),
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
         CHECK(doc.blocks()[0].text == "Xabc", "out-of-range insert clamps");
      }

      // 19. Clamp lands on a UTF-8 codepoint boundary
      {
         text_document doc;
         doc.insert({0, 0}, "中");      // 3-byte CJK character
         auto p = doc.clamp({0, 1});     // inside the character
         CHECK(p.offset == 0, "clamp steps back to the codepoint boundary");
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

      printf("%s\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
      return failures == 0 ? 0 : 1;
   }

   std::shared_ptr<text_document> make_document()
   {
      return std::make_shared<text_document>(
         std::vector<text_block>{
            {block_type::heading1, "Text Editor"},
            {block_type::paragraph,
             "A block-structured plain text editor. Type to insert; "
             "Enter splits the block; Backspace at the block start joins "
             "it with the previous one."},
            {block_type::quote,
             "Quote block: click anywhere to place the caret, drag to "
             "select, Ctrl+Z/Y to undo/redo."},
            {block_type::paragraph,
             "中文输入测试：这段是中文，输入法直接打字，光标按字符移动。"},
            {block_type::code_block, "int main() {\n   return 0;\n}"},
            {block_type::list_item, "list item one"},
            {block_type::list_item, "list item two"},
         });
   }
}

int main(int argc, char* argv[])
{
   if (argc > 1 && std::string(argv[1]) == "--selftest")
      return run_selftest();

   // The document is plain text and the block fonts come from the theme.
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
