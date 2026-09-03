/*=============================================================================
   Copyright (c) 2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_STYLE_TAB_MAY_02_2020)
#define ELEMENTS_STYLE_TAB_MAY_02_2020

#include <elements/element/style/button.hpp>
#include <elements/element/size.hpp>
#include <optional>

namespace cycfi::elements
{
   template <concepts::Element Button>
   inline auto tab(Button&& button)
   {
      return choice(std::forward<Button>(button));
   }

   // Tab styler whose active color follows the current theme at draw time
   // unless an explicit color was provided
   struct tab_styler : basic_button_styler
   {
      // Keep the gen chain wrapping tab_styler itself so .rounded_top()
      // copies the full styler instead of slicing it to basic_button_styler
      using base_type = tab_styler;

                              tab_styler(
                                 std::string text
                               , std::optional<color> active = std::nullopt
                              )
                               : basic_button_styler(std::move(text))
                               , _active(active)
                              {}

      color                   get_active_body_color() const override
      {
         return (_active ? *_active : get_theme().active_tab_color).opacity(0.5);
      }

   private:

      std::optional<color>    _active;
   };

   inline auto tab(
      std::string text
    , std::optional<color> active_color = std::nullopt
   )
   {
      return tab(
         hmin(hmin_pad(20,
            button_styler_gen<tab_styler>{std::move(text), active_color}
               .rounded_top()
         ))
      );
   }
}

#endif
