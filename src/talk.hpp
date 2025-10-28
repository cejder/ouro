#pragma once

#include "common.hpp"

#include <raylib.h>

using TALKER_TRIGGER_FUNC = void (*)(void *data);
using TALKER_TRIGGER_DATA = void *;

fwd_decl(String);
fwd_decl(EntityTalker);

// INFO: This might be the ugliest or even the most beautiful thing I have my laid eyes on. I am not sure yet.
// Why do we decrement the node_idx by 1 in responses? Well because we increment the index in the question part
// for the next node, since we do not know when to increment if we were to do it in the responses, we just do
// it earlier and reference the index earlier. I mean we could have also just added a macro for T_QUESTION_END
// or something like that but this feels neater in usage IF EVERYTHING GOES WELL.

// WARN: This format is ESSENTIAL otherwise no compilerino. I forgot why and I am too lazy to check right now.
// INFO: Me from the future: it's this part here T_STRINGIFY(dialogues/d_name.oudh) -> T_STRINGIFY(dialogues / d_name.oudh) with format ergo fucky.

#define T_STRINGIFY(M_x) #M_x
// NOLINTNEXTLINE
#define T_LOAD_DIALOGUE(M_d_name) T_STRINGIFY(dialogues/M_d_name.oudh)

#define T_CONCAT_HELPER(M_a, M_b, M_c) M_a##_##M_b##_##M_c
#define T_CONCAT(M_a, M_b, M_c) T_CONCAT_HELPER(M_a, M_b, M_c)

#define T_INIT(M_d_name, M_node_count)                                                \
    auto *t_##M_d_name = mmpa(DialogueRoot *, sizeof(DialogueRoot) * (M_node_count)); \
    SZ M_node_count_##M_d_name = M_node_count;                                        \
    SZ node_idx_##M_d_name = 0

