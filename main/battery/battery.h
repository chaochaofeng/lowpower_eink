#ifndef _BATTERY_H
#define _BATTERY_H

#include "esp_adc/adc_oneshot.h"

typedef struct _battery_info {
    int voltage;
    int adc_raw;
    bool charging;
    bool battery_exist;
    int status;
} battery_info;

extern battery_info batteryinfo;

void battery_init(void);
#endif
