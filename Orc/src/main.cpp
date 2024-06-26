#include "alias.hpp"

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#define FS_NO_GLOBALS
#include <LittleFS.h>
#include <vector>

#include <FastLED.h>
#include <SdFat.h>

#include "driver/i2s.h"
#include "esp_random.h"

// #define ENABLE_MAGNETIC_SENSOR 0

#if LOLIN_D32

constexpr u8 I2S_BCK  = 13;
constexpr u8 I2S_DOUT = 12; // DIN pin
constexpr u8 I2S_LCK  = 14;

constexpr u8 SD_MISO = 32;
constexpr u8 SD_MOSI = 25;
constexpr u8 SD_CLK  = 33;
constexpr u8 SD_CS   = 26;

constexpr u8 I2C_SDA = 21;
constexpr u8 I2C_SCL = 22;

constexpr u8 BUILTIN_LED_PIN = 5;
constexpr u8 LED_OUT         = 19;

constexpr u8 SNOOZE_BUTTON = 18;
#endif

SdFat32  SD;
SPIClass vSPI;
bool     SD_card_initialized = false;

constexpr u32 led_count = 2;
CRGB          leds[led_count];

constexpr u16 MPU6050_address = 0x68;

#ifdef ENABLE_MAGNETIC_SENSOR
constexpr u16 LSM303DLHC_mag_address = 0x1E;
#endif

/*
   FS_SEL
   0    ±  250 °/s
   1    ±  500 °/s
   2    ± 1000 °/s
   3    ± 2000 °/s
*/
constexpr u8  FS_SEL          = 1;
constexpr s16 FS_SEL_values[] = {250, 500, 1000, 2000};

/*
    AFS_SEL
    0    ± 2g
    1    ± 4g
    2    ± 8g
    3    ± 16g
 */
constexpr u8  AFS_SEL          = 2;
constexpr s16 AFS_SEL_values[] = {2, 4, 8, 16};

void
PrintDirectory(File32 dir, u8 depth = 0)
{
   while (true)
   {
      File32 file = dir.openNextFile();
      if (!file)
      {
         return;
      }

      for (u8 i = 0; i < depth; i++)
         Serial.print(">");

      if (file.isDirectory())
      {
         file.printName(&Serial);
         Serial.print(F("\n"));
         PrintDirectory(file, depth + 1);
      }
      else
      {
         file.printName(&Serial);
         Serial.print(F("\n    Size: "));
         Serial.print(file.size() / 1024 / 1024.f, DEC);
         Serial.println(F(" MB"));
      }
      file.close();
   }
   dir.rewindDirectory();
}

bool
EndsWithWav(char* str, size_t size)
{
   if (size < 4)
      return false;

   constexpr char wav_str[] = ".wav";
   for (u16 i = 0; i < 4; i++)
   {
      if (str[i + size - 4] != wav_str[i])
      {
         return false;
      }
   }
   return true;
}

std::vector<String>
ListAllWaveFiles(File32 dir)
{
   std::vector<String> vec;
   while (true)
   {
      auto file = dir.openNextFile();
      if (!file)
      {
         break;
      }
      if (!file.isDirectory())
      {
         char   filename[64];
         size_t size = file.getName(filename, sizeof(filename));

         // Serial.print(F("File: "));
         // file.printName(&Serial);
         // Serial.print(F("\n"));
         if (EndsWithWav(filename, size))
         {
            vec.push_back(String(filename, size));
         }
      }
   }
   vec.shrink_to_fit();
   return std::move(vec);
}

struct WaveFile
{
   File32 handle;

   static constexpr u32 buffer_size = 256;
   u8*                  buffer      = nullptr;

   u8* read_ptr       = 0;
   u8* end            = 0;
   u32 data_left_size = U32_MAX;
};

bool
LoadFileIntoBuffer(WaveFile& file)
{
   if (!file.data_left_size)
      return false;

   u32 old_data = file.end - file.read_ptr;
   u8* dest     = file.buffer;
   if (old_data)
   {
      memcpy(dest, file.read_ptr, old_data);

      // Serial.println("old data:");
      // for (u32 i = 0; i < old_data; i++)
      // {
      //    Serial.print(dest[i], HEX);
      // }

      dest += old_data;
   }
   u32 size_to_read = file.buffer + file.buffer_size - dest;
   size_to_read     = min(size_to_read, file.data_left_size);
   u32 loaded       = file.handle.read(dest, size_to_read);
   file.data_left_size -= loaded;

   // Serial.println("\nnew data:");
   // for (u32 i = 0; i < loaded; i++)
   // {
   //    Serial.print(dest[i], HEX);
   // }

   file.read_ptr = file.buffer;
   file.end      = dest + loaded;
   if (loaded == 0)
   {
      file.data_left_size = 0; // TODO: check that this isn't happening
      Serial.println("ERROR!");
   }
   return (loaded == 0);
}

