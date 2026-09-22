# ESP32 Multimedia Player (GIF + WAV)

A standalone ESP32-WROOM-32-based multimedia player that displays GIF animations on an ST7789 TFT screen while playing WAV audio through an I2S amplifier, with volume controlled by a physical potentiometer. All assets are read directly from the SD card — no WiFi or network connection required.

GIFs and audio are **paired based on the same filename**
(`007.gif` is always played together with `007.wav`). Each time the button is pressed, the board randomly selects a new pair.

## Features

* Full-screen 240x240 GIF animation on an ST7789 TFT.
* WAV audio playback (16-bit PCM, mono/stereo) via I2S with real-time volume control from a potentiometer.
* Automatically scans the SD card folder at boot, pairs gif+wav files based on their filenames, and writes the result to `manifest.json` — no manual configuration is required when adding or removing assets.
* Randomizes gif+wav pairs with a single button.
* Dual-core architecture: audio streaming (core 0) runs independently from GIF animation (core 1), so the GIF never stutters while waiting for audio.
* SD card access from both cores is protected by a mutex to prevent race conditions.

## Required Hardware

| Component      | Example                                       |
| -------------- | --------------------------------------------- |
| Board          | ESP32-WROOM-32 (any dev board)                |
| Display        | TFT ST7789 240x240, SPI interface             |
| SD card module | SPI microSD reader                            |
| Amplifier      | I2S DAC/amplifier, e.g. MAX98357A or PCM5102A |
| Speaker        | According to amplifier output                 |
| Potentiometer  | 10K linear, for volume control                |
| Push button    | Momentary, normally-open                      |

## Wiring

All pins are defined in [`config.h`](./config.h). Defaults:

| Function             | ESP32 Pin          |
| -------------------- | ------------------ |
| TFT SCK (VSPI bus)   | GPIO 18            |
| TFT MOSI / SDA       | GPIO 23            |
| TFT DC               | GPIO 2             |
| TFT RST              | GPIO 4             |
| TFT Backlight        | GPIO 15            |
| SD SCK (HSPI bus)    | GPIO 14            |
| SD MISO              | GPIO 12            |
| SD MOSI              | GPIO 13            |
| SD CS                | GPIO 5             |
| I2S BCLK             | GPIO 27            |
| I2S LRC              | GPIO 26            |
| I2S DIN              | GPIO 25            |
| Volume potentiometer | GPIO 34 (ADC1_CH6) |
| Push button          | GPIO 32 → GND      |

### Wiring Diagram

```text
                         ESP32-WROOM-32
                    ┌─────────────────────┐
                    │                     │
      GPIO 18 ──────┤ TFT SCK             │
      GPIO 23 ──────┤ TFT MOSI            │
       GPIO 2 ──────┤ TFT DC              │
       GPIO 4 ──────┤ TFT RST             │
      GPIO 15 ──────┤ TFT Backlight       │
                    │                     │
      GPIO 14 ──────┤ SD SCK              │
      GPIO 12 ──────┤ SD MISO             │
      GPIO 13 ──────┤ SD MOSI             │
       GPIO 5 ──────┤ SD CS               │
                    │                     │
      GPIO 27 ──────┤ I2S BCLK            │
      GPIO 26 ──────┤ I2S LRC             │
      GPIO 25 ──────┤ I2S DIN             │
                    │                     │
      GPIO 34 ──────┤ Volume              │
      GPIO 32 ──────┤ Button              │
                    │                     │
          5V ───────┤ Power               │
         GND ───────┤ Ground              │
                    └─────────────────────┘
```

**Important notes:**

* The TFT CS is assumed to be permanently connected to GND (not controlled by software). The TFT (VSPI) and SD card (HSPI) are intentionally separated onto **two different physical SPI buses**, so no additional CS wire needs to be added to the TFT.
  Do not connect the SD card and TFT to the same physical pins if they are declared as separate `SPIClass` instances — the two hardware peripherals will compete for the same bus, causing SD card initialization to fail without a clear error message.
* GPIO12 is a *strapping pin* that determines the flash voltage during boot. If the board enters a boot loop after the SD module is connected, move `SD_MISO` to another pin (e.g. GPIO 33) and update `config.h` accordingly.
* Use a Class 10 / UHS-1 SD card for smooth playback.

