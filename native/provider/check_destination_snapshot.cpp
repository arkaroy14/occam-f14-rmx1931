// Sparse host-memory fixtures. No phone attachment or native metadata calls.
#include "f14_destination_snapshot.h"
#include <vector>
#include <assert.h>
#include <stdio.h>
using namespace f14dest;
static constexpr uintptr_t hal=0x10000000,handle=0x20000,inner_ptr=0x30000,
    pools=0x40000,slots=0x50000,bases=0x70000,descs=0x80000,arena=0x90000;
struct Region {uintptr_t address;std::vector<uint8_t> bytes;};
struct Memory {
    std::vector<Region> regions;unsigned calls=0,hits=0;uintptr_t fail=0,mutate=0;
    void add(uintptr_t p,size_t n){regions.push_back({p,std::vector<uint8_t>(n)});}
    void put(uintptr_t p,const void*v,size_t n){
        for(auto&r:regions)if(p>=r.address&&p-r.address<=r.bytes.size()&&n<=r.bytes.size()-(p-r.address)){
            memcpy(r.bytes.data()+p-r.address,v,n);return;
        }assert(false);
    }
    template<class T>void put(uintptr_t p,T v){put(p,&v,sizeof(v));}
    bool operator()(uintptr_t p,void*out,size_t n){
        ++calls;if(p==fail)return false;
        for(const auto&r:regions)if(p>=r.address&&p-r.address<=r.bytes.size()&&n<=r.bytes.size()-(p-r.address)){
            memcpy(out,r.bytes.data()+p-r.address,n);
            if(p==mutate&&++hits>1)static_cast<uint8_t*>(out)[0]^=1;
            return true;
        }return false;
    }
};
static Memory fixture(f14source::Snapshot&s){
    Memory m; m.add(handle,120);m.add(inner_ptr,24);m.add(pools,16);m.add(slots,1024*56);
    m.add(bases,64*4);m.add(descs,64*32);m.add(arena,1024);
    m.add(hal+0x5b8b28,8);m.add(hal+0x5c2b58,4);m.add(hal+0x96b180,1);m.add(hal+0x96b3b0,0x24);
    m.put(handle,uint32_t(0x28913080));m.put(handle+8,pools);m.put(handle+16,pools+16);
    m.put(handle+0x5c,uint32_t(1024));m.put(handle+0x68,inner_ptr);
    m.put(inner_ptr,Inner{hal+0x5b8b28,slots,1024,0});m.put(pools,Pool{arena,1024,0});
    m.put(hal+0x5b8b28,uintptr_t(hal+0x50e640));m.put(hal+0x5c2b58,uint32_t(1024));
    m.put(hal+0x96b180,uint8_t(1));m.put(hal+0x96b3b0,bases);m.put(hal+0x96b3c8,descs);
    m.put(hal+0x96b3d0,uint32_t(64));m.put(bases+0x33*4,uint32_t(500));
    m.put(descs+0x33*32+8,uint32_t(0x80330000));m.put(descs+0x33*32+12,uint32_t(100));
    s.walk_complete=true;s.header_stable=true;
    for(unsigned i=0;i<3;++i){
        const auto&t=targets[i];unsigned index=i==2?500+0x13a:t.tag,offset=i*80;
        Slot slot{};slot.data=arena+offset;slot.count=t.count;slot.expected=t.bytes;
        slot.tag=t.tag;slot.index=index;slot.max_size=t.bytes;slot.offset=offset;
        m.put(slots+index*56,slot);
        auto&sv=s.values[t.source_index];sv.state=f14source::present_nonzero;sv.bytes=t.bytes;
        for(unsigned j=0;j<t.bytes;++j)sv.payload[j]=uint8_t(i+j+1);
        m.put(arena+offset,sv.payload,t.bytes);
    }return m;
}
int main(){
    f14source::Snapshot source{};auto m=fixture(source);
    auto s=capture(handle,hal,source,m);
    assert(s.structure_stable&&s.vendor_ready&&s.count==1024&&s.reads<=40);
    for(const auto&v:s.values)assert(v.read&&v.stable&&v.compared&&v.equal);
    assert(m.calls==s.reads);
    auto mismatch=m;mismatch.calls=0;mismatch.put(arena,uint8_t(99));
    auto a=capture(handle,hal,source,mismatch);assert(a.values[0].compared&&!a.values[0].equal);
    auto unread=m;unread.calls=0;unread.fail=arena;
    auto b=capture(handle,hal,source,unread);assert(!b.values[0].read&&!b.values[0].compared);
    auto badpool=m;badpool.calls=0;badpool.put(slots+2*56+0xc,uint32_t(3));
    auto c=capture(handle,hal,source,badpool);assert(!c.values[0].read);
    auto badptr=m;badptr.calls=0;badptr.put(slots+2*56,uintptr_t(arena+1020));
    assert(!capture(handle,hal,source,badptr).values[0].read);
    auto badtag=m;badtag.calls=0;badtag.put(slots+2*56+0x20,uint32_t(1));
    assert(!capture(handle,hal,source,badtag).values[0].read);
    auto wrong=m;wrong.calls=0;wrong.put(handle+0x70,uint32_t(1));
    assert(!capture(handle,hal,source,wrong).structure_stable);
    auto global=m;global.calls=0;global.put(hal+0x5c2b58,uint32_t(1025));
    assert(!capture(handle,hal,source,global).structure_stable);
    auto vendor=m;vendor.calls=0;vendor.put(hal+0x96b180,uint8_t(0));
    auto d=capture(handle,hal,source,vendor);assert(!d.vendor_ready&&d.values[0].equal&&!d.values[2].compared);
    auto race=m;race.calls=0;race.mutate=slots+2*56;
    auto e=capture(handle,hal,source,race);assert(!e.values[0].stable&&!e.values[0].compared);
    auto hrace=m;hrace.calls=0;hrace.mutate=handle;
    auto f=capture(handle,hal,source,hrace);assert(!f.structure_stable&&!f.values[0].compared);
    auto sourcebad=source;sourcebad.header_stable=false;auto sm=m;sm.calls=0;
    assert(!capture(handle,hal,sourcebad,sm).values[0].compared);
    puts("PASS three_source_destination_matches mismatch unreadable_payload bad_pool bad_pointer wrong_tag hash_variant global_count vendor_guard slot_race handle_race unstable_source bounded40 zero_native_calls");
}
