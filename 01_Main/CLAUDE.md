# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System (ESP-IDF)

All commands must be run from `d:\Document\Projects\ESP32-S3-PhotoPainter\01_Main\` with the ESP-IDF environment activated.

```bash
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> flash monitor
idf.py -p <PORT> monitor
idf.py menuconfig      # Select board type, language, AI features
idf.py fullclean
```

**Board selection**: In `menuconfig → Xiaozhi Assistant → Board Type`, select `Waveshare ESP32-S3-PhotoPainter`. Project name is `xiaozhi` (CMakeLists.txt), version `2.0.1`.

## Three Operating Modes

Boot reads `PhotPainterMode` (u8) from NVS namespace `"PhotoPainter"`. Mode is selected by pressing BOOT button during startup (cycles 1→2→3), confirmed with long press.

| Mode | Value | Entry function | Description |
|------|-------|----------------|-------------|
| Basic | `0x01` | `User_Basic_mode_app_init()` | Cycles SD card images; deep sleep between displays |
| Network | `0x02` | `User_Network_mode_app_init()` | HTTP server for image upload; AP (`esp_network`/`1234567890`, `192.168.4.1`) or STA (mDNS `esp32-s3-photopainter.local`) |
| Xiaozhi | `0x03` | `app.Start()` | AI voice assistant + image generation via Volcano Ark API |
| Mode Select | `0x04` | `Mode_Selection_Init()` | Boot mode selection screen |

`Mode_Flag` (u8, NVS) = `0x01` means mode was just set → triggers restart and clear.

## Component Architecture

### `components/user_app_bsp/` — Application glue

- `user_app.cpp` — Defines all global objects, LED tasks, button tasks, SD init, calls `User_Mode_init()`. **All global hardware singletons live here.**
- `mode_src/` — One `.cpp` per mode; each exports a single `init` function called from `main/main.cc`.

### `components/app_bsp/` — Application-level services

- `ai_app.cpp` — `BaseAIModel`: POSTs to Volcano Ark API, gets image URL, downloads JPG to PSRAM, dithers to 7 e-paper colors, saves BMP to SD card. Config read from `/sdcard/ai_config.json`.
- `imgdecode_app.cpp` — `ImgDecodeDither`: decodes JPG/PNG/BMP to RGB888, Floyd-Steinberg dither to 7 colors, nearest-color quantization, BMP encode.
- `server_app.cpp` — HTTP server for Mode 2 (AP/STA, image upload endpoint, mDNS).
- `weather_app.cpp` — `WeatherPort`: decodes weather JSON, maps weather type strings to SD card image paths.
- `client_app.c` — HTTP client for fetching weather JSON.

### `components/port_bsp/` — Hardware BSP

- `display_bsp.cpp` — `ePaperPort`: SPI e-paper driver (MOSI=11, SCL=10, DC=8, CS=9, RST=12, BUSY=13), display 800×480. Key display methods:
  - `EPD_SDcardBmpShakingColor()` — pre-dithered BMP at exact size
  - `EPD_SDcardIMGShakingColor()` — JPG/PNG/BMP at fixed 800×480
  - `EPD_SDcardScaleIMGShakingColor()` — any image with auto-scale to fit
  - `EPD_Display()` — flush buffer to panel via SPI
- `sdcard_bsp.cpp` — `CustomSDPort`: SDMMC 4-bit, mounts at `/sdcard`, scans directories into a linked list.
- `button_bsp.c` — FreeRTOS event groups: `BootButtonGroups`, `GP4ButtonGroups`, `PWRButtonGroups`. Bit 0 = single click, bit 1 = long press, bit 2 = double click.
- `i2c_bsp.cpp` — `I2cMasterBus`: I2C wrapper (SDA=48, SCL=47, port=0).
- `i2c_equipment.cpp` — `Shtc3Port`: SHTC3 temperature/humidity sensor driver (I2C addr `0x70`); used in Mode 3 to report ambient T/H.
- `codec_bsp.cpp` — `CodecPort`: ES8311 DAC + ES7210 ADC audio codec.
- `power_bsp.cpp` — AXP2101 PMIC via I2C (addr `0x34`): battery info and charging state.
- `traverse_nvs.cpp` — `TraverseNvs`: reads saved WiFi credentials from NVS blobs (used by Network mode).

### `main/` — Entry point and xiaozhi framework hooks

- `main.cc` — `app_main()`: NVS init, mode dispatch.
- `application.cc` — xiaozhi-esp32 `Application` singleton; used only in Mode 3.
- `xiaozhi_mode.cpp` (in `user_app_bsp/mode_src/`) — Implements three callbacks invoked by the xiaozhi framework:
  - `xiaozhi_init_received()` — fires on version announcement; fetches weather, triggers e-paper draw.
  - `xiaozhi_ai_Message()` — fires on conversation events; copies user text to `str_ai_chat_buff`.
  - `xiaozhi_application_received()` — fires on app state changes (`idle`/`listening`/`speaking`).

## Key Global Objects (defined in `user_app.cpp`)

```cpp
CustomSDPort *SDPort;           // SD card, mounted at /sdcard
ImgDecodeDither decdither;      // Image decode + Floyd-Steinberg dither
ePaperPort ePaperDisplay(decdither, 11,10,8,9,12,13, 800,480, 1350,1350);
                                // max decode resolution: 1350×1350
