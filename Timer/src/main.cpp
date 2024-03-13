#include <Arduino.h>

#include "alias.hpp"
#include "msg.hpp"
#include "message_timer.hpp"

#include <TFT_eSPI.h>
#include <LittleFS.h>

constexpr u8 I2C_SDA = 18;
constexpr u8 I2C_SCL = 17;

constexpr u8 VBAT = 4;

constexpr u8 BUTTON_LEFT  = 0;
constexpr u8 BUTTON_RIGHT = 14;

constexpr u8 ROTARY_A = 1;
constexpr u8 ROTARY_B = 2;

constexpr u8 LCD_Power_On = 15;

TFT_eSPI      tft           = TFT_eSPI();
constexpr s16 screen_width  = TFT_HEIGHT;
constexpr s16 screen_height = TFT_WIDTH;

ClientId this_client_id = ClientId::Timer;

TimerStatus status;

s16 digit_width = 0;

void
setup()
{
    pinMode(LCD_Power_On, OUTPUT);
    digitalWrite(LCD_Power_On, HIGH);

    Serial.begin(SERIAL_BAUD_RATE);

    // while (digitalRead(0) == HIGH)
    // {
    //     delay(50);
    // }
    Serial.print(F("Init\n"));

    tft.init();
    tft.setRotation(1);
    tft.setTextWrap(false);
    tft.fillScreen(TFT_BLACK);

    if (TFT_BL > 0)
    {
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }

    if (!LittleFS.begin())
    {
        Serial.println(F("LittleFS.begin() failed!"));
    }

    Serial.println("Start wifi");
    StartWifi();

    tft.loadFont("FanjofeyAH-120", LittleFS);
    tft.setTextDatum(ML_DATUM);
    digit_width = tft.drawString("5", 0, 0);
    tft.fillScreen(TFT_BLACK);
}

void
loop()
{
    Message message = ReceiveMessage();
    switch (message.header.type)
    {
    case MessageType::TimerCommand:
    {
        TimerCommand cmd;
        cmd.serialize(message.deserializer);

        Serial.println(F("TimerCommand"));

        status.time_left = cmd.time_left;

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

    static bool was_connected = true;
    if (wifi_state == WifiState::Connected)
    {
        if (!was_connected)
        {
            tft.loadFont("FanjofeyAH-120", LittleFS);
        }

        static s32 curr_time = 0;
        if (status.time_left != curr_time || !was_connected)
        {
            curr_time = status.time_left;
            tft.fillScreen(TFT_BLACK);
            s16 x = 0;

            char txt[2] = "0";

            s32 time_displayed = curr_time;

            if (time_displayed > 0)
            {
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
            }
            else
            {
                tft.setTextColor(TFT_RED, TFT_BLACK);
                time_displayed = -time_displayed;
            }

            u32 minutes = time_displayed / 60;
            tft.setTextDatum(ML_DATUM);
            txt[0] = '0' + minutes / 10;
            tft.drawString(txt, 0, screen_height / 2);
            txt[0] = '0' + minutes % 10;
            tft.drawString(txt, digit_width, screen_height / 2);

            tft.setTextDatum(MC_DATUM);
            tft.drawString(":", screen_width / 2, screen_height / 2);

            u32 seconds = time_displayed % 60;
            tft.setTextDatum(MR_DATUM);
            txt[0] = '0' + seconds / 10;
            tft.drawString(txt, screen_width - 1 - digit_width,
                           screen_height / 2);
            txt[0] = '0' + seconds % 10;
            tft.drawString(txt, screen_width - 1, screen_height / 2);

            // {
            //     auto ser = Serializer(SerializerMode::Serialize,
            //                           {packet_buffer, udp_packet_size});

            //     char str_buffer[10];
            //     auto len = sprintf(str_buffer, "%02d:%02d", minutes,
            //     seconds); LogMessage msg   = {}; msg.string.start =
            //     (u8*)str_buffer; msg.string.end   = (u8*)(str_buffer + len);
            //     msg.getHeader(this_client_id).serialize(ser);
            //     msg.serialize(ser);

            //     udp.beginPacket(server_connection.address,
            //                     server_connection.port);
            //     BufferPtr buffer = {ser.full_buffer.start, ser.buffer.start};
            //     udp.write(buffer.start, buffer.end - buffer.start);
            //     udp.endPacket();
            // }
        }
        was_connected = true;
    }
    else
    {
        constexpr u32 update_period    = 500;
        static u32    time_next_update = 0;

        if (was_connected)
        {
            was_connected = false;

            tft.loadFont("BlackChancery-36", LittleFS);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextDatum(TC_DATUM);
            time_next_update = millis();
        }

        if (millis() >= time_next_update)
        {
            time_next_update += update_period;

            static s16 y = 0;
            y += 10;
            if (y + tft.fontHeight() * 2 > screen_height)
            {
                y = 0;
            }

            tft.fillScreen(TFT_BLACK);
            if (wifi_state == WifiState::WaitingForWifi)
            {
                tft.drawString("Recherche du réseau", screen_width / 2, y);
                tft.drawString("Wifi", screen_width / 2, y + tft.fontHeight());
            }
            else
            {
                tft.drawString("Attente de connexion", screen_width / 2, y);
                tft.drawString("du pc", screen_width / 2, y + tft.fontHeight());
            }
        }
    }
}