#ifndef __WIFICON_H__
#define __WIFICON_H__

#include "esp_wifi.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_DISCONNECTED_BIT BIT2

struct wificon_st {
    bool is_inited;
    bool is_connected;
    wifi_mode_t mode;
    char ssid[64];
    char passwd[64];
    esp_netif_t *s_netif;
    EventGroupHandle_t event_group;

    int (*init_sta)(const char *ssid, const char *passwd);
    int (*init_ap)(const char *ssid, const char *passwd);
    int (*connect)(void);
    int (*disconnect)(void);
    int (*scan)(uint16_t *number, wifi_ap_record_t *ap_info);
    int (*stop)(void);
    void (*deinit)(void);
};

extern struct wificon_st wificon;

#endif
