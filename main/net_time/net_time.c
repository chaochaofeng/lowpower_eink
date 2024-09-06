/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "freertos/event_groups.h"

#include "esp_http_client.h"
#include "net_time.h"

#include "app_storage.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "ui_helpers.h"

/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define GET_TIME_DONE_EVENT        BIT2
#define GET_TIME_REQ_EVENT    BIT3
#define WAIT_COMPLETE_BIT         BIT4

static const char *TAG = "wifi station";
static int s_retry_num = 0;

#define EXAMPLE_ESP_WIFI_SSID      "Xiaomi_2A40"
#define EXAMPLE_ESP_WIFI_PASS      "fc159357123"
#define EXAMPLE_ESP_MAXIMUM_RETRY  3
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
		lv_obj_set_style_text_color(ui_wifistatus, lv_color_hex(0x005000), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
		lv_obj_set_style_text_color(ui_wifistatus, lv_color_hex(0x00C800), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static int wifi_init_sta(char *ssid, char *password)
{
	esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

	esp_wifi_restore();
    ESP_ERROR_CHECK(esp_netif_init());

    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));


    s_wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
	     .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
	     .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

	ESP_LOGI(TAG, "ssid:%s password:%s", ssid, password);
	strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
	if (strlen(password) != 0) {
		strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
	}

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
	esp_wifi_connect();

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ssid, password);

		return 0;
    } else {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s bits:0x%x",
                 ssid, password, bits);

		ui_Screen4_screen_init();
		lv_disp_load_scr(ui_Screen4);

		return -1;
    }

	bits = xEventGroupWaitBits(s_wifi_event_group,
		WAIT_COMPLETE_BIT,
		pdFALSE,
		pdTRUE,
		portMAX_DELAY);
}


#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

static const char *week_abb[] = {
	"Mon",
	"Tue",
	"Wed",
	"Thur",
	"Fri",
	"Sat",
	"Sun",
	NULL,
};

static const char *mon_abb[] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sept",
	"Oct",
	"Nov",
	"Dec",
};

static int16_t parse_item(char *week, const char *abb[])
{
	uint8_t i = 0;

	for (i = 0; abb[i]; i++) {
		if (strstr(week, abb[i]))
			return i+1;
	}

	return 0;
}

//Sun, 16 Oct 2022 14:12:06 GMT
static int parse_time(char *msg, struct ntime_st *ntime)
{
	char ctmp[5];
	char *bcp = NULL, *acp = NULL;

	acp = strstr(msg, ",");
	strncpy(ctmp, msg, acp - msg);
	ctmp[acp - msg] = '\0';
	ESP_LOGI(TAG, "week:%s", ctmp);
	ntime->week = parse_item(ctmp, week_abb);
	if (!ntime->week) {
		ESP_LOGE(TAG, "parse week err");
	}

	acp += 2;
	strncpy(ctmp, acp, 2);
	ctmp[2] = '\0';
	ESP_LOGI(TAG, "day:%s", ctmp);
	ntime->day = atoi(ctmp);
	if (!ntime->day) {
		ESP_LOGE(TAG, "parse day err");
	}

	acp += 3;
	bcp = acp;
	acp = strstr(bcp, " ");
	strncpy(ctmp, bcp, acp - bcp);
	ctmp[acp - bcp] = '\0';
	ESP_LOGI(TAG, "mon:%s", ctmp);
	ntime->mon = parse_item(ctmp, mon_abb);
	if (!ntime->mon) {
		ESP_LOGE(TAG, "parse mon err");
	}

	bcp = acp+1;
	acp = strstr(bcp, " ");
	strncpy(ctmp, bcp, acp - bcp);
	ctmp[acp - bcp] = '\0';
	ESP_LOGI(TAG, "year:%s", ctmp);
	ntime->year = atoi(ctmp);
	if (!ntime->year) {
		ESP_LOGE(TAG, "parse year err");
	}

	bcp = acp+1;
	acp = strstr(bcp, ":");
	strncpy(ctmp, bcp, acp - bcp);
	ctmp[acp - bcp] = '\0';
	ESP_LOGI(TAG, "hour:%s", ctmp);
	ntime->hour = atoi(ctmp);
	if (!ntime->hour) {
		ESP_LOGE(TAG, "parse hour err");
	}

	bcp = acp+1;
	acp = strstr(bcp, ":");
	strncpy(ctmp, bcp, acp - bcp);
	ctmp[acp - bcp] = '\0';
	ESP_LOGI(TAG, "min:%s", ctmp);
	ntime->min = atoi(ctmp);
	if (!ntime->min) {
		ESP_LOGE(TAG, "parse min err");
	}

	bcp = acp+1;
	acp = strstr(bcp, " ");
	strncpy(ctmp, bcp, acp - bcp);
	ctmp[acp - bcp] = '\0';
	ESP_LOGI(TAG, "sec:%s", ctmp);
	ntime->sec = atoi(ctmp);
	if (!ntime->min) {
		ESP_LOGE(TAG, "parse sec err");
	}

	return 0;
}

