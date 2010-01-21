// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/in_process_browser_test.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#if defined(OS_WIN)
#include "chrome/browser/views/frame/browser_view.h"
#endif
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"
#include "sandbox/src/dep.h"

#if defined(OS_NIX)
#include "base/singleton.h"
#include "chrome/browser/renderer_host/render_sandbox_host_linux.h"
#include "chrome/browser/zygote_host_linux.h"

namespace {

// A helper class to do Linux-only initialization only once per process.
class LinuxHostInit {
 public:
  LinuxHostInit() {
    RenderSandboxHostLinux* shost = Singleton<RenderSandboxHostLinux>::get();
    shost->Init("");
    ZygoteHost* zhost = Singleton<ZygoteHost>::get();
    zhost->Init("");
  }
  ~LinuxHostInit() {}
};

}  // namespace
#endif

extern int BrowserMain(const MainFunctionParams&);

const wchar_t kUnitTestShowWindows[] = L"show-windows";

// Default delay for the time-out at which we stop the
// inner-message loop the first time.
const int kInitialTimeoutInMS = 30000;

// Delay for sub-sequent time-outs once the initial time-out happened.
const int kSubsequentTimeoutInMS = 5000;

InProcessBrowserTest::InProcessBrowserTest()
    : browser_(NULL),
      show_window_(false),
      dom_automation_enabled_(false),
      single_process_(false),
      original_single_process_(false),
      initial_timeout_(kInitialTimeoutInMS) {
}

InProcessBrowserTest::~InProcessBrowserTest() {
}

void InProcessBrowserTest::SetUp() {
  // Cleanup the user data dir.
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ASSERT_LT(10, static_cast<int>(user_data_dir.value().size())) <<
      "The user data directory name passed into this test was too "
      "short to delete safely.  Please check the user-data-dir "
      "argument and try again.";
  if (ShouldDeleteProfile())
    ASSERT_TRUE(file_util::DieFileDie(user_data_dir, true));

  // The unit test suite creates a testingbrowser, but we want the real thing.
  // Delete the current one. We'll install the testing one in TearDown.
  delete g_browser_process;

  // Don't delete the resources when BrowserMain returns. Many ui classes
  // cache SkBitmaps in a static field so that if we delete the resource
  // bundle we'll crash.
  browser_shutdown::delete_resources_on_shutdown = false;

  CommandLine* command_line = CommandLine::ForCurrentProcessMutable();
  original_command_line_.reset(new CommandLine(*command_line));

  SetUpCommandLine(command_line);

#if defined(OS_WIN)
  // Hide windows on show.
  if (!command_line->HasSwitch(kUnitTestShowWindows) && !show_window_)
    BrowserView::SetShowState(SW_HIDE);
#endif

  if (dom_automation_enabled_)
    command_line->AppendSwitch(switches::kDomAutomationController);

  if (single_process_)
    command_line->AppendSwitch(switches::kSingleProcess);

  // Turn off tip loading for tests; see http://crbug.com/17725
  command_line->AppendSwitch(switches::kDisableWebResources);

  command_line->AppendSwitchWithValue(switches::kUserDataDir,
                                      user_data_dir.ToWStringHack());

  // For some reason the sandbox wasn't happy running in test mode. These
  // tests aren't intended to test the sandbox, so we turn it off.
  command_line->AppendSwitch(switches::kNoSandbox);

  // Don't show the first run ui.
  command_line->AppendSwitch(switches::kNoFirstRun);

  // Single-process mode is not set in BrowserMain so it needs to be processed
  // explicitlty.
  original_single_process_ = RenderProcessHost::run_renderer_in_process();
  if (command_line->HasSwitch(switches::kSingleProcess))
    RenderProcessHost::set_run_renderer_in_process(true);

  // Explicitly set the path of the exe used for the renderer and plugin,
  // otherwise they'll try to use unit_test.exe.
  FilePath subprocess_path;
  PathService::Get(base::FILE_EXE, &subprocess_path);
  subprocess_path = subprocess_path.DirName();
  subprocess_path = subprocess_path.AppendASCII(WideToASCII(
      chrome::kBrowserProcessExecutablePath));
  command_line->AppendSwitchWithValue(switches::kBrowserSubprocessPath,
                                      subprocess_path.ToWStringHack());

  // Enable warning level logging so that we can see when bad stuff happens.
  command_line->AppendSwitch(switches::kEnableLogging);
  command_line->AppendSwitchWithValue(switches::kLoggingLevel,
                                      IntToWString(1));  // warning

  SandboxInitWrapper sandbox_wrapper;
  MainFunctionParams params(*command_line, sandbox_wrapper, NULL);
  params.ui_task =
      NewRunnableMethod(this, &InProcessBrowserTest::RunTestOnMainThreadLoop);

  host_resolver_ = new net::RuleBasedHostResolverProc(
      new IntranetRedirectHostResolverProc(NULL));

  // Something inside the browser does this lookup implicitly. Make it fail
  // to avoid external dependency. It won't break the tests.
  host_resolver_->AddSimulatedFailure("*.google.com");

  // See http://en.wikipedia.org/wiki/Web_Proxy_Autodiscovery_Protocol
  // We don't want the test code to use it.
  host_resolver_->AddSimulatedFailure("wpad");

  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc(
      host_resolver_.get());

  SetUpInProcessBrowserTestFixture();
  BrowserMain(params);
  TearDownInProcessBrowserTestFixture();
}

