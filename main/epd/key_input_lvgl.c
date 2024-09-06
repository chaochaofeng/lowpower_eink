#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "lvgl.h"

#define KEY_PREV  18
#define KEY_NEXT  19
#define KEY_ENTER 13

#define TAG "key_input"

static gpio_config_t init_input_io(gpio_num_t num)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << num);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    return io_conf;
}

void key_init(void)
{
	int keyenter = KEY_ENTER;
	int keynext  = KEY_NEXT;
    int keyprev  = KEY_PREV;

	gpio_config_t input_io = init_input_io(keyenter);
	gpio_config(&input_io);

	input_io = init_input_io(keynext);
    // input_io.pull_up_en = 1;
	gpio_config(&input_io);

    input_io = init_input_io(keyprev);
    gpio_config(&input_io);
}

static uint32_t keypad_get_key(void)
{
	if (gpio_get_level(KEY_PREV) == 0)
		return LV_KEY_LEFT;

	if (gpio_get_level(KEY_NEXT) == 0)
		return LV_KEY_RIGHT;

	if (gpio_get_level(KEY_ENTER) == 0)
		return LV_KEY_ENTER;

    return 0;
}

static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static uint32_t last_key = 0;
	uint32_t act_key = 0;

    /*Get whether the a key is pressed and save the pressed key*/
	act_key = keypad_get_key();
    if(act_key != 0) {
        data->state = LV_INDEV_STATE_PR;

        last_key = act_key;
        ESP_LOGI(TAG, "act_key: %ld", act_key);
    }
    else if (last_key == 0) {
        data->state = LV_INDEV_STATE_REL;
    }

    data->key = last_key;
}

lv_indev_t *lv_indev;

void lv_port_indev_init(void)
{
	static lv_indev_drv_t indev_drv;

	ESP_LOGI(TAG, "Starting lv_port_indev_init");

	key_init();

    /*Register a keypad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = keypad_read;
    lv_indev = lv_indev_drv_register(&indev_drv);
}

void lv_port_indev_setgroup(lv_group_t * g)
{
	lv_indev_set_group(lv_indev, g);
}

