static void draw_map_trail_segment(int x,int y,int w,int h,const mesh_map_segment_t *s,pax_col_t color){
    double dx=s->x1-s->x0,dy=s->y1-s->y0;
    // A one-pixel dark border keeps trails visible on bright offline roads.
    for(int side=-1;side<=1;side+=2){
        double ox=fabs(dx)<fabs(dy)?side:0,oy=fabs(dx)>=fabs(dy)?side:0;
        pax_draw_line(&fb,0xFF101513,x+fmax(0,fmin(w-1,s->x0+ox)),y+fmax(0,fmin(h-1,s->y0+oy)),x+fmax(0,fmin(w-1,s->x1+ox)),y+fmax(0,fmin(h-1,s->y1+oy)));
    }
    pax_draw_line(&fb,color,x+s->x0,y+s->y0,x+s->x1,y+s->y1);
}
static mesh_trail_point_t map_trail_points[MESH_TRAIL_NODE_LIMIT];
static void draw_map_trails(int x,int y,int w,int h,time_t now){
    if(!map_trail_mode||now<1700000000||now>UINT32_MAX)return;
    // Other trails first, highlighted selection last, then current markers above.
    for(unsigned pass=0;pass<2;pass++)for(size_t i=0;i<mesh_radio_node_count();i++){
        mesh_node_t node;
        if(!mesh_radio_get_node(i,&node)||!mesh_map_node_visible(&map_filters,&node,now)||
           (node.number==map_selected_node)!=(pass==1)||(map_trail_mode==MAP_TRAILS_SELECTED&&node.number!=map_selected_node))continue;
        size_t n=mesh_trails_series(node.number,(uint32_t)now,map_trail_points,MESH_TRAIL_NODE_LIMIT);
        pax_col_t color=node.number==mesh_radio_local_node()?0xFF36B8FF:pass?UI_GREEN:0xFF788F81;
        for(size_t j=1;j<n;j++){
            mesh_trail_point_t *a=&map_trail_points[j-1],*b=&map_trail_points[j];
            if(!mesh_trails_join(a,b,(uint32_t)now))continue;
            mesh_map_segment_t segments[3];size_t count=mesh_map_segments(&map_view,a->latitude_i,a->longitude_i,b->latitude_i,b->longitude_i,w,h,segments);
            for(size_t s=0;s<count;s++)draw_map_trail_segment(x,y,w,h,&segments[s],color);
        }
    }
}

static void map_trail_summary(uint32_t node,time_t now,char *out,size_t size){
    if(!map_trail_mode){out[0]=0;return;}
    if(now<1700000000||now>UINT32_MAX){snprintf(out,size,"trail needs a valid clock");return;}
    size_t n=mesh_trails_series(node,(uint32_t)now,map_trail_points,MESH_TRAIL_NODE_LIMIT),links=0;
    for(size_t i=1;i<n;i++)if(mesh_trails_join(&map_trail_points[i-1],&map_trail_points[i],(uint32_t)now))links++;
    if(links)snprintf(out,size,"%u trail link%s",(unsigned)links,links==1?"":"s");
    else snprintf(out,size,n<2?"trail needs 2 timed fixes":"trail gaps / precision changes");
}
