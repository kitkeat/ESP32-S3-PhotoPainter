# ESP32-S3-PhotoPainter — File Reference

Quick orientation to every important source file in this project. Custom code lives under `components/`; the `main/` directory is mostly the upstream [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) framework, adapted for this board.

---

## 1. Entry Point (`main/`)

### Core

| File | Description |
|------|-------------|
| `main.cc` | `app_main()` — initializes NVS, reads `PhotPainterMode`, dispatches to the correct mode init function. |
| `application.cc/.h` | `Application` singleton from the xiaozhi-esp32 framework. Manages the AI voice assistant lifecycle. Used only in Mode 3. |
| `settings.cc/.h` | NVS-backed key-value settings store; wraps `nvs_flash` for persistent config. |
| `system_info.cc/.h` | Reads chip model, flash size, free heap, firmware version; used for diagnostics. |
| `device_state.h` | Enums for device states (`kDeviceStateStarting`, `kDeviceStateIdle`, etc.). |
| `device_state_event.cc/.h` | State-transition events and handlers; ties device states to audio/display reactions. |
| `assets.cc/.h` | Loads embedded flash assets (audio prompts, UI images) into memory at boot. |
| `ota.cc/.h` | Over-the-air firmware update logic; checks server for new firmware, downloads and applies. |
| `mcp_server.cc/.h` | Model Context Protocol server; exposes device tools (PTT, system info) to connected AI clients. |

### Audio Subsystem (`main/audio/`)

| File | Description |
|------|-------------|
| `audio_codec.h/.cc` | Abstract base class for all audio codecs; defines `Read()`, `Write()`, `SetVolume()`. |
| `audio_service.h/.cc` | Manages the full audio pipeline: codec → processor → wake word → protocol. |
| `audio_processor.h` | Interface for audio processors (AFE, no-op). |
| `wake_word.h` | Interface for wake word detectors. |
| `codecs/es8311_audio_codec.*` | **Active codec on PhotoPainter.** ES8311 DAC + ES7210 ADC driver. |
| `codecs/es8374_audio_codec.*` | ES8374 codec driver (other boards). |
| `codecs/es8388_audio_codec.*` | ES8388 codec driver (other boards). |
| `codecs/es8389_audio_codec.*` | ES8389 codec driver (other boards). |
| `codecs/box_audio_codec.*` | ESP-BOX onboard codec driver. |
| `codecs/dummy_audio_codec.*` | Stub codec that discards all audio; used for testing. |
| `codecs/no_audio_codec.*` | No-op codec for boards without audio hardware. |
| `processors/afe_audio_processor.*` | ESP AFE pipeline: acoustic echo cancellation, VAD, beamforming. |
| `processors/audio_debugger.*` | Dumps raw audio frames to serial for debugging. |
| `processors/no_audio_processor.*` | Pass-through processor (no DSP). |
| `wake_words/afe_wake_word.*` | Wake word detection via the ESP AFE engine. |
| `wake_words/custom_wake_word.*` | Custom wake word model loader. |
| `wake_words/esp_wake_word.*` | ESP built-in wake word (e.g., "Hi ESP"). |

### Display Subsystem (`main/display/`)

| File | Description |
|------|-------------|
| `display.h/.cc` | Abstract display interface (`ShowStatus()`, `ShowChat()`, etc.). |
| `lcd_display.*` | LCD panel implementation (used on LCD-equipped boards, not PhotoPainter). |
| `oled_display.*` | OLED panel implementation (128×64, SSD1306-style). |
| `lvgl_display/lvgl_display.*` | LVGL framebuffer integration; drives LCD/OLED via LVGL render loop. |
| `lvgl_display/lvgl_font.*` | Loads and caches LVGL-compatible bitmap fonts. |
| `lvgl_display/lvgl_image.*` | Image rendering helpers for LVGL (PNG/BMP decode → LVGL canvas). |
| `lvgl_display/lvgl_theme.*` | Light/dark theme manager for LVGL UI. |
| `lvgl_display/emoji_collection.*` | Emoji glyph atlas; maps Unicode points to embedded bitmaps. |
| `lvgl_display/gif/gifdec.*` | Third-party GIF decoder library. |
| `lvgl_display/gif/gifdec_mve.h` | Move-semantics wrapper for gifdec. |
| `lvgl_display/gif/lvgl_gif.*` | LVGL widget that plays animated GIFs using gifdec. |

### LED / Indicator (`main/led/`)

| File | Description |
|------|-------------|
| `led.h` | Abstract LED interface (`SetColor()`, `Blink()`). |
| `gpio_led.*` | Simple GPIO on/off LED driver. |
| `single_led.*` | Single-color LED with blink pattern support. |
| `circular_strip.*` | WS2812 addressable LED ring driver (NeoPixel-style). |

### Communication Protocols (`main/protocols/`)

