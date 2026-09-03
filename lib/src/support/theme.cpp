/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements/support/theme.hpp>
#include <elements/element/dial.hpp>
#include <elements/view.hpp>

namespace cycfi::elements
{
   theme::theme()
    : panel_color                {rgba(28, 30, 34, 192)}
    , window_background_color    {rgba(35, 35, 37, 255)}
    , frame_color                {rgba(220, 220, 220, 80)}
    , frame_hilite_color         {rgba(220, 220, 220, 160)}
    , frame_shadow_color         {rgba(0, 0, 0, 102)}
    , frame_corner_radius        {3.0}
    , frame_stroke_width         {1.0}
    , scrollbar_color            {rgba(80, 80, 80, 80)}
    , scrollbar_width            {10}
    , default_button_color       {rgba(0, 0, 0, 50)}
    , button_margin              {10, 5, 10, 5}
    , button_corner_radius       {4.0}
    , button_text_icon_space     {8.0}
    , slide_button_size          {30, 16}
    , slide_button_on_color      {rgba(0, 127, 255, 200)}
    , slide_button_base_color    {rgba(127, 127, 127, 100)}
    , slide_button_thumb_color   {rgba(240, 240, 240, 200)}
    , active_tab_color           {colors::gray[50]}

    , controls_color             {rgba(18, 49, 85, 200)}
    , controls_frame_stroke_width{1.5}
    , indicator_color            {rgba(0, 127, 255, 200)}
    , indicator_bright_color     {indicator_color.level(1.5)}
    , indicator_hilite_color     {indicator_color.level(2.0)}
    , basic_font_color           {rgba(220, 220, 220, 200)}
    , disabled_opacity           {0.45}

// The symbols_font font is the OS supplied font that includes unicode symbols
// such as Miscellaneous Technical : Unicode U+2300 – U+23FF (8960–9215)
#if defined(__APPLE__)
    , system_font                {font_descr{"Lucida Grande"}}
#elif defined(_WIN32)
    , system_font                {font_descr{"Segoe UI Symbol"}}
#elif defined(__linux__)
    , system_font                {font_descr{"DejaVu Sans"}}
#endif

    , element_background_opacity {32.0f / 255.0f}

    , heading_font_color         {basic_font_color}
    , heading_font               {font_descr{"Roboto", 15.0}.medium()}
    , heading_text_align         {canvas::middle | canvas::center}

    , label_font_color           {basic_font_color}
    , label_font                 {font_descr{"Open Sans", 14.0}}
    , label_text_align           {canvas::middle | canvas::center}

    , icon_color                 {basic_font_color}
    , icon_font                  {font_descr{"elements_basic", 16.0}}
    , icon_button_color          {default_button_color}

    , text_box_font_color        {basic_font_color}
    , text_box_font              {font_descr{"Open Sans", 14.0}}
    , text_box_hilite_color      {rgba(0, 127, 255, 100)}
    , text_box_caret_color       {rgba(0, 190, 255, 255)}
    , text_box_caret_width       {1.2}
    , inactive_font_color        {rgba(127, 127, 127, 150)}
    , input_box_text_limit       {1024}

    , mono_spaced_font           {font_descr{"Roboto Mono", 14.0}}

    , ticks_color                {rgba(127, 127, 127, 150)}
    , major_ticks_level          {0.5}
    , major_ticks_width          {1.5}
    , minor_ticks_level          {0.4}
    , minor_ticks_width          {0.7}

    , major_grid_color           {frame_color}
    , major_grid_width           {0.5}
    , minor_grid_color           {indicator_color}
    , minor_grid_width           {0.4}

    , dialog_button_size         {100}
    , message_textbox_size       {{300, 120}}

    , dial_mode                  {dial_mode_enum::linear}
    , dial_linear_range          {200}

    , child_window_title_size    {1.0}
    , child_window_opacity       {0.95}

    , slider_track_color         {rgba(0, 0, 0, 255)}
    , slider_thumb_color         {rgba(0, 0, 0, 255)}
   {
   }

