// Host-only fixtures for bounds/failure handling; never attaches to a device.
#include "f14_source_snapshot.h"
#include <vector>
#include <assert.h>
#include <stdio.h>
using namespace f14source;
static constexpr uintptr_t base=0x10000;
struct Memory {
    std::vector<uint8_t> bytes;unsigned calls=0,fail_call=0;bool change_header=false;
    bool operator()(uintptr_t p,void*out,size_t n){
        ++calls;if(calls==fail_call || p<base || p-base>bytes.size() || n>bytes.size()-(p-base))return false;
        memcpy(out,bytes.data()+p-base,n);
        if(change_header && p==base && calls>1)static_cast<Header*>(out)->flags^=1;
        return true;
    }
};
static Memory fixture(unsigned entries=242,unsigned capacity=256){
    Memory m;Header h{};h.version=1;h.count=entries;h.capacity=capacity;h.entries=48;
    h.data=48+capacity*16;h.data_capacity=2048;h.data_count=1024;h.size=h.data+h.data_capacity;
    h.vendor=UINT64_MAX;m.bytes.resize(h.size);memcpy(m.bytes.data(),&h,48);
    for(unsigned i=0;i<entries;++i){Entry e{0xf0000000+i,1,i,1,{0,0,0}};memcpy(m.bytes.data()+48+i*16,&e,16);}
    return m;
}
static void set(Memory&m,unsigned i,Entry e){memcpy(m.bytes.data()+48+i*16,&e,16);}
int main(){
    auto m=fixture();
    set(m,0,{2,4,0,2,{0,0,0}});
    set(m,1,{0x8033002c,1,0x42c80000,2,{0,0,0}});
    set(m,2,{0x80330059,7,16,2,{0,0,0}});
    set(m,3,{0x80000000,12,48,0,{0,0,0}});
    Header h;memcpy(&h,m.bytes.data(),48);
    float gains[]={2,1,1,1.5f};memcpy(m.bytes.data()+h.data,gains,16);
    uint32_t blob[]={1,0x30000001,0x12345678};memcpy(m.bytes.data()+h.data+48,blob,12);
    auto a=capture(base,0x80000000,m);
    assert(a.header_valid&&a.walk_complete&&a.header_stable&&a.header.capacity==256);
    assert(a.values[0].state==present_nonzero&&a.values[0].bytes==16);
    assert(a.values[1].state==absent&&a.values[9].state==present_nonzero);
    assert(a.values[12].state==zero_bytes&&a.values[13].bytes==8);
    assert(a.reads==m.calls&&a.reads<=62);
    auto broken=m;broken.calls=0;broken.fail_call=2;
    auto b=capture(base,0x80000000,broken);assert(!b.walk_complete&&b.values[1].state==unknown);
    auto payload=m;payload.calls=0;payload.fail_call=3;
    auto c=capture(base,0x80000000,payload);assert(c.values[0].state==unreadable);
    auto malformed=m;set(malformed,0,{2,4,0xfffffffc,2,{0,0,0}});malformed.calls=0;
    auto d=capture(base,0x80000000,malformed);assert(d.table_invalid&&d.values[0].state==unreadable);
    auto wrong=m;set(wrong,0,{2,4,0,1,{0,0,0}});wrong.calls=0;
    assert(capture(base,0x80000000,wrong).values[0].state==wrong_type_or_count);
    auto dup=m;set(dup,4,{2,4,0,2,{0,0,0}});dup.calls=0;
    assert(capture(base,0x80000000,dup).values[0].state==duplicate);
    auto race=m;race.calls=0;race.change_header=true;
    auto r=capture(base,0x80000000,race);assert(!r.header_stable&&r.values[1].state==unknown);
    auto limit=fixture(385,400);assert(capture(base,0x80000000,limit).limit_exceeded&&limit.calls==1);
    auto high=fixture(384,400);assert(capture(base,0x80000000,high).walk_complete&&high.calls<=62);
    auto align=m;align.calls=0;assert(!capture(base+1,0x80000000,align).header_valid);
    // Header fuzz: no out-of-range direct read; sanitizers check our own parser.
    uint32_t seed=0x1931;
    for(unsigned i=0;i<10000;++i){
        auto f=fixture(1,1);seed=seed*1664525+1013904223;
        unsigned field=(seed>>16)%9;memcpy(f.bytes.data()+field*4,&seed,4);
        auto s=capture(base,0x80000000,f);assert(s.reads<=62);
    }
    puts("PASS valid242 spare_capacity inline/out_of_line stats_zero blob_header_only missing_chunk unreadable_payload bad_offset wrong_type duplicate header_race count_limit384 alignment header_fuzz10000 read_budget62");
}
