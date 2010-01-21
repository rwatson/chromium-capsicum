// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_STRIP_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_STRIP_H_

#include "base/gfx/point.h"
#include "base/message_loop.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/tabs/tab.h"
#include "views/controls/button/image_button.h"
#include "views/view.h"

class BrowserExtender;
class DraggedTabController;
class ScopedMouseCloseWidthCalculator;
class TabStripModel;

namespace views {
class ImageView;
#if defined(TOOLKIT_GTK)
class WidgetGtk;
#elif defined(OS_WIN)
class WidgetWin;
#endif
}

///////////////////////////////////////////////////////////////////////////////
//
// TabStrip
//
//  A View that represents the TabStripModel. The TabStrip has the
//  following responsibilities:
//    - It implements the TabStripModelObserver interface, and acts as a
//      container for Tabs, and is also responsible for creating them.
//    - It takes part in Tab Drag & Drop with Tab, TabDragHelper and
//      DraggedTab, focusing on tasks that require reshuffling other tabs
//      in response to dragged tabs.
//
///////////////////////////////////////////////////////////////////////////////
class TabStrip : public views::View,
                 public TabStripModelObserver,
                 public Tab::TabDelegate,
                 public views::ButtonListener,
                 public MessageLoopForUI::Observer {
 public:
  explicit TabStrip(TabStripModel* model);
  virtual ~TabStrip();

  // Returns true if the TabStrip can accept input events. This returns false
  // when the TabStrip is animating to a new state and as such the user should
  // not be allowed to interact with the TabStrip.
  bool CanProcessInputEvents() const;

  // Accessors for the model and individual Tabs.
  TabStripModel* model() const { return model_; }

  // Destroys the active drag controller.
  void DestroyDragController();

  // Removes the drag source Tab from this TabStrip, and deletes it.
  void DestroyDraggedSourceTab(Tab* tab);

  // Retrieve the ideal bounds for the Tab at the specified index.
  gfx::Rect GetIdealBounds(int index);

  // Returns the currently selected tab.
  Tab* GetSelectedTab() const;

  // Create the new tab button.
  void InitTabStripButtons();

  // Returns the preferred height of this TabStrip. This is based on the
  // typical height of its constituent tabs.
  int GetPreferredHeight();

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Set the background offset used by inactive tabs to match the frame image.
  void SetBackgroundOffset(gfx::Point offset);

  // Returns true if the specified point(TabStrip coordinates) is
  // in the window caption area of the browser window.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Returns true if a drag session is currently active.
  bool IsDragSessionActive() const;

  // Return true if this tab strip is compatible with the provided tab strip.
  // Compatible tab strips can transfer tabs during drag and drop.
  bool IsCompatibleWith(TabStrip* other) const;

  // Sets the bounds of the tab at the specified |tab_index|. |tab_bounds| are
  // in TabStrip coordinates.
  void SetDraggedTabBounds(int tab_index, const gfx::Rect& tab_bounds);

  // Updates the loading animations displayed by tabs in the tabstrip to the
  // next frame.
  void UpdateLoadingAnimations();

  // views::View overrides:
  virtual void PaintChildren(gfx::Canvas* canvas);
  virtual views::View* GetViewByID(int id) const;
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  // NOTE: the drag and drop methods are invoked from FrameView. This is done to
  // allow for a drop region that extends outside the bounds of the TabStrip.
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual bool GetAccessibleName(std::wstring* name);
  virtual void SetAccessibleName(const std::wstring& name);
  virtual views::View* GetViewForPoint(const gfx::Point& point);
  virtual void ThemeChanged();
 protected:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents, int from_index, int to_index,
                        bool pinned_state_changed);
  virtual void TabChangedAt(TabContents* contents, int index,
                            TabChangeType change_type);
  virtual void TabPinnedStateChanged(TabContents* contents, int index);

  // Tab::Delegate implementation:
  virtual bool IsTabSelected(const Tab* tab) const;
  virtual void SelectTab(Tab* tab);
  virtual void CloseTab(Tab* tab);
  virtual bool IsCommandEnabledForTab(
      TabStripModel::ContextMenuCommand command_id, const Tab* tab) const;
  virtual void ExecuteCommandForTab(
      TabStripModel::ContextMenuCommand command_id, Tab* tab);
  virtual void StartHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id, Tab* tab);
  virtual void StopHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id, Tab* tab);
  virtual void StopAllHighlighting();
  virtual void MaybeStartDrag(Tab* tab, const views::MouseEvent& event);
  virtual void ContinueDrag(const views::MouseEvent& event);
  virtual bool EndDrag(bool canceled);
  virtual bool HasAvailableDragActions() const;

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // MessageLoop::Observer implementation:
#if defined(OS_WIN)
  virtual void WillProcessMessage(const MSG& msg);
  virtual void DidProcessMessage(const MSG& msg);