| File | Description |
|------|-------------|
| `protocol.h/.cc` | Base protocol class; defines `SendAudio()`, `SendJson()`, connection callbacks. |
| `mqtt_protocol.*` | MQTT transport; connects to broker, subscribes to command topics. |
| `websocket_protocol.*` | WebSocket transport; connects to xiaozhi server for AI voice streaming. |

### Board Support (`main/boards/`)

| File | Description |
|------|-------------|
| `common/board.*` | Base `Board` class all boards inherit; holds codec, display, LED, button pointers. |
| `common/wifi_board.*` | Extends `Board` with WiFi provisioning and STA/AP management. |
| `common/dual_network_board.*` | Extends `Board` for boards with both WiFi and 4G (ML307). |
| `common/ml307_board.*` | ML307 4G LTE modem board support. |
| `common/button.*` | Debounced button input; emits single-click, long-press, double-click events. |
| `common/backlight.*` | PWM backlight brightness control. |
| `common/camera.h` | Camera interface definition. |
| `common/esp32_camera.*` | ESP32 camera driver integration. |
| `common/knob.*` | Rotary encoder (quadrature) input handler. |
| `common/i2c_device.*` | Base class for I2C peripheral drivers; wraps read/write with address. |
| `common/adc_battery_monitor.*` | Battery level estimation via ADC voltage divider. |
| `common/axp2101.*` | AXP2101 PMIC driver: battery voltage, charge current, power rails. |
| `common/sy6970.*` | SY6970 USB charger IC driver. |
| `common/power_save_timer.*` | Inactivity timer that dims/sleeps display after a configurable idle period. |
| `common/sleep_timer.*` | Schedules entry into ESP32 deep sleep. |
| `common/system_reset.*` | Factory reset and watchdog-triggered reboot logic. |
| `common/press_to_talk_mcp_tool.*` | Exposes PTT button as an MCP tool callable by AI clients. |
| `common/afsk_demod.*` | AFSK audio demodulator (used for config delivery via audio tone). |
| `waveshare_esp32_s3_photo_painter/` | **Active board.** PhotoPainter-specific `Board` subclass: wires up ES8311 codec, e-paper display, AXP2101 PMIC, and board-specific GPIO. |

### OTA / MCP

Already listed in the Core table above (`ota.*`, `mcp_server.*`).

---

## 2. Custom Components (`components/`)

These are the project-specific components written for the PhotoPainter. They sit on top of the xiaozhi framework.

### `user_app_bsp/` — Application Glue

| File | Description |
|------|-------------|
| `user_app.cpp/.h` | Defines all global hardware singletons (`SDPort`, `ePaperDisplay`, `I2cBus`, `epaper_gui_semapHandle`). Runs LED and button FreeRTOS tasks. Calls `User_Mode_init()` to dispatch boot mode. |
| `mode_src/Mode_Selection.cpp` | Boot mode selection screen. BOOT button cycles 1→2→3, long press writes mode to NVS and restarts. |
| `mode_src/Basic_mode.cpp` | **Mode 1.** Scans `/sdcard/06_user_foundation_img/`, cycles images on the e-paper display, enters deep sleep between refreshes (timer + EXT1 wakeup). |
| `mode_src/Network_mode.cpp` | **Mode 2.** Starts HTTP server; AP mode (`esp_network` / `1234567890`, `192.168.4.1`) or STA mode (mDNS `esp32-s3-photopainter.local`). Accepts image uploads via browser. |
| `mode_src/xiaozhi_mode.cpp` | **Mode 3.** Implements three xiaozhi framework callbacks: `xiaozhi_init_received()` (fetch weather, draw e-paper), `xiaozhi_ai_Message()` (queue image generation), `xiaozhi_application_received()` (state changes). |

### `app_bsp/` — Application-Level Services

| File | Description |
|------|-------------|
| `ai_app.cpp/.h` | `BaseAIModel`: reads `/sdcard/ai_config.json`, POSTs prompt to Volcano Ark API (Doubao-Seedream-4.0), downloads returned JPG URL into PSRAM, dithers to 7 e-paper colors, saves BMP to `/sdcard/05_user_ai_img/`. |
| `imgdecode_app.cpp/.h` | `ImgDecodeDither`: decodes JPG/PNG/BMP to RGB888, scales to 800×480 (nearest-neighbor), applies Floyd-Steinberg dithering to 7-color e-paper palette, encodes result as BMP. |
| `server_app.cpp/.h` | HTTP server for Mode 2: manages AP/STA WiFi, hosts `/upload` multipart endpoint, registers mDNS service. |
| `weather_app.cpp/.h` | `WeatherPort`: parses weather JSON (city, temperature, condition), maps condition strings to SD card image filenames for e-paper display. |
| `client_app.c/.h` | Lightweight HTTP client (esp-http-client wrapper) used to fetch weather JSON from a remote API. |
| `jpg_src/image_io.*` | JPEG I/O helpers (file-backed source/destination for TJpgDec / libjpeg). Used internally by `ImgDecodeDither`. |
| `list_src/list.*` | Generic doubly-linked list; used by `CustomSDPort` to build directory scan results. |
| `list_src/list_iterator.c` | Iterator helpers for the linked list. |
| `list_src/list_node.c` | Node allocation and deallocation. |
| `json_inc/ArduinoJson.h` | Vendored header-only ArduinoJson library; used in `ai_app` and `weather_app` for JSON parsing. |

