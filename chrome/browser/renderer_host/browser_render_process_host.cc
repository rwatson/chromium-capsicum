// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "chrome/browser/renderer_host/browser_render_process_host.h"

#include <algorithm>
#include <limits>
#include <vector>

#if defined(OS_POSIX)
#include <utility>  // for pair<>
#endif

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/field_trial.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/spellcheck_host.h"
#include "chrome/browser/visitedlink_master.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/result_codes.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_thread.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_switches.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

using WebKit::WebCache;

#include "third_party/skia/include/core/SkBitmap.h"


// This class creates the IO thread for the renderer when running in
// single-process mode.  It's not used in multi-process mode.
class RendererMainThread : public base::Thread {
 public:
  explicit RendererMainThread(const std::string& channel_id)
      : base::Thread("Chrome_InProcRendererThread"),
        channel_id_(channel_id),
        render_process_(NULL) {
  }

  ~RendererMainThread() {
    Stop();
  }

 protected:
  virtual void Init() {
#if defined(OS_WIN)
    CoInitialize(NULL);
#endif

    render_process_ = new RenderProcess();
    render_process_->set_main_thread(new RenderThread(channel_id_));
    // It's a little lame to manually set this flag.  But the single process
    // RendererThread will receive the WM_QUIT.  We don't need to assert on
    // this thread, so just force the flag manually.
    // If we want to avoid this, we could create the InProcRendererThread
    // directly with _beginthreadex() rather than using the Thread class.
    base::Thread::SetThreadWasQuitProperly(true);
  }

  virtual void CleanUp() {
    delete render_process_;

#if defined(OS_WIN)
    CoUninitialize();
#endif
  }

 private:
  std::string channel_id_;
  // Deleted in CleanUp() on the renderer thread, so don't use a smart pointer.
  RenderProcess* render_process_;
};


// Size of the buffer after which individual link updates deemed not warranted
// and the overall update should be used instead.
static const unsigned kVisitedLinkBufferThreshold = 50;

// This class manages buffering and sending visited link hashes (fingerprints)
// to renderer based on widget visibility.
// As opposed to the VisitedLinkEventListener in profile.cc, which coalesces to
// reduce the rate of messages being sent to render processes, this class
// ensures that the updates occur only when explicitly requested. This is
// used by BrowserRenderProcessHost to only send Add/Reset link events to the
// renderers when their tabs are visible and the corresponding RenderViews are
// created.
class VisitedLinkUpdater {
 public:
  VisitedLinkUpdater() : reset_needed_(false), has_receiver_(false) {}

  // Buffers |links| to update, but doesn't actually relay them.
  void AddLinks(const VisitedLinkCommon::Fingerprints& links) {
    if (reset_needed_)
      return;

    if (pending_.size() + links.size() > kVisitedLinkBufferThreshold) {
      // Once the threshold is reached, there's no need to store pending visited
      // link updates -- we opt for resetting the state for all links.
      AddReset();
      return;
    }

    pending_.insert(pending_.end(), links.begin(), links.end());
  }

  // Tells the updater that sending individual link updates is no longer
  // necessary and the visited state for all links should be reset.
  void AddReset() {
    reset_needed_ = true;
    pending_.clear();
  }

  // Sends visited link update messages: a list of links whose visited state
  // changed or reset of visited state for all links.
  void Update(IPC::Channel::Sender* sender) {
    DCHECK(sender);

    if (!has_receiver_)
      return;

    if (reset_needed_) {
      sender->Send(new ViewMsg_VisitedLink_Reset());
      reset_needed_ = false;
      return;
    }

    if (pending_.size() == 0)
      return;

    sender->Send(new ViewMsg_VisitedLink_Add(pending_));

    pending_.clear();
  }

  // Notifies the updater that it is now safe to send visited state updates.
  void ReceiverReady(IPC::Channel::Sender* sender) {
    has_receiver_ = true;
    // Go ahead and send whatever we already have buffered up.
    Update(sender);
  }

 private:
  bool reset_needed_;
  bool has_receiver_;
  VisitedLinkCommon::Fingerprints pending_;
};

