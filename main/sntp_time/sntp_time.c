#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

#include "wificon.h"
#include "sntp_time.h"

#define TAG "sntp_time"

#define SNTP_SERVER "ntp.aliyun.com"

sntp_time sn_time;

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void sntp_update_task(void *pvParameters)
{
    char strftime_buf[64];
    int ret;

    time_t now = 0;
    struct tm timeinfo;

    sn_time.sem = xSemaphoreCreateBinary();

    ret = wificon.init_sta("Xiaomi_2A40", "fc159357123");//CONFIG_WIFICON_REMOTE_AP_SSID, CONFIG_WIFICON_REMOTE_AP_PASSWD);
    if (ret == ESP_OK) {
        if (wificon.is_connected) {
            esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
            config.sync_cb = time_sync_notification_cb;
            esp_netif_sntp_init(&config);

            int retry = 0;
            const int retry_count = 15;
            while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
                ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to connect to AP");
        wificon.deinit();
    }

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();

    int delaytime = 0;
    while (1) {
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

        sn_time.year = timeinfo.tm_year;
        sn_time.mon = timeinfo.tm_mon + 1;
        sn_time.day = timeinfo.tm_mday;
        sn_time.hour = timeinfo.tm_hour;
        sn_time.min = timeinfo.tm_min;
        sn_time.wday = timeinfo.tm_wday;

        xSemaphoreGive(sn_time.sem);

        ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
        delaytime = 60 - timeinfo.tm_sec;
        vTaskDelay(delaytime * 1000 / portTICK_PERIOD_MS);
    }
}

void sntp_Init(void)
{
    memset(&sn_time, 0, sizeof(sntp_time));

    xTaskCreate(sntp_update_task, "sntp_update_task", 4096, NULL, 4, NULL);
}