### `port_bsp/` — Hardware BSP

| File | Description |
|------|-------------|
| `display_bsp.cpp/.h` | `ePaperPort`: SPI e-paper driver (MOSI=11, SCL=10, DC=8, CS=9, RST=12, BUSY=13, 800×480). Key methods: `EPD_SDcardBmpShakingColor()`, `EPD_SDcardIMGShakingColor()`, `EPD_SDcardScaleIMGShakingColor()`, `EPD_Display()`. Always acquire `epaper_gui_semapHandle` before calling. |
| `sdcard_bsp.cpp/.h` | `CustomSDPort`: SDMMC 4-bit interface, mounts at `/sdcard`, `SDPort_ScanListDir()` builds a linked list of filenames in a directory. |
| `button_bsp.c/.h` | FreeRTOS event groups (`BootButtonGroups`, `GP4ButtonGroups`, `PWRButtonGroups`). Bit 0 = single click, bit 1 = long press, bit 2 = double click. |
| `i2c_bsp.cpp/.h` | `I2cMasterBus`: thin wrapper around `i2c_master` driver. SDA=48, SCL=47, port 0. |
| `i2c_equipment.cpp/.h` | `Shtc3Port`: SHTC3 temperature/humidity sensor driver (I2C addr `0x70`); reads T/H for Mode 3 ambient display. |
| `codec_bsp.cpp/.h` | `CodecPort`: initializes ES8311 DAC and ES7210 ADC via the `codec_board` component. |
| `led_bsp.c/.h` | Green LED (status) and Red LED (error) GPIO control with simple on/off/blink helpers. |
| `power_bsp.cpp/.h` | Wraps XPowers AXP2101 library: exposes battery voltage, percentage, charge state, and power-off. |
| `traverse_nvs.cpp/.h` | `TraverseNvs`: iterates NVS blobs to recover saved WiFi SSID/password (used by Network mode to show saved networks). |
| `src/fonts/fonts.h` | Font descriptor header for all embedded bitmap fonts. |
| `src/fonts/font14CN.c` | 14 pt Chinese bitmap font. |
| `src/fonts/font18CN.c` | 18 pt Chinese bitmap font. |
| `src/fonts/font22CN.c` | 22 pt Chinese bitmap font. |
| `src/fonts/font24.c` | 24 pt ASCII bitmap font. |
| `src/multi_button/multi_button.*` | Third-party multi-button debounce library used by `button_bsp`. |

### `codec_board/` — Waveshare Codec Board Component

| File | Description |
|------|-------------|
| `codec_board.c` | Top-level codec board init; detects connected codec IC and calls appropriate driver. |
| `codec_init.c` | Low-level ES8311/ES7210 register init sequences. |
| `cfg_parse.c` | Parses codec configuration from a descriptor structure. |
| `lcd_init.c` | LCD controller init (not used in e-paper mode; present for hardware compatibility). |
| `dummy_codec.*` | No-op codec implementation for boards without audio. |
| `drv/tca9554.*` | TCA9554 I²C GPIO expander driver (controls codec power and mux lines on the codec board). |
| `include/codec_board.h` | Public API: `codec_board_init()`, `codec_board_set_volume()`. |
| `include/codec_init.h` | Internal codec init API. |

### `pmicpower/` — XPowers AXP2101 Library

| File | Description |
|------|-------------|
| `power_bsp.cpp/.h` | Project-level wrapper: `PowerPort` class that calls XPowers to read battery and manage power rails. |
| `src/XPowersLib.h` | XPowers library main header; selects correct IC variant at compile time. |
| `src/XPowersLibInterface.cpp` | Shared I²C interface implementation used by all XPowers IC drivers. |
| `src/REG/AXP2101Constants.h` | **Active.** AXP2101 register address map. |
| `src/REG/AXP192Constants.h` | AXP192 register map (not used on PhotoPainter). |
| `src/REG/AXP202Constants.h` | AXP202 register map (not used on PhotoPainter). |
| `src/REG/AXP216Constants.h` | AXP216 register map (not used on PhotoPainter). |
| `src/REG/BQ25896Constants.h` | BQ25896 charger IC constants (not used on PhotoPainter). |
| `src/REG/SY6970Constants.h` | SY6970 charger IC constants (not used on PhotoPainter). |
| `src/REG/HUSB238Constants.h` | HUSB238 USB PD controller constants (not used on PhotoPainter). |
| `src/REG/GeneralPPMConstants.h` | Generic power path management constants shared across ICs. |
