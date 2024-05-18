#include <Arduino.h>
#include "alias.hpp"
#include "msg.hpp"
#include "message_targets.hpp"

#include <ADS1X15.h>
// #include <FastLED.h>

ClientId this_client_id = ClientId::Targets;

#if MINI_C3
constexpr u8 I2C_SDA = 2;
constexpr u8 I2C_SCL = 1;
#elif WROOM
constexpr u8 I2C_SDA = 32;
constexpr u8 I2C_SCL = 33;
#elif ESP32S3
constexpr u8 I2C_SDA = 4;
constexpr u8 I2C_SCL = 5;

constexpr u8 INBUILT_LED_OUT = 48;
// constexpr u8 LEDS_OUT        = 6;
#endif

TwoWire i2c = TwoWire(0);

struct I2cMultiplexer
{
    I2cMultiplexer(u8 A0, u8 A1, u8 A2)
    {
        // 7 bit address = 1 1 1 0 A2 A1 A0
        address = 0b1110000;
        address |= (A0 << 0);
        address |= (A1 << 1);
        address |= (A2 << 2);
    }

    bool
    setChannel(u8 channel)
    {
        if (curr_channel == channel)
            return true;

        bool success = false;
        i2c.beginTransmission(address);
        i2c.write(1 << channel);
        auto err = i2c.endTransmission();
        if (err == 0)
        {
            success      = true;
            curr_channel = channel;
        }
        else
        {
            Serial.printf("Error: Could not find multiplexer, error %hhu\n",
                          err);
        }
        return success;
    }

    u8 address;
    s8 curr_channel = -1;
};

I2cMultiplexer i2c_multiplexer(0, 0, 0);

struct Adc
{
    Adc(u8 i2c_channel_in, u8 addr) :
        ads(addr, &i2c), i2c_channel(i2c_channel_in)
    {}

    u8      i2c_channel = 0;
    ADS1115 ads;

    static constexpr u16 sample_count = 8;

    s16  samples[sample_count] = {};
    u16  next_sample           = 0;
    u8   target_index          = 0;
    bool over_threshold        = false;

    // u32 average = 0; // Average peak to peak x256
};

Adc adcs[] = {
    Adc(0, 0x48 | 0b00), //
    Adc(0, 0x48 | 0b01), //
    Adc(1, 0x48 | 0b00), //
    Adc(1, 0x48 | 0b01)  //
};
constexpr u8 adc_count = sizeof(adcs) / sizeof(adcs[0]);

TargetsStatus status;
bool          need_resend_status   = false;
constexpr u32 resend_period        = 100;
u32           time_last_state_sent = 0;

// CRGB led_color;

void
setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    // FastLED.addLeds<NEOPIXEL, INBUILT_LED_OUT>(&led_color, 1);
    // led_color = CRGB(255, 0, 0);
    // FastLED.show();

    // FastLED.addLeds<NEOPIXEL, LEDS_OUT>(leds, led_count);

    delay(1000);
    Serial.println(F("Hello"));

    if (!i2c.setPins(I2C_SDA, I2C_SCL))
    {
        Serial.print(F("i2c.setPins error\n"));
    }
    if (!i2c.begin())
    {
        Serial.print(F("i2c.begin error\n"));
    }

    for (u8 j = 0; j < adc_count; j++)
    {
        auto& adc        = adcs[j];
        adc.target_index = j;

        i2c_multiplexer.setChannel(adc.i2c_channel);
        adc.ads.begin();
        if (adc.ads.isConnected())
        {
            Serial.print(F("ADS1115 is connected\n"));
        }
        else
        {
            Serial.print(F("ADS1115 is not connected\n"));
            continue;
        }
        /*
         *  Gain index   Max Voltage
         *  0 	         ±6.144V
         *  1 	         ±4.096V
         *  2 	         ±2.048V
         *  4 	         ±1.024V
         *  8 	         ±0.512V
         *  16 	         ±0.256V
         */
        adc.ads.setGain(8);
        adc.ads.setMode(1); // Single shot mode

        adc.ads.requestADC_Differential_0_1();
    }

    Serial.println(F("Start wifi"));
    StartWifi(true);
}

void
SetCommand(TargetsCommand cmd)
{
    need_resend_status = false;
    for (u8 i = 0; i < target_count; i++)
    {
        if (cmd.set_hitpoints[i] >= 0)
        {
            status.hitpoints[i] = cmd.set_hitpoints[i];
        }
        else if (cmd.hitpoints[i] != status.hitpoints[i])
        {
            need_resend_status = true;
        }

        u8 bit = (1 << i);
        if (status.enabled & bit != cmd.enable & bit)
        {
            if (cmd.enable & bit)
            {
                // Enabled
            }
            else
            {
                // Disabled
            }
        }
    }
    status.enabled = cmd.enable;
    status.talk    = cmd.talk;
}

