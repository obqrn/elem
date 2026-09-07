/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_TEXT_EDITOR_SEPTEMBER_6_2026)
#define ELEMENTS_TEXT_EDITOR_SEPTEMBER_6_2026

#include <elements/element/element.hpp>
#include <elements/element/rich_text.hpp>
#include <elements/element/text_document.hpp>
#include <cstddef>
#include <memory>
#include <vector>

namespace cycfi::elements
{
   /**
    * \class text_editor_element
    *
    * \brief
    *    A block-structured plain-text editor over a text_document. Each
    *    block is laid out with a rich_text_layout (single span per block,
    *    styled by the block type: headings use the heading font). Editing
    *    is paragraph-incremental: a keystroke re-lays out only the touched
    *    block(s).
    *
    *    The editor supports a caret, a selection (mouse and keyboard),
    *    undo/redo, and the clipboard. IME input arrives via the text event
    *    (one codepoint per event).
    *
    *    Note: the document is shared; external document changes require
    *    refresh_layouts() before the next draw.
    */
   class text_editor_element : public element
   {
   public:

      explicit          text_editor_element(
                           std::shared_ptr<text_document> doc
                         , float width = full_extent
                        );

      // Move-only: the block render cache holds unique pixmaps.
                           text_editor_element(text_editor_element&&) = default;
      text_editor_element& operator=(text_editor_element&&) = default;
                           text_editor_element(text_editor_element const&) = delete;
      text_editor_element& operator=(text_editor_element const&) = delete;

      view_limits       limits(basic_context const& ctx) const override;
      void              layout(context const& ctx) override;
      void              draw(context const& ctx) override;

      bool              click(context const& ctx, mouse_button btn) override;
      void              drag(context const& ctx, mouse_button btn) override;
      bool              cursor(context const& ctx, point p, cursor_tracking status) override;
      bool              key(context const& ctx, key_info k) override;
      bool              text(context const& ctx, text_info info) override;

      bool              wants_focus() const override   { return true; }
      bool              wants_control() const override { return true; }
      void              begin_focus(focus_request req) override;
      bool              end_focus() override;

      text_document&    document()                { return *_doc; }
      text_document const& document() const       { return *_doc; }

      // Re-layout everything. Call after mutating the document from the
      // outside (e.g. a markdown import); edits made through this editor
      // re-layout incrementally.
      void              refresh_layouts();

   private:

      using position = text_document::position;

      void              ensure_layouts() const;
      void              relayout_blocks(std::size_t first, std::size_t last) const;
      void              recompute_block_y() const;

      position          position_at(point p) const;
      position          move_left(position p) const;
      position          move_right(position p) const;
      position          move_vertical(context const& ctx, position p, int dir) const;
      void              clamp_caret();

      void              insert_text(context const& ctx, string_view s);
      void              erase_selection(context const& ctx);
      std::string       selection_text() const;
      void              select_all();

      // Caret/selection helpers
      position          sel_min() const;
      position          sel_max() const;
      rect              caret_rect() const;
      rect              selection_bounds() const;

      // Line height at a position: exact when the layout has a line for
      // it, the block font size otherwise (the refresh rect is only ever
      // slightly over-sized).
      float             caret_line_height(position p) const;

      // The local (content-space) rect of a block.
      rect              block_rect(std::size_t i) const;

      // Repaint the union of the old and new caret/selection areas. Used
      // for clicks, drags and keyboard caret moves: the paint area never
      // grows past the caret and selection extents, so these repaints do
      // not touch unrelated content (no flicker).
      void              repaint_caret(
                           context const& ctx, rect old_caret, rect old_sel) const;

      // Repaint after a document edit. Covers the old/new caret and
      // selection plus the affected blocks [first, last). If the bottom of
      // the affected range moved (its height changed), everything below
      // shifted too, so the repaint extends to the content bottom.
      void              repaint_edit(
                           context const& ctx, rect old_caret, rect old_sel
                         , std::size_t first, std::size_t last
                         , float anchor_before) const;

      std::shared_ptr<text_document>   _doc;
      float                            _width;
      position                         _caret{0, 0};
      position                         _anchor{0, 0};
      bool                             _has_selection = false;
      bool                             _is_focus = false;
      mutable float                    _desired_x = -1; // vertical navigation

      mutable std::vector<rich_text_layout> _layouts;
      mutable std::vector<float>            _block_y;
      mutable bool                          _layouts_valid = false;

      // Per-block prerendered pixmaps: a repaint blits them instead of
      // re-shaping and re-rasterizing the text (keeps repaints fast and
      // visually stable).
      mutable std::vector<std::unique_ptr<pixmap>> _block_cache;
      mutable std::vector<bool>                    _cache_valid;
   };

   text_editor_element text_editor(
      std::shared_ptr<text_document> doc
    , float width = full_extent
   );
}

#endif
