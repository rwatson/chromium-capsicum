// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header is meant to be included in multiple passes, hence no traditional
// header guard.
// See ipc_message_macros.h for explanation of the macros and passes.

#include <map>
#include <string>
#include <vector>

#include "build/build_config.h"

#include "app/clipboard/clipboard.h"
#include "app/gfx/native_widget_types.h"
#include "base/file_path.h"
#include "base/nullable_string16.h"
#include "base/platform_file.h"
#include "base/gfx/rect.h"
#include "base/shared_memory.h"
#include "base/values.h"
#include "chrome/common/css_colors.h"
#include "chrome/common/extensions/update_manifest.h"
#include "chrome/common/nacl_types.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/transport_dib.h"
#include "chrome/common/view_types.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webplugin.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

// TODO(mpcomplete): rename ViewMsg and ViewHostMsg to something that makes
// more sense with our current design.

// IPC_MESSAGE macros choke on extra , in the std::map, when expanding. We need
// to typedef it to avoid that.
// Substitution map for l10n messages.
typedef std::map<std::string, std::string> SubstitutionMap;

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

IPC_BEGIN_MESSAGES(View)
  // Used typically when recovering from a crash.  The new rendering process
  // sets its global "next page id" counter to the given value.
  IPC_MESSAGE_CONTROL1(ViewMsg_SetNextPageID,
                       int32 /* next_page_id */)

  // Sends System Colors corresponding to a set of CSS color keywords
  // down the pipe.
  // This message must be sent to the renderer immediately on launch
  // before creating any new views.
  // The message can also be sent during a renderer's lifetime if system colors
  // are updated.
  // TODO(jeremy): Possibly change IPC format once we have this all hooked up.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetCSSColors,
                      std::vector<CSSColors::CSSColorMapping>)

  // Tells the renderer to create a new view.
  // This message is slightly different, the view it takes is the view to
  // create, the message itself is sent as a non-view control message.
  IPC_MESSAGE_CONTROL4(ViewMsg_New,
                       gfx::NativeViewId, /* parent window */
                       RendererPreferences,
                       WebPreferences,
                       int32 /* view id */)

  // Tells the renderer to set its maximum cache size to the supplied value
  IPC_MESSAGE_CONTROL3(ViewMsg_SetCacheCapacities,
                       size_t /* min_dead_capacity */,
                       size_t /* max_dead_capacity */,
                       size_t /* capacity */)

  // Reply in response to ViewHostMsg_ShowView or ViewHostMsg_ShowWidget.
  // similar to the new command, but used when the renderer created a view
  // first, and we need to update it
  IPC_MESSAGE_ROUTED1(ViewMsg_CreatingNew_ACK,
                      gfx::NativeViewId /* parent_hwnd */)

  // Sends updated preferences to the renderer.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetRendererPrefs,
                      RendererPreferences)

  // Tells the renderer to perform the given action on the media player
  // located at the given point.
  IPC_MESSAGE_ROUTED2(ViewMsg_MediaPlayerActionAt,
                      gfx::Point, /* location */
                      WebKit::WebMediaPlayerAction)

  // Tells the render view to close.
  IPC_MESSAGE_ROUTED0(ViewMsg_Close)

  // Tells the render view to change its size.  A ViewHostMsg_PaintRect message
  // is generated in response provided new_size is not empty and not equal to
  // the view's current size.  The generated ViewHostMsg_PaintRect message will
  // have the IS_RESIZE_ACK flag set. It also receives the resizer rect so that
  // we don't have to fetch it every time WebKit asks for it.
  IPC_MESSAGE_ROUTED2(ViewMsg_Resize,
                      gfx::Size /* new_size */,
                      gfx::Rect /* resizer_rect */)

  // Sent to inform the view that it was hidden.  This allows it to reduce its
  // resource utilization.
  IPC_MESSAGE_ROUTED0(ViewMsg_WasHidden)

  // Tells the render view that it is no longer hidden (see WasHidden), and the
  // render view is expected to respond with a full repaint if needs_repainting
  // is true.  In that case, the generated ViewHostMsg_PaintRect message will
  // have the IS_RESTORE_ACK flag set.  If needs_repainting is false, then this
  // message does not trigger a message in response.
  IPC_MESSAGE_ROUTED1(ViewMsg_WasRestored,
                      bool /* needs_repainting */)

  // Tells the render view to capture a thumbnail image of the page. The
  // render view responds with a ViewHostMsg_Thumbnail.
  IPC_MESSAGE_ROUTED0(ViewMsg_CaptureThumbnail)

  // Tells the render view to switch the CSS to print media type, renders every
  // requested pages and switch back the CSS to display media type.
  IPC_MESSAGE_ROUTED0(ViewMsg_PrintPages)

  // Tells the render view that printing is done so it can clean up.
  IPC_MESSAGE_ROUTED2(ViewMsg_PrintingDone,
                      int /* document_cookie */,
                      bool /* success */)

  // Tells the renderer to dump as much memory as it can, perhaps because we
  // have memory pressure or the renderer is (or will be) paged out.  This
  // should only result in purging objects we can recalculate, e.g. caches or
  // JS garbage, not in purging irreplaceable objects.
  IPC_MESSAGE_CONTROL0(ViewMsg_PurgeMemory)

  // Tells the render view that a ViewHostMsg_UpdateRect message was processed.
  // This signals the render view that it can send another UpdateRect message.
  IPC_MESSAGE_ROUTED0(ViewMsg_UpdateRect_ACK)

  // Message payload includes:
  // 1. A blob that should be cast to WebInputEvent
  // 2. An optional boolean value indicating if a RawKeyDown event is associated
  //    to a keyboard shortcut of the browser.
  IPC_MESSAGE_ROUTED0(ViewMsg_HandleInputEvent)

  // This message notifies the renderer that the next key event is bound to one
  // or more pre-defined edit commands. If the next key event is not handled
  // by webkit, the specified edit commands shall be executed against current
  // focused frame.
  // Parameters
  // * edit_commands (see chrome/common/edit_command_types.h)
  //   Contains one or more edit commands.
  // See third_party/WebKit/WebCore/editing/EditorCommand.cpp for detailed
  // definition of webkit edit commands.
  //
  // This message must be sent just before sending a key event.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetEditCommandsForNextKeyEvent,
                      EditCommands /* edit_commands */)

  // Message payload is the name/value of a WebCore edit command to execute.
  IPC_MESSAGE_ROUTED2(ViewMsg_ExecuteEditCommand,
                      std::string, /* name */
                      std::string /* value */)

  IPC_MESSAGE_ROUTED0(ViewMsg_MouseCaptureLost)

  // TODO(darin): figure out how this meshes with RestoreFocus
  IPC_MESSAGE_ROUTED1(ViewMsg_SetFocus, bool /* enable */)

  // Tells the renderer to focus the first (last if reverse is true) focusable
  // node.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetInitialFocus, bool /* reverse */)

  // Tells the renderer to perform the specified navigation, interrupting any
  // existing navigation.
  IPC_MESSAGE_ROUTED1(ViewMsg_Navigate, ViewMsg_Navigate_Params)

  IPC_MESSAGE_ROUTED0(ViewMsg_Stop)

  // Tells the renderer to load the specified html text and report a navigation
  // to display_url if passing true for new navigation.
  IPC_MESSAGE_ROUTED4(ViewMsg_LoadAlternateHTMLText,
                      std::string /* utf8 html text */,
                      bool, /* new navigation */
                      GURL /* display url */,
                      std::string /* security info */)

  // This message notifies the renderer that the user has closed the FindInPage
  // window (and that the selection should be cleared and the tick-marks
  // erased). If |clear_selection| is true, it will also clear the current
  // selection.
  IPC_MESSAGE_ROUTED1(ViewMsg_StopFinding, bool /* clear_selection */)

  // These messages are typically generated from context menus and request the
  // renderer to apply the specified operation to the current selection.
  IPC_MESSAGE_ROUTED0(ViewMsg_Undo)
  IPC_MESSAGE_ROUTED0(ViewMsg_Redo)
  IPC_MESSAGE_ROUTED0(ViewMsg_Cut)
  IPC_MESSAGE_ROUTED0(ViewMsg_Copy)
#if defined(OS_MACOSX)
  IPC_MESSAGE_ROUTED0(ViewMsg_CopyToFindPboard)
