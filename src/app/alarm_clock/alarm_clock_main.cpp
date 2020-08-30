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

#include "alarm_clock.h"
#include "alarm_clock_main.h"

#include "gui/mainbar/app_tile/app_tile.h"
#include "gui/mainbar/main_tile/main_tile.h"
#include "gui/mainbar/mainbar.h"
#include "gui/statusbar.h"

#include "alarm.h"
#include "widget_factory.h"
#include "widget_styles.h"

lv_task_t * _alarm_clock_task;
lv_obj_t *alarm_active_switch = NULL;
LV_IMG_DECLARE(exit_32px);
LV_IMG_DECLARE(setup_32px);
LV_IMG_DECLARE(refresh_32px);
LV_FONT_DECLARE(Ubuntu_72px);

static lv_obj_t *hour_roller = NULL;
static lv_obj_t *min_roller = NULL;

static void exit_alarm_clock_main_event_cb( lv_obj_t * obj, lv_event_t event );
static void enter_alarm_clock_setup_event_cb( lv_obj_t * obj, lv_event_t event );
//void alarm_clock_task( lv_task_t * task );

static char* get_roller_content(int count, bool zeros){
    static char content[60 * 3]; //max 60 sec, 2 digits + \n (on last line is \0 instead)
    int pos = 0;
    for (int i = 0; i < count; ++i){
        if (i < 10){
            if (zeros){
                content[pos++] = '0';
            }
        }
        else{
            content[pos++] = '0' + i / 10;
        }
        content[pos++] = '0' + i % 10;

        content[pos++] = (i == count - 1 ? '\0' : '\n');
    }
    return content;
}

void alarm_clock_main_setup( uint32_t tile_num ) {
    lv_obj_t * main_tile = mainbar_get_tile_obj( tile_num );

    lv_obj_t * labeled_alarm_switch  = wf_add_labeled_switch(
        main_tile, "Activated", main_tile, LV_ALIGN_IN_TOP_LEFT, 10, 10, &alarm_active_switch
    );

    int cont_width = lv_disp_get_hor_res( NULL ) - 20;
    int cont_height = lv_disp_get_ver_res( NULL ) - 32 - 30 - 40;
    lv_obj_t *alarm_clock_roller_cont = wf_add_container(
        main_tile, labeled_alarm_switch, LV_ALIGN_OUT_BOTTOM_MID, 0, 10, cont_width , cont_height
    );
    hour_roller = wf_add_roller(main_tile, get_roller_content(24, false), alarm_clock_roller_cont, LV_ALIGN_IN_LEFT_MID);
    wf_add_label(main_tile, ":", alarm_clock_roller_cont, LV_ALIGN_CENTER, 0, 0);
    min_roller = wf_add_roller(main_tile, get_roller_content(60, true), alarm_clock_roller_cont, LV_ALIGN_IN_RIGHT_MID);

    wf_add_image_button(
        main_tile, exit_32px, main_tile, LV_ALIGN_IN_BOTTOM_LEFT, 10, -10, exit_alarm_clock_main_event_cb
    );

    wf_add_image_button(
        main_tile, setup_32px, main_tile, LV_ALIGN_IN_BOTTOM_RIGHT, -10, -10, enter_alarm_clock_setup_event_cb
    );

    // uncomment the following block of code to remove the "myapp" label in background
    /*lv_style_set_text_opa( &alarm_clock_main_style, LV_OBJ_PART_MAIN, LV_OPA_70);
    lv_style_set_text_font( &alarm_clock_main_style, LV_STATE_DEFAULT, &Ubuntu_72px);
    lv_obj_t *app_label = lv_label_create( alarm_clock_main_tile, NULL);
    lv_label_set_text( app_label, "alarm clock");
    lv_obj_reset_style_list( app_label, LV_OBJ_PART_MAIN );
    lv_obj_add_style( app_label, LV_OBJ_PART_MAIN, &alarm_clock_main_style );
    lv_obj_align( app_label, alarm_clock_main_tile, LV_ALIGN_CENTER, 0, 0);
    */

    // create an task that runs every secound
    //_alarm_clock_task = lv_task_create( alarm_clock_task, 1000, LV_TASK_PRIO_MID, NULL );

}

void alarm_clock_set_gui_values(){
    lv_roller_set_selected(hour_roller, alarm_get_hour(), LV_ANIM_OFF);
    lv_roller_set_selected(min_roller, alarm_get_minute(), LV_ANIM_OFF);

    if (alarm_is_enabled()){
        lv_switch_on(alarm_active_switch, LV_ANIM_OFF);
    }
    else{
        lv_switch_off(alarm_active_switch, LV_ANIM_OFF);
    }
}

static void enter_alarm_clock_setup_event_cb( lv_obj_t * obj, lv_event_t event ) {
    switch( event ) {
        case( LV_EVENT_CLICKED ):
            statusbar_hide( true );
            mainbar_jump_to_tilenumber( alarm_clock_get_app_setup_tile_num(), LV_ANIM_ON );
            break;
    }
}

static void exit_alarm_clock_main_event_cb( lv_obj_t * obj, lv_event_t event ) {
    switch( event ) {
        case( LV_EVENT_CLICKED ):
            alarm_set(lv_roller_get_selected(hour_roller), lv_roller_get_selected(min_roller));
            alarm_set_enabled(lv_switch_get_state(alarm_active_switch));
            alarm_store_data();
            alarm_clock_hide_app_icon_info( ! lv_switch_get_state(alarm_active_switch) );
            mainbar_jump_to_maintile( LV_ANIM_OFF );
            break;
    }
}

//void alarm_clock_task( lv_task_t * task ) {
    // put your code her
//}
