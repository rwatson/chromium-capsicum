// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_LINUX_AUDIO_MANAGER_FREEBSD_H_
#define MEDIA_AUDIO_LINUX_AUDIO_MANAGER_FREEBSD_H_

//#include <map>

//#include "base/ref_counted.h"
//#include "base/scoped_ptr.h"
//#include "base/thread.h"
#include "media/audio/audio_output.h"

class AudioManagerFreeBSD : public AudioManager {
 public:
  AudioManagerFreeBSD();

  // Call before using a newly created AudioManagerFreeBSD instance.
  virtual void Init();

  // Implementation of AudioManager.
  virtual bool HasAudioDevices();
  virtual AudioOutputStream* MakeAudioStream(Format format, int channels,
                                             int sample_rate,
                                             char bits_per_sample);
  virtual void MuteAll();
  virtual void UnMuteAll();

 protected:
  // Friend function for invoking the destructor at exit.
  friend void DestroyAudioManagerFreeBSD(void*);
  virtual ~AudioManagerFreeBSD();

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioManagerFreeBSD);
};

#endif  // MEDIA_AUDIO_LINUX_AUDIO_MANAGER_FREEBSD_H_