#define T_STATEMENT(M_d_name, M_node_id, M_sentence_text, M_node_id_next)                                                                  \
    if (node_idx_##M_d_name >= M_node_count_##M_d_name) {                                                                                  \
        lle("Node index exceeded allocated count (%zu) in T_STATEMENT", node_idx_##M_d_name);                                              \
        return;                                                                                                                            \
    }                                                                                                                                      \
    (t_##M_d_name)[node_idx_##M_d_name].node_id = PS(#M_node_id);                                                                          \
    if (find_dialogue_node_by_id(t_##M_d_name, node_idx_##M_d_name, (t_##M_d_name)[node_idx_##M_d_name].node_id->c, true)) {               \
        lle("Node with the same ID (%s) already exists in dialogue tree (%s)", (t_##M_d_name)[node_idx_##M_d_name].node_id->c, #M_d_name); \
        return;                                                                                                                            \
    }                                                                                                                                      \
    (t_##M_d_name)[node_idx_##M_d_name].node_text = PS(M_sentence_text);                                                                   \
    (t_##M_d_name)[node_idx_##M_d_name].node_id_next = PS(#M_node_id_next);                                                                \
    (node_idx_##M_d_name)++

#define T_QUESTION(M_d_name, M_node_id, M_question_text, M_resp_count)                                                                     \
    if (node_idx_##M_d_name >= M_node_count_##M_d_name) {                                                                                  \
        lle("Node index exceeded allocated count (%zu) in T_QUESTION", node_idx_##M_d_name);                                               \
        return;                                                                                                                            \
    }                                                                                                                                      \
    (t_##M_d_name)[node_idx_##M_d_name].node_id = PS(#M_node_id);                                                                          \
    if (find_dialogue_node_by_id(t_##M_d_name, node_idx_##M_d_name, (t_##M_d_name)[node_idx_##M_d_name].node_id->c, true)) {               \
        lle("Node with the same ID (%s) already exists in dialogue tree (%s)", (t_##M_d_name)[node_idx_##M_d_name].node_id->c, #M_d_name); \
        return;                                                                                                                            \
    }                                                                                                                                      \
    (t_##M_d_name)[node_idx_##M_d_name].node_text = PS(M_question_text);                                                                   \
    (t_##M_d_name)[node_idx_##M_d_name].response_count = M_resp_count;                                                                     \
    (t_##M_d_name)[node_idx_##M_d_name].responses = mmpa(DialogueResponse *, sizeof(DialogueResponse) * (M_resp_count));                   \
    SZ T_CONCAT(resp_idx, M_d_name, M_node_id) = 0;                                                                                        \
    (node_idx_##M_d_name)++

#define T_RESPONSE(M_d_name, M_node_id, M_resp_id, M_resp_text, M_node_id_next)                                                              \
    if (T_CONCAT(resp_idx, M_d_name, M_node_id) >= (t_##M_d_name)[node_idx_##M_d_name - 1].response_count) {                                 \
        lle("Response index exceeded allocated count (%zu) in T_RESPONSE", T_CONCAT(resp_idx, M_d_name, M_node_id));                         \
        return;                                                                                                                              \
    }                                                                                                                                        \
    (t_##M_d_name)[node_idx_##M_d_name - 1].responses[T_CONCAT(resp_idx, M_d_name, M_node_id)].response_id = PS(#M_resp_id);                 \
    if (find_dialogue_response_by_id(&(t_##M_d_name)[node_idx_##M_d_name - 1], T_CONCAT(resp_idx, M_d_name, M_node_id), #M_resp_id, true)) { \
        lle("Response with the same ID (%s) already exists in this question node", #M_resp_id);                                              \
        return;                                                                                                                              \
    }                                                                                                                                        \
    (t_##M_d_name)[node_idx_##M_d_name - 1].responses[T_CONCAT(resp_idx, M_d_name, M_node_id)].response_text = PS(M_resp_text);              \
    (t_##M_d_name)[node_idx_##M_d_name - 1].responses[T_CONCAT(resp_idx, M_d_name, M_node_id)].node_id_next = PS(#M_node_id_next);           \
    (T_CONCAT(resp_idx, M_d_name, M_node_id))++

#define T_RESPONSE_TRIGGER(M_d_name, M_node_id, M_resp_id, M_resp_text, M_node_id_next, M_tfunc, M_tdata)                                    \
    if (T_CONCAT(resp_idx, M_d_name, M_node_id) >= (t_##M_d_name)[node_idx_##M_d_name - 1].response_count) {                                 \
        lle("Response index exceeded allocated count in T_RESPONSE");                                                                        \
        return;                                                                                                                              \
    }                                                                                                                                        \
    (t_##M_d_name)[node_idx_##M_d_name - 1].responses[T_CONCAT(resp_idx, M_d_name, M_node_id)].response_id = PS(#M_resp_id);                 \
    if (find_dialogue_response_by_id(&(t_##M_d_name)[node_idx_##M_d_name - 1], T_CONCAT(resp_idx, M_d_name, M_node_id), #M_resp_id, true)) { \
        lle("Response with the same ID (%s) already exists in this question node (%s)", #M_resp_id, #M_d_name);                              \
        return;                                                                                                                              \
    }                                                                                                                                        \
    (t_##M_d_name)[node_idx_##M_d_name - 1].responses[T_CONCAT(resp_idx, M_d_name, M_node_id)].response_text = PS(M_resp_text);              \
    (t_##M_d_name)[node_idx_##M_d_name - 1].responses[T_CONCAT(resp_idx, M_d_name, M_node_id)].node_id_next = PS(#M_node_id_next);           \
    (t_##M_d_name)[node_idx_##M_d_name - 1].responses[T_CONCAT(resp_idx, M_d_name, M_node_id)].trigger_func = M_tfunc;                       \
    (t_##M_d_name)[node_idx_##M_d_name - 1].responses[T_CONCAT(resp_idx, M_d_name, M_node_id)].trigger_data = M_tdata;                       \
    (T_CONCAT(resp_idx, M_d_name, M_node_id))++

#define T_SET(M_d_name, M_eid)                                                                                                                                \
    _assert_(M_node_count_##M_d_name == node_idx_##M_d_name,                                                                                                  \
             PS("%s: Set M_node_count differs from added node count: Set to %zu, but have %zu", #M_d_name, M_node_count_##M_d_name, node_idx_##M_d_name)->c); \
    entity_enable_talker(M_eid, t_##M_d_name, M_node_count_##M_d_name)

#define T_START THE_START
#define T_QUIT THE_END

// WARN: Please use these instead of the one in the header for matching here.
#define T_START_CSTR "T_START"
#define T_QUIT_CSTR "T_QUIT"

struct DialogueResponse {
    String *response_id;
    String *response_text;
    String *node_id_next;  // i.e. using this response leads to this ID.
    TALKER_TRIGGER_DATA trigger_data;
    TALKER_TRIGGER_FUNC trigger_func;
};

struct DialogueNode {  // We call this a node, since it can either be a question or a statement.
    String *node_id;
    String *node_text;
    union {
        DialogueResponse *responses;  // i.e. we either have responses that jump to other nodes or ...
        String *node_id_next;         // ... we have a node and an ID we will jump to.
    };
    SZ response_count;
    SZ response_selection;
};

using DialogueRoot = DialogueNode;  // Root alias

DialogueNode *find_dialogue_node_by_id(DialogueRoot *root, SZ nodes_count, C8 const *id, BOOL can_fail);
DialogueResponse *find_dialogue_response_by_id(DialogueNode *question, SZ response_count, C8 const *id, BOOL can_fail);

struct Conversation {
    DialogueRoot *root;
    DialogueNode *current_node_ptr;
    SZ nodes_count;
    SZ char_count_to_print;
    F32 time_since_last_char;
};

void talker_update(EntityTalker *talker, F32 dt, Vector3 position, F32 entity_width);
void talker_draw(EntityTalker *talker, C8 const *name);
