#include "profiler.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "common.hpp"
#include "cvar.hpp"
#include "ease.hpp"
#include "input.hpp"
#include "log.hpp"
#include "math.hpp"
#include "message.hpp"
#include "render.hpp"
#include "render_tooltip.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"
#include "unit.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>

#include <algorithm>
#define GET_CYCLES() __rdtsc()
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <mach/mach_time.h>
#define GET_CYCLES() mach_absolute_time()
#else
#error "Architecture not supported"
#endif

Profiler g_profiler = {};

void static inline i_reset_track(ProfilerTrack *t) {
    t->executions       = 0;
    t->sum_e_frequency  = 0.0;
    t->avg_e_frequency  = 0.0;
    t->min_delta_time   = F64_MAX;
    t->max_delta_time   = F64_MIN;
    t->sum_delta_time   = 0.0;
    t->avg_delta_time   = 0.0;
    t->min_delta_cycles = U64_MAX;
    t->max_delta_cycles = 0;
    t->sum_delta_cycles = 0;
    t->avg_delta_cycles = 0;
    t->want_reset       = false;
}

C8 static const *i_get_stack_overflow_debug_info(ProfilerTrack *t) {
    String *s = TS("PROFILER STACK OVERFLOW DETECTED!\n");
    string_appendf(s, "Current call stack depth: %zu\n", g_profiler.call_stack_depth);
    string_appendf(s, "Maximum allowed depth: %d\n", PROFILER_CALL_STACK_MAX_DEPTH);
    string_appendf(s, "Track causing overflow: %s\n", t->label);
    string_appendf(s, "Current generation: %zu\n", g_profiler.current_generation);
    string_appendf(s, "\n");

    // Count and show active tracks
    SZ active_count      = 0;
    SZ total_tracks      = 0;
    C8 const **label     = nullptr;
    ProfilerTrack *track = nullptr;
    MAP_EACH_PTR(&g_profiler.track_map, label, track) {
        total_tracks++;
        if (track->previous_generation == g_profiler.current_generation) { active_count++; }
    }

    string_appendf(s, "Active tracks: %zu / %zu total tracks\n", active_count, total_tracks);
    string_appendf(s, "\nUNCLOSED TRACKS (started but not ended this frame):\n");

    // Create array to sort tracks by depth for better visualization
    struct TrackInfo {
        C8 const *label;
        ProfilerTrack *track;
    };
    TrackInfo active_tracks[PROFILER_TRACK_MAX_COUNT];
    SZ active_idx = 0;
    SZ min_depth  = SIZE_MAX;

    // Collect active tracks and find minimum depth
    MAP_EACH_PTR(&g_profiler.track_map, label, track) {
        if (track->previous_generation == g_profiler.current_generation && active_idx < PROFILER_TRACK_MAX_COUNT) {
            active_tracks[active_idx].label = *label;
            active_tracks[active_idx].track = track;
            min_depth = glm::min(min_depth, track->depth);
            active_idx++;
        }
    }

    // Simple bubble sort by depth for clear call stack visualization
    for (SZ i = 0; i < active_idx - 1; i++) {
        for (SZ j = 0; j < active_idx - i - 1; j++) {
            if (active_tracks[j].track->depth > active_tracks[j + 1].track->depth) {
                TrackInfo const temp = active_tracks[j];
                active_tracks[j]     = active_tracks[j + 1];
                active_tracks[j + 1] = temp;
            }
        }
    }

    // Analyze for patterns to suggest likely causes
    SZ same_name_count              = 0;
    SZ max_single_depth             = 0;
    C8 const *deepest_repeated_name = nullptr;

    for (SZ i = 0; i < active_idx; i++) {
        SZ count_same_name = 0;
        for (SZ j = 0; j < active_idx; j++) {
            if (ou_strcmp(active_tracks[i].label, active_tracks[j].label) == 0) { count_same_name++; }
        }
        if (count_same_name > same_name_count) {
            same_name_count = count_same_name;
            deepest_repeated_name = active_tracks[i].label;
        }
        max_single_depth = glm::max(active_tracks[i].track->depth, max_single_depth);
    }

    // Display sorted tracks with normalized indentation (min_depth = level 0)
    for (SZ i = 0; i < active_idx; i++) {
        TrackInfo const *info             = &active_tracks[i];
        ProfilerTrack const *active_track = info->track;

        // Normalize indentation: subtract min_depth so deepest starts at level 0
        SZ const normalized_depth = active_track->depth >= min_depth ? active_track->depth - min_depth : 0;
        for (SZ d = 0; d < normalized_depth; d++) { string_appendf(s, "  "); }

        // Calculate how long this track has been running
        F64 const current_time = time_get_glfw();
        F64 const running_time = current_time - active_track->start_time;

        string_appendf(s, "├─ %s [depth:%zu, running:%.4fms, calls:%" PRIu64 "]\n", info->label, active_track->depth, BASE_TO_MILLI(running_time),
                       active_track->executions);
    }

    if (active_idx == 0) { string_appendf(s, "  (No active tracks found - this shouldn't happen!)\n"); }

    // Smart cause analysis
    string_appendf(s, "\nLIKELY CAUSE ANALYSIS:\n");

    if (same_name_count > 3 && deepest_repeated_name) {
        string_appendf(s, "RECURSION DETECTED: '%s' appears %zu times in stack\n", deepest_repeated_name, same_name_count);
        string_appendf(s, "   > Check for infinite/deep recursion in this function\n");
        string_appendf(s, "   > Add recursion depth limiting or base case checks\n");
    } else if (active_count > (SZ)(PROFILER_CALL_STACK_MAX_DEPTH * 0.8)) {
        string_appendf(s, " DEEP CALL CHAIN: %zu active tracks (%.1f%% of limit)\n", active_count,
                       (F32)active_count / (F32)PROFILER_CALL_STACK_MAX_DEPTH * 100.0F);
        string_appendf(s, "   > Very deep call stack, possibly legitimate but extreme nesting\n");
        string_appendf(s, "   > Consider refactoring to reduce call depth\n");
    } else {
        string_appendf(s, " MISSING PEND(): Likely missing PEND() calls\n");
        string_appendf(s, "   > Look for early returns, exceptions, or forgotten PEND() calls\n");
        string_appendf(s, "   > Each PBEGIN() must have matching PEND() in same scope\n");
    }

    string_appendf(s, "\nOTHER POSSIBLE CAUSES:\n");
    string_appendf(s, "- Exception/early return skipping PEND() in a profiled section\n");
    string_appendf(s, "- Mismatched profiler macro usage (PBEGIN without PEND)\n");
    string_appendf(s, "- Threading issues with profiler state\n");

    return s->c;
}

