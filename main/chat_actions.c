#include "chat_actions.h"
#include "message_export.h"
#include "message_reuse.h"
#include "emoji_picker.h"
#include <stdio.h>
#include <string.h>
static size_t thread_count(const mesh_chat_context_t *ctx) {
    return ctx->peer ? mesh_radio_conversation_count(ctx->peer) : mesh_radio_channel_message_count(ctx->channel);
}
static bool thread_message(const mesh_chat_context_t *ctx,size_t index,mesh_message_t *out) {
    return ctx->peer ? mesh_radio_get_conversation_message(ctx->peer,index,out) : mesh_radio_get_channel_message(ctx->channel,index,out);
}
bool mesh_chat_is_target_picker(mesh_chat_mode_t mode) {
    return mode==CHAT_TARGETS || mode==CHAT_REPLY_TARGETS || mode==CHAT_RETRY_TARGETS || mode==CHAT_COPY_TARGETS || mode==CHAT_DELETE_TARGETS;
}
bool mesh_chat_is_confirmation(mesh_chat_mode_t mode) {
    return mode==CHAT_RETRY_REVIEW || mode==CHAT_CLEAR_CONFIRM || mode==CHAT_COPY_REVIEW || mode==CHAT_DELETE_REVIEW || mode==CHAT_EXPORT_REVIEW;
}
mesh_chat_action_entry_t mesh_chat_action_entry(size_t index,bool replying) {
    static const mesh_chat_action_entry_t entries[MESH_CHAT_ACTION_COUNT]={
        {"INSERT EMOJI","Add an emoji to your draft"},
        {"SEND A TAPBACK","Choose a message and reaction"},
        {"RETRY FAILED MESSAGE","Review destination and text before resending"},
        {"CLEAR THIS CONVERSATION","Confirm removal; pending sends are kept"},
        {"SEARCH MESSAGES","Find text or senders in this chat or all saved messages"},
        {"REPLY TO MESSAGE","Quote a message; your draft stays separate"},
        {"REUSE MESSAGE TEXT","Copy to draft for editing; nothing sends automatically"},
        {"DELETE ONE MESSAGE","Confirm local removal; pending sends are kept"},
        {"EXPORT CONVERSATION","Save this chat as readable text on the SD card"},
        {"BROWSE SD ARCHIVE","Read or search older messages from this conversation"}
    };
    if(index>=MESH_CHAT_ACTION_COUNT)return (mesh_chat_action_entry_t){0};
    mesh_chat_action_entry_t entry=entries[index];
    if(index==CHAT_REPLY_MESSAGE&&replying)entry.name="CANCEL REPLY";
    return entry;
}
void mesh_chat_actions_open(mesh_chat_actions_t *state) {
    state->modal = CHAT_ACTIONS;
    state->action_selection = 0;
    state->feedback[0] = '\0';
}

static bool eligible_target(const mesh_message_t *message,mesh_chat_mode_t mode) {
    switch(mode) {
        case CHAT_TARGETS: return message->packet_id!=0;
        case CHAT_RETRY_TARGETS:
            return message->outgoing && message->packet_id && message->delivery==MESH_DELIVERY_FAILED;
        case CHAT_DELETE_TARGETS: return message->delivery!=MESH_DELIVERY_PENDING;
        case CHAT_COPY_TARGETS: return true;
        default: return false;
    }
}
static void collect_targets(mesh_chat_actions_t *state,const mesh_chat_context_t *ctx,mesh_chat_mode_t mode) {
    state->target_count=state->target_selection=0;
    // One snapshot path for all pickers: incoming messages cannot move a selection.
    for(size_t i=thread_count(ctx);i>0&&state->target_count<MESH_MESSAGE_HISTORY_MAX;i--) {
        mesh_message_t message;
        if(thread_message(ctx,i-1,&message)&&eligible_target(&message,mode))
            state->targets[state->target_count++]=message;
    }
    if(!state->target_count) {
        const char *reason=mode==CHAT_TARGETS?"No retained messages available for a tapback.":
            mode==CHAT_RETRY_TARGETS?"No failed outgoing messages in this chat.":
            mode==CHAT_DELETE_TARGETS?"No removable messages. Pending sends are kept.":
            "No retained messages to reuse.";
        snprintf(state->feedback,sizeof(state->feedback),"%s",reason);
        return;
    }
    if(mode==CHAT_TARGETS) {
        for(size_t i=0;i<state->target_count;i++) {
            if(!state->targets[i].outgoing){state->target_selection=i;break;}
        }
    }
    state->modal=mode;
    state->feedback[0]='\0';
}
static void open_reaction_targets(mesh_chat_actions_t *state,const mesh_chat_context_t *ctx) {
    collect_targets(state,ctx,CHAT_TARGETS);
}
static void open_retry_targets(mesh_chat_actions_t *state,const mesh_chat_context_t *ctx) {
    state->confirm_selected=false;
    collect_targets(state,ctx,CHAT_RETRY_TARGETS);
}
static void open_saved_targets(mesh_chat_actions_t *state,const mesh_chat_context_t *ctx,bool deleting) {
    collect_targets(state,ctx,deleting?CHAT_DELETE_TARGETS:CHAT_COPY_TARGETS);
}
static void accept_chat_emoji(mesh_chat_actions_t *state,const mesh_chat_context_t *ctx) {
    const mesh_picker_emoji_t *emoji = mesh_picker_get(state->emoji_selection);
    if (!emoji) return;
    if (state->picker_reaction) {
        if (!state->target_count || state->target_selection >= state->target_count ||
            !mesh_radio_send_reaction(&state->targets[state->target_selection], emoji->text)) {
            snprintf(state->feedback, sizeof(state->feedback), "Not queued. Check radio or reselect message.");
            return;
        }
        snprintf(ctx->feedback, ctx->feedback_size, "Tapback queued to radio.");
    } else {
        if (!mesh_picker_insert(ctx->draft, ctx->draft_capacity, state->emoji_selection)) {
            snprintf(state->feedback, sizeof(state->feedback), "No room: message limit is 200 bytes.");
            return;
        }
        snprintf(ctx->feedback, ctx->feedback_size, "Emoji added. Enter: send message.");
    }
    state->modal = 0;
    state->feedback[0] = '\0';
}