BrowserRenderProcessHost::BrowserRenderProcessHost(Profile* profile)
    : RenderProcessHost(profile),
      visible_widgets_(0),
      backgrounded_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(cached_dibs_cleaner_(
            base::TimeDelta::FromSeconds(5),
            this, &BrowserRenderProcessHost::ClearTransportDIBCache)),
      extension_process_(false) {
  widget_helper_ = new RenderWidgetHelper();

  registrar_.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                 Source<Profile>(profile));
  registrar_.Add(this, NotificationType::SPELLCHECK_HOST_REINITIALIZED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::SPELLCHECK_WORD_ADDED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::SPELLCHECK_AUTOSPELL_TOGGLED,
                 NotificationService::AllSources());

  visited_link_updater_.reset(new VisitedLinkUpdater());

  WebCacheManager::GetInstance()->Add(id());
  ChildProcessSecurityPolicy::GetInstance()->Add(id());

  // Note: When we create the BrowserRenderProcessHost, it's technically
  //       backgrounded, because it has no visible listeners.  But the process
  //       doesn't actually exist yet, so we'll Background it later, after
  //       creation.
}

BrowserRenderProcessHost::~BrowserRenderProcessHost() {
  WebCacheManager::GetInstance()->Remove(id());
  ChildProcessSecurityPolicy::GetInstance()->Remove(id());

  // We may have some unsent messages at this point, but that's OK.
  channel_.reset();
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  // Destroy the AudioRendererHost properly.
  if (audio_renderer_host_.get())
    audio_renderer_host_->Destroy();

  ClearTransportDIBCache();

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_PORT_DELETED_DEBUG,
      Source<IPC::Message::Sender>(this),
      NotificationService::NoDetails());
}

bool BrowserRenderProcessHost::Init(bool is_extensions_process,
                                    URLRequestContextGetter* request_context) {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (channel_.get()) {
    // Ensure that |is_extensions_process| doesn't change across multiple calls
    // to Init().
    if (!run_renderer_in_process()) {
      DCHECK_EQ(extension_process_, is_extensions_process);
    }
    return true;
  }

  extension_process_ = is_extensions_process;

  // run the IPC channel on the shared IO thread.
  base::Thread* io_thread = g_browser_process->io_thread();

  // Construct the AudioRendererHost with the IO thread.
  audio_renderer_host_ = new AudioRendererHost();

  scoped_refptr<ResourceMessageFilter> resource_message_filter =
      new ResourceMessageFilter(g_browser_process->resource_dispatcher_host(),
                                id(),
                                audio_renderer_host_.get(),
                                PluginService::GetInstance(),
                                g_browser_process->print_job_manager(),
                                profile(),
                                widget_helper_,
                                request_context);

  // Find the renderer before creating the channel so if this fails early we
  // return without creating the channel.
  FilePath renderer_path = ChildProcessHost::GetChildPath();
  if (renderer_path.empty())
    return false;

  // Setup the IPC channel.
  const std::string channel_id =
      ChildProcessInfo::GenerateRandomChannelID(this);
  channel_.reset(
      new IPC::SyncChannel(channel_id, IPC::Channel::MODE_SERVER, this,
                           resource_message_filter,
                           io_thread->message_loop(), true,
                           g_browser_process->shutdown_event()));
  // As a preventive mesure, we DCHECK if someone sends a synchronous message
  // with no time-out, which in the context of the browser process we should not
  // be doing.
  channel_->set_sync_messages_with_no_timeout_allowed(false);

  if (run_renderer_in_process()) {
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  Since the primordial
    // thread in the renderer process runs the WebKit code and can sometimes
    // make blocking calls to the UI thread (i.e. this thread), they need to run
    // on separate threads.
    in_process_renderer_.reset(new RendererMainThread(channel_id));

    base::Thread::Options options;
#if !defined(USE_X11)
    // In-process plugins require this to be a UI message loop.
    options.message_loop_type = MessageLoop::TYPE_UI;
#else
    // We can't have multiple UI loops on X, so we don't support
    // in-process plugins.
    options.message_loop_type = MessageLoop::TYPE_DEFAULT;
#endif
    in_process_renderer_->StartWithOptions(options);

    OnProcessLaunched();  // Fake a callback that the process is ready.
  } else {
    // Build command line for renderer.  We call AppendRendererCommandLine()
    // first so the process type argument will appear first.
    CommandLine* cmd_line = new CommandLine(renderer_path);
    AppendRendererCommandLine(cmd_line);
    cmd_line->AppendSwitchWithValue(switches::kProcessChannelID,
                                    ASCIIToWide(channel_id));

    // Spawn the child process asynchronously to avoid blocking the UI thread.
    child_process_.reset(new ChildProcessLauncher(
#if defined(OS_WIN)
        FilePath(),
#elif defined(POSIX)
        base::environment_vector(),
        channel_->GetClientFileDescriptor(),
#endif
        cmd_line,
        this));

    fast_shutdown_started_ = false;
  }

  return true;
}

