// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_item_gtk.h"

#include "app/l10n_util.h"
#include "app/gfx/canvas_paint.h"
#include "app/gfx/font.h"
#include "app/gfx/text_elider.h"
#include "app/gtk_dnd_util.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/gtk/download_shelf_gtk.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "net/base/net_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// The width of the |menu_button_| widget. It has to be at least as wide as the
// bitmap that we use to draw it, i.e. 16, but can be more.
const int kMenuButtonWidth = 16;

// Padding on left and right of items in dangerous download prompt.
const int kDangerousElementPadding = 3;

// Amount of space we allot to showing the filename. If the filename is too wide
// it will be elided.
const int kTextWidth = 140;

// The minimum width we will ever draw the download item. Used as a lower bound
// during animation. This number comes from the width of the images used to
// make the download item.
const int kMinDownloadItemWidth = download_util::kSmallProgressIconSize;

// New download item animation speed in milliseconds.
const int kNewItemAnimationDurationMs = 800;

// How long the 'download complete' animation should last for.
const int kCompleteAnimationDurationMs = 2500;

// Width of the body area of the download item.
// TODO(estade): get rid of the fudge factor. http://crbug.com/18692
const int kBodyWidth = kTextWidth + 50 + download_util::kSmallProgressIconSize;

// The font size of the text, and that size rounded down to the nearest integer
// for the size of the arrow in GTK theme mode.
const double kTextSize = 13.4;  // 13.4px == 10pt @ 96dpi

}  // namespace

// DownloadShelfContextMenuGtk -------------------------------------------------

class DownloadShelfContextMenuGtk : public DownloadShelfContextMenu,
                                    public MenuGtk::Delegate {
 public:
  // The constructor creates the menu and immediately pops it up.
  // |model| is the download item model associated with this context menu,
  // |widget| is the button that popped up this context menu, and |e| is
  // the button press event that caused this menu to be created.
  DownloadShelfContextMenuGtk(BaseDownloadItemModel* model,
                              DownloadItemGtk* download_item)
      : DownloadShelfContextMenu(model),
        download_item_(download_item),
        menu_is_for_complete_download_(false),
        method_factory_(this) {
  }

  ~DownloadShelfContextMenuGtk() {
  }

  void Popup(GtkWidget* widget, GdkEvent* event) {
    // Create the menu if we have not created it yet or we created it for
    // an in-progress download that has since completed.
    bool download_is_complete = download_->state() == DownloadItem::COMPLETE;
    if (menu_.get() == NULL ||
        (download_is_complete && !menu_is_for_complete_download_)) {
      menu_.reset(new MenuGtk(this, download_is_complete ?
          finished_download_menu : in_progress_download_menu));
      menu_is_for_complete_download_ = download_is_complete;
    }
    menu_->Popup(widget, event);
  }

  // MenuGtk::Delegate implementation ------------------------------------------
  virtual bool IsCommandEnabled(int id) const {
    return IsItemCommandEnabled(id);
  }

  virtual bool IsItemChecked(int id) const {
    return ItemIsChecked(id);
  }

  virtual void ExecuteCommandById(int id) {
    ExecuteItemCommand(id);
  }

  virtual void StoppedShowing() {
    download_item_->menu_showing_ = false;
    gtk_widget_queue_draw(download_item_->menu_button_);
  }

 private:
  // The menu we show on Popup(). We keep a pointer to it for a couple reasons:
  //  * we don't want to have to recreate the menu every time it's popped up.
  //  * we have to keep it in scope for longer than the duration of Popup(), or
  //    completing the user-selected action races against the menu's
  //    destruction.
  scoped_ptr<MenuGtk> menu_;

  // The download item that created us.
  DownloadItemGtk* download_item_;

  // If true, the MenuGtk in |menu_| refers to a finished download menu.
  bool menu_is_for_complete_download_;

  // We show slightly different menus if the download is in progress vs. if the
  // download has finished.
  static MenuCreateMaterial in_progress_download_menu[];

  static MenuCreateMaterial finished_download_menu[];

  ScopedRunnableMethodFactory<DownloadShelfContextMenuGtk> method_factory_;
};

