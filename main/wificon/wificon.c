#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"

#include "nvs_flash.h"

#include "wificon.h"

#define TAG "wificon"
#define TAG_AP "wificon-ap"
#define TAG_STA "wificon-sta"

// #define WIFICON_STA_SSID           CONFIG_WIFICON_REMOTE_AP_SSID
// #define WIFICON_STA_PASSWD         CONFIG_WIFICON_REMOTE_AP_PASSWD
#define WIFICON_MAXIMUM_RETRY      CONFIG_WIFICON_MAXIMUM_RETRY

// #define WIFICON_AP_SSID            CONFIG_WIFICON_AP_SSID
// #define WIFICON_AP_PASSWD          CONFIG_WIFICON_AP_PASSWD
#define WIFICON_MAX_STA_CONN          CONFIG_WIFICON_MAX_STA_CONN
#define WIFICON_CHANNEL               CONFIG_WIFICON_CHANNEL

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    struct wificon_st *wific = (struct wificon_st *) arg;
    int ret;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ret = esp_wifi_connect();
        ESP_LOGI(TAG_STA, "Station started ret=%d", ret);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));

        wific->is_connected = true;

        xEventGroupSetBits(wific->event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG_STA, "Wifi disconnect");
        wific->is_connected = false;
        xEventGroupSetBits(wific->event_group, WIFI_DISCONNECTED_BIT);
    }
}

static int init(struct wificon_st *wifi)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize event group */
    wifi->event_group = xEventGroupCreate();

    /* Register Event handler */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &wifi_event_handler,
                    wifi,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler,
                    wifi,
                    NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi->is_inited = true;
    return 0;
}

static int init_sta(const char *ssid, const char *passwd)
{
    struct wificon_st *wifi = &wificon;

    if (strlen(ssid) == 0)
        return -1;

    init(wifi);

    wifi->s_netif = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_sta_config = {
        .sta = {
            //.scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = WIFICON_MAXIMUM_RETRY,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "PASSWORD IDENTIFIER",
        },
    };

    memcpy(wifi->ssid, ssid, strlen(ssid));
    if (strlen(passwd) > 0)
        memcpy(wifi->passwd, passwd, strlen(passwd));

    memcpy(wifi_sta_config.sta.ssid, wifi->ssid, strlen(wifi->ssid));

    if (strlen(wifi->passwd) > 0)
        memcpy(wifi_sta_config.sta.password, wifi->passwd, strlen(wifi->passwd));
    else
        wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    
    ESP_LOGI(TAG_STA, "will connect SSID:%s password:%s", wifi_sta_config.sta.ssid, wifi_sta_config.sta.password);

    esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));

    wifi->mode = WIFI_MODE_STA;
    wifi->is_connected = false;

    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(wifi->event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_DISCONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_STA, "connected to ap SSID:%s password:%s",
                 wifi_sta_config.sta.ssid, wifi_sta_config.sta.password);
    } else if (bits & WIFI_FAIL_BIT || bits & WIFI_DISCONNECTED_BIT) {
        ESP_LOGI(TAG_STA, "Failed to connect to SSID:%s, password:%s",
                 wifi_sta_config.sta.ssid, wifi_sta_config.sta.password);
    } else {
        ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
        return -1;
    }

    return 0;
}

static int init_ap(const char *ssid, const char *passwd)
{
    struct wificon_st *wifi = &wificon;

    if (strlen(ssid) == 0)
        return -1;

    init(wifi);

    wifi->s_netif = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .channel = WIFICON_CHANNEL,
            .max_connection = WIFICON_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    memcpy(wifi_ap_config.ap.ssid, ssid, strlen(ssid));

    if (strlen(passwd) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        memcpy(wifi_ap_config.ap.password, passwd, strlen(passwd));
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    wifi->mode = WIFI_MODE_AP;

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ssid, passwd, WIFICON_CHANNEL);

    return 0;
}

static int scan(uint16_t *number, wifi_ap_record_t *ap_info)
{
    uint16_t ap_count = 0;

    esp_wifi_scan_start(NULL, true);

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    esp_wifi_scan_get_ap_num(&ap_count);
    esp_wifi_scan_get_ap_records(number, ap_info);

    esp_wifi_scan_stop();

    return ap_count;
}

static int connect(void)
{
    struct wificon_st *wc = &wificon;

    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed! ret:%x", ret);
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(wc->event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_DISCONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_STA, "connected to ap SSID:%s password:%s",
                 wc->ssid, wc->passwd);
    } else if (bits & WIFI_FAIL_BIT || bits & WIFI_DISCONNECTED_BIT) {
        ESP_LOGI(TAG_STA, "Failed to connect to SSID:%s, password:%s",
                 wc->ssid, wc->passwd);
        goto fail;
    } else {
        ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
        goto fail;
    }

    return 0;
fail:
    esp_wifi_disconnect();
    return -1;
}

static int disconnect(void)
{
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi disconnect failed! ret:%x", ret);
        return ret;
    }

    return 0;
}

static int stop(void)
{
    struct wificon_st *wc = &wificon;
    if (wc->is_inited == false)
        return 0;

    if (wc->is_connected) {
        esp_wifi_disconnect();
        EventBits_t bits = xEventGroupWaitBits(wc->event_group,
                                        WIFI_DISCONNECTED_BIT,
                                        pdFALSE,
                                        pdFALSE,
                                        portMAX_DELAY);
        if (bits & WIFI_DISCONNECTED_BIT) {
            ESP_LOGI(TAG_STA, "disconnect SSID:%s", wc->ssid);
            wc->is_connected = false;
        }
    }

    return esp_wifi_stop();
}

static void deinit(void)
{
    struct wificon_st *wifi = &wificon;

    if (wifi->is_connected) {
        esp_wifi_disconnect();
        EventBits_t bits = xEventGroupWaitBits(wifi->event_group,
                                        WIFI_DISCONNECTED_BIT,
                                        pdFALSE,
                                        pdFALSE,
                                        portMAX_DELAY);
        if (bits & WIFI_DISCONNECTED_BIT) {
            ESP_LOGI(TAG_STA, "disconnect SSID:%s", wifi->ssid);
            wifi->is_connected = false;
        }
    }

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
    esp_event_loop_delete_default();

    vEventGroupDelete(wifi->event_group);

    esp_wifi_stop();

    if (wifi->s_netif)
        esp_netif_destroy_default_wifi(wifi->s_netif);

    wifi->event_group = NULL;
    wifi->s_netif = NULL;
    wifi->is_inited = false;
}

struct wificon_st wificon = {
    .is_inited = false,
    .is_connected = false,
    .mode = WIFI_MODE_NULL,
    .event_group = NULL,
    .ssid[0] = '\0',
    .passwd[0] = '\0',
    .s_netif = NULL,

    .init_ap = init_ap,
    .init_sta = init_sta,
    .connect = connect,
    .disconnect = disconnect,
    .scan = scan,
    .stop = stop,
    .deinit = deinit,
};