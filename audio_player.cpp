#include "audio_player.h"
#include "globals.h"
#include "config.h"
#include "driver/i2s.h"
#include "driver/adc.h"

// ====================================================================
// VOLUME
// ====================================================================

static float getVolume() {
  int raw = adc1_get_raw(POT_PIN);
  float volume = (float)raw / 4095.0f;
  if (volume < 0.01f) volume = 0.0f;
  return volume;
}

static int16_t applyVolume(int16_t sample, float volume) {
  int32_t value = (int32_t)(sample * volume);
  if (value > 32767) value = 32767;
  if (value < -32768) value = -32768;
  return (int16_t)value;
}

// ====================================================================
// I2S SETUP
// ====================================================================

static void setupI2S(uint32_t sampleRate) {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = sampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_I2S_MSB;
  config.intr_alloc_flags = 0;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  i2s_driver_install(I2S_PORT, &config, 0, NULL);

  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_BCLK;
  pins.ws_io_num = I2S_LRC;
  pins.data_out_num = I2S_DIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

// ====================================================================
// PARSING WAV
// ====================================================================

static uint16_t readLE16(uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t readLE32(uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool readExactFile(File &f, uint8_t* buf, size_t len) {
  size_t got = 0;
  while (got < len) {
    int n = f.read(buf + got, len - got);
    if (n <= 0) return false;
    got += n;
  }
  return true;
}

static bool skipBytesFile(File &f, uint32_t count) {
  uint8_t tmp[64];
  while (count > 0) {
    uint32_t chunk = min(count, (uint32_t)sizeof(tmp));
    if (!readExactFile(f, tmp, chunk)) return false;
    count -= chunk;
  }
  return true;
}

// Note: caller require holding sdMutex while this parse is running,
// because the parser relies on a stable File cursor position across reads.
static bool parseWavFile(File &f, WAVInfo &wav) {
  uint8_t header[12];
  if (!readExactFile(f, header, 12)) return false;

  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    Serial.println("Invalid WAV (not RIFF/WAVE)");
    return false;
  }

  bool foundFmt = false, foundData = false;

  while (!foundData) {
    uint8_t ch[8];
    if (!readExactFile(f, ch, 8)) return false;

    char id[5];
    memcpy(id, ch, 4);
    id[4] = 0;
    uint32_t sz = readLE32(ch + 4);

    if (strcmp(id, "fmt ") == 0) {
      if (sz < 16) return false;
      uint8_t fmt[16];
      if (!readExactFile(f, fmt, 16)) return false;

      wav.audioFormat   = readLE16(fmt);
      wav.channels      = readLE16(fmt + 2);
      wav.sampleRate    = readLE32(fmt + 4);
      wav.bitsPerSample = readLE16(fmt + 14);

      if (sz > 16 && !skipBytesFile(f, sz - 16)) return false;
      foundFmt = true;
    } else if (strcmp(id, "data") == 0) {
      wav.dataSize = sz;
      foundData = true;
    } else {
      if (!skipBytesFile(f, sz)) return false;
    }

    if (sz & 1) {
      if (!skipBytesFile(f, 1)) return false;
    }
  }

  if (!foundFmt || !foundData) return false;
  if (wav.audioFormat != 1)    { Serial.println("WAV must be PCM"); return false; }
  if (wav.bitsPerSample != 16) { Serial.println("WAV must be 16-bit"); return false; }
  if (wav.channels != 1 && wav.channels != 2) {
    Serial.println("WAV must be mono/stereo");
    return false;
  }

  return true;
}

// ====================================================================
// AUDIO TASK (core 0)
//
// Waits for commands, plays a file once (or loop), then waits for the
// next command. A new command that arrives in the middle of playback
// will immediately stop the current song.
// ====================================================================

static void audioTask(void* param) {
  static uint8_t buf[AUDIO_BUF_LEN];
  static int16_t stereoBuf[AUDIO_BUF_LEN];

  char cmd[CMD_LEN];

  for (;;) {
    if (xQueueReceive(audioCmdQueue, &cmd, portMAX_DELAY) != pdTRUE) continue;

    bool loopIt = (cmd[0] == 'L');
    String basename = String(&cmd[2]);
    String path = String(AUDIO_DIR) + "/" + basename + ".wav";

    File audioFile;
    WAVInfo wavInfo;

    if (xSemaphoreTake(sdMutex, portMAX_DELAY) != pdTRUE) {
      Serial.printf("Fail to take SD mutex: %s\n", path.c_str());
      continue;
    }

    audioFile = SD.open(path.c_str(), FILE_READ);
    bool wavOK = false;

    if (audioFile) {
      wavOK = parseWavFile(audioFile, wavInfo);
    }

    if (!wavOK) {
      Serial.printf("Fail to open/parse WAV: %s\n", path.c_str());
      if (audioFile) audioFile.close();
      xSemaphoreGive(sdMutex);
      continue;
    }

    uint32_t dataStart = audioFile.position();
    xSemaphoreGive(sdMutex);

    uint32_t bytesPlayed = 0;

    i2s_set_clk(I2S_PORT, wavInfo.sampleRate,
        I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    i2s_zero_dma_buffer(I2S_PORT);

    Serial.printf("Audio play: %s (%lu Hz, %u ch, %s)\n",
        path.c_str(), (unsigned long)wavInfo.sampleRate,
        wavInfo.channels, loopIt ? "loop" : "once");

    bool interrupted = false;

    while (true) {
      if (uxQueueMessagesWaiting(audioCmdQueue) > 0) {
        interrupted = true;
        break;
      }

      uint32_t remaining = wavInfo.dataSize - bytesPlayed;

      if (remaining == 0) {
        if (loopIt) {
          if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
            audioFile.seek(dataStart);
            xSemaphoreGive(sdMutex);
          }
          bytesPlayed = 0;
          continue;
        }
        break;
      }

      size_t toRead = (remaining < AUDIO_BUF_LEN) ? remaining : AUDIO_BUF_LEN;

      int n = 0;
      if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
        n = audioFile.read(buf, toRead);
        xSemaphoreGive(sdMutex);
      }

      if (n <= 0) break;
      bytesPlayed += n;

      float volume = getVolume();

      if (wavInfo.channels == 1) {
        int16_t* mono = (int16_t*)buf;
        size_t samples = n / 2;
        size_t sOut = 0;

        for (size_t i = 0; i < samples; i++) {
          int16_t s = applyVolume(mono[i], volume);
          stereoBuf[sOut++] = s;
          stereoBuf[sOut++] = s;
        }

        size_t written = 0;
        i2s_write(I2S_PORT, stereoBuf, sOut * 2, &written, portMAX_DELAY);
      } else {
        int16_t* stereo = (int16_t*)buf;
        size_t samples = n / 2;

        for (size_t i = 0; i < samples; i++) {
          stereo[i] = applyVolume(stereo[i], volume);
        }

        size_t written = 0;
        i2s_write(I2S_PORT, buf, n, &written, portMAX_DELAY);
      }
    }

    if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
      if (audioFile) audioFile.close();
      xSemaphoreGive(sdMutex);
    }

    i2s_zero_dma_buffer(I2S_PORT);

    if (!interrupted) {
      Serial.println("Audio finished (waiting for play button).");
    }
  }
}

// ====================================================================
// PUBLIC INTERFACE
// ====================================================================

void sendAudioCommand(const String &basename, bool loopIt) {
  char cmd[CMD_LEN];
  memset(cmd, 0, sizeof(cmd));

  // Format: "L:007" for loop, "S:007" for once
  String payload = (loopIt ? "L:" : "S:") + basename;
  payload.toCharArray(cmd, sizeof(cmd));

  xQueueOverwrite(audioCmdQueue, cmd);
}

void audioInit() {
  setupI2S(44100);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(POT_PIN, ADC_ATTEN_DB_11);

  audioCmdQueue = xQueueCreate(1, CMD_LEN);

  xTaskCreatePinnedToCore(audioTask, "audioTask", 8192, NULL, 1, NULL, 0);
}
