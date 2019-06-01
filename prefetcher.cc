#include "sim.h"

void Cache::init_prefetcher(){
}

uint64_t next_line(uint64_t addr){
//	return (((addr) >> 6) + 1) << 6;
}

void Cache::prefetch(uint64_t addr){
/*	uint64_t next_addr = next_line(addr);
	Packet ne(0, addr, 1 << 1, 3);

			

	add_to_fill(ne);
		
	if(next_addr != 0){
		uint32_t set = get_set(next_addr);
		uint8_t vic_way = find_victim(set, next_addr);	



		update_replacement(vic_way, set, next_addr);
		operate_replacement(vic_way, set, next_addr);
	}
*/	
}
