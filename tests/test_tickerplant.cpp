#include "tickerplant/tickerplant.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <thread>
using namespace tickerplant;
namespace {
struct FeedFixture:testing::Test {
 std::vector<uint64_t> delivered;std::vector<std::pair<uint64_t,uint64_t>> requests;
 FeedHandler make(uint64_t start=1){return FeedHandler(start,[this](uint64_t a,uint64_t b){requests.emplace_back(a,b);},[this](const Tick&t){delivered.push_back(t.sequence);});}
};
}
TEST_F(FeedFixture, InOrder){auto f=make();f.receive({1});f.receive({2});EXPECT_EQ(delivered,(std::vector<uint64_t>{1,2}));}
TEST_F(FeedFixture, Duplicate){auto f=make();f.receive({1});f.receive({1});EXPECT_EQ(delivered.size(),1u);}
TEST_F(FeedFixture, Reordered){auto f=make();f.receive({2});f.receive({1});EXPECT_EQ(delivered,(std::vector<uint64_t>{1,2}));}
TEST_F(FeedFixture, MissingRange){auto f=make();f.receive({4});ASSERT_EQ(requests.size(),1u);EXPECT_EQ(requests[0],(std::make_pair(uint64_t(1),uint64_t(3))));}
TEST_F(FeedFixture, RecoverRange){auto f=make();f.receive({3});f.receive({1});f.receive({2});EXPECT_EQ(delivered.size(),3u);}
TEST_F(FeedFixture, NoRepeatRequest){auto f=make();f.receive({3});f.receive({3});EXPECT_EQ(requests.size(),1u);}
TEST_F(FeedFixture, NewGapOnly){auto f=make();f.receive({3});f.receive({5});EXPECT_EQ(requests.size(),2u);EXPECT_EQ(requests[1],(std::make_pair(uint64_t(4),uint64_t(4))));}
TEST_F(FeedFixture, OldIgnored){auto f=make();f.receive({1});f.receive({1});EXPECT_EQ(f.next_sequence(),2u);}
TEST_F(FeedFixture, StartAtTen){auto f=make(10);f.receive({10});EXPECT_EQ(delivered[0],10u);}
TEST_F(FeedFixture, BufferCount){auto f=make();f.receive({4});EXPECT_EQ(f.buffered(),1u);}
TEST_F(FeedFixture, FlushBuffer){auto f=make();f.receive({3});f.receive({2});f.receive({1});EXPECT_EQ(f.buffered(),0u);}
TEST_F(FeedFixture, Outstanding){auto f=make();f.receive({3});EXPECT_EQ(f.outstanding(),2u);}
TEST_F(FeedFixture, OutstandingCleared){auto f=make();f.receive({3});f.receive({1});f.receive({2});EXPECT_EQ(f.outstanding(),0u);}
TEST_F(FeedFixture, GapAtStart){auto f=make(7);f.receive({9});EXPECT_EQ(requests[0].first,7u);}
TEST_F(FeedFixture, EmptyInitially){auto f=make();EXPECT_EQ(f.buffered(),0u);}
TEST(Ring, RejectNonPowerTwo){EXPECT_THROW(SpscRing<int> q(3),std::invalid_argument);}
TEST(Ring, RejectOne){EXPECT_THROW(SpscRing<int> q(1),std::invalid_argument);}
TEST(Ring, Empty){SpscRing<int> q(2);int v;EXPECT_FALSE(q.pop(v));}
TEST(Ring, PushPop){SpscRing<int> q(2);int v=0;EXPECT_TRUE(q.push(42));EXPECT_TRUE(q.pop(v));EXPECT_EQ(v,42);}
TEST(Ring, Full){SpscRing<int> q(2);EXPECT_TRUE(q.push(1));EXPECT_TRUE(q.push(2));EXPECT_FALSE(q.push(3));}
TEST(Ring, Wrap){SpscRing<int> q(2);int v;for(int i=0;i<100;++i){EXPECT_TRUE(q.push(i));EXPECT_TRUE(q.pop(v));EXPECT_EQ(v,i);}}
TEST(Ring, Capacity){SpscRing<int> q(8);EXPECT_EQ(q.capacity(),8u);}
TEST(Ring, Concurrent){SpscRing<uint64_t> q(1024);std::thread p([&]{for(uint64_t i=0;i<100000;++i)while(!q.push(i))std::this_thread::yield();});uint64_t v;for(uint64_t i=0;i<100000;++i){while(!q.pop(v))std::this_thread::yield();ASSERT_EQ(v,i);}p.join();}
TEST(Publisher, Empty){ConflatingPublisher p;auto id=p.subscribe();EXPECT_TRUE(p.drain(id).empty());}
TEST(Publisher, Conflate){ConflatingPublisher p;auto id=p.subscribe();p.publish({1,10,2});p.publish({2,20,2});auto v=p.drain(id);ASSERT_EQ(v.size(),1u);EXPECT_EQ(v[0].sequence,2u);}
TEST(Publisher, MultipleSymbols){ConflatingPublisher p;auto id=p.subscribe();p.publish({1,10,1});p.publish({2,20,2});EXPECT_EQ(p.drain(id).size(),2u);}
TEST(Publisher, Independent){ConflatingPublisher p;auto a=p.subscribe(),b=p.subscribe();p.publish({1});EXPECT_EQ(p.drain(a).size(),1u);EXPECT_EQ(p.drain(b).size(),1u);}
TEST(Publisher, Unsubscribe){ConflatingPublisher p;auto id=p.subscribe();p.unsubscribe(id);p.publish({1});EXPECT_TRUE(p.drain(id).empty());}
TEST(Publisher, DrainClears){ConflatingPublisher p;auto id=p.subscribe();p.publish({1});p.drain(id);EXPECT_TRUE(p.drain(id).empty());}
TEST(Log, ReopenAndReplay){auto path=std::filesystem::temp_directory_path()/"tickerplant_test_reopen.bin";std::filesystem::remove(path);{TickLog l(path);l.append({1,100,1});l.append({2,200,2});}{TickLog l(path);EXPECT_EQ(l.size(),2u);auto v=l.replay(2);ASSERT_EQ(v.size(),1u);EXPECT_EQ(v[0].price,200);}std::filesystem::remove(path);}
TEST(Log, RejectDuplicate){auto path=std::filesystem::temp_directory_path()/"tickerplant_test_duplicate.bin";std::filesystem::remove(path);{TickLog l(path);l.append({1});EXPECT_THROW(l.append({1}),std::invalid_argument);}std::filesystem::remove(path);}