static bool is_leap_year(uint16_t year)
{
	if (year%4)
		return false;

	if (year % 100)
		return true;

	if (year % 400)
		return false;
	else
		return true;
}

static uint16_t cal_mon_days(uint16_t year, uint16_t mon)
{
	switch (mon) {
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			return 31;
		case 2:
			if (is_leap_year(year))
				return 29;
			else
				return 28;
		default:
			return 30;
	}
}
static void ntime_gmt_to_beijing(struct ntime_st *ntime)
{
	uint16_t days = 0;
	printf("%s entry\n", __func__);

	ntime->hour += 8;
	if (ntime->hour < 24)
		goto out;

	ntime->hour %= 24;
	ntime->day++;

	days = cal_mon_days(ntime->year, ntime->mon);
	if (ntime->day <= days)
		goto out;

	ntime->day -= days;
	ntime->mon++;

	if (ntime->mon <= 12)
		goto out;

	ntime->year++;
out:
	ESP_LOGE(TAG, "date  %d-%d-%d %d:%d:%d", ntime->year,ntime->mon, ntime->day,
		ntime->hour, ntime->min, ntime->sec);
}

struct ntime_st ns;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
    		if (strstr(evt->header_key,"Date")) {
				parse_time(evt->header_value, &ns);
				ntime_gmt_to_beijing(&ns);
				
				xEventGroupSetBits(s_wifi_event_group, GET_TIME_DONE_EVENT);
			}
			break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                } else {
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
    }
    return ESP_OK;
}

