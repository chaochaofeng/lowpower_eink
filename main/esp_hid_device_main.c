/* This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this software is
   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "app_storage.h"
#include "web_server.h"
#include "battery.h"
#include "aht20.h"
#include "sntp_time.h"
#include "u8g2.h"
#include "u8g2_d_epd2in66.h"
#include "key.h"
#include "core/inc/base.h"
#include "screen/screen.h"

static const char *TAG = "app_main";

void deep_sleep_register_gpio_wakeup(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT(GPIO_NUM_1),
        .mode = GPIO_MODE_INPUT,
    };

    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(BIT(GPIO_NUM_1), ESP_GPIO_WAKEUP_GPIO_LOW));

    printf("Enabling GPIO wakeup on pins GPIO%d\n", GPIO_NUM_1);
}

static void deep_sleep_rtc_timer_wakeup(int sec)
{
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(sec * 1000000));
}

static void entry_deep_sleep(int sec)
{
    deep_sleep_rtc_timer_wakeup(sec);
    deep_sleep_register_gpio_wakeup();

    esp_deep_sleep_start();
}

void u8g2_init_epd2in66drv(void)
{
    u8g2_SetupEpd2in66drv(get_u8g2(), U8G2_R2);
	u8x8_InitDisplay(u8g2_GetU8x8(get_u8g2()));
	u8x8_SetPowerSave(u8g2_GetU8x8(get_u8g2()), 0);
    u8g2_ClearBuffer(get_u8g2());
}

void app_main(void)
{
    esp_err_t ret = 0;
    int wakeup_inv = 0;
    int delaytime = 0;

	ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    u8g2_init_epd2in66drv();
    ug_mainScreen_init();
    ug_base_flush(mainScreen);

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        ESP_LOGI(TAG, "GPIO wakeup");
        ug_input_proc(UG_KEY_ENTER);
    }

    battery_init();
    AHT20_Init();
    key_init();

    while (1) {
        sntp_Init();
        battery_update_data();
        aht20_update_data();

        ug_base_set_context_fmt(ui_hour, "%d", sn_time.hour);
        ug_base_set_context_fmt(ui_min, "%02d", sn_time.min);
        ug_base_set_context_fmt(ui_date, "%02d-%02d", sn_time.mon, sn_time.day);

        ug_base_set_context_fmt(ui_temp, "%d\xc2\xb0""C", aht_data.temperature);
        ug_base_set_context_fmt(ui_humi, "%d", aht_data.humidity);

        if (ug_get_curscreen() == mainScreen) {
            u8g2_ClearBuffer(get_u8g2());

            ug_base_flush(mainScreen);

            u8g2_NextPage(get_u8g2());
            
            sntp_update_time();

            if (sn_time.hour >= 23 || sn_time.hour < 6) {
                wakeup_inv = 5 * 60;    //5min wakeup
            } else {
                wakeup_inv = 1 * 60;    //1min wakeup
            }

            delaytime = wakeup_inv - sn_time.sec;
            entry_deep_sleep(delaytime);
        }

        delaytime = 60 - sn_time.sec;
        vTaskDelay(delaytime * 1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "init done");
}