int BrowserRenderProcessHost::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void BrowserRenderProcessHost::CancelResourceRequests(int render_widget_id) {
  widget_helper_->CancelResourceRequests(render_widget_id);
}

void BrowserRenderProcessHost::CrossSiteClosePageACK(
    const ViewMsg_ClosePage_Params& params) {
  widget_helper_->CrossSiteClosePageACK(params);
}

bool BrowserRenderProcessHost::WaitForUpdateMsg(
    int render_widget_id,
    const base::TimeDelta& max_delay,
    IPC::Message* msg) {
  // The post task to this thread with the process id could be in queue, and we
  // don't want to dispatch a message before then since it will need the handle.
  if (child_process_.get() && child_process_->IsStarting())
    return false;

  return widget_helper_->WaitForUpdateMsg(render_widget_id, max_delay, msg);
}

void BrowserRenderProcessHost::ReceivedBadMessage(uint32 msg_type) {
  BadMessageTerminateProcess(msg_type, GetHandle());
}

void BrowserRenderProcessHost::ViewCreated() {
  visited_link_updater_->ReceiverReady(this);
}

void BrowserRenderProcessHost::WidgetRestored() {
  // Verify we were properly backgrounded.
  DCHECK_EQ(backgrounded_, (visible_widgets_ == 0));
  visible_widgets_++;
  visited_link_updater_->Update(this);
  SetBackgrounded(false);
}

void BrowserRenderProcessHost::WidgetHidden() {
  // On startup, the browser will call Hide
  if (backgrounded_)
    return;

  DCHECK_EQ(backgrounded_, (visible_widgets_ == 0));
  visible_widgets_--;
  DCHECK_GE(visible_widgets_, 0);
  if (visible_widgets_ == 0) {
    DCHECK(!backgrounded_);
    SetBackgrounded(true);
  }
}

void BrowserRenderProcessHost::SendVisitedLinkTable(
    base::SharedMemory* table_memory) {
  // Check if the process is still starting and we don't have a handle for it
  // yet, in which case this will happen later when InitVisitedLinks is called.
  if (!run_renderer_in_process() &&
      (!child_process_.get() || child_process_->IsStarting())) {
    return;
  }

  base::SharedMemoryHandle handle_for_process;
  table_memory->ShareToProcess(GetHandle(), &handle_for_process);
  if (base::SharedMemory::IsHandleValid(handle_for_process))
    Send(new ViewMsg_VisitedLink_NewTable(handle_for_process));
}

void BrowserRenderProcessHost::AddVisitedLinks(
    const VisitedLinkCommon::Fingerprints& links) {
  visited_link_updater_->AddLinks(links);
  if (visible_widgets_ == 0)
    return;

  visited_link_updater_->Update(this);
}

void BrowserRenderProcessHost::ResetVisitedLinks() {
  visited_link_updater_->AddReset();
  if (visible_widgets_ == 0)
    return;

  visited_link_updater_->Update(this);
}