void static inline i_start_frame(ProfilerTrack *t, F64 start_time, U64 start_cycles) {
    _assert_(g_profiler.call_stack_depth < PROFILER_CALL_STACK_MAX_DEPTH, i_get_stack_overflow_debug_info(t));

    if (t->want_reset) { i_reset_track(t); }
    t->executions++;

    t->previous_generation = g_profiler.current_generation;
    t->start_time          = start_time;
    t->start_cycles        = start_cycles;
    t->depth               = g_profiler.call_stack_depth;

    g_profiler.call_stack_depth++;
}

void static inline i_end_frame(ProfilerTrack *t) {
    _assert_(g_profiler.call_stack_depth > 0, "Call stack is 0 even though we are trying to end a frame");

    t->end_cycles        = GET_CYCLES();
    t->delta_cycles      = t->end_cycles - t->start_cycles;
    t->min_delta_cycles  = glm::min(t->min_delta_cycles, t->delta_cycles);
    t->max_delta_cycles  = glm::max(t->max_delta_cycles, t->delta_cycles);
    t->sum_delta_cycles += t->delta_cycles;
    t->avg_delta_cycles  = t->sum_delta_cycles / t->executions;

    t->end_time        = time_get_glfw();
    t->delta_time      = t->end_time - t->start_time;
    t->min_delta_time  = glm::min(t->min_delta_time, t->delta_time);
    t->max_delta_time  = glm::max(t->max_delta_time, t->delta_time);
    t->sum_delta_time += t->delta_time;
    t->avg_delta_time  = t->sum_delta_time / (F64)t->executions;

    t->e_frequency      = 1.0 / t->delta_time;
    t->sum_e_frequency += t->e_frequency;
    t->avg_e_frequency  = t->sum_e_frequency / (F64)t->executions;
    g_profiler.call_stack_depth--;
}

void profiler_init() {
    g_profiler.call_stack_depth = 0;

    ProfilerTrackMap_init(&g_profiler.track_map, MEMORY_TYPE_ARENA_PERMANENT, 0);

    g_profiler.initialized = true;
}

void profiler_update() {
    if (g_profiler.flame_graph.want_reset) {
        g_profiler.flame_graph.main_thread_track    = {};
        g_profiler.flame_graph.track_count          = 0;
        g_profiler.flame_graph.start_time           = 0;
        g_profiler.flame_graph.end_time             = 0;
        g_profiler.flame_graph.selected_track_count = 0;
        g_profiler.flame_graph.selected_track       = nullptr;
        // Clear multi-track histories
        for (SZ i = 0; i < PROFILER_TRACK_MAX_COUNT; ++i) {
            g_profiler.flame_graph.selected_tracks_history_indices[i] = 0;
            for (SZ j = 0; j < PROFILER_FRAME_TIMES_TIMELINE_MAX_COUNT; ++j) { g_profiler.flame_graph.selected_tracks_history[i][j] = 0.0; }
        }
        g_profiler.flame_graph.want_reset = false;
    }

    g_profiler.current_generation++;

    if (input_is_action_pressed(IA_PROFILER_FLAME_GRAPH_PAUSE_TOGGLE)) { g_profiler.flame_graph.paused = !g_profiler.flame_graph.paused; }
    if (input_is_action_pressed(IA_PROFILER_FLAME_GRAPH_SHOW_TOGGLE)) { c_profiler__flame_graph_enabled = !c_profiler__flame_graph_enabled; }
}

struct IFrameGraphLayout {
    F32 legend_width;
    F32 legend_height;
    F32 legend_padding;
    F32 legend_entry_height;
    F32 color_to_text_spacing;
    F32 color_box_size;
    AFont *legend_font;
    Vector2 legend_pos;
    F32 total_height;
    Vector2 graph_pos;
    Rectangle plot_area;
};

SZ static i_detect_legend_hover(ProfilerFlameGraph *fg, Vector2 mouse_pos, IFrameGraphLayout const *layout) {
    if (fg->selected_track_count == 0 || !layout->legend_font) { return SIZE_MAX; }

    for (SZ i = 0; i < fg->selected_track_count; ++i) {
        F32 const entry_y = layout->legend_pos.y + layout->legend_padding + ((F32)i * layout->legend_entry_height);
        Rectangle const legend_entry_rec = {layout->legend_pos.x + layout->legend_padding, entry_y, layout->legend_width - (layout->legend_padding * 2.0F),
                                             layout->legend_entry_height};
        if (CheckCollisionPointRec(mouse_pos, legend_entry_rec)) { return i; }
    }
    return SIZE_MAX;
}

