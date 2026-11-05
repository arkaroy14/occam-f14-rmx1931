#pragma once
#include <sys/syscall.h>
#include <sys/uio.h>
#include <time.h>
#include "property_entry_veneer.h"

static std::atomic<uintptr_t> tuning_chi_base{0};
#ifdef OCCAM_METADATA_SUBMIT_LINK
static std::atomic<uint64_t> tuning_submit_epoch{0};
#endif
static std::atomic<bool> tuning_entry_enabled{false};
static std::atomic<unsigned> tuning_entry_count{0};
static std::atomic<bool> tuning_site_reported{false};
static std::atomic<time_t> tuning_entry_deadline{0};
static std::atomic<time_t> tuning_recent_deadline{0};
static std::atomic<uintptr_t> tuning_request_settings{0};
static std::atomic<unsigned> tuning_unknown_count{0};
static std::atomic<unsigned> tuning_metadata_count{0};
struct TuningCallContext { bool observe; uintptr_t call; };
static thread_local TuningCallContext tuning_call_context{};
static constexpr const char* tuning_marker="/data/vendor/camera/ocfresh_tuning_entry_audit";

// Kernel copies from our own address space and reports EFAULT rather than
// dereferencing an unchecked frame/object pointer in the observer.
static bool tuning_read(uintptr_t address, void* out, size_t bytes) {
    if(address<4096 || bytes>128 || address+bytes<address)return false;
    iovec local{out,bytes},remote{reinterpret_cast<void*>(address),bytes};
    return syscall(__NR_process_vm_readv,getpid(),&local,1,&remote,1,0)==static_cast<ssize_t>(bytes);
}
static bool tuning_live(){
    if(!tuning_entry_enabled.load())return false;
    int saved=errno;timespec now{};
    bool live=clock_gettime(CLOCK_MONOTONIC,&now)==0&&now.tv_sec<=tuning_entry_deadline.load();
    errno=saved;return live;
}
static bool tuning_recent(){
    int saved=errno;timespec now{};
    bool recent=tuning_live()&&clock_gettime(CLOCK_MONOTONIC,&now)==0&&now.tv_sec<=tuning_recent_deadline.load();
    errno=saved;return recent;
}
static void tuning_process_request(void* request){
    if(!tuning_live())return;
    int saved=errno;
    struct Request { uint32_t frame,padding;uintptr_t settings,input; } data{};
    if(tuning_read(reinterpret_cast<uintptr_t>(request),&data,sizeof(data))&&data.input){
        timespec now{};
        if(clock_gettime(CLOCK_MONOTONIC,&now)==0){
            tuning_request_settings.store(data.settings);tuning_recent_deadline.store(now.tv_sec+3);
        }
    }
    errno=saved;
}
static void tuning_configure(uint32_t id,const F14HighConfig* config) {
    int saved=errno;
#ifdef OCCAM_METADATA_SUBMIT_LINK
    tuning_submit_epoch.fetch_add(1);
#endif
    bool armed=id==7&&config&&config->mode==0x8019&&access(tuning_marker,R_OK)==0;
    tuning_entry_enabled.store(false);
    if(armed){
        timespec now{};bool clock_ok=clock_gettime(CLOCK_MONOTONIC,&now)==0;
        tuning_entry_count.store(0);tuning_unknown_count.store(0);tuning_site_reported.store(false);tuning_entry_deadline.store(now.tv_sec+180);
        tuning_metadata_count.store(0);
        tuning_recent_deadline.store(0);tuning_request_settings.store(0);
        tuning_entry_enabled.store(clock_ok);
        __android_log_print(ANDROID_LOG_INFO,"OcTuning","ARMED camera=7 mode=8019 max_events=32 ttl_seconds=180 clock_ok=%d base=%p merge_event_unobserved=1",
            clock_ok,reinterpret_cast<void*>(tuning_chi_base.load()));
    }
    errno=saved;
}
static void tuning_observe(const F14PropertyEntry* entry) {
#ifdef OCCAM_METADATA_COPY_CONTRACT_ONLY
    // This build only observes pSetAndroidMetadata. The original property
    // wrapper and all existing functional identity contexts remain unchanged.
    (void)entry;return;
#endif
    if(!tuning_entry_enabled.load()||!entry)return;
    uintptr_t base=tuning_chi_base.load(),lr=entry->x19_x30[11];
    if(!base||lr!=base+0xea928)return;
    tuning_call_context={false,0};
    int saved=errno;timespec now{};
    if(clock_gettime(CLOCK_MONOTONIC,&now)!=0||now.tv_sec>tuning_entry_deadline.load()){
        if(tuning_entry_enabled.exchange(false))__android_log_print(ANDROID_LOG_INFO,"OcTuning","EXPIRED no_more_observations=1");
        errno=saved;return;
    }
    // At ea924 x29=FillSP+f0. Saved caller FP is GenerateSP+11c0.
    // Fill's saved LR distinguishes the two original call sites.
    uintptr_t fp=entry->x19_x30[10],pair[2]{};
    uintptr_t generate_sp=entry->entry_sp+0x140;
    if(fp!=entry->entry_sp+0xf0 || !tuning_read(fp,pair,sizeof(pair))){
        if(tuning_entry_enabled.exchange(false))__android_log_print(ANDROID_LOG_WARN,"OcTuning","FRAME_CONTRACT_UNAVAILABLE observation_disabled=1");
        errno=saved;return;
    }
    if(pair[1]!=base+0xbe3f4&&pair[1]!=base+0xbe4f8){
        // Only Fill's common saved arguments are valid here. Never apply the
        // realtime caller's stack layout or infer an app frame on this arm.
        if(tuning_recent()&&tuning_unknown_count.fetch_add(1)<8){
            tuning_call_context={true,pair[1]-base-4};
            uint32_t word=0;bool word_ok=false;
            if(pair[1]==base+0xc9fa8)word_ok=tuning_read(entry->x19_x30[4],&word,sizeof(word));
            __android_log_print(ANDROID_LOG_INFO,"OcTuning",
                "OTHER_ENTRY call_rva=%lx chi=%p w1_raw=%u w2_raw=%u x5=%p x5_word=%u word_ok=%d app_frame_unobserved=1 path_arm_unproven=1 temporal_window_only=1",
                static_cast<unsigned long>(pair[1]-base-4),reinterpret_cast<void*>(entry->x19_x30[1]),
                static_cast<uint32_t>(entry->x19_x30[5]),static_cast<uint32_t>(entry->x19_x30[0]),
                reinterpret_cast<void*>(entry->x19_x30[4]),word,word_ok);
        }
        errno=saved;return;
    }
    if(pair[0]!=generate_sp+0x11c0){
        if(tuning_entry_enabled.exchange(false))__android_log_print(ANDROID_LOG_WARN,"OcTuning","REALTIME_FRAME_CONTRACT_UNAVAILABLE observation_disabled=1");
        errno=saved;return;
    }
    uintptr_t object=0,request=0;
    struct Request {uint32_t frame,padding;uintptr_t settings,input;} request_data{};
    if(!tuning_read(generate_sp+0x150,&object,sizeof(object))
        ||!tuning_read(generate_sp+0x1d0,&request,sizeof(request))
        ||!tuning_read(request,&request_data,sizeof(request_data))){
        if(tuning_entry_enabled.exchange(false))__android_log_print(ANDROID_LOG_WARN,"OcTuning","REQUEST_READ_UNAVAILABLE observation_disabled=1");
        errno=saved;return;
    }
    if(!tuning_site_reported.exchange(true))__android_log_print(ANDROID_LOG_INFO,"OcTuning",
        "SITE_READY property_return=ea928 tuning_call=%lx frame_chain_verified=1 self_read_verified=1 input_present=%d",
        static_cast<unsigned long>(pair[1]-base-4),request_data.input!=0);
    // Preview also enters tuning. Only processed-input requests are sampled.
    if(!request_data.input){errno=saved;return;}
    tuning_call_context={true,pair[1]-base-4};
    unsigned event=tuning_entry_count.fetch_add(1);
    if(event>=32){
        if(tuning_entry_enabled.exchange(false))__android_log_print(ANDROID_LOG_WARN,"OcTuning","COVERAGE_LIMIT events=32 merge_event_unobserved=1");
        errno=saved;return;
    }
    uint32_t later_flag=0,app_frame=0,feature_word=0;uintptr_t cache=0,pipeline=0;
    bool flag_ok=tuning_read(object+0x1246f0c,&later_flag,sizeof(later_flag));
    bool cache_ok=tuning_read(object+0x1247958,&cache,sizeof(cache));
    bool frame_ok=tuning_read(object+0x1247960,&app_frame,sizeof(app_frame));
    bool feature_ok=tuning_read(entry->x19_x30[4],&feature_word,sizeof(feature_word));
    bool pipeline_ok=tuning_read(entry->entry_sp+0x108,&pipeline,sizeof(pipeline));
    __android_log_print(ANDROID_LOG_INFO,"OcTuning",
        "ENTRY event=%u call_rva=%lx request_frame=%u app_frame=%u app_frame_ok=%d object=%p request=%p settings=%p input=%p chi=%p usecase_submode=%u sensor_mode_index=%u feature_word=%u feature_ok=%d pipeline_index=%lu pipeline_ok=%d later_merge_flag=%u flag_ok=%d cache=%p cache_ok=%d chi_equals_cache=%d merge_event_unobserved=1",
        event,static_cast<unsigned long>(pair[1]-base-4),request_data.frame,app_frame,frame_ok,
        reinterpret_cast<void*>(object),reinterpret_cast<void*>(request),reinterpret_cast<void*>(request_data.settings),reinterpret_cast<void*>(request_data.input),
        reinterpret_cast<void*>(entry->x19_x30[1]),static_cast<uint32_t>(entry->x19_x30[5]),static_cast<uint32_t>(entry->x19_x30[0]),
        feature_word,feature_ok,static_cast<unsigned long>(pipeline),pipeline_ok,later_flag,flag_ok,
        reinterpret_cast<void*>(cache),cache_ok,cache_ok&&entry->x19_x30[1]==cache);
    errno=saved;
}
