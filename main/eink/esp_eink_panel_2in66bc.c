/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eink_panel_2in66bc.h"

static const char *TAG = "lcd_panel.eink";

static uint8_t eink_prevbuf[EINK_WIDTH * EINK_HEIGHT / 8];

static esp_err_t panel_eink_del(esp_lcd_panel_t *panel);
static esp_err_t panel_eink_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_eink_init(esp_lcd_panel_t *panel);
static esp_err_t panel_eink_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_eink_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_eink_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_eink_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_eink_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_eink_disp_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
} eink_panel_t;

esp_err_t esp_lcd_new_panel_eink_2in66bc(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    eink_panel_t *eink = NULL;

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    eink = calloc(1, sizeof(eink_panel_t));
    ESP_GOTO_ON_FALSE(eink, ESP_ERR_NO_MEM, err, TAG, "no mem for eink panel");

    ESP_LOGI(TAG, "esp_lcd_new_panel_eink_2in66bc eink panel @%p", eink);

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    eink->io = io;
    eink->reset_gpio_num = panel_dev_config->reset_gpio_num;
    eink->reset_level = panel_dev_config->flags.reset_active_high;
    eink->base.del = panel_eink_del;
    eink->base.reset = panel_eink_reset;
    eink->base.init = panel_eink_init;
    eink->base.draw_bitmap = panel_eink_draw_bitmap;
    eink->base.set_gap = panel_eink_set_gap;
    eink->base.mirror = panel_eink_mirror;
    eink->base.swap_xy = panel_eink_swap_xy;
    eink->base.disp_off = panel_eink_disp_off;
    *ret_panel = &(eink->base);
    ESP_LOGD(TAG, "new eink panel @%p", eink);

    memset(eink_prevbuf, 0xff, sizeof(eink_prevbuf));

    return ESP_OK;

err:
    if (eink) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(eink);
    }
    return ret;
}

static esp_err_t panel_eink_del(esp_lcd_panel_t *panel)
{
    eink_panel_t *eink = __containerof(panel, eink_panel_t, base);

    if (eink->reset_gpio_num >= 0) {
        gpio_reset_pin(eink->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del eink panel @%p", eink);
    free(eink);
    return ESP_OK;
}

static esp_err_t panel_eink_reset(esp_lcd_panel_t *panel)
{
    eink_panel_t *eink = __containerof(panel, eink_panel_t, base);
    esp_lcd_panel_io_handle_t io = eink->io;

    ESP_LOGI(TAG, "panel_eink_reset eink panel @%p", eink);
    // perform hardware reset
    if (eink->reset_gpio_num >= 0) {
        gpio_set_level(eink->reset_gpio_num, eink->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(eink->reset_gpio_num, !eink->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else { // perform software reset
        //esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5m before sending new command
    }

    return ESP_OK;
}

static esp_err_t panel_eink_init(esp_lcd_panel_t *panel)
{
    eink_panel_t *eink = __containerof(panel, eink_panel_t, base);
    esp_lcd_panel_io_handle_t io = eink->io;

    ESP_LOGI(TAG, "panel_eink_init eink panel @%p", eink);

    esp_lcd_panel_io_tx_param(io, 0x06, (uint8_t[]) {
        0x17, 0x17, 0x17,
    }, 3);

    esp_lcd_panel_io_tx_param(io, 0x04, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_lcd_panel_io_tx_param(io, 0x00, (uint8_t[]) {
        0x8F,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0x50, (uint8_t[]) { 0xF0,}, 1);
    esp_lcd_panel_io_tx_param(io, 0x61, (uint8_t[]) {
        EINK_WIDTH,
        EINK_HEIGHT >> 8,
        EINK_HEIGHT & 0xFF,
    }, 3);

    return ESP_OK;
}

static esp_err_t panel_eink_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    eink_panel_t *eink = __containerof(panel, eink_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = eink->io;

    // define an area of frame memory where MCU can access

    // transfer frame buffer
    size_t len = (x_end + 1 - x_start) * (y_end + 1 - y_start) / 8;
    esp_lcd_panel_io_tx_color(io, 0x10, color_data, len);
    esp_lcd_panel_io_tx_param(io, 0x92, NULL, 0);
    esp_lcd_panel_io_tx_color(io, 0x13, eink_prevbuf, len);
    esp_lcd_panel_io_tx_param(io, 0x92, NULL, 0);

    esp_lcd_panel_io_tx_param(io, 0x12, NULL, 0);

    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

static esp_err_t panel_eink_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    eink_panel_t *eink = __containerof(panel, eink_panel_t, base);
    esp_lcd_panel_io_handle_t io = eink->io;

    return ESP_OK;
}

static esp_err_t panel_eink_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    eink_panel_t *eink = __containerof(panel, eink_panel_t, base);
    esp_lcd_panel_io_handle_t io = eink->io;

    return ESP_OK;
}

static esp_err_t panel_eink_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    eink_panel_t *eink = __containerof(panel, eink_panel_t, base);

    return ESP_OK;
}

static esp_err_t panel_eink_disp_off(esp_lcd_panel_t *panel, bool off)
{
    eink_panel_t *eink = __containerof(panel, eink_panel_t, base);
    esp_lcd_panel_io_handle_t io = eink->io;
#if 0
    int command = 0;
    if (off) {
        command = LCD_CMD_DISPOFF;
    } else {
        command = LCD_CMD_DISPON;
    }
    esp_lcd_panel_io_tx_param(io, command, NULL, 0);
#endif
    return ESP_OK;
}
