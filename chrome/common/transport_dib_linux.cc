// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "base/gfx/size.h"
#include "base/logging.h"
#include "chrome/common/transport_dib.h"
#include "chrome/common/x11_util.h"
#include "skia/ext/platform_canvas.h"

// The shmat system call uses this as it's invalid return address
static void *const kInvalidAddress = (void*) -1;

TransportDIB::TransportDIB()
    : key_(-1),
      address_(kInvalidAddress),
      x_shm_(0),
      display_(NULL),
      size_(0) {
}

TransportDIB::~TransportDIB() {
  if (address_ != kInvalidAddress) {
    shmdt(address_);
#if defined(OS_FREEBSD)
    shmctl(key_, IPC_RMID, 0);
#endif
    address_ = kInvalidAddress;
  }

  if (x_shm_) {
    DCHECK(display_);
    x11_util::DetachSharedMemory(display_, x_shm_);
  }
}

// static
TransportDIB* TransportDIB::Create(size_t size, uint32 sequence_num) {
  // We use a mode of 0666 since the X server won't attach to memory which is
  // 0600 since it can't know if it (as a root process) is being asked to map
  // someone else's private shared memory region.
  const int shmkey = shmget(IPC_PRIVATE, size, 0666);
  if (shmkey == -1) {
    DLOG(ERROR) << "Failed to create SysV shared memory region"
                << " errno:" << errno;
    return NULL;
  }

  void* address = shmat(shmkey, NULL /* desired address */, 0 /* flags */);
  // Here we mark the shared memory for deletion. Since we attached it in the
  // line above, it doesn't actually get deleted but, if we crash, this means
  // that the kernel will automatically clean it up for us.
#if !defined(OS_FREEBSD)
// BSD: A shmctl IPC_RMID call here renders all future shared memory calls for
// BSD: a particular key to fail on FreeBSD, so I moved this call to the
// BSD: destructor.  Of course, this means chromium crashes on FreeBSD don't
// BSD: clean up shared memory.
  shmctl(shmkey, IPC_RMID, 0);
#endif
  if (address == kInvalidAddress)
    return NULL;

  TransportDIB* dib = new TransportDIB;

  dib->key_ = shmkey;
  dib->address_ = address;
  dib->size_ = size;
  return dib;
}

TransportDIB* TransportDIB::Map(Handle shmkey) {
  struct shmid_ds shmst;
  if (shmctl(shmkey, IPC_STAT, &shmst) == -1)
    return NULL;

  void* address = shmat(shmkey, NULL /* desired address */, 0 /* flags */);
  if (address == kInvalidAddress)
    return NULL;

  TransportDIB* dib = new TransportDIB;

  dib->address_ = address;
  dib->size_ = shmst.shm_segsz;
  dib->key_ = shmkey;
  return dib;
}

bool TransportDIB::is_valid(Handle dib) {
  return dib >= 0;
}

skia::PlatformCanvas* TransportDIB::GetPlatformCanvas(int w, int h) {
  return new skia::PlatformCanvas(w, h, true,
                                  reinterpret_cast<uint8_t*>(memory()));
}

void* TransportDIB::memory() const {
  DCHECK_NE(address_, kInvalidAddress);
  return address_;
}

TransportDIB::Id TransportDIB::id() const {
  return key_;
}

TransportDIB::Handle TransportDIB::handle() const {
  return key_;
}

XID TransportDIB::MapToX(Display* display) {
  if (!x_shm_) {
    x_shm_ = x11_util::AttachSharedMemory(display, key_);
    display_ = display;
  }

  return x_shm_;
}