#endif
  IPC_MESSAGE_ROUTED0(ViewMsg_Paste)
  IPC_MESSAGE_ROUTED1(ViewMsg_Replace, string16)
  IPC_MESSAGE_ROUTED0(ViewMsg_ToggleSpellCheck)
  IPC_MESSAGE_ROUTED0(ViewMsg_Delete)
  IPC_MESSAGE_ROUTED0(ViewMsg_SelectAll)
  IPC_MESSAGE_ROUTED1(ViewMsg_ToggleSpellPanel, bool)

  // This message tells the renderer to advance to the next misspelling. It is
  // sent when the user clicks the "Find Next" button on the spelling panel.
  IPC_MESSAGE_ROUTED0(ViewMsg_AdvanceToNextMisspelling)

  // Copies the image at location x, y to the clipboard (if there indeed is an
  // image at that location).
  IPC_MESSAGE_ROUTED2(ViewMsg_CopyImageAt,
                      int /* x */,
                      int /* y */)

  // History system notification that the visited link database has been
  // replaced. It has one SharedMemoryHandle argument consisting of the table
  // handle. This handle is valid in the context of the renderer
  IPC_MESSAGE_CONTROL1(ViewMsg_VisitedLink_NewTable, base::SharedMemoryHandle)

  // History system notification that a link has been added and the link
  // coloring state for the given hash must be re-calculated.
  IPC_MESSAGE_CONTROL1(ViewMsg_VisitedLink_Add, std::vector<uint64>)

  // History system notification that one or more history items have been
  // deleted, which at this point means that all link coloring state must be
  // re-calculated.
  IPC_MESSAGE_CONTROL0(ViewMsg_VisitedLink_Reset)

  // Notification that the user scripts have been updated. It has one
  // SharedMemoryHandle argument consisting of the pickled script data. This
  // handle is valid in the context of the renderer.
  IPC_MESSAGE_CONTROL1(ViewMsg_UserScripts_UpdatedScripts,
                       base::SharedMemoryHandle)

  // Sent when the user wants to search for a word on the page (find in page).
  IPC_MESSAGE_ROUTED3(ViewMsg_Find,
                      int /* request_id */,
                      string16 /* search_text */,
                      WebKit::WebFindOptions)

  // Send from the browser to the rendered to get the text content of the page.
  IPC_MESSAGE_ROUTED0(ViewMsg_DeterminePageText)

  // Send from the renderer to the browser to return the text content of the
  // page.
  IPC_MESSAGE_ROUTED1(ViewMsg_DeterminePageText_Reply,
                      std::wstring /* the language */)

  // Send from the renderer to the browser to return the script running result.
  IPC_MESSAGE_ROUTED2(ViewMsg_ExecuteCodeFinished,
                      int, /* request id */
                      bool /* whether the script ran successfully */)

  // Sent when the headers are available for a resource request.
  IPC_MESSAGE_ROUTED2(ViewMsg_Resource_ReceivedResponse,
                      int /* request_id */,
                      ResourceResponseHead)

  // Sent as download progress is being made, size of the resource may be
  // unknown, in that case |size| is -1.
  IPC_MESSAGE_ROUTED3(ViewMsg_Resource_DownloadProgress,
                      int /* request_id */,
                      int64 /* position */,
                      int64 /* size */)

  // Sent as upload progress is being made.
  IPC_MESSAGE_ROUTED3(ViewMsg_Resource_UploadProgress,
                      int /* request_id */,
                      int64 /* position */,
                      int64 /* size */)

  // Sent when the request has been redirected.  The receiver is expected to
  // respond with either a FollowRedirect message (if the redirect is to be
  // followed) or a CancelRequest message (if it should not be followed).
  IPC_MESSAGE_ROUTED3(ViewMsg_Resource_ReceivedRedirect,
                      int /* request_id */,
                      GURL /* new_url */,
                      ResourceResponseHead)

  // Sent when some data from a resource request is ready. The handle should
  // already be mapped into the process that receives this message.
  IPC_MESSAGE_ROUTED3(ViewMsg_Resource_DataReceived,
                      int /* request_id */,
                      base::SharedMemoryHandle /* data */,
                      int /* data_len */)

  // Sent when the request has been completed.
  IPC_MESSAGE_ROUTED3(ViewMsg_Resource_RequestComplete,
                      int /* request_id */,
                      URLRequestStatus /* status */,
                      std::string /* security info */)

  // Request for the renderer to evaluate an xpath to a frame and execute a
  // javascript: url in that frame's context. The message is completely
  // asynchronous and no corresponding response message is sent back.
  //
  // frame_xpath contains the modified xpath notation to identify an inner
  // subframe (starting from the root frame). It is a concatenation of
  // number of smaller xpaths delimited by '\n'. Each chunk in the string can
  // be evaluated to a frame in its parent-frame's context.
  //
  // Example: /html/body/iframe/\n/html/body/div/iframe/\n/frameset/frame[0]
  // can be broken into 3 xpaths
  // /html/body/iframe evaluates to an iframe within the root frame
  // /html/body/div/iframe evaluates to an iframe within the level-1 iframe
  // /frameset/frame[0] evaluates to first frame within the level-2 iframe
  //
  // jscript_url is the string containing the javascript: url to be executed
  // in the target frame's context. The string should start with "javascript:"
  // and continue with a valid JS text.
  IPC_MESSAGE_ROUTED2(ViewMsg_ScriptEvalRequest,
                      std::wstring,  /* frame_xpath */
                      std::wstring  /* jscript_url */)

  // Request for the renderer to evaluate an xpath to a frame and insert css
  // into that frame's document. See ViewMsg_ScriptEvalRequest for details on
  // allowed xpath expressions.
  IPC_MESSAGE_ROUTED3(ViewMsg_CSSInsertRequest,
                      std::wstring,  /* frame_xpath */
                      std::string,  /* css string */
                      std::string  /* element id */)

  // Log a message to the console of the target frame
  IPC_MESSAGE_ROUTED3(ViewMsg_AddMessageToConsole,
                      string16 /* frame_xpath */,
                      string16 /* message */,
                      WebKit::WebConsoleMessage::Level /* message_level */)

  // RenderViewHostDelegate::RenderViewCreated method sends this message to a
  // new renderer to notify it that it will host developer tools UI and should
  // set up all neccessary bindings and create DevToolsClient instance that
  // will handle communication with inspected page DevToolsAgent.
  IPC_MESSAGE_ROUTED0(ViewMsg_SetupDevToolsClient)

  // Change the zoom level for the current main frame.  If the level actually
  // changes, a ViewHostMsg_DidZoomHost message will be sent back to the browser
  // telling it what host got zoomed and what its current zoom level is.
  IPC_MESSAGE_ROUTED1(ViewMsg_Zoom,
                      PageZoom::Function /* function */)

  // Set the zoom level for a particular hostname that the renderer is in the
  // process of loading.  This will be stored, to be used if the load commits
  // and ignored otherwise.
  IPC_MESSAGE_ROUTED2(ViewMsg_SetZoomLevelForLoadingHost,
                      std::string /* host */,
                      int /* zoom_level */)

  // Set the zoom level for a particular hostname, so all render views
  // displaying this host can update their zoom levels to match.
  IPC_MESSAGE_CONTROL2(ViewMsg_SetZoomLevelForCurrentHost,
                       std::string /* host */,
                       int /* zoom_level */)

  // Change encoding of page in the renderer.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetPageEncoding,
                      std::string /*new encoding name*/)

  // Reset encoding of page in the renderer back to default.
  IPC_MESSAGE_ROUTED0(ViewMsg_ResetPageEncodingToDefault)

  // Requests the renderer to reserve a range of page ids.
  IPC_MESSAGE_ROUTED1(ViewMsg_ReservePageIDRange,
                      int  /* size_of_range */)

  // Fill a form with data and optionally submit it
  IPC_MESSAGE_ROUTED1(ViewMsg_FormFill,
                      FormData /* form */)

  // Fill a password form and prepare field autocomplete for multiple
  // matching logins.
  IPC_MESSAGE_ROUTED1(ViewMsg_FillPasswordForm,
                      webkit_glue::PasswordFormDomManager::FillData)

  // D&d drop target messages.
  IPC_MESSAGE_ROUTED4(ViewMsg_DragTargetDragEnter,
                      WebDropData /* drop_data */,
                      gfx::Point /* client_pt */,
                      gfx::Point /* screen_pt */,
                      WebKit::WebDragOperationsMask /* ops_allowed */)
  IPC_MESSAGE_ROUTED3(ViewMsg_DragTargetDragOver,
                      gfx::Point /* client_pt */,
                      gfx::Point /* screen_pt */,
                      WebKit::WebDragOperationsMask /* ops_allowed */)
  IPC_MESSAGE_ROUTED0(ViewMsg_DragTargetDragLeave)
  IPC_MESSAGE_ROUTED2(ViewMsg_DragTargetDrop,
                      gfx::Point /* client_pt */,
                      gfx::Point /* screen_pt */)

  // Notifies the renderer of updates in mouse position of an in-progress
  // drag.  if |ended| is true, then the user has ended the drag operation.
  IPC_MESSAGE_ROUTED4(ViewMsg_DragSourceEndedOrMoved,
                      gfx::Point /* client_pt */,
                      gfx::Point /* screen_pt */,
                      bool /* ended */,
                      WebKit::WebDragOperation /* drag_operation */)

  // Notifies the renderer that the system DoDragDrop call has ended.
  IPC_MESSAGE_ROUTED0(ViewMsg_DragSourceSystemDragEnded)

  // Used to tell a render view whether it should expose various bindings
  // that allow JS content extended privileges.  See BindingsPolicy for valid
  // flag values.
  IPC_MESSAGE_ROUTED1(ViewMsg_AllowBindings,
                      int /* enabled_bindings_flags */)

  // Tell the renderer to add a property to the DOMUI binding object.  This
  // only works if we allowed DOMUI bindings.
  IPC_MESSAGE_ROUTED2(ViewMsg_SetDOMUIProperty,
                      std::string /* property_name */,
                      std::string /* property_value_json */)

  // This message starts/stop monitoring the status of the focused edit
  // control of a renderer process.
  // Parameters
  // * is_active (bool)
  //   Represents whether or not the IME is active in a browser process.
  //   The possible actions when a renderer process receives this message are
  //   listed below:
  //     Value Action
  //     true  Start sending IPC messages, ViewHostMsg_ImeUpdateStatus
  //           to notify the status of the focused edit control.
  //     false Stop sending IPC messages, ViewHostMsg_ImeUpdateStatus.
  IPC_MESSAGE_ROUTED1(ViewMsg_ImeSetInputMode,
                      bool /* is_active */)

  // This message sends a string being composed with IME.
  // Parameters
  // * string_type (int)
  //   Represents the type of the 'ime_string' parameter.
  //   Its possible values and description are listed below:
  //     Value         Description
  //     -1            The parameter is not used.
  //     1             The parameter represents a result string.
  //     0             The parameter represents a composition string.
  // * cursor_position (int)
  //   Represents the position of the cursor
  // * target_start (int)
  //   Represents the position of the beginning of the selection
  // * target_end (int)
  //   Represents the position of the end of the selection
  // * ime_string (std::wstring)
  //   Represents the string retrieved from IME (Input Method Editor)
  IPC_MESSAGE_ROUTED5(ViewMsg_ImeSetComposition,
                      WebKit::WebCompositionCommand, /* command */
                      int, /* cursor_position */
                      int, /* target_start */
                      int, /* target_end */
                      string16 /* ime_string */ )

  // This passes a set of webkit preferences down to the renderer.
  IPC_MESSAGE_ROUTED1(ViewMsg_UpdateWebPreferences, WebPreferences)

  // Used to notify the render-view that the browser has received a reply for
  // the Find operation and is interested in receiving the next one. This is
  // used to prevent the renderer from spamming the browser process with
  // results.
  IPC_MESSAGE_ROUTED0(ViewMsg_FindReplyACK)

  // Used to notify the render-view that we have received a target URL. Used
  // to prevent target URLs spamming the browser.
  IPC_MESSAGE_ROUTED0(ViewMsg_UpdateTargetURL_ACK)

  // Sets the alternate error page URL (link doctor) for the renderer process.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetAltErrorPageURL, GURL)

  // Install the first missing pluign.
  IPC_MESSAGE_ROUTED0(ViewMsg_InstallMissingPlugin)

  // Tells the renderer to empty its plugin list cache, optional reloading
  // pages containing plugins.
  IPC_MESSAGE_CONTROL1(ViewMsg_PurgePluginListCache,
                       bool /* reload_pages */)

  IPC_MESSAGE_ROUTED1(ViewMsg_RunFileChooserResponse,
                      std::vector<FilePath> /* selected files */)

  // Used to instruct the RenderView to go into "view source" mode.
  IPC_MESSAGE_ROUTED0(ViewMsg_EnableViewSourceMode)

  IPC_MESSAGE_ROUTED2(ViewMsg_UpdateBackForwardListCount,
                      int /* back_list_count */,
                      int /* forward_list_count */)

  // Retreive information from the MSAA DOM subtree, for accessibility purposes.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewMsg_GetAccessibilityInfo,
                             webkit_glue::WebAccessibility::InParams
                             /* input parameters */,
                             webkit_glue::WebAccessibility::OutParams
                             /* output parameters */)

  // Requests the renderer to clear cashed accessibility information. Takes an
  // id to clear a specific hashmap entry, and a bool; true clears all, false
  // does not.
  IPC_MESSAGE_ROUTED2(ViewMsg_ClearAccessibilityInfo,
                      int  /* accessibility object id */,
                      bool /* clear_all */)

  // Get all savable resource links from current webpage, include main
  // frame and sub-frame.
  IPC_MESSAGE_ROUTED1(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                      GURL /* url of page which is needed to save */)

  // Get html data by serializing all frames of current page with lists
  // which contain all resource links that have local copy.
  IPC_MESSAGE_ROUTED3(ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
                      std::vector<GURL> /* urls that have local copy */,
                      std::vector<FilePath> /* paths of local copy */,
                      FilePath /* local directory path */)

  // Requests application info for the page. The renderer responds back with
  // ViewHostMsg_DidGetApplicationInfo.
  IPC_MESSAGE_ROUTED1(ViewMsg_GetApplicationInfo, int32 /*page_id*/)

  // Requests the renderer to download the specified favicon image encode it as
  // PNG and send the PNG data back ala ViewHostMsg_DidDownloadFavIcon.
  IPC_MESSAGE_ROUTED3(ViewMsg_DownloadFavIcon,
                      int /* identifier for the request */,
                      GURL /* URL of the image */,
                      int /* Size of the image. Normally 0, but set if you have
                             a preferred image size to request, such as when
                             downloading the favicon */)

  // When a renderer sends a ViewHostMsg_Focus to the browser process,
  // the browser has the option of sending a ViewMsg_CantFocus back to
  // the renderer.
  IPC_MESSAGE_ROUTED0(ViewMsg_CantFocus)

  // Instructs the renderer to invoke the frame's shouldClose method, which
  // runs the onbeforeunload event handler.  Expects the result to be returned
  // via ViewHostMsg_ShouldClose.
  IPC_MESSAGE_ROUTED0(ViewMsg_ShouldClose)

  // Instructs the renderer to close the current page, including running the
  // onunload event handler. See the struct in render_messages.h for more.
  //
  // Expects a ClosePage_ACK message when finished, where the parameters are
  // echoed back.
  IPC_MESSAGE_ROUTED1(ViewMsg_ClosePage,
                      ViewMsg_ClosePage_Params)

  // Asks the renderer to send back stats on the WebCore cache broken down by
  // resource types.
  IPC_MESSAGE_CONTROL0(ViewMsg_GetCacheResourceStats)

  // Asks the renderer to send back Histograms.
  IPC_MESSAGE_CONTROL1(ViewMsg_GetRendererHistograms,
                       int /* sequence number of Renderer Histograms. */)

