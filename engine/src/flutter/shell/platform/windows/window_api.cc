// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_api.h"
#include <dwmapi.h>
#include <windowsx.h>
#include <cmath>
#include <vector>
#include "dpi_utils.h"
#include "flutter_windows_engine.h"
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

WindowApi::~WindowApi() {
  // Stop all animations when destroyed
  if (window_) {
    HWND hwnd = window_->GetWindowHandle();
    if (hwnd) {
      StopAllAnimations();
    }
  }
}

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

void WindowApi::SetNoSystemMenu() {
  if (!window_) {
    return;
  }

  HWND window_handle = window_->GetWindowHandle();
  if (!window_handle) {
    return;
  }
  DWORD gwlStyle = GetWindowLong(window_handle, GWL_STYLE);
  SetWindowLong(window_handle, GWL_STYLE, gwlStyle & ~(WS_SYSMENU));
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

void WindowApi::ShowWindowApi(int nCmd) {
  if (nCmd == SW_SHOW || nCmd == SW_SHOWNOACTIVATE) {
    FlutterWindowsEngine* engine = window_->GetEngine();
    if (engine != nullptr) {
      engine->SetNextFrameCallback(
          [this, nCmd]() { ::ShowWindow(window_->GetWindowHandle(), nCmd); });
      return;
    }
  }
  ::ShowWindow(window_->GetWindowHandle(), nCmd);
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

// Animation timer interval in milliseconds (targeting ~60 FPS).
constexpr UINT kAnimationTimerInterval = 16;

// Easing function implementations.
double WindowApi::CalculateEasing(double t, AnimationEasingType easing) {
  // Clamp t to [0, 1]
  t = (std::max)(0.0, (std::min)(1.0, t));

  switch (easing) {
    case AnimationEasingType::kLinear:
      return t;

    case AnimationEasingType::kEaseIn:
      // Quadratic ease in
      return t * t;

    case AnimationEasingType::kEaseOut:
      // Quadratic ease out
      return 1.0 - (1.0 - t) * (1.0 - t);

    case AnimationEasingType::kEaseInOut:
      // Quadratic ease in-out
      if (t < 0.5) {
        return 2.0 * t * t;
      } else {
        return 1.0 - pow(-2.0 * t + 2.0, 2) / 2.0;
      }

    case AnimationEasingType::kSpringBounce:
      return CalculateSpringBounce(t, spring_damping_, spring_stiffness_);

    case AnimationEasingType::kOvershoot: {
      // Overshoot effect (goes past target then settles back)
      const double c1 = 1.70158;
      const double c3 = c1 + 1.0;
      return 1.0 + c3 * pow(t - 1.0, 3) + c1 * pow(t - 1.0, 2);
    }

    default:
      return t;
  }
}

// macOS-style spring bounce calculation.
double WindowApi::CalculateSpringBounce(double t,
                                        double damping,
                                        double stiffness) {
  if (t >= 1.0)
    return 1.0;
  if (t <= 0.0)
    return 0.0;

  // Spring physics simulation for macOS-like bounce effect.
  // Uses a damped harmonic oscillator model.
  const double omega = sqrt(stiffness);  // Angular frequency
  const double zeta = damping;           // Damping ratio

  double result;
  if (zeta < 1.0) {
    // Underdamped: oscillates before settling (bouncy effect)
    const double omega_d = omega * sqrt(1.0 - zeta * zeta);
    const double decay = exp(-zeta * omega * t);
    result = 1.0 - decay * (cos(omega_d * t) +
                            (zeta * omega / omega_d) * sin(omega_d * t));
  } else if (zeta == 1.0) {
    // Critically damped: fastest approach without oscillation
    const double decay = exp(-omega * t);
    result = 1.0 - decay * (1.0 + omega * t);
  } else {
    // Overdamped: slow approach without oscillation
    const double s1 = -omega * (zeta + sqrt(zeta * zeta - 1.0));
    const double s2 = -omega * (zeta - sqrt(zeta * zeta - 1.0));
    result = 1.0 - (s2 * exp(s1 * t) - s1 * exp(s2 * t)) / (s2 - s1);
  }

  return (std::max)(0.0, (std::min)(1.0, result));
}

void WindowApi::SetSpringParameters(double damping, double stiffness) {
  spring_damping_ = (std::max)(0.1, (std::min)(2.0, damping));
  spring_stiffness_ = (std::max)(10.0, (std::min)(500.0, stiffness));
}

// High-precision timer frequency (cached)
static LARGE_INTEGER g_qpc_frequency = {0};

static void EnsureQPCFrequency() {
  if (g_qpc_frequency.QuadPart == 0) {
    QueryPerformanceFrequency(&g_qpc_frequency);
  }
}

static double GetElapsedMs(const LARGE_INTEGER& start) {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return static_cast<double>(now.QuadPart - start.QuadPart) * 1000.0 /
         static_cast<double>(g_qpc_frequency.QuadPart);
}

void WindowApi::UpdateAnimation(WindowAnimation& anim) {
  if (!anim.is_active || !anim.hwnd) {
    return;
  }

  EnsureQPCFrequency();
  double elapsed = GetElapsedMs(anim.start_time_qpc);

  if (elapsed >= static_cast<double>(anim.duration)) {
    // Animation complete
    anim.progress = 1.0;
    ApplyAnimationFrame(anim, 1.0);
    anim.is_active = false;

    // Kill the timer
    KillTimer(anim.hwnd, anim.timer_id);

    // Call completion callback
    if (anim.on_complete) {
      anim.on_complete();
    }
    return;
  }

  // Calculate progress using high-precision time
  anim.progress = elapsed / static_cast<double>(anim.duration);

  // Apply easing
  double eased_progress = CalculateEasing(anim.progress, anim.easing);

  // Apply the animation frame
  ApplyAnimationFrame(anim, eased_progress);
}

void WindowApi::ApplyAnimationFrame(WindowAnimation& anim,
                                    double eased_progress) {
  if (!anim.hwnd) {
    return;
  }

  // Use cached scale_factor and shadow offsets (computed at animation start)
  const double scale_factor = anim.scale_factor;

  switch (anim.property) {
    case AnimationPropertyType::kPosition: {
      double current_x =
          anim.start_x + (anim.target_x - anim.start_x) * eased_progress;
      double current_y =
          anim.start_y + (anim.target_y - anim.start_y) * eased_progress;

      int physical_x = static_cast<int>(current_x * scale_factor);
      int physical_y = static_cast<int>(current_y * scale_factor);

      // Apply cached shadow offset
      physical_x -= static_cast<int>(anim.shadow_left);
      physical_y -= static_cast<int>(anim.shadow_top);

      SetWindowPos(anim.hwnd, nullptr, physical_x, physical_y, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE);
      break;
    }

    case AnimationPropertyType::kSize: {
      double current_width =
          anim.start_width +
          (anim.target_width - anim.start_width) * eased_progress;
      double current_height =
          anim.start_height +
          (anim.target_height - anim.start_height) * eased_progress;

      int physical_width = static_cast<int>(current_width * scale_factor);
      int physical_height = static_cast<int>(current_height * scale_factor);

      // Apply cached shadow offset
      physical_width += static_cast<int>(anim.shadow_left + anim.shadow_right);
      physical_height += static_cast<int>(anim.shadow_top + anim.shadow_bottom);

      SetWindowPos(anim.hwnd, nullptr, 0, 0, physical_width, physical_height,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE);
      break;
    }

    case AnimationPropertyType::kBounds: {
      double current_x =
          anim.start_x + (anim.target_x - anim.start_x) * eased_progress;
      double current_y =
          anim.start_y + (anim.target_y - anim.start_y) * eased_progress;
      double current_width =
          anim.start_width +
          (anim.target_width - anim.start_width) * eased_progress;
      double current_height =
          anim.start_height +
          (anim.target_height - anim.start_height) * eased_progress;

      int physical_x = static_cast<int>(current_x * scale_factor);
      int physical_y = static_cast<int>(current_y * scale_factor);
      int physical_width = static_cast<int>(current_width * scale_factor);
      int physical_height = static_cast<int>(current_height * scale_factor);

      // Apply cached shadow offset
      physical_x -= static_cast<int>(anim.shadow_left);
      physical_y -= static_cast<int>(anim.shadow_top);
      physical_width += static_cast<int>(anim.shadow_left + anim.shadow_right);
      physical_height += static_cast<int>(anim.shadow_top + anim.shadow_bottom);

      SetWindowPos(anim.hwnd, nullptr, physical_x, physical_y, physical_width,
                   physical_height,
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE);
      break;
    }

    case AnimationPropertyType::kOpacity: {
      double current_opacity =
          anim.start_opacity +
          (anim.target_opacity - anim.start_opacity) * eased_progress;

      // Only set WS_EX_LAYERED once (at animation start, opacity is already
      // set)
      SetLayeredWindowAttributes(
          anim.hwnd, 0,
          static_cast<BYTE>((std::max)(0.0, (std::min)(1.0, current_opacity)) *
                            255),
          LWA_ALPHA);
      break;
    }
  }

  // Sync with DWM compositor for smoother animation (VSync alignment)
  DwmFlush();
}

bool WindowApi::OnTimer(WPARAM wParam, LPARAM lParam) {
  UINT_PTR timer_id = static_cast<UINT_PTR>(wParam);

  auto it = active_animations_.find(timer_id);
  if (it == active_animations_.end()) {
    return false;  // Not our timer
  }

  WindowAnimation& anim = it->second;
  if (!anim.is_active) {
    // Animation was stopped, clean up
    active_animations_.erase(it);
    return true;
  }

  UpdateAnimation(anim);

  // Clean up completed animations
  if (!anim.is_active) {
    active_animations_.erase(it);
  }

  return true;
}

void WindowApi::StopConflictingAnimations(AnimationPropertyType new_property) {
  std::vector<UINT_PTR> to_stop;

  for (const auto& pair : active_animations_) {
    const WindowAnimation& anim = pair.second;
    if (!anim.is_active) {
      continue;
    }

    bool conflicts = false;
    switch (new_property) {
      case AnimationPropertyType::kPosition:
        // Position conflicts with Bounds (both modify position)
        conflicts = (anim.property == AnimationPropertyType::kPosition ||
                     anim.property == AnimationPropertyType::kBounds);
        break;

      case AnimationPropertyType::kSize:
        // Size conflicts with Bounds (both modify size)
        conflicts = (anim.property == AnimationPropertyType::kSize ||
                     anim.property == AnimationPropertyType::kBounds);
        break;

      case AnimationPropertyType::kBounds:
        // Bounds conflicts with Position, Size, and other Bounds
        conflicts = (anim.property == AnimationPropertyType::kPosition ||
                     anim.property == AnimationPropertyType::kSize ||
                     anim.property == AnimationPropertyType::kBounds);
        break;

      case AnimationPropertyType::kOpacity:
        // Opacity only conflicts with other Opacity animations
        conflicts = (anim.property == AnimationPropertyType::kOpacity);
        break;
    }

    if (conflicts) {
      to_stop.push_back(pair.first);
    }
  }

  // Stop all conflicting animations
  for (UINT_PTR timer_id : to_stop) {
    StopAnimation(timer_id);
  }
}

// ============================================================
// Immediate Application Helpers (for setting start values)
// ============================================================

void WindowApi::ApplyPositionImmediate(HWND hwnd,
                                       double x,
                                       double y,
                                       double scale_factor) {
  int physical_x = static_cast<int>(x * scale_factor);
  int physical_y = static_cast<int>(y * scale_factor);

  // Account for shadow offset
  RECT frame_rect, window_rect;
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                      &frame_rect, sizeof(frame_rect))) &&
      GetWindowRect(hwnd, &window_rect)) {
    LONG left_shadow = frame_rect.left - window_rect.left;
    LONG top_shadow = frame_rect.top - window_rect.top;
    physical_x -= static_cast<int>(left_shadow);
    physical_y -= static_cast<int>(top_shadow);
  }

  SetWindowPos(hwnd, nullptr, physical_x, physical_y, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowApi::ApplySizeImmediate(HWND hwnd,
                                   double width,
                                   double height,
                                   double scale_factor) {
  int physical_width = static_cast<int>(width * scale_factor);
  int physical_height = static_cast<int>(height * scale_factor);

  // Account for shadow
  RECT frame_rect, window_rect;
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                      &frame_rect, sizeof(frame_rect))) &&
      GetWindowRect(hwnd, &window_rect)) {
    LONG left_shadow = frame_rect.left - window_rect.left;
    LONG right_shadow = window_rect.right - frame_rect.right;
    LONG top_shadow = frame_rect.top - window_rect.top;
    LONG bottom_shadow = window_rect.bottom - frame_rect.bottom;
    physical_width += static_cast<int>(left_shadow + right_shadow);
    physical_height += static_cast<int>(top_shadow + bottom_shadow);
  }

  SetWindowPos(hwnd, nullptr, 0, 0, physical_width, physical_height,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowApi::ApplyBoundsImmediate(HWND hwnd,
                                     double x,
                                     double y,
                                     double width,
                                     double height,
                                     double scale_factor) {
  int physical_x = static_cast<int>(x * scale_factor);
  int physical_y = static_cast<int>(y * scale_factor);
  int physical_width = static_cast<int>(width * scale_factor);
  int physical_height = static_cast<int>(height * scale_factor);

  // Account for shadow
  RECT frame_rect, window_rect;
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                      &frame_rect, sizeof(frame_rect))) &&
      GetWindowRect(hwnd, &window_rect)) {
    LONG left_shadow = frame_rect.left - window_rect.left;
    LONG right_shadow = window_rect.right - frame_rect.right;
    LONG top_shadow = frame_rect.top - window_rect.top;
    LONG bottom_shadow = window_rect.bottom - frame_rect.bottom;
    physical_x -= static_cast<int>(left_shadow);
    physical_y -= static_cast<int>(top_shadow);
    physical_width += static_cast<int>(left_shadow + right_shadow);
    physical_height += static_cast<int>(top_shadow + bottom_shadow);
  }

  SetWindowPos(hwnd, nullptr, physical_x, physical_y, physical_width,
               physical_height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowApi::ApplyOpacityImmediate(HWND hwnd, double opacity) {
  LONG ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
  SetWindowLong(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
  SetLayeredWindowAttributes(
      hwnd, 0,
      static_cast<BYTE>((std::max)(0.0, (std::min)(1.0, opacity)) * 255),
      LWA_ALPHA);
}

// ============================================================
// Unified Animation Interface Implementation
// ============================================================

UINT_PTR WindowApi::StartAnimation(const AnimationRequest& request) {
  if (!window_) {
    return 0;
  }

  HWND hwnd = window_->GetWindowHandle();
  if (!hwnd) {
    return 0;
  }

  // Stop any conflicting animations before starting new one
  StopConflictingAnimations(request.property);

  // Apply custom spring parameters if specified
  if (request.use_custom_spring) {
    spring_damping_ = request.spring_damping;
    spring_stiffness_ = request.spring_stiffness;
  }

  // Get DPI scale factor (cached for the animation duration)
  UINT dpi = GetDpiForHWND(hwnd);
  double scale_factor = static_cast<double>(dpi) / USER_DEFAULT_SCREEN_DPI;

  // Get shadow offsets (cached for the animation duration)
  LONG shadow_left = 0, shadow_right = 0, shadow_top = 0, shadow_bottom = 0;
  RECT frame_rect, window_rect;
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                      &frame_rect, sizeof(frame_rect))) &&
      GetWindowRect(hwnd, &window_rect)) {
    shadow_left = frame_rect.left - window_rect.left;
    shadow_right = window_rect.right - frame_rect.right;
    shadow_top = frame_rect.top - window_rect.top;
    shadow_bottom = window_rect.bottom - frame_rect.bottom;
  }

  // Initialize high-precision timer
  EnsureQPCFrequency();

  // Create animation instance
  WindowAnimation anim;
  anim.timer_id = next_timer_id_++;
  anim.hwnd = hwnd;
  anim.property = request.property;
  anim.easing = request.easing;
  anim.duration = request.duration;
  anim.spring_damping = spring_damping_;
  anim.spring_stiffness = spring_stiffness_;
  anim.on_complete = request.on_complete;
  anim.is_active = true;

  // Cache scale factor and shadow offsets
  anim.scale_factor = scale_factor;
  anim.shadow_left = shadow_left;
  anim.shadow_right = shadow_right;
  anim.shadow_top = shadow_top;
  anim.shadow_bottom = shadow_bottom;

  // Use high-precision timer
  QueryPerformanceCounter(&anim.start_time_qpc);

  // Initialize start and target values based on property type
  switch (request.property) {
    case AnimationPropertyType::kPosition: {
      // Get current position (use cached frame_rect if available)
      if (frame_rect.left != 0 || frame_rect.top != 0 ||
          frame_rect.right != 0 || frame_rect.bottom != 0) {
        anim.start_x = static_cast<double>(frame_rect.left) / scale_factor;
        anim.start_y = static_cast<double>(frame_rect.top) / scale_factor;
      } else if (GetWindowRect(hwnd, &window_rect)) {
        anim.start_x = static_cast<double>(window_rect.left) / scale_factor;
        anim.start_y = static_cast<double>(window_rect.top) / scale_factor;
      }
      anim.target_x = request.target_x;
      anim.target_y = request.target_y;
      break;
    }

    case AnimationPropertyType::kSize: {
      // Get current size (use cached frame_rect if available)
      if (frame_rect.right - frame_rect.left > 0) {
        anim.start_width =
            static_cast<double>(frame_rect.right - frame_rect.left) /
            scale_factor;
        anim.start_height =
            static_cast<double>(frame_rect.bottom - frame_rect.top) /
            scale_factor;
      } else if (GetWindowRect(hwnd, &window_rect)) {
        anim.start_width =
            static_cast<double>(window_rect.right - window_rect.left) /
            scale_factor;
        anim.start_height =
            static_cast<double>(window_rect.bottom - window_rect.top) /
            scale_factor;
      }
      anim.target_width = request.target_width;
      anim.target_height = request.target_height;
      break;
    }

    case AnimationPropertyType::kBounds: {
      // Get current bounds (use cached frame_rect if available)
      if (frame_rect.right - frame_rect.left > 0) {
        anim.start_x = static_cast<double>(frame_rect.left) / scale_factor;
        anim.start_y = static_cast<double>(frame_rect.top) / scale_factor;
        anim.start_width =
            static_cast<double>(frame_rect.right - frame_rect.left) /
            scale_factor;
        anim.start_height =
            static_cast<double>(frame_rect.bottom - frame_rect.top) /
            scale_factor;
      } else if (GetWindowRect(hwnd, &window_rect)) {
        anim.start_x = static_cast<double>(window_rect.left) / scale_factor;
        anim.start_y = static_cast<double>(window_rect.top) / scale_factor;
        anim.start_width =
            static_cast<double>(window_rect.right - window_rect.left) /
            scale_factor;
        anim.start_height =
            static_cast<double>(window_rect.bottom - window_rect.top) /
            scale_factor;
      }
      anim.target_x = request.target_x;
      anim.target_y = request.target_y;
      anim.target_width = request.target_width;
      anim.target_height = request.target_height;
      break;
    }

    case AnimationPropertyType::kOpacity: {
      // Get current opacity and ensure WS_EX_LAYERED is set
      anim.start_opacity = 1.0;
      LONG ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
      if (!(ex_style & WS_EX_LAYERED)) {
        // Set WS_EX_LAYERED once at animation start
        SetWindowLong(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
      }
      if (ex_style & WS_EX_LAYERED) {
        BYTE alpha;
        DWORD flags;
        if (GetLayeredWindowAttributes(hwnd, nullptr, &alpha, &flags)) {
          if (flags & LWA_ALPHA) {
            anim.start_opacity = static_cast<double>(alpha) / 255.0;
          }
        }
      }
      anim.target_opacity = request.target_opacity;
      break;
    }
  }

  // Store and start timer
  active_animations_[anim.timer_id] = anim;
  SetTimer(hwnd, anim.timer_id, kAnimationTimerInterval, nullptr);

  return anim.timer_id;
}

