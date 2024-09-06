#ifndef __ESP_EINK_PANEL_2IN66BC__
#define __ESP_EINK_PANEL_2IN66BC__

#define EINK_WIDTH 152
#define EINK_HEIGHT 296

esp_err_t esp_lcd_new_panel_eink_2in66bc(const esp_lcd_panel_io_handle_t io,
    const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);
#endif