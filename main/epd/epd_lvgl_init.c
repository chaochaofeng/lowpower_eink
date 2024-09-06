#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

#include "disp_drv.h"

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (5 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2

#define ROUNDUP(x,n) ((x+(n-1))&(~(n-1)))
#define BIT_SET(a,b) ((a) |= (1U<<(7-(b))))
#define BIT_CLEAR(a,b) ((a) &= ~(1U<<(7-(b))))

#define TAG "EPD_LVGL"

static void epd_lvgl_set_px_cb(struct _lv_disp_drv_t * disp_drv, uint8_t * buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
                      lv_color_t color, lv_opa_t opa)
{
    uint16_t stride = ROUNDUP(disp_drv->hor_res, 8);
    uint16_t _x = x;
    uint16_t _y = y;

    if (disp_drv->rotated == LV_DISP_ROT_270) {
        _x = stride - 1 - y;
        _y = x;
    } else if (disp_drv->rotated == LV_DISP_ROT_90) {
        _x = y;
        _y = disp_drv->ver_res - 1 - x;
    }

    uint16_t byte_index = (_y * stride + _x) >> 3;
    uint16_t bit_index  = _x & 0x7;

   //if (y == 0)
    // {
    //     ESP_LOGI(TAG,"start (%d, %d) (%d, %d) bi:%d,bti:%d", x, y, _x, _y, byte_index, bit_index);
    // } 

    if ( color.full != 0 ) {
        BIT_SET(buf[byte_index], bit_index);
    }
    else {
        BIT_CLEAR(buf[byte_index], bit_index);
    }
}

static void epd_lvgl_flush_cb(struct _lv_disp_drv_t * disp_drv,
	const lv_area_t *area, lv_color_t *px_map)
{
    struct disp_dev_st* disp_dev = (struct disp_dev_st*)disp_drv->user_data;

    if (lv_disp_flush_is_last(disp_drv)) {
        ESP_LOGI(TAG,"hor:%d ver:%d s:%d", disp_drv->hor_res, disp_drv->ver_res, ROUNDUP(disp_drv->ver_res, 8));
        disp_flush(disp_dev, 0, 0, disp_dev->width, disp_dev->height, (unsigned char *)px_map);
        disp_on(disp_dev);
    }

    lv_disp_flush_ready(disp_drv);
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");

    while (1) {
        lv_timer_handler();

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void epd_set_global_flush(lv_disp_t * dispp, bool global)
{
    struct disp_dev_st* disp_dev = dispp->driver->user_data;

    if (global) {
        disp_dev->flash_all = 1;
        disp_dev->buf_index = EPD_FRONT;
    } else {
        disp_dev->flash_all = 0;
    }

}

void lvgl_spi_epd_init(void)
{
	static lv_disp_drv_t disp_drv;
    static lv_disp_t *gdisp;

    struct disp_dev_st* disp_dev = disp_init("epd_2in66");
    if (!disp_dev) {
        ESP_LOGE(TAG,"ddev init err\n");
        return;
    }

    disp_dev->flash_all = 1;
    disp_dev->buf_index = EPD_FRONT;

    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = heap_caps_malloc(disp_dev->width * disp_dev->height / 8, MALLOC_CAP_DMA);
    assert(buf1);

    static lv_disp_draw_buf_t draw_buf_dsc_1;
    lv_disp_draw_buf_init(&draw_buf_dsc_1, buf1, NULL, disp_dev->width * disp_dev->height / 8);

	lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = disp_dev->width;
    disp_drv.ver_res = disp_dev->height;

	disp_drv.flush_cb = epd_lvgl_flush_cb;
	//disp_drv.drv_update_cb = epd_lvgl_update_cb;
    disp_drv.set_px_cb = epd_lvgl_set_px_cb;
	disp_drv.draw_buf = &draw_buf_dsc_1;
	disp_drv.user_data = disp_dev;
    disp_drv.direct_mode = 1;

	gdisp = lv_disp_drv_register(&disp_drv);

    lv_disp_set_rotation(gdisp, LV_DISP_ROT_90);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);
}