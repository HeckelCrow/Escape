#include "audio.hpp"
#include "print.hpp"
#include "scope_exit.hpp"

#include <al.h>
#include <alext.h>
#include <sndfile.h>

struct Source
{
    Source() {}
    ALuint al_source  = 0;
    u32    playing_id = 0;

    u32 next_playing_id = 1;
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
InitAudio(u32 source_count)
{
    audio = {};

    auto enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
    if (enumeration)
    {
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

        ALenum state;
        alGetSourcei(source.al_source, AL_SOURCE_STATE, &state);

        if (state == AL_PLAYING)
        {
            highest_playing_index = std::max(highest_playing_index, index);
        }
        else if (source.playing_id)
        {
            // Just got paused or stopped
            source.playing_id = 0;
            alSourcei(source.al_source, AL_BUFFER, NULL);
        }

        auto err = alGetError();
        if (err != AL_NO_ERROR)
        {
            PrintError("OpenAL Error: {}\n", alGetString(err));
        }
    }
    audio.source_playing_count = highest_playing_index + 1;
}

AudioBuffer
LoadAudioFile(const Path& path)
{
    AudioBuffer buffer;
    auto        filename = path.string();
    SF_INFO     sfinfo;
    auto        sndfile = sf_open(filename.c_str(), SFM_READ, &sfinfo);

    if (!sndfile)
    {
        PrintError("Could not open audio in {}: {}\n", filename.c_str(),
                   sf_strerror(sndfile));
    }

    SCOPE_EXIT({ sf_close(sndfile); });

    if (sfinfo.frames < 1)
    {
        PrintError("Bad sample count in {} ({})\n", filename.c_str(),
                   sfinfo.frames);
        return {};
    }

    if (!alIsExtensionPresent("AL_EXT_FLOAT32"))
    {
        PrintError("AL_EXT_FLOAT32 extension not present\n");
        return {};
    }
    ALint splblockalign  = 1;
    ALint byteblockalign = sfinfo.channels * 4;
    auto  format         = AL_NONE;

    if (sfinfo.channels == 1)
    {
        format = AL_FORMAT_MONO_FLOAT32;
    }
    else if (sfinfo.channels == 2)
    {
        format = AL_FORMAT_STEREO_FLOAT32;
    }

    if (sfinfo.frames / splblockalign > (sf_count_t)(INT_MAX / byteblockalign))
    {
        PrintError("Too many samples in {} ({})\n", filename, sfinfo.frames);
        return {};
    }

    std::vector<f32> audio_buffer;
    audio_buffer.resize(sfinfo.channels * sfinfo.frames);

    auto num_frames =
        sf_readf_float(sndfile, audio_buffer.data(), sfinfo.frames);

    if (num_frames < 1)
    {
        PrintError("Failed to read samples in {} ({})\n", filename, num_frames);
        return {};
    }

    alGenBuffers(1, &buffer.al_buffer);
    alBufferData(buffer.al_buffer, format, audio_buffer.data(),
                 (ALsizei)audio_buffer.size() * sizeof(f32), sfinfo.samplerate);

    audio_buffer.clear();
    audio_buffer.shrink_to_fit();

    auto err = alGetError();
    if (err != AL_NO_ERROR)
    {
        PrintError("OpenAL Error: {}\n", alGetString(err));
        return {};
    }

    err = alGetError();
    if (err != AL_NO_ERROR)
    {
        PrintError("OpenAL Error: {}\n", alGetString(err));
        return {};
    }

    return buffer;
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
PlayAudio(const AudioBuffer& buffer)
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

    Print("Playing buffer {} on source {} (id {})\n", buffer.al_buffer,
          source.al_source, playing.playing_id);
    alSourcei(source.al_source, AL_BUFFER, (ALint)buffer.al_buffer);
    alSourcePlay(source.al_source);

    return playing;
}

void
StopAudio(AudioPlaying playing)
{
    const auto& source = audio.sources[playing.source_index];

    if (source.playing_id == playing.playing_id)
    {
        alSourceStop(source.al_source);
    }
}