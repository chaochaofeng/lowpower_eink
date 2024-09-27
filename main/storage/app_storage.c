#include <string.h>
#include <assert.h>

#include "esp_log.h"

#include "nvs_flash.h"
#include "nvs.h"

#define NVS_NAMESPACE "wifi_info"
#define WIFI_SSID_KAY "ssid"
#define WIFI_PSWD_KAY "password"

void wifi_info_set(char* ssid, char* password)
{
    esp_err_t err = 0;

    if (strlen(ssid) == 0) {
        ESP_LOGE("APP_STORAGE", "SSID is empty");
        return;
    }

    nvs_handle_t my_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE("APP_STORAGE", "Error opening NVS handle! (%d)\n", err);
        return;
    }

    err = nvs_set_str(my_handle, WIFI_SSID_KAY, ssid);
    if (err != ESP_OK) {
        ESP_LOGE("APP_STORAGE", "Error writing %s to NVS (%d)\n", WIFI_SSID_KAY, err);
        return;
    }

    ESP_LOGI("APP_STORAGE", "Write ssid:%s\n", ssid);

    if (strlen(password) == 0) {
        ESP_LOGI("APP_STORAGE", "Password is empty");
        nvs_erase_key(my_handle, WIFI_PSWD_KAY);
        goto out;
    }

    err = nvs_set_str(my_handle, WIFI_PSWD_KAY, password);
    if (err != ESP_OK) {
        ESP_LOGE("APP_STORAGE", "Error writing %s to NVS (%d)\n", WIFI_PSWD_KAY, err);
    }

out:
    nvs_commit(my_handle);
}

int wifi_info_get(char *ssid, char *password)
{
    esp_err_t err = 0;
    char read_data[128];

    nvs_handle_t my_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE("APP_STORAGE", "Error opening NVS handle! (%d)\n", err);
        return err;
    }

    memset(read_data, 0x00, sizeof(read_data));
    // Read
    ESP_LOGI("APP_STORAGE","Reading restart counter from NVS ... ");
    size_t read_counter = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_str(my_handle, WIFI_SSID_KAY, NULL, &read_counter);
    if (err != ESP_OK) {
        ESP_LOGE("APP_STORAGE", "Error reading %s from NVS (%d)\n", WIFI_SSID_KAY, err);
        return err;
    }

    ESP_LOGI("APP_STORAGE", "Read from NVS has success, cnt:%d\n", read_counter);

    err = nvs_get_str(my_handle, WIFI_SSID_KAY, read_data, &read_counter);
    if (err != ESP_OK) {
        ESP_LOGI("APP_STORAGE", "Read %s from NVS failed (%d)\n", WIFI_PSWD_KAY, err);
        return err;
    }

    memcpy(ssid, read_data, read_counter);
    ssid[read_counter] = '\0';
    ESP_LOGI("APP_STORAGE", "ssid:[%s]", ssid);

    err = nvs_get_str(my_handle, WIFI_PSWD_KAY, NULL, &read_counter);
    if (err != ESP_OK) {
        ESP_LOGE("APP_STORAGE", "Error reading %s from NVS (%d)\n", WIFI_SSID_KAY, err);
        goto out;
    }

    ESP_LOGI("APP_STORAGE", "Read from NVS has success, cnt:%d\n", read_counter);

    memset(read_data, 0x00, sizeof(read_data));
    err = nvs_get_str(my_handle, WIFI_PSWD_KAY, read_data, &read_counter);
    if (err != ESP_OK) {
        ESP_LOGI("APP_STORAGE", "Read %s from NVS failed (%d)\n", WIFI_PSWD_KAY, err);
        goto out;
    }

    memcpy(password, read_data, read_counter);
    password[read_counter] = '\0';
    ESP_LOGI("APP_STORAGE", "password:[%s]", password);

    return 0;
out:
    password[0] = '\0';
    return 0;
}

void wifi_clear_info(void)
{
    esp_err_t ret = 0;
    nvs_handle_t my_handle;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("APP_STORAGE", "Error opening NVS handle! (%d)\n", ret);
        return;
    }

    nvs_erase_key(my_handle, WIFI_SSID_KAY);
    nvs_erase_key(my_handle, WIFI_PSWD_KAY);

    nvs_commit(my_handle);

    ESP_LOGI("APP_STORAGE", "%s erase storage ok", __func__);
}