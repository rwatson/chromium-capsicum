// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "app/gfx/codec/png_codec.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_temp_dir.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"

namespace {

const char* GetDesktopName() {
#if defined(GOOGLE_CHROME_BUILD)
  return "google-chrome.desktop";
#else  // CHROMIUM_BUILD
  static const char* name = NULL;
  if (!name) {
    // Allow $CHROME_DESKTOP to override the built-in value, so that development
    // versions can set themselves as the default without interfering with
    // non-official, packaged versions using the built-in value.
    name = getenv("CHROME_DESKTOP");
    if (!name)
      name = "chromium-browser.desktop";
  }
  return name;
#endif
}

// Helper to launch xdg scripts. We don't want them to ask any questions on the
// terminal etc.
bool LaunchXdgUtility(const std::vector<std::string>& argv) {
  // xdg-settings internally runs xdg-mime, which uses mv to move newly-created
  // files on top of originals after making changes to them. In the event that
  // the original files are owned by another user (e.g. root, which can happen
  // if they are updated within sudo), mv will prompt the user to confirm if
  // standard input is a terminal (otherwise it just does it). So make sure it's
  // not, to avoid locking everything up waiting for mv.
  int devnull = open("/dev/null", O_RDONLY);
  if (devnull < 0)
    return false;
  base::file_handle_mapping_vector no_stdin;
  no_stdin.push_back(std::make_pair(devnull, STDIN_FILENO));

  base::ProcessHandle handle;
  if (!base::LaunchApp(argv, no_stdin, false, &handle)) {
    close(devnull);
    return false;
  }
  close(devnull);

  int success_code;
  base::WaitForExitCode(handle, &success_code);
  return success_code == EXIT_SUCCESS;
}

bool GetDesktopShortcutTemplate(std::string* output) {
  std::vector<FilePath> search_paths;

  const char* xdg_data_home = getenv("XDG_DATA_HOME");
  if (xdg_data_home)
    search_paths.push_back(FilePath(xdg_data_home));

  const char* xdg_data_dirs = getenv("XDG_DATA_DIRS");
  if (xdg_data_dirs) {
    CStringTokenizer tokenizer(xdg_data_dirs,
                               xdg_data_dirs + strlen(xdg_data_dirs), ":");
    while (tokenizer.GetNext()) {
      FilePath data_dir(tokenizer.token());
      search_paths.push_back(data_dir);
      search_paths.push_back(data_dir.Append("applications"));
    }
  }

  // Add some fallback paths for systems which don't have XDG_DATA_DIRS or have
  // it incomplete.
  search_paths.push_back(FilePath("/usr/share/applications"));
  search_paths.push_back(FilePath("/usr/local/share/applications"));

  std::string template_filename(GetDesktopName());
  for (std::vector<FilePath>::const_iterator i = search_paths.begin();
       i != search_paths.end(); ++i) {
    FilePath path = (*i).Append(template_filename);
    if (file_util::PathExists(path))
      return file_util::ReadFileToString(path, output);
  }

  return false;
}

class CreateDesktopShortcutTask : public Task {
 public:
  explicit CreateDesktopShortcutTask(
      const ShellIntegration::ShortcutInfo& shortcut_info)
      : shortcut_info_(shortcut_info) {
  }

  virtual void Run() {
    // TODO(phajdan.jr): Report errors from this function, possibly as infobars.
    std::string template_contents;
    if (!GetDesktopShortcutTemplate(&template_contents))
      return;

    FilePath shortcut_filename =
        ShellIntegration::GetDesktopShortcutFilename(shortcut_info_.url);
    if (shortcut_filename.empty())
      return;

    std::string icon_name = CreateIcon(shortcut_filename);

    std::string contents = ShellIntegration::GetDesktopFileContents(
        template_contents, shortcut_info_.url, shortcut_info_.title,
        icon_name);

    if (shortcut_info_.create_on_desktop)
      CreateOnDesktop(shortcut_filename, contents);

    if (shortcut_info_.create_in_applications_menu)
      CreateInApplicationsMenu(shortcut_filename, contents);
  }

