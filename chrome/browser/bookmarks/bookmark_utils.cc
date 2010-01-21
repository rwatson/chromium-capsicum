// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include "app/clipboard/clipboard.h"
#include "app/drag_drop_types.h"
#include "app/l10n_util.h"
#include "app/tree_node_iterator.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_strings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "views/event.h"

using base::Time;

namespace {

// A PageNavigator implementation that creates a new Browser. This is used when
// opening a url and there is no Browser open. The Browser is created the first
// time the PageNavigator method is invoked.
class NewBrowserPageNavigator : public PageNavigator {
 public:
  explicit NewBrowserPageNavigator(Profile* profile)
      : profile_(profile),
        browser_(NULL) {}

  virtual ~NewBrowserPageNavigator() {
    if (browser_)
      browser_->window()->Show();
  }

  Browser* browser() const { return browser_; }

  virtual void OpenURL(const GURL& url,
                       const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) {
    if (!browser_) {
      Profile* profile = (disposition == OFF_THE_RECORD) ?
          profile_->GetOffTheRecordProfile() : profile_;
      browser_ = Browser::Create(profile);
      // Always open the first tab in the foreground.
      disposition = NEW_FOREGROUND_TAB;
    }
    browser_->OpenURL(url, referrer, NEW_FOREGROUND_TAB, transition);
  }

 private:
  Profile* profile_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(NewBrowserPageNavigator);
};

void CloneDragDataImpl(BookmarkModel* model,
                       const BookmarkDragData::Element& element,
                       const BookmarkNode* parent,
                       int index_to_add_at) {
  if (element.is_url) {
    model->AddURL(parent, index_to_add_at, element.title, element.url);
  } else {
    const BookmarkNode* new_folder = model->AddGroup(parent,
                                                     index_to_add_at,
                                                     element.title);
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      CloneDragDataImpl(model, element.children[i], new_folder, i);
  }
}

// Returns the number of descendants of node that are of type url.
int DescendantURLCount(const BookmarkNode* node) {
  int result = 0;
  for (int i = 0; i < node->GetChildCount(); ++i) {
    const BookmarkNode* child = node->GetChild(i);
    if (child->is_url())
      result++;
    else
      result += DescendantURLCount(child);
  }
  return result;
}

// Implementation of OpenAll. Opens all nodes of type URL and recurses for
// groups. |navigator| is the PageNavigator used to open URLs. After the first
// url is opened |opened_url| is set to true and |navigator| is set to the
// PageNavigator of the last active tab. This is done to handle a window
// disposition of new window, in which case we want subsequent tabs to open in
// that window.
void OpenAllImpl(const BookmarkNode* node,
                 WindowOpenDisposition initial_disposition,
                 PageNavigator** navigator,
                 bool* opened_url) {
  if (node->is_url()) {
    WindowOpenDisposition disposition;
    if (*opened_url)
      disposition = NEW_BACKGROUND_TAB;
    else
      disposition = initial_disposition;
    (*navigator)->OpenURL(node->GetURL(), GURL(), disposition,
                          PageTransition::AUTO_BOOKMARK);
    if (!*opened_url) {
      *opened_url = true;
      // We opened the first URL which may have opened a new window or clobbered
      // the current page, reset the navigator just to be sure.
      Browser* new_browser = BrowserList::GetLastActive();
      if (new_browser) {
        TabContents* current_tab = new_browser->GetSelectedTabContents();
        DCHECK(new_browser && current_tab);
        if (new_browser && current_tab)
          *navigator = current_tab;
      }  // else, new_browser == NULL, which happens during testing.
    }
  } else {
    // Group, recurse through children.
    for (int i = 0; i < node->GetChildCount(); ++i) {
      OpenAllImpl(node->GetChild(i), initial_disposition, navigator,
                  opened_url);
    }
  }
}

bool ShouldOpenAll(gfx::NativeWindow parent,
                   const std::vector<const BookmarkNode*>& nodes) {
  int descendant_count = 0;
  for (size_t i = 0; i < nodes.size(); ++i)
    descendant_count += DescendantURLCount(nodes[i]);
  if (descendant_count < bookmark_utils::num_urls_before_prompting)
    return true;

  std::wstring message =
      l10n_util::GetStringF(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
                            IntToWString(descendant_count));
#if defined(OS_WIN)
  return MessageBox(parent, message.c_str(),
                    l10n_util::GetString(IDS_PRODUCT_NAME).c_str(),
                    MB_YESNO | MB_ICONWARNING | MB_TOPMOST) == IDYES;
#else
  // TODO(port): Display a dialog prompt.
  NOTIMPLEMENTED();
  return true;
#endif
}

// Comparison function that compares based on date modified of the two nodes.
bool MoreRecentlyModified(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_group_modified() > n2->date_group_modified();
}

// Returns true if |text| contains each string in |words|. This is used when
// searching for bookmarks.
bool DoesBookmarkTextContainWords(const std::wstring& text,
                                  const std::vector<std::wstring>& words) {
  for (size_t i = 0; i < words.size(); ++i) {
    if (text.find(words[i]) == std::wstring::npos)
      return false;
  }
  return true;
}

// Returns true if |node|s title or url contains the strings in |words|.
// |languages| argument is user's accept-language setting to decode IDN.
bool DoesBookmarkContainWords(const BookmarkNode* node,
                              const std::vector<std::wstring>& words,
                              const std::wstring& languages) {
  return
      DoesBookmarkTextContainWords(
          l10n_util::ToLower(node->GetTitle()), words) ||
      DoesBookmarkTextContainWords(
          l10n_util::ToLower(UTF8ToWide(node->GetURL().spec())), words) ||
      DoesBookmarkTextContainWords(l10n_util::ToLower(net::FormatUrl(
          node->GetURL(), languages, false, true, NULL, NULL, NULL)), words);
}

}  // namespace