template<typename T>
bool
ReadFile(WaveFile& file, T& value)
{
   u32 available = file.end - file.read_ptr;
   if (available < sizeof(T))
   {
      LoadFileIntoBuffer(file);
   }
   available = file.end - file.read_ptr;
   if (available >= sizeof(T))
   {
      value = *(T*)file.read_ptr;
      file.read_ptr += sizeof(T);
      return true;
   }
   return false;
}

struct WaveHeader
{
   u16 channel_count   = 0;
   u32 sample_rate     = 0;
   u16 bits_per_sample = 0;
   // u32 data_size       = 0;
};

constexpr u32
StringToU32(const char* str)
{
   return ((u32)str[0]) | ((u32)str[1] << 8) | ((u32)str[2] << 16)
          | ((u32)str[3] << 24);
}

static_assert(StringToU32("RIFF") == 0x46464952, "");

bool
DecodeWaveHeader(WaveHeader& header, WaveFile& file)
{
   u32 riff = 0;
   if (!ReadFile(file, riff))
      return false;
   if (riff != StringToU32("RIFF"))
   {
      Serial.println(F("Not a RIFF file"));
      Serial.println(riff, HEX);
      return false;
   }
   u32 file_size = 0;
   if (!ReadFile(file, file_size))
      return false;
   u32 wave = 0;
   if (!ReadFile(file, wave))
      return false;
   if (wave != StringToU32("WAVE"))
   {
      Serial.println(F("Not a WAVE file"));
      return false;
   }
   u32 fmt = 0;
   if (!ReadFile(file, fmt))
      return false;
   if (fmt != StringToU32("fmt "))
   {
      Serial.println(F("Missing fmt"));
      return false;
   }
   u32 format_data_length = 0;
   if (!ReadFile(file, format_data_length))
      return false;
   u16 format_type = 0;
   if (!ReadFile(file, format_type))
      return false;
   if (!ReadFile(file, header.channel_count))
      return false;
   if (header.channel_count > 2)
      return false;

   if (!ReadFile(file, header.sample_rate))
      return false;
   u32 byterate = 0;
   if (!ReadFile(file, byterate))
      return false;
   u16 bits_per_sample_total = 0;
   if (!ReadFile(file, bits_per_sample_total))
      return false;
   if (!ReadFile(file, header.bits_per_sample))
      return false;

   while (true)
   {
      u32 data_str = 0;
      if (!ReadFile(file, data_str))
         return false;

      u32 section_size = 0;
      if (!ReadFile(file, section_size))
         return false;

      if (data_str == StringToU32("data"))
      {
         // header.data_size = section_size;
         u32 already_loaded  = file.end - file.read_ptr;
         file.data_left_size = section_size - already_loaded;
         break;
      }
      else if (data_str == StringToU32("LIST"))
      {
         u32 index   = 0;
         u32 type_id = 0;
         if (!ReadFile(file, type_id))
            return false;
         index += 4;
         if (type_id == StringToU32("INFO"))
         {
            while (index != section_size)
            {
               u32 info_id = 0;
               if (!ReadFile(file, info_id))
                  return false;
               index += 4;
               u32 text_size = 0;
               if (!ReadFile(file, text_size))
                  return false;
               index += 4;

               switch (info_id)
               {
               case StringToU32("INAM"): Serial.print(F("Title: ")); break;
               case StringToU32("IART"): Serial.print(F("Artist: ")); break;
               case StringToU32("ICOP"): Serial.print(F("Copyright: ")); break;
               case StringToU32("ICRD"):
                  Serial.print(F("Creation date: "));
                  break;
               case StringToU32("IGNR"): Serial.print(F("Genre: ")); break;
               case StringToU32("IPRD"): Serial.print(F("Album: ")); break;
               case StringToU32("ITRK"):
               case StringToU32("IPRT"): Serial.print(F("Track #: ")); break;
               default: Serial.printf("%.*s: ", 4, (char*)&info_id); break;
               }
               for (u32 i = 0; i < text_size; i++)
               {
                  u8 c = 0;
                  if (!ReadFile(file, c))
                     return false;
                  if (c)
                     Serial.print((char)c);
               }
               index += text_size;
               Serial.print('\n');

               if (index != section_size && (index & 0b1))
               {
                  // 16 bit alignement
                  u8 discard = 0;
                  if (!ReadFile(file, discard))
                     return false;
                  index++;
               }
            }
         }
         else
         {
            // Skip
            for (u32 i = 0; i < section_size; i++)
            {
               u8 discard = 0;
               if (!ReadFile(file, discard))
                  return false;
            }
         }
      }
      else
      {
         // Skip
         for (u32 i = 0; i < section_size; i++)
         {
            u8 discard = 0;
            if (!ReadFile(file, discard))
               return false;
         }
      }
   }

   return true;
}

