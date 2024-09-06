#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include <esp_http_server.h>
#include "freertos/event_groups.h"

#include "net_time.h"
#include "dns_server.h"
#include "ui.h"
#include "app_storage.h"

#define TAG "epaper-web"

static char resp_buf[2560];

extern const unsigned char web_ui_top_start[]    asm("_binary_web_ui_top_html_start");
extern const unsigned char web_ui_top_end[]      asm("_binary_web_ui_top_html_end");
extern const unsigned char web_ui_bottom_start[] asm("_binary_web_ui_bottom_html_start");
extern const unsigned char web_ui_bottom_end[]   asm("_binary_web_ui_bottom_html_end");

const char *nowifi = "no wifi in around\n";

static char sta_ssid[32] = {0};
static char sta_pass[32] = {0};
static httpd_handle_t g_http_handle;

static EventGroupHandle_t wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_CONFIG_SUCCESS_BIT      BIT2
#define WIFI_CONFIG_FAIL_BIT      BIT3

void stop_webserver(httpd_handle_t server);


uint16_t scan_wifi(wifi_ap_record_t **wlist)
{
	esp_err_t ret;
	wifi_ap_record_t *list = NULL;
	uint16_t apCount = 0;

	wifi_scan_config_t scanConf = {
		.ssid = NULL,
		.bssid = NULL,
		.channel = 0,
		.show_hidden = 1,
	};

	ret = esp_wifi_scan_start(&scanConf, 1);
	if (ret == ESP_OK) {
		esp_wifi_scan_get_ap_num(&apCount);
		list = (wifi_ap_record_t *)heap_caps_calloc(apCount,
				sizeof(wifi_ap_record_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, list));
		for (int i = 0; i < apCount; i++) {
			printf("(%d,\"%s\",%d,\""MACSTR" %d\")\r\n",list[i].authmode, list[i].ssid, list[i].rssi,
                 MAC2STR(list[i].bssid),list[i].primary);
		}

		*wlist = list;
	}

	return apCount;
}

/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    /* Send response with custom headers and body set as the
     * string passed in user context*/
	wifi_ap_record_t *wlist = NULL;
	uint16_t apcont = 0;
    int ret = 0;

	apcont = scan_wifi(&wlist);

    httpd_resp_set_type(req, "text/html");

    const size_t top_send_size = (web_ui_top_end - web_ui_top_start);
    ESP_LOGD(TAG, "top_send_size :%d", top_send_size);

    ret = httpd_resp_send_chunk(req, (const char *)web_ui_top_start, top_send_size);
    if (ret) {
        ESP_LOGE(TAG, "httpd_resp_send_chunk failed webuitop ret=%d", ret);
    }

    if (apcont) {
        for (int i = 0; i < apcont; i++) {
            if (strlen((const char *)wlist[i].ssid) == 0)
                continue;

            ret = httpd_resp_sendstr_chunk(req, "<option>");
            if (ret) {
                ESP_LOGE(TAG, "httpd_resp_sendstr_chunk <option> failed ret=%d", ret);
            }
            ret = httpd_resp_sendstr_chunk(req, (const char *)wlist[i].ssid);
            if (ret) {
                ESP_LOGE(TAG, "httpd_resp_sendstr_chunk failed ret=%d %s", ret, wlist[i].ssid);
            }
            ret = httpd_resp_sendstr_chunk(req, "</option>");
            if (ret) {
                ESP_LOGE(TAG, "httpd_resp_sendstr_chunk </option> failed ret=%d", ret);
            }
        }
    }

    const size_t bottom_send_size = (web_ui_bottom_end - web_ui_bottom_start);
    ESP_LOGD(TAG, "bottom_send_size :%d", bottom_send_size);

    ret = httpd_resp_send_chunk(req, (const char *)web_ui_bottom_start, bottom_send_size);
    if (ret) {
        ESP_LOGE(TAG, "httpd_resp_send_chunk webuitopbottom failed ret=%d", ret);
    }
    ret = httpd_resp_sendstr_chunk(req, NULL); //end
    if (ret) {
        ESP_LOGE(TAG, "httpd_resp_send_chunk null failed ret=%d", ret);
    }

    ESP_LOGI(TAG, "httpd_resp_send_chunk done");
    ESP_LOGD(TAG, "%s", resp_buf);

	if (wlist) {
		heap_caps_free(wlist);
		wlist = NULL;
	}

    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
};

static int parse_item(char *dst, char *src, char *item)
{
	char *before = NULL;
	char *after = NULL;
	before = strstr(src, item);
	if (before) {
		ESP_LOGI(TAG, "%d : %s\n", __LINE__, before);
		after = strstr(before, "&");
        if (!after) {
            strcpy(dst, before + strlen(item));
            dst[strlen(before) - strlen(item)] = '\0';
            goto out;
        }
		ESP_LOGI(TAG, "%d : %s %d %d\n", __LINE__, after, after - before, strlen(item));
		if ((after - before) == strlen(item)) {
			ESP_LOGI(TAG, "%s is null", item);
			return -1;
		}

		strncpy(dst, before+strlen(item), after - before - strlen(item));
		dst[after - before - strlen(item) + 1] = '\0'; 

	} else {
		ESP_LOGI(TAG, "can't find buf [%s]\n", src);
        return -1;
	}

out:
	ESP_LOGI(TAG, "%s[%s]\n", item, dst);
	return 0;
}