namespace bookmark_utils {

int num_urls_before_prompting = 15;

int PreferredDropOperation(int source_operations, int operations) {
  int common_ops = (source_operations & operations);
  if (!common_ops)
    return 0;
  if (DragDropTypes::DRAG_COPY & common_ops)
    return DragDropTypes::DRAG_COPY;
  if (DragDropTypes::DRAG_LINK & common_ops)
    return DragDropTypes::DRAG_LINK;
  if (DragDropTypes::DRAG_MOVE & common_ops)
    return DragDropTypes::DRAG_MOVE;
  return DragDropTypes::DRAG_NONE;
}

int BookmarkDragOperation(const BookmarkNode* node) {
  if (node->is_url()) {
    return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE |
           DragDropTypes::DRAG_LINK;
  }
  return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
}

int BookmarkDropOperation(Profile* profile,
                          const views::DropTargetEvent& event,
                          const BookmarkDragData& data,
                          const BookmarkNode* parent,
                          int index) {
  if (data.IsFromProfile(profile) && data.size() > 1)
    // Currently only accept one dragged node at a time.
    return DragDropTypes::DRAG_NONE;

  if (!bookmark_utils::IsValidDropLocation(profile, data, parent, index))
    return DragDropTypes::DRAG_NONE;

  if (data.GetFirstNode(profile)) {
    // User is dragging from this profile: move.
    return DragDropTypes::DRAG_MOVE;
  }
  // User is dragging from another app, copy.
  return PreferredDropOperation(event.GetSourceOperations(),
      DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK);
}

int PerformBookmarkDrop(Profile* profile,
                        const BookmarkDragData& data,
                        const BookmarkNode* parent_node,
                        int index) {
  const BookmarkNode* dragged_node = data.GetFirstNode(profile);
  BookmarkModel* model = profile->GetBookmarkModel();
  if (dragged_node) {
    // Drag from same profile, do a move.
    model->Move(dragged_node, parent_node, index);
    return DragDropTypes::DRAG_MOVE;
  } else if (data.has_single_url()) {
    // New URL, add it at the specified location.
    std::wstring title = data.elements[0].title;
    if (title.empty()) {
      // No title, use the host.
      title = UTF8ToWide(data.elements[0].url.host());
      if (title.empty())
        title = l10n_util::GetString(IDS_BOOMARK_BAR_UNKNOWN_DRAG_TITLE);
    }
    model->AddURL(parent_node, index, title, data.elements[0].url);
    return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK;
  } else {
    // Dropping a group from different profile. Always accept.
    bookmark_utils::CloneDragData(model, data.elements, parent_node, index);
    return DragDropTypes::DRAG_COPY;
  }
}

bool IsValidDropLocation(Profile* profile,
                         const BookmarkDragData& data,
                         const BookmarkNode* drop_parent,
                         int index) {
  if (!drop_parent->is_folder()) {
    NOTREACHED();
    return false;
  }

  if (!data.is_valid())
    return false;

  if (data.IsFromProfile(profile)) {
    std::vector<const BookmarkNode*> nodes = data.GetNodes(profile);
    for (size_t i = 0; i < nodes.size(); ++i) {
      // Don't allow the drop if the user is attempting to drop on one of the
      // nodes being dragged.
      const BookmarkNode* node = nodes[i];
      int node_index = (drop_parent == node->GetParent()) ?
          drop_parent->IndexOfChild(nodes[i]) : -1;
      if (node_index != -1 && (index == node_index || index == node_index + 1))
        return false;

      // drop_parent can't accept a child that is an ancestor.
      if (drop_parent->HasAncestor(node))
        return false;
    }
    return true;
  }
  // From the same profile, always accept.
  return true;
}

void CloneDragData(BookmarkModel* model,
                   const std::vector<BookmarkDragData::Element>& elements,
                   const BookmarkNode* parent,
                   int index_to_add_at) {
  if (!parent->is_folder() || !model) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < elements.size(); ++i)
    CloneDragDataImpl(model, elements[i], parent, index_to_add_at + i);
}