void BrowserRenderProcessHost::AppendRendererCommandLine(
    CommandLine* command_line) const {
  // Pass the process type first, so it shows first in process listings.
  // Extensions use a special pseudo-process type to make them distinguishable,
  // even though they're just renderers.
  command_line->AppendSwitchWithValue(switches::kProcessType,
      extension_process_ ? switches::kExtensionProcess :
                           switches::kRendererProcess);

  if (logging::DialogsAreSuppressed())
    command_line->AppendSwitch(switches::kNoErrorDialogs);

  // Now send any options from our own command line we want to propogate.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  PropogateBrowserCommandLineToRenderer(browser_command_line, command_line);

  // Pass on the browser locale.
  const std::string locale = g_browser_process->GetApplicationLocale();
  command_line->AppendSwitchWithValue(switches::kLang, ASCIIToWide(locale));

  // If we run FieldTrials, we want to pass to their state to the renderer so
  // that it can act in accordance with each state, or record histograms
  // relating to the FieldTrial states.
  std::string field_trial_states;
  FieldTrialList::StatesToString(&field_trial_states);
  if (!field_trial_states.empty()) {
    command_line->AppendSwitchWithValue(switches::kForceFieldTestNameAndValue,
        field_trial_states);
  }

  // A command prefix is something prepended to the command line of the spawned
  // process. It is supported only on POSIX systems.
#if defined(OS_POSIX)
  if (browser_command_line.HasSwitch(switches::kRendererCmdPrefix)) {
    // launch the renderer child with some prefix (usually "gdb --args")
    const std::wstring prefix =
        browser_command_line.GetSwitchValue(switches::kRendererCmdPrefix);
    command_line->PrependWrapper(prefix);
  }
#endif  // defined(OS_POSIX)

  ChildProcessHost::SetCrashReporterCommandLine(command_line);

  const std::string user_data_dir =
      browser_command_line.GetSwitchValueASCII(switches::kUserDataDir);
  if (!user_data_dir.empty())
    command_line->AppendSwitchWithValue(switches::kUserDataDir, user_data_dir);
#if defined(OS_CHROMEOS)
  const std::string& profile =
      browser_command_line.GetSwitchValueASCII(switches::kProfile);
  if (!profile.empty())
    command_line->AppendSwitchWithValue(switches::kProfile, profile);
#endif
}

void BrowserRenderProcessHost::PropogateBrowserCommandLineToRenderer(
    const CommandLine& browser_cmd,
    CommandLine* renderer_cmd) const {
  // Propagate the following switches to the renderer command line (along
  // with any associated values) if present in the browser command line.
  static const char* const switch_names[] = {
    switches::kRendererAssertTest,
    switches::kRendererCrashTest,
    switches::kRendererStartupDialog,
    switches::kNoSandbox,
    switches::kTestSandbox,
    switches::kEnableSeccompSandbox,
#if !defined (GOOGLE_CHROME_BUILD)
    // This is an unsupported and not fully tested mode, so don't enable it for
    // official Chrome builds.
    switches::kInProcessPlugins,
#endif
    switches::kDomAutomationController,
    switches::kUserAgent,
    switches::kJavaScriptFlags,
    switches::kRecordMode,
    switches::kPlaybackMode,
    switches::kNoJsRandomness,
    switches::kDisableBreakpad,
    switches::kFullMemoryCrashReport,
    switches::kEnableLogging,
    switches::kDumpHistogramsOnExit,
    switches::kDisableLogging,
    switches::kLoggingLevel,
    switches::kDebugPrint,
    switches::kMemoryProfiling,
    switches::kEnableWatchdog,
    switches::kMessageLoopHistogrammer,
    switches::kEnableDCHECK,
    switches::kSilentDumpOnDCHECK,
    switches::kUseLowFragHeapCrt,
    switches::kEnableStatsTable,
    switches::kExperimentalSpellcheckerFeatures,
    switches::kDisableAudio,
    switches::kSimpleDataSource,
    switches::kEnableBenchmarking,
    switches::kInternalNaCl,
    switches::kInternalPepper,
    switches::kDisableByteRangeSupport,
    switches::kDisableDatabases,
    switches::kDisableDesktopNotifications,
    switches::kDisableWebSockets,
    switches::kDisableLocalStorage,
    switches::kEnableSessionStorage,
    switches::kDisableSharedWorkers,
    switches::kEnableApplicationCache,
    switches::kShowPaintRects,
    // We propagate the Chrome Frame command line here as well in case the
    // renderer is not run in the sandbox.
    switches::kChromeFrame,
#if defined(OS_MACOSX)
    // Allow this to be set when invoking the browser and relayed along.
    switches::kEnableSandboxLogging,
#endif
  };

  for (size_t i = 0; i < arraysize(switch_names); ++i) {
    if (browser_cmd.HasSwitch(switch_names[i])) {
      renderer_cmd->AppendSwitchWithValue(switch_names[i],
          browser_cmd.GetSwitchValueASCII(switch_names[i]));
    }
  }

  // Disable databases in incognito mode.
  if (profile()->IsOffTheRecord() &&
      !browser_cmd.HasSwitch(switches::kDisableDatabases)) {
    renderer_cmd->AppendSwitch(switches::kDisableDatabases);
  }
}

