#include "debug.hpp"
#include "array.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "common.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "ini.hpp"
#include "input.hpp"
#include "log.hpp"
#include "math.hpp"
#include "memory.hpp"
#include "menu.hpp"
#include "message.hpp"
#include "option.hpp"
#include "particles_2d.hpp"
#include "particles_3d.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"
#include "unit.hpp"
#include "world.hpp"

#include <raymath.h>
#include <glm/gtc/type_ptr.hpp>

DBG g_dbg = {};

C8 static const *i_dbg_window_name_strings[DBG_WID_COUNT] = {
    "REND",
    "MEMO",
    "SCEN",
    "WORL",
    "ASSE",
    "OPTI",
    "PERF",
    "AUDI",
    "TARG",
    "CUS1",
    "CUS2",
};

Color static const i_dbg_window_highlight_colors[DBG_WID_COUNT] = {
    color_darken({0xff, 0x00, 0x00, 0xff}, 0.35F),  // Render. (Red)
    color_darken({0xff, 0x7f, 0x00, 0xff}, 0.35F),  // Memory. (Orange)
    color_darken({0xff, 0xff, 0x00, 0xff}, 0.35F),  // Scene. (Yellow)
    color_darken({0x00, 0xff, 0x00, 0xff}, 0.35F),  // World. (Green)
    color_darken({0x00, 0x00, 0xff, 0xff}, 0.35F),  // Asset. (Blue)
    color_darken({0xff, 0x00, 0xff, 0xff}, 0.35F),  // Options. (Magenta)
    color_darken({0x7f, 0x00, 0xff, 0xff}, 0.35F),  // Performance. (Purple)
    color_darken({0xff, 0xc0, 0xcb, 0xff}, 0.35F),  // Audio. (Pink)
    color_darken({0xa5, 0x2a, 0x2a, 0xff}, 0.35F),  // Targets. (Brown)
    color_darken({0x00, 0x00, 0x00, 0xff}, 0.35F),  // Custom. (Black)
    color_darken({0x00, 0x00, 0x00, 0xff}, 0.35F),  // Custom. (Black)
};

Color static const i_dbg_button_state_colors[DBG_WIDGET_MOUSE_STATE_COUNT] = {
    NEARNEARBLACK,                  // Normal.
    EVAPURPLE,                      // Hover.
    color_darken(EVAORANGE, 0.5F),  // Pressed.
};

void static i_reset_windows(void *data) {
    unused(data);

    dbg_reset_windows(true);
    dbg_save_ini();
}

void static i_toggle_pause(void *data) {
    unused(data);

    F32 static last_dt_mod = 1.0F;
    F32 const dt_mod       = time_get_delta_mod();

    if (dt_mod == 0.0F) {
        time_set_delta_mod(last_dt_mod);
    } else {
        last_dt_mod = dt_mod;
        time_set_delta_mod(0.0F);
    }
}

#define qi(label, value) i_setup_typical_label_value((label), (value))
void static i_setup_typical_label_value(C8 const *label, C8 const *value) {
    AFont *font = g_dbg.medium_font;
    dwil(label, font, NAYBEIGE);
    dwil(TS("%s", value)->c, font, GREEN);
    dwis(3.0F);
}

#define qil(label, value) i_setup_typical_label_value_with_sep((label), (value))
void static i_setup_typical_label_value_with_sep(C8 const *label, C8 const *value) {
    AFont *font = g_dbg.medium_font;
    dwil(label, font, NAYBEIGE);
    dwil(TS(">> %s", value)->c, font, GREEN);
    dwis(3.0F);
}

#define qilc(label, value, value_color) i_setup_typical_label_value_with_sep_and_color((label), (value), (value_color))
void static i_setup_typical_label_value_with_sep_and_color(C8 const *label, C8 const *value, Color const value_color) {
    AFont *font = g_dbg.medium_font;
    dwil(label, font, NAYBEIGE);
    dwil(TS(">> %s", value)->c, font, value_color);
    dwis(3.0F);
}


#define qilcs(label, value, value_color) i_setup_typical_label_value_with_sep_color_and_shadow((label), (value), (value_color))
void static i_setup_typical_label_value_with_sep_color_and_shadow(C8 const *label, C8 const *value, Color const value_color) {
    AFont *font = g_dbg.medium_font;
    dwil(label, font, NAYBEIGE);
    dwils(TS(">> %s", value)->c, font, value_color);
    dwis(3.0F);
}

#define qilv3i(label, value) i_setup_typical_label_vec3_with_sep((label), (value), false)
#define qilv3f(label, value) i_setup_typical_label_vec3_with_sep((label), (value), true)
void static i_setup_typical_label_vec3_with_sep(C8 const *label, Vector3 value, BOOL as_float) {
    AFont *font         = g_dbg.medium_font;
    C8 const *x_color   = "#ff0000ff";
    C8 const *y_color   = "#00ff00ff";
    C8 const *z_color   = "#0000ffff";
    C8 const *num_color = "#00e430ff";
    C8 const *format    = as_float ? "%6.2f" : "%.0f";
    C8 const *t         = TS(">> \\ouc{%%s}%%s:\\ouc{%s}   %s", num_color, format)->c;

    dwil(label, font, NAYBEIGE);
    dwilo(TS(t, x_color, "X", value.x)->c, font, DARKGRAY);
    dwilo(TS(t, y_color, "Y", value.y)->c, font, DARKGRAY);
    dwilo(TS(t, z_color, "Z", value.z)->c, font, DARKGRAY);
    dwis(3.0F);
}

#define qilv2i(label, value) i_setup_typical_label_vec2_with_sep((label), (value), false)
#define qilv2f(label, value) i_setup_typical_label_vec2_with_sep((label), (value), true)
void static i_setup_typical_label_vec2_with_sep(C8 const *label, Vector2 value, BOOL as_float) {
    AFont    *font      = g_dbg.medium_font;
    C8 const *x_color   = "#ff0000ff";
    C8 const *y_color   = "#00ff00ff";
    C8 const *num_color = "#00e430ff";
    C8 const *format    = as_float ? "%6.2f" : "%.0f";
    C8 const *t         = TS(">> \\ouc{%%s}%%s:\\ouc{%s}   %s", num_color, format)->c;

    dwil(label, font, NAYBEIGE);
    dwilo(TS(t, x_color, "X", value.x)->c, font, DARKGRAY);
    dwilo(TS(t, y_color, "Y", value.y)->c, font, DARKGRAY);

    dwis(3.0F);
}

void static i_setup_arena_info(C8 const *title, MemoryType type, ArenaStats *stats, Vector2 fillbar_size) {
    AFont *medium_font = g_dbg.medium_font;
    AFont *large_font  = g_dbg.large_font;

    C8 pretty_buffer_1[PRETTY_BUFFER_SIZE] = {};
    C8 pretty_buffer_2[PRETTY_BUFFER_SIZE] = {};

    unit_to_pretty_prefix_binary_u("B", stats->total_used, pretty_buffer_1, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);
    unit_to_pretty_prefix_binary_u("B", stats->total_capacity, pretty_buffer_2, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);

    dwil(title, large_font, color_darken(color_contrast(DBG_WINDOW_BG_COLOR), 0.50F));
    C8 const *value_color = "#d1b897ff";
    dwilo(TS("%8s: \\ouc{%s}%zu", "Arenas", value_color, stats->arena_count)->c, medium_font, GREEN);
    dwilo(TS("%8s: \\ouc{%s}%zu", "Allocs", value_color, stats->total_allocation_count)->c, medium_font, GREEN);
    dwilo(TS("%8s: \\ouc{%s}%s", "Used", value_color, pretty_buffer_1)->c, medium_font, GREEN);
    dwilo(TS("%8s: \\ouc{%s}%s", "Cap", value_color, pretty_buffer_2)->c, medium_font, GREEN);

    dwifb(nullptr, medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, fillbar_size, (F32)stats->total_used, 0.0F,
          (F32)stats->total_capacity, false, nullptr, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);

    ArenaTimeline *tl = memory_get_arena_timeline(type);

    F32 const max = type == MEMORY_TYPE_ARENA_DEBUG ? DBG_ARENA_ALLOCATIONS_TILL_RESET : 100000.0F;
    dwitl(LIME, NEARBLACK, tl->total_allocations_count, ARENA_TIMELINE_MAX_COUNT, 0.00F, max, "%.0f %s", "allocs");
}

void static i_setup_light_arena_info(C8 const *name, Arena *arena) {
    AFont *font = g_dbg.medium_font;

    C8 pretty_buffer_1[PRETTY_BUFFER_SIZE] = {};
    C8 pretty_buffer_2[PRETTY_BUFFER_SIZE] = {};

    unit_to_pretty_prefix_binary_u("B", arena->used, pretty_buffer_1, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);
    unit_to_pretty_prefix_binary_u("B", arena->capacity, pretty_buffer_2, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);

    String *label = TS("%s: %s / %s", name, pretty_buffer_1, pretty_buffer_2);
    dwil(label->c, font, GREEN);
}

void static i_call_cbs(DBGWID wid) {
    for (SZ j = 0; j < g_dbg.windows[wid].cb; ++j) {
        DBGWidgetCallback *cb = &g_dbg.windows[wid].cbs[j];
        cb->func(cb->data);
    }
}

