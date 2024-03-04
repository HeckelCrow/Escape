#pragma once
#include "alias.hpp"

struct AudioBuffer
{
    AudioBuffer() {}
    Path path;
    bool streaming = false;

    // Pre loaded:
    u32 al_buffer = 0;
};

AudioBuffer LoadAudioFile(const Path& path, bool streaming = false);
void        DestroyAudioBuffer(AudioBuffer& buffer);

struct AudioPlaying
{
    AudioPlaying() {}
    s32 source_index = -1;
    u32 playing_id   = 0;
};

AudioPlaying PlayAudio(const AudioBuffer& buffer);
void         StopAudio(AudioPlaying& playing);
void         SetGain(AudioPlaying playing, f32 gain);

bool InitAudio(u32 source_count);
void TerminateAudio();

void UpdateAudio();