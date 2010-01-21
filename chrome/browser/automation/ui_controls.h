// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H_
#define CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H_

#include "build/build_config.h"

#include <string>

#if defined(OS_WIN)
#include <wtypes.h>
#endif

#include "app/gfx/native_widget_types.h"
#include "base/gfx/point.h"
#include "base/keyboard_codes.h"

#if defined(TOOLKIT_VIEWS)
namespace views {
class View;
}
#endif

class Task;

namespace ui_controls {

// Many of the functions in this class include a variant that takes a Task.
// The version that takes a Task waits until the generated event is processed.
// Once the generated event is processed the Task is Run (and deleted). Note
// that this is a somewhat fragile process in that any event of the correct
// type (key down, mouse click, etc.) will trigger the Task to be run. Hence
// a usage such as
//
//   SendKeyPress(...);
//   SendKeyPressNotifyWhenDone(..., task);
//
// might trigger |task| early.
//
// Note: Windows does not currently do anything with the |window| argument for
// these functions, so passing NULL is ok.

// Send a key press with/without modifier keys.
bool SendKeyPress(gfx::NativeWindow window,
                  base::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt);
bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                base::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                Task* task);

// Simulate a mouse move. (x,y) are absolute screen coordinates.
bool SendMouseMove(long x, long y);
bool SendMouseMoveNotifyWhenDone(long x, long y, Task* task);

enum MouseButton {
  LEFT = 0,
  MIDDLE,
  RIGHT,
};

// Used to indicate the state of the button when generating events.
enum MouseButtonState {
  UP = 1,
  DOWN = 2
};

// Sends a mouse down and/or up message. The click will be sent to wherever
// the cursor currently is, so be sure to move the cursor before calling this
// (and be sure the cursor has arrived!).
bool SendMouseEvents(MouseButton type, int state);
bool SendMouseEventsNotifyWhenDone(MouseButton type, int state, Task* task);
// Same as SendMouseEvents with UP | DOWN.
bool SendMouseClick(MouseButton type);

// A combination of SendMouseMove to the middle of the view followed by
// SendMouseEvents.
void MoveMouseToCenterAndPress(
#if defined(TOOLKIT_VIEWS)
    views::View* view,
#elif defined(TOOLKIT_GTK)
    GtkWidget* widget,
#endif
    MouseButton button,
    int state,
    Task* task);

}  // ui_controls

#endif  // CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H_
