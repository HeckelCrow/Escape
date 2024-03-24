#include <Arduino.h>
#include "alias.hpp"
#include "msg.hpp"
#include "message_door_lock.hpp"

ClientId this_client_id = ClientId::DoorLock;

constexpr u8 MAGLOCK_DOOR   = 32;
constexpr u8 MAGLOCK_MORDOR = 33;
constexpr u8 LATCHLOCK_OUT  = 25; // Latch with button in series

DoorLockStatus status;

u32 latchlock_force_open_time = 0;

constexpr u8 tune_dutycycle   = 190;
constexpr u8 silent_dutycycle = 110;

void
setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    // Should be initialized to 0:
    pinMode(MAGLOCK_DOOR, OUTPUT);
    digitalWrite(MAGLOCK_DOOR, 0);

    pinMode(MAGLOCK_MORDOR, OUTPUT);
    digitalWrite(MAGLOCK_MORDOR, 0);

    pinMode(LATCHLOCK_OUT, OUTPUT);
    digitalWrite(LATCHLOCK_OUT, 0);
    ////

    for (u8 i = 0; i < 2; i++)
    {
        ledcSetup(i, 1000 /*Hz*/, 8 /*bits*/);
        ledcWrite(i, tune_dutycycle);
    }

    // delay(2000);
    Serial.println("Start wifi");
    StartWifi();
}

constexpr u16 tone_NoSound = 50000;
constexpr u16 tone_D5      = 587;
constexpr u16 tone_E5      = 659;
constexpr u16 tone_F5s     = 740;
constexpr u16 tone_G5      = 784;
constexpr u16 tone_A5      = 880;
constexpr u16 tone_B5      = 988;
constexpr u16 tone_C6s     = 1109;
constexpr u16 tone_D6      = 1175;

struct Note
{
    u16 f;
    u16 duration;
};

constexpr Note notes[]    = {{tone_NoSound, 1}, {tone_D5, 1},      {tone_E5, 1},
                          {tone_F5s, 4},     {tone_A5, 4},      {tone_F5s, 3},
                          {tone_E5, 1},      {tone_F5s, 1},     {tone_E5, 1},
                          {tone_D5, 7},      {tone_NoSound, 3}, {tone_F5s, 4},
                          {tone_A5, 2},      {tone_B5, 6},      {tone_D6, 2},
                          {tone_C6s, 6},     {tone_A5, 2},      {tone_F5s, 6},
                          {tone_G5, 1},      {tone_F5s, 1},     {tone_E5, 5}};
constexpr auto note_count = sizeof(notes) / sizeof(notes[0]);

u32 curr_note[2]       = {0};
u32 time_start_note[2] = {0};

void
SetCommand(DoorLockCommand cmd)
{
    u8 channel = 0;
    if (cmd.lock_door == LockState::SoftLock)
    {
        if (status.lock_door != LockState::SoftLock)
        {
            time_start_note[channel] = millis();
            curr_note[channel]       = 0;
            ledcChangeFrequency(channel, notes[curr_note[channel]].f, 8);
            ledcWrite(channel, silent_dutycycle);
            ledcAttachPin(MAGLOCK_DOOR, channel);
        }
    }
    else
    {
        if (status.lock_door == LockState::SoftLock)
        {
            ledcDetachPin(MAGLOCK_DOOR);
            time_start_note[channel] = 0;
        }
        digitalWrite(MAGLOCK_DOOR, (cmd.lock_door == LockState::Locked));
    }
    channel = 1;
    if (cmd.lock_mordor == LockState::SoftLock)
    {
        if (status.lock_mordor != LockState::SoftLock)
        {
            time_start_note[channel] = millis();
            curr_note[channel]       = 0;
            ledcChangeFrequency(channel, notes[curr_note[channel]].f, 8);
            ledcWrite(channel, silent_dutycycle);
            ledcAttachPin(MAGLOCK_MORDOR, channel);
        }
    }
    else
    {
        if (status.lock_mordor == LockState::SoftLock)
        {
            ledcDetachPin(MAGLOCK_MORDOR);
            time_start_note[channel] = 0;
        }
        digitalWrite(MAGLOCK_MORDOR, (cmd.lock_mordor == LockState::Locked));
    }

    u32 elapsed = millis() - latchlock_force_open_time;
    if (cmd.lock_tree == LatchLockState::ForceOpen
        && elapsed > latchlock_timeout_retry)
    {
        latchlock_force_open_time = millis();
        digitalWrite(LATCHLOCK_OUT, HIGH);
        Serial.println(F("Eject!"));
    }

    status.lock_door   = cmd.lock_door;
    status.lock_mordor = cmd.lock_mordor;
    status.lock_tree   = cmd.lock_tree;

    if (latchlock_force_open_time)
    {
        status.tree_open_duration = millis() - latchlock_force_open_time;
    }
    else
    {
        status.tree_open_duration = 0;
    }
}

void
loop()
{
    if (latchlock_force_open_time != 0)
    {
        u32 elapsed = millis() - latchlock_force_open_time;
        if (elapsed > latchlock_timeout)
        {
            digitalWrite(LATCHLOCK_OUT, LOW);
        }
        if (elapsed > latchlock_timeout_retry)
        {
            latchlock_force_open_time = 0;
            Serial.println(F("Reset!"));
        }
    }

    for (u8 i = 0; i < 2; i++)
    {
        if (time_start_note[i] != 0)
        {
            if (millis() - time_start_note[i]
                > notes[curr_note[i]].duration * 150)
            {
                curr_note[i]++;
                Serial.println(curr_note[i]);
                if (curr_note[i] < note_count)
                {
                    time_start_note[i] = millis();
                    ledcChangeFrequency(i, notes[curr_note[i]].f, 8);
                    if (notes[curr_note[i]].f == tone_NoSound)
                        ledcWrite(i, silent_dutycycle);
                    else
                        ledcWrite(i, tune_dutycycle);
                }
                else
                {
                    time_start_note[i] = 0;
                    ledcChangeFrequency(i, tone_NoSound, 8);
                    ledcWrite(i, silent_dutycycle);
                }
            }
        }
    }
    Message message = ReceiveMessage();
    switch (message.header.type)
    {
    case MessageType::DoorLockCommand:
    {
        DoorLockCommand cmd;
        cmd.serialize(message.deserializer);

        Serial.println(F("DoorLockCommand"));

        SetCommand(cmd);

        auto ser = Serializer(SerializerMode::Serialize,
                              {packet_buffer, udp_packet_size});

        status.getHeader().serialize(ser);

        status.serialize(ser);

        udp.beginPacket(server_connection.address, server_connection.port);
        BufferPtr buffer = {ser.full_buffer.start, ser.buffer.start};
        udp.write(buffer.start, buffer.end - buffer.start);
        udp.endPacket();
    }
    break;
    case MessageType::Reset:
    {
        digitalWrite(MAGLOCK_DOOR, 0);
        digitalWrite(MAGLOCK_MORDOR, 0);
        digitalWrite(LATCHLOCK_OUT, 0);
        delay(1000);
        ESP.restart();
    }
    break;
    }
}