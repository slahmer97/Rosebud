#include "core.h"

#define HALF_CORE_COUNT (8/2)

struct Desc packet;

int main(void){

  int id=core_id();
  int next_core=id+HALF_CORE_COUNT;

  // Do this at the beginnig, so load balancer can fill the slots while 
  // initializing other things.
  init_hdr_slots(16, 0x804000, 128);
  init_slots(16, 0x000000, 16384);
  set_masks(0x30); // Enable only Evict + Poke

  if (id<HALF_CORE_COUNT){
    while (1){
      if (in_pkt_ready()){
        read_in_pkt(&packet);
        packet.port = (char)next_core; 
        send_to_core(&packet);
      }
    }
  } else {
    while (1){
      if (in_pkt_ready()){
        read_in_pkt(&packet);
        packet.port = 1;
        pkt_send(&packet);
      }
    }
  }
  
  return 1;
}
