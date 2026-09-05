/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements/element/style/slide_switch.hpp>

namespace cycfi::elements
{
   view_limits slide_switch_styler::limits(basic_context const& /*ctx*/) const
   {
      auto& thm = get_theme();
      auto  size = thm.slide_button_size;
      return {{size.x, size.y}, {size.x, size.y}};
   }

   void slide_switch_styler::draw(context const& ctx)
   {
      auto btn = find_parent<basic_button*>(ctx);
      if (!btn)
         return;

      auto& canvas_ = ctx.canvas;
      auto canvas_state = canvas_.new_state();
      auto const& theme_ = get_theme();
      auto bounds = ctx.bounds;
      auto height = bounds.height();
      auto radius = height/2;

      auto value = btn->value();
      auto enabled = ctx.enabled;

      // Animate sliding
      auto diff = value - _val;
      if (std::abs(diff) > 0.1)
      {
         constexpr auto alpha = 0.3;
         _val += alpha * diff;
         ctx.view.refresh(ctx);
      }
      else
      {
         _val = value;
      }

      if (theme_.ui_style == ui_style_enum::winui_style)
      {
         // WinUI ToggleSwitch: accent pill when on, neutral pill with a
         // border when off, and a knob that swells in the on state.
         // (Fluent keeps the knob at a fixed 12/40th of the track width;
         // the swell approximates its on-state elevation ring.)
         auto t = _val < 0.0f? 0.0f : (_val > 1.0f? 1.0f : _val);
         auto const& on_c  = theme_.slide_button_on_color;
         auto const& off_c = theme_.slide_button_base_color;
         auto track_c = color{
            on_c.red*t + off_c.red*(1-t)
          , on_c.green*t + off_c.green*(1-t)
          , on_c.blue*t + off_c.blue*(1-t)
          , on_c.alpha*t + off_c.alpha*(1-t)
         };
         auto knob_r = height * (0.3f + 0.075f*t);
         auto knob_c = theme_.slide_button_thumb_color;
         auto off_stroke = is_light_theme(theme_)?
            colors::black.opacity(0.2) : colors::white.opacity(0.5)
            ;
         auto knob_stroke = colors::black.opacity(0.25);

         if (!enabled)
         {
            track_c = track_c.opacity(track_c.alpha * theme_.disabled_opacity);
            knob_c = knob_c.opacity(knob_c.alpha * theme_.disabled_opacity);
            off_stroke = off_stroke.opacity(off_stroke.alpha * theme_.disabled_opacity);
            knob_stroke = knob_stroke.opacity(knob_stroke.alpha * theme_.disabled_opacity);
         }

         auto r = bounds.inset(0.5, 0.5);
         canvas_.begin_path();
         canvas_.add_round_rect(r, radius);
         canvas_.fill_style(track_c);
         canvas_.fill();

         if (!value)
         {
            canvas_.begin_path();
            canvas_.add_round_rect(r, radius);
            canvas_.line_width(1);
            canvas_.stroke_style(off_stroke);
            canvas_.stroke();
         }

         auto span = (bounds.right-bounds.left) - (2*knob_r);
         auto xpos = bounds.left + knob_r + (span * _val);
         canvas_.begin_path();
         canvas_.add_circle({xpos, bounds.top+radius, knob_r});
         canvas_.fill_style(knob_c);
         canvas_.fill();
         canvas_.line_width(1);
         canvas_.stroke_style(knob_stroke);
         canvas_.stroke();
         return;
      }

      auto color = value?
         theme_.slide_button_on_color :
         theme_.slide_button_base_color
         ;
      if (!enabled)
         color = color.opacity(color.alpha * theme_.disabled_opacity);

      // Draw base
      canvas_.begin_path();
      canvas_.add_round_rect(bounds, radius);
      canvas_.fill_style(color);
      canvas_.fill();

      color = theme_.slide_button_thumb_color;
      if (!enabled)
         color = color.opacity(color.alpha * theme_.disabled_opacity);

      auto span = (bounds.right-bounds.left)-height; // height == diameter of thumb
      auto xpos = bounds.left + radius + (span * _val);

      // Draw the thumb
      canvas_.begin_path();
      canvas_.add_circle({xpos, bounds.top+radius, radius-1.5f});
      canvas_.fill_style(color);
      canvas_.fill();
   }

   bool slide_switch_styler::wants_control() const
   {
      return true;
   }
}
