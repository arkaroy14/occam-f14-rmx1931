#pragma once
static std::atomic<uintptr_t> metadata_hal_base{0};
#ifdef OCCAM_METADATA_SOURCE_SNAPSHOT
#include "f14_source_snapshot.h"
#ifdef OCCAM_METADATA_DESTINATION_SNAPSHOT
#include "f14_destination_snapshot.h"
#endif
static std::atomic<unsigned> metadata_source_snapshots{0};
static void metadata_observe_source(void*handle,const void*source,uint32_t result){
    // Four snapshots per provider lifetime, regardless of session reconfigure.
    unsigned event=metadata_source_snapshots.fetch_add(1);
    if(event>=4)return;
    uint32_t blob=0;bool blob_ok=tuning_read(reinterpret_cast<uintptr_t>(handle)+0x90,&blob,sizeof(blob));
    // Invalid selector cannot silently fall back to an assumed private tag.
    auto reader=[](uintptr_t p,void*out,size_t n){return tuning_read(p,out,n);};
    auto s=f14source::capture(reinterpret_cast<uintptr_t>(source),blob_ok?blob:0xffffffff,reader);
#ifdef OCCAM_METADATA_DESTINATION_SNAPSHOT
    if(result==0){
        auto d=f14dest::capture(reinterpret_cast<uintptr_t>(handle),metadata_hal_base.load(),s,reader);
        __android_log_print(ANDROID_LOG_INFO,"OcMetaOps",
            "DEST_HEADER event=%u handle=%p status=%s slots=%u global_slots=%u pools=%u reads=%u structure_stable=%d vendor_index_ready=%d selected_tags_only=1 final_ISP_consumption_unobserved=1 atomic_snapshot_unproven=1",
            event,handle,d.status,d.count,d.global_count,d.pools,d.reads,d.structure_stable,d.vendor_ready);
        for(size_t i=0;i<3;++i){
            const auto&v=d.values[i];char hex[145]{};static constexpr char digits[]="0123456789abcdef";
            if(v.read)for(unsigned j=0;j<f14dest::targets[i].bytes;++j){hex[j*2]=digits[v.payload[j]>>4];hex[j*2+1]=digits[v.payload[j]&15];}
            __android_log_print(ANDROID_LOG_INFO,"OcMetaOps",
                "DEST_TAG event=%u handle=%p tag=%x index=%u status=%s slot_tag=%x slot_index=%u count=%u expected_bytes=%u max_size=%u pool=%u offset=%u pool_capacity=%u payload_read=%d stable=%d compared=%d source_equal=%d hex=%s",
                event,handle,f14dest::targets[i].tag,v.index,v.status,v.slot.tag,v.slot.index,v.slot.count,v.slot.expected,v.slot.max_size,
                v.slot.pool,v.slot.offset,v.pool.capacity,v.read,v.stable,v.compared,v.equal,hex);
        }
    }
#endif
    const auto&h=s.header;
    __android_log_print(ANDROID_LOG_INFO,"OcMetaOps",
        "SOURCE_HEADER event=%u caller=c9748 handle=%p source=%p result=%u header_read=%d header_valid=%d limit_exceeded=%d size=%u version=%u flags=%x entries=%u capacity=%u entries_start=%u data_count=%u data_capacity=%u data_start=%u vendor=%llx walked=%u walk_complete=%d header_stable=%d table_invalid=%d reads=%u blob_tag=%x blob_read=%d native_handle_complete_contents_unobserved=1 frame_association_unproven=1 stats_setter_return_unobserved=1 atomic_snapshot_unproven=1",
        event,handle,source,result,s.header_read,s.header_valid,s.limit_exceeded,h.size,h.version,h.flags,h.count,h.capacity,
        h.entries,h.data_count,h.data_capacity,h.data,static_cast<unsigned long long>(h.vendor),s.walked,s.walk_complete,
        s.header_stable,s.table_invalid,s.reads+1,blob,blob_ok);
    for(size_t i=0;i<f14source::target_count;++i){
        const auto&v=s.values[i];char hex[145]{};static constexpr char digits[]="0123456789abcdef";
        for(unsigned j=0;j<v.bytes;++j){hex[j*2]=digits[v.payload[j]>>4];hex[j*2+1]=digits[v.payload[j]&15];}
        __android_log_print(ANDROID_LOG_INFO,"OcMetaOps",
            "SOURCE_TAG event=%u source=%p tag=%x name=%s state=%s seen=%u type=%u count=%u sampled_bytes=%u hex=%s payload_scope=%s",
            event,source,i==f14source::target_count-1?blob:f14source::targets[i].tag,f14source::targets[i].name,
            f14source::state_name(v.state),v.seen,v.type,v.count,v.bytes,hex,i==f14source::target_count-1?"header_only":"whole_selected_scalar");
    }
}
#endif
// Original HAL-published callbacks; no instruction/GOT patch or synthetic result.
using HalEntry = uintptr_t(*)(void*);
using HalGetOps = uintptr_t(*)(void*);
using HalMetaGet = uint32_t(*)(void*,uint32_t,void**);
using HalMetaSet = uint32_t(*)(void*,uint32_t,const void*,uint32_t);
using HalSetAndroid = uint32_t(*)(void*,const void*);
static std::atomic<HalEntry> metadata_hal_entry{nullptr};
static std::atomic<HalGetOps> metadata_get_ops{nullptr};
static std::atomic<HalMetaGet> metadata_original_get{nullptr};
static std::atomic<HalMetaSet> metadata_original_set{nullptr};
static std::atomic<HalSetAndroid> metadata_original_android{nullptr};
static std::atomic<unsigned> metadata_ready_reports{0};
static bool metadata_budget(){
    if(!tuning_live())return false;
    unsigned event=tuning_metadata_count.fetch_add(1);
    if(event==128)__android_log_print(ANDROID_LOG_WARN,"OcMetaOps","COVERAGE_LIMIT events=128 further_metadata_events_unobserved=1");
    return event<128;
}
static uintptr_t metadata_chi_rva(const void* caller){
    uintptr_t base=tuning_chi_base.load(),value=reinterpret_cast<uintptr_t>(caller);
    return base&&value>=base&&value-base<0xf6470?value-base:0;
}
#ifdef OCCAM_METADATA_SUBMIT_LINK
#include "f14_submit_link_adapter.h"
#endif
static uint32_t metadata_observe_get(void*handle,uint32_t tag,void**out){
    int before=errno;uintptr_t caller=metadata_chi_rva(__builtin_return_address(0));
    errno=before;uint32_t result=metadata_original_get.load()(handle,tag,out);int after=errno;
    // Record only getters the original tuning code actually executed. Sizes
    // below are the original caller's own loads, not inferred tag type sizes.
    size_t bytes=caller==0xeaa94?16:(caller==0xeaae0?1:((caller==0xeaa1c||caller==0xeaa44)?4:0));
    if(bytes&&tuning_call_context.observe&&metadata_budget()){
        uintptr_t data=0;uint32_t words[4]{};
        bool pointer_ok=tuning_read(reinterpret_cast<uintptr_t>(out),&data,sizeof(data));
        bool data_ok=result==0&&pointer_ok&&tuning_read(data,words,bytes);
        __android_log_print(ANDROID_LOG_INFO,"OcMetaOps",
            "GET caller=%lx tuning_call=%lx handle=%p tag=%x result=%u data=%p bytes=%zu read_ok=%d words=%08x,%08x,%08x,%08x",
            static_cast<unsigned long>(caller),static_cast<unsigned long>(tuning_call_context.call),handle,tag,result,
            reinterpret_cast<void*>(data),bytes,data_ok,words[0],words[1],words[2],words[3]);
    }
    errno=after;return result;
}
static uint32_t metadata_observe_set(void*handle,uint32_t tag,const void*data,uint32_t elements){
    int before=errno;uintptr_t caller=metadata_chi_rva(__builtin_return_address(0));
    bool observe=caller==0xeabb4&&elements==0x3c&&tuning_call_context.observe&&metadata_budget();
    // The original Fill stack constructs this 60-byte local record. The API's
    // element count is kept separate; no runtime tag type/byte length assumed.
    uint32_t words[15]{};bool read_ok=observe&&tuning_read(reinterpret_cast<uintptr_t>(data),words,sizeof(words));
    errno=before;uint32_t result=metadata_original_set.load()(handle,tag,data,elements);int after=errno;
    if(observe)__android_log_print(ANDROID_LOG_INFO,"OcMetaOps",
        "TUNING_SET caller=eabb4 tuning_call=%lx handle=%p tag=%x elements=%u sampled_stack_bytes=60 read_ok=%d result=%u words=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x",
        static_cast<unsigned long>(tuning_call_context.call),handle,tag,elements,read_ok,result,
        words[0],words[1],words[2],words[3],words[4],words[5],words[6],words[7],words[8],words[9],words[10],words[11],words[12],words[13],words[14]);
    errno=after;return result;
}
static uint32_t metadata_observe_android(void*handle,const void*source){
    int before=errno;uintptr_t caller=metadata_chi_rva(__builtin_return_address(0));
#ifdef OCCAM_METADATA_COPY_CONTRACT_ONLY
    bool observe=caller==0xc9748&&tuning_recent()&&metadata_budget();
#else
    bool observe=tuning_live()&&((caller==0xbe4b4&&reinterpret_cast<uintptr_t>(source)==tuning_request_settings.load())
        ||(caller==0xc9748&&tuning_recent()))&&metadata_budget();
#endif
    errno=before;uint32_t result=metadata_original_android.load()(handle,source);int after=errno;
#ifdef OCCAM_METADATA_COPY_CONTRACT_ONLY
    if(observe){
#ifdef OCCAM_METADATA_SOURCE_SNAPSHOT
        metadata_observe_source(handle,source,result);
#else
        uintptr_t inner=0,vtable=0,lookup=0;uint32_t count=0,blob=0;
        // Current vendor libcamera_metadata SHAca8bf272... accessor@a510 is
        // exactly LDRw0,[x0,#0xc];RET. Controller pins that library. Do not call
        // pGetTag or infer an entry count/type from its returned payload.
        bool count_ok=tuning_read(reinterpret_cast<uintptr_t>(source)+0xc,&count,sizeof(count));
        bool inner_ok=tuning_read(reinterpret_cast<uintptr_t>(handle)+0x68,&inner,sizeof(inner));
        bool vtable_ok=inner_ok&&tuning_read(inner,&vtable,sizeof(vtable));
        bool lookup_ok=vtable_ok&&tuning_read(vtable,&lookup,sizeof(lookup));
        bool blob_ok=tuning_read(reinterpret_cast<uintptr_t>(handle)+0x90,&blob,sizeof(blob));
        uintptr_t base=metadata_hal_base.load();
        __android_log_print(ANDROID_LOG_INFO,"OcMetaOps",
            "COPY_CONTRACT caller=c9748 handle=%p source=%p result=%u source_entries=%u count_ok=%d inner=%p inner_ok=%d vtable=%p vtable_ok=%d vtable_hal_rva=%lx lookup=%p lookup_ok=%d lookup_hal_rva=%lx blob_tag=%x blob_ok=%d original_call_executed=1 no_extra_getter=1 tag_copy_completeness_unobserved=1 frame_unobserved=1 temporal_window_only=1",
            handle,source,result,count,count_ok,reinterpret_cast<void*>(inner),inner_ok,
            reinterpret_cast<void*>(vtable),vtable_ok,static_cast<unsigned long>(vtable_ok&&vtable>=base?vtable-base:0),
            reinterpret_cast<void*>(lookup),lookup_ok,static_cast<unsigned long>(lookup_ok&&lookup>=base?lookup-base:0),blob,blob_ok);
#endif
    }
#else
    if(observe)__android_log_print(ANDROID_LOG_INFO,"OcMetaOps",
        "SET_ANDROID caller=%lx handle=%p source=%p result=%u original_call_executed=1 frame_unobserved=1 temporal_window_only=%d",
        static_cast<unsigned long>(caller),handle,source,result,caller==0xc9748);
#endif
    errno=after;return result;
}
static uintptr_t metadata_observe_ops(void*table){
    // Raw x0 return is retained even though the published table filler is
    // conventionally void. Both original functions always run unchanged.
    uintptr_t result=metadata_get_ops.load()(table);int after=errno;
    if(table){
        auto**ops=static_cast<void**>(table);uintptr_t base=metadata_hal_base.load();
        if(ops[4]==reinterpret_cast<void*>(base+0x4b4d20)
            &&ops[8]==reinterpret_cast<void*>(base+0x4b4e58)
            &&ops[10]==reinterpret_cast<void*>(base+0x4b51e8)
            &&ops[13]==reinterpret_cast<void*>(base+0x4b5f70)){
            metadata_original_get.store(reinterpret_cast<HalMetaGet>(ops[4]));
            metadata_original_set.store(reinterpret_cast<HalMetaSet>(ops[8]));
            metadata_original_android.store(reinterpret_cast<HalSetAndroid>(ops[10]));
#ifndef OCCAM_METADATA_COPY_CONTRACT_ONLY
            ops[4]=reinterpret_cast<void*>(metadata_observe_get);
            ops[8]=reinterpret_cast<void*>(metadata_observe_set);
#endif
            ops[10]=reinterpret_cast<void*>(
#ifdef OCCAM_METADATA_SUBMIT_LINK
                f14_copy_frame_entry
#else
                metadata_observe_android
#endif
            );
            if(metadata_ready_reports.fetch_add(1)<4)__android_log_print(ANDROID_LOG_INFO,"OcMetaOps",
#ifdef OCCAM_METADATA_COPY_CONTRACT_ONLY
                "COPY_OP_READY table=%p bytes=296 set_android=10 getter_setter_merge_untouched=1 observation_marker_gated=1",table);
#else
                "OPS_READY table=%p bytes=296 get=4 set=8 set_android=10 pMerge_untouched=13 observation_marker_gated=1",table);
#endif
        }else __android_log_print(ANDROID_LOG_WARN,"OcMetaOps","OPS_IDENTITY_MISMATCH original_table_retained=1");
    }
    errno=after;return result;
}
static uintptr_t metadata_observe_entry(void*table){
    uintptr_t result=metadata_hal_entry.load()(table);int after=errno;
    if(table){auto**context=static_cast<void**>(table);uintptr_t base=metadata_hal_base.load();
#ifdef OCCAM_METADATA_SUBMIT_LINK
        if(context[13]==reinterpret_cast<void*>(base+0x4b1cf8)
            &&context[18]==reinterpret_cast<void*>(base+0x4b2980)){
            submit_original.store(reinterpret_cast<HalPipelineSubmit>(context[13]));
            context[13]=reinterpret_cast<void*>(submit_observe);
            __android_log_print(ANDROID_LOG_INFO,"OcSubmit","SUBMIT_OP_READY slot=13 original_hal_rva=4b1cf8 marker_gated=1 max_scans=64 max_copies=4 max_records=4 post_slot_reads=0");
        }
#endif
        if(context[18]==reinterpret_cast<void*>(base+0x4b2980)){
            metadata_get_ops.store(reinterpret_cast<HalGetOps>(context[18]));
            context[18]=reinterpret_cast<void*>(metadata_observe_ops);
        }
    }
    errno=after;return result;
}