MenuCreateMaterial DownloadShelfContextMenuGtk::finished_download_menu[] = {
  { MENU_NORMAL, OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN, 0, NULL },
  { MENU_CHECKBOX, ALWAYS_OPEN_TYPE, IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE,
    0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, SHOW_IN_FOLDER, IDS_DOWNLOAD_MENU_SHOW, 0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, CANCEL, IDS_DOWNLOAD_MENU_CANCEL, 0, NULL},
  { MENU_END, 0, 0, 0, NULL },
};

MenuCreateMaterial DownloadShelfContextMenuGtk::in_progress_download_menu[] = {
  { MENU_CHECKBOX, OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE,
    0, NULL },
  { MENU_CHECKBOX, ALWAYS_OPEN_TYPE, IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE,
    0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_CHECKBOX, TOGGLE_PAUSE, IDS_DOWNLOAD_MENU_PAUSE_ITEM, 0, NULL},
  { MENU_NORMAL, SHOW_IN_FOLDER, IDS_DOWNLOAD_MENU_SHOW, 0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, CANCEL, IDS_DOWNLOAD_MENU_CANCEL, 0, NULL},
  { MENU_END, 0, 0, 0, NULL },
};

// DownloadItemGtk -------------------------------------------------------------

NineBox* DownloadItemGtk::body_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_active_ = NULL;

NineBox* DownloadItemGtk::menu_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_active_ = NULL;

NineBox* DownloadItemGtk::dangerous_nine_box_ = NULL;