void static i_setup_dbg_gui() {
    F32 const dt_mod    = time_get_delta_mod();
    AFont *small_font   = g_dbg.small_font;
    AFont *medium_font  = g_dbg.medium_font;
    AFont *large_font   = g_dbg.large_font;
    auto contrast_color = color_darken(color_contrast(DBG_WINDOW_BG_COLOR), 0.25F);

    // ===============================================================
    // ======================= Window: RENDER ========================
    // ===============================================================
    dwnd(DBG_WID_RENDER) {
        RenderMode *render_order = g_render.rmode_order;
        String *render_order_str = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);

        SZ longest_render_mode_str_len = 0;
        for (SZ i = 0; i < RMODE_COUNT; ++i) {
            longest_render_mode_str_len = glm::max(longest_render_mode_str_len, ou_strlen(render_mode_to_cstr(render_order[i])));
        }

        for (SZ i = 0; i < RMODE_COUNT; ++i) {
            SZ const draw_calls = g_render.rmode_data[i].previous_draw_call_count;
            C8 const *render_mode_str = render_mode_to_cstr(render_order[i]);
            C8 const *line_break = i < RMODE_COUNT && i != 0 ? "\n" : "";
            string_append(render_order_str, TS("%s%-*s (%zu)", line_break, (S32)(longest_render_mode_str_len + 1), render_mode_str, draw_calls)->c);
        }

        qil("Render Size", TS("%dx%d", c_video__render_resolution_width, c_video__render_resolution_height)->c);
        qil("Window Size", TS("%dx%d", c_video__window_resolution_width, c_video__window_resolution_height)->c);
        qi("Render Order", render_order_str->c);

        Color const clear_color   = scenes_get_clear_color();
        Color const ambient_color = render_get_ambient_color();
        Color const major_color   = g_render.sketch.major_color;
        Color const minor_color   = g_render.sketch.minor_color;

        qilcs("Clear Color", TS("%d %d %d %d", clear_color.r, clear_color.g, clear_color.b, clear_color.a)->c, clear_color);
        qilcs("Ambient Color", TS("%d %d %d %d", ambient_color.r, ambient_color.g, ambient_color.b, ambient_color.a)->c, ambient_color);
        qilcs("Major Color", TS("%d %d %d %d", major_color.r, major_color.g, major_color.b, major_color.a)->c, major_color);
        qilcs("Minor Color", TS("%d %d %d %d", minor_color.r, minor_color.g, minor_color.b, minor_color.a)->c, minor_color);

        for (SZ i = 0; i < RMODE_COUNT; ++i) {
            Color const tint_color = g_render.rmode_data[i].tint_color;

            // Skip if it's just the default color.
            if (color_is_equal(tint_color, RENDER_DEFAULT_TINT_COLOR)) { continue; }

            qilcs(TS("Tint Color %s", render_mode_to_cstr(render_order[i]))->c, TS("%d %d %d %d", tint_color.r, tint_color.g, tint_color.b, tint_color.a)->c, tint_color);
        }

        dwis(5.0F);

        dwib("Wireframe Mode", medium_font, g_render.wireframe_mode ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); g_render.wireframe_mode = !g_render.wireframe_mode; }, nullptr);
        dwib("Sketch Mode", medium_font, c_render__sketch_effect ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_render__sketch_effect = !c_render__sketch_effect; }, nullptr);
        dwib("Major Color", medium_font, g_render.sketch.major_color, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); render_sketch_set_major_color(color_random()); }, nullptr);
        dwib("Minor Color", medium_font, g_render.sketch.minor_color, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); render_sketch_set_minor_color(color_random()); }, nullptr);
        dwib("Swap", medium_font, WHITE, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); render_sketch_swap_colors(); }, nullptr);
        dwib("Both", medium_font, NAYBEIGE, DBG_REF_BUTTON_SIZE,
             [](void *data) {
                 unused(data);
                 Color major;
                 Color minor;
                 for (SZ i = 0; i < DBG_MAX_RANDOM_COLOR_ATTEMPTS; ++i) {
                     major = color_random();
                     minor = color_random();
                     if (!color_is_similar(major, minor, 0.2F)) { break; }
                 }
                 render_sketch_set_major_color(major);
                 render_sketch_set_minor_color(minor);
             }, nullptr);
        dwib("Both Pastel", medium_font, NAYBEIGE, DBG_REF_BUTTON_SIZE,
             [](void *data) {
                 unused(data);
                 Color major;
                 Color minor;
                 for (SZ i = 0; i < DBG_MAX_RANDOM_COLOR_ATTEMPTS; ++i) {
                     major = color_random_pastel();
                     minor = color_random_pastel();
                     if (!color_is_similar(major, minor, 0.1F)) { break; }
                 }
                 render_sketch_set_major_color(major);
                 render_sketch_set_minor_color(minor);
             }, nullptr);
        dwib("Both Vibrant", medium_font, NAYBEIGE, DBG_REF_BUTTON_SIZE,
             [](void *data) {
                 unused(data);
                 Color major;
                 Color minor;
                 for (SZ i = 0; i < DBG_MAX_RANDOM_COLOR_ATTEMPTS; ++i) {
                     major = color_random_vibrant();
                     minor = color_random_vibrant();
                     if (!color_is_similar(major, minor, 0.1F)) { break; }
                 }
                 render_sketch_set_major_color(major);
                 render_sketch_set_minor_color(minor);
             }, nullptr);
        dwib("Both Contrast", medium_font, NAYBEIGE, DBG_REF_BUTTON_SIZE,
             [](void *data) {
                 unused(data);
                 Color const major = color_random();
                 Color const minor = color_contrast(major);
                 render_sketch_set_major_color(major);
                 render_sketch_set_minor_color(minor);
             }, nullptr);
        dwib("Both Pastel+Contrast", medium_font, NAYBEIGE, DBG_REF_BUTTON_SIZE,
             [](void *data) {
                 unused(data);
                 Color const major = color_random_pastel();
                 Color const minor = color_contrast(major);
                 render_sketch_set_major_color(major);
                 render_sketch_set_minor_color(minor);
             }, nullptr);
        dwib("Both Vibrant+Contrast", medium_font, NAYBEIGE, DBG_REF_BUTTON_SIZE,
             [](void *data) {
                 unused(data);
                 Color const major = color_random_vibrant();
                 Color const minor = color_contrast(major);
                 render_sketch_set_major_color(major);
                 render_sketch_set_minor_color(minor);
             }, nullptr);

        // Light intensity
        F32 static last_intensity[LIGHTS_MAX] = {};
        for (SZ i = 0; i < LIGHTS_MAX; ++i) {
            Light *light = &g_lighting.lights[i];

            auto fg = DBG_FILLBAR_FG_COLOR;
            if (!light->enabled) { fg = color_darken(DBG_FILLBAR_FG_COLOR, 0.5F); }

            Color text_color;
            color_from_vec4(&text_color, light->color);

            dwifb(TS("Light %zu", i)->c, medium_font, fg, DBG_FILLBAR_BG_COLOR, text_color,
                  DBG_REF_FILLBAR_SIZE, light->intensity, 0.0F, LIGHT_DEFAULT_INTENSITY * 2.0F,
                  true, &light->intensity, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);

            if (last_intensity[i] != light->intensity) { light->dirty = true; }
            last_intensity[i] = light->intensity;
        }

        dwis(15.0F);

        // Particle system statistics
        dwil(TS("2DP (%d)", PARTICLES_2D_MAX)->c, medium_font, NAYBEIGE);
        dwilo(TS("%zu idx | %zu tex", g_particles2d.write_index, g_particles2d.textures.count)->c, medium_font, DARKGRAY);
        // Reorder 2D ring buffer for chronological display (oldest to newest, left to right)
        F32 static particles2d_ordered[PARTICLES_2D_SPAWN_RATE_HISTORY_SIZE] = {};
        SZ const particles2d_read_index = g_particles2d.spawn_rate_index;
        for (SZ i = 0; i < PARTICLES_2D_SPAWN_RATE_HISTORY_SIZE; ++i) {
            SZ const src_index     = (particles2d_read_index + i) % PARTICLES_2D_SPAWN_RATE_HISTORY_SIZE;
            particles2d_ordered[i] = g_particles2d.spawn_rate_history[src_index];
        }
        dwitl(LIME, NEARBLACK, particles2d_ordered, PARTICLES_2D_SPAWN_RATE_HISTORY_SIZE, 0.0F, PARTICLES_2D_MAX, "%.0f %s", "p/s");

        dwis(5.0F);

        dwil(TS("3DP (%d)", PARTICLES_3D_MAX)->c, medium_font, NAYBEIGE);
        dwilo(TS("%zu idx | %zu tex", g_particles3d.write_index, g_particles3d.textures.count)->c, medium_font, DARKGRAY);
        // Reorder 3D ring buffer for chronological display (oldest to newest, left to right)
        F32 static particles3d_ordered[PARTICLES_3D_SPAWN_RATE_HISTORY_SIZE] = {};
        SZ const particles3d_read_index = g_particles3d.spawn_rate_index;
        for (SZ i = 0; i < PARTICLES_3D_SPAWN_RATE_HISTORY_SIZE; ++i) {
            SZ const src_index     = (particles3d_read_index + i) % PARTICLES_3D_SPAWN_RATE_HISTORY_SIZE;
            particles3d_ordered[i] = g_particles3d.spawn_rate_history[src_index];
        }
        dwitl(CYAN, NEARBLACK, particles3d_ordered, PARTICLES_3D_SPAWN_RATE_HISTORY_SIZE, 0.0F, PARTICLES_3D_MAX, "%.0f %s", "p/s");

        i_call_cbs(DBG_WID_RENDER);
    }

    // ===============================================================
    // ======================= Window: MEMORY ========================
    // ===============================================================
    dwnd(DBG_WID_MEMORY) {
        ArenaStats perm_stats  = memory_get_previous_arena_stats(MEMORY_TYPE_ARENA_PERMANENT);
        ArenaStats trans_stats = memory_get_previous_arena_stats(MEMORY_TYPE_ARENA_TRANSIENT);
        ArenaStats debug_stats = memory_get_previous_arena_stats(MEMORY_TYPE_ARENA_DEBUG);
        ArenaStats math_stats  = memory_get_previous_arena_stats(MEMORY_TYPE_ARENA_MATH);

        i_setup_arena_info("PERM Arena",  MEMORY_TYPE_ARENA_PERMANENT, &perm_stats,  DBG_REF_FILLBAR_SIZE); dwis(5.0F);
        i_setup_arena_info("TRANS Arena", MEMORY_TYPE_ARENA_TRANSIENT, &trans_stats, DBG_REF_FILLBAR_SIZE); dwis(5.0F);
        i_setup_arena_info("DEBUG Arena", MEMORY_TYPE_ARENA_DEBUG, &debug_stats, DBG_REF_FILLBAR_SIZE); dwis(5.0F);
        i_setup_arena_info("MATH Arena",  MEMORY_TYPE_ARENA_MATH, &math_stats,  DBG_REF_FILLBAR_SIZE); dwis(5.0F);

        i_call_cbs(DBG_WID_MEMORY);
    }

    // ===============================================================
    // ======================== Window: SCENE ========================
    // ===============================================================
    dwnd(DBG_WID_SCENE) {
        Scene *current_scene      = &g_scenes.scenes[g_scenes.current_scene_type];
        Scene *overlay_scene      = &g_scenes.scenes[g_scenes.current_overlay_scene_type];
        Camera2D *active_camera2d = c2d_get_ptr();
        Camera3D *active_camera3d = c3d_get_ptr();

        qil("Scene", TS("%s (%d)", scenes_to_cstr(current_scene->type), current_scene->type)->c);
        qil("Overlay Scene", TS("%s (%d)", scenes_to_cstr(overlay_scene->type), overlay_scene->type)->c);

        for (SZ i = SCENE_NONE + 1; i < SCENE_COUNT; ++i) {
            auto scene_type = (SceneType)i;

            auto scene_color = RED;
            if (scene_type == g_scenes.current_scene_type) {
                scene_color = GREEN;
            } else if (scene_type == g_scenes.current_overlay_scene_type) {
                scene_color = BLUE;
            }

            dwib(scenes_to_cstr(scene_type), medium_font, scene_color, DBG_REF_BUTTON_SIZE,
                 [](void *data) {
                     auto scene = (SceneType)(SZ)data;
                     if (g_scenes.current_overlay_scene_type != SCENE_NONE) { scenes_pop_overlay_scene(false, false); }
                     scenes_set_scene(scene);
                 }, (void *)scene_type);
        }

        dwis(5.0F);

        dwil("Camera2D", large_font, contrast_color);
        qilv2i("Offset", active_camera2d->offset);
        qilv2i("Target", active_camera2d->target);
        qil("Rotation", TS("%.2f", active_camera2d->rotation)->c);
        qil("Zoom", TS("%.2f", active_camera2d->zoom)->c);

        dwis(2.5F);

        dwil("Camera3D", large_font, contrast_color);
        qilv3f("Position", active_camera3d->position);
        qilv3f("Target", active_camera3d->target);
        qilv3f("Up", active_camera3d->up);
        qil("Fovy", TS("%.2f", active_camera3d->fovy)->c);
        qil("Projection", TS("%s", active_camera3d->projection == CAMERA_PERSPECTIVE ? "Perspective" : "Orthographic")->c);
        dwis(2.5F);

        i_call_cbs(DBG_WID_SCENE);
    }

    // ===============================================================
    // ======================== Window: WORLD ========================
    // ===============================================================
    dwnd(DBG_WID_WORLD) {
        C8 pretty_buffer_1[PRETTY_BUFFER_SIZE] = {};
        C8 pretty_buffer_2[PRETTY_BUFFER_SIZE] = {};
        C8 pretty_buffer_3[PRETTY_BUFFER_SIZE] = {};

        unit_to_pretty_prefix_u("", g_world->visible_vertex_count, pretty_buffer_1, PRETTY_BUFFER_SIZE, UNIT_PREFIX_MEGA);

        dwil("World", large_font, contrast_color);
        qil("Visible Vertices", TS("%s", pretty_buffer_1)->c);
        qil("Max Generation", TS("%u", g_world->max_gen)->c);
        qil("Entities", TS("%u", g_world->active_ent_count)->c);
        qil("Selected Entity", g_world->selected_id != INVALID_EID ? TS("%u", g_world->selected_id)->c : "NO SELECTION");

        // NOTE: We want to skip "NONE" ergo starting at 1
        for (SZ i = 1; i < ENTITY_TYPE_COUNT; ++i) { qil(TS("TYPE: %s", entity_type_to_cstr((EntityType)i))->c, TS("%u", g_world->entity_type_counts[i])->c); }

        dwis(5.0F);

        SZ const recorder_cursor = world_recorder_get_record_cursor();
        SZ const playback_cursor = world_recorder_get_playback_cursor();
        BOOL const is_recording  = world_recorder_is_recording_state();
        BOOL const is_looping    = world_recorder_is_looping_state();
        String *record_string    = TS("WREC %04zu", recorder_cursor);
        String *loop_string      = TS("PLAY %04zu", playback_cursor);

        dwil("Recorder", large_font, contrast_color);

        // Size information
        SZ const single_state_size   = world_recorder_get_state_size_bytes();
        SZ const total_recorded_size = world_recorder_get_total_recorded_size_bytes();

        unit_to_pretty_prefix_binary_u("B", single_state_size, pretty_buffer_2, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);
        unit_to_pretty_prefix_binary_u("B", total_recorded_size, pretty_buffer_3, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);

        qil("Single State Size", pretty_buffer_2);
        qil("Total Recorded Size", pretty_buffer_3);

        dwis(3.0F);

        // Controls
        dwib(record_string->c, medium_font, is_recording ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); world_recorder_toggle_record_state(); }, nullptr);
        dwib(loop_string->c, medium_font, is_looping ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); world_recorder_toggle_loop_state(); }, nullptr);
        dwib("CLEAR", medium_font, GOLD, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); world_recorder_clear(); }, nullptr);
        dwib("CHECK DISK", medium_font, ORANGE, DBG_REF_BUTTON_SIZE,
             [](void *data) {
                 unused(data);
                 SZ const actual_disk_usage = world_recorder_get_actual_disk_usage_bytes();
                 if (actual_disk_usage == 0) { mi("No world state files found", RED); return; }
                 C8 pretty_buffer[PRETTY_BUFFER_SIZE] = {};
                 unit_to_pretty_prefix_binary_u("B", actual_disk_usage, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);
                 mio(TS("Actual Disk Usage: \\ouc{#00ff00ff}%s", pretty_buffer)->c, WHITE);
             }, nullptr);

        // Progress bar
        dwifb(nullptr, medium_font, ORANGE, DARKGRAY, BLANK, DBG_REF_FILLBAR_SIZE, (F32)playback_cursor, 0,(F32)recorder_cursor,
              true, world_recorder_get_playback_cursor_ptr(), DBG_WIDGET_DATA_TYPE_INT, DBG_WIDGET_CB_NONE);
        dwib("PURGE", medium_font, GRAY, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); world_recorder_delete_recorded_state(); }, nullptr);

        i_call_cbs(DBG_WID_WORLD);
    }

    // ===============================================================
    // ======================== Window: ASSET ========================
    // ===============================================================
    dwnd(DBG_WID_ASSET) {
        qil("Update Thread", g_assets.run_update_thread ? "Running" : "Stopped");
        qil("Update Interval", TS("%dms", A_RELOAD_THREAD_SLEEP_DURATION_MS)->c);

        dwib("Dump Usage Info", medium_font, DBG_WINDOW_FG_COLOR, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_console__enabled = true; asset_print_state(); }, nullptr);
        dwis(5.0F);

        C8 const *value_color = "#d1b897ff";
        dwilo(TS("%10s:\\ouc{%s} %zu", "Models", value_color, g_assets.model_count)->c, medium_font, GREEN);
        dwilo(TS("%10s:\\ouc{%s} %zu", "Textures", value_color, g_assets.texture_count)->c, medium_font, GREEN);
        dwilo(TS("%10s:\\ouc{%s} %zu", "Sounds", value_color, g_assets.sound_count)->c, medium_font, GREEN);
        dwilo(TS("%10s:\\ouc{%s} %zu", "Fonts", value_color, g_assets.font_count)->c, medium_font, GREEN);
        dwilo(TS("%10s:\\ouc{%s} %zu", "Shaders", value_color, g_assets.shader_count)->c, medium_font, GREEN);
        dwilo(TS("%10s:\\ouc{%s} %zu", "Skyboxes", value_color, g_assets.skybox_count)->c, medium_font, GREEN);
        dwilo(TS("%10s:\\ouc{%s} %zu", "Terrains", value_color, g_assets.terrain_count)->c, medium_font, GREEN);

        i_call_cbs(DBG_WID_ASSET);
    }

    // ===============================================================
    // ======================= Window: OPTIONS =======================
    // ===============================================================
    dwnd(DBG_WID_OPTIONS) {
        dwil("Debug", large_font, contrast_color);
        dwib("Sticky Windows", medium_font, c_debug__windows_sticky ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_debug__windows_sticky = !c_debug__windows_sticky; }, nullptr);
        dwib("Reset Windows", medium_font, DBG_WINDOW_FG_COLOR, DBG_REF_BUTTON_SIZE, i_reset_windows, nullptr);
        dwib("Collapse Windows", medium_font, DBG_WINDOW_FG_COLOR, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); for (auto &window : g_dbg.windows) { window.flags |= DBG_WFLAG_COLLAPSED; }}, nullptr);
        dwib("Uncollapse Windows", medium_font, DBG_WINDOW_FG_COLOR, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); for (auto &window : g_dbg.windows) { window.flags &= ~DBG_WFLAG_COLLAPSED; }}, nullptr);

        dwis(5.0F);
        dwil("Debug", large_font, contrast_color);
        dwib("Console", medium_font, c_console__enabled ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_console__enabled = !c_console__enabled; }, nullptr);
        dwib("Frame Info", medium_font, c_video__fps_info ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); option_set_game_show_frame_info(!c_video__fps_info); }, nullptr);
        dwib("Cursor Info", medium_font, c_debug__cursor_info ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_debug__cursor_info = !c_debug__cursor_info; }, nullptr);
        dwib("Keybindings", medium_font, c_debug__keybindings_info ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_debug__keybindings_info = !c_debug__keybindings_info; }, nullptr);
        dwib("Draw Menu Debug", medium_font, c_debug__menu_info ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_debug__menu_info = !c_debug__menu_info; }, nullptr);
        dwib("Draw BBox", medium_font, c_debug__bbox_info ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_debug__bbox_info = !c_debug__bbox_info; }, nullptr);
        dwib("Draw Terrain Info", medium_font, c_debug__terrain_info ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_debug__terrain_info = !c_debug__terrain_info; }, nullptr);
        dwib("Draw Grid Debug", medium_font, c_debug__grid_info ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_debug__grid_info = !c_debug__grid_info; }, nullptr);

        dwis(5.0F);
        dwil("DT Modifier", large_font, contrast_color);
        dwifb(TS("%.2f", dt_mod)->c, medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
              dt_mod, 0.05F, glm::max(10.0F, dt_mod), true, time_get_delta_mod_ptr(), DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);
        BOOL const is_paused = dt_mod == 0.0F;
        dwib(is_paused ? "PAUSED" : "RUNNING", medium_font, is_paused ? RED : GREEN, DBG_REF_BUTTON_SIZE, i_toggle_pause, nullptr);

        if (time_get_delta_mod() == 1.0F) {
            dwib("SLOW MOTION", medium_font, GOLD, DBG_REF_BUTTON_SIZE,
                 [](void *data) { unused(data); time_set_delta_mod(0.1F); }, nullptr);
        } else {
            dwib("RESET", medium_font, GOLD, DBG_REF_BUTTON_SIZE,
                 [](void *data) { unused(data); time_set_delta_mod(1.0F); }, nullptr);
        }

        dwis(2.0F);
        dwil("Video", large_font, contrast_color);

        // Resolution dropdown
        SZ static selected_resolution       = SZ_MAX;
        Vector2Array *available_resolutions = &g_options.available_resolutions;
        if (available_resolutions->count > 0) {
            C8 const **resolution_strings = mmta(C8 const **, sizeof(C8 *) * available_resolutions->count);
            for (SZ i = 0; i < available_resolutions->count; ++i) {
                Vector2 const res = array_get(available_resolutions, i);
                resolution_strings[i] = TS("%dx%d", (S32)res.x, (S32)res.y)->c;
            }

            // Find current resolution index
            if (selected_resolution == SZ_MAX) {
                Vector2 const curr_res = render_get_render_resolution();
                for (SZ i = 0; i < available_resolutions->count; ++i) {
                    Vector2 const res = array_get(available_resolutions, i);
                    if (Vector2Equals(curr_res, res)) {
                        selected_resolution = i;
                        break;
                    }
                }
                if (selected_resolution == SZ_MAX) { selected_resolution = 0; }
            }

            dwidd("Resolution", medium_font, DBG_WINDOW_FG_COLOR, GOLD, GREEN, DBG_REF_DROPDOWN_SIZE, resolution_strings,
                  available_resolutions->count, &selected_resolution, nullptr, nullptr);

            // Apply resolution change if dropdown selection changed
            if (selected_resolution < available_resolutions->count) {
                Vector2 const selected_res = array_get(available_resolutions, selected_resolution);
                Vector2 const current_res  = render_get_render_resolution();
                if (!Vector2Equals(selected_res, current_res)) { option_set_video_resolution(selected_res); }
            }
        }

        dwib(TS("%d", c_video__fps_max)->c, medium_font, DBG_WINDOW_FG_COLOR, DBG_REF_BUTTON_SIZE,
             [](void *data) {
                 unused(data);
                 // Cycle FPS limit through multiples of refresh rate (1x->2x->3x->4x->5x->uncapped)
                 S32 const monitor_hz      = GetMonitorRefreshRate(GetCurrentMonitor());
                 S32 const current_max_fps = c_video__fps_max;
                 S32 target_max_fps        = current_max_fps ? current_max_fps : monitor_hz;
                 if (current_max_fps % monitor_hz == 0 && current_max_fps / monitor_hz <= 5) {
                     target_max_fps = (current_max_fps / monitor_hz == 5) ? 0 : current_max_fps + monitor_hz;
                 }
                 option_set_video_max_fps(target_max_fps);
             }, nullptr);
        dwib("VSync", medium_font, c_video__vsync ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); option_set_video_vsync(!c_video__vsync); }, nullptr);

        dwis(5.0F);
        S32 const main_gamepad_idx       = input_get_main_gamepad_idx();
        BOOL const gamepad_connected     = main_gamepad_idx != -1;
        SZ const max_gamepad_name_length = 30;
        if (gamepad_connected) {
            String *gamepad_name = TS("%s", GetGamepadName(main_gamepad_idx));
            string_truncate(gamepad_name, max_gamepad_name_length, "...");
            dwil(TS("Gamepad %d connected", main_gamepad_idx)->c, medium_font, GREEN);
            dwil(gamepad_name->c, small_font, NAYBEIGE);
        } else {
            dwil("No Gamepad connected", medium_font, RED);
        }

        i_call_cbs(DBG_WID_OPTIONS);
    }

    // ===============================================================
    // ===================== Window: PERFORMANCE =====================
    // ===============================================================
    dwnd(DBG_WID_PERFORMANCE) {
        ProfilerTrack *main_loop = profiler_get_track(ML_NAME);

        dwil("General Info", large_font, contrast_color);
        dwil("currently in", medium_font, GRAY);
#ifdef OURO_TRACE
        dwil("TRACE", large_font, CYAN);
#elif OURO_DEBUG
        dwil("DEBUG", large_font, ORANGE);
#elif OURO_DEVEL
        dwil("DEVEL", large_font, YELLOW);
#else
        dwil("RELEASE", large_font, RED);
#endif
        dwis(3.0F);

        F32 static fluctuation_samples[DBG_FPS_FLUCTUATION_SAMPLE_COUNT] = {0};
        SZ static sample_count    = 0;
        SZ static sample_index    = 0;
        F32 const current_fps     = 1.0F / (F32)main_loop->delta_time;
        S32 const max_fps         = c_video__fps_max;
        BOOL const max_fps_capped = max_fps != 0;

        if (max_fps_capped) {
            F32 const fluctuation_pct = ((current_fps - (F32)max_fps) / (F32)max_fps) * 100.0F;
            fluctuation_samples[sample_index] = fluctuation_pct;
            sample_index = (sample_index + 1) % DBG_FPS_FLUCTUATION_SAMPLE_COUNT;
            sample_count++;
        }

        if (max_fps_capped && sample_count > DBG_FPS_FLUCTUATION_SAMPLE_COUNT) {
            F32 avg_fluctuation = 0.0F;
            for (F32 const fluctuation_sample : fluctuation_samples) { avg_fluctuation += fluctuation_sample; }
            avg_fluctuation /= (F32)DBG_FPS_FLUCTUATION_SAMPLE_COUNT;
            C8 const sign = (avg_fluctuation >= 0) ? '+' : '-';
            qil("FPS", TS("%.2f (%d target, %c%.4f%%)", current_fps, max_fps, sign, glm::abs(avg_fluctuation))->c);
        } else {
            qil("FPS", TS("%.2f", current_fps)->c);
        }

        qil("Time (regular/untouched/GLFW)", TS("%.2fs/%.2fs/%.2fs", time_get(), time_get_untouched(), time_get_glfw())->c);

        qil("DT (cur/avg/max)",
            TS("%.2fms/%.2fms/%.2fms", BASE_TO_MILLI(main_loop->delta_time), BASE_TO_MILLI(main_loop->avg_delta_time), BASE_TO_MILLI(main_loop->max_delta_time))->c);

        qil("Cycles (cur/avg/max)",
            TS("%.2fMc/%.2fMc/%.2fMc", (F32)(main_loop->delta_cycles) / MEGA(1), (F32)(main_loop->avg_delta_cycles) / MEGA(1), (F32)(main_loop->max_delta_cycles) / MEGA(1))->c);

        dwib("Reset Profiler", medium_font, DBG_WINDOW_FG_COLOR, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); profiler_reset(); }, nullptr);
        dwis(5.0F);

        dwicb("Show Tracks", medium_font, DBG_WINDOW_FG_COLOR, DBG_REF_CHECKBOX_SIZE, &c_debug__profiler_hide_trivial_tracks, nullptr, nullptr);
        dwicb("Hide Trivial Tracks", medium_font, DBG_WINDOW_FG_COLOR, DBG_REF_CHECKBOX_SIZE, &c_debug__profiler_hide_trivial_tracks, nullptr, nullptr);

        AFont *perf_table_font   = medium_font;
        SZ static selected_sort  = SZ_MAX;
        SZ const sort_type_count = 7;

        struct ISortFnEntry {
            S32 (*sort_fn)(void const *a, void const *b);
            C8 const *label;
            F32 trivial_threshold;  // in milliseconds
        } static const sort_fn_array[sort_type_count] = {
            {nullptr, "None", 0},

            {[](const void *a, const void *b) {
                ProfilerTrack const *track_a = *(ProfilerTrack const *const *)a;
                ProfilerTrack const *track_b = *(ProfilerTrack const *const *)b;
                if (track_a->delta_time > track_b->delta_time) { return -1; }
                if (track_a->delta_time < track_b->delta_time) { return 1; }
                return 0;
            }, "Current", 0.1F},

            {[](const void *a, const void *b) {
                ProfilerTrack const *track_a = *(ProfilerTrack const *const *)a;
                ProfilerTrack const *track_b = *(ProfilerTrack const *const *)b;
                if (track_a->avg_delta_time > track_b->avg_delta_time) { return -1; }
                if (track_a->avg_delta_time < track_b->avg_delta_time) { return 1; }
                return 0;
            }, "Average", 0.1F},

            {[](const void *a, const void *b) {
                ProfilerTrack const *track_a = *(ProfilerTrack const *const *)a;
                ProfilerTrack const *track_b = *(ProfilerTrack const *const *)b;
                if (track_a->max_delta_time > track_b->max_delta_time) { return -1; }
                if (track_a->max_delta_time < track_b->max_delta_time) { return 1; }
                return 0;
            }, "Maximum", 0.1F},

            {[](const void *a, const void *b) {
                ProfilerTrack const *track_a = *(ProfilerTrack const *const *)a;
                ProfilerTrack const *track_b = *(ProfilerTrack const *const *)b;
                if (track_a->sum_delta_time > track_b->sum_delta_time) { return -1; }
                if (track_a->sum_delta_time < track_b->sum_delta_time) { return 1; }
                return 0;
            }, "Sum", 1000.0F},

            {[](const void *a, const void *b) {
                ProfilerTrack const *track_a = *(ProfilerTrack const *const *)a;
                ProfilerTrack const *track_b = *(ProfilerTrack const *const *)b;
                return ou_strcmp(track_a->label, track_b->label);
            }, "Label", 0},

            {[](const void *a, const void *b) {
                ProfilerTrack const *track_a = *(const ProfilerTrack *const *)a;
                ProfilerTrack const *track_b = *(ProfilerTrack const *const *)b;
                if (track_a->depth < track_b->depth) { return -1; }
                if (track_a->depth > track_b->depth) { return 1; }
                return 0;
            }, "Depth", 0},
        };

        C8 const **sort_types = mmta(C8 const **, sizeof(C8 *) * sort_type_count);
        for (SZ i = 0; i < sort_type_count; ++i) { sort_types[i] = sort_fn_array[i].label; }
        if (selected_sort == DBG_WIDGET_DROPDOWN_ITEM_IDX_NONE) { selected_sort = 2; }

        dwidd("Sort by", medium_font, DBG_WINDOW_FG_COLOR, GOLD, GREEN, DBG_REF_DROPDOWN_SIZE, sort_types, sort_type_count, &selected_sort, nullptr, nullptr);

        BOOL const hide_trivial_tracks = c_debug__profiler_hide_trivial_tracks;

        if (c_debug__profiler_tracks) {
            ProfilerTrack *sorted_tracks[PROFILER_TRACK_MAX_COUNT];
            SZ sorted_track_count    = 0;
            S32 max_track_char_count = 0;  // Apparently this needs to be an integer for format specifiers.
            C8 const **label_k       = nullptr;
            ProfilerTrack *track_v   = nullptr;
            MAP_EACH_PTR(&g_profiler.track_map, label_k, track_v) {
                sorted_tracks[sorted_track_count] = track_v;
                String *temp = TS("%s", sorted_tracks[sorted_track_count]->label);
                string_delete_after_first(temp, '(');
                max_track_char_count = glm::max(max_track_char_count, (S32)ou_strlen(temp->c));
                sorted_track_count++;
            }

            ISortFnEntry const *sort_fn_entry = &sort_fn_array[selected_sort];
            if (sort_fn_entry->sort_fn) { qsort((void *)sorted_tracks, sorted_track_count, sizeof(ProfilerTrack *), sort_fn_entry->sort_fn); }

            dwis(5.0F);
            dwil("Frame Tracks", large_font, contrast_color);

            String *header_str = TS("%10s %10s %10s %10s %*s %6s", "CUR", "AVG", "MAX", "SUM", max_track_char_count, "LABEL", "DPT");
            dwil(header_str->c, perf_table_font, CYAN);

            SZ const current_frame_generation = g_profiler.current_generation;

            for (SZ i = 0; i < sorted_track_count; ++i) {
                ProfilerTrack *track = sorted_tracks[i];

                // Do not render tracks that had no activity for 100 generations.
                if (current_frame_generation - track->previous_generation > 100) { continue; }

                if (hide_trivial_tracks && sort_fn_entry->trivial_threshold > 0) {
                    F32 const threshold = (F32)MILLI_TO_BASE(sort_fn_entry->trivial_threshold);
                    F32 value = 0;

                    switch (selected_sort) {
                        case 1: {
                            value = (F32)track->delta_time;
                        } break;
                        case 2: {
                            value = (F32)track->avg_delta_time;
                        } break;
                        case 3: {
                            value = (F32)track->max_delta_time;
                        } break;
                        case 4: {
                            value = (F32)track->sum_delta_time;
                        } break;
                        default: {
                            _unimplemented_();
                        }
                    }

                    if (value < threshold) { continue; }
                }

                // Define color thresholds and ranges as constants
                F32 const COLOR_THRESHOLD_1 = 0.5F;
                F32 const COLOR_THRESHOLD_2 = 1.0F;
                F32 const COLOR_THRESHOLD_3 = 2.0F;
                F32 const COLOR_THRESHOLD_4 = 3.0F;
                F32 const MAX_COLOR_TIME    = 4.0F;

                Color color_a;
                Color color_b;

                SZ const depth     = track->depth;
                F32 const dt       = (F32)BASE_TO_MILLI(track->delta_time);
                F32 const clamp_dt = (dt > MAX_COLOR_TIME) ? MAX_COLOR_TIME : dt;
                F32 ratio          = 0.0F;

                if (clamp_dt > COLOR_THRESHOLD_4) {
                    color_a = RED;
                    color_b = MAROON;
                    ratio = (clamp_dt - COLOR_THRESHOLD_4) / (MAX_COLOR_TIME - COLOR_THRESHOLD_4);
                } else if (clamp_dt > COLOR_THRESHOLD_3) {
                    color_a = ORANGE;
                    color_b = RED;
                    ratio = (clamp_dt - COLOR_THRESHOLD_3) / (COLOR_THRESHOLD_4 - COLOR_THRESHOLD_3);
                } else if (clamp_dt > COLOR_THRESHOLD_2) {
                    color_a = YELLOW;
                    color_b = ORANGE;
                    ratio = (clamp_dt - COLOR_THRESHOLD_2) / (COLOR_THRESHOLD_3 - COLOR_THRESHOLD_2);
                } else if (clamp_dt > COLOR_THRESHOLD_1) {
                    color_a = GREEN;
                    color_b = YELLOW;
                    ratio = (clamp_dt - COLOR_THRESHOLD_1) / (COLOR_THRESHOLD_2 - COLOR_THRESHOLD_1);
                } else {
                    color_a = LIME;
                    color_b = GREEN;
                    ratio = clamp_dt / COLOR_THRESHOLD_1;
                }

                Color const final_color = color_lerp(color_a, color_b, ratio);

                C8 pretty_cur_dt[PRETTY_BUFFER_SIZE] = {};
                C8 pretty_avg_dt[PRETTY_BUFFER_SIZE] = {};
                C8 pretty_max_dt[PRETTY_BUFFER_SIZE] = {};
                C8 pretty_sum_dt[PRETTY_BUFFER_SIZE] = {};
                unit_to_pretty_time_f(BASE_TO_NANO(track->delta_time), pretty_cur_dt, PRETTY_BUFFER_SIZE, UNIT_TIME_HOURS);
                unit_to_pretty_time_f(BASE_TO_NANO(track->avg_delta_time), pretty_avg_dt, PRETTY_BUFFER_SIZE, UNIT_TIME_HOURS);
                unit_to_pretty_time_f(BASE_TO_NANO(track->max_delta_time), pretty_max_dt, PRETTY_BUFFER_SIZE, UNIT_TIME_HOURS);
                unit_to_pretty_time_f(BASE_TO_NANO(track->sum_delta_time), pretty_sum_dt, PRETTY_BUFFER_SIZE, UNIT_TIME_HOURS);

                String *temp = TS("%s", track->label);
                string_delete_after_first(temp, '(');
                String *frame_info_str = TS("%10s %10s %10s %10s %*s %6zu",
                                            pretty_cur_dt, pretty_avg_dt, pretty_max_dt, pretty_sum_dt,
                                            max_track_char_count, temp->c, depth);
                dwil(frame_info_str->c, perf_table_font, final_color);
            }
        } else {
            dwis(5.0F);
        }

        dwis(5.0F);
        dwil("Frame Timing", large_font, contrast_color);
        dwitl(LIME, NEARBLACK, time_get_frame_times(), PROFILER_FRAME_TIMES_TIMELINE_MAX_COUNT, 0.00F, (F32)BASE_TO_MILLI(1.0F / 30.0F), "%.2f%s", "ms");

        i_call_cbs(DBG_WID_PERFORMANCE);
    }

    // ===============================================================
    // ======================== Window: AUDIO ========================
    // ===============================================================
    dwnd(DBG_WID_AUDIO) {
        dwil("Current Tracks", large_font, contrast_color);
        qil("Music", audio_get_current_music_name());
        qil("Ambience", audio_get_current_ambience_name());

        dwis(5.0F);

        FMOD::System *fmod_system = audio_get_fmod_system();

        dwil("Memory Usage", large_font, contrast_color);
        S32 current_alloc  = 0;
        S32 max_alloc      = 0;
        FMOD_RESULT result = FMOD::Memory_GetStats(&current_alloc, &max_alloc, false);
        if (result != FMOD_OK) {
            dwil("Failed to get FMOD memory stats.", medium_font, RED);
        } else {
            C8 pretty_current_alloc[PRETTY_BUFFER_SIZE] = {};
            C8 pretty_max_alloc[PRETTY_BUFFER_SIZE] = {};

            unit_to_pretty_prefix_binary_u("B", (U64)current_alloc, pretty_current_alloc, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);
            unit_to_pretty_prefix_binary_u("B", (U64)max_alloc, pretty_max_alloc, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);

            qil("Current", pretty_current_alloc);
            qil("Max", pretty_max_alloc);
        }

        dwis(5.0F);
        dwil("CPU Usage", large_font, contrast_color);
        FMOD_CPU_USAGE usage = {};
        result = fmod_system->getCPUUsage(&usage);
        if (result != FMOD_OK) {
            dwil("Failed to get FMOD CPU usage.", medium_font, RED);

        } else {
            // DSP mixing engine CPU usage. Percentage of FMOD_THREAD_TYPE_MIXER, or main thread if FMOD_INIT_MIX_FROM_UPDATE flag is used with System::init.
            dwifb("DSP", medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
                  usage.dsp, 0.0F, 100.0F, false, nullptr, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);

            // Streaming engine CPU usage. Percentage of FMOD_THREAD_TYPE_STREAM, or main thread if FMOD_INIT_STREAM_FROM_UPDATE flag is used with System::init.
            dwifb("Stream", medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
                  usage.stream, 0.0F, 100.0F, false, nullptr, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);

            // Geometry engine CPU usage. Percentage of FMOD_THREAD_TYPE_GEOMETRY.
            dwifb("Geometry", medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
                  usage.geometry, 0.0F, 100.0F, false, nullptr, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);

            // System::update CPU usage. Percentage of main thread.
            dwifb("Update", medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
                  usage.update, 0.0F, 100.0F, false, nullptr, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);

            // Convolution reverb processing thread #1 CPU usage. Percentage of FMOD_THREAD_TYPE_CONVOLUTION1.
            dwifb("Convolution1", medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
                  usage.convolution1, 0.0F, 100.0F, false, nullptr, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);

            // Convolution reverb processing thread #2 CPU usage. Percentage of FMOD_THREAD_TYPE_CONVOLUTION2.
            dwifb("Convolution2", medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
                  usage.convolution2, 0.0F, 100.0F, false, nullptr, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);
        }

        dwis(5.0F);

        for (S32 i = 0; i < ACG_COUNT; ++i) {
            F32 *vol   = audio_get_volume_ptr((AudioChannelGroup)i);
            F32 *pitch = audio_get_pitch_ptr((AudioChannelGroup)i);
            F32 *pan   = audio_get_pan_ptr((AudioChannelGroup)i);

            dwil(TS("%s", audio_channel_group_to_cstr((AudioChannelGroup)(i)))->c, large_font, contrast_color);
            dwifb("Volume", medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
                  *vol, 0.0F, 1.0F, true, vol, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);
            dwifb("Pitch", medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
                  *pitch, 0.0F, 2.0F, true, pitch, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);
            dwifb("Pan", medium_font, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE,
                  *pan, 0.0F, 1.0F, true, pan, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);
            dwis(10.0F);
        }

        // NOTE: Any volume changes here are not saved in the options file, that's why we are adding an explicit save button.
        dwib("Save", large_font, GOLD, DBG_REF_BUTTON_SIZE,
             [](void *data){ unused(data); cvar_save(); }, nullptr);

        i_call_cbs(DBG_WID_AUDIO);
    }

    // ===============================================================
    // ======================= Window: TARGETS =======================
    // ===============================================================
    dwnd(DBG_WID_TARGETS) {
        BOOL const dark_mode_enabled = c_debug__texture_widget_dark_bg;
        dwib(TS("Dark Mode: %s", dark_mode_enabled ? "ON" : "OFF")->c, medium_font, dark_mode_enabled ? GREEN : RED, DBG_REF_BUTTON_SIZE,
             [](void *data) { unused(data); c_debug__texture_widget_dark_bg = !c_debug__texture_widget_dark_bg; }, nullptr);

        Vector2 const res             = render_get_render_resolution();
        SZ const current_frame_gen    = g_profiler.current_generation;
        auto inactive_color           = MAROON;
        Vector2 const ref_button_size = DBG_REF_BUTTON_SIZE;
        F32 const perc                = ref_button_size.x / res.x;

        for (SZ i = 0; i < RMODE_LAST_LAYER; ++i) {  // Skip the final render mode.
            RenderTexture *target      = &g_render.rmode_data[i].target;
            SZ const gen               = g_render.rmode_data[i].generation;
            Vector2 const texture_size = {res.x * perc, res.y * perc};
            Color highlight_color      = g_render.rmode_data[i].tint_color;
            BOOL const is_current_gen  = current_frame_gen - 1 == gen;  // -1 because we increment the generation after rendering.
            highlight_color            = is_current_gen ? highlight_color : Fade(inactive_color, 0.33F);
            Color const texture_tint   = is_current_gen ? WHITE : Fade(inactive_color, 0.33F);
            dwitx(TS("%s\nGEN: %zu\nRES:%dx%d", render_mode_to_cstr((RenderMode)i), gen, target->texture.width, target->texture.height)->c,
                  target, texture_size, highlight_color, texture_tint);
            if (i < RMODE_COUNT - 1) { dwis(10.0F); }
        }

        Vector2 const texture_size = {res.x * perc, res.y * perc};
        RenderTexture *target = &g_render.final_render_target;
        dwitx(TS("FINAL\nRES:%dx%d", target->texture.width, target->texture.height)->c, target, texture_size, WHITE, WHITE);

        i_call_cbs(DBG_WID_TARGETS);
    }

    // ===============================================================
    // ======================= Window: CUSTOM ========================
    // ===============================================================
    dwnd(DBG_WID_CUSTOM_0) {
        i_call_cbs(DBG_WID_CUSTOM_0);
    }

    dwnd(DBG_WID_CUSTOM_1) {
        i_call_cbs(DBG_WID_CUSTOM_1);
    }
}

