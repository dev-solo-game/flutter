// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_WINDOWS_WINDOW_API_H_
#define FLUTTER_SHELL_PLATFORM_WINDOWS_WINDOW_API_H_

#include <shobjidl.h>
#include <windows.h>
#include <wrl/client.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "flutter/shell/geometry/geometry.h"
#include "flutter/shell/platform/common/public/flutter_export.h"
#include "window_manager.h"

namespace flutter {

class HostWindow;

// ============================================================
// C-compatible Animation Types (using int8_t for enums)
// ============================================================

// Animation easing type values.
constexpr int8_t kAnimationEasingLinear = 0;
constexpr int8_t kAnimationEasingEaseIn = 1;
constexpr int8_t kAnimationEasingEaseOut = 2;
constexpr int8_t kAnimationEasingEaseInOut = 3;
constexpr int8_t kAnimationEasingSpringBounce = 4;
constexpr int8_t kAnimationEasingOvershoot = 5;

// Animation property type values.
constexpr int8_t kAnimationPropertyPosition = 0;
constexpr int8_t kAnimationPropertySize = 1;
constexpr int8_t kAnimationPropertyBounds = 2;
constexpr int8_t kAnimationPropertyOpacity = 3;
struct ActualWindowPosition {
  double x;
  double y;
};
struct WindowPositionRequest {
  bool has_pos = false;
  double x;
  double y;
};

struct ActualWindowBounds {
  double x;
  double y;
  double width;
  double height;
};

struct WindowBoundsRequest {
  WindowPositionRequest position;
  WindowSizeRequest size;
};

// Position animation request structure (C-compatible).
struct PositionAnimationRequest {
  ActualWindowPosition target_pos;
  uint32_t duration;
  int8_t easing;
  bool use_custom_spring;
  double spring_damping;
  double spring_stiffness;
};

// Size animation request structure (C-compatible).
struct SizeAnimationRequest {
  ActualWindowSize target_size;
  uint32_t duration;
  int8_t easing;
  bool use_custom_spring;
  double spring_damping;
  double spring_stiffness;
};

// Bounds animation request structure (C-compatible).
struct BoundsAnimationRequest {
  ActualWindowBounds target_bounds;
  uint32_t duration;
  int8_t easing;
  bool use_custom_spring;
  double spring_damping;
  double spring_stiffness;
};

// Opacity animation request structure (C-compatible).
struct OpacityAnimationRequest {
  double target_opacity;
  uint32_t duration;
  int8_t easing;
};

// ============================================================
// C++ Animation Types
// ============================================================

// Animation easing type enumeration.
enum class AnimationEasingType {
  kLinear,        // Linear interpolation
  kEaseIn,        // Ease in (slow start)
  kEaseOut,       // Ease out (slow end)
  kEaseInOut,     // Ease in and out
  kSpringBounce,  // macOS-style spring bounce effect
  kOvershoot,     // Overshoot and settle back
};

// Animation property type enumeration.
enum class AnimationPropertyType {
  kPosition,  // Animate window position
  kSize,      // Animate window size
  kBounds,    // Animate both position and size
  kOpacity,   // Animate window opacity
};

// Animation request structure for unified animation interface.
struct AnimationRequest {
  AnimationPropertyType property = AnimationPropertyType::kPosition;
  AnimationEasingType easing = AnimationEasingType::kSpringBounce;

  // Target values (use based on property type)
  double target_x = 0;          // For kPosition, kBounds
  double target_y = 0;          // For kPosition, kBounds
  double target_width = 0;      // For kSize, kBounds
  double target_height = 0;     // For kSize, kBounds
  double target_opacity = 1.0;  // For kOpacity

  // Animation timing
  DWORD duration = 300;  // Animation duration in milliseconds

  // Spring parameters (for kSpringBounce easing)
  bool use_custom_spring = false;
  double spring_damping = 0.7;
  double spring_stiffness = 100.0;

  // Completion callback
  std::function<void()> on_complete = nullptr;

  // Helper static methods to create common animation requests
  static AnimationRequest Position(
      double x,
      double y,
      DWORD duration = 300,
      AnimationEasingType easing = AnimationEasingType::kSpringBounce) {
    AnimationRequest req;
    req.property = AnimationPropertyType::kPosition;
    req.target_x = x;
    req.target_y = y;
    req.duration = duration;
    req.easing = easing;
    return req;
  }

