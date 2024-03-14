#include <Arduino.h>

#include "alias.hpp"
#include "msg.hpp"
#include "message_ring_dispenser.hpp"

constexpr u8 SERVO_COMMAND = 7;

ClientId this_client_id = ClientId::RingDispenser;

RingDispenserStatus status;

constexpr u16 pwm_bits        = 12;
constexpr u16 servo_default   = ((u16)1 << pwm_bits) / 20 * 2.5;
constexpr u16 servo_activated = ((u16)1 << pwm_bits) / 20 * 1;

u32 activation_start = 0;

void
setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.print(F("Init\n"));

    ledcSetup(0, 50 /*Hz*/, pwm_bits);
    ledcAttachPin(SERVO_COMMAND, 0);
    ledcWrite(0, servo_default);

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

        if (cmd.activate)
        {
            if (!status.activated)
            {
                activation_start = millis();
                ledcWrite(0, servo_activated);
            }
        }
        else
        {
            if (status.activated)
            {
                ledcWrite(0, servo_default);
            }
        }
        status.activated = cmd.activate;

        auto ser = Serializer(SerializerMode::Serialize,
                              {packet_buffer, udp_packet_size});

        status.getHeader(this_client_id).serialize(ser);
        status.serialize(ser);

        udp.beginPacket(server_connection.address, server_connection.port);
        BufferPtr buffer = {ser.full_buffer.start, ser.buffer.start};
        udp.write(buffer.start, buffer.end - buffer.start);
        udp.endPacket();
    }
    break;
    case MessageType::Reset: { ESP.restart();
    }
    break;
    }

    if (status.activated && millis() > activation_start + activation_duration)
    {
        ledcWrite(0, servo_default);
        status.activated = false;
    }
}