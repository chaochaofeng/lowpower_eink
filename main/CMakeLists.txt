set(srcs "esp_hid_device_main.c"
         #"esp_hid_gap.c"
         "storage/app_storage.c"
         "web_server/web_server.c"
         "battery/battery.c"
         "humiture/aht20.c"
)

file(GLOB_RECURSE epdsrcs epd/*.c)
file(GLOB_RECURSE uisrcs ui/core/src/*.c ui/screen/*.c ui/fonts/*.c)
file(GLOB_RECURSE wificonsrcs wificon/*.c)
file(GLOB_RECURSE sntptimesrcs sntp_time/*.c)
file(GLOB_RECURSE keysrcs key/*.c)

set(include_dirs "."
         "sntp_time"
         "storage"
         "web_server"
         "battery"
         "humiture"
         "epd"
         "ui"
         "key"
         "wificon")

idf_component_register(SRCS "${srcs}" "${epdsrcs}" "${wificonsrcs}" "${sntptimesrcs}" "${uisrcs}" "${keysrcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES u8g2 "driver" esp_hid esp_http_client esp-tls esp_http_server nvs_flash esp_adc
                       EMBED_FILES "web_ui_top.html" "web_ui_bottom.html")
