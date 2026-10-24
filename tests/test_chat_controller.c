#define main old_radio_tests
#include "test_direct_messages.c"
#undef main
#include "emoji_picker.c"
#define MESH_EXPORT_ROOT "controller-export"
#include "message_export.c"
#include "chat_actions.c"
static mesh_chat_actions_t channel_actions,direct_actions;
int main(void) {
    reset();
    assert(store_message("!11223344",(const uint8_t*)"channel saved",13,1800000000,0,false,1));messages[0].packet_id=10;
    assert(store_message("!55667788",(const uint8_t*)"direct saved",12,1800000001,42,false,0));messages[1].packet_id=11;
    assert(store_message("!11223344",(const uint8_t*)"legacy no id",12,1800000002,0,false,1));
    char draft[201]="channel draft",dm_draft[201]="direct draft",feedback[80],dm_feedback[80];
    uint32_t reply=77,dm_reply=0;size_t scroll=4,dm_scroll=2;
    mesh_chat_context_t channel={.channel=1,.draft=draft,.draft_capacity=sizeof(draft),.feedback=feedback,.feedback_size=sizeof(feedback),.reply_id=&reply,.scroll=&scroll};
    mesh_chat_context_t direct={.peer=42,.draft=dm_draft,.draft_capacity=sizeof(dm_draft),.feedback=dm_feedback,.feedback_size=sizeof(dm_feedback),.reply_id=&dm_reply,.scroll=&dm_scroll};
    size_t writes=write_count;
    assert(mesh_chat_actions_input(&channel_actions,&channel,MESH_CHAT_KEY_RETURN)==MESH_CHAT_IGNORED);
    for(unsigned i=0;i<MESH_CHAT_ACTION_COUNT;i++){mesh_chat_action_entry_t entry=mesh_chat_action_entry(i,false);assert(entry.name&&entry.detail);}
    assert(!mesh_chat_action_entry(MESH_CHAT_ACTION_COUNT,false).name);
    assert(!strcmp(mesh_chat_action_entry(CHAT_REPLY_MESSAGE,true).name,"CANCEL REPLY"));
    mesh_chat_actions_open(&channel_actions);channel_actions.action_selection=CHAT_REUSE_MESSAGE;
    mesh_chat_actions_input(&channel_actions,&channel,MESH_CHAT_KEY_RETURN);
    assert(channel_actions.modal==CHAT_COPY_TARGETS&&channel_actions.target_count==2&&channel_actions.targets[0].packet_id==0);
    mesh_chat_actions_input(&channel_actions,&channel,MESH_CHAT_KEY_RETURN);assert(!channel_actions.confirm_selected&&mesh_chat_is_confirmation(channel_actions.modal));
    // A second instance uses its own snapshot and cannot change the channel draft/selection.
    mesh_chat_actions_open(&direct_actions);direct_actions.action_selection=CHAT_REUSE_MESSAGE;
    mesh_chat_actions_input(&direct_actions,&direct,MESH_CHAT_KEY_RETURN);assert(direct_actions.target_count==1&&direct_actions.targets[0].peer_node==42);
    assert(channel_actions.modal==CHAT_COPY_REVIEW&&reply==77&&!strcmp(draft,"channel draft"));
    mesh_chat_actions_input(&direct_actions,&direct,MESH_CHAT_KEY_RETURN);mesh_chat_actions_input(&direct_actions,&direct,MESH_CHAT_KEY_RIGHT);mesh_chat_actions_input(&direct_actions,&direct,MESH_CHAT_KEY_RETURN);
    assert(!strcmp(dm_draft,"direct saved")&&!direct_actions.modal&&dm_scroll==0&&reply==77&&!strcmp(draft,"channel draft")&&write_count==writes);
    mesh_chat_actions_input(&channel_actions,&channel,MESH_CHAT_KEY_RETURN);assert(channel_actions.modal==CHAT_COPY_TARGETS&&!strcmp(draft,"channel draft")&&reply==77);
    // Search requests navigation without touching either draft or sending.
    mesh_chat_actions_open(&channel_actions);channel_actions.action_selection=CHAT_SEARCH_MESSAGES;
    assert(mesh_chat_actions_input(&channel_actions,&channel,MESH_CHAT_KEY_RETURN)==MESH_CHAT_SEARCH&&!channel_actions.modal&&!strcmp(draft,"channel draft")&&reply==77&&write_count==writes);
    // Reply/tapback targets require an ID, while copy/delete can include legacy text.
    mesh_chat_actions_open(&channel_actions);channel_actions.action_selection=CHAT_SEND_TAPBACK;
    mesh_chat_actions_input(&channel_actions,&channel,MESH_CHAT_KEY_RETURN);assert(channel_actions.target_count==1&&channel_actions.targets[0].packet_id==10&&mesh_chat_is_target_picker(channel_actions.modal));
    assert(mesh_chat_actions_input(&channel_actions,&channel,MESH_CHAT_KEY_OTHER)==MESH_CHAT_HANDLED&&write_count==writes);
    mesh_chat_actions_input(&channel_actions,&channel,MESH_CHAT_KEY_ESC);assert(channel_actions.modal==CHAT_ACTIONS);
    mesh_chat_actions_input(&channel_actions,&channel,MESH_CHAT_KEY_ESC);assert(!channel_actions.modal);
    puts("PASS: production controller without main.c slicing, independent chat snapshots/drafts, cancel defaults, legacy targets, search handoff and no implicit transmission");
}