base::ProcessHandle BrowserRenderProcessHost::GetHandle() {
  // child_process_ is null either because we're in single process mode, we have
  // done fast termination, or the process has crashed.
  if (run_renderer_in_process() || !child_process_.get())
    return base::Process::Current().handle();

  if (child_process_->IsStarting())
    return base::kNullProcessHandle;

  return child_process_->GetHandle();
}

void BrowserRenderProcessHost::InitVisitedLinks() {
  VisitedLinkMaster* visitedlink_master = profile()->GetVisitedLinkMaster();
  if (!visitedlink_master)
    return;

  SendVisitedLinkTable(visitedlink_master->shared_memory());
}

void BrowserRenderProcessHost::InitUserScripts() {
  UserScriptMaster* user_script_master = profile()->GetUserScriptMaster();

  // Incognito profiles won't have user scripts.
  if (!user_script_master)
    return;

  if (!user_script_master->ScriptsReady()) {
    // No scripts ready.  :(
    return;
  }

  // Update the renderer process with the current scripts.
  SendUserScriptsUpdate(user_script_master->GetSharedMemory());
}

void BrowserRenderProcessHost::InitExtensions() {
  // TODO(aa): Should only bother sending these function names if this is an
  // extension process.
  std::vector<std::string> function_names;
  ExtensionFunctionDispatcher::GetAllFunctionNames(&function_names);
  Send(new ViewMsg_Extension_SetFunctionNames(function_names));
}

void BrowserRenderProcessHost::SendUserScriptsUpdate(
    base::SharedMemory *shared_memory) {
  // Process is being started asynchronously.  We'll end up calling
  // InitUserScripts when it's created which will call this again.
  if (child_process_.get() && child_process_->IsStarting())
    return;

  base::SharedMemoryHandle handle_for_process;
  if (!shared_memory->ShareToProcess(GetHandle(), &handle_for_process)) {
    // This can legitimately fail if the renderer asserts at startup.
    return;
  }

  if (base::SharedMemory::IsHandleValid(handle_for_process)) {
    Send(new ViewMsg_UserScripts_UpdatedScripts(handle_for_process));
  }
}

bool BrowserRenderProcessHost::FastShutdownIfPossible() {
  if (run_renderer_in_process())
    return false;  // Single process mode can't do fast shutdown.

  if (!child_process_.get() || child_process_->IsStarting() || !GetHandle())
    return false;  // Render process hasn't started or is probably crashed.

  // Test if there's an unload listener.
  // NOTE: It's possible that an onunload listener may be installed
  // while we're shutting down, so there's a small race here.  Given that
  // the window is small, it's unlikely that the web page has much
  // state that will be lost by not calling its unload handlers properly.
  if (!sudden_termination_allowed())
    return false;

  // Check for any external tab containers, since they may still be running even
  // though this window closed.
  listeners_iterator iter(ListenersIterator());
  while (!iter.IsAtEnd()) {
    // NOTE: This is a bit dangerous.  We know that for now, listeners are
    // always RenderWidgetHosts.  But in theory, they don't have to be.
    const RenderWidgetHost* widget =
        static_cast<const RenderWidgetHost*>(iter.GetCurrentValue());
    DCHECK(widget);
    if (widget && widget->IsRenderView()) {
      const RenderViewHost* rvh = static_cast<const RenderViewHost*>(widget);
      if (rvh->delegate()->IsExternalTabContainer())
        return false;
    }

    iter.Advance();
  }

  child_process_.reset();
  fast_shutdown_started_ = true;
  return true;
}