constexpr u16
FloatTo88(f32 volume)
{
   return (u16)(volume * (1 << 8));
}

s16
ScaleBy88(s32 value, u16 scale)
{
   value = (value * scale) >> 8;
   if (value < S16_MIN)
      return S16_MIN;
   if (value > S16_MAX)
      return S16_MAX;
   return value;
}

s16
ScaleByU16(s32 value, u16 scale)
{
   value = (value * scale) >> 16;
   if (value < S16_MIN)
      return S16_MIN;
   if (value > S16_MAX)
      return S16_MAX;
   return value;
}

/*
   https://docs.fileformat.com/audio/wav/
   https://www.recordingblogs.com/wiki/list-chunk-of-a-wave-file
   https://www.robotplanet.dk/audio/wav_meta_data/
*/

struct Wave
{
   WaveFile   file;
   WaveHeader header;
   s32        last_sample = S32_MIN;
};

bool
LoadWaveFile(Wave& wave, const File32& file32)
{
   wave             = Wave();
   wave.file.handle = file32;
   wave.file.buffer = (u8*)malloc(wave.file.buffer_size);

   if (!wave.file.buffer)
   {
      Serial.println(F("malloc failed"));
      return false;
   }

   if (!DecodeWaveHeader(wave.header, wave.file))
   {
      Serial.println(F("DecodeWaveHeader failed"));
      return false;
   }

   i2s_set_sample_rates(I2S_NUM_0, wave.header.sample_rate);

   return true;
}

void
DestroyWaveFile(Wave& wave)
{
   if (wave.file.buffer)
      free(wave.file.buffer);
}

struct WavePlayer
{
   bool playing = false;
   u16  volume  = FloatTo88(0.1f);

   Wave wave;
};

WavePlayer player = {};

bool
LoadSamples(WavePlayer& p)
{
   if (!p.playing)
      return false;

   while (true)
   {
      if (p.wave.last_sample != S32_MIN)
      {
         u32 sample = p.wave.last_sample;

         size_t bytes_written;
         i2s_write(I2S_NUM_0, &sample, sizeof(s16) * 2, &bytes_written, 0);

         if (bytes_written == 0)
            break;
      }

      s16 left  = 0;
      s16 right = 0;
      if (p.wave.header.channel_count == 1)
      {
         s16 sample = 0;
         if (!ReadFile(p.wave.file, sample))
         {
            p.playing = false;
            return false;
         }

         left  = sample;
         right = sample;
      }
      else if (p.wave.header.channel_count == 2)
      {
         if (!ReadFile(p.wave.file, left))
         {
            p.playing = false;
            return false;
         }

         if (!ReadFile(p.wave.file, right))
         {
            p.playing = false;
            return false;
         }
      }

      p.wave.last_sample = (ScaleBy88(left, p.volume) & 0xFFFF)
                           | (ScaleBy88(right, p.volume) << 16);
   }
   return true;
}

void
StartWaveFile(File32 file, u16 volume)
{
   if (!file)
   {
      return;
   }
   Serial.print("StartWaveFile: ");
   file.printName(&Serial);
   Serial.println();

   DestroyWaveFile(player.wave);
   LoadWaveFile(player.wave, file);
   player.volume  = volume;
   player.playing = true;
}

struct Vec3
{
   f32 x;
   f32 y;
   f32 z;
};

