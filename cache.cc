#include "sim.h"

Cache::Cache(int sets, int assoc, int blocksize, int delay, int bw, int level): sets(sets), 
	assoc(assoc), blocksize(blocksize), latency(delay), bandwidth_latency(bw), level(level){

	bool handled;
	uint64_t handling_addr;
	uint64_t handling_tick;	 
	uint64_t tick = 0;

	set_full_track = new bool[sets];
	for(int a = 0; a < sets; a++){
		set_full_track[a] = false;;
	}

	ACCESS_CYCLES = 0;
	
	dram_channels = 2;
	mm_latency = 100;

	queue_size = 64;
	queue_total = 0;
	qhead = 0;
	qtail = 0;
	
	fill_queue_size = 32;
	fill_total = 0;
	replacement_size = 32;
	fhead = 0;
	ftail = 0;

	mshr_size = 32;	
/*			switch(level){
		case 2: mshr_size = 8;  break;
		case 4: mshr_size = 16; break;
		case 8: mshr_size = 32; break;	
		default: mshr_size = 1;				
	}
*/
	mindex = 0;

	hits = 0;
	mshr_hits = 0;
	pf_hits = 0;
	misses = 0;
	evictions = 0;
	accesses = 0;
	test_acc = 0;

	cache_lines = new Block*[sets];
    for(int a = 0; a < sets; a++){
        cache_lines[a] = new Block[assoc];
        for(int b = 0; b < assoc; b++){
            cache_lines[a][b].valid = false;
			cache_lines[a][b].dead = true;
			cache_lines[a][b].next_access = 0;
		    cache_lines[a][b].lru = b;
        }
	}

	
	bandwidth_queue = new Packet[queue_size];
	req_queue = new Packet[queue_size];
	fill_queue = new Packet[fill_queue_size];
	mshr = new Packet[mshr_size];

	for(int a = 0; a < queue_size; a++)
		req_queue[a].valid = false;

	for(int a = 0; a < fill_queue_size; a++)
		fill_queue[a].valid = false;

	for(int a =0; a < mshr_size; a++)
		mshr[a].valid = false;
	assert(req_queue);
}