// ============================================================
// Convenience Animation Methods (delegate to StartAnimation)
// ============================================================

UINT_PTR WindowApi::StartPositionAnimation(double target_x,
                                           double target_y,
                                           DWORD duration,
                                           AnimationEasingType easing,
                                           std::function<void()> on_complete) {
  AnimationRequest request;
  request.property = AnimationPropertyType::kPosition;
  request.target_x = target_x;
  request.target_y = target_y;
  request.duration = duration;
  request.easing = easing;
  request.on_complete = on_complete;
  return StartAnimation(request);
}

UINT_PTR WindowApi::StartSizeAnimation(double target_width,
                                       double target_height,
                                       DWORD duration,
                                       AnimationEasingType easing,
                                       std::function<void()> on_complete) {
  AnimationRequest request;
  request.property = AnimationPropertyType::kSize;
  request.target_width = target_width;
  request.target_height = target_height;
  request.duration = duration;
  request.easing = easing;
  request.on_complete = on_complete;
  return StartAnimation(request);
}

UINT_PTR WindowApi::StartBoundsAnimation(double target_x,
                                         double target_y,
                                         double target_width,
                                         double target_height,
                                         DWORD duration,
                                         AnimationEasingType easing,
                                         std::function<void()> on_complete) {
  AnimationRequest request;
  request.property = AnimationPropertyType::kBounds;
  request.target_x = target_x;
  request.target_y = target_y;
  request.target_width = target_width;
  request.target_height = target_height;
  request.duration = duration;
  request.easing = easing;
  request.on_complete = on_complete;
  return StartAnimation(request);
}