u32 hit_time     = 0;
u32 hit_duration = 3000;

void
NewSample(Adc& ch, s16 value)
{
    ch.samples[ch.next_sample] = value;
    ch.next_sample             = (ch.next_sample + 1) % ch.sample_count;

    s16 min = ch.samples[0];
    s16 max = ch.samples[0];
    for (u16 i = 1; i < ch.sample_count; i++)
    {
        if (ch.samples[i] < min)
            min = ch.samples[i];
        if (ch.samples[i] > max)
            max = ch.samples[i];
    }

    s32 peak_to_peak = (s32)max - (s32)min;

    // if (status.enabled & (1 << ch.target_index))
    // {
    Serial.printf(">piezo%d:%d\n", ch.target_index, peak_to_peak);
    Serial.printf(">piezo_value%d:%d\n", ch.target_index, value);
    // Serial.printf(">avg%d:%d\n", ch.target_index, ch.average / 256);
    // }

    // s16 threshold = 3 * ch.average / 256;
    // if (peak_to_peak > threshold)
    // {
    //     if (!ch.over_threshold)
    //     {
    //         ch.over_threshold = true;
    //         if (status.enabled & (1 << ch.target_index))
    //         {
    //             constexpr s8 hp_min = -10;
    //             if (status.hitpoints[ch.target_index] > hp_min)
    //             {
    //                 status.hitpoints[ch.target_index]--;
    //                 need_resend_status = true;

    //                 hit_time = millis();
    //             }
    //         }
    //     }
    //     ch.average -= ch.average / 256 / 4;
    //     ch.average += peak_to_peak / 4;
    // }
    // else
    // {
    //     ch.average -= ch.average / 256;
    //     ch.average += peak_to_peak;
    //     ch.over_threshold = false;
    // }
    // if (ch.average < 5 * 256)
    // {
    //     ch.average = 5 * 256;
    // }
}

void
loop()
{
    // constexpr u32 led_update_period = 1000 / 60;
    // static u32    next_led_update   = millis();
    // if (millis() > next_led_update)
    // {
    //     next_led_update += led_update_period;
    //     // led_color = CRGB(CHSV(beatsin16(4, 0, 255), 255, 255));

    //     accum88 bpm = 40 * 255;
    //     if (hit_time != 0)
    //     {
    //         if (millis() < hit_time + hit_duration)
    //         {
    //             bpm = 200 * 255;
    //         }
    //         else
    //         {
    //             hit_time = 0;
    //         }
    //     }

    //     if (hit_time == 0 && status.talk & (1 << 0))
    //     {
    //         led_color = CRGB(255, 0, 0);
    //     }
    //     else
    //     {
    //         led_color = CRGB(CHSV(beatsin88(bpm, 0, 20), 255, 255));
    //     }

    //     FastLED.show();
    // }

    if (need_resend_status && wifi_state == WifiState::Connected)
    {
        if (millis() > time_last_state_sent + resend_period)
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
    }

    constexpr u32 receive_message_period = 50;
    static u32    next_receive_message   = millis();
    if (millis() >= next_receive_message)
    {
        next_receive_message += receive_message_period;

        Message message = ReceiveMessage();
        switch (message.header.type)
        {
        case MessageType::TargetsCommand:
        {
            TargetsCommand cmd;
            cmd.serialize(message.deserializer);

            Serial.println(F("TargetsCommand"));

            SetCommand(cmd);

            if (cmd.ask_for_ack)
            {
                status.ask_for_ack = false;

                auto ser = Serializer(SerializerMode::Serialize,
                                      {packet_buffer, udp_packet_size});

                status.getHeader().serialize(ser);
                status.serialize(ser);

                udp.beginPacket(server_connection.address,
                                server_connection.port);
                BufferPtr buffer = {ser.full_buffer.start, ser.buffer.start};
                udp.write(buffer.start, buffer.end - buffer.start);
                udp.endPacket();
                time_last_state_sent = millis();
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

        while (Serial.available())
        {
            char c = (char)Serial.read();
            Serial.print(c);
        }
    }
    for (u8 j = 0; j < adc_count; j++)
    {
        auto& adc = adcs[j];
        i2c_multiplexer.setChannel(adc.i2c_channel);
        if (adc.ads.isReady())
        {
            s16 value = adc.ads.getValue();
            NewSample(adc, value);
            adc.ads.requestADC_Differential_0_1();
        }
    }
}