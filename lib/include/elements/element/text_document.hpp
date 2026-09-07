/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_TEXT_DOCUMENT_SEPTEMBER_6_2026)
#define ELEMENTS_TEXT_DOCUMENT_SEPTEMBER_6_2026

#include <infra/string_view.hpp>
#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
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
    *    A single paragraph of text. A hard newline is a block separator:
    *    a non-code block never contains one. A code_block is the
    *    exception: its text may contain newlines (Enter inside a code
    *    block inserts a literal newline instead of splitting the block).
    */
   struct text_block
   {
      block_type  type = block_type::paragraph;
      std::string text;
   };

   /**
    * \class text_document
    *
    * \brief
    *    A flat, block-structured text document with editing primitives and
    *    undo/redo. The document owns its blocks; the editor elements are
    *    views over it.
    *
    *    Editing primitives are the only mutation entry points (the blocks
    *    are exposed read-only), so every mutation is undoable. Undo uses
    *    block-level snapshots: an edit snapshots only the blocks it touches
    *    (one block for character edits, two for split/join), so undo cost
    *    is independent of the document size — large documents stay
    *    responsive.
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
      // it is kept as a literal newline. Erasing across a block boundary
      // joins the blocks; if the last block is a code_block the joined
      // block becomes a code_block (it may now hold newlines).
      std::pair<size_type, size_type>
                           insert(position pos, string_view text);
      std::pair<size_type, size_type>
                           erase(position first, position last);
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

      std::vector<text_block>  _blocks;
      std::vector<undo_entry>  _undo;
      std::vector<undo_entry>  _redo;
   };
}

#endif
