static void packet_record_text(const packet_record_t *record,char *out,size_t size){
    if(!record->valid)snprintf(out,size,"No delivery yet");
    else snprintf(out,size,"%lu drops / %lu moves",(unsigned long)record->drops,(unsigned long)record->moves);
}
static void draw_packet_records(void){
    pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,20,28,84,"PACKET RUNNER RECORDS");
    char text[96];snprintf(text,sizeof(text),"Routes cleared: %u/3  |  %s",packet_scores.highest_route,packet_scores_dirty||!packet_scores_ready?"SESSION / SAVE PENDING":"SAVED ON DEVICE");
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,12,28,115,text);
    for(unsigned i=0;i<4;i++){
        int y=145+i*48;pax_draw_rect(&fb,UI_PANEL,28,y,logical_h_res-56,40);
        char label[32],value[80];if(i<3)snprintf(label,sizeof(label),"ROUTE %u",i+1);else snprintf(label,sizeof(label),"FULL 3-ROUTE RUN");
        packet_record_text(i<3?&packet_scores.route[i]:&packet_scores.run,value,sizeof(value));
        pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,13,40,y+14,label);
        float width=pax_text_size(pax_font_sky_mono,13,value).x;pax_draw_text(&fb,WHITE,pax_font_sky_mono,13,logical_h_res-40-width,y+14,value);
    }
    draw_rich_text(packet_scores_feedback,28,350,12,logical_h_res-56,20,3,UI_MUTED);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,28,logical_v_res-UI_HINT_OFFSET,"H / Enter / Esc: game  S: retry record save");
}
static void draw_packet_runner(void){
    if(packet_scores_open){draw_packet_records();return;}
    char score[128];snprintf(score,sizeof(score),"ROUTE %u/3  MOVES %u  DROPS %u  RELAYS %u/2  SHIELD %s",
        packet_game.level+1,packet_game.moves,packet_game.drops,(unsigned)((packet_game.relays&1)!=0)+(unsigned)((packet_game.relays&2)!=0),packet_game.shield?"ON":"--");
    pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,12,28,78,score);
    char best[72];packet_record_text(&packet_scores.route[packet_game.level<3?packet_game.level:0],best,sizeof(best));
    snprintf(score,sizeof(score),"BEST ROUTE: %s%s | CHASER: %s",best,packet_scores_dirty?" (pending)":"",(packet_game.turn+1)%2?"waits next":"hunts next");
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,28,97,score);
    const int cell=38,left=((int)logical_h_res-PACKET_COLS*cell)/2,top=110;
    for(int y=0;y<PACKET_ROWS;y++)for(int x=0;x<PACKET_COLS;x++){
        int px=left+x*cell,py=top+y*cell;char tile=packet_runner_tile(packet_game.level,x,y);
        pax_draw_rect(&fb,UI_PANEL,px+1,py+1,cell-2,cell-2);
        if(tile=='#')pax_draw_tri(&fb,UI_BORDER,px+5,py+31,px+19,py+6,px+33,py+31);
        else if(tile=='!')pax_draw_text(&fb,KEY_X_COLOR,pax_font_sky_mono,24,px+11,py+7,"X");
        else if(tile=='R'){
            bool collected=packet_game.relays&packet_runner_relay_bit(packet_game.level,x,y);
            pax_outline_circle(&fb,collected?UI_GREEN:KEY_TRIANGLE_COLOR,px+19,py+19,12);
            pax_draw_text(&fb,collected?UI_GREEN:KEY_TRIANGLE_COLOR,pax_font_sky_mono,14,px+14,py+12,collected?"+":"R");
        }else if(tile=='O'&&!(packet_game.shields_taken&packet_runner_shield_bit(packet_game.level,x,y))){
            const pax_col_t blue=0xFF36B8FF;
            pax_draw_line(&fb,blue,px+8,py+8,px+30,py+8);pax_draw_line(&fb,blue,px+8,py+8,px+10,py+24);pax_draw_line(&fb,blue,px+10,py+24,px+19,py+32);pax_draw_line(&fb,blue,px+19,py+32,px+28,py+24);pax_draw_line(&fb,blue,px+28,py+24,px+30,py+8);
            pax_draw_text(&fb,blue,pax_font_sky_mono,16,px+14,py+11,"+");
        }else if(tile=='D'||tile=='S')pax_draw_text(&fb,tile=='D'?UI_GREEN:UI_MUTED,pax_font_sky_mono,19,px+12,py+9,tile=='D'?"D":"S");
    }
    for(unsigned i=0;i<PACKET_ENEMIES;i++){
        int x,y;if(!packet_runner_enemy(&packet_game,i,&x,&y))continue;int ex=left+x*cell,ey=top+y*cell;
        if(i==PACKET_CHASER){
            pax_draw_tri(&fb,KEY_TRIANGLE_COLOR,ex+19,ey+5,ex+33,ey+19,ex+5,ey+19);pax_draw_tri(&fb,KEY_TRIANGLE_COLOR,ex+5,ey+19,ex+33,ey+19,ex+19,ey+33);
        }else{pax_draw_rect(&fb,KEY_X_COLOR,ex+8,ey+9,22,20);pax_draw_line(&fb,KEY_X_COLOR,ex+19,ey+4,ex+19,ey+9);}
        pax_draw_rect(&fb,BLACK,ex+12,ey+14,4,4);pax_draw_rect(&fb,BLACK,ex+23,ey+14,4,4);
    }
    int px=left+packet_game.x*cell,py=top+packet_game.y*cell;
    if(packet_game.shield)pax_outline_circle(&fb,0xFF36B8FF,px+19,py+19,16);
    pax_draw_rect(&fb,UI_GREEN,px+9,py+10,20,18);pax_draw_line(&fb,UI_PANEL,px+9,py+10,px+19,py+20);pax_draw_line(&fb,UI_PANEL,px+29,py+10,px+19,py+20);
    const char *status=packet_game.delivered?
        (packet_new_best?"NEW BEST! Delivered. Enter: next route / new run":packet_game.level+1==PACKET_LEVELS?"MESH MASTER! All routes delivered. Enter: play again":"DELIVERED TO MESH! Enter: next route"):
        packet_game.shielded?"Shield absorbed the hit! Move away from danger.":
        packet_game.dropped?(packet_game.intercepted?"A jammer caught your packet. Route reset.":"Interference ate your packet. Route reset."):
        packet_game.picked_shield?"Shield ready: absorbs one collision. Find the relays!":
        packet_game.retried?"Route restarted. Moves kept; an active retry counts as a drop.":
        packet_game.bumped?"Mountains block the signal. Find a way around.":
        packet_runner_tile(packet_game.level,packet_game.x,packet_game.y)=='D'?"Collect both relay nodes before delivery.":
        "R relays -> D. Blue shield: one hit. Amber chaser: every 2 turns.";
    pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,11,28,389,status);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,28,logical_v_res-UI_HINT_OFFSET,"Arrows:move  Space:wait  R:retry  H:records  Esc:About");
}