void static i_calculate_frame_graph_layout(ProfilerFlameGraph *fg, Vector2 res, IFrameGraphLayout *layout) {
    layout->legend_width          = 0.0F;
    layout->legend_height         = 0.0F;
    layout->legend_font           = nullptr;
    layout->legend_padding        = 20.0F;
    layout->legend_entry_height   = (F32)c_profiler__frame_graph_legend_font_size + 6.0F;
    layout->color_to_text_spacing = 10.0F;
    layout->color_box_size        = (F32)c_profiler__frame_graph_legend_font_size;

    if (fg->selected_track_count > 0) {
        layout->legend_font = asset_get_font(c_profiler__frame_graph_legend_font.cstr, c_profiler__frame_graph_legend_font_size);

        F32 max_text_width = 0.0F;
        for (SZ i = 0; i < fg->selected_track_count; ++i) {
            String const *track_name = TS("%s", fg->selected_tracks[i]->label);
            Vector2 const text_size  = measure_text(layout->legend_font, track_name->c);
            max_text_width = glm::max(max_text_width, text_size.x);
        }

        layout->legend_width  = layout->legend_padding + layout->color_box_size + layout->color_to_text_spacing + max_text_width + layout->legend_padding;
        layout->legend_height = ((F32)fg->selected_track_count * layout->legend_entry_height) + (layout->legend_padding * 2.0F);
    }

    F32 const frame_graph_height = 180.0F;
    F32 const container_padding  = 30.0F;
    layout->total_height = glm::max(frame_graph_height, layout->legend_height + container_padding);

    F32 const flame_graph_padding_x = 10.0F;
    F32 const graph_width           = res.x - (flame_graph_padding_x * 2.0F);
    layout->graph_pos = {flame_graph_padding_x, c_profiler__flame_graph_position_y * res.y};

    F32 const container_y  = layout->graph_pos.y - layout->total_height - 20.0F;
    Rectangle const bg_rec = {layout->graph_pos.x, container_y, graph_width, layout->total_height};

    AFont *yaxis_font            = asset_get_font(c_profiler__frame_graph_y_axis_font.cstr, c_profiler__frame_graph_y_axis_font_size);
    F32 const label_sample_width = measure_text(yaxis_font, "16.67ms").x;
    F32 const left_margin        = label_sample_width + 15.0F;
    F32 const top_margin         = 15.0F;
    F32 const right_margin       = 15.0F + layout->legend_width + (layout->legend_width > 0 ? 10.0F : 0.0F);
    F32 const bottom_margin      = 15.0F;

    layout->plot_area = {
        bg_rec.x + left_margin,
        bg_rec.y + top_margin,
        bg_rec.width - left_margin - right_margin,
        bg_rec.height - top_margin - bottom_margin,
    };

    layout->legend_pos = {
        layout->plot_area.x + layout->plot_area.width + 10.0F,
        layout->plot_area.y + ((layout->plot_area.height - layout->legend_height) * 0.5F),
    };
}

void static i_draw_frame_info() {
    ProfilerTrack const *track = profiler_get_track(ML_NAME);
    Vector2 const res          = render_get_render_resolution();
    String const *text =
        TS("\\ouc{#ff5e48ff}AVG FT: \\ouc{#ffffffff}%5.2f ms  \\ouc{#ff5e48ff}MAX FT: \\ouc{#ffffffff}%5.2f ms  \\ouc{#ff5e48ff}FT: \\ouc{#ffffffff}%5.2f ms"
           "  \\ouc{#ff5e48ff}FPS: \\ouc{#ffffffff}%7.2f",
           BASE_TO_MILLI(track->avg_delta_time), BASE_TO_MILLI(track->max_delta_time), BASE_TO_MILLI(track->delta_time), 1.0 / track->delta_time);
    AFont *font                      = g_render.default_font;
    F32 const padding_x              = ui_scale_x(1.0F);
    F32 const padding_y              = ui_scale_y(1.0F);
    F32 const margin_x               = ui_scale_x(0.5F);
    F32 const margin_y               = ui_scale_y(0.2F);
    Vector2 const text_dimensions    = measure_text_ouc(font, text->c);
    Vector2 const text_pos           = {res.x - text_dimensions.x - padding_x, res.y - text_dimensions.y - padding_y};
    Vector2 const text_dim           = measure_text_ouc(font, text->c);
    Vector2 const text_shadow_offset = ui_shadow_offset(0.075F, 0.1F);
    Vector2 const bg_pos             = {text_pos.x - margin_x, text_pos.y - margin_y};
    Vector2 const bg_dim             = {text_dim.x + (margin_x * 2.0F), text_dim.y + (margin_y * 2.0F)};

    d2d_rectangle_v(bg_pos, bg_dim, Fade(BLACK, 0.5F));
    d2d_text_ouc_shadow(font, text->c, text_pos, WHITE, BLACK, text_shadow_offset);
}

void static i_draw_profiler_button(Rectangle rec, C8 const *text, BOOL is_hovered, Color bg_color, AFont *font) {
    Color const button_bg     = is_hovered ? Fade(bg_color, 0.8F) : Fade(bg_color, 0.6F);
    Color const button_border = is_hovered ? WHITE : Fade(WHITE, 0.6F);
    d2d_rectangle_rounded_rec(rec, 0.1F, 4, button_bg);
    d2d_rectangle_rounded_rec_lines(rec, 0.1F, 4, button_border);

    Vector2 const text_size = measure_text(font, text);
    Vector2 const text_pos = {
        rec.x + ((rec.width - text_size.x) * 0.5F),
        rec.y + ((rec.height - text_size.y) * 0.5F),
    };
    Color const text_color = is_hovered ? WHITE : Fade(WHITE, 0.9F);
    d2d_text(font, text, text_pos, text_color);
}