DownloadItemGtk::DownloadItemGtk(DownloadShelfGtk* parent_shelf,
                                 BaseDownloadItemModel* download_model)
    : parent_shelf_(parent_shelf),
      arrow_(NULL),
      menu_showing_(false),
      theme_provider_(GtkThemeProvider::GetFrom(
                          parent_shelf->browser()->profile())),
      progress_angle_(download_util::kStartAngleDegrees),
      download_model_(download_model),
      bounding_widget_(parent_shelf->GetRightBoundingWidget()),
      dangerous_prompt_(NULL),
      dangerous_label_(NULL),
      icon_(NULL),
      creation_time_(base::Time::Now()) {
  LoadIcon();

  body_.Own(gtk_button_new());
  gtk_widget_set_app_paintable(body_.get(), TRUE);

  g_signal_connect(body_.get(), "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(body_.get(), "clicked",
                   G_CALLBACK(OnClick), this);
  GTK_WIDGET_UNSET_FLAGS(body_.get(), GTK_CAN_FOCUS);
  // Remove internal padding on the button.
  GtkRcStyle* no_padding_style = gtk_rc_style_new();
  no_padding_style->xthickness = 0;
  no_padding_style->ythickness = 0;
  gtk_widget_modify_style(body_.get(), no_padding_style);
  g_object_unref(no_padding_style);

  name_label_ = gtk_label_new(NULL);

  UpdateNameLabel();

  status_label_ = gtk_label_new(NULL);
  // Left align and vertically center the labels.
  gtk_misc_set_alignment(GTK_MISC(name_label_), 0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(status_label_), 0, 0.5);
  // Until we switch to vector graphics, force the font size.
  gtk_util::ForceFontSizePixels(name_label_, kTextSize);
  gtk_util::ForceFontSizePixels(status_label_, kTextSize);

  // Stack the labels on top of one another.
  GtkWidget* text_stack = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(text_stack), name_label_, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(text_stack), status_label_, FALSE, FALSE, 0);

  // We use a GtkFixed because we don't want it to have its own window.
  // This choice of widget is not critically important though.
  progress_area_.Own(gtk_fixed_new());
  gtk_widget_set_size_request(progress_area_.get(),
      download_util::kSmallProgressIconSize,
      download_util::kSmallProgressIconSize);
  gtk_widget_set_app_paintable(progress_area_.get(), TRUE);
  g_signal_connect(progress_area_.get(), "expose-event",
                   G_CALLBACK(OnProgressAreaExpose), this);

  // Put the download progress icon on the left of the labels.
  GtkWidget* body_hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(body_.get()), body_hbox);
  gtk_box_pack_start(GTK_BOX(body_hbox), progress_area_.get(), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(body_hbox), text_stack, TRUE, TRUE, 0);

  menu_button_ = gtk_button_new();
  gtk_widget_set_app_paintable(menu_button_, TRUE);
  GTK_WIDGET_UNSET_FLAGS(menu_button_, GTK_CAN_FOCUS);
  g_signal_connect(menu_button_, "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(menu_button_, "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEvent), this);
  g_object_set_data(G_OBJECT(menu_button_), "left-align-popup",
                    reinterpret_cast<void*>(true));

  GtkWidget* shelf_hbox = parent_shelf->GetHBox();
  hbox_.Own(gtk_hbox_new(FALSE, 0));
  g_signal_connect(hbox_.get(), "expose-event",
                   G_CALLBACK(OnHboxExpose), this);
  gtk_box_pack_start(GTK_BOX(hbox_.get()), body_.get(), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_.get()), menu_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(shelf_hbox), hbox_.get(), FALSE, FALSE, 0);
  // Insert as the leftmost item.
  gtk_box_reorder_child(GTK_BOX(shelf_hbox), hbox_.get(), 1);
  g_signal_connect(G_OBJECT(shelf_hbox), "size-allocate",
                   G_CALLBACK(OnShelfResized), this);

  get_download()->AddObserver(this);

  new_item_animation_.reset(new SlideAnimation(this));
  new_item_animation_->SetSlideDuration(kNewItemAnimationDurationMs);
  gtk_widget_show_all(hbox_.get());

  if (IsDangerous()) {
    // Hide the download item components for now.
    gtk_widget_hide(body_.get());
    gtk_widget_hide(menu_button_);

    // Create an hbox to hold it all.
    dangerous_hbox_ = gtk_hbox_new(FALSE, kDangerousElementPadding);

    // Add padding at the beginning and end. The hbox will add padding between
    // the empty labels and the other elements.
    GtkWidget* empty_label_a = gtk_label_new(NULL);
    GtkWidget* empty_label_b = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(dangerous_hbox_), empty_label_a,
                       FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(dangerous_hbox_), empty_label_b,
                     FALSE, FALSE, 0);

    // Create the warning icon.
    dangerous_image_ = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(dangerous_hbox_), dangerous_image_,
                       FALSE, FALSE, 0);

    dangerous_label_ = gtk_label_new(NULL);
    // We pass TRUE, TRUE so that the label will condense to less than its
    // request when the animation is going on.
    gtk_box_pack_start(GTK_BOX(dangerous_hbox_), dangerous_label_,
                       TRUE, TRUE, 0);

    // Create the nevermind button.
    GtkWidget* dangerous_decline = gtk_button_new_with_label(
        l10n_util::GetStringUTF8(IDS_DISCARD_DOWNLOAD).c_str());
    g_signal_connect(dangerous_decline, "clicked",
                     G_CALLBACK(OnDangerousDecline), this);
    gtk_util::CenterWidgetInHBox(dangerous_hbox_, dangerous_decline, false, 0);

    // Create the ok button.
    GtkWidget* dangerous_accept = gtk_button_new_with_label(
        l10n_util::GetStringUTF8(
            download_model->download()->is_extension_install() ?
                IDS_CONTINUE_EXTENSION_DOWNLOAD : IDS_SAVE_DOWNLOAD).c_str());
    g_signal_connect(dangerous_accept, "clicked",
                     G_CALLBACK(OnDangerousAccept), this);
    gtk_util::CenterWidgetInHBox(dangerous_hbox_, dangerous_accept, false, 0);

    // Put it in an alignment so that padding will be added on the left and
    // right.
    dangerous_prompt_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(dangerous_prompt_),
        0, 0, kDangerousElementPadding, kDangerousElementPadding);
    gtk_container_add(GTK_CONTAINER(dangerous_prompt_), dangerous_hbox_);
    gtk_box_pack_start(GTK_BOX(hbox_.get()), dangerous_prompt_, FALSE, FALSE,
                       0);
    gtk_widget_set_app_paintable(dangerous_prompt_, TRUE);
    gtk_widget_set_redraw_on_allocate(dangerous_prompt_, TRUE);
    g_signal_connect(dangerous_prompt_, "expose-event",
                     G_CALLBACK(OnDangerousPromptExpose), this);
    gtk_widget_show_all(dangerous_prompt_);
  }

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  theme_provider_->InitThemesFor(this);

  new_item_animation_->Show();
}

