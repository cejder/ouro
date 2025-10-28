#pragma once

#include "common.hpp"

struct Alarm {
    F32 elapsed;
    F32 duration;
    BOOL is_active;
    BOOL is_looping;
};

void alarm_start(Alarm *alarm, F32 duration, BOOL is_looping);
void alarm_stop(Alarm *alarm);
BOOL alarm_tick(Alarm *alarm, F32 dtu);
F32 alarm_get_progress_perc(Alarm *alarm);
F32 alarm_get_progress(Alarm *alarm);
F32 alarm_get_remaining_perc(Alarm *alarm);
F32 alarm_get_remaining(Alarm *alarm);