bool BrowserRenderProcessHost::SendWithTimeout(IPC::Message* msg,
                                               int timeout_ms) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->SendWithTimeout(msg, timeout_ms);
}

// This is a platform specific function for mapping a transport DIB given its id
TransportDIB* BrowserRenderProcessHost::MapTransportDIB(
    TransportDIB::Id dib_id) {
#if defined(OS_WIN)
  // On Windows we need to duplicate the handle from the remote process
  HANDLE section = win_util::GetSectionFromProcess(
      dib_id.handle, GetHandle(), false /* read write */);
  return TransportDIB::Map(section);
#elif defined(OS_MACOSX) || defined(CHROMIUM_CAPSICUM)
  // On OSX, the browser allocates all DIBs and keeps a file descriptor around
  // for each.
  return widget_helper_->MapTransportDIB(dib_id);
#elif defined(OS_NIX)
  return TransportDIB::Map(dib_id);
#endif  // defined(OS_NIX)
}

TransportDIB* BrowserRenderProcessHost::GetTransportDIB(
    TransportDIB::Id dib_id) {
  const std::map<TransportDIB::Id, TransportDIB*>::iterator
      i = cached_dibs_.find(dib_id);
  if (i != cached_dibs_.end()) {
    cached_dibs_cleaner_.Reset();
    return i->second;
  }

  TransportDIB* dib = MapTransportDIB(dib_id);
  if (!dib)
    return NULL;

  if (cached_dibs_.size() >= MAX_MAPPED_TRANSPORT_DIBS) {
    // Clean a single entry from the cache
    std::map<TransportDIB::Id, TransportDIB*>::iterator smallest_iterator;
    size_t smallest_size = std::numeric_limits<size_t>::max();

    for (std::map<TransportDIB::Id, TransportDIB*>::iterator
         i = cached_dibs_.begin(); i != cached_dibs_.end(); ++i) {
      if (i->second->size() <= smallest_size) {
        smallest_iterator = i;
        smallest_size = i->second->size();
      }
    }

    delete smallest_iterator->second;
    cached_dibs_.erase(smallest_iterator);
  }

  cached_dibs_[dib_id] = dib;
  cached_dibs_cleaner_.Reset();
  return dib;
}

void BrowserRenderProcessHost::ClearTransportDIBCache() {
  STLDeleteContainerPairSecondPointers(
      cached_dibs_.begin(), cached_dibs_.end());
  cached_dibs_.clear();
}

bool BrowserRenderProcessHost::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }

  if (child_process_.get() && child_process_->IsStarting()) {
    queued_messages_.push(msg);
    return true;
  }

  return channel_->Send(msg);
}

void BrowserRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
  mark_child_process_activity_time();
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // dispatch control messages
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(BrowserRenderProcessHost, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_PageContents, OnPageContents)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UpdatedCacheStats,
                          OnUpdatedCacheStats)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SuddenTerminationChanged,
                          SuddenTerminationChanged);
      IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionAddListener,
                          OnExtensionAddListener)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionRemoveListener,
                          OnExtensionRemoveListener)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionCloseChannel,
                          OnExtensionCloseChannel)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SpellChecker_RequestDictionary,
                          OnSpellCheckerRequestDictionary)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP_EX()

    if (!msg_is_ok) {
      // The message had a handler, but its de-serialization failed.
      // We consider this a capital crime. Kill the renderer if we have one.
      ReceivedBadMessage(msg.type());
    }
    return;
  }

  // dispatch incoming messages to the appropriate TabContents
  IPC::Channel::Listener* listener = GetListenerByID(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }
    return;
  }
  listener->OnMessageReceived(msg);
}