UINT_PTR WindowApi::StartOpacityAnimation(double target_opacity,
                                          DWORD duration,
                                          AnimationEasingType easing,
                                          std::function<void()> on_complete) {
  AnimationRequest request;
  request.property = AnimationPropertyType::kOpacity;
  request.target_opacity = target_opacity;
  request.duration = duration;
  request.easing = easing;
  request.on_complete = on_complete;
  return StartAnimation(request);
}

void WindowApi::StopAnimation(UINT_PTR timer_id) {
  auto it = active_animations_.find(timer_id);
  if (it != active_animations_.end()) {
    WindowAnimation& anim = it->second;
    if (anim.hwnd) {
      KillTimer(anim.hwnd, timer_id);
    }
    active_animations_.erase(it);
  }
}

void WindowApi::StopAllAnimations() {
  if (!window_) {
    return;
  }

  HWND hwnd = window_->GetWindowHandle();
  if (!hwnd) {
    return;
  }

  auto it = active_animations_.begin();
  while (it != active_animations_.end()) {
    KillTimer(hwnd, it->first);
    it = active_animations_.erase(it);
  }
}

bool WindowApi::HasActiveAnimation() {
  for (const auto& pair : active_animations_) {
    if (pair.second.is_active) {
      return true;
    }
  }
  return false;
}

}  // namespace flutter

