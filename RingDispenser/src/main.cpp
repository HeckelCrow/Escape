#include <Arduino.h>

#include <FastLED.h>

#include "alias.hpp"
#include "serial_in.hpp"
#include "msg.hpp"
#include "message_ring_dispenser.hpp"

#if WROOM
constexpr u8 SERVO_COMMAND = 15;
constexpr u8 LED_OUT       = 12;
constexpr u8 VIBR          = 18;
constexpr u8 VIBR_EN       = 5;

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
constexpr u8  servo_channel   = 0;
constexpr u16 servo_default   = ((u16)1 << pwm_bits) / 20 * 2.5;
constexpr u16 servo_activated = ((u16)1 << pwm_bits) / 20 * 1;

constexpr u8 vibr_channel = 1;
bool         vibr_on      = false;

constexpr u32 led_count = 7;
CRGB          leds[led_count];

bool all_rings_detected = false;

struct PulsePattern
{
    u8  pulse_count = 0;
    u32 timeout     = 0;
};

PulsePattern pulse_pattern;

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
    // Turn the LEDs off.
    FastLED.addLeds<NEOPIXEL, LED_OUT>(leds, led_count);
    for (auto& led : leds)
    {
        led = CRGB(0);
    }
    FastLED.show();

    Serial.begin(SERIAL_BAUD_RATE);
    Serial.print(F("Hello\n"));

    PrintSerialCommands();

    ledcSetup(servo_channel, 50 /*Hz*/, pwm_bits);
    ledcAttachPin(SERVO_COMMAND, servo_channel);
    ledcWrite(servo_channel, servo_default);

    pinMode(VIBR, OUTPUT);
    digitalWrite(VIBR, 0);
    pinMode(VIBR_EN, INPUT_PULLUP);

    // ledcSetup(vibr_channel, 1000 /*Hz*/, pwm_bits);
    // ledcAttachPin(VIBR, vibr_channel);
    // ledcWrite(vibr_channel, 0);

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
UpdateLeds(bool on)
{
    u32           time              = millis();
    constexpr u32 led_update_period = 1000 / 60;
    static u32    next_led_update   = time;
    static bool   leds_on           = false;

    if (time > next_led_update)
    {
        next_led_update += led_update_period;

        if (on)
        {
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
        else if (leds_on)
        {
            for (auto& led : leds)
            {
                led = CRGB(0);
            }
            FastLED.show();
        }
        leds_on = on;
    }
}

void
SetAllRingsDetected(bool in)
{
    if (all_rings_detected == in)
        return;

    all_rings_detected = in;
    if (all_rings_detected)
    {
        ledcWrite(servo_channel, servo_activated);
    }
    else
    {
        ledcWrite(servo_channel, servo_default);
    }
}

PulsePattern
MakePulsePattern(u8 ring_detected_count)
{
    PulsePattern pattern = {};
    if (all_rings_detected)
    {
        pattern.pulse_count = 0;
        pattern.timeout     = 200;
    }
    else if (ring_detected_count < 10) // [0, 9]
    {
        pattern.pulse_count = 0;
        pattern.timeout     = 200;
    }
    else if (ring_detected_count < 16) // [10, 15]
    {
        pattern.pulse_count = 1;
        pattern.timeout     = 10000 - (ring_detected_count - 10) * 1000;
    }
    else if (ring_detected_count < 19) // [16, 18]
    {
        pattern.pulse_count = ring_detected_count - 16 + 2;
        pattern.timeout     = 5000;
    }
    else
    {
        // This shouldn't happen
        pattern.pulse_count = 0;
        pattern.timeout     = 200;
    }
    return pattern;
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
            SetAllRingsDetected(true);
        }
        else if (cmd.state == RingDispenserState::ForceDeactivate)
        {
            SetAllRingsDetected(false);
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

    u8 i                   = 0;
    u8 ring_detected_count = 0;
    for (auto col : PIN_COLUMNS)
    {
        digitalWrite(col, 1);
        for (auto row : PIN_ROWS)
        {
            u8 bit = rings_remap[i];
            if (digitalRead(row))
            {
                status.rings_detected |= (1ul << bit);
                ring_detected_count++;
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
        constexpr u32 open_duration = 1000;
        static u32    time_close    = 0;
        u32           time          = millis();

        if (ring_detected_count == 19)
        {
            time_close = time + open_duration;
        }

        SetAllRingsDetected((time < time_close));
    }
    UpdateLeds(all_rings_detected);

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

    if (digitalRead(VIBR_EN))
    {
        static u32    time_next_vibr = millis();
        constexpr u32 pulse_duration = 200;

        if (millis() > time_next_vibr)
        {
            if (vibr_on)
            {
                vibr_on = false;
                time_next_vibr += pulse_duration;
            }
            else
            {
                if (pulse_pattern.pulse_count == 0
                    && pulse_pattern.timeout == 0)
                {
                    pulse_pattern = MakePulsePattern(ring_detected_count);
                }

                if (pulse_pattern.pulse_count > 0)
                {
                    pulse_pattern.pulse_count--;
                    vibr_on = true;
                    time_next_vibr += pulse_duration;
                }
                else
                {
                    time_next_vibr += pulse_pattern.timeout;
                    pulse_pattern.timeout = 0;
                }
            }

            if (vibr_on)
            {
                // ledcWrite(vibr_channel, ((u16)1 << pwm_bits) / 2);
                digitalWrite(VIBR, 1);
            }
            else
            {
                // ledcWrite(vibr_channel, 0);
                digitalWrite(VIBR, 0);
            }
        }
    }
    else
    {
        if (vibr_on)
        {
            vibr_on = false;
            digitalWrite(VIBR, 0);
        }
    }

    UpdateSerial();
}