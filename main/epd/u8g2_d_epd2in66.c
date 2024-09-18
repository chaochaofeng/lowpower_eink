#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "disp_drv.h"
#include "u8g2.h"

#define TAG "u8g2_epd"

struct disp_dev_st* g_disp_dev = NULL;
/*========================================================*/
/* framebuffer struct */
struct _u8x8_epd2in66_drv_struct
{
	u8x8_msg_cb u8x8_bitmap_display_old_cb;
	int fbfd;
	uint32_t width;
	uint32_t height;
	uint8_t *u8x8_buf;
	uint8_t *u8g2_buf;
	uint8_t *fbp;
    uint32_t bits_per_pixel;
	uint32_t active_color;
    uint32_t bg_color;
};

typedef struct _u8x8_epd2in66_drv_struct u8x8_epd2in66drv_t;
static u8x8_epd2in66drv_t u8x8_epd2in66drv;

static u8x8_display_info_t u8x8_epd2in66fb_info =
{
	/* chip_enable_level = */ 0,
	/* chip_disable_level = */ 1,

	/* post_chip_enable_wait_ns = */ 0,
	/* pre_chip_disable_wait_ns = */ 0,
	/* reset_pulse_width_ms = */ 0,
	/* post_reset_wait_ms = */ 0,
	/* sda_setup_time_ns = */ 0,
	/* sck_pulse_width_ns = */ 0,
	/* sck_clock_hz = */ 4000000UL,
	/* spi_mode = */ 1,
	/* i2c_bus_clock_100kHz = */ 0,
	/* data_setup_time_ns = */ 0,
	/* write_pulse_width_ns = */ 0,
	/* tile_width = */ 8,		/* dummy value */
	/* tile_hight = */ 4,		/* dummy value */
	/* default_x_offset = */ 0,
	/* flipmode_x_offset = */ 0,
	/* pixel_width = */ 64,		/* dummy value */
	/* pixel_height = */ 32		/* dummy value */
};

/*========================================================*/
/* framebuffer functions */

uint8_t u8x8_epd2in66drv_alloc(u8x8_epd2in66drv_t *fb)
{
	size_t tile_width;
	size_t tile_height;
	size_t screensize = 0;

	tile_width = (fb->width+7)/8;
	tile_height = (fb->height+7)/8;
	screensize = tile_width*tile_height * 8;

	fb->u8x8_buf = (uint8_t *)malloc(screensize*2);
	fb->u8g2_buf = (uint8_t *)fb->u8x8_buf + screensize;

	if ( fb->u8x8_buf == NULL ) {
		fb->u8g2_buf = NULL;
		return 0;
	}

	// Map the device to memory
	fb->fbp = g_disp_dev->buff[EPD_FRONT];

	memset(fb->fbp, 0xFF, screensize);
	return 1;
}

#define ROUNDUP(x,n) ((x+(n-1))&(~(n-1)))
#define BIT_SET(a,b) ((a) |= (1U<<(7-(b))))
#define BIT_CLEAR(a,b) ((a) &= ~(1U<<(7-(b))))

void u8x8_epd2in66drv_DrawTiles(u8x8_epd2in66drv_t *fb, uint16_t tx, uint16_t ty, uint8_t tile_cnt, uint8_t *tile_ptr)
{
	uint8_t byte;

	memset(fb->u8x8_buf, 0x00, 8*tile_cnt);

	for(int i=0; i < tile_cnt * 8; i++){
		byte = *tile_ptr++;
		for(int bit=0; bit < 8;bit++){
			if(byte & (1 << bit))
				fb->u8x8_buf[tile_cnt*bit+(i/8)] |= (1 << i%8);
		}
	}

	uint8_t *fbp = (uint8_t *)fb->fbp;

    uint16_t byte_index;
    uint16_t bit_index;
    uint16_t _x;
    uint16_t _y;

    for(int y=0; y<8;y++){
        for(int x=0; x<8*tile_cnt;x++) {
			_x = fb->height - 1 - ((ty*8)+y);
			_y = x;

            byte_index = (_y * fb->height + _x) >> 3;
            bit_index  = _x & 0x7;

            if(fb->u8x8_buf[(x/8) + (y*tile_cnt) ] & (1 << x%8))
                BIT_CLEAR(fbp[byte_index], bit_index);
            else
                BIT_SET(fbp[byte_index], bit_index);
        }
    }
}
/*========================================================*/
/* functions for handling of the global objects */