// ============================================================
// C Export Animation API Implementations
// ============================================================

namespace {

// Helper function to get WindowApi from HWND.
std::shared_ptr<flutter::WindowApi> GetWindowApiFromHwnd(HWND hwnd) {
  if (!hwnd) {
    return nullptr;
  }
  flutter::HostWindow* host_window =
      flutter::HostWindow::GetThisFromHandle(hwnd);
  if (!host_window) {
    return nullptr;
  }
  return host_window->GetApi();
}

// Helper function to convert C easing type to C++ enum.
flutter::AnimationEasingType ConvertEasingType(int8_t easing) {
  switch (easing) {
    case flutter::kAnimationEasingLinear:
      return flutter::AnimationEasingType::kLinear;
    case flutter::kAnimationEasingEaseIn:
      return flutter::AnimationEasingType::kEaseIn;
    case flutter::kAnimationEasingEaseOut:
      return flutter::AnimationEasingType::kEaseOut;
    case flutter::kAnimationEasingEaseInOut:
      return flutter::AnimationEasingType::kEaseInOut;
    case flutter::kAnimationEasingSpringBounce:
      return flutter::AnimationEasingType::kSpringBounce;
    case flutter::kAnimationEasingOvershoot:
      return flutter::AnimationEasingType::kOvershoot;
    default:
      return flutter::AnimationEasingType::kLinear;
  }
}

}  // namespace

