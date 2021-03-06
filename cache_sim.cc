#include "sim.h"
#include <time.h>
using namespace std;

bool warmed_up = false; 
int l1_l2 = 0;
int l2_l3 = 0;
int l2_l1 = 0;
int l3_l1 = 0;
int l3_l2 = 0;

int L1_lat = 1;
int L2_lat = 8;
int L3_lat = 20;

uint64_t tick;
uint64_t handling_addr;
uint64_t handling_tick;
bool handled_first;
bool cache_handled;
uint64_t num_events;
Event* event_head;
Event* event_tail;

bool db = 0;

////////////Cache settings\\\\\\\\\\\\\\\\\\
//Cache Capacity = (sets * assoc * blocksize)
int blocksize = 64;

int l1_sets = 128;
int l2_sets = 1024;
int l3_sets = 2048;

int l1_ways = 4;
int l2_ways = 8;
int l3_ways = 16;
//////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

int l3_stall = 0;
int l2_stall = 0;
int l3_fill_stall = 0;
int l2_fill_stall = 0;
int l1_fill_stall = 0;

int l3_rep_stall = 0;
int l2_rep_stall = 0;
int l1_rep_stall = 0;

int l3_bw = 1;
int l2_bw = 1;
int l1_bw = 1;


uint64_t elapsed_second,
		 elapsed_minute,
		 elapsed_hour; 

