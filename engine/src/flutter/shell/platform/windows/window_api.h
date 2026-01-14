// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_WINDOWS_WINDOW_API_H_
#define FLUTTER_SHELL_PLATFORM_WINDOWS_WINDOW_API_H_

#include <shobjidl.h>
#include <windows.h>
#include <wrl/client.h>

#include "flutter/shell/geometry/geometry.h"
#include "window_manager.h"

namespace flutter {

class HostWindow;

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
  bool IsMinimized();

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
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_WINDOWS_WINDOW_API_H_
