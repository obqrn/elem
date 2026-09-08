/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace
{
   struct image
   {
      int width = 0;
      int height = 0;
      std::vector<std::uint32_t> pixels;

      bool valid() const
      {
         return width > 0 && height > 0 &&
            pixels.size() == std::size_t(width) * std::size_t(height);
      }
   };

   struct image_diff
   {
      std::size_t changed = 0;
      RECT bounds = {};
   };

   void wait_ms(unsigned ms)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
   }

   std::wstring full_path(std::wstring path)
   {
      std::vector<wchar_t> buffer(32768);
      auto length = GetFullPathNameW(
         path.c_str(), DWORD(buffer.size()), buffer.data(), nullptr);
      if (length == 0 || length >= buffer.size())
         return path;
      return {buffer.data(), length};
   }

   HWND find_main_window(DWORD pid)
   {
      struct state
      {
         DWORD pid;
         HWND result = nullptr;
      } data{pid};

      EnumWindows([](HWND hwnd, LPARAM param) -> BOOL
      {
         auto& data = *reinterpret_cast<state*>(param);
         DWORD window_pid = 0;
         GetWindowThreadProcessId(hwnd, &window_pid);
         if (window_pid != data.pid)
            return TRUE;

         wchar_t title[128] = {};
         GetWindowTextW(hwnd, title, int(std::size(title)));
         if (std::wstring(title) == L"Text Editor")
         {
            data.result = hwnd;
            return FALSE;
         }
         return TRUE;
      }, LPARAM(&data));
      return data.result;
   }

   HWND find_view(HWND main)
   {
      HWND result = nullptr;
      EnumChildWindows(main, [](HWND hwnd, LPARAM param) -> BOOL
      {
         auto result = reinterpret_cast<HWND*>(param);
         wchar_t class_name[128] = {};
         GetClassNameW(hwnd, class_name, int(std::size(class_name)));
         if (std::wstring(class_name) == L"ElementsView")
         {
            *result = hwnd;
            return FALSE;
         }
         return TRUE;
      }, LPARAM(&result));
      return result;
   }

   bool wait_for_windows(DWORD pid, HWND& main, HWND& view)
   {
      auto deadline = GetTickCount64() + 10000;
      while (GetTickCount64() < deadline)
      {
         main = find_main_window(pid);
         view = main? find_view(main) : nullptr;
         if (main && view && IsWindowVisible(main) && IsWindowVisible(view))
            return true;
         wait_ms(50);
      }
      return false;
   }

   bool activate(HWND main, HWND view, DWORD pid)
   {
      // Windows can keep the previous test's window in the foreground for a
      // short period after it exits. Retry the activation so a repeated CTest
      // run does not turn that host timing into a product failure.
      for (int attempt = 0; attempt != 20; ++attempt)
      {
         auto foreground = GetForegroundWindow();
         auto foreground_thread = foreground?
            GetWindowThreadProcessId(foreground, nullptr) : 0;
         auto target_thread = GetWindowThreadProcessId(main, nullptr);
         bool attached = foreground_thread != 0 && target_thread != 0 &&
            foreground_thread != target_thread &&
            AttachThreadInput(foreground_thread, target_thread, TRUE);

         AllowSetForegroundWindow(pid);
         ShowWindow(main, SW_SHOWNORMAL);
         SetWindowPos(main, HWND_TOP, 41, 75, 0, 0,
            SWP_NOSIZE | SWP_SHOWWINDOW);
         BringWindowToTop(main);
         SetForegroundWindow(main);
         SetActiveWindow(main);
         SetFocus(view);

         if (attached)
            AttachThreadInput(foreground_thread, target_thread, FALSE);

         if (GetForegroundWindow() == main)
            return true;
         wait_ms(25);
      }
      return false;
   }

   bool has_input_focus(HWND main, HWND view)
   {
      if (GetForegroundWindow() != main)
         return false;

      auto thread = GetWindowThreadProcessId(main, nullptr);
      GUITHREADINFO info = {sizeof(GUITHREADINFO)};
      if (!thread || !GetGUIThreadInfo(thread, &info))
         return false;
      return info.hwndActive == main && info.hwndFocus == view;
   }

   void print_focus_state(HWND main, HWND view, char const* phase)
   {
      auto foreground = GetForegroundWindow();
      auto thread = GetWindowThreadProcessId(main, nullptr);
      GUITHREADINFO info = {sizeof(GUITHREADINFO)};
      auto gui_info = thread && GetGUIThreadInfo(thread, &info);
      std::printf("FOCUS %s foreground=%d active=%d focus=%d target=%d/%d\n",
         phase, foreground == main, gui_info && info.hwndActive == main,
         gui_info && info.hwndFocus == view, main != nullptr, view != nullptr);
   }

   bool sync_window(HWND hwnd)
   {
      DWORD_PTR result = 0;
      return SendMessageTimeoutW(hwnd, WM_NULL, 0, 0,
         SMTO_ABORTIFHUNG, 5000, &result) != 0;
   }

   bool repaint_now(HWND view)
   {
      InvalidateRect(view, nullptr, FALSE);
      UpdateWindow(view);
      return sync_window(view);
   }

   image capture(HWND view)
   {
      RECT client = {};
      if (!GetClientRect(view, &client))
         return {};

      POINT origin = {0, 0};
      if (!ClientToScreen(view, &origin))
         return {};

      int width = client.right - client.left;
      int height = client.bottom - client.top;
      if (width <= 0 || height <= 0)
         return {};

      auto screen = GetDC(nullptr);
      auto memory = screen? CreateCompatibleDC(screen) : nullptr;
      auto bitmap = (screen && memory)?
         CreateCompatibleBitmap(screen, width, height) : nullptr;
      if (!screen || !memory || !bitmap)
      {
         if (bitmap) DeleteObject(bitmap);
         if (memory) DeleteDC(memory);
         if (screen) ReleaseDC(nullptr, screen);
         return {};
      }

      auto previous = SelectObject(memory, bitmap);
      auto copied = BitBlt(memory, 0, 0, width, height,
         screen, origin.x, origin.y, SRCCOPY) != FALSE;

      BITMAPINFO info = {};
      info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      info.bmiHeader.biWidth = width;
      info.bmiHeader.biHeight = -height;
      info.bmiHeader.biPlanes = 1;
      info.bmiHeader.biBitCount = 32;
      info.bmiHeader.biCompression = BI_RGB;
      std::vector<std::uint8_t> raw(std::size_t(width) * height * 4);
      copied = copied && GetDIBits(memory, bitmap, 0, height, raw.data(),
         &info, DIB_RGB_COLORS) != 0;

      image result;
      if (copied)
      {
         result.width = width;
         result.height = height;
         result.pixels.resize(std::size_t(width) * height);
         for (std::size_t i = 0; i != result.pixels.size(); ++i)
         {
            auto p = raw.data() + i * 4;
            result.pixels[i] = (std::uint32_t(p[2]) << 16) |
               (std::uint32_t(p[1]) << 8) | p[0];
         }
      }

      SelectObject(memory, previous);
      DeleteObject(bitmap);
      DeleteDC(memory);
      ReleaseDC(nullptr, screen);
      return result;
   }

   image_diff compare(image const& a, image const& b)
   {
      image_diff result;
      if (!a.valid() || !b.valid() ||
          a.width != b.width || a.height != b.height)
      {
         result.changed = 1;
         return result;
      }

      int left = a.width;
      int top = a.height;
      int right = -1;
      int bottom = -1;
      for (int y = 0; y != a.height; ++y)
      {
         for (int x = 0; x != a.width; ++x)
         {
            auto p = a.pixels[std::size_t(y) * a.width + x];
            auto q = b.pixels[std::size_t(y) * a.width + x];
            auto different = [](std::uint32_t first,
               std::uint32_t second)
            {
               auto r1 = int((first >> 16) & 0xff);
               auto g1 = int((first >> 8) & 0xff);
               auto b1 = int(first & 0xff);
               auto r2 = int((second >> 16) & 0xff);
               auto g2 = int((second >> 8) & 0xff);
               auto b2 = int(second & 0xff);
               return std::abs(r1 - r2) > 2 ||
                  std::abs(g1 - g2) > 2 || std::abs(b1 - b2) > 2;
            };
            if (!different(p, q))
               continue;

            ++result.changed;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
         }
      }
      if (right >= 0)
         result.bounds = {left, top, right + 1, bottom + 1};
      return result;
   }

   bool write_bmp(image const& source, char const* path)
   {
      if (!source.valid())
         return false;

      BITMAPFILEHEADER file_header = {};
      BITMAPINFOHEADER info_header = {};
      info_header.biSize = sizeof(info_header);
      info_header.biWidth = source.width;
      info_header.biHeight = -source.height;
      info_header.biPlanes = 1;
      info_header.biBitCount = 32;
      info_header.biCompression = BI_RGB;
      file_header.bfType = 0x4D42;
      file_header.bfOffBits = sizeof(file_header) + sizeof(info_header);
      file_header.bfSize = file_header.bfOffBits +
         DWORD(source.pixels.size() * sizeof(std::uint32_t));

      auto file = std::fopen(path, "wb");
      if (!file)
         return false;

      bool ok = std::fwrite(&file_header, sizeof(file_header), 1, file) == 1 &&
         std::fwrite(&info_header, sizeof(info_header), 1, file) == 1;
      for (auto pixel : source.pixels)
      {
         std::uint8_t bgra[4] = {
            std::uint8_t(pixel & 0xff),
            std::uint8_t((pixel >> 8) & 0xff),
            std::uint8_t((pixel >> 16) & 0xff), 0};
         ok = ok && std::fwrite(bgra, sizeof(bgra), 1, file) == 1;
      }
      auto close_ok = std::fclose(file) == 0;
      return ok && close_ok;
   }

   void save_failure_frames(image const& immediate, image const& reference)
   {
      static unsigned sequence = 0;
      auto id = sequence++;
      char immediate_path[96] = {};
      char reference_path[96] = {};
      std::snprintf(immediate_path, sizeof(immediate_path),
         "rich_editor_failure_%u_immediate.bmp", id);
      std::snprintf(reference_path, sizeof(reference_path),
         "rich_editor_failure_%u_reference.bmp", id);
      auto immediate_saved = write_bmp(immediate, immediate_path);
      auto reference_saved = write_bmp(reference, reference_path);
      std::printf("DIAGNOSTIC frames immediate=%s(%d) reference=%s(%d)\n",
         immediate_path, immediate_saved, reference_path, reference_saved);
   }

   bool send_click(HWND view, int x, int y, double scale)
   {
      POINT point = {
         LONG(std::lround(x * scale)), LONG(std::lround(y * scale))};
      if (!ClientToScreen(view, &point) || !SetCursorPos(point.x, point.y))
         return false;

      INPUT input = {};
      input.type = INPUT_MOUSE;
      input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
      if (SendInput(1, &input, sizeof(input)) != 1)
         return false;
      if (!sync_window(view))
         return false;
      UpdateWindow(view);

      wait_ms(20);
      input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
      if (SendInput(1, &input, sizeof(input)) != 1)
         return false;
      if (!sync_window(view))
         return false;
      UpdateWindow(view);
      return true;
   }

   bool send_drag(HWND view, int x1, int y1, int x2, int y2, double scale)
   {
      POINT start = {
         LONG(std::lround(x1 * scale)), LONG(std::lround(y1 * scale))};
      POINT end = {
         LONG(std::lround(x2 * scale)), LONG(std::lround(y2 * scale))};
      if (!ClientToScreen(view, &start) || !ClientToScreen(view, &end) ||
          !SetCursorPos(start.x, start.y))
         return false;

      wait_ms(30);
      INPUT input = {};
      input.type = INPUT_MOUSE;
      input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
      if (SendInput(1, &input, sizeof(input)) != 1 ||
          !sync_window(view))
         return false;
      UpdateWindow(view);

      auto middle = POINT{
         (start.x + end.x) / 2, (start.y + end.y) / 2};
      for (auto point : {middle, end})
      {
         wait_ms(30);
         if (!SetCursorPos(point.x, point.y) || !sync_window(view))
            return false;
         UpdateWindow(view);
      }

      input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
      if (SendInput(1, &input, sizeof(input)) != 1 ||
          !sync_window(view))
         return false;
      UpdateWindow(view);
      return true;
   }

   bool send_key(HWND view, WORD key)
   {
      INPUT input = {};
      input.type = INPUT_KEYBOARD;
      input.ki.wScan = WORD(MapVirtualKeyW(key, MAPVK_VK_TO_VSC));
      auto extended = key == VK_HOME || key == VK_END ||
         key == VK_LEFT || key == VK_RIGHT ||
         key == VK_UP || key == VK_DOWN;
      input.ki.dwFlags = KEYEVENTF_SCANCODE |
         (extended? KEYEVENTF_EXTENDEDKEY : 0);
      if (SendInput(1, &input, sizeof(input)) != 1 ||
          !sync_window(view))
         return false;
      UpdateWindow(view);

      wait_ms(20);
      input.ki.dwFlags = (extended? KEYEVENTF_EXTENDEDKEY : 0) |
         KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
      if (SendInput(1, &input, sizeof(input)) != 1 ||
          !sync_window(view))
         return false;
      UpdateWindow(view);
      return true;
   }

   bool send_unicode_text(HWND view, std::wstring const& text)
   {
      // Send the exact UTF-16 WM_CHAR units. KEYEVENTF_UNICODE is a keyboard
      // injection API and converts isolated surrogate units to '?' on this
      // Windows environment, so it cannot test the editor's pair handling.
      for (auto unit : text)
      {
         DWORD_PTR result = 0;
         if (!SendMessageTimeoutW(view, WM_CHAR, WPARAM(unit), 0,
                SMTO_ABORTIFHUNG, 2000, &result) || result == 0)
            return false;
      }
      if (!sync_window(view))
         return false;
      UpdateWindow(view);
      return true;
   }

   bool send_unichar(HWND view, std::uint32_t codepoint)
   {
      DWORD_PTR result = 0;
      if (!SendMessageTimeoutW(view, WM_UNICHAR, WPARAM(codepoint), 0,
             SMTO_ABORTIFHUNG, 2000, &result) || result == 0)
         return false;
      return sync_window(view);
   }

   bool send_control_key(HWND view, WORD key)
   {
      INPUT input = {};
      input.type = INPUT_KEYBOARD;
      input.ki.wScan = WORD(MapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC));
      input.ki.dwFlags = KEYEVENTF_SCANCODE;
      if (SendInput(1, &input, sizeof(input)) != 1 ||
          !sync_window(view))
         return false;
      UpdateWindow(view);

      wait_ms(10);
      input.ki.wScan = WORD(MapVirtualKeyW(key, MAPVK_VK_TO_VSC));
      input.ki.dwFlags = KEYEVENTF_SCANCODE;
      if (SendInput(1, &input, sizeof(input)) != 1 ||
          !sync_window(view))
         return false;
      UpdateWindow(view);

      wait_ms(20);
      input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
      if (SendInput(1, &input, sizeof(input)) != 1 ||
          !sync_window(view))
         return false;
      UpdateWindow(view);

      wait_ms(10);
      input.ki.wScan = WORD(MapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC));
      input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
      if (SendInput(1, &input, sizeof(input)) != 1 ||
          !sync_window(view))
         return false;
      UpdateWindow(view);
      return true;
   }

   bool set_clipboard_text(std::wstring const& text)
   {
      if (!OpenClipboard(nullptr) || !EmptyClipboard())
      {
         CloseClipboard();
         return false;
      }

      auto length = text.size() + 1;
      auto memory = GlobalAlloc(GMEM_MOVEABLE,
         SIZE_T(length) * sizeof(wchar_t));
      if (!memory)
      {
         CloseClipboard();
         return false;
      }
      auto target = static_cast<wchar_t*>(GlobalLock(memory));
      if (!target)
      {
         GlobalFree(memory);
         CloseClipboard();
         return false;
      }
      CopyMemory(target, text.c_str(), SIZE_T(length) * sizeof(wchar_t));
      GlobalUnlock(memory);
      if (!SetClipboardData(CF_UNICODETEXT, memory))
      {
         GlobalFree(memory);
         CloseClipboard();
         return false;
      }
      CloseClipboard();
      return true;
   }

   bool set_clipboard_text(wchar_t const* text)
   {
      return set_clipboard_text(std::wstring{text});
   }

   std::wstring clipboard_text()
   {
      if (!OpenClipboard(nullptr))
         return {};

      auto object = GetClipboardData(CF_UNICODETEXT);
      auto value = object? static_cast<wchar_t*>(GlobalLock(object)) : nullptr;
      std::wstring result = value? std::wstring{value} : std::wstring{};
      if (value)
         GlobalUnlock(object);
      CloseClipboard();
      return result;
   }

   bool clipboard_equals(wchar_t const* expected)
   {
      return clipboard_text() == expected;
   }

   int color_distance(std::uint32_t first, std::uint32_t second)
   {
      auto channel = [](std::uint32_t pixel, int shift)
      {
         return int((pixel >> shift) & 0xff);
      };
      return std::abs(channel(first, 16) - channel(second, 16)) +
         std::abs(channel(first, 8) - channel(second, 8)) +
         std::abs(channel(first, 0) - channel(second, 0));
   }

   bool has_text_raster(image const& frame, double scale)
   {
      if (!frame.valid())
         return false;

      auto background = frame.pixels[std::size_t(std::min(frame.height - 1, 5)) *
         frame.width + std::min(frame.width - 1, 5)];
      auto left = std::min(frame.width, std::max(0, int(std::lround(20 * scale))));
      auto top = std::min(frame.height, std::max(0, int(std::lround(20 * scale))));
      auto right = std::min(frame.width, std::max(left,
         int(std::lround(700 * scale))));
      auto bottom = std::min(frame.height, std::max(top,
         int(std::lround(220 * scale))));
      std::size_t ink = 0;
      int strongest_edge = 0;
      for (int y = top; y < bottom; ++y)
      {
         for (int x = left; x < right; ++x)
         {
            auto pixel = frame.pixels[std::size_t(y) * frame.width + x];
            if (color_distance(pixel, background) <= 18)
               continue;
            ++ink;
            if (x + 1 < right)
               strongest_edge = std::max(strongest_edge,
                  color_distance(pixel,
                     frame.pixels[std::size_t(y) * frame.width + x + 1]));
            if (y + 1 < bottom)
               strongest_edge = std::max(strongest_edge,
                  color_distance(pixel,
                     frame.pixels[std::size_t(y + 1) * frame.width + x]));
         }
      }
      return ink >= 20 && strongest_edge >= 30;
   }

   bool validate_update(HWND view, image const& previous,
      char const* label, image& current)
   {
      auto immediate = capture(view);
      if (!immediate.valid())
      {
         std::printf("FAIL %s: unable to capture immediate frame\n", label);
         return false;
      }
      auto state_diff = compare(immediate, previous);
      if (state_diff.changed == 0)
      {
         std::printf("FAIL %s: visible state did not change\n", label);
         save_failure_frames(immediate, previous);
         return false;
      }

      if (!repaint_now(view))
      {
         std::printf("FAIL %s: full repaint did not complete\n", label);
         return false;
      }
      auto reference = capture(view);
      auto stale_diff = compare(immediate, reference);
      if (!reference.valid() || stale_diff.changed > 8)
      {
         std::printf("FAIL %s: immediate frame differs from full repaint "
            "by %zu pixels in [%ld,%ld,%ld,%ld]\n", label,
            stale_diff.changed, stale_diff.bounds.left, stale_diff.bounds.top,
            stale_diff.bounds.right, stale_diff.bounds.bottom);
         save_failure_frames(immediate, reference);
         return false;
      }

      std::printf("PASS %s\n", label);
      current = std::move(reference);
      return true;
   }

   void stop_process(PROCESS_INFORMATION const& process, HWND main)
   {
      if (main)
         PostMessageW(main, WM_CLOSE, 0, 0);
      if (WaitForSingleObject(process.hProcess, 2000) == WAIT_TIMEOUT)
         TerminateProcess(process.hProcess, 1);
      WaitForSingleObject(process.hProcess, 5000);
      CloseHandle(process.hThread);
      CloseHandle(process.hProcess);
   }
}