String static *i_get_window_extra_title_info(SZ wid) {
    switch (wid) {
        case DBG_WID_RENDER: {
            // Return the render size.
            return TS("%s (%dx%d)", i_dbg_window_name_strings[wid], c_video__render_resolution_width, c_video__render_resolution_height);
        }

        case DBG_WID_MEMORY: {
            // Return the total allocation count.
            SZ total_alloc_count = 0;
            ArenaStats const perm_stats  = memory_get_previous_arena_stats(MEMORY_TYPE_ARENA_PERMANENT);
            ArenaStats const temp_stats  = memory_get_previous_arena_stats(MEMORY_TYPE_ARENA_TRANSIENT);
            ArenaStats const debug_stats = memory_get_previous_arena_stats(MEMORY_TYPE_ARENA_DEBUG);
            total_alloc_count += perm_stats.total_allocation_count;
            total_alloc_count += temp_stats.total_allocation_count;
            total_alloc_count += debug_stats.total_allocation_count;
            return TS("%s (%zu allocs)", i_dbg_window_name_strings[wid], total_alloc_count);
        }

        case DBG_WID_SCENE: {
            // Return the current scene type (or overlay scene type if it exists)
            SceneType const scene_type = g_scenes.current_overlay_scene_type != SCENE_NONE ? g_scenes.current_overlay_scene_type : g_scenes.current_scene_type;
            return TS("%s (%s)", i_dbg_window_name_strings[wid], scenes_to_cstr(scene_type));
        }

        case DBG_WID_WORLD: {
            // Return the current recording state.
            BOOL const is_recording = world_recorder_is_recording_state();
            BOOL const is_looping   = world_recorder_is_looping_state();
            return TS("%s (%s) (%u)", i_dbg_window_name_strings[wid], is_recording ? "WREC" : (is_looping ? "PLAY" : "STOP"), g_world->active_ent_count);
        }

        case DBG_WID_ASSET: {
            // Return if the update thread is running.
            return TS("%s (%s)", i_dbg_window_name_strings[wid], g_assets.run_update_thread ? "Running" : "Stopped");
        }

        case DBG_WID_OPTIONS: {
            // Return the delta time modifier.
            F32 const dt_mod = time_get_delta_mod();
            return TS("%s (%.2fx)", i_dbg_window_name_strings[wid], dt_mod);
        }

        case DBG_WID_PERFORMANCE: {
            // Return the current frame time.
            return TS("%s (%.2fms)", i_dbg_window_name_strings[wid], BASE_TO_MILLI(profiler_get_track(ML_NAME)->delta_time));
        }

        case DBG_WID_AUDIO: {
            // Return the current song playing.
            return TS("%s (%s)", i_dbg_window_name_strings[wid], audio_get_current_music_name());
        }

        case DBG_WID_TARGETS: {
            // Return the current frame generation.
            return TS("%s (%zu)", i_dbg_window_name_strings[wid], g_profiler.current_generation);
        }

        case DBG_WID_CUSTOM_0:  // Fallthrough.
        case DBG_WID_CUSTOM_1:  // Fallthrough.
        default: {
            return TS("%s", i_dbg_window_name_strings[wid]);
        }
    }
}