void static i_draw_hover_frame_graph_track_info(ProfilerFlameGraphTrack *hovered_fg_track, AFont *font) {
    ProfilerTrack const *tooltip_track = &hovered_fg_track->track_data;
    Vector2 const mouse_pos            = input_get_mouse_position();

    C8 pretty_buffer[PRETTY_BUFFER_SIZE] = {};
    RenderTooltip tt = {};
    render_tooltip_init(font, &tt, TS("  "), {mouse_pos.x, mouse_pos.y, 0.0F}, false);

    // Function info
    rib_STR(&tt, "\\ouc{#ff5e48ff}F_\\ouc{#ffffffff}NAME", tooltip_track->label);
    rib_SZ(&tt,  "\\ouc{#ff5e48ff}F_\\ouc{#ffffffff}DEPTH", tooltip_track->depth);
    rib_U64(&tt, "\\ouc{#ff5e48ff}F_\\ouc{#ffffffff}CALLS", tooltip_track->executions);

    // Timing info (ms)
    rib_MS(&tt, "\\ouc{#ffd700ff}T_\\ouc{#ffffffff}CURRENT", (F32)BASE_TO_MILLI(tooltip_track->delta_time));
    rib_MS(&tt, "\\ouc{#ffd700ff}T_\\ouc{#ffffffff}AVERAGE", (F32)BASE_TO_MILLI(tooltip_track->avg_delta_time));
    rib_MS(&tt, "\\ouc{#ffd700ff}T_\\ouc{#ffffffff}MIN", (F32)BASE_TO_MILLI(tooltip_track->min_delta_time));
    rib_MS(&tt, "\\ouc{#ffd700ff}T_\\ouc{#ffffffff}MAX", (F32)BASE_TO_MILLI(tooltip_track->max_delta_time));

    // Performance info
    if (g_profiler.flame_graph.main_thread_track.delta_time > 0.0) {
        F32 const frame_percent = ((F32)tooltip_track->delta_time / (F32)g_profiler.flame_graph.main_thread_track.delta_time) * 100.0F;
        rib_PERC(&tt, "\\ouc{#00ff7fff}P_\\ouc{#ffffffff}FRAME", frame_percent);
    }
    rib_F32(&tt, "\\ouc{#00ff7fff}P_\\ouc{#ffffffff}FPS", (F32)tooltip_track->e_frequency);
    rib_F32(&tt, "\\ouc{#00ff7fff}P_\\ouc{#ffffffff}AVG_FPS", (F32)tooltip_track->avg_e_frequency);

    // Cycle info
    unit_to_pretty_prefix_u("C", tooltip_track->delta_cycles, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    rib_STR(&tt, "\\ouc{#ff69b4ff}C_\\ouc{#ffffffff}CURRENT", pretty_buffer);
    unit_to_pretty_prefix_u("C", tooltip_track->avg_delta_cycles, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    rib_STR(&tt, "\\ouc{#ff69b4ff}C_\\ouc{#ffffffff}AVERAGE", pretty_buffer);
    unit_to_pretty_prefix_u("C", tooltip_track->min_delta_cycles, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    rib_STR(&tt, "\\ouc{#ff69b4ff}C_\\ouc{#ffffffff}MIN", pretty_buffer);
    unit_to_pretty_prefix_u("C", tooltip_track->max_delta_cycles, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    rib_STR(&tt, "\\ouc{#ff69b4ff}C_\\ouc{#ffffffff}MAX", pretty_buffer);

    render_tooltip_draw(&tt);
}

void profiler_draw() {
    if (c_video__fps_info) { i_draw_frame_info(); }

    if (c_profiler__flame_graph_enabled) {
        ProfilerFlameGraph *fg = &g_profiler.flame_graph;

        Vector2 const res = render_get_render_resolution();

        // Layout constants
        F32 const flame_graph_padding_x   = 10.0F;
        F32 const track_height_multiplier = 2.00F;
        F32 const text_padding_left       = 5.0F;
        F32 const min_frame_duration      = 0.0001F;

        // Graph dimensions
        F32 const track_height  = (F32)c_profiler__flame_graph_font_size * track_height_multiplier;
        F32 const graph_width   = res.x - (flame_graph_padding_x * 2.0F);
        Vector2 const graph_pos = {flame_graph_padding_x, c_profiler__flame_graph_position_y * res.y};

        // Font and text styling
        AFont *font                            = asset_get_font(c_profiler__flame_graph_font.cstr, c_profiler__flame_graph_font_size);
        Color const track_text_color           = WHITE;
        Color const track_text_shadow_color    = BLACK;
        Vector2 const track_text_shadow_offset = {1.0F, 1.0F};

        // Track colors and styling - varied palette with good contrast
        SZ const track_color_count = 8;
        Color const track_colors[track_color_count] = {
            NAYBLUE, EVAFATGREEN, EVAFATORANGE, EVAFATPURPLE, DEEPOCEANBLUE, HACKERGREEN, EVAFATBLUE, PERSIMMON,
        };

        // Mouse interaction
        Vector2 const mouse_pos                   = input_get_mouse_position();
        BOOL some_collision                       = false;
        ProfilerFlameGraphTrack *hovered_fg_track = nullptr;

        // Static variables for drag state tracking
        BOOL static is_dragging               = false;
        BOOL static was_mouse_down_last_frame = false;

        for (SZ i = 0; i < fg->track_count; ++i) {
            ProfilerFlameGraphTrack *fg_track = &fg->tracks[i];

            F64 const frame_duration = fg->end_time - fg->start_time;
            if (frame_duration <= min_frame_duration) { continue; }

            F64 const relative_start         = (fg_track->start_time - fg->start_time) / frame_duration;
            F64 const relative_width         = (fg_track->end_time - fg_track->start_time) / frame_duration;
            F32 const padding_between_tracks = 1.0F;

            Rectangle const track_rec = {
                graph_pos.x + ((F32)relative_start * graph_width) - padding_between_tracks,
                graph_pos.y + ((F32)(fg_track->depth - 1) * track_height) - padding_between_tracks,
                ((F32)relative_width * graph_width) - padding_between_tracks,
                track_height - padding_between_tracks,
            };

            if (track_rec.width < 1.0F) { continue; }

            SZ const color_index = (fg_track->depth - 1) % track_color_count;
            auto base_color      = g_profiler.flame_graph.paused ? color_lerp(track_colors[color_index], NAYGREEN, 0.5F) : track_colors[color_index];

            // Check for hover effect
            BOOL const is_hovered = CheckCollisionPointRec(mouse_pos, track_rec);

            // Enhanced hover effects and selection highlighting
            Color track_color = base_color;

            // Check if this track is selected
            BOOL is_selected = false;
            for (SZ s = 0; s < fg->selected_track_count; ++s) {
                if (fg->selected_tracks[s] == fg_track) {
                    is_selected = true;
                    break;
                }
            }

            if (is_selected) {
                // Selected track gets subtle highlighting - just slightly brighter
                track_color = color_blend(track_color, WHITE);
                track_color = Fade(track_color, 0.9F);  // Constant subtle brightness
            } else if (is_hovered) {
                // Apply multiple color effects for stronger visual feedback
                track_color     = color_saturated(track_color);                           // Increase saturation
                track_color     = color_blend(track_color, WHITE);                        // Brighten with white blend
                F32 const pulse = (F32)((math_sin_f64(time_get() * 8.0) * 0.15) + 0.85);  // Subtle pulsing
                track_color     = Fade(track_color, pulse);                               // Apply pulse effect
            }

            // Draw main rectangle with rounded corners
            F32 const roundness = 0.15F;
            S32 const segments  = 8;
            d2d_rectangle_rounded_rec(track_rec, roundness, segments, track_color);

            // Add glowing border effect only on hover (not selection)
            if (is_hovered) {
                F32 const border_pulse = (F32)((math_sin_f64(time_get() * 12.0) * 0.3) + 0.7);
                Color border_color     = color_blend(WHITE, YELLOW);
                border_color = Fade(border_color, border_pulse);

                // Draw border with slight expansion
                Rectangle const border_rec = {track_rec.x - 1.0F, track_rec.y - 1.0F, track_rec.width + 2.0F, track_rec.height + 2.0F};
                d2d_rectangle_rounded_rec_lines(border_rec, roundness, segments, border_color);
            }

            // Constants for text fitting and fading
            F32 const text_fit_threshold      = 1.0F;   // Minimum fit factor to draw text
            F32 const fade_start_threshold    = 1.1F;   // Start fading when slightly above threshold
            F32 const text_alpha_threshold    = 0.01F;  // Minimum alpha to draw
            F32 const shadow_alpha_multiplier = 0.8F;

            // Get text and its dimensions
            F64 const track_duration = fg_track->end_time - fg_track->start_time;
            C8 pretty_time[PRETTY_BUFFER_SIZE];
            // Convert from seconds to nanoseconds (unit_to_pretty_time_f expects nanoseconds)
            F64 const track_duration_ns = BASE_TO_NANO(track_duration);
            unit_to_pretty_time_f(track_duration_ns, pretty_time, PRETTY_BUFFER_SIZE, UNIT_TIME_MILLISECONDS);
            String const *track_text      = TS("%s \\ouc{#c0d6e4ff}(%s)", fg_track->label, pretty_time);
            Vector2 const text_dimensions = measure_text_ouc(font, track_text->c);

            // Calculate available width and fit factor
            F32 const available_width = track_rec.width - (2.0F * text_padding_left);
            F32 const fit_factor      = available_width / text_dimensions.x;

            // Calculate text alpha based on fit factor
            F32 text_alpha = 1.0F;
            if (fit_factor < text_fit_threshold) {
                text_alpha = 0.0F;  // Don't draw if text doesn't fit
            } else if (fit_factor < fade_start_threshold) {
                F32 const t = (fit_factor - text_fit_threshold) / (fade_start_threshold - text_fit_threshold);
                text_alpha = ease_out_back(t, 0.0F, 1.0F, 1.0F);
            }

            // Draw text if visible enough
            if (text_alpha > text_alpha_threshold) {
                // Calculate centered text position
                Vector2 const text_position = {track_rec.x + text_padding_left, track_rec.y + ((track_rec.height - text_dimensions.y) / 2.0F)};

                // Apply fade to colors and adjust for hover
                Color base_text_color   = track_text_color;
                Color base_shadow_color = track_text_shadow_color;

                if (is_hovered) {
                    // Enhanced text hover effects
                    F32 const text_pulse = (F32)((math_sin_f64(time_get() * 10.0) * 0.2) + 0.8);  // Faster pulse for text
                    base_text_color      = color_blend(YELLOW, WHITE);                            // Bright yellow-white blend
                    base_text_color      = Fade(base_text_color, text_pulse);                     // Apply pulsing
                    base_shadow_color    = color_blend(ORANGE, RED);                              // Warm shadow color
                }

                Color const faded_text_color   = Fade(base_text_color, text_alpha);
                Color const faded_shadow_color = Fade(base_shadow_color, text_alpha * shadow_alpha_multiplier);

                // Draw text with shadow
                d2d_text_ouc_shadow(font, track_text->c, text_position, faded_text_color, faded_shadow_color, track_text_shadow_offset);
            }

            if (is_hovered) {
                hovered_fg_track = fg_track;
                some_collision   = true;
            }
        }

        // Button configuration
        F32 const button_height  = 35.0F;
        F32 const button_width   = 50.0F;
        F32 const button_spacing = 8.0F;

        // Calculate button Y position - above frame graph if shown, otherwise above flame graph
        F32 buttons_y = graph_pos.y - button_height - 5.0F;
        if (fg->selected_track) {
            // Calculate frame graph dimensions (same logic as below)
            F32 legend_height = 0.0F;
            if (fg->selected_track_count > 0) {
                F32 const legend_entry_height = (F32)c_profiler__frame_graph_legend_font_size + 6.0F;
                F32 const legend_padding      = 20.0F;
                legend_height = ((F32)fg->selected_track_count * legend_entry_height) + (legend_padding * 2.0F);
            }
            F32 const frame_graph_height = 180.0F;
            F32 const container_padding  = 30.0F;
            F32 const total_height       = glm::max(frame_graph_height, legend_height + container_padding);
            buttons_y = graph_pos.y - total_height - 20.0F - button_height - 5.0F;
        }

        // Ensure buttons stay within screen bounds (5px margins)
        if (buttons_y < 5.0F) {
            F32 const adjustment = 5.0F - buttons_y;
            c_profiler__flame_graph_position_y += adjustment / res.y;
        }
        F32 const buttons_bottom = buttons_y + button_height;
        if (buttons_bottom > res.y - 5.0F) {
            F32 const adjustment = buttons_bottom - (res.y - 5.0F);
            c_profiler__flame_graph_position_y -= adjustment / res.y;
        }

        // Button rectangles and hover detection
        Rectangle const drag_handle       = {graph_pos.x, buttons_y, button_width, button_height};
        Rectangle const select_all_button = {graph_pos.x + button_width + button_spacing, buttons_y, button_width, button_height};
        BOOL const handle_hovered         = CheckCollisionPointRec(mouse_pos, drag_handle);
        BOOL const select_all_hovered     = CheckCollisionPointRec(mouse_pos, select_all_button);

        // Draw buttons using helper function
        AFont *button_font = asset_get_font(c_profiler__frame_graph_button_font.cstr, c_profiler__frame_graph_button_font_size);
        i_draw_profiler_button(drag_handle, ":::", handle_hovered, DARKGRAY, button_font);
        i_draw_profiler_button(select_all_button, "ALL", select_all_hovered, NAYGREEN, button_font);

        // Handle drag state transitions and track selection
        BOOL const mouse_down_this_frame = input_is_mouse_down(I_MOUSE_LEFT);

        // Handle mouse click events
        if (mouse_down_this_frame && !was_mouse_down_last_frame) {
            if (handle_hovered) {
                // Handle clicked - start dragging
                is_dragging = true;
            } else if (select_all_hovered) {
                // ALL button clicked - toggle between select all and deselect all
                BOOL const all_tracks_selected = (fg->selected_track_count == fg->track_count && fg->track_count > 0);

                if (all_tracks_selected) {
                    // Deselect all tracks
                    fg->selected_track_count = 0;
                    fg->selected_track       = nullptr;
                } else {
                    // Select all visible tracks
                    fg->selected_track_count = 0;
                    fg->selected_track       = nullptr;

                    // Add all visible tracks to selection
                    SZ const tracks_to_select = glm::min(fg->track_count, (SZ)PROFILER_TRACK_MAX_COUNT);
                    for (SZ i = 0; i < tracks_to_select; ++i) {
                        fg->selected_tracks[fg->selected_track_count] = &fg->tracks[i];
                        fg->selected_track_count++;
                    }

                    // Set first track as primary if any were selected
                    if (fg->selected_track_count > 0) {
                        fg->selected_track = fg->selected_tracks[0];
                        // Clear all track histories when selecting all
                        for (SZ i = 0; i < PROFILER_TRACK_MAX_COUNT; ++i) {
                            fg->selected_tracks_history_indices[i] = 0;
                            fg->selected_tracks_max_time[i] = 0.0;
                            for (SZ j = 0; j < PROFILER_FRAME_TIMES_TIMELINE_MAX_COUNT; ++j) { fg->selected_tracks_history[i][j] = 0.0; }
                        }
                    }
                }
            } else if (some_collision) {
                // Track selection - simple toggle

                // Check if track is already selected
                BOOL already_selected = false;
                SZ selected_index = 0;
                for (SZ i = 0; i < fg->selected_track_count; ++i) {
                    if (fg->selected_tracks[i] == hovered_fg_track) {
                        already_selected = true;
                        selected_index   = i;
                        break;
                    }
                }

                if (already_selected) {
                    // Remove from selection
                    for (SZ i = selected_index; i < fg->selected_track_count - 1; ++i) { fg->selected_tracks[i] = fg->selected_tracks[i + 1]; }
                    fg->selected_track_count--;

                    // Update primary selected track
                    fg->selected_track = (fg->selected_track_count > 0) ? fg->selected_tracks[0] : nullptr;
                } else {
                    // Add to selection (if we have space)
                    if (fg->selected_track_count < PROFILER_TRACK_MAX_COUNT) {
                        fg->selected_tracks[fg->selected_track_count] = hovered_fg_track;
                        fg->selected_track_count++;

                        // Set as primary if it's the first selection
                        if (fg->selected_track_count == 1) {
                            fg->selected_track = hovered_fg_track;
                            // Clear track histories when selecting a new primary track
                            for (SZ i = 0; i < PROFILER_TRACK_MAX_COUNT; ++i) {
                                fg->selected_tracks_history_indices[i] = 0;
                                fg->selected_tracks_max_time[i]        = 0.0;
                                for (SZ j = 0; j < PROFILER_FRAME_TIMES_TIMELINE_MAX_COUNT; ++j) { fg->selected_tracks_history[i][j] = 0.0; }
                            }
                        }
                    }
                }
            }
            // Removed the else clause that started dragging on any empty area click
        }

        // Stop dragging when mouse is released
        if (!mouse_down_this_frame && was_mouse_down_last_frame) { is_dragging = false; }

        // Apply dragging movement
        if (is_dragging && mouse_down_this_frame) {
            F32 const mouse_delta_y = input_get_mouse_delta().y;
            c_profiler__flame_graph_position_y += mouse_delta_y / res.y;
        }

        // Update last frame state
        was_mouse_down_last_frame = mouse_down_this_frame;

        // Draw frame graph for selected track ABOVE the flame graph
        if (fg->selected_track) {
            // Use dedicated frame graph font settings
            AFont *y_axis_font = asset_get_font(c_profiler__frame_graph_y_axis_font.cstr, c_profiler__frame_graph_y_axis_font_size);

            // Calculate legend dimensions first to determine total height needed
            F32 legend_width   = 0.0F;
            F32 legend_height  = 0.0F;
            AFont *legend_font = nullptr;

            // Legend constants
            F32 const legend_padding        = 20.0F;
            F32 const legend_entry_height   = (F32)c_profiler__frame_graph_legend_font_size + 6.0F;
            F32 const color_to_text_spacing = 10.0F;
            F32 const color_box_size        = (F32)c_profiler__frame_graph_legend_font_size;

            if (fg->selected_track_count > 0) {
                legend_font = asset_get_font(c_profiler__frame_graph_legend_font.cstr, c_profiler__frame_graph_legend_font_size);

                // Find longest track name using legend font
                F32 max_text_width = 0.0F;
                for (SZ i = 0; i < fg->selected_track_count; ++i) {
                    String const *track_name = TS("%s", fg->selected_tracks[i]->label);
                    Vector2 const text_size  = measure_text(legend_font, track_name->c);
                    max_text_width           = glm::max(max_text_width, text_size.x);
                }

                legend_width  = legend_padding + color_box_size + color_to_text_spacing + max_text_width + legend_padding;
                legend_height = ((F32)fg->selected_track_count * legend_entry_height) + (legend_padding * 2.0F);
            }

            // Determine total height needed - use the larger of frame graph or legend height
            F32 const frame_graph_height = 180.0F;
            F32 const container_padding  = 30.0F;  // Top and bottom padding for the container
            F32 const total_height       = glm::max(frame_graph_height, legend_height + container_padding);

            F32 const container_y  = graph_pos.y - total_height - 20.0F;
            Rectangle const bg_rec = {graph_pos.x, container_y, graph_width, total_height};

            // Background with sharp corners
            d2d_rectangle_rec(bg_rec, Fade(BLACK, 0.55F));

            // Dynamic margins - legend takes space from the right, Y-axis uses its own font for sizing
            F32 const label_sample_width = measure_text(y_axis_font, "16.67ms").x;
            F32 const left_margin        = label_sample_width + 15.0F;
            F32 const top_margin         = 15.0F;
            F32 const right_margin       = 15.0F + legend_width + (legend_width > 0 ? 10.0F : 0.0F);  // Extra spacing if legend exists
            F32 const bottom_margin      = 15.0F;

            Rectangle const plot_area = {
                bg_rec.x + left_margin,
                bg_rec.y + top_margin,
                bg_rec.width - left_margin - right_margin,
                bg_rec.height - top_margin - bottom_margin,
            };

            // Use linear scaling for more intuitive reading
            F64 const min_time    = 0.0;
            SZ const history_size = PROFILER_FRAME_TIMES_TIMELINE_MAX_COUNT;

            // Find actual max value across all selected tracks for dynamic scaling (using cached max values)
            F64 max_time = 1.0 / 60.0;  // Default to ~16.67ms (60fps)
            for (SZ track_idx = 0; track_idx < fg->selected_track_count; ++track_idx) {
                max_time = glm::max(max_time, fg->selected_tracks_max_time[track_idx]);
            }
            // Add 10% padding to the top
            max_time *= 1.1;

            // Grid lines and labels with proper font
            SZ const grid_count = 10;
            for (SZ i = 0; i <= grid_count; ++i) {
                F32 const t = (F32)i / (F32)grid_count;
                F32 const y = plot_area.y + (plot_area.height * t);

                // Grid line
                d2d_line_ex({plot_area.x, y}, {plot_area.x + plot_area.width, y}, 1.0F, Fade(WHITE, 0.2F));

                // Time label using linear scaling and Y-axis font
                C8 time_str[32];
                F64 const time_val = max_time - (t * (max_time - min_time));
                unit_to_pretty_time_f(BASE_TO_NANO(time_val), time_str, 32, UNIT_TIME_MILLISECONDS);

                Vector2 const text_size = measure_text(y_axis_font, time_str);
                Vector2 const label_pos = {plot_area.x - text_size.x - 8, y - (text_size.y * 0.5F)};
                d2d_text(y_axis_font, time_str, label_pos, Fade(WHITE, 0.8F));
            }

            // Check for legend hover first (before drawing lines)
            SZ hovered_legend_track_idx = SIZE_MAX;  // Track which legend item is hovered
            if (fg->selected_track_count > 0 && legend_font) {
                // Legend positioning (reuse existing variables)
                Vector2 const legend_pos = {plot_area.x + plot_area.width + 10.0F, plot_area.y + ((plot_area.height - legend_height) * 0.5F)};

                // Check hover for each legend entry
                for (SZ i = 0; i < fg->selected_track_count; ++i) {
                    F32 const entry_y                = legend_pos.y + legend_padding + ((F32)i * legend_entry_height);
                    Rectangle const legend_entry_rec = {legend_pos.x + legend_padding, entry_y, legend_width - (legend_padding * 2.0F), legend_entry_height};
                    if (CheckCollisionPointRec(mouse_pos, legend_entry_rec)) {
                        hovered_legend_track_idx = i;
                        break;
                    }
                }
            }

            // Draw multi-track lines with their respective legend colors
            if (fg->selected_track_count > 0) {
                // Same legend colors as in the legend

                // Draw each selected track
                for (SZ track_idx = 0; track_idx < fg->selected_track_count; ++track_idx) {
                    // Count valid data points for this track
                    SZ valid_count = 0;
                    for (SZ i = 0; i < history_size; ++i) {
                        if (fg->selected_tracks_history[track_idx][i] > 0.0) { valid_count++; }
                    }

                    if (valid_count > 1) {
                        F32 const x_step = plot_area.width / (F32)(valid_count - 1);
                        Vector2 points[history_size];
                        SZ points_plotted = 0;

                        // Collect data points
                        for (SZ i = 0; i < history_size; ++i) {
                            SZ const idx       = (fg->selected_tracks_history_indices[track_idx] + i) % history_size;
                            F64 const time_val = fg->selected_tracks_history[track_idx][idx];

                            if (time_val > 0.0) {
                                // X position
                                F32 const x = plot_area.x + ((F32)points_plotted * x_step);

                                // Y position using linear scaling
                                F64 const clamped_time = glm::clamp(time_val, min_time, max_time);
                                F32 const norm_y       = (F32)((clamped_time - min_time) / (max_time - min_time));
                                F32 const y            = plot_area.y + plot_area.height - (norm_y * plot_area.height);

                                points[points_plotted++] = {x, y};
                            }
                        }

                        // Draw crisp connected lines for this track (no dots)
                        if (points_plotted > 1) {
                            Color track_color  = color_hue_by_seed((U32)track_idx);
                            F32 line_thickness = 2.0F;

                            // Apply hover effects if this track's legend is hovered
                            if (hovered_legend_track_idx == track_idx) {
                                track_color    = color_sync_blinking_very_fast(track_color, color_invert(track_color));
                                line_thickness = 4.0F;  // Make line thicker when hovered
                            }

                            if (line_thickness <= 2.0F) {
                                // Use fast line strip for normal thickness
                                d2d_line_strip(points, points_plotted, track_color);
                            } else {
                                // Fall back to individual thick lines for hover effect
                                for (SZ i = 0; i < points_plotted - 1; ++i) { d2d_line_ex(points[i], points[i + 1], line_thickness, track_color); }
                            }
                        }
                    }
                }
            }

            // Draw legend to the right of the graph
            if (fg->selected_track_count > 0 && legend_width > 0 && legend_font) {
                // Legend positioning - to the right of the graph, use pre-calculated height
                Vector2 const legend_pos = {
                    plot_area.x + plot_area.width + 10.0F,                     // Right of the graph with spacing
                    plot_area.y + ((plot_area.height - legend_height) * 0.5F)  // Vertically centered
                };

                // Legend entries (no background needed since it's on the frame graph)
                for (SZ i = 0; i < fg->selected_track_count; ++i) {
                    ProfilerFlameGraphTrack const *track = fg->selected_tracks[i];
                    Color const track_color = color_hue_by_seed((U32)i);

                    F32 const entry_y = legend_pos.y + legend_padding + ((F32)i * legend_entry_height);

                    // Create hover area for this legend entry
                    Rectangle const legend_entry_rec = {legend_pos.x + legend_padding, entry_y, legend_width - (legend_padding * 2.0F), legend_entry_height};
                    BOOL const entry_hovered         = CheckCollisionPointRec(mouse_pos, legend_entry_rec);

                    // Color indicator box
                    Rectangle const color_box = {legend_pos.x + legend_padding, entry_y + 3.0F, color_box_size, color_box_size};
                    Color const box_color     = entry_hovered ? color_sync_blinking_very_fast(track_color, color_invert(track_color)) : track_color;
                    d2d_rectangle_rounded_rec(color_box, 0.1F, 4, box_color);

                    // Track name using legend font
                    F32 const text_x       = legend_pos.x + legend_padding + color_box_size + 10.0F;  // More spacing
                    Vector2 const text_pos = {text_x, entry_y + 3.0F};
                    Color const text_color = entry_hovered ? color_sync_blinking_very_fast(WHITE, YELLOW) : WHITE;
                    d2d_text(legend_font, track->label, text_pos, text_color);
                }
            }
        }

        if (hovered_fg_track) { i_draw_hover_frame_graph_track_info(hovered_fg_track, font); }
    }
}

void profiler_finalize() {
    if (g_profiler.flame_graph.paused) { return; }

    ProfilerFlameGraph *fg = &g_profiler.flame_graph;
    fg->main_thread_track  = *profiler_get_track(ML_NAME);

    F64 const new_start = fg->main_thread_track.start_time;
    F64 const new_end   = fg->main_thread_track.end_time;

    fg->start_time  = new_start;
    fg->end_time    = new_end;
    fg->track_count = 0;

    C8 const **label           = nullptr;
    ProfilerTrack const *track = nullptr;
    MAP_EACH_PTR(&g_profiler.track_map, label, track) {
        // NOTE:
        // Skipping threads (depth 0) and profiler_finalize itself. We check if it is depth 1 before checking
        // for "profiler_finalize" label to avoid redundant string comparisons with tracks that are definitely
        // not the track we are looking for.
        if (track->depth == 0 || (track->depth == 1 && ou_strstr(track->label, "profiler_finalize"))) { continue; }

        ProfilerFlameGraphTrack *fg_track = &fg->tracks[fg->track_count];

        F32 const new_track_start = (F32)track->start_time;
        F32 const new_track_end   = (F32)track->end_time;

        ou_strncpy(fg_track->label, track->label, PROFILER_TRACK_MAX_LABEL_LENGTH - 1);
        fg_track->label[PROFILER_TRACK_MAX_LABEL_LENGTH - 1] = '\0';
        fg_track->depth      = track->depth;
        fg_track->start_time = new_track_start;
        fg_track->end_time   = new_track_end;
        fg_track->track_data = *track;

        fg->track_count++;
    }

    // Update selected tracks history for frame graph
    for (SZ sel_idx = 0; sel_idx < fg->selected_track_count; ++sel_idx) {
        ProfilerFlameGraphTrack const *selected_track = fg->selected_tracks[sel_idx];

        // Find the current data for this selected track
        for (SZ i = 0; i < fg->track_count; ++i) {
            if (ou_strcmp(fg->tracks[i].label, selected_track->label) == 0) {
                // Add timing data to this track's history
                SZ const history_size = PROFILER_FRAME_TIMES_TIMELINE_MAX_COUNT;
                F64 const timing_data = fg->tracks[i].end_time - fg->tracks[i].start_time;
                fg->selected_tracks_history[sel_idx][fg->selected_tracks_history_indices[sel_idx]] = timing_data;
                fg->selected_tracks_history_indices[sel_idx] = (fg->selected_tracks_history_indices[sel_idx] + 1) % history_size;

                // Update running max for performance optimization
                fg->selected_tracks_max_time[sel_idx] = glm::max(timing_data, fg->selected_tracks_max_time[sel_idx]);

                break;
            }
        }
    }
}

ProfilerTrack *profiler_get_track(C8 const *label) {
    return ProfilerTrackMap_get(&g_profiler.track_map, label);
}

void profiler_track_begin(C8 const *label) {
    // We get the start related values as soon as possible.
    F64 const start_time   = time_get_glfw();
    U64 const start_cycles = GET_CYCLES();

    ProfilerTrack *track = profiler_get_track(label);
    if (!track) {
        ProfilerTrack new_track = {};
        SZ const dest_size = sizeof(new_track.label);
        ou_strncpy(new_track.label, label, dest_size - 1);
        new_track.label[dest_size - 1] = '\0';
        i_reset_track(&new_track);
        ProfilerTrackMap_insert(&g_profiler.track_map, label, new_track);
        track = ProfilerTrackMap_get(&g_profiler.track_map, label);
    }

    i_start_frame(track, start_time, start_cycles);
}

void profiler_track_end(C8 const *label) {
    ProfilerTrack *track = profiler_get_track(label);
    if (!track) {
        lle("Track %s not found, cannot end since it was never started", label);
        return;
    }

    // We get the end related stuff as late as possible.
    i_end_frame(track);
}

void profiler_reset() {
    C8 const **label     = nullptr;
    ProfilerTrack *track = nullptr;
    MAP_EACH_PTR(&g_profiler.track_map, label, track) {
        track->want_reset = true;
    }
    g_profiler.flame_graph.want_reset = true;
    time_reset();
}
