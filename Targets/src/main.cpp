#include <Arduino.h>
#include "alias.hpp"
#include "msg.hpp"
#include "message_targets.hpp"

#include <ADS1X15.h>

ClientId this_client_id = ClientId::DoorLock;

constexpr u8 I2C_SDA = 2;
constexpr u8 I2C_SCL = 1;

TwoWire      i2c    = TwoWire(0);
ADS1115      adcs[] = {ADS1115(0x48 | 0b00, &i2c), ADS1115(0x48 | 0b01, &i2c)};
constexpr u8 adcs_count = sizeof(adcs) / sizeof(adcs[0]);

TargetsStatus status;

void
setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    for (auto& adc : adcs)
    {
        adc.begin();
        if (!adc.isConnected())
        {
            Serial.print(F("adc is not connected\n"));
            continue;
        }
        adc.setGain(8);
        adc.setMode(1); // Single shot mode

        // adc.requestADC_Differential_0_1();
    }

    Serial.println("Start wifi");
    StartWifi(true);
}

void
SetCommand(TargetsCommand cmd)
{}

void
loop()
{
    Message message = ReceiveMessage();
    switch (message.header.type)
    {
    case MessageType::TargetsCommand:
    {
        TargetsCommand cmd;
        cmd.serialize(message.deserializer);

        Serial.println(F("TargetsCommand"));

        SetCommand(cmd);

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
}