void static i_update_widgets() {
    g_dbg.small_font  = asset_get_font(c_debug__small_font.cstr, c_debug__small_font_size);
    g_dbg.medium_font = asset_get_font(c_debug__medium_font.cstr, c_debug__medium_font_size);
    g_dbg.large_font  = asset_get_font(c_debug__large_font.cstr, c_debug__large_font_size);

    // Reset window widgets.
    for (auto &window : g_dbg.windows) { window.widget_count = 0; }

    i_setup_dbg_gui();

    DBGWidgetUpdatePackage p = {};
    p.mouse_pos   = input_get_mouse_position();
    p.mouse_delta = input_get_mouse_delta();

    for (SZ j = 0; j < I_MOUSE_COUNT; ++j) {
        p.is_mouse_down[j]     = is_mouse_down((IMouse)j);
        p.is_mouse_pressed[j]  = is_mouse_pressed((IMouse)j);
        p.is_mouse_released[j] = is_mouse_released((IMouse)j);
    }

    BOOL mouse_click_used = false;
    AFont *title_font = g_dbg.large_font;

    // First pass: Update widget positions and window sizes
    for (SZ i = 0; i < DBG_WID_COUNT; ++i) {
        DBGWindow *window = &g_dbg.windows[i];

        window->bounds.width  = DBG_WINDOW_MIN_WIDTH;
        window->bounds.height = 0.0F;
        p.window_rec          = &window->bounds;

        // Widgets.
        if (!(window->flags & DBG_WFLAG_COLLAPSED)) {
            for (SZ j = 0; j < window->widget_count; ++j) {
                p.widget = &window->widgets[j];
                p.widget->update_func(&p);
            }
        }

        // Window.
        Vector2 const text_dimensions = measure_text(title_font, i_get_window_extra_title_info(i)->c);
        F32 const min_x               = text_dimensions.x + (DBG_WINDOW_INNER_PADDING * 2);
        F32 const min_y               = DBG_WINDOW_TITLE_BAR_HEIGHT + (DBG_WINDOW_INNER_PADDING * 2);
        window->bounds.width = glm::max(window->bounds.width, min_x);
        if (window->flags & DBG_WFLAG_COLLAPSED) { window->bounds.width = DBG_WINDOW_MIN_WIDTH; }
        window->bounds.height = glm::max(window->bounds.height + DBG_WINDOW_TITLE_BAR_HEIGHT + (DBG_WINDOW_INNER_PADDING * 2), min_y);
    }

    // Second pass: Handle input in reverse z-order (front to back)

    // NOTE: We only do the second pass if there is nothing in big view
    if (g_dbg.modal_overlay_active) { return; }

    BOOL static sticky_enabled_reported = false;

    for (S32 order_idx = DBG_WID_COUNT - 1; order_idx >= 0; --order_idx) {
        S32 const i       = g_dbg.window_order[order_idx];
        DBGWindow *window = &g_dbg.windows[i];

        // Window titlebar bounds.
        Rectangle const titlebar_bounds = {window->bounds.x, window->bounds.y, window->bounds.width, DBG_WINDOW_TITLE_BAR_HEIGHT};
        BOOL const mouse_over_titlebar  = CheckCollisionPointRec(p.mouse_pos, titlebar_bounds);
        BOOL const mouse_over_window    = CheckCollisionPointRec(p.mouse_pos, window->bounds);

        if (mouse_over_window && p.is_mouse_pressed[I_MOUSE_LEFT] && !mouse_click_used) {
            g_dbg.last_clicked_wid = i;
            mouse_click_used = true;  // Important: mark click as used
        }

        if (mouse_over_titlebar && p.is_mouse_released[I_MOUSE_RIGHT] && !mouse_click_used) {
            g_dbg.last_clicked_wid = i;
            window->flags ^= DBG_WFLAG_COLLAPSED;
            mouse_click_used = true;
        }

        if ((mouse_over_titlebar && p.is_mouse_down[I_MOUSE_LEFT] && !mouse_click_used) || (g_dbg.dragging_wid == i && p.is_mouse_down[I_MOUSE_LEFT])) {
            if (g_dbg.dragging_wid != DBG_NO_WID && g_dbg.dragging_wid != i) { continue; }

            if (c_debug__windows_sticky) {
                if (!sticky_enabled_reported) {
                    mw("Sticky windows are enabled, dragging is disabled.", ORANGE);
                    sticky_enabled_reported = true;
                }
            } else {
                window->bounds.x += p.mouse_delta.x;
                window->bounds.y += p.mouse_delta.y;
                g_dbg.dragging_wid = i;
                mouse_click_used = true;

                // Update widget positions
                for (SZ j = 0; j < window->widget_count; ++j) {
                    DBGWidget *widget = &window->widgets[j];
                    widget->bounds.x += p.mouse_delta.x;
                    widget->bounds.y += p.mouse_delta.y;
                }
            }
        }
    }

    if (p.is_mouse_released[I_MOUSE_LEFT]) {
        g_dbg.dragging_wid      = DBG_NO_WID;
        sticky_enabled_reported = false;
    }

    // Reorder windows based on the last clicked window.
    if (g_dbg.last_clicked_wid != DBG_NO_WID) {
        for (SZ i = 0; i < DBG_WID_COUNT; ++i) {
            if (g_dbg.window_order[i] == g_dbg.last_clicked_wid) {
                for (SZ j = i; j < DBG_WID_COUNT - 1; ++j) { g_dbg.window_order[j] = g_dbg.window_order[j + 1]; }
                g_dbg.window_order[DBG_WID_COUNT - 1] = (DBGWID)g_dbg.last_clicked_wid;
                break;
            }
        }
    }

    g_dbg.last_widget_count = 0;
    for (const auto &window : g_dbg.windows) { g_dbg.last_widget_count += window.widget_count; }
}