static mesh_chat_mode_t previous_mode(const mesh_chat_actions_t *state) {
    switch(state->modal) {
        case CHAT_EMOJI: return state->picker_reaction?CHAT_TARGETS:CHAT_ACTIONS;
        case CHAT_TARGETS:
        case CHAT_REPLY_TARGETS:
        case CHAT_RETRY_TARGETS:
        case CHAT_CLEAR_CONFIRM:
        case CHAT_COPY_TARGETS:
        case CHAT_DELETE_TARGETS:
        case CHAT_EXPORT_REVIEW:
        case CHAT_EXPORT_STATUS: return CHAT_ACTIONS;
        case CHAT_COPY_REVIEW: return CHAT_COPY_TARGETS;
        case CHAT_DELETE_REVIEW: return CHAT_DELETE_TARGETS;
        case CHAT_RETRY_REVIEW: return CHAT_RETRY_TARGETS;
        default: return CHAT_CLOSED;
    }
}

mesh_chat_input_result_t mesh_chat_actions_input(mesh_chat_actions_t *state,const mesh_chat_context_t *ctx,mesh_chat_key_t key) {
    if (!state->modal) return MESH_CHAT_IGNORED;
    if (key == MESH_CHAT_KEY_ESC) {
        state->modal=previous_mode(state);
        state->feedback[0] = '\0';
        return MESH_CHAT_HANDLED;
    }
    if (key == MESH_CHAT_KEY_RETURN) {
        if (state->modal == CHAT_ACTIONS) {
            if(state->action_selection == CHAT_REUSE_MESSAGE)open_saved_targets(state,ctx,false);
            else if(state->action_selection == CHAT_DELETE_MESSAGE)open_saved_targets(state,ctx,true);
            else if(state->action_selection == CHAT_EXPORT_THREAD){state->clear_peer=ctx->peer;state->clear_channel=ctx->channel;state->confirm_selected=false;state->modal=CHAT_EXPORT_REVIEW;}
            else if (state->action_selection == CHAT_REPLY_MESSAGE) {
                if ((*ctx->reply_id)) {
                    (*ctx->reply_id) = 0; state->modal = 0;
                    snprintf(ctx->feedback, ctx->feedback_size, "Reply cancelled. Draft kept.");
                } else {
                    open_reaction_targets(state,ctx);
                    if (state->modal == CHAT_TARGETS) state->modal = CHAT_REPLY_TARGETS;
                    else snprintf(state->feedback, sizeof(state->feedback), "No retained messages available to reply to.");
                }
            } else if (state->action_selection == CHAT_BROWSE_ARCHIVE) {state->modal=CHAT_CLOSED;return MESH_CHAT_ARCHIVE;
            } else if (state->action_selection == CHAT_SEARCH_MESSAGES) { state->modal=CHAT_CLOSED; return MESH_CHAT_SEARCH; }
            else if (state->action_selection == CHAT_SEND_TAPBACK) open_reaction_targets(state,ctx);
            else if (state->action_selection == CHAT_RETRY_MESSAGE) open_retry_targets(state,ctx);
            else if (state->action_selection == CHAT_CLEAR_THREAD) {
                state->clear_peer = ctx->peer; state->clear_channel = ctx->channel;
                state->confirm_selected = false; state->modal = CHAT_CLEAR_CONFIRM;
            } else { state->picker_reaction = false; state->emoji_selection = 0; state->modal = CHAT_EMOJI; }
        } else if(state->modal==CHAT_COPY_TARGETS||state->modal==CHAT_DELETE_TARGETS){
            if(state->target_selection<state->target_count){state->modal=state->modal==CHAT_COPY_TARGETS?CHAT_COPY_REVIEW:CHAT_DELETE_REVIEW;state->confirm_selected=false;}
        } else if(state->modal==CHAT_COPY_REVIEW){
            if(!state->confirm_selected)state->modal=CHAT_COPY_TARGETS;
            else if(state->target_selection<state->target_count){
                const char *text=state->targets[state->target_selection].text;
                size_t copied=mesh_message_reuse(ctx->draft,ctx->draft_capacity,text);
                (*ctx->reply_id)=0;(*ctx->scroll)=0;state->modal=0;
                snprintf(ctx->feedback,ctx->feedback_size,copied<strlen(text)?"Copied first %u bytes. Edit before sending.":"Copied to draft. Edit, then Enter to send.",(unsigned)copied);
            }
        } else if(state->modal==CHAT_DELETE_REVIEW){
            if(!state->confirm_selected)state->modal=CHAT_DELETE_TARGETS;
            else if(state->target_selection<state->target_count&&mesh_radio_delete_message(&state->targets[state->target_selection])){
                state->modal=0;(*ctx->scroll)=0;
                snprintf(ctx->feedback,ctx->feedback_size,"Message removed from this device.");
            }else snprintf(state->feedback,sizeof(state->feedback),"Not deleted. Message changed, is pending, or save failed.");
        } else if(state->modal==CHAT_EXPORT_REVIEW){
            if(!state->confirm_selected)state->modal=CHAT_ACTIONS;
            else{
                mesh_chat_export_result_t result=mesh_chat_export_start(state->clear_peer,state->clear_channel);
                if(result==MESH_CHAT_EXPORT_OK)state->modal=CHAT_EXPORT_STATUS;
                else snprintf(state->feedback,sizeof(state->feedback),"%s",result==MESH_CHAT_EXPORT_BUSY?"An export is still running.":result==MESH_CHAT_EXPORT_EMPTY?"No saved messages in this chat.":"Cannot start export. Check memory and retry.");
            }
        } else if(state->modal==CHAT_EXPORT_STATUS)state->modal=CHAT_ACTIONS;
        else if (state->modal == CHAT_REPLY_TARGETS && state->target_selection < state->target_count) {
            (*ctx->reply_id) = state->targets[state->target_selection].packet_id;
            state->modal = 0; (*ctx->scroll) = 0;
            snprintf(ctx->feedback, ctx->feedback_size, "Reply selected. Type, then Enter to send.");
        } else if (state->modal == CHAT_TARGETS && state->target_count) {
            state->picker_reaction = true; state->emoji_selection = 0; state->modal = CHAT_EMOJI;
        } else if (state->modal == CHAT_EMOJI) accept_chat_emoji(state,ctx);
        else if (state->modal == CHAT_RETRY_TARGETS && state->target_count) { state->modal = CHAT_RETRY_REVIEW; state->confirm_selected = false; }
        else if (state->modal == CHAT_RETRY_REVIEW) {
            if (!state->confirm_selected) state->modal = CHAT_RETRY_TARGETS;
            else if (state->target_selection < state->target_count && mesh_radio_retry_message(&state->targets[state->target_selection])) {
                state->modal = 0; (*ctx->scroll) = 0;
                snprintf(ctx->feedback, ctx->feedback_size, "Retry queued to radio.");
            } else snprintf(state->feedback, sizeof(state->feedback), "Not queued. Check radio or reselect failed message.");
        } else if (state->modal == CHAT_CLEAR_CONFIRM) {
            size_t removed;
            if (!state->confirm_selected) state->modal = CHAT_ACTIONS;
            else if (mesh_radio_clear_thread(state->clear_peer, state->clear_channel, &removed)) {
                state->modal = 0; (*ctx->scroll) = 0;
                snprintf(ctx->feedback, ctx->feedback_size, "Cleared %u; pending sends kept.", (unsigned)removed);
            } else snprintf(state->feedback, sizeof(state->feedback), "Could not save history. Nothing cleared.");
        }
        return MESH_CHAT_HANDLED;
    }
    if (key == MESH_CHAT_KEY_UP || key == MESH_CHAT_KEY_DOWN ||
        key == MESH_CHAT_KEY_LEFT || key == MESH_CHAT_KEY_RIGHT) {
        bool forward = key == MESH_CHAT_KEY_DOWN || key == MESH_CHAT_KEY_RIGHT;
        if (state->modal == CHAT_ACTIONS) {
            if (forward && state->action_selection + 1 < MESH_CHAT_ACTION_COUNT) state->action_selection++;
            else if (!forward && state->action_selection) state->action_selection--;
        } else if (mesh_chat_is_confirmation(state->modal)) state->confirm_selected = forward;
        else if (mesh_chat_is_target_picker(state->modal)) {
            if (forward && state->target_selection + 1 < state->target_count) state->target_selection++;
            if (!forward && state->target_selection) state->target_selection--;
        } else if (state->modal == CHAT_EMOJI) {
            size_t step = key == MESH_CHAT_KEY_UP || key == MESH_CHAT_KEY_DOWN ? 8 : 1;
            size_t count = mesh_picker_count();
            if (forward && state->emoji_selection + step < count) state->emoji_selection += step;
            if (!forward && state->emoji_selection >= step) state->emoji_selection -= step;
        }
        state->feedback[0] = '\0';
    }
    // While a menu is open, consume other keys rather than editing/sending.
    return MESH_CHAT_HANDLED;
}

