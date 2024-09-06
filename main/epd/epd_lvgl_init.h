#ifndef __EPD_LVGL_INIT_H__
#define __EPD_LVGL_INIT_H__

void lvgl_spi_epd_init(void);

void epd_set_global_flush(lv_disp_t * dispp, bool global);
#endif