void static i_draw_cursor_info() {
    AFont *font                    = g_dbg.medium_font;
    Vector2 const render_mouse_pos = input_get_mouse_position();
    Vector2 const screen_mouse_pos = input_get_mouse_position_screen();

    String *mouse_pos_text =
        TS("%*s: %.0f,%.0f\n%*s: %.0f,%.0f", 6, "REL", render_mouse_pos.x, render_mouse_pos.y, 6, "ABS", screen_mouse_pos.x, screen_mouse_pos.y);

    // Draw background rectangle in white alpha half-ish visible.
    Vector2 const text_dimensions = measure_text(font, mouse_pos_text->c);
    F32 const padding = 1.0F;
    Rectangle const text_bounds = {
        render_mouse_pos.x - padding,
        render_mouse_pos.y - padding,
        text_dimensions.x + (padding * 2.0F),
        text_dimensions.y + (padding * 2.0F),
    };
    d2d_rectangle_rec(text_bounds, Fade(BLACK, 0.65F));
    d2d_text(font, mouse_pos_text->c, render_mouse_pos, YELLOW);
}

void static i_draw_widgets() {
    for (auto wid : g_dbg.window_order) {
        DBGWindow *window      = &g_dbg.windows[wid];
        Color const color      = i_dbg_window_highlight_colors[wid];
        BOOL const is_dragging = g_dbg.dragging_wid == wid;

        if (!(window->flags & DBG_WFLAG_COLLAPSED)) {
            // Outer black border but always 1 pixel larger than the window including the border.
            d2d_rectangle_rec(
                {
                    window->bounds.x - DBG_WINDOW_BORDER_SIZE - 1,
                    window->bounds.y - DBG_WINDOW_BORDER_SIZE - 1,
                    window->bounds.width + (DBG_WINDOW_BORDER_SIZE * 2) + 2,
                    window->bounds.height + (DBG_WINDOW_BORDER_SIZE * 2) + 2,
                },
                BLACK);

            // Window border.
            d2d_rectangle_rec(
                {
                    window->bounds.x - DBG_WINDOW_BORDER_SIZE,
                    window->bounds.y - DBG_WINDOW_BORDER_SIZE,
                    window->bounds.width + (DBG_WINDOW_BORDER_SIZE * 2),
                    window->bounds.height + (DBG_WINDOW_BORDER_SIZE * 2),
                },
                is_dragging ? RED : color);

            // Window.
            d2d_rectangle_rec(
                {
                    window->bounds.x,
                    window->bounds.y,
                    window->bounds.width,
                    window->bounds.height,
                },
                DBG_WINDOW_BG_COLOR);
        } else {
            // Collapsed window with border like above.
            d2d_rectangle_rec(
                {
                    window->bounds.x - DBG_WINDOW_BORDER_SIZE - 1,
                    window->bounds.y - DBG_WINDOW_BORDER_SIZE - 1,
                    window->bounds.width + (DBG_WINDOW_BORDER_SIZE * 2) + 2,
                    DBG_WINDOW_TITLE_BAR_HEIGHT + (DBG_WINDOW_BORDER_SIZE * 2) + 2,
                },
                BLACK);

            // Collapsed window border.
            d2d_rectangle_rec(
                {
                    window->bounds.x - DBG_WINDOW_BORDER_SIZE,
                    window->bounds.y - DBG_WINDOW_BORDER_SIZE,
                    window->bounds.width + (DBG_WINDOW_BORDER_SIZE * 2),
                    DBG_WINDOW_TITLE_BAR_HEIGHT + (DBG_WINDOW_BORDER_SIZE * 2),
                },
                color);
        }

        // Window titlebar.
        d2d_rectangle_rec({window->bounds.x, window->bounds.y, window->bounds.width, DBG_WINDOW_TITLE_BAR_HEIGHT}, color);

        // Window title text.
        AFont *title_font = g_dbg.medium_font;

        Vector2 const text_position = {
            window->bounds.x + DBG_WINDOW_INNER_PADDING,
            window->bounds.y + ((DBG_WINDOW_TITLE_BAR_HEIGHT - (F32)title_font->font_size) / 2.0F),
        };

        Vector2 const shadow_offset = {(F32)(title_font->font_size) * 0.075F, (F32)(title_font->font_size) * 0.075F};
        d2d_text_shadow(title_font, i_get_window_extra_title_info(wid)->c, text_position, WHITE, BLACK, shadow_offset);

        // Widgets.
        // Some widgets need to be drawn afterwards so we other elements don't overlap them.
        SZ dropdown_count_to_draw_after_count = 0;
        BOOL const dropdown_exists            = g_dbg.current_dropdown_idx > 0;
        DBGWidget **dropdown_to_draw_after    = nullptr;
        if (dropdown_exists) { dropdown_to_draw_after = mmta(DBGWidget **, sizeof(DBGWidget) * g_dbg.current_dropdown_idx); }

        DBGWidgetDrawPackage p = {};

        if (!(window->flags & DBG_WFLAG_COLLAPSED)) {
            for (SZ j = 0; j < window->widget_count; ++j) {
                p.widget = &window->widgets[j];

                if (p.widget->type != DBG_WIDGET_TYPE_DROPDOWN) {
                    p.widget->draw_func(&p);
                } else {
                    if (dropdown_exists) { dropdown_to_draw_after[dropdown_count_to_draw_after_count++] = p.widget; }
                }
            }
        }

        for (SZ i = 0; i < dropdown_count_to_draw_after_count; ++i) {
            p.widget = dropdown_to_draw_after[i];
            p.widget->draw_func(&p);
        }
    }
}

void static i_prepare_color_view_selection_texture(Color input_color) {
    // Get HSV values from the input color
    ColorHSV hsv;
    color_to_hsv(input_color, &hsv);

    // Define component dimensions
    S32 const gradient_width  = 200;
    S32 const gradient_height = 180;
    S32 const slider_height   = 40;
    S32 const preview_size    = 40;
    S32 const padding         = 8;

    // Calculate total dimensions required
    S32 const texture_width  = padding + gradient_width + padding + preview_size + padding;
    S32 const texture_height = padding + gradient_height + padding + slider_height + padding;

    // Calculate component positions
    S32 const gradient_x   = padding;
    S32 const gradient_y   = padding;
    S32 const hue_slider_x = padding;
    S32 const hue_slider_y = gradient_y + gradient_height + padding;
    S32 const preview_x    = gradient_x + gradient_width + padding;
    S32 const preview_y    = gradient_y;

    // Create an image with exact dimensions needed
    g_dbg.color_view_selection_image = GenImageColor(texture_width, texture_height, BLANK);

    // Draw rectangular SV gradient (main gradient with current hue)
    for (S32 y = 0; y < gradient_height; ++y) {
        for (S32 x = 0; x < gradient_width; ++x) {
            // Calculate saturation and value from position
            F32 const saturation = (F32)x / (F32)(gradient_width - 1);
            F32 const value      = 1.0F - ((F32)y / (F32)(gradient_height - 1));  // Top is maximum value

            // Use current hue with position-derived saturation and value
            Color const color = ColorFromHSV(hsv.h, saturation, value);

            // Draw pixel on the image
            S32 const px = gradient_x + x;
            S32 const py = gradient_y + y;
            ImageDrawPixel(&g_dbg.color_view_selection_image, px, py, color);
        }
    }

    // Draw hue slider (horizontal rainbow)
    for (S32 y = 0; y < slider_height; ++y) {
        for (S32 x = 0; x < gradient_width; ++x) {
            // Calculate hue from x position (0-360)
            F32 const hue = ((F32)x / (F32)(gradient_width - 1)) * 360.0F;

            // Full saturation and value for the slider
            Color const color = ColorFromHSV(hue, 1.0F, 1.0F);

            // Draw pixel on the image
            S32 const px = hue_slider_x + x;
            S32 const py = hue_slider_y + y;
            ImageDrawPixel(&g_dbg.color_view_selection_image, px, py, color);
        }
    }

    // Draw color preview box
    for (S32 y = 0; y < preview_size; ++y) {
        for (S32 x = 0; x < preview_size; ++x) {
            S32 const px = preview_x + x;
            S32 const py = preview_y + y;
            ImageDrawPixel(&g_dbg.color_view_selection_image, px, py, input_color);
        }
    }

    // Draw complementary color preview below the main preview
    Color const complementary = ColorFromHSV(math_mod_f32(hsv.h + 180.0F, 360.0F), hsv.s, hsv.v);
    for (S32 y = 0; y < preview_size; ++y) {
        for (S32 x = 0; x < preview_size; ++x) {
            S32 const px = preview_x + x;
            S32 const py = preview_y + preview_size + padding + y;
            ImageDrawPixel(&g_dbg.color_view_selection_image, px, py, complementary);
        }
    }

    // First, create a clean texture for color sampling
    if (g_dbg.color_view_clean_texture.id != 0) { UnloadTexture(g_dbg.color_view_clean_texture); }
    g_dbg.color_view_clean_texture = LoadTextureFromImage(g_dbg.color_view_selection_image);

    // Now create a display copy and draw indicators on it
    Image display_image = ImageCopy(g_dbg.color_view_selection_image);

    // Calculate positions for markers
    S32 const sv_x  = gradient_x + (S32)(hsv.s * (F32)(gradient_width - 1));
    S32 const sv_y  = gradient_y + (S32)((1.0F - hsv.v) * (F32)(gradient_height - 1));
    S32 const hue_x = hue_slider_x + (S32)((hsv.h / 360.0F) * (F32)(gradient_width - 1));

    // Get the exact color at the SV selector position for contrast calculation
    Color const sv_bg_color     = ColorFromHSV(hsv.h, hsv.s, hsv.v);
    Color const sv_marker_color = color_contrast(sv_bg_color);

    // Draw SV selector circle
    for (S32 y = sv_y - 5; y <= sv_y + 5; y++) {
        for (S32 x = sv_x - 5; x <= sv_x + 5; x++) {
            F32 const dist_sq = (F32)(((x - sv_x) * (x - sv_x)) + ((y - sv_y) * (y - sv_y)));
            if (dist_sq <= 25.0F && dist_sq > 16.0F) { ImageDrawPixel(&display_image, x, y, sv_marker_color); }
        }
    }

    // Get the color at the hue position for contrast calculation
    Color const hue_bg_color     = ColorFromHSV(hsv.h, 1.0F, 1.0F);
    Color const hue_marker_color = color_contrast(hue_bg_color);

    // Draw hue selector (vertical line)
    for (S32 y = 0; y < slider_height; y++) {
        // Draw main vertical line
        ImageDrawPixel(&display_image, hue_x, hue_slider_y + y, hue_marker_color);

        // Add small horizontal caps at top and bottom for better visibility
        if (y == 0 || y == slider_height - 1) {
            for (S32 x = -2; x <= 2; x++) {
                if (x != 0) {  // Skip center pixel as it's already drawn
                    ImageDrawPixel(&display_image, hue_x + x, hue_slider_y + y, hue_marker_color);
                }
            }
        }
    }

    // Load the display texture (with indicators)
    if (g_dbg.color_view_selection_texture.id != 0) { UnloadTexture(g_dbg.color_view_selection_texture); }
    g_dbg.color_view_selection_texture = LoadTextureFromImage(display_image);

    // Clean up the temporary display image
    UnloadImage(display_image);
}

void dbg_init() {
    console_init();

    g_dbg.small_font  = asset_get_font(c_debug__small_font.cstr, c_debug__small_font_size);
    g_dbg.medium_font = asset_get_font(c_debug__medium_font.cstr, c_debug__medium_font_size);
    g_dbg.large_font  = asset_get_font(c_debug__large_font.cstr, c_debug__large_font_size);

    g_dbg.dragging_wid        = DBG_NO_WID;
    g_dbg.last_clicked_wid    = DBG_NO_WID;
    g_dbg.active_dropdown_idx = DBG_WIDGET_DROPDOWN_IDX_NONE;

    i_prepare_color_view_selection_texture(BLACK);

    for (S32 i = 0; i < DBG_WID_COUNT; ++i) { g_dbg.window_order[i] = (DBGWID)i; }

    dbg_load_ini();

    g_dbg.initialized = true;
}

void dbg_quit() {
    dbg_save_ini();
}

void dbg_update() {
    PBEGIN("BODY_DBG_UPDATE_PART_1");

    // We need to reset this every frame.
    g_dbg.modal_overlay_active = false;

    // Some quick keybindings for debugging/development
    if (is_pressed_or_repeat(IA_DBG_CYCLE_VIDEO_RESOLUTION)) {
        Vector2 const next_res = option_get_next_video_resolution();
        mio(TS("Setting resolution to: \\ouc{#00ff00ff}%.0fx%.0f", next_res.x, next_res.y)->c, WHITE);
        option_set_video_resolution(next_res);
    }

    if (is_pressed(IA_DBG_TOGGLE_DBG)) {
        // Toggle debug state
        c_debug__enabled = !c_debug__enabled;

        // If debug is now disabled, ensure console is disabled too
        if (!c_debug__enabled) { c_console__enabled = false; }
    }

    if (is_pressed(IA_CONSOLE_TOGGLE) || (c_console__enabled && is_pressed(IA_OPEN_OVERLAY_MENU))) {
        // Only allow toggling console if debug is enabled
        if (!c_debug__enabled) {
            // If debug is disabled and console toggle is pressed, enable debug first
            c_debug__enabled   = true;
            c_console__enabled = true;
        } else {
            // If debug is enabled, just toggle console normally
            c_console__enabled = !c_console__enabled;
        }
    }

    if (is_pressed(IA_DBG_RESET_WINDOWS)) { i_reset_windows(nullptr); }

    if (is_pressed(IA_DBG_TOGGLE_PAUSE_TIME)) {
        i_toggle_pause(nullptr);
        mio(TS("Time %s", time_is_paused() ? "\\ouc{#ff0000ff}paused" : "\\ouc{#00ff00ff}unpaused")->c, WHITE);
    }

    F32 const mouse_wheel_movement = input_get_mouse_wheel();

    if (is_mod(I_MODIFIER_SHIFT)) {
        S32 static power_step = 0;  // 0 means 2^0 = 1.0x (normal speed)

        if (mouse_wheel_movement != 0.0F) {
            S32 const new_step   = power_step + (S32)mouse_wheel_movement;
            F32 const new_dt_mod = glm::pow(2.0F, (F32)new_step);

            if (new_step >= -8 && new_step <= 8) {
                power_step = new_step;
                time_set_delta_mod(new_dt_mod);

                // Generate speed description
                if (power_step == 0) {
                    miof(WHITE, "Time speed: \\ouc{#00ff00ff}NORMAL \\ouc{#aaaaaaff}(step %+d)", power_step);
                } else if (power_step > 0) {
                    miof(WHITE, "Time speed: \\ouc{#00ff00ff}%.0fx FASTER \\ouc{#aaaaaaff}(step %+d)", new_dt_mod, power_step);
                } else {
                    miof(WHITE, "Time speed: \\ouc{#00ff00ff}%.0fx SLOWER \\ouc{#aaaaaaff}(step %+d)", 1.0F / new_dt_mod, power_step);
                }
            } else {
                // At limit case
                F32 const current_dt_mod = glm::pow(2.0F, (F32)power_step);
                if (power_step > 0) {
                    miof(WHITE, "Time speed: \\ouc{#00ff00ff}%.0fx FASTER \\ouc{#aaaaaaff}(step %+d) \\ouc{#ff0000ff}[LIMIT]", current_dt_mod, power_step);
                } else {
                    miof(WHITE, "Time speed: \\ouc{#00ff00ff}%.0fx SLOWER \\ouc{#aaaaaaff}(step %+d) \\ouc{#ff0000ff}[LIMIT]", 1.0F / current_dt_mod, power_step);
                }
            }
        }

        if (is_mouse_pressed(I_MOUSE_MIDDLE)) {
            power_step = 0;
            time_set_delta_mod(1.0F);
            miof(WHITE, "Time speed: \\ouc{#00ff00ff}NORMAL \\ouc{#aaaaaaff}(step +0) \\ouc{#ffff00ff}[RESET]");
        }
    }

    // We need to reset the dropdown idx and the active idx after every frame before we update the widgets.
    g_dbg.current_dropdown_idx = 0;
    g_dbg.active_dropdown_idx  = DBG_WIDGET_DROPDOWN_IDX_NONE;

    PEND("BODY_DBG_UPDATE_PART_1");

    PP(console_update());
    if (c_debug__enabled) { PP(i_update_widgets()); }

    PBEGIN("BODY_DBG_UPDATE_PART_2");

    ArenaStats const stats = memory_get_previous_arena_stats(MEMORY_TYPE_ARENA_DEBUG);
    if (stats.total_allocation_count > DBG_ARENA_ALLOCATIONS_TILL_RESET) { memory_reset_type(MEMORY_TYPE_ARENA_DEBUG); }

    if (c_debug__windows_sticky) { dbg_reset_windows(false); }

    PEND("BODY_DBG_UPDATE_PART_2");
}

