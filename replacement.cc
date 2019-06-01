#include "sim.h"

uint32_t Cache::find_victim(uint32_t set, uint64_t addr){
	return lru_victim(set);
}

void Cache::update_replacement(uint8_t way, uint32_t set, uint64_t addr){
	//BOOKMARK
	update_lru(way, set, addr);
}

void Cache::operate_replacement(uint8_t victim, 
								uint32_t set, uint64_t addr){

	if(level != (1 << 3) && cache_lines[set][victim].valid){
		Packet evict( 0, cache_lines[set][victim].addr, 0, latency); 
		if(lower_cache -> add_to_fill(evict) == -1){
			evictions++;	
			assert(false);
		}
	}

	evictions++;

	cache_lines[set][victim].addr = addr;
	cache_lines[set][victim].valid = 1;
	cache_lines[set][victim].dirty = 0;
	cache_lines[set][victim].prefetch = 0;

	return;
}


void Cache::update_lru(uint8_t way, uint32_t set, uint64_t addr){


	for (uint32_t i = 0; i < assoc; i++) {
		
		if(cache_lines[set][way].valid){		

			if (cache_lines[set][i].lru <= cache_lines[set][way].lru 
				&& way != i && cache_lines[set][i].valid) {
				cache_lines[set][i].lru++;
				if(cache_lines[set][i].lru >= assoc){
					assert(false);
				}
			}
		}else if(way != i && cache_lines[set][i].valid) {
			cache_lines[set][i].lru++;
			if(cache_lines[set][i].lru >= assoc){
				assert(false);
			}
		}
	}
	
	cache_lines[set][way].lru = 0; // promote to the MRU position

}

uint32_t Cache::lru_victim(uint32_t set){

	//Find invalid line
	for( uint32_t i = 0; i < assoc ; i++){
		if(!cache_lines[set][i].valid){
			return i;
		}
	}

	//Find LRU position and replace otherwise
	for( uint32_t i = 0; i < assoc; i++){
		if( cache_lines[set][i].lru == assoc-1){
			return i;
		}
	}

	assert(false);
	return assoc+1;
}