extern "C" {
void InternalFlutterWindows_WindowApi_DragWindow(HWND hwnd, int32_t state) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->DragWindow(state);
  }
}

void InternalFlutterWindows_WindowApi_SetBounds(
    HWND hwnd,
    const flutter::WindowBoundsRequest* request) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->SetBounds(request);
  }
}

flutter::ActualWindowBounds InternalFlutterWindows_WindowApi_GetWindowBounds(
    HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  flutter::ActualWindowBounds result;
  result.x = 0;
  result.y = 0;
  result.width = 0;
  result.height = 0;
  if (window) {
    const flutter::Rect rect = window->GetApi()->GetBounds();
    result.x = rect.left();
    result.y = rect.top();
    result.width = rect.width();
    result.height = rect.height();
  }
  return result;
}

void InternalFlutterWindows_WindowApi_FocusWindow(HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->FocusWindow();
  }
}

void InternalFlutterWindows_WindowApi_SetNoFrame(HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->SetNoFrame();
  }
}

bool InternalFlutterWindows_WindowApi_IsAlwaysOnTop(HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    return window->GetApi()->IsAlwaysOnTop();
  }
  return false;
}

void InternalFlutterWindows_WindowApi_SetAlwaysOnTop(HWND hwnd,
                                                     bool is_always_on_top) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->SetAlwaysOnTop(is_always_on_top);
  }
}

