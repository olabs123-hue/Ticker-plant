#include "tickerplant/tickerplant.hpp"
#include <fstream>
namespace tickerplant {
namespace {
constexpr uint32_t MAGIC=0x5449434b;constexpr size_t RECORD=24;
void w32(std::ostream& s,uint32_t v){for(int i=0;i<4;++i)s.put(char((v>>(i*8))&255));}
void w64(std::ostream& s,uint64_t v){for(int i=0;i<8;++i)s.put(char((v>>(i*8))&255));}
bool r32(std::istream& s,uint32_t& v){v=0;for(int i=0;i<4;++i){int c=s.get();if(c==EOF)return false;v|=uint32_t(uint8_t(c))<<(i*8);}return true;}
bool r64(std::istream& s,uint64_t& v){v=0;for(int i=0;i<8;++i){int c=s.get();if(c==EOF)return false;v|=uint64_t(uint8_t(c))<<(i*8);}return true;}
bool read(std::istream& s,Tick& t){uint32_t m;uint64_t p;if(!r32(s,m)||m!=MAGIC||!r64(s,t.sequence)||!r64(s,p)||!r32(s,t.symbol))return false;t.price=static_cast<int64_t>(p);return true;}
}
TickLog::TickLog(std::filesystem::path path):path_(std::move(path)){
 if(!std::filesystem::exists(path_)){std::ofstream f(path_,std::ios::binary);if(!f)throw std::runtime_error("create log failed");}
 std::ifstream in(path_,std::ios::binary);uint64_t off=0;Tick t;
 while(read(in,t)){offsets_[t.sequence]=off;off+=RECORD;}
 if(std::filesystem::file_size(path_)!=off)std::filesystem::resize_file(path_,off);
}
void TickLog::append(Tick t){
 if(offsets_.count(t.sequence))throw std::invalid_argument("duplicate sequence");
 uint64_t off=std::filesystem::file_size(path_);std::ofstream out(path_,std::ios::binary|std::ios::app);
 w32(out,MAGIC);w64(out,t.sequence);w64(out,static_cast<uint64_t>(t.price));w32(out,t.symbol);
 out.flush();if(!out)throw std::runtime_error("append failed");offsets_[t.sequence]=off;
}
void TickLog::flush(){/* flushes userspace streams on append; no fsync durability guarantee */}
std::vector<Tick> TickLog::replay(uint64_t from)const{
 std::vector<Tick> out;std::ifstream in(path_,std::ios::binary);
 for(auto it=offsets_.lower_bound(from);it!=offsets_.end();++it){in.clear();in.seekg(static_cast<std::streamoff>(it->second));Tick t;if(!read(in,t))throw std::runtime_error("corrupt log");out.push_back(t);}return out;
}
}