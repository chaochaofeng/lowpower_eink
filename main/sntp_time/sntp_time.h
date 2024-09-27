#ifndef _SNTP_H
#define _SNTP_H

typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t mon;
    uint16_t year;
    uint8_t wday;
    bool cal;
    bool status;
} sntp_time;

extern sntp_time sn_time;

void sntp_Init(void);

void sntp_update_time(void);
int sntp_cali_time(void);
#endif
