// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_WINDOWS_WINDOW_API_TIMER_H_
#define FLUTTER_SHELL_PLATFORM_WINDOWS_WINDOW_API_TIMER_H_

#include <windows.h>
#include <atomic>
#include <mutex>
#include <set>

#include "window_api.h"

namespace flutter {

class FlutterWindowsEngine;
class HostWindow;

// Unified timer manager for window animations.
// This class manages a single timer queue that serves all windows,
// calling their OnAnimationTick method at ~60 FPS when active.
//
// Thread safety:
// - All window operations run on main thread via PostTask
// - AddWindow/RemoveWindow are called from main thread
class WindowApiTimer {
 public:
  // Returns the singleton instance.
  static WindowApiTimer& GetInstance();

  // Adds a window to receive animation ticks.
  // The timer will start automatically if not already running.
  void AddWindowApi(std::shared_ptr<WindowApi> windowApi);

  // Removes a window from receiving animation ticks.
  // The timer will stop automatically when no windows remain.
  void RemoveWindowApi(std::shared_ptr<WindowApi> windowApi);

  void ClearWindows();

  // Checks if a window is currently registered.
  bool HasWindowApi(std::shared_ptr<WindowApi> window);

  // Returns the number of registered windows.
  size_t GetWindowCount();

  // Initializes the timer with the Flutter engine.
  // This must be called before using the timer.
  void Initialize(FlutterWindowsEngine* engine);

  // Gets the Flutter engine pointer.
  FlutterWindowsEngine* GetEngine() const { return engine_; }

  // Checks if the timer has been initialized.
  bool IsInitialized() const { return initialized_; }

 private:
  WindowApiTimer();
  ~WindowApiTimer();

  // Non-copyable and non-movable.
  WindowApiTimer(const WindowApiTimer&) = delete;
  WindowApiTimer& operator=(const WindowApiTimer&) = delete;

  // Timer callback function.
  static VOID CALLBACK TimerCallback(PVOID lpParameter,
                                     BOOLEAN TimerOrWaitFired);

  // Called by the timer callback to process all registered windows.
  void OnTick();

  // Registered windows that need animation ticks.
  std::set<std::shared_ptr<WindowApi>> windowApis_;

  // Cached copy of windows for iteration (avoids allocation on each tick).
  std::vector<std::shared_ptr<WindowApi>> windowApis_cache_;

  // Flag indicating if windowApis_cache_ needs to be rebuilt.
  std::atomic<bool> windowApis_dirty_{true};

  // Mutex for protecting windowApis_ access from multiple threads.
  mutable std::mutex windowApis_mutex_;

  // Timer queue handle (created once, kept alive).
  HANDLE timer_queue_ = nullptr;

  // Timer handle (created/destroyed as needed).
  HANDLE timer_ = nullptr;

  // Flag indicating if timer is running.
  std::atomic<bool> timer_running_{false};

  // Cached QPC frequency for high-precision timing.
  LARGE_INTEGER qpc_frequency_ = {0};

  // Last tick time for calculating delta.
  double last_tick_time_ms_ = 0;

  // Flutter engine pointer for accessing engine functionality.
  FlutterWindowsEngine* engine_ = nullptr;

  // Flag indicating if Initialize has been called.
  bool initialized_ = false;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_WINDOWS_WINDOW_API_TIMER_H_
