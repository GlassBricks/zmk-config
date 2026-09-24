/*
 * Custom e-paper status screen.
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <stdio.h>
#include <string.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
#include <zmk/split/central.h>
#define PERIPHERAL_SOURCE 0
#endif

#include "canvas.h"

LV_IMG_DECLARE(Forest);

#define GAME_LAYER_NAME "GAME"
#define MAC_LAYER_NAME "MAC"

#define PERCENT_FIELD_RIGHT 65
#define SUFFIX_FIELD_LEFT 68

static lv_obj_t *battery_canvas;
static lv_obj_t *battery2_canvas;
static lv_obj_t *mode_canvas;

static uint8_t battery_buf[CANVAS_BUF_SIZE];
static uint8_t battery2_buf[CANVAS_BUF_SIZE];
static uint8_t mode_buf[CANVAS_BUF_SIZE];

static struct {
    uint8_t battery;
    uint8_t peripheral_battery;
    bool charging;
    const char *endpoint_symbol;
    int profile;
    bool game;
    bool mac;
} screen;

static const char *endpoint_symbol(struct zmk_endpoint_instance endpoint) {
    switch (endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        return LV_SYMBOL_USB;
    case ZMK_TRANSPORT_BLE:
        if (zmk_ble_active_profile_is_open()) {
            return LV_SYMBOL_SETTINGS;
        }
        return zmk_ble_active_profile_is_connected() ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE;
    default:
        return LV_SYMBOL_CLOSE;
    }
}

static void draw_percent_and_suffix(lv_obj_t *canvas, const char *percent, const char *suffix) {
    lv_draw_label_dsc_t label_dsc;

    if (percent != NULL) {
        init_label_dsc(&label_dsc, CANVAS_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
        canvas_draw_text(canvas, 0, 0, PERCENT_FIELD_RIGHT, &label_dsc, percent);
    }

    init_label_dsc(&label_dsc, CANVAS_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT);
    canvas_draw_text(canvas, SUFFIX_FIELD_LEFT, 0, CANVAS_SIZE - SUFFIX_FIELD_LEFT, &label_dsc,
                     suffix);
}

static void draw_battery_region(void) {
    static uint8_t last_battery = UINT8_MAX;
    static bool last_charging;
    static const char *last_symbol;

    if (screen.battery == last_battery && screen.charging == last_charging &&
        screen.endpoint_symbol == last_symbol) {
        return;
    }
    last_battery = screen.battery;
    last_charging = screen.charging;
    last_symbol = screen.endpoint_symbol;

    lv_canvas_fill_bg(battery_canvas, CANVAS_BACKGROUND, LV_OPA_COVER);
    draw_battery(battery_canvas, screen.battery, screen.charging);

    char percent[8];
    snprintf(percent, sizeof(percent), "%u%%", screen.battery);
    draw_percent_and_suffix(battery_canvas, percent,
                            screen.endpoint_symbol ? screen.endpoint_symbol : "");

    rotate_canvas(battery_canvas);
}

static void draw_battery2(void) {
    static int last_profile = -1;
    static uint8_t last_peripheral_battery = UINT8_MAX;

    if (screen.profile == last_profile && screen.peripheral_battery == last_peripheral_battery) {
        return;
    }
    last_profile = screen.profile;
    last_peripheral_battery = screen.peripheral_battery;

    lv_canvas_fill_bg(battery2_canvas, CANVAS_BACKGROUND, LV_OPA_COVER);

    char profile[8];
    snprintf(profile, sizeof(profile), "B%d", screen.profile + 1);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    draw_battery(battery2_canvas, screen.peripheral_battery, false);
    char percent[8];
    snprintf(percent, sizeof(percent), "%u%%", screen.peripheral_battery);
    draw_percent_and_suffix(battery2_canvas, percent, profile);
#else
    draw_percent_and_suffix(battery2_canvas, NULL, profile);
#endif

    rotate_canvas(battery2_canvas);
}

static void draw_mode_region(void) {
    static bool last_game, last_mac, primed;

    if (primed && screen.game == last_game && screen.mac == last_mac) {
        return;
    }
    last_game = screen.game;
    last_mac = screen.mac;
    primed = true;

    lv_canvas_fill_bg(mode_canvas, CANVAS_BACKGROUND, LV_OPA_COVER);

    char text[16];
    snprintf(text, sizeof(text), "%s%s", screen.game ? "GAME" : "BASE", screen.mac ? " swp" : "");

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, CANVAS_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT);
    canvas_draw_text(mode_canvas, 0, 0, CANVAS_SIZE, &label_dsc, text);

    rotate_canvas(mode_canvas);
}


struct battery_view {
    uint8_t percent;
    uint8_t peripheral_percent;
    bool charging;
};

static struct battery_view battery_get_state(const zmk_event_t *eh) {
    struct battery_view view = {
        .percent = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .charging = zmk_usb_is_powered(),
#endif
    };

#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    zmk_split_central_get_peripheral_battery_level(PERIPHERAL_SOURCE, &view.peripheral_percent);
#endif

    return view;
}

static void battery_update_cb(struct battery_view state) {
    screen.battery = state.percent;
    screen.peripheral_battery = state.peripheral_percent;
    screen.charging = state.charging;
    draw_battery_region();
    draw_battery2();
}

ZMK_DISPLAY_WIDGET_LISTENER(hlc_battery, struct battery_view, battery_update_cb, battery_get_state)
ZMK_SUBSCRIPTION(hlc_battery, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
ZMK_SUBSCRIPTION(hlc_battery, zmk_peripheral_battery_state_changed);
#endif
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(hlc_battery, zmk_usb_conn_state_changed);
#endif

struct output_view {
    const char *symbol;
    int profile;
};

static struct output_view output_get_state(const zmk_event_t *eh) {
    return (struct output_view){
        .symbol = endpoint_symbol(zmk_endpoint_get_selected()),
        .profile = zmk_ble_active_profile_index(),
    };
}

static void output_update_cb(struct output_view state) {
    screen.endpoint_symbol = state.symbol;
    screen.profile = state.profile;

    draw_battery_region();
    draw_battery2();
}

ZMK_DISPLAY_WIDGET_LISTENER(hlc_output, struct output_view, output_update_cb, output_get_state)
ZMK_SUBSCRIPTION(hlc_output, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(hlc_output, zmk_usb_conn_state_changed);
#endif
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(hlc_output, zmk_ble_active_profile_changed);
#endif

struct mode_view {
    bool game;
    bool mac;
};

static bool layer_active_by_name(const char *name) {
    for (zmk_keymap_layer_index_t i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(i);
        const char *layer_name = zmk_keymap_layer_name(id);
        if (layer_name != NULL && strcmp(layer_name, name) == 0) {
            return zmk_keymap_layer_active(id);
        }
    }
    return false;
}

static struct mode_view mode_get_state(const zmk_event_t *eh) {
    return (struct mode_view){
        .game = layer_active_by_name(GAME_LAYER_NAME),
        .mac = layer_active_by_name(MAC_LAYER_NAME),
    };
}

static void mode_update_cb(struct mode_view state) {
    screen.game = state.game;
    screen.mac = state.mac;

    draw_mode_region();
}

ZMK_DISPLAY_WIDGET_LISTENER(hlc_mode, struct mode_view, mode_update_cb, mode_get_state)
ZMK_SUBSCRIPTION(hlc_mode, zmk_layer_state_changed);

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen_obj = lv_obj_create(NULL);

    lv_obj_t *root = lv_obj_create(screen_obj);
    lv_obj_set_size(root, 184, 88);

    battery_canvas = lv_canvas_create(root);
    lv_obj_align(battery_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(battery_canvas, battery_buf, CANVAS_SIZE, CANVAS_SIZE,
                         CANVAS_COLOR_FORMAT);

    battery2_canvas = lv_canvas_create(root);
    lv_obj_align(battery2_canvas, LV_ALIGN_TOP_LEFT, 18, 0);
    lv_canvas_set_buffer(battery2_canvas, battery2_buf, CANVAS_SIZE, CANVAS_SIZE,
                         CANVAS_COLOR_FORMAT);

    mode_canvas = lv_canvas_create(root);
    lv_obj_align(mode_canvas, LV_ALIGN_TOP_LEFT, 36, 0);
    lv_canvas_set_buffer(mode_canvas, mode_buf, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    lv_obj_t *art = lv_img_create(root);
    lv_image_set_src(art, &Forest);
    lv_obj_align(art, LV_ALIGN_TOP_LEFT, 56 , 0);

    hlc_battery_init();
    hlc_output_init();
    hlc_mode_init();

    return screen_obj;
}