  static AnimationRequest Size(
      double width,
      double height,
      DWORD duration = 300,
      AnimationEasingType easing = AnimationEasingType::kSpringBounce) {
    AnimationRequest req;
    req.property = AnimationPropertyType::kSize;
    req.target_width = width;
    req.target_height = height;
    req.duration = duration;
    req.easing = easing;
    return req;
  }

  static AnimationRequest Bounds(
      double x,
      double y,
      double width,
      double height,
      DWORD duration = 300,
      AnimationEasingType easing = AnimationEasingType::kSpringBounce) {
    AnimationRequest req;
    req.property = AnimationPropertyType::kBounds;
    req.target_x = x;
    req.target_y = y;
    req.target_width = width;
    req.target_height = height;
    req.duration = duration;
    req.easing = easing;
    return req;
  }

  static AnimationRequest Opacity(
      double opacity,
      DWORD duration = 300,
      AnimationEasingType easing = AnimationEasingType::kEaseOut) {
    AnimationRequest req;
    req.property = AnimationPropertyType::kOpacity;
    req.target_opacity = opacity;
    req.duration = duration;
    req.easing = easing;
    return req;
  }

  // Convenience methods for common effects
  static AnimationRequest FadeIn(DWORD duration = 300) {
    return Opacity(1.0, duration, AnimationEasingType::kEaseOut);
  }

  static AnimationRequest FadeOut(DWORD duration = 300) {
    return Opacity(0.0, duration, AnimationEasingType::kEaseIn);
  }
};

// Window animation instance structure.
struct WindowAnimation {
  UINT_PTR timer_id = 0;  // Timer identifier
  HWND hwnd = nullptr;    // Target window handle
  AnimationPropertyType property = AnimationPropertyType::kPosition;
  AnimationEasingType easing = AnimationEasingType::kSpringBounce;

  // Start values
  double start_x = 0;
  double start_y = 0;
  double start_width = 0;
  double start_height = 0;
  double start_opacity = 1.0;

  // Target values
  double target_x = 0;
  double target_y = 0;
  double target_width = 0;
  double target_height = 0;
  double target_opacity = 1.0;

  // Animation timing (using high-precision timer)
  LARGE_INTEGER start_time_qpc;  // High-precision start time
  DWORD duration = 300;          // Animation duration (ms)
  double progress = 0.0;         // Current progress (0.0 - 1.0)

  // Cached values (computed once at animation start)
  double scale_factor = 1.0;  // DPI scale factor
  LONG shadow_left = 0;       // Shadow offsets
  LONG shadow_right = 0;
  LONG shadow_top = 0;
  LONG shadow_bottom = 0;

  // Spring bounce parameters (for macOS-style effect)
  double spring_damping = 0.7;      // Damping ratio (0-1, lower = more bounce)
  double spring_stiffness = 100.0;  // Spring stiffness
  double spring_velocity = 0.0;     // Initial velocity

  // Callback when animation completes
  std::function<void()> on_complete;

  // Whether the animation is active
  bool is_active = false;
};

// A utility class that provides window manipulation APIs.
// All methods take a HostWindow* as the first parameter.
// This class maintains state data such as drag information.
class WindowApi {
 public:
  explicit WindowApi(HostWindow* window);
  ~WindowApi();

  // Sets the window bounds (position and size).
  void SetBounds(const WindowBoundsRequest* request);

  // Gets the window bounds.
  Rect GetBounds();

  // Gets the window position.
  Point GetPosition();

  // Handles window dragging.
  // state: 0 = start (record mouse position and enter drag state)
  //        1 = update (move window based on current mouse position)
  //        2 = end (exit drag state)
  void DragWindow(int state);

  void SetNoSystemMenu();
  // Removes the window frame.
  void SetNoFrame();

  // Expands the window to cover all monitors.
  void FullOnMonitors();

  // Returns whether the window is always on top.
  bool IsAlwaysOnTop();

  // Sets whether the window should be always on top.
  void SetAlwaysOnTop(bool is_always_on_top);

  // Returns whether the window is resizable.
  bool IsResizable();

  // Sets whether the window should be resizable.
  void SetResizable(bool is_resizable);

