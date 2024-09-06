set(srcs "esp_hid_device_main.c"
         #"esp_hid_gap.c"
         #"storage/app_storage.c"
         #"web_server/web_server.c"
         "battery/battery.c"
         "humiture/aht20.c"
)
file(GLOB_RECURSE epdsrcs epd/*.c)
file(GLOB_RECURSE uisrcs ui/*.c)
file(GLOB_RECURSE wificonsrcs wificon/*.c)
file(GLOB_RECURSE sntptimesrcs sntp_time/*.c)

set(include_dirs "."
         "sntp_time"
         "storage"
         "web_server"
         "battery"
         "humiture"
         "epd"
         "eink"
         "ui"
         "wificon")

idf_component_register(SRCS "${srcs}" "${uisrcs}" "${epdsrcs}" "${wificonsrcs}" "${sntptimesrcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid esp_lcd lvgl esp_http_client esp-tls esp_http_server nvs_flash esp_adc
                       EMBED_FILES "web_ui_top.html" "web_ui_bottom.html")