bool Cache::operate(){
	//printf("cache op %d\n",level);	
	//tick++;
	
	//handle access
	if(qtail != qhead && req_queue[qhead].nxt_time_step <= tick){
		assert(qtail != qhead);
		/*int po = qhead;
		while(po != qtail){
			printf("Req queue %lu %lu\n",req_queue[po].addr, req_queue[po].nxt_time_step);
			if(po == queue_size-1)
				po = 0;
			else
				po++;
		}*/

		//Get the head of the queue, if it is at the current time,
		//execute it
		uint64_t addr = req_queue[qhead].addr;
		bool readwrite = req_queue[qhead].read_write;	
		uint32_t set = get_set(addr);
		int way = get_way(set, addr);

		//printf("Request %lu\n",addr);

		//if its not a write hit on an clean block,
		if(way != -1){
			
			if(level != (1<<3)){
				if(lower_cache->get_way(lower_cache->get_set(addr),addr) != -1
				&& lower_cache->cache_lines[lower_cache->get_set(addr)]
				[lower_cache->get_way(lower_cache->get_set(addr),addr)].valid){
					if(db)printf("current lvl %d head %d tail %d "\
				"addr %lx low cache addr %lx\n",level/2,qhead,qtail,addr, 
					lower_cache->cache_lines[lower_cache->get_set(addr)]
							[lower_cache->get_way(
							lower_cache->get_set(addr),addr)].addr);
					assert(lower_cache->get_way(
					lower_cache->get_set(addr),addr) == -1);
				}
			}else if(level != (1 << 1)){
				assert(upper_cache->get_way(upper_cache->
				get_set(addr),addr) == -1);
			}

			if(level == (1 << 1)){
				
				handled_first = true;
				cache_handled = true;
				handling_addr = addr;
				handling_tick = tick;
			}

			//printf("hit on way %d\n",way);
					
			//If a hit in >L1	
			if(level != 1 << 1){
				
 				//Since this is an exclusive cache, the data needs to 
 				//be put into the L1 fill queue and the current line 
 				//needs to be invalidated in this level of the cache

				bool fill_L1 = false;
				req_queue[qhead].nxt_time_step = tick + latency;
				
				Cache *tgt = upper_cache;
				while(tgt->level != 1 << 1)
					tgt = tgt->upper_cache;

				//if(level == 1 << 1)
				//	printf("hit fill\n");
				if( tgt -> add_to_fill(req_queue[qhead]) == -1 ){
					req_queue[qhead].nxt_time_step -= latency;
					//printf("failed to add to fill\n");
					return false;	
				}

				//printf("removing %lu and lowering lru\n",
				//	cache_lines[set][way].addr);

				for(int a = 0; a < assoc; a++){
					/*if(level == 1 << 2)
						printf("way %d %lu lru %d v %d\n",
							a, cache_lines[set][a].addr, 
							cache_lines[set][a].lru, 
							cache_lines[set][a].valid);*/
					if(cache_lines[set][a].lru > cache_lines[set][way].lru)
						cache_lines[set][a].lru--;
				}

				cache_lines[set][way].valid = false;

				/*Cache *tgt = upper_cache;
				int fill_idx[3] = {-1,-1,-1};
				
				bool fill_done = false;
				bool fail_to_fill = false;	
				while(!fill_done){
					//Add it to the upper cache's fill
					req_queue[qhead].nxt_time_step = tick + latency;
					assert(lg2(tgt->level)-1 > -1);
					fill_idx[lg2(tgt->level)-1] = tgt->add_to_fill(req_queue[qhead]);
					if(fill_idx[lg2(tgt->level)-1]){
						req_queue[qhead].nxt_time_step -= latency;
						fail_to_fill = true;
						break;
					}
		
					if(tgt->upper_cache == NULL){
						fill_done = true;
						break;
					}

					tgt = tgt->upper_cache;
				}

				if(fail_to_fill){
					fill_done = false;
					tgt = upper_cache;
					while(!fill_done){
						assert(lg2(tgt->level)-1 > -1);
						if(fill_idx[(tgt->level)-1] == -1){
							return false;
						}else{
							fill_queue[fill_idx[lg2(tgt->level)-1]].valid = false;
						}

						if(tgt->upper_cache == NULL)
							return false;
						
						tgt = tgt->upper_cache;
					}
				}*/
			}

			//BOOKMARK
			//Update replacement to show hit
			//update_replacement(way, set, addr );

			//Can do L1 prefetcher
			//if( level == 1 << 1)
			//	prefetch(addr);
			
			//update q	
			ACCESS_CYCLES += (tick - req_queue[qhead].acc_cycle);
			//printf("HIT ACC %d tick %d cyc %d tick - req %d addr %lx\n",
			//	ACCESS_CYCLES, tick, req_queue[qhead].acc_cycle, 
			//	tick - req_queue[qhead].acc_cycle,
			//	req_queue[qhead].addr);	
			remove_queue(&req_queue[qhead]);
	
			if(!req_queue[qhead].pf)
				hits++;
			else if(cache_lines[set][way].prefetch)
				pf_hits++;

			accesses++;

			return true;

		}else{
			

			int mshr_loc = check_mshr(addr);
			
			//Check mshr
			if(mshr_loc >= 0){
				mshr[mshr_loc] = req_queue[qhead];	
				//Remove request from queue
				remove_queue(&req_queue[qhead]);	
				
				//Do not continue, instruction already in flight
				if(!req_queue[qhead].pf){
					accesses++;
					mshr_hits++;	
				}
/*				if( level == 1 << 1)
					prefetch(addr);*/
				return true;
			}

			//If there are more cache levels, add to queue			
			if(level != (1 << 3)){
				//With the latency of the current level
				req_queue[qhead].nxt_time_step = tick + latency;

				if(mshr_loc < 0 && (level == 1 << 1)){
					//Add to MSHR. If the mshr is full, then stall
					if ( !add_to_mshr(req_queue[qhead]) ){
						//printf("failed to add to mshr\n");
						return false;			
					}
				}
	
				//if the lower queue is full, delay request until next operation
				if(db) printf("adding to lower %d\n",req_queue[qhead].nxt_time_step);
				if(!lower_cache -> add_to_queue(req_queue[qhead])){
					req_queue[qhead].nxt_time_step -= latency;

					if(level == 1 << 1){
						mshr_loc = check_mshr(addr);
						mshr[mshr_loc].valid = 0;
					}
					//printf("failed to add to lower queue\n");
					return false;
				}else{
					//printf("added to lower queue\n");
				}
				
				/*if( level == 1 << 1)
					prefetch(addr);*/

				remove_queue(&req_queue[qhead]);
				if(!req_queue[qhead].pf)
					misses++;

				accesses++;
				return true;

			}else{

				//Otherwise send to main memory (or pretend to)
				//and put it in the fill queues of other caches, including this one
				//in 200cycles
				//Add it to the upper cache's fill
				req_queue[qhead].nxt_time_step = tick + mm_latency;

				//fill this level
				//int here_fill = add_to_fill(req_queue[qhead]);

				//if(here_fill > -1)
				//printf("ADDED TO L3\n");	
				//Did not add it to the current level fill queue
				/*if(here_fill == -1){
					req_queue[qhead].nxt_time_step -= mm_latency;
					return false;
				}*/
			
				/*printf("tick %d rel %ld access addr %lu fill addr %lu %d %d\n",tick,
						fill_queue[here_fill].nxt_time_step,
						req_queue[qhead].addr,fill_queue[here_fill].addr, 
						fill_queue[here_fill].valid, here_fill);*/
			
				bool fill_L1 = false;
				
				Cache *tgt = upper_cache;
				while(tgt->level != 1 << 1){
					tgt = tgt->upper_cache;
				}
	
				if( tgt -> add_to_fill(req_queue[qhead]) == -1){
					req_queue[qhead].nxt_time_step -= mm_latency;
					return false;	
				}
	
				/*
				Cache *tgt = upper_cache;
				int fill_idx[3] = {-1,-1,-1};

				bool fill_done = false;
				bool fail_to_fill = false;	
				int i = 0;

				while(!fill_done){
					i = lg2(tgt->level)-1;
					//Add it to the upper cache's fill
					req_queue[qhead].nxt_time_step = tick + mm_latency;
					fill_queue[here_fill].valid = false;
					fill_idx[i] = tgt->add_to_fill(req_queue[qhead]);

					assert(i > -1);

					if(fill_idx[i] < -1){
						req_queue[qhead].nxt_time_step -= mm_latency;
						fail_to_fill = true;
						break;
					}
					
					if(tgt->upper_cache == NULL){
						fill_done = true;
						break;
					}

					tgt = tgt->upper_cache;
				}


				if(fail_to_fill){
					fill_done = false;
					fill_queue[here_fill].valid = false;
					tgt = upper_cache;
					while(!fill_done){
						assert(lg2(tgt->level)-1 > -1);
						if(fill_idx[lg2(tgt->level)-1] == -1){
							return false;
						}else{
							fill_queue[fill_idx[lg2(tgt->level)-1]].valid = false;
						}

						if(tgt->upper_cache == NULL)
							return false;
						
						tgt = tgt->upper_cache;
					}
				}*/	
			
				/*if(lower_cache->add_to_fill(req_queue[qhead])){
					req_queue[qhead].nxt_time_step -= mm_latency;

					//remove the entry from the current fill queue
					fill_queue[fill_idx]->valid = false;
					return false;
				}*/
				/*if(!make_rfo && mshr_loc != -1){
					//Add to MSHR. If the mshr is full, then stall
					if ( !add_to_mshr(req_queue[qhead]) ){
						return false;			
					}
				}*/
			}
			
			/*if( level == 1 << 1)
				prefetch(addr);*/
			remove_queue(&req_queue[qhead]);	
			//if(!req_queue[qhead].pf)
			misses++;
			accesses++;
			return true;	
		}
		//prefetch	
		//if( level == 1 << 1)
		//	prefetch(addr);	
	}
		
	return false;
}

