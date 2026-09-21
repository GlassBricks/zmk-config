/*
 * Canvas helpers for the 1bpp e-paper panel, adapted from
 * zmk-halcyon-module/boards/shields/mod_display_epaper/widgets/util.{c,h}.
 */

#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

#define CANVAS_SIZE 88
/* Smallest colour format lv_draw_sw_rotate supports. */
#define CANVAS_COLOR_FORMAT LV_COLOR_FORMAT_L8
#define CANVAS_BUF_SIZE                                                                            \
    LV_CANVAS_BUF_SIZE(CANVAS_SIZE, CANVAS_SIZE, LV_COLOR_FORMAT_GET_BPP(CANVAS_COLOR_FORMAT),     \
                       LV_DRAW_BUF_STRIDE_ALIGN)

#define CANVAS_BACKGROUND lv_color_white()
#define CANVAS_FOREGROUND lv_color_black()

void rotate_canvas(lv_obj_t *canvas);
void draw_battery(lv_obj_t *canvas, uint8_t percent, bool charging);

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
                    lv_text_align_t align);
void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color);

void canvas_draw_rect(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                      lv_draw_rect_dsc_t *draw_dsc);
void canvas_draw_text(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t max_w,
                      lv_draw_label_dsc_t *draw_dsc, const char *txt);
void canvas_draw_img(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, const lv_image_dsc_t *src,
                     lv_draw_image_dsc_t *draw_dsc);
