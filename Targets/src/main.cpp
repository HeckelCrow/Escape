#include <Arduino.h>
#include <LittleFS.h>
#include <ADS1X15.h>
#include <FastLED.h>

#include "alias.hpp"
#include "msg.hpp"
#include "message_targets.hpp"
#include "serial_in.hpp"

ClientId this_client_id = ClientId::Targets;

struct Button
{
    enum class State
    {
        Pressed,
        Clicked,
        NotPressed,
        Released,
    };

    Button();
    Button(u8 pin_) : pin(pin_), debounce(millis())
    {
        pinMode(pin, INPUT_PULLUP);
        prev_value = digitalRead(pin);
    }

    State
    getState()
    {
        State state =
            (prev_value == pressed_state) ? State::Pressed : State::NotPressed;

        u8  value          = digitalRead(pin);
        u32 state_duration = millis() - debounce;
        if (value != prev_value && state_duration > 200)
        {
            prev_value = value;
            debounce   = millis();

            state = (value == pressed_state) ? State::Clicked : State::Released;
        }

        return state;
    }

    u8  pin;
    u8  pressed_state = LOW;
    u8  prev_value    = 0;
    u32 debounce      = 0;
};

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

constexpr u8 SERVO_COMMAND[target_count] = {6, 7, 15, 17};
Button       buttons[target_count]       = {{9}, {10}, {11}, {12}};

constexpr u8 SWITCH        = 21;
constexpr u8 DOOR_LOCK_OUT = 13;
#endif

TwoWire i2c = TwoWire(0);

constexpr u16 servo_pwm_bits   = 12;
constexpr u16 servo_closed_pwm = ((u16)1 << servo_pwm_bits) / 20.0 * 2;
constexpr u16 servo_open_pwm   = ((u16)1 << servo_pwm_bits) / 20.0 * 1;

u32           last_servo_command_time[target_count] = {};
constexpr u32 servo_command_duration                = 1000;
f32           servos_closed[target_count]           = {};
bool          servos_closing[target_count]          = {}; // Closing or opening

struct Sensor
{
    static constexpr u16 sample_count = 8;

    s16  samples[sample_count] = {};
    u16  next_sample           = 0;
    u8   target_index          = 0;
    bool over_threshold        = false;
    u32  hit_time              = 0;
    u16  threshold             = 2000;
};

struct Adc
{
    Adc(u8 addr) : ads(addr, &i2c) {}

    ADS1115 ads;
    Sensor  channels[2];
    u8      curr_request = 0;
};

Adc adcs[] = {
    Adc(0x48 | 0b00), //
    Adc(0x48 | 0b01), //
};
constexpr u8 adc_count = sizeof(adcs) / sizeof(adcs[0]);

TargetsStatus status;
bool          need_resend_status   = false;
constexpr u32 resend_period        = 100;
u32           time_last_state_sent = 0;

TargetsGraph graph;

CRGB led_color;

void
CloseServo(u8 index, f32 closed)
{
    if (closed == servos_closed[index])
        return;

    if (!last_servo_command_time[index])
    {
        ledcAttachPin(SERVO_COMMAND[index], index);
    }
    u32 duty_cycle =
        servo_open_pwm + closed * ((f32)servo_closed_pwm - servo_open_pwm);
    ledcWrite(index, duty_cycle);
    servos_closed[index]           = closed;
    last_servo_command_time[index] = millis();

    Serial.printf("Close[%hhu] %f\n", index, closed);
}

void
PrintSerialCommands()
{
    Serial.println(F(""));
    Serial.println(F("Serial commands:"));
    Serial.println(F("reset"));
    Serial.println(F("scan"));
    Serial.println(F("thresholds 2000 2000 2000 2000"));
    Serial.println(F(""));
}

void
ReadSpaces(const char** str_ptr)
{
    auto* str = *str_ptr;
    while (*str)
    {
        if (*str != ' ')
        {
            break;
        }
        str++;
    }
    *str_ptr = str;
}

const char*
ReadWord(const char** str_ptr)
{
    auto* str = *str_ptr;
    auto* ret = str;
    while (*str)
    {
        if (*str == ' ')
        {
            break;
        }
        str++;
    }
    *str_ptr = str;
    return ret;
}

u16
ReadU16(const char** str_ptr)
{
    u16   value = 0;
    auto* str   = *str_ptr;
    while (*str)
    {
        if (*str == ' ')
        {
            break;
        }
        if (*str >= '0' && *str <= '9')
        {
            value *= 10;
            value += *str - '0';
        }
        else
        {
            Serial.println(F("Error in ReadU16, not a number"));
            break;
        }

        str++;
    }
    *str_ptr = str;
    return value;
}

bool
StringMatch(const char* str1, const char* end1, const char* str2)
{
    while (str1 < end1 && *str2)
    {
        if (*str1 != *str2)
        {
            return false;
        }
        str1++;
        str2++;
    }

    if (str1 == end1 && *str2 == 0)
    {
        return true;
    }

    return false;
}

