// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines various defaults whose values varies depending upon the OS.

#ifndef CHROME_BROWSER_DEFAULTS_H_
#define CHROME_BROWSER_DEFAULTS_H_

#include "build/build_config.h"
#include "chrome/browser/session_startup_pref.h"

namespace browser_defaults {

#if defined(OS_NIX)

// Size of the font in pixels used in the autocomplete box for normal windows.
extern const double kAutocompleteEditFontPixelSize;

// Size of the font in pixels used in the autocomplete box for popup windows.
extern const double kAutocompleteEditFontPixelSizeInPopup;

// Size of the font used in the autocomplete popup.
extern const int kAutocompletePopupFontSize;

// Can the user toggle the system title bar?
extern const bool kCanToggleSystemTitleBar;

#endif

// The default value for session startup.
extern const SessionStartupPref::Type kDefaultSessionStartupType;

// Width of pinned tabs.
extern const int kPinnedTabWidth;

// Should session restore restore popup windows?
extern const bool kRestorePopups;

// Can the browser be alive without any browser windows?
extern const bool kBrowserAliveWithNoWindows;

// Should a link be shown on the bookmark bar allowing the user to import
// bookmarks?
extern const bool kShowImportOnBookmarkBar;

// Should the exit menu item be shown in the toolbar menu?
extern const bool kShowExitMenuItem;

// Should the about menu item be shown in the toolbar app menu?
extern const bool kShowAboutMenuItem;

// Does the OS support other browsers? If not, operations such as default
// browser check are not done.
extern const bool kOSSupportsOtherBrowsers;

// Does the download page have the show in folder option?
extern const bool kDownloadPageHasShowInFolder;

}  // namespace browser_defaults

#endif  // CHROME_BROWSER_DEFAULTS_H_
