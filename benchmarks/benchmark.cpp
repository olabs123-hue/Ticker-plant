#include "tickerplant/tickerplant.hpp"
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
using namespace tickerplant;
int main(int argc,char**argv){
 size_t n=1000000;if(argc>1)n=std::stoull(argv[1]);
 std::mt19937_64 rng(20261023);std::uniform_real_distribution<double> chance(0,1);
 std::vector<Tick> network,recovery;network.reserve(n*2);recovery.reserve(n/20);
 for(size_t i=1;i<=n;++i){Tick t{uint64_t(i),int64_t(10000+i%100),uint32_t(i%8)};if(chance(rng)<.02)recovery.push_back(t);else network.push_back(t);if(chance(rng)<.02)network.push_back(t);}
 for(size_t i=1;i<network.size();++i)if(chance(rng)<.02)std::swap(network[i-1],network[i]);
 size_t delivered=0,requests=0;uint64_t previous=0;bool valid=true;
 FeedHandler feed(1,[&](uint64_t,uint64_t){++requests;},[&](const Tick& t){valid&=(t.sequence==previous+1);previous=t.sequence;++delivered;});
 auto start=std::chrono::steady_clock::now();
 for(auto t:network)feed.receive(t);for(auto t:recovery)feed.receive(t);
 double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"ticks="<<n<<" received="<<network.size()<<" recovered="<<recovery.size()<<" delivered="<<delivered<<" recovery_requests="<<requests<<" elapsed_seconds="<<seconds<<" throughput_ticks_per_second="<<delivered/seconds<<" ordered="<<valid<<"\n";
 return (!valid||delivered!=n)?1:0;
}