#if defined(USE_TCMALLOC)
  // Asks the renderer to send back tcmalloc stats.
  IPC_MESSAGE_CONTROL0(ViewMsg_GetRendererTcmalloc)
#endif

  // Asks the renderer to send back V8 heap stats.
  IPC_MESSAGE_CONTROL0(ViewMsg_GetV8HeapStats)

  // Notifies the renderer about ui theme changes
  IPC_MESSAGE_ROUTED0(ViewMsg_ThemeChanged)

  // Notifies the renderer that a paint is to be generated for the rectangle
  // passed in.
  IPC_MESSAGE_ROUTED1(ViewMsg_Repaint,
                      gfx::Size /* The view size to be repainted */)

  // Posts a message to the renderer.
  IPC_MESSAGE_ROUTED3(ViewMsg_HandleMessageFromExternalHost,
                      std::string /* The message */,
                      std::string /* The origin */,
                      std::string /* The target*/)

  // Sent to the renderer when a popup window should no longer count against
  // the current popup count (either because it's not a popup or because it was
  // a generated by a user action or because a constrained popup got turned
  // into a full window).
  IPC_MESSAGE_ROUTED0(ViewMsg_DisassociateFromPopupCount)

  // Notifies the renderer of the appcache that has been selected for a
  // a particular host. This is sent in reply to AppCacheMsg_SelectCache.
  IPC_MESSAGE_CONTROL3(AppCacheMsg_CacheSelected,
                       int /* host_id */,
                       int64 /* appcache_id */,
                       appcache::Status)

  // Notifies the renderer of an AppCache status change.
  IPC_MESSAGE_CONTROL2(AppCacheMsg_StatusChanged,
                       std::vector<int> /* host_ids */,
                       appcache::Status)

  // Notifies the renderer of an AppCache event.
  IPC_MESSAGE_CONTROL2(AppCacheMsg_EventRaised,
                       std::vector<int> /* host_ids */,
                       appcache::EventID)

  // Reply to the ViewHostMsg_QueryFormFieldAutofill message with the autofill
  // suggestions.
  IPC_MESSAGE_ROUTED3(ViewMsg_QueryFormFieldAutofill_ACK,
                      int /* id of the request message */,
                      std::vector<string16> /* suggestions */,
                      int /* index of default suggestion */)

  // Sent by the Browser process to alert a window about whether a blocked
  // popup notification is visible. The renderer assumes every new window is a
  // blocked popup until notified otherwise.
  IPC_MESSAGE_ROUTED1(ViewMsg_PopupNotificationVisibilityChanged,
                      bool /* Whether it is visible */)

  // Sent by AudioRendererHost to renderer to request an audio packet.
  IPC_MESSAGE_ROUTED3(ViewMsg_RequestAudioPacket,
                      int /* stream id */,
                      size_t /* bytes in buffer */,
                      int64 /* message timestamp */)

  // Tell the renderer process that the audio stream has been created, renderer
  // process would be given a ShareMemoryHandle that it should write to from
  // then on.
  IPC_MESSAGE_ROUTED3(ViewMsg_NotifyAudioStreamCreated,
                      int /* stream id */,
                      base::SharedMemoryHandle /* handle */,
                      int /* length */)

  // Notification message sent from AudioRendererHost to renderer for state
  // update after the renderer has requested a Create/Start/Close.
  IPC_MESSAGE_ROUTED2(ViewMsg_NotifyAudioStreamStateChanged,
                      int /* stream id */,
                      ViewMsg_AudioStreamState /* new state */)

  IPC_MESSAGE_ROUTED2(ViewMsg_NotifyAudioStreamVolume,
                      int /* stream id */,
                      double /* volume */)

  // Notification that a move or resize renderer's containing window has
  // started.
  IPC_MESSAGE_ROUTED0(ViewMsg_MoveOrResizeStarted)

  // The browser sends this message in response to all extension api calls.
  IPC_MESSAGE_ROUTED4(ViewMsg_ExtensionResponse,
                      int /* request_id */,
                      bool /* success */,
                      std::string /* response */,
                      std::string /* error */)

  // This message is optionally routed.  If used as a control message, it
  // will call a javascript function in every registered context in the
  // target process.  If routed, it will be restricted to the contexts that
  // are part of the target RenderView.
  // |args| is a list of primitive Value types that are passed to the function.
  IPC_MESSAGE_ROUTED2(ViewMsg_ExtensionMessageInvoke,
                      std::string /* function_name */,
                      ListValue /* args */)

  // Tell the renderer process all known extension function names.
  IPC_MESSAGE_CONTROL1(ViewMsg_Extension_SetFunctionNames,
                       std::vector<std::string>)

  // Tell the renderer process which permissions the given extension has. See
  // Extension::Permissions for which elements correspond to which permissions.
  IPC_MESSAGE_CONTROL2(ViewMsg_Extension_SetAPIPermissions,
                       std::string /* extension_id */,
                       std::vector<std::string> /* permissions */)

  // Tell the renderer process which host permissions the given extension has.
  IPC_MESSAGE_CONTROL2(
      ViewMsg_Extension_SetHostPermissions,
      GURL /* source extension's origin */,
      std::vector<URLPattern> /* URLPatterns the extension can access */)

  // Tell the renderer process all known page action ids for a particular
  // extension.
  IPC_MESSAGE_CONTROL2(ViewMsg_Extension_UpdatePageActions,
                       std::string /* extension_id */,
                       std::vector<std::string> /* page_action_ids */)

  // Changes the text direction of the currently selected input field (if any).
  IPC_MESSAGE_ROUTED1(ViewMsg_SetTextDirection,
                      WebKit::WebTextDirection /* direction */)

  // Tells the renderer to clear the focused node (if any).
  IPC_MESSAGE_ROUTED0(ViewMsg_ClearFocusedNode)

  // Make the RenderView transparent and render it onto a custom background. The
  // background will be tiled in both directions if it is not large enough.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetBackground,
                      SkBitmap /* background */)

  // Reply to ViewHostMsg_RequestMove, ViewHostMsg_ShowView, and
  // ViewHostMsg_ShowWidget to inform the renderer that the browser has
  // processed the move.  The browser may have ignored the move, but it finished
  // processing.  This is used because the renderer keeps a temporary cache of
  // the widget position while these asynchronous operations are in progress.
  IPC_MESSAGE_ROUTED0(ViewMsg_Move_ACK)

  // Used to instruct the RenderView to send back updates to the preferred size.
  IPC_MESSAGE_ROUTED0(ViewMsg_EnablePreferredSizeChangedMode)

  // Used to inform the renderer that the browser has displayed its
  // requested notification.
  IPC_MESSAGE_ROUTED1(ViewMsg_PostDisplayToNotificationObject,
                      int /* notification_id */)

  // Used to inform the renderer that the browser has encountered an error
  // trying to display a notification.
  IPC_MESSAGE_ROUTED2(ViewMsg_PostErrorToNotificationObject,
                      int /* notification_id */,
                      string16 /* message */)

  // Informs the renderer that the one if its notifications has closed.
  IPC_MESSAGE_ROUTED2(ViewMsg_PostCloseToNotificationObject,
                      int /* notification_id */,
                      bool /* by_user */)

  // Informs the renderer that the one if its notifications has closed.
  IPC_MESSAGE_ROUTED1(ViewMsg_PermissionRequestDone,
                      int /* request_id */)

  // Activate/deactivate the RenderView (i.e., set its controls' tint
  // accordingly, etc.).
  IPC_MESSAGE_ROUTED1(ViewMsg_SetActive,
                      bool /* active */)

  // Response message to ViewHostMsg_CreateShared/DedicatedWorker.
  // Sent when the worker has started.
  IPC_MESSAGE_ROUTED0(ViewMsg_WorkerCreated)

  // Tell the renderer which browser window it's being attached to.
  IPC_MESSAGE_ROUTED1(ViewMsg_UpdateBrowserWindowId,
                      int /* id of browser window */)

  // Tell the renderer which type this view is.
  IPC_MESSAGE_ROUTED1(ViewMsg_NotifyRenderViewType,
                      ViewType::Type /* view_type */)

  // Notification that renderer should run some JavaScript code.
  IPC_MESSAGE_ROUTED1(ViewMsg_ExecuteCode,
                      ViewMsg_ExecuteCode_Params)

  // Returns a file handle
  IPC_MESSAGE_CONTROL2(ViewMsg_DatabaseOpenFileResponse,
                       int32 /* the ID of the message we're replying to */,
                       ViewMsg_DatabaseOpenFileResponse_Params)

  // Returns a SQLite error code
  IPC_MESSAGE_CONTROL2(ViewMsg_DatabaseDeleteFileResponse,
                       int32 /* the ID of the message we're replying to */,
                       int /* SQLite error code */)

  // Returns the attributes of a file
  IPC_MESSAGE_CONTROL2(ViewMsg_DatabaseGetFileAttributesResponse,
                       int32 /* the ID of the message we're replying to */,
                       int32 /* the attributes for the given DB file */)

  // Returns the size of a file
  IPC_MESSAGE_CONTROL2(ViewMsg_DatabaseGetFileSizeResponse,
                       int32 /* the ID of the message we're replying to */,
                       int64 /* the size of the given DB file */)

  // Notifies the child process of the new database size
  IPC_MESSAGE_CONTROL4(ViewMsg_DatabaseUpdateSize,
                       string16 /* the origin */,
                       string16 /* the database name */,
                       int64 /* the new database size */,
                       int64 /* space available to origin */)

  // Storage events are broadcast to renderer processes.
  IPC_MESSAGE_CONTROL1(ViewMsg_DOMStorageEvent,
                       ViewMsg_DOMStorageEvent_Params)