bool InternalFlutterWindows_WindowApi_IsResizable(HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    return window->GetApi()->IsResizable();
  }
  return false;
}

void InternalFlutterWindows_WindowApi_SetResizable(HWND hwnd,
                                                   bool is_resizable) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->SetResizable(is_resizable);
  }
}

bool InternalFlutterWindows_WindowApi_IsMinimized(HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    return window->GetApi()->IsWindowMinimized();
  }
  return false;
}

void InternalFlutterWindows_WindowApi_Restore(HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->Restore();
  }
}

bool InternalFlutterWindows_WindowApi_IsSkipTaskbar(HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    return window->GetApi()->IsSkipTaskbar();
  }
  return false;
}

void InternalFlutterWindows_WindowApi_SetSkipTaskbar(HWND hwnd,
                                                     bool is_skip_taskbar) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->SetSkipTaskbar(is_skip_taskbar);
  }
}

void InternalFlutterWindows_WindowApi_CenterWindowOnMonitor(HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->CenterWindowOnMonitor();
  }
}

void InternalFlutterWindows_WindowApi_ShowWindow(HWND hwnd, int nCmd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->ShowWindowApi(nCmd);
  }
}

void InternalFlutterWindows_WindowApi_SetNoSystemMenu(HWND hwnd) {
  flutter::HostWindow* window = flutter::HostWindow::GetThisFromHandle(hwnd);
  if (window) {
    window->GetApi()->SetNoSystemMenu();
  }
}