Vec3
ReadAcceleration(u8 addr_bit)
{
   Vec3 a;
   u8   addr = MPU6050_address | (addr_bit & 0b1);
   Wire.beginTransmission(addr);
   Wire.write(0x3B);
   Wire.endTransmission();
   Wire.requestFrom(addr, (size_t)6);
   s16 x = (Wire.read() << 8) | Wire.read();
   s16 y = (Wire.read() << 8) | Wire.read();
   s16 z = (Wire.read() << 8) | Wire.read();

   constexpr f32 scale = (f32)FS_SEL_values[FS_SEL] / S16_MAX;

   a.x = x * scale;
   a.y = y * scale;
   a.z = z * scale;

   return a;
}

Vec3
ReadGyroscope(u8 addr_bit)
{
   Vec3 a;
   u8   addr = MPU6050_address | (addr_bit & 0b1);
   Wire.beginTransmission(addr);
   Wire.write(0x43);
   Wire.endTransmission();
   Wire.requestFrom(addr, (size_t)6);
   s16 x = (Wire.read() << 8) | Wire.read();
   s16 y = (Wire.read() << 8) | Wire.read();
   s16 z = (Wire.read() << 8) | Wire.read();

   constexpr f32 scale = (f32)AFS_SEL_values[AFS_SEL] / S16_MAX;

   a.x = x * scale;
   a.y = y * scale;
   a.z = z * scale;

   return a;
}

#ifdef ENABLE_MAGNETIC_SENSOR
bool
ReadMagneticField(Vec3& a)
{
   Wire.beginTransmission(LSM303DLHC_mag_address);
   Wire.write(0x09);
   Wire.endTransmission();
   Wire.requestFrom(LSM303DLHC_mag_address, (size_t)1);
   u8 SR_REG_M = Wire.read();
   // DRDY (Data-ready bit) is always one and I don't understand why.
   // LOCK (Data output register lock) seems to be set to 0 when new data is
   // available.
   if (SR_REG_M & 0b10)
   {
      return false;
   }

   Wire.beginTransmission(LSM303DLHC_mag_address);
   Wire.write(0x03);
   Wire.endTransmission();
   Wire.requestFrom(LSM303DLHC_mag_address, (size_t)6);
   s16 x = (Wire.read() << 8) | Wire.read();
   s16 z = (Wire.read() << 8) | Wire.read();
   s16 y = (Wire.read() << 8) | Wire.read();

   // input field range[Gauss] ±1.3
   // Output range -2048 to +2047

   // constexpr f32 scale = 1.f;
   constexpr f32 scale = 1.3f / 2047;

   a.x = x * scale;
   a.y = y * scale;
   a.z = z * scale;

   return true;
}

inline f32
NormalizeMag(f32 value, f32 min, f32 max)
{
   f32 n = 0.f;
   if (max - min > 0.4f)
   {
      n = (value - min) / (max - min) * 2.f - 1.f;
   }
   return n;
}
#endif // ENABLE_MAGNETIC_SENSOR

Vec3
Normalize(Vec3 v)
{
   f32 len_inv = 1.f / sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));

   v.x = v.x * len_inv;
   v.y = v.y * len_inv;
   v.z = v.z * len_inv;

   return v;
}

constexpr f32
Squared(f32 x)
{
   return x * x;
}

struct KalmanEstimate
{
   f32 value = 0.f;
   f32 var   = Squared(2.f); // TODO: Pick value
};

// www.kalmanfilter.net
inline void
KalmanFilterStep(const f32 dt, const f32 measure, const f32 vel_measure,
                 KalmanEstimate& estimate)
{
   constexpr f32 vel_measure_var = Squared(4.f); // TODO: Pick value
   constexpr f32 measure_var     = Squared(3.f); // TODO: Pick value

   // Predict
   estimate.value += dt * vel_measure;
   estimate.var += dt * dt * vel_measure_var;

   // Update
   f32 kalman_gain = estimate.var / (estimate.var + measure_var);
   estimate.value += kalman_gain * (measure - estimate.value);
   estimate.var *= (1.f - kalman_gain);
}

inline f32
Clamp(f32 x, f32 min, f32 max)
{
   if (x < min)
      return min;
   if (x > max)
      return max;
   return x;
}

struct MPU6050
{
   Vec3           gyro_offset;
   KalmanEstimate pitch_estimate = {};
   KalmanEstimate roll_estimate  = {};
   u32            prev_time      = 0;

   Vec3 dir_ref = {0, 0, 0};
};

MPU6050 mpu6050[2] = {};

