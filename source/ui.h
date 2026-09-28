#pragma once
#include <citro2d.h>
#include <stdbool.h>
#include "save.h"
#include "stopwatch.h"

/* Hit rectangle for touch input */
typedef struct { float x, y, w, h; } HitRect;

/* Color palette — dark grey background, white text */
#define CLR_BG          C2D_Color32(0x30, 0x30, 0x30, 0xFF)
#define CLR_TEXT        C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
#define CLR_TEXT_DIM    C2D_Color32(0x99, 0x99, 0x99, 0xFF)
#define CLR_BTN         C2D_Color32(0x50, 0x50, 0x50, 0xFF)
#define CLR_MODAL_BG    C2D_Color32(0x40, 0x40, 0x40, 0xFF)
#define CLR_OVERLAY     C2D_Color32(0x00, 0x00, 0x00, 0xA0)

#define CLR_TAB_ACTIVE  C2D_Color32(0x50, 0x50, 0x50, 0xFF)
#define CLR_TAB_INACT   C2D_Color32(0x22, 0x22, 0x22, 0xFF)
#define CLR_TAB_SEP     C2D_Color32(0x40, 0x40, 0x40, 0xFF)

/* Display & Power Modes */
typedef enum {
    SCREEN_MODE_ALL_ON,
    SCREEN_MODE_BOTTOM_OFF,
    SCREEN_MODE_ALL_OFF
} DisplayPowerMode;

/* Modes (Tab bar items) */
typedef enum {
    MODE_ALARM,
    MODE_CLOCK,
    MODE_STOPWATCH,
    MODE_TIMER
} AppMode;

/* --- Tab bar layout constants (docked at y=200, h=40) --- */
extern const HitRect TAB_ALARM;
extern const HitRect TAB_CLOCK;
extern const HitRect TAB_STOPWATCH;
extern const HitRect TAB_TIMER;

/* --- Global header bar hit targets --- */
extern const HitRect BTN_SETTINGS_ICON;

/* --- Shared Time Arrow layout --- */
extern HitRect ARROW_H_UP,   ARROW_H_DOWN;
extern HitRect ARROW_M_UP,   ARROW_M_DOWN;
extern HitRect ARROW_S_UP,   ARROW_S_DOWN;

/* --- Date Edit Column Arrows & Buttons --- */
extern HitRect ARROW_COL1_UP, ARROW_COL1_DOWN;
extern HitRect ARROW_COL2_UP, ARROW_COL2_DOWN;
extern HitRect ARROW_COL3_UP, ARROW_COL3_DOWN;
extern const HitRect BTN_FMT_LEFT;
extern const HitRect BTN_FMT_RIGHT;

void ui_update_date_hitboxes(DateFormat fmt);

/* --- Stopwatch buttons --- */
extern const HitRect BTN_SW_START;
extern const HitRect BTN_SW_LAP;
extern const HitRect BTN_SW_PAUSE;
extern const HitRect BTN_SW_RESUME;
extern const HitRect BTN_SW_RESET;
extern const HitRect BTN_SW_RESET_PAUSED;

/* --- Timer buttons --- */
extern const HitRect BTN_TMR_START;
extern const HitRect BTN_TMR_PAUSE;
extern const HitRect BTN_TMR_RESUME;
extern const HitRect BTN_TMR_RESET;

/* --- Alarm modes & views --- */
typedef enum {
    ALARM_VIEW_LIST,
    STATE_ALARM_ADD,
    STATE_ALARM_EDIT
} AlarmView;

