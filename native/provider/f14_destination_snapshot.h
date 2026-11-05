#pragma once
#include "f14_source_snapshot.h"
// Only the pinned HAL's indexed variant. No virtual/native metadata API call.
namespace f14dest {
struct Inner {uintptr_t vtable,slots;uint32_t count,padding;};
struct Pool {uintptr_t base;uint32_t capacity,state;};
struct Slot {
    uintptr_t data;uint32_t count,pool,offset,expected;uint64_t unused;
    uint32_t tag,index,max_size;uint8_t tail[12];
};
static_assert(sizeof(Inner)==24&&sizeof(Pool)==16&&sizeof(Slot)==56,"pinned native slot ABI");
static_assert(offsetof(Slot,tag)==32&&offsetof(Slot,max_size)==40,"native slot identity ABI");
struct Target {uint32_t tag,count,bytes;size_t source_index;};
static constexpr Target targets[]={{2,4,16,0},{1,9,72,1},{0x80330059,7,28,12}};
struct Value {
    const char*status="not_observed";Slot slot{};Pool pool{};uint32_t index=UINT32_MAX;
    bool read=false,stable=false,compared=false,equal=false;uint8_t payload[72]{};
};
struct Snapshot {
    const char*status="not_observed";unsigned reads=0;uint32_t count=0,pools=0,global_count=0;
    uintptr_t inner=0;bool structure_stable=false,vendor_ready=false;Value values[3];
};
static uint32_t u32(const uint8_t*p,size_t off){uint32_t v;memcpy(&v,p+off,4);return v;}
static uintptr_t ptr(const uint8_t*p,size_t off){uintptr_t v;memcpy(&v,p+off,sizeof(v));return v;}
static bool span(uintptr_t p,uint64_t bytes){return p>=4096&&bytes<=UINTPTR_MAX-p;}
template<class Reader> struct Reads {
    Reader&reader;unsigned calls=0;
    bool operator()(uintptr_t p,void*out,size_t bytes){
        if(calls>=40||bytes>128||!span(p,bytes))return false;
        ++calls;return reader(p,out,bytes);
    }
};
template<class Reader> static Snapshot capture(uintptr_t handle,uintptr_t hal,
        const f14source::Snapshot&source,Reader&reader){
    Snapshot out;Reads<Reader> read{reader};
    auto run=[&](){
        uint8_t h[120]{};
        if(!read(handle,h,sizeof(h))){out.status="handle_unreadable";return;}
        if(u32(h,0)!=0x28913080){out.status="magic_mismatch";return;}
        out.count=u32(h,0x5c);out.inner=ptr(h,0x68);
        if(u32(h,0x70)!=0){out.status="unsupported_inner_variant";return;}
        uintptr_t begin=ptr(h,8),end=ptr(h,16);
        if(!span(begin,0)||end<begin||(end-begin)%16||(end-begin)/16>128){out.status="pool_vector_bounds";return;}
        out.pools=(end-begin)/16;
        if(!out.count||out.count>4096){out.status="slot_count_limit";return;}
        Inner inner{};uintptr_t lookup=0;
        if(!read(out.inner,&inner,sizeof(inner)) || !span(hal,0x96b3d4)
                || !read(hal+0x5c2b58,&out.global_count,4)){
            out.status="layout_unreadable";return;
        }
        if(inner.vtable!=hal+0x5b8b28||inner.count!=out.count||out.global_count!=out.count
                ||!span(inner.slots,uint64_t(out.count)*56)){
            out.status="indexed_layout_mismatch";return;
        }
        if(!read(inner.vtable,&lookup,sizeof(lookup))||lookup!=hal+0x50e640){out.status="lookup_identity_mismatch";return;}
        uint8_t guard=0;uintptr_t bases=0,descs=0;uint32_t sections=0,base=0,desc[4]{};
        out.vendor_ready=read(hal+0x96b180,&guard,1)&&(guard&1)
            &&read(hal+0x96b3b0,&bases,8)&&read(hal+0x96b3c8,&descs,8)
            &&read(hal+0x96b3d0,&sections,4)&&sections>0x33&&sections<=256
            &&span(bases,uint64_t(sections)*4)&&span(descs,uint64_t(sections)*32)
            &&read(bases+0x33*4,&base,4)&&read(descs+0x33*32,desc,16)
            &&desc[2]==0x80330000&&desc[3]>0x59
            &&uint64_t(base)+0x13a<out.count;
        for(size_t i=0;i<3;++i){
            Value&v=out.values[i];const auto&t=targets[i];
            if(i==2&&!out.vendor_ready){v.status="vendor_index_unproven";continue;}
            v.index=i==2?base+0x13a:t.tag;
            uintptr_t address=inner.slots+uint64_t(v.index)*56;
            if(!read(address,&v.slot,sizeof(Slot))){v.status="slot_unreadable";continue;}
            const auto&s=v.slot;
            if(s.index!=v.index){v.status="slot_index_mismatch";continue;}
            if(!s.data){v.status="unpopulated_observed";continue;}
            if(s.tag!=t.tag||s.count!=t.count||s.expected!=t.bytes||s.max_size<t.bytes){
                v.status="slot_tag_shape_mismatch";continue;
            }
            if(s.pool>=out.pools){v.status="pool_index_bounds";continue;}
            uintptr_t pool_address=begin+uint64_t(s.pool)*16;
            if(!read(pool_address,&v.pool,sizeof(Pool))){v.status="pool_unreadable";continue;}
            if(v.pool.capacity>32*1024*1024||uint64_t(s.offset)+t.bytes>v.pool.capacity
                    ||!span(v.pool.base,v.pool.capacity)||s.data!=v.pool.base+s.offset){
                v.status="payload_pool_bounds";continue;
            }
            v.read=read(s.data,v.payload,t.bytes);
            if(!v.read){v.status="payload_unreadable";continue;}
            Slot after{};Pool pool_after{};uint8_t again[72]{};
            v.stable=read(s.data,again,t.bytes)&&read(address,&after,sizeof(Slot))
                &&read(pool_address,&pool_after,sizeof(Pool))
                &&memcmp(again,v.payload,t.bytes)==0&&memcmp(&after,&s,sizeof(Slot))==0
                &&memcmp(&pool_after,&v.pool,sizeof(Pool))==0;
            v.status=v.stable?"payload_observed_stable":"payload_or_slot_changed";
            const auto&sv=source.values[t.source_index];
            v.compared=v.stable&&source.walk_complete&&source.header_stable&&sv.bytes==t.bytes
                &&(sv.state==f14source::present_nonzero||sv.state==f14source::zero_bytes);
            v.equal=v.compared&&memcmp(v.payload,sv.payload,t.bytes)==0;
        }
        uint8_t after[120]{};Inner inner_after{};
        out.structure_stable=read(handle,after,sizeof(after))&&read(out.inner,&inner_after,sizeof(Inner))
            &&u32(after,0)==u32(h,0)&&ptr(after,8)==begin&&ptr(after,16)==end
            &&u32(after,0x5c)==out.count&&ptr(after,0x68)==out.inner&&u32(after,0x70)==0
            &&memcmp(&inner_after,&inner,sizeof(Inner))==0;
        if(!out.structure_stable)for(auto&v:out.values){v.compared=false;v.equal=false;}
        out.status=out.structure_stable?"selected_fields_observed":"handle_structure_changed";
    };
    run();out.reads=read.calls;return out;
}
} // namespace f14dest