DownloadItemGtk::~DownloadItemGtk() {
  icon_consumer_.CancelAllRequests();
  StopDownloadProgress();
  get_download()->RemoveObserver(this);
  g_signal_handlers_disconnect_by_func(parent_shelf_->GetHBox(),
      reinterpret_cast<gpointer>(OnShelfResized), this);
  gtk_widget_show_all(parent_shelf_->GetHBox());

  hbox_.Destroy();
  progress_area_.Destroy();
  body_.Destroy();
}

void DownloadItemGtk::OnDownloadUpdated(DownloadItem* download) {
  DCHECK_EQ(download, get_download());

  if (dangerous_prompt_ != NULL &&
      download->safety_state() == DownloadItem::DANGEROUS_BUT_VALIDATED) {
    // We have been approved.
    gtk_widget_show_all(hbox_.get());
    gtk_widget_destroy(dangerous_prompt_);
    gtk_widget_set_size_request(body_.get(), kBodyWidth, -1);
    dangerous_prompt_ = NULL;
  }

  if (download->full_path() != icon_filepath_) {
    // Turns out the file path is "unconfirmed %d.download" for dangerous
    // downloads. When the download is confirmed, the file is renamed on
    // another thread, so reload the icon if the download filename changes.
    LoadIcon();
  }

  switch (download->state()) {
    case DownloadItem::REMOVING:
      parent_shelf_->RemoveDownloadItem(this);  // This will delete us!
      return;
    case DownloadItem::CANCELLED:
      StopDownloadProgress();
      gtk_widget_queue_draw(progress_area_.get());
      break;
    case DownloadItem::COMPLETE:
      if (download->auto_opened()) {
        parent_shelf_->RemoveDownloadItem(this);  // This will delete us!
        return;
      }
      StopDownloadProgress();

      // Set up the widget as a drag source.
      gtk_drag_source_set(body_.get(), GDK_BUTTON1_MASK, NULL, 0,
                          GDK_ACTION_COPY);
      GtkDndUtil::SetSourceTargetListFromCodeMask(body_.get(),
                                                  GtkDndUtil::TEXT_URI_LIST |
                                                  GtkDndUtil::CHROME_NAMED_URL);
      g_signal_connect(body_.get(), "drag-data-get",
                       G_CALLBACK(OnDragDataGet), this);

      complete_animation_.reset(new SlideAnimation(this));
      complete_animation_->SetSlideDuration(kCompleteAnimationDurationMs);
      complete_animation_->SetTweenType(SlideAnimation::NONE);
      complete_animation_->Show();
      break;
    case DownloadItem::IN_PROGRESS:
      get_download()->is_paused() ?
          StopDownloadProgress() : StartDownloadProgress();
      break;
    default:
      NOTREACHED();
  }

  // Now update the status label. We may have already removed it; if so, we
  // do nothing.
  if (!status_label_) {
    return;
  }

  std::wstring status_text = download_model_->GetStatusText();
  status_text_ = WideToUTF8(status_text);
  // Remove the status text label.
  if (status_text.empty()) {
    gtk_widget_destroy(status_label_);
    status_label_ = NULL;
    return;
  }

  UpdateStatusLabel(status_label_, status_text_);
}

