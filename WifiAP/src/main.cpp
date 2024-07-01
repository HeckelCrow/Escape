#include <Arduino.h>
#include "alias.hpp"
#include "msg.hpp"

#if LILYGO_MID
constexpr u8 BACKLIGHT = 38;
#endif

void
setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.print(F("Hello\n"));

    StartWifi(true);
}

void
loop()
{}