  // Centers the window on the current monitor.
  void CenterWindowOnMonitor();

  // Returns whether the window is minimized.
  bool IsWindowMinimized();

  // Restores the window from minimized state.
  void Restore();

  // Brings the window to focus.
  void FocusWindow();

  // Returns whether the window is hidden from taskbar.
  bool IsSkipTaskbar();

  // Sets whether the window should be hidden from taskbar.
  void SetSkipTaskbar(bool is_skip_taskbar);

  // Sets the window opacity.
  void SetOpacity(double opacity);

  // Sets the window background color.
  void SetBackgroundColor(int backgroundColorA,
                          int backgroundColorR,
                          int backgroundColorG,
                          int backgroundColorB);

  // Sets whether the window should ignore mouse events.
  void SetIgnoreMouseEvents(bool ignore);

  void ShowWindowApi(int nCmd);

  static LRESULT OnNcCalcSize(HWND hwnd, WPARAM wParam, LPARAM lParam);
  static LRESULT OnNcHitTest(HWND hwnd,
                             WPARAM wParam,
                             LPARAM lParam,
                             int titleBarHeightLogical);

  // WM_TIMER handler for window animations.
  // Returns true if the timer was handled, false otherwise.
  bool OnTimer(WPARAM wParam, LPARAM lParam);

  // ============================================================
  // Unified Animation Interface
  // ============================================================

  // Starts an animation based on the request parameters.
  // This is the primary interface for starting animations.
  // Returns the timer ID for the animation, or 0 if failed.
  UINT_PTR StartAnimation(const AnimationRequest& request);

  // ============================================================
  // Convenience Animation Methods (call StartAnimation internally)
  // ============================================================

  // Starts a position animation with spring bounce effect.
  UINT_PTR StartPositionAnimation(
      double target_x,
      double target_y,
      DWORD duration = 300,
      AnimationEasingType easing = AnimationEasingType::kSpringBounce,
      std::function<void()> on_complete = nullptr);

  // Starts a size animation with spring bounce effect.
  UINT_PTR StartSizeAnimation(
      double target_width,
      double target_height,
      DWORD duration = 300,
      AnimationEasingType easing = AnimationEasingType::kSpringBounce,
      std::function<void()> on_complete = nullptr);

  // Starts a bounds animation with spring bounce effect.
  UINT_PTR StartBoundsAnimation(
      double target_x,
      double target_y,
      double target_width,
      double target_height,
      DWORD duration = 300,
      AnimationEasingType easing = AnimationEasingType::kSpringBounce,
      std::function<void()> on_complete = nullptr);

  // Starts an opacity animation.
  UINT_PTR StartOpacityAnimation(
      double target_opacity,
      DWORD duration = 300,
      AnimationEasingType easing = AnimationEasingType::kEaseOut,
      std::function<void()> on_complete = nullptr);

  // Stops an animation by timer ID.
  void StopAnimation(UINT_PTR timer_id);

  // Stops all animations for this window.
  void StopAllAnimations();

  // Checks if this window has active animations.
  bool HasActiveAnimation();

  // Sets spring bounce parameters for future animations.
  void SetSpringParameters(double damping, double stiffness);

 private:
  // Private helper method for setting background color on a specific HWND.
  void SetBackgroundColorHwnd(HWND hWnd,
                              int backgroundColorA,
                              int backgroundColorR,
                              int backgroundColorG,
                              int backgroundColorB);

  // The associated host window.
  HostWindow* window_;

  // Drag state data.
  POINT drag_start_cursor_pos_ = {0, 0};
  POINT drag_start_window_pos_ = {0, 0};
  bool is_dragging_ = false;

  // Skip taskbar state.
  bool is_skip_taskbar_ = false;

  // Resizable state.
  bool is_resizable_ = false;

  // Taskbar list interface for taskbar operations.
  Microsoft::WRL::ComPtr<ITaskbarList> task_bar_list_;

  // Animation storage and management.
  std::unordered_map<UINT_PTR, WindowAnimation> active_animations_;
  UINT_PTR next_timer_id_ = 1000;
  double spring_damping_ = 0.7;
  double spring_stiffness_ = 100.0;

