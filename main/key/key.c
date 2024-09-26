#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "core/inc/base.h"

#define TAG "key_input"

#define KEY_PREV  18
#define KEY_NEXT  19
#define KEY_ENTER 1

static QueueHandle_t gpio_evt_queue = NULL;

struct key_info
{
    int key;
    bool pressed;
    TickType_t tick;
};

static gpio_config_t init_input_io(gpio_num_t num)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << num);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    return io_conf;
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;

    struct key_info key_info;
    key_info.key = gpio_num;
    key_info.pressed = !gpio_get_level(gpio_num);
    key_info.tick = xTaskGetTickCountFromISR();

    xQueueSendFromISR(gpio_evt_queue, &key_info, NULL);
}

static int key_map(int gpio_num)
{
    switch (gpio_num) {
        case KEY_ENTER: return UG_KEY_ENTER;
        case KEY_NEXT : return UG_KEY_NEXT;
        case KEY_PREV : return UG_KEY_PREV;
    }

    return -1;
}

static void gpio_task_example(void* arg)
{
    struct key_info key_info;
    struct key_info last_key_info;

    last_key_info.key = 0;
    last_key_info.pressed = 0;
    last_key_info.tick = 0;

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &key_info, portMAX_DELAY)) {
            if (key_info.key == last_key_info.key && key_info.tick - last_key_info.tick < 50)
                continue;

            ug_input_proc(key_map(key_info.key));

            last_key_info.key = key_info.key;
            last_key_info.pressed = key_info.pressed;
            last_key_info.tick = key_info.tick;
        }
    }
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

    gpio_install_isr_service(0);

    gpio_set_intr_type(keyenter, GPIO_INTR_NEGEDGE);

    gpio_evt_queue = xQueueCreate(5, sizeof(struct key_info));

    gpio_isr_handler_add(keyenter, gpio_isr_handler, (void*) keyenter);
    gpio_isr_handler_add(keynext, gpio_isr_handler, (void*) keynext);
    gpio_isr_handler_add(keyprev, gpio_isr_handler, (void*) keyprev);

   xTaskCreate(gpio_task_example, "gpio_task_example", 8192, NULL, 6, NULL);
}