void DownloadItemGtk::AnimationProgressed(const Animation* animation) {
  if (animation == complete_animation_.get()) {
    gtk_widget_queue_draw(progress_area_.get());
  } else {
    if (IsDangerous()) {
      int progress = static_cast<int>((dangerous_hbox_full_width_ -
                                       dangerous_hbox_start_width_) *
                                      new_item_animation_->GetCurrentValue());
      int showing_width = dangerous_hbox_start_width_ + progress;
      gtk_widget_set_size_request(dangerous_hbox_, showing_width, -1);
    } else {
      DCHECK(animation == new_item_animation_.get());
      int showing_width = std::max(kMinDownloadItemWidth,
          static_cast<int>(kBodyWidth *
                           new_item_animation_->GetCurrentValue()));
      gtk_widget_set_size_request(body_.get(), showing_width, -1);
    }
  }
}

void DownloadItemGtk::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_THEME_CHANGED) {
    // Our GtkArrow is only visible in gtk mode. Otherwise, we let the custom
    // rendering code do whatever it wants.
    if (theme_provider_->UseGtkTheme()) {
      if (!arrow_) {
        arrow_ = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
        gtk_widget_set_size_request(arrow_,
                                    static_cast<int>(kTextSize),
                                    static_cast<int>(kTextSize));
        gtk_container_add(GTK_CONTAINER(menu_button_), arrow_);
      }

      gtk_widget_set_size_request(menu_button_, -1, -1);
      gtk_widget_show(arrow_);
    } else {
      InitNineBoxes();

      gtk_widget_set_size_request(menu_button_, kMenuButtonWidth, 0);

      if (arrow_)
        gtk_widget_hide(arrow_);
    }

    UpdateNameLabel();
    UpdateStatusLabel(status_label_, status_text_);
    UpdateDangerWarning();
  }
}

DownloadItem* DownloadItemGtk::get_download() {
  return download_model_->download();
}

bool DownloadItemGtk::IsDangerous() {
  return get_download()->safety_state() == DownloadItem::DANGEROUS;
}

// Download progress animation functions.

void DownloadItemGtk::UpdateDownloadProgress() {
  progress_angle_ = (progress_angle_ +
                     download_util::kUnknownIncrementDegrees) %
                    download_util::kMaxDegrees;
  gtk_widget_queue_draw(progress_area_.get());
}

void DownloadItemGtk::StartDownloadProgress() {
  if (progress_timer_.IsRunning())
    return;
  progress_timer_.Start(
      base::TimeDelta::FromMilliseconds(download_util::kProgressRateMs), this,
      &DownloadItemGtk::UpdateDownloadProgress);
}

void DownloadItemGtk::StopDownloadProgress() {
  progress_timer_.Stop();
}

// Icon loading functions.

void DownloadItemGtk::OnLoadIconComplete(IconManager::Handle handle,
                                         SkBitmap* icon_bitmap) {
  icon_ = icon_bitmap;
  gtk_widget_queue_draw(progress_area_.get());
}

void DownloadItemGtk::LoadIcon() {
  icon_consumer_.CancelAllRequests();
  IconManager* im = g_browser_process->icon_manager();
  icon_filepath_ = get_download()->full_path();
  im->LoadIcon(icon_filepath_,
               IconLoader::SMALL, &icon_consumer_,
               NewCallback(this, &DownloadItemGtk::OnLoadIconComplete));
}