/* --- Alarm buttons --- */
extern const HitRect BTN_ALARM_ADD;
extern const HitRect BTN_ALARM_EDIT_SAVE;
extern const HitRect BTN_ALARM_EDIT_CANCEL;
extern const HitRect BTN_ALARM_EDIT_DELETE;
extern const HitRect BTN_ALARM_REPEAT_LEFT;
extern const HitRect BTN_ALARM_REPEAT_RIGHT;
extern const HitRect BTN_ALARM_TONE_LEFT;
extern const HitRect BTN_ALARM_TONE_RIGHT;
extern const HitRect BTN_ALARM_TONE_PREVIEW;
extern const HitRect BTN_ALARM_LABEL_INPUT;
extern const HitRect BTN_ALARM_DISMISS;
extern const HitRect BTN_ALARM_MISSED_OK;
extern const HitRect BTN_TIMER_DISMISS;

/* Dedicated 2-Column Alarm Stepper Arrows */
extern const HitRect ARROW_ALARM_H_UP,   ARROW_ALARM_H_DOWN;
extern const HitRect ARROW_ALARM_M_UP,   ARROW_ALARM_M_DOWN;

/* --- Settings overlay buttons --- */
extern const HitRect BTN_SET_BACK;
extern const HitRect BTN_SET_MANUAL;
extern const HitRect BTN_SET_SAVE;
extern const HitRect BTN_SET_TIME_DATE;
extern const HitRect BTN_SET_DISPLAY;
extern const HitRect BTN_SET_ABOUT;
extern const HitRect BTN_SET_EDIT_TIME;
extern const HitRect BTN_SET_EDIT_DATE_BTN;
extern const HitRect BTN_SET_RESET;
extern const HitRect BTN_RESET_TIME;
extern const HitRect BTN_SET_EDIT;
extern const HitRect BTN_EDIT_DATE;

/* --- Display & Power buttons --- */
extern const HitRect BTN_DISP_BOTH_OFF;
extern const HitRect BTN_DISP_BOT_OFF;
extern const HitRect BTN_DISP_AUTO_LEFT;
extern const HitRect BTN_DISP_AUTO_RIGHT;

/* --- Modal buttons --- */
extern const HitRect BTN_OK;
extern const HitRect BTN_CANCEL;
extern const HitRect BTN_CONFIRM;

/* --- World Clock buttons --- */
extern const HitRect BTN_CLOCK_ADD;
extern const HitRect BTN_CITY_PICKER_BACK;
extern const HitRect BTN_MODAL_CITY_CANCEL;
extern const HitRect BTN_MODAL_CITY_DEL;
extern const HitRect BTN_MODAL_HOME_CANCEL;
extern const HitRect BTN_MODAL_HOME_SET;

/* --- Shared Drawing Primitives --- */
void draw_button(C2D_TextBuf buf, const HitRect *r, const char *label);
void draw_button_scaled(C2D_TextBuf buf, const HitRect *r, const char *label, float scale, u32 bg_color);
void draw_text_centered_x(C2D_TextBuf buf, const char *str, float y, float scale, float screen_w);
void draw_stepper_arrow_button_horizontal(const HitRect *r, bool is_left);
void ui_set_scissor(GPU_SCISSORMODE mode, u32 x, u32 y, u32 w, u32 h);

/* --- Drawing functions --- */

/* Top screen */
void ui_draw_top_status_bar(C2D_TextBuf buf, u8 wifi_bars, u8 battery_percent, bool is_charging);
void ui_draw_top_clock_with_date(C2D_TextBuf buf, int h, int m, int s, const char* date_str);
void ui_draw_top_clock_with_home(C2D_TextBuf buf, int h, int m, int s, const char* date_str, const char* home_city, const char* home_country);
void ui_draw_top_clock_with_alarm_status(C2D_TextBuf buf, int h, int m, int s, const char* date_str, const char* alarm_status);
void ui_draw_top_stopwatch(C2D_TextBuf buf, const Stopwatch* sw, int hh, int mm, int ss, int cs, bool show_hours);
void ui_draw_top_timer(C2D_TextBuf buf, int hh, int mm, int ss);

/* Bottom screen — Header & Navigation */
void ui_draw_header(C2D_TextBuf buf, C2D_Image settings_icon, const char* title);
void ui_draw_tab_bar(C2D_TextBuf buf, AppMode active, const C2D_Image tab_icons[4]);

