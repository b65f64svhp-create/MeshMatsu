// Hidden turn-based game. Radio polling, alerts and power management stay active.
static packet_runner_t packet_game;
static unsigned packet_secret_progress;
static packet_scores_t packet_scores;
static bool packet_scores_ready,packet_scores_dirty,packet_scores_open,packet_new_best;
static char packet_scores_feedback[112];
static void packet_scores_sync(void){
    if(!packet_scores_ready){
        packet_scores_t saved;esp_err_t result=packet_scores_load(&saved);
        if(result!=ESP_OK){snprintf(packet_scores_feedback,sizeof(packet_scores_feedback),"Cannot read saved records. Session records kept; S retries.");return;}
        packet_scores_merge(&packet_scores,&saved);packet_scores_ready=true;
    }
    if(packet_scores_dirty){
        if(packet_scores_save(&packet_scores)!=ESP_OK){snprintf(packet_scores_feedback,sizeof(packet_scores_feedback),"Records not saved. Kept in memory; S retries.");return;}
        packet_scores_dirty=false;
    }
    snprintf(packet_scores_feedback,sizeof(packet_scores_feedback),"Records saved on this device. Fewer drops wins; moves break ties.");
}
static void packet_move(int dx,int dy){
    bool delivered=packet_game.delivered;uint8_t relays=packet_game.relays;
    packet_runner_move(&packet_game,dx,dy);
    if(!packet_game.advanced)return;
    packet_new_best=false;
    if(packet_game.delivered&&!delivered){
        bool changed=packet_scores_record(&packet_scores,&packet_game);packet_new_best=changed&&packet_scores_ready;
        packet_scores_dirty|=changed;if(changed)packet_scores_sync();
        mesh_sound_play(MESH_SOUND_GAME_DELIVERED);
    }else if(packet_game.dropped||packet_game.shielded)mesh_sound_play(MESH_SOUND_GAME_DROP);
    else if(packet_game.picked_shield||packet_game.relays!=relays)mesh_sound_play(MESH_SOUND_GAME_RELAY);
}
static bool packet_navigation(int key){
    if(current_screen!=SCREEN_PACKET_RUNNER)return false;
    if(packet_scores_open){
        if(key==BSP_INPUT_NAVIGATION_KEY_ESC||key==BSP_INPUT_NAVIGATION_KEY_RETURN){packet_scores_open=false;return true;}
        return key==BSP_INPUT_NAVIGATION_KEY_LEFT||key==BSP_INPUT_NAVIGATION_KEY_RIGHT||key==BSP_INPUT_NAVIGATION_KEY_UP||key==BSP_INPUT_NAVIGATION_KEY_DOWN;
    }
    if(key==BSP_INPUT_NAVIGATION_KEY_LEFT)packet_move(-1,0);
    else if(key==BSP_INPUT_NAVIGATION_KEY_RIGHT)packet_move(1,0);
    else if(key==BSP_INPUT_NAVIGATION_KEY_UP)packet_move(0,-1);
    else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN)packet_move(0,1);
    else if(key==BSP_INPUT_NAVIGATION_KEY_RETURN){packet_runner_next(&packet_game);packet_new_best=false;}
    else return false;
    return true;
}
static bool packet_key(char key){
    if(current_screen==SCREEN_ABOUT){
        if(packet_runner_secret(&packet_secret_progress,key)){
            packet_runner_reset(&packet_game);packet_scores_open=packet_new_best=false;
            if(!packet_scores_ready)packet_scores_sync();
            current_screen=SCREEN_PACKET_RUNNER;return true;
        }
        return false;
    }
    if(current_screen!=SCREEN_PACKET_RUNNER){packet_secret_progress=0;return false;}
    if(key=='h'||key=='H'){packet_scores_open=!packet_scores_open;if(packet_scores_open&&(!packet_scores_ready||packet_scores_dirty))packet_scores_sync();}
    else if(packet_scores_open){
        if(key=='s'||key=='S')packet_scores_sync();
        else if(key=='\r'||key=='\n')packet_scores_open=false;
    }
    else if(key=='\r'||key=='\n'){packet_runner_next(&packet_game);packet_new_best=false;}
    else if(key=='r'||key=='R'){packet_runner_retry(&packet_game);packet_new_best=false;}
    else if(key==' ')packet_move(0,0);
    return true;
}
#include "packet_runner_draw.inc"
