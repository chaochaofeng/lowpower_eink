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
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "ui.h"

#include "app_storage.h"
#include "web_server.h"
#include "battery.h"
#include "aht20.h"
#include "disp_drv.h"
#include "epd_lvgl_init.h"
#include "key_input_lvgl.h"
#include "sntp_time.h"
#include "wificon.h"

static const char *TAG = "app_main";

void app_main(void)
{
    esp_err_t ret = 0;

	ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    lvgl_spi_epd_init();
    lv_port_indev_init();
    ui_init();

    sntp_Init();

    battery_init();

    AHT20_Init();

    while (1) {
        ret = xSemaphoreTake(sn_time.sem, portMAX_DELAY);
        if (ret == pdTRUE) {
            lv_label_set_text_fmt(ui_hour, "%d", sn_time.hour);
            lv_label_set_text_fmt(ui_min, "%02d", sn_time.min);
            lv_label_set_text_fmt(ui_dataLabel, "%02d-%02d", sn_time.mon, sn_time.day);

            if (wificon.is_connected) {
                lv_obj_clear_flag(ui_wifi, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(ui_wifi, LV_OBJ_FLAG_HIDDEN);
            }

            if (batteryinfo.status) {
                battery_ui_update(batteryinfo.voltage,
                    batteryinfo.battery_exist, batteryinfo.charging);
            }

            if (aht_data.status) {
                aht20_update_ui(aht_data.temperature, aht_data.humidity);
            }
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "init done");
}