int parse_time(char *buf)
{
	char tmp[10];
	bool flag = false;

	struct ntime_st ns;
	memset(tmp, 0, 10);
	if (!parse_item(tmp, buf, "hour=")) {
		ns.hour = atoi(tmp);
		flag = true;
	}
	memset(tmp, 0, 10);
	if (!parse_item(tmp, buf, "min=")) {
		ns.min = atoi(tmp);
		flag = true;
	}

	memset(tmp, 0, 10);
	if (!parse_item(tmp, buf, "sec=")) {
		ns.sec = atoi(tmp);
		flag = true;
	}

	memset(tmp, 0, 10);
	if (!parse_item(tmp, buf, "day=")) {
		ns.day = atoi(tmp);
		flag = true;
	}

	memset(tmp, 0, 10);
	if (!parse_item(tmp, buf, "mon=")) {
		ns.mon = atoi(tmp);
		flag = true;
	}

	memset(tmp, 0, 10);
	if (!parse_item(tmp, buf, "year=")) {
		ns.year = atoi(tmp);	
		flag = true;
	}

	if (flag) {
		ESP_LOGI(TAG, "%04d-%02d-%02d %02d:%02d:%02d\n", ns.year, ns.mon, ns.day,
			ns.hour, ns.min, ns.sec);

		//update_time_from_nt(&ns);
		return 0;
	}

	return -1;
}

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;
	bool flag = false;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }
		buf[ret] = '\0';

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
        	
        // End response
        httpd_resp_send_chunk(req, NULL, 0);
    }

	if (!parse_item(sta_ssid, buf, "ssid=")) {
		if (!parse_item(sta_pass, buf, "password=")) {
			flag= true;
		}
	}


    if (flag) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONFIG_SUCCESS_BIT);
        wifi_info_set(sta_ssid, sta_pass);
    }

    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/configwifi",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret;

    if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (buf == '0') {
        /* URI handlers can be unregistered using the uri string */
        ESP_LOGI(TAG, "Unregistering /hello and /echo URIs");
        httpd_unregister_uri(req->handle, "/hello");
        httpd_unregister_uri(req->handle, "/echo");
        /* Register the custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    else {
        ESP_LOGI(TAG, "Registering /hello and /echo URIs");
        httpd_register_uri_handler(req->handle, &hello);
        httpd_register_uri_handler(req->handle, &echo);
        /* Unregister custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
    }

    /* Respond with empty body */
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t ctrl = {
    .uri       = "/ctrl",
    .method    = HTTP_PUT,
    .handler   = ctrl_put_handler,
    .user_ctx  = NULL
};


/* 启动 Web 服务器的函数 */
httpd_handle_t start_webserver(void)
{
    /* 生成默认的配置参数 */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* 置空 esp_http_server 的实例句柄 */
    httpd_handle_t server = NULL;

    /* 启动 httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* 注册 URI 处理程序 */
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &ctrl);

    }
    /* 如果服务器启动失败，返回的句柄是 NULL */
    return server;
}

/* 停止 Web 服务器的函数 */
void stop_webserver(httpd_handle_t server)
{
    if (server) {
        /* 停止 httpd server */
        httpd_stop(server);
    }
}

#define EXAMPLE_ESP_WIFI_SSID      "wificonfig"
#define EXAMPLE_ESP_WIFI_PASS      "88888888"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       2

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	static uint8_t s_retry_num = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 2) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_softap(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

int wifi_init_softsta(char *ssid, char *password)
{
    //ESP_ERROR_CHECK(esp_event_loop_create_default());
    //esp_netif_create_default_wifi_sta();

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
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	strncpy((char *)wifi_config.sta.ssid, ssid, strlen(sta_ssid));
	strncpy((char *)wifi_config.sta.password, password, strlen(sta_pass));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
					 wifi_config.sta.ssid, wifi_config.sta.password);

	ESP_LOGE(TAG, "WaitBits");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 sta_ssid, sta_pass);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 sta_ssid, sta_pass);

		return -1;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
		return -1;
    }

	return 0;
}

char *get_wifi_sta_ssid(void)
{
	return sta_ssid;
}

char *get_wifi_sta_password(void)
{
	return sta_pass;
}

char *get_wifi_ap_ssid(void)
{
	return EXAMPLE_ESP_WIFI_SSID;
}
char *get_wifi_ap_pass(void)
{
	return EXAMPLE_ESP_WIFI_PASS;
}

void esp_web_server_stop()
{
    ESP_LOGI(TAG, "stopping webserver");

    esp_wifi_stop();
	esp_wifi_deinit();
    esp_event_loop_delete_default();
	stop_webserver(g_http_handle);

    esp_restart();
}

int esp_web_server_init()
{
    char buf[64];
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");

    wifi_event_group = xEventGroupCreate();

    wifi_init_softap();

    ui_Screen3_screen_init();
    lv_disp_load_scr(ui_Screen3);

    sprintf(buf, "WIFI: %s", get_wifi_ap_ssid());
	_ui_label_set_property(ui_apwifissd, _UI_LABEL_PROPERTY_TEXT, buf);

	g_http_handle = start_webserver();

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONFIG_SUCCESS_BIT | WIFI_CONFIG_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
    
    esp_web_server_stop();
    if (bits & WIFI_CONFIG_SUCCESS_BIT) {
        return 0;
    } else {
        return -1;
    }
}

//http://quan.suning.com/getSysTime.do
//https://devapi.qweather.com/v7/weather/now?location=101020600&key=7caabeb0908946b5b2df713f6c639add