#else
  virtual void WillProcessEvent(GdkEvent* event);
  virtual void DidProcessEvent(GdkEvent* event);
#endif

  // Horizontal gap between pinned and non-pinned tabs.
  static const int pinned_to_non_pinned_gap_;

 private:
  class InsertTabAnimation;
  class MoveTabAnimation;
  class PinAndMoveAnimation;
  class PinnedTabAnimation;
  class RemoveTabAnimation;
  class ResizeLayoutAnimation;
  class TabAnimation;

  friend class DraggedTabController;
  friend class InsertTabAnimation;
  friend class MoveTabAnimation;
  friend class PinAndMoveAnimation;
  friend class PinnedTabAnimation;
  friend class RemoveTabAnimation;
  friend class ResizeLayoutAnimation;
  friend class TabAnimation;

  TabStrip();
  void Init();

  // Set the images for the new tab button.
  void LoadNewTabButtonImage();

  // Retrieves the Tab at the specified index. Take care in using this, you may
  // need to use GetTabAtAdjustForAnimation.
  Tab* GetTabAt(int index) const;

  // Returns the tab at the specified index. If a remove animation is on going
  // and the index is >= the index of the tab being removed, the index is
  // incremented. While a remove operation is on going the indices of the model
  // do not line up with the indices of the view. This method adjusts the index
  // accordingly.
  //
  // Use this instead of GetTabAt if the index comes from the model.
  Tab* GetTabAtAdjustForAnimation(int index) const;

  // Gets the number of Tabs in the collection.
  int GetTabCount() const;

  // Returns the number of pinned tabs.
  int GetPinnedTabCount() const;

  // -- Tab Resize Layout -----------------------------------------------------

  // Returns the exact (unrounded) current width of each tab.
  void GetCurrentTabWidths(double* unselected_width,
                           double* selected_width) const;

  // Returns the exact (unrounded) desired width of each tab, based on the
  // desired strip width and number of tabs.  If
  // |width_of_tabs_for_mouse_close_| is nonnegative we use that value in
  // calculating the desired strip width; otherwise we use the current width.
  // |pinned_tab_count| gives the number of pinned tabs, and |tab_count| the
  // number of pinned and non-pinned tabs.
  void GetDesiredTabWidths(int tab_count,
                           int pinned_tab_count,
                           double* unselected_width,
                           double* selected_width) const;

  // Returns the horizontal offset before the tab at |tab_index|.
  int GetTabHOffset(int tab_index);

  // Perform an animated resize-relayout of the TabStrip immediately.
  void ResizeLayoutTabs();

  // Returns whether or not the cursor is currently in the "tab strip zone"
  // which is defined as the region above the TabStrip and a bit below it.
  bool IsCursorInTabStripZone() const;

  // Ensure that the message loop observer used for event spying is added and
  // removed appropriately so we can tell when to resize layout the tab strip.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

  // -- Link Drag & Drop ------------------------------------------------------

  // Returns the bounds to render the drop at, in screen coordinates. Sets
  // |is_beneath| to indicate whether the arrow is beneath the tab, or above
  // it.
  gfx::Rect GetDropBounds(int drop_index, bool drop_before, bool* is_beneath);

  // Updates the location of the drop based on the event.
  void UpdateDropIndex(const views::DropTargetEvent& event);

  // Sets the location of the drop, repainting as necessary.
  void SetDropIndex(int index, bool drop_before);

  // Returns the drop effect for dropping a URL on the tab strip. This does
  // not query the data in anyway, it only looks at the source operations.
  int GetDropEffect(const views::DropTargetEvent& event);

  // Returns the image to use for indicating a drop on a tab. If is_down is
  // true, this returns an arrow pointing down.
  static SkBitmap* GetDropArrowImage(bool is_down);

  // -- Animations ------------------------------------------------------------

  // Generates the ideal bounds of the TabStrip when all Tabs have finished
  // animating to their desired position/bounds. This is used by the standard
  // Layout method and other callers like the DraggedTabController that need
  // stable representations of Tab positions.
  void GenerateIdealBounds();

  // Lays out the New Tab button, assuming the right edge of the last Tab on
  // the TabStrip at |last_tab_right|.
  void LayoutNewTabButton(double last_tab_right, double unselected_width);

  // A generic Layout method for various classes of TabStrip animations,
  // including Insert, Remove and Resize Layout cases/
  void AnimationLayout(double unselected_width);

  // Starts various types of TabStrip animations.
  void StartResizeLayoutAnimation();
  void StartInsertTabAnimation(int index);
  void StartRemoveTabAnimation(int index, TabContents* contents);
  void StartMoveTabAnimation(int from_index, int to_index);
  void StartPinnedTabAnimation(int index);
  void StartPinAndMoveTabAnimation(int from_index, int to_index,
                                   const gfx::Rect& start_bounds);

  // Notifies the TabStrip that the specified TabAnimation has completed.
  // Optionally a full Layout will be performed, specified by |layout|.
  void FinishAnimation(TabAnimation* animation, bool layout);

  // Finds the index of the TabContents corresponding to |tab| in our
  // associated TabStripModel, or -1 if there is none (e.g. the specified |tab|
  // is being animated closed).
  int GetIndexOfTab(const Tab* tab) const;

  // Calculates the available width for tabs, assuming a Tab is to be closed.
  int GetAvailableWidthForTabs(Tab* last_tab) const;

  // Returns true if the specified point in TabStrip coords is within the
  // hit-test region of the specified Tab.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tabstrip_coords);

  // Cleans up the Tab from the TabStrip at the specified |index|.  This is
  // called from the tab animation code and is not a general-purpose method.
  void RemoveTabAt(int index);

  // Called from the message loop observer when a mouse movement has occurred
  // anywhere over our containing window.
  void HandleGlobalMouseMoveEvent();

  // -- Member Variables ------------------------------------------------------

  // Our model.
  TabStripModel* model_;

  // A factory that is used to construct a delayed callback to the
  // ResizeLayoutTabsNow method.
  ScopedRunnableMethodFactory<TabStrip> resize_layout_factory_;

  // True if the TabStrip has already been added as a MessageLoop observer.
  bool added_as_message_loop_observer_;

  // True if a resize layout animation should be run a short delay after the
  // mouse exits the TabStrip.
  bool needs_resize_layout_;

  // The "New Tab" button.
  views::ImageButton* newtab_button_;
  gfx::Size newtab_button_size_;

  // The current widths of various types of tabs.  We save these so that, as
  // users close tabs while we're holding them at the same size, we can lay out
  // tabs exactly and eliminate the "pixel jitter" we'd get from just leaving
  // them all at their existing, rounded widths.
  double current_unselected_width_;
  double current_selected_width_;

  // If this value is nonnegative, it is used in GetDesiredTabWidths() to
  // calculate how much space in the tab strip to use for tabs.  Most of the
  // time this will be -1, but while we're handling closing a tab via the mouse,
  // we'll set this to the edge of the last tab before closing, so that if we
  // are closing the last tab and need to resize immediately, we'll resize only
  // back to this width, thus once again placing the last tab under the mouse
  // cursor.
  int available_width_for_tabs_;

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;

  // The size of the new tab button must be hardcoded because we need to be
  // able to lay it out before we are able to get its image from the
  // ThemeProvider.  It also makes sense to do this, because the size of the
  // new tab button should not need to be calculated dynamically.
  static const int kNewTabButtonWidth = 28;
  static const int kNewTabButtonHeight = 18;

  // Used during a drop session of a url. Tracks the position of the drop as
  // well as a window used to highlight where the drop occurs.
  struct DropInfo {
    DropInfo(int index, bool drop_before, bool paint_down);
    ~DropInfo();

    // Index of the tab to drop on. If drop_before is true, the drop should
    // occur between the tab at drop_index - 1 and drop_index.
    // WARNING: if drop_before is true it is possible this will == tab_count,
    // which indicates the drop should create a new tab at the end of the tabs.
    int drop_index;
    bool drop_before;

    // Direction the arrow should point in. If true, the arrow is displayed
    // above the tab and points down. If false, the arrow is displayed beneath
    // the tab and points up.
    bool point_down;

    // Renders the drop indicator.
    // TODO(beng): should be views::Widget.
#if defined(OS_WIN)
    views::WidgetWin* arrow_window;
#else
    views::WidgetGtk* arrow_window;
#endif
    views::ImageView* arrow_view;

   private:
    DISALLOW_COPY_AND_ASSIGN(DropInfo);
  };

  // Valid for the lifetime of a drag over us.
  scoped_ptr<DropInfo> drop_info_;

  // The controller for a drag initiated from a Tab. Valid for the lifetime of
  // the drag session.
  scoped_ptr<DraggedTabController> drag_controller_;

  // The Tabs we contain, and their last generated "good" bounds.
  struct TabData {
    Tab* tab;
    gfx::Rect ideal_bounds;
  };
  std::vector<TabData> tab_data_;

  // The currently running animation.
  scoped_ptr<TabAnimation> active_animation_;

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_STRIP_H_