void BrowserRenderProcessHost::OnChannelConnected(int32 peer_pid) {
#if defined(IPC_MESSAGE_LOG_ENABLED)
  Send(new ViewMsg_SetIPCLoggingEnabled(IPC::Logging::current()->Enabled()));
#endif
}

// Static. This function can be called from any thread.
void BrowserRenderProcessHost::BadMessageTerminateProcess(
    uint32 msg_type, base::ProcessHandle process) {
  LOG(ERROR) << "bad message " << msg_type << " terminating renderer.";
  if (run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just crash.
    CHECK(false);
  }
  NOTREACHED();
  base::KillProcess(process, ResultCodes::KILLED_BAD_MESSAGE, false);
}

void BrowserRenderProcessHost::OnChannelError() {
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.
  // The OnChannelError notification can fire multiple times due to nested sync
  // calls to a renderer. If we don't have a valid channel here it means we
  // already handled the error.
  if (!channel_.get())
    return;

  // NULL in single process mode or if fast termination happened.
  bool did_crash =
      child_process_.get() ? child_process_->DidProcessCrash() : false;

  if (did_crash) {
    UMA_HISTOGRAM_PERCENTAGE("BrowserRenderProcessHost.ChildCrashes",
                             extension_process_ ? 2 : 1);
  }

  RendererClosedDetails details(did_crash, extension_process_);
  NotificationService::current()->Notify(
      NotificationType::RENDERER_PROCESS_CLOSED,
      Source<RenderProcessHost>(this),
      Details<RendererClosedDetails>(&details));

  WebCacheManager::GetInstance()->Remove(id());
  child_process_.reset();
  channel_.reset();

  IDMap<IPC::Channel::Listener>::iterator iter(&listeners_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->OnMessageReceived(
        ViewHostMsg_RenderViewGone(iter.GetCurrentKey()));
    iter.Advance();
  }

  ClearTransportDIBCache();

  // this object is not deleted at this point and may be reused later.
  // TODO(darin): clean this up
}

void BrowserRenderProcessHost::OnPageContents(const GURL& url,
                                              int32 page_id,
                                              const std::wstring& contents) {
  Profile* p = profile();
  if (!p || p->IsOffTheRecord())
    return;

  HistoryService* hs = p->GetHistoryService(Profile::IMPLICIT_ACCESS);
  if (hs)
    hs->SetPageContents(url, contents);
}

void BrowserRenderProcessHost::OnUpdatedCacheStats(
    const WebCache::UsageStats& stats) {
  WebCacheManager::GetInstance()->ObserveStats(id(), stats);
}

void BrowserRenderProcessHost::SuddenTerminationChanged(bool enabled) {
  set_sudden_termination_allowed(enabled);
}

void BrowserRenderProcessHost::SetBackgrounded(bool backgrounded) {
  // Note: we always set the backgrounded_ value.  If the process is NULL
  // (and hence hasn't been created yet), we will set the process priority
  // later when we create the process.
  backgrounded_ = backgrounded;
  if (!child_process_.get() || child_process_->IsStarting())
    return;

#if defined(OS_WIN)
  // The cbstext.dll loads as a global GetMessage hook in the browser process
  // and intercepts/unintercepts the kernel32 API SetPriorityClass in a
  // background thread. If the UI thread invokes this API just when it is
  // intercepted the stack is messed up on return from the interceptor
  // which causes random crashes in the browser process. Our hack for now
  // is to not invoke the SetPriorityClass API if the dll is loaded.
  if (GetModuleHandle(L"cbstext.dll"))
    return;
#endif  // OS_WIN

  child_process_->SetProcessBackgrounded(backgrounded);
}

