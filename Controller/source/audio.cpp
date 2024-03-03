#include "audio.hpp"
#include "print.hpp"
#include "scope_exit.hpp"

#include <al.h>
#include <alext.h>
#include <sndfile.h>

struct Audio
{
    Audio() {}
    ALCdevice*  device  = nullptr;
    ALCcontext* context = nullptr;

    std::vector<ALuint> free_sources;
    std::vector<ALuint> used_sources;
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

    auto context = alcCreateContext(audio.device, NULL);
    if (!alcMakeContextCurrent(context))
    {
        PrintError("alcMakeContextCurrent failed\n");
        return false;
    }

    audio.free_sources.resize(source_count);
    alGenSources(source_count, audio.free_sources.data());

    return true;
}

void
TerminateAudio()
{
    if (audio.free_sources.size())
    {
        alDeleteSources(audio.free_sources.size(), audio.free_sources.data());
        audio.free_sources.clear();
    }
    if (audio.used_sources.size())
    {
        alDeleteSources(audio.used_sources.size(), audio.used_sources.data());
        audio.used_sources.clear();
    }
    if (audio.device)
    {
        alcCloseDevice(audio.device);
    }
    if (audio.context)
    {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(audio.context);
    }
}

void
UpdateAudio()
{
    for (auto it = audio.used_sources.begin(); it != audio.used_sources.end();)
    {
        ALenum state;
        alGetSourcei(*it, AL_SOURCE_STATE, &state);

        if (state != AL_PLAYING)
        {
            // Paused or stopped
            audio.free_sources.push_back(*it);
            it = audio.used_sources.erase(it);
            continue;
        }

        auto err = alGetError();
        if (err != AL_NO_ERROR)
        {
            PrintError("OpenAL Error: {}\n", alGetString(err));
        }

        it++;
    }
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

    alGenBuffers(1, &buffer.buffer);
    alBufferData(buffer.buffer, format, audio_buffer.data(),
                 audio_buffer.size() * sizeof(f32), sfinfo.samplerate);

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
    if (buffer.buffer && alIsBuffer(buffer.buffer))
    {
        alDeleteBuffers(1, &buffer.buffer);
        buffer.buffer = 0;
    }
}

AudioPlayer
PlayAudio(AudioBuffer buffer)
{
    AudioPlayer player = {};
    if (audio.free_sources.empty())
    {
        PrintError("No free source available\n");
        return player;
    }

    player.source = audio.free_sources.back();
    audio.used_sources.push_back(player.source);
    audio.free_sources.pop_back();

    alSourcei(player.source, AL_BUFFER, (ALint)buffer.buffer);
    alSourcePlay(player.source);

    return player;
}