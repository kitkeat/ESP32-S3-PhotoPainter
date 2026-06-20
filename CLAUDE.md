# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is firmware for the **Waveshare ESP32-S3-PhotoPainter**, a 7.3-inch 7-color e-paper display device powered by an ESP32-S3. The project has two firmware implementations:

- **`01_Example/xiaozhi-esp32/`** — Main ESP-IDF firmware (C/C++). Based on the open-source [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) project, adapted for the PhotoPainter hardware. This is the primary codebase.
- **`05_ArduinoExample/`** — Arduino example for audio playback/recording only (not the main image display firmware).

Current version: **1.5.0** — uses Volcano Ark model (Doubao-Seedream-4.0) for AI image generation.

## Build System (ESP-IDF)

All build commands must be run from `01_Example/xiaozhi-esp32/` with ESP-IDF environment activated.

```bash
# Build
idf.py build

# Flash to device
idf.py -p <PORT> flash

# Flash + monitor serial output
idf.py -p <PORT> flash monitor

# Monitor only
idf.py -p <PORT> monitor

# Configure menuconfig (select board, language, AI model, etc.)
idf.py menuconfig

# Clean build
idf.py fullclean
```

The project name is `xiaozhi` (defined in `CMakeLists.txt`). Version is set via `set(PROJECT_VER "2.0.1")`.

## Architecture: Three Operating Modes

The device boots and reads a mode byte from NVS (`PhotoPainter` namespace, key `PhotPainterMode`). Mode selection is done by pressing the BOOT button during startup (cycling through 1→2→3), then confirming with a long press.

| Mode | Name | Description |
| --- | --- | --- |
| 1 | Basic mode | Cycles through images from SD card (`/sdcard/06_user_foundation_img/`); sleeps between displays using deep sleep + timer/ext1 wakeup |
| 2 | Network mode | Hosts an HTTP web server (AP mode: SSID `esp_network`, pass `1234567890`, IP `192.168.4.1`; or STA mode: mDNS `esp32-s3-photopainter.local`). Users send images via browser. |
| 3 | Xiaozhi mode | AI voice assistant integration using xiaozhi-esp32; generates images via Volcano Ark API (Doubao-Seedream-4.0 model) based on voice/text commands, also shows weather data |

## Component Structure (`01_Example/xiaozhi-esp32/components/`)

### `user_app_bsp/` — Application glue layer (main custom code)

- `user_app.cpp/.h` — Global hardware init (`User_Mode_init()`), LED task management, SD card init, mode dispatch
- `mode_src/Mode_Selection.cpp` — Boot mode selection UI using BOOT button + audio feedback
- `mode_src/Basic_mode.cpp` — Mode 1 implementation: SD card image cycling, deep sleep management
- `mode_src/Network_mode.cpp` — Mode 2 implementation: HTTP server for image upload, AP/STA switching
- `mode_src/xiaozhi_mode.cpp` — Mode 3 implementation: AI voice assistant, weather display, AI image generation loop

### `app_bsp/` — Application-level services

- `ai_app.cpp/.h` — `BaseAIModel` class: HTTP calls to Volcano Ark API, downloads JPG from URL, dithers to e-paper color space, saves to SD card
- `imgdecode_app.cpp/.h` — `ImgDecodeDither` class: JPG/PNG/BMP decoding, Floyd-Steinberg dithering to 7 colors, nearest-color quantization, BMP encoding
- `server_app.cpp/.h` — HTTP server for Mode 2 (AP/STA WiFi, image upload endpoint, mDNS)
- `weather_app.cpp/.h` — `WeatherPort` class: parses weather JSON, maps weather types to SD card images
- `client_app.c/.h` — HTTP client for fetching weather data

### `port_bsp/` — Hardware BSP (board support package)

- `display_bsp.cpp/.h` — `ePaperPort` class: SPI e-paper driver; key methods:
  - `EPD_SDcardBmpShakingColor()` — display pre-dithered BMP
  - `EPD_SDcardIMGShakingColor()` — display JPG/PNG/BMP with dithering (fixed size)
  - `EPD_SDcardScaleIMGShakingColor()` — same but with auto-scale to fit
  - `EPD_Display()` — flush buffer to e-paper panel
