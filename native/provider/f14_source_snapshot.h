#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Pinned mapped vendor metadata SHA ca8bf272...: a370, ae70, 9db0.
// No API calls or unchecked target dereferences. Defensive limits are observer
// limits, not assertions about the original metadata implementation.
namespace f14source {
struct Header {
    uint32_t size,version,flags,count,capacity,entries,data_count,data_capacity,data;
    uint32_t padding;
    uint64_t vendor;
};
struct Entry { uint32_t tag,count,value; uint8_t type,reserved[3]; };
static_assert(sizeof(Header)==48 && offsetof(Header,vendor)==40,"metadata header ABI");
static_assert(sizeof(Entry)==16 && offsetof(Entry,type)==12,"metadata entry ABI");
struct Target {uint32_t tag,count;uint8_t type;const char*name;};
static constexpr Target targets[]={
    {0x2,4,2,"colorCorrection.gains"}, {0x1,9,5,"colorCorrection.transform"},
    {0xe0002,1,1,"sensor.sensitivity"}, {0xe0000,1,3,"sensor.exposureTime"},
    {0x10028,1,1,"control.postRawSensitivityBoost"},
    {0xe001c,4,2,"sensor.dynamicBlackLevel"}, {0xe0012,3,5,"sensor.neutralColorPoint"},
    {0xd0000,4,1,"scaler.cropRegion"}, {0xe0010,1,3,"sensor.timestamp"},
    {0x8033002c,1,2,"oplus.rawhdr.isp.luxindex"},
    {0x80450006,1,2,"qti.stats_control.drc_gain"},
    {0x80450007,1,2,"qti.stats_control.dark_boost_gain"},
    {0x80330059,7,2,"oplus.supernight.reprocess.stats"},
    {0x80000000,0,0,"private_property"}
};
static constexpr size_t target_count=sizeof(targets)/sizeof(targets[0]);
enum State {unknown,absent,wrong_type_or_count,unreadable,zero_bytes,present_nonzero,duplicate};
static const char*state_name(State s){
    static const char*names[]={"unknown","absent","wrong_type_or_count","unreadable",
        "zero_bytes","present_nonzero","duplicate"};return names[s];
}
struct Value {State state;uint32_t count;uint8_t type;unsigned seen,bytes;uint8_t payload[72];};
struct Snapshot {
    Header header;Value values[target_count];unsigned reads,walked;
    bool header_read,header_valid,limit_exceeded,walk_complete,header_stable,table_invalid;
};
template<class Reader> struct BoundedReader {
    Reader&read;unsigned calls=0;
    bool operator()(uintptr_t p,void*out,size_t n){
        if(calls>=62 || n>128 || p<4096 || p>UINTPTR_MAX-n)return false;
        ++calls;return read(p,out,n);
    }
};
template<class Reader> static Snapshot capture(uintptr_t source,uint32_t blob_tag,Reader&reader){
    Snapshot out{};BoundedReader<Reader> read{reader};
    Header&h=out.header;
    out.header_read=read(source,&h,sizeof(h));
    if(!out.header_read){out.reads=read.calls;return out;}
    out.limit_exceeded=h.size>1024*1024 || h.count>384;
    const uint64_t entries_end=uint64_t(h.entries)+uint64_t(h.capacity)*16;
    const uint64_t data_end=uint64_t(h.data)+h.data_capacity;
    out.header_valid=(source&7)==0 && h.version==1 && h.size>=48
        && source<=UINTPTR_MAX-h.size && h.count<=h.capacity
        && h.data_count<=h.data_capacity && h.entries>=48 && (h.entries&3)==0
        && entries_end<=h.data && (h.data&7)==0 && data_end<=h.size;
    if(!out.header_valid || out.limit_exceeded){out.reads=read.calls;return out;}
    static constexpr unsigned type_size[]={1,4,4,8,8,8};
    for(unsigned start=0;start<h.count;start+=8){
        Entry chunk[8]{};unsigned n=h.count-start;if(n>8)n=8;
        if(!read(source+h.entries+uint64_t(start)*16,chunk,n*sizeof(Entry)))break;
        for(unsigned j=0;j<n;++j){
            const Entry&e=chunk[j];++out.walked;
            if(e.type>=6){out.table_invalid=true;continue;}
            const uint64_t bytes=uint64_t(e.count)*type_size[e.type];
            const uint64_t aligned=(bytes+7)&~uint64_t(7);
            const bool valid=bytes<=4 ? (e.count!=0 || e.value==0)
                : (e.value&7)==0 && uint64_t(e.value)+aligned<=h.data_count
                    && uint64_t(h.data)+e.value+bytes<=h.size;
            if(!valid)out.table_invalid=true;
            for(size_t k=0;k<target_count;++k){
                const bool blob=k==target_count-1;
                if(e.tag!=(blob?blob_tag:targets[k].tag))continue;
                Value&v=out.values[k];
                if(++v.seen>1){v.state=duplicate;continue;}
                v.type=e.type;v.count=e.count;
                if(e.type!=targets[k].type || (blob?e.count<4:e.count!=targets[k].count)){
                    v.state=wrong_type_or_count;continue;
                }
                if(!valid){v.state=unreadable;continue;}
                v.bytes=blob?(e.count>=8?8:4):static_cast<unsigned>(bytes);
                if(bytes<=4)memcpy(v.payload,&e.value,v.bytes);
                else if(!read(source+h.data+e.value,v.payload,v.bytes)){
                    v.bytes=0;v.state=unreadable;continue;
                }
                bool nonzero=false;for(unsigned z=0;z<v.bytes;++z)nonzero|=v.payload[z]!=0;
                v.state=nonzero?present_nonzero:zero_bytes;
            }
        }
    }
    out.walk_complete=out.walked==h.count && !out.table_invalid;
    Header after{};
    out.header_stable=read(source,&after,sizeof(after))&&memcmp(&after,&h,sizeof(h))==0;
    if(out.walk_complete && out.header_stable)
        for(auto&v:out.values)if(v.seen==0)v.state=absent;
    // Header equality detects some races, not in-place payload mutations.
    out.reads=read.calls;return out;
}
} // namespace f14source
