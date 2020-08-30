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

#include "rtc.h"
#include <stdbool.h>
#include <hardware/powermgm.h>

static TTGOClass *s_ttgo = NULL;

void rtc_setup(TTGOClass *ttgo){
    s_ttgo = ttgo;
    pinMode(RTC_INT, INPUT_PULLUP);
    attachInterrupt(RTC_INT, [] {
       powermgm_set_event(POWERMGM_RTC_ALARM);
       //detachInterrupt(RTC_INT);
    }, FALLING);
}

void rtc_set_alarm(uint8_t hour, uint8_t minute){
    s_ttgo->rtc->setAlarm(hour, minute,PCF8563_NO_ALARM, PCF8563_NO_ALARM);
}

void rtc_enable_alarm(){
    s_ttgo->rtc->enableAlarm();
}

void rtc_disable_alarm(){
    s_ttgo->rtc->disableAlarm();
}

bool rtc_is_time(uint8_t hour, uint8_t minute){
    RTC_Date date_time = s_ttgo->rtc->getDateTime();
    return hour == date_time.hour && minute == date_time.minute;
}
