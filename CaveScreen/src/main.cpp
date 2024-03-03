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

// File pngfile;
// PNG  png;

// void*
// pngOpen(const char* filename, int32_t* size)
// {
//     Serial.printf("Attempting to open %s\n", filename);
//     pngfile = SD.open(filename, "r");
//     *size   = pngfile.size();
//     return &pngfile;
// }

// void
// pngClose(void* handle)
// {
//     File pngfile = *((File*)handle);
//     if (pngfile)
//         pngfile.close();
// }

// int32_t
// pngRead(PNGFILE* page, uint8_t* buffer, int32_t length)
// {
//     if (!pngfile)
//         return 0;
//     page = page; // Avoid warning
//     return pngfile.read(buffer, length);
// }

// int32_t
// pngSeek(PNGFILE* page, int32_t position)
// {
//     if (!pngfile)
//         return 0;
//     page = page; // Avoid warning
//     return pngfile.seek(position);
// }

// void
// pngDraw(PNGDRAW* pDraw)
// {
//     uint16_t lineBuffer[screen_width];
//     png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN,
//     0xffffffff); tft.pushImage(0, 0 + pDraw->y, pDraw->iWidth, 1,
//     lineBuffer);
// }

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
    File jpgFile = SD.open("/grotte3.jpg", FILE_READ);

    JpegDec.decodeSdFile(jpgFile);
    delay(1000);

    Serial.println(F("==============="));
    Serial.println(F("JPEG image info"));
    Serial.println(F("==============="));
    Serial.print(F("Width      :"));
    Serial.println(JpegDec.width);
    Serial.print(F("Height     :"));
    Serial.println(JpegDec.height);
    Serial.print(F("Components :"));
    Serial.println(JpegDec.comps);
    Serial.print(F("MCU / row  :"));
    Serial.println(JpegDec.MCUSPerRow);
    Serial.print(F("MCU / col  :"));
    Serial.println(JpegDec.MCUSPerCol);
    Serial.print(F("Scan type  :"));
    Serial.println(JpegDec.scanType);
    Serial.print(F("MCU width  :"));
    Serial.println(JpegDec.MCUWidth);
    Serial.print(F("MCU height :"));
    Serial.println(JpegDec.MCUHeight);
    Serial.println(F("==============="));

    renderJPEG(0, 0);
    delay(4000);
    tft.fillScreen(random(0xFFFF));
    tft.drawString("Hello", screen_width / 2, 0);

    // s16 rc =
    //     png.open("/grotte.png", pngOpen, pngClose, pngRead, pngSeek,
    //     pngDraw);

    // if (rc == PNG_SUCCESS)
    // {
    //     tft.startWrite();
    //     Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n",
    //                   png.getWidth(), png.getHeight(), png.getBpp(),
    //                   png.getPixelType());
    //     uint32_t dt = millis();
    //     if (png.getWidth() > screen_width)
    //     {
    //         Serial.println("Image too wide for allocated line buffer
    //         size!");
    //     }
    //     else
    //     {
    //         rc = png.decode(NULL, PNG_FAST_PALETTE);
    //         png.close();
    //     }
    //     tft.endWrite();
    //     // How long did rendering take...
    //     Serial.print(millis() - dt);
    //     Serial.println("ms");
    // }
}