int wmain(int argc, wchar_t* argv[])
{
   if (argc < 2 || argc > 3)
   {
      std::printf("usage: RichEditorClickRegression <RichEditor.exe> "
         "[--selection|--keyboard|--unicode|--paste|--dpi|--history]\n");
      return 2;
   }
   auto scenario = argc == 3? std::wstring(argv[2]) : std::wstring{};
   if (!scenario.empty() && scenario != L"--selection" &&
       scenario != L"--keyboard" && scenario != L"--unicode" &&
       scenario != L"--paste" && scenario != L"--dpi" &&
       scenario != L"--history")
   {
      std::printf("FAIL unknown regression scenario\n");
      return 2;
   }

   SetProcessDPIAware();
   auto executable = full_path(argv[1]);
   auto separator = executable.find_last_of(L"\\/");
   auto working_directory = separator == std::wstring::npos?
      std::wstring{} : executable.substr(0, separator);
   std::wstring command = L"\"" + executable + L"\"";
   std::vector<wchar_t> command_line(command.begin(), command.end());
   command_line.push_back(L'\0');

   STARTUPINFOW startup = {};
   startup.cb = sizeof(startup);
   startup.dwFlags = STARTF_USESHOWWINDOW;
   startup.wShowWindow = SW_SHOWNORMAL;
   PROCESS_INFORMATION process = {};
   if (!CreateProcessW(executable.c_str(), command_line.data(), nullptr,
      nullptr, FALSE, 0, nullptr,
      working_directory.empty()? nullptr : working_directory.c_str(),
      &startup, &process))
   {
      std::printf("FAIL unable to start RichEditor (error=%lu)\n",
         GetLastError());
      return 1;
   }

   HWND main = nullptr;
   HWND view = nullptr;
   auto fail = [&]() -> int
   {
      stop_process(process, main);
      return 1;
   };

   if (!wait_for_windows(process.dwProcessId, main, view))
   {
      std::printf("FAIL RichEditor window was not found\n");
      stop_process(process, main);
      return 1;
   }
   if (!activate(main, view, process.dwProcessId))
   {
      print_focus_state(main, view, "activation failed");
      std::printf("FAIL RichEditor could not become foreground\n");
      stop_process(process, main);
      return 1;
   }
   if (!has_input_focus(main, view))
   {
      print_focus_state(main, view, "after activation");
      std::printf("FAIL RichEditor did not receive keyboard focus\n");
      stop_process(process, main);
      return 1;
   }

   wait_ms(300);
   if (!repaint_now(view))
   {
      std::printf("FAIL initial repaint did not complete\n");
      stop_process(process, main);
      return 1;
   }
   auto previous = capture(view);
   if (!previous.valid())
   {
      std::printf("FAIL unable to capture RichEditor\n");
      stop_process(process, main);
      return 1;
   }

   if (!repaint_now(view))
   {
      std::printf("FAIL second initial repaint did not complete\n");
      stop_process(process, main);
      return 1;
   }
   auto initial_reference = capture(view);
   auto initial_diff = compare(previous, initial_reference);
   if (!initial_reference.valid() || initial_diff.changed > 8)
   {
      std::printf("FAIL initial repaint is unstable: %zu pixels in "
         "[%ld,%ld,%ld,%ld]\n", initial_diff.changed,
         initial_diff.bounds.left, initial_diff.bounds.top,
         initial_diff.bounds.right, initial_diff.bounds.bottom);
      save_failure_frames(previous, initial_reference);
      print_focus_state(main, view, "initial repaint");
      stop_process(process, main);
      return 1;
   }
   previous = std::move(initial_reference);

   auto focus_or_fail = [&](char const* phase)
   {
      if (has_input_focus(main, view))
         return true;
      if (activate(main, view, process.dwProcessId))
      {
         wait_ms(10);
         if (has_input_focus(main, view))
            return true;
      }
      print_focus_state(main, view, phase);
      std::printf("FAIL input focus lost before %s\n", phase);
      return false;
   };

   auto dpi = GetDpiForWindow(view);
   auto scale = dpi == 0? 1.0 : double(dpi) / 96.0;
   if (scenario.empty())
   {
      struct click_point { int x; int y; };
      auto points = std::vector<click_point>{
         {130, 75}, {200, 75}, {130, 150}, {200, 150},
         {300, 125}, {560, 125}, {130, 75}
      };

      for (auto point : points)
      {
         if (!focus_or_fail("click"))
            return fail();
         if (!send_click(view, point.x, point.y, scale))
         {
            std::printf("FAIL SendInput click at %d,%d\n", point.x, point.y);
            return fail();
         }

         char label[80] = {};
         std::snprintf(label, sizeof(label),
            "click %d,%d immediate caret state is current",
            point.x, point.y);
         image current;
         if (!validate_update(view, previous, label, current))
            return fail();
         previous = std::move(current);
         wait_ms(1000);
      }
   }
   else if (scenario == L"--selection")
   {
      if (!focus_or_fail("selection anchor click"))
         return fail();
      if (!send_click(view, 130, 75, scale))
      {
         std::printf("FAIL unable to place the selection anchor\n");
         return fail();
      }
      image current;
      if (!validate_update(view, previous, "selection anchor click", current))
         return fail();
      previous = std::move(current);

      if (!focus_or_fail("selection drag"))
         return fail();
      if (!send_drag(view, 130, 75, 300, 150, scale))
      {
         std::printf("FAIL unable to send drag selection\n");
         return fail();
      }
      if (!validate_update(view, previous, "drag selection is current", current))
         return fail();
      previous = std::move(current);

      if (!focus_or_fail("selection clearing click"))
         return fail();
      if (!send_click(view, 130, 75, scale))
      {
         std::printf("FAIL unable to click-clear the selection\n");
         return fail();
      }
      if (!validate_update(view, previous,
         "click clears selection without residual highlight", current))
         return fail();
   }
   else if (scenario == L"--keyboard")
   {
      if (!focus_or_fail("keyboard anchor click"))
         return fail();
      if (!send_click(view, 300, 150, scale))
      {
         std::printf("FAIL unable to place the keyboard caret\n");
         return fail();
      }
      image current;
      if (!validate_update(view, previous, "keyboard anchor click", current))
         return fail();
      previous = std::move(current);

      struct key_case { WORD key; char const* label; };
      auto keys = std::vector<key_case>{
         {VK_HOME, "Home moves caret immediately"},
         {VK_RIGHT, "Right moves caret immediately"},
         {VK_END, "End moves caret immediately"},
         {VK_LEFT, "Left moves caret immediately"}
      };
      for (auto key : keys)
      {
         if (!focus_or_fail(key.label))
            return fail();
         if (!send_key(view, key.key))
         {
            std::printf("FAIL unable to send keyboard input for %s\n",
               key.label);
            return fail();
         }
         if (!validate_update(view, previous, key.label, current))
            return fail();
         previous = std::move(current);
      }
   }
   else if (scenario == L"--unicode")
   {
      image current;
      if (!focus_or_fail("Unicode select all") ||
          !send_click(view, 300, 150, scale) ||
          !send_control_key(view, WORD('A')))
      {
         std::printf("FAIL unable to select all for Unicode input\n");
         return fail();
      }
      auto selected = capture(view);
      auto selection_diff = compare(selected, previous);
      if (!selected.valid() || selection_diff.changed == 0)
      {
         std::printf("FAIL Ctrl+A did not produce a visible selection\n");
         return fail();
      }

      auto expected = std::wstring{L"\u4E2D\U0001F642"};
      if (!send_unicode_text(view, expected))
      {
         std::printf("FAIL unable to send WM_CHAR Unicode input\n");
         return fail();
      }
      if (!validate_update(view, previous,
         "WM_CHAR commits Chinese and Emoji", current))
         return fail();
      previous = std::move(current);

      expected += L"\u754C";
      if (!send_unichar(view, 0x754C))
      {
         std::printf("FAIL unable to send WM_UNICHAR input\n");
         return fail();
      }
      if (!validate_update(view, previous,
         "WM_UNICHAR commits a complete codepoint", current))
         return fail();
      previous = std::move(current);

      if (!focus_or_fail("Unicode clipboard verification") ||
          !send_control_key(view, WORD('A')) ||
          !send_control_key(view, WORD('C')))
      {
         std::printf("FAIL unable to copy Unicode input for verification\n");
         return fail();
      }
      if (clipboard_text() != expected)
      {
         std::printf("FAIL Unicode clipboard content does not match\n");
         return fail();
      }
      std::printf("PASS WM_CHAR/WM_UNICHAR UTF-16 input\n");
   }
   else if (scenario == L"--paste")
   {
      image current;
      if (!focus_or_fail("large paste select all") ||
          !send_click(view, 300, 150, scale) ||
          !send_control_key(view, WORD('A')))
      {
         std::printf("FAIL unable to select all for large paste\n");
         return fail();
      }
      auto selected = capture(view);
      auto selection_diff = compare(selected, previous);
      if (!selected.valid() || selection_diff.changed == 0)
      {
         std::printf("FAIL Ctrl+A did not produce a visible selection\n");
         return fail();
      }

      std::wstring paste;
      paste.reserve(60 * 1024);
      for (int i = 0; i != 2048; ++i)
         paste += L"paste-performance-0123456789 ";
      if (!set_clipboard_text(paste) || !clipboard_equals(paste.c_str()))
      {
         std::printf("FAIL unable to establish large paste clipboard\n");
         return fail();
      }

      LARGE_INTEGER frequency = {};
      LARGE_INTEGER begin = {};
      LARGE_INTEGER end = {};
      QueryPerformanceFrequency(&frequency);
      QueryPerformanceCounter(&begin);
      if (!focus_or_fail("large paste") ||
          !send_control_key(view, WORD('V')))
      {
         std::printf("FAIL unable to send large paste\n");
         return fail();
      }
      QueryPerformanceCounter(&end);
      auto elapsed_ms = frequency.QuadPart == 0? 0.0 :
         1000.0 * double(end.QuadPart - begin.QuadPart) /
         double(frequency.QuadPart);
      if (elapsed_ms > 5000.0)
      {
         std::printf("FAIL large paste took %.1f ms\n", elapsed_ms);
         return fail();
      }
      if (!validate_update(view, previous, "large paste is current", current))
         return fail();
      previous = std::move(current);

      if (!set_clipboard_text(L"clipboard-copy-sentinel") ||
          !clipboard_equals(L"clipboard-copy-sentinel"))
      {
         std::printf("FAIL unable to establish clipboard copy sentinel\n");
         return fail();
      }
      if (!focus_or_fail("large paste clipboard verification") ||
          !send_control_key(view, WORD('A')) ||
          !send_control_key(view, WORD('C')))
      {
         std::printf("FAIL unable to copy large paste for verification\n");
         return fail();
      }
      auto copied = clipboard_text();
      if (copied != paste)
      {
         std::printf("FAIL large paste content changed (expected %zu, got %zu)\n",
            paste.size(), copied.size());
         return fail();
      }
      std::printf("PASS large paste %zu UTF-16 code units in %.1f ms\n",
         paste.size(), elapsed_ms);
   }
   else if (scenario == L"--dpi")
   {
      if (!has_text_raster(previous, scale))
      {
         std::printf("FAIL text raster is empty or has no measurable edge\n");
         return fail();
      }
      if (!repaint_now(view))
      {
         std::printf("FAIL DPI repaint did not complete\n");
         return fail();
      }
      auto current = capture(view);
      auto repaint_diff = compare(previous, current);
      if (!current.valid() || repaint_diff.changed > 8)
      {
         std::printf("FAIL DPI repaint changed %zu pixels\n",
            repaint_diff.changed);
         save_failure_frames(previous, current);
         return fail();
      }
      std::printf("PASS DPI %lu cached text raster is stable at %.2fx\n",
         GetDpiForWindow(view), scale);
   }
   else
   {
      if (!focus_or_fail("history anchor click"))
         return fail();
      if (!send_click(view, 300, 150, scale))
      {
         std::printf("FAIL unable to place the history caret\n");
         return fail();
      }
      image current;
      if (!validate_update(view, previous, "history anchor click", current))
         return fail();
      previous = std::move(current);
      auto anchor = previous;

      if (!set_clipboard_text(L"X"))
      {
         std::printf("FAIL unable to set history test clipboard\n");
         return fail();
      }
      if (!clipboard_equals(L"X"))
      {
         std::printf("FAIL history test clipboard readback is not X\n");
         return fail();
      }
      if (!focus_or_fail("history paste") ||
          !send_control_key(view, WORD('V')))
      {
         std::printf("FAIL unable to paste history test text\n");
         return fail();
      }
      if (!validate_update(view, previous, "paste is current", current))
         return fail();
      auto pasted = current;
      previous = std::move(current);

      if (!focus_or_fail("history undo"))
         return fail();
      if (!send_control_key(view, WORD('Z')))
      {
         std::printf("FAIL unable to send Ctrl+Z\n");
         return fail();
      }
      if (!validate_update(view, previous, "undo is current", current))
         return fail();
      if (compare(current, anchor).changed > 8)
      {
         std::printf("FAIL undo did not restore the anchor frame\n");
         return fail();
      }
      previous = std::move(current);

      if (!focus_or_fail("history redo"))
         return fail();
      if (!send_control_key(view, WORD('Y')))
      {
         std::printf("FAIL unable to send Ctrl+Y\n");
         return fail();
      }
      if (!validate_update(view, previous, "redo is current", current))
         return fail();
      if (compare(current, pasted).changed > 8)
      {
         std::printf("FAIL redo did not restore the pasted frame\n");
         return fail();
      }
   }

   std::printf("ALL TESTS PASSED\n");
   stop_process(process, main);
   return 0;
}
