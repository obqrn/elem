/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements/element/text_document.hpp>
#include <infra/assert.hpp>
#include <algorithm>
#include <iterator>

namespace cycfi::elements
{
   namespace
   {
      std::string normalize_newlines(string_view text)
      {
         std::string result;
         result.reserve(text.size());
         for (std::size_t i = 0; i != text.size(); ++i)
         {
            if (text[i] == '\r')
            {
               if (i + 1 < text.size() && text[i + 1] == '\n')
                  ++i;
               result += '\n';
            }
            else
               result += text[i];
         }
         return result;
      }

      bool same_font(font_descr const& a, font_descr const& b)
      {
         return a._families == b._families
            && a._size == b._size
            && a._weight == b._weight
            && a._slant == b._slant
            && a._stretch == b._stretch;
      }

      bool same_style(text_span const& a, text_style const& b)
      {
         return same_font(a.font_, b.font_) && a.color_ == b.color_;
      }

      std::size_t block_size(text_block const& block)
      {
         std::size_t result = 0;
         for (auto const& span : block.spans)
            result += span.text.size();
         return result;
      }

      void append_span(std::vector<text_span>& spans, text_span span)
      {
         if (span.text.empty())
            return;

         if (!spans.empty() && spans.back().font_._families == span.font_._families
             && spans.back().font_._size == span.font_._size
             && spans.back().font_._weight == span.font_._weight
             && spans.back().font_._slant == span.font_._slant
             && spans.back().font_._stretch == span.font_._stretch
             && spans.back().color_ == span.color_)
         {
            spans.back().text += span.text;
         }
         else
            spans.push_back(std::move(span));
      }

      void append_span(std::vector<text_span>& spans, std::string text,
         text_style style)
      {
         append_span(spans, {std::move(text), style.font_, style.color_});
      }

      void append_spans(std::vector<text_span>& destination,
         std::vector<text_span>&& source)
      {
         for (auto& span : source)
            append_span(destination, std::move(span));
      }

      void merge_spans(text_block& block)
      {
         std::vector<text_span> merged;
         merged.reserve(block.spans.size());
         for (auto& span : block.spans)
            append_span(merged, std::move(span));
         block.spans = std::move(merged);
      }

      void normalize_spans(text_block& block)
      {
         std::vector<text_span> normalized;
         normalized.reserve(block.spans.size());
         for (auto const& span : block.spans)
         {
            auto text = normalize_newlines(span.text);
            if (block.type != block_type::code_block)
               std::replace(text.begin(), text.end(), '\n', ' ');
            append_span(normalized, std::move(text), span.style());
         }
         block.spans = std::move(normalized);
      }

      // Split a run at a clamped byte offset and return the run index at the
      // boundary. Untouched run strings are moved, not rebuilt.
      std::size_t split_at(text_block& block, std::size_t offset)
      {
         std::size_t base = 0;
         for (std::size_t i = 0; i != block.spans.size(); ++i)
         {
            auto const length = block.spans[i].text.size();
            if (offset == base)
               return i;
            if (offset < base + length)
            {
               auto local = offset - base;
               text_span right = block.spans[i];
               right.text = block.spans[i].text.substr(local);
               block.spans[i].text.resize(local);
               block.spans.insert(block.spans.begin() + i + 1,
                  std::move(right));
               return i + 1;
            }
            base += length;
         }
         return block.spans.size();
      }

      void insert_at(text_block& block, std::size_t offset,
         string_view text, text_style style)
      {
         if (text.empty())
            return;
         auto index = split_at(block, offset);
         block.spans.insert(block.spans.begin() + index,
            {std::string(text), style.font_, style.color_});
         merge_spans(block);
      }

      void erase_at(text_block& block, std::size_t first,
         std::size_t last)
      {
         if (first == last)
            return;
         auto left = split_at(block, first);
         auto right = split_at(block, last);
         block.spans.erase(block.spans.begin() + left,
            block.spans.begin() + right);
         merge_spans(block);
      }

      bool range_needs_style(text_block const& block, std::size_t first,
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
            if (overlap_first < overlap_last && !same_style(span, style))
               return true;
            base = span_last;
         }
         return false;
      }

