#include "message_navigation.h"
#include <string.h>
static size_t thread_count(uint32_t peer,uint8_t channel){return peer?mesh_radio_conversation_count(peer):mesh_radio_channel_message_count(channel);}
static bool thread_message(uint32_t peer,uint8_t channel,size_t index,mesh_message_t *out){return peer?mesh_radio_get_conversation_message(peer,index,out):mesh_radio_get_channel_message(channel,index,out);}
static bool same_message(const mesh_message_t *a,const mesh_message_t *b){return a->peer_node==b->peer_node&&a->channel==b->channel&&a->packet_id==b->packet_id&&a->timestamp==b->timestamp&&a->outgoing==b->outgoing&&a->reply_id==b->reply_id&&!strcmp(a->from,b->from)&&!strcmp(a->text,b->text);}
void mesh_message_capture_anchor(uint32_t peer,uint8_t channel,size_t offset,mesh_message_anchor_t *anchor){
 size_t count=thread_count(peer,channel);anchor->valid=offset&&offset<count&&thread_message(peer,channel,count-1-offset,&anchor->message);
}
size_t mesh_message_open_offset(uint32_t peer,uint8_t channel,mesh_message_anchor_t *anchor){
 size_t count=thread_count(peer,channel),index=0;size_t offset=mesh_radio_first_unread(peer,channel,&index)&&index<count?count-1-index:0;
 mesh_message_capture_anchor(peer,channel,offset,anchor);return offset;
}
size_t mesh_message_resolve_offset(uint32_t peer,uint8_t channel,size_t offset,mesh_message_anchor_t *anchor){
 size_t count=thread_count(peer,channel);
 if(!offset||!count){anchor->valid=false;return 0;}
 if(anchor->valid){
  for(size_t i=0;i<count;i++){mesh_message_t message;if(thread_message(peer,channel,i,&message)&&same_message(&anchor->message,&message))return count-1-i;}
  // The viewed message was evicted/deleted: stay at the oldest retained message.
  offset=count-1;
 }else if(offset>=count)offset=count-1;
 mesh_message_capture_anchor(peer,channel,offset,anchor);return offset;
}
