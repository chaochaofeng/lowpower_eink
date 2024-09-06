#ifndef __EPAPER_H
#define __EPAPER_H

#ifndef uint64_t
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;
#endif

#define PIN_NUM_RST		GPIO_NUM_9
#define PIN_NUM_MOSI	GPIO_NUM_7
#define PIN_NUM_CLK		GPIO_NUM_6
#define PIN_NUM_CS		GPIO_NUM_10
#define PIN_NUM_DC		GPIO_NUM_8
#define PIN_NUM_BUSY	GPIO_NUM_2

//-----------------LCD�˿ڶ���---------------- 

// #define EPD_SCLK_Clr() gpio_set_level(PIN_NUM_CLK, 0) //SCL=SCLK
// #define EPD_SCLK_Set() gpio_set_level(PIN_NUM_CLK, 1)

// #define EPD_MOSI_Clr() gpio_set_level(PIN_NUM_MOSI, 0)//SDA=MOSI
// #define EPD_MOSI_Set() gpio_set_level(PIN_NUM_MOSI, 1)

// #define EPD_RES_Clr()  gpio_set_level(PIN_NUM_RST, 0)//RES
// #define EPD_RES_Set()  gpio_set_level(PIN_NUM_RST, 1)

#define EPD_DC_Clr()   gpio_set_level(PIN_NUM_DC, 0)//DC
#define EPD_DC_Set()   gpio_set_level(PIN_NUM_DC, 1)

#define EPD_CS_Clr()  gpio_set_level(PIN_NUM_CS, 0);//BLK
#define EPD_CS_Set()  gpio_set_level(PIN_NUM_CS, 1);

void epaper_spi_init(spi_host_device_t host_id);
void epaper_spi_send_cmd(uint8_t cmd);
void epaper_spi_send_data(uint8_t data);
void spi_send_data(const uint8_t *linedata, uint32_t nbyte);

#define EPD_2IN13BC_SendCommand epaper_spi_send_cmd
#define EPD_2IN13BC_SendData epaper_spi_send_data
#endif


