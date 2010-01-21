// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ICON_LOADER_H_
#define CHROME_BROWSER_ICON_LOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"

#if defined(OS_WIN)
// On Windows, we group files by their extension, with several exceptions:
// .dll, .exe, .ico. See IconManager.h for explanation.
typedef std::wstring IconGroupID;
#elif defined(OS_POSIX)
// On POSIX, we group files by MIME type.
typedef std::string IconGroupID;
#endif

class MessageLoop;
class SkBitmap;

////////////////////////////////////////////////////////////////////////////////
//
// A facility to read a file containing an icon asynchronously in the IO
// thread. Returns the icon in the form of an SkBitmap.
//
////////////////////////////////////////////////////////////////////////////////
class IconLoader : public base::RefCountedThreadSafe<IconLoader> {
 public:
  enum IconSize {
    SMALL = 0,  // 16x16
    NORMAL,     // 32x32
    LARGE
  };

  class Delegate {
   public:
    // Invoked when an icon has been read. |source| is the IconLoader. If the
    // icon has been successfully loaded, result is non-null. This method must
    // return true if it is taking ownership of the returned bitmap.
    virtual bool OnBitmapLoaded(IconLoader* source, SkBitmap* result) = 0;

   protected:
    virtual ~Delegate() {}
  };

  IconLoader(const IconGroupID& group, IconSize size, Delegate* delegate);

  // Start reading the icon on the file thread.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<IconLoader>;

  virtual ~IconLoader();

  void ReadIcon();

  void NotifyDelegate();

  // The message loop object of the thread in which we notify the delegate.
  MessageLoop* target_message_loop_;

  IconGroupID group_;

  IconSize icon_size_;

  SkBitmap* bitmap_;

  Delegate* delegate_;

#if defined(USE_X11)
  // On X11 systems we use gdk's pixbuf loader, which has to execute on the UI
  // thread, so we only read the file on the background thread and parse it
  // on the main thread.
  void ParseIcon();
  FilePath filename_;
  std::string icon_data_;
#endif

  DISALLOW_COPY_AND_ASSIGN(IconLoader);
};

#endif  // CHROME_BROWSER_ICON_LOADER_H_