#if defined(IPC_MESSAGE_LOG_ENABLED)
  // Tell the renderer process to begin or end IPC message logging.
  IPC_MESSAGE_CONTROL1(ViewMsg_SetIPCLoggingEnabled,
                       bool /* on or off */)
#endif

  // Socket Stream messages:
  // These are messages from the browser to the SocketStreamHandle on
  // a renderer.

  // A |socket_id| is assigned by ViewHostMsg_SocketStream_Connect.
  // The Socket Stream is connected. The SocketStreamHandle should keep track
  // of how much it has pending (how much it has requested to be sent) and
  // shouldn't go over |max_pending_send_allowed| bytes.
  IPC_MESSAGE_CONTROL2(ViewMsg_SocketStream_Connected,
                       int /* socket_id */,
                       int /* max_pending_send_allowed */)

  // |data| is received on the Socket Stream.
  IPC_MESSAGE_CONTROL2(ViewMsg_SocketStream_ReceivedData,
                       int /* socket_id */,
                       std::vector<char> /* data */)

  // |amount_sent| bytes of data requested by
  // ViewHostMsg_SocketStream_SendData has been sent on the Socket Stream.
  IPC_MESSAGE_CONTROL2(ViewMsg_SocketStream_SentData,
                       int /* socket_id */,
                       int /* amount_sent */)

  // The Socket Stream is closed.
  IPC_MESSAGE_CONTROL1(ViewMsg_SocketStream_Closed,
                       int /* socket_id */)

  // SpellChecker messages.

  // Passes some initialization params to the renderer's spellchecker. This can
  // be called directly after startup or in (async) response to a
  // RequestDictionary ViewHost message.
  IPC_MESSAGE_CONTROL4(ViewMsg_SpellChecker_Init,
                       IPC::PlatformFileForTransit /* bdict_file */,
                       std::vector<std::string> /* custom_dict_words */,
                       std::string /* language */,
                       bool /* auto spell correct */)

  // A word has been added to the custom dictionary; update the local custom
  // word list.
  IPC_MESSAGE_CONTROL1(ViewMsg_SpellChecker_WordAdded,
                       std::string /* word */)

  // Toggle the auto spell correct functionality.
  IPC_MESSAGE_CONTROL1(ViewMsg_SpellChecker_EnableAutoSpellCorrect,
                       bool /* enable */)

  // Executes custom context menu action that was provided from WebKit.
  IPC_MESSAGE_ROUTED1(ViewMsg_CustomContextMenuAction,
                      unsigned /* action */)
IPC_END_MESSAGES(View)


//-----------------------------------------------------------------------------
// TabContents messages
// These are messages sent from the renderer to the browser process.

