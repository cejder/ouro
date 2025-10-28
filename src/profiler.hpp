#pragma once

#include "common.hpp"
#include "map.hpp"

#define PROFILER_TRACK_MAX_LABEL_LENGTH 256
#define PROFILER_TRACK_MAX_COUNT 1024
#define PROFILER_FRAME_TIMES_TIMELINE_MAX_COUNT 256
#define PROFILER_CALL_STACK_MAX_DEPTH 64

fwd_decl(AFont);

struct ProfilerTrack {
    C8 label[PROFILER_TRACK_MAX_LABEL_LENGTH];
    SZ previous_generation;
    F64 e_frequency;
    F64 sum_e_frequency;
    F64 avg_e_frequency;
    U64 executions;
    F64 start_time;
    F64 end_time;
    F64 delta_time;
    F64 min_delta_time;
    F64 max_delta_time;
    F64 sum_delta_time;
    F64 avg_delta_time;
    U64 start_cycles;
    U64 end_cycles;
    U64 delta_cycles;
    U64 min_delta_cycles;
    U64 max_delta_cycles;
    U64 sum_delta_cycles;
    U64 avg_delta_cycles;
    BOOL want_reset;
    SZ depth;
};

struct ProfilerFlameGraphTrack {
    C8 label[PROFILER_TRACK_MAX_LABEL_LENGTH];
    F64 start_time;
    F64 end_time;
    SZ depth;
    ProfilerTrack track_data;
};

struct ProfilerFlameGraph {
    BOOL paused;
    ProfilerTrack main_thread_track;
    ProfilerFlameGraphTrack tracks[PROFILER_TRACK_MAX_COUNT];
    SZ track_count;
    F64 start_time;
    F64 end_time;
    BOOL want_reset;
    ProfilerFlameGraphTrack *selected_track;
    ProfilerFlameGraphTrack *selected_tracks[PROFILER_TRACK_MAX_COUNT];
    SZ selected_track_count;
    F64 selected_tracks_history[PROFILER_TRACK_MAX_COUNT][PROFILER_FRAME_TIMES_TIMELINE_MAX_COUNT];  // History for each selected track
    SZ selected_tracks_history_indices[PROFILER_TRACK_MAX_COUNT];                                    // History indices for each track
    F64 selected_tracks_max_time[PROFILER_TRACK_MAX_COUNT];
};

MAP_DECLARE(ProfilerTrackMap, C8 const *, ProfilerTrack, MAP_HASH_CSTR, MAP_EQUAL_CSTR);

struct Profiler {
    BOOL initialized;
    ProfilerTrackMap track_map;
    SZ current_generation;
    SZ call_stack_depth;
    ProfilerFlameGraph flame_graph;
    BOOL mouse_over;
};

Profiler extern g_profiler;

void profiler_init();
void profiler_update();
void profiler_draw();
void profiler_finalize();
ProfilerTrack *profiler_get_track(C8 const *label);
void profiler_track_begin(C8 const *label);
void profiler_track_end(C8 const *label);
void profiler_reset();

#define ML_NAME              "thread_MAIN"
#define AT_NAME              "thread_ASSET"
#define CT_NAME              "thread_CVAR"
#define ML_PROFILE(function) profiler_track_begin(ML_NAME); function; profiler_track_end(ML_NAME)

#if defined(OURO_PROFILE)
#define PP(function)         profiler_track_begin(#function); function; profiler_track_end(#function)
#define PBEGIN(name)         profiler_track_begin(name)
#define PBEGINF(fmt, var) do {                                   \
    C8 static buffer##__LINE__[PROFILER_TRACK_MAX_LABEL_LENGTH]; \
    ou_snprintf(buffer##__LINE__, PROFILER_TRACK_MAX_LABEL_LENGTH, fmt, var); \
    PBEGIN(buffer##__LINE__); \
} while(0)
#define PEND(name)           profiler_track_end(name)
#define PENDF(fmt, var) do {                                   \
    C8 static buffer##__LINE__[PROFILER_TRACK_MAX_LABEL_LENGTH]; \
    ou_snprintf(buffer##__LINE__, PROFILER_TRACK_MAX_LABEL_LENGTH, fmt, var); \
    PEND(buffer##__LINE__); \
} while(0)
#else
#define PP(function)         do { function; } while(0)
#define PBEGIN(name)         do {} while(0)
#define PBEGINF(fmt, var)    do {} while(0)
#define PEND(name)           do {} while(0)
#define PENDF(fmt, var)      do {} while(0)
#endif
