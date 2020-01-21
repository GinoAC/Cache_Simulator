#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <algorithm>

#define LOG2_BLOCK 6
#define LOG2_PAGE 12 

#define perfect 0 

extern bool db;
extern uint64_t tick;
extern uint64_t handling_addr;
extern uint64_t handling_tick;
extern bool handled_first;
extern bool cache_handled;


extern uint64_t num_events;

using namespace std;

enum Level{ L1, L2, L3};

struct Event{
	public: 

		Event(){}
		Event* next;
		//int level;
		uint64_t time;
};

extern Event* event_head;
extern Event* event_tail;

class Block{
	public:
		bool	valid,
				prefetch,
				dirty,
				dead;
		uint8_t	lru;
		//bool moving;
		int distance;
		uint64_t addr,
				 next_access;
	
	Block(){
		valid = 0;
		prefetch = 0;
		dirty = 0;
		addr = 0;
		lru = 0;
		distance = -1;
	};

	Block(bool v, bool pf, uint64_t	addr):
		valid(v),prefetch(pf),addr(addr){
		
		dirty = 0;
		lru = 0;
		distance = -1;
	}
};

class Packet{
	public: 
		bool read_write;
		uint64_t addr,
			acc_cycle,
			next_access;
		int fill_level,
			nxt_time_step;
		bool valid, pf, dead;
		int origin;	

	Packet( bool rw, uint64_t adr, int fill, int cycle ){
		addr = adr;
		read_write = rw;
		fill_level = fill;
		nxt_time_step = cycle;
		valid = true;
		pf = false;
		origin = -1;
		acc_cycle = 0;
		next_access = 0;
		dead = true;
	};
	
	Packet(){
		addr = 0;
		read_write = 0;
		fill_level = 0;
		nxt_time_step = 0;
		valid = false;
		pf = false;
		origin = 0;
		acc_cycle = 0;
		next_access = 0;
		dead = true;	
	};

};

class Cache{

	public:
		int l1_sets;
		int l2_sets;
		int l3_sets;

		int sets,
			assoc,
			blocksize;
		
		int hits,
			mshr_hits,
			misses,
			evictions,
			pf_hits,
			accesses,
			test_acc;
		
		int level,
			latency,
			mm_latency;

		int queue_size,
			queue_total,
			qhead,
			qtail,
			fill_queue_size,
			fill_total,
			fhead,
			ftail,		
			mshr_size,
			mindex;
	
		int bandwidth_latency;
	
		uint64_t ACCESS_CYCLES;
		uint8_t dram_channels;
			
		int lg2_sets,
			lg2_blocks;	

		uint8_t replacement_size; 
	
		Packet *bandwidth_queue;	
		Packet *req_queue;
		Packet *mshr;
		Packet *fill_queue;

		Block **cache_lines;	

		Cache *lower_cache;
		Cache *upper_cache;

		//describes the number of invalid ways in a set for 
		//loading in data in perfect mode
		bool* set_full_track;

		Cache(int sets, int assoc, int blocksize, int delay, int bw, int level);	
	
		std::vector<uint64_t> replacement_queue;
						
		int get_set(uint64_t addr);
		bool set_full(uint32_t set);
		bool cache_full();
		int get_way(uint32_t set, uint64_t addr);
		bool check_hit(uint32_t set, uint64_t addr);
		int check_mshr(uint64_t addr);
		uint64_t align(uint64_t addr);
		uint32_t get_page(uint64_t addr);
		bool check_fill(uint64_t addr);
		bool check_replacement(uint64_t addr);	
		bool check_rep_queue(uint64_t addr);
		bool fill_full();

		bool add_to_queue(Packet &access);	
		int add_to_fill(Packet &access);	
		int add_to_replacement(Packet &access);	
		bool add_to_mshr(Packet &access);	
		
		void remove_queue(Packet *access);
		void remove_mshr(Packet *access);
		void remove_fill(int idx);
		
		bool operate();
		bool fill();

		Event* pop_event_queue();
		bool push_event_queue(uint64_t time);

		//Replacement
		uint32_t find_victim(uint32_t set, uint64_t addr);
		void update_replacement(uint8_t way, uint32_t set, uint64_t addr);	
		void operate_replacement(uint8_t victim, uint32_t set, uint64_t addr);
		uint32_t lru_victim(uint32_t set);
		void update_lru(uint8_t way, uint32_t set, uint64_t addr);

		//Prefetching
		void init_prefetcher();
		void prefetch(uint64_t addr);

		bool addr_present(uint64_t addr);
		uint64_t get_addr(int set, int way);
		

		int lg2(int n)
		{
		    int i=0, m = n, c = -1;
			for (i=0; m; i++) {
		    	m /= 2;
		    	c++;
		    }
			return c;
		}

	~Cache(){
		for(int a = 0; a < sets; a++)
			delete [] cache_lines[a];
		delete [] cache_lines;

		delete [] bandwidth_queue;	
		delete [] req_queue;	
		delete [] fill_queue;	
		delete [] mshr;
		delete [] set_full_track;
	}	
		
};
