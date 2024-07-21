#include "audio.hpp"
#include "print.hpp"
#include "scope_exit.hpp"

#include <al.h>
#include <alext.h>
#include <sndfile.h>

/*
   A Source plays one sound file at a time.
*/
struct Source
{
    Source() {}
    ALuint al_source  = 0;
    u32    playing_id = 0;

    u32  next_playing_id = 1;
    bool should_stop     = false;
    bool streaming       = false;
    // Streaming:
    SF_INFO  sf_info;
    SNDFILE* snd_file = nullptr;

    sconst u32 buffer_count             = 4;
    sconst s32 buffer_size              = 8 * 1024;
    u32        al_buffers[buffer_count] = {0};
};

struct Audio
{
    Audio() {}
    ALCdevice*  device  = nullptr;
    ALCcontext* context = nullptr;

    std::vector<Source> sources;
    s32                 source_playing_count = 0;
};

Audio audio;

bool
LoadFileToBuffer(SNDFILE* sndfile, SF_INFO sf_info, ALuint al_buffer,
                 sf_count_t sample_count)
{
    auto format = AL_NONE;
    if (sf_info.channels == 1)
    {
        format = AL_FORMAT_MONO_FLOAT32;
    }
    else if (sf_info.channels == 2)
    {
        format = AL_FORMAT_STEREO_FLOAT32;
    }

    if (sample_count / sizeof(f32) > (sf_count_t)(INT_MAX / sizeof(f32)))
    {
        PrintError("Too many samples ({})\n", sample_count);
        return false;
    }

    std::vector<f32> audio_buffer;
    audio_buffer.resize((u64)sf_info.channels * sample_count);

    auto num_frames =
        sf_readf_float(sndfile, audio_buffer.data(), sample_count);

    if (num_frames < 1)
    {
        return false;
    }

    alBufferData(al_buffer, format, audio_buffer.data(),
                 (ALsizei)audio_buffer.size() * sizeof(f32),
                 sf_info.samplerate);

    auto err = alGetError();
    if (err != AL_NO_ERROR)
    {
        PrintError("OpenAL Error: {}\n", alGetString(err));
        return {};
    }

    return true;
}

AudioBuffer
LoadAudioFile(const Path& path, bool streaming)
{
    // TODO: Maybe this should return an AudioBuffer and a bool. Right now we
    // can only test if al_buffer is not 0. We can't tell if the file is valid
    // when we want to stream it, because we need a Source to start loading a
    // file with streaming.
    AudioBuffer buffer;
    buffer.path      = path;
    buffer.streaming = streaming;

    if (!streaming)
    {
        auto    filename = path.string();
        SF_INFO sf_info;
        auto    sndfile = sf_open(filename.c_str(), SFM_READ, &sf_info);

        if (!sndfile)
        {
            PrintError("Could not open audio in {}: {}\n", filename.c_str(),
                       sf_strerror(sndfile));
            return {};
        }

        SCOPE_EXIT({ sf_close(sndfile); });

        if (sf_info.frames < 1)
        {
            PrintError("Bad sample count in {} ({})\n", filename.c_str(),
                       sf_info.frames);
            return {};
        }

        if (!alIsExtensionPresent("AL_EXT_FLOAT32"))
        {
            PrintError("AL_EXT_FLOAT32 extension not present\n");
            return {};
        }

        alGenBuffers(1, &buffer.al_buffer);
        LoadFileToBuffer(sndfile, sf_info, buffer.al_buffer, sf_info.frames);
    }

    return buffer;
}

void
InitStream(const Path& path, Source& source)
{
    auto filename   = path.string();
    source.snd_file = sf_open(filename.c_str(), SFM_READ, &source.sf_info);

    if (!source.snd_file)
    {
        PrintError("Could not open audio in {}: {}\n", filename.c_str(),
                   sf_strerror(source.snd_file));
    }

    if (source.sf_info.frames < 1)
    {
        PrintError("Bad sample count in {} ({})\n", filename.c_str(),
                   source.sf_info.frames);
        return;
    }

    if (!alIsExtensionPresent("AL_EXT_FLOAT32"))
    {
        PrintError("AL_EXT_FLOAT32 extension not present\n");
        return;
    }

    alGenBuffers(source.buffer_count, source.al_buffers);

    for (u32 i = 0; i < source.buffer_count; i++)
    {
        auto al_buffer = source.al_buffers[i];
        if (!LoadFileToBuffer(source.snd_file, source.sf_info, al_buffer,
                              source.buffer_size))
        {
            break;
        }
        alSourceQueueBuffers(source.al_source, 1, &al_buffer);
    }
    alSourcePlay(source.al_source);

    auto err = alGetError();
    if (err != AL_NO_ERROR)
    {
        PrintError("OpenAL Error: {}\n", alGetString(err));
    }
}

