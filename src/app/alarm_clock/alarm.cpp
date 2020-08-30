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

#include <hardware/rtc.h>
#include <SPIFFS.h>
#include "hardware/json_psram_allocator.h"

#define CONFIG_FILE_PATH "/alarm.json"

typedef struct {
    int version = 1;
    bool enabled = 0;
    bool vibe = 1;
    int hour = 0;
    int minute = 0;
} alarm_config_t;

static alarm_config_t config;

void alarm_set(uint8_t hour, uint8_t minute){
    if (config.enabled){
         rtc_disable_alarm();
    }
    config.hour = hour;
    config.minute = minute;
    rtc_set_alarm(hour, minute);

    if (config.enabled) rtc_enable_alarm();
}

void alarm_set_enabled(bool enable){
    log_i("alarm set enabled: %d", enable);
    config.enabled = enable;
    if (config.enabled){
        rtc_enable_alarm();
    }
    else{
        rtc_disable_alarm();
    }
}

bool alarm_is_enabled(){
    return config.enabled;
}

bool alarm_is_time(){
    return rtc_is_time(config.hour, config.minute);
}

uint8_t alarm_get_hour(){
    return config.hour;
}

uint8_t alarm_get_minute(){
    return config.minute;
}

void alarm_set_vibe(bool vibe){
    config.vibe = vibe;
}

bool alarm_is_vibe_allowed(){
    return config.vibe;
}

void alarm_load_data(){
    if (! SPIFFS.exists( CONFIG_FILE_PATH ) ) {
        return;
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

    config.enabled = doc["enabled"].as<bool>();
    config.vibe = doc["vibe"].as<bool>();
    config.hour = doc["hour"].as<int>();
    config.minute = doc["minute"].as<int>();

    doc.clear();
    file.close();
}

void alarm_store_data(){
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

    doc["enabled"] = config.enabled;
    doc["vibe"] = config.vibe;
    doc["hour"] = config.hour;
    doc["minute"] = config.minute;

    if ( serializeJsonPretty( doc, file ) == 0) {
        log_e("Failed to write config file");
    }

    doc.clear();
    file.close();
}
