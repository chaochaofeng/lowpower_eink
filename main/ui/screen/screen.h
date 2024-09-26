#ifndef _SCREEN_H
#define _SCREEN_H

#include "core/inc/base.h"

extern ug_base *mainScreen;
extern ug_base *ui_hour;
extern ug_base *ui_min;
extern ug_base *ui_date;
extern ug_base *ui_temp;
extern ug_base *ui_humi;

extern ug_base *ui_battery_charge;
extern ug_base *ui_battery;
extern ug_base *ui_wifi;

void ug_mainScreen_init(void);

#endif