#include "alarm.hpp"
#include "assert.hpp"

void alarm_start(Alarm *alarm, F32 duration, BOOL is_looping) {
    _assert_(duration > 0.0F, "Alarm duration must be greater than 0");
    alarm->duration   = duration;
    alarm->elapsed    = 0.0F;
    alarm->is_active  = true;
    alarm->is_looping = is_looping;
}

void alarm_stop(Alarm *alarm) {
    alarm->elapsed   = 0.0F;
    alarm->is_active = false;
}

BOOL alarm_tick(Alarm *alarm, F32 dtu) {
    if (!alarm->is_active) { return true; }

    alarm->elapsed += dtu;
    if (alarm->elapsed >= alarm->duration) {
        alarm_stop(alarm);
        if (alarm->is_looping) { alarm_start(alarm, alarm->duration, true); }
        return true;
    }

    return false;
}

F32 alarm_get_progress_perc(Alarm *alarm) {
    if (!alarm->is_active) { return 0.0F; }
    return alarm->elapsed / alarm->duration;
}

F32 alarm_get_progress(Alarm *alarm) {
    if (!alarm->is_active) { return 0.0F; }
    return alarm->elapsed;
}

F32 alarm_get_remaining_perc(Alarm *alarm) {
    if (!alarm->is_active) { return 0.0F; }
    return 1.0F - alarm_get_progress_perc(alarm);
}

F32 alarm_get_remaining(Alarm *alarm) {
    if (!alarm->is_active) { return 0.0F; }
    return alarm->duration - alarm->elapsed;
}
