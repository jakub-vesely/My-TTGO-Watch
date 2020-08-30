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

#include <config.h>
#include <TTGO.h>

void rtc_setup(TTGOClass *ttgo);
void rtc_set_alarm(uint8_t hour, uint8_t minute);
void rtc_enable_alarm();
void rtc_disable_alarm();
bool rtc_is_time(uint8_t hour, uint8_t minute);

// void rtc_enable_alarm(bool enable);
// bool rtc_is_alarm_time();

// uint8_t rtc_get_alarm_hour();
// uint8_t rtc_get_alarm_min();
// bool rtc_is_alarm_enabled();

