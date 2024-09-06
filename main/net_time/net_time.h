
#ifndef _NET_TIME_H
#define _NET_TIME_H

#include "esp_http_client.h"

struct ntime_st {
	int16_t year;
	int16_t mon;
	int16_t day;
	int16_t week;
	int16_t hour;
	int16_t min;
	int16_t sec;
};

int app_wifi_time_init(char *ssid, char *password);
void app_time_task_init(void);

#endif