void OpenAll(gfx::NativeWindow parent,
             Profile* profile,
             PageNavigator* navigator,
             const std::vector<const BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition) {
  if (!ShouldOpenAll(parent, nodes))
    return;

  NewBrowserPageNavigator navigator_impl(profile);
  if (!navigator) {
    Browser* browser =
        BrowserList::FindBrowserWithType(profile, Browser::TYPE_NORMAL);
    if (!browser || !browser->GetSelectedTabContents()) {
      navigator = &navigator_impl;
    } else {
      if (initial_disposition != NEW_WINDOW &&
          initial_disposition != OFF_THE_RECORD) {
        browser->window()->Activate();
      }
      navigator = browser->GetSelectedTabContents();
    }
  }

  bool opened_url = false;
  for (size_t i = 0; i < nodes.size(); ++i)
    OpenAllImpl(nodes[i], initial_disposition, &navigator, &opened_url);
}

void OpenAll(gfx::NativeWindow parent,
             Profile* profile,
             PageNavigator* navigator,
             const BookmarkNode* node,
             WindowOpenDisposition initial_disposition) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  OpenAll(parent, profile, navigator, nodes, initial_disposition);
}

void CopyToClipboard(BookmarkModel* model,
                     const std::vector<const BookmarkNode*>& nodes,
                     bool remove_nodes) {
#if defined(OS_WIN) || defined(OS_NIX)
  if (nodes.empty())
    return;

  BookmarkDragData(nodes).WriteToClipboard(NULL);

  if (remove_nodes) {
    for (size_t i = 0; i < nodes.size(); ++i) {
      model->Remove(nodes[i]->GetParent(),
                    nodes[i]->GetParent()->IndexOfChild(nodes[i]));
    }
  }
#else
  // Not implemented on mac yet.
#endif
}

void PasteFromClipboard(BookmarkModel* model,
                        const BookmarkNode* parent,
                        int index) {
#if defined(OS_WIN) || defined(OS_NIX)
  if (!parent)
    return;

  BookmarkDragData bookmark_data;
  if (!bookmark_data.ReadFromClipboard())
    return;

  if (index == -1)
    index = parent->GetChildCount();
  bookmark_utils::CloneDragData(model, bookmark_data.elements, parent, index);
#else
  // Not implemented on mac yet.
#endif
}

bool CanPasteFromClipboard(const BookmarkNode* node) {
  if (!node)
    return false;

#if defined(OS_MACOSX)
  NOTIMPLEMENTED();
  return false;
#else
  return g_browser_process->clipboard()->IsFormatAvailableByString(
      BookmarkDragData::kClipboardFormatString, Clipboard::BUFFER_STANDARD);
#endif
}

std::string GetNameForURL(const GURL& url) {
  if (url.is_valid()) {
    return WideToUTF8(net::GetSuggestedFilename(
        url, std::string(), std::string(), FilePath()).ToWStringHack());
  } else {
    return l10n_util::GetStringUTF8(IDS_APP_UNTITLED_SHORTCUT_FILE_NAME);
  }
}