bool Cache::fill(){
	//find the most recent request to be filled
	int recent = -1;
	int ridx = -1;
	
	int val = 0;
	for(int a = 0; a < fill_queue_size; a++){
		if(fill_queue[a].valid){
			val++;
		}
	}

	for(int a = 0; a < fill_queue_size; a++){
		
		if(fill_queue[a].valid 
		&& fill_queue[a].nxt_time_step <= tick 
		&& (fill_queue[a].nxt_time_step <= recent
		|| recent == -1)){
			recent = fill_queue[a].nxt_time_step;
			ridx = a;
		}
	}

	//Nothing to fill so no reason to continue
	if( ridx == -1){
		//printf("DIDN'T FILL\n");
		return false;
	}

	Packet access = fill_queue[ridx];
	uint64_t addr = fill_queue[ridx].addr;
	bool readwrite = fill_queue[ridx].read_write;	
	uint32_t set = get_set(addr);
	int way = get_way(set, addr);

	//exclusive assumes fill
	uint8_t vic_way = find_victim(set, addr);	

	//It was already in the cache, but it was being written to
	if(way > -1 && readwrite){
		cache_lines[set][way].dirty = true;
	}

	update_replacement(vic_way, set, addr);
	operate_replacement(vic_way, set, addr);
	
	
	//printf("perf fill %d %lu \n", check_mshr(addr),addr);
	if(check_mshr(addr) > -1)
		mshr[check_mshr(addr)].valid = 0;
		//remove_mshr(&mshr[check_mshr(addr)]);

	//}
	//Packet empty;
	uint64_t a = fill_queue[ridx].addr;

	if(level == 1 << 1 && !fill_queue[ridx].pf){
		ACCESS_CYCLES += tick - fill_queue[ridx].acc_cycle;
		//printf("FILL ACC %lx tick %d cyc %d tick - req %d addr %lx\n",
		//		ACCESS_CYCLES, tick, fill_queue[ridx].acc_cycle, 
		//		tick - fill_queue[ridx].acc_cycle,
		//		fill_queue[ridx].addr);	
	}
	
	//assert(fill_queue[ridx].nxt_time_step == event_head->time);
	
	fill_queue[ridx].valid = false; //= empty;

	//pop_event_queue();
	
	//if(check_fill(a) != false)
	//	printf("invalidating fill entry %lx\n",fill_queue[ridx].addr);
	//
	//assert(check_fill(a) == false);
	//
	return true;	
}

