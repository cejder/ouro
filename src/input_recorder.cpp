#include "input.hpp"

void mouse_recorder_init(MouseTape *tape, C8 const *name) {
    array_init(MEMORY_TYPE_PARENA, &tape->frames, 0);
    tape->name = string_create(MEMORY_TYPE_PARENA, "%s", name);
}

void mouse_recorder_record_frame(MouseTape* tape) {
    MouseFrameInfo info = {};
    info.position   = input_get_mouse_position();
    info.wheel      = input_get_mouse_wheel();
    for (SZ i = 1; i < I_MOUSE_COUNT; ++i) {
        info.mouse_down[i]     = is_mouse_down((IMouse)i);
        info.mouse_up[i]       = is_mouse_up((IMouse)i);
        info.mouse_pressed[i]  = is_mouse_pressed((IMouse)i);
        info.mouse_released[i] = is_mouse_released((IMouse)i);
    }

    array_push(&tape->frames, info);
}

void mouse_recorder_reset(MouseTape* tape) {
    array_clear(&tape->frames);
}

void mouse_recorder_save(MouseTape *tape) {
    String *path = TS("%s%s.out", MOUSE_TAPES_PATH, tape->name->c);
    String *out  = string_create_empty(MEMORY_TYPE_TARENA);

    for (SZ i = 1; i < tape->frames.count; ++i) {
        string_appendf(out, "%.2f;", tape->frames.data[i].position.x);
        string_appendf(out, "%.2f;", tape->frames.data[i].position.y);
        string_appendf(out, "%.2f;", tape->frames.data[i].wheel);
        for (SZ j = 0; j < I_MOUSE_COUNT; ++j) {
            string_appendf(out, "%d;", tape->frames.data[i].mouse_down[j]);
            string_appendf(out, "%d;", tape->frames.data[i].mouse_up[j]);
            string_appendf(out, "%d;", tape->frames.data[i].mouse_pressed[j]);
            string_appendf(out, "%d;", tape->frames.data[i].mouse_released[j]);
        }
    }

    if (!DirectoryExists(MOUSE_TAPES_PATH)) { MakeDirectory(MOUSE_TAPES_PATH); }

    S32 comp_data_size = 0;
    U8 *comp_data = CompressData((U8 const *)out->c, (S32)out->length, &comp_data_size);
    if (comp_data == nullptr) {
        lle("Failed to compress mouse tape data for tape '%s'", tape->name->c);
        return;
    }

    if (!SaveFileData(path->c, comp_data, comp_data_size)) {
        lle("Failed to save compressed mouse tape data for tape '%s'", tape->name->c);
        MemFree(comp_data);
        return;
    }

    MemFree(comp_data);
}

void mouse_recorder_load(MouseTape *tape, C8 const *name) {
    if (tape->name == nullptr) { mouse_recorder_init(tape, name); }

    String *path = TS("%s%s.out", MOUSE_TAPES_PATH, tape->name->c);

    if (!FileExists(path->c)) {
        lle("Failed to load mouse tape data for tape '%s': File not found (%s)", name, path->c);
        return;
    }

    S32 comp_data_size = 0;
    U8 *comp_data = LoadFileData(path->c, &comp_data_size);
    if (comp_data == nullptr) {
        lle("Failed to load mouse tape data for tape '%s': LoadFileData failed (%s)", name, path->c);
        return;
    }

    S32 data_size = 0;
    U8 *data = DecompressData(comp_data, comp_data_size, &data_size);
    UnloadFileData(comp_data);
    if (data == nullptr) {
        lle("Failed to decompress mouse tape data for tape '%s'", name);
        return;
    }

    array_clear(&tape->frames);

    SZ count = SZ_MAX;
    String **split = string_split(string_create(MEMORY_TYPE_TARENA, "%s", (C8 const *)data), ';', &count);
    MemFree(data);

    SZ const position_fields         = 3;
    SZ const mouse_states_per_button = 4;
    SZ const fields_per_frame        = position_fields + (I_MOUSE_COUNT * mouse_states_per_button);
    SZ const num_frames              = count / fields_per_frame;

    for (SZ frame_idx = 0; frame_idx < num_frames; ++frame_idx) {
        MouseFrameInfo info = {};

        SZ const base_idx = frame_idx * fields_per_frame;
        info.position.x   = string_to_f32(split[base_idx]);
        info.position.y   = string_to_f32(split[base_idx + 1]);
        info.wheel        = string_to_f32(split[base_idx + 2]);

        for (SZ j = 0; j < I_MOUSE_COUNT; ++j) {
            SZ const mouse_base    = base_idx + position_fields + (j * mouse_states_per_button);
            info.mouse_down[j]     = string_to_s32(split[mouse_base]);
            info.mouse_up[j]       = string_to_s32(split[mouse_base + 1]);
            info.mouse_pressed[j]  = string_to_s32(split[mouse_base + 2]);
            info.mouse_released[j] = string_to_s32(split[mouse_base + 3]);
        }

        array_push(&tape->frames, info);
    }
}
