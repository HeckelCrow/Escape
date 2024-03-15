#include <Arduino.h>

#include "alias.hpp"

#include <TFT_eSPI.h>
#include <SD.h>
#include <JPEGDecoder.h>

constexpr u8 I2C_SDA = 21;
constexpr u8 I2C_SCL = 22;

constexpr u8 VBAT = 35;

constexpr u8 SD_MISO = 2;
constexpr u8 SD_MOSI = 15;
constexpr u8 SD_SCLK = 14;
constexpr u8 SD_CS   = 13;

constexpr u8 SPEAKER_PWD = 19;
constexpr u8 SPEAKER_OUT = 25;

constexpr u8 BUTTON_LEFT  = 38;
constexpr u8 BUTTON_MID   = 37;
constexpr u8 BUTTON_RIGHT = 39;
constexpr u8 BUTTON_BOOT  = 0;

TFT_eSPI      tft           = TFT_eSPI();
constexpr s16 screen_width  = TFT_HEIGHT;
constexpr s16 screen_height = TFT_WIDTH;

SPIClass sdSPI(VSPI);

String* images_paths;
u32     image_count     = 0;
s32     curr_image      = 0;
s32     image_displayed = -1;

struct Button
{
    enum class State
    {
        Pressed,
        Clicked,
        NotPressed,
        Released,
        LongPress,
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

            if (!long_pressed)
            {
                state =
                    (value == pressed_state) ? State::Clicked : State::Released;
            }
            long_pressed = false;
        }
        else if (state_duration > 1000 && !long_pressed)
        {
            if (prev_value == pressed_state)
            {
                state        = State::LongPress;
                long_pressed = true;
            }
        }
        return state;
    }

    u8   pin;
    u8   pressed_state = LOW;
    u8   prev_value    = 0;
    bool long_pressed  = false;
    u32  debounce      = 0;
};

bool
IsPressed(Button::State state)
{
    return (state == Button::State::Pressed)
           || (state == Button::State::Clicked)
           || (state == Button::State::LongPress);
}

Button button_top(BUTTON_RIGHT);
Button button_bottom(BUTTON_LEFT);

void
setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.print(F("Init\n"));

    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, HIGH);

    tft.init();
    tft.setRotation(1);
    tft.setTextWrap(false);
    tft.fillScreen(TFT_BLACK);

    if (TFT_BL > 0)
    {
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }

    sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, sdSPI))
    {
        Serial.println("SDCard MOUNT FAIL");
    }

    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    // tft.drawString("Init", screen_width / 2, screen_height / 2);

    tft.loadFont("BlackChancery-36", SD);

    u32 cardSize = SD.cardSize() / (1024 * 1024);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("SDCard Size: ", screen_width / 2, screen_height / 2);
    tft.drawString(String(cardSize) + "MB", screen_width / 2,
                   screen_height / 2 + tft.fontHeight());

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Hello", screen_width / 2, 0);

    auto dir = SD.open("/images");
    while (true)
    {
        auto file = dir.openNextFile();
        if (!file)
        {
            break;
        }
        image_count++;
    }
    images_paths = new String[image_count];

    dir.rewindDirectory();
    for (u32 i = 0; i < image_count; i++)
    {
        auto file = dir.openNextFile();
        if (!file)
        {
            break;
        }
        images_paths[i] = "/";
        images_paths[i] += file.path();
    }
}

void
renderJPEG(int xpos, int ypos)
{
    // retrieve infomration about the image
    uint16_t* pImg;
    uint16_t  mcu_w = JpegDec.MCUWidth;
    uint16_t  mcu_h = JpegDec.MCUHeight;
    uint32_t  max_x = JpegDec.width;
    uint32_t  max_y = JpegDec.height;

    // Jpeg images are draw as a set of image block (tiles) called Minimum
    // Coding Units (MCUs) Typically these MCUs are 16x16 pixel blocks Determine
    // the width and height of the right and bottom edge image blocks
#define minimum(a, b) (((a) < (b)) ? (a) : (b))
    uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
    uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    // record the current time so we can measure how long it takes to draw an
    // image
    uint32_t drawTime = millis();

    // save the coordinate of the right and bottom edges to assist image
    // cropping to the screen size
    max_x += xpos;
    max_y += ypos;

    // read each MCU block until there are no more
    while (JpegDec.read())
    {
        // save a pointer to the image block
        pImg = JpegDec.pImage;

        // calculate where the image block should be drawn on the screen
        int mcu_x = JpegDec.MCUx * mcu_w + xpos;
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;

        // check if the image block size needs to be changed for the right and
        // bottom edges
        if (mcu_x + mcu_w <= max_x)
            win_w = mcu_w;
        else
            win_w = min_w;
        if (mcu_y + mcu_h <= max_y)
            win_h = mcu_h;
        else
            win_h = min_h;

        // calculate how many pixels must be drawn
        uint32_t mcu_pixels = win_w * win_h;

        // draw image block if it will fit on the screen
        if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
        {
            // open a window onto the screen to paint the pixels into
            // tft.setAddrWindow(mcu_x, mcu_y, mcu_x + win_w - 1, mcu_y +
            // win_h - 1);
            tft.setAddrWindow(mcu_x, mcu_y, win_w, win_h);
            // push all the image block pixels to the screen
            while (mcu_pixels--)
                tft.pushColor(*pImg++); // Send to TFT 16 bits at a time
        }

        // stop drawing blocks if the bottom of the screen has been reached
        // the abort function will close the file
        else if ((mcu_y + win_h) >= tft.height())
            JpegDec.abort();
    }

    // calculate how long it took to draw the image
    drawTime = millis() - drawTime; // Calculate the time it took

    // print the results to the serial port
    Serial.print("Total render time was    : ");
    Serial.print(drawTime);
    Serial.println(" ms");
    Serial.println("=====================================");
}

void
loop()
{
    if (button_top.getState() == Button::State::Clicked)
    {
        curr_image++;
        if (curr_image >= image_count)
            curr_image = 0;
    }
    if (button_bottom.getState() == Button::State::Clicked)
    {
        curr_image--;
        if (curr_image < 0)
            curr_image = image_count - 1;
    }

    if (image_displayed != curr_image)
    {
        image_displayed = curr_image;
        File jpgFile    = SD.open(images_paths[curr_image].c_str(), FILE_READ);

        JpegDec.decodeSdFile(jpgFile);

        // Serial.println(F("==============="));
        // Serial.println(F("JPEG image info"));
        // Serial.println(F("==============="));
        // Serial.print(F("Width      :"));
        // Serial.println(JpegDec.width);
        // Serial.print(F("Height     :"));
        // Serial.println(JpegDec.height);
        // Serial.print(F("Components :"));
        // Serial.println(JpegDec.comps);
        // Serial.print(F("MCU / row  :"));
        // Serial.println(JpegDec.MCUSPerRow);
        // Serial.print(F("MCU / col  :"));
        // Serial.println(JpegDec.MCUSPerCol);
        // Serial.print(F("Scan type  :"));
        // Serial.println(JpegDec.scanType);
        // Serial.print(F("MCU width  :"));
        // Serial.println(JpegDec.MCUWidth);
        // Serial.print(F("MCU height :"));
        // Serial.println(JpegDec.MCUHeight);
        // Serial.println(F("==============="));

        renderJPEG(0, 0);
    }
}