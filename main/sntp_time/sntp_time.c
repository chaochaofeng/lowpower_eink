#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

#include "wificon.h"
#include "sntp_time.h"
#include "app_storage.h"

#define TAG "sntp_time"

#define SNTP_SERVER "ntp.aliyun.com"

sntp_time sn_time;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "cal time success");
}

static int sntp_time_cal_start(void)
{
    int ret;
    int retry = 0;
    const int retry_count = 15;

    char ssid[64];
    char password[64];

    ret = wifi_info_get(ssid, password);
    if (ret != ESP_OK) {
        memcpy(ssid, CONFIG_WIFICON_REMOTE_AP_SSID, sizeof(CONFIG_WIFICON_REMOTE_AP_SSID));
        memcpy(password, CONFIG_WIFICON_REMOTE_AP_PASSWD, sizeof(CONFIG_WIFICON_REMOTE_AP_PASSWD));
    }

    ret = wificon.init_sta(ssid, password);//CONFIG_WIFICON_REMOTE_AP_SSID, CONFIG_WIFICON_REMOTE_AP_PASSWD);
    if (ret == ESP_OK) {
        if (wificon.is_connected) {
            esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
            config.sync_cb = time_sync_notification_cb;
            esp_netif_sntp_init(&config);

            while (esp_netif_sntp_sync_wait(1000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
                ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to connect to AP");

        wificon.deinit();
    }

    if (ret != ESP_OK || retry >= retry_count) {
        ESP_LOGE(TAG, "Failed to get time");
        sn_time.cal = 1;
    } else {
        sn_time.cal = 0;
    }

    return sn_time.cal;
}

static void sntp_time_cal_stop(void)
{
    esp_netif_sntp_deinit();

    wificon.deinit();
}

int sntp_cali_time(void)
{
    int ret = 0;
    ret = sntp_time_cal_start();

    sntp_time_cal_stop();

    return ret;
}

void sntp_update_time(void)
{
    time_t now = 0;
    struct tm timeinfo;
    char strftime_buf[64];
    static bool first_time = true;

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    sn_time.year = timeinfo.tm_year;
    sn_time.mon = timeinfo.tm_mon + 1;
    sn_time.day = timeinfo.tm_mday;
    sn_time.hour = timeinfo.tm_hour;
    sn_time.min = timeinfo.tm_min;
    sn_time.wday = timeinfo.tm_wday;
    sn_time.sec = timeinfo.tm_sec;

    if ((sn_time.min == 0 || sn_time.cal) && first_time) {
        first_time = false;

        sntp_cali_time();
        sntp_update_time();
    }

    first_time = true;
    sn_time.status = 1;
}

void sntp_Init(void)
{
    memset(&sn_time, 0, sizeof(sntp_time));

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
    sntp_update_time();

    // xTaskCreate(sntp_update_task, "sntp_update_task", 4096, NULL, 4, NULL);
}