void InProcessBrowserTest::TearDown() {
  // Reinstall testing browser process.
  delete g_browser_process;
  g_browser_process = new TestingBrowserProcess();

  browser_shutdown::delete_resources_on_shutdown = true;

#if defined(WIN)
  BrowserView::SetShowState(-1);
#endif

  *CommandLine::ForCurrentProcessMutable() = *original_command_line_;
  RenderProcessHost::set_run_renderer_in_process(original_single_process_);
}

HTTPTestServer* InProcessBrowserTest::StartHTTPServer() {
  // The HTTPServer must run on the IO thread.
  DCHECK(!http_server_.get());
  http_server_ = HTTPTestServer::CreateServer(
      L"chrome/test/data",
      g_browser_process->io_thread()->message_loop());
  return http_server_.get();
}

// Creates a browser with a single tab (about:blank), waits for the tab to
// finish loading and shows the browser.
Browser* InProcessBrowserTest::CreateBrowser(Profile* profile) {
  Browser* browser = Browser::Create(profile);

  browser->AddTabWithURL(GURL(chrome::kAboutBlankURL), GURL(),
                         PageTransition::START_PAGE, true, -1, false, NULL);

  // Wait for the page to finish loading.
  ui_test_utils::WaitForNavigation(
      &browser->GetSelectedTabContents()->controller());

  browser->window()->Show();

  return browser;
}

void InProcessBrowserTest::RunTestOnMainThreadLoop() {
  // In the long term it would be great if we could use a TestingProfile
  // here and only enable services you want tested, but that requires all
  // consumers of Profile to handle NULL services.
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  if (!profile) {
    // We should only be able to get here if the profile already exists and
    // has been created.
    NOTREACHED();
    MessageLoopForUI::current()->Quit();
    return;
  }


  // Before we run the browser, we have to hack the path to the exe to match
  // what it would be if Chrome was running, because it is used to fork renderer
  // processes, on Linux at least (failure to do so will cause a browser_test to
  // be run instead of a renderer).
  FilePath chrome_path;
  CHECK(PathService::Get(base::FILE_EXE, &chrome_path));
  chrome_path = chrome_path.DirName();
#if defined(OS_WIN)
  chrome_path = chrome_path.Append(chrome::kBrowserProcessExecutablePath);
#elif defined(OS_POSIX)
  chrome_path = chrome_path.Append(
      WideToASCII(chrome::kBrowserProcessExecutablePath));
#endif
  CHECK(PathService::Override(base::FILE_EXE, chrome_path));

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(chrome_browser_net::SetUrlRequestMocksEnabled, true));

#if defined(OS_NIX)
  // Initialize the RenderSandbox and Zygote hosts. Apparently they get used
  // for InProcessBrowserTest, and this is not the normal browser startup path.
  Singleton<LinuxHostInit>::get();
#endif

  browser_ = CreateBrowser(profile);

  // Start the timeout timer to prevent hangs.
  MessageLoopForUI::current()->PostDelayedTask(FROM_HERE,
      NewRunnableMethod(this, &InProcessBrowserTest::TimedOut),
      initial_timeout_);

  RunTestOnMainThread();
  CleanUpOnMainThread();

  // Close all browser windows.  This might not happen immediately, since some
  // may need to wait for beforeunload and unload handlers to fire in a tab.
  // When all windows are closed, the last window will call Quit().
#if defined(OS_MACOSX)
  // When the browser window closes, Cocoa will generate an inner-loop that
  // processes the RenderProcessHost delete task, so allow task nesting.
  bool old_state = MessageLoopForUI::current()->NestableTasksAllowed();
  MessageLoopForUI::current()->SetNestableTasksAllowed(true);
#endif
  BrowserList::const_iterator browser = BrowserList::begin();
  for (; browser != BrowserList::end(); ++browser)
    (*browser)->CloseWindow();
#if defined(OS_MACOSX)
  MessageLoopForUI::current()->SetNestableTasksAllowed(old_state);
#endif

  // Stop the HTTP server.
  http_server_ = NULL;
}

void InProcessBrowserTest::TimedOut() {
  DCHECK(MessageLoopForUI::current()->IsNested());

  std::string error_message = "Test timed out. Each test runs for a max of ";
  error_message += IntToString(kInitialTimeoutInMS);
  error_message += " ms (kInitialTimeoutInMS).";

  GTEST_NONFATAL_FAILURE_(error_message.c_str());

  // Start the timeout timer to prevent hangs.
  MessageLoopForUI::current()->PostDelayedTask(FROM_HERE,
      NewRunnableMethod(this, &InProcessBrowserTest::TimedOut),
      kSubsequentTimeoutInMS);

  MessageLoopForUI::current()->Quit();
}

void InProcessBrowserTest::SetInitialTimeoutInMS(int timeout_value) {
  DCHECK_GT(timeout_value, 0);
  initial_timeout_ = timeout_value;
}