- `sdcard_bsp.cpp/.h` — `CustomSDPort` class: SDMMC 4-bit interface, directory scanning into a linked list
- `button_bsp.c/.h` — `BootButtonGroups`, `GP4ButtonGroups`, `PWRButtonGroups` FreeRTOS event groups for button events (single click = bit 0, long press = bit 1, double click = bit 2)
- `i2c_bsp.cpp/.h` — `I2cMasterBus` class: I2C bus wrapper (SDA=48, SCL=47, port=0)
- `codec_bsp.cpp/.h` — `CodecPort` class: ES8311 DAC + ES7210 ADC codec management for audio
- `led_bsp.c/.h` — Green/Red LED control
- `power_bsp.cpp/.h` — AXP2101 PMIC via I2C (address 0x34): battery info, charging status

### `codec_board/` — Waveshare codec board component (ESP-IDF component with `idf_component.yml`)

### `pmicpower/` — XPowers library for AXP2101 PMIC

## Key Global Objects (defined in `user_app.cpp`)

```cpp
CustomSDPort *SDPort;           // SD card, mounted at /sdcard
ImgDecodeDither decdither;      // Image decode + Floyd-Steinberg dither
ePaperPort ePaperDisplay(decdither, 11,10,8,9,12,13, 800,480, 1350,1350);
                                // MOSI=11, SCL=10, DC=8, CS=9, RST=12, BUSY=13
                                // Display: 800×480, max decode: 1350×1350
I2cMasterBus I2cBus(48,47,0);  // SDA=48, SCL=47, port=0
SemaphoreHandle_t epaper_gui_semapHandle; // Mutex: always take before drawing
```

## NVS Namespace

All persistent config lives under NVS namespace `"PhotoPainter"`:

- `PhotPainterMode` (u8): 0x01=Mode1, 0x02=Mode2, 0x03=Mode3, 0x04=mode selection screen
- `Mode_Flag` (u8): 0x01 = mode just set (triggers restart + clear)
- `NetworkMode` (u8): 0=AP, 1=STA (for Mode 2)
- WiFi credentials stored by xiaozhi-esp32 framework for Mode 3

## SD Card Directory Layout

```text
/sdcard/
  02_sys_ap_img/      # System UI images for Mode 2
  06_user_foundation_img/  # User images for Mode 1 cycling (also temp decode target: sys_decode.bmp)
  ai_config.json      # AI model config (url, model, key, time) read by BaseAIModel
```

The `ai_config.json` format: `{"url":"...","model":"Doubao-Seedream-4.0","key":"...","time":780}`

## Xiaozhi Mode AI Flow (Mode 3)

1. `xiaozhi_init_received()` fires after AI assistant announces its version
2. Fetches weather → displays on e-paper
3. `xiaozhi_ai_Message()` fires on AI conversation events → triggers `BaseAIModel::BaseAIModel_SetChat()` with image prompt
4. `BaseAIModel` POSTs to Volcano Ark API, gets image URL, downloads JPG to PSRAM, dithers to 7-color space, saves BMP to SD card, then `ePaperPort` displays it
5. Voice commands can also control image carousel interval (`img_loopTimer`, default 1 min)

## Image Processing Pipeline

Raw image → `ImgDecodeDither` (JPG/PNG/BMP decode to RGB888) → `ImgDecode_ScaleRgb888Nearest()` (scale to 800×480 if needed) → `ImgDecode_DitherRgb888()` (Floyd-Steinberg to 7 e-paper colors) → `ImgDecode_EncodingBmpToSdcard()` → `ePaperPort` reads BMP and sends to e-paper panel via SPI.

Supported image formats: JPG (Baseline DCT only), PNG, BMP 24-bit. Max resolution for auto-scale: 1350×1350.

## Button Behavior Summary

- **BOOT button single click (bit 0)**: Mode-dependent (next image in Mode 1, enter provisioning in Mode 3)
- **BOOT button long press (bit 1)**: Enter deep sleep (any mode)
- **BOOT button double click (bit 2)**: Show battery info on e-paper (any mode)
- **GP4 (KEY) button single click**: Mode selection cycling (during mode select screen)
- **GP4 long press**: Confirm mode selection → save to NVS → restart

## Arduino Example (`05_ArduinoExample/`)

`01_Audio_Test.ino` — standalone Arduino sketch for testing ES8311/ES7210 audio codec. Not related to the main e-paper display firmware. Configure Arduino IDE with: Board=ESP32-S3-DevKitC-1, Flash=16MB, PSRAM=OPI PSRAM.