   namespace
   {
      bool same_font(font_descr const& a, font_descr const& b)
      {
         return a._families == b._families
            && a._size == b._size
            && a._weight == b._weight
            && a._slant == b._slant
            && a._stretch == b._stretch;
      }
   }

   bool theme::layout_changed(theme const& other) const
   {
      // Note: child_window_title_size / dialog_button_size /
      // message_textbox_size are baked when their controls are created,
      // so changing them does not invalidate existing layouts and is
      // intentionally NOT compared here.
      return
         !same_font(system_font, other.system_font)
      || !same_font(heading_font, other.heading_font)
      || !same_font(label_font, other.label_font)
      || !same_font(icon_font, other.icon_font)
      || !same_font(text_box_font, other.text_box_font)
      || !same_font(mono_spaced_font, other.mono_spaced_font)
      || scrollbar_width != other.scrollbar_width
      || button_margin != other.button_margin
      || slide_button_size != other.slide_button_size
      || button_text_icon_space != other.button_text_icon_space
      ;
   }

   theme make_dark_theme()
   {
      return theme{};
   }

   theme make_light_theme()
   {
      theme thm;

      thm.panel_color                = rgba(242, 243, 247, 225);
      thm.window_background_color    = rgba(232, 233, 238, 255);
      thm.frame_color                = rgba(0, 0, 0, 60);
      thm.frame_hilite_color         = rgba(0, 0, 0, 120);
      thm.frame_shadow_color         = rgba(0, 0, 0, 40);
      thm.scrollbar_color            = rgba(0, 0, 0, 60);
      thm.default_button_color       = rgba(0, 0, 0, 25);
      thm.slide_button_on_color      = rgba(0, 110, 230, 220);
      thm.slide_button_base_color    = rgba(0, 0, 0, 50);
      thm.slide_button_thumb_color   = rgba(255, 255, 255, 255);
      thm.active_tab_color           = rgba(0, 0, 0, 90);

      thm.controls_color             = rgba(40, 90, 140, 230);
      thm.indicator_color            = rgba(0, 110, 230, 220);
      // On a light background, brighter = closer to white = less visible.
      // Explicitly pick darker, more saturated blues for the bright/hilite
      // variants instead of the dark theme's level(>1) derivation.
      thm.indicator_bright_color     = rgba(0, 90, 190, 255);
      thm.indicator_hilite_color     = rgba(0, 60, 150, 255);
      thm.basic_font_color           = rgba(20, 20, 24, 230);

      thm.heading_font_color         = thm.basic_font_color;
      thm.label_font_color           = thm.basic_font_color;
      thm.icon_color                 = thm.basic_font_color;

      thm.text_box_font_color        = thm.basic_font_color;
      thm.text_box_hilite_color      = rgba(0, 110, 230, 60);
      thm.text_box_caret_color       = rgba(0, 90, 200, 255);
      thm.inactive_font_color        = rgba(0, 0, 0, 90);

      thm.ticks_color                = rgba(0, 0, 0, 90);
      thm.major_grid_color           = rgba(0, 0, 0, 60);
      thm.minor_grid_color           = thm.indicator_color;

      thm.slider_track_color         = rgba(0, 0, 0, 50);
      thm.slider_thumb_color         = rgba(30, 30, 34, 255);

      return thm;
   }

   // The global theme
   theme& global_theme::_theme()
   {
      static theme thm;
      return thm;
   }

   theme const& get_theme()
   {
      return global_theme::_theme();
   }

   void set_theme(theme const& thm)
   {
      auto& current = global_theme::_theme();
      bool relayout = thm.layout_changed(current);
      current = thm;

      // Iterate over a snapshot: a layout() pass may indirectly create or
      // destroy views, which would invalidate iterators into the live list.
      auto reg = view::views();
      for (auto* v : reg)
      {
         if (relayout)
            v->layout();   // layout() also refreshes
         else
            v->refresh();
      }
   }
}