void
UpdateSerial()
{
    if (auto* str = ReadSerial())
    {
        auto* cmd = ReadWord(&str);

        if (StringMatch(cmd, str, "reset"))
        {
            Serial.println(F("Reset now."));
            ESP.restart();
        }
        else if (StringMatch(cmd, str, "scan"))
        {
            WifiScan();
        }
        else if (StringMatch(cmd, str, "thresholds"))
        {
            for (u8 j = 0; j < adc_count; j++)
            {
                for (u8 i = 0; i < 2; i++)
                {
                    ReadSpaces(&str);
                    auto& th = adcs[j].channels[i].threshold = ReadU16(&str);
                }
            }

            Serial.println();
            for (u8 j = 0; j < adc_count; j++)
            {
                for (u8 i = 0; i < 2; i++)
                {
                    Serial.printf("threshold %d = %d\n",
                                  adcs[j].channels[i].target_index,
                                  adcs[j].channels[i].threshold);
                }
            }
            Serial.println();

            if (LittleFS.begin())
            {
                File file = LittleFS.open("/thresholds", "w");
                if (file)
                {
                    for (u8 j = 0; j < adc_count; j++)
                    {
                        for (u8 i = 0; i < 2; i++)
                        {
                            auto& th = adcs[j].channels[i].threshold;
                            file.write((u8*)&th, sizeof(th));
                        }
                    }
                    file.close();
                }
                else
                {
                    Serial.println(F("Can't create threshold file"));
                }

                LittleFS.end();
            }
            else
            {
                Serial.println(F("LittleFS Mount Failed"));
            }
        }
        else
        {
            Serial.print(F("Unknown command: "));
            while (cmd != str)
            {
                Serial.print(*cmd);
                cmd++;
            }
            Serial.println();
            PrintSerialCommands();
        }
    }
}

void
setup()
{
    // Should be initialized to 0:
    pinMode(DOOR_LOCK_OUT, OUTPUT);
    digitalWrite(DOOR_LOCK_OUT, 0);

    Serial.begin(SERIAL_BAUD_RATE);

    FastLED.addLeds<NEOPIXEL, INBUILT_LED_OUT>(&led_color, 1);
    led_color = CRGB(0, 0, 0);
    FastLED.show();

    Serial.println(F("Hello"));

    PrintSerialCommands();

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
        auto& adc = adcs[j];

        for (u8 i = 0; i < 2; i++)
        {
            adc.channels[i].target_index = j * 2 + i;
        }

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
        adc.ads.setMode(1);     // Single shot mode
        adc.ads.setDataRate(7); // 6: 475 SPS, 7: 860

        adc.curr_request = 0;
        adc.ads.requestADC_Differential_0_1();
    }

    if (LittleFS.begin())
    {
        File file = LittleFS.open("/thresholds", "r");
        if (file)
        {
            Serial.println(F("Loading threshold file"));
            for (u8 j = 0; j < adc_count; j++)
            {
                for (u8 i = 0; i < 2; i++)
                {
                    auto& th = adcs[j].channels[i].threshold;
                    file.read((u8*)&th, sizeof(th));
                }
            }
            file.close();
        }
        else
        {
            Serial.println(F("Can't load threshold file"));
        }

        LittleFS.end();
    }
    else
    {
        Serial.println(F("LittleFS Mount Failed"));
    }

    Serial.println();
    for (u8 j = 0; j < adc_count; j++)
    {
        for (u8 i = 0; i < 2; i++)
        {
            Serial.printf("threshold %d = %d\n",
                          adcs[j].channels[i].target_index,
                          adcs[j].channels[i].threshold);
        }
    }
    Serial.println();

    for (u8 i = 0; i < target_count; i++)
    {
        ledcSetup(i, 50 /*Hz*/, servo_pwm_bits);
        ledcAttachPin(SERVO_COMMAND[i], i);
        ledcWrite(i, servo_closed_pwm);
        servos_closed[i]           = 1.f;
        servos_closing[i]          = true;
        last_servo_command_time[i] = millis();
    }

    pinMode(SWITCH, INPUT_PULLUP);
    bool AP_mode = (digitalRead(SWITCH) == LOW);

    Serial.println(F("Start wifi"));
    StartWifi(AP_mode);
}

void
SendTargetsGraphMessage()
{
    auto ser =
        Serializer(SerializerMode::Serialize, {packet_buffer, udp_packet_size});

    graph.getHeader().serialize(ser);
    graph.serialize(ser);

    udp.beginPacket(server_connection.address, server_connection.port);
    BufferPtr buffer = {ser.full_buffer.start, ser.buffer.start};
    udp.write(buffer.start, buffer.end - buffer.start);
    udp.endPacket();

    for (u8 i = 0; i < target_count; i++)
    {
        graph.buffer_count[i] = 0;
    }
}

