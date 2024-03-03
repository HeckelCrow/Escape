#pragma once
#include "alias.hpp"

bool InitAudio(u32 source_count);
void TerminateAudio();

void UpdateAudio();

struct AudioBuffer
{
    AudioBuffer() {}
    u32 buffer = 0;
};

AudioBuffer LoadAudioFile(const Path& path);
void        DestroyAudioBuffer(AudioBuffer& buffer);

struct AudioPlayer
{
    AudioPlayer() {}
    u32 buffer = 0;
    u32 source = 0;
};

AudioPlayer PlayAudio(AudioBuffer buffer);