I2cMasterBus I2cBus(48,47,0);
SemaphoreHandle_t epaper_gui_semapHandle; // Mutex: always take before drawing
EventGroupHandle_t epaper_groups;         // Triggers GUI task refresh
```

**Always take `epaper_gui_semapHandle` before calling any `EPD_*` display method.**

## Image Processing Pipeline

Raw image → `ImgDecodeDither` (decode to RGB888) → `ImgDecode_ScaleRgb888Nearest()` (scale to 800×480) → `ImgDecode_DitherRgb888()` (Floyd-Steinberg → 7 e-paper colors) → `ImgDecode_EncodingBmpToSdcard()` → `ePaperPort` reads BMP → SPI to e-paper panel.

Supported input formats: JPG (Baseline DCT only), PNG, BMP 24-bit. Max auto-scale resolution: 1350×1350.

## SD Card Directory Layout

```
/sdcard/
  01_sys_init_img/       # Weather display template BMP
  02_sys_ap_img/         # System UI images for Mode 2
  05_user_ai_img/        # AI-generated images (Mode 3 carousel)
  06_user_foundation_img/  # User images for Mode 1; also temp decode target: sys_decode.bmp
  ai_config.json         # AI model config: {"url":"...","model":"Doubao-Seedream-4.0","key":"...","time":780}
```

## AI Image Generation Flow (Mode 3)

1. `User_xiaozhi_app_init()` reads `/sdcard/ai_config.json`, initializes `BaseAIModel`.
2. `SDPort->SDPort_ScanListDir("/sdcard/05_user_ai_img")` builds the image carousel list.
3. On voice input → `xiaozhi_ai_Message()` copies text → `ai_IMG_Task` wakes → `BaseAIModel::BaseAIModel_SetChat()` → Volcano Ark API → downloads JPG to PSRAM → dither → save BMP → `epaper_groups` bit 2 → `gui_user_Task` displays it.
4. `ai_IMG_LoopTask` carousels through saved AI images every `img_loopTimer` ms (default 60 000 ms).

## menuconfig Options of Note

Under `Xiaozhi Assistant`:
- **Board Type**: must be `Waveshare ESP32-S3-PhotoPainter`
- **Default Language**: `Chinese` (default) or others
- **Flash Assets**: controls whether voice/UI assets are flashed alongside firmware
- **Enable Wake Word Detection (AFE)**: requires ESP32-S3 + PSRAM; enabled by default
- **Enable Device-Side AEC**: available for PhotoPainter board
