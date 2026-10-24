#pragma once
#include "radio_link.h"
#include <stddef.h>
#include <stdbool.h>
typedef enum {
    CHAT_CLOSED, CHAT_ACTIONS, CHAT_TARGETS, CHAT_EMOJI, CHAT_RETRY_TARGETS,
    CHAT_RETRY_REVIEW, CHAT_CLEAR_CONFIRM, CHAT_REPLY_TARGETS, CHAT_COPY_TARGETS,
    CHAT_COPY_REVIEW, CHAT_DELETE_TARGETS, CHAT_DELETE_REVIEW, CHAT_EXPORT_REVIEW,
    CHAT_EXPORT_STATUS
} mesh_chat_mode_t;
typedef enum {
    CHAT_INSERT_EMOJI, CHAT_SEND_TAPBACK, CHAT_RETRY_MESSAGE, CHAT_CLEAR_THREAD,
    CHAT_SEARCH_MESSAGES, CHAT_REPLY_MESSAGE, CHAT_REUSE_MESSAGE, CHAT_DELETE_MESSAGE,
    CHAT_EXPORT_THREAD, CHAT_BROWSE_ARCHIVE, MESH_CHAT_ACTION_COUNT
} mesh_chat_action_t;
typedef enum { MESH_CHAT_KEY_OTHER, MESH_CHAT_KEY_UP, MESH_CHAT_KEY_DOWN,
    MESH_CHAT_KEY_LEFT, MESH_CHAT_KEY_RIGHT, MESH_CHAT_KEY_RETURN, MESH_CHAT_KEY_ESC } mesh_chat_key_t;
typedef enum { MESH_CHAT_IGNORED, MESH_CHAT_HANDLED, MESH_CHAT_SEARCH, MESH_CHAT_ARCHIVE } mesh_chat_input_result_t;
typedef struct {const char *name,*detail;} mesh_chat_action_entry_t;
// The app owns the fixed snapshot in PSRAM. The controller has no hidden state.
typedef struct {
    mesh_chat_mode_t modal;
    size_t action_selection,emoji_selection,target_selection,target_count;
    bool picker_reaction,confirm_selected;
    uint32_t clear_peer;
    uint8_t clear_channel;
    char feedback[80];
    mesh_message_t targets[MESH_MESSAGE_HISTORY_MAX];
} mesh_chat_actions_t;
// Borrowed current-thread state, valid only during a controller call.
typedef struct {
    uint32_t peer;
    uint8_t channel;
    char *draft,*feedback;
    size_t draft_capacity,feedback_size;
    uint32_t *reply_id;
    size_t *scroll;
} mesh_chat_context_t;
void mesh_chat_actions_open(mesh_chat_actions_t *state);
mesh_chat_input_result_t mesh_chat_actions_input(mesh_chat_actions_t *state,const mesh_chat_context_t *context,mesh_chat_key_t key);
bool mesh_chat_is_target_picker(mesh_chat_mode_t mode);
bool mesh_chat_is_confirmation(mesh_chat_mode_t mode);
mesh_chat_action_entry_t mesh_chat_action_entry(size_t index,bool replying);
