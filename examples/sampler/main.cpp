/*=============================================================================
   Elements Sampler — all main example features aggregated into a single
   window, organized as notebook pages (tabs on the left).

   Pages are adapted from the individual examples under ../.
=============================================================================*/
#include <elements.hpp>
#include <infra/support.hpp>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <cmath>
#include <array>
#include <functional>
#include <vector>
#include <chrono>
#include <iostream>

using namespace cycfi::elements;
using namespace std::chrono_literals;

// Background that follows the active theme's window_background_color
struct themed_background : element
{
   void draw(context const& ctx) override
   {
      auto& cnv = ctx.canvas;
      cnv.begin_path();
      cnv.add_rect(ctx.bounds);
      cnv.fill_style(get_theme().window_background_color);
      cnv.fill();
   }
};

///////////////////////////////////////////////////////////////////////////////
// Page: Basic Sliders & Knobs (examples/basic_sliders_and_knobs)
///////////////////////////////////////////////////////////////////////////////
namespace ns_sliders
{
   using slider_ptr = std::shared_ptr<basic_slider_base>;
   slider_ptr hsliders[3];
   slider_ptr vsliders[3];

   using dial_ptr = std::shared_ptr<basic_dial>;
   dial_ptr dials[3];

   template <bool is_vertical>
   auto make_markers()
   {
      auto track = basic_track<5, is_vertical>();
      return slider_labels<10>(
         slider_marks_lin<40>(track), 0.8,
         "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"
      );
   }

   auto make_hslider(int index)
   {
      hsliders[index] = share(
         slider(
            basic_thumb<25>(),
            make_markers<false>(),
            (index + 1) * 0.25
         )
      );
      return align_middle(hmargin({20, 20}, hold(hsliders[index])));
   }

   auto make_hsliders()
   {
      return hmin_size(300,
         vtile(make_hslider(0), make_hslider(1), make_hslider(2))
      );
   }

   auto make_vslider(int index)
   {
      vsliders[index] = share(
         slider(
            basic_thumb<25>(),
            make_markers<true>(),
            (index + 1) * 0.25
         )
      );
      return align_center(vmargin({20, 20}, hold(vsliders[index])));
   }

   auto make_vsliders()
   {
      return hmin_size(300,
         htile(make_vslider(0), make_vslider(1), make_vslider(2))
      );
   }

   auto make_dial(int index)
   {
      dials[index] = share(
         dial(
            radial_marks<20>(basic_knob<50>()),
            (index + 1) * 0.25
         )
      );

      auto markers = radial_labels<15>(
         hold(dials[index]), 0.7,
         "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"
      );

      return align_center_middle(markers);
   }

   auto make_dials()
   {
      return hmargin(20,
         vtile(make_dial(0), make_dial(1), make_dial(2))
      );
   }

   auto make_controls()
   {
      return
         margin({20, 10, 20, 10},
            vmin_size(400,
               htile(
                  margin({20, 20, 20, 20}, pane("Vertical Sliders", make_vsliders(), 0.8f)),
                  margin({20, 20, 20, 20}, pane("Horizontal Sliders", make_hsliders(), 0.8f)),
                  hstretch(0.5, margin({20, 20, 20, 20}, pane("Knobs", make_dials(), 0.8f)))
               )
            )
         );
   }

   void link_control(int index, view& view_)
   {
      vsliders[index]->on_change =
         [index, &view_](double val)
         {
            hsliders[index]->value(val);
            dials[index]->value(val);
            view_.refresh(*hsliders[index]);
            view_.refresh(*dials[index]);
         };

      hsliders[index]->on_change =
         [index, &view_](double val)
         {
            vsliders[index]->value(val);
            dials[index]->value(val);
            view_.refresh(*vsliders[index]);
            view_.refresh(*dials[index]);
         };

      dials[index]->on_change =
         [index, &view_](double val)
         {
            vsliders[index]->value(val);
            hsliders[index]->value(val);
            view_.refresh(*vsliders[index]);
            view_.refresh(*hsliders[index]);
         };
   }

   void link_controls(view& view_)
   {
      link_control(0, view_);
      link_control(1, view_);
      link_control(2, view_);
   }

