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

#pragma once

#include "TTGO.h"

#define RTCCTL_ALARM_OCCURRED   _BV(0)
#define RTCCTL_ALARM_TERM_SET   _BV(1)
#define RTCCTL_ALARM_DISABLED   _BV(2)
#define RTCCTL_ALARM_ENABLED    _BV(3)

#define DAYS_IN_WEEK 7
#define RTCCTL_ALARM_NOT_SET -1

typedef void ( * RTCCTL_CALLBACK_FUNC ) ( EventBits_t event );

typedef struct {
    EventBits_t event;
    RTCCTL_CALLBACK_FUNC event_cb;
} rtcctl_event_cb_t;

typedef struct {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    bool week_days[DAYS_IN_WEEK]; //starting from sunday to be aligned with tm
} rtcctl_alarm_t;

/*
    * @brief setup rtc controller routine
    */
void rtcctl_setup( void );

/*
    * @brief rtc controller loop routine
    */
void rtcctl_loop( void );

/*
    * @brief registers a callback function which is called on a corresponding event
    *
    * @param   event   possible values: RTCCTL_ALARM_OCCURRED, RTCCTL_ALARM_TERM_SET, RTCCTL_ALARM_ENABLED and RTCCTL_ALARM_DISABLED
    * @param   rtc_event_cb   pointer to the callback function
    */
void rtcctl_register_cb( EventBits_t event, RTCCTL_CALLBACK_FUNC rtc_event_cb );
    
/*
    * @brief set an alarm
    *
    * @param   pointer to alarm_data struct
    *
    */
void rtcctl_set_alarm( rtcctl_alarm_t *alarm_data );

/*
    * @brief   returns pointer to data coresponding to alarm
    */
rtcctl_alarm_t *rtcctl_get_alarm_data();

/**
 * @brief find and set term for next alarm 
*/
void rtcctl_set_next_alarm();

/**
 * @brief if alarm is set, returns day of week number where sunday=0, othervise is returned DAY_NOT_SET 
*/
int rtcctl_get_next_alarm_week_day();