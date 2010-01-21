// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This interface is for managing the global services of the application. Each
// service is lazily created when requested the first time. The service getters
// will return NULL if the service is not available, so callers must check for
// this condition.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "ipc/ipc_message.h"

class AutomationProviderList;
class Clipboard;
class DevToolsManager;
class DownloadRequestManager;
class GoogleURLTracker;
class IntranetRedirectDetector;
class IconManager;
class MetricsService;
class NotificationUIManager;
class PrefService;
class ProfileManager;
class DebuggerWrapper;
class ResourceDispatcherHost;
class SuspendController;
class ThumbnailGenerator;
class WebAppInstallerService;

namespace base {
class Thread;
class WaitableEvent;
}

#if defined(OS_WIN)
namespace sandbox {
class BrokerServices;
}
#endif  // defined(OS_WIN)

namespace printing {
class PrintJobManager;
}

// NOT THREAD SAFE, call only from the main thread.
// These functions shouldn't return NULL unless otherwise noted.
class BrowserProcess {
 public:
  BrowserProcess() {}
  virtual ~BrowserProcess() {}

  // Invoked when the user is logging out/shutting down. When logging off we may
  // not have enough time to do a normal shutdown. This method is invoked prior
  // to normal shutdown and saves any state that must be saved before we are
  // continue shutdown.
  virtual void EndSession() = 0;

  // Services: any of these getters may return NULL
  virtual ResourceDispatcherHost* resource_dispatcher_host() = 0;

  virtual MetricsService* metrics_service() = 0;
  virtual ProfileManager* profile_manager() = 0;
  virtual PrefService* local_state() = 0;
  virtual DebuggerWrapper* debugger_wrapper() = 0;
  virtual DevToolsManager* devtools_manager() = 0;
  virtual Clipboard* clipboard() = 0;

  // Returns the manager for desktop notifications.
  virtual NotificationUIManager* notification_ui_manager() = 0;

  // Returns the thread that we perform I/O coordination on (network requests,
  // communication with renderers, etc.
  // NOTE: You should ONLY use this to pass to IPC or other objects which must
  // need a MessageLoop*.  If you just want to post a task, use
  // ChromeThread::PostTask (or other variants) as they take care of checking
  // that a thread is still alive, race conditions, lifetime differences etc.
  // If you still must use this, need to check the return value for NULL.
  virtual base::Thread* io_thread() = 0;

  // Returns the thread that we perform random file operations on. For code
  // that wants to do I/O operations (not network requests or even file: URL
  // requests), this is the thread to use to avoid blocking the UI thread.
  // It might be nicer to have a thread pool for this kind of thing.
  virtual base::Thread* file_thread() = 0;

  // Returns the thread that is used for database operations such as the web
  // database. History has its own thread since it has much higher traffic.
  virtual base::Thread* db_thread() = 0;

#if defined(USE_X11)
  // Returns the thread that is used to process UI requests in cases where
  // we can't route the request to the UI thread. Note that this thread
  // should only be used by the IO thread and this method is only safe to call
  // from the UI thread so, if you've ended up here, something has gone wrong.
  // This method is only included for uniformity.
  virtual base::Thread* background_x11_thread() = 0;
#endif

#if defined(OS_WIN)
  virtual sandbox::BrokerServices* broker_services() = 0;
  virtual void InitBrokerServices(sandbox::BrokerServices*) = 0;
#endif  // defined(OS_WIN)

  virtual IconManager* icon_manager() = 0;

  virtual ThumbnailGenerator* GetThumbnailGenerator() = 0;

  virtual AutomationProviderList* InitAutomationProviderList() = 0;

  virtual void InitDebuggerWrapper(int port) = 0;

  virtual unsigned int AddRefModule() = 0;
  virtual unsigned int ReleaseModule() = 0;

  virtual bool IsShuttingDown() = 0;

  virtual printing::PrintJobManager* print_job_manager() = 0;

  virtual GoogleURLTracker* google_url_tracker() = 0;
  virtual IntranetRedirectDetector* intranet_redirect_detector() = 0;

  // Returns the locale used by the application.
  virtual const std::string& GetApplicationLocale() = 0;
  virtual void set_application_locale(const std::string& locale) = 0;

  DownloadRequestManager* download_request_manager();

  // Returns an event that is signaled when the browser shutdown.
  virtual base::WaitableEvent* shutdown_event() = 0;

  // Returns a reference to the user-data-dir based profiles vector.
  std::vector<std::wstring>& user_data_dir_profiles() {
    return user_data_dir_profiles_;
  }

  // Trigger an asynchronous check to see if we have the inspector's files on
  // disk.
  virtual void CheckForInspectorFiles() = 0;

  // Return true iff we found the inspector files on disk. It's possible to
  // call this function before we have a definite answer from the disk. In that
  // case, we default to returning true.
  virtual bool have_inspector_files() const = 0;

#if defined(IPC_MESSAGE_LOG_ENABLED)
  // Enable or disable IPC logging for the browser, all processes
  // derived from ChildProcess (plugin etc), and all
  // renderers.
  virtual void SetIPCLoggingEnabled(bool enable) = 0;
#endif

 private:
  // User-data-dir based profiles.
  std::vector<std::wstring> user_data_dir_profiles_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcess);
};

extern BrowserProcess* g_browser_process;

#endif  // CHROME_BROWSER_BROWSER_PROCESS_H_