bool
UpdateStream(Source& source)
{
    ALint processed = 0;

    alGetSourcei(source.al_source, AL_BUFFERS_PROCESSED, &processed);

    auto err = alGetError();
    if (err != AL_NO_ERROR)
    {
        PrintError("OpenAL Error: {}\n", alGetString(err));
        return false;
    }

    while (processed > 0)
    {
        ALuint free_buffer = 0;

        alSourceUnqueueBuffers(source.al_source, 1, &free_buffer);
        processed--;

        if (LoadFileToBuffer(source.snd_file, source.sf_info, free_buffer,
                             source.buffer_size))
        {
            alSourceQueueBuffers(source.al_source, 1, &free_buffer);
        }
    }

    if (!source.should_stop)
    {
        ALint state;
        alGetSourcei(source.al_source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING && state != AL_PAUSED)
        {
            ALint queued;
            alGetSourcei(source.al_source, AL_BUFFERS_QUEUED, &queued);
            if (queued == 0)
            {
                return false;
            }
            PrintWarning("Underrun\n");
            alSourcePlay(source.al_source);

            auto err = alGetError();
            if (err != AL_NO_ERROR)
            {
                PrintError("OpenAL Error: {}\n", alGetString(err));
            }
        }
    }

    return true;
}

void
TerminateStream(Source& source)
{
    if (source.snd_file)
    {
        sf_close(source.snd_file);
    }
    if (source.al_buffers[0] && alIsBuffer(source.al_buffers[0]))
    {
        alDeleteBuffers(source.buffer_count, source.al_buffers);
    }
}

void
DestroyAudioBuffer(AudioBuffer& buffer)
{
    if (buffer.al_buffer && alIsBuffer(buffer.al_buffer))
    {
        // TODO: Is this too slow? Does it matter?
        for (auto& source : audio.sources)
        {
            if (source.playing_id)
            {
                ALint buffer_playing = 0;
                alGetSourcei(source.al_source, AL_BUFFER, &buffer_playing);
                if (buffer_playing == buffer.al_buffer)
                {
                    alSourceStop(source.al_source);
                    alSourcei(source.al_source, AL_BUFFER, NULL);
                }
            }
        }
        alDeleteBuffers(1, &buffer.al_buffer);
        buffer.al_buffer = 0;
    }
}

AudioPlaying
PlayAudio(const AudioBuffer& buffer, AudioSettings s)
{
    AudioPlaying playing = {};

    for (s32 i = 0; i < (s32)audio.sources.size(); i++)
    {
        auto& source = audio.sources[i];
        if (source.playing_id == 0)
        {
            // This source is free
            playing.source_index = i;
            break;
        }
    }

    if (playing.source_index == -1)
    {
        PrintError("No free source available\n");
        return playing;
    }

    auto& source       = audio.sources[playing.source_index];
    playing.playing_id = source.next_playing_id++;
    if (!source.next_playing_id)
        source.next_playing_id++;
    source.playing_id = playing.playing_id;

    audio.source_playing_count =
        std::max(audio.source_playing_count, playing.source_index + 1);

    source.streaming = buffer.streaming;
    if (buffer.streaming)
    {
        InitStream(buffer.path, source);
    }
    else
    {
        alSourcei(source.al_source, AL_BUFFER, (ALint)buffer.al_buffer);
        alSourcef(source.al_source, AL_GAIN, s.gain * s.gain);
        alSourcef(source.al_source, AL_PITCH, s.pitch);
        alSourcePlay(source.al_source);
    }

    source.should_stop = false;

    Print("Playing {} on source {} (id {})\n", buffer.path.filename().string(),
          source.al_source, playing.playing_id);

    return playing;
}

