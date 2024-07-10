#include "timer.hpp"
#include "print.hpp"

#include <imgui.h>

s64
DivideAndRoundDown(s64 numerator, s64 denominator)
{
    if (numerator < 0 && numerator % denominator != 0)
    {
        return numerator / denominator - 1;
    }
    return numerator / denominator;
}
Timer::Timer()
{
    last_measure = Clock::now();
    paused       = true;

    for (auto const& dir_entry :
         std::filesystem::directory_iterator{"data/timer/"})
    {
        if (dir_entry.is_regular_file())
        {
            Print("Loading {}\n", dir_entry.path().string());
            sounds.push_back(LoadAudioFile(dir_entry.path()));
        }
    }
}
Timer::~Timer()
{
    for (auto& sound : sounds)
        DestroyAudioBuffer(sound);
}

void
DrawTimer(Timer& timer)
{
    bool editing = false;
    if (ImGui::Begin(utf8("Chrono")))
    {
        ImGui::Text(utf8("Chrono"));
        auto millis      = (s32)DivideAndRoundDown(timer.time.count(), 1'000);
        s32  minutes     = millis / 1000 / 60;
        s32  seconds     = millis / 1000 % 60;
        bool update_time = false;
        auto flags       = ImGuiInputTextFlags_AutoSelectAll;
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("##Minutes", &minutes, 1, 100, flags))
        {
            update_time = true;
        }
        if (ImGui::IsItemActive())
        {
            editing = true;
        }
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("##Seconds", &seconds, 1, 100, flags))
        {
            update_time = true;
        }
        if (ImGui::IsItemActive())
        {
            editing = true;
        }
        if (update_time)
        {
            timer.time = Seconds((s64)minutes * 60 + (s64)seconds);
        }

        ImGui::BeginDisabled(!timer.paused);
        if (ImGui::Button(utf8("Go!")))
        {
            timer.paused = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(timer.paused);
        if (ImGui::Button(utf8("Pause")))
        {
            timer.paused = true;
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!timer.paused);
        if (ImGui::Button(utf8("Remise à zero")))
        {
            auto ms      = timer.time.count() / 1000;
            auto sec     = ms / 1000 % 60;
            auto minutes = ms / 1000 / 60;
            PrintSuccess("Last time: {:02}:{:02}\n", minutes, sec);
            timer.time = Seconds(0);
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)
            && !timer.paused)
        {
            if (ImGui::BeginTooltip())
            {
                ImGui::Text(utf8("Le bouton de remise à zero est désactivé "
                                 "lorsque le chrono est lancé pour éviter de "
                                 "cliquer dessus par erreur."));
                ImGui::EndTooltip();
            }
        }

        ImGui::Separator();
        ImGui::Text(utf8("Rappel toutes les"));
        ImGui::SameLine();
        s32 reminder = timer.reminder_period.count() / 1'000'000 / 60;
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt(utf8("minutes##Rappel toutes les"), &reminder))
        {
            timer.reminder_period = Minutes(reminder);
        }
        ImGui::Checkbox(utf8("Jouer automatiquement"), &timer.play_sound_auto);
        if (ImGui::Button(utf8("Jouer manuellement")))
        {
            StopAudio(timer.playing);
            timer.playing = PlayAudio(timer.sounds[timer.sound_selected]);
            SetGain(timer.playing, timer.sound_gain / 100.f);
        }
        if (IsPlaying(timer.playing))
        {
            if (ImGui::Button(utf8("Arrêter le rappel")))
            {
                StopAudio(timer.playing);
            }
        }

        auto& sound_selected = timer.sounds[timer.sound_selected];
        if (ImGui::BeginCombo(utf8("Son"),
                              sound_selected.path.filename().string().c_str()))
        {
            u32 i = 0;
            for (auto& sound : timer.sounds)
            {
                const bool is_selected = (i == timer.sound_selected);
                if (ImGui::Selectable(sound.path.filename().string().c_str(),
                                      is_selected))
                {
                    timer.sound_selected = i;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();

                i++;
            }
            ImGui::EndCombo();
        }

        if (ImGui::SliderInt(utf8("Volume"), &timer.sound_gain, 0, 100))
        {
            SetGain(timer.playing, timer.sound_gain / 100.f);
        }
    }
    ImGui::End();

    auto now = Clock::now();
    auto elapsed =
        std::chrono::duration_cast<Duration>(now - timer.last_measure);
    if (!timer.paused && !editing)
    {
        auto prev = timer.time;
        timer.time += elapsed;

        if (timer.play_sound_auto && timer.reminder_period != Minutes(0))
        {
            if (prev / timer.reminder_period
                < timer.time / timer.reminder_period)
            {
                StopAudio(timer.playing);
                timer.playing = PlayAudio(timer.sounds[timer.sound_selected]);
                SetGain(timer.playing, timer.sound_gain / 100.f);
            }
        }
    }
    timer.last_measure = now;
}