#ifdef ENABLE_MAGNETIC_SENSOR
KalmanEstimate yaw_estimate = {};
Vec3           mag_min;
Vec3           mag_max;
#endif // ENABLE_MAGNETIC_SENSOR

void
InitMPU6050(u8 addr_bit)
{
   u8 addr = MPU6050_address | (addr_bit & 0b1);

   Wire.beginTransmission(addr);
   Wire.write(0x6B);
   Wire.write(0x00);
   Wire.endTransmission();

   // DLPF_CFG = 5: 10Hz low pass filter
   Wire.beginTransmission(addr);
   Wire.write(0x1A);
   Wire.write(0x05);
   Wire.endTransmission();

   Wire.beginTransmission(addr);
   Wire.write(0x1B);
   Wire.write(FS_SEL << 3);
   Wire.endTransmission();

   Wire.beginTransmission(addr);
   Wire.write(0x1C);
   Wire.write(AFS_SEL << 3);
   Wire.endTransmission();

   mpu6050[addr].prev_time = micros();

   for (u16 i = 0; i < 1000; i++)
   {
      auto gyro = ReadGyroscope(addr_bit);

      auto& offset = mpu6050[addr].gyro_offset;
      offset.x += gyro.x / 1000.f;
      offset.y += gyro.y / 1000.f;
      offset.z += gyro.z / 1000.f;
   }
}

bool
InitSDCard()
{
   vSPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
   if (!SD.begin(SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(20), &vSPI)))
   {
      if (SD.card()->errorCode())
      {
         Serial.println(F("\nSD initialization failed.\n"
                          "Do not reformat the card!\n"
                          "Is the card correctly inserted?\n"
                          "Is chipSelect set to the correct value?\n"
                          "Does another SPI device need to be disabled?\n"
                          "Is there a wiring/soldering problem?\n"));
         Serial.print(F("errorCode: "));
         Serial.println(int(SD.card()->errorCode()), HEX);
         Serial.print(F("errorData: "));
         Serial.println(int(SD.card()->errorData()));
         return false;
      }

      if (SD.vol()->fatType() == 0)
      {
         Serial.println(F("Can't find a valid FAT16/FAT32 partition."));
         return false;
      }
      else
      {
         Serial.println(F("Can't determine error type\n"));
         return false;
      }
   }
   Serial.println(F("Card successfully initialized."));

   uint32_t size = SD.card()->sectorCount();
   if (size == 0)
   {
      Serial.println(
          F("Can't determine the card size.\n Try another SD card or "
            "reduce the SPI bus speed."));
   }
   else
   {
      uint32_t sizeMB = 0.000512 * size + 0.5;
      Serial.print("SD Card Size:");
      Serial.print(sizeMB);
      Serial.println("MB");

      if ((sizeMB > 1100 && SD.vol()->sectorsPerCluster() < 64)
          || (sizeMB < 2200 && SD.vol()->fatType() == 32))
      {
         Serial.print(
             F("\nThis card should be reformatted for best performance.\n"));
         Serial.print(
             F("Use a cluster size of 32 KB for cards larger than 1GB.\n"));
         Serial.print(
             F("Only cards larger than 2 GB should be formatted FAT32.\n"));
      }
   }
   SD_card_initialized = true;
   return true;
}

