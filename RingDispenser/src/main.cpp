#include <Arduino.h>
#include <FastLED.h>

#include "alias.hpp"
#include "serial_in.hpp"
#include "msg.hpp"
#include "message_ring_dispenser.hpp"

#if WROOM
constexpr u8 SERVO_COMMAND = 15;
constexpr u8 LED_OUT       = 12;

constexpr u8 column_count              = 4;
constexpr u8 row_count                 = 5;
constexpr u8 PIN_COLUMNS[column_count] = {23, 22, 21, 19};
constexpr u8 PIN_ROWS[row_count]       = {32, 33, 25, 26, 27};

// rings_remap maps the values of (column * row_count + row) to the index of
// each ring [0, 18]. These indices are used to set and reset bits inside a u32,
// so 31 is used to note an invalid index.
constexpr u8 rings_remap[column_count * row_count] = {
    18, 13, 9,  6, 2,  //
    17, 12, 8,  5, 1,  //
    16, 11, 7,  4, 0,  //
    15, 10, 14, 3, 31, //
};
#endif

ClientId this_client_id = ClientId::RingDispenser;

RingDispenserStatus status = {};

u32           last_cmd_rings       = 0;
constexpr u32 resend_period        = 100;
u32           time_last_state_sent = 0;

constexpr u16 pwm_bits        = 12;
constexpr u16 servo_default   = ((u16)1 << pwm_bits) / 20 * 2.5;
constexpr u16 servo_activated = ((u16)1 << pwm_bits) / 20 * 1;

constexpr u32 led_count = 7;
CRGB          leds[led_count];

void
setup()
{
    // Turn the LEDs off.
    FastLED.addLeds<NEOPIXEL, LED_OUT>(leds, led_count);
    for (auto& led : leds)
    {
        led = CRGB(0);
    }
    FastLED.show();

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

            status.getHeader().serialize(ser);
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
            u8 bit = rings_remap[i];
            if (digitalRead(row))
            {
                status.rings_detected |= (1ul << bit);
            }
            else
            {
                status.rings_detected &= ~(1ul << bit);
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
            constexpr u32 led_update_period = 1000 / 60;
            static u32    next_led_update   = millis();

            if (ledcRead(0) != servo_activated)
            {
                ledcWrite(0, servo_activated);
                // The rings just got detected, we need to reset next_led_update
                // to ignore the time we didn't want to update the LEDs.
                next_led_update = millis();
            }

            if (millis() > next_led_update)
            {
                next_led_update += led_update_period;

                auto    offset     = beat8(/*bpm*/ 256 * 120);
                auto    brightness = beatsin88(/*bpm*/ 256 * 33, 64, 255);
                uint8_t i          = 0;
                for (auto& led : leds)
                {
                    auto x = sin8(offset + i * 255 / (led_count + 1));
                    led    = CRGB(CHSV(map8(x, 20, 40), 255, brightness));
                    i++;
                }
                FastLED.show();
            }
        }
        else
        {
            if (ledcRead(0) != servo_default)
            {
                ledcWrite(0, servo_default);

                for (auto& led : leds)
                {
                    led = CRGB(0);
                }
                FastLED.show();
            }
        }
    }

    if (last_cmd_rings != status.rings_detected
        && millis() > time_last_state_sent + resend_period
        && wifi_state == WifiState::Connected)
    {
        status.ask_for_ack = true;

        auto ser = Serializer(SerializerMode::Serialize,
                              {packet_buffer, udp_packet_size});

        status.getHeader().serialize(ser);
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
            log.getHeader().serialize(ser);
            log.serialize(ser);

            udp.beginPacket(server_connection.address, server_connection.port);
            BufferPtr buffer = {ser.full_buffer.start, ser.buffer.start};
            udp.write(buffer.start, buffer.end - buffer.start);
            udp.endPacket();
        }
    }
}