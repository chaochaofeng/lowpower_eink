#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_log.h"
#include "disp_drv.h"

#define PIN_NUM_RST		GPIO_NUM_9
#define PIN_NUM_MOSI	GPIO_NUM_7
#define PIN_NUM_CLK		GPIO_NUM_6
#define PIN_NUM_CS		GPIO_NUM_10
#define PIN_NUM_DC		GPIO_NUM_8
#define PIN_NUM_BUSY	GPIO_NUM_2

#define EPD_296_152_HIGHT 296
#define EPD_296_152_WIDTH 152

extern void epaper_spi_init(spi_host_device_t host_id);
extern void epaper_spi_send_cmd(uint8_t cmd);
extern void epaper_spi_send_data(uint8_t data);
extern void spi_send_data(const uint8_t *linedata, uint32_t nbyte);

#define EPD_2IN66_SendCommand epaper_spi_send_cmd
#define EPD_2IN66_SendData epaper_spi_send_data

#define mdelay(ms) vTaskDelay(ms / portTICK_PERIOD_MS)
#define delay_ms(ms) mdelay(ms)

const unsigned char WF_PARTIAL[159] ={
0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x40,0x40,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0A,0x00,0x00,0x00,0x00,0x00,0x02,0x01,0x00,0x00,
0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x22,0x22,0x22,0x22,0x22,0x22,
0x00,0x00,0x00,0x22,0x17,0x41,0xB0,0x32,0x36,
};

/******************************partial screen update LUT*********************************/
const unsigned char lut_20_vcom0_partial[] =
    {
        0x00,
        0x19,
        0x01,
        0x00,
        0x00,
        0x01,

};

const unsigned char lut_21_ww_partial[] =
    {
        // 10 w
        0x00,
        0x19,
        0x01,
        0x00,
        0x00,
        0x01,

};

const unsigned char lut_22_bw_partial[] =
    {
        // 10 w
        0x80,
        0x19,
        0x01,
        0x00,
        0x00,
        0x01,

};

const unsigned char lut_23_wb_partial[] =
    {
        // 01 b
        0x40,
        0x19,
        0x01,
        0x00,
        0x00,
        0x01,

};

const unsigned char lut_24_bb_partial[] =
    {
        // 01 b
        0x00,
        0x19,
        0x01,
        0x00,
        0x00,
        0x01,

};


const unsigned char lut_20_vcom0_full[] =
    {
        0x00, 0x08, 0x00, 0x00, 0x00, 0x02,
        0x60, 0x28, 0x28, 0x00, 0x00, 0x01,
        0x00, 0x14, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x12, 0x12, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00};