      void style_range(text_block& block, std::size_t first,
         std::size_t last, text_style const& style)
      {
         if (first == last)
            return;
         auto left = split_at(block, first);
         auto right = split_at(block, last);
         for (auto i = left; i != right; ++i)
         {
            block.spans[i].font_ = style.font_;
            block.spans[i].color_ = style.color_;
         }
         merge_spans(block);
      }
   }

   text_block::text_block(block_type type_, std::string text_)
    : type(type_)
   {
      if (!text_.empty())
         spans.push_back({std::move(text_), {}, {}});
   }

   std::size_t text_block::size() const
   {
      return block_size(*this);
   }

   std::string text_block::plain_text() const
   {
      std::string result;
      result.reserve(size());
      for (auto const& span : spans)
         result += span.text;
      return result;
   }

   text_document::text_document()
   {
      _blocks.push_back({});
   }

   text_document::text_document(std::vector<text_block> blocks)
    : _blocks(std::move(blocks))
   {
      if (_blocks.empty())
         _blocks.push_back({});
      for (auto& block : _blocks)
         normalize_spans(block);
   }

   std::size_t text_document::text_size() const
   {
      std::size_t n = 0;
      for (auto const& block : _blocks)
         n += block.size();
      return n;
   }

   std::size_t text_document::block_at(std::size_t doc_off) const
   {
      std::size_t acc = 0;
      for (std::size_t i = 0; i != _blocks.size(); ++i)
      {
         acc += _blocks[i].size();
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
         off += _blocks[i].size();
      return off + pos.offset;
   }

   text_document::position text_document::clamp(position pos) const
   {
      if (_blocks.empty())
         return {};
      pos.block = std::min(pos.block, _blocks.size() - 1);
      pos.offset = std::min(pos.offset, _blocks[pos.block].size());

      // Step back to a UTF-8 codepoint boundary without flattening the
      // block. Editing never splits a run except at a codepoint boundary.
      std::size_t base = 0;
      for (auto const& span : _blocks[pos.block].spans)
      {
         auto span_last = base + span.text.size();
         if (pos.offset < span_last)
         {
            auto local = pos.offset - base;
            while (local > 0 && local < span.text.size() &&
               (std::uint8_t(span.text[local]) & 0xC0) == 0x80)
               --local;
            pos.offset = base + local;
            break;
         }
         base = span_last;
      }
      return pos;
   }

   void text_document::record(size_type first, size_type count)
   {
      undo_entry e;
      e.first = first;
      e.snapshot.assign(_blocks.begin() + first,
         _blocks.begin() + first + count);
      e.new_count = count;
      _undo.push_back(std::move(e));
      if (_undo.size() > max_undo_depth)
         _undo.erase(_undo.begin(),
            _undo.begin() + (_undo.size() - max_undo_depth));
      _redo.clear();
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::insert(position pos, string_view text)
   {
      return insert_impl(pos, text, true, nullptr);
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::insert(position pos, string_view text, text_style style)
   {
      return insert_impl(pos, text, true, &style);
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::insert_impl(position pos, string_view text,
      bool record_undo, text_style const* style)
   {
      pos = clamp(pos);
      if (text.empty())
         return {pos.block, pos.block + 1};

      auto normalized = normalize_newlines(text);
      string_view input{normalized.data(), normalized.size()};
      auto chosen_style = style? *style : style_at(pos);

      // A code block keeps its newlines as literal characters.
      if (_blocks[pos.block].type == block_type::code_block)
      {
         if (record_undo)
            record(pos.block, 1);
         insert_at(_blocks[pos.block], pos.offset, input, chosen_style);
         return {pos.block, pos.block + 1};
      }

      auto nl = input.find('\n');
      if (nl == std::string_view::npos)
      {
         if (record_undo)
            record(pos.block, 1);
         insert_at(_blocks[pos.block], pos.offset, input, chosen_style);
         return {pos.block, pos.block + 1};
      }

      if (record_undo)
         record(pos.block, 1);

      auto& block = _blocks[pos.block];
      auto split = split_at(block, pos.offset);
      std::vector<text_span> tail(
         std::make_move_iterator(block.spans.begin() + split),
         std::make_move_iterator(block.spans.end()));
      block.spans.erase(block.spans.begin() + split, block.spans.end());

      std::vector<text_block> pieces;
      pieces.reserve(4);
      std::size_t start = 0;
      while (nl != std::string_view::npos)
      {
         text_block piece(block.type);
         append_span(piece.spans,
            std::string(input.substr(start, nl - start)), chosen_style);
         pieces.push_back(std::move(piece));
         start = nl + 1;
         nl = input.find('\n', start);
      }
      text_block piece(block.type);
      append_span(piece.spans, std::string(input.substr(start)), chosen_style);
      pieces.push_back(std::move(piece));

      append_spans(block.spans, std::move(pieces.front().spans));
      append_spans(pieces.back().spans, std::move(tail));
      merge_spans(block);

      auto piece_count = pieces.size();
      _blocks.insert(_blocks.begin() + pos.block + 1,
         std::make_move_iterator(pieces.begin() + 1),
         std::make_move_iterator(pieces.end()));
      if (record_undo)
         _undo.back().new_count = piece_count;

      return {pos.block, pos.block + piece_count};
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::erase(position first, position last)
   {
      return erase_impl(first, last, true);
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::erase_impl(position first, position last,
      bool record_undo)
   {
      first = clamp(first);
      last = clamp(last);
      if (first > last)
         std::swap(first, last);

      if (first == last)
         return {first.block, first.block + 1};

      if (first.block == last.block)
      {
         if (record_undo)
            record(first.block, 1);
         erase_at(_blocks[first.block], first.offset, last.offset);
         return {first.block, first.block + 1};
      }

      auto count = last.block - first.block + 1;
      if (record_undo)
         record(first.block, count);

      auto& first_block = _blocks[first.block];
      auto& last_block = _blocks[last.block];
      auto last_type = last_block.type;
      auto last_split = split_at(last_block, last.offset);
      std::vector<text_span> tail(
         std::make_move_iterator(last_block.spans.begin() + last_split),
         std::make_move_iterator(last_block.spans.end()));
      last_block.spans.erase(last_block.spans.begin() + last_split,
         last_block.spans.end());

      auto first_split = split_at(first_block, first.offset);
      first_block.spans.erase(first_block.spans.begin() + first_split,
         first_block.spans.end());
      append_spans(first_block.spans, std::move(tail));
      merge_spans(first_block);
      if (last_type == block_type::code_block)
         first_block.type = block_type::code_block;

      _blocks.erase(_blocks.begin() + first.block + 1,
         _blocks.begin() + last.block + 1);
      if (record_undo)
         _undo.back().new_count = 1;
      return {first.block, first.block + 1};
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::replace(position first, position last, string_view text)
   {
      first = clamp(first);
      last = clamp(last);
      if (first > last)
         std::swap(first, last);
      return replace(first, last, text, style_at(first));
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::replace(position first, position last, string_view text,
      text_style style)
   {
      std::string replacement(text);
      first = clamp(first);
      last = clamp(last);
      if (first > last)
         std::swap(first, last);
      if (first == last)
         return insert(first, replacement, style);

      auto count = last.block - first.block + 1;
      record(first.block, count);
      erase_impl(first, last, false);
      auto result = insert_impl(first, replacement, false, &style);
      _undo.back().new_count = result.second - result.first;
      return result;
   }

   text_style text_document::style_at(position pos) const
   {
      pos = clamp(pos);
      auto const& block = _blocks[pos.block];
      std::size_t base = 0;
      if (block.spans.empty())
         return {};
      for (auto const& span : block.spans)
      {
         auto span_last = base + span.text.size();
         if (pos.offset <= span_last)
            return span.style();
         base = span_last;
      }
      return block.spans.back().style();
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::set_style(position first, position last, text_style style)
   {
      first = clamp(first);
      last = clamp(last);
      if (first > last)
         std::swap(first, last);
      if (first == last)
         return {first.block, first.block + 1};

      bool changed = false;
      for (auto i = first.block; i <= last.block; ++i)
      {
         auto begin = i == first.block? first.offset : 0;
         auto end = i == last.block? last.offset : _blocks[i].size();
         if (range_needs_style(_blocks[i], begin, end, style))
         {
            changed = true;
            break;
         }
      }
      if (!changed)
         return {first.block, last.block + 1};

      record(first.block, last.block - first.block + 1);
      for (auto i = first.block; i <= last.block; ++i)
      {
         auto begin = i == first.block? first.offset : 0;
         auto end = i == last.block? last.offset : _blocks[i].size();
         style_range(_blocks[i], begin, end, style);
      }
      return {first.block, last.block + 1};
   }

   std::pair<text_document::size_type, text_document::size_type>
   text_document::set_type(size_type block, block_type type)
   {
      CYCFI_ASSERT(block < _blocks.size(), "Block index out of range");
      if (_blocks[block].type == type)
         return {block, block + 1};
      record(block, 1);
      _blocks[block].type = type;
      if (type != block_type::code_block)
      {
         for (auto& span : _blocks[block].spans)
            std::replace(span.text.begin(), span.text.end(), '\n', ' ');
         merge_spans(_blocks[block]);
      }
      return {block, block + 1};
   }

   void text_document::undo()
   {
      if (_undo.empty())
         return;

      auto e = std::move(_undo.back());
      _undo.pop_back();

      undo_entry r;
      r.first = e.first;
      r.snapshot.assign(_blocks.begin() + e.first,
         _blocks.begin() + e.first + e.new_count);
      r.new_count = e.snapshot.size();
      _redo.push_back(std::move(r));

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
