// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_api_timer.h"
#include "flutter_windows_engine.h"
#include "host_window.h"
#include "window_api.h"

namespace flutter {

WindowApiTimer& WindowApiTimer::GetInstance() {
  static WindowApiTimer instance;
  return instance;
}

WindowApiTimer::WindowApiTimer() {
  // Cache the QPC frequency for high-precision timing.
  QueryPerformanceFrequency(&qpc_frequency_);
}

WindowApiTimer::~WindowApiTimer() {
  // Stop timer if running.
  if (timer_running_.load(std::memory_order_acquire) && timer_) {
    DeleteTimerQueueTimer(timer_queue_, timer_, INVALID_HANDLE_VALUE);
    timer_ = nullptr;
    timer_running_.store(false, std::memory_order_release);
  }

  // Delete timer queue.
  if (timer_queue_) {
    DeleteTimerQueue(timer_queue_);
    timer_queue_ = nullptr;
  }
}

void WindowApiTimer::Initialize(FlutterWindowsEngine* engine) {
  if (initialized_) {
    return;  // Already initialized.
  }

  engine_ = engine;

  // Initialize last tick time.
  LARGE_INTEGER current_time;
  QueryPerformanceCounter(&current_time);
  last_tick_time_ms_ = static_cast<double>(current_time.QuadPart) * 1000.0 /
                       static_cast<double>(qpc_frequency_.QuadPart);

  // Create the timer queue.
  if (!timer_queue_) {
    timer_queue_ = CreateTimerQueue();
    if (!timer_queue_) {
      return;  // Failed to create timer queue.
    }
  }

  // Create timer with high frequency for smooth animations.
  // Using WT_EXECUTEINTIMERTHREAD for lower latency.
  constexpr DWORD kTimerPeriodMs = 1;
  BOOL result =
      CreateTimerQueueTimer(&timer_, timer_queue_, TimerCallback, this,
                            0,               // Due time (start immediately).
                            kTimerPeriodMs,  // Period.
                            WT_EXECUTEINTIMERTHREAD);

  if (result) {
    timer_running_.store(true, std::memory_order_release);
  }

  initialized_ = true;
}

void WindowApiTimer::AddWindowApi(std::shared_ptr<WindowApi> windowApi) {
  if (!windowApi) {
    return;
  }
  std::lock_guard<std::mutex> lock(windowApis_mutex_);
  windowApis_.insert(windowApi);
  windowApis_dirty_.store(true, std::memory_order_release);
}

void WindowApiTimer::RemoveWindowApi(std::shared_ptr<WindowApi> windowApi) {
  if (!windowApi) {
    return;
  }
  std::lock_guard<std::mutex> lock(windowApis_mutex_);
  windowApis_.erase(windowApi);
  windowApis_dirty_.store(true, std::memory_order_release);
}

void WindowApiTimer::ClearWindows() {
  std::lock_guard<std::mutex> lock(windowApis_mutex_);
  windowApis_.clear();
  windowApis_dirty_.store(true, std::memory_order_release);
}

bool WindowApiTimer::HasWindowApi(std::shared_ptr<WindowApi> windowApi) {
  std::lock_guard<std::mutex> lock(windowApis_mutex_);
  return windowApis_.find(windowApi) != windowApis_.end();
}

size_t WindowApiTimer::GetWindowCount() {
  std::lock_guard<std::mutex> lock(windowApis_mutex_);
  return windowApis_.size();
}

VOID CALLBACK WindowApiTimer::TimerCallback(PVOID lpParameter,
                                            BOOLEAN TimerOrWaitFired) {
  WindowApiTimer* self = static_cast<WindowApiTimer*>(lpParameter);
  if (!self) {
    return;
  }

  // Check if timer should be running.
  if (!self->timer_running_.load(std::memory_order_acquire)) {
    return;
  }
  // Process tick.
  self->OnTick();
}

void WindowApiTimer::OnTick() {
  // Calculate current time in milliseconds.
  LARGE_INTEGER current_time;
  QueryPerformanceCounter(&current_time);
  double current_time_ms = static_cast<double>(current_time.QuadPart) * 1000.0 /
                           static_cast<double>(qpc_frequency_.QuadPart);

  // Calculate delta time since last tick.
  double delta_ms = current_time_ms - last_tick_time_ms_;
  last_tick_time_ms_ = current_time_ms;

  // Skip if delta is too large (e.g., first tick or system sleep).
  if (delta_ms > 100.0) {
    delta_ms = 16.0;  // Default to ~60 FPS.
  }

  // Rebuild cache only when window list has changed (dirty flag set).
  if (windowApis_dirty_.load(std::memory_order_acquire)) {
    std::lock_guard<std::mutex> lock(windowApis_mutex_);
    // Double-check after acquiring lock.
    if (windowApis_dirty_.load(std::memory_order_relaxed)) {
      windowApis_cache_.clear();
      windowApis_cache_.reserve(windowApis_.size());
      for (const auto& window : windowApis_) {
        windowApis_cache_.push_back(window);
      }
      windowApis_dirty_.store(false, std::memory_order_release);
    }
  }

  // Tick each window using the cached copy.
  // Note: Cache is safe to iterate without lock since only OnTick modifies it.
  for (const auto& window : windowApis_cache_) {
    if (window) {
      window->OnAnimationTickOnThread(delta_ms);
    }
  }
}

}  // namespace flutter