void
UpdateLeds(bool on)
{
   u32           time              = millis();
   constexpr u32 led_update_period = 1000 / 30;
   static u32    next_led_update   = time;
   static bool   leds_on           = false;

   static u8 t = 0;

   if (time > next_led_update)
   {
      next_led_update += led_update_period;

      if (on)
      {
         t += 1;
         auto    x = sin8(t);
         uint8_t i = 0;
         for (auto& led : leds)
         {
            led = CRGB(CHSV(map8(x, 0, 20), 255, 255));
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
ReadSpaces(const char** str_ptr)
{
   auto* str = *str_ptr;
   while (*str)
   {
      if (*str != ' ' && *str != '\r' && *str != '\n')
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
      if (*str == ' ' || *str == '\r' || *str == '\n')
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
      if (*str == ' ' || *str == '\r' || *str == '\n')
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

u32 time_sound_ended = 0;

struct SoundCooldownRange
{
   u32 min;
   u32 max;
};

std::vector<String> awake_sounds;
std::vector<String> moved_sounds;

constexpr SoundCooldownRange moved_sound_cooldown_range = {200, 1000};
constexpr SoundCooldownRange awake_sound_cooldown_range = {1000, 5000};

u16 moved_sound_cooldown = 0;
u16 awake_sound_cooldown = 0;

u16 volume_moved = FloatTo88(0.5f);
u16 volume_awake = FloatTo88(0.3f);
u16 volume_sleep = FloatTo88(0.1f);

u32           fall_asleep_time     = 0;
constexpr u32 fall_asleep_duration = 1000 * 60 * 5;

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
   Serial.printf("ESP-idf Version %s\n", esp_get_idf_version());
   Serial.print("ESP Arduino Version " STRINGIFY(ESP_ARDUINO_VERSION_MAJOR) //
                "." STRINGIFY(ESP_ARDUINO_VERSION_MINOR)                    //
                "." STRINGIFY(ESP_ARDUINO_VERSION_PATCH) "\n");             //

   pinMode(SNOOZE_BUTTON, INPUT_PULLUP);

   if (InitSDCard())
   {
      auto dir = SD.open("/");
      PrintDirectory(dir);

      dir = SD.open("/awake");
      if (dir)
      {
         awake_sounds = ListAllWaveFiles(dir);
         Serial.println(F("Awake sounds:"));
         for (auto& s : awake_sounds)
         {
            Serial.println(s);
         }
         Serial.println();
      }
      dir = SD.open("/moved");
      if (dir)
      {
         moved_sounds = ListAllWaveFiles(dir);
         Serial.println(F("Moved sounds:"));
         for (auto& s : moved_sounds)
         {
            Serial.println(s);
         }
         Serial.println();
      }

      Serial.println(F("Loading volume file"));
      auto file = SD.open("/volume.txt");
      char data[255];
      auto size  = file.read(data, sizeof(data) - 1);
      data[size] = 0;
      file.close();
      const char* str = data;
      while (*str)
      {
         ReadSpaces(&str);
         auto word = ReadWord(&str);
         if (StringMatch(word, str, "sleep"))
         {
            ReadSpaces(&str);
            volume_sleep = FloatTo88(ReadU16(&str) / 100.f);
         }
         else if (StringMatch(word, str, "awake"))
         {
            ReadSpaces(&str);
            volume_awake = FloatTo88(ReadU16(&str) / 100.f);
         }
         else if (StringMatch(word, str, "moved"))
         {
            ReadSpaces(&str);
            volume_moved = FloatTo88(ReadU16(&str) / 100.f);
         }
         else
         {
            Serial.print(F("Unknown volume: "));
            while (word != str)
            {
               Serial.print(*word);
               word++;
            }
            Serial.println();
         }
      }
   }
   else
   {
      Serial.print(F("InitSDCard Failed\n"));
   }

   i2s_config_t i2s_config = {
       .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
       .sample_rate          = 44100,
       .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
       .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo
       .communication_format = I2S_COMM_FORMAT_STAND_I2S,
       .intr_alloc_flags     = 0, // Default interrupt priority
       .dma_buf_count        = 8,
       .dma_buf_len          = 64, // TODO: pick buffer length, default 64
       .use_apll             = false,
       .tx_desc_auto_clear   = true, // Auto clear tx descriptor on underflow
       .fixed_mclk           = 0};

   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

   i2s_pin_config_t pin_config = {.bck_io_num   = I2S_BCK,
                                  .ws_io_num    = I2S_LCK,
                                  .data_out_num = I2S_DOUT,
                                  .data_in_num  = I2S_PIN_NO_CHANGE};
   i2s_set_pin(I2S_NUM_0, &pin_config);

   Wire.begin(I2C_SDA, I2C_SCL);

   for (u8 addr = 0; addr < 2; addr++)
   {
      InitMPU6050(addr);
   }

#ifdef ENABLE_MAGNETIC_SENSOR
   u8 data_rate = 0b100; // 15Hz
   Wire.beginTransmission(LSM303DLHC_mag_address);
   Wire.write(0x00); // CRA_REG_M
   Wire.write(data_rate << 2);
   Wire.endTransmission();

   u8 mag_gain = 0b000; //  input field range[Gauss] ±1.3
   // Output range -2048 to +2047
   Wire.beginTransmission(LSM303DLHC_mag_address);
   Wire.write(0x01); // CRB_REG_M
   Wire.write(mag_gain << 5);
   Wire.endTransmission();

   u8 MR_REG_M = 0b00; //  Continuous-conversion mode
   Wire.beginTransmission(LSM303DLHC_mag_address);
   Wire.write(0x02);
   Wire.write(MR_REG_M);
   Wire.endTransmission();

   while (!ReadMagneticField(mag_min))
      ;
   mag_max = mag_min;
#endif // ENABLE_MAGNETIC_SENSOR
}

inline Vec3
Cross(const Vec3& a, const Vec3& b)
{
   Vec3 result;
   result.x = a.y * b.z - a.z * b.y;
   result.y = a.z * b.x - a.x * b.z;
   result.z = a.x * b.y - a.y * b.x;
   return result;
}

inline f32
Dot(const Vec3& a, const Vec3& b)
{
   return a.x * b.x + a.y * b.y + a.z * b.z;
}

void
loop()
{
   if (digitalRead(SNOOZE_BUTTON) == 0)
   {
      fall_asleep_time = 0;
   }
   bool is_asleep = (millis() > fall_asleep_time);
   // if (player.state != Wave_Stop)
   if (player.playing)
   {
      if (!LoadSamples(player))
      {
         time_sound_ended = millis();
      }
   }
   else
   {
      if (is_asleep)
      {
         // Asleep
         StartWaveFile(SD.open("/sleep/sleep.wav"), volume_sleep);
      }
      else if (millis() > time_sound_ended + awake_sound_cooldown)
      {
         // Grunting while awake
         u32    rand  = esp_random();
         u32    index = rand % awake_sounds.size();
         String path  = "/awake/" + awake_sounds[index];

         StartWaveFile(SD.open(path), volume_awake);

         rand                 = esp_random();
         auto& range          = awake_sound_cooldown_range;
         awake_sound_cooldown = rand % (range.max - range.min) + range.min;
         Serial.printf("Awake sound cooldown = %d\n", awake_sound_cooldown);
      }
   }

   UpdateLeds(!is_asleep);

   for (u8 addr = 0; addr < 2; addr++)
   {
      auto& mpu   = mpu6050[addr];
      auto  accel = ReadAcceleration(addr);

      f32 pitch = -atan(accel.x / sqrt(accel.y * accel.y + accel.z * accel.z));
      f32 roll  = atan(accel.y / sqrt(accel.x * accel.x + accel.z * accel.z));

      auto gyro = ReadGyroscope(addr);
      gyro.x -= mpu.gyro_offset.x;
      gyro.y -= mpu.gyro_offset.y;
      gyro.z -= mpu.gyro_offset.z;

      auto time     = micros();
      f32  dt       = (f32)(time - mpu.prev_time) / 1000000.f;
      mpu.prev_time = time;
      KalmanFilterStep(dt, pitch, gyro.y, mpu.pitch_estimate);
      KalmanFilterStep(dt, roll, gyro.x, mpu.roll_estimate);

      Vec3 vec;
      f32  cos_pitch = cosf(mpu.pitch_estimate.value);
      f32  sin_pitch = sinf(mpu.pitch_estimate.value);
      f32  cos_roll  = cosf(mpu.roll_estimate.value);
      f32  sin_roll  = sinf(mpu.roll_estimate.value);
      vec.x          = cos_pitch * cos_roll;
      vec.y          = cos_pitch * sin_roll;
      vec.z          = sin_pitch;

      if (mpu.dir_ref.x == 0 && mpu.dir_ref.y == 0 && mpu.dir_ref.z == 0)
      {
         mpu.dir_ref = vec;
      }
      else if (Dot(vec, mpu.dir_ref) < cos(20.f * PI / 180.f))
      {
         mpu.dir_ref = vec;

         u32 time = millis();

         // If asleep or moved_sound_cooldown ended
         if (time > fall_asleep_time
             || (time > time_sound_ended + moved_sound_cooldown
                 && !player.playing))
         {
            // Grunting when moved
            u32    rand  = esp_random();
            u32    index = rand % moved_sounds.size();
            String path  = "/moved/" + moved_sounds[index];

            StartWaveFile(SD.open(path), volume_moved);

            rand                 = esp_random();
            auto& range          = moved_sound_cooldown_range;
            moved_sound_cooldown = rand % (range.max - range.min) + range.min;
            Serial.printf("Moved sound cooldown = %d\n", moved_sound_cooldown);

            fall_asleep_time = time + fall_asleep_duration;
         }
      }

      // Serial.printf(">vecx%d:%f\n", addr, vec.x);
      // Serial.printf(">vecy%d:%f\n", addr, vec.y);
      // Serial.printf(">vecz%d:%f\n", addr, vec.z);

      // Serial.printf(">dir_refx%d:%f\n", addr, mpu.dir_ref.x);
      // Serial.printf(">dir_refy%d:%f\n", addr, mpu.dir_ref.y);
      // Serial.printf(">dir_refz%d:%f\n", addr, mpu.dir_ref.z);

      // Serial.printf(">dot%d:%f\n", addr, Dot(vec, mpu.dir_ref) * (180.f /
      // PI));

      // Serial.printf(">accelx:%d\n", accel.x);
      // Serial.printf(">accely:%d\n", accel.y);
      // Serial.printf(">accelz:%d\n", accel.z);

      // Serial.printf(">gyrox:%d\n", gyro.x);
      // Serial.printf(">gyroy:%d\n", gyro.y);
      // Serial.printf(">gyroz:%d\n", gyro.z);

      // Serial.printf(">pitch%d:%f\n", addr, pitch * (180.f / PI));
      // Serial.printf(">roll%d:%f\n", addr, roll * (180.f / PI));

      // Serial.printf(">pitch_estimate%d:%f\n", addr,
      //               mpu.pitch_estimate.value * (180.f / PI));
      // Serial.printf(">roll_estimate%d:%f\n", addr,
      //               mpu.roll_estimate.value * (180.f / PI));

      // Serial.printf(">pitch_estimate_var%d:%f\n", addr,
      // mpu.pitch_estimate.var); Serial.printf(">roll_estimate_var%d:%f\n",
      // addr, mpu.roll_estimate.var);
   }

#ifdef ENABLE_MAGNETIC_SENSOR
   Vec3 mag;
   if (ReadMagneticField(mag))
   {
      // Vec3 dir;
      // // dir.x = cos(roll_estimate.value) * cos(pitch_estimate.value);
      // // dir.y = sin(roll_estimate.value) * cos(pitch_estimate.value);
      // // dir.z = sin(pitch_estimate.value);
      // dir.x = cos(pitch_estimate.value) * cos(roll_estimate.value);
      // dir.y = sin(pitch_estimate.value) * cos(roll_estimate.value);
      // dir.z = sin(roll_estimate.value);
      // // X+: down
      // // Z+: usb
      // // Y+:  "left" (right hand rule)

      // // TODO: Improve this
      // Vec3 u1 = Cross(dir, {0.f, 1.f, 0.f});
      // Vec3 u2 = Cross(dir, u1);

      // Serial.printf(">dirx:%f\n", dir.x);
      // Serial.printf(">diry:%f\n", dir.y);
      // Serial.printf(">dirz:%f\n", dir.z);

      mag_min.x = min(mag_min.x, mag.x);
      mag_min.y = min(mag_min.y, mag.y);
      // mag_min.z = min(mag_min.z, mag.z);

      mag_max.x = max(mag_max.x, mag.x);
      mag_max.y = max(mag_max.y, mag.y);
      // mag_max.z = max(mag_max.z, mag.z);

      Vec3 mag_n;
      mag_n.x = NormalizeMag(mag.x, mag_min.x, mag_max.x);
      mag_n.y = NormalizeMag(mag.y, mag_min.y, mag_max.y);
      // mag_n.z = NormalizeMag(mag.z, mag_min.z, mag_max.z);

      // f32 mag_x = Dot(mag_n, u1);
      // f32 mag_y = Dot(mag_n, u2);

      Serial.printf(">magx:%f\n", mag.x);
      Serial.printf(">magy:%f\n", mag.y);
      // Serial.printf(">magz:%f\n", mag.z);

      Serial.printf(">mag_n_x:%f\n", mag_n.x);
      Serial.printf(">mag_n_y:%f\n", mag_n.y);
      // Serial.printf(">mag_x:%f\n", mag_x);
      // Serial.printf(">mag_y:%f\n", mag_y);

      f32 yaw = -atan2(mag_n.y, mag_n.x);

      static auto prev_time = micros();
      auto        time      = micros();
      f32         dt        = (f32)(time - prev_time) / 1000000.f;
      prev_time             = time;
      KalmanFilterStep(dt, yaw, gyro.z, yaw_estimate);

      Serial.printf(">yaw:%f\n", yaw * (180.f / PI));
      Serial.printf(">yaw_estimate:%f\n", yaw_estimate.value * (180.f / PI));
   }
#endif // ENABLE_MAGNETIC_SENSOR
}