static void http_rest_with_url(void *pvParameters)
{
    esp_http_client_config_t config = {
        .url = "https://www.baidu.com",
        .event_handler = _http_event_handler,
        .user_data = pvParameters,        // Pass address of local buffer to get response
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

	ESP_LOGE(TAG, "%s ns %p", __func__, config.user_data);

    // GET
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void http_test_task(void *pvParameters)
{
	EventBits_t bits;

	ESP_LOGI(TAG, "%s", __func__);

	while (1) {
		bits = xEventGroupWaitBits(s_wifi_event_group, GET_TIME_REQ_EVENT, 1, 0, portMAX_DELAY);
		ESP_LOGI(TAG, "event bits %x", bits);
		if (bits & GET_TIME_REQ_EVENT) {
			ESP_LOGI(TAG, "GET_TIME_REQ");
    		http_rest_with_url(pvParameters);
		}
	}

    vTaskDelete(NULL);
}

static int app_wifi_get_time_wait_done(void)
{
	EventBits_t bits;

	bits = xEventGroupWaitBits(s_wifi_event_group, GET_TIME_DONE_EVENT, 0, 1, 7000/portTICK_RATE_MS);
    if (bits &= GET_TIME_DONE_EVENT) {
		ESP_LOGI(TAG, "GET_TIME_DONE");
	} else {
		ESP_LOGI(TAG, "GET_TIME_DONE timeout");
		return -1;
	}

	ESP_LOGE(TAG, "date %d:%d:%d", ns.hour, ns.min, ns.sec);

	return 0;
}

static int app_wifi_get_time(void)
{
	ESP_LOGI(TAG, "set GET_TIME_REQ_EVENT");
	xEventGroupSetBits(s_wifi_event_group, GET_TIME_REQ_EVENT);

	return 0;
}

static void timer_flush(uint8_t min, uint8_t hour, uint8_t day, uint8_t mon)
{
    char buf[6];

    sprintf(buf, "%02d", hour);
    _ui_label_set_property(ui_hour, _UI_LABEL_PROPERTY_TEXT, buf);

    sprintf(buf, "%02d", min);
    _ui_label_set_property(ui_min, _UI_LABEL_PROPERTY_TEXT, buf);

	sprintf(buf, "%02d-%02d", mon, day);
	_ui_label_set_property(ui_date, _UI_LABEL_PROPERTY_TEXT, buf);
}

void timer_task(lv_timer_t * tmr)
{
    uint8_t hour = ns.hour;
    uint8_t min = ns.min;
	uint8_t day = ns.day;
	uint8_t mon = ns.mon;
	uint8_t sec = ns.sec;

	static bool flag = false;
	if (flag)
		lv_obj_set_style_text_color(ui_fenhao, lv_color_hex(0xE0E0E0), LV_PART_MAIN | LV_STATE_DEFAULT);
	else
		lv_obj_set_style_text_color(ui_fenhao, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

	flag = !flag;

	sec++;
	if (sec != 60) {
		goto out;
	}

	sec = 0;
	min++;
	if (min == 60) {
		min = 0;
		hour++;

		app_wifi_get_time();
		if (hour == 24) {
			hour = 0;

			day++;
			if (day == 31) {
				day = 0;
				mon++;
			}
		}

	}

	timer_flush(min, hour, day, mon);

out:
	ns.sec  = sec;
	ns.min  = min;
	ns.hour = hour;
	ns.day  = day;
	ns.mon  = mon;

    lv_timer_resume(tmr);
}

int app_wifi_time_init(char *ssid, char *password)
{
	int ret = 0;
	char buf[48];

	ui_Screen1_screen_init();
	ui_Screen2_screen_init();
	lv_disp_load_scr(ui_Screen2);

	sprintf(buf, "connecting  %s", ssid);
	ESP_LOGI(TAG, "%s", buf);
	_ui_label_set_property(ui_ConInfo, _UI_LABEL_PROPERTY_TEXT, buf);
	ret = wifi_init_sta(ssid, password);
	if (ret != 0) {
		return ret;
	}

	sprintf(buf, "%s", "Calibrating time");
	_ui_label_set_property(ui_ConInfo, _UI_LABEL_PROPERTY_TEXT, buf);

    xTaskCreate(http_test_task, "http_test_task", 8192, NULL, 5, NULL);

	app_wifi_get_time();
	ret = app_wifi_get_time_wait_done();
	if (ret != 0) {
		sprintf(buf, "%s", "Calibrating time failed");
		_ui_label_set_property(ui_ConInfo, _UI_LABEL_PROPERTY_TEXT, buf);
		return ret;
	}

	sprintf(buf, "%s", "Calibrating time success");
	_ui_label_set_property(ui_ConInfo, _UI_LABEL_PROPERTY_TEXT, buf);
	vTaskDelay(500 / portTICK_PERIOD_MS);

	return 0;
}

void app_time_task_init(void)
{
    lv_timer_t * timer = NULL;

	lv_disp_load_scr(ui_Screen1);

	timer = lv_timer_create(timer_task, 1000, timer);

	timer_flush(ns.min, ns.hour, ns.day, ns.mon);

	lv_disp_load_scr(ui_Screen1);
}