void
Kill(u8 i)
{
    CloseServo(i, 0.f);
    servos_closing[i] = false;
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

            if (status.hitpoints[i] <= 0)
            {
                Serial.printf("Target %hhd dead from command\n", i);
                Kill(i);
            }
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

    for (u8 j = 0; j < adc_count; j++)
    {
        for (u8 i = 0; i < 2; i++)
        {
            const auto& th           = adcs[j].channels[i].threshold;
            const auto& index        = adcs[j].channels[i].target_index;
            status.thresholds[index] = th;
        }
    }

    status.enabled    = cmd.enable;
    status.door_state = cmd.door_state;
    if (status.send_sensor_data != cmd.send_sensor_data)
    {
        if (status.send_sensor_data && graph.buffer_count)
        {
            // We send what's in the buffer
            SendTargetsGraphMessage();
        }
        status.send_sensor_data = cmd.send_sensor_data;
    }
}

void
NewSample(Sensor& ch, s16 value)
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

    u16 peak_to_peak = (s32)max - (s32)min;

    if (status.send_sensor_data)
    {
        auto& count = graph.buffer_count[ch.target_index];
        if (count >= graph.buffer_max_count)
        {
            SendTargetsGraphMessage();
        }
        graph.buffer[ch.target_index][count] = peak_to_peak;
        count++;
    }

    // if (status.enabled & (1 << ch.target_index))
    // {
    // Serial.printf(">piezo%d:%d\n", ch.target_index, peak_to_peak);
    // Serial.printf(">piezo_value%d:%d\n", ch.target_index, value);
    // Serial.printf(">avg%d:%d\n", ch.target_index, ch.average / 256);
    // }

    if (peak_to_peak >= ch.threshold)
    {
        if (!ch.over_threshold)
        {
            ch.over_threshold = true;
            if (status.enabled & (1 << ch.target_index))
            {
                constexpr s8  hp_min       = -10;
                constexpr u32 hit_cooldown = 1000;
                auto          time         = millis();
                if (ch.hit_time == 0 || time > ch.hit_time + hit_cooldown)
                {
                    ch.hit_time = time;
                    if (status.hitpoints[ch.target_index] > hp_min)
                    {
                        status.hitpoints[ch.target_index]--;
                        need_resend_status = true;

                        if (status.hitpoints[ch.target_index] <= 0)
                        {
                            Serial.printf("Target %hhd dead from hit\n",
                                          ch.target_index);
                            Kill(ch.target_index);
                        }
                    }
                }
            }
        }
    }
    else if (peak_to_peak < ch.threshold / 2)
    {
        ch.over_threshold = false;
    }
}

void
loop()
{
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

        UpdateSerial();
    }

    for (u8 j = 0; j < adc_count; j++)
    {
        auto& adc = adcs[j];
        if (adc.ads.isReady())
        {
            s16 value = adc.ads.getValue();

            NewSample(adc.channels[adc.curr_request], value);

            adc.curr_request = (adc.curr_request == 0) ? 1 : 0;
            if (adc.curr_request == 0)
            {
                adc.ads.requestADC_Differential_0_1();
            }
            else
            {
                adc.ads.requestADC_Differential_2_3();
            }
        }
    }

    static u32 last_time   = millis();
    u32        time        = millis();
    u32        update_time = time - last_time;
    last_time              = time;
    for (u8 i = 0; i < target_count; i++)
    {
        auto button_state = buttons[i].getState();

        if (button_state == Button::State::Clicked)
        {
            servos_closing[i] = !servos_closing[i];
        }
        constexpr f32 time_to_close = 1500.f;
        if (servos_closing[i] && servos_closed[i] < 1.f)
        {
            // The latch is slowly closing
            f32 new_close = servos_closed[i] + update_time / time_to_close;
            if (new_close > 0.9f)
            {
                new_close = 1.f;
                if (status.hitpoints[i] <= 0)
                {
                    // We give the target 1hp otherwise the latch would open
                    // again.
                    status.hitpoints[i] = 1;
                    need_resend_status  = true;
                }
            }
            CloseServo(i, new_close);
        }
        else if (!servos_closing[i] && servos_closed[i] > 0.f)
        {
            // The latch is slowly opening
            f32 new_close = servos_closed[i] - update_time / time_to_close;
            if (new_close < 0.1f)
            {
                new_close = 0.f;
            }
            CloseServo(i, new_close);
        }

        if (last_servo_command_time[i])
        {
            if (millis() > last_servo_command_time[i] + servo_command_duration)
            {
                // After some time we stop the command of the servo to reduce
                // heating for nothing.
                last_servo_command_time[i] = 0;
                ledcDetachPin(SERVO_COMMAND[i]);
                Serial.printf("Stop servo %hhu\n", i);
            }
        }
    }

    switch (status.door_state)
    {
    case TargetsDoorState::OpenWhenTargetsAreDead:
    {
        u8 to_write = 0; // Open
        for (const auto& hp : status.hitpoints)
        {
            if (hp > 0)
            {
                to_write = 1; // Close
                break;
            }
        }
        digitalWrite(DOOR_LOCK_OUT, to_write);
    }
    break;
    case TargetsDoorState::Open: digitalWrite(DOOR_LOCK_OUT, 0); break;
    case TargetsDoorState::Close: digitalWrite(DOOR_LOCK_OUT, 1); break;
    }
}