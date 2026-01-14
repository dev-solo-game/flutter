// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_api.h"
#include <dwmapi.h>
#include <windowsx.h>
#include "dpi_utils.h"
#include "host_window.h"

namespace {

struct MonitorData {
  HMONITOR monitor = nullptr;
  RECT monitor_rect = {};
  RECT work_rect = {};
  UINT dpi = 96;
  double scale_factor = 1.0;
};

MonitorData GetMonitorUnderMouse() {
  MonitorData data;

  POINT cursor;
  if (!GetCursorPos(&cursor)) {
    return data;
  }

  data.monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);

  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);

  if (!GetMonitorInfo(data.monitor, &mi)) {
    return data;
  }

  data.monitor_rect = mi.rcMonitor;
  data.work_rect = mi.rcWork;

  data.dpi = flutter::GetDpiForMonitor(data.monitor);
  data.scale_factor = static_cast<double>(data.dpi) / 96.0;

  return data;
}

}  // namespace

namespace flutter {

WindowApi::WindowApi(HostWindow* window) : window_(window) {}

WindowApi::~WindowApi() = default;

void WindowApi::SetBounds(const WindowBoundsRequest* request) {
  if (!window_) {
    return;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return;
  }

  // Get DPI scale factor to convert logical pixels to physical pixels
  UINT const dpi = GetDpiForHWND(window_handle);
  double const scale_factor =
      static_cast<double>(dpi) / USER_DEFAULT_SCREEN_DPI;

  // Convert logical position to physical position
  int const physical_x = static_cast<int>(request->position.x * scale_factor);
  int const physical_y = static_cast<int>(request->position.y * scale_factor);

  // Convert logical size to physical size
  int const physical_width =
      static_cast<int>(request->size.preferred_view_width * scale_factor);
  int const physical_height =
      static_cast<int>(request->size.preferred_view_height * scale_factor);

  UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
  if (!request->position.has_pos) {
    flags |= SWP_NOMOVE;
  }
  if (!request->size.has_preferred_view_size) {
    flags |= SWP_NOSIZE;
  }

  // Account for drop shadow offset when setting position.
  // The user expects to set the visible frame position, not the window rect.
  RECT frame_rect;
  RECT window_rect;
  if (SUCCEEDED(DwmGetWindowAttribute(window_handle,
                                      DWMWA_EXTENDED_FRAME_BOUNDS, &frame_rect,
                                      sizeof(frame_rect))) &&
      GetWindowRect(window_handle, &window_rect)) {
    // Calculate shadow offset
    LONG const left_shadow = frame_rect.left - window_rect.left;
    LONG const top_shadow = frame_rect.top - window_rect.top;
    LONG const right_shadow = window_rect.right - frame_rect.right;
    LONG const bottom_shadow = window_rect.bottom - frame_rect.bottom;

    // Adjust position and size to account for shadow
    int const new_x = physical_x - static_cast<int>(left_shadow);
    int const new_y = physical_y - static_cast<int>(top_shadow);
    int const new_width =
        physical_width + static_cast<int>(left_shadow + right_shadow);
    int const new_height =
        physical_height + static_cast<int>(top_shadow + bottom_shadow);

    SetWindowPos(window_handle, nullptr, new_x, new_y, new_width, new_height,
                 flags);
  } else {
    // Fallback without shadow adjustment
    SetWindowPos(window_handle, nullptr, physical_x, physical_y, physical_width,
                 physical_height, flags);
  }
}

Point WindowApi::GetPosition() {
  if (!window_) {
    return Point(0, 0);
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return Point(0, 0);
  }

  // Get DPI scale factor to convert physical pixels to logical pixels
  UINT const dpi = GetDpiForHWND(window_handle);
  double const scale_factor =
      static_cast<double>(dpi) / USER_DEFAULT_SCREEN_DPI;

  // Use DWMWA_EXTENDED_FRAME_BOUNDS to get the visible frame position
  // (excluding drop shadow), which matches what the user expects.
  RECT frame_rect;
  if (SUCCEEDED(DwmGetWindowAttribute(window_handle,
                                      DWMWA_EXTENDED_FRAME_BOUNDS, &frame_rect,
                                      sizeof(frame_rect)))) {
    // Convert physical position to logical position
    return Point(static_cast<double>(frame_rect.left) / scale_factor,
                 static_cast<double>(frame_rect.top) / scale_factor);
  }

  // Fallback to window rect if DWM fails
  RECT window_rect;
  if (GetWindowRect(window_handle, &window_rect)) {
    return Point(static_cast<double>(window_rect.left) / scale_factor,
                 static_cast<double>(window_rect.top) / scale_factor);
  }

  return Point(0, 0);
}

Rect WindowApi::GetBounds() {
  if (!window_) {
    return Rect(Point(0, 0), Size(0, 0));
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return Rect(Point(0, 0), Size(0, 0));
  }

  // Get DPI scale factor to convert physical pixels to logical pixels
  UINT const dpi = GetDpiForHWND(window_handle);
  double const scale_factor =
      static_cast<double>(dpi) / USER_DEFAULT_SCREEN_DPI;

  // Use DWMWA_EXTENDED_FRAME_BOUNDS to get the visible frame bounds
  // (excluding drop shadow), which matches what the user expects.
  RECT frame_rect;
  if (SUCCEEDED(DwmGetWindowAttribute(window_handle,
                                      DWMWA_EXTENDED_FRAME_BOUNDS, &frame_rect,
                                      sizeof(frame_rect)))) {
    // Convert physical bounds to logical bounds
    return Rect(Point(static_cast<double>(frame_rect.left) / scale_factor,
                      static_cast<double>(frame_rect.top) / scale_factor),
                Size(static_cast<double>(frame_rect.right - frame_rect.left) /
                         scale_factor,
                     static_cast<double>(frame_rect.bottom - frame_rect.top) /
                         scale_factor));
  }

  // Fallback to window rect if DWM fails
  RECT window_rect;
  if (GetWindowRect(window_handle, &window_rect)) {
    return Rect(Point(static_cast<double>(window_rect.left) / scale_factor,
                      static_cast<double>(window_rect.top) / scale_factor),
                Size(static_cast<double>(window_rect.right - window_rect.left) /
                         scale_factor,
                     static_cast<double>(window_rect.bottom - window_rect.top) /
                         scale_factor));
  }

  return Rect(Point(0, 0), Size(0, 0));
}

void WindowApi::DragWindow(int state) {
  if (!window_) {
    return;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return;
  }

  switch (state) {
    case 0: {
      // Start dragging: record current cursor and window position
      GetCursorPos(&drag_start_cursor_pos_);
      RECT window_rect;
      if (GetWindowRect(window_handle, &window_rect)) {
        drag_start_window_pos_.x = window_rect.left;
        drag_start_window_pos_.y = window_rect.top;
      }
      is_dragging_ = true;
      break;
    }
    case 1: {
      // Update: move window based on cursor delta
      if (is_dragging_) {
        POINT current_cursor_pos;
        GetCursorPos(&current_cursor_pos);

        LONG delta_x = current_cursor_pos.x - drag_start_cursor_pos_.x;
        LONG delta_y = current_cursor_pos.y - drag_start_cursor_pos_.y;

        LONG new_x = drag_start_window_pos_.x + delta_x;
        LONG new_y = drag_start_window_pos_.y + delta_y;

        SetWindowPos(window_handle, nullptr, new_x, new_y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      }
      break;
    }
    case 2: {
      // End dragging
      is_dragging_ = false;
      break;
    }
    default:
      break;
  }
}

void WindowApi::SetNoFrame() {
  if (!window_) {
    return;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return;
  }

  WINDOWINFO window_info = {.cbSize = sizeof(WINDOWINFO)};
  GetWindowInfo(window_handle, &window_info);
  SetWindowLong(window_handle, GWL_STYLE,
                window_info.dwStyle & ~(WS_CAPTION | WS_THICKFRAME));

  SetWindowLong(
      window_handle, GWL_EXSTYLE,
      window_info.dwExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
}

void WindowApi::FullOnMonitors() {
  if (!window_) {
    return;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return;
  }

  double const virtual_screen_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  double const virtual_screen_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  SetWindowPos(window_handle, nullptr, 0, 0,
               static_cast<int>(virtual_screen_width),
               static_cast<int>(virtual_screen_height),
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool WindowApi::IsAlwaysOnTop() {
  if (!window_) {
    return false;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return false;
  }

  DWORD dwExStyle = GetWindowLong(window_handle, GWL_EXSTYLE);
  return (dwExStyle & WS_EX_TOPMOST) != 0;
}

void WindowApi::SetAlwaysOnTop(bool is_always_on_top) {
  if (!window_) {
    return;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return;
  }

  SetWindowPos(window_handle, is_always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
               0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

bool WindowApi::IsResizable() {
  return is_resizable_;
}

void WindowApi::SetResizable(bool is_resizable) {
  if (!window_) {
    return;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return;
  }

  is_resizable_ = is_resizable;

  DWORD gwlStyle = GetWindowLong(window_handle, GWL_STYLE);
  if (is_resizable) {
    gwlStyle |= WS_THICKFRAME;
  } else {
    gwlStyle &= ~WS_THICKFRAME;
  }
  ::SetWindowLong(window_handle, GWL_STYLE, gwlStyle);
}

void WindowApi::CenterWindowOnMonitor() {
  if (!window_) {
    return;
  }

  HWND hwnd = window_->GetWindowHandle();
  if (!hwnd) {
    return;
  }

  MonitorData monitor_data = GetMonitorUnderMouse();
  if (!monitor_data.monitor) {
    return;
  }

  RECT const& work = monitor_data.work_rect;

  RECT frame_rect;
  if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                   &frame_rect, sizeof(frame_rect)))) {
    GetWindowRect(hwnd, &frame_rect);
  }

  LONG frame_width = frame_rect.right - frame_rect.left;
  LONG frame_height = frame_rect.bottom - frame_rect.top;

  LONG target_frame_x = work.left + (work.right - work.left - frame_width) / 2;
  LONG target_frame_y = work.top + (work.bottom - work.top - frame_height) / 2;

  RECT window_rect;
  GetWindowRect(hwnd, &window_rect);

  LONG shadow_offset_x = frame_rect.left - window_rect.left;
  LONG shadow_offset_y = frame_rect.top - window_rect.top;

  LONG final_x = target_frame_x - shadow_offset_x;
  LONG final_y = target_frame_y - shadow_offset_y;

  SetWindowPos(hwnd, nullptr, final_x, final_y, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool WindowApi::IsWindowMinimized() {
  if (!window_) {
    return false;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return false;
  }

  WINDOWPLACEMENT windowPlacement;
  GetWindowPlacement(window_handle, &windowPlacement);

  return windowPlacement.showCmd == SW_SHOWMINIMIZED;
}

void WindowApi::Restore() {
  if (!window_) {
    return;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return;
  }

  WINDOWPLACEMENT windowPlacement;
  GetWindowPlacement(window_handle, &windowPlacement);

  if (windowPlacement.showCmd != SW_NORMAL) {
    PostMessage(window_handle, WM_SYSCOMMAND, SC_RESTORE, 0);
  }
}

void WindowApi::FocusWindow() {
  if (!window_) {
    return;
  }

  HWND hWnd = window_->GetWindowHandle();
  if (!hWnd) {
    return;
  }

  if (IsWindowMinimized()) {
    Restore();
  }

  ::SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  SetForegroundWindow(hWnd);
}

bool WindowApi::IsSkipTaskbar() {
  return is_skip_taskbar_;
}

void WindowApi::SetSkipTaskbar(bool is_skip_taskbar) {
  if (!window_) {
    return;
  }

  HWND hWnd = window_->GetWindowHandle();
  if (!hWnd) {
    return;
  }

  is_skip_taskbar_ = is_skip_taskbar;

  if (!task_bar_list_) {
    HRESULT hr =
        ::CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&task_bar_list_));
    if (SUCCEEDED(hr) && FAILED(task_bar_list_->HrInit())) {
      task_bar_list_ = nullptr;
    }
  }

  if (task_bar_list_) {
    LPVOID lp = NULL;
    CoInitialize(lp);
    task_bar_list_->HrInit();
    if (!is_skip_taskbar)
      task_bar_list_->AddTab(hWnd);
    else
      task_bar_list_->DeleteTab(hWnd);
  }
}

void WindowApi::SetOpacity(double opacity) {
  if (!window_) {
    return;
  }

  HWND hWnd = window_->GetWindowHandle();
  if (!hWnd) {
    return;
  }

  long gwlExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
  SetWindowLong(hWnd, GWL_EXSTYLE, gwlExStyle | WS_EX_LAYERED);
  SetLayeredWindowAttributes(hWnd, 0, static_cast<int8_t>(255 * opacity), 0x02);
}

void WindowApi::SetBackgroundColor(int backgroundColorA,
                                   int backgroundColorR,
                                   int backgroundColorG,
                                   int backgroundColorB) {
  if (!window_) {
    return;
  }

  HWND hWnd = window_->GetWindowHandle();
  if (!hWnd) {
    return;
  }

  SetBackgroundColorHwnd(hWnd, backgroundColorA, backgroundColorR,
                         backgroundColorG, backgroundColorB);
}

void WindowApi::SetIgnoreMouseEvents(bool ignore) {
  if (!window_) {
    return;
  }

  HWND hwnd = window_->GetWindowHandle();
  if (!hwnd) {
    return;
  }

  LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
  if (ignore)
    ex_style |= (WS_EX_TRANSPARENT | WS_EX_LAYERED);
  else
    ex_style &= ~(WS_EX_TRANSPARENT | WS_EX_LAYERED);

  ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
}

void WindowApi::SetBackgroundColorHwnd(HWND hWnd,
                                       int backgroundColorA,
                                       int backgroundColorR,
                                       int backgroundColorG,
                                       int backgroundColorB) {
  bool isTransparent = backgroundColorA == 0 && backgroundColorR == 0 &&
                       backgroundColorG == 0 && backgroundColorB == 0;
  const HINSTANCE hModule = LoadLibrary(TEXT("user32.dll"));
  if (hModule) {
    typedef enum _ACCENT_STATE {
      ACCENT_DISABLED = 0,
      ACCENT_ENABLE_GRADIENT = 1,
      ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
      ACCENT_ENABLE_BLURBEHIND = 3,
      ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
      ACCENT_ENABLE_HOSTBACKDROP = 5,
      ACCENT_INVALID_STATE = 6
    } ACCENT_STATE;
    struct ACCENTPOLICY {
      int nAccentState;
      int nFlags;
      int nColor;
      int nAnimationId;
    };
    struct WINCOMPATTRDATA {
      int nAttribute;
      PVOID pData;
      ULONG ulDataSize;
    };
    typedef BOOL(WINAPI * pSetWindowCompositionAttribute)(HWND,
                                                          WINCOMPATTRDATA*);
    const pSetWindowCompositionAttribute SetWindowCompositionAttribute =
        (pSetWindowCompositionAttribute)GetProcAddress(
            hModule, "SetWindowCompositionAttribute");
    if (SetWindowCompositionAttribute) {
      int32_t accent_state = isTransparent ? ACCENT_ENABLE_TRANSPARENTGRADIENT
                                           : ACCENT_ENABLE_GRADIENT;
      ACCENTPOLICY policy = {
          accent_state, 2,
          ((backgroundColorA << 24) + (backgroundColorB << 16) +
           (backgroundColorG << 8) + (backgroundColorR)),
          0};
      WINCOMPATTRDATA data = {19, &policy, sizeof(policy)};
      SetWindowCompositionAttribute(hWnd, &data);
    }
    FreeLibrary(hModule);
  }
}
inline int ScaleByDpi(int value, UINT dpi) {
  return MulDiv(value, dpi, 96);
}
struct NcMetrics {
  int cxFrame;
  int cyFrame;
  int paddedBorder;
  int resizeBorder;
};

NcMetrics GetNcMetrics(UINT dpi) {
  NcMetrics m{};
  m.cxFrame = ScaleByDpi(GetSystemMetrics(SM_CXFRAME), dpi);
  m.cyFrame = ScaleByDpi(GetSystemMetrics(SM_CYFRAME), dpi);
  m.paddedBorder = ScaleByDpi(GetSystemMetrics(SM_CXPADDEDBORDER), dpi);
  m.resizeBorder = m.cxFrame + m.paddedBorder;
  return m;
}

LRESULT WindowApi::OnNcCalcSize(HWND hwnd, WPARAM wParam, LPARAM lParam) {
  if (!wParam) {
    return 0;
  }
  NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lParam;
  UINT const dpi = flutter::GetDpiForHWND(hwnd);
  if (IsZoomed(hwnd)) {
    auto m = GetNcMetrics(dpi);

    params->rgrc[0].left += m.resizeBorder;
    params->rgrc[0].right -= m.resizeBorder;
    params->rgrc[0].top += m.resizeBorder;
    params->rgrc[0].bottom -= m.resizeBorder;
  } else {
    const auto originalTop = params->rgrc[0].top;
    const auto originalSize = params->rgrc[0];
    // apply the default frame
    const auto ret = DefWindowProc(hwnd, WM_NCCALCSIZE, wParam, lParam);
    if (ret != 0) {
      return ret;
    }
    auto newSize = params->rgrc[0];
    // Re-apply the original top from before the size of the default frame was
    // applied.
    newSize.top = originalTop;
    params->rgrc[0] = newSize;
  }
  return 0;
}

LRESULT WindowApi::OnNcHitTest(HWND hwnd,
                                WPARAM wParam,
                                LPARAM lParam,
                                int titleBarHeightLogical) {
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
  ScreenToClient(hwnd, &pt);

  RECT rc;
  GetClientRect(hwnd, &rc);
  UINT const dpi = flutter::GetDpiForHWND(hwnd);

  auto m = GetNcMetrics(dpi);
  int titleBarHeight = ScaleByDpi(titleBarHeightLogical, dpi);

  if (pt.y < m.resizeBorder) {
    if (pt.x < m.resizeBorder)
      return HTTOPLEFT;
    if (pt.x > rc.right - m.resizeBorder)
      return HTTOPRIGHT;
    return HTTOP;
  }
  if (pt.y > rc.bottom - m.resizeBorder) {
    if (pt.x < m.resizeBorder)
      return HTBOTTOMLEFT;
    if (pt.x > rc.right - m.resizeBorder)
      return HTBOTTOMRIGHT;
    return HTBOTTOM;
  }
  if (pt.x < m.resizeBorder)
    return HTLEFT;
  if (pt.x > rc.right - m.resizeBorder)
    return HTRIGHT;

  if (pt.y < titleBarHeight) {
    return HTCAPTION;
  }

  return HTCLIENT;
}

}  // namespace flutter
