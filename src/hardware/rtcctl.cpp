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

#include "rtcctl.h"

#include <TTGO.h>
#include <SPIFFS.h>

#include "hardware/powermgm.h"
#include "hardware/json_psram_allocator.h"

#include <time.h>

#define CONFIG_FILE_PATH "/rtcctr.json"
EventGroupHandle_t rtcctl_status = NULL;
portMUX_TYPE rtcctlMux = portMUX_INITIALIZER_UNLOCKED;

#define VERSION_KEY "version"
#define ENABLED_KEY "enabled"
#define HOUR_KEY "hour"
#define MINUTE_KEY "minute"
#define WEEK_DAYS_KEY "week_days" 

static rtcctl_alarm_t alarm_data = {
    .enabled = false,
    .hour = 0,
    .minute = 0,
    .week_days = {false, false, false, false, false, false, false,}
}; 
static time_t alarm_time = 0;

static void IRAM_ATTR irq( void );
static void send_event_cb( EventBits_t event );
static void clear_event( EventBits_t bits );
static bool get_event( EventBits_t bits );
static void load_data();

static rtcctl_event_cb_t *event_cb_table = NULL;
static uint32_t event_cb_entrys = 0;

void rtcctl_setup( void ){
    rtcctl_status = xEventGroupCreate();

    pinMode( RTC_INT, INPUT_PULLUP);
    attachInterrupt( RTC_INT, &irq, FALLING );

    load_data();
}

void rtcctl_loop( void ) {
    // fire callback
    if ( !powermgm_get_event( POWERMGM_STANDBY ) ) {
        if ( get_event( RTCCTL_ALARM_OCCURRED ) ) {
            send_event_cb( RTCCTL_ALARM_OCCURRED );
            clear_event( RTCCTL_ALARM_OCCURRED );
        }
    }
}

static bool is_any_day_enabled(){
    for (int index = 0; index < DAYS_IN_WEEK; ++index){
        if (alarm_data.week_days[index])
            return true; 
    }
    return false;
}

static time_t find_next_alarm_day(int day_of_week, time_t now){
    //it is expected that test if any day is enabled has been performed
    
    time_t ret_val = now;
    int wday_index = day_of_week;
    do {
        ret_val += 60 * 60 * 24; //number of seconds in day
        if (++wday_index == DAYS_IN_WEEK){
            wday_index = 0;
        } 
        if (alarm_data.week_days[wday_index]){
            return ret_val;
        }        
    } while (wday_index != day_of_week);
    
    return ret_val; //the same day of next week
}

static void set_next_alarm(TTGOClass *ttgo){
    if (!is_any_day_enabled()){
        ttgo->rtc->setAlarm( PCF8563_NO_ALARM, PCF8563_NO_ALARM, PCF8563_NO_ALARM, PCF8563_NO_ALARM );    
        send_event_cb( RTCCTL_ALARM_TERM_SET );
        return;
    } 

    //trc ans system must be synchronized, it is important when alarm has been raised and we want to set next concurency 
    //if the synchronisation is not done the time can be set to now again
    ttgo->rtc->syncToSystem(); 
    
    time_t now;
    time(&now);
    alarm_time = now;
    struct tm  alarm_tm;
    localtime_r(&alarm_time, &alarm_tm);
    alarm_tm.tm_hour = alarm_data.hour;
    alarm_tm.tm_min = alarm_data.minute;
    alarm_time = mktime(&alarm_tm);

    if (!alarm_data.week_days[alarm_tm.tm_wday] || alarm_time <= now){
       alarm_time = find_next_alarm_day( alarm_tm.tm_wday, alarm_time );
       localtime_r(&alarm_time, &alarm_tm);
    }
    //it is better define alarm by day in month rather than weekday. This way will be work-around an error in pcf8563 source and will avoid eaising alarm when there is only one alarm in the week (today) and alarm time is set to now
    ttgo->rtc->setAlarm( alarm_tm.tm_hour, alarm_tm.tm_min, alarm_tm.tm_mday, PCF8563_NO_ALARM );
    send_event_cb( RTCCTL_ALARM_TERM_SET );
}

void rtcctl_set_next_alarm(){
    TTGOClass *ttgo = TTGOClass::getWatch();
    if (alarm_data.enabled){
        ttgo->rtc->disableAlarm();
    }

    set_next_alarm(ttgo);
    
    if (alarm_data.enabled){
        ttgo->rtc->enableAlarm();
    }
}

static void IRAM_ATTR irq( void ) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /*
     * setup an RTC event
     */
    xEventGroupSetBitsFromISR( rtcctl_status, RTCCTL_ALARM_OCCURRED, &xHigherPriorityTaskWoken );
    if ( xHigherPriorityTaskWoken ) {
        portYIELD_FROM_ISR();
    }
    powermgm_set_event( POWERMGM_RTC_ALARM );
}