uint64_t InternalFlutterWindows_WindowApi_StartPositionAnimation(
    HWND hwnd,
    const flutter::PositionAnimationRequest* request) {
  if (!request) {
    return 0;
  }
  auto api = GetWindowApiFromHwnd(hwnd);
  if (!api) {
    return 0;
  }

  flutter::AnimationEasingType easing = ConvertEasingType(request->easing);

  // Set custom spring parameters if specified.
  if (request->use_custom_spring) {
    api->SetSpringParameters(request->spring_damping,
                             request->spring_stiffness);
  }

  flutter::AnimationRequest anim_request = flutter::AnimationRequest::Position(
      request->target_pos.x, request->target_pos.y, request->duration, easing);

  return api->StartAnimation(anim_request);
}

uint64_t InternalFlutterWindows_WindowApi_StartSizeAnimation(
    HWND hwnd,
    const flutter::SizeAnimationRequest* request) {
  if (!request) {
    return 0;
  }
  auto api = GetWindowApiFromHwnd(hwnd);
  if (!api) {
    return 0;
  }

  flutter::AnimationEasingType easing = ConvertEasingType(request->easing);

  if (request->use_custom_spring) {
    api->SetSpringParameters(request->spring_damping,
                             request->spring_stiffness);
  }

  flutter::AnimationRequest anim_request = flutter::AnimationRequest::Size(
      request->target_size.width, request->target_size.height,
      request->duration, easing);

  return api->StartAnimation(anim_request);
}

