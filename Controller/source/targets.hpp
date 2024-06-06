#pragma once
#include "alias.hpp"
#include "client.hpp"
#include "msg/message_targets.hpp"
#include "audio.hpp"

constexpr f32 orc_pitch_min = 0.7f;
constexpr f32 orc_pitch_max = 1.2f;

struct Targets
{
    Targets();
    ~Targets();
    void receiveMessage(Client& client, const TargetsStatus& msg, bool print);
    void update(Client& client);

    TargetsCommand command;
    TargetsStatus  last_status;

    AudioPlaying sound_playing[target_count];
    Timepoint    time_last_sound;

    std::vector<AudioBuffer> orcs;
    std::vector<AudioBuffer> orc_deaths;
    std::vector<AudioBuffer> orc_hurts;
    std::vector<AudioBuffer> orc_mads;

    s32 gain_global    = 0;
    s32 gain_orcs      = 70;
    s32 gain_orcs_hurt = 100; // For hurt and death sounds

    s32 min_time_between_sounds = 700;
    s32 sound_probability       = 200;
};