IPC_BEGIN_MESSAGES(ViewHost)
  // Sent by the renderer when it is creating a new window.  The browser creates
  // a tab for it and responds with a ViewMsg_CreatingNew_ACK.  If route_id is
  // MSG_ROUTING_NONE, the view couldn't be created.
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_CreateWindow,
                              int /* opener_id */,
                              bool /* user_gesture */,
                              int /* route_id */)

  // Similar to ViewHostMsg_CreateWindow, except used for sub-widgets, like
  // <select> dropdowns.  This message is sent to the TabContents that
  // contains the widget being created.
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_CreateWidget,
                              int /* opener_id */,
                              bool /* focus on show */,
                              int /* route_id */)

  // These two messages are sent to the parent RenderViewHost to display the
  // page/widget that was created by CreateWindow/CreateWidget.  routing_id
  // refers to the id that was returned from the Create message above.
  // The initial_position parameter is a rectangle in screen coordinates.
  //
  // FUTURE: there will probably be flags here to control if the result is
  // in a new window.
  IPC_MESSAGE_ROUTED5(ViewHostMsg_ShowView,
                      int /* route_id */,
                      WindowOpenDisposition /* disposition */,
                      gfx::Rect /* initial_pos */,
                      bool /* opened_by_user_gesture */,
                      GURL /* creator_url */)

  IPC_MESSAGE_ROUTED2(ViewHostMsg_ShowWidget,
                      int /* route_id */,
                      gfx::Rect /* initial_pos */)

  // Message to show a popup menu using native cocoa controls (Mac only).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowPopup,
                      ViewHostMsg_ShowPopup_Params)

  // This message is sent after ViewHostMsg_ShowView to cause the RenderView
  // to run in a modal fashion until it is closed.
  IPC_SYNC_MESSAGE_ROUTED0_0(ViewHostMsg_RunModal)

  IPC_MESSAGE_CONTROL1(ViewHostMsg_UpdatedCacheStats,
                       WebKit::WebCache::UsageStats /* stats */)

  // Indicates the renderer is ready in response to a ViewMsg_New or
  // a ViewMsg_CreatingNew_ACK.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_RenderViewReady)

  // Indicates the renderer process is gone.  This actually is sent by the
  // browser process to itself, but keeps the interface cleaner.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_RenderViewGone)

  // Sent by the renderer process to request that the browser close the view.
  // This corresponds to the window.close() API, and the browser may ignore
  // this message.  Otherwise, the browser will generates a ViewMsg_Close
  // message to close the view.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_Close)

  // Sent by the renderer process to request that the browser move the view.
  // This corresponds to the window.resizeTo() and window.moveTo() APIs, and
  // the browser may ignore this message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_RequestMove,
                      gfx::Rect /* position */)

  // Notifies the browser that a frame in the view has changed. This message
  // has a lot of parameters and is packed/unpacked by functions defined in
  // render_messages.h.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_FrameNavigate,
                      ViewHostMsg_FrameNavigate_Params)

  // Notifies the browser that we have session history information.
  // page_id: unique ID that allows us to distinguish between history entries.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateState,
                      int32 /* page_id */,
                      std::string /* state */)

  // Notifies the browser that a document has been loaded in a frame.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DocumentLoadedInFrame)

  // Changes the title for the page in the UI when the page is navigated or the
  // title changes.
  // TODO(darin): use a UTF-8 string to reduce data size
  IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateTitle, int32, std::wstring)

  // Change the encoding name of the page in UI when the page has detected
  // proper encoding name.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateEncoding,
                      std::string /* new encoding name */)

  // Notifies the browser that we want to show a destination url for a potential
  // action (e.g. when the user is hovering over a link).
  IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateTargetURL, int32, GURL)

  // Sent when the renderer starts loading the page. This corresponds to
  // WebKit's notion of the throbber starting. Note that sometimes you may get
  // duplicates of these during a single load.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DidStartLoading)

  // Sent when the renderer is done loading a page. This corresponds to WebKit's
  // noption of the throbber stopping.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DidStopLoading)

  // Sent when the document element is available for the toplevel frame.  This
  // happens after the page starts loading, but before all resources are
  // finished.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DocumentAvailableInMainFrame)

  // Sent when the renderer loads a resource from its memory cache.
  // The security info is non empty if the resource was originally loaded over
  // a secure connection.
  // Note: May only be sent once per URL per frame per committed load.
  IPC_MESSAGE_ROUTED4(ViewHostMsg_DidLoadResourceFromMemoryCache,
                      GURL /* url */,
                      std::string  /* frame_origin */,
                      std::string  /* main_frame_origin */,
                      std::string  /* security info */)

  // Sent when the renderer displays insecure content in a secure page.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DidDisplayInsecureContent)

  // Sent when the renderer runs insecure content in a secure origin.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_DidRunInsecureContent,
                      std::string  /* security_origin */)

  // Sent when the renderer starts a provisional load for a frame.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DidStartProvisionalLoadForFrame,
                      bool /* true if it is the main frame */,
                      GURL /* url */)

  // Sent when the renderer fails a provisional load with an error.
  IPC_MESSAGE_ROUTED4(ViewHostMsg_DidFailProvisionalLoadWithError,
                      bool /* true if it is the main frame */,
                      int /* error_code */,
                      GURL /* url */,
                      bool /* true if the failure is the result of
                              navigating to a POST again and we're going to
                              show the POST interstitial */ )

  // Sent to update part of the view.  In response to this message, the host
  // generates a ViewMsg_UpdateRect_ACK message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateRect,
                      ViewHostMsg_UpdateRect_Params)

  // Acknowledges receipt of a ViewMsg_HandleInputEvent message.
  // Payload is a WebInputEvent::Type which is the type of the event, followed
  // by an optional WebInputEvent which is provided only if the event was not
  // processed.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_HandleInputEvent_ACK)

  IPC_MESSAGE_ROUTED0(ViewHostMsg_Focus)
  IPC_MESSAGE_ROUTED0(ViewHostMsg_Blur)
  IPC_MESSAGE_ROUTED0(ViewHostMsg_FocusedNodeChanged)

  // Returns the window location of the given window.
  // TODO(shess): Provide a mapping from reply_msg->routing_id() to
  // HWND so that we can eliminate the NativeViewId parameter.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetWindowRect,
                             gfx::NativeViewId /* window */,
                             gfx::Rect /* Out: Window location */)

  IPC_MESSAGE_ROUTED1(ViewHostMsg_SetCursor, WebCursor)
  // Result of string search in the page.
  // Response to ViewMsg_Find with the results of the requested find-in-page
  // search, the number of matches found and the selection rect (in screen
  // coordinates) for the string found. If |final_update| is false, it signals
  // that this is not the last Find_Reply message - more will be sent as the
  // scoping effort continues.
  IPC_MESSAGE_ROUTED5(ViewHostMsg_Find_Reply,
                      int /* request_id */,
                      int /* number of matches */,
                      gfx::Rect /* selection_rect */,
                      int /* active_match_ordinal */,
                      bool /* final_update */)

  // Makes a resource request via the browser.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_RequestResource,
                      int /* request_id */,
                      ViewHostMsg_Resource_Request)

  // Cancels a resource request with the ID given as the parameter.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_CancelRequest,
                      int /* request_id */)

  // Follows a redirect that occured for the resource request with the ID given
  // as the parameter.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_FollowRedirect,
                      int /* request_id */,
                      bool /* has_new_first_party_for_cookies */,
                      GURL /* new_first_party_for_cookies */)

  // Makes a synchronous resource request via the browser.
  IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_SyncLoad,
                             int /* request_id */,
                             ViewHostMsg_Resource_Request,
                             SyncLoadResult)

  // Used to set a cookie.  The cookie is set asynchronously, but will be
  // available to a subsequent ViewHostMsg_GetCookies request.
  IPC_MESSAGE_CONTROL3(ViewHostMsg_SetCookie,
                       GURL /* url */,
                       GURL /* first_party_for_cookies */,
                       std::string /* cookie */)

  // Used to get cookies for the given URL
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_GetCookies,
                              GURL /* url */,
                              GURL /* first_party_for_cookies */,
                              std::string /* cookies */)

  // Used to get raw cookie information for the given URL
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_GetRawCookies,
                              GURL /* url */,
                              GURL /* first_party_for_cookies */,
                              std::vector<webkit_glue::WebCookie>
                                  /* raw_cookies */)

  // Used to delete cookie for the given URL and name
  IPC_SYNC_MESSAGE_CONTROL2_0(ViewHostMsg_DeleteCookie,
                              GURL /* url */,
                              std::string /* cookie_name */)

  // Used to get the list of plugins
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetPlugins,
                              bool /* refresh*/,
                              std::vector<WebPluginInfo> /* plugins */)

  // Returns a path to a plugin for the given url and mime type.  If there's
  // no plugin, an empty string is returned.
  IPC_SYNC_MESSAGE_CONTROL3_2(ViewHostMsg_GetPluginPath,
                              GURL /* url */,
                              GURL /* policy_url */,
                              std::string /* mime_type */,
                              FilePath /* filename */,
                              std::string /* actual mime type for url */)

  // Requests spellcheck for a word.
  IPC_SYNC_MESSAGE_ROUTED2_2(ViewHostMsg_SpellCheck,
                             string16 /* word to check */,
                             int /* document tag*/,
                             int /* misspell location */,
                             int /* misspell length */)

  // Asks the browser for a unique document tag.
  IPC_SYNC_MESSAGE_ROUTED0_1(ViewHostMsg_GetDocumentTag,
                             int /* the tag */)


  // This message tells the spellchecker that a document, identified by an int
  // tag, has been closed and all of the ignored words for that document can be
  // forgotten.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_DocumentWithTagClosed,
                      int /* the tag */)

  // Tells the browser to display or not display the SpellingPanel
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowSpellingPanel,
                      bool /* if true, then show it, otherwise hide it*/)

  // Tells the browser to update the spelling panel with the given word.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateSpellingPanelWithMisspelledWord,
                      string16 /* the word to update the panel with */)

  // Initiate a download based on user actions like 'ALT+click'.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DownloadUrl,
                      GURL /* url */,
                      GURL /* referrer */)

  // Used to go to the session history entry at the given offset (ie, -1 will
  // return the "back" item).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_GoToEntryAtOffset,
                      int /* offset (from current) of history item to get */)

  IPC_SYNC_MESSAGE_ROUTED4_2(ViewHostMsg_RunJavaScriptMessage,
                             std::wstring /* in - alert message */,
                             std::wstring /* in - default prompt */,
                             GURL         /* in - originating page URL */,
                             int          /* in - dialog flags */,
                             bool         /* out - success */,
                             std::wstring /* out - prompt field */)

  // Sets the contents for the given page (URL and page ID are the first two
  // arguments) given the contents that is the 3rd.
  IPC_MESSAGE_CONTROL3(ViewHostMsg_PageContents, GURL, int32, std::wstring)

  // Used to get the extension message bundle.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetExtensionMessageBundle,
                              std::string /* extension id */,
                              SubstitutionMap /* message bundle */)

  // Specifies the URL as the first parameter (a wstring) and thumbnail as
  // binary data as the second parameter.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_Thumbnail,
                      GURL /* url */,
                      ThumbnailScore /* score */,
                      SkBitmap /* bitmap */)

  // Notification that the url for the favicon of a site has been determined.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateFavIconURL,
                      int32 /* page_id */,
                      GURL /* url of the favicon */)

  // Used to tell the parent that the user right clicked on an area of the
  // content area, and a context menu should be shown for it. The params
  // object contains information about the node(s) that were selected when the
  // user right clicked.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ContextMenu, ContextMenuParams)

  // Request that the given URL be opened in the specified manner.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_OpenURL,
                      GURL /* url */,
                      GURL /* referrer */,
                      WindowOpenDisposition /* disposition */)

  // Notify that the preferred size of the content changed.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_DidContentsPreferredSizeChange,
                      gfx::Size /* pref_size */)

  // Following message is used to communicate the values received by the
  // callback binding the JS to Cpp.
  // An instance of browser that has an automation host listening to it can
  // have a javascript send a native value (string, number, boolean) to the
  // listener in Cpp. (DomAutomationController)
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DomOperationResponse,
                      std::string  /* json_string */,
                      int  /* automation_id */)

  // A message from HTML-based UI.  When (trusted) Javascript calls
  // send(message, args), this message is sent to the browser.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DOMUISend,
                      std::string  /* message */,
                      std::string  /* args (as a JSON string) */)

  // A message for an external host.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_ForwardMessageToExternalHost,
                      std::string  /* message */,
                      std::string  /* origin */,
                      std::string  /* target */)

  // A renderer sends this to the browser process when it wants to
  // create a plugin.  The browser will create the plugin process if
  // necessary, and will return a handle to the channel on success.
  // On error an empty string is returned.
  IPC_SYNC_MESSAGE_CONTROL3_2(ViewHostMsg_OpenChannelToPlugin,
                              GURL /* url */,
                              std::string /* mime_type */,
                              std::wstring /* locale */,
                              IPC::ChannelHandle /* handle to channel */,
                              WebPluginInfo /* info */)

  // A renderer sends this to the browser process when it wants to start
  // a new instance of the Native Client process. The browser will launch
  // the process and return a handle to an IMC channel.
  IPC_SYNC_MESSAGE_CONTROL2_3(ViewHostMsg_LaunchNaCl,
                              std::wstring /* url for the NaCl module */,
                              int /* channel number */,
                              nacl::FileDescriptor /* imc channel handle */,
                              base::ProcessHandle /* NaCl process handle */,
                              base::ProcessId /* NaCl process id */)

#if defined(USE_X11)
  // A renderer sends this when it needs a browser-side widget for
  // hosting a windowed plugin. id is the XID of the plugin window, for which
  // the container is created.
  IPC_SYNC_MESSAGE_ROUTED1_0(ViewHostMsg_CreatePluginContainer,
                             gfx::PluginWindowHandle /* id */)

  // Destroy a plugin container previously created using CreatePluginContainer.
  // id is the XID of the plugin window corresponding to the container that is
  // to be destroyed.
  IPC_SYNC_MESSAGE_ROUTED1_0(ViewHostMsg_DestroyPluginContainer,
                             gfx::PluginWindowHandle /* id */)
