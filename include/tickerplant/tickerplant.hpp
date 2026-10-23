#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <utility>
#include <algorithm>
namespace tickerplant {
struct Tick { uint64_t sequence=0; int64_t price=0; uint32_t symbol=0; };
template<typename T> class SpscRing {
public:
 explicit SpscRing(size_t n):data_(n),mask_(n-1){if(n<2||(n&(n-1)))throw std::invalid_argument("capacity must be power of two >=2");}
 bool push(const T& x){size_t t=tail_.load(std::memory_order_relaxed);if(t-head_.load(std::memory_order_acquire)==data_.size())return false;data_[t&mask_]=x;tail_.store(t+1,std::memory_order_release);return true;}
 bool pop(T& x){size_t h=head_.load(std::memory_order_relaxed);if(h==tail_.load(std::memory_order_acquire))return false;x=data_[h&mask_];head_.store(h+1,std::memory_order_release);return true;}
 size_t capacity()const{return data_.size();}
private:
 std::vector<T> data_;size_t mask_;alignas(64) std::atomic<size_t> head_{0};alignas(64) std::atomic<size_t> tail_{0};
};
class FeedHandler {
public:
 using Request=std::function<void(uint64_t,uint64_t)>;using Deliver=std::function<void(const Tick&)>;
 FeedHandler(uint64_t first,Request request,Deliver deliver):next_(first),request_(std::move(request)),deliver_(std::move(deliver)){}
 void receive(Tick tick){
  if(tick.sequence<next_||pending_.count(tick.sequence))return;
  if(tick.sequence>next_){uint64_t start=std::max(next_,requested_high_+1);if(start<tick.sequence){request_(start,tick.sequence-1);requested_high_=tick.sequence-1;}}
  pending_.emplace(tick.sequence,tick);
  while(true){auto it=pending_.find(next_);if(it==pending_.end())break;deliver_(it->second);pending_.erase(it);++next_;}
 }
 uint64_t next_sequence()const{return next_;}
 size_t buffered()const{return pending_.size();}
 size_t outstanding()const{return requested_high_>=next_?size_t(requested_high_-next_+1):0;}
private:
 uint64_t next_;Request request_;Deliver deliver_;std::map<uint64_t,Tick> pending_;uint64_t requested_high_=0;
};
class TickLog {
public:
 explicit TickLog(std::filesystem::path path);
 void append(Tick tick);
 void flush();
 std::vector<Tick> replay(uint64_t from)const;
 size_t size()const{return offsets_.size();}
private:
 std::filesystem::path path_;std::map<uint64_t,uint64_t> offsets_;
};
class ConflatingPublisher {
public:
 using Subscriber=uint64_t;
 Subscriber subscribe(){auto id=++last_;pending_.try_emplace(id);return id;}
 void enable(Subscriber id){pending_.try_emplace(id);}
 void unsubscribe(Subscriber id){pending_.erase(id);}
 void publish(Tick tick){for(auto& entry:pending_)entry.second[tick.symbol]=tick;}
 std::vector<Tick> drain(Subscriber id){std::vector<Tick> out;auto it=pending_.find(id);if(it==pending_.end())return out;for(auto& entry:it->second)out.push_back(entry.second);it->second.clear();return out;}
private:
 Subscriber last_=0;std::unordered_map<Subscriber,std::map<uint32_t,Tick>> pending_;
};
}