int Cache::get_set(uint64_t addr){
	
    return ((addr >> 6) & ((1 << lg2_sets) - 1));

}

bool Cache::set_full(uint32_t set){
	if(!set_full_track[set]){
		for(int a =0; a < assoc; a++){
			if(!cache_lines[set][a].valid)
				return false;
		}
		set_full_track[set] = true;
		return true;
	}else{
		return true;
	}
}

int Cache::get_way(uint32_t set, uint64_t addr){
	for(int a = 0; a < assoc; a++){
		if(((cache_lines[set][a].addr >> (lg2_blocks))
			== ((addr) >> (lg2_blocks))) && cache_lines[set][a].valid){
			//printf("hit on %lx\n",cache_lines[set][a].addr);
			return a;	
		}
	}
	return -1;
}

bool Cache::check_hit(uint32_t set, uint64_t addr){

	for(int x = 0; x < assoc; x++){
		if((cache_lines[set][x].addr >> ((lg2_blocks) + lg2_sets)) 
			== ((addr) >> (lg2_sets+(lg2_blocks))) 
			&& cache_lines[set][x].valid)

			return true;
	}

	return false;
}

uint64_t Cache::align(uint64_t addr){
	return (addr) >> 6;
}

uint32_t Cache::get_page(uint64_t addr){
	
}

bool Cache::add_to_queue(Packet &access){
	
	if(queue_total == queue_size - 1)
		return false;	

	//Packet acc(access.read_write, access.addr,
	//   access.fill_level, access.nxt_time_step);

	uint64_t time = access.nxt_time_step;
	
	req_queue[qtail] = access;
	
	if(qtail == queue_size - 1)
		qtail = 0;
	else
		qtail++;

	//printf("adding time %lx\n", time);
	//push_event_queue(time);

	queue_total++;
	return true;	

}

void Cache::remove_queue(Packet *access){
	//Remove from queue
	//Packet empty;
	//*req_queue[qhead] = empty;	

	//printf("nxt time %lx time %lx\n",access->nxt_time_step, event_head->time);
	//assert(access->nxt_time_step == event_head->time);

	//if(num_events != 0)
	//	pop_event_queue();

	Packet empty;
	*access = empty;
	access->valid = false;
	if(qhead == queue_size - 1)
		qhead = 0;
	else	
		qhead++;

	queue_total--;
}

void Cache::remove_mshr(Packet *access){
	Packet empty;
	*access = empty;
	/*for(int a = 0; a < mshr_size; a++){
		if(align(mshr[a].addr) == align(access.addr) && mshr[a].valid){
			Packet empty;
			*mshr[a] = empty;	
		}
	}*/	
}