std::vector<const BookmarkNode*> GetMostRecentlyModifiedGroups(
    BookmarkModel* model,
    size_t max_count) {
  std::vector<const BookmarkNode*> nodes;
  TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* parent = iterator.Next();
    if (parent->is_folder() && parent->date_group_modified() > base::Time()) {
      if (max_count == 0) {
        nodes.push_back(parent);
      } else {
        std::vector<const BookmarkNode*>::iterator i =
            std::upper_bound(nodes.begin(), nodes.end(), parent,
                             &MoreRecentlyModified);
        if (nodes.size() < max_count || i != nodes.end()) {
          nodes.insert(i, parent);
          while (nodes.size() > max_count)
            nodes.pop_back();
        }
      }
    }  // else case, the root node, which we don't care about or imported nodes
       // (which have a time of 0).
  }

  if (nodes.size() < max_count) {
    // Add the bookmark bar and other nodes if there is space.
    if (find(nodes.begin(), nodes.end(), model->GetBookmarkBarNode()) ==
        nodes.end()) {
      nodes.push_back(model->GetBookmarkBarNode());
    }

    if (nodes.size() < max_count &&
        find(nodes.begin(), nodes.end(), model->other_node()) == nodes.end()) {
      nodes.push_back(model->other_node());
    }
  }
  return nodes;
}

void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<const BookmarkNode*>* nodes) {
  TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_url()) {
      std::vector<const BookmarkNode*>::iterator insert_position =
          std::upper_bound(nodes->begin(), nodes->end(), node,
                           &MoreRecentlyAdded);
      if (nodes->size() < count || insert_position != nodes->end()) {
        nodes->insert(insert_position, node);
        while (nodes->size() > count)
          nodes->pop_back();
      }
    }
  }
}

bool MoreRecentlyAdded(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_added() > n2->date_added();
}

void GetBookmarksContainingText(BookmarkModel* model,
                                const std::wstring& text,
                                size_t max_count,
                                const std::wstring& languages,
                                std::vector<const BookmarkNode*>* nodes) {
  std::vector<std::wstring> words;
  QueryParser parser;
  parser.ExtractQueryWords(l10n_util::ToLower(text), &words);
  if (words.empty())
    return;

  TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_url() && DoesBookmarkContainWords(node, words, languages)) {
      nodes->push_back(node);
      if (nodes->size() == max_count)
        return;
    }
  }
}

bool DoesBookmarkContainText(const BookmarkNode* node,
                             const std::wstring& text,
                             const std::wstring& languages) {
  std::vector<std::wstring> words;
  QueryParser parser;
  parser.ExtractQueryWords(l10n_util::ToLower(text), &words);
  if (words.empty())
    return false;

  return (node->is_url() && DoesBookmarkContainWords(node, words, languages));
}

static const BookmarkNode* CreateNewNode(BookmarkModel* model,
    const BookmarkNode* parent, const BookmarkEditor::EditDetails& details,
    const std::wstring& new_title, const GURL& new_url,
    BookmarkEditor::Handler* handler) {
  const BookmarkNode* node;
  if (details.type == BookmarkEditor::EditDetails::NEW_URL) {
    node = model->AddURL(parent, parent->GetChildCount(), new_title, new_url);
  } else if (details.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
    node = model->AddGroup(parent, parent->GetChildCount(), new_title);
    for (size_t i = 0; i < details.urls.size(); ++i) {
      model->AddURL(node, node->GetChildCount(), details.urls[i].second,
                    details.urls[i].first);
    }
    model->SetDateGroupModified(parent, Time::Now());
  } else {
    NOTREACHED();
    return NULL;
  }

  if (handler)
    handler->NodeCreated(node);
  return node;
}

