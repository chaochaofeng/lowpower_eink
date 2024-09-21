#ifndef _AHT20_H
#define _AHT20_H

typedef struct {
    uint8_t status;
    uint8_t humidity;
    uint8_t temperature;
} aht20_data;

extern aht20_data aht_data;

void AHT20_Init(void);
void aht20_update_data(void);

#endif