  // Animation helper methods.
  double CalculateEasing(double t, AnimationEasingType easing);
  double CalculateSpringBounce(double t, double damping, double stiffness);
  void UpdateAnimation(WindowAnimation& anim);
  void ApplyAnimationFrame(WindowAnimation& anim, double eased_progress);

  // Stops animations that would conflict with the given property type.
  // Position conflicts with Bounds (both modify position).
  // Size conflicts with Bounds (both modify size).
  // Bounds conflicts with both Position and Size.
  void StopConflictingAnimations(AnimationPropertyType new_property);

  // Immediate application helpers (for setting start values before animation).
  void ApplyPositionImmediate(HWND hwnd,
                              double x,
                              double y,
                              double scale_factor);
  void ApplySizeImmediate(HWND hwnd,
                          double width,
                          double height,
                          double scale_factor);
  void ApplyBoundsImmediate(HWND hwnd,
                            double x,
                            double y,
                            double width,
                            double height,
                            double scale_factor);
  void ApplyOpacityImmediate(HWND hwnd, double opacity);
};

}  // namespace flutter

// ============================================================
// C Export Animation APIs
// ============================================================

extern "C" {
FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_DragWindow(HWND hwnd, int32_t state);

FLUTTER_EXPORT
flutter::ActualWindowBounds InternalFlutterWindows_WindowApi_GetWindowBounds(
    HWND hwnd);

FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_SetBounds(
    HWND hwnd,
    const flutter::WindowBoundsRequest* request);

FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_FocusWindow(HWND hwnd);

FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_SetNoFrame(HWND hwnd);

FLUTTER_EXPORT
bool InternalFlutterWindows_WindowApi_IsAlwaysOnTop(HWND hwnd);

FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_SetAlwaysOnTop(HWND hwnd,
                                                     bool is_always_on_top);

FLUTTER_EXPORT
bool InternalFlutterWindows_WindowApi_IsResizable(HWND hwnd);

FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_SetResizable(HWND hwnd,
                                                   bool is_resizable);

FLUTTER_EXPORT
bool InternalFlutterWindows_WindowApi_IsMinimized(HWND hwnd);

FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_Restore(HWND hwnd);

FLUTTER_EXPORT
bool InternalFlutterWindows_WindowApi_IsSkipTaskbar(HWND hwnd);
FLUTTER_EXPORT void InternalFlutterWindows_WindowApi_SetSkipTaskbar(
    HWND hwnd,
    bool is_skip_taskbar);
FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_CenterWindowOnMonitor(HWND hwnd);
FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_ShowWindow(HWND hwnd, int nCmd);
FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_SetNoSystemMenu(HWND hwnd);
// Starts a position animation. Returns the timer ID, or 0 if failed.
FLUTTER_EXPORT
uint64_t InternalFlutterWindows_WindowApi_StartPositionAnimation(
    HWND hwnd,
    const flutter::PositionAnimationRequest* request);

// Starts a size animation. Returns the timer ID, or 0 if failed.
FLUTTER_EXPORT
uint64_t InternalFlutterWindows_WindowApi_StartSizeAnimation(
    HWND hwnd,
    const flutter::SizeAnimationRequest* request);

// Starts a bounds animation. Returns the timer ID, or 0 if failed.
FLUTTER_EXPORT
uint64_t InternalFlutterWindows_WindowApi_StartBoundsAnimation(
    HWND hwnd,
    const flutter::BoundsAnimationRequest* request);

// Starts an opacity animation. Returns the timer ID, or 0 if failed.
FLUTTER_EXPORT
uint64_t InternalFlutterWindows_WindowApi_StartOpacityAnimation(
    HWND hwnd,
    const flutter::OpacityAnimationRequest* request);

// Stops an animation by timer ID.
FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_StopAnimation(HWND hwnd,
                                                    uint64_t timer_id);

// Stops all animations for a window.
FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_StopAllAnimations(HWND hwnd);

// Checks if a window has active animations.
FLUTTER_EXPORT
bool InternalFlutterWindows_WindowApi_HasActiveAnimation(HWND hwnd);

// Sets the spring parameters for future animations.
FLUTTER_EXPORT
void InternalFlutterWindows_WindowApi_SetSpringParameters(HWND hwnd,
                                                          double damping,
                                                          double stiffness);

}  // extern "C"

#endif  // FLUTTER_SHELL_PLATFORM_WINDOWS_WINDOW_API_H_