#endif

  // Clipboard IPC messages

  // This message is used when the object list does not contain a bitmap.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ClipboardWriteObjectsAsync,
      Clipboard::ObjectMap /* objects */)
  // This message is used when the object list contains a bitmap.
  // It is synchronized so that the renderer knows when it is safe to
  // free the shared memory used to transfer the bitmap.
  IPC_SYNC_MESSAGE_CONTROL1_0(ViewHostMsg_ClipboardWriteObjectsSync,
      Clipboard::ObjectMap /* objects */)
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_ClipboardIsFormatAvailable,
                              std::string /* format */,
                              Clipboard::Buffer /* buffer */,
                              bool /* result */)
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_ClipboardReadText,
                              Clipboard::Buffer /* buffer */,
                              string16 /* result */)
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_ClipboardReadAsciiText,
                              Clipboard::Buffer  /* buffer */,
                              std::string /* result */)
  IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_ClipboardReadHTML,
                              Clipboard::Buffer  /* buffer */,
                              string16 /* markup */,
                              GURL /* url */)

#if defined(OS_MACOSX)
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ClipboardFindPboardWriteStringAsync,
      string16 /* text */)
#endif

#if defined(OS_WIN)
  // Request that the given font be loaded by the browser.
  // Please see ResourceMessageFilter::OnLoadFont for details.
  IPC_SYNC_MESSAGE_CONTROL1_0(ViewHostMsg_LoadFont,
                              LOGFONT /* font data */)
#endif  // defined(OS_WIN)

  // Returns WebScreenInfo corresponding to the view.
  // TODO(shess): Provide a mapping from reply_msg->routing_id() to
  // HWND so that we can eliminate the NativeViewId parameter.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetScreenInfo,
                             gfx::NativeViewId /* view */,
                             WebKit::WebScreenInfo /* results */)

  // Send the tooltip text for the current mouse position to the browser.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_SetTooltipText,
                      std::wstring /* tooltip text string */,
                      WebKit::WebTextDirection /* text direction hint */)

  // Notification that the text selection has changed.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_SelectionChanged,
                      std::string /* currently selected text */)

  // Asks the browser to display the file chooser.  The result is returned in a
  // ViewHost_RunFileChooserResponse message.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_RunFileChooser,
                      bool /* multiple_files */,
                      string16 /* title */,
                      FilePath /* Default file name */)

  // Notification that password forms have been seen that are candidates for
  // filling/submitting by the password manager
  IPC_MESSAGE_ROUTED1(ViewHostMsg_PasswordFormsSeen,
                      std::vector<webkit_glue::PasswordForm> /* forms */)

  // Notification that a form has been submitted.  The user hit the button.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_FormFieldValuesSubmitted,
                      webkit_glue::FormFieldValues /* form */)

  // Used to tell the parent the user started dragging in the content area. The
  // WebDropData struct contains contextual information about the pieces of the
  // page the user dragged. The parent uses this notification to initiate a
  // drag session at the OS level.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_StartDragging,
                      WebDropData /* drop_data */,
                      WebKit::WebDragOperationsMask /* ops_allowed */)

  // The page wants to update the mouse cursor during a drag & drop operation.
  // |is_drop_target| is true if the mouse is over a valid drop target.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateDragCursor,
                      WebKit::WebDragOperation /* drag_operation */)

  // Tells the browser to move the focus to the next (previous if reverse is
  // true) focusable element.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_TakeFocus, bool /* reverse */)

  // Notification that the page has an OpenSearch description document
  // associated with it.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_PageHasOSDD,
                      int32 /* page_id */,
                      GURL /* url of OS description document */,
                      bool /* autodetected */)

  // required for synchronizing IME windows.
  // Parameters
  // * control (ViewHostMsg_ImeControl)
  //   It specifies the code for controlling the IME attached to
  //   the browser process. This parameter should be one of the values
  //   listed below.
  //     + IME_DISABLE
  //       Deactivate the IME attached to a browser process.
  //       This code is typically used for notifying a renderer process
  //       moves its input focus to a password input. A browser process
  //       finishes the current composition and deactivate IME.
  //       If a renderer process sets its input focus to another edit
  //       control which is not a password input, it needs to re-activate
  //       IME, it has to send another message with this code IME_MOVE_WINDOWS
  //       and set the new caret position.
  //     + IME_MOVE_WINDOWS
  //       Activate the IME attached to a browser process and set the position
  //       of its IME windows.
  //       This code is typically used for the following cases:
  //         - Notifying a renderer process moves the caret position of the
  //           focused edit control, or;
  //         - Notifying a renderer process moves its input focus from a
  //           password input to an editable control which is NOT a password
  //           input.
  //           A renderer process also has to set caret_rect and
  //           specify the new caret rectangle.
  //     + IME_COMPLETE_COMPOSITION
  //       Finish the current composition.
  //       This code is used for notifying a renderer process moves its
  //       input focus from an editable control being composed to another one
  //       which is NOT a password input. A browser process closes its IME
  //       windows without changing the activation status of its IME, i.e. it
  //       keeps activating its IME.
  // * caret_rect (gfx::Rect)
  //   They specify the rectangle of the input caret.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_ImeUpdateStatus,
                      ViewHostMsg_ImeControl, /* control */
                      gfx::Rect /* caret_rect */)

  // Tells the browser that the renderer is done calculating the number of
  // rendered pages according to the specified settings.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DidGetPrintedPagesCount,
                      int /* rendered document cookie */,
                      int /* number of rendered pages */)

  // Sends back to the browser the rendered "printed page" that was requested by
  // a ViewMsg_PrintPage message or from scripted printing. The memory handle in
  // this message is already valid in the browser process.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_DidPrintPage,
                      ViewHostMsg_DidPrintPage_Params /* page content */)

  // The renderer wants to know the default print settings.
  IPC_SYNC_MESSAGE_ROUTED0_1(ViewHostMsg_GetDefaultPrintSettings,
                             ViewMsg_Print_Params /* default_settings */)

#if defined(OS_WIN) || defined(OS_MACOSX)
  // It's the renderer that controls the printing process when it is generated
  // by javascript. This step is about showing UI to the user to select the
  // final print settings. The output parameter is the same as
  // ViewMsg_PrintPages which is executed implicitly.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_ScriptedPrint,
                              ViewHostMsg_ScriptedPrint_Params,
                              ViewMsg_PrintPages_Params /* settings choosen by
                                                          the user*/)
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  // WebKit and JavaScript error messages to log to the console
  // or debugger UI.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_AddMessageToConsole,
                      std::wstring, /* msg */
                      int32, /* line number */
                      std::wstring /* source id */)

  // Stores new inspector settings in the profile.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateInspectorSettings,
                      std::string  /* raw_settings */)

  // Wraps an IPC message that's destined to the DevToolsClient on
  // DevToolsAgent->browser hop.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardToDevToolsClient,
                      IPC::Message /* one of DevToolsClientMsg_XXX types */)

  // Wraps an IPC message that's destined to the DevToolsAgent on
  // DevToolsClient->browser hop.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardToDevToolsAgent,
                      IPC::Message /* one of DevToolsAgentMsg_XXX types */)

  // Activates (brings to the front) corresponding dev tools window.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_ActivateDevToolsWindow)

  // Closes dev tools window that is inspecting current render_view_host.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_CloseDevToolsWindow)

  // Attaches dev tools window that is inspecting current render_view_host.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DockDevToolsWindow)

  // Detaches dev tools window that is inspecting current render_view_host.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_UndockDevToolsWindow)

  // Updates runtime features store in devtools manager in order to support
  // cross-navigation instrumentation.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DevToolsRuntimeFeatureStateChanged,
                      std::string /* feature */,
                      bool /* enabled */)

  // Send back a string to be recorded by UserMetrics.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UserMetricsRecordAction,
                      std::string /* action */)

  // Send back histograms as vector of pickled-histogram strings.
  IPC_MESSAGE_CONTROL2(ViewHostMsg_RendererHistograms,
                       int, /* sequence number of Renderer Histograms. */
                       std::vector<std::string>)

#if defined USE_TCMALLOC
  // Send back tcmalloc stats output.
  IPC_MESSAGE_CONTROL2(ViewHostMsg_RendererTcmalloc,
                       int          /* pid */,
                       std::string  /* tcmalloc debug output */)
