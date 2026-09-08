/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_TEXT_STYLE_SEPTEMBER_8_2026)
#define ELEMENTS_TEXT_STYLE_SEPTEMBER_8_2026

#include <elements/support/color.hpp>
#include <elements/support/font.hpp>
#include <string>

namespace cycfi::elements
{
   /**
    * \struct text_style
    *
    * \brief
    *    The style carried by an editable text run. An empty font family or a
    *    zero-alpha color means inherit the block/theme default in an editor.
    */
   struct text_style
   {
      font_descr  font_;
      color       color_ = {};
   };

   /**
    * \struct text_span
    *
    * \brief
    *    A run of text with one inline style. It is shared by read-only rich
    *    text layouts and the editable text document.
    */
   struct text_span
   {
      std::string  text;
      font_descr   font_;
      color        color_ = colors::black;

      text_style   style() const { return {font_, color_}; }
   };

   inline bool operator==(text_style const& a, text_style const& b)
   {
      return a.font_._families == b.font_._families
         && a.font_._size == b.font_._size
         && a.font_._weight == b.font_._weight
         && a.font_._slant == b.font_._slant
         && a.font_._stretch == b.font_._stretch
         && a.color_ == b.color_;
   }

   inline bool operator!=(text_style const& a, text_style const& b)
   {
      return !(a == b);
   }
}

#endif
