#include "role_options.h"
static const struct { uint32_t value;const char *name,*description; } roles[]={
 {0,"Client","Normal handheld messaging. Relays packets when useful to the mesh."},
 {1,"Client Mute","Sends and receives your messages. Does not forward other nodes' packets."},
 {8,"Client Hidden","Reduces routine broadcasts. By default, forwards only traffic on your own channels."},
 {12,"Client Base","Base station: always relays traffic to or from favorites. Other traffic uses Client behavior."},
 {2,"Router","Infrastructure: always relays early. Intended for well-placed fixed sites; may change radio power and client links."},
 {11,"Router Late","Infrastructure: always relays after other nodes. Useful for fixed sites filling coverage gaps."},
 {5,"Tracker","Prioritizes position updates; needs a position source. Forwards while awake; power-saving sleep can pause reception."},
 {6,"Sensor","Prioritizes telemetry; needs configured sensors. Forwards while awake; power-saving sleep can pause reception."},
 {7,"TAK","For a TAK/ATAK connection. Reduces routine broadcasts. Also forwards mesh traffic."},
 {10,"TAK Tracker","Automatic TAK position updates; needs compatible position setup. Forwards other traffic unless rebroadcast is disabled."},
 {9,"Lost and Found","Sends recovery locations to the primary channel; needs a position source. Also forwards mesh traffic."}
};
size_t mesh_role_count(void){return sizeof(roles)/sizeof(roles[0]);}
uint32_t mesh_role_at(size_t index){return roles[index<mesh_role_count()?index:0].value;}
const char *mesh_role_name(uint32_t role){for(size_t i=0;i<mesh_role_count();i++)if(roles[i].value==role)return roles[i].name;return "Legacy / unknown role";}
const char *mesh_role_description(uint32_t role){for(size_t i=0;i<mesh_role_count();i++)if(roles[i].value==role)return roles[i].description;return "This reported role is not offered for new selections. Choose a supported role to change it.";}
