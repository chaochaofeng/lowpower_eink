#include <string.h>
#include <assert.h>
#include "esp_partition.h"
#include "esp_log.h"

void wifi_info_set(char* ssid, char* password)
{
    
    char read_data[128];
    char store_data[128] = {0};
    esp_err_t ret = 0;

    const esp_partition_t *storage = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, "storage");
    assert(storage != NULL);

    ESP_LOGI("APP_STORAGE", "storage :%s", storage->label);
    ESP_LOGI("APP_STORAGE", "storage size :%x", storage->size);
    ESP_LOGI("APP_STORAGE", "storage address :%x", storage->address);

    memset(store_data, 0x00, sizeof(store_data));
 
    sprintf(store_data, "ssid:%s; password:%s;", ssid, password);

    ret = esp_partition_read(storage, 0, read_data, sizeof(read_data));
    if (ret != ESP_OK) {
        ESP_LOGI("APP_STORAGE", "1 esp_partition_read failed");
    }

    ESP_LOGI("APP_STORAGE", "storage :%s", read_data);

    if (memcmp(store_data, read_data, strlen(store_data)) != 0) {
        ret = esp_partition_erase_range(storage, 0, SPI_FLASH_SEC_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGI("APP_STORAGE", "esp_partition_erase_range failed");
            return;
        }

        ret = esp_partition_write(storage, 0, store_data, strlen(store_data));
        if (ret != ESP_OK) {
            ESP_LOGI("APP_STORAGE", "2 esp_partition_write failed");
        } else {
            ESP_LOGI("APP_STORAGE", "write %d storage :%s", strlen(store_data), store_data);

            ret = esp_partition_read(storage, 0, read_data, sizeof(read_data));
            if (ret != ESP_OK) {
                ESP_LOGI("APP_STORAGE", "3 esp_partition_read failed");
            }

            ESP_LOGI("APP_STORAGE", "write after read storage :%s", read_data);
        }
    }
}

/* ssid:xxxxx; password:xxxxx; */

char* find_wifi_info(char* data, char* item, uint8_t *item_len)
{
    uint8_t i = 0;
    char* p = data;

    if (data == NULL || item == NULL) {
        return NULL;
    }

    p = strstr(data, item);
    if ( p == NULL) {
        return NULL;
    }

    p += strlen(item);

    while (*(p + i) != ';') {
        i++;
        if (i > 63) {
            break;
        }
    }

    *item_len = i;

    return p;
}

int wifi_info_get(char *ssid, char *password)
{
    esp_err_t ret = 0;
    uint8_t i = 0;
    char read_data[128];
    char* start = NULL;

    const esp_partition_t *storage = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, "storage");
    assert(storage != NULL);

    ret = esp_partition_read(storage, 0, read_data, sizeof(read_data));
    if (ret != ESP_OK) {
        ESP_LOGI("APP_STORAGE", "esp_partition_read failed %s", __func__);
        return ret;
    }

    while (read_data[i] != 0xff) {
        i++;
    }

    read_data[i] = '\0';

    ESP_LOGI("APP_STORAGE", "%s storage read :%s", __func__, read_data);

    start = find_wifi_info(read_data, "ssid:", &i);
    if (start == NULL || i == 0) {
        ESP_LOGI("APP_STORAGE", "ssid not found");

        return -1;
    }

    memcpy(ssid, start, i);
    ssid[i] = '\0';
    ESP_LOGD("APP_STORAGE", "num:%d ssid:[%s] %d", i, ssid);

    start = find_wifi_info(read_data, "password:", &i);
    if (start == NULL || i == 0) {
        ESP_LOGI("APP_STORAGE", "password not found");
        return 0;
    }
    memcpy(password, start, i);
    password[i] = '\0';
    ESP_LOGD("APP_STORAGE", "num:%d password:[%s]", i, password);

    return 0;
}

void wifi_clear_info(void)
{
    esp_err_t ret = 0;
    const esp_partition_t *storage = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, "storage");
    assert(storage != NULL);

    ret = esp_partition_erase_range(storage, 0, SPI_FLASH_SEC_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGI("APP_STORAGE", "esp_partition_erase_range failed");
        return;
    }

    ESP_LOGI("APP_STORAGE", "%s erase storage ok", __func__);
}