uint64_t InternalFlutterWindows_WindowApi_StartBoundsAnimation(
    HWND hwnd,
    const flutter::BoundsAnimationRequest* request) {
  if (!request) {
    return 0;
  }
  auto api = GetWindowApiFromHwnd(hwnd);
  if (!api) {
    return 0;
  }

  flutter::AnimationEasingType easing = ConvertEasingType(request->easing);

  if (request->use_custom_spring) {
    api->SetSpringParameters(request->spring_damping,
                             request->spring_stiffness);
  }

  flutter::AnimationRequest anim_request = flutter::AnimationRequest::Bounds(
      request->target_bounds.x, request->target_bounds.y,
      request->target_bounds.width, request->target_bounds.height,
      request->duration, easing);

  return api->StartAnimation(anim_request);
}

uint64_t InternalFlutterWindows_WindowApi_StartOpacityAnimation(
    HWND hwnd,
    const flutter::OpacityAnimationRequest* request) {
  if (!request) {
    return 0;
  }
  auto api = GetWindowApiFromHwnd(hwnd);
  if (!api) {
    return 0;
  }

  flutter::AnimationEasingType easing = ConvertEasingType(request->easing);

  flutter::AnimationRequest anim_request = flutter::AnimationRequest::Opacity(
      request->target_opacity, request->duration, easing);

  return api->StartAnimation(anim_request);
}

void InternalFlutterWindows_WindowApi_StopAnimation(HWND hwnd,
                                                    uint64_t timer_id) {
  auto api = GetWindowApiFromHwnd(hwnd);
  if (!api) {
    return;
  }
  api->StopAnimation(timer_id);
}

void InternalFlutterWindows_WindowApi_StopAllAnimations(HWND hwnd) {
  auto api = GetWindowApiFromHwnd(hwnd);
  if (!api) {
    return;
  }
  api->StopAllAnimations();
}

bool InternalFlutterWindows_WindowApi_HasActiveAnimation(HWND hwnd) {
  auto api = GetWindowApiFromHwnd(hwnd);
  if (!api) {
    return false;
  }
  return api->HasActiveAnimation();
}

void InternalFlutterWindows_WindowApi_SetSpringParameters(HWND hwnd,
                                                          double damping,
                                                          double stiffness) {
  auto api = GetWindowApiFromHwnd(hwnd);
  if (!api) {
    return;
  }
  api->SetSpringParameters(damping, stiffness);
}
}