const BookmarkNode* ApplyEditsWithNoGroupChange(BookmarkModel* model,
    const BookmarkNode* parent, const BookmarkEditor::EditDetails& details,
    const std::wstring& new_title, const GURL& new_url,
    BookmarkEditor::Handler* handler) {
  if (details.type == BookmarkEditor::EditDetails::NEW_URL ||
      details.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
    return CreateNewNode(model, parent, details, new_title, new_url, handler);
  }

  const BookmarkNode* node = details.existing_node;
  DCHECK(node);
  const BookmarkNode* old_parent = node->GetParent();
  int old_index = old_parent ? old_parent->IndexOfChild(node) : -1;

  // If we're not showing the tree we only need to modify the node.
  if (old_index == -1) {
    NOTREACHED();
    return node;
  }

  if (new_url != node->GetURL()) {
    // TODO(sky): need SetURL on the model.
    const BookmarkNode* new_node = model->AddURLWithCreationTime(old_parent,
        old_index, new_title, new_url, node->date_added());
    model->Remove(old_parent, old_index + 1);
    return new_node;
  } else {
    model->SetTitle(node, new_title);
  }
  return node;
}

const BookmarkNode* ApplyEditsWithPossibleGroupChange(BookmarkModel* model,
    const BookmarkNode* new_parent, const BookmarkEditor::EditDetails& details,
    const std::wstring& new_title, const GURL& new_url,
    BookmarkEditor::Handler* handler) {
  if (details.type == BookmarkEditor::EditDetails::NEW_URL ||
      details.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
    return CreateNewNode(model, new_parent, details, new_title, new_url,
                         handler);
  }

  const BookmarkNode* node = details.existing_node;
  DCHECK(node);
  const BookmarkNode* old_parent = node->GetParent();
  int old_index = old_parent->IndexOfChild(node);
  const BookmarkNode* return_node = node;

  Time date_added = node->date_added();
  if (new_parent == node->GetParent()) {
    // The parent is the same.
    if (node->is_url() && new_url != node->GetURL()) {
      model->Remove(old_parent, old_index);
      return_node = model->AddURLWithCreationTime(old_parent, old_index,
          new_title, new_url, date_added);
    } else {
      model->SetTitle(node, new_title);
    }
  } else if (node->is_url() && new_url != node->GetURL()) {
    // The parent and URL changed.
    model->Remove(old_parent, old_index);
    return_node = model->AddURLWithCreationTime(new_parent,
        new_parent->GetChildCount(), new_title, new_url, date_added);
  } else {
    // The parent and title changed. Move the node and change the title.
    model->Move(node, new_parent, new_parent->GetChildCount());
    model->SetTitle(node, new_title);
  }
  return return_node;
}

// Formerly in BookmarkBarView
void ToggleWhenVisible(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  const bool always_show = !prefs->GetBoolean(prefs::kShowBookmarkBar);

  // The user changed when the bookmark bar is shown, update the preferences.
  prefs->SetBoolean(prefs::kShowBookmarkBar, always_show);
  prefs->ScheduleSavePersistentPrefs();

  // And notify the notification service.
  Source<Profile> source(profile);
  NotificationService::current()->Notify(
      NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
      source,
      NotificationService::NoDetails());
}

void RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kBookmarkManagerPlacement);
  prefs->RegisterIntegerPref(prefs::kBookmarkManagerSplitLocation, -1);
}

void RegisterUserPrefs(PrefService* prefs) {
  // Formerly in BookmarkBarView
  prefs->RegisterBooleanPref(prefs::kShowBookmarkBar, false);

  // Formerly in BookmarkTableView
  prefs->RegisterIntegerPref(prefs::kBookmarkTableNameWidth1, -1);
  prefs->RegisterIntegerPref(prefs::kBookmarkTableURLWidth1, -1);
  prefs->RegisterIntegerPref(prefs::kBookmarkTableNameWidth2, -1);
  prefs->RegisterIntegerPref(prefs::kBookmarkTableURLWidth2, -1);
  prefs->RegisterIntegerPref(prefs::kBookmarkTablePathWidth, -1);
}

void GetURLAndTitleToBookmark(TabContents* tab_contents,
                              GURL* url,
                              std::wstring* title) {
  *url = tab_contents->GetURL();
  *title = UTF16ToWideHack(tab_contents->GetTitle());
}

void GetURLsForOpenTabs(Browser* browser,
    std::vector<std::pair<GURL, std::wstring> >* urls) {
  for (int i = 0; i < browser->tab_count(); ++i) {
    std::pair<GURL, std::wstring> entry;
    GetURLAndTitleToBookmark(browser->GetTabContentsAt(i), &(entry.first),
                             &(entry.second));
    urls->push_back(entry);
  }
}

}  // namespace bookmark_utils