void BrowserRenderProcessHost::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::USER_SCRIPTS_UPDATED: {
      base::SharedMemory* shared_memory =
          Details<base::SharedMemory>(details).ptr();
      if (shared_memory) {
        SendUserScriptsUpdate(shared_memory);
      }
      break;
    }
    case NotificationType::SPELLCHECK_HOST_REINITIALIZED: {
      InitSpellChecker();
      break;
    }
    case NotificationType::SPELLCHECK_WORD_ADDED: {
      AddSpellCheckWord(
          reinterpret_cast<const Source<SpellCheckHost>*>(&source)->
          ptr()->last_added_word());
      break;
    }
    case NotificationType::SPELLCHECK_AUTOSPELL_TOGGLED: {
      PrefService* prefs = profile()->GetPrefs();
      EnableAutoSpellCorrect(
          prefs->GetBoolean(prefs::kEnableAutoSpellCorrect));
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void BrowserRenderProcessHost::OnProcessLaunched() {
  // Now that the process is created, set its backgrounding accordingly.
  SetBackgrounded(backgrounded_);

  InitVisitedLinks();
  InitUserScripts();
  InitExtensions();
  // We don't want to initialize the spellchecker unless SpellCheckHost has been
  // created. In InitSpellChecker(), we know if GetSpellCheckHost() is NULL
  // then the spellchecker has been turned off, but here, we don't know if
  // it's been turned off or just not loaded yet.
  if (profile()->GetSpellCheckHost())
    InitSpellChecker();

  if (max_page_id_ != -1)
    Send(new ViewMsg_SetNextPageID(max_page_id_ + 1));

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }

  NotificationService::current()->Notify(
      NotificationType::RENDERER_PROCESS_CREATED,
      Source<RenderProcessHost>(this), NotificationService::NoDetails());
}

void BrowserRenderProcessHost::OnExtensionAddListener(
    const std::string& event_name) {
  if (profile()->GetExtensionMessageService()) {
    profile()->GetExtensionMessageService()->AddEventListener(
        event_name, id());
  }
}

void BrowserRenderProcessHost::OnExtensionRemoveListener(
    const std::string& event_name) {
  if (profile()->GetExtensionMessageService()) {
    profile()->GetExtensionMessageService()->RemoveEventListener(
        event_name, id());
  }
}

void BrowserRenderProcessHost::OnExtensionCloseChannel(int port_id) {
  if (profile()->GetExtensionMessageService()) {
    profile()->GetExtensionMessageService()->CloseChannel(port_id);
  }
}

void BrowserRenderProcessHost::OnSpellCheckerRequestDictionary() {
  // We may have gotten multiple requests from different renderers. We don't
  // want to initialize multiple times in this case, so we set |force| to false.
  profile()->ReinitializeSpellCheckHost(false);
}

void BrowserRenderProcessHost::AddSpellCheckWord(const std::string& word) {
  Send(new ViewMsg_SpellChecker_WordAdded(word));
}

void BrowserRenderProcessHost::InitSpellChecker() {
  SpellCheckHost* spellcheck_host = profile()->GetSpellCheckHost();
  if (spellcheck_host) {
    PrefService* prefs = profile()->GetPrefs();
    IPC::PlatformFileForTransit file;

    if (spellcheck_host->bdict_file() != base::kInvalidPlatformFileValue) {
#if defined(OS_POSIX)
      file = base::FileDescriptor(spellcheck_host->bdict_file(), false);
#elif defined(OS_WIN)
      ::DuplicateHandle(::GetCurrentProcess(), spellcheck_host->bdict_file(),
                        GetHandle(), &file, 0, false, DUPLICATE_SAME_ACCESS);
#endif
    }

    Send(new ViewMsg_SpellChecker_Init(
        file,
        spellcheck_host->custom_words(),
        spellcheck_host->language(),
        prefs->GetBoolean(prefs::kEnableAutoSpellCorrect)));
  } else {
    Send(new ViewMsg_SpellChecker_Init(
        IPC::InvalidPlatformFileForTransit(),
        std::vector<std::string>(),
        std::string(),
        false));
  }
}

void BrowserRenderProcessHost::EnableAutoSpellCorrect(bool enable) {
  Send(new ViewMsg_SpellChecker_EnableAutoSpellCorrect(enable));
}
