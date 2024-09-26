#include "esp_log.h"

#include "screen/screen.h"
#include "web_server.h"

#define TAG "screen"

extern const uint8_t montmedium_font_82x[] U8G2_FONT_SECTION("montmedium_font_82x");
extern const uint8_t battery_font24[] U8G2_FONT_SECTION("battery_font24");
extern const uint8_t myicon_font24[] U8G2_FONT_SECTION("myicon_font24");
extern const uint8_t wifi_font24[] U8G2_FONT_SECTION("wifi_font24");
extern const uint8_t wifi_font64[] U8G2_FONT_SECTION("wifi_font64");
extern const uint8_t menu_font64[] U8G2_FONT_SECTION("menu_font64");

ug_base *mainScreen;
ug_base *ui_hour;
ug_base *ui_min;
ug_base *ui_date;
ug_base *ui_temp;
ug_base *ui_humi;
ug_base *ui_humi_png;
ug_base *ui_battery_charge;
ug_base *ui_battery;
ug_base *ui_wifi;
ug_base *ui_trigger;

ug_base *ui_menu_wifi;
ug_base *ui_menu_setting;
ug_base *ui_menu_back;

ug_base *menuWifiScreenconfigwifi;
ug_base *menuWifiBack;

ug_base *menuSettingScreenconfig;
ug_base *menuSettingBack;

static void ug_menuSettingScreen_init(void);
static void ug_menuScreen_init(void);