void DownloadItemGtk::UpdateNameLabel() {
  // TODO(estade): This is at best an educated guess, since we don't actually
  // use gfx::Font() to draw the text. This is why we need to add so
  // much padding when we set the size request. We need to either use gfx::Font
  // or somehow extend TextElider.
  std::wstring elided_filename = gfx::ElideFilename(
      get_download()->GetFileName(),
      gfx::Font(), kTextWidth);
  if (theme_provider_->UseGtkTheme()) {
    gtk_util::SetLabelColor(name_label_, NULL);
  } else {
    GdkColor color = theme_provider_->GetGdkColor(
        BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
    gtk_util::SetLabelColor(name_label_, &color);
  }

  gtk_label_set_text(GTK_LABEL(name_label_),
                     WideToUTF8(elided_filename).c_str());
}

void DownloadItemGtk::UpdateStatusLabel(GtkWidget* status_label,
                                        const std::string& status_text) {
  if (status_label) {
    if (theme_provider_->UseGtkTheme()) {
      gtk_util::SetLabelColor(status_label, NULL);
    } else {
      GdkColor color = theme_provider_->GetGdkColor(
          BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
      gtk_util::SetLabelColor(status_label, &color);
    }

    gtk_label_set_label(GTK_LABEL(status_label), status_text.c_str());
  }
}

void DownloadItemGtk::UpdateDangerWarning() {
  if (dangerous_prompt_) {
    // We create |dangerous_warning| as a wide string so we can more easily
    // calculate its length in characters.
    std::wstring dangerous_warning;
    if (get_download()->is_extension_install()) {
      dangerous_warning =
          l10n_util::GetString(IDS_PROMPT_DANGEROUS_DOWNLOAD_EXTENSION);
    } else {
      std::wstring elided_filename = gfx::ElideFilename(
          get_download()->original_name(), gfx::Font(), kTextWidth);

      dangerous_warning =
          l10n_util::GetStringF(IDS_PROMPT_DANGEROUS_DOWNLOAD, elided_filename);
    }

    if (theme_provider_->UseGtkTheme()) {
      gtk_image_set_from_stock(GTK_IMAGE(dangerous_image_),
          GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_SMALL_TOOLBAR);

      gtk_util::SetLabelColor(dangerous_label_, NULL);
    } else {
      // Set the warning icon.
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      GdkPixbuf* download_pixbuf = rb.GetPixbufNamed(IDR_WARNING);
      gtk_image_set_from_pixbuf(GTK_IMAGE(dangerous_image_), download_pixbuf);

      GdkColor color = theme_provider_->GetGdkColor(
          BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
      gtk_util::SetLabelColor(dangerous_label_, &color);
    }

    gtk_label_set_label(GTK_LABEL(dangerous_label_),
                        WideToUTF8(dangerous_warning).c_str());

    // Until we switch to vector graphics, force the font size.
    gtk_util::ForceFontSizePixels(dangerous_label_, kTextSize);
    gtk_label_set_line_wrap(GTK_LABEL(dangerous_label_), TRUE);

    double width_chars = 0.6 * dangerous_warning.size();
    gint dangerous_width;
    gtk_util::GetWidgetSizeFromCharacters(dangerous_label_, width_chars, 0,
                                          &dangerous_width, NULL);
    gtk_widget_set_size_request(dangerous_label_, dangerous_width, -1);

    // The width will depend on the text. We must do this each time we possibly
    // change the label above.
    GtkRequisition req;
    gtk_widget_size_request(dangerous_hbox_, &req);
    dangerous_hbox_full_width_ = req.width;
    gtk_widget_size_request(dangerous_label_, &req);
    dangerous_hbox_start_width_ = dangerous_hbox_full_width_ - req.width;
  }
}

// static
void DownloadItemGtk::InitNineBoxes() {
  if (body_nine_box_normal_)
    return;

  body_nine_box_normal_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM);

  body_nine_box_prelight_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP_H,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H);

  body_nine_box_active_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP_P,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P);

  menu_nine_box_normal_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM, 0, 0);

  menu_nine_box_prelight_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP_H, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H, 0, 0);

  menu_nine_box_active_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP_P, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P, 0, 0);

  dangerous_nine_box_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_NO_DD,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_NO_DD,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_NO_DD);
}

