#ifndef _SNTP_H
#define _SNTP_H

typedef struct {
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t mon;
    uint16_t year;
    uint8_t wday;
    SemaphoreHandle_t sem;
} sntp_time;

extern sntp_time sn_time;

void sntp_Init(void);

#endif
