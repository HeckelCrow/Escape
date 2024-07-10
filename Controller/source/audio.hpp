#pragma once
#include "alias.hpp"

/*
   AudioBuffer represents a sound file.
*/
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

/*
   AudioPlaying is a handle to a sound being played.
*/
struct AudioPlaying
{
    AudioPlaying() {}
    s32 source_index = -1;
    u32 playing_id   = 0;
};

struct AudioSettings
{
    AudioSettings() {}

    AudioSettings
    operator*(const AudioSettings& other)
    {
        AudioSettings res;
        res.gain  = gain * other.gain;
        res.pitch = pitch * other.pitch;
        return res;
    }

    f32 gain  = 1.f;
    f32 pitch = 1.f;
};

inline AudioSettings
Gain(f32 gain)
{
    AudioSettings s;
    s.gain = gain;
    return s;
}

inline AudioSettings
Pitch(f32 pitch)
{
    AudioSettings s;
    s.pitch = pitch;
    return s;
}

AudioPlaying PlayAudio(const AudioBuffer& buffer, AudioSettings s = {});
void         StopAudio(AudioPlaying& playing);
bool         IsPlaying(const AudioPlaying& playing);
void         SetGain(AudioPlaying playing, f32 gain);
void         SetPitch(AudioPlaying playing, f32 pitch);

bool InitAudio(u32 source_count);
void TerminateAudio();

void UpdateAudio();