void static i_draw_paused_banner() {
    Vector2 const res      = render_get_render_resolution();;
    AFont *const font      = asset_get_font("GoMono", ui_font_size(3));
    C8 const *text         = "TIME PAUSED";
    F32 const padding      = ui_scale_x(0.5F);
    Vector2 const text_dim = measure_text(font, text);
    Rectangle const rec = {
        (res.x / 2.0F) - (text_dim.x / 2.0F) - padding,
        padding,
        text_dim.x + (padding * 2.0F),
        text_dim.y + (padding * 2.0F),
    };
    d2d_rectangle_rounded_rec(rec, 0.15F, 12, Fade(color_sync_blinking_fast(MAROON, RED), 0.66F));
    d2d_text_shadow(font, text, {rec.x + padding, rec.y + padding}, WHITE, BLACK, ui_shadow_offset(0.075F, 0.1F));
}

void dbg_draw() {
    if (time_is_paused()) { i_draw_paused_banner(); }

    if (!c_debug__enabled) { return; }

    i_draw_widgets();
    if (c_console__enabled) { console_draw(); }
    if (c_debug__cursor_info) { i_draw_cursor_info(); }
}

void dbg_add_cb(DBGWID wid, DBGWidgetCallback cb) {
    DBGWindow *window = &g_dbg.windows[wid];
    if (window->cb >= DBG_CALLBACK_PER_WINDOW_MAX) {
        lle("Window callback count exceeded maximum of %d for window %d", DBG_CALLBACK_PER_WINDOW_MAX, wid);
        return;
    }

    window->cbs[window->cb].func = cb.func;
    window->cbs[window->cb].data = cb.data;
    window->cb++;
}

void dbg_remove_cb(DBGWID wid, DBGWidgetCallback cb) {
    DBGWindow *window = &g_dbg.windows[wid];

    for (SZ i = 0; i < window->cb; ++i) {
        if (window->cbs[i].func == cb.func && window->cbs[i].data == cb.data) {
            for (SZ j = i; j < window->cb - 1; ++j) { window->cbs[j] = window->cbs[j + 1]; }
            window->cb--;
            break;
        }
    }
}

void dbg_set_active_window(DBGWID wid) {
    g_dbg.active_wid = wid;
}

DBGWidget static *i_get_next_free_widget() {
    DBGWindow *window = &g_dbg.windows[g_dbg.active_wid];
    if (window->widget_count >= DBG_WIDGET_MAX) {
        lle("Window widget count exceeded maximum");
        return nullptr;
    }
    return window->widgets + window->widget_count++;
}

void static i_parse_bounds(Rectangle *window_rec, Rectangle *widget_rec, BOOL ignore_width) {
    F32 const title_bar_height = DBG_WINDOW_TITLE_BAR_HEIGHT + DBG_WINDOW_INNER_PADDING;
    F32 const total_padding    = DBG_WINDOW_INNER_PADDING * 2;

    widget_rec->x = window_rec->x + DBG_WINDOW_INNER_PADDING;
    widget_rec->y = window_rec->y + window_rec->height + title_bar_height;

    if (!ignore_width) {
        F32 const required_width = widget_rec->width + total_padding;
        F32 const min_width = (F32)DBG_WINDOW_MIN_WIDTH;
        window_rec->width = glm::max(window_rec->width, glm::max(required_width, min_width));
    }
    window_rec->height += widget_rec->height + DBG_WIDGET_PADDING;
}

BOOL static i_is_window_being_dragged() {
    return g_dbg.dragging_wid != DBG_NO_WID;
}

BOOL static i_is_dropdown_open() {
    return g_dbg.active_dropdown_idx != DBG_WIDGET_DROPDOWN_IDX_NONE;
}

BOOL static i_is_ui_interaction_in_progress() {
    return i_is_window_being_dragged() || i_is_dropdown_open();
}

BOOL static i_is_widget_interaction_blocked() {
    return i_is_ui_interaction_in_progress() || g_dbg.modal_overlay_active;
}

#define DBG_SHADOW_COLOR BLACK
#define DBG_SHADOW_OFFSET(font) (Vector2){(F32)((font)->font_size) * 0.066F, (F32)((font)->font_size) * 0.066F}

// ===============================================================
// ====================== Widget: SEPARATOR ======================
// ===============================================================
void dbg_widget_separator_add(F32 height) {
    DBGWidget *w        = i_get_next_free_widget();
    w->type             = DBG_WIDGET_TYPE_SEPARATOR;
    w->separator.height = height;
    w->update_func      = dbg_widget_separator_update;
    w->draw_func        = dbg_widget_separator_draw;
}

void dbg_widget_separator_update(DBGWidgetUpdatePackage *p) {
    p->widget->bounds.height = p->widget->separator.height;
    i_parse_bounds(p->window_rec, &p->widget->bounds, false);
}

void dbg_widget_separator_draw(DBGWidgetDrawPackage *p) {}

// ===============================================================
// ======================== Widget: LABEL ========================
// ===============================================================
void dbg_widget_label_add(C8 const *text, AFont *font, Color text_color, BOOL is_ouc, BOOL has_shadow) {
    DBGWidget *w        = i_get_next_free_widget();
    w->type             = DBG_WIDGET_TYPE_LABEL;
    w->label.text       = text;
    w->label.font       = font;
    w->label.text_color = text_color;
    w->label.is_ouc     = is_ouc;
    w->label.has_shadow = has_shadow;
    w->update_func      = dbg_widget_label_update;
    w->draw_func        = dbg_widget_label_draw;
}

void dbg_widget_label_update(DBGWidgetUpdatePackage *p) {
    DBGWidgetLabel *wl = &p->widget->label;
    DBGWidget *w       = p->widget;
    AFont *font        = wl->font;

    Vector2 text_dimensions;
    if (wl->is_ouc) {
        text_dimensions = measure_text_ouc(font, wl->text);
    } else {
        text_dimensions = measure_text(font, wl->text);
    }

    w->bounds.width  = text_dimensions.x;
    w->bounds.height = text_dimensions.y;
    i_parse_bounds(p->window_rec, &w->bounds, false);
}

void dbg_widget_label_draw(DBGWidgetDrawPackage *p) {
    DBGWidget *w       = p->widget;
    DBGWidgetLabel *wl = &w->label;
    AFont *font        = wl->font;
    Vector2 position   = {w->bounds.x, w->bounds.y};

    // If the string starts with ">> ", render it via 2 different colors.
    if (wl->is_ouc) {
        if (wl->has_shadow) {
            d2d_text_ouc_shadow(font, wl->text, position, wl->text_color, DBG_SHADOW_COLOR, DBG_SHADOW_OFFSET(font));
        } else {
            d2d_text_ouc(font, wl->text, position, wl->text_color);
        }
    } else if (ou_strncmp(wl->text, ">> ", 3) == 0) {
        position.x += measure_text(font, ">> ").x;
        d2d_text(font, ">> ", {w->bounds.x, w->bounds.y}, DARKGRAY);
        // +3 to skip ">> ".
        if (wl->has_shadow) {
            d2d_text_ouc_shadow(font, wl->text + 3, position, wl->text_color, DBG_SHADOW_COLOR, DBG_SHADOW_OFFSET(font));
        } else {
            d2d_text(font, wl->text + 3, position, wl->text_color);
        }
    } else {
        if (wl->has_shadow) {
            d2d_text_ouc_shadow(font, wl->text, position, wl->text_color, DBG_SHADOW_COLOR, DBG_SHADOW_OFFSET(font));
        } else {
            d2d_text(font, wl->text, position, wl->text_color);
        }
    }
}

// ===============================================================
// ======================= Widget: BUTTON ========================
// ===============================================================
void dbg_widget_button_add(C8 const *text, AFont *font, Color text_color, Vector2 min_button_size, DBGWidgetCallback on_click_cb) {
    DBGWidget *w              = i_get_next_free_widget();
    w->type                   = DBG_WIDGET_TYPE_BUTTON;
    w->button.text            = text;
    w->button.font            = font;
    w->button.text_color      = text_color;
    w->button.min_button_size = min_button_size;
    w->button.on_click_cb     = on_click_cb;
    w->button.mouse_state     = DBG_WIDGET_MOUSE_STATE_IDLE;
    w->update_func            = dbg_widget_button_update;
    w->draw_func              = dbg_widget_button_draw;
}

void dbg_widget_button_update(DBGWidgetUpdatePackage *p) {
    DBGWidget *w       = p->widget;
    DBGWidgetButton *b = &w->button;

    w->bounds.width  = b->min_button_size.x;
    w->bounds.height = b->min_button_size.y;
    i_parse_bounds(p->window_rec, &w->bounds, false);

    if (i_is_widget_interaction_blocked()) {
        b->mouse_state = DBG_WIDGET_MOUSE_STATE_IDLE;
        return;
    }

    if (CheckCollisionPointRec(p->mouse_pos, w->bounds)) {
        b->mouse_state = p->is_mouse_down[I_MOUSE_LEFT] ? DBG_WIDGET_MOUSE_STATE_CLICK : DBG_WIDGET_MOUSE_STATE_HOVER;

        if (p->is_mouse_released[I_MOUSE_LEFT]) {
            if (b->on_click_cb.func) { b->on_click_cb.func(b->on_click_cb.data); }
        }

        return;
    }

    b->mouse_state = DBG_WIDGET_MOUSE_STATE_IDLE;
}

void dbg_widget_button_draw(DBGWidgetDrawPackage *p) {
    DBGWidget *w       = p->widget;
    DBGWidgetButton *b = &w->button;
    AFont *font        = b->font;

    d2d_rectangle_rec(w->bounds, i_dbg_button_state_colors[b->mouse_state]);
    Vector2 const position = {
        w->bounds.x + ((w->bounds.width - measure_text(font, b->text).x) / 2.0F),
        w->bounds.y + ((w->bounds.height - (F32)font->font_size) / 2.0F),
    };
    d2d_text_shadow(font, b->text, position, b->text_color, DBG_SHADOW_COLOR, DBG_SHADOW_OFFSET(font));
}

// ===============================================================
// ====================== Widget: CHECKBOX =======================
// ===============================================================
void dbg_widget_checkbox_add(C8 const *text, AFont *font, Color text_color, Vector2 checkbox_size, BOOL *check_ptr, DBGWidgetCallback on_click_cb) {
    DBGWidget *w              = i_get_next_free_widget();
    w->type                   = DBG_WIDGET_TYPE_CHECKBOX;
    w->checkbox.text          = text;
    w->checkbox.font          = font;
    w->checkbox.text_color    = text_color;
    w->checkbox.checkbox_size = checkbox_size;
    w->checkbox.check_ptr     = check_ptr;
    w->checkbox.on_click_cb   = on_click_cb;
    w->update_func            = dbg_widget_checkbox_update;
    w->draw_func              = dbg_widget_checkbox_draw;
}

void dbg_widget_checkbox_update(DBGWidgetUpdatePackage *p) {
    DBGWidget *w          = p->widget;
    DBGWidgetCheckbox *cb = &w->checkbox;
    AFont *font           = cb->font;

    w->bounds.width = cb->checkbox_size.x + DBG_WIDGET_ELEMENT_TO_TEXT_PADDING + measure_text(font, cb->text).x;
    w->bounds.height = cb->checkbox_size.y;
    i_parse_bounds(p->window_rec, &w->bounds, false);

    if (i_is_widget_interaction_blocked()) {
        cb->mouse_state = DBG_WIDGET_MOUSE_STATE_IDLE;
        return;
    }

    if (CheckCollisionPointRec(p->mouse_pos, w->bounds)) {
        cb->mouse_state = p->is_mouse_down[I_MOUSE_LEFT] ? DBG_WIDGET_MOUSE_STATE_CLICK : DBG_WIDGET_MOUSE_STATE_HOVER;

        if (p->is_mouse_released[I_MOUSE_LEFT]) {
            *cb->check_ptr = !*cb->check_ptr;
            if (cb->on_click_cb.func) { cb->on_click_cb.func(cb->on_click_cb.data); }
        }
    } else {
        cb->mouse_state = DBG_WIDGET_MOUSE_STATE_IDLE;
    }
}

void dbg_widget_checkbox_draw(DBGWidgetDrawPackage *p) {
    DBGWidget *w          = p->widget;
    DBGWidgetCheckbox *cb = &w->checkbox;
    AFont *font           = cb->font;

    d2d_rectangle_v({w->bounds.x, w->bounds.y}, cb->checkbox_size, i_dbg_button_state_colors[cb->mouse_state]);

    Vector2 position = {w->bounds.x, w->bounds.y};
    position.x += cb->checkbox_size.x + DBG_WIDGET_ELEMENT_TO_TEXT_PADDING;
    position.y += (w->bounds.height - (F32)font->font_size) / 2.0F;
    d2d_text(font, cb->text, position, cb->text_color);

    if (*cb->check_ptr) {
        Vector2 check_position = {w->bounds.x, w->bounds.y};
        check_position.x += cb->checkbox_size.x / 4.0F;
        check_position.y += cb->checkbox_size.y / 4.0F;
        d2d_rectangle_v(check_position, {cb->checkbox_size.x / 2.0F, cb->checkbox_size.y / 2.0F}, GREEN);
    }
}

// ===============================================================
// ====================== Widget: DROPDOWN =======================
// ===============================================================
void dbg_widget_dropdown_add(C8 const *text,
                             AFont *font,
                             Color text_color,
                             Color item_color,
                             Color hovered_item_color,
                             Vector2 min_dropdown_size,
                             C8 const **dropdown_items,
                             SZ dropdown_item_count,
                             SZ *selected_idx,
                             DBGWidgetCallback on_click_cb) {
    DBGWidget *w                          = i_get_next_free_widget();
    w->type                               = DBG_WIDGET_TYPE_DROPDOWN;
    w->dropdown.dropdown_idx              = g_dbg.current_dropdown_idx++;
    w->dropdown.text                      = text;
    w->dropdown.font                      = font;
    w->dropdown.text_color                = text_color;
    w->dropdown.item_color                = item_color;
    w->dropdown.hovered_item_color        = hovered_item_color;
    w->dropdown.dropdown_size             = min_dropdown_size;
    w->dropdown.selected_idx              = selected_idx;
    w->dropdown.on_click_cb               = on_click_cb;
    w->update_func                        = dbg_widget_dropdown_update;
    w->draw_func                          = dbg_widget_dropdown_draw;
    w->dropdown.dropdown_item_hovered_idx = DBG_WIDGET_DROPDOWN_ITEM_IDX_NONE;
    w->dropdown.dropdown_items            = mmta(C8 const **, sizeof(C8 const *) * dropdown_item_count);
    w->dropdown.dropdown_item_count       = dropdown_item_count;
    for (SZ i = 0; i < dropdown_item_count; ++i) { w->dropdown.dropdown_items[i] = dropdown_items[i]; }
}

