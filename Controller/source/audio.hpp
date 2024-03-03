#pragma once
#include "alias.hpp"

bool InitAudio(u32 source_count);
void TerminateAudio();

void UpdateAudio();

struct AudioBuffer
{
    AudioBuffer() {}
    u32 al_buffer = 0;
};

AudioBuffer LoadAudioFile(const Path& path);
void        DestroyAudioBuffer(AudioBuffer& buffer);

struct AudioPlaying
{
    AudioPlaying() {}
    s32 source_index = -1;
    u32 playing_id   = 0;
};

AudioPlaying PlayAudio(const AudioBuffer& buffer);
void         StopAudio(AudioPlaying playing);