gboolean DownloadItemGtk::OnHboxExpose(GtkWidget* widget, GdkEventExpose* e,
                                       DownloadItemGtk* download_item) {
  if (download_item->theme_provider_->UseGtkTheme()) {
    int border_width = GTK_CONTAINER(widget)->border_width;
    int x = widget->allocation.x + border_width;
    int y = widget->allocation.y + border_width;
    int width = widget->allocation.width - border_width * 2;
    int height = widget->allocation.height - border_width * 2;

    if (download_item->IsDangerous()) {
      // Draw a simple frame around the area when we're displaying the warning.
      gtk_paint_shadow(widget->style, widget->window,
                       static_cast<GtkStateType>(widget->state),
                       static_cast<GtkShadowType>(GTK_SHADOW_OUT),
                       &e->area, widget, "frame",
                       x, y, width, height);
    } else {
      // Manually draw the GTK button border around the download item. We draw
      // the left part of the button (the file), a divider, and then the right
      // part of the button (the menu). We can't draw a button on top of each
      // other (*cough*Clearlooks*cough*) so instead, to draw the left part of
      // the button, we instruct GTK to draw the entire button...with a
      // doctored clip rectangle to the left part of the button sans
      // separator. We then repeat this for the right button.
      GtkStyle* style = download_item->body_.get()->style;

      GtkAllocation left_allocation = download_item->body_.get()->allocation;
      GdkRectangle left_clip = {
        left_allocation.x, left_allocation.y,
        left_allocation.width, left_allocation.height
      };

      GtkAllocation right_allocation = download_item->menu_button_->allocation;
      GdkRectangle right_clip = {
        right_allocation.x, right_allocation.y,
        right_allocation.width, right_allocation.height
      };

      GtkShadowType body_shadow =
          GTK_BUTTON(download_item->body_.get())->depressed ?
          GTK_SHADOW_IN : GTK_SHADOW_OUT;
      gtk_paint_box(style, widget->window,
                    static_cast<GtkStateType>(
                        GTK_WIDGET_STATE(download_item->body_.get())),
                    body_shadow,
                    &left_clip, widget, "button",
                    x, y, width, height);

      GtkShadowType menu_shadow =
          GTK_BUTTON(download_item->menu_button_)->depressed ?
          GTK_SHADOW_IN : GTK_SHADOW_OUT;
      gtk_paint_box(style, widget->window,
                    static_cast<GtkStateType>(
                        GTK_WIDGET_STATE(download_item->menu_button_)),
                    menu_shadow,
                    &right_clip, widget, "button",
                    x, y, width, height);

      // Doing the math to reverse engineer where we should be drawing our line
      // is hard and relies on copying GTK internals, so instead steal the
      // allocation of the gtk arrow which is close enough (and will error on
      // the conservative side).
      GtkAllocation arrow_allocation = download_item->arrow_->allocation;
      gtk_paint_vline(style, widget->window,
                      static_cast<GtkStateType>(GTK_WIDGET_STATE(widget)),
                      &e->area, widget, "button",
                      arrow_allocation.y,
                      arrow_allocation.y + arrow_allocation.height,
                      left_allocation.x + left_allocation.width);
    }
  }
  return FALSE;
}

// static
gboolean DownloadItemGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                                   DownloadItemGtk* download_item) {
  if (!download_item->theme_provider_->UseGtkTheme()) {
    bool is_body = widget == download_item->body_.get();

    NineBox* nine_box = NULL;
    // If true, this widget is |body_|, otherwise it is |menu_button_|.
    if (GTK_WIDGET_STATE(widget) == GTK_STATE_PRELIGHT)
      nine_box = is_body ? body_nine_box_prelight_ : menu_nine_box_prelight_;
    else if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE)
      nine_box = is_body ? body_nine_box_active_ : menu_nine_box_active_;
    else
      nine_box = is_body ? body_nine_box_normal_ : menu_nine_box_normal_;

    // When the button is showing, we want to draw it as active. We have to do
    // this explicitly because the button's state will be NORMAL while the menu
    // has focus.
    if (!is_body && download_item->menu_showing_)
      nine_box = menu_nine_box_active_;

    nine_box->RenderToWidget(widget);
  }

  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);

  return TRUE;
}

// static
void DownloadItemGtk::OnClick(GtkWidget* widget, DownloadItemGtk* item) {
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.open_download",
                           base::Time::Now() - item->creation_time_);

  DownloadItem* download = item->get_download();

  if (download->state() == DownloadItem::IN_PROGRESS) {
    download->set_open_when_complete(
        !download->open_when_complete());
  } else if (download->state() == DownloadItem::COMPLETE) {
    download_util::OpenDownload(download);
  }
}

