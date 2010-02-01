// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_main_platform_delegate.h"

#include "base/debug_util.h"
#include "base/rand_util.h"

#include <sys/capability.h>
#include <libcapsicum.h>

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  int err;

  err = lc_limitfd(STDIN_FILENO, CAP_EVENT | CAP_FSTAT | CAP_READ |
    CAP_SEEK);
  if (err)
    return false;
  err = lc_limitfd(STDOUT_FILENO, CAP_EVENT | CAP_FSTAT | CAP_SEEK |
    CAP_WRITE);
  if (err)
    return false;
  err = lc_limitfd(STDERR_FILENO, CAP_EVENT | CAP_FSTAT | CAP_SEEK |
    CAP_WRITE);
  if (err)
    return false;
  if (cap_enter() < 0)
    return false;
  return true;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
}
