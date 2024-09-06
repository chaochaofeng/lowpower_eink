#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "battery.h"

#define 	BATTERY_CHANGE	    3
#define 	ADC_BATTERY_EN		1
#define     ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP

#define     ADC_BATTERY_ATTEN           ADC_ATTEN_DB_12
#define     ADC1_BATTERY_CHAN           ADC_CHANNEL_0

static int adc_raw[2][10];
static int voltage[2][10];

static const char *TAG = "ADC SINGLE";
static uint32_t rising_time 	= 0;
static int  read_inv = 100;

battery_info batteryinfo = {0};

#define abs(x) ((x) < 0 ? -(x) : (x))

static gpio_config_t init_output_io(gpio_num_t num)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << num);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    return io_conf;
}

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void IRAM_ATTR battery_change_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    
    if (gpio_get_level(gpio_num)) {
        rising_time++;
    }
}

static uint32_t get_battery_voltage(adc_oneshot_unit_handle_t adchandle, adc_cali_handle_t adc1_cali_chan0_handle, bool do_calibration)
{
    gpio_set_level(ADC_BATTERY_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_ERROR_CHECK(adc_oneshot_read(adchandle, ADC1_BATTERY_CHAN, &adc_raw[0][0]));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC1_BATTERY_CHAN, adc_raw[0][0]);
    if (do_calibration) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw[0][0], &voltage[0][0]));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC1_BATTERY_CHAN, voltage[0][0]);
    }

    gpio_set_level(ADC_BATTERY_EN, 0);

    return voltage[0][0];
}

static void battery_read_task(void *pvParameters)
{
    int voltage = 0;

    int cnt = read_inv + 1;
    bool battery_exist = 0;

    batteryinfo.voltage = 0;
    batteryinfo.adc_raw = 0;
    batteryinfo.charging = 0;
    batteryinfo.battery_exist = 0;

    //-------------ADC1 Init---------------//
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_BATTERY_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_BATTERY_CHAN, &config));

    //-------------ADC1 Calibration Init---------------//
    adc_cali_handle_t adc1_cali_chan0_handle = NULL;
    bool do_calibration1_chan0 = example_adc_calibration_init(
        ADC_UNIT_1, ADC1_BATTERY_CHAN, ADC_BATTERY_ATTEN, &adc1_cali_chan0_handle);
 
    while (1) {
        if (rising_time > 5) {
            battery_exist = 0;
            batteryinfo.battery_exist = 0;
        }
        else {
            battery_exist = 1;
            batteryinfo.battery_exist = 1;
        }

        rising_time = 0;

        if (battery_exist) {
            if (cnt >= read_inv) {
                cnt = 0;
    
                voltage = get_battery_voltage(adc1_handle, adc1_cali_chan0_handle, do_calibration1_chan0);

                batteryinfo.voltage = (int)(0.33 * voltage - 600);

                if (batteryinfo.voltage > 100)
                    batteryinfo.voltage = 100;
                else if (batteryinfo.voltage < 0)
                    batteryinfo.voltage = 0;
            }

            batteryinfo.charging = !gpio_get_level(BATTERY_CHANGE);

            cnt++;
        }

        batteryinfo.status = 1;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void battery_init(void)
{
	gpio_config_t output_io = init_output_io(ADC_BATTERY_EN);
	gpio_config(&output_io);

    gpio_config_t input_io = init_output_io(BATTERY_CHANGE);
    input_io.mode = GPIO_MODE_INPUT;
	gpio_config(&input_io);
    gpio_install_isr_service(0);

    gpio_set_intr_type(BATTERY_CHANGE, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(BATTERY_CHANGE, battery_change_isr_handler, (void*) BATTERY_CHANGE);

    xTaskCreate(battery_read_task, "battery_read_task", 8192, NULL, 4, NULL);
}
