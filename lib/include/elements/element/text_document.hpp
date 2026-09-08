/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_TEXT_DOCUMENT_SEPTEMBER_6_2026)
#define ELEMENTS_TEXT_DOCUMENT_SEPTEMBER_6_2026

#include <elements/element/text_style.hpp>
#include <infra/string_view.hpp>
#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace cycfi::elements
{
   /**
    * \enum block_type
    *
    * \brief
    *    The structural type of a text block. A hard newline always starts a
    *    new block; the block type carries the markdown-level semantics
    *    (headings, lists, quotes, code) on top of the flat block list.
    */
   enum class block_type : std::uint8_t
   {
      paragraph,
      heading1, heading2, heading3, heading4, heading5, heading6,
      list_item,
      quote,
      code_block
   };

   /**
    * \struct text_block
    *
    * \brief
    *    A block of editable rich text. The spans are stored in document
    *    order and adjacent spans with the same style are normalized together.
    *    A hard newline is a block separator for non-code blocks; code blocks
    *    may contain literal newlines.
    */
   struct text_block
   {
      block_type             type = block_type::paragraph;
      std::vector<text_span> spans;

                           text_block() = default;
                           text_block(block_type type_)
                            : type(type_)
                           {}
                           text_block(block_type type_, std::string text_);
                           text_block(block_type type_,
                              std::vector<text_span> spans_)
                            : type(type_)
                            , spans(std::move(spans_))
                           {}
                           text_block(block_type type_,
                              std::initializer_list<text_span> spans_)
                            : type(type_)
                            , spans(spans_)
                           {}

      std::size_t           size() const;
      std::string           plain_text() const;
   };

   /**
    * \class text_document
    *
    * \brief
    *    A flat, block-structured rich text document with editing primitives
    *    and undo/redo. The document owns its blocks; editor elements are
    *    views over it. Positions remain byte offsets into the concatenated
    *    text of a block, so style-run boundaries do not affect navigation.
    *
    *    Editing primitives are the only mutation entry points (the blocks
    *    are exposed read-only), so every mutation is undoable. Undo uses
    *    block-level snapshots: an edit snapshots only the blocks it touches,
    *    so undo cost is independent of the document size. A character edit
    *    changes only the affected block's run vector; untouched blocks and
    *    runs keep their allocations.
    */
   class text_document
   {
   public:

      using size_type = std::size_t;

      struct position
      {
         size_type  block = 0;
         size_type  offset = 0;

         auto operator<=>(position const&) const = default;
      };

                           text_document();
      explicit             text_document(std::vector<text_block> blocks);

      // Read access. The blocks are read-only from the outside: mutate
      // through the editing primitives below.
      std::vector<text_block> const&
                           blocks() const          { return _blocks; }
      size_type            size() const            { return _blocks.size(); }
      bool                 empty() const           { return _blocks.empty(); }
      std::size_t          text_size() const;

      // Offsets: document-level byte offsets across blocks, and
      // (block, offset) positions. Offsets do not count the implicit
      // newline between blocks.
      size_type            block_at(std::size_t doc_off) const;
      std::size_t          doc_offset(position pos) const;
      position             clamp(position pos) const;

      // Editing primitives. Each returns the range of affected blocks
      // [first, last) so views can re-layout incrementally. A `\n` inside
      // the inserted text splits blocks, except inside a code_block where
      // it is kept as a literal newline. New text inherits the style at the
      // insertion point unless an explicit style is supplied.
      std::pair<size_type, size_type>
                           insert(position pos, string_view text);
      std::pair<size_type, size_type>
                           insert(position pos, string_view text,
                              text_style style);
      std::pair<size_type, size_type>
                           erase(position first, position last);
      // Replace a range as one undoable operation. Newline normalization and
      // block handling follow insert().
      std::pair<size_type, size_type>
                           replace(position first, position last, string_view text);
      std::pair<size_type, size_type>
                           replace(position first, position last,
                              string_view text, text_style style);
      // Return the stored style at a caret position. An empty font family or
      // zero-alpha color means the editor/theme default is inherited.
      text_style           style_at(position pos) const;
      // Apply one style to the selected text range. The implicit newline
      // between blocks is not styled. A collapsed range is a no-op.
      std::pair<size_type, size_type>
                           set_style(position first, position last,
                              text_style style);
      std::pair<size_type, size_type>
                           set_type(size_type block, block_type type);

      // Undo/redo
      void                 undo();
      void                 redo();
      bool                 can_undo() const       { return !_undo.empty(); }
      bool                 can_redo() const       { return !_redo.empty(); }

   private:

      struct undo_entry
      {
         size_type                first;     // First affected block
         std::vector<text_block>  snapshot;  // Original state of the range
         size_type                new_count; // Blocks after the edit
      };

      static constexpr std::size_t max_undo_depth = 1000;

      // Record the state of [first, first + count) before an edit and
      // install the entry on the undo stack (clearing redo).
      void                 record(size_type first, size_type count);
      std::pair<size_type, size_type>
                           insert_impl(position pos, string_view text,
                              bool record_undo,
                              text_style const* style);
      std::pair<size_type, size_type>
                           erase_impl(position first, position last,
                              bool record_undo);

      std::vector<text_block>  _blocks;
      std::vector<undo_entry>  _undo;
      std::vector<undo_entry>  _redo;
   };
}

#endif
