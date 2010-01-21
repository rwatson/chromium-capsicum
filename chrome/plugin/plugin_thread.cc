// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/plugin_thread.h"

#include "build/build_config.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/process_util.h"
#include "base/thread_local.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/plugin/chrome_plugin_host.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/render_thread.h"
#include "net/base/net_errors.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"

static base::LazyInstance<base::ThreadLocalPointer<PluginThread> > lazy_tls(
    base::LINKER_INITIALIZED);

PluginThread::PluginThread()
    : preloaded_plugin_module_(NULL) {
  plugin_path_ =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kPluginPath);

  lazy_tls.Pointer()->Set(this);
#if defined(TOOLKIT_GTK)
  {
    // XEmbed plugins assume they are hosted in a Gtk application, so we need
    // to initialize Gtk in the plugin process.
    g_thread_init(NULL);
    const std::vector<std::string>& args =
        CommandLine::ForCurrentProcess()->argv();
    int argc = args.size();
    scoped_array<char *> argv(new char *[argc + 1]);
    for (size_t i = 0; i < args.size(); ++i) {
      // TODO(piman@google.com): can gtk_init modify argv? Just being safe
      // here.
      argv[i] = strdup(args[i].c_str());
    }
    argv[argc] = NULL;
    char **argv_pointer = argv.get();

    // Flash has problems receiving clicks with newer GTKs due to the
    // client-side windows change.  To be safe, we just always set the
    // backwards-compat environment variable.
    setenv("GDK_NATIVE_WINDOWS", "1", 1);

    gtk_init(&argc, &argv_pointer);
    for (size_t i = 0; i < args.size(); ++i) {
      free(argv[i]);
    }
  }
#endif

  PatchNPNFunctions();

  // Preload the library to avoid loading, unloading then reloading
  preloaded_plugin_module_ = base::LoadNativeLibrary(plugin_path_);

  ChromePluginLib::Create(plugin_path_, GetCPBrowserFuncsForPlugin());

  scoped_refptr<NPAPI::PluginLib> plugin =
      NPAPI::PluginLib::CreatePluginLib(plugin_path_);
  if (plugin.get()) {
    plugin->NP_Initialize();
  }

  // Certain plugins, such as flash, steal the unhandled exception filter
  // thus we never get crash reports when they fault. This call fixes it.
  message_loop()->set_exception_restoration(true);
}

PluginThread::~PluginThread() {
  if (preloaded_plugin_module_) {
    base::UnloadNativeLibrary(preloaded_plugin_module_);
    preloaded_plugin_module_ = NULL;
  }
  PluginChannelBase::CleanupChannels();
  NPAPI::PluginLib::UnloadAllPlugins();
  ChromePluginLib::UnloadAllPlugins();

  if (webkit_glue::ShouldForcefullyTerminatePluginProcess())
    base::KillProcess(base::GetCurrentProcessHandle(), 0, /* wait= */ false);

  lazy_tls.Pointer()->Set(NULL);
}

PluginThread* PluginThread::current() {
  return lazy_tls.Pointer()->Get();
}

void PluginThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PluginThread, msg)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_CreateChannel, OnCreateChannel)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_PluginMessage, OnPluginMessage)
#if defined(OS_MACOSX)
  IPC_MESSAGE_HANDLER(PluginProcessMsg_PluginFocusNotify,
                      OnPluginFocusNotify)
#endif
  IPC_END_MESSAGE_MAP()
}

void PluginThread::OnCreateChannel(int renderer_id,
                                   bool off_the_record) {
  scoped_refptr<PluginChannel> channel = PluginChannel::GetPluginChannel(
      renderer_id, ChildProcess::current()->io_message_loop());
  IPC::ChannelHandle channel_handle;
  if (channel.get()) {
    channel_handle.name = channel->channel_name();
#if defined(OS_POSIX)
    // On POSIX, pass the renderer-side FD. Also mark it as auto-close so that
    // it gets closed after it has been sent.
    int renderer_fd = channel->DisownRendererFd();
    channel_handle.socket = base::FileDescriptor(renderer_fd, true);
#endif
    channel->set_off_the_record(off_the_record);
  }

  Send(new PluginProcessHostMsg_ChannelCreated(channel_handle));
}

void PluginThread::OnPluginMessage(const std::vector<unsigned char> &data) {
  // We Add/Release ref here to ensure that something will trigger the
  // shutdown mechanism for processes started in the absence of renderer's
  // opening a plugin channel.
  ChildProcess::current()->AddRefProcess();
  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(plugin_path_);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    chrome_plugin->functions().on_message(data_ptr, data_len);
  }
  ChildProcess::current()->ReleaseProcess();
}

#if defined(OS_MACOSX)
void PluginThread::OnPluginFocusNotify(uint32 instance_id) {
  WebPluginDelegateImpl* instance =
      reinterpret_cast<WebPluginDelegateImpl*>(instance_id);
  std::set<WebPluginDelegateImpl*> active_delegates =
      WebPluginDelegateImpl::GetActiveDelegates();
  for (std::set<WebPluginDelegateImpl*>::iterator iter =
           active_delegates.begin();
       iter != active_delegates.end(); iter++) {
    (*iter)->FocusNotify(instance);
  }
}
#endif

namespace webkit_glue {

#if defined(OS_WIN)
bool DownloadUrl(const std::string& url, HWND caller_window) {
  PluginThread* plugin_thread = PluginThread::current();
  if (!plugin_thread) {
    return false;
  }

  IPC::Message* message =
      new PluginProcessHostMsg_DownloadUrl(MSG_ROUTING_NONE, url,
                                           ::GetCurrentProcessId(),
                                           caller_window);
  return plugin_thread->Send(message);
}
#endif

bool GetPluginFinderURL(std::string* plugin_finder_url) {
  if (!plugin_finder_url) {
    NOTREACHED();
    return false;
  }

  PluginThread* plugin_thread = PluginThread::current();
  if (!plugin_thread)
    return false;

  plugin_thread->Send(
      new PluginProcessHostMsg_GetPluginFinderUrl(plugin_finder_url));
  DCHECK(!plugin_finder_url->empty());
  return true;
}

bool IsDefaultPluginEnabled() {
#if defined(OS_WIN)
  return true;
#elif defined(OS_NIX)
  // http://code.google.com/p/chromium/issues/detail?id=10952
  return false;
#elif defined(OS_MACOSX)
  // http://code.google.com/p/chromium/issues/detail?id=17392
  return false;
#endif
}

// Dispatch the resolve proxy resquest to the right code, depending on which
// process the plugin is running in {renderer, browser, plugin}.
bool FindProxyForUrl(const GURL& url, std::string* proxy_list) {
  int net_error;
  std::string proxy_result;

  bool result;
  if (IsPluginProcess()) {
    result = PluginThread::current()->Send(
        new PluginProcessHostMsg_ResolveProxy(url, &net_error, &proxy_result));
  } else {
    result = RenderThread::current()->Send(
        new ViewHostMsg_ResolveProxy(url, &net_error, &proxy_result));
  }

  if (!result || net_error != net::OK)
    return false;

  *proxy_list = proxy_result;
  return true;
}

} // namespace webkit_glue
