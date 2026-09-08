/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License (https://opensource.org/licenses/MIT)

   Key mapping ported to C++ from GLFW3

   Copyright (c) 2009-2016 Camilla Berglund <elmindreda@glfw.org>

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would
      be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not
      be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
      distribution.
=============================================================================*/
#include <elements/base_view.hpp>
#include <elements/support/canvas.hpp>
#include <elements/support/resource_paths.hpp>
#include <elements/support/theme.hpp>
#include <cairo.h>
#include <cairo-win32.h>
#include <Windowsx.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <map>
#include "drag_and_drop.hpp"
#include "utils.hpp"

namespace cycfi::elements
{
   key_code translate_key(WPARAM wparam, LPARAM lparam);

   // Convert a wide Unicode string to an UTF8 string
   std::string utf8_encode(std::wstring const& wstr)
   {
      if (wstr.empty())
         return {};
      int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
      std::string result(size, 0);
      WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, nullptr, nullptr);
      return result;
   }

   // Convert an UTF8 string to a wide Unicode String
   std::wstring utf8_decode(std::string const& str)
   {
      if (str.empty())
         return {};
      int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
      std::wstring result(size, 0);
      MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
      return result;
   }

   namespace
   {
      constexpr unsigned IDT_TIMER1 = 100;
      HCURSOR current_cursor = nullptr;

      struct view_info
      {
         using time_point = std::chrono::time_point<std::chrono::steady_clock>;
         using key_map = std::map<key_code, key_action>;

         base_view*     vptr = nullptr;
         bool           is_dragging = false;
         bool           drag_started = false;
         POINT          drag_start = {};
         HDC            hdc = nullptr;
         HDC            offscreen_hdc = nullptr;
         HBITMAP        offscreen_buff = nullptr;
         int            w = 0;
         int            h = 0;
         bool           mouse_in_window = false;
         time_point     click_start = {};
         int            click_count = 0;
         time_point     _scroll_start = {};
         double         _velocity = 0;
         point          _scroll_dir;
         key_map        keys = {};
         std::uint16_t  pending_high_surrogate = 0;
      };

      view_info* get_view_info(HWND hwnd)
      {
         auto param = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
         return reinterpret_cast<view_info*>(param);
      }

      void make_offscreen_dc(HDC hdc, view_info* info, int w, int h)
      {
         info->hdc = hdc;
         info->w = w;
         info->h = h;

         // Free-up the previous off-screen DC
         if (info->offscreen_buff)
            DeleteObject(info->offscreen_buff);
         if (info->offscreen_hdc)
            DeleteDC(info->offscreen_hdc);

         // Create an off-screen DC for double-buffering
         info->offscreen_hdc = CreateCompatibleDC(hdc);
         info->offscreen_buff = CreateCompatibleBitmap(hdc, w, h);
      }

      LRESULT on_paint(HWND hwnd, view_info* info)
      {
         if (base_view* view = info->vptr)
         {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT dirty = ps.rcPaint;
            SetBkMode(hdc, TRANSPARENT);

            RECT r;
            GetWindowRect(hwnd, &r);
            auto win_width = r.right-r.left;
            auto win_height = r.bottom-r.top;

            if (hdc != info->hdc || win_width != info->w || win_height != info->h)
               make_offscreen_dc(hdc, info, win_width, win_height);

            HANDLE hold = SelectObject(info->offscreen_hdc, info->offscreen_buff);

            // The off-screen bitmap is reused between paints. Clear only the
            // invalidated pixels before drawing so transparent cached text
            // cannot preserve an old caret or selection highlight.
            auto const& bg = get_theme().window_background_color;
            auto to_byte = [](float value)
            {
               return static_cast<BYTE>(
                  std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            auto brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
            SetDCBrushColor(info->offscreen_hdc, RGB(
               to_byte(bg.red), to_byte(bg.green), to_byte(bg.blue)));
            FillRect(info->offscreen_hdc, &dirty, brush);

            // Create the cairo surface and context.
            cairo_surface_t* surface = cairo_win32_surface_create(info->offscreen_hdc);
            cairo_t* context = cairo_create(surface);
            auto scale = get_scale_for_window(hwnd);
            cairo_scale(context, scale, scale);

            view->draw(context);

            // Cleanup.
            cairo_destroy(context);
            cairo_surface_destroy(surface);

            // Transfer the off-screen DC to the screen
            auto w = dirty.right-dirty.left;
            auto h = dirty.bottom-dirty.top;
            BitBlt(hdc, dirty.left, dirty.top, w, h, info->offscreen_hdc
              , dirty.left, dirty.top, SRCCOPY);

            SelectObject(info->offscreen_hdc, hold);
            EndPaint(hwnd, &ps);
         }
         return 0;
      }

      int get_mods()
      {
         int mods = 0;

         auto&& test = [](int key)
         {
            return GetAsyncKeyState(key) & 0x8000;
         };

         if (test(VK_SHIFT))
            mods |= mod_shift;
         if (test(VK_CONTROL))
            mods |= mod_control | mod_action;
         if (test(VK_MENU))
            mods |= mod_alt;
         if (test(VK_LWIN) || test(VK_RWIN))
            mods |= mod_super;

         return mods;
      }

      mouse_button get_button(
         HWND hwnd, view_info* info, UINT message
       , WPARAM /* wparam */, LPARAM lparam)
      {
         float pos_x = GET_X_LPARAM(lparam);
         float pos_y = GET_Y_LPARAM(lparam);

         auto scale = get_scale_for_window(hwnd);
         pos_x /= scale;
         pos_y /= scale;

         bool down = info->is_dragging;
         switch (message)
         {
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
               {
                  auto now = std::chrono::steady_clock::now();
                  auto elapsed = now - info->click_start;
                  info->click_start = now;
                  if (elapsed > std::chrono::milliseconds(GetDoubleClickTime()))
                     info->click_count = 1;
                  else
                     ++info->click_count;
                  if (!info->is_dragging)
                  {
                     info->is_dragging = true;
                     info->drag_started = false;
                     info->drag_start = {
                        GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                     SetCapture(hwnd);
                  }
                  down = true;
               }
               break;

            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
               down = false;
               if (info->is_dragging)
               {
                  info->is_dragging = false;
                  info->drag_started = false;
                  ReleaseCapture();
               }
               break;
         }

         auto const which =
             [message]()
             {
                 switch (message)
                 {
                     case WM_LBUTTONDOWN:
                     case WM_LBUTTONUP:
                         return mouse_button::left;

                     case WM_MBUTTONDOWN:
                     case WM_MBUTTONUP:
                         return mouse_button::middle;

                     case WM_RBUTTONDOWN:
                     case WM_RBUTTONUP:
                         return mouse_button::right;

                     default:
                         return mouse_button::left;
                 }
             }();

         return {
            down,
            info->click_count,
            which,
            get_mods(),
            {pos_x, pos_y}
         };
      }

      bool drag_threshold_crossed(view_info const& info, LPARAM lparam)
      {
         auto dx = std::abs(GET_X_LPARAM(lparam) - info.drag_start.x);
         auto dy = std::abs(GET_Y_LPARAM(lparam) - info.drag_start.y);
         return dx >= GetSystemMetrics(SM_CXDRAG) ||
            dy >= GetSystemMetrics(SM_CYDRAG);
      }

      bool handle_key(base_view& _view, view_info::key_map& keys, key_info k)
      {
         bool repeated = false;

         if (k.action == key_action::release
            && keys[k.key] == key_action::release)
            return false;

         if (k.action == key_action::press
            && keys[k.key] == key_action::press)
            repeated = true;

         keys[k.key] = k.action;

         if (repeated)
            k.action = key_action::repeat;

         return _view.key(k);
      }

      bool on_key(HWND /* hwnd */, view_info* info, WPARAM wparam, LPARAM lparam)
      {
         auto const key = translate_key(wparam, lparam);
         auto const action = ((lparam >> 31) & 1) ? key_action::release : key_action::press;
         auto mods = get_mods();

         // GetAsyncKeyState can lag behind an injected or queued key message.
         // Preserve modifier state from the per-view key map while dispatching
         // the following key event.
         auto key_down = [info](key_code code)
         {
            auto i = info->keys.find(code);
            return i != info->keys.end() &&
               (i->second == key_action::press ||
                i->second == key_action::repeat);
         };
         if (key_down(key_code::left_shift) || key_down(key_code::right_shift))
            mods |= mod_shift;
         if (key_down(key_code::left_control) ||
             key_down(key_code::right_control))
            mods |= mod_control | mod_action;
         if (key_down(key_code::left_alt) || key_down(key_code::right_alt))
            mods |= mod_alt;
         if (key_down(key_code::left_super) || key_down(key_code::right_super))
            mods |= mod_super;

         if (key == key_code::unknown)
            return false;

         if (action == key_action::release && wparam == VK_SHIFT)
         {
            // HACK: Release both Shift keys on Shift up event, as when both
            //       are pressed the first release does not emit any event
            bool r1 = handle_key(*info->vptr, info->keys, {key_code::left_shift, action, mods});
            bool r2 = handle_key(*info->vptr, info->keys, {key_code::right_shift, action, mods});
            return r1 || r2;
         }
         else if (wparam == VK_SNAPSHOT)
         {
            // HACK: Key down is not reported for the Print Screen key
            bool r1 = handle_key(*info->vptr, info->keys, {key, key_action::press, mods});
            bool r2 = handle_key(*info->vptr, info->keys, {key, key_action::release, mods});
            return r1 || r2;
         }

         return handle_key(*info->vptr, info->keys, {key, action, mods});
      }

      void on_cursor(HWND hwnd, base_view* view, LPARAM lparam, cursor_tracking state)
      {
         float pos_x = GET_X_LPARAM(lparam);
         float pos_y = GET_Y_LPARAM(lparam);

         auto scale = get_scale_for_window(hwnd);
         pos_x /= scale;
         pos_y /= scale;

         view->cursor({pos_x, pos_y}, state);
      }

      namespace
      {
         static auto mouse_wheel_line_delta =
            []{
               UINT wheel_scroll_lines;
               SystemParametersInfoA(SPI_GETWHEELSCROLLLINES, 0, &wheel_scroll_lines, 0);
               return float(WHEEL_DELTA) / wheel_scroll_lines;
            }();
      }

      void on_scroll(HWND hwnd, view_info* info, LPARAM lparam, point dir)
      {
         static constexpr auto accel_weight = 0.1;
         auto acceleration = 1 + (std::max(std::abs(dir.x), std::abs(dir.y)) * accel_weight);
         auto now = std::chrono::steady_clock::now();
         auto elapsed = now - info->_scroll_start;
         info->_scroll_start = now;

         std::chrono::duration<double, std::milli> fp_ms = elapsed;

         bool reset_accel =
            elapsed > std::chrono::milliseconds(250) ||
            (info->_scroll_dir.x > 0 != dir.x > 0) ||
            (info->_scroll_dir.y > 0 != dir.y > 0)
            ;
         info->_scroll_dir = dir;
         if (reset_accel)
            info->_velocity = 1.0;
         else
            info->_velocity *= acceleration;

         static constexpr auto max_velocity = 100.0;
         dir.x *= std::min(info->_velocity, max_velocity);
         dir.y *= std::min(info->_velocity, max_velocity);

         POINT pos;
         pos.x = GET_X_LPARAM(lparam);
         pos.y = GET_Y_LPARAM(lparam);
         ScreenToClient(hwnd, &pos);

         float scale = get_scale_for_window(hwnd);
         info->vptr->scroll(dir, {pos.x / scale, pos.y / scale});
      }

      bool on_text(view_info* info, base_view& view, UINT message, WPARAM wparam)
      {
         if (message == WM_UNICHAR && wparam == UNICODE_NOCHAR)
         {
            // WM_UNICHAR is not sent by Windows, but is sent by some
            // third-party input method engine Returning true here announces
            // support for this message
            return true;
         }

         if (message == WM_SYSCHAR)
         {
            info->pending_high_surrogate = 0;
            return false;
         }

         std::uint32_t codepoint = 0;
         if (message == WM_UNICHAR)
         {
            // WM_UNICHAR carries a complete UTF-32 codepoint.
            info->pending_high_surrogate = 0;
            codepoint = static_cast<std::uint32_t>(wparam);
         }
         else
         {
            // WM_CHAR carries UTF-16 code units. Supplementary characters
            // arrive as a high/low-surrogate pair and must be combined before
            // they reach the UTF-8 document model.
            auto unit = static_cast<std::uint16_t>(wparam);
            if (unit >= 0xD800 && unit <= 0xDBFF)
            {
               info->pending_high_surrogate = unit;
               return true;
            }
            if (unit >= 0xDC00 && unit <= 0xDFFF)
            {
               if (!info->pending_high_surrogate)
                  return false;
               codepoint = 0x10000u +
                  ((std::uint32_t(info->pending_high_surrogate) - 0xD800u) << 10) +
                  (unit - 0xDC00u);
               info->pending_high_surrogate = 0;
            }
            else
            {
               // An unpaired high surrogate is discarded when the next
               // character is not its low-surrogate continuation.
               info->pending_high_surrogate = 0;
               codepoint = unit;
            }
         }

         if (codepoint < 32 || (codepoint > 126 && codepoint < 160))
            return 0;

         if (codepoint > 0x10FFFF ||
             (codepoint >= 0xD800 && codepoint <= 0xDFFF))
            return false;
         return view.text({codepoint, get_mods()});
      }

      LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
      {
         auto* info = get_view_info(hwnd);
         switch (message)
         {
            case WM_PAINT:
               return on_paint(hwnd, info);

            case WM_ERASEBKGND:
               return true;

            case WM_NCHITTEST:
               {
                  // For frameless resizable windows, the system sends
                  // WM_NCHITTEST to this child view (it covers the whole
                  // client area). Pass edge hit-testing through to the
                  // top-level window via HTTRANSPARENT so the OS performs
                  // the resize; everything else stays HTCLIENT and is
                  // handled by the view (buttons, title-bar dragging).
                  auto parent = GetParent(hwnd);
                  if (parent && GetPropW(parent, L"ElementsFramelessResizable"))
                  {
                     POINT pt{int(short(LOWORD(lparam))), int(short(HIWORD(lparam)))};
                     RECT r;
                     GetWindowRect(hwnd, &r);

                     auto scale = get_scale_for_window(hwnd);
                     auto m = LONG(6 * scale);

                     bool left = pt.x < r.left + m;
                     bool right = pt.x >= r.right - m;
                     bool top = pt.y < r.top + m;
                     bool bottom = pt.y >= r.bottom - m;

                     if (left || right || top || bottom)
                        return HTTRANSPARENT;
                  }
                  return DefWindowProcW(hwnd, message, wparam, lparam);
               }

            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
               SetFocus(hwnd);
               [[fallthrough]];

            case WM_LBUTTONUP: case WM_MBUTTONUP: case WM_RBUTTONUP:
               // $$$ JDG $$$ todo: prevent double btn up and down
               info->vptr->click(get_button(hwnd, info, message, wparam, lparam));
               break;

            case WM_MOUSEMOVE:
               if (info->is_dragging)
               {
                  if (!info->drag_started &&
                      drag_threshold_crossed(*info, lparam))
                     info->drag_started = true;
                  if (info->drag_started)
                     info->vptr->drag(
                        get_button(hwnd, info, message, wparam, lparam));
               }
               else
               {
                  if (!info->mouse_in_window)
                  {
                     on_cursor(hwnd, info->vptr, lparam, cursor_tracking::entering);
                     info->mouse_in_window = true;
                  }
                  TRACKMOUSEEVENT tme;
                  tme.cbSize = sizeof(tme);
                  tme.hwndTrack = hwnd;
                  tme.dwFlags = TME_HOVER | TME_LEAVE;
                  tme.dwHoverTime = 16;
                  TrackMouseEvent(&tme);
               }
               break;

            case WM_MOUSELEAVE:
               info->mouse_in_window = false;
               on_cursor(hwnd, info->vptr, lparam, cursor_tracking::leaving);
               break;

            case WM_CAPTURECHANGED:
               // A capture can be stolen by another window without sending
               // a button-up to this view. Do not leave the next click in
               // the drag path.
               info->is_dragging = false;
               info->drag_started = false;
               break;

            case WM_MOUSEHOVER:
               on_cursor(hwnd, info->vptr, lparam, cursor_tracking::hovering);
               break;

            case WM_MOUSEWHEEL:
               {
                  float delta = GET_WHEEL_DELTA_WPARAM(wparam);
                  on_scroll(hwnd, info, lparam, {0, delta / mouse_wheel_line_delta});
               }
               break;

            case WM_MOUSEHWHEEL:
               {
                  float delta = -GET_WHEEL_DELTA_WPARAM(wparam);
                  on_scroll(hwnd, info, lparam, {delta / mouse_wheel_line_delta, 0});
               }
               break;

            case WM_SETCURSOR:
               if (LOWORD(lparam) == HTCLIENT && current_cursor != GetCursor())
                  SetCursor(current_cursor);
               break;

            case WM_TIMER:
               if (wparam == IDT_TIMER1)
                  info->vptr->poll();
               break;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
               {
                  bool handled = on_key(hwnd, info, wparam, lparam);
                  if (!handled)
                  {
                     HWND rootHWnd = GetAncestor(hwnd, GA_ROOT);
                     SendMessage(rootHWnd, message, wparam, lparam);
                     return DefWindowProc(hwnd, message, wparam, lparam);
                  }
                  return handled;
               }
               break;

            case WM_CHAR:
            case WM_SYSCHAR:
            case WM_UNICHAR:
               return on_text(info, *info->vptr, message, wparam);

            case WM_SETFOCUS:
               info->vptr->begin_focus();
               break;

            case WM_KILLFOCUS:
               {
                  info->pending_high_surrogate = 0;
                  info->vptr->end_focus();
               }
               break;

            default:
               return DefWindowProcW(hwnd, message, wparam, lparam);
         }
         return 0;
      }

      struct init_view_class
      {
         init_view_class()
         {
            WNDCLASSW windowClass = {};
            windowClass.hbrBackground = nullptr;
            windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
            windowClass.hInstance = nullptr;
            windowClass.lpfnWndProc = WndProc;
            windowClass.lpszClassName = L"ElementsView";
            windowClass.style = CS_HREDRAW | CS_VREDRAW;
            if (!RegisterClassW(&windowClass))
               MessageBoxW(nullptr, L"Could not register class", L"Error", MB_OK);

            auto pwd = fs::current_path();
            add_search_path(pwd / "resources");
         }
      };
   }

   namespace
   {
      HWND make_window(base_view* _this, host_window_handle parent, RECT bounds)
      {
         static init_view_class init;

         HWND hwnd = CreateWindowW(
            L"ElementsView",
            nullptr,
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0,
            parent, nullptr, nullptr,
            nullptr
         );

         MoveWindow(
            hwnd, bounds.left, bounds.top,
            bounds.right-bounds.left, bounds.bottom-bounds.top,
            true // repaint
         );

         view_info* info = new view_info{_this};
         SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(info));

         // Create 1ms timer
         SetTimer(hwnd, IDT_TIMER1, 1, (TIMERPROC) nullptr);

         // Create and register the drop target
         if (IDropTarget* pDropTarget = DropTarget::CreateInstance(_this, hwnd))
         {
            RegisterDragDrop(hwnd, pDropTarget);
            pDropTarget->Release();
         }

         SetFocus(hwnd);
         return hwnd;
      }
   }

   base_view::base_view(extent size_)
   {
	   RECT bounds = {0, 0, LONG(size_.x), LONG(size_.y)};
	   _view = make_window(this, nullptr, bounds);
   }

   base_view::base_view(host_window_handle h)
   {
      RECT bounds;
      GetClientRect(h, &bounds);
      _view = make_window(this, h, bounds);
   }

   base_view::~base_view()
   {
      auto info = get_view_info(_view);

      // Free-up the off-screen DC
      if (info->offscreen_buff)
         DeleteObject(info->offscreen_buff);
      if (info->offscreen_hdc)
         DeleteDC(info->offscreen_hdc);

      KillTimer(_view, IDT_TIMER1);
      RevokeDragDrop(_view);
      delete info;
      DeleteObject(_view);
   }

   point base_view::cursor_pos() const
   {
      POINT pos;
      GetCursorPos(&pos);
      ScreenToClient(_view, &pos);
      float scale = get_scale_for_window(_view);
      return {float(pos.x) / scale, float(pos.y) / scale};
   }

   elements::extent base_view::size() const
   {
      float scale = get_scale_for_window(_view);
      RECT r;
      GetWindowRect(_view, &r);
      return {float(r.right-r.left) / scale, float(r.bottom-r.top) / scale};
   }

   void base_view::size(elements::extent p)
   {
      auto scale = get_scale_for_window(_view);
      auto parent = GetParent(_view);
      RECT bounds;
      GetClientRect(parent, &bounds);

      MoveWindow(
         _view, bounds.left, bounds.top,
         p.x * scale, p.y * scale,
         true // repaint
      );
   }

   void base_view::refresh()
   {
      RECT bounds;
      GetClientRect(_view, &bounds);
      InvalidateRect(_view, &bounds, false);
   }

   void base_view::refresh(rect area)
   {
      auto scale = get_scale_for_window(_view);
      RECT r;
      // Invalidation must contain every pixel touched by an antialiased
      // edge. Truncating the right/bottom edge can leave the last caret
      // column or row outside the update region at fractional DPI scales.
      r.left = LONG(std::floor(area.left * scale));
      r.right = LONG(std::ceil(area.right * scale));
      r.top = LONG(std::floor(area.top * scale));
      r.bottom = LONG(std::ceil(area.bottom * scale));
      if (r.right <= r.left)
         r.right = r.left + 1;
      if (r.bottom <= r.top)
         r.bottom = r.top + 1;
      InvalidateRect(_view, &r, false);
   }

   std::string clipboard()
   {
      if (!OpenClipboard(nullptr))
         return {};

      HANDLE object = GetClipboardData(CF_UNICODETEXT);
      if (!object)
      {
         CloseClipboard();
         return {};
      }

      WCHAR* buffer = static_cast<WCHAR*>(GlobalLock(object));
      if (!buffer)
      {
         CloseClipboard();
         return {};
      }

      std::wstring source{buffer, std::char_traits<WCHAR>::length(buffer)};

      GlobalUnlock(object);
      CloseClipboard();

      return utf8_encode(source);
   }

   void clipboard(std::string const& text)
   {
      auto len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
      if (!len)
         return;

      HANDLE object = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
      if (!object)
         return;

      WCHAR* buffer = static_cast<WCHAR*>(GlobalLock(object));
      if (!buffer)
      {
         GlobalFree(object);
         return;
      }

      MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buffer, len);
      GlobalUnlock(object);

      if (!OpenClipboard(nullptr))
      {
         GlobalFree(object);
         return;
      }

      EmptyClipboard();
      if (!SetClipboardData(CF_UNICODETEXT, object))
         GlobalFree(object);
      CloseClipboard();
   }

   void set_cursor(cursor_type type)
   {
      struct cursors
      {
         cursors()
         {
            _cursors[cursor_type::arrow]        = LoadCursor(nullptr, IDC_ARROW);
            _cursors[cursor_type::ibeam]        = LoadCursor(nullptr, IDC_IBEAM);
            _cursors[cursor_type::cross_hair]   = LoadCursor(nullptr, IDC_CROSS);
            _cursors[cursor_type::hand]         = LoadCursor(nullptr, IDC_HAND);
            _cursors[cursor_type::h_resize]     = LoadCursor(nullptr, IDC_SIZEWE);
            _cursors[cursor_type::v_resize]     = LoadCursor(nullptr, IDC_SIZENS);
         }

         std::map<cursor_type, HCURSOR> _cursors;
      };
      static cursors data;

      HCURSOR cursor = data._cursors[type];
      if (cursor != GetCursor())
      {
         current_cursor = cursor;
         SetCursor(cursor);
         ShowCursor(true);
      }
   }

   namespace
   {
      int get_scroll_direction()
      {
         const wchar_t* path = L"Software\\Microsoft\\Windows\\CurrentVersion\\PrecisionTouchPad";
         const wchar_t* name = L"ScrollDirection";

         HKEY hkey;

         // Open the registry key
         LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &hkey);

         if (result == ERROR_SUCCESS)
         {
            // Read the specified value from the registry
            DWORD value;
            DWORD dataSize = sizeof(value);

            result = RegQueryValueExW(hkey, name, nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &dataSize);

            // Close the registry key
            RegCloseKey(hkey);

            if (result == ERROR_SUCCESS)
               return value == 0? 1 : -1;
         }
         // Return default 1 if there was an error or the value was not found
         return 1;
      }
   }

   point scroll_direction()
   {
      static int scroll_dir = get_scroll_direction();
      return {1.0f, 1.0f * scroll_dir};
   }
}
