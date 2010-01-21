// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NATIVE_WEB_KEYBOARD_EVENT_H_
#define CHROME_COMMON_NATIVE_WEB_KEYBOARD_EVENT_H_

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif  // __OBJC__
#elif defined(OS_POSIX)
typedef struct _GdkEventKey GdkEventKey;
#endif

// Owns a platform specific event; used to pass own and pass event through
// platform independent code.
struct NativeWebKeyboardEvent : public WebKit::WebKeyboardEvent {
  NativeWebKeyboardEvent();

#if defined(OS_WIN)
  NativeWebKeyboardEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
#elif defined(OS_MACOSX)
  explicit NativeWebKeyboardEvent(NSEvent *event);
  NativeWebKeyboardEvent(wchar_t character,
                         int state,
                         double time_stamp_seconds);
#elif defined(TOOLKIT_GTK)
  explicit NativeWebKeyboardEvent(const GdkEventKey* event);
  NativeWebKeyboardEvent(wchar_t character,
                         int state,
                         double time_stamp_seconds);
#endif

  NativeWebKeyboardEvent(const NativeWebKeyboardEvent& event);
  ~NativeWebKeyboardEvent();

  NativeWebKeyboardEvent& operator=(const NativeWebKeyboardEvent& event);

#if defined(OS_WIN)
  MSG os_event;
#elif defined(OS_MACOSX)
  NSEvent* os_event;

  // True if the browser should ignore this event if it's not handled by the
  // renderer. This happens for RawKeyDown events that are created while IME is
  // active and is necessary to prevent backspace from doing "history back" if
  // it is hit in ime mode.
  bool skip_in_browser;
#elif defined(TOOLKIT_GTK)
  GdkEventKey* os_event;
#endif
};

#endif  // CHROME_COMMON_NATIVE_WEB_KEYBOARD_EVENT_H_