#endif

  // Sends back stats about the V8 heap.
  IPC_MESSAGE_CONTROL2(ViewHostMsg_V8HeapStats,
                       int /* size of heap (allocated from the OS) */,
                       int /* bytes in use */)

  // Request for a DNS prefetch of the names in the array.
  // NameList is typedef'ed std::vector<std::string>
  IPC_MESSAGE_CONTROL1(ViewHostMsg_DnsPrefetch,
                      std::vector<std::string> /* hostnames */)

  // Notifies when default plugin updates status of the missing plugin.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_MissingPluginStatus,
                      int /* status */)

  // Sent by the renderer process to indicate that a plugin instance has
  // crashed.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_CrashedPlugin,
                      FilePath /* plugin_path */)

  // Displays a JavaScript out-of-memory message in the infobar.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_JSOutOfMemory)

  // Displays a box to confirm that the user wants to navigate away from the
  // page. Replies true if yes, false otherwise, the reply string is ignored,
  // but is included so that we can use OnJavaScriptMessageBoxClosed.
  IPC_SYNC_MESSAGE_ROUTED2_2(ViewHostMsg_RunBeforeUnloadConfirm,
                             GURL,        /* in - originating frame URL */
                             std::wstring /* in - alert message */,
                             bool         /* out - success */,
                             std::wstring /* out - This is ignored.*/)

  IPC_MESSAGE_ROUTED3(ViewHostMsg_SendCurrentPageAllSavableResourceLinks,
                      std::vector<GURL> /* all savable resource links */,
                      std::vector<GURL> /* all referrers of resource links */,
                      std::vector<GURL> /* all frame links */)

  IPC_MESSAGE_ROUTED3(ViewHostMsg_SendSerializedHtmlData,
                      GURL /* frame's url */,
                      std::string /* data buffer */,
                      int32 /* complete status */)

  IPC_SYNC_MESSAGE_ROUTED4_1(ViewHostMsg_ShowModalHTMLDialog,
                             GURL /* url */,
                             int /* width */,
                             int /* height */,
                             std::string /* json_arguments */,
                             std::string /* json_retval */)

  IPC_MESSAGE_ROUTED2(ViewHostMsg_DidGetApplicationInfo,
                      int32 /* page_id */,
                      webkit_glue::WebApplicationInfo)

  // Provides the result from running OnMsgShouldClose.  |proceed| matches the
  // return value of the the frame's shouldClose method (which includes the
  // onbeforeunload handler): true if the user decided to proceed with leaving
  // the page.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ShouldClose_ACK,
                      bool /* proceed */)

  // Indicates that the current page has been closed, after a ClosePage
  // message. The parameters are just echoed from the ClosePage request.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ClosePage_ACK,
                      ViewMsg_ClosePage_Params)

  IPC_MESSAGE_ROUTED4(ViewHostMsg_DidDownloadFavIcon,
                      int /* Identifier of the request */,
                      GURL /* URL of the image */,
                      bool /* true if there was a network error */,
                      SkBitmap /* image_data */)

  // Sent to query MIME information.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetMimeTypeFromExtension,
                              FilePath::StringType /* extension */,
                              std::string /* mime_type */)
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetMimeTypeFromFile,
                              FilePath /* file_path */,
                              std::string /* mime_type */)
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetPreferredExtensionForMimeType,
                              std::string /* mime_type */,
                              FilePath::StringType /* extension */)

  // Get the CPBrowsingContext associated with the renderer sending this
  // message.
  IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GetCPBrowsingContext,
                              uint32 /* context */)

  // Sent when the renderer process is done processing a DataReceived
  // message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_DataReceived_ACK,
                      int /* request_id */)

  // Sent when a provisional load on the main frame redirects.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_DidRedirectProvisionalLoad,
                      int /* page_id */,
                      GURL /* last url */,
                      GURL /* url redirected to */)

  // Sent by the renderer process to acknowledge receipt of a
  // DownloadProgress message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_DownloadProgress_ACK,
                      int /* request_id */)

  // Sent by the renderer process to acknowledge receipt of a
  // UploadProgress message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UploadProgress_ACK,
                      int /* request_id */)

  // Sent when the renderer changes the zoom level for a particular host, so the
  // browser can update its records.
  IPC_MESSAGE_CONTROL2(ViewHostMsg_DidZoomHost,
                       std::string /* host */,
                       int /* zoom_level */)

#if defined(OS_WIN)
  // Duplicates a shared memory handle from the renderer to the browser. Then
  // the renderer can flush the handle.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_DuplicateSection,
                             base::SharedMemoryHandle /* renderer handle */,
                             base::SharedMemoryHandle /* browser handle */)
#endif

#if defined(OS_NIX)
  // Asks the browser create a temporary file for the renderer to fill
  // in resulting NativeMetafile in printing.
  IPC_SYNC_MESSAGE_CONTROL0_2(ViewHostMsg_AllocateTempFileForPrinting,
                              base::FileDescriptor /* temp file fd */,
                              int /* fd in browser*/)
  IPC_MESSAGE_CONTROL1(ViewHostMsg_TempFileForPrintingWritten,
                       int /* fd in browser */)
#endif

#if defined(OS_MACOSX)
  // Asks the browser create a block of shared memory for the renderer to pass
  // NativeMetafile data to the browser.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_AllocatePDFTransport,
                             size_t /* buffer size */,
                             base::SharedMemoryHandle /* browser handle */)
#endif

  // Provide the browser process with information about the WebCore resource
  // cache.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ResourceTypeStats,
                       WebKit::WebCache::ResourceTypeStats)

  // Notify the browser that this render process can or can't be suddenly
  // terminated.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_SuddenTerminationChanged,
                       bool /* enabled */)

  // Returns the window location of the window this widget is embeded.
  // TODO(shess): Provide a mapping from reply_msg->routing_id() to
  // HWND so that we can eliminate the NativeViewId parameter.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetRootWindowRect,
                             gfx::NativeViewId /* window */,
                             gfx::Rect /* Out: Window location */)

  // Informs the browser of a new appcache host.
  IPC_MESSAGE_CONTROL1(AppCacheMsg_RegisterHost,
                       int /* host_id */)

  // Informs the browser of an appcache host being destroyed.
  IPC_MESSAGE_CONTROL1(AppCacheMsg_UnregisterHost,
                       int /* host_id */)

  // Initiates the cache selection algorithm for the given host.
  // This is sent prior to any subresource loads. An AppCacheMsg_CacheSelected
  // message will be sent in response.
  // 'host_id' indentifies a specific document or worker
  // 'document_url' the url of the main resource
  // 'appcache_document_was_loaded_from' the id of the appcache the main
  //     resource was loaded from or kNoCacheId
  // 'opt_manifest_url' the manifest url specified in the <html> tag if any
  IPC_MESSAGE_CONTROL4(AppCacheMsg_SelectCache,
                       int /* host_id */,
                       GURL  /* document_url */,
                       int64 /* appcache_document_was_loaded_from */,
                       GURL  /* opt_manifest_url */)

  // Informs the browser of a 'foreign' entry in an appcache.
  IPC_MESSAGE_CONTROL3(AppCacheMsg_MarkAsForeignEntry,
                       int /* host_id */,
                       GURL  /* document_url */,
                       int64 /* appcache_document_was_loaded_from */)

  // Returns the status of the appcache associated with host_id.
  IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheMsg_GetStatus,
                              int /* host_id */,
                              appcache::Status)

  // Initiates an update of the appcache associated with host_id.
  IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheMsg_StartUpdate,
                              int /* host_id */,
                              bool /* success */)

  // Swaps a new pending appcache, if there is one, into use for host_id.
  IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheMsg_SwapCache,
                              int /* host_id */,
                              bool /* success */)

  // Returns the resizer box location in the window this widget is embeded.
  // Important for Mac OS X, but not Win or Linux.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetRootWindowResizerRect,
                             gfx::NativeViewId /* window */,
                             gfx::Rect /* Out: Window location */)

  // Queries the browser for suggestion for autofill in a form input field.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_QueryFormFieldAutofill,
                      int /* id of this message */,
                      string16 /* field name */,
                      string16 /* user entered text */)

  // Instructs the browser to remove the specified autofill-entry from the
  // database.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_RemoveAutofillEntry,
                      string16 /* field name */,
                      string16 /* value */)

  // Get the list of proxies to use for |url|, as a semicolon delimited list
  // of "<TYPE> <HOST>:<PORT>" | "DIRECT". See also
  // PluginProcessHostMsg_ResolveProxy which does the same thing.
  IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_ResolveProxy,
                              GURL /* url */,
                              int /* network error */,
                              std::string /* proxy list */)

  // Request that got sent to browser for creating an audio output stream
  IPC_MESSAGE_ROUTED2(ViewHostMsg_CreateAudioStream,
                      int /* stream_id */,
                      ViewHostMsg_Audio_CreateStream)

  // Tell the browser the audio buffer prepared for stream
  // (render_view_id, stream_id) is filled and is ready to be consumed.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_NotifyAudioPacketReady,
                      int /* stream_id */,
                      size_t /* packet size */)

  // Start buffering the audio stream specified by (render_view_id, stream_id).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_PlayAudioStream,
                      int /* stream_id */)

  // Pause the audio stream specified by (render_view_id, stream_id).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_PauseAudioStream,
                      int /* stream_id */)

  // Close an audio stream specified by (render_view_id, stream_id).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_CloseAudioStream,
                      int /* stream_id */)

  // Get audio volume of the stream specified by (render_view_id, stream_id).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_GetAudioVolume,
                      int /* stream_id */)

  // Set audio volume of the stream specified by (render_view_id, stream_id).
  // TODO(hclam): change this to vector if we have channel numbers other than 2.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_SetAudioVolume,
                      int /* stream_id */,
                      double /* volume */)

  // A renderer sends this message when an extension process starts an API
  // request. The browser will always respond with a ViewMsg_ExtensionResponse.
  IPC_MESSAGE_ROUTED4(ViewHostMsg_ExtensionRequest,
                      std::string /* name */,
                      ListValue /* argument */,
                      int /* callback id */,
                      bool /* has_callback */)

  // Notify the browser that this renderer added a listener to an event.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ExtensionAddListener,
                       std::string /* name */)

  // Notify the browser that this renderer removed a listener from an event.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ExtensionRemoveListener,
                       std::string /* name */)

#if defined(OS_MACOSX) || defined(CHROMIUM_CAPSICUM)
  // On OSX, we cannot allocated shared memory from within the sandbox, so
  // this call exists for the renderer to ask the browser to allocate memory
  // on its behalf. We return a file descriptor to the POSIX shared memory.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_AllocTransportDIB,
                              size_t, /* bytes requested */
                              TransportDIB::Handle /* DIB */)

  // Since the browser keeps handles to the allocated transport DIBs, this
  // message is sent to tell the browser that it may release them when the
  // renderer is finished with them.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_FreeTransportDIB,
                       TransportDIB::Id /* DIB id */)
