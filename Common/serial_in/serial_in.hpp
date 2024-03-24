#pragma once
#include <Arduino.h>
#include "alias.hpp"

constexpr u32 serial_in_buffer_size = 256;
extern char   serial_in_buffer[serial_in_buffer_size];
extern u32    serial_in_buffer_next;

inline const char*
ReadSerial()
{
    const char* out = nullptr;
    while (Serial.available())
    {
        char c = (char)Serial.read();

        if (c == '\n' || (serial_in_buffer_next + 1 >= serial_in_buffer_size))
        {
            serial_in_buffer[serial_in_buffer_next] = '\0';
            serial_in_buffer_next                   = 0;

            out = serial_in_buffer;
            Serial.println(serial_in_buffer);
            break;
        }
        else
        {
            serial_in_buffer[serial_in_buffer_next++] = c;
        }
    }
    return out;
}