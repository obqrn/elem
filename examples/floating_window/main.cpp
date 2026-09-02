/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <elements.hpp>

using namespace cycfi::elements;

// Main window background color
auto constexpr bkd_color = rgba(35, 35, 37, 255);
auto background = box(bkd_color);

///////////////////////////////////////////////////////////////////////////////
// window_drag_base
//
// A proxy that moves its host OS window when the subject (e.g. a custom
// title bar) is dragged. Unlike the built-in `movable` element (which moves
// an in-view floating_element), this moves the actual top-level window via
// window::position(), so the window can be dragged anywhere on the desktop,
// including outside the main window.
//
// Drag math: while dragging, the host window follows the cursor, so the
// client coordinates reported in each drag event are relative to the already
// moved window. Instead of accumulating per-event deltas (which would lag),
// we re-anchor every event:
//
//    new_position = current_position + (current_cursor - start_cursor)
//
// `start_cursor` is captured once at click time. Since
// current_position + current_cursor tracks the cursor in a fixed space
// (the frame-to-client offset is constant), this formula is exact.
///////////////////////////////////////////////////////////////////////////////
class window_drag_base : public proxy_base
{
public:

   using proxy_base::proxy_base;

   bool                    click(context const& ctx, mouse_button btn) override;
   void                    drag(context const& ctx, mouse_button btn) override;
   element*                hit_test(context const& ctx, point p, bool leaf, bool control) override;
   bool                    wants_control() const override { return true; }

   std::function<window*()> get_window;

private:

   bool                    _dragging = false;
   point                   _cursor_start;
};

// The whole title bar is a valid hit target (like movable_base does), so
// dragging works even when the click misses the close button.
element* window_drag_base::hit_test(context const& ctx, point p, bool /*leaf*/, bool /*control*/)
{
   if (ctx.enabled && is_enabled() && ctx.bounds.includes(p))
      return this;
   return nullptr;
}

bool window_drag_base::click(context const& ctx, mouse_button btn)
{
   // Let the subject (e.g. a close button) handle the click first.
   bool handled = proxy_base::click(ctx, btn);
   if (!handled)
   {
      if (btn.down && get_window && get_window())
      {
         _dragging = true;
         _cursor_start = btn.pos;
         return true;
      }
      _dragging = false;
   }
   else
   {
      // The subject consumed the click (e.g. a button); don't leave a
      // drag latched from an earlier press on the title-bar area.
      _dragging = false;
   }
   return handled;
}

void window_drag_base::drag(context const& ctx, mouse_button btn)
{
   if (_dragging && get_window)
   {
      if (auto* win = get_window())
      {
         auto w = win->position();
         win->position(w.move(
            btn.pos.x - _cursor_start.x,
            btn.pos.y - _cursor_start.y
         ));
      }
   }
   else
   {
      proxy_base::drag(ctx, btn);
   }
}

template <concepts::Element Subject>
inline proxy<cycfi::remove_cvref_t<Subject>, window_drag_base>
window_drag(window& win, Subject&& subject)
{
   auto p = proxy<cycfi::remove_cvref_t<Subject>, window_drag_base>
      {std::forward<Subject>(subject)};

   p.get_window = [&win]() { return &win; };
   return p;
}

//////////////////////////////////////////////////////////////////////////////
// window_content_base
//
// Reports the subject's minimum size as the minimum, but allows the maximum
// to grow to full extent. Without this, a fixed-size child (e.g. a label,
// whose min == max == text size) would pin the window's maximum size and
// prevent horizontal resizing.
///////////////////////////////////////////////////////////////////////////////
class window_content_base : public proxy_base
{
public:

   using proxy_base::proxy_base;

   view_limits             limits(basic_context const& ctx) const override;
};

view_limits window_content_base::limits(basic_context const& ctx) const
{
   auto  e_limits = this->subject().limits(ctx);
   return {{e_limits.min.x, e_limits.min.y}, {full_extent, full_extent}};
}

template <concepts::Element Subject>
inline proxy<cycfi::remove_cvref_t<Subject>, window_content_base>
window_content(Subject&& subject)
{
   return {std::forward<Subject>(subject)};
}

///////////////////////////////////////////////////////////////////////////////
// Frameless floating window content
///////////////////////////////////////////////////////////////////////////////
auto make_title_bar(window& win)
{
   auto min_btn = icon_button(icons::angle_down, 0.8, rgba(0, 0, 0, 0));
   min_btn.on_click = [&win](bool) { win.minimize(); };

   auto max_btn = icon_button(icons::angle_up, 0.8, rgba(0, 0, 0, 0));
   max_btn.on_click = [&win](bool) { win.maximize(); };

   auto close_btn = icon_button(icons::cancel, 0.8, rgba(0, 0, 0, 0));
   close_btn.on_click = [&win](bool) { win.close(); };

   auto buttons = htile(min_btn, max_btn, close_btn);

   return window_drag(win,
      layer(
         align_right(margin({4, 4, 8, 4}, buttons)),
         margin({10, 5, 10, 5}, align_left(label("Floating Window"))),
         title_bar{}
      )
   );
}

auto make_floating_content(window& win)
{
   return vtile(
      make_title_bar(win),
      margin({20, 20, 20, 20}, align_middle(label(
         "Drag the title bar to move,\n"
         "resize from any edge,\n"
         "buttons: minimize | maximize | close."
      )))
   );
}

int main(int argc, char* argv[])
{
   app _app("Floating Window");
   window _win(_app.name());
   _win.on_close = [&_app]() { _app.stop(); };

   view view_(_win);
   view_.content(
      window_content(
         margin({20, 20, 20, 20}, align_middle(label("Close the main window to exit.")))
      ),
      background
   );

   // A separate, frameless, resizable top-level window (window::resizable
   // only; no with_title). On Windows the host handles edge hit-testing
   // and resizing itself (WM_NCHITTEST + capture-based resize loop).
   window _float("Floating", window::resizable, rect{260, 160, 580, 400});
   view float_view(_float);
   float_view.content(
      window_content(make_floating_content(_float)),
      background
   );

   _app.run();
   return 0;
}
