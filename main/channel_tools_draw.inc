static void draw_channel_tools(void) {
    mesh_channel_t current={0};mesh_radio_get_channel(ct_slot,&current);
    char title[100];snprintf(title,sizeof(title),"Slot %u: %s / %s",ct_slot,current.name[0]?current.name:"(preset name)",channel_role(current.role));
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,12,20,67,title);
    if(ct_mode==CT_MENU){
        const char *names[]={current.loaded&&!current.role?"SHARE QR (SLOT DISABLED)":"SHARE QR / CHANNEL LINK","IMPORT CHANNEL LINK","REPLACE CHANNEL KEY","GENERATE NEW 256-BIT KEY","CHANGE CHANNEL ROLE","DISABLE CHANNEL (KEEP KEY)","REMOVE CHANNEL (ERASE KEY)"};
        for(unsigned i=0;i<7;i++){int y=88+i*42;pax_draw_rect(&fb,ct_selection==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,37);pax_draw_rect(&fb,ct_selection==i?UI_GREEN:UI_BORDER,20,y,3,37);pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,13,34,y+11,names[i]);}
    }else if(ct_mode==CT_QR){
        ct_refresh_share();
        if(ct_qr_valid){int size=qrcodegen_getSize(ct_qr),scale=300/(size+8),side=(size+8)*scale,x=20,y=92;
            pax_draw_rect(&fb,0xffffffff,x,y,side,side);for(int row=0;row<size;row++)for(int col=0;col<size;col++)if(qrcodegen_getModule(ct_qr,col,row))pax_draw_rect(&fb,0xff000000,x+(col+4)*scale,y+(row+4)*scale,scale,scale);
            draw_rich_text("Scan with a phone to add this channel. The QR contains its key.",345,96,14,logical_h_res-365,24,4,UI_GREEN);
            draw_rich_text("LoRa settings are excluded. Both radios need the same modem preset and frequency.",345,208,12,logical_h_res-365,22,4,UI_MUTED);
            draw_rich_text(ct_url,345,306,9,logical_h_res-365,15,6,WHITE);
        }else draw_rich_text(ct_share_status,20,100,15,logical_h_res-40,24,5,KEY_TRIANGLE_COLOR);
    }else if(ct_mode==CT_LINK){
        draw_rich_text("Enter the full Meshtastic channel link. Enter opens a review; it never imports immediately.",20,96,14,logical_h_res-40,24,3,UI_GREEN);
        pax_draw_rect(&fb,UI_SELECTED,20,180,logical_h_res-40,190);
        size_t n=strlen(ct_url);const char *visible=n>650?ct_url+n-650:ct_url;
        draw_rich_text(visible[0]?visible:"https://meshtastic.org/e/#...",32,190,11,logical_h_res-64,19,8,WHITE);
    }else if(ct_mode==CT_KEY){
        draw_rich_text("Enter an existing channel key or use the generated key below. Other radios must adopt the same key after you change it.",20,96,14,logical_h_res-40,24,4,UI_GREEN);
        pax_draw_rect(&fb,UI_SELECTED,20,214,logical_h_res-40,90);
        draw_rich_text(ct_show_key?ct_key:ct_key[0]?"Key entered (hidden)":"Type Base64 or a 16/32-byte hexadecimal key",34,232,13,logical_h_res-68,22,3,WHITE);
        draw_rich_text("Space: show/hide key. Enter: review. Nothing is saved until confirmed.",20,330,12,logical_h_res-40,22,3,UI_MUTED);
    }else if(ct_mode==CT_ROLE){
        pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,20,20,108,channel_role(ct_role));
        draw_rich_text("Primary controls default broadcasts and can change the radio frequency. Choosing Primary demotes the previous primary. Secondary shares that frequency. Disabled keeps its saved key.",20,162,14,logical_h_res-40,26,6,WHITE);
        draw_rich_text("Keep active slots consecutive. To disable/remove, start with the last secondary. Promote another channel before disabling the current primary.",20,334,11,logical_h_res-40,20,3,UI_MUTED);
    }else if(ct_mode==CT_IMPORT){
        mesh_channel_t offer={0};mesh_radio_share_channel_info(&ct_share,ct_entry,&offer);
        char text[160];const char *labels[]={"CHANNEL WITHIN LINK","DESTINATION SLOT","REVIEW IMPORT"};
        for(unsigned i=0;i<3;i++){int y=96+i*65;pax_draw_rect(&fb,ct_field==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,58);pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,11,34,y+6,labels[i]);
            if(!i)snprintf(text,sizeof(text),"%u / %u: %s (%s)",ct_entry+1,ct_share.count,offer.name[0]?offer.name:"preset name",offer.default_key?"default key":offer.has_key?"custom key":"radio default/inherited key");
            else if(i==1)snprintf(text,sizeof(text),"Slot %u: %s",ct_slot,current.role?"REPLACES existing channel":"unused");else snprintf(text,sizeof(text),"Enter to review this channel only");
            pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,14,34,y+29,text);}
        draw_rich_text(ct_share.has_lora?"This link also contains radio settings. MeshMatsu imports only the selected channel; LoRa settings stay unchanged.":"One selected channel is imported. Other channels and radio settings stay unchanged.",20,310,12,logical_h_res-40,22,4,UI_MUTED);
    }else if(ct_mode==CT_CONFIRM){
        const char *warning=ct_action==CT_CHANGE_KEY?"Replace this channel's encryption key? Radios with the old key will no longer exchange messages on it.":
            ct_action==CT_CHANGE_ROLE?"Apply the selected channel role? Making it primary may retune the radio and demotes the current primary.":
            ct_action==CT_DISABLE?"Disable this channel but keep its settings and key? Its saved chats remain on this device.":
            ct_action==CT_ERASE?"Remove this channel and erase its key on the radio? Saved chats remain tied to this slot. Other radios keep their copies.":
            current.role?"Replace the selected destination channel with the channel from this link? Its old key and settings will be replaced. Saved chats remain tied to this slot.":"Add the selected channel from this link? The radio's LoRa settings will stay unchanged.";
        draw_rich_text(warning,20,104,15,logical_h_res-40,27,6,WHITE);
        if(ct_action==CT_IMPORT_CHANNEL){mesh_channel_t c={0};mesh_radio_share_channel_info(&ct_share,ct_entry,&c);char text[96];snprintf(text,sizeof(text),"Import %s into slot %u",c.name[0]?c.name:"(preset name)",ct_slot);draw_rich_text(text,20,276,13,logical_h_res-40,22,2,UI_GREEN);}
        if(ct_action==CT_CHANGE_ROLE){char text[64];snprintf(text,sizeof(text),"New role: %s",channel_role(ct_role));pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,14,20,278,text);}
        for(unsigned i=0;i<2;i++){int width=((int)logical_h_res-52)/2,x=20+i*(width+12);bool selected=ct_confirm==(i!=0);pax_draw_rect(&fb,selected?UI_GREEN:UI_PANEL,x,350,width,36);pax_draw_text(&fb,selected?BLACK:UI_GREEN,pax_font_sky_mono,13,x+12,361,i?"CONFIRM CHANGE":"CANCEL");}
    }else if(ct_mode==CT_WAIT){
        mesh_channel_operation_t op=mesh_radio_channel_operation();const char *status=op.state==MESH_FAVORITE_SAVING?"Saving; waiting for fresh radio readback...":op.state==MESH_FAVORITE_SAVED?"Channel change confirmed by radio.":op.state==MESH_FAVORITE_TIMEOUT?"Not confirmed: timed out. Check current radio values.":op.state==MESH_FAVORITE_INTERRUPTED?"Not confirmed: radio identity changed.":"Radio did not confirm this change. Reopen to check.";
        draw_rich_text(status,20,110,17,logical_h_res-40,28,5,op.state==MESH_FAVORITE_SAVED?UI_GREEN:KEY_TRIANGLE_COLOR);
        draw_rich_text("The change is sent once. A radio reboot may be expected; MeshMatsu checks readback after reconnection.",20,270,13,logical_h_res-40,24,4,UI_MUTED);
    }
    if(ct_feedback[0])draw_rich_text(ct_feedback,20,395,10,logical_h_res-40,18,1,KEY_TRIANGLE_COLOR);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,
        ct_mode==CT_IMPORT?"Up/Down: field  L/R: value  Enter: next/review  Esc: back":ct_mode==CT_CONFIRM?"Left/Right: Cancel or Confirm  Enter: choose  Esc: back":"Up/Down: choose  Enter: open/review  Esc: back");
}