void rtcctl_register_cb( EventBits_t event, RTCCTL_CALLBACK_FUNC rtcctl_event_cb ) {
    event_cb_entrys++;

    if ( event_cb_table == NULL ) {
        event_cb_table = ( rtcctl_event_cb_t * )ps_malloc( sizeof( rtcctl_event_cb_t ) * event_cb_entrys );
        if ( event_cb_table == NULL ) {
            log_e("rtc_event_cb_table malloc failed");
            while(true);
        }
    }
    else {
        rtcctl_event_cb_t *new_rtcctl_event_cb_table = NULL;

        new_rtcctl_event_cb_table = ( rtcctl_event_cb_t * )ps_realloc( event_cb_table, sizeof( rtcctl_event_cb_t ) * event_cb_entrys );
        if ( new_rtcctl_event_cb_table == NULL ) {
            log_e("rtc_event_cb_table realloc failed");
            while(true);
        }
        event_cb_table = new_rtcctl_event_cb_table;
    }

    event_cb_table[ event_cb_entrys - 1 ].event = event;
    event_cb_table[ event_cb_entrys - 1 ].event_cb = rtcctl_event_cb;
    log_i("register rtc_event_cb success (%p)", event_cb_table[ event_cb_entrys - 1 ].event_cb );
}

void send_event_cb( EventBits_t event ) {
    if ( event_cb_entrys == 0 ) {
      return;
    }

    for ( int entry = 0 ; entry < event_cb_entrys ; entry++ ) {
        yield();
        if ( event & event_cb_table[ entry ].event ) {
            log_i("call rtc_event_cb (%p)", event_cb_table[ entry ].event_cb );
            event_cb_table[ entry ].event_cb( event );
        }
    }
}

void clear_event( EventBits_t bits ) {
    portENTER_CRITICAL(&rtcctlMux);
    xEventGroupClearBits( rtcctl_status, bits );
    portEXIT_CRITICAL(&rtcctlMux);
}

bool get_event( EventBits_t bits ) {
    portENTER_CRITICAL(&rtcctlMux);
    EventBits_t temp = xEventGroupGetBits( rtcctl_status ) & bits;
    portEXIT_CRITICAL(&rtcctlMux);
    if ( temp )
        return( true );

    return( false );
}

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

    rtcctl_alarm_t stored_data;
    stored_data.enabled = doc[ENABLED_KEY].as<bool>();
    stored_data.hour = doc[HOUR_KEY].as<uint8_t>();
    stored_data.minute =  doc[MINUTE_KEY].as<uint8_t>();
    uint8_t stored_week_days = doc[WEEK_DAYS_KEY].as<uint8_t>();
    for (int index = 0; index < DAYS_IN_WEEK; ++index){
        stored_data.week_days[index] = ((stored_week_days >> index) & 1) != 0;
    }
    rtcctl_set_alarm(&stored_data);

    doc.clear();
    file.close();
}

static void store_data(){
    if ( SPIFFS.exists( CONFIG_FILE_PATH ) ) {
        SPIFFS.remove( CONFIG_FILE_PATH );
        log_i("remove old binary rtcl config");
    }

    fs::File file = SPIFFS.open( CONFIG_FILE_PATH, FILE_WRITE );
    if (!file) {
        log_e("Can't open file: %s!", CONFIG_FILE_PATH );
        return;
    }

    SpiRamJsonDocument doc( 1000 );

    doc[VERSION_KEY] = 1;
    doc[ENABLED_KEY] = alarm_data.enabled;
    doc[HOUR_KEY] = alarm_data.hour;
    doc[MINUTE_KEY] = alarm_data.minute;

    uint8_t week_days_to_store = 0;
    for (int index = 0; index < DAYS_IN_WEEK; ++index){
        week_days_to_store |= alarm_data.week_days[index] << index; 
    }
    doc[WEEK_DAYS_KEY] = week_days_to_store;
    
    if ( serializeJsonPretty( doc, file ) == 0) {
        log_e("Failed to write rtcl config file");
    }

    doc.clear();
    file.close();
}

void rtcctl_set_alarm( rtcctl_alarm_t *data ){
    TTGOClass *ttgo = TTGOClass::getWatch();
    bool was_enabled = alarm_data.enabled;
    if (was_enabled){
        ttgo->rtc->disableAlarm();
    }
    alarm_data = *data;
    store_data();

    set_next_alarm(ttgo);

    if (was_enabled && !alarm_data.enabled){
        //already disabled
        send_event_cb( RTCCTL_ALARM_DISABLED );
    }
    else if (was_enabled && alarm_data.enabled){
        ttgo->rtc->enableAlarm();
        //nothing actually changed;
    }
    else if (!was_enabled && alarm_data.enabled){
        ttgo->rtc->enableAlarm();
        send_event_cb( RTCCTL_ALARM_ENABLED );    
    }    
}

rtcctl_alarm_t *rtcctl_get_alarm_data(){
    return &alarm_data;
}

int rtcctl_get_next_alarm_week_day(){
    if (!is_any_day_enabled()){
        return RTCCTL_ALARM_NOT_SET;
    }
    tm alarm_tm;
    localtime_r(&alarm_time, &alarm_tm);
    return alarm_tm.tm_wday;
}