/* Bottom screen — Tab modes */
typedef struct {
    float scroll_y;
    float touch_start_y;
    float touch_start_x;
    bool  is_dragging;
    bool  potential_tap;
    int   candidate_index;
    bool  candidate_is_toggle;
    bool  candidate_is_delete;
    int   selected_index;
} AlarmListState;

typedef enum {
    CLOCK_VIEW_LIST,
    CLOCK_VIEW_PICKER
} ClockView;

typedef struct {
    float scroll_y;
    float touch_start_y;
    float touch_start_x;
    bool  is_dragging;
    bool  potential_tap;
    int   candidate_index;
    bool  candidate_is_delete;
    int   selected_index;
} WorldClockListState;

typedef struct {
    float scroll_y;
    float touch_start_y;
    float touch_start_x;
    bool  is_dragging;
    bool  potential_tap;
    int   candidate_index;
    int   selected_index;
} CityPickerState;

void ui_draw_alarm_list(C2D_TextBuf buf, SaveData* save, AlarmListState* state, C2D_Image settings_icon, C2D_Image trash_icon);
void ui_draw_alarm_edit(C2D_TextBuf buf, int h, int m, u8 repeat_mode, u8 ringtone_id, bool is_new, const char* ringtone_name, const char* label, bool is_playing);
void ui_draw_alarm_delete_confirm(C2D_TextBuf buf);
void ui_draw_alarm_ringing_top(C2D_TextBuf buf, int h, int m, u8 repeat_mode, const char* label, u32 frame_counter);
void ui_draw_alarm_ringing_bottom(C2D_TextBuf buf, int h, int m, u8 repeat_mode, const char* label);
void ui_draw_alarm_missed_modal(C2D_TextBuf buf, int missed_count);
void ui_draw_clock_bottom(C2D_TextBuf buf);
void ui_draw_world_clock_list(C2D_TextBuf buf, SaveData* save, WorldClockListState* state, C2D_Image settings_icon, C2D_Image trash_icon);
void ui_draw_city_picker(C2D_TextBuf buf, const SaveData* save, CityPickerState* state);
void ui_draw_world_clock_delete_confirm(C2D_TextBuf buf, u8 city_id);
void ui_draw_world_clock_set_home_confirm(C2D_TextBuf buf, u8 city_id);
void ui_draw_world_clock_already_home(C2D_TextBuf buf, u8 city_id);


void ui_draw_stopwatch_idle(C2D_TextBuf buf);
void ui_draw_stopwatch_running(C2D_TextBuf buf);
void ui_draw_stopwatch_paused(C2D_TextBuf buf);
void ui_draw_timer_adjust(C2D_TextBuf buf, int h, int m, int s);
void ui_draw_timer_running(C2D_TextBuf buf);
void ui_draw_timer_paused(C2D_TextBuf buf);
void ui_draw_timer_ringing_top(C2D_TextBuf buf, int h, int m, int s, u32 frame_counter);
void ui_draw_timer_ringing_bottom(C2D_TextBuf buf, int h, int m, int s);

/* Bottom screen — Settings overlay screens */
void ui_draw_settings_main(C2D_TextBuf buf);
void ui_draw_settings_time_date_menu(C2D_TextBuf buf);
void ui_draw_settings_edit_time(C2D_TextBuf buf, int h, int m, int s);
void ui_draw_settings_edit_date(C2D_TextBuf buf, int y, int m, int d, DateFormat fmt);
void ui_draw_settings_display(C2D_TextBuf buf, u8 auto_sleep_idx);
void ui_draw_modal_confirm(C2D_TextBuf buf);
void ui_draw_modal_confirm_reset_time(C2D_TextBuf buf);
void ui_draw_modal_timer_zero(C2D_TextBuf buf);
void ui_draw_modal_success(C2D_TextBuf buf, const char* msg);

/* First boot */
void ui_draw_first_boot(C2D_TextBuf buf, bool show_ok);