const unsigned char lut_21_ww_full[] =
    {
        0x40, 0x08, 0x00, 0x00, 0x00, 0x02,
        0x90, 0x28, 0x28, 0x00, 0x00, 0x01,
        0x40, 0x14, 0x00, 0x00, 0x00, 0x01,
        0xA0, 0x12, 0x12, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const unsigned char lut_22_bw_full[] =
    {
        0x40, 0x08, 0x00, 0x00, 0x00, 0x02,
        0x90, 0x28, 0x28, 0x00, 0x00, 0x01,
        0x40, 0x14, 0x00, 0x00, 0x00, 0x01,
        0xA0, 0x12, 0x12, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const unsigned char lut_23_wb_full[] =
    {
        0x80, 0x08, 0x00, 0x00, 0x00, 0x02,
        0x90, 0x28, 0x28, 0x00, 0x00, 0x01,
        0x80, 0x14, 0x00, 0x00, 0x00, 0x01,
        0x50, 0x12, 0x12, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const unsigned char lut_24_bb_full[] =
    {
        0x80, 0x08, 0x00, 0x00, 0x00, 0x02,
        0x90, 0x28, 0x28, 0x00, 0x00, 0x01,
        0x80, 0x14, 0x00, 0x00, 0x00, 0x01,
        0x50, 0x12, 0x12, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


static void Epaper_gpio_Init(void)
{
	gpio_reset_pin(PIN_NUM_DC);
			/* Set the GPIO as a push/pull output */
	gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_NUM_DC, 0);

	gpio_reset_pin(PIN_NUM_CS);
			/* Set the GPIO as a push/pull output */
	gpio_set_direction(PIN_NUM_CS, GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_NUM_CS, 0);

	gpio_reset_pin(PIN_NUM_RST);
		/* Set the GPIO as a push/pull output */
	gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_NUM_RST, 1);

	gpio_set_direction(PIN_NUM_BUSY, GPIO_MODE_INPUT);
}

static void epaper_reset(void)
{
	gpio_set_level(PIN_NUM_RST, 1);
	delay_ms(200);
	gpio_set_level(PIN_NUM_RST, 0);
	delay_ms(50);
	gpio_set_level(PIN_NUM_RST, 1);
	delay_ms(50);
}

static void epaper_checkbusy(void)
{
	uint8_t retry_cont = 0;
	while(gpio_get_level(PIN_NUM_BUSY) == 0) {
		delay_ms(100);
		retry_cont++;
		if (retry_cont > 50) {
			printf("check busy timeout\n");
			break;
		}
	}

	printf("busy release\n");
}

static void EPD_2IN66_SetLUA(const uint8_t *data, uint16_t n)
{
    uint16_t count;
    for (count = 0; count < n; count++)
    {
        EPD_2IN66_SendData(*(data + count));
    }
}

void epd_2in66_dev_init(unsigned int width, unsigned int height)
{
	epaper_reset();

    epaper_checkbusy();

#if 0
	EPD_2IN66_SendCommand(0x00);
	EPD_2IN66_SendData(0x0e);
	epaper_checkbusy();

	EPD_2IN66_SendCommand(0x70);
	EPD_2IN66_SendData(0x01);
	EPD_2IN66_SendData(0xaa);

	// EPD_2IN66_SendCommand(0x00);
	// EPD_2IN66_SendData(0xcf);
	// EPD_2IN66_SendData(0x8d);
	 EPD_2IN66_SendCommand(0x00); // panel setting
	 EPD_2IN66_SendData(0xbf);	  // LUT from REG 128x296  

	 EPD_2IN66_SendData(0x0d);	  // VCOM to 0V fast
	 EPD_2IN66_SendCommand(0x30); // PLL setting

	 EPD_2IN66_SendCommand(0x61); // resolution setting
	 EPD_2IN66_SendData(width);
	 EPD_2IN66_SendData(height / 256);
	 EPD_2IN66_SendData(height % 256);

	 EPD_2IN66_SendCommand(0x82); // vcom_DC setting
	 EPD_2IN66_SendData(0x1c);	  // -0.1 + 28 * -0.05 = -1.5V test, better
	 
	 EPD_2IN66_SendCommand(0x50); // VCOM AND DATA INTERVAL SETTING
	 EPD_2IN66_SendData(0x17);	  // WBmode:VBDF 17|D7 VBDW 97 VBDB 57	 WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7

	 EPD_2IN66_SendCommand(0x20);
	 EPD_2IN66_SetLUA(lut_20_vcom0_full, sizeof(lut_20_vcom0_full));
	 EPD_2IN66_SendCommand(0x21);
	 EPD_2IN66_SetLUA(lut_21_ww_full, sizeof(lut_21_ww_full));
	 EPD_2IN66_SendCommand(0x22);
	 EPD_2IN66_SetLUA(lut_22_bw_full, sizeof(lut_22_bw_full));
	 EPD_2IN66_SendCommand(0x23);
	 EPD_2IN66_SetLUA(lut_23_wb_full, sizeof(lut_23_wb_full));
	 EPD_2IN66_SendCommand(0x24);
	 EPD_2IN66_SetLUA(lut_24_bb_full, sizeof(lut_24_bb_full));

	 EPD_2IN66_SendCommand(0x04); // POWER ON
	 epaper_checkbusy();
#else
	EPD_2IN66_SendCommand(0x01); // POWER SETTING
	 EPD_2IN66_SendData(0x03);
	 EPD_2IN66_SendData(0x00);
	 EPD_2IN66_SendData(0x2b);
	 EPD_2IN66_SendData(0x2b);
	 EPD_2IN66_SendData(0x03);
	 EPD_2IN66_SendCommand(0x06); // boost soft start
	 EPD_2IN66_SendData(0x17);	  // A
	 EPD_2IN66_SendData(0x17);	  // B
	 EPD_2IN66_SendData(0x17);	  // C
	
	 EPD_2IN66_SendCommand(0x00); // panel setting
	 EPD_2IN66_SendData(0xbf);	  // LUT from REG 128x296  

	 EPD_2IN66_SendData(0x0d);	  // VCOM to 0V fast
	 EPD_2IN66_SendCommand(0x30); // PLL setting
	 EPD_2IN66_SendData(0x3a);	  // 3a 100HZ	29 150Hz 39 200HZ 31 171HZ
	 EPD_2IN66_SendCommand(0x61); // resolution setting
	 EPD_2IN66_SendData(width);
	 EPD_2IN66_SendData(height / 256);
	 EPD_2IN66_SendData(height % 256);
	 EPD_2IN66_SendCommand(0x82); // vcom_DC setting

	 EPD_2IN66_SendData(0x1c);	  // -0.1 + 28 * -0.05 = -1.5V test, better
	 
	 EPD_2IN66_SendCommand(0x50); // VCOM AND DATA INTERVAL SETTING
	 EPD_2IN66_SendData(0x17);	  // WBmode:VBDF 17|D7 VBDW 97 VBDB 57	 WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7

	 EPD_2IN66_SendCommand(0x20);
	 EPD_2IN66_SetLUA(lut_20_vcom0_full, sizeof(lut_20_vcom0_full));
	 EPD_2IN66_SendCommand(0x21);
	 EPD_2IN66_SetLUA(lut_21_ww_full, sizeof(lut_21_ww_full));
	 EPD_2IN66_SendCommand(0x22);
	 EPD_2IN66_SetLUA(lut_22_bw_full, sizeof(lut_22_bw_full));
	 EPD_2IN66_SendCommand(0x23);
	 EPD_2IN66_SetLUA(lut_23_wb_full, sizeof(lut_23_wb_full));
	 EPD_2IN66_SendCommand(0x24);
	 EPD_2IN66_SetLUA(lut_24_bb_full, sizeof(lut_24_bb_full));
	 EPD_2IN66_SendCommand(0x04); // POWER ON
	 epaper_checkbusy();
#endif
	//delay_ms(1000);
}

static void EPD_2IN66_SetLUA_P(void)
{
    uint16_t count;
    EPD_2IN66_SendCommand(0x32);
    for(count=0;count<153;count++){
        EPD_2IN66_SendData(WF_PARTIAL[count]);
    }    
    epaper_checkbusy();
}

static void _writeDataPGM(const uint8_t *data, uint16_t n, int16_t fill_with_zeroes)
{

    for (uint16_t i = 0; i < n; i++)
    {
        EPD_2IN66_SendData(*data++);
    }
    while (fill_with_zeroes > 0)
    {
        EPD_2IN66_SendData(0x00);
        fill_with_zeroes--;
    }
}

void EPD_2IN66_Init_Partial(unsigned int width, unsigned int height)
{
    epd_2in66_dev_init(width, height);
#if 1
    EPD_2IN66_SendCommand(0x20);
    _writeDataPGM(lut_20_vcom0_partial, sizeof(lut_20_vcom0_partial), 44 - sizeof(lut_20_vcom0_partial));
    EPD_2IN66_SendCommand(0x21);
    _writeDataPGM(lut_21_ww_partial, sizeof(lut_21_ww_partial), 42 - sizeof(lut_21_ww_partial));
    EPD_2IN66_SendCommand(0x22);
    _writeDataPGM(lut_22_bw_partial, sizeof(lut_22_bw_partial), 42 - sizeof(lut_22_bw_partial));
    EPD_2IN66_SendCommand(0x23);
    _writeDataPGM(lut_23_wb_partial, sizeof(lut_23_wb_partial), 42 - sizeof(lut_23_wb_partial));
    EPD_2IN66_SendCommand(0x24);
    _writeDataPGM(lut_24_bb_partial, sizeof(lut_24_bb_partial), 42 - sizeof(lut_24_bb_partial));
#endif
    EPD_2IN66_SendCommand(0x04); // POWER ON
    epaper_checkbusy();
    EPD_2IN66_SendCommand(0x91); // This command makes the display enter partial mode
    EPD_2IN66_SendCommand(0x90); // resolution setting
    EPD_2IN66_SendData(0);       // x-start
    EPD_2IN66_SendData(152 - 1); // x-end

    EPD_2IN66_SendData(0);
    EPD_2IN66_SendData(0); // y-start

    EPD_2IN66_SendData(296 / 256);
    EPD_2IN66_SendData(296 % 256 - 1); // y-end
    EPD_2IN66_SendData(0x28);

}

static void epaper_turn_ondisplay(uint8_t is_all)
{
	printf("turn_ondisplay all:%d\n", is_all);
	if (!is_all)
		epaper_spi_send_cmd(0x92);

	epaper_spi_send_cmd(0x12);

	epaper_checkbusy();
}

void epd_2in66_clear(uint16_t width, uint16_t height)
{
	uint16_t i = 0;

    EPD_2IN66_SendCommand(0x10); // Transfer old data
    for (i = 0; i < (width / 8) * height; i++)
    {
        EPD_2IN66_SendData(0xFF);
    }
    EPD_2IN66_SendCommand(0x13); // Transfer new data
    for (i = 0; i < (width / 8) * height; i++)
    {
        EPD_2IN66_SendData(0xFF); // Transfer the actual displayed data
    }

    epaper_turn_ondisplay(1);
}

void EPD_2IN66_Sleep(void)
{
	EPD_2IN66_SendCommand(0X02);
	epaper_checkbusy();
	
	gpio_set_level(PIN_NUM_DC, 0);
	gpio_set_level(PIN_NUM_CS, 0);
	gpio_set_level(PIN_NUM_RST, 0);
    //EPD_2IN66_ReadBusy();
}

static int epaper_2in66_init(struct disp_dev_st *dev)
{
	struct disp_drv_st *drv = NULL;

	if (!dev) {
		printf("epaper_2in66 init err\n");
		return -1;
	}

	epaper_spi_init(SPI2_HOST);
	Epaper_gpio_Init();

	drv = dev->drv;

	drv->priv_data[EPD_BLACK] 	= dev->buff[EPD_BLACK];
	drv->priv_data[EPD_RED] 	= dev->buff[EPD_RED];
	memset(drv->priv_data[EPD_BLACK], 0xff, dev->width * dev->height / 8);
	memset(drv->priv_data[EPD_RED], 0xff, dev->width * dev->height / 8);

	//epd_2in66_dev_init(dev->width, dev->height);

	//epd_2in66_clear(dev->width, dev->height);

	return 0;
}

static void epaper_2in66_uninit(struct disp_dev_st *dev)
{
}
static void epaper_2in66_flush(struct disp_dev_st *dev,
	unsigned int x, unsigned int y,
	unsigned int w, unsigned int h, uint8_t *fb)
{
	uint8_t *tmp = fb;
	uint8_t (*disp_buf)[dev->width / 8];
	struct disp_drv_st *drv = NULL;

	if (!dev) {
		printf("epaper_2in66 flush err\n");
		return;
	}

	drv = dev->drv;

	if (dev->buf_index == EPD_BACK)
		disp_buf = drv->priv_data[EPD_BACK];
	else
		disp_buf = drv->priv_data[EPD_FRONT];

	for (uint32_t y_index = y; y_index < y + h; y_index++) {
		for (uint32_t x_index = x / 8; x_index < (x+w) / 8; x_index++) {
			if (y_index > dev->height - 1)
				y_index = dev->height - 1;

			if (x_index > dev->width / 8 -1)
				x_index = dev->width / 8 -1;

			disp_buf[y_index][x_index] = *tmp;
			tmp++;
		}
	}
}

static void epaper_2in66_draw_pixel(struct disp_dev_st *dev, unsigned int x, unsigned int y, uint8_t color)
{
	uint8_t (*disp_buf)[dev->width / 8];
	uint8_t tmp = 0;
	uint32_t x_index = 0, shift_bit = 0;
	struct disp_drv_st *drv = NULL;

	if (!dev) {
		printf("epaper_2in66 draw_pixel err\n");
		return;
	}
	
	drv = dev->drv;
	disp_buf = drv->priv_data[dev->draw_color];

	x_index = x / 8;
	shift_bit = x % 8;

	tmp = disp_buf[y][x_index];
	if (color)
		tmp |= 1 << (7 - shift_bit);
	else
		tmp &= ~(1 << (7 - shift_bit));

	disp_buf[y][x_index] = tmp;
}

static void epaper_2in66_clear_screen(struct disp_dev_st *dev)
{
	if (!dev) {
		printf("epaper_2in66 clear_screen err\n");
		return;
	}

	epd_2in66_clear(dev->width, dev->height);
}

void epaper_2in66_clear_area_screen(struct disp_dev_st *dev,
		unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	uint8_t (*disp_buf)[dev->width / 8];
	struct disp_drv_st *drv = NULL;

	if (!dev) {
		printf("epaper_2in66 clear_area_screen err\n");
		return;
	}

	drv = dev->drv;
	disp_buf = drv->priv_data[dev->draw_color];

	for (uint32_t y_index = y; y_index < y + h; y_index++) {
		for (uint32_t x_index = x / 8; x_index < (x+w) / 8; x_index++) {
			disp_buf[y_index][x_index] = 0xFF;
		}
	}
}

void EPD_2in66_Display_img(const uint8_t* img, uint16_t width, uint16_t height)
{
	uint16_t Width, Height;
	Width = (width % 8 == 0)? (width / 8 ): (width / 8 + 1);
	Height = height;

	EPD_2IN66_SendCommand(0x10);
	for (int j = 0; j < Height; j++)
	{
		for (int i = 0; i < Width; i++)
		{
			EPD_2IN66_SendData(0xFF);
		}
	}
	EPD_2IN66_SendCommand(0x13);
#if 1
	spi_send_data(img, Height*Width);
#else
	for (uint16_t j = 0; j < Height; j++) {
		for (uint16_t i = 0; i < Width; i++) {
			EPD_2IN13BC_SendData(img[j*Width + i]);
		}
	}
#endif
}

void EPD_2in66_Display_part_img(const uint8_t* old_img, const uint8_t* new_img, uint16_t width, uint16_t height)
{
	uint16_t Width, Height;
	Width = (width % 8 == 0)? (width / 8 ): (width / 8 + 1);
	Height = height;

	EPD_2IN66_SendCommand(0x10);

	spi_send_data(old_img, Height*Width);

	EPD_2IN66_SendCommand(0x13);
#if 1
	spi_send_data(new_img, Height*Width);
#else
	for (uint16_t j = 0; j < Height; j++) {
		for (uint16_t i = 0; i < Width; i++) {
			EPD_2IN13BC_SendData(img[j*Width + i]);
		}
	}
#endif
}


static uint8_t epd_2in66_black_buf1[EPD_296_152_HIGHT][EPD_296_152_WIDTH / 8];
static uint8_t epd_2in66_black_buf2[EPD_296_152_HIGHT][EPD_296_152_WIDTH / 8];

static void epaper_2in66_disp_on(struct disp_dev_st *dev)
{
	struct disp_drv_st *drv = NULL;

	if (!dev) {
		printf("epaper_2in66 disp_on err\n");
		return;
	}

	drv = dev->drv;

	if (dev->flash_all) {
		epd_2in66_dev_init(dev->width, dev->height);
		EPD_2in66_Display_img(drv->priv_data[EPD_FRONT], dev->width, dev->height);
	}
	else {
		EPD_2IN66_Init_Partial(dev->width, dev->height);
		EPD_2in66_Display_part_img(drv->priv_data[EPD_BACK],
			drv->priv_data[EPD_FRONT], dev->width, dev->height);
	}

	memcpy(drv->priv_data[EPD_BACK], drv->priv_data[EPD_FRONT], dev->width * dev->height / 8);
	epaper_turn_ondisplay(dev->flash_all);
	EPD_2IN66_Sleep();
}
static void epaper_2in66_disp_off(struct disp_dev_st *dev)
{
	EPD_2IN66_Sleep();
}

struct disp_drv_st epaper_2in66_drv = {
	.name = "epd_2in66",
	.init			= epaper_2in66_init,
	.uninit			= epaper_2in66_uninit,
	.flush			= epaper_2in66_flush,
	.draw_pixel		= epaper_2in66_draw_pixel,
	.clear_screen	= epaper_2in66_clear_screen,
	.clear_area_screen = epaper_2in66_clear_area_screen,
	.disp_on		= epaper_2in66_disp_on,
	.disp_off		= epaper_2in66_disp_off,
};

struct disp_dev_st epaper_2in66_dev = {
	.name	= "epd_2in66",
	.width	= EPD_296_152_WIDTH,
	.height	= EPD_296_152_HIGHT,
	.drv	= NULL,
	.buff[EPD_BLACK]	= epd_2in66_black_buf1,
	.buff[EPD_RED]	= epd_2in66_black_buf2,
};