static TaskHandle_t webserver_task_handle = NULL;
static void webserver_task(void* arg)
{
    int ret;

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    esp_web_server_init();

    u8g2_ClearBuffer(get_u8g2());
    u8g2_SetDrawColor(get_u8g2(), 1);
    u8g2_SetFont(get_u8g2(), u8g2_font_logisoso24_tf);
    u8g2_DrawUTF8(get_u8g2(), 40, 90, "AP:configwifi");
    u8g2_DrawUTF8(get_u8g2(), 40, 130, "IP:192.168.4.1");
    ug_base_flush(ui_menu_wifi);

    u8g2_NextPage(get_u8g2());

    while (1) {
        ret = esp_web_server_state();
        if (ret == 0x4) {
            ESP_LOGI(TAG, "webserver_task: 0x4");

            u8g2_ClearBuffer(get_u8g2());
            u8g2_SetDrawColor(get_u8g2(), 1);
            u8g2_SetFont(get_u8g2(), u8g2_font_logisoso24_tf);
            u8g2_DrawUTF8(get_u8g2(), 10, 90, "wifi config success");
            ug_base_flush(ui_menu_wifi);
            u8g2_NextPage(get_u8g2());

            vTaskDelay(2000 / portTICK_PERIOD_MS);

            webserver_task_handle = NULL;

            ug_input_proc(UG_KEY_ENTER);

            vTaskDelay(2000 / portTICK_PERIOD_MS);
            vTaskDelete(NULL);
        } else if (ret == 0x8) {
            ESP_LOGI(TAG, "webserver_task: 0x8");
            u8g2_ClearBuffer(get_u8g2());
            u8g2_SetDrawColor(get_u8g2(), 1);
            u8g2_SetFont(get_u8g2(), u8g2_font_logisoso24_tf);
            u8g2_DrawUTF8(get_u8g2(), 20, 90, "wifi config failed");
            ug_base_flush(ui_menu_wifi);
            u8g2_NextPage(get_u8g2());

            vTaskDelay(2000 / portTICK_PERIOD_MS);

            webserver_task_handle = NULL;

            ug_input_proc(UG_KEY_ENTER);

            vTaskDelay(2000 / portTICK_PERIOD_MS);
            vTaskDelete(NULL);
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

static int menuWifi_event_cb(struct ug_base_st *base, int event)
{

    if (event != UG_KEY_ENTER)
        return 0;

    if (base == menuWifiBack) {
        base->selected = false;

        ug_set_curscreen(ui_trigger);
        ug_set_focus(ui_menu_wifi);

        ug_base_rmall_child(ui_menu_wifi);
    } else if (base == menuWifiScreenconfigwifi) {
        if (base->selected) {
            ug_base_enable_visible(menuWifiBack, false);

            ug_base_enable_bg(menuWifiScreenconfigwifi, true);
            ug_base_set_pos(menuWifiScreenconfigwifi, 40, 30);

            u8g2_SetFont(get_u8g2(), u8g2_font_logisoso24_tf);
            u8g2_DrawUTF8(get_u8g2(), 40, 90, "init wifi ap ...");

            ug_base_enable_visible(menuWifiBack, false);

            xTaskCreate(webserver_task, "webserver_task", 4096, NULL, 5, &webserver_task_handle);
        } else {
            ESP_LOGI(TAG, "menuWifiScreenconfigwifi deselected");

            ug_base_enable_bg(menuWifiScreenconfigwifi, false);
            ug_base_set_pos(menuWifiScreenconfigwifi, 40, 40);

            ug_base_enable_visible(menuWifiBack, true);

            if (webserver_task_handle)
                vTaskDelete(webserver_task_handle);

            esp_web_server_stop();

            ESP_LOGI(TAG, "menuWifiScreenconfigwifi deselected exit");
        }
    }

    return 0;
}

static void ug_menuWifiScreen_init(void)
{
    menuWifiScreenconfigwifi = create_base(ui_menu_wifi, UG_TYPE_ITEM);
    ug_base_set_font(menuWifiScreenconfigwifi, u8g2_font_logisoso24_tf);
    ug_base_set_context_type(menuWifiScreenconfigwifi, TYPE_TEXT);
    ug_base_set_context(menuWifiScreenconfigwifi, "ConfigWifi");
    ug_base_set_pos(menuWifiScreenconfigwifi, 40, 40);
    ug_base_enable_focus(menuWifiScreenconfigwifi, true);
    menuWifiScreenconfigwifi->outline_pad.pad_w = 6;
    menuWifiScreenconfigwifi->cb = menuWifi_event_cb;

    menuWifiBack = create_base(ui_menu_wifi, UG_TYPE_ITEM);
    ug_base_set_font(menuWifiBack, u8g2_font_logisoso24_tf);
    ug_base_set_context_type(menuWifiBack, TYPE_TEXT);
    ug_base_set_context(menuWifiBack, "BACK");
    ug_base_set_pos(menuWifiBack, 40, 110);
    ug_base_enable_focus(menuWifiBack, true);
    menuWifiBack->cb = menuWifi_event_cb;
    menuWifiBack->outline_pad.pad_w = 6;
}

static int menuSetting_event_cb(struct ug_base_st *base, int event)
{
    if (event != UG_KEY_ENTER)
        return 0;

    if (base == menuSettingBack) {
        base->selected = false;

        ug_set_curscreen(ui_trigger);
        ug_set_focus(ui_menu_setting);

        ug_base_rmall_child(ui_menu_setting);
    } else if (base == menuSettingScreenconfig) {
        if (base->selected) {
            ug_base_enable_bg(menuSettingScreenconfig, true);
            ug_base_set_pos(menuSettingScreenconfig, 40, 30);

            u8g2_SetDrawColor(get_u8g2(), 1);
            u8g2_SetFont(get_u8g2(), u8g2_font_logisoso24_tf);
            u8g2_DrawUTF8(get_u8g2(), 40, 90, "calibrate time ...");

            ug_base_enable_visible(menuSettingBack, false);
        } else {
            ug_base_enable_bg(menuSettingScreenconfig, false);
            ug_base_set_pos(menuSettingScreenconfig, 40, 40);

            ug_base_enable_visible(menuSettingBack, true);
        }
    }

    return 0;
}

static int menu_event_cb(struct ug_base_st *base, int event)
{
    if (event != UG_KEY_ENTER)
        return 0;

    if (base == ui_trigger) {
        ESP_LOGI(TAG, "entry menu\n");

        ug_menuScreen_init();
    } else if (base == ui_menu_back) {
        base->selected = false;

        ug_set_curscreen(mainScreen);
        ug_set_focus(ui_trigger);

        ug_base_rmall_child(ui_trigger);
    } else if (base == ui_menu_wifi) {
        ESP_LOGI(TAG, "entry menu wifi\n");
        ug_menuWifiScreen_init();
    } else if (base == ui_menu_setting) {
        ESP_LOGI(TAG, "entry menu Setting\n");
        ug_menuSettingScreen_init();
    }

    return 0;
}

static void ug_menuSettingScreen_init(void)
{
    menuSettingScreenconfig = create_base(ui_menu_setting, UG_TYPE_ITEM);
    ug_base_set_font(menuSettingScreenconfig, u8g2_font_logisoso24_tf);
    ug_base_set_context_type(menuSettingScreenconfig, TYPE_TEXT);
    ug_base_set_context(menuSettingScreenconfig, "Cal Time");
    ug_base_set_pos(menuSettingScreenconfig, 40, 40);
    ug_base_enable_focus(menuSettingScreenconfig, true);
    menuSettingScreenconfig->outline_pad.pad_w = 6;
    menuSettingScreenconfig->cb = menuSetting_event_cb;

    menuSettingBack = create_base(ui_menu_setting, UG_TYPE_ITEM);
    ug_base_set_font(menuSettingBack, u8g2_font_logisoso24_tf);
    ug_base_set_context_type(menuSettingBack, TYPE_TEXT);
    ug_base_set_context(menuSettingBack, "BACK");
    ug_base_set_pos(menuSettingBack, 40, 110);
    ug_base_enable_focus(menuSettingBack, true);
    menuSettingBack->cb = menuSetting_event_cb;
    menuSettingBack->outline_pad.pad_w = 6;
}

static void ug_menuScreen_init(void)
{
    ui_menu_wifi = create_base(ui_trigger, UG_TYPE_MENU);
    ug_base_set_font(ui_menu_wifi, wifi_font64);
    ug_base_set_context_type(ui_menu_wifi, TYPE_GLYPH);
    ug_base_set_glph_encoder(ui_menu_wifi, 10);
    ug_base_set_pos(ui_menu_wifi, 40, 80);
    ug_base_enable_focus(ui_menu_wifi, true);
    ui_menu_wifi->w = 64;
    ui_menu_wifi->h = 64;
    ui_menu_wifi->cb = menu_event_cb;

    ui_menu_setting = create_base(ui_trigger, UG_TYPE_MENU);
    ug_base_set_font(ui_menu_setting, menu_font64);
    ug_base_set_context_type(ui_menu_setting, TYPE_GLYPH);
    ug_base_set_glph_encoder(ui_menu_setting, 10);
    ug_base_set_pos(ui_menu_setting, 120, 80);
    ug_base_enable_focus(ui_menu_setting, true);
    ui_menu_setting->w = 64;
    ui_menu_setting->h = 64;
    ui_menu_setting->cb = menu_event_cb;

    ui_menu_back = create_base(ui_trigger, UG_TYPE_ITEM);
    ug_base_set_font(ui_menu_back, menu_font64);
    ug_base_set_context_type(ui_menu_back, TYPE_GLYPH);
    ug_base_set_glph_encoder(ui_menu_back, 11);
    ug_base_set_pos(ui_menu_back, 200, 80);
    ug_base_enable_focus(ui_menu_back, true);
    ui_menu_back->w = 64;
    ui_menu_back->h = 64;

    ui_menu_back->cb = menu_event_cb;
}

static void status_bar_init(void)
{
    int start_x = 277;
    int inv_x   = 24;
    int cnt = 0;

    ui_battery_charge = create_base(mainScreen, UG_TYPE_ITEM);
    ug_base_set_font(ui_battery_charge, battery_font24);
    ug_base_set_context_type(ui_battery_charge, TYPE_GLYPH);
    ug_base_set_glph_encoder(ui_battery_charge, 16);
    ug_base_set_pos(ui_battery_charge, start_x, 26);

    cnt++;
    ui_battery = create_base(mainScreen, UG_TYPE_ITEM);
    ug_base_set_font(ui_battery, battery_font24);
    ug_base_set_context_type(ui_battery, TYPE_GLYPH);
    ug_base_set_glph_encoder(ui_battery, 14);
    ug_base_set_pos(ui_battery, start_x - cnt * inv_x + 6, 26);

    cnt++;
    ui_wifi = create_base(mainScreen, UG_TYPE_ITEM);
    ug_base_set_font(ui_wifi, wifi_font24);
    ug_base_set_glph_encoder(ui_wifi, 10);
    ug_base_set_context_type(ui_wifi, TYPE_GLYPH);
    ug_base_set_pos(ui_wifi, start_x - cnt * inv_x, 26);
}


void ug_mainScreen_init(void)
{
    mainScreen = create_base(NULL, UG_TYPE_MENU);

    ui_hour = create_base(mainScreen, UG_TYPE_ITEM);
    ug_base_set_context_type(ui_hour, TYPE_TEXT);
    ug_base_enable_bg(ui_hour, true);
    ug_base_set_pos(ui_hour, 25, 100);
    ug_base_set_font(ui_hour, montmedium_font_82x);
    ug_base_set_context(ui_hour, "8");
    ui_hour->w = 108;
    ui_hour->bg_pad.pad_w = 5;
    ui_hour->bg_pad.pad_h = 10;
    ui_hour->bg_pad.pad_r = 8;

    ui_min = create_base(mainScreen, UG_TYPE_ITEM);
    ug_base_set_context_type(ui_min, TYPE_TEXT);
    ug_base_enable_bg(ui_min, true);
    ug_base_set_pos(ui_min, 25 + 118 + 20, 100);
    ug_base_set_font(ui_min, montmedium_font_82x);
    ug_base_set_context(ui_min, "00");
    ui_min->w = 108;
    ui_min->bg_pad.pad_w = 5;
    ui_min->bg_pad.pad_h = 10;
    ui_min->bg_pad.pad_r = 8;

    ui_date = create_base(mainScreen, UG_TYPE_ITEM);
    ug_base_set_context_type(ui_date, TYPE_TEXT);
    ug_base_set_pos(ui_date, 180, 145);
    ug_base_set_font(ui_date, u8g2_font_inb24_mf);
    ug_base_set_context(ui_date, "01-01");

    ui_temp = create_base(mainScreen, UG_TYPE_ITEM);
    ug_base_set_font(ui_temp, u8g2_font_logisoso24_tf);
    ug_base_set_context_type(ui_temp, TYPE_TEXT);
    ug_base_set_context(ui_temp, "25\xc2\xb0""C");
    ug_base_set_pos(ui_temp, 15, 145);

    ui_humi = create_base(mainScreen, UG_TYPE_ITEM);
    ug_base_set_font(ui_humi, u8g2_font_logisoso24_tf);
    ug_base_set_context_type(ui_humi, TYPE_TEXT);
    ug_base_set_context(ui_humi, "60");
    ug_base_set_pos(ui_humi, 110, 145);

    ui_humi_png = create_base(mainScreen, UG_TYPE_ITEM);
    ug_base_set_font(ui_humi_png, myicon_font24);
    ug_base_set_context_type(ui_humi_png, TYPE_GLYPH);
    ug_base_set_glph_encoder(ui_humi_png, 17);
    ug_base_set_pos(ui_humi_png, 78, 150);

    status_bar_init();

    ui_trigger = create_base(mainScreen, UG_TYPE_MENU);

    ui_trigger->can_focus = true;
    ui_trigger->cb = menu_event_cb;
    ug_set_focus(ui_trigger);
}
