#ifndef _WEB_SERVER_H
#define _WEB_SERVER_H

#include <esp_system.h>
#include <esp_http_server.h>

int esp_web_server_init();
void esp_web_server_stop();

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);
int wifi_init_softsta(char *ssid, char *password);

char *get_wifi_sta_ssid(void);
char *get_wifi_sta_password(void);
char *get_wifi_ap_ssid(void);
char *get_wifi_ap_pass(void);

#endif

