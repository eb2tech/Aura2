#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *settings;
    lv_obj_t *current_conditions_image;
    lv_obj_t *current_temperature_label;
    lv_obj_t *obj0;
    lv_obj_t *feels_temperature_label;
    lv_obj_t *current_time_label;
    lv_obj_t *toggle_forecast_container;
    lv_obj_t *temperature_grid;
    lv_obj_t *forecast_type_label;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *ipv4_label;
    lv_obj_t *mdns_label;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_SETTINGS = 2,
};

void create_screen_main();
void tick_screen_main();

void create_screen_settings();
void tick_screen_settings();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/