int Cache::add_to_fill(Packet &access){

	//printf("Adding %lx to the fill queue of level %d\n",access.addr,level);
	/*if(level == 1 << 3){
		printf("before\n");
		for(int a = 0; a < fill_queue_size; a++)
			printf("a %d  addr %lu val %d\n",
				a, fill_queue[a].addr, fill_queue[a].valid);
	}*/

	for(int x = 0; x < fill_queue_size; x++){

		if(fill_queue[x].valid == false){
			Packet acc(access.read_write, access.addr,
					   access.fill_level, access.nxt_time_step);
		
			//if(level == 1 << 1)
			//	printf("adding access %lu to l1 fill %lu\n",
			//		access.addr, acc.addr);			
			
			fill_queue[x] = access;
			fill_queue[x].valid = true;
			fill_queue[x].origin = access.origin;

			//push_event_queue(access.nxt_time_step);

			/*
			if(level == 1 << 3){
				printf("after\n");
				for(int a = 0; a < fill_queue_size; a++)
					printf("a %d  addr %lu val %d\n",
					a, fill_queue[a].addr, fill_queue[a].valid);
				printf("returning x %d\n",x);
			}
			*/
			
			return x;
		}
	}

	return -1;
/*	if(fill_total == fill_queue_size - 1)
		return false;	

	fill_queue[ftail] = access;
	if(ftail == fill_queue_size - 1)
		ftail = 0;
	else
		ftail++;

	fill_total++;*/
}

int Cache::add_to_replacement(Packet &access){
	replacement_queue.push_back(access.addr);
	return 1;
}

bool Cache::add_to_mshr(Packet &access){

	for(int a = 0; a < mshr_size; a++){
		//printf("mshr %d %lu %d %d\n",a,mshr[a].addr, mshr[a].valid, mshr_size);
		
		if(mshr[a].valid  == false){
			//Packet acc(access.read_write, access.addr,
			//		   access.fill_level, access.nxt_time_step);
			mshr[a].read_write = access.read_write; // = acc;
			mshr[a].addr = access.addr;
			mshr[a].fill_level = access.fill_level;
			mshr[a].nxt_time_step = access.nxt_time_step;
			mshr[a].valid = true;
			return true;
		}
	}
	return false;
}

int Cache::check_mshr(uint64_t addr){

	for(int a = 0; a < mshr_size; a++){
		if(mshr[a].valid && (align(mshr[a].addr) == align(addr)))
			return a;
	}

	return -1;

}

int lg2(int n)
{
	int i, m = n, c = -1;
	for (i=0; m; i++) {
		m /= 2;
		c++;
	}
	return c;
}

bool Cache::cache_full(){
	
	for(int a = 0; a < sets; a++){
		 if(!set_full(a))
			return false;	
	}
	return true;
}

bool Cache::check_rep_queue(uint64_t addr){
	for(int a = 0; a < replacement_queue.size(); a++){
		if(align(replacement_queue[a]) == align(addr))
			return true;
	}
	return false;
}

bool Cache::check_fill(uint64_t addr){
	for(int a = 0; a < fill_queue_size; a++){
		if(fill_queue[a].valid && align(fill_queue[a].addr) == align(addr))
			return true;
	}
	return false;
}

bool Cache::check_replacement(uint64_t addr){
	for(int a = 0; a < replacement_queue.size(); a++){
		if(align(replacement_queue[a]) == align(addr))
			return true;
	}
	return false;
}

bool Cache::fill_full(){
	int num = 0;
	for(int a = 0; a < fill_queue_size; a++){
		if(fill_queue[a].valid)
			num++;
	}
	
	if(num == fill_queue_size)
		return true;

	return false;
}

bool Cache::addr_present(uint64_t addr){
	int set = get_set(addr);
	return get_way(set,addr) != -1;
}

uint64_t Cache::get_addr(int set, int way){
	return cache_lines[set][way].addr;
}

bool Cache::push_event_queue(uint64_t time){

	Event* e = new Event();
	e->time = time;
	//e->level = level;
	
	if(num_events == 0){
		event_head = e;
		event_tail = e;
		num_events++;
	}else if(num_events == 1){
		event_head->next = e;
		event_tail = e;
		num_events++;
	}else{
		event_tail->next = e;
		event_tail = e;
		num_events++;
	}
	
	return true;
}

Event* Cache::pop_event_queue(){

	if(num_events == 0){
		assert(false);
	}else if(num_events == 1){

		Event* e = event_head;
		num_events--;

		event_head = NULL;
		event_tail = NULL;

		return e;

	}else{

		Event* e = event_head;
		event_head = event_head->next;
		num_events--;

		return e;

	
	}
}