 private:
  std::string CreateIcon(const FilePath& shortcut_filename) {
    if (shortcut_info_.favicon.isNull())
      return std::string();

    // TODO(phajdan.jr): Report errors from this function, possibly as infobars.
    ScopedTempDir temp_dir;
    if (!temp_dir.CreateUniqueTempDir())
      return std::string();

    FilePath temp_file_path = temp_dir.path().Append(
        shortcut_filename.ReplaceExtension("png"));

    std::vector<unsigned char> png_data;
    gfx::PNGCodec::EncodeBGRASkBitmap(shortcut_info_.favicon, false, &png_data);
    int bytes_written = file_util::WriteFile(temp_file_path,
        reinterpret_cast<char*>(png_data.data()), png_data.size());

    if (bytes_written != static_cast<int>(png_data.size()))
      return std::string();

    std::vector<std::string> argv;
    argv.push_back("xdg-icon-resource");
    argv.push_back("install");

    // Always install in user mode, even if someone runs the browser as root
    // (people do that).
    argv.push_back("--mode");
    argv.push_back("user");

    argv.push_back("--size");
    argv.push_back(IntToString(shortcut_info_.favicon.width()));

    argv.push_back(temp_file_path.value());
    std::string icon_name = temp_file_path.BaseName().RemoveExtension().value();
    argv.push_back(icon_name);
    LaunchXdgUtility(argv);
    return icon_name;
  }

  void CreateOnDesktop(const FilePath& shortcut_filename,
                       const std::string& contents) {
    // TODO(phajdan.jr): Report errors from this function, possibly as infobars.

    // Make sure that we will later call openat in a secure way.
    DCHECK_EQ(shortcut_filename.BaseName().value(), shortcut_filename.value());

    FilePath desktop_path;
    if (!PathService::Get(chrome::DIR_USER_DESKTOP, &desktop_path))
      return;
#if !defined(OS_FREEBSD)
// BSD: Linux-specific calls like openat are used so defined out for BSD.
    int desktop_fd = open(desktop_path.value().c_str(), O_RDONLY | O_DIRECTORY);
    if (desktop_fd < 0)
      return;

    int fd = openat(desktop_fd, shortcut_filename.value().c_str(),
                    O_CREAT | O_EXCL | O_WRONLY,
                    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (fd < 0) {
      HANDLE_EINTR(close(desktop_fd));
      return;
    }

    ssize_t bytes_written = file_util::WriteFileDescriptor(fd, contents.data(),
                                                           contents.length());
    HANDLE_EINTR(close(fd));

    if (bytes_written != static_cast<ssize_t>(contents.length())) {
      // Delete the file. No shortuct is better than corrupted one. Use unlinkat
      // to make sure we're deleting the file in the directory we think we are.
      // Even if an attacker manager to put something other at
      // |shortcut_filename| we'll just undo his action.
      unlinkat(desktop_fd, shortcut_filename.value().c_str(), 0);
    }

    HANDLE_EINTR(close(desktop_fd));
#endif  // !defined(OS_FREEBSD)
  }

  void CreateInApplicationsMenu(const FilePath& shortcut_filename,
                                const std::string& contents) {
    // TODO(phajdan.jr): Report errors from this function, possibly as infobars.
    ScopedTempDir temp_dir;
    if (!temp_dir.CreateUniqueTempDir())
      return;

    FilePath temp_file_path = temp_dir.path().Append(shortcut_filename);

    int bytes_written = file_util::WriteFile(temp_file_path, contents.data(),
                                             contents.length());

    if (bytes_written != static_cast<int>(contents.length()))
      return;

    std::vector<std::string> argv;
    argv.push_back("xdg-desktop-menu");
    argv.push_back("install");

    // Always install in user mode, even if someone runs the browser as root
    // (people do that).
    argv.push_back("--mode");
    argv.push_back("user");

    argv.push_back(temp_file_path.value());
    LaunchXdgUtility(argv);
  }

  const ShellIntegration::ShortcutInfo shortcut_info_;

  DISALLOW_COPY_AND_ASSIGN(CreateDesktopShortcutTask);
};

}  // namespace

// We delegate the difficulty of setting the default browser in Linux desktop
// environments to a new xdg utility, xdg-settings. We have to include a copy of
// it for this to work, obviously, but that's actually the suggested approach
// for xdg utilities anyway.

bool ShellIntegration::SetAsDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("set");
  argv.push_back("default-web-browser");
  argv.push_back(GetDesktopName());
  return LaunchXdgUtility(argv);
}

