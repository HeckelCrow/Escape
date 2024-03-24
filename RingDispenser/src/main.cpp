#include <Arduino.h>

#include "alias.hpp"
#include "serial_in.hpp"
#include "msg.hpp"
#include "message_ring_dispenser.hpp"

#if WROOM
constexpr u8 SERVO_COMMAND = 15;

constexpr u8 column_count              = 4;
constexpr u8 row_count                 = 5;
constexpr u8 PIN_COLUMNS[column_count] = {23, 22, 21, 19};
constexpr u8 PIN_ROWS[row_count]       = {32, 33, 25, 26, 27};
#endif

ClientId this_client_id = ClientId::RingDispenser;

RingDispenserStatus status = {};

u32           last_cmd_rings       = 0;
constexpr u32 resend_period        = 100;
u32           time_last_state_sent = 0;

constexpr u16 pwm_bits        = 12;
constexpr u16 servo_default   = ((u16)1 << pwm_bits) / 20 * 2.5;
constexpr u16 servo_activated = ((u16)1 << pwm_bits) / 20 * 1;

void
setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.print(F("Init\n"));

    ledcSetup(0, 50 /*Hz*/, pwm_bits);
    ledcAttachPin(SERVO_COMMAND, 0);
    ledcWrite(0, servo_default);

    for (auto col : PIN_COLUMNS)
    {
        pinMode(col, OUTPUT);
    }
    for (auto row : PIN_ROWS)
    {
        pinMode(row, INPUT_PULLDOWN);
    }

    StartWifi();
}

void
loop()
{
    Message message = ReceiveMessage();
    switch (message.header.type)
    {
    case MessageType::RingDispenserCommand:
    {
        RingDispenserCommand cmd;
        cmd.serialize(message.deserializer);

        Serial.println(F("RingDispenserCommand"));

        if (cmd.state == RingDispenserState::ForceActivate)
        {
            if (ledcRead(0) != servo_activated)
                ledcWrite(0, servo_activated);
        }
        else if (cmd.state == RingDispenserState::ForceDeactivate)
        {
            if (ledcRead(0) != servo_default)
                ledcWrite(0, servo_default);
        }
        status.state   = cmd.state;
        last_cmd_rings = cmd.rings_detected;

        if (cmd.ask_for_ack)
        {
            status.ask_for_ack = false;
            auto ser           = Serializer(SerializerMode::Serialize,
                                  {packet_buffer, udp_packet_size});

            status.getHeader(this_client_id).serialize(ser);
            status.serialize(ser);

            udp.beginPacket(server_connection.address, server_connection.port);
            BufferPtr buffer = {ser.full_buffer.start, ser.buffer.start};
            udp.write(buffer.start, buffer.end - buffer.start);
            udp.endPacket();
        }
    }
    break;
    case MessageType::Reset:
    {
        delay(1000);
        ESP.restart();
    }
    break;
    }

    if (wifi_state != WifiState::Connected)
    {
        status.state = RingDispenserState::DetectRings;
    }

    u8 i = 0;
    for (auto col : PIN_COLUMNS)
    {
        digitalWrite(col, 1);
        for (auto row : PIN_ROWS)
        {
            if (digitalRead(row))
            {
                status.rings_detected |= (1ul << i);
            }
            else
            {
                status.rings_detected &= ~(1ul << i);
            }
            i++;
        }
        digitalWrite(col, 0);
    }
    if (status.state == RingDispenserState::DetectRings)
    {
        constexpr u32 all_19_rings = (1 << 19) - 1;
        if (status.rings_detected == all_19_rings)
        {
            if (ledcRead(0) != servo_activated)
                ledcWrite(0, servo_activated);
        }
        else
        {
            if (ledcRead(0) != servo_default)
                ledcWrite(0, servo_default);
        }
    }

    if (last_cmd_rings != status.rings_detected
        && millis() > time_last_state_sent + resend_period
        && wifi_state == WifiState::Connected)
    {
        status.ask_for_ack = true;

        auto ser = Serializer(SerializerMode::Serialize,
                              {packet_buffer, udp_packet_size});

        status.getHeader(this_client_id).serialize(ser);
        status.serialize(ser);

        udp.beginPacket(server_connection.address, server_connection.port);
        BufferPtr buffer = {ser.full_buffer.start, ser.buffer.start};
        udp.write(buffer.start, buffer.end - buffer.start);
        udp.endPacket();
        time_last_state_sent = millis();
    }
    if (auto str = ReadSerial())
    {
        if (wifi_state == WifiState::Connected)
        {
            auto ser = Serializer(SerializerMode::Serialize,
                                  {packet_buffer, udp_packet_size});

            LogMessage log = {};
            log.severity   = LogSeverity::Info;
            log.string     = {(u8*)str, strlen(str)};
            log.getHeader(this_client_id).serialize(ser);
            log.serialize(ser);

            udp.beginPacket(server_connection.address, server_connection.port);
            BufferPtr buffer = {ser.full_buffer.start, ser.buffer.start};
            udp.write(buffer.start, buffer.end - buffer.start);
            udp.endPacket();
        }
    }
}