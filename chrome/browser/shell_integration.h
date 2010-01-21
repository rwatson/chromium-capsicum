// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_H__
#define CHROME_BROWSER_SHELL_INTEGRATION_H__

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

class FilePath;

class ShellIntegration {
 public:
  // Sets Chrome as default browser (only for current user). Returns false if
  // this operation fails.
  static bool SetAsDefaultBrowser();

  // On Linux, it may not be possible to determine or set the default browser
  // on some desktop environments or configurations. So, we use this enum and
  // not a plain bool. (Note however that if used like a bool, this enum will
  // have reasonable behavior.)
  enum DefaultBrowserState {
    NOT_DEFAULT_BROWSER = 0,
    IS_DEFAULT_BROWSER,
    UNKNOWN_DEFAULT_BROWSER
  };

  // Attempt to determine if this instance of Chrome is the default browser and
  // return the appropriate state. (Defined as being the handler for HTTP/HTTPS
  // protocols; we don't want to report "no" here if the user has simply chosen
  // to open HTML files in a text editor and FTP links with an FTP client.)
  static DefaultBrowserState IsDefaultBrowser();

  // Returns true if Firefox is likely to be the default browser for the current
  // user. This method is very fast so it can be invoked in the UI thread.
  static bool IsFirefoxDefaultBrowser();

  struct ShortcutInfo {
    GURL url;
    string16 title;
    string16 description;
    SkBitmap favicon;

    bool create_on_desktop;
    bool create_in_applications_menu;

    // For Windows, this refers to quick launch bar prior to Win7. In Win7,
    // this means "pin to taskbar". For Mac/Linux, this could be used for
    // Mac dock or the gnome/kde application launcher. However, those are not
    // implemented yet.
    bool create_in_quick_launch_bar;
  };

#if defined(USE_X11)
  // Returns filename for .desktop file based on |url|, sanitized for security.
  static FilePath GetDesktopShortcutFilename(const GURL& url);

  // Returns contents for .desktop file based on |template_contents|, |url|
  // and |title|. The |template_contents| should be contents of .desktop file
  // used to launch Chrome.
  static std::string GetDesktopFileContents(
      const std::string& template_contents, const GURL& url,
      const string16& title, const std::string& icon_name);

  // Creates a desktop shortcut. It is not guaranteed to exist immediately after
  // returning from this function, because actual file operation is done on the
  // file thread.
  static void CreateDesktopShortcut(const ShortcutInfo& shortcut_info);
#endif  // defined(USE_X11)

#if defined(OS_WIN)
  // Generates Win7 app id for given app name and profile path. The returned app
  // id is in the format of "|app_name|[.<profile_id>]". "profile_id" is
  // appended when user override the default value.
  static std::wstring GetAppId(const wchar_t* app_name,
                               const FilePath& profile_path);

  // Generates Win7 app id for Chromium by calling GetAppId with
  // chrome::kBrowserAppID as app_name.
  static std::wstring GetChromiumAppId(const FilePath& profile_path);
#endif  // defined(OS_WIN)

  // The current default browser UI state
  enum DefaultBrowserUIState {
    STATE_PROCESSING,
    STATE_NOT_DEFAULT,
    STATE_IS_DEFAULT,
    STATE_UNKNOWN
  };

  class DefaultBrowserObserver {
   public:
    // Updates the UI state to reflect the current default browser state.
    virtual void SetDefaultBrowserUIState(DefaultBrowserUIState state) = 0;
    virtual ~DefaultBrowserObserver() {}
  };
  //  A helper object that handles checking if Chrome is the default browser on
  //  Windows and also setting it as the default browser. These operations are
  //  performed asynchronously on the file thread since registry access is
  //  involved and this can be slow.
  //
  class DefaultBrowserWorker
      : public base::RefCountedThreadSafe<DefaultBrowserWorker> {
   public:
    explicit DefaultBrowserWorker(DefaultBrowserObserver* observer);

    // Checks if Chrome is the default browser.
    void StartCheckDefaultBrowser();

    // Sets Chrome as the default browser.
    void StartSetAsDefaultBrowser();

    // Called to notify the worker that the view is gone.
    void ObserverDestroyed();

   private:
    friend class base::RefCountedThreadSafe<DefaultBrowserWorker>;

    virtual ~DefaultBrowserWorker() {}

    // Functions that track the process of checking if Chrome is the default
    // browser.  |ExecuteCheckDefaultBrowser| checks the registry on the file
    // thread.  |CompleteCheckDefaultBrowser| notifies the view to update on the
    // UI thread.
    void ExecuteCheckDefaultBrowser();
    void CompleteCheckDefaultBrowser(DefaultBrowserState state);

    // Functions that track the process of setting Chrome as the default
    // browser.  |ExecuteSetAsDefaultBrowser| updates the registry on the file
    // thread.  |CompleteSetAsDefaultBrowser| notifies the view to update on the
    // UI thread.
    void ExecuteSetAsDefaultBrowser();
    void CompleteSetAsDefaultBrowser();

    // Updates the UI in our associated view with the current default browser
    // state.
    void UpdateUI(DefaultBrowserState state);

    DefaultBrowserObserver* observer_;

    DISALLOW_COPY_AND_ASSIGN(DefaultBrowserWorker);
  };
};

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_H__
