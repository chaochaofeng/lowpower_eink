#ifndef _APP_STORAAGE_H
#define _APP_STORAAGE_H

void wifi_info_set(char* ssid, char* password);
int wifi_info_get(char *ssid, char *password);
void wifi_clear_info(void);
#endif