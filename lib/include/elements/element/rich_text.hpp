/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_RICH_TEXT_SEPTEMBER_5_2026)
#define ELEMENTS_RICH_TEXT_SEPTEMBER_5_2026

#include <elements/support/canvas.hpp>
#include <elements/support/color.hpp>
#include <elements/support/font.hpp>
#include <elements/base_view.hpp>
#include <elements/element/element.hpp>
#include <elements/element/text_style.hpp>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace cycfi::elements
{
   /**
    * \class rich_text_layout
    *
    * \brief
    *    The layout engine for styled text. It takes a sequence of text
    *    spans, performs greedy word wrapping across span boundaries, and
    *    draws the result line by line. All measurement and drawing go
    *    through the canvas abstraction (measure_text / fill_text); no
    *    backend-specific types leak out of this class.
    *
    *    Layout rules (aligned with the single-style text break_lines
    *    behavior):
    *       - Words are broken at whitespace boundaries. A word that is
    *         wider than the line is hard-broken character by character.
    *       - A hard newline ends the line immediately; consecutive
    *         newlines produce empty lines. Spaces after a hard newline are
    *         preserved (they are significant in markdown, e.g. indented
    *         code blocks).
    *       - Spaces pending at a wrap-induced line end are dropped
    *         (wrapped lines never start with a space).
    *       - Line height is the max of the span font heights; spans share
    *         a common baseline equal to the max ascent of the line.
    *
    *    An empty line gets its height from the current span's font
    *    metrics.
    *
    *    A single character wider than the line is placed anyway (the line
    *    may exceed the width); this avoids an infinite loop.
    */
   class rich_text_layout
   {
   public:

      struct segment
      {
         std::size_t  span;    // Index into the spans vector
         std::size_t  first;   // Byte offset of the segment start
         std::size_t  last;    // Byte offset of the segment end (half-open)
         float        x;       // Horizontal offset within the line
      };

      struct line
      {
         std::vector<segment>  segments;
         float                 width = 0;       // rendered glyph advance
         float                 caret_width = 0; // includes dropped spaces
         float                 ascent = 0;
         float                 descent = 0;
      };

                           rich_text_layout() = default;
      explicit             rich_text_layout(std::vector<text_span> spans)
                            : _spans(std::move(spans))
                           {}

      void                 layout(float width);
      void                 draw(
                              canvas& cnv, point pos
                            , int align = canvas::left
                           );

      // Edit-model queries. Offsets are byte offsets into the concatenated
      // span text (i.e. the layout's own byte space, one byte per span text
      // byte). Positions are relative to the layout origin (top-left).
      // x_at/byte_at use measured UTF-8 codepoint boundaries, so caret
      // placement and hit testing do not treat a multibyte character as
      // several equal-width bytes.
      std::size_t          byte_at(point p) const;
      float                x_at(std::size_t byte) const;
      std::size_t          line_at(std::size_t byte) const;
      point                caret_pos(std::size_t byte) const;
      // Return the half-open byte interval associated with a visual line.
      // It includes the hard-newline byte and any whitespace omitted at a
      // wrap boundary; an out-of-range line returns {0, 0}.
      std::pair<std::size_t, std::size_t>
                           line_range(std::size_t line) const;

      point                size() const       { return _size; }
      std::vector<line> const& lines() const  { return _lines; }
      std::vector<text_span> const& spans() const { return _spans; }

   private:

      struct caret_point
      {
         std::size_t  byte;
         float        x;
      };

      std::size_t          span_base(std::size_t span) const;
      std::size_t          total_size() const;
      std::size_t          snap_byte(std::size_t byte) const;

      std::vector<text_span>  _spans;
      std::vector<font>       _fonts;  // one resolved font per span, cached
                                       // at layout time (fontconfig lookup
                                       // happens once, not per draw)
      std::vector<std::size_t> _span_bases; // concatenated byte offsets
      std::vector<line>       _lines;
      // Half-open byte interval associated with each visual line. The end is
      // the next line's start, so it includes a hard-newline byte and spaces
      // omitted at a wrap boundary. The final line ends at total_size().
      std::vector<std::pair<std::size_t, std::size_t>> _line_ranges;
      std::vector<std::vector<caret_point>> _line_carets;
      point                   _size = {};
   };

   /**
    * \class rich_text_element
    *
    * \brief
    *    A read-only rich text element. The width constrains the text;
    *    pass full_extent for a single, unwrapped line. The align parameter
    *    follows the canvas::text_alignment semantics (horizontal:
    *    left/center/right; vertical: top/middle/bottom).
    */
   class rich_text_element : public element
   {
   public:

                           rich_text_element(
                              std::vector<text_span> spans
                            , float width = full_extent
                            , int align = canvas::left
                           );

      view_limits          limits(basic_context const& ctx) const override;
      void                 draw(context const& ctx) override;

   private:

      void                 layout_(float width) const;

      mutable rich_text_layout _layout;
      float                _width;
      int                  _align;
      mutable float        _laid_width = -1;
   };

   rich_text_element rich_text(
      std::vector<text_span>  spans
    , float                   width = full_extent
    , int                     align = canvas::left
   );
}

#endif
