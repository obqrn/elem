/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements/element/style/check_box.hpp>

namespace cycfi::elements
{
   void check_box_styler::draw(context const& ctx)
   {
      auto btn = find_parent<basic_button*>(ctx);
      if (!btn)
         return;

      auto& canvas_ = ctx.canvas;
      auto canvas_state = canvas_.new_state();
      auto const& theme_ = get_theme();
      rect box = ctx.bounds.move(15, 0);

      box.width(box.height());

      auto value = btn->value();
      auto hilite = btn->hilite();
      auto tracking = btn->tracking();
      auto enabled = ctx.enabled;

      if (theme_.ui_style == ui_style_enum::winui_style)
      {
         // WinUI CheckBox: rounded square, solid accent fill with a white
         // check mark when checked, subtle fill with a border otherwise.
         bool light = is_light_theme(theme_);

         auto body_r = box.inset(1, 1);
         color fill_c;
         color stroke_c;
         if (value || tracking)
         {
            fill_c = theme_.accent_color;
            if (tracking)
               fill_c = fill_c.level(0.8);
            stroke_c = fill_c;
         }
         else
         {
            fill_c = light? colors::white : colors::white.opacity(0.06);
            stroke_c = light?
               colors::black.opacity(0.6) :
               colors::white.opacity(0.55)
               ;
            if (hilite && enabled)
               stroke_c = theme_.accent_color;
         }

         if (!enabled)
         {
            fill_c = fill_c.opacity(fill_c.alpha * theme_.disabled_opacity);
            stroke_c = stroke_c.opacity(stroke_c.alpha * theme_.disabled_opacity);
         }

         canvas_.begin_path();
         canvas_.add_round_rect(body_r, 2);
         canvas_.fill_style(fill_c);
         canvas_.fill();

         canvas_.begin_path();
         canvas_.add_round_rect(body_r, 2);
         canvas_.line_width(1);
         canvas_.stroke_style(stroke_c);
         canvas_.stroke();

         if (value || tracking)
         {
            auto icon_c = colors::white;
            if (!enabled)
               icon_c = icon_c.opacity(theme_.disabled_opacity);
            draw_icon(canvas_, box, icons::ok, 14, icon_c);
         }
      }
      else
      {
         color outline_color = (enabled && hilite)?
            theme_.frame_hilite_color :
            theme_.frame_color;

         if (!enabled)
            outline_color = outline_color.opacity(
               outline_color.alpha * theme_.disabled_opacity);

         // Draw check mark
         if (enabled)
         {
            color icon_c = (value || tracking) ?
               ((enabled && hilite)?
                  theme_.indicator_hilite_color : theme_.indicator_bright_color) :
               theme_.basic_font_color.opacity(theme_.element_background_opacity)
               ;

            if (tracking)
               icon_c = icon_c.level(0.2);

            if (value || tracking)
               draw_icon(canvas_, box, icons::ok, 14, icon_c);
         }
         else
         {
            if (value)
               draw_icon(canvas_, box, icons::ok, 14, outline_color);
         }

         // Draw box
         auto line_width = theme_.controls_frame_stroke_width;

         canvas_.line_width(line_width);
         canvas_.begin_path();
         canvas_.add_round_rect(box.inset(1, 1), 3);
         canvas_.stroke_style(outline_color);
         canvas_.stroke();

         // Pseudo glow
         if (enabled)
         {
            auto glow_width = hilite? line_width*2 : line_width;
            auto inset = glow_width/3;
            auto glow_box = box.inset(inset, inset);
            canvas_.add_round_rect(glow_box, 4);
            canvas_.line_width(glow_width);
            canvas_.stroke_style(outline_color.opacity(0.1));
            canvas_.stroke();
         }
      }

      // Draw text
      auto text_c = enabled?
         theme_.label_font_color :
         theme_.label_font_color.opacity(
            theme_.label_font_color.alpha * theme_.disabled_opacity);
      canvas_.fill_style(text_c);
      canvas_.font(theme_.label_font);
      canvas_.text_align(canvas_.left | canvas_.middle);
      float cx = box.right + 10;
      float cy = ctx.bounds.top + (ctx.bounds.height() / 2);
      canvas_.fill_text(_text.c_str(), point{cx, cy});
   }
}