void dbg_widget_dropdown_update(DBGWidgetUpdatePackage *p) {
    DBGWidget *w          = p->widget;
    DBGWidgetDropdown *dd = &w->dropdown;
    AFont *font           = dd->font;
    F32 widest_item       = 0.0F;

    for (SZ i = 0; i < dd->dropdown_item_count; ++i) {
        Vector2 const dropdown_item_size = measure_text(font, dd->dropdown_items[i]);
        widest_item = glm::max(widest_item, dropdown_item_size.x);
    }

    w->bounds.width  = dd->dropdown_size.x + DBG_WIDGET_ELEMENT_TO_TEXT_PADDING + widest_item;
    w->bounds.height = dd->dropdown_size.y;
    i_parse_bounds(p->window_rec, &w->bounds, false);

    F32 extra_height = 0.0F;
    if (dd->is_open) { extra_height = measure_text(font, dd->dropdown_items[0]).y * (F32)dd->dropdown_item_count; }

    Rectangle const check_bounds = {w->bounds.x, w->bounds.y, w->bounds.width, w->bounds.height + extra_height};

    if (i_is_widget_interaction_blocked()) { dd->mouse_state = DBG_WIDGET_MOUSE_STATE_IDLE; }

    BOOL const on_dd = CheckCollisionPointRec(p->mouse_pos, check_bounds);
    BOOL const grace = g_dbg.dropdown_close_grace_period > 0.0F;
    if (!on_dd && grace) { g_dbg.dropdown_close_grace_period -= time_get_delta(); }
    if (on_dd) { g_dbg.dropdown_close_grace_period = DBG_CLOSE_GRACE_PERIOD_START; }
    if (on_dd || grace) {
        if (p->is_mouse_down[I_MOUSE_LEFT] && !dd->is_open) {
            // dd->mouse_state = DBG_WIDGET_MOUSE_STATE_CLICK;
            dd->is_open = true;
        } else {
            dd->mouse_state = DBG_WIDGET_MOUSE_STATE_HOVER;
        }

        g_dbg.active_dropdown_idx = dd->dropdown_idx;

        if (g_dbg.active_dropdown_idx == dd->dropdown_idx && dd->is_open) {
            F32 const current_y       = check_bounds.y + w->bounds.height;
            F32 const item_height     = extra_height / (F32)dd->dropdown_item_count;
            F32 const selected_height = ((p->mouse_pos.y - current_y) / item_height);
            if (selected_height >= 0) { dd->dropdown_item_hovered_idx = (SZ)selected_height; }
        }

        BOOL const hovered_idx_valid = dd->dropdown_item_hovered_idx < dd->dropdown_item_count;

        if (p->is_mouse_released[I_MOUSE_LEFT] && hovered_idx_valid) {
            *dd->selected_idx = dd->dropdown_item_hovered_idx;
            if (dd->on_click_cb.func) { dd->on_click_cb.func(dd->on_click_cb.data); }
            dd->is_open = false;
        }

        return;
    }

    dd->mouse_state               = DBG_WIDGET_MOUSE_STATE_IDLE;
    dd->dropdown_item_hovered_idx = DBG_WIDGET_DROPDOWN_ITEM_IDX_NONE;
    dd->is_open                   = false;
}

void dbg_widget_dropdown_draw(DBGWidgetDrawPackage *p) {
    DBGWidget *w                  = p->widget;
    DBGWidgetDropdown *dd         = &w->dropdown;
    AFont *font                   = dd->font;
    Color const dd_color          = i_dbg_button_state_colors[dd->mouse_state];
    F32 const pad_from_text_to_dd = 5.0F;
    Vector2 text_position         = {w->bounds.x, w->bounds.y};

    text_position.x += DBG_WIDGET_ELEMENT_TO_TEXT_PADDING;
    text_position.y += (w->bounds.height - (F32)font->font_size) / 2.0F;
    d2d_text(font, dd->text, text_position, dd->text_color);
    Vector2 const text_dimensions = measure_text(font, dd->text);

    F32 widest_item = 0.0F;
    for (SZ i = 0; i < dd->dropdown_item_count; ++i) {
        Vector2 const dropdown_item_size = measure_text(font, dd->dropdown_items[i]);
        widest_item = glm::max(widest_item, dropdown_item_size.x);
    }
    widest_item += DBG_WIDGET_ELEMENT_TO_TEXT_PADDING * 2.0F;

    dd->dropdown_size.x = glm::max(dd->dropdown_size.x, widest_item);

    if (dd->is_open && dd->dropdown_item_count > 0 && g_dbg.dragging_wid == DBG_NO_WID) {
        d2d_rectangle_v({pad_from_text_to_dd + DBG_WIDGET_ELEMENT_TO_TEXT_PADDING + text_dimensions.x + w->bounds.x, w->bounds.y},
                        {dd->dropdown_size.x, dd->dropdown_size.y + ((F32)dd->dropdown_item_count * text_dimensions.y) + DBG_WIDGET_ELEMENT_TO_TEXT_PADDING},
                        dd_color);
    } else {
        d2d_rectangle_v({pad_from_text_to_dd + DBG_WIDGET_ELEMENT_TO_TEXT_PADDING + text_dimensions.x + w->bounds.x, w->bounds.y},
                        dd->dropdown_size,
                        dd_color);
    }

    if (*dd->selected_idx != DBG_WIDGET_DROPDOWN_ITEM_IDX_NONE) {
        Vector2 const selected_item_position = {
            pad_from_text_to_dd + text_position.x + text_dimensions.x + DBG_WIDGET_ELEMENT_TO_TEXT_PADDING,
            text_position.y,
        };

        d2d_text_shadow(font, dd->dropdown_items[*dd->selected_idx], selected_item_position, dd->text_color, DBG_SHADOW_COLOR, DBG_SHADOW_OFFSET(font));
    }

    if (i_is_window_being_dragged()) { return; }

    if (dd->is_open) {
        for (SZ i = 0; i < dd->dropdown_item_count; ++i) {
            Vector2 const item_position = {
                pad_from_text_to_dd + text_position.x + text_dimensions.x + DBG_WIDGET_ELEMENT_TO_TEXT_PADDING,
                DBG_WIDGET_ELEMENT_TO_TEXT_PADDING + w->bounds.y + ((F32)font->font_size * (F32)(i + 1)),
            };

            if (i == dd->dropdown_item_hovered_idx) {
                d2d_rectangle_v({item_position.x - pad_from_text_to_dd, item_position.y}, {dd->dropdown_size.x, (F32)font->font_size}, DARKESTGRAY);
            }

            Color const item_color = dd->dropdown_item_hovered_idx == i ? dd->hovered_item_color : dd->item_color;
            d2d_text_shadow(font, dd->dropdown_items[i], item_position, item_color, DBG_SHADOW_COLOR, DBG_SHADOW_OFFSET(font));
        }
    }
}

// ===============================================================
// ======================= Widget: FILLBAR =======================
// ===============================================================
void dbg_widget_fillbar_add(C8 const *text,
                            AFont *font,
                            Color fill_color,
                            Color empty_color,
                            Color text_color,
                            Vector2 fillbar_size,
                            F32 current_value,
                            F32 min_value,
                            F32 max_value,
                            BOOL is_slider,
                            void *slider_value,
                            DBGWidgetDataType slider_data_type,
                            DBGWidgetCallback on_click_cb) {
    DBGWidget *w                = i_get_next_free_widget();
    w->type                     = DBG_WIDGET_TYPE_FILLBAR;
    w->fillbar.text             = text;
    w->fillbar.font             = font;
    w->fillbar.fill_color       = fill_color;
    w->fillbar.empty_color      = empty_color;
    w->fillbar.text_color       = text_color;
    w->fillbar.fillbar_size     = fillbar_size;
    w->fillbar.current_value    = current_value;
    w->fillbar.min_value        = min_value;
    w->fillbar.max_value        = max_value;
    w->fillbar.is_slider        = is_slider;
    w->fillbar.slider_value     = slider_value;
    w->fillbar.slider_data_type = slider_data_type;
    w->fillbar.on_click_cb      = on_click_cb;
    w->update_func              = dbg_widget_fillbar_update;
    w->draw_func                = dbg_widget_fillbar_draw;
}

void dbg_widget_fillbar_update(DBGWidgetUpdatePackage *p) {
    DBGWidget *w         = p->widget;
    DBGWidgetFillbar *fb = &w->fillbar;
    w->bounds.width      = fb->fillbar_size.x;
    w->bounds.height     = fb->fillbar_size.y;
    i_parse_bounds(p->window_rec, &w->bounds, false);

    if (fb->is_slider) {
        if (i_is_widget_interaction_blocked()) { return; }

        BOOL const in_fb = CheckCollisionPointRec(p->mouse_pos, w->bounds);

        // Handle right-click to cycle min/max
        if (in_fb && p->is_mouse_pressed[I_MOUSE_RIGHT]) {
            F32 target_value = 0.0F;

            // If current value is min, go to max; if current value is max, go to min; otherwise go to min
            if (fb->current_value == fb->min_value) {
                target_value = fb->max_value;
            } else {
                target_value = fb->min_value;
            }

            // Set the value
            if (fb->slider_data_type == DBG_WIDGET_DATA_TYPE_INT) {
                *(S32 *)fb->slider_value = (S32)target_value;
            } else if (fb->slider_data_type == DBG_WIDGET_DATA_TYPE_U8) {
                *(U8 *)fb->slider_value = (U8)target_value;
            } else {
                *(F32 *)fb->slider_value = target_value;
            }
            fb->current_value = target_value;
            if (fb->on_click_cb.func) { fb->on_click_cb.func(fb->on_click_cb.data); }
            return;
        }

        // Normal left-click drag behavior
        if (!in_fb || !p->is_mouse_down[I_MOUSE_LEFT]) { return; }
        F32 const perc_x = (p->mouse_pos.x - w->bounds.x) / w->bounds.width;

        if (fb->slider_data_type == DBG_WIDGET_DATA_TYPE_INT) {
            S32 *slider_value = (S32 *)fb->slider_value;
            *slider_value     = (S32)math_lerp_f32(fb->min_value, fb->max_value, perc_x);
            fb->current_value = (F32)*slider_value;
        } else if (fb->slider_data_type == DBG_WIDGET_DATA_TYPE_U8) {
            U8 *slider_value  = (U8 *)fb->slider_value;
            *slider_value     = (U8)math_lerp_f32(fb->min_value, fb->max_value, perc_x);
            fb->current_value = (F32)*slider_value;
        } else {
            F32 *slider_value = (F32 *)fb->slider_value;
            *slider_value     = math_lerp_f32(fb->min_value, fb->max_value, perc_x);
            fb->current_value = *slider_value;
        }
        if (fb->on_click_cb.func) { fb->on_click_cb.func(fb->on_click_cb.data); }
    }
}

void dbg_widget_fillbar_draw(DBGWidgetDrawPackage *p) {
    DBGWidget *w         = p->widget;
    DBGWidgetFillbar *fb = &w->fillbar;
    AFont *font          = fb->font;

    // If the max_value is 0, we just draw the empty fillbar.
    if (fb->max_value == 0.0F) {
        d2d_rectangle_rec(w->bounds, fb->empty_color);
        return;
    }

    Color const faded_fill_color = Fade(fb->fill_color, 0.5F);
    F32 const fillbar_perc       = fb->current_value / fb->max_value;
    d2d_rectangle_v({w->bounds.x, w->bounds.y}, fb->fillbar_size, fb->empty_color);
    d2d_rectangle_v({w->bounds.x, w->bounds.y}, {fb->fillbar_size.x * fillbar_perc, fb->fillbar_size.y}, faded_fill_color);

    Vector2 position   = {w->bounds.x, w->bounds.y};
    C8 const *perc_str = nullptr;
    BOOL ouc           = true;
    if (fb->text) {
        perc_str = TS("%.2f%% \\ouc{#bfcc00ff}(%s)", fillbar_perc * 100.0F, fb->text)->c;
        position.x += (fb->fillbar_size.x / 2.0F) - (measure_text_ouc(font, perc_str).x / 2.0F);
    } else {
        perc_str = TS("%.2f%%", fillbar_perc * 100.0F)->c;
        position.x += (fb->fillbar_size.x / 2.0F) - (measure_text(font, perc_str).x / 2.0F);
        ouc = false;
    }
    position.y += (fb->fillbar_size.y - (F32)font->font_size) / 2.0F;

    if (ouc) {
        d2d_text_ouc_shadow(font, perc_str, position, fb->text_color, BLACK, DBG_SHADOW_OFFSET(font));
    } else {
        d2d_text_shadow(font, perc_str, position, fb->text_color, BLACK, DBG_SHADOW_OFFSET(font));
    }
}

// ===============================================================
// ====================== Widget: TIMELINE =======================
// ===============================================================
void dbg_widget_timeline_add(Color foreground_color,
                             Color background_color,
                             F32 *data_ptr,
                             SZ data_count,
                             F32 min_value,
                             F32 max_value,
                             C8 const *fmt,
                             C8 const *unit) {
    DBGWidget *w                 = i_get_next_free_widget();
    w->type                      = DBG_WIDGET_TYPE_TIMELINE;
    w->timeline.data_ptr         = data_ptr;
    w->timeline.data_count       = data_count;
    w->timeline.foreground_color = foreground_color;
    w->timeline.background_color = background_color;
    w->timeline.min_value        = min_value;
    w->timeline.max_value        = max_value;
    w->timeline.fmt              = fmt;
    w->timeline.unit             = unit;
    w->update_func               = dbg_widget_timeline_update;
    w->draw_func                 = dbg_widget_timeline_draw;
}

void dbg_widget_timeline_update(DBGWidgetUpdatePackage *p) {
    DBGWidget *w     = p->widget;
    w->bounds.width  = p->window_rec->width;
    w->bounds.height = DBG_TIMELINE_HEIGHT;
    i_parse_bounds(p->window_rec, &w->bounds, true);
}

void dbg_widget_timeline_draw(DBGWidgetDrawPackage *p) {
    DBGWidget *w          = p->widget;
    DBGWidgetTimeline *tl = &w->timeline;
    Rectangle bounds      = w->bounds;

    // NOTE: I am not sure if we are doing this right. We just introduced the ignore_width
    // in i_parse_bounds and we need to now adjust the width, so it looks fine.
    // It is a cluster fuck but it's just debug tooling.

    bounds.width -= ((2.0F * DBG_WINDOW_BORDER_SIZE) + DBG_WINDOW_INNER_PADDING);

    if (tl->data_count < 2) { return; }

    d2d_rectangle_rec(bounds, tl->background_color);

    F32 const step_x = bounds.height / (tl->max_value - tl->min_value);
    F32 const step_y = bounds.width / (F32)tl->data_count;

    // Build points array for line strip - much more efficient than individual lines
    static Vector2 points[TIME_FRAME_TIMES_TIMELINE_MAX_COUNT];
    for (SZ i = 0; i < tl->data_count; ++i) {
        F32 value = tl->data_ptr[i];
        value = glm::clamp(value, tl->min_value, tl->max_value);
        points[i] = {bounds.x + ((F32)i * step_y), bounds.y + bounds.height - ((value - tl->min_value) * step_x)};
    }

    // Fix last point width compensation
    if (tl->data_count > 1) { points[tl->data_count - 1].x = bounds.x + bounds.width; }

    // Draw entire timeline with single line strip call
    d2d_line_strip(points, tl->data_count, tl->foreground_color);

    // Draw zero mark if it's within the bounds (not when it's at the very top or bottom).
    if (tl->min_value < 0.0F && tl->max_value > 0.0F) {
        F32 const zero_y = bounds.y + bounds.height - ((-tl->min_value) * step_x);
        if (zero_y >= bounds.y && zero_y <= bounds.y + bounds.height) {
            d2d_line({bounds.x, zero_y}, {bounds.x + bounds.width, zero_y}, Fade(color_invert(tl->foreground_color), 0.5F));
        }
    }

    AFont *font = g_dbg.medium_font;

    SZ const latest_value_idx  = glm::min(tl->data_count - 1, (SZ)(PROFILER_FRAME_TIMES_TIMELINE_MAX_COUNT - 1));
    C8 const *latest_value_str = TS(tl->fmt, tl->data_ptr[latest_value_idx], tl->unit ? tl->unit : "")->c;

    Vector2 const text_dimensions = measure_text(font, latest_value_str);
    Vector2 const position = {
        bounds.x + bounds.width - (bounds.width / 2.0F) - (text_dimensions.x / 2.0F),
        bounds.y + bounds.height - (bounds.height / 2.0F) - (text_dimensions.y / 2.0F),
    };

    // Color based on proximity to min (green) or max (red) value.
    Color const text_color = color_lerp(OFFCOLOR, ONCOLOR, 1.0F - (tl->data_ptr[latest_value_idx] / tl->max_value));
    d2d_text_shadow(font, latest_value_str, position, text_color, DBG_SHADOW_COLOR, DBG_SHADOW_OFFSET(font));
}

