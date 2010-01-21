// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/freebsd/audio_manager_freebsd.h"

#include "base/at_exit.h"
#include "base/logging.h"
//#include "media/audio/fake_audio_output_stream.h"
//#include "media/audio/linux/alsa_output.h"
//#include "media/audio/linux/alsa_wrapper.h"

namespace {
AudioManagerFreeBSD* g_audio_manager = NULL;
}  // namespace

// Implementation of AudioManager.
bool AudioManagerFreeBSD::HasAudioDevices() {
  NOTIMPLEMENTED();
  return false;
}

AudioOutputStream* AudioManagerFreeBSD::MakeAudioStream(Format format,
                                                        int channels,
                                                        int sample_rate,
                                                        char bits_per_sample) {
  NOTIMPLEMENTED();
  return NULL;
}

AudioManagerFreeBSD::AudioManagerFreeBSD() {
}

AudioManagerFreeBSD::~AudioManagerFreeBSD() {
}

void AudioManagerFreeBSD::Init() {
}

void AudioManagerFreeBSD::MuteAll() {
  NOTIMPLEMENTED();
}

void AudioManagerFreeBSD::UnMuteAll() {
  NOTIMPLEMENTED();
}

// TODO(ajwong): Collapse this with the windows version.
void DestroyAudioManagerFreeBSD(void* not_used) {
  delete g_audio_manager;
  g_audio_manager = NULL;
}

AudioManager* AudioManager::GetAudioManager() {
  if (!g_audio_manager) {
    g_audio_manager = new AudioManagerFreeBSD();
    g_audio_manager->Init();
    base::AtExitManager::RegisterCallback(&DestroyAudioManagerFreeBSD, NULL);
  }
  return g_audio_manager;
}
