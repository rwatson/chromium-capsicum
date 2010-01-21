// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/dragged_tab_view.h"

#include "app/gfx/canvas.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/tabs/native_view_photobooth.h"
#include "chrome/browser/views/tabs/tab_renderer.h"
#include "third_party/skia/include/core/SkShader.h"
#include "views/widget/widget.h"
#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#elif defined(TOOLKIT_GTK)
#include "views/widget/widget_gtk.h"
#endif

const int kTransparentAlpha = 200;
const int kOpaqueAlpha = 255;
const int kDragFrameBorderSize = 2;
const int kTwiceDragFrameBorderSize = 2 * kDragFrameBorderSize;
const float kScalingFactor = 0.5;
const int kAnimateToBoundsDurationMs = 150;
static const SkColor kDraggedTabBorderColor = SkColorSetRGB(103, 129, 162);

////////////////////////////////////////////////////////////////////////////////
// DraggedTabView, public:

DraggedTabView::DraggedTabView(TabContents* datasource,
                               const gfx::Point& mouse_tab_offset,
                               const gfx::Size& contents_size)
    : renderer_(new TabRenderer),
      attached_(false),
      show_contents_on_drag_(true),
      mouse_tab_offset_(mouse_tab_offset),
      attached_tab_size_(TabRenderer::GetMinimumSelectedSize()),
      photobooth_(NULL),
      contents_size_(contents_size),
      close_animation_(this),
      tab_width_(0) {
  set_parent_owned(false);

  renderer_->UpdateData(datasource, false);

#if defined(OS_WIN)
  container_.reset(new views::WidgetWin);
  container_->set_delete_on_destroy(false);
  container_->set_window_style(WS_POPUP);
  container_->set_window_ex_style(
    WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
  container_->set_can_update_layered_window(false);
  container_->Init(NULL, gfx::Rect(0, 0, 0, 0));
  container_->SetContentsView(this);

  BOOL drag;
  if ((::SystemParametersInfo(SPI_GETDRAGFULLWINDOWS, 0, &drag, 0) != 0) &&
      (drag == FALSE)) {
    show_contents_on_drag_ = false;
  }
#else
  container_.reset(new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP));
  container_->MakeTransparent();
  container_->set_delete_on_destroy(false);
  container_->Init(NULL, gfx::Rect(0, 0, 0, 0));
  container_->SetContentsView(this);
#endif
}

DraggedTabView::~DraggedTabView() {
  if (close_animation_.IsAnimating())
    close_animation_.Stop();
  GetParent()->RemoveChildView(this);
  container_->CloseNow();
}

void DraggedTabView::MoveTo(const gfx::Point& screen_point) {
  int x;
  if (UILayoutIsRightToLeft() && !attached_) {
    // On RTL locales, a dragged tab (when it is not attached to a tab strip)
    // is rendered using a right-to-left orientation so we should calculate the
    // window position differently.
    gfx::Size ps = GetPreferredSize();
    x = screen_point.x() - ScaleValue(ps.width()) + mouse_tab_offset_.x() +
        ScaleValue(
            renderer_->MirroredXCoordinateInsideView(mouse_tab_offset_.x()));
  } else {
    x = screen_point.x() + mouse_tab_offset_.x() -
        ScaleValue(mouse_tab_offset_.x());
  }
  int y = screen_point.y() + mouse_tab_offset_.y() -
      ScaleValue(mouse_tab_offset_.y());

#if defined(OS_WIN)
  int show_flags = container_->IsVisible() ? SWP_NOZORDER : SWP_SHOWWINDOW;

  container_->SetWindowPos(HWND_TOP, x, y, 0, 0,
                           SWP_NOSIZE | SWP_NOACTIVATE | show_flags);
#else
  gfx::Rect bounds;
  container_->GetBounds(&bounds, true);
  container_->SetBounds(gfx::Rect(x, y, bounds.width(), bounds.height()));
  if (!container_->IsVisible())
    container_->Show();
#endif
}

void DraggedTabView::Attach(int selected_width) {
  attached_ = true;
  photobooth_ = NULL;
#if defined(OS_WIN)
  container_->SetOpacity(kOpaqueAlpha);
#endif
  Resize(selected_width);
}

void DraggedTabView::Resize(int width) {
  attached_tab_size_.set_width(width);
  ResizeContainer();
  Update();
}

void DraggedTabView::set_pinned(bool pinned) {
  renderer_->set_pinned(pinned);
}

bool DraggedTabView::pinned() const {
  return renderer_->pinned();
}

void DraggedTabView::Detach(NativeViewPhotobooth* photobooth) {
  // Detached tabs are never pinned.
  renderer_->set_pinned(false);

  if (attached_tab_size_.width() != tab_width_) {
    // The attached tab size differs from current tab size. Resize accordingly.
    Resize(tab_width_);
  }

  attached_ = false;
  photobooth_ = photobooth;
#if defined(OS_WIN)
  container_->SetOpacity(kTransparentAlpha);
#endif
  ResizeContainer();
  Update();
}

void DraggedTabView::Update() {
#if defined(OS_WIN)
  container_->set_can_update_layered_window(true);
  SchedulePaint();
  container_->PaintNow(gfx::Rect());
  container_->set_can_update_layered_window(false);
#else
  SchedulePaint();
#endif
}