#endif

  // A renderer sends this to the browser process when it wants to create a
  // worker.  The browser will create the worker process if necessary, and
  // will return the route id on success.  On error returns MSG_ROUTING_NONE.
  IPC_SYNC_MESSAGE_CONTROL4_1(ViewHostMsg_CreateWorker,
                              GURL /* url */,
                              bool /* is_shared */,
                              string16 /* name */,
                              int /* render_view_route_id */,
                              int /* route_id */)

  // This message is sent to the browser to see if an instance of this shared
  // worker already exists (returns route_id != MSG_ROUTING_NONE). This route
  // id can be used to forward messages to the worker via ForwardToWorker. If a
  // non-empty name is passed, also validates that the url matches the url of
  // the existing worker. If a matching worker is found, the passed-in
  // document_id is associated with that worker, to ensure that the worker
  // stays alive until the document is detached.
  IPC_SYNC_MESSAGE_CONTROL3_2(ViewHostMsg_LookupSharedWorker,
                              GURL /* url */,
                              string16 /* name */,
                              unsigned long long /* document_id */,
                              int /* route_id */,
                              bool /* url_mismatch */)

  // A renderer sends this to the browser process when a document has been
  // detached. The browser will use this to constrain the lifecycle of worker
  // processes (SharedWorkers are shut down when their last associated document
  // is detached).
  IPC_MESSAGE_CONTROL1(ViewHostMsg_DocumentDetached,
                       unsigned long long /* document_id */)

  // A message sent to the browser on behalf of a renderer which wants to show
  // a desktop notification.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_ShowDesktopNotification,
                      GURL /* origin */,
                      GURL /* contents_url */,
                      int /* notification_id */)
  IPC_MESSAGE_ROUTED5(ViewHostMsg_ShowDesktopNotificationText,
                      GURL /* origin */,
                      GURL /* icon_url */,
                      string16 /* title */,
                      string16 /* text */,
                      int /* notification_id */)
  IPC_MESSAGE_ROUTED1(ViewHostMsg_CancelDesktopNotification,
                      int /* notification_id */ )
  IPC_MESSAGE_ROUTED2(ViewHostMsg_RequestNotificationPermission,
                      GURL /* origin */,
                      int /* callback_context */)
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_CheckNotificationPermission,
                             GURL /* origin */,
                             int /* permission_result */)

  // Sent if the worker object has sent a ViewHostMsg_CreateDedicatedWorker
  // message and not received a ViewMsg_WorkerCreated reply, but in the
  // mean time it's destroyed.  This tells the browser to not create the queued
  // worker.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_CancelCreateDedicatedWorker,
                       int /* route_id */)

  // Wraps an IPC message that's destined to the worker on the renderer->browser
  // hop.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ForwardToWorker,
                       IPC::Message /* message */)

  // Open a channel to all listening contexts owned by the extension with
  // the given ID.  This always returns a valid port ID which can be used for
  // sending messages.  If an error occurred, the opener will be notified
  // asynchronously.
  IPC_SYNC_MESSAGE_CONTROL4_1(ViewHostMsg_OpenChannelToExtension,
                              int /* routing_id */,
                              std::string /* source_extension_id */,
                              std::string /* target_extension_id */,
                              std::string /* channel_name */,
                              int /* port_id */)

  // Get a port handle to the given tab.  The handle can be used for sending
  // messages to the extension.
  IPC_SYNC_MESSAGE_CONTROL4_1(ViewHostMsg_OpenChannelToTab,
                              int /* routing_id */,
                              int /* tab_id */,
                              std::string /* extension_id */,
                              std::string /* channel_name */,
                              int /* port_id */)

  // Send a message to an extension process.  The handle is the value returned
  // by ViewHostMsg_OpenChannelTo*.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_ExtensionPostMessage,
                      int /* port_id */,
                      std::string /* message */)

  // Send a message to an extension process.  The handle is the value returned
  // by ViewHostMsg_OpenChannelTo*.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ExtensionCloseChannel,
                       int /* port_id */)

  // Sent as a result of a focus change in the renderer (if accessibility is
  // enabled), to notify the browser side that its accessibility focus needs to
  // change as well. Takes the id of the accessibility object that now has
  // focus.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_AccessibilityFocusChange,
                      int /* accessibility object id */)

  // Message sent from the renderer to the browser to request that the browser
  // close all idle sockets.  Used for debugging/testing.
  IPC_MESSAGE_CONTROL0(ViewHostMsg_CloseIdleConnections)

  // Message sent from the renderer to the browser to request that the browser
  // close all idle sockets.  Used for debugging/testing.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_SetCacheMode,
                       bool /* enabled */)

  // There's one LocalStorage namespace per profile and one SessionStorage
  // namespace per tab.  This will find or create the proper namespace.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_DOMStorageNamespaceId,
                              DOMStorageType /* storage_type */,
                              int64 /* new_namespace_id */)

  // Used by SessionStorage to clone a namespace per the spec.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_DOMStorageCloneNamespaceId,
                              int64 /* namespace_id to clone */,
                              int64 /* new_namespace_id */)

  // Get the storage area id for a particular origin within a namespace.
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_DOMStorageStorageAreaId,
                              int64 /* namespace_id */,
                              string16 /* origin */,
                              int64 /* storage_area_id */)

  // Get the length of a storage area.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_DOMStorageLength,
                              int64 /* storage_area_id */,
                              unsigned /* length */)

  // Get a the ith key within a storage area.
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_DOMStorageKey,
                              int64 /* storage_area_id */,
                              unsigned /* index */,
                              NullableString16 /* key */)

  // Get a value based on a key from a storage area.
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_DOMStorageGetItem,
                              int64 /* storage_area_id */,
                              string16 /* key */,
                              NullableString16 /* value */)

  // Set a value that's associated with a key in a storage area.
  IPC_SYNC_MESSAGE_CONTROL4_1(ViewHostMsg_DOMStorageSetItem,
                              int64 /* storage_area_id */,
                              string16 /* key */,
                              string16 /* value */,
                              GURL /* url */,
                              bool /* quota_exception */)

  // Remove the value associated with a key in a storage area.
  IPC_MESSAGE_CONTROL3(ViewHostMsg_DOMStorageRemoveItem,
                       int64 /* storage_area_id */,
                       string16 /* key */,
                       GURL /* url */)

  // Clear the storage area.
  IPC_MESSAGE_CONTROL2(ViewHostMsg_DOMStorageClear,
                       int64 /* storage_area_id */,
                       GURL /* url */)

  // Get file size in bytes. Set result to -1 if failed to get the file size.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetFileSize,
                              FilePath /* path */,
                              int64 /* result */)

  // Sent by the renderer process to acknowledge receipt of a
  // ViewMsg_CSSInsertRequest message and css has been inserted into the frame.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_OnCSSInserted)

  // Asks the browser process to open a DB file with the given name
  IPC_MESSAGE_CONTROL3(ViewHostMsg_DatabaseOpenFile,
                       string16 /* vfs file name */,
                       int /* desired flags */,
                       int32 /* a unique message ID */)

  // Asks the browser process to delete a DB file
  IPC_MESSAGE_CONTROL3(ViewHostMsg_DatabaseDeleteFile,
                       string16 /* vfs file name */,
                       bool /* whether or not to sync the directory */,
                       int32 /* a unique message ID */)

  // Asks the browser process to return the attributes of a DB file
  IPC_MESSAGE_CONTROL2(ViewHostMsg_DatabaseGetFileAttributes,
                       string16 /* vfs file name */,
                       int32 /* a unique message ID */)

  // Asks the browser process to return the size of a DB file
  IPC_MESSAGE_CONTROL2(ViewHostMsg_DatabaseGetFileSize,
                       string16 /* vfs file name */,
                       int32 /* a unique message ID */)

  // Notifies the browser process that a new database has been opened
  IPC_MESSAGE_CONTROL4(ViewHostMsg_DatabaseOpened,
                       string16 /* origin identifier */,
                       string16 /* database name */,
                       string16 /* database description */,
                       int64 /* estimated size */)

  // Notifies the browser process that a database might have been modified
  IPC_MESSAGE_CONTROL2(ViewHostMsg_DatabaseModified,
                       string16 /* origin identifier */,
                       string16 /* database name */)

  // Notifies the browser process that a database is about to close
  IPC_MESSAGE_CONTROL2(ViewHostMsg_DatabaseClosed,
                       string16 /* origin identifier */,
                       string16 /* database name */)

  //---------------------------------------------------------------------------
  // Socket Stream messages:
  // These are messages from the SocketStreamHandle to the browser.

  // Open new Socket Stream for the |socket_url| identified by |socket_id|
  // in the renderer process.
  // The browser starts connecting asynchronously.
  // Once Socket Stream connection is established, the browser will send
  // ViewMsg_SocketStream_Connected back.
  IPC_MESSAGE_CONTROL2(ViewHostMsg_SocketStream_Connect,
                       GURL /* socket_url */,
                       int /* socket_id */)

  // Request to send data on the Socket Stream.
  // SocketStreamHandle can send data at most |max_pending_send_allowed| bytes,
  // which is given by ViewMsg_SocketStream_Connected at any time.
  // The number of pending bytes can be tracked by size of |data| sent
  // and |amount_sent| parameter of ViewMsg_SocketStream_DataSent.
  // That is, the following constraints is applied:
  //  (accumulated total of |data|) - (accumulated total of |amount_sent|)
  // <= |max_pending_send_allowed|
  // If the SocketStreamHandle ever tries to exceed the
  // |max_pending_send_allowed|, the connection will be closed.
  IPC_MESSAGE_CONTROL2(ViewHostMsg_SocketStream_SendData,
                       int /* socket_id */,
                       std::vector<char> /* data */)

  // Request to close the Socket Stream.
  // The browser will send ViewMsg_SocketStream_Closed back when the Socket
  // Stream is completely closed.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_SocketStream_Close,
                       int /* socket_id */)

  //---------------------------------------------------------------------------
  // Request for cryptographic operation messages:
  // These are messages from the renderer to the browser to perform a
  // cryptographic operation.

  // Asks the browser process to generate a keypair for grabbing a client
  // certificate from a CA (<keygen> tag), and returns the signed public
  // key and challenge string.
  IPC_SYNC_MESSAGE_CONTROL3_1(ViewHostMsg_Keygen,
                              uint32 /* key size index */,
                              std::string /* challenge string */,
                              GURL /* URL of requestor */,
                              std::string /* signed public key and challenge */)

  // The renderer has tried to spell check a word, but couldn't because no
  // dictionary was available to load. Request that the browser find an
  // appropriate dictionary and return it.
  IPC_MESSAGE_CONTROL0(ViewHostMsg_SpellChecker_RequestDictionary)

  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_SpellChecker_PlatformCheckSpelling,
                              string16 /* word */,
                              int /* document tag */,
                              bool /* correct */)

  IPC_SYNC_MESSAGE_CONTROL1_1(
      ViewHostMsg_SpellChecker_PlatformFillSuggestionList,
      string16 /* word */,
      std::vector<string16> /* suggestions */)

IPC_END_MESSAGES(ViewHost)
