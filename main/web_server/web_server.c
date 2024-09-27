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

#include "app_storage.h"
#include "wificon.h"

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

#define WIFI_CONNECTED_BIT              BIT0
#define WIFI_FAIL_BIT                   BIT1
#define WIFI_CONFIG_SUCCESS_BIT         BIT2
#define WIFI_CONFIG_FAIL_BIT            BIT3

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
			printf("(%d, %s, %d)\r\n",list[i].authmode, list[i].ssid, list[i].rssi);
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

#if 0
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
#endif

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
    } else {
        xEventGroupSetBits(wifi_event_group, WIFI_CONFIG_FAIL_BIT);
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

int esp_web_server_state(void)
{
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
        WIFI_CONFIG_SUCCESS_BIT | WIFI_CONFIG_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (bits & WIFI_CONFIG_SUCCESS_BIT) {
        wifi_info_set(sta_ssid, sta_pass);
        return 0;
    }

    return -1;
}

void esp_web_server_stop()
{
    ESP_LOGI(TAG, "stopping webserver");
	stop_webserver(g_http_handle);

    wificon.deinit();
}

int esp_web_server_init()
{
    wificon.init_ap("clock_ap", "");

    wifi_event_group = xEventGroupCreate();

	g_http_handle = start_webserver();

    return 0;
}

