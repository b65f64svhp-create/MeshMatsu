static void draw_chat_confirmation(const char *accept_label) {
    int width=((int)logical_h_res-52)/2;
    for(unsigned i=0;i<2;i++) {
        int x=20+i*(width+12);
        bool selected=chat_actions.confirm_selected==(i!=0);
        pax_draw_rect(&fb,selected?UI_GREEN:UI_PANEL,x,350,width,36);
        pax_draw_text(&fb,selected?BLACK:UI_GREEN,pax_font_sky_mono,13,x+12,361,i?accept_label:"CANCEL");
    }
}
// Drawing stays on the app task; menu behavior lives in chat_actions.c.
static void draw_chat_actions(void) {
    if (chat_actions.modal == CHAT_ACTIONS) {
        size_t first = chat_actions.action_selection / 6 * 6;
        char page[32];snprintf(page,sizeof(page),"%u-%u / %u",(unsigned)first+1,(unsigned)(first+6<MESH_CHAT_ACTION_COUNT?first+6:MESH_CHAT_ACTION_COUNT),(unsigned)MESH_CHAT_ACTION_COUNT);
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,logical_h_res-120,70,page);
        for (size_t i = first; i < MESH_CHAT_ACTION_COUNT && i < first + 6; i++) {
            mesh_chat_action_entry_t entry=mesh_chat_action_entry(i,draft_reply_ids[draft_slot]!=0);
            int y = 84 + (i-first) * 51;
            pax_draw_rect(&fb, chat_actions.action_selection == i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 47);
            pax_draw_rect(&fb, chat_actions.action_selection == i ? UI_GREEN : UI_BORDER, 20, y, 3, 47);
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 13, 34, y + 7, entry.name);
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 34, y + 28, entry.detail);
        }
    } else if (mesh_chat_is_target_picker(chat_actions.modal)) {
        size_t first = chat_actions.target_selection / 4 * 4;
        for (size_t i = first; i < chat_actions.target_count && i < first + 4; i++) {
            const mesh_message_t *message = &chat_actions.targets[i];
            int y = 92 + (i - first) * 74;
            pax_draw_rect(&fb, chat_actions.target_selection == i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 66);
            pax_draw_rect(&fb, chat_actions.target_selection == i ? UI_GREEN : UI_BORDER, 20, y, 3, 66);
            draw_message_header(message, y + 7, i + 1);
            draw_rich_text(message->text, 32, y + 28, 13, logical_h_res - 64, 22, 1, WHITE);
        }
        char range[48]; snprintf(range, sizeof(range), "%u / %u messages", (unsigned)(chat_actions.target_selection + 1), (unsigned)chat_actions.target_count);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 390, range);
    } else if (chat_actions.modal == CHAT_EMOJI) {
        pax_draw_rect(&fb, UI_PANEL, 20, 84, logical_h_res - 40, 54);
        if (chat_actions.picker_reaction && chat_actions.target_selection < chat_actions.target_count) {
            const mesh_message_t *target = &chat_actions.targets[chat_actions.target_selection];
            char name[40], label[64];
            mesh_radio_message_sender_name(target, name, sizeof(name));
            snprintf(label, sizeof(label), "Reacting to %s:", name);
            draw_rich_text(label, 32, 90, 11, logical_h_res - 64, 16, 1, UI_MUTED);
            draw_rich_text(target->text, 32, 108, 12, logical_h_res - 64, 22, 1, WHITE);
        } else {
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 32, 92, "Add to your draft; send when ready.");
            draw_rich_text(message_draft[0] ? message_draft : "Empty draft", 32, 110, 12, logical_h_res - 64, 22, 1, WHITE);
        }
        int cell_w = ((int)logical_h_res - 40) / 8;
        for (size_t i = 0; i < mesh_picker_count(); i++) {
            int x = 20 + (i % 8) * cell_w, y = 150 + (i / 8) * 52;
            bool selected = i == chat_actions.emoji_selection;
            pax_draw_rect(&fb, selected ? UI_SELECTED : UI_PANEL, x + 2, y + 2, cell_w - 4, 48);
            if (selected) pax_outline_rect(&fb, UI_GREEN, x + 2, y + 2, cell_w - 4, 48);
            const mesh_picker_emoji_t *emoji = mesh_picker_get(i);
            draw_rich_text(emoji->text, x + (cell_w - MESH_EMOJI_SIZE) / 2, y + 16, 14, MESH_EMOJI_SIZE + 2, 22, 1, WHITE);
        }
        const mesh_picker_emoji_t *selected = mesh_picker_get(chat_actions.emoji_selection);
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 20, 372, selected ? selected->name : "Choose emoji");
    }
    if (chat_actions.modal == CHAT_RETRY_REVIEW || chat_actions.modal == CHAT_CLEAR_CONFIRM) {
        char destination[80];
        uint32_t peer = chat_actions.modal == CHAT_CLEAR_CONFIRM ? chat_actions.clear_peer : chat_actions.targets[chat_actions.target_selection].peer_node;
        uint8_t channel = chat_actions.modal == CHAT_CLEAR_CONFIRM ? chat_actions.clear_channel : chat_actions.targets[chat_actions.target_selection].channel;
        if (peer) { char name[40]; peer_name(peer, name, sizeof(name)); snprintf(destination, sizeof(destination), "To: %.28s / !%08lx", name, (unsigned long)peer); }
        else { mesh_channel_t info; bool loaded = mesh_radio_get_channel(channel, &info); snprintf(destination, sizeof(destination), "Channel %u: %s", channel, loaded && info.name[0] ? info.name : "channel chat"); }
        draw_rich_text(destination, 20, 90, 12, logical_h_res - 40, 20, 2, UI_GREEN);
        if (chat_actions.modal == CHAT_RETRY_REVIEW) {
            pax_draw_rect(&fb, UI_PANEL, 20, 142, logical_h_res - 40, 132);
            draw_rich_text(chat_actions.targets[chat_actions.target_selection].text, 32, 152, 13, logical_h_res - 64, 22, 5, WHITE);
            draw_rich_text(mesh_radio_delivery_text(&chat_actions.targets[chat_actions.target_selection]), 20, 281, 11, logical_h_res-40, 18, 1, KEY_TRIANGLE_COLOR);
            draw_rich_text("One new send cycle. A lost ACK can mean the message already arrived; retrying may duplicate it.", 20, 307, 10, logical_h_res - 40, 16, 2, UI_MUTED);
        } else {
            draw_rich_text("Clear recent messages and their tapbacks from this conversation? SD archive is separate. Other chats and your draft stay intact. Messages awaiting confirmation are kept.", 20, 150, 14, logical_h_res - 40, 24, 6, WHITE);
        }
        draw_chat_confirmation(chat_actions.modal == CHAT_RETRY_REVIEW ? "SEND AGAIN" : "CLEAR CHAT");
    }
    if(chat_actions.modal==CHAT_COPY_REVIEW||chat_actions.modal==CHAT_DELETE_REVIEW||chat_actions.modal==CHAT_EXPORT_REVIEW){
        bool copying=chat_actions.modal==CHAT_COPY_REVIEW, exporting=chat_actions.modal==CHAT_EXPORT_REVIEW;
        if(!exporting&&chat_actions.target_selection<chat_actions.target_count){
            const mesh_message_t *m=&chat_actions.targets[chat_actions.target_selection];
            pax_draw_rect(&fb,UI_PANEL,20,88,logical_h_res-40,142);
            draw_message_header(m,96,0);draw_rich_text(m->text,32,124,13,logical_h_res-64,22,4,WHITE);
        }
        const char *explanation=exporting?"Save all currently retained messages from this chat to a new text file on SD. The file contains private message text without encryption. Open it on your computer with TextEdit.":copying?"Replace your current draft with this text? The reply link is cleared. Text over 200 bytes is shortened at a whole character. Review and edit before sending.":"Delete this recent message and its tapbacks? SD archive is separate. It stays on other devices. Other messages and your draft are kept.";
        draw_rich_text(explanation,20,exporting?112:244,13,logical_h_res-40,21,exporting?8:4,WHITE);
        draw_chat_confirmation(exporting?"EXPORT TEXT":copying?"REPLACE DRAFT":"DELETE MESSAGE");
    }else if(chat_actions.modal==CHAT_EXPORT_STATUS){
        mesh_chat_export_status_t status=mesh_chat_export_status();
        draw_rich_text(status.message,20,110,15,logical_h_res-40,26,7,status.success?UI_GREEN:WHITE);
        draw_rich_text(status.busy?"Messaging continues while the file is written.":"SD card: meshmatsu/exports. Open the .txt file with TextEdit on your Mac.",20,300,13,logical_h_res-40,22,3,UI_MUTED);
    }
    if (chat_actions.feedback[0]) pax_draw_text(&fb, KEY_TRIANGLE_COLOR, pax_font_sky_mono, 10, 20, 400, chat_actions.feedback);
    const char *hint = chat_actions.modal == CHAT_ACTIONS ? "Up/Down: choose  Enter: open  Esc: chat" :
                       mesh_chat_is_target_picker(chat_actions.modal) ? "Up/Down: message  Enter: review  Esc: back" :
                       mesh_chat_is_confirmation(chat_actions.modal) ? "L/R: Cancel or confirm  Enter: choose  Esc: back" :
                       chat_actions.modal==CHAT_EXPORT_STATUS?"Enter/Esc: actions; export keeps running":
                       chat_actions.picker_reaction ? "Arrows: emoji  Enter: queue tapback  Esc: back" :
                       "Arrows: emoji  Enter: add to draft  Esc: back";
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET, hint);
}