// ===============================================================
// ======================= Widget: TEXTURE =======================
// ===============================================================
void dbg_widget_texture_add(C8 const *text, RenderTexture *texture, Vector2 texture_size, Color highlight_color, Color texture_tint) {
    DBGWidget *w               = i_get_next_free_widget();
    w->type                    = DBG_WIDGET_TYPE_TEXTURE;
    w->texture.text            = text;
    w->texture.texture         = texture;
    w->texture.texture_size    = texture_size;
    w->texture.highlight_color = highlight_color;
    w->texture.texture_tint    = texture_tint;
    w->update_func             = dbg_widget_texture_update;
    w->draw_func               = dbg_widget_texture_draw;
}

void dbg_widget_texture_update(DBGWidgetUpdatePackage *p) {
    DBGWidget *w     = p->widget;
    w->bounds.width  = w->texture.texture_size.x;
    w->bounds.height = w->texture.texture_size.y;
    i_parse_bounds(p->window_rec, &w->bounds, false);

    if (i_is_ui_interaction_in_progress()) { return; }

    // Reset big view flag by default
    w->texture.want_big_view = false;

    // Only set to true if mouse is over and button is down
    if (CheckCollisionPointRec(p->mouse_pos, w->bounds)) {
        w->texture.want_big_view = (p->is_mouse_down[I_MOUSE_LEFT]);
        w->texture.want_depth    = (p->is_mouse_down[I_MOUSE_RIGHT]);
    }
}

void dbg_widget_texture_draw(DBGWidgetDrawPackage *p) {
    DBGWidget const *w        = p->widget;
    DBGWidgetTexture const *t = &w->texture;
    AFont *font               = g_dbg.medium_font;
    Texture const *to_draw    = t->want_depth ? &t->texture->depth : &t->texture->texture;

    // Only draw miniatures if nothing is in big view
    if (!g_dbg.modal_overlay_active) {
        Rectangle const bounds         = w->bounds;
        Rectangle const src            = {0.0F, 0.0F, (F32)to_draw->width, (F32)-to_draw->height};
        Rectangle const dst            = {bounds.x, bounds.y, bounds.width, bounds.height};
        Rectangle const outside_border = {bounds.x - 1, bounds.y - 1, bounds.width + 2, bounds.height + 2};
        Color const background_color   = c_debug__texture_widget_dark_bg ? BLACK : MAGENTA;
        d2d_rectangle_lines_ex(outside_border, 1, t->highlight_color);
        d2d_rectangle_rec(bounds, background_color);
        d2d_texture_pro_raylib(*to_draw, src, dst, {}, 0.0F, t->texture_tint);
        d2d_text_shadow(font, t->text, {bounds.x + 2.0F, bounds.y + 2.0F}, t->highlight_color, DBG_SHADOW_COLOR, DBG_SHADOW_OFFSET(font));
    }

    // Draw big view if requested and nothing else is already in big view
    if (t->want_big_view && !g_dbg.modal_overlay_active) {
        g_dbg.modal_overlay_active = true;  // Set flag to prevent other textures from drawing
        Vector2 const res   = render_get_render_resolution();;
        Rectangle const src = {0.0F, 0.0F, (F32)to_draw->width, (F32)-to_draw->height};
        Rectangle const dst = {0.0F, 0.0F, res.x, res.y};
        d2d_rectangle_rec({0.0F, 0.0F, res.x, res.y}, c_debug__texture_widget_dark_bg ? BLACK : MAGENTA);
        d2d_texture_pro_raylib(*to_draw, src, dst, {}, 0.0F, WHITE);
    }
}

// ===============================================================
// ===================== Widget: COLOR VIEW ======================
// ===============================================================
void dbg_widget_color_view_add(C8 const *text, Color *color_ptr, Vector2 size, DBGWidgetCallback on_color_change_cb) {
    DBGWidget *w                     = i_get_next_free_widget();
    w->type                          = DBG_WIDGET_TYPE_COLOR_VIEW;
    w->color_view.text               = text;
    w->color_view.color_ptr          = color_ptr;
    w->color_view.size               = size;
    w->update_func                   = dbg_widget_color_view_update;
    w->draw_func                     = dbg_widget_color_view_draw;
    w->color_view.on_color_change_cb = on_color_change_cb;
}

void dbg_widget_color_view_update(DBGWidgetUpdatePackage *p) {
    DBGWidget *w           = p->widget;
    DBGWidgetColorView *cv = &p->widget->color_view;
    w->bounds.width        = cv->size.x;
    w->bounds.height       = cv->size.y;
    i_parse_bounds(p->window_rec, &w->bounds, true);

    cv->big_view_src = {
        0.0F,
        0.0F,
        (F32)g_dbg.color_view_selection_texture.width,
        (F32)g_dbg.color_view_selection_texture.height,
    };

    // Get the screen dimensions to check boundaries
    Vector2 const render_res = render_get_render_resolution();;

    // Check if the color picker would go off-screen on the right
    F32 const right_edge     = w->bounds.x + w->bounds.width + cv->big_view_src.width;
    BOOL const fits_on_right = right_edge <= render_res.x;
    F32 const margin         = 5.0F;

    // Position the color picker on the right or left based on available space
    if (fits_on_right) {
        // Default position (right side)
        cv->big_view_dst = {
            w->bounds.x + w->bounds.width + margin,
            w->bounds.y,
            cv->big_view_src.width,
            cv->big_view_src.height,
        };
    } else {
        // Not enough space on right, position on left side
        cv->big_view_dst = {
            w->bounds.x - cv->big_view_src.width - margin,
            w->bounds.y,
            cv->big_view_src.width,
            cv->big_view_src.height,
        };
    }

    if (i_is_ui_interaction_in_progress()) { return; }

    Vector2 const mouse_pos      = p->mouse_pos;
    BOOL const inside_big_view   = CheckCollisionPointRec(mouse_pos, cv->big_view_dst);
    BOOL const inside_color_view = CheckCollisionPointRec(mouse_pos, w->bounds);

    // Check if mouse is in the area between the color view and big view
    BOOL inside_area_inbetween = false;
    if (fits_on_right) {
        // When big view is on the right
        Rectangle const inbetween_area = {
            w->bounds.x + w->bounds.width,
            glm::min(w->bounds.y, cv->big_view_dst.y),
            margin,
            glm::max(w->bounds.y + w->bounds.height, cv->big_view_dst.y + cv->big_view_dst.height) - glm::min(w->bounds.y, cv->big_view_dst.y),
        };
        inside_area_inbetween = CheckCollisionPointRec(mouse_pos, inbetween_area);
    } else {
        // When big view is on the left
        Rectangle const inbetween_area = {
            cv->big_view_dst.x + cv->big_view_dst.width,
            glm::min(w->bounds.y, cv->big_view_dst.y),
            margin,
            glm::max(w->bounds.y + w->bounds.height, cv->big_view_dst.y + cv->big_view_dst.height) - glm::min(w->bounds.y, cv->big_view_dst.y),
        };
        inside_area_inbetween = CheckCollisionPointRec(mouse_pos, inbetween_area);
    }

    // Set big view visibility based on modifier key and mouse position
    cv->want_big_view = is_mod(I_MODIFIER_CTRL) && (inside_big_view || inside_color_view || inside_area_inbetween);

    BOOL want_new_texture = false;
    if (cv->want_big_view && inside_big_view) {
        if (p->is_mouse_pressed[I_MOUSE_LEFT]) {
            Color const got_color = dbg_get_color_at_color_view(mouse_pos, cv->big_view_dst);
            if (!color_is_equal(got_color, g_dbg.color_view_last_color) && got_color.a > 0) {
                want_new_texture = true;
                *cv->color_ptr   = got_color;
            }
            if (cv->on_color_change_cb.func) { cv->on_color_change_cb.func(cv->on_color_change_cb.data); }
        }

        if (p->is_mouse_pressed[I_MOUSE_RIGHT]) {
            g_dbg.color_view_copied_color = dbg_get_color_at_color_view(mouse_pos, cv->big_view_dst);
            C8 buf[32];  // RRGGBBAA = 4*8 bytes
            color_hex_from_color(g_dbg.color_view_copied_color, buf);
            mio(TS("Color copied: \\ouc{#%s}#%s", buf, buf)->c, WHITE);
        }

        if (p->is_mouse_pressed[I_MOUSE_MIDDLE]) {
            want_new_texture = true;
            *cv->color_ptr = g_dbg.color_view_copied_color;
            C8 buf[32];  // RRGGBBAA = 4*8 bytes
            color_hex_from_color(g_dbg.color_view_copied_color, buf);
            mio(TS("Color pasted: \\ouc{#%s}#%s", buf, buf)->c, WHITE);
            if (cv->on_color_change_cb.func) { cv->on_color_change_cb.func(cv->on_color_change_cb.data); }
        }
    }

    if (cv->want_big_view) {
        if (!color_is_equal(*cv->color_ptr, g_dbg.color_view_last_color)) { want_new_texture = true; }
    }

    if (want_new_texture) {
        i_prepare_color_view_selection_texture(*cv->color_ptr);
        g_dbg.color_view_last_color = *cv->color_ptr;
    }
}

void dbg_widget_color_view_draw(DBGWidgetDrawPackage *p) {
    DBGWidget const *w           = p->widget;
    Rectangle const bounds       = w->bounds;
    DBGWidgetColorView const *cv = &w->color_view;
    AFont *font                  = g_dbg.medium_font;
    F32 const padding            = 2.0F;
    Rectangle const top_bounds   = {bounds.x, bounds.y, bounds.width, (F32)font->font_size + (padding * 2.0F)};
    d2d_rectangle_rec(bounds, *cv->color_ptr);
    d2d_rectangle_rec(top_bounds, Fade(BLACK, 0.5F));
    d2d_text_shadow(g_dbg.medium_font, cv->text, {bounds.x + padding, bounds.y + padding}, WHITE, DBG_SHADOW_COLOR, DBG_SHADOW_OFFSET(font));

    if (cv->want_big_view && !g_dbg.modal_overlay_active) {
        g_dbg.modal_overlay_active = true;
        d2d_rectangle_rec(cv->big_view_dst, Fade(DBG_WINDOW_BG_COLOR, 0.8F));
        d2d_texture_pro_raylib(g_dbg.color_view_selection_texture, cv->big_view_src, cv->big_view_dst, {}, 0.0F, WHITE);
    }
}

void dbg_reset_windows(BOOL everything) {
    // First press: If active, deactivate debug
    if (!c_debug__enabled) {
        c_debug__enabled = true;
        return;
    }

    // Second press: If a window not collapsed, collapse window
    BOOL some_window_not_collapsed = false;
    for (SZ i = 0; i < DBG_WID_COUNT - 1; ++i) {
        DBGWindow *window = &g_dbg.windows[i];
        if (!(window->flags & DBG_WFLAG_COLLAPSED)) { some_window_not_collapsed = true; }
        window->flags |= DBG_WFLAG_COLLAPSED;
    }
    if (some_window_not_collapsed) { return; }

    // Third press: Rest
    DBGWindow *first_window            = &g_dbg.windows[0];
    Vector2 const res                  = render_get_render_resolution();;
    F32 const padding_to_screen        = 20.0F;
    F32 const padding_between_elements = 10.0F;
    first_window->bounds.x = res.x - first_window->bounds.width - padding_to_screen;
    first_window->bounds.y = 15.0F;

    for (SZ i = 0; i < DBG_WID_COUNT - 1; ++i) {
        DBGWindow *window      = &g_dbg.windows[i];
        DBGWindow *next_window = &g_dbg.windows[i + 1];
        next_window->bounds.x  = res.x - next_window->bounds.width - padding_to_screen;
        next_window->bounds.y  = window->bounds.y + window->bounds.height + padding_between_elements;
    }

    if (everything) {
        g_dbg.active_wid        = DBG_WID_OPTIONS;
        g_dbg.dragging_wid      = DBG_NO_WID;
        c_debug__windows_sticky = false;
    }
}

void dbg_load_ini() {
    BOOL exists = false;
    g_dbg.ini = ini_file_create(MEMORY_TYPE_ARENA_TRANSIENT, DBG_INI_FILE_PATH, &exists);
    if (!exists) {
        // Since the config did not exist, we just want to reset everything to default and return early
        dbg_reset_windows(true);
        return;
    }

    for (SZ i = 0; i < DBG_WID_COUNT; ++i) {
        String *label = TS("window_%s", i_dbg_window_name_strings[i]);
        if (!ini_file_header_exists(g_dbg.ini, label->c)) {
            llw("Window %s not found in ini file", i_dbg_window_name_strings[i]);
            continue;
        }

        g_dbg.windows[i].flags    = ini_file_get_s32(g_dbg.ini, label->c, "flags", false);
        g_dbg.windows[i].bounds.x = (F32)ini_file_get_s32(g_dbg.ini, label->c, "pos_x", 0);
        g_dbg.windows[i].bounds.y = (F32)ini_file_get_s32(g_dbg.ini, label->c, "pos_y", 0);
    }
}

void dbg_save_ini() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_ARENA_TRANSIENT, DBG_INI_FILE_PATH, nullptr);

    for (SZ i = 0; i < DBG_WID_COUNT; ++i) {
        String *label = TS("window_%s", i_dbg_window_name_strings[i]);
        ini_file_set_s32(ini, label->c, "flags", g_dbg.windows[i].flags);
        ini_file_set_s32(ini, label->c, "pos_x", (S32)g_dbg.windows[i].bounds.x);
        ini_file_set_s32(ini, label->c, "pos_y", (S32)g_dbg.windows[i].bounds.y);
    }

    ini_file_save(ini);
}

Color dbg_get_color_at_color_view(Vector2 pos, Rectangle color_view_bounds) {
    // Get the dimensions directly from the clean texture
    S32 const texture_width  = g_dbg.color_view_clean_texture.width;
    S32 const texture_height = g_dbg.color_view_clean_texture.height;

    // Calculate the relative position (0.0 to 1.0) within the bounds
    F32 const rel_x = (pos.x - color_view_bounds.x) / color_view_bounds.width;
    F32 const rel_y = (pos.y - color_view_bounds.y) / color_view_bounds.height;

    // Map to the actual pixel space of the image data
    S32 const img_x = (S32)(rel_x * (F32)texture_width);
    S32 const img_y = (S32)(rel_y * (F32)texture_height);

    // Bounds check
    if (img_x < 0 || img_y < 0 || img_x >= texture_width || img_y >= texture_height) { return BLANK; }

    // We can't access texture data directly, so we need to get the color from the original image
    // Get color from the CLEAN image (no indicators)
    Color pixel_color = GetImageColor(g_dbg.color_view_selection_image, img_x, img_y);

    return pixel_color;
}

BOOL dbg_is_mouse_over_or_dragged() {
    if (!c_debug__enabled)                { return false; }
    if (g_dbg.dragging_wid != DBG_NO_WID) { return true;  }

    Vector2 const mouse_pos = input_get_mouse_position();

    for (auto &window : g_dbg.windows) {
        if (CheckCollisionPointRec(mouse_pos, window.bounds)) { return true; }
    }

    return g_profiler.mouse_over;
}