void
StopAudio(AudioPlaying& playing)
{
    if (playing.source_index < 0
        || playing.source_index >= audio.sources.size())
    {
        return;
    }

    auto& source = audio.sources[playing.source_index];

    if (source.playing_id == playing.playing_id)
    {
        alSourceStop(source.al_source);
        source.should_stop   = true;
        playing.source_index = -1;
    }
}

bool
IsPlaying(const AudioPlaying& playing)
{
    if (playing.source_index < 0
        || playing.source_index >= audio.sources.size())
    {
        return false;
    }
    auto& source = audio.sources[playing.source_index];

    if (source.playing_id == playing.playing_id)
    {
        return true;
    }
    return false;
}

void
SetGain(AudioPlaying playing, f32 gain)
{
    if (playing.source_index < 0
        || playing.source_index >= audio.sources.size())
    {
        return;
    }
    auto& source = audio.sources[playing.source_index];
    if (source.playing_id == playing.playing_id)
    {
        alSourcef(source.al_source, AL_GAIN, gain * gain);
    }
}

void
SetPitch(AudioPlaying playing, f32 pitch)
{
    if (playing.source_index < 0
        || playing.source_index >= audio.sources.size())
    {
        return;
    }
    auto& source = audio.sources[playing.source_index];
    if (source.playing_id == playing.playing_id)
    {
        Print("Set source {} (id {}) pitch to {}\n", source.al_source,
              playing.playing_id, pitch);
        alSourcef(source.al_source, AL_PITCH, pitch);
    }
}

bool
InitAudio(u32 source_count)
{
    audio = {};

    auto enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
    if (enumeration)
    {
        // We list the audio devices available. Right now we don't use it, we
        // pick the default one.
        auto device_names = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
        Print("Devices:\n");

        auto curr = device_names;
        while (curr && *curr != '\0')
        {
            StrPtr name = curr;
            curr += name.size() + 1;
            Print("{}\n", name);
        }
    }

    audio.device = alcOpenDevice(NULL);
    if (!audio.device)
    {
        PrintError("alcOpenDevice failed\n");
        return false;
    }

    ALCint max_source_count = 0;
    alcGetIntegerv(audio.device, ALC_MONO_SOURCES, 1, &max_source_count);
    Print("Max mono source count: {}\n", max_source_count);

    max_source_count = 0;
    alcGetIntegerv(audio.device, ALC_STEREO_SOURCES, 1, &max_source_count);
    Print("Max stereo source count: {}\n", max_source_count);

    audio.context = alcCreateContext(audio.device, NULL);
    if (!alcMakeContextCurrent(audio.context))
    {
        PrintError("alcMakeContextCurrent failed\n");
        return false;
    }

    // source_count is the maximum number of sound played at the same time. Some
    // sounds may have long trailing silences that still take up a source until
    // they end or are stopped.
    audio.sources.resize(source_count);
    for (auto& source : audio.sources)
    {
        alGenSources(1, &source.al_source);
    }

    return true;
}

void
TerminateAudio()
{
    if (audio.sources.size())
    {
        for (auto& source : audio.sources)
        {
            alDeleteSources(1, &source.al_source);
        }
        audio.sources.clear();
    }
    if (audio.context)
    {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(audio.context);
    }
    if (audio.device)
    {
        alcCloseDevice(audio.device);
    }
}

void
UpdateAudio()
{
    s32 highest_playing_index = -1;
    for (s32 i = 0; i < audio.source_playing_count; i++)
    {
        s32     index  = audio.source_playing_count - i - 1;
        Source& source = audio.sources[index];

        if (source.streaming)
        {
            UpdateStream(source);
        }

        ALenum state;
        alGetSourcei(source.al_source, AL_SOURCE_STATE, &state);

        if (state == AL_PLAYING)
        {
            highest_playing_index = std::max(highest_playing_index, index);
        }
        else if (source.playing_id)
        {
            // Just got paused or stopped

            source.playing_id  = 0;
            source.should_stop = true;
            alSourcei(source.al_source, AL_BUFFER, NULL);

            if (source.streaming)
            {
                TerminateStream(source);
            }
        }

        auto err = alGetError();
        if (err != AL_NO_ERROR)
        {
            PrintError("OpenAL Error: {}\n", alGetString(err));
        }
    }
    audio.source_playing_count = highest_playing_index + 1;
}