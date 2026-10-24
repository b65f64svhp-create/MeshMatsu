// Pure, bounded session history. Only actual dated radio positions are recorded.
// No SD writes, location requests, interpolation, or sharing-precision changes.
static EXT_RAM_BSS_ATTR struct {uint32_t node;mesh_trail_point_t point;} trail_points[MESH_TRAIL_LIMIT];
static size_t trail_count;
void mesh_trails_reset(void){memset(trail_points,0,sizeof(trail_points));trail_count=0;}
size_t mesh_trails_count(void){return trail_count;}
void mesh_trails_forget(uint32_t node){for(size_t i=0;i<MESH_TRAIL_LIMIT;i++)if(trail_points[i].node==node&&node){trail_points[i].node=0;trail_count--;}}
static bool trail_recent(const mesh_trail_point_t *p,uint32_t now){return now>=1700000000&&p->time>=1700000000&&p->until>=p->time&&p->until<=now&&now-p->until<=MESH_TRAIL_AGE;}
bool mesh_trails_add(uint32_t node,int32_t lat,int32_t lon,uint32_t stamp,uint8_t precision,uint32_t now){
 if(!node||node==UINT32_MAX||stamp<1700000000||precision>32||lat< -900000000||lat>900000000||lon< -1800000000||lon>1800000000||
    (now>=1700000000&&((uint64_t)stamp>(uint64_t)now+300||(stamp<=now&&now-stamp>MESH_TRAIL_AGE))))return false;
 size_t free_slot=MESH_TRAIL_LIMIT,oldest=MESH_TRAIL_LIMIT,old_node=MESH_TRAIL_LIMIT,last=MESH_TRAIL_LIMIT,n=0;
 for(size_t i=0;i<MESH_TRAIL_LIMIT;i++){
  if(trail_points[i].node&&now>=1700000000){mesh_trail_point_t *p=&trail_points[i].point;
   if((p->until<=now&&now-p->until>MESH_TRAIL_AGE)||(uint64_t)p->time>(uint64_t)now+300){trail_points[i].node=0;trail_count--;}
  }
  if(!trail_points[i].node){if(free_slot==MESH_TRAIL_LIMIT)free_slot=i;continue;}
  if(oldest==MESH_TRAIL_LIMIT||trail_points[i].point.until<trail_points[oldest].point.until)oldest=i;
  if(trail_points[i].node!=node)continue;
  n++;if(old_node==MESH_TRAIL_LIMIT||trail_points[i].point.until<trail_points[old_node].point.until)old_node=i;
  if(last==MESH_TRAIL_LIMIT||trail_points[i].point.until>trail_points[last].point.until)last=i;
 }
 if(last<MESH_TRAIL_LIMIT){mesh_trail_point_t *p=&trail_points[last].point;
  if(stamp<=p->until)return false;
  if(p->latitude_i==lat&&p->longitude_i==lon&&p->precision==precision){p->until=stamp;return true;}
 }
 size_t at=n>=MESH_TRAIL_NODE_LIMIT?old_node:free_slot<MESH_TRAIL_LIMIT?free_slot:oldest;
 if(at==MESH_TRAIL_LIMIT||(trail_points[at].node&&stamp<trail_points[at].point.until))return false;
 if(!trail_points[at].node)trail_count++;
 trail_points[at].node=node;trail_points[at].point=(mesh_trail_point_t){.latitude_i=lat,.longitude_i=lon,.time=stamp,.until=stamp,.precision=precision};return true;
}
size_t mesh_trails_series(uint32_t node,uint32_t now,mesh_trail_point_t *out,size_t capacity){
 if(!node||!out)return 0;
 if(capacity>MESH_TRAIL_NODE_LIMIT)capacity=MESH_TRAIL_NODE_LIMIT;
 size_t count=0;
 for(size_t i=0;i<MESH_TRAIL_LIMIT;i++)if(trail_points[i].node==node&&trail_recent(&trail_points[i].point,now)){
  mesh_trail_point_t p=trail_points[i].point;size_t at=count;
  if(at==capacity){if(!capacity||p.time<=out[0].time)continue;memmove(out,out+1,(capacity-1)*sizeof(*out));at=capacity-1;}else count++;
  while(at&&p.time<out[at-1].time){out[at]=out[at-1];at--;}out[at]=p;
 }
 return count;
}
bool mesh_trails_join(const mesh_trail_point_t *a,const mesh_trail_point_t *b,uint32_t now){
 return a&&b&&trail_recent(a,now)&&trail_recent(b,now)&&b->time>a->until&&b->time-a->until<=MESH_TRAIL_GAP&&a->precision==b->precision;
}
