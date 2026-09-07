/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements/element/text_document.hpp>
#include <infra/assert.hpp>
#include <algorithm>

namespace cycfi::elements
{
   text_document::text_document()
   {
      _blocks.push_back({});
   }

   text_document::text_document(std::vector<text_block> blocks)
    : _blocks(std::move(blocks))
   {
      if (_blocks.empty())
         _blocks.push_back({});
   }

   std::size_t text_document::text_size() const
   {
      std::size_t n = 0;
      for (auto const& b : _blocks)
         n += b.text.size();
      return n;
   }

   std::size_t text_document::block_at(std::size_t doc_off) const
   {
      std::size_t acc = 0;
      for (std::size_t i = 0; i != _blocks.size(); ++i)
      {
         acc += _blocks[i].text.size();
         if (doc_off < acc)
            return i;
      }
      return _blocks.size() - 1;
   }

   std::size_t text_document::doc_offset(position pos) const
   {
      CYCFI_ASSERT(pos.block < _blocks.size(), "Block index out of range");
      std::size_t off = 0;
      for (std::size_t i = 0; i != pos.block; ++i)
         off += _blocks[i].text.size();
      return off + pos.offset;
   }

   text_document::position text_document::clamp(position pos) const
   {
      if (_blocks.empty())
         return {};
      pos.block = std::min(pos.block, _blocks.size() - 1);
      pos.offset = std::min(pos.offset, _blocks[pos.block].text.size());
      // Step back to a UTF-8 codepoint boundary: an offset inside a
      // multi-byte character would split it across edits.
      auto& t = _blocks[pos.block].text;
      while (pos.offset > 0 && (uint8_t(t[pos.offset]) & 0xC0) == 0x80)
         --pos.offset;
      return pos;
   }

   void text_document::record(size_type first, size_type count)
   {
      undo_entry e;
      e.first = first;
      e.snapshot.assign(_blocks.begin() + first, _blocks.begin() + first + count);
      e.new_count = count;
      _undo.push_back(std::move(e));
      // Bound the undo memory: drop the oldest entries. (Coalescing
      // adjacent single-block edits is a later optimization.)
      if (_undo.size() > max_undo_depth)
         _undo.erase(_undo.begin(), _undo.begin() + (_undo.size() - max_undo_depth));
      _redo.clear();
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::insert(position pos, string_view text)
   {
      pos = clamp(pos);
      if (text.empty())
         return {pos.block, pos.block + 1};

      // A code block keeps its newlines: Enter inside a code block inserts
      // a literal newline instead of splitting the block.
      if (_blocks[pos.block].type == block_type::code_block)
      {
         record(pos.block, 1);
         auto& b = _blocks[pos.block];
         b.text.insert(pos.offset, text.data(), text.size());
         return {pos.block, pos.block + 1};
      }

      auto nl = text.find('\n');
      if (nl == std::string_view::npos)
      {
         // Simple single-block insertion.
         record(pos.block, 1);
         auto& b = _blocks[pos.block];
         b.text.insert(pos.offset, text.data(), text.size());
         return {pos.block, pos.block + 1};
      }

      // Multi-line insertion: split the text at newlines and merge the
      // first/last pieces with the surrounding block text.
      record(pos.block, 1);

      auto& b = _blocks[pos.block];
      auto tail = b.text.substr(pos.offset);   // text after the caret
      b.text.resize(pos.offset);

      std::vector<text_block> added;
      added.reserve(4);
      std::size_t start = 0;
      while (nl != std::string_view::npos)
      {
         added.push_back({b.type, std::string(text.substr(start, nl - start))});
         start = nl + 1;
         nl = text.find('\n', start);
      }
      added.push_back({b.type, std::string(text.substr(start))});

      // The first piece extends the current block; the last piece absorbs
      // the tail of the original block.
      b.text += added.front().text;
      added.back().text += tail;

      _blocks.insert(_blocks.begin() + pos.block + 1,
         std::make_move_iterator(added.begin() + 1),
         std::make_move_iterator(added.end()));
      _undo.back().new_count = added.size();

      return {pos.block, pos.block + 1 + added.size() - 1};
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::erase(position first, position last)
   {
      if (first > last)
         std::swap(first, last);

      first = clamp(first);
      last = clamp(last);

      if (first == last)
         return {first.block, first.block + 1};

      if (first.block == last.block)
      {
         // Single-block erase.
         record(first.block, 1);
         auto& b = _blocks[first.block];
         b.text.erase(first.offset, last.offset - first.offset);
         return {first.block, first.block + 1};
      }

      // Cross-block erase: keep the head of the first block and the tail
      // of the last block, joining them into the first block. If the last
      // block is a code_block the joined text may contain newlines, so the
      // joined block must be a code_block: only code blocks may hold them.
      auto count = last.block - first.block + 1;
      record(first.block, count);

      auto& b = _blocks[first.block];
      auto tail = _blocks[last.block].text.substr(last.offset);
      b.text.resize(first.offset);
      b.text += tail;
      if (_blocks[last.block].type == block_type::code_block)
         b.type = block_type::code_block;

      _blocks.erase(_blocks.begin() + first.block + 1,
         _blocks.begin() + last.block + 1);
      _undo.back().new_count = 1;

      return {first.block, first.block + 1};
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::set_type(size_type block, block_type type)
   {
      CYCFI_ASSERT(block < _blocks.size(), "Block index out of range");
      record(block, 1);
      _blocks[block].type = type;
      // A non-code block must not contain newlines: flatten any that a
      // multi-line code block carried over.
      if (type != block_type::code_block)
      {
         auto& t = _blocks[block].text;
         std::replace(t.begin(), t.end(), '\n', ' ');
      }
      return {block, block + 1};
   }

   void text_document::undo()
   {
      if (_undo.empty())
         return;

      auto e = std::move(_undo.back());
      _undo.pop_back();

      // Snapshot the current state of the edited range for redo.
      undo_entry r;
      r.first = e.first;
      r.snapshot.assign(_blocks.begin() + e.first,
         _blocks.begin() + e.first + e.new_count);
      r.new_count = e.snapshot.size();
      _redo.push_back(std::move(r));

      // Restore the pre-edit state.
      _blocks.erase(_blocks.begin() + e.first,
         _blocks.begin() + e.first + e.new_count);
      _blocks.insert(_blocks.begin() + e.first,
         e.snapshot.begin(), e.snapshot.end());
   }

   void text_document::redo()
   {
      if (_redo.empty())
         return;

      auto e = std::move(_redo.back());
      _redo.pop_back();

      undo_entry r;
      r.first = e.first;
      r.snapshot.assign(_blocks.begin() + e.first,
         _blocks.begin() + e.first + e.new_count);
      r.new_count = e.snapshot.size();
      _undo.push_back(std::move(r));

      _blocks.erase(_blocks.begin() + e.first,
         _blocks.begin() + e.first + e.new_count);
      _blocks.insert(_blocks.begin() + e.first,
         e.snapshot.begin(), e.snapshot.end());
   }
}
