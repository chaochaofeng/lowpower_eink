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
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"

#include "app_storage.h"
#include "web_server.h"
#include "battery.h"
#include "aht20.h"
#include "disp_drv.h"
#include "sntp_time.h"
#include "wificon.h"
#include "u8g2.h"
#include "u8g2_d_epd2in66.h"

static const char *TAG = "app_main";

u8g2_t u8g2;

extern const uint8_t montmedium_font_82x[] U8G2_FONT_SECTION("montmedium_font_82x");
extern const uint8_t myicon_font24[] U8G2_FONT_SECTION("myicon_font24");

static void display_bg_char(int x, int y, int inv_x, int inv_y, int r,
                            const uint8_t *font, char *str, int char_num_max)
{
    int box_x = x, box_y = y, box_w, box_h;
    int str_x, str_y;
    int char_w;

    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);
	u8g2_SetFont(&u8g2, font);

    box_w = u8g2_GetMaxCharWidth(&u8g2) * char_num_max + inv_x * 2;
    box_h = u8g2_GetMaxCharHeight(&u8g2) + inv_y * 2;

    printf("box w:%d h:%d str w:%d h:%d\n", box_w, box_h, u8g2_GetMaxCharWidth(&u8g2), u8g2_GetMaxCharHeight(&u8g2));

	u8g2_DrawRBox(&u8g2, box_x, box_y, box_w, box_h, r);

    u8g2_SetFontMode(&u8g2, 1);
    u8g2_SetDrawColor(&u8g2, 0);

    char_w = u8g2_GetStrWidth(&u8g2, str);

    str_x = box_x + (box_w - char_w) / 2;
    str_y = box_y + inv_y + u8g2_GetAscent(&u8g2);

    u8g2_DrawUTF8(&u8g2, str_x, str_y, str);
}


static void display_time(int hour, int min)
{
    char buf[3];

    sprintf(buf, "%d", hour);
    display_bg_char(20, 30, 5, 10, 8, montmedium_font_82x, buf, 2);

    sprintf(buf, "%02d", min);
    display_bg_char(20 + 118 + 20, 30, 5, 10, 8, montmedium_font_82x, buf, 2);
}

void app_main(void)
{
    esp_err_t ret = 0;

	ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    //lvgl_spi_epd_init();
    //lv_port_indev_init();
    //ui_init();

    sntp_Init();

    battery_init();

    AHT20_Init();

    u8g2_SetupEpd2in66drv(&u8g2, U8G2_R2);
	u8x8_InitDisplay(u8g2_GetU8x8(&u8g2));
	u8x8_SetPowerSave(u8g2_GetU8x8(&u8g2), 0);
    u8g2_ClearBuffer(&u8g2);

    while (1) {
        ret = xSemaphoreTake(sn_time.sem, portMAX_DELAY);
        if (ret == pdTRUE) {
            
            display_time(sn_time.hour, sn_time.min);

            u8g2_NextPage(&u8g2);
#if 0
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
#endif
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "init done");
}
