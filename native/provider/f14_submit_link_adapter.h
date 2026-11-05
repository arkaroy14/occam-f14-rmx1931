#pragma once
#include <stddef.h>
#include "f14_submit_freshness.h"
// Isolated observation build only: exact HAL/CHI publication gates still apply.
using HalPipelineSubmit=uint32_t(*)(void*,const void*);
static std::atomic<HalPipelineSubmit> submit_original{nullptr};
static std::atomic<unsigned> submit_copy_budget{0},submit_scan_budget{0},submit_log_budget{0};
static std::atomic<bool> submit_copy_limit_reported{false},submit_scan_limit_reported{false},submit_log_limit_reported{false};
static thread_local f14submit::Witness submit_witness;
static bool submit_budget(std::atomic<unsigned>&counter,unsigned limit){
    unsigned current=counter.load();
    while(current<limit)if(counter.compare_exchange_weak(current,current+1))return true;
    return false;
}
static const char*submit_state_name(f14submit::State state){
    switch(state){
      case f14submit::State::linked:return "copy_frame_handle_match";
      case f14submit::State::no_copy:return "no_copy_witness";
      case f14submit::State::invalid_record:return "invalid_record";
      case f14submit::State::epoch_changed:return "epoch_changed";
      case f14submit::State::frame_changed:return "frame_changed";
      case f14submit::State::handle_changed:return "handle_changed";
    }return "unknown";
}
extern "C" __attribute__((used,visibility("hidden")))
uint32_t f14_copy_frame_dispatch(void*handle,const void*source,uint64_t frame,uintptr_t caller){
    int before=errno;
    submit_witness.begin_copy();
    uintptr_t base=tuning_chi_base.load();uint64_t epoch=tuning_submit_epoch.load();
    bool qualified=base&&caller==base+0xc9748&&tuning_recent();
    bool observe=qualified&&submit_budget(submit_copy_budget,4);
    if(qualified&&!observe&&!submit_copy_limit_reported.exchange(true))
        __android_log_print(ANDROID_LOG_WARN,"OcSubmit","COPY_LIMIT max=4 subsequent_copy_events_unobserved=1");
    errno=before;uint32_t result=metadata_original_android.load()(handle,source);int after=errno;
    bool same_epoch=epoch==tuning_submit_epoch.load();
    if(observe&&same_epoch&&tuning_live()){
        submit_witness.end_copy(epoch,frame,reinterpret_cast<uintptr_t>(handle),
            reinterpret_cast<uintptr_t>(source),result,true);
        __android_log_print(ANDROID_LOG_INFO,"OcSubmit",
            "COPY_FRAME epoch=%llu frame=%llu handle=%p source=%p result=%u original_call_executed=1 captured_register=x25 tag_completeness_unobserved=1",
            static_cast<unsigned long long>(epoch),static_cast<unsigned long long>(frame),handle,source,result);
    }
    errno=after;return result;
}
#if defined(__aarch64__)
extern "C" __attribute__((naked,used,visibility("hidden")))
uint32_t f14_copy_frame_entry(void*,const void*){
    // Caller c96e8 loaded full CHI frame from sp+268 into callee-saved x25.
    // Tail-pass it and the original LR before any C++ prologue. x0/x1, SP,
    // x18 and all callee-saved registers remain untouched by this veneer.
    __asm__ volatile("mov x2, x25\n" "mov x3, x30\n" "b f14_copy_frame_dispatch\n");
}
#else
static uint32_t f14_copy_frame_entry(void*handle,const void*source){
    return f14_copy_frame_dispatch(handle,source,0,0); // forwarding fixture only
}
#endif
struct SubmitHeader {uintptr_t session;uint32_t count,padding;uintptr_t records;};
struct SubmitRecord {
    uint64_t frame;uintptr_t pipeline;uint32_t input_count,pad0;uintptr_t input;
    uint32_t output_count,pad1;uintptr_t output,reserved,input_meta,output_meta,private_data;
};
static_assert(sizeof(SubmitHeader)==24&&sizeof(SubmitRecord)==80,"pinned F14 record sizes");
static_assert(offsetof(SubmitRecord,input_meta)==0x38,"pinned input metadata field");
static uint32_t submit_observe(void*context,const void*request){
    int before=errno;
    auto copy=submit_witness.take(); // consume before reads/logging/original call
    uint64_t epoch=tuning_submit_epoch.load();
    SubmitHeader header{};SubmitRecord record{};bool record_read=false,observe=false;
    if(tuning_live()&&(copy.valid||tuning_recent())){
        bool scan=submit_budget(submit_scan_budget,64);
        if(!scan&&!submit_scan_limit_reported.exchange(true))
            __android_log_print(ANDROID_LOG_WARN,"OcSubmit","SCAN_LIMIT max=64 subsequent_submissions_unobserved=1");
        if(scan&&tuning_read(reinterpret_cast<uintptr_t>(request),&header,sizeof(header))&&header.count==1&&header.records){
            record_read=tuning_read(header.records,&record,sizeof(record));
            bool input=record_read&&record.input_count==1&&record.input;
            observe=input&&submit_budget(submit_log_budget,4);
            if(input&&!observe&&!submit_log_limit_reported.exchange(true))
                __android_log_print(ANDROID_LOG_WARN,"OcSubmit","SUBMIT_LIMIT max=4 subsequent_records_unobserved=1");
        }
    }
    auto link=f14submit::Witness::match(copy,epoch,record.frame,record.input_meta,observe);
    bool pre_epoch_stable=epoch==tuning_submit_epoch.load();
    errno=before;uint32_t result=submit_original.load()(context,request);int after=errno;
    if(observe)__android_log_print(ANDROID_LOG_INFO,"OcSubmit",
        "SUBMIT_FRAME epoch=%llu frame=%llu context=%p session=%p pipeline=%p input=%p input_meta=%p output_meta=%p result=%u copy_status=%s copy_epoch=%llu copy_frame=%llu copy_handle=%p copy_source=%p copy_sequence=%llu pre_epoch_stable=%d post_epoch_stable=%d metadata_payload_unobserved=1 bound_slot_unobserved=1 final_ISP_consumption_unobserved=1",
        static_cast<unsigned long long>(epoch),static_cast<unsigned long long>(record.frame),context,
        reinterpret_cast<void*>(header.session),reinterpret_cast<void*>(record.pipeline),reinterpret_cast<void*>(record.input),
        reinterpret_cast<void*>(record.input_meta),reinterpret_cast<void*>(record.output_meta),result,submit_state_name(link.state),
        static_cast<unsigned long long>(copy.epoch),static_cast<unsigned long long>(copy.frame),
        reinterpret_cast<void*>(copy.handle),reinterpret_cast<void*>(copy.source),static_cast<unsigned long long>(copy.sequence),
        pre_epoch_stable,epoch==tuning_submit_epoch.load());
    errno=after;return result;
}
