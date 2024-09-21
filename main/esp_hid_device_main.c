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

static void display_temp_hum(int temp, int humi)
{
    char buf[6];
    memset(buf, 0, sizeof(buf));

    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);

    sprintf(buf, "%d\xc2\xb0""C", temp);

    u8g2_SetFont(&u8g2, u8g2_font_logisoso24_tf);
    u8g2_DrawUTF8(&u8g2, 15, 145, buf);

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%d", humi);

    u8g2_DrawUTF8(&u8g2, 110, 145, buf);

    u8g2_SetFont(&u8g2, myicon_font24);
    u8g2_DrawGlyph(&u8g2, 78, 150, 17);
}

static void display_date(int mon, int day)
{
    char buf[6];

    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);

    sprintf(buf, "%02d-%02d", mon, day);

    u8g2_SetFont(&u8g2, u8g2_font_inb24_mf);
    u8g2_DrawUTF8(&u8g2, 180, 145, buf);
}

static void display_battery(int voltage, int charge)
{
    int encoder = 10;
    int start_x = 277;
    int inv_x   = 24;
    int cnt = 0;

    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);

    u8g2_SetFont(&u8g2, myicon_font24);

    if (charge)
        u8g2_DrawGlyph(&u8g2, 277, 26, 16);

    cnt++;

    if (voltage < 20)
        encoder += 0;
    else if (voltage < 35)
        encoder += 1;
    else if (voltage < 55)
        encoder += 2;
    else if (voltage < 70)
        encoder += 3;
    else if (voltage < 85)
        encoder += 4;
    else
        encoder += 5;

    u8g2_DrawGlyph(&u8g2, start_x - cnt * inv_x + 6, 26, encoder);
    cnt++;

    u8g2_DrawGlyph(&u8g2, start_x - cnt * inv_x, 26, 19);
}

void app_main(void)
{
    esp_err_t ret = 0;

	ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    sntp_Init();

    battery_init();

    AHT20_Init();

    u8g2_SetupEpd2in66drv(&u8g2, U8G2_R2);
	u8x8_InitDisplay(u8g2_GetU8x8(&u8g2));
	u8x8_SetPowerSave(u8g2_GetU8x8(&u8g2), 0);
    u8g2_ClearBuffer(&u8g2);

    int delaytime;
    while (1) {
        u8g2_ClearBuffer(&u8g2);

        sntp_update_time();
        battery_update_data();
        aht20_update_data();

        display_time(sn_time.hour, sn_time.min);

        display_date(sn_time.mon, sn_time.day);

        display_temp_hum(aht_data.temperature, aht_data.humidity);

        display_battery(batteryinfo.voltage, batteryinfo.charging);

        u8g2_NextPage(&u8g2);

        sntp_update_time();

        delaytime = 60 - sn_time.sec;
        vTaskDelay(delaytime * 1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "init done");
}
