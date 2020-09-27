/****************************************************************************
 *   Copyright  2020  Jakub Vesely
 *   Email: jakub_vesely@seznam.cz
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include <TTGO.h>
#include <SPIFFS.h>

#include "alarm_clock.h"
#include "alarm_clock_main.h"
#include "alarm_clock_setup.h"
#include "alarm_in_progress.h"
#include "gui/mainbar/app_tile/app_tile.h"
#include "gui/mainbar/main_tile/main_tile.h"
#include "gui/mainbar/mainbar.h"
#include "gui/statusbar.h"
#include "hardware/json_psram_allocator.h"
#include "hardware/powermgm.h"
#include "hardware/rtcctl.h"
#include "hardware/timesync.h"


#define VERSION_KEY "version"
#define VIBE_KEY "vibe"
#define FADE_KEY "fade"
#define CONFIG_FILE_PATH "/alarm_clock.json"

#define AM "AM"
#define PM "PM"

#define AM_ONE "A"
#define PM_ONE "P"
#define LABEL_MAX_SIZE 11

static const char alarm_clock_week_day_2[7][3] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"}; 
static const char alarm_clock_week_day_3[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// declare you images or fonts you need
LV_IMG_DECLARE(alarm_clock_64px);
LV_IMG_DECLARE(alarm_clock_48px);

static uint32_t main_tile_num;
static uint32_t setup_tile_num;
static alarm_properties_t properties;

#ifdef ALARM_CLOCK_WIDGET
    static lv_obj_t *alarm_clock_widget_label;
    static lv_obj_t *alarm_clock_widget_cont;
#endif

// declare callback functions
static void enter_alarm_clock_event_cb( lv_obj_t * obj, lv_event_t event );

static void load_data(){
    if (! SPIFFS.exists( CONFIG_FILE_PATH ) ) {
        return; //wil be used default values set during theier creation
    }

    fs::File file = SPIFFS.open( CONFIG_FILE_PATH, FILE_READ );
    if (!file) {
        log_e("Can't open file: %s!", CONFIG_FILE_PATH );
        return;
    }

    int filesize = file.size();
    SpiRamJsonDocument doc( filesize * 2 );

    DeserializationError error = deserializeJson( doc, file );
    if ( error ) {
        log_e("update check deserializeJson() failed: %s", error.c_str() );
        return;
    }

    properties.vibe = doc[VIBE_KEY].as<bool>();
    properties.fade = doc[FADE_KEY].as<bool>();

    doc.clear();
    file.close();
}

static void store_data(){
    if ( SPIFFS.exists( CONFIG_FILE_PATH ) ) {
        SPIFFS.remove( CONFIG_FILE_PATH );
        log_i("remove old binary weather config");
    }

    fs::File file = SPIFFS.open( CONFIG_FILE_PATH, FILE_WRITE );
    if (!file) {
        log_e("Can't open file: %s!", CONFIG_FILE_PATH );
        return;
    }

    SpiRamJsonDocument doc( 1000 );

    doc[VERSION_KEY] = 1;
    doc[VIBE_KEY] = properties.vibe;
    doc[FADE_KEY] = properties.fade;

    if ( serializeJsonPretty( doc, file ) == 0) {
        log_e("Failed to write config file");
    }

    doc.clear();
    file.close();
}

static void create_alarm_app_icon(){
    // create an app icon, label it and get the lv_obj_t icon container
    lv_obj_t * alarm_clock_icon_cont = app_tile_register_app( "alarm");
    // set your own icon and register her callback to activate by an click
    // remember, an app icon must have an size of 64x64 pixel with an alpha channel
    // use https://lvgl.io/tools/imageconverter to convert your images and set "true color with alpha" to get fancy images
    // the resulting c-file can put in /app/examples/images/
    lv_obj_t * alarm_clock_icon = lv_imgbtn_create( alarm_clock_icon_cont, NULL );
    lv_imgbtn_set_src( alarm_clock_icon, LV_BTN_STATE_RELEASED, &alarm_clock_64px);
    lv_imgbtn_set_src( alarm_clock_icon, LV_BTN_STATE_PRESSED, &alarm_clock_64px);
    lv_imgbtn_set_src( alarm_clock_icon, LV_BTN_STATE_CHECKED_RELEASED, &alarm_clock_64px);
    lv_imgbtn_set_src( alarm_clock_icon, LV_BTN_STATE_CHECKED_PRESSED, &alarm_clock_64px);
    lv_obj_reset_style_list( alarm_clock_icon, LV_OBJ_PART_MAIN );
    lv_obj_align( alarm_clock_icon , alarm_clock_icon_cont, LV_ALIGN_IN_TOP_LEFT, 0, 0 );
    lv_obj_set_event_cb( alarm_clock_icon, enter_alarm_clock_event_cb );

    // make app icon drag scroll the mainbar
    mainbar_add_slide_element(alarm_clock_icon);
}

static void main_tile_activate_callback (){
    alarm_clock_main_set_data_to_display(rtcctl_get_alarm_data(), timesync_get_24hr());
}

static void main_tile_hibernate_callback (){
    rtcctl_set_alarm(alarm_clock_main_get_data_to_store());
}

static void setup_tile_activate_callback (){
    alarm_clock_setup_set_data_to_display(&properties);
}

static void setup_tile_hibernate_callback (){
   properties = *alarm_clock_setup_get_data_to_store();
   store_data();
}

static void create_alarm_main_tile(uint32_t tile_num ){
    alarm_clock_main_setup( main_tile_num );
    mainbar_add_tile_activate_cb(tile_num, main_tile_activate_callback);
    mainbar_add_tile_hibernate_cb(tile_num, main_tile_hibernate_callback);
}

static void create_alarm_setup_tile(uint32_t tile_num){
    alarm_clock_setup_setup( tile_num );
    mainbar_add_tile_activate_cb(tile_num, setup_tile_activate_callback);
    mainbar_add_tile_hibernate_cb(tile_num, setup_tile_hibernate_callback);
}

static void create_alarm_in_progress_tile(){
    alarm_in_progress_tile_setup();
}

#ifdef ALARM_CLOCK_WIDGET
    static void alarm_term_changed_cb(EventBits_t event ){
        switch ( event ){
            case ( RTCCTL_ALARM_ENABLED):
            case ( RTCCTL_ALARM_DISABLED):
            case ( RTCCTL_ALARM_TERM_SET ):
                lv_label_set_text( alarm_clock_widget_label, alarm_clock_get_clock_label(true));
                break;
        }
        //content has been changed text position must be recalculated
        lv_obj_align( alarm_clock_widget_label, alarm_clock_widget_cont, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    }
#endif

static void create_widget_icon(){
#ifdef ALARM_CLOCK_WIDGET
    // app icon container
    // get an widget container from main_tile
    // remember, an widget icon must have an size of 64x64 pixel
    // total size of the container is 64x80 pixel, the bottom 16 pixel is for your label
    alarm_clock_widget_cont = main_tile_register_widget();
    lv_obj_t *alarm_clock_widget_icon = lv_imgbtn_create( alarm_clock_widget_cont, NULL );
    lv_imgbtn_set_src( alarm_clock_widget_icon, LV_BTN_STATE_RELEASED, &alarm_clock_48px);
    lv_imgbtn_set_src( alarm_clock_widget_icon, LV_BTN_STATE_PRESSED, &alarm_clock_48px);
    lv_imgbtn_set_src( alarm_clock_widget_icon, LV_BTN_STATE_CHECKED_RELEASED, &alarm_clock_48px);
    lv_imgbtn_set_src( alarm_clock_widget_icon, LV_BTN_STATE_CHECKED_PRESSED, &alarm_clock_48px);
    lv_obj_reset_style_list( alarm_clock_widget_icon, LV_OBJ_PART_MAIN );
    lv_obj_align( alarm_clock_widget_icon , alarm_clock_widget_cont, LV_ALIGN_IN_TOP_MID, 0, 0 );
    lv_obj_set_event_cb( alarm_clock_widget_icon, enter_alarm_clock_event_cb );

    // make widget icon drag scroll the mainbar
    mainbar_add_slide_element(alarm_clock_widget_icon);

    // label your widget
    alarm_clock_widget_label = lv_label_create( alarm_clock_widget_cont , NULL); 
    alarm_clock_get_clock_label(true);
    lv_label_set_align(alarm_clock_widget_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_reset_style_list( alarm_clock_widget_label, LV_OBJ_PART_MAIN );
    lv_obj_align( alarm_clock_widget_label, alarm_clock_widget_cont, LV_ALIGN_IN_BOTTOM_MID, 0, 0);

    // RTCCTL_ALARM_TERM_SET doesn't have to be caught because alarm is disabled when new alarm is set. After the set it is enabled again
    rtcctl_register_cb(RTCCTL_ALARM_ENABLED | RTCCTL_ALARM_DISABLED| RTCCTL_ALARM_TERM_SET , alarm_term_changed_cb);
#endif // ALARM_CLOCK_WIDGET
}

static void  alarm_occurred_event_event_callback ( EventBits_t event ){
    switch ( event ){
        case ( RTCCTL_ALARM_OCCURRED ):
            alarm_in_progress_start_alarm();
            rtcctl_set_next_alarm();
            break;
    }
}

void powermgmt_callback( EventBits_t event ){
    switch( event ) {
        case( POWERMGM_STANDBY ):
            alarm_in_progress_finish_alarm();
            break;
    }
}

// setup routine for example app
void alarm_clock_setup( void ) {
    load_data();

    create_alarm_app_icon();
    create_widget_icon();

    // register 2 vertical tiles and get the first tile number and save it for later use
    main_tile_num = mainbar_add_app_tile( 1, 2 );
    create_alarm_main_tile(main_tile_num);

    setup_tile_num = main_tile_num + 1;
    create_alarm_setup_tile(setup_tile_num);
    create_alarm_in_progress_tile();

    rtcctl_register_cb(RTCCTL_ALARM_OCCURRED , alarm_occurred_event_event_callback);
    powermgm_register_cb(POWERMGM_STANDBY, powermgmt_callback);
}

uint32_t alarm_clock_get_app_main_tile_num( void ) {
    return( main_tile_num );
}

uint32_t alarm_clock_get_app_setup_tile_num( void ) {
    return( setup_tile_num );
}

static void enter_alarm_clock_event_cb( lv_obj_t * obj, lv_event_t event ) {
    switch( event ) {
        case( LV_EVENT_CLICKED ):
            statusbar_hide( true );
            mainbar_jump_to_tilenumber( main_tile_num, LV_ANIM_OFF );
            break;
    }
}

alarm_properties_t * alarm_clock_get_properties(){
    return &properties;
}

int alarm_clock_get_am_pm_hour(int hour24){
    //FIXME: taken from main_tile.cpp. It would be good to place common function somewhere and use it on both places
    if (hour24 == 0){
        return 12;
    }
    if (hour24 > 12){
        return hour24 - 12;
    }
    return hour24;
}

char const* alarm_clock_get_am_pm_value(int hour24, bool short_format){
    if (hour24 < 12){
        return short_format ? AM_ONE : AM;
    }

    return short_format ? PM_ONE : PM;
}

char const * alarm_clock_get_week_day(int index, bool short_format){
    return short_format ? alarm_clock_week_day_2[index] : alarm_clock_week_day_3[index];
}

char * alarm_clock_get_clock_label(bool show_day)
{
    static char text[LABEL_MAX_SIZE]; //DoW + '\n' + HH:MMA  + '\0' 
    int next_alarm_week_day =  rtcctl_get_next_alarm_week_day();
    rtcctl_alarm_t *alarm_data = rtcctl_get_alarm_data();
    if (alarm_data->enabled && next_alarm_week_day != RTCCTL_ALARM_NOT_SET)
        snprintf(
            text, 
            LABEL_MAX_SIZE, 
            "%s%s%d:%.2d%s", 
            show_day ? alarm_clock_get_week_day(next_alarm_week_day, false) : "",
            show_day ? "\n" : "",
            timesync_get_24hr() ? alarm_data->hour : alarm_clock_get_am_pm_hour(alarm_data->hour),
            alarm_data->minute,
            timesync_get_24hr() ? "" : alarm_clock_get_am_pm_value(alarm_data->hour, true) 
        );
    else
        snprintf(text, LABEL_MAX_SIZE, "\n--:--");
    lv_label_set_text( alarm_clock_widget_label, text);
    return text;
}
