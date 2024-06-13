#include <Arduino.h>
#include "alias.hpp"
#include "msg.hpp"
#include "message_door_lock.hpp"
#include "serial_in.hpp"

ClientId this_client_id = ClientId::DoorLock;

constexpr u8 MAGLOCK_DOOR   = 32;
constexpr u8 MAGLOCK_MORDOR = 33;
constexpr u8 LATCHLOCK_OUT  = 25; // Latch with button in series

DoorLockStatus status;

u32 latchlock_force_open_time = 0;

constexpr u8 soft_lock_dutycycle = 110;

void
PrintSerialCommands()
{
    Serial.println(F(""));
    Serial.println(F("Serial commands:"));
    Serial.println(F("reset"));
    Serial.println(F("scan"));
    Serial.println(F(""));
}

void
UpdateSerial()
{
    if (auto* str = ReadSerial())
    {
        if (strcmp(str, "reset") == 0)
        {
            Serial.println(F("Reset now."));
            ESP.restart();
        }
        else if (strcmp(str, "scan") == 0)
        {
            WifiScan();
        }
        else
        {
            Serial.print(F("Unknown command: "));
            Serial.println(str);
            PrintSerialCommands();
        }
    }
}

void
setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println(F("Hello"));

    PrintSerialCommands();

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
        ledcWrite(i, soft_lock_dutycycle);
    }

    Serial.println("Start wifi");
    StartWifi();
}

void
SetCommand(DoorLockCommand cmd)
{
    if (cmd.lock_door == LockState::SoftLock)
    {
        if (status.lock_door != LockState::SoftLock)
        {
            ledcAttachPin(MAGLOCK_DOOR, 0);
        }
    }
    else
    {
        if (status.lock_door == LockState::SoftLock)
        {
            ledcDetachPin(MAGLOCK_DOOR);
        }
        digitalWrite(MAGLOCK_DOOR, (cmd.lock_door == LockState::Locked));
    }

    if (cmd.lock_mordor == LockState::SoftLock)
    {
        if (status.lock_mordor != LockState::SoftLock)
        {
            ledcAttachPin(MAGLOCK_MORDOR, 1);
        }
    }
    else
    {
        if (status.lock_mordor == LockState::SoftLock)
        {
            ledcDetachPin(MAGLOCK_MORDOR);
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
    UpdateSerial();

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