int main(int argc, char** argv)
{
	
	if(argc < 5){
		printf("Requires input of format: "\
		"./main [# warmup instructions] [# simulation instructions] [trace file] [# of cache levels]\n");
		return 0;
	}

	int IC = 0;
	uint64_t warmup;
	int limit;
	int levels;
	char* inter;
	time_t timer;
	struct tm exe = {0};
	tick = 0;

	num_events = 0;
	
	event_head = NULL;
	event_tail = NULL;

	time(&timer);

	warmup = strtol(argv[1], &inter, 10);
	limit = strtol(argv[2], &inter, 10);
	levels = strtol(argv[4], &inter, 10);

	
	handled_first = false;
	handling_addr = 0;
	handling_tick = 0;

	uint64_t last_handling_tick = 0;

	//Based on the number of levels specified, the cache hierarchy is built
	Cache L1(l1_sets, l1_ways, blocksize, L1_lat, l1_bw, Level::L1);

	L1.lg2_sets = L1.lg2(L1.sets);
	L1.lg2_blocks = L1.lg2(L1.blocksize);
	
	L1.upper_cache = NULL;
	L1.lower_cache = NULL;
	
	Cache L2(l2_sets, l2_ways, blocksize, L2_lat, l1_bw, Level::L2);
	Cache L3(l3_sets, l3_ways, blocksize, L3_lat, l1_bw, Level::L3);

	if(levels >=2){	
		L2.lg2_sets = L2.lg2(L2.sets);
		L2.lg2_blocks = L2.lg2(L2.blocksize);
		
		L1.lower_cache = &L2;
		L2.lower_cache = NULL;
		L2.upper_cache = &L1;
	}

	if(levels >= 3){

		L3.lg2_sets = L3.lg2(L3.sets);
		L3.lg2_blocks = L3.lg2(L3.blocksize);
		
		L2.lower_cache = &L3;
		L3.lower_cache = NULL;
		L3.upper_cache = &L2;
	}

	ifstream trace_file;

	printf("%s\n",argv[3]);
    trace_file.open(argv[3], ios::binary | ios::in);

	if(!trace_file.is_open())
		assert(false);
	
	Packet access(0,0,0,0);
	uint64_t acc_cycle = 0;
	uint64_t last_acc_cycle = 0;
	uint64_t addr = 0;
	bool rw;
	bool hit = 0;
	bool queue_stall = false;
	bool mem = 0;
	int mems = 0;
	int op = 0;
	int acc_num = 0;
	uint64_t instructions = 0;

	int idle = 0;
	bool handled = true;
	
	while(trace_file){
		if(!queue_stall && handled){

			if(acc_cycle != 0)
				last_acc_cycle = acc_cycle;

			trace_file >> acc_cycle;
			trace_file >> mem;

			if(mem){
				trace_file >> rw >> addr;
				trace_file >> instructions;
				
				//access = new Packet(rw, addr, 1, 0);//last one is timing

				if(db)printf("\nacc_cycle %ld next_addr %lx inst %lu\n",
					acc_cycle, addr, instructions);	

				access.acc_cycle = acc_cycle;
				access.read_write = rw;
				access.addr = addr,
				access.fill_level = 2;
				access.nxt_time_step = acc_cycle + L1.latency;
				mems++;
				handled = false;

				acc_num++;

			}else{
				op++;
			}
		}
								//instructions 	
		if(acc_cycle <= tick && (acc_num + op ) < (limit + warmup)){
			if( L1.add_to_queue(access) ){
				if(db && queue_stall) printf("queue no longer stalling\n");
				queue_stall = false;
				handled = true;
			}else{
				if(db)printf("queue stalling acc cycle %ld tick %ld\n", acc_cycle, tick);
				queue_stall = true;
				handled = false;
			}
		}else if(db && (acc_num + op) >= (limit + warmup))
			printf("waiting for queues to drain\n");

		//printf("\n");
		//BOOKMARK
		switch(levels){
			case 3:
				L3.fill();
				L3.operate();
			case 2:
				L2.fill();
				L2.operate();
			case 1:
				L1.fill();
				L1.operate();
			break;
			default:
				assert(false);
		}

		tick++;
		if(tick % 10000000 == 0){
			
			elapsed_second = (uint64_t)(time(NULL) - timer);
			elapsed_minute = elapsed_second / 60;
			elapsed_hour = elapsed_minute / 60;
			elapsed_minute -= elapsed_hour*60;
			elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);
			printf("Elapsed Simulation Time - %lu:%lu:%lu\n",elapsed_hour,elapsed_minute,elapsed_second);	
			printf("@ IC %d memop %d TICK %ld ACC_CYCLE %ld\n", acc_num + op, op, tick, acc_cycle);
		}
		//printf("%d OP AND SHOULD BE CHECKING %d\n",op, op >= (limit + warmup));
		bool l1_finished = (L1.fill_total == 0 && L1.qhead == L1.qtail);
		if((acc_num + op) >= (limit + warmup)){
		
			if(levels == 1 && l1_finished)	
				break;
			
			bool l2_finished = (L2.fill_total == 0 && L2.qhead == L2.qtail);

			if(levels == 2 && l1_finished && l2_finished)
				break;

			if(levels == 3 && l1_finished && l2_finished && L3.fill_total == 0 && L3.qhead == L3.qtail)
				break;	
		}

		if((acc_num + op) >= warmup && !warmed_up){
			warmed_up = true;
			printf("Warmup complete. Instructions: %ld\n",instructions);

			L1.accesses = 0;
			L1.hits = 0;
			L1.misses = 0;
			L1.evictions = 0;
			
			if(levels >=2){	
				L2.accesses = 0;
				L2.hits = 0;
				L2.misses = 0;
				L2.evictions = 0;	
			}
	
			if(levels >=2){	
				L3.accesses = 0;
				L3.hits = 0;
				L3.misses = 0;
				L3.evictions = 0;	
			}
		}
	}
	
	trace_file.close();
	
	elapsed_second = (uint64_t)(time(NULL) - timer);
	elapsed_minute = elapsed_second / 60;
	elapsed_hour = elapsed_minute / 60;
	elapsed_minute -= elapsed_hour*60;
	elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);
	printf("Elapsed Simulation Time - %lu:%lu:%lu\n",elapsed_hour,elapsed_minute,elapsed_second);	
	printf("@ IC %d memop %d TICK %ld ACC_CYCLE %ld\n", op + acc_num, op, tick, acc_cycle);

	printf("mem %d op %d\n",mems, op);

	printf("IC %d MC %d TICK %lx acc_cycle %lu\n", 
		op + acc_num, op, tick, last_acc_cycle); 

	printf("STALLS: L3 %d L2 %d FILL_STALLS L3 %d L2 %d L1 %d REP_STALL L3 %d L2 %d L1 %d\n",
	 	l3_stall, l2_stall, 
	 	l3_fill_stall, l2_fill_stall,
	 	l1_fill_stall, l3_rep_stall, 
	 	l2_rep_stall, l1_rep_stall); 


	printf("L1 hits %d pf_hits %d mshr_hits %d misses %d evictions %d access %d"\
		" MPKI %lf ACCESS_CYCLES %lf AMAT %f test_acc %d\n", 
		L1.hits, L1.pf_hits, L1.mshr_hits, L1.misses, L1.evictions, 
		L1.accesses, float(L1.misses)/float((op + acc_num - warmup)/1000), 
		float(L1.ACCESS_CYCLES), float(L1.ACCESS_CYCLES)/float(acc_num), acc_num);
	
	if(levels >=2){	
		printf("L2 hits %d pf_hits %d mshr_hits %d misses %d evictions %d access %d MPKI %lf\n", 
			L2.hits, L2.pf_hits, L2.mshr_hits, L2.misses, L2.evictions, 
			L2.accesses, float(L2.misses)/float((op + acc_num -warmup)/1000));
	}
	
	if(levels >=3){	
		printf("L3 hits %d pf_hits %d mshr_hits %d misses %d evictions %d access %d MPKI %lf\n", 
			L3.hits, L3.pf_hits, L3.mshr_hits, L3.misses, L3.evictions, 
			L3.accesses, float(L3.misses)/float((op + acc_num - warmup)/1000));
	}

	printf("Simulation complete in %ld clk cycles\n",tick);

	return 0;
}

