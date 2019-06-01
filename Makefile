make_cache: cache.cc cache_sim.cc prefetcher.cc replacement.cc sim.h
	g++ -g -O3 -std=c++11  cache.cc cache_sim.cc prefetcher.cc replacement.cc sim.h -o main


#g++ -g cache.cc cache_sim.cc prefetcher.cc replacement.cc sim.h -o main