## Project Folder Structure

```text
esp32-multimedia/
├── esp32-multimedia.ino                   setup() & loop() only
├── config.h                               all pins & constants
├── globals.h / globals.cpp                shared state & objects between modules
├── display.h / display.cpp                TFT ST7789 driver (low-level)
├── gif_player.h / gif_player.cpp          AnimatedGIF callback, reads GIFs from SD
├── sd_manifest.h / sd_manifest.cpp        SD scan, gif+wav pairing, manifest.json
├── audio_player.h / audio_player.cpp      WAV parsing, I2S, audio task (core 0)
├── player_control.h / player_control.cpp  select & play one pair
├── button.h / button.cpp                  button detection (short press = randomize)
├── README.md
└── .gitignore
```

Each module has a single responsibility. Shared state (e.g. `manifestItems[]`, `sdMutex`, and the `gif` object) is defined once in `globals.cpp` and accessed by other modules through `extern` declarations in `globals.h`.

## SD Card Folder Structure

```text
/esp32-multimedia/
├── gif/
│   ├── 000.gif
│   ├── 001.gif
│   └── ...
├── audio/
│   ├── 000.wav
│   ├── 001.wav
│   └── ...
└── manifest.json    <- automatically rewritten on every boot
```

* Only basenames that have a **matching** gif+wav pair with exactly the same name (case-insensitive) are used. `005.gif` without `005.wav` (or vice versa) will be ignored.
* The number of GIF and WAV files can be different; only matching pairs are required.
* WAV files must be 16-bit PCM, mono or stereo, with any sample rate.
* GIFs should ideally be 240x240 and **not delta-encoded**. If an animation appears frozen or does not play correctly, coalesce it first with ImageMagick:

```bash
magick input.gif -coalesce -layers Optimize none output.gif
```

## How It Works

1. **Boot**: `sd_manifest` scans `/gif` and `/audio`, matches basenames that have pairs, writes the result to `manifest.json`, then reads the file back as the data source for randomization (rather than scanning the SD card directly every time the button is pressed).
2. `player_control` selects a random pair and tells `gif_player` (GIF basename) and `audio_player` (play command through the queue) to start playback.
3. `gif_player` runs on core 1 through `loop()`, continuously playing the GIF frames (endless loop).
4. `audio_player` runs as a separate task on core 0, plays the WAV **once**, then remains idle while waiting for the next command.
5. Pressing the button causes `button.cpp` to call `playRandomPair()` again — the currently playing audio is immediately stopped when the new command enters the queue.

## Installation & Build

1. Install [Arduino IDE](https://www.arduino.cc/en/software) and the [ESP32 board package](https://github.com/espressif/arduino-esp32).
2. Install the [AnimatedGIF](https://github.com/bitbank2/AnimatedGIF) library (bitbank2) through the Library Manager. Other libraries (`SD`, `SPI`, `driver/i2s.h`, `driver/adc.h`) are already included with the ESP32 core.
3. Open `esp32-multimedia.ino` — all `.h`/`.cpp` files in the same folder will automatically be compiled.
4. Select **ESP32 Dev Module** under Tools > Board.
5. Prepare the SD card using the folder structure above, then upload the sketch.

## Configuration

Behavior options are located at the top of `config.h`:

```cpp
// false = play audio once, then remain silent (default)
// true  = automatically repeat audio until the button is pressed
#define AUTO_REPEAT_AUDIO false
```

## Troubleshooting

| Symptom                                                 | Possible Cause                                                                                                                                              |
| ------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Serial output gets stuck at `"Initializing SD Card..."` | SD and TFT are sharing the same physical pins while using two different `SPIClass` instances — separate the buses or check the HSPI wiring                  |
| `SD Card FAILED after 3 attempts`                       | Card is not formatted as FAT32, wiring is incorrect, or jumper wires are too long for the SPI speed being used — try lowering the clock speed in `sdInit()` |
| GIF is displayed but does not animate                   | Source GIF is *delta-encoded* (only changed pixels are stored). Coalesce it first with ImageMagick (see the SD card folder structure section)               |
| Board enters a boot loop after the SD card is connected | GPIO12 (strapping pin) is being used as `SD_MISO` — move it to another pin                                                                                  |

