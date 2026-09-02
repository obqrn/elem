/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License (https://opensource.org/licenses/MIT)
=============================================================================*/
#include <elements/window.hpp>
#include <elements/support.hpp>

#include "utils.hpp"

namespace elements = cycfi::elements;

namespace cycfi::elements
{
   // UTF8 conversion utils defined in base_view.cpp

   // Convert a wide Unicode string to an UTF8 string
   std::string utf8_encode(std::wstring const& wstr);

   // Convert an UTF8 string to a wide Unicode String
   std::wstring utf8_decode(std::string const& str);

   namespace
   {
      struct window_info
      {
         window*     wptr = nullptr;
         view_limits limits = {};

         // State for the custom borderless-resize loop (see WM_NCLBUTTONDOWN
         // in handle_event).
         int         sizing_edge = 0;
         RECT        sizing_start = {};
         POINT       sizing_grab = {};
      };

      window_info* get_window_info(HWND hwnd)
      {
         auto param = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
         return reinterpret_cast<window_info*>(param);
      }

      void disable_close(HWND hwnd)
      {
         EnableMenuItem(GetSystemMenu(hwnd, FALSE), SC_CLOSE,
            MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
      }

      void disable_minimize(HWND hwnd)
      {
         SetWindowLongW(hwnd, GWL_STYLE,
         GetWindowLongW(hwnd, GWL_STYLE) & ~WS_MINIMIZEBOX);
         SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
      }

      [[maybe_unused]]
      void disable_maximize(HWND hwnd)
      {
         SetWindowLongW(hwnd, GWL_STYLE,
         GetWindowLongW(hwnd, GWL_STYLE) & ~WS_MAXIMIZEBOX);
         SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
      }

      void disable_resize(HWND hwnd)
      {
         SetWindowLongW(hwnd, GWL_STYLE,
         GetWindowLongW(hwnd, GWL_STYLE) & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX);
         SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
      }

      LRESULT on_close(window* win)
      {
         if (win && win->on_close)
            win->on_close();
         return 0;
      }

      BOOL CALLBACK for_each_child(HWND child, LPARAM lParam)
      {
         LPRECT bounds = (LPRECT) lParam;
         MoveWindow(
            child,
            0, 0,
            bounds->right,
            bounds->bottom,
            TRUE);

         // Make sure the child window is visible.
         ShowWindow(child, SW_SHOW);
         return true;
      }

      LRESULT on_size(HWND hwnd)
      {
         RECT bounds;
         GetClientRect(hwnd, &bounds);
         EnumChildWindows(hwnd, for_each_child, (LPARAM) &bounds);
         return 0;
      }

      POINT window_frame_size(HWND hwnd)
      {
         RECT content, frame;
         POINT extra;
         GetClientRect(hwnd, &content);
         GetWindowRect(hwnd, &frame);
         extra.x = (frame.right - frame.left) - content.right;
         extra.y = (frame.bottom - frame.top) - content.bottom;
         return extra;
      }

      void constrain_size(HWND hwnd, RECT& r, view_limits limits, int edge = WMSZ_BOTTOMRIGHT)
      {
         // WM_SIZING's wparam carries WMSZ_* codes (1..8); hit-testing and
         // WM_NCLBUTTONDOWN carry HT* codes (10..17). Normalize so this
         // function works for both callers.
         switch (edge)
         {
            case HTLEFT:        edge = WMSZ_LEFT;        break;
            case HTRIGHT:       edge = WMSZ_RIGHT;       break;
            case HTTOP:         edge = WMSZ_TOP;         break;
            case HTTOPLEFT:     edge = WMSZ_TOPLEFT;     break;
            case HTTOPRIGHT:    edge = WMSZ_TOPRIGHT;    break;
            case HTBOTTOM:      edge = WMSZ_BOTTOM;      break;
            case HTBOTTOMLEFT:  edge = WMSZ_BOTTOMLEFT;  break;
            case HTBOTTOMRIGHT: edge = WMSZ_BOTTOMRIGHT; break;
            default: break;
         }

         auto scale = get_scale_for_window(hwnd);
         auto extra = window_frame_size(hwnd);

         double minx = limits.min.x * scale + extra.x;
         double maxx = limits.max.x * scale + extra.x;
         double miny = limits.min.y * scale + extra.y;
         double maxy = limits.max.y * scale + extra.y;

         // Which edges are being dragged? WM_SIZING's wparam is the hit-test
         // code of the dragged edge/corner. We must clamp the MOVING edge
         // against the fixed one, otherwise shrinking a left/top edge below
         // the minimum would drag the opposite edge along and make the
         // window appear to move instead of stopping at the minimum size.
         bool left_moving   = edge == WMSZ_LEFT   || edge == WMSZ_TOPLEFT   || edge == WMSZ_BOTTOMLEFT;
         bool right_moving  = edge == WMSZ_RIGHT  || edge == WMSZ_TOPRIGHT  || edge == WMSZ_BOTTOMRIGHT;
         bool top_moving    = edge == WMSZ_TOP    || edge == WMSZ_TOPLEFT   || edge == WMSZ_TOPRIGHT;
         bool bottom_moving = edge == WMSZ_BOTTOM || edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT;

         // Fall back to the right/bottom edges (e.g. programmatic resizes).
         if (!left_moving && !right_moving)
            right_moving = true;
         if (!top_moving && !bottom_moving)
            bottom_moving = true;

         double w = double(r.right - r.left);
         double h = double(r.bottom - r.top);

         if (left_moving && !right_moving)
         {
            if (w < minx)
               r.left = LONG(r.right - minx);
            else if (w > maxx && maxx < 1E9)
               r.left = LONG(r.right - maxx);
         }
         else if (right_moving && !left_moving)
         {
            if (w < minx)
               r.right = LONG(r.left + minx);
            else if (w > maxx && maxx < 1E9)
               r.right = LONG(r.left + maxx);
         }

         if (top_moving && !bottom_moving)
         {
            if (h < miny)
               r.top = LONG(r.bottom - miny);
            else if (h > maxy && maxy < 1E9)
               r.top = LONG(r.bottom - maxy);
         }
         else if (bottom_moving && !top_moving)
         {
            if (h < miny)
               r.bottom = LONG(r.top + miny);
            else if (h > maxy && maxy < 1E9)
               r.bottom = LONG(r.top + maxy);
         }
      }

      LRESULT CALLBACK handle_event(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
      {
         auto* info = get_window_info(hwnd);
         switch (message)
         {
            case WM_NCLBUTTONDOWN:
               {
                  // Borderless windows cannot rely on DefWindowProc's modal
                  // size loop: without WS_THICKFRAME the loop does not send
                  // WM_SIZING, so shrinking a left/top edge past the minimum
                  // would invert the rect and turn the resize into a window
                  // move. Instead we run our own loop with explicit
                  // minimum/maximum clamping.
                  auto edge = int(wparam);
                  bool is_edge = edge >= HTLEFT && edge <= HTBOTTOMRIGHT;
                  if (info && GetPropW(hwnd, L"ElementsFramelessResizable") && is_edge)
                  {
                     POINT pt{int(short(LOWORD(lparam))), int(short(HIWORD(lparam)))};
                     GetWindowRect(hwnd, &info->sizing_start);
                     info->sizing_grab = pt;
                     info->sizing_edge = edge;
                     SetCapture(hwnd);
                     return 0;
                  }
                  // Not a frameless edge drag: let the system handle it
                  // (e.g. HTCAPTION moves a standard window's title bar).
                  return DefWindowProcW(hwnd, message, wparam, lparam);
               }

            case WM_MOUSEMOVE:
               if (info && info->sizing_edge)
               {
                  POINT pt;
                  GetCursorPos(&pt);

                  auto edge = info->sizing_edge;
                  auto const& start = info->sizing_start;
                  auto const& grab = info->sizing_grab;
                  RECT nr = start;

                  bool left_moving   = edge == HTLEFT   || edge == HTTOPLEFT   || edge == HTBOTTOMLEFT;
                  bool right_moving  = edge == HTRIGHT  || edge == HTTOPRIGHT  || edge == HTBOTTOMRIGHT;
                  bool top_moving    = edge == HTTOP    || edge == HTTOPLEFT   || edge == HTTOPRIGHT;
                  bool bottom_moving = edge == HTBOTTOM || edge == HTBOTTOMLEFT || edge == HTBOTTOMRIGHT;

                  if (left_moving)
                     nr.left = start.left + (pt.x - grab.x);
                  if (right_moving)
                     nr.right = start.right + (pt.x - grab.x);
                  if (top_moving)
                     nr.top = start.top + (pt.y - grab.y);
                  if (bottom_moving)
                     nr.bottom = start.bottom + (pt.y - grab.y);

                  constrain_size(hwnd, nr, info->limits, edge);

                  SetWindowPos(
                     hwnd, nullptr,
                     nr.left, nr.top,
                     nr.right - nr.left, nr.bottom - nr.top,
                     SWP_NOZORDER | SWP_NOACTIVATE
                  );
                  return 0;
               }
               return DefWindowProcW(hwnd, message, wparam, lparam);

            case WM_NCLBUTTONUP:
            case WM_LBUTTONUP:
               if (info && info->sizing_edge)
               {
                  info->sizing_edge = 0;
                  ReleaseCapture();
                  return 0;
               }
               return DefWindowProcW(hwnd, message, wparam, lparam);

            case WM_CAPTURECHANGED:
               if (info)
                  info->sizing_edge = 0;
               return DefWindowProcW(hwnd, message, wparam, lparam);

            case WM_CLOSE:
               ShowWindow(hwnd, SW_HIDE);
               return on_close(info->wptr);

            case WM_DPICHANGED:
            case WM_SIZE:
               return on_size(hwnd);

            case WM_NCHITTEST:
               if (GetPropW(hwnd, L"ElementsFramelessResizable"))
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

                  if (top && left)       return HTTOPLEFT;
                  if (top && right)      return HTTOPRIGHT;
                  if (bottom && left)    return HTBOTTOMLEFT;
                  if (bottom && right)   return HTBOTTOMRIGHT;
                  if (left)              return HTLEFT;
                  if (right)             return HTRIGHT;
                  if (top)               return HTTOP;
                  if (bottom)            return HTBOTTOM;

                  return HTCLIENT;
               }
               return DefWindowProcW(hwnd, message, wparam, lparam);

            case WM_SIZING:
               if (info)
               {
                  auto& r = *reinterpret_cast<RECT*>(lparam);
                  constrain_size(hwnd, r, info->limits, int(wparam));
               }
               break;

            default:
               return DefWindowProcW(hwnd, message, wparam, lparam);
         }
         return 0;
      }

      struct init_window_class
      {
         init_window_class()
         {
            WNDCLASSW windowClass = {};
            windowClass.hbrBackground = nullptr;
            windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.lpfnWndProc = handle_event;
            windowClass.lpszClassName = L"ElementsWindow";
            windowClass.style = CS_HREDRAW | CS_VREDRAW;
            if (!RegisterClassW(&windowClass))
               MessageBoxW(nullptr, L"Could not register class", L"Error", MB_OK);
         }
      };
   }

   window::window(std::string const& name, int style_, rect const& bounds)
   {
      static init_window_class init;

      std::wstring wname = utf8_decode(name);
      #ifdef ELEMENTS_HOST_ONLY_WIN7
      auto scale = 1.0f;
      #else
      auto scale = GetDpiForSystem() / 96.0f;
      #endif

      // Frameless windows (no `with_title` style, e.g. window::bare)
      // are created as borderless popup windows. Resize is handled via
      // WM_NCHITTEST (see handle_event) instead of a system frame.
      auto win_style = (style_ & with_title)? WS_OVERLAPPEDWINDOW : WS_POPUP;

      _window = CreateWindowW(
         L"ElementsWindow",
         wname.c_str(),
         win_style,
         bounds.left * scale, bounds.top * scale,
         bounds.width() * scale, bounds.height() * scale,
         nullptr, nullptr, nullptr,
         nullptr
      );

      auto* info = new window_info{this};
      SetWindowLongPtrW(_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(info));

      // Marker used by the child view (WM_NCHITTEST) to pass edge
      // hit-testing through to this top-level window, enabling native
      // borderless resize.
      if (!(style_ & with_title) && (style_ & resizable))
         SetPropW(_window, L"ElementsFramelessResizable", (HANDLE)1);

      if (style_ & with_title)
      {
         if (!(style_ & closable))
            disable_close(_window);
         if (!(style_ & miniaturizable))
            disable_minimize(_window);
         if (!(style_ & resizable))
            disable_resize(_window);
      }

      // Sets the app icon for the window to show on the titlebar.
      // The IDI_ELEMENTS_APP_ICON icon id should be defined in a resource file.
      HICON hIcon = LoadIcon(GetModuleHandle(nullptr), TEXT("IDI_ELEMENTS_APP_ICON"));
      if (hIcon)
         ::SendMessage(_window, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

      ShowWindow(_window, SW_RESTORE);
   }

   window::~window()
   {
      delete get_window_info(_window);
      DeleteObject(_window);
   }

   point window::size() const
   {
      auto scale = get_scale_for_window(_window);
      RECT frame;
      GetWindowRect(_window, &frame);
      return {
         float((frame.right - frame.left) / scale),
         float((frame.bottom - frame.top) / scale)
      };
   }

   void window::size(point const& p)
   {
      auto scale = get_scale_for_window(_window);
      RECT frame;
      GetWindowRect(_window, &frame);
      frame.right = frame.left + (p.x * scale);
      frame.bottom = frame.top + (p.y * scale);
      constrain_size(
         _window, frame, get_window_info(_window)->limits);

      MoveWindow(
         _window, frame.left, frame.top,
         frame.right - frame.left,
         frame.bottom - frame.top,
         true // repaint
      );
   }

   void window::limits(view_limits limits_)
   {
      get_window_info(_window)->limits = limits_;
      RECT frame;
      GetWindowRect(_window, &frame);
      constrain_size(
         _window, frame, get_window_info(_window)->limits);

      MoveWindow(
         _window, frame.left, frame.top,
         frame.right - frame.left,
         frame.bottom - frame.top,
         true // repaint
      );
   }

   point window::position() const
   {
      auto scale = get_scale_for_window(_window);
      RECT frame;
      GetWindowRect(_window, &frame);
      return {float(frame.left / scale), float(frame.top / scale)};
   }

   void window::position(point const& p)
   {
      auto scale = get_scale_for_window(_window);
      RECT frame;
      GetWindowRect(_window, &frame);

      MoveWindow(
         _window, p.x * scale, p.y * scale,
         frame.right - frame.left,
         frame.bottom - frame.top,
         true // repaint
      );
   }

   void window::close()
   {
      ::SendMessage(_window, WM_CLOSE, 0, 0);
   }

   void window::minimize()
   {
      ::ShowWindow(_window, SW_MINIMIZE);
   }

   void window::maximize()
   {
      // Toggle between maximized and restored.
      ::ShowWindow(_window, IsZoomed(_window)? SW_RESTORE : SW_MAXIMIZE);
   }
}

