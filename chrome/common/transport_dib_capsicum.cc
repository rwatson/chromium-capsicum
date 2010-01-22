// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/transport_dib.h"

#include <unistd.h>
#include <sys/stat.h>

#include "base/eintr_wrapper.h"
#include "base/shared_memory.h"
#include "skia/ext/platform_canvas.h"

TransportDIB::TransportDIB()
    : size_(0) {
}

TransportDIB::TransportDIB(TransportDIB::Handle dib)
    : shared_memory_(dib, false /* read write */),
      size_(0) {
}

TransportDIB::~TransportDIB() {
}

// static
TransportDIB* TransportDIB::Create(size_t size, uint32 sequence_num) {
  TransportDIB* dib = new TransportDIB;
  if (!dib->shared_memory_.Create(L"", false /* read write */,
                                  false /* do not open existing */, size)) {
    delete dib;
    return NULL;
  }

  dib->size_ = size;
  return dib;
}

// static
TransportDIB* TransportDIB::Map(TransportDIB::Handle handle) {
  if (!is_valid(handle))
    return NULL;

  TransportDIB* dib = new TransportDIB(handle);
  struct stat st;
  if ((fstat(handle.fd, &st) != 0) ||
      (!dib->shared_memory_.Map(st.st_size))) {
    delete dib;
    HANDLE_EINTR(close(handle.fd));
    return NULL;
  }

  dib->size_ = st.st_size;

  return dib;
}

bool TransportDIB::is_valid(Handle dib) {
  return dib.fd >= 0;
}

skia::PlatformCanvas* TransportDIB::GetPlatformCanvas(int w, int h) {
  return new skia::PlatformCanvas(w, h, true,
                                  reinterpret_cast<uint8_t*>(memory()));
}

void* TransportDIB::memory() const {
  return shared_memory_.memory();
}

TransportDIB::Id TransportDIB::id() const {
  return shared_memory_.id();
}

TransportDIB::Handle TransportDIB::handle() const {
  return shared_memory_.handle();
}