// static
gboolean DownloadItemGtk::OnProgressAreaExpose(GtkWidget* widget,
    GdkEventExpose* event, DownloadItemGtk* download_item) {
  // Create a transparent canvas.
  gfx::CanvasPaint canvas(event, false);
  if (download_item->complete_animation_.get()) {
    if (download_item->complete_animation_->IsAnimating()) {
      download_util::PaintDownloadComplete(&canvas,
          widget->allocation.x, widget->allocation.y,
          download_item->complete_animation_->GetCurrentValue(),
          download_util::SMALL);
    }
  } else if (download_item->get_download()->state() !=
             DownloadItem::CANCELLED) {
    download_util::PaintDownloadProgress(&canvas,
        widget->allocation.x, widget->allocation.y,
        download_item->progress_angle_,
        download_item->get_download()->PercentComplete(),
        download_util::SMALL);
  }

  // |icon_| may be NULL if it is still loading. If the file is an unrecognized
  // type then we will get back a generic system icon. Hence there is no need to
  // use the chromium-specific default download item icon.
  if (download_item->icon_) {
    const int offset = download_util::kSmallProgressIconOffset;
    canvas.DrawBitmapInt(*download_item->icon_,
        widget->allocation.x + offset, widget->allocation.y + offset);
  }

  return TRUE;
}

// static
gboolean DownloadItemGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                                 GdkEvent* event,
                                                 DownloadItemGtk* item) {
  // Stop any completion animation.
  if (item->complete_animation_.get() &&
      item->complete_animation_->IsAnimating()) {
    item->complete_animation_->End();
  }

  if (event->type == GDK_BUTTON_PRESS) {
    GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
    if (event_button->button == 1) {
      if (item->menu_.get() == NULL) {
        item->menu_.reset(new DownloadShelfContextMenuGtk(
                          item->download_model_.get(), item));
      }
      item->menu_->Popup(button, event);
      item->menu_showing_ = true;
      gtk_widget_queue_draw(button);
    }
  }

  return FALSE;
}

// static
void DownloadItemGtk::OnShelfResized(GtkWidget *widget,
                                     GtkAllocation *allocation,
                                     DownloadItemGtk* item) {
  bool out_of_bounds =
      item->hbox_->allocation.x + item->hbox_->allocation.width >
      item->bounding_widget_->allocation.x;
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    out_of_bounds = !out_of_bounds;

  if (out_of_bounds)
    gtk_widget_hide(item->hbox_.get());
  else
    gtk_widget_show(item->hbox_.get());
}

// static
void DownloadItemGtk::OnDragDataGet(GtkWidget* widget, GdkDragContext* context,
                                    GtkSelectionData* selection_data,
                                    guint target_type, guint time,
                                    DownloadItemGtk* item) {
  GURL url = net::FilePathToFileURL(item->get_download()->full_path());

  GtkDndUtil::WriteURLWithName(selection_data, url,
      UTF8ToUTF16(item->get_download()->GetFileName().value()), target_type);
}

// static
gboolean DownloadItemGtk::OnDangerousPromptExpose(GtkWidget* widget,
    GdkEventExpose* event, DownloadItemGtk* item) {
  if (!item->theme_provider_->UseGtkTheme()) {
    // The hbox renderer will take care of the border when in GTK mode.
    dangerous_nine_box_->RenderToWidget(widget);
  }
  return FALSE;  // Continue propagation.
}

// static
void DownloadItemGtk::OnDangerousAccept(GtkWidget* button,
                                        DownloadItemGtk* item) {
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.save_download",
                           base::Time::Now() - item->creation_time_);
  item->get_download()->manager()->DangerousDownloadValidated(
      item->get_download());
}

// static
void DownloadItemGtk::OnDangerousDecline(GtkWidget* button,
                                         DownloadItemGtk* item) {
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download",
                           base::Time::Now() - item->creation_time_);
  if (item->get_download()->state() == DownloadItem::IN_PROGRESS)
    item->get_download()->Cancel(true);
  item->get_download()->Remove(true);
}