void DraggedTabView::AnimateToBounds(const gfx::Rect& bounds,
                                     Callback0::Type* callback) {
  animation_callback_.reset(callback);

  gfx::Rect initial_bounds;
  GetWidget()->GetBounds(&initial_bounds, true);
  animation_start_bounds_ = initial_bounds;
  animation_end_bounds_ = bounds;

  close_animation_.SetSlideDuration(kAnimateToBoundsDurationMs);
  close_animation_.SetTweenType(SlideAnimation::EASE_OUT);
  if (!close_animation_.IsShowing()) {
    close_animation_.Reset();
    close_animation_.Show();
  }
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabView, AnimationDelegate implementation:

void DraggedTabView::AnimationProgressed(const Animation* animation) {
  int delta_x = (animation_end_bounds_.x() - animation_start_bounds_.x());
  int x = animation_start_bounds_.x() +
      static_cast<int>(delta_x * animation->GetCurrentValue());
  int y = animation_end_bounds_.y();
#if defined(OS_WIN)
  container_->SetWindowPos(NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
#else
  gfx::Rect bounds;
  container_->GetBounds(&bounds, true);
  container_->SetBounds(gfx::Rect(x, y, bounds.width(), bounds.height()));
#endif
}

void DraggedTabView::AnimationEnded(const Animation* animation) {
  animation_callback_->Run();
}

void DraggedTabView::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabView, views::View overrides:

void DraggedTabView::Paint(gfx::Canvas* canvas) {
  if (attached_) {
    PaintAttachedTab(canvas);
  } else {
    if (show_contents_on_drag_) {
      PaintDetachedView(canvas);
    } else {
      PaintFocusRect(canvas);
    }
  }
}

void DraggedTabView::Layout() {
  if (attached_) {
    gfx::Size prefsize = GetPreferredSize();
    renderer_->SetBounds(0, 0, prefsize.width(), prefsize.height());
  } else {
    int left = 0;
    if (UILayoutIsRightToLeft())
      left = GetPreferredSize().width() - attached_tab_size_.width();
    // The renderer_'s width should be attached_tab_size_.width() in both LTR
    // and RTL locales. Wrong width will cause the wrong positioning of the tab
    // view in dragging. Please refer to http://crbug.com/6223 for details.
    renderer_->SetBounds(left, 0, attached_tab_size_.width(),
                         attached_tab_size_.height());
  }
}

gfx::Size DraggedTabView::GetPreferredSize() {
  if (attached_)
    return attached_tab_size_;

  int width = std::max(attached_tab_size_.width(), contents_size_.width()) +
      kTwiceDragFrameBorderSize;
  int height = attached_tab_size_.height() + kDragFrameBorderSize +
      contents_size_.height();
  return gfx::Size(width, height);
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabView, private:

void DraggedTabView::PaintAttachedTab(gfx::Canvas* canvas) {
  renderer_->ProcessPaint(canvas);
}

void DraggedTabView::PaintDetachedView(gfx::Canvas* canvas) {
  gfx::Size ps = GetPreferredSize();
  gfx::Canvas scale_canvas(ps.width(), ps.height(), false);
  SkBitmap& bitmap_device = const_cast<SkBitmap&>(
      scale_canvas.getTopPlatformDevice().accessBitmap(true));
  bitmap_device.eraseARGB(0, 0, 0, 0);

  scale_canvas.FillRectInt(kDraggedTabBorderColor, 0,
      attached_tab_size_.height() - kDragFrameBorderSize,
      ps.width(), ps.height() - attached_tab_size_.height());
  int image_x = kDragFrameBorderSize;
  int image_y = attached_tab_size_.height();
  int image_w = ps.width() - kTwiceDragFrameBorderSize;
  int image_h =
      ps.height() - kTwiceDragFrameBorderSize - attached_tab_size_.height();
  scale_canvas.FillRectInt(SK_ColorBLACK, image_x, image_y, image_w, image_h);
  photobooth_->PaintScreenshotIntoCanvas(
      &scale_canvas,
      gfx::Rect(image_x, image_y, image_w, image_h));
  renderer_->ProcessPaint(&scale_canvas);

  SkIRect subset;
  subset.set(0, 0, ps.width(), ps.height());
  SkBitmap mipmap = scale_canvas.ExtractBitmap();
  mipmap.buildMipMap(true);

  SkShader* bitmap_shader =
      SkShader::CreateBitmapShader(mipmap, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode);

  SkMatrix shader_scale;
  shader_scale.setScale(kScalingFactor, kScalingFactor);
  bitmap_shader->setLocalMatrix(shader_scale);

  SkPaint paint;
  paint.setShader(bitmap_shader);
  paint.setAntiAlias(true);
  bitmap_shader->unref();

  SkRect rc;
  rc.fLeft = 0;
  rc.fTop = 0;
  rc.fRight = SkIntToScalar(ps.width());
  rc.fBottom = SkIntToScalar(ps.height());
  canvas->drawRect(rc, paint);
}

void DraggedTabView::PaintFocusRect(gfx::Canvas* canvas) {
  gfx::Size ps = GetPreferredSize();
  canvas->DrawFocusRect(0, 0,
                        static_cast<int>(ps.width() * kScalingFactor),
                        static_cast<int>(ps.height() * kScalingFactor));
}

void DraggedTabView::ResizeContainer() {
  gfx::Size ps = GetPreferredSize();
  int w = ScaleValue(ps.width());
  int h = ScaleValue(ps.height());
#if defined(OS_WIN)
  SetWindowPos(container_->GetNativeView(), HWND_TOPMOST, 0, 0, w, h,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
#else
  gfx::Rect bounds;
  container_->GetBounds(&bounds, true);
  container_->SetBounds(gfx::Rect(bounds.x(), bounds.y(), w, h));
#endif
}

int DraggedTabView::ScaleValue(int value) {
  return attached_ ? value : static_cast<int>(value * kScalingFactor);
}