ShellIntegration::DefaultBrowserState ShellIntegration::IsDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("check");
  argv.push_back("default-web-browser");
  argv.push_back(GetDesktopName());

  std::string reply;
  if (!base::GetAppOutput(CommandLine(argv), &reply)) {
    // xdg-settings failed: we can't determine or set the default browser.
    return UNKNOWN_DEFAULT_BROWSER;
  }

  // Allow any reply that starts with "yes".
  return (reply.find("yes") == 0) ? IS_DEFAULT_BROWSER : NOT_DEFAULT_BROWSER;
}

bool ShellIntegration::IsFirefoxDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("get");
  argv.push_back("default-web-browser");

  std::string browser;
  // We don't care about the return value here.
  base::GetAppOutput(CommandLine(argv), &browser);
  return browser.find("irefox") != std::string::npos;
}

FilePath ShellIntegration::GetDesktopShortcutFilename(const GURL& url) {
  // Use a prefix, because xdg-desktop-menu requires it.
  std::string filename =
      WideToUTF8(chrome::kBrowserProcessExecutableName) + "-" + url.spec();
  file_util::ReplaceIllegalCharactersInPath(&filename, '_');

  FilePath desktop_path;
  if (!PathService::Get(chrome::DIR_USER_DESKTOP, &desktop_path))
    return FilePath();

  FilePath filepath = desktop_path.Append(filename);
  FilePath alternative_filepath(filepath.value() + ".desktop");
  for (size_t i = 1; i < 100; ++i) {
    if (file_util::PathExists(FilePath(alternative_filepath))) {
      alternative_filepath = FilePath(filepath.value() + "_" + IntToString(i) +
                                      ".desktop");
    } else {
      return FilePath(alternative_filepath).BaseName();
    }
  }

  return FilePath();
}

std::string ShellIntegration::GetDesktopFileContents(
    const std::string& template_contents, const GURL& url,
    const string16& title, const std::string& icon_name) {
  // See http://standards.freedesktop.org/desktop-entry-spec/latest/
  // Although not required by the spec, Nautilus on Ubuntu Karmic creates its
  // launchers with an xdg-open shebang. Follow that convention.
  std::string output_buffer("#!/usr/bin/env xdg-open\n");
  StringTokenizer tokenizer(template_contents, "\n");
  while (tokenizer.GetNext()) {
    if (tokenizer.token().substr(0, 5) == "Exec=") {
      std::string exec_path = tokenizer.token().substr(5);
      StringTokenizer exec_tokenizer(exec_path, " ");
      std::string final_path;
      while (exec_tokenizer.GetNext()) {
        if (exec_tokenizer.token() != "%U")
          final_path += exec_tokenizer.token() + " ";
      }
      std::string switches;
      CPB_GetCommandLineArgumentsCommon(url.spec().c_str(), &switches);
      output_buffer += std::string("Exec=") + final_path + switches + "\n";
    } else if (tokenizer.token().substr(0, 5) == "Name=") {
      std::string final_title = UTF16ToUTF8(title);
      // Make sure no endline characters can slip in and possibly introduce
      // additional lines (like Exec, which makes it a security risk). Also
      // use the URL as a default when the title is empty.
      if (final_title.empty() ||
          final_title.find("\n") != std::string::npos ||
          final_title.find("\r") != std::string::npos) {
        final_title = url.spec();
      }
      output_buffer += StringPrintf("Name=%s\n", final_title.c_str());
    } else if (tokenizer.token().substr(0, 11) == "GenericName" ||
               tokenizer.token().substr(0, 7) == "Comment" ||
               tokenizer.token().substr(0, 1) == "#") {
      // Skip comment lines.
    } else if (tokenizer.token().substr(0, 5) == "Icon=" &&
               !icon_name.empty()) {
      output_buffer += StringPrintf("Icon=%s\n", icon_name.c_str());
    } else {
      output_buffer += tokenizer.token() + "\n";
    }
  }
  return output_buffer;
}

void ShellIntegration::CreateDesktopShortcut(
    const ShortcutInfo& shortcut_info) {
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      new CreateDesktopShortcutTask(shortcut_info));
}