/* allocate bitmap */
/* will be called by u8x8_SetupBitmap or u8g2_SetupBitmap */
static uint8_t u8x8_SetEpd2in66drvDevice(U8X8_UNUSED u8x8_t *u8x8,
    uint32_t width, uint32_t height)
{
	u8x8_epd2in66drv.width = width;
	u8x8_epd2in66drv.height = height;

	/* update the global framebuffer object, allocate memory */
	if ( u8x8_epd2in66drv_alloc(&u8x8_epd2in66drv) == 0 )
		return 0;

	/* update the u8x8 info object */
	u8x8_epd2in66fb_info.tile_width = (u8x8_epd2in66drv.width+7)/8;
	u8x8_epd2in66fb_info.tile_height = (u8x8_epd2in66drv.height+7)/8;
	u8x8_epd2in66fb_info.pixel_width = u8x8_epd2in66drv.width;
	u8x8_epd2in66fb_info.pixel_height = u8x8_epd2in66drv.height;
	return 1;
}

/* draw tiles to the bitmap, called by the device procedure */
static void u8x8_DrawEpd_drvTiles(U8X8_UNUSED u8x8_t *u8x8, uint16_t tx, uint16_t ty, uint8_t tile_cnt, uint8_t *tile_ptr)
{
	u8x8_epd2in66drv_DrawTiles(&u8x8_epd2in66drv, tx, ty, tile_cnt, tile_ptr);
}

static uint8_t u8x8_framebuffer_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
	u8g2_uint_t x, y, c;
	uint8_t *ptr;
	switch(msg)
	{
	case U8X8_MSG_DISPLAY_SETUP_MEMORY:
		u8x8_d_helper_display_setup_memory(u8x8, &u8x8_epd2in66fb_info);
		break;
	case U8X8_MSG_DISPLAY_INIT:
		u8x8_d_helper_display_init(u8x8);	/* update low level interfaces (not required here) */
		break;
	case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
		break;
	case U8X8_MSG_DISPLAY_SET_FLIP_MODE:
		break;
	case U8X8_MSG_DISPLAY_DRAW_TILE:
		x = ((u8x8_tile_t *)arg_ptr)->x_pos;
		y = ((u8x8_tile_t *)arg_ptr)->y_pos;
		c = ((u8x8_tile_t *)arg_ptr)->cnt;
		ptr = ((u8x8_tile_t *)arg_ptr)->tile_ptr;
		do
		{
			u8x8_DrawEpd_drvTiles(u8x8, x, y, c, ptr);
			x += c;
			arg_int--;
		} while( arg_int > 0 );
		break;
	case U8X8_MSG_DISPLAY_REFRESH:
        disp_on(g_disp_dev);
		break;
	default:
	  return 0;
	}
	return 1;
}

void u8x8_SetupEpd2in66drv(u8x8_t *u8x8, uint32_t width, uint32_t height)
{
	u8x8_SetEpd2in66drvDevice(u8x8, width, height);

	/* setup defaults */
	u8x8_SetupDefaults(u8x8);

	/* setup specific callbacks */
	u8x8->display_cb = u8x8_framebuffer_cb;

	/* setup display info */
	u8x8_SetupMemory(u8x8);
}

void u8g2_SetupEpd2in66drv(u8g2_t *u8g2, const u8g2_cb_t *u8g2_cb)
{
    g_disp_dev = disp_init("epd_2in66");
    if (!g_disp_dev) {
        ESP_LOGE(TAG,"ddev init err\n");
        return;
    }

    g_disp_dev->flash_all = 1;
    g_disp_dev->buf_index = EPD_FRONT;

	/* allocate bitmap, assign the device callback to u8x8 */
	u8x8_SetupEpd2in66drv(u8g2_GetU8x8(u8g2), g_disp_dev->height, g_disp_dev->width);

	/* configure u8g2 in full buffer mode */
	u8g2_SetupBuffer(u8g2, u8x8_epd2in66drv.u8g2_buf, (u8x8_epd2in66fb_info.pixel_height+7)/8, u8g2_ll_hvline_vertical_top_lsb, u8g2_cb);
}