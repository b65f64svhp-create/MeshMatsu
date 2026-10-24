static bool node_nickname_key(char key){
    if(current_screen!=SCREEN_NODES||node_modal!=NODE_NICKNAME||key=='\r'||key=='\n')return false;
    if(key=='\b'||key==127){if(nickname_replace){nickname_draft[0]=0;nickname_replace=false;}else mesh_compose_backspace(nickname_draft);}
    else if((unsigned char)key>=32&&key!=127){if(nickname_replace){nickname_draft[0]=0;nickname_replace=false;}size_t n=strlen(nickname_draft);if(n<39){nickname_draft[n]=key;nickname_draft[n+1]=0;}}
    return true;
}