   auto make_page(view& view_)
   {
      auto page = make_controls();
      link_controls(view_);
      return page;
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Sprite Sliders & Knobs (examples/sprite_sliders_and_knobs)
///////////////////////////////////////////////////////////////////////////////
namespace ns_sprite_sliders
{
   using slider_ptr = std::shared_ptr<basic_slider_base>;
   slider_ptr vsliders[3];

   using dial_ptr = std::shared_ptr<basic_dial>;
   dial_ptr dials[3];

   template <bool is_vertical>
   auto make_markers()
   {
      auto track = basic_track<5, is_vertical>();
      return slider_labels<10>(
         slider_marks_lin<40>(track), 0.8,
         "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"
      );
   }

   auto make_vslider(int index)
   {
      image slider_knob = image{"slider-white.png", 1.0/4};

      vsliders[index] = share(slider(
         align_center(slider_knob),
         make_markers<true>(),
         (index + 1) * 0.25
      ));
      return align_center(vmargin({20, 20}, hold(vsliders[index])));
   }

   auto make_vsliders()
   {
      return hmin_size(250,
         margin_right(10, htile(
            make_vslider(0),
            make_vslider(1),
            make_vslider(2)
         ))
      );
   }

   auto make_dial(int index)
   {
      float const knob_scale = 1.0/3;
      sprite knob = sprite{
         "knob_sprites_white_128x128.png",
         128 * knob_scale, knob_scale
      };

      dials[index] = share(
         dial(
            radial_marks<15>(knob),
            (index + 1) * 0.25
         )
      );

      auto markers = radial_labels<15>(
         hold(dials[index]), 0.7,
         "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"
      );

      return align_center_middle(markers);
   }

   auto make_dials()
   {
      return hmargin(20,
         vtile(
            make_dial(0),
            make_dial(1),
            make_dial(2)
         )
      );
   }

   auto make_controls()
   {
      return
         margin({20, 10, 20, 10},
            vmin_size(350,
               htile(
                  margin({20, 20, 20, 20}, pane("Sliders", make_vsliders(), 0.8f)),
                  hstretch(0.5, margin({20, 20, 20, 20}, pane("Knobs", make_dials(), 0.8f)))
               )
            )
         );
   }

   void link_control(int index, view& view_)
   {
      vsliders[index]->on_change =
         [index, &view_](double val)
         {
            dials[index]->basic_dial::value(val);
            view_.refresh(*dials[index]);
         };

      dials[index]->on_change =
         [index, &view_](double val)
         {
            vsliders[index]->slider_base::value(val);
            view_.refresh(*vsliders[index]);
         };
   }

   void link_controls(view& view_)
   {
      link_control(0, view_);
      link_control(1, view_);
      link_control(2, view_);
   }

   auto make_page(view& view_)
   {
      auto page = make_controls();
      link_controls(view_);
      return page;
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Range Sliders (examples/range_slider)
///////////////////////////////////////////////////////////////////////////////
namespace ns_range_slider
{
   // Background for the min/max input boxes: follows the active theme.
   // idle = window background color; active = indicator tint; error = red.
   struct input_box_bg : element
   {
      enum state_enum { idle, active, error };
      state_enum _state = idle;

      void draw(context const& ctx) override
      {
         auto& cnv = ctx.canvas;
         auto& thm = get_theme();
         color c;
         switch (_state)
         {
            case active: c = thm.indicator_color.opacity(0.35); break;
            case error:  c = colors::red.opacity(0.35); break;
            default:     c = thm.window_background_color; break;
         }
         cnv.fill_style(c);
         cnv.fill_rect(ctx.bounds);
      }
   };

   auto make_range_slider(view& _view, auto _range_slider, std::string _title, std::string _subtitle)
   {
      auto _min_textbox = share(input_box("min level"));
      auto _max_textbox = share(input_box("max level"));
      auto _min_bg = share(input_box_bg{});
      auto _max_bg = share(input_box_bg{});

      auto pretty_printer =
         [] (float value)
         {
            std::stringstream ss;
            ss << std::setprecision(3) << value;
            return ss.str();
         };

      auto axis_transform =
         [] (float value)
         {
            return value*10;
         };

      auto axis_transform_inv =
         [] (float value)
         {
            return value/10;
         };

      _range_slider->on_change.first =
         [&_view, _range_slider, _min_textbox, pretty_printer, axis_transform] (float value)
         {
            _min_textbox->second->set_text(pretty_printer(axis_transform(value)));
            _view.refresh(_min_textbox->first);
         };
      _range_slider->edit_value_first(_range_slider->value_first());

      _range_slider->on_change.second =
         [&_view, _range_slider, _max_textbox, pretty_printer, axis_transform] (float value)
         {
            _max_textbox->second->set_text(pretty_printer(axis_transform(value)));
            _view.refresh(_max_textbox->first);
         };
      _range_slider->edit_value_second(_range_slider->value_second());

      _min_textbox->second->on_text =
         [_min_bg] (std::string_view text)
         {
            _min_bg->_state = text.empty()?
               input_box_bg::idle : input_box_bg::active;
         };

      _min_textbox->second->on_enter =
         [&_view, _range_slider, _min_bg, axis_transform_inv] (std::string_view text)->bool
         {
            try
            {
               _range_slider->value_first(axis_transform_inv(std::stof(std::string(text))));
               _min_bg->_state = input_box_bg::idle;
               _view.refresh(*_range_slider);
            }
            catch (std::exception&)
            {
               _min_bg->_state = input_box_bg::error;
            }
            _view.refresh(*_min_bg);
            return true;
         };

      _max_textbox->second->on_text =
         [_max_bg] (std::string_view text)
         {
            _max_bg->_state = text.empty()?
               input_box_bg::idle : input_box_bg::active;
         };

      _max_textbox->second->on_enter =
         [&_view, _range_slider, _max_bg, axis_transform_inv] (std::string_view text)->bool
         {
            try
            {
               _range_slider->value_second(axis_transform_inv(std::stof(std::string(text))));
               _max_bg->_state = input_box_bg::idle;
               _view.refresh(*_range_slider);
            }
            catch (std::exception&)
            {
               _max_bg->_state = input_box_bg::error;
            }
            _view.refresh(*_max_bg);
            return true;
         };

      return margin(
         {10, 10, 10, 10},
         layer(
            vtile_spaced(10,
               align_center(
                  label(_title).font_size(18)
               ),
               align_center(
                  label(_subtitle)
               ),
               margin(
                  {50, 10, 50, 0},
                  hold(_range_slider)
               ),
               layer(
                  align_left(
                     margin(
                        {50, 10, 50, 10},
                        hsize(
                           100,
                           layer(
                              link(_min_textbox->first),
                              hold(_min_bg)
                           )
                        )
                     )
                  ),
                  align_right(
                     margin(
                        {50, 10, 50, 10},
                        hsize(
                           100,
                           layer(
                              link(_max_textbox->first),
                              hold(_max_bg)
                           )
                        )
                     )
                  )
               )
            ),
            frame{}
         )
      );
   }

   auto make_log_range_slider(view& _view)
   {
      double min_val = 1e-4;
      double max_val = 1e0;
      auto track = basic_track<5, false>();
      auto _range_slider = share(range_slider(
         basic_thumb<20>(),
         basic_thumb<20>(),
         slider_labels<10>(
            slider_marks_log<20, 4>(track), 0.8, "1e-4", "1e-3", "1e-2", "1e-1", "1e0"
         ),
         {0.1, 0.8},
         +0.5
      ));

      auto _min_textbox = share(input_box("min level"));
      auto _max_textbox = share(input_box("max level"));
      auto _min_bg = share(input_box_bg{});
      auto _max_bg = share(input_box_bg{});

      auto pretty_printer =
         [] (float value)
         {
            std::stringstream ss;
            ss << std::setprecision(2) << std::scientific << value;
            return ss.str();
         };

      auto axis_transform_inv =
         [min_val, max_val] (float value)
         {
            return (std::log10(value)-std::log10(min_val))/(std::log10(max_val)-std::log10(min_val))*(1-0)+0;
         };

      auto axis_transform =
         [min_val, max_val] (float x)
         {
            double logy = (x-0)/(1-0)*(std::log10(max_val)-std::log10(min_val)) + std::log10(min_val);
            return std::pow(10, logy);
         };

      _range_slider->on_change.first =
         [&_view, _min_textbox, pretty_printer, axis_transform] (float value)
         {
            _min_textbox->second->set_text(pretty_printer(axis_transform(value)));
            _view.refresh(_min_textbox->first);
         };
      _range_slider->edit_value_first(_range_slider->value_first());

      _range_slider->on_change.second =
         [&_view, _max_textbox, pretty_printer, axis_transform] (float value)
         {
            _max_textbox->second->set_text(pretty_printer(axis_transform(value)));
            _view.refresh(_max_textbox->first);
         };
      _range_slider->edit_value_second(_range_slider->value_second());

      _min_textbox->second->on_text =
         [_min_bg] (std::string_view text)
         {
            _min_bg->_state = text.empty()?
               input_box_bg::idle : input_box_bg::active;
         };

      _min_textbox->second->on_enter =
         [&_view, _range_slider, _min_bg, axis_transform_inv] (std::string_view text)->bool
         {
            try {
               _range_slider->value_first(axis_transform_inv(std::stof(std::string(text))));
               _min_bg->_state = input_box_bg::idle;
               _view.refresh(*_range_slider);
            } catch (std::exception&) {
               _min_bg->_state = input_box_bg::error;
            }
            _view.refresh(*_min_bg);
            return true;
         };

      _max_textbox->second->on_text =
         [_max_bg] (std::string_view text)
         {
            _max_bg->_state = text.empty()?
               input_box_bg::idle : input_box_bg::active;
         };

      _max_textbox->second->on_enter =
         [&_view, _range_slider, _max_bg, axis_transform_inv] (std::string_view text)->bool
         {
            try {
               _range_slider->value_second(axis_transform_inv(std::stof(std::string(text))));
               _max_bg->_state = input_box_bg::idle;
               _view.refresh(*_range_slider);
            } catch (std::exception&) {
               _max_bg->_state = input_box_bg::error;
            }
            _view.refresh(*_max_bg);
            return true;
         };

      return margin(
         {10, 10, 10, 10},
         layer(
            vtile_spaced(10,
               align_center(
                  label("Logarithmic range slider").font_size(18)
               ),
               align_center(
                  label("Overlapping thumbs.")
               ),
               margin(
                  {50, 10, 50, 0},
                  hold(_range_slider)
               ),
               layer(
                  align_left(
                     margin(
                        {50, 10, 50, 10},
                        hsize(
                           100,
                           layer(
                              link(_min_textbox->first),
                              hold(_min_bg)
                           )
                        )
                     )
                  ),
                  align_right(
                     margin(
                        {50, 10, 50, 10},
                        hsize(
                           100,
                           layer(
                              link(_max_textbox->first),
                              hold(_max_bg)
                           )
                        )
                     )
                  )
               )
            ),
            frame{}
         )
      );
   }

   auto make_default_range_slider(view& _view)
   {
      auto track = basic_track<5, false>();
      auto _range_slider = share(range_slider(
         fixed_size(
            {8, 27},
            rbox(colors::light_gray, 2)
         ),
         fixed_size(
            {8, 27},
            rbox(colors::light_gray, 2)
         ),
         slider_labels<11>(
            slider_marks_lin<20, 10, 5>(track), 0.8, "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"
         ),
         {0.1, 0.8}
      ));
      return make_range_slider(
         _view, _range_slider,
         "Default linear range slider",
         "Non-overlapping thumbs");
   }

   auto make_overlapping_range_slider(view& _view)
   {
      auto track = basic_track<5, false>();
      auto _range_slider = share(range_slider(
         fixed_size(
            {8, 27},
            rbox(colors::lime_green.level(0.8), 2)
         ),
         fixed_size(
            {8, 27},
            rbox(colors::orange_red.level(0.8), 2)
         ),
         slider_labels<11>(
            slider_marks_lin<20, 5, 10>(track), 0.8, "0", "2", "4", "6", "8", "10"
         ),
         {0.1, 0.8},
         +0.5
      ));
      return make_range_slider(
         _view, _range_slider,
         "Linear range slider",
         "Overlapping thumbs. Alt/Option-click to switch active thumb.");
   }

   auto make_page(view& _view)
   {
      return
         vscroller(
            margin({20, 10, 20, 20},
               vtile(
                  make_default_range_slider(_view),
                  make_overlapping_range_slider(_view),
                  make_log_range_slider(_view)
               )
            )
         );
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Buttons (examples/buttons)
///////////////////////////////////////////////////////////////////////////////
namespace ns_buttons
{
   constexpr auto bred     = colors::red.opacity(0.4);
   constexpr auto bgreen   = colors::green.level(0.7).opacity(0.4);
   constexpr auto bblue    = colors::blue.opacity(0.4);
   constexpr auto brblue   = colors::royal_blue.opacity(0.4);
   constexpr auto pgold    = colors::gold.opacity(0.8);

   struct my_custom_button : button_styler_base
   {
      view_limits limits(basic_context const& ctx) const override;
      void        draw(context const& ctx) override;
   };

   view_limits my_custom_button::limits(basic_context const& ctx) const
   {
      return {{200, 30}, {full_extent, 30}};
   }

   void my_custom_button::draw(context const& ctx)
   {
      auto& cnv = ctx.canvas;
      auto bounds = ctx.bounds;

      auto btn = find_parent<basic_button*>(ctx);
      if (!btn)
         return;

      bool value = btn->value();
      bool hilite = btn->hilite();
      bool enabled = ctx.enabled;

      bounds = bounds.inset(1, 1);
      if (value)
         bounds = bounds.move(1, 1);

      cnv.fill_style(colors::dark_slate_blue);
      cnv.fill_round_rect(bounds, 8);
      cnv.line_width(1);
      cnv.stroke_style(
         hilite?
            colors::antique_white.opacity(0.5) :
            colors::antique_white.opacity(0.3)
         );
      cnv.stroke_round_rect(bounds, 8);

      cnv.font(font_descr{"Roboto", 14.0}.italic());
      cnv.fill_style(hilite? pgold.level(1.2) : pgold);
      cnv.text_align(cnv.center | cnv.middle);
      cnv.fill_text("My Custom Button", center_point(bounds));
   }

   auto make_custom_button()
   {
      return momentary_button(my_custom_button());
   }

   auto make_buttons(view& view_)
   {
      auto mbutton         = button("Momentary Button");
      auto tbutton         = toggle_button("Toggle Button", 1.0, bred);
      auto lbutton         = share(latching_button("Latching Button", 1.0, bgreen));
      auto reset           = share(button("Clear Latch", icons::lock_open, 1.0, bblue));
      auto note            = button(icons::cog, "Setup", 1.0, brblue);
      auto disabled_button = button("Disabled Button");

      auto left            =  momentary_button(
                                 button_styler{"Left"}
                                    .align_left()
                                    .icon(icons::left_circled)
                                    .icon_left()
                                    .body_color(bred)
                              );
      auto center          =  momentary_button(
                                 button_styler{"Center"}
                                    .body_color(bblue)
                              );
      auto right           =  momentary_button(
                                 button_styler{"Right"}
                                    .align_right()
                                    .icon(icons::right_circled)
                                    .body_color(bgreen)
                              );

      auto left_rounded    =  momentary_button(
                                 button_styler{"Rounded Left"}
                                    .align_left()
                                    .icon(icons::left_circled)
                                    .icon_left()
                                    .body_color(bred)
                                    .rounded_left(10)
                              );

      auto center_square   =  momentary_button(
                                 button_styler{"Square Center"}
                                    .body_color(bblue)
                                    .corner_radius(0)
                              );

      auto right_rounded   =  momentary_button(
                                 button_styler{"Rounded Right"}
                                    .align_right()
                                    .icon(icons::right_circled)
                                    .body_color(bgreen)
                                    .corner_radius(0, 10, 10, 0)
                              );

      auto slide_btn1      = slide_switch();
      auto slide_btn2      = slide_switch();

      slide_btn1.value(true);
      slide_btn2.enable(false);

      auto custom          = make_custom_button();

      disabled_button.enable(false);
      reset->enable(false);

      lbutton->on_click =
         [reset = get(reset), &view_](bool) mutable
         {
            if (auto p = reset.lock())
            {
               (*p)->set_icon(icons::lock);
               p->enable(true);
               view_.refresh(*p);
            }
         };

      reset->on_click =
         [lbutton = get(lbutton), reset = get(reset), &view_](bool) mutable
         {
            if (auto p = lbutton.lock())
            {
               p->value(0);
               view_.refresh(*p);
            }

            if (auto p = reset.lock())
            {
               (*p)->set_icon(icons::lock_open);
               p->enable(false);
               view_.refresh(*p);
            }
         };

      auto disabled_label = label("Disabled");
      disabled_label.enable(false);

      return
         margin({20, 20, 20, 20},
            vtile_spaced(15.0,
               mbutton,
               tbutton,
               hold(lbutton),
               hold(reset),
               note,
               disabled_button,
               htile_spaced(10.0,
                  label("Enabled"),
                  align_left(slide_btn1),
                  label("Slide Buttons"),
                  align_right(slide_btn2),
                  disabled_label
               ),
               hgrid(left, center, right),
               hgrid(left_rounded, center_square, right_rounded),
               custom
            )
         );
   }

   auto make_controls(view& view_)
   {
      auto  check_box1 = check_box("Reionizing electrons");
      auto  check_box2 = check_box("The Nexus Meridian Unfolding");
      auto  check_box3 = check_box("Serenity Dreamscape Exploration");
      auto  check_box4 = check_box("Forever Disabled");
      auto  check_box5 = check_box("Forever Checked");

      check_box1.value(true);
      check_box2.value(true);
      check_box3.value(true);
      check_box4.enable(false);
      check_box5.value(true);
      check_box5.enable(false);

      auto  check_boxes =
            group("Check boxes",
               margin({10, 45, 20, 20},
                  vtile_spaced(10.0,
                     align_left(check_box1),
                     align_left(check_box2),
                     align_left(check_box3),
                     align_left(check_box4),
                     align_left(check_box5)
                  )
               )
            );

      auto  radio_button1 = radio_button("Eons from now");
      auto  radio_button2 = radio_button("Ultra-sentient particles");
      auto  radio_button3 = radio_button("The stratosphere is electrified");
      auto  radio_button4 = radio_button("No, no, not me");

      radio_button1.select(true);
      radio_button4.enable(false);

      auto  radio_buttons =
            group("Radio Buttons",
               margin({10, 45, 20, 20},
                  vtile_spaced(10.0,
                     align_left(radio_button1),
                     align_left(radio_button2),
                     align_left(radio_button3),
                     align_left(radio_button4)
                  )
               )
            );

      auto indicator_color = get_theme().indicator_color;

      auto disabled_icon_button = icon_button(icons::block, 1.2);
      disabled_icon_button.enable(false);

      auto  icon_buttons =
            group("Icon Buttons",
               margin({10, 45, 20, 10},
                  htile(
                     align_center(toggle_icon_button(icons::power, 1.2, indicator_color)),
                     align_center(icon_button(icons::magnifying_glass, 1.2)),
                     align_center(icon_button(icons::left_circled, 1.2)),
                     align_center(toggle_icon_button(icons::left, icons::right, 1.2)),
                     align_center(disabled_icon_button)
                  )
               )
            );
      float const button_scale = 1.0/4;
      sprite power_button = sprite{"power_180x632.png", 158*button_scale, button_scale};
      sprite phase_button = sprite{"phase_180x790.png", 158*button_scale, button_scale};
      sprite mail_button = sprite{"mail_180x790.png", 158*button_scale, button_scale};
      sprite transpo_button = sprite{"transpo_180x632.png", 158*button_scale, button_scale};

      auto phase_disabled = toggle_button(phase_button);
      phase_disabled.enable(false);

      auto  sprite_buttons =
            group("Sprite Buttons",
               margin({10, 45, 20, 10},
                  htile(
                     align_center(toggle_button(power_button)),
                     align_center(toggle_button(phase_button)),
                     align_center(momentary_button(mail_button)),
                     align_center(toggle_button(transpo_button)),
                     align_center(phase_disabled)
                  )
               )
            );

      return
         vscroller(
            vtile(
               hgrid(
                  make_buttons(view_),
                  vtile(
                     margin({20, 20, 20, 20}, check_boxes),
                     margin({20, 20, 20, 20}, radio_buttons)
                  )
               ),
               hgrid(
                  hmin_size(250, margin({20, 20, 20, 20}, icon_buttons)),
                  hmin_size(250, margin({20, 20, 20, 20}, sprite_buttons))
               )
            )
         );
   }

   auto make_page(view& view_)
   {
      return make_controls(view_);
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: WinUI (Fluent-style controls under make_winui_theme)
///////////////////////////////////////////////////////////////////////////////
namespace ns_winui
{
   auto make_buttons()
   {
      auto normal   = button("Button");
      auto disabled = button("Disabled");
      auto toggle   = toggle_button("Toggle");
      auto accent   = button(
         icons::cog, "Settings", 1.0, colors::dodger_blue.opacity(0.9));

      disabled.enable(false);

      return
         group("Buttons",
            margin({10, 45, 20, 10},
               htile(
                  align_center(normal),
                  align_center(disabled),
                  align_center(toggle),
                  align_center(accent)
               )
            )
         );
   }

   auto make_switches()
   {
      auto on       = slide_switch();
      auto off      = slide_switch();
      auto disabled = slide_switch();

      on.value(true);
      disabled.enable(false);

      return
         group("Toggle Switches",
            margin({10, 45, 20, 10},
               htile(
                  align_center(on),
                  align_center(off),
                  align_center(disabled)
               )
            )
         );
   }

   auto make_check_boxes()
   {
      auto checked   = check_box("Checked");
      auto unchecked = check_box("Unchecked");
      auto disabled  = check_box("Disabled");

      checked.value(true);
      disabled.enable(false);

      return
         group("Check Boxes",
            margin({10, 45, 20, 10},
               htile(
                  align_center(checked),
                  align_center(unchecked),
                  align_center(disabled)
               )
            )
         );
   }

   auto make_slider()
   {
      auto s = slider(basic_thumb<16>(), basic_track<4>(), 0.5);

      return
         group("Slider",
            margin({10, 45, 20, 10},
               align_middle(hsize(400, s))
            )
         );
   }

   auto make_text_entry()
   {
      auto tbox = input_box("Enter text");

      return
         group("Text Entry",
            margin({10, 45, 20, 10},
               align_middle(hsize(300, tbox.first))
            )
         );
   }

   auto make_page()
   {
      return
         vscroller(
            vtile(
               make_buttons(),
               make_switches(),
               make_check_boxes(),
               make_slider(),
               make_text_entry()
            )
         );
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Menus (examples/menus)
///////////////////////////////////////////////////////////////////////////////
namespace ns_menus
{
   auto make_selection_menu()
   {
      return selection_menu(
         [](std::string_view select)
         {
            // This will be called when an item is selected
         },
         {
            "Quantum Feedback Loop",
            "Psionic Wave Oscillator",
            "Gaia Abiogenesis",
            "Chaotic Synchronicity",
            "Omega Quadrant",
            "Photonic Mesh",
            "Antimatter Soup",
            "Dark Beta Quarks",
            "Cosmic Infrared Shift"
         }
      ).first;
   }

   auto make_popup_menu(char const* title, menu_position pos)
   {
      auto popup  = button_menu(title, pos);

      auto skf = shortcut_key{key_code::f, mod_action};
      auto skp = shortcut_key{key_code::p, mod_action};

      auto quantum_feedback_loop = menu_item("Quantum Feedback Loop", skf);
      auto psionic_wave_oscillator = menu_item("Psionic Wave Oscillator", skp);
      auto photonic_mesh = menu_item("Photonic Mesh");
      auto antimatter_soup = menu_item("Antimatter Soup");

      static bool enable = false;
      photonic_mesh.is_enabled   = []{ return enable; };
      antimatter_soup.is_enabled = []{ return enable; };

      quantum_feedback_loop.on_click   = [](){ enable = true; };
      psionic_wave_oscillator.on_click = [](){ enable = false; };

      auto sk1 = shortcut_key{key_code::g, mod_action};
      auto sk2 = shortcut_key{key_code::c, mod_action+mod_shift};
      auto sk3 = shortcut_key{key_code::b, mod_action+mod_alt};

      auto menu =
         layer(
            vtile(
               photonic_mesh,
               quantum_feedback_loop,
               psionic_wave_oscillator,
               menu_item("Gaia Abiogenesis", sk1),
               menu_item_spacer(),
               menu_item("Chaotic Synchronicity", sk2),
               menu_item("Omega Quadrant"),
               antimatter_soup,
               menu_item("Dark Beta Quarks", sk3),
               menu_item("Cosmic Infrared Shift")
            ),
            panel{}
         );

      popup.menu(hsize(300, menu));

      return popup;
   }

   auto make_dynamic_menu(char const* title, menu_position pos)
   {
      auto popup  = button_menu(title, pos);

      auto populate_menu =
         [](auto& popup)
         {
            char const* items[] =
            {
               "Seeker, look within",
               "Empower yourself",
               "Have you found your circuit?",
               "Complexity to the next level",
               "Ultra-angelic consciousness",
               "Indigo Child",
               "Vector of synchronicity",
               "Aspiration is a constant",
               "Strange Soup",
               "Nonchalant Slave",
               "Shiva will amplify your mind",
               "This mission never ends",
               "The future is now",
               "Who are we?",
               "One cannot self-actualize",
               "Ennobling source of stardust"
            };

            vtile_composite list;
            for (auto item : items)
               list.push_back(share(menu_item(item)));

            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(list.begin(), list.end(), g);

            auto menu =
               layer(
                  vsize(150, vscroller(list)),
                  panel{}
               );

            popup.menu(menu);
         };

      popup.on_open_menu = populate_menu;
      return popup;
   }

   auto make_menus()
   {
      return
         margin({20, 0, 20, 20},
            vtile(
               hmin_size(300, make_selection_menu()),
               margin_top(20, make_popup_menu("Dropdown Menu", menu_position::bottom_right)),
               margin_top(20, make_dynamic_menu("Dynamic Menu", menu_position::bottom_right)),
               margin_top(20, scroller(image{"deep_space.jpg"})),
               margin_top(20, make_popup_menu("Dropup Menu", menu_position::top_right))
            )
         );
   }

   auto make_page()
   {
      return margin({20, 20, 20, 20}, make_menus());
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Text & Icons (examples/text_and_icons)
///////////////////////////////////////////////////////////////////////////////
namespace ns_text_icons
{
   std::string const text =
      "We are in the midst of an intergalatic condensing of beauty that will "
      "clear a path toward the planet itself. The quantum leap of rebirth is "
      "now happening worldwide. It is time to take healing to the next level. "
      "Soon there will be a deepening of chi the likes of which the infinite "
      "has never seen. The universe is approaching a tipping point. This "
      "vision quest never ends. Imagine a condensing of what could be. "
      "We can no longer afford to live with stagnation. Suffering is born "
      "in the gap where stardust has been excluded. You must take a stand "
      "against discontinuity.\n\n"

      "Without complexity, one cannot dream. Stagnation is the antithesis of "
      "life-force. Only a seeker of the galaxy may engender this wellspring of hope."
      "Yes, it is possible to eliminate the things that can destroy us, but not "
      "without wellbeing on our side. Where there is delusion, faith cannot thrive. "
      "You may be ruled by desire without realizing it. Do not let it eliminate "
      "the growth of your journey.\n\n"

      "--New-Age Bullshit Generator"
   ;

   auto make_basic_text(view& view_)
   {
      auto make_framed_label =
         [](auto&& make_label, float top = 10)
         {
            return margin(
               {10, top, 10, 10},
               layer(
                  margin({10, 5, 10, 5}, std::move(make_label)),
                  frame{}
               )
            );
         };

      auto make_label =
         [make_framed_label](auto const& label_)
         {
            return make_framed_label(halign(0.5, label_));
         };

      auto icons_ =
         margin({10, 0, 10, 10},
            htile(
               align_center(icon{icons::docs}),
               align_center(icon{icons::right}),
               align_center(icon{icons::trash}),
               align_center(icon{icons::block}),
               align_center(icon{icons::cw}),
               align_center(icon{icons::attention}),
               align_center(icon{icons::menu}),
               align_center(icon{icons::lightbulb}),
               align_center(icon{icons::sliders}),
               align_center(icon{icons::exchange})
            )
         );

      static float const grid[] = {0.32, 1.0};

      auto my_label =
         [=](auto text)
         {
            return margin_right(10, label(text).text_align(canvas::right));
         };

      auto my_input =
         [=](auto caption, auto input)
         {
            return margin_bottom(10, hgrid(grid, my_label(caption), input));
         };

      auto thank_you = message_box1(
         view_, "Thank you!",
         icons::attention, [](){}
      );

      auto error_want_more = message_box1(
         view_, "No! Aurelia Starweaver wants the $1000000 you owe her!",
         icons::attention, [](){}
      );

      auto got_the_money = std::make_shared<bool>(false);

      auto in = input_box("Show me the money");
      in.second->on_enter =
         [input = in.second.get(), &view_, error_want_more, thank_you, got_the_money](std::string_view text)->bool
         {
            if (text == "")
            {
               return true;
            }
            else if (text == "$1000000")
            {
               if (!*got_the_money)
               {
                  open_popup(thank_you, view_);
                  input->select_all();
                  view_.refresh(*input);
                  *got_the_money = true;
               }
               return true;
            }
            else
            {
               if (!*got_the_money)
               {
                  open_popup(error_want_more, view_);
                  input->set_text("");
                  input->select_all();
                  view_.refresh(*input);
                  return false;
               }
              return true;
            }
         };

      in.second->on_end_focus = in.second->on_enter;

      auto clip_left = basic_input_box::clip_left;

      auto text_input =
         pane("Text Input",
            margin({10, 5, 10, 5},
               vtile(
                  my_input("Gimme Some", in.first),
                  my_input("Gimme Some More", input_box("Show me more", clip_left).first),
                  my_input("Cute Text Boxes",
                     htile(
                        input_box(0.7).first,
                        margin_left(10, input_box(0.7).first),
                        margin_left(10, input_box(0.7).first)
                     )
                  )
               )
            ))
         ;

      auto labels =
         margin_top(20, pane("Labels",
            vtile(
               make_label(label("Hello, Universe. This is Elements.")
                  .font(font_descr{"Open Sans"}.semi_bold())
                  .font_size(18)
               ),
               make_label(
                  vtile(
                     label("A cross-platform,")
                        .text_align(canvas::center),
                     label("fine-grained,")
                        .text_align(canvas::left),
                     label("highly modular C++ GUI library.")
                        .text_align(canvas::right),
                     label("Based on a GUI framework written in the mid 90s named Pica."),
                     label("Now, Joel rewrote my code using modern C++17.")
                  )
               )
            )))
         ;

      return
         margin(
            {10, 0, 10, 10},
            vtile(
               text_input,
               labels,
               margin_top(20, pane("Icons", std::move(icons_))),
               empty()
            )
         );
   }

   auto make_basic_text2()
   {
      auto textbox = share(vport(basic_text_box{text}.read_only()));
      return hmin_size(350, margin(
            {10, 0, 10, 10},
            hold(textbox)
         ));
   }

   auto make_elements(view& view_)
   {
      return
         max_size({1280, 640},
            margin({20, 10, 20, 10},
               htile(
                  margin({20, 20, 20, 20}, make_basic_text(view_)),
                  margin({20, 20, 20, 20},
                     pane("Text Box", make_basic_text2())
                  )
               )
            )
         );
   }

   auto make_page(view& view_)
   {
      return scroller(make_elements(view_));
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Text Edit (examples/text_edit)
///////////////////////////////////////////////////////////////////////////////
namespace ns_text_edit
{
   std::string const text1 =
      "一千条路…\n"
      "仁、义、礼、智、信…\n"
      "开放包容、视野宽广\n\n"
   ;

   std::string const text2 =
      "To traverse the quest is to become one with it.\n\n"

      "You and I are adventurers of the quantum cycle. The goal of expanding wave "
      "functions is to plant the seeds of non-locality rather than pain. "
      "The complexity of the present time seems to demand a redefining of our "
      "bodies if we are going to survive. "
      "We are at a crossroads of will and greed. Humankind has nothing to lose. "
      "Our conversations with other storytellers have led to an evolving of "
      "hyper-sentient consciousness. "
      "If you have never experienced this flow on a cosmic scale, it can be "
      "difficult to self-actualize. Although you may not realize it, you are "
      "ancient. Have you found your vision quest?\n\n"
   ;

   auto make_edit_box()
   {
      char const* font_family = "文泉驿微米黑, \"WenQuanYi Micro Hei\"";
      auto text = text1+text2;

      return
         scroller(
            margin(
               {20, 20, 20, 20},
               align_left_top(hsize(800,
                  basic_text_box(text, font_descr{font_family, 14}
               )))
            )
         );
   }

   auto make_page()
   {
      return make_edit_box();
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Lists (examples/list)
///////////////////////////////////////////////////////////////////////////////
namespace ns_list
{
   auto make_page()
   {
      auto&& draw_cell =
         [](std::size_t index)
         {
            auto text = "This is item number " + std::to_string(index+1);
            return share(margin({20, 2, 20, 2}, align_left(label(text))));
         };

      auto my_composer =
         basic_cell_composer(
            1000000,
            draw_cell
         );

      auto content = share(list{my_composer});

      return vscroller(hold(content));
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Selection List (examples/selection_list)
///////////////////////////////////////////////////////////////////////////////
namespace ns_selection
{
   struct my_element : element, selectable
   {
      my_element(int n)
       : _n{n}
      {}

      void draw(context const& ctx) override
      {
         auto& cnv = ctx.canvas;
         auto state = cnv.new_state();
         auto const& theme_ = get_theme();

         if (_is_selected)
         {
            cnv.begin_path();
            cnv.add_rect(ctx.bounds);
            cnv.fill_style(theme_.indicator_color.opacity(0.6));
            cnv.fill();
         }

         auto middle = ctx.bounds.top + (ctx.bounds.height()/2);
         cnv.fill_style(theme_.label_font_color);
         cnv.font(theme_.label_font);
         cnv.text_align(cnv.left | cnv.middle);
         cnv.fill_text(std::string{"Item "} + std::to_string(_n), point{ctx.bounds.left+10, middle});
      }

      bool is_selected() const override
      {
         return _is_selected;
      }

      void select(bool state) override
      {
         _is_selected = state;
      }

      int _n;
      bool _is_selected = false;
   };

   auto make_page()
   {
      auto&& draw_cell =
         [](std::size_t index)
         {
            return share(
               margin({20, 0, 20, 0},
                  align_left(
                     vsize(25, my_element(index+1))
                  )
               )
            );
         };

      auto my_composer =
         basic_cell_composer(
            200,
            draw_cell
         );

      auto content = share(
         selection_list(
            list{my_composer, false}
         )
      );

      return vscroller(hold(content));
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Icons Gallery (examples/icons_list)
///////////////////////////////////////////////////////////////////////////////
namespace ns_icons
{
   inline auto make_icon_label(std::string name, int i)
   {
      auto h =
      htile(
         label(name),
         align_right(hsize(64, icon(i)))
      );
      return share(h);
   }

   auto make_page()
   {
      vtile_composite comp;
      comp.push_back(make_icon_label("left", icons::left));
      comp.push_back(make_icon_label("right", icons::right));
      comp.push_back(make_icon_label("up", icons::up));
      comp.push_back(make_icon_label("down", icons::down));
      comp.push_back(make_icon_label("left_circled", icons::left_circled));
      comp.push_back(make_icon_label("right_circled", icons::right_circled));
      comp.push_back(make_icon_label("up_circled", icons::up_circled));
      comp.push_back(make_icon_label("down_circled", icons::down_circled));
      comp.push_back(make_icon_label("angle_left", icons::angle_left));
      comp.push_back(make_icon_label("angle_right", icons::angle_right));
      comp.push_back(make_icon_label("angle_up", icons::angle_up));
      comp.push_back(make_icon_label("angle_down", icons::angle_down));
      comp.push_back(make_icon_label("angle_double_left", icons::angle_double_left));
      comp.push_back(make_icon_label("angle_double_right", icons::angle_double_right));
      comp.push_back(make_icon_label("angle_double_up", icons::angle_double_up));
      comp.push_back(make_icon_label("angle_double_down", icons::angle_double_down));
      comp.push_back(make_icon_label("angle_circled_left", icons::angle_circled_left));
      comp.push_back(make_icon_label("angle_circled_right", icons::angle_circled_right));
      comp.push_back(make_icon_label("exclamation", icons::exclamation));
      comp.push_back(make_icon_label("block", icons::block));
      comp.push_back(make_icon_label("pencil", icons::pencil));
      comp.push_back(make_icon_label("pin", icons::pin));
      comp.push_back(make_icon_label("resize_vertical", icons::resize_vertical));
      comp.push_back(make_icon_label("resize_horizontal", icons::resize_horizontal));
      comp.push_back(make_icon_label("move", icons::move));
      comp.push_back(make_icon_label("resize_full", icons::resize_full));
      comp.push_back(make_icon_label("resize_small", icons::resize_small));
      comp.push_back(make_icon_label("magnifying_glass", icons::magnifying_glass));
      comp.push_back(make_icon_label("zoom_in", icons::zoom_in));
      comp.push_back(make_icon_label("zoom_out", icons::zoom_out));
      comp.push_back(make_icon_label("volume_off", icons::volume_off));
      comp.push_back(make_icon_label("volume_down", icons::volume_down));
      comp.push_back(make_icon_label("volume_up", icons::volume_up));
      comp.push_back(make_icon_label("cw", icons::cw));
      comp.push_back(make_icon_label("ccw", icons::ccw));
      comp.push_back(make_icon_label("cycle", icons::cycle));
      comp.push_back(make_icon_label("shuffle", icons::shuffle));
      comp.push_back(make_icon_label("exchange", icons::exchange));
      comp.push_back(make_icon_label("power", icons::power));
      comp.push_back(make_icon_label("play", icons::play));
      comp.push_back(make_icon_label("stop", icons::stop));
      comp.push_back(make_icon_label("pause", icons::pause));
      comp.push_back(make_icon_label("record", icons::record));
      comp.push_back(make_icon_label("to_end", icons::to_end));
      comp.push_back(make_icon_label("to_start", icons::to_start));
      comp.push_back(make_icon_label("fast_forward", icons::fast_forward));
      comp.push_back(make_icon_label("fast_backward", icons::fast_backward));
      comp.push_back(make_icon_label("wrench", icons::wrench));
      comp.push_back(make_icon_label("trash", icons::trash));
      comp.push_back(make_icon_label("trash_empty", icons::trash_empty));
      comp.push_back(make_icon_label("ok", icons::ok));
      comp.push_back(make_icon_label("cancel", icons::cancel));
      comp.push_back(make_icon_label("plus", icons::plus));
      comp.push_back(make_icon_label("minus", icons::minus));
      comp.push_back(make_icon_label("cog", icons::cog));
      comp.push_back(make_icon_label("doc", icons::doc));
      comp.push_back(make_icon_label("docs", icons::docs));
      comp.push_back(make_icon_label("lock_open", icons::lock_open));
      comp.push_back(make_icon_label("lock", icons::lock));
      comp.push_back(make_icon_label("sliders", icons::sliders));
      comp.push_back(make_icon_label("floppy", icons::floppy));
      comp.push_back(make_icon_label("attention", icons::attention));
      comp.push_back(make_icon_label("info", icons::info));
      comp.push_back(make_icon_label("error", icons::error));
      comp.push_back(make_icon_label("lightbulb", icons::lightbulb));
      comp.push_back(make_icon_label("mixer", icons::mixer));
      comp.push_back(make_icon_label("hand", icons::hand));
      comp.push_back(make_icon_label("question", icons::question));
      comp.push_back(make_icon_label("menu", icons::menu));
      comp.push_back(make_icon_label("link", icons::link));
      comp.push_back(make_icon_label("unlink", icons::unlink));
      comp.push_back(make_icon_label("folder_open", icons::folder_empty));
      comp.push_back(make_icon_label("folder_open_empty", icons::folder_open_empty));

      return
         vscroller(
            margin({10, 10, 10, 10},
               margin({10, 10, 30, 10}, comp)
            )
         );
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Thumbwheels (examples/thumbwheels)
///////////////////////////////////////////////////////////////////////////////
namespace ns_thumbwheels
{
   template <typename E>
   auto decorate(E&& e)
   {
      return hsize(170, align_center(margin({25, 5, 25, 5},
         std::forward<E>(e)
      )));
   }

   auto make_label(std::string text = "")
   {
      return decorate(heading(text)
         .font_color(get_theme().indicator_hilite_color)
         .font_size(24)
      );
   }

   auto make_thumbwheel1()
   {
      auto&& compose =
         [](std::size_t index)
         {
            auto text = "Item no.  " + std::to_string(index+1);
            return share(make_label(text));
         };

      auto tw = share(vthumbwheel(20, compose));

      return margin_top(20,
         layer(
            hold(tw),
            frame{}
         )
      );
   }

   auto make_thumbwheel2(char const* unit, float offset, float scale, int precision)
   {
      auto label = make_label();

      auto&& as_string =
         [=](double val)
         {
            std::ostringstream out;
            out.precision(precision);
            out << std::fixed << ((val * scale) + offset) << unit;
            return out.str();
         };

      auto tw = share(thumbwheel(as_label<double>(as_string, label)));

      return margin_top(20,
         layer(
            hold(tw),
            frame{}
         )
      );
   }

   auto make_xy_thumbwheel()
   {
      auto label = make_label();

      auto&& draw =
         [=](context const& ctx, point val)
         {
            auto& cnv = ctx.canvas;
            cnv.begin_path();
            cnv.add_round_rect(ctx.bounds, get_theme().frame_corner_radius);
            cnv.fill_style(color(val.x, 0.0, 1.0).level(val.y));
            cnv.fill();
         };

      auto tw = share(thumbwheel(fixed_size({170, 36}, draw_value<point>(draw))));
      tw->value({0.25f, 0.25f});

      return margin_top(20, hold(tw));
   }

   auto make_page()
   {
      return
         margin({20, 0, 20, 20},
            vtile(
               align_center_middle(make_thumbwheel1()),
               align_center_middle(make_thumbwheel2(" Hz", 20, 100, 2)),
               align_center_middle(make_thumbwheel2(" ms", 0, 10000, 0)),
               align_center_middle(make_xy_thumbwheel())
            )
         );
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Status Bars (examples/status_bars)
///////////////////////////////////////////////////////////////////////////////
namespace ns_status_bars
{
   constexpr auto bred     = colors::red.opacity(0.4);
   constexpr auto bgreen   = colors::green.level(0.7).opacity(0.8);
   constexpr auto pgold    = colors::gold.opacity(0.8);

   bool run = false;
   void prog_animate(concepts::StatusBar auto& prog_bar, view& view_)
   {
      if (run)
      {
         if (auto val = prog_bar.value() + 0.005; val > 1.0)
         {
            prog_bar.value(1.0);
            run = false;
         }
         else
         {
            prog_bar.value(val);
         }
         view_.refresh(prog_bar);
         view_.post(10ms,
            [&]()
            {
               prog_animate(prog_bar, view_);
            }
         );
      }
   }

   auto make_bars(view& view_)
   {
      auto prog_bar = share(progress_bar(rbox(colors::black), rbox(pgold)));
      auto bsy_bar = share(busy_bar(rbox(colors::black), rbox(bgreen)));
      auto start_stop = toggle_icon_button(icons::play, icons::stop, 2.0, bred);

      start_stop.on_click =
         [prog_bar, bsy_bar, &view_](bool state)
         {
            if (state)
            {
               bsy_bar->start(view_, 10ms);
               run = true;
               prog_bar->value(0.0);
               prog_animate(*prog_bar, view_);
            }
            else
            {
               bsy_bar->stop(view_);
               run = false;
            }
         };

      return
         margin({20, 20, 20, 20},
            htile_spaced(20,
               align_middle(start_stop),
               vtile_spaced(10,
                  vsize(27, hold(prog_bar)),
                  vsize(27, hold(bsy_bar))
               )
            )
         );
   }

   auto make_page(view& view_)
   {
      return make_bars(view_);
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Tooltips (examples/tooltip)
///////////////////////////////////////////////////////////////////////////////
namespace ns_tooltip
{
   constexpr auto bred     = colors::red.opacity(0.4);
   constexpr auto bgreen   = colors::green.level(0.7).opacity(0.4);
   constexpr auto bblue    = colors::blue.opacity(0.4);
   constexpr auto pgold    = colors::gold.opacity(0.8);

   auto make_tip(std::string text)
   {
      return layer(
         margin({20, 8, 20, 8}, label(text))
       , panel{}
      );
   }

   template <typename Label>
   auto make_button(std::string text, color c, Label label, view& view_)
   {
      static int i = 1;
      std::string orig{label->get_text().data(), label->get_text().size()};
      auto tt = tooltip(toggle_button("Option " + std::to_string(i++), 1.0f, c), make_tip(text));
      tt.on_hover =
         [label, text, &view_, orig = label->get_text()](bool visible)
         {
            label->set_text(visible? text : orig);
            view_.refresh(*label.get(), 3);
         };
      return tt;
   }

   auto make_buttons(view& view_)
   {
      auto status = share(label("Have you found your vision quest?"));
      auto button1 = make_button("Eons from now", bred, status, view_);
      auto button2 = make_button("Take a stand against delusion", bgreen, status, view_);
      auto button3 = make_button("Totality is calling", bblue, status, view_);
      auto button4 = make_button("Four-dimensional superstructures", pgold, status, view_);

      return
         margin({50, 20, 50, 40},
            vtile(
               align_center(margin_top(20, hold(status))),
               margin_top(20, button1),
               margin_top(20, button2),
               margin_top(20, button3),
               margin_top(20, button4)
            )
         );
   }

   auto make_page(view& view_)
   {
      return make_buttons(view_);
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Child Window (examples/child_window)
///////////////////////////////////////////////////////////////////////////////
namespace ns_child_window
{
   // Child windows nested inside a notebook page. Clicking a window
   // raises it to the front within the page (click-to-front), and clicks
   // on a window covering the left navigation are captured by the window
   // rather than passing through. Move / resize / minimize / maximize /
   // close all work.
   template <typename Content>
   auto make_floating_window(std::string title, rect bounds, Content&& content)
   {
      auto buttons = htile_composite{};
      buttons.push_back(
         share(minimizable(plain_icon_button(icons::angle_down, 0.8)))
      );
      buttons.push_back(
         share(maximizable(plain_icon_button(icons::angle_up, 0.8)))
      );
      buttons.push_back(
         share(closable(plain_icon_button(icons::cancel, 0.8)))
      );

      return child_window(
         bounds,
         resizable(
            pane_ex(
               movable(
                  layer(
                     align_right(buttons),
                     title_bar{}
                  )
               ),
               std::move(title),
               std::forward<Content>(content),
               get_theme().child_window_title_size,
               std::nullopt // opacity: follow theme at draw time
            )
         )
      );
   }

   auto make_page()
   {
      layer_composite result;

      // Child Window 2 at the bottom, Child Window 1 at the top (matching
      // the previous `layer` stacking order).
      result.push_back(
         share(make_floating_window("Child Window 2", {60, 60, 350, 250},
            hmin_size(250, scroller(image{"deep_space.jpg"}))
         ))
      );
      result.push_back(
         share(make_floating_window("Child Window 1", {10, 10, 300, 200},
            hmin_size(250, scroller(image{"deep_space.jpg"}))
         ))
      );
      return result;
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Custom Control (examples/custom_control)
///////////////////////////////////////////////////////////////////////////////
namespace ns_custom_control
{
   // Function to calculate distance
   float distance(point a, point b)
   {
     float c = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
     return sqrtf(c);
   }

   // A custom control element
   class my_custom_control : public tracker<>, public receiver<float>
   {
   public:

      using point_array = std::array<point, 4>;
      constexpr static float default_value = 100;

      void        draw(context const& ctx) override;
      bool        click(context const& ctx, mouse_button btn) override;
      void        keep_tracking(context const& ctx, tracker_info& track_info) override;
      bool        cursor(context const& ctx, point p, cursor_tracking status) override;

      float       value() const override     { return _radius; }
      void        value(float val) override  { _radius = val; }

      using on_change_f = std::function<void(float)>;
      on_change_f on_change;

   private:

      bool        _mouse_over = false;
      int         _choosen_knob = -1;
      point_array _knobs;
      float       _radius = default_value;
   };

   void my_custom_control::draw(context const& ctx)
   {
      ctx.canvas.fill_style(color(0.8, 0.8, 0.8));
      ctx.canvas.fill_rect(ctx.bounds);

      point cursor_pos = ctx.cursor_pos();

      ctx.canvas.line_width(1.0);
      ctx.canvas.stroke_style(color(0, 0, 0.5));
      auto outer_ring = circle(center_point(ctx.bounds), _radius);
      ctx.canvas.add_circle(outer_ring);
      ctx.canvas.stroke();

      _knobs = point_array
         {
            point(outer_ring.bounds().left, center_point(ctx.bounds).y),
            point(outer_ring.bounds().right, center_point(ctx.bounds).y),
            point(center_point(ctx.bounds).x, outer_ring.bounds().top),
            point(center_point(ctx.bounds).x, outer_ring.bounds().bottom),
         };

      for (auto knob : _knobs)
      {
         ctx.canvas.line_width(1.0);
         ctx.canvas.stroke_style(color(0, 0, 0.6));
         ctx.canvas.fill_style(color(1.0, 1.0, 1.0));
         ctx.canvas.add_circle(circle(knob, 5.0));
         ctx.canvas.fill_preserve();
         ctx.canvas.stroke();

         auto d = distance(knob, cursor_pos);
         if (_mouse_over && (d < 10.0))
         {
            ctx.canvas.line_width(2.0);
            ctx.canvas.stroke_style(color(0, 0, 0.8));
            ctx.canvas.add_circle(circle(knob, 7.0));
            ctx.canvas.stroke();
         }
      }

      if (_choosen_knob != -1)
      {
         ctx.canvas.line_width(2.0);
         ctx.canvas.stroke_style(color(0, 0.8, 0));
         ctx.canvas.add_circle(circle(_knobs[_choosen_knob], 7.0));
         ctx.canvas.stroke();
      }

      if (_mouse_over)
      {
         ctx.canvas.line_width(1.0);
         ctx.canvas.stroke_style(color(0, 0.5, 0.5, 0.2));
         ctx.canvas.add_circle(circle(cursor_pos.x, cursor_pos.y, 10.0));
         ctx.canvas.stroke();
      }
   }

   bool my_custom_control::click(context const& ctx, mouse_button btn)
   {
      tracker<>::click(ctx, btn);

      if (btn.state == mouse_button::left)
      {
         for (std::size_t i = 0, e = _knobs.size(); i != e; ++i)
         {
            auto d = distance(_knobs[i], btn.pos);
            if (d < 10.0)
               _choosen_knob =  i;
         }
      }
      return true;
   }

   void my_custom_control::keep_tracking(context const& ctx, tracker_info& track_info)
   {
      auto center = center_point(ctx.bounds);
      _radius = std::abs(distance(center, track_info.current));
      cycfi::clamp(_radius, 50, 150);
      if (on_change)
         on_change(_radius);
      ctx.view.refresh(ctx);
   }

   bool my_custom_control::cursor(context const& ctx, point p, cursor_tracking status)
   {
      switch (status)
      {
         case cursor_tracking::hovering:
         case cursor_tracking::entering:
            _mouse_over = true;
            ctx.view.refresh(ctx);
            break;
         case cursor_tracking::leaving:
            _mouse_over = false;
      };
      return false;
   }

   // Application state for the custom control page
   class custom_page
   {
   public:

      constexpr static float default_value = my_custom_control::default_value;
      using dial_ptr = std::shared_ptr<basic_dial>;
      using label_ptr = decltype(share(label("")));

      explicit custom_page(view& view_)
       : _view{view_}
      {}

      auto make_control()
      {
         return
            layer(
               align_center_middle(
                  fixed_size({400, 400},
                     link(_my_control)
                  )),
               image{"wall.jpg"}
            );
      }

      auto make_button()
      {
         auto btn = share(button("Reset Size"));

         btn->on_click =
            [this](bool /*state*/)
            {
               set_value(default_value);
            };

         return
            margin({50, 10, 50, 10},
               hold(btn)
            );
      }

      auto make_dial()
      {
         float const knob_scale = 1.0;
         sprite knob = sprite{
            "knob_sprites_150x150_darker.png",
            150 * knob_scale, knob_scale
         };

         _dial = share(dial(knob, 0.5));

         _dial->on_change =
            [this](double val)
            {
               set_value(50 + (val * 100));
            };

         _my_control.on_change =
            [this](double val)
            {
               set_value(val);
            };

         return align_center_middle(hold(_dial));
      }

      auto make_label()
      {
         _label = share(label(""));
         return align_center_middle(hold(_label));
      }

      void set_value(float val)
      {
         _my_control.value(val);
         _dial->value((val - 50) / 100);

         _label->set_text("Radius: " + std::to_string(int(std::round(val))));

         _view.refresh();
      }

      auto make_control_panel()
      {
         return margin(
            {50, 20, 50, 20},
            layer(
               vgrid(
                  span(5, make_dial()),
                  span(1, make_label()),
                  span(1, make_button())
               ),
               panel{}
            )
         );
      }

      auto make_page()
      {
         auto panel = make_control_panel();
         auto control = make_control();
         set_value(default_value);
         return htile(std::move(panel), std::move(control));
      }

   private:

      view&              _view;
      my_custom_control  _my_control;
      dial_ptr           _dial;
      label_ptr          _label;
   };

   auto make_page(view& view_)
   {
      static auto state = std::make_shared<custom_page>(view_);
      return state->make_page();
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Scale (examples/scale)
///////////////////////////////////////////////////////////////////////////////
namespace ns_scale
{
   auto make_column()
   {
      auto  check_box1 = check_box("Reionizing electrons");
      auto  check_box2 = check_box("The Nexus Meridian Unfolding");
      auto  check_box3 = check_box("Serenity Dreamscape Exploration");

      check_box1.value(true);
      check_box2.value(true);
      check_box3.value(true);

      auto  group0 =
            group("Text Boxes",
               margin({10, 10, 20, 20},
                  margin_top(35,
                     vtile(
                        left_caption(input_box("Show me the money").first, "Gimme Some"),
                        margin_top(10, left_caption(input_box("Show me more").first, "Gimme Some More"))
                     )
                  )
               )
            );

      auto  group1 =
            group("Check boxes",
               margin({10, 10, 20, 20},
                  margin_top(25,
                     vtile(
                        margin_top(10, align_left(check_box1)),
                        margin_top(10, align_left(check_box2)),
                        margin_top(10, align_left(check_box3))
                     )
                  )
               )
            );

      auto indicator_color = get_theme().indicator_color;

      float const button_scale = 1.0/4;
      sprite power_button = sprite{"power_180x632.png", 158*button_scale, button_scale};
      sprite phase_button = sprite{"phase_180x632.png", 158*button_scale, button_scale};
      sprite mail_button = sprite{"mail_180x632.png", 158*button_scale, button_scale};
      sprite transpo_button = sprite{"transpo_180x632.png", 158*button_scale, button_scale};

      auto  group2 =
            group("Buttons",
               margin({10, 10, 20, 10},
                  margin_top(35,
                     vtile(
                        htile(
                           align_center(toggle_icon_button(icons::power, 1.2, indicator_color)),
                           align_center(icon_button(icons::magnifying_glass, 1.2)),
                           align_center(icon_button(icons::left_circled, 1.2)),
                           align_center(toggle_icon_button(icons::left, icons::right, 1.2))
                        ),
                        htile(
                           align_center(toggle_button(power_button)),
                           align_center(toggle_button(phase_button)),
                           align_center(momentary_button(mail_button)),
                           align_center(toggle_button(transpo_button))
                        )
                     )
                  )
               )
            );

      return vtile(
         margin({20, 20, 20, 20}, group0),
         margin({20, 20, 20, 20}, group1),
         margin({20, 20, 20, 20}, group2)
      );
   }

   auto make_controls()
   {
      return
         margin({20, 10, 20, 10},
            htile(
               align_top(margin({20, 20, 20, 20},
                  pane("Scale = 1.0", make_column())
               )),
               align_top(margin({20, 20, 20, 20},
                  scale(0.8, pane("Scale = 0.8", make_column()))
               ))
            )
         );
   }

   auto make_page()
   {
      return make_controls();
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Drop File (examples/drop_file)
///////////////////////////////////////////////////////////////////////////////
namespace ns_drop_file
{
   auto make_page(view& view_)
   {
      auto image_    = share(image{"space.jpg"});
      auto drop_box_ = share(drop_box(scroller(hold(image_)), {"text/uri-list"}));

      drop_box_->on_drop = [image_ = get(image_), &view_](drop_info const& info)
      {
         if (contains_filepaths(info.data))
         {
            auto paths = get_filepaths(info.data);
            if (paths.size() == 1)
            {
               auto const& image_path = paths[0];
               if (auto p = image_.lock())
               {
                  try
                  {
                     auto img = image{image_path};
                     *p = img;
                     view_.refresh(*p);
                  }
                  catch (std::runtime_error const&)
                  {
                     return false;
                  }
                  return true;
               }
            }
         }
         return false;
      };

      return
         layer(
            align_center_middle(label("Drop a picture here").font_size(20)),
            margin({20, 20, 20, 20}, hold(drop_box_))
         );
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Dialogs & Popups (examples/dialogs + popups, button-triggered)
///////////////////////////////////////////////////////////////////////////////
namespace ns_dialogs
{
   auto dialog_content()
   {
      auto  check_box1 = check_box("Alpha Stone");
      auto  check_box2 = check_box("The Zorane Gambit");
      auto  check_box3 = check_box("Cael Vosburgh's Exploration");

      check_box1.value(true);
      check_box2.value(true);
      check_box3.value(true);

      return
         hsize(300, simple_heading(
            margin({10, 10, 10, 10},
               vtile(
                  margin_top(10, align_left(check_box1)),
                  margin_top(10, align_left(check_box2)),
                  margin_top(10, align_left(check_box3))
               )
            ),
            "The Thraxian Legacy",
            1.1
         ));
   }

   auto make_page(view& view_)
   {
      auto msg_button = button("Message Box");
      msg_button.on_click =
         [&view_](bool)
         {
            char const* msg = "Patience... Wait for it...\n\n"
               "The nexus is overflowing with supercharged waveforms. "
               "Awareness is a constant. ";
            open_popup(message_box0(msg, icons::hand), view_);
         };

      auto alert_button = button("Alert");
      alert_button.on_click =
         [&view_](bool)
         {
            char const* alert_text =
               "We are being called to explore the cosmos itself as an "
               "interface between will and energy.";
            open_popup(message_box1(view_, alert_text, icons::attention, [](){}), view_);
         };

      auto choice_button = button("Choice");
      choice_button.on_click =
         [&view_](bool)
         {
            char const* choice_text =
               "Our conversations with other lifeforms have led to a "
               "summoning of ultra-amazing consciousness.";
            open_popup(
               message_box2(view_, choice_text, icons::question, [](){}, [](){}),
               view_
            );
         };

      auto dialog_button = button("Dialog");
      dialog_button.on_click =
         [&view_](bool)
         {
            open_popup(dialog2(view_, dialog_content(), [](){}, [](){}), view_);
         };

      return
         margin({20, 20, 20, 20},
            vtile(
               align_center(margin_top(10, label("Dialogs and popups").font_size(18))),
               margin_top(20, hsize(120, msg_button)),
               margin_top(20, hsize(120, alert_button)),
               margin_top(20, hsize(120, choice_button)),
               margin_top(20, hsize(120, dialog_button))
            )
         );
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Model (examples/model)
///////////////////////////////////////////////////////////////////////////////
namespace ns_model
{
   struct my_model
   {
      enum preset
      {
         preset_none,
         preset_100_percent,
         preset_75_percent,
         preset_50_percent,
         preset_25_percent,
         preset_0_percent,
      };

      value_model<float>   _value = 1.0;
      value_model<preset>  _preset = preset_100_percent;
   };

   auto make_dial(my_model& model, view& view_)
   {
      auto dial_ptr =
         share(
            dial(
               radial_marks<20>(basic_knob<80>()),
               1.0
            )
         );

      dial_ptr->on_change =
         [&model](double val)
         {
            if (model._value != val)
            {
               model._value = val;
               model._preset = my_model::preset_none;
            }
         };

      model._value.on_update(
         [&view_, dial_ptr](double val)
         {
            dial_ptr->value(val);
            view_.refresh(*dial_ptr);
         }
      );

      auto control = radial_labels<15>(
         hold(dial_ptr),
         0.7,
         "0", "1", "2", "3", "4",
         "5", "6", "7", "8", "9", "10"
      );

      return align_center_middle(control);
   }

   auto make_preset_menu(my_model& model, view& view_)
   {
      static char const* preset_labels[] = {
            "100 Percent",
            "75 Percent",
            "50 Percent",
            "25 Percent",
            "0 Percent",
         };

      static auto preset_map =
         std::unordered_map<std::string_view, my_model::preset> {
            { preset_labels[0], my_model::preset_100_percent  },
            { preset_labels[1], my_model::preset_75_percent },
            { preset_labels[2], my_model::preset_50_percent },
            { preset_labels[3], my_model::preset_25_percent  },
            { preset_labels[4], my_model::preset_0_percent  }
         };

      auto preset_menu =
         selection_menu(
            [&model](std::string_view select_str)
            {
               auto select = preset_map[select_str];

               model._preset = select;

               switch (select)
               {
                  case my_model::preset_100_percent:
                     model._value = 1.0;
                     break;
                  case my_model::preset_75_percent:
                     model._value = 0.75;
                     break;
                  case my_model::preset_50_percent:
                     model._value = 0.5;
                     break;
                  case my_model::preset_25_percent:
                     model._value = 0.25;
                     break;
                  case my_model::preset_0_percent:
                     model._value = 0.0;
                     break;
                  default:
                     break;
               };
            },
            preset_labels
         );

      model._preset.on_update(
         [&view_, label = preset_menu.second, &model](my_model::preset val)
         {
            if (val == my_model::preset_none)
            {
               auto text = label->get_text();
               if (text[0] != '*')
                  label->set_text("*" + std::string{text});
            }
            else
            {
               label->set_text(preset_labels[int(val)-1]);
            }
            view_.refresh(*label);
         }
      );

      return
         align_top(
            align_center(
               hsize(180,
                  htile_spaced(10.0,
                     label("Preset:"),
                     preset_menu.first
                  )
               )
            )
         );
   }

   auto make_input_box(my_model& model, view& view_)
   {
      auto tbox = input_box("value");

      model._value.on_update(
         [&view_, input = tbox.second](double val)
         {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2) << val;
            input->set_text(stream.str());
            input->select_all();
            view_.refresh(*input);
         }
      );

      tbox.second->on_enter =
         [&model, &view_](std::string_view text)->bool
         {
            std::string::size_type pos;
            std::string error{""};
            double val;

            try
            {
               val = std::stod(std::string{text}, &pos);
            }
            catch(std::invalid_argument const& e)
            {
               error = "Illegal characters in input.";
            }

            if (pos != text.size())
            {
               error = "Illegal characters in input.";
            }
            else if (val < 0.0 || val > 1.0)
            {
               error =
                  "Number is out of range. "
                  "Expected range is from 0.0 to 1.0."
                  ;
            }
            else
            {
               model._value = val;
               model._preset = my_model::preset_none;
            }

            if (error != "")
            {
               auto on_ok =
                  [&model]()
                  {
                     model._value.update();
                  };

               auto popup = message_box1(view_, error, icons::attention, on_ok);
               open_popup(popup, view_);
               return false;
            }
            return true;
         };

      tbox.second->on_escape =
         [&model]()->bool
         {
            model._value.update();
            return true;
         };

      return
         align_bottom(
            align_center(
               hsize(40, tbox.first)
            )
         );
   }

   auto make_content(my_model& model, view& view_)
   {
      static float const grid_coords[] = {0.1, 0.9, 1.0};

      return
         margin({20, 20, 20, 20},
            group(
               margin({20, 20, 20, 20},
                  vgrid(grid_coords,
                     make_preset_menu(model, view_),
                     make_dial(model, view_),
                     make_input_box(model, view_)
                  )
               )
            )
         );
   }

   auto make_page(view& view_)
   {
      static my_model model;
      return make_content(model, view_);
   }
}

///////////////////////////////////////////////////////////////////////////////
// Page: Animation (examples/simple_animation)
///////////////////////////////////////////////////////////////////////////////
namespace ns_animation
{
   float position = 0.0;
   constexpr float incr = 0.001;
   constexpr auto fps = 1000ms / 60;
   bool running = false;

   void animate(view& view_, vport_element& port)
   {
      if (!running)
         return;

      position += incr;
      if (position < 1.0)
      {
         auto* port_ptr = &port;
         view_.post(fps, [&view_, port_ptr]() { animate(view_, *port_ptr); });
      }
      else
      {
         position = 1.0;
      }
      port.valign(position);
      view_.refresh(port);
   }

   auto make_page(view& view_)
   {
      auto port = share(vport(image{"moving.png"}));
      auto play_stop = toggle_icon_button(icons::play, icons::stop, 2.0);

      play_stop.on_click =
         [port, &view_](bool state)
         {
            if (state)
            {
               running = true;
               position = 0.0;
               view_.post(fps, [port, &view_]() { animate(view_, *port); });
            }
            else
            {
               running = false;
            }
         };

      return
         margin({20, 20, 20, 20},
            vtile(
               align_center(margin_top(20, play_stop)),
               margin_top(20, hold(port))
            )
         );
   }
}

///////////////////////////////////////////////////////////////////////////////
// Main: assemble all pages into one notebook window, building pages lazily
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
   app _app("Elements Sampler");
   window _win(_app.name(), window::standard, {50, 50, 1280, 800});
   _win.on_close = [&_app]() { _app.stop(); };

   view view_(_win);

   // Page registry: a name and a builder. The builder is called only the
   // first time its tab is selected.
   struct page_spec
   {
      std::string                       name;
      std::function<element_ptr(view&)> build;
   };

   std::vector<page_spec> specs = {
      {"Sliders & Knobs",  [](view& v) { return share(ns_sliders::make_page(v)); }},
      {"Sprite Sliders",   [](view& v) { return share(ns_sprite_sliders::make_page(v)); }},
      {"Range Sliders",    [](view& v) { return share(ns_range_slider::make_page(v)); }},
      {"Buttons",          [](view& v) { return share(ns_buttons::make_page(v)); }},
      {"WinUI",            [](view&  ) { return share(ns_winui::make_page()); }},
      {"Menus",            [](view&  ) { return share(ns_menus::make_page()); }},
      {"Text & Icons",     [](view& v) { return share(ns_text_icons::make_page(v)); }},
      {"Text Edit",        [](view&  ) { return share(ns_text_edit::make_page()); }},
      {"Lists",            [](view&  ) { return share(ns_list::make_page()); }},
      {"Selection List",   [](view&  ) { return share(ns_selection::make_page()); }},
      {"Icons",            [](view&  ) { return share(ns_icons::make_page()); }},
      {"Thumbwheels",      [](view&  ) { return share(ns_thumbwheels::make_page()); }},
      {"Status Bars",      [](view& v) { return share(ns_status_bars::make_page(v)); }},
      {"Tooltips",         [](view& v) { return share(ns_tooltip::make_page(v)); }},
      {"Child Window",     [](view&  ) { return share(ns_child_window::make_page()); }},
      {"Custom Control",   [](view& v) { return share(ns_custom_control::make_page(v)); }},
      {"Scale",            [](view&  ) { return share(ns_scale::make_page()); }},
      {"Drop File",        [](view& v) { return share(ns_drop_file::make_page(v)); }},
      {"Dialogs & Popups", [](view& v) { return share(ns_dialogs::make_page(v)); }},
      {"Model",            [](view& v) { return share(ns_model::make_page(v)); }},
      {"Animation",        [](view& v) { return share(ns_animation::make_page(v)); }}
   };

   // Deck of pages. Every page starts as a placeholder and is replaced by
   // its real content the first time it is selected.
   auto pages = std::make_shared<deck_composite>();
   auto built = std::make_shared<std::vector<bool>>(specs.size(), false);

   // Tabs
   std::vector<element_ptr> tabs;
   for (std::size_t i = 0; i < specs.size(); ++i)
   {
      pages->push_back(share(empty()));

      auto t = share(tab(specs[i].name));
      if (auto* btn = find_element<basic_button*>(t.get()))
      {
         btn->on_click =
            [i, pages, built, build = specs[i].build, &view_](bool state) mutable
            {
               if (state)
               {
                  if (!(*built)[i])
                  {
                     (*pages)[i] = build(view_);
                     (*built)[i] = true;
                  }
                  pages->select(i);
                  view_.refresh(*pages);
               }
            };
      }
      tabs.push_back(t);
   }

   // Build and select the first page eagerly
   (*pages)[0] = specs[0].build(view_);
   (*built)[0] = true;
   pages->select(0);

   // Select the first tab (choice button)
   if (auto* btn = find_element<basic_button*>(tabs[0].get()))
      btn->value(true);

   // Theme toggles: the scheme toggle switches dark/light and the style
   // toggle switches classic/WinUI skins. They compose, so any scheme can
   // be combined with either skin (set_theme: color-only switch repaints,
   // font/layout switch re-layouts).
   auto theme_toggle = share(toggle_button("Theme: Dark"));
   auto style_toggle = share(toggle_button("WinUI Style: Off"));
   std::weak_ptr<std::remove_reference_t<decltype(*theme_toggle)>> weak_toggle =
      theme_toggle;
   std::weak_ptr<std::remove_reference_t<decltype(*style_toggle)>> weak_style =
      style_toggle;

   auto apply_theme = [](bool dark, bool winui)
   {
      auto start = std::chrono::steady_clock::now();
      set_theme(
         winui? make_winui_theme(dark) :
         dark? make_dark_theme() :
         make_light_theme()
      );
      auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
         std::chrono::steady_clock::now() - start
      ).count();
      std::cout << "[theme] switched to "
                << (winui? "winui-" : "")
                << (dark? "dark" : "light")
                << ", set_theme took " << elapsed << " us" << std::endl;
   };

   theme_toggle->on_click =
      [&view_, weak_toggle, weak_style, apply_theme](bool state)
      {
         bool dark = !state;
         bool winui = false;
         if (auto t = weak_style.lock())
            winui = t->value();

         apply_theme(dark, winui);

         // Keep the label in sync with the toggle state (avoid capturing
         // the shared_ptr by value, which would create a reference cycle)
         if (auto t = weak_toggle.lock())
         {
            t->actual_subject().set_text(
               state ? "Theme: Light" : "Theme: Dark"
            );
            view_.refresh(*t);
         }
      };

   style_toggle->on_click =
      [&view_, weak_toggle, weak_style, apply_theme](bool state)
      {
         bool dark = true;
         if (auto t = weak_toggle.lock())
            dark = !t->value();

         apply_theme(dark, state);

         if (auto t = weak_style.lock())
         {
            t->actual_subject().set_text(
               state ? "WinUI Style: On" : "WinUI Style: Off"
            );
            view_.refresh(*t);
         }
      };

   // Vertical tab bar on the left, with the theme toggles at the top
   vtile_composite tab_bar;
   tab_bar.push_back(
      share(align_center(margin({10, 10, 10, 10}, hold(theme_toggle))))
   );
   tab_bar.push_back(
      share(align_center(margin({10, 0, 10, 10}, hold(style_toggle))))
   );
   for (auto& t : tabs)
      tab_bar.push_back(share(hold(t)));

   view_.content(
      htile(
         align_top(std::move(tab_bar)),
         hold(pages)
      ),
      themed_background{}
   );

   _app.run();
   return 0;
}
