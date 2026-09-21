/*
 * Custom e-paper status screen.
 *
 * Refresh discipline is the whole point of this file: a partial refresh of the
 * SSD1680 costs a second or so of current, so every draw_* recomputes what it
 * would render, compares it against the previous value and returns early when
 * nothing changed. In particular the mode region derives only from whether the
 * GAME and MAC layers are active, so holding NUM/SYM/EXT/FUN/ADJ fires the
 * layer event but redraws nothing.
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

#include "canvas.h"

LV_IMG_DECLARE(Forest);

/*
 * Matched against the keymap's `display-name`s rather than layer indices, so
 * that renumbering layers in the keymap cannot silently desync this file.
 */
#define GAME_LAYER_NAME "GAME"
#define MAC_LAYER_NAME "MAC"

static lv_obj_t *battery_canvas;
static lv_obj_t *mode_canvas;
static lv_obj_t *profile_canvas;

static uint8_t battery_buf[CANVAS_BUF_SIZE];
static uint8_t mode_buf[CANVAS_BUF_SIZE];
static uint8_t profile_buf[CANVAS_BUF_SIZE];

/* Only ever touched from the display work queue. */
static struct {
    uint8_t battery;
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

    char text[16];
    snprintf(text, sizeof(text), "%u%% %s", screen.battery,
             screen.endpoint_symbol ? screen.endpoint_symbol : "");

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, CANVAS_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    canvas_draw_text(battery_canvas, 0, 0, CANVAS_SIZE, &label_dsc, text);

    rotate_canvas(battery_canvas);
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

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, CANVAS_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT);
    canvas_draw_text(mode_canvas, 0, 0, CANVAS_SIZE, &label_dsc, screen.game ? "GAME" : "BASE");

    /*
     * A second line: "GAME MAC" in montserrat_16 is 93px, wider than the 88px
     * canvas, so a single line wraps the "MAC" out from under the visible strip.
     */
    if (screen.mac) {
        lv_draw_label_dsc_t mac_dsc;
        init_label_dsc(&mac_dsc, CANVAS_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_LEFT);
        canvas_draw_text(mode_canvas, 0, 18, CANVAS_SIZE, &mac_dsc, "MAC");
    }

    rotate_canvas(mode_canvas);
}

static void draw_profile_region(void) {
    static int last_profile = -1;

    if (screen.profile == last_profile) {
        return;
    }
    last_profile = screen.profile;

    lv_canvas_fill_bg(profile_canvas, CANVAS_BACKGROUND, LV_OPA_COVER);

    char text[16];
    snprintf(text, sizeof(text), "BT %d", screen.profile + 1);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, CANVAS_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT);
    canvas_draw_text(profile_canvas, 0, 0, CANVAS_SIZE, &label_dsc, text);

    rotate_canvas(profile_canvas);
}

struct battery_view {
    uint8_t percent;
    bool charging;
};

static struct battery_view battery_get_state(const zmk_event_t *eh) {
    return (struct battery_view){
        .percent = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .charging = zmk_usb_is_powered(),
#endif
    };
}

static void battery_update_cb(struct battery_view state) {
    screen.battery = state.percent;
    screen.charging = state.charging;
    draw_battery_region();
}

ZMK_DISPLAY_WIDGET_LISTENER(hlc_battery, struct battery_view, battery_update_cb, battery_get_state)
ZMK_SUBSCRIPTION(hlc_battery, zmk_battery_state_changed);
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
    draw_profile_region();
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

    profile_canvas = lv_canvas_create(root);
    lv_obj_align(profile_canvas, LV_ALIGN_TOP_LEFT, 24, 0);
    lv_canvas_set_buffer(profile_canvas, profile_buf, CANVAS_SIZE, CANVAS_SIZE,
                         CANVAS_COLOR_FORMAT);

    mode_canvas = lv_canvas_create(root);
    lv_obj_align(mode_canvas, LV_ALIGN_TOP_LEFT, 44, 0);
    lv_canvas_set_buffer(mode_canvas, mode_buf, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    lv_obj_t *art = lv_img_create(root);
    lv_image_set_src(art, &Forest);
    lv_obj_align(art, LV_ALIGN_TOP_LEFT, 74, 0);

    hlc_battery_init();
    hlc_output_init();
    hlc_mode_init();

    return screen_obj;
}
