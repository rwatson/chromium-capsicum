// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_

#include <string>

#include "base/perftimer.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_popup_host.h"
#include "chrome/browser/jsmessage_box_client.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/extensions/extension_view.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/extension_view_gtk.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/cocoa/extension_view_mac.h"
#endif
#include "chrome/common/notification_registrar.h"


class Browser;
class Extension;
class ExtensionProcessManager;
class RenderProcessHost;
class RenderWidgetHost;
class RenderWidgetHostView;
class TabContents;
struct WebPreferences;

// This class is the browser component of an extension component's RenderView.
// It handles setting up the renderer process, if needed, with special
// privileges available to extensions.  It may have a view to be shown in the
// in the browser UI, or it may be hidden.
class ExtensionHost : public ExtensionPopupHost::PopupDelegate,
                      public RenderViewHostDelegate,
                      public RenderViewHostDelegate::View,
                      public ExtensionFunctionDispatcher::Delegate,
                      public NotificationObserver,
                      public JavaScriptMessageBoxClient {
 public:
  class ProcessCreationQueue;

  // Enable DOM automation in created render view hosts.
  static void EnableDOMAutomation() { enable_dom_automation_ = true; }

  ExtensionHost(Extension* extension, SiteInstance* site_instance,
                const GURL& url, ViewType::Type host_type);
  ~ExtensionHost();

#if defined(TOOLKIT_VIEWS)
  void set_view(ExtensionView* view) { view_.reset(view); }
  ExtensionView* view() const { return view_.get(); }
#elif defined(TOOLKIT_GTK)
  ExtensionViewGtk* view() const { return view_.get(); }
#elif defined(OS_MACOSX)
  ExtensionViewMac* view() const { return view_.get(); }
#else
  // TODO(port): implement
  void* view() const { return NULL; }
#endif

  // Create an ExtensionView and tie it to this host and |browser|.  Note NULL
  // is a valid argument for |browser|.  Extension views may be bound to
  // tab-contents hosted in ExternalTabContainer objects, which do not
  // instantiate Browser objects.
  void CreateView(Browser* browser);

  Extension* extension() { return extension_; }
  RenderViewHost* render_view_host() const { return render_view_host_; }
  RenderProcessHost* render_process_host() const;
  SiteInstance* site_instance() const;
  bool did_stop_loading() const { return did_stop_loading_; }
  bool document_element_available() const {
    return document_element_available_;
  }
  Profile* profile() const { return profile_; }

  // Sets the the ViewType of this host (e.g. mole, toolstrip).
  void SetRenderViewType(ViewType::Type type);

  // Returns true if the render view is initialized and didn't crash.
  bool IsRenderViewLive() const;

  // Prepares to initializes our RenderViewHost by creating its RenderView and
  // navigating to this host's url. Uses host_view for the RenderViewHost's view
  // (can be NULL). This happens delayed to avoid locking the UI.
  void CreateRenderViewSoon(RenderWidgetHostView* host_view);

  // Sets |url_| and navigates |render_view_host_|.
  void NavigateToURL(const GURL& url);

  // Insert the theme CSS for a toolstrip/mole.
  void InsertThemeCSS();

  // RenderViewHostDelegate implementation.
  virtual RenderViewHostDelegate::View* GetViewDelegate();
  virtual const GURL& GetURL() const { return url_; }
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual ViewType::Type GetRenderViewType() const;
  virtual int GetBrowserWindowID() const;
  virtual void RenderViewGone(RenderViewHost* render_view_host);
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params);
  virtual void DidStopLoading();
  virtual void DocumentAvailableInMainFrame(RenderViewHost* render_view_host);

  virtual WebPreferences GetWebkitPrefs();
  virtual void ProcessDOMUIMessage(const std::string& message,
                                   const Value* content,
                                   int request_id,
                                   bool has_callback);
  virtual void RunJavaScriptMessage(const std::wstring& message,
                                    const std::wstring& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message);
  virtual void Close(RenderViewHost* render_view_host);
  virtual RendererPreferences GetRendererPrefs(Profile* profile) const;

  // RenderViewHostDelegate::View
  virtual void CreateNewWindow(int route_id);
  virtual void CreateNewWidget(int route_id, bool activatable);
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture,
                                 const GURL& creator_url);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos);
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_operations);
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation);
  virtual void GotFocus();
  virtual void TakeFocus(bool reverse);
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);
  virtual void HandleMouseEvent();
  virtual void HandleMouseLeave();
  virtual void UpdatePreferredSize(const gfx::Size& new_size);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // JavaScriptMessageBoxClient
  virtual std::wstring GetMessageBoxTitle(const GURL& frame_url,
                                          bool is_alert);
  virtual gfx::NativeWindow GetMessageBoxRootWindow();
  virtual void OnMessageBoxClosed(IPC::Message* reply_msg,
                                  bool success,
                                  const std::wstring& prompt);
  virtual void SetSuppressMessageBoxes(bool suppress_message_boxes) {}
  virtual TabContents* AsTabContents() { return NULL; }
  virtual ExtensionHost* AsExtensionHost() { return this; }

 protected:
  // Internal functions used to support the CreateNewWidget() method. If a
  // platform requires plugging into widget creation at a lower level, then a
  // subclass might want to override these functions, but otherwise they should
  // be fine just implementing RenderWidgetHostView::InitAsPopup().
  //
  // The Create function returns the newly created widget so it can be
  // associated with the given route. When the widget needs to be shown later,
  // we'll look it up again and pass the object to the Show functions rather
  // than the route ID.
  virtual RenderWidgetHostView* CreateNewWidgetInternal(int route_id,
                                                        bool activatable);
  virtual void ShowCreatedWidgetInternal(RenderWidgetHostView* widget_host_view,
                                         const gfx::Rect& initial_pos);
 private:
  friend class ProcessCreationQueue;

  // Whether to allow DOM automation for created RenderViewHosts. This is used
  // for testing.
  static bool enable_dom_automation_;

  // Actually create the RenderView for this host. See CreateRenderViewSoon.
  void CreateRenderViewNow();

  // ExtensionFunctionDispatcher::Delegate
  // If this ExtensionHost has a view, this returns the Browser that view is a
  // part of.  If this is a global background page, we use the active Browser
  // instead.
  virtual Browser* GetBrowser() const;
  virtual ExtensionHost* GetExtensionHost() { return this; }

  // ExtensionPopupHost::Delegate
  virtual RenderViewHost* GetRenderViewHost() { return render_view_host(); }

  // Returns true if we're hosting a background page.
  // This isn't valid until CreateRenderView is called.
  bool is_background_page() const { return !view(); }

  // The extension that we're hosting in this view.
  Extension* extension_;

  // The profile that this host is tied to.
  Profile* profile_;

  // Optional view that shows the rendered content in the UI.
#if defined(TOOLKIT_VIEWS)
  scoped_ptr<ExtensionView> view_;
#elif defined(TOOLKIT_GTK)
  scoped_ptr<ExtensionViewGtk> view_;
#elif defined(OS_MACOSX)
  scoped_ptr<ExtensionViewMac> view_;
#endif

  // The host for our HTML content.
  RenderViewHost* render_view_host_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // Whether the RenderWidget has reported that it has stopped loading.
  bool did_stop_loading_;

  // True if the main frame has finished parsing.
  bool document_element_available_;

  // The URL being hosted.
  GURL url_;

  NotificationRegistrar registrar_;

  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;

  // Only EXTENSION_TOOLSTRIP, EXTENSION_POPUP, and EXTENSION_BACKGROUND_PAGE
  // are used here, others are not hosted by ExtensionHost.
  ViewType::Type extension_host_type_;

  // Used to measure how long it's been since the host was created.
  PerfTimer since_created_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHost);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
