#include "alias.hpp"

#include <Arduino.h>
#include <SPI.h>
#define FS_NO_GLOBALS
#include <LittleFS.h>

#include <SdFat.h>

#include "driver/i2s.h"

#if LOLIN_D32

constexpr u8 I2S_BCK  = 13;
constexpr u8 I2S_DOUT = 12; // DIN pin
constexpr u8 I2S_LCK  = 14;

constexpr u8 SD_MISO = 32;
constexpr u8 SD_MOSI = 25;
constexpr u8 SD_CLK  = 33;
constexpr u8 SD_CS   = 26;

#endif

File32 sound_dir;
f32    volume = 0.1;

SdFat32  SD;
SPIClass vSPI;

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

struct WaveFile
{
   File32 handle;

   static constexpr u32 buffer_size = 256;
   u8*                  buffer      = nullptr;

   u8* read_ptr = 0;
   u8* end      = 0;
};

bool
LoadFileIntoBuffer(WaveFile& file)
{
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
   u32 loaded = file.handle.read(dest, file.buffer + file.buffer_size - dest);

   // Serial.println("\nnew data:");
   // for (u32 i = 0; i < loaded; i++)
   // {
   //    Serial.print(dest[i], HEX);
   // }

   file.read_ptr = file.buffer;
   file.end      = dest + loaded;

   return loaded != 0;
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
   u32 data_size       = 0;
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
         header.data_size = section_size;
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
   u16        volume      = FloatTo88(0.1f);
};

Wave wave_file = {};

bool
LoadWaveFile(Wave& wave, const File32& file32)
{
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

bool
LoadSamples(Wave& wave)
{
   while (true)
   {
      if (wave.last_sample != S32_MIN)
      {
         size_t bytes_written;
         i2s_write(I2S_NUM_0, &wave.last_sample, sizeof(s16) * 2,
                   &bytes_written, 0);
         if (bytes_written == 0)
            break;
      }

      s16 left  = 0;
      s16 right = 0;
      if (wave.header.channel_count == 1)
      {
         s16 sample = 0;
         if (!ReadFile(wave.file, sample))
            return false;

         left  = sample;
         right = sample;
      }
      else if (wave.header.channel_count == 2)
      {
         if (!ReadFile(wave.file, left))
            return false;

         if (!ReadFile(wave.file, right))
            return false;
      }
      wave.last_sample = (ScaleBy88(left, wave.volume) & 0xffff)
                         | (ScaleBy88(right, wave.volume) << 16);
   }
   return true;
}

void
StartNextFile()
{
   while (true)
   {
      File32 file = sound_dir.openNextFile();
      if (!file)
      {
         break;
      }

      char   filename[64];
      size_t size = file.getName(filename, sizeof(filename));

      Serial.print(F("File: "));
      file.printName(&Serial);
      Serial.print(F("\n"));
      if (EndsWithWav(filename, size))
      {
         DestroyWaveFile(wave_file);
         LoadWaveFile(wave_file, file);
         break;
      }
   }
}

void
setup()
{
   Serial.begin(SERIAL_BAUD_RATE);

   Serial.print(F("Init\n"));
   Serial.printf("ESP-idf Version %s\n", esp_get_idf_version());
   Serial.print("ESP Arduino Version " STRINGIFY(ESP_ARDUINO_VERSION_MAJOR) //
                "." STRINGIFY(ESP_ARDUINO_VERSION_MINOR)                    //
                "." STRINGIFY(ESP_ARDUINO_VERSION_PATCH) "\n");             //

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
         return;
      }

      if (SD.vol()->fatType() == 0)
      {
         Serial.println(F("Can't find a valid FAT16/FAT32 partition."));
         return;
      }
      else
      {
         Serial.println(F("Can't determine error type\n"));
         return;
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
             F("Use a cluster size of 32 KB for cards larger than 1 GB.\n"));
         Serial.print(
             F("Only cards larger than 2 GB should be formatted FAT32.\n"));
      }
   }

   sound_dir = SD.open("/");
   PrintDirectory(sound_dir);

   i2s_config_t i2s_config = {
       .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
       .sample_rate          = 44100,
       .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
       .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo
       .communication_format = I2S_COMM_FORMAT_STAND_I2S,
       .intr_alloc_flags     = 0, // Default interrupt priority
       .dma_buf_count        = 8,
       .dma_buf_len          = 256, // TODO: pick buffer length, default 64
       .use_apll             = false,
       .tx_desc_auto_clear   = true, // Auto clear tx descriptor on underflow
       .fixed_mclk           = 0};

   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

   i2s_pin_config_t pin_config = {.bck_io_num   = I2S_BCK,
                                  .ws_io_num    = I2S_LCK,
                                  .data_out_num = I2S_DOUT,
                                  .data_in_num  = I2S_PIN_NO_CHANGE};
   i2s_set_pin(I2S_NUM_0, &pin_config);

   StartNextFile();
}

f32 t = 0;

void
loop()
{
   if (!LoadSamples(wave_file))
   {
      StartNextFile();
   }
}