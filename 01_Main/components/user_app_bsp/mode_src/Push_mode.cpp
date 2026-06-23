#include <stdio.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <nvs_flash.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include "display_bsp.h"
#include "server_app.h"
#include "button_bsp.h"
#include "user_app.h"
#include "traverse_nvs.h"

#define TAG "PushMode"
#define ext_wakeup_pin_1 GPIO_NUM_0
#define ext_wakeup_pin_3 GPIO_NUM_4

// Survive deep sleep so interval can be tuned via NVS in future
static RTC_DATA_ATTR int push_sleep_seconds  = 30 * 60; // 30-minute poll interval
static RTC_DATA_ATTR int push_window_seconds = 60;      // 60-second listen window

static bool server_started = false;

static void enter_deep_sleep(void) {
    const uint64_t mask1 = 1ULL << ext_wakeup_pin_1;
    const uint64_t mask3 = 1ULL << ext_wakeup_pin_3;
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(mask1 | mask3, ESP_EXT1_WAKEUP_ANY_LOW));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(ext_wakeup_pin_1));
    ESP_ERROR_CHECK(rtc_gpio_pullup_en(ext_wakeup_pin_1));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(ext_wakeup_pin_3));
    ESP_ERROR_CHECK(rtc_gpio_pullup_en(ext_wakeup_pin_3));
    esp_sleep_enable_timer_wakeup((uint64_t)push_sleep_seconds * 1000000ULL);
    if (server_started) {
        ServerPort_SetNetworkSleep();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_deep_sleep_start();
}

static void push_network_Task(void *arg) {
    ePaperDisplay.EPD_Init();
    TickType_t start_tick = xTaskGetTickCount();

    for (;;) {
        EventBits_t even = xEventGroupWaitBits(ServerPortGroups, set_bit_all, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));

        if (get_bit_button(even, 2)) {
            if (pdTRUE == xSemaphoreTake(epaper_gui_semapHandle, 2000)) {
                xEventGroupSetBits(Green_led_Mode_queue, set_bit_button(6));
                Green_led_arg = 1;
                ePaperDisplay.EPD_SDcardBmpShakingColor("/sdcard/02_sys_ap_img/user_send.bmp", 0, 0);
                ePaperDisplay.EPD_Display();
                xSemaphoreGive(epaper_gui_semapHandle);
                Green_led_arg = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            enter_deep_sleep();
        }

        uint32_t elapsed_s = (xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS / 1000;
        if (elapsed_s >= (uint32_t)push_window_seconds) {
            ESP_LOGW(TAG, "Listen window expired — returning to sleep");
            enter_deep_sleep();
        }
    }
}

static void push_boot_button_Task(void *arg) {
    for (;;) {
        EventBits_t even = xEventGroupWaitBits(BootButtonGroups, GroupBit1, pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
        if (even & GroupBit1) {
            ESP_LOGW(TAG, "Long press — entering deep sleep");
            enter_deep_sleep();
        }
    }
}

void User_Push_mode_app_init(void) {
    TraverseNvs *nvs_viewer = new TraverseNvs();
    wifi_credential_t creden = nvs_viewer->Get_WifiCredentialFromNVS();

    if (0 == creden.is_valid) {
        ESP_LOGE(TAG, "No WiFi credentials in NVS — sleeping");
        enter_deep_sleep();
        return;
    }

    uint8_t res = ServerPort_NetworkSTAInit(creden);
    if (0 == res) {
        ESP_LOGE(TAG, "WiFi connection failed — sleeping");
        enter_deep_sleep();
        return;
    }

    Mdns_init_config();
    ServerPort_init(SDPort);
    server_started = true;

    ESP_LOGI(TAG, "Listening for image push (window: %ds)", push_window_seconds);
    xEventGroupSetBits(Red_led_Mode_queue, set_bit_button(0));
    xTaskCreate(push_network_Task, "push_network_Task", 6 * 1024, NULL, 2, NULL);
    xTaskCreate(push_boot_button_Task, "push_boot_button_Task", 4 * 1024, NULL, 2, NULL);
}
