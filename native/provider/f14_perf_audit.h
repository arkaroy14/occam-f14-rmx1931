#pragma once
#include <stdio.h>
// Observation only. Exact F14 dlsym sites and pulled ROM stub are pinned by
// the controller; original calls/results/errno are retained. No performance vote.
using PerfAcquire=int(*)(int,int,int*,int);
using PerfRelease=int(*)(int);
using PerfHint=int(*)(int,const char*,int,int);
static std::atomic<PerfAcquire> perf_original_acquire{nullptr};
static std::atomic<PerfRelease> perf_original_release{nullptr};
static std::atomic<PerfHint> perf_original_hint{nullptr};
static std::atomic<uintptr_t> perf_orms_base{0};
static std::atomic<time_t> perf_deadline{0};
static std::atomic<unsigned> perf_events{0},perf_bindings{0};
static std::atomic<bool> perf_expired{false};
static constexpr const char*perf_marker="/data/vendor/camera/ocfresh_perf_audit";
static bool perf_observe_event(){
    int saved=errno;bool allowed=false;timespec now{};
    if(!perf_expired.load()&&access(perf_marker,R_OK)==0&&clock_gettime(CLOCK_MONOTONIC,&now)==0){
        time_t deadline=perf_deadline.load();
        if(!deadline){
            time_t empty=0;perf_deadline.compare_exchange_strong(empty,now.tv_sec+240);
            deadline=perf_deadline.load();
        }
        if(deadline&&now.tv_sec<=deadline){
            unsigned n=perf_events.load();
            while(n<32)if(perf_events.compare_exchange_weak(n,n+1)){allowed=true;break;}
            if(n>=32&&!perf_expired.exchange(true))__android_log_print(ANDROID_LOG_WARN,"OcPerf","LIMIT events=32 observation_off=1");
        }else if(deadline&&!perf_expired.exchange(true))__android_log_print(ANDROID_LOG_INFO,"OcPerf","EXPIRED ttl_seconds=240 observation_off=1");
    }
    errno=saved;return allowed;
}
static void perf_orms_snapshot(){
    uintptr_t base=perf_orms_base.load(),service=0;
    bool ok=base&&tuning_read(base+0x6578,&service,sizeof(service));
    __android_log_print(ANDROID_LOG_INFO,"OcPerf","ORMS_CACHE base=%p slot=6578 read_ok=%d service=%p cached_pointer_only=1 backend_effect_unobserved=1",
        reinterpret_cast<void*>(base),ok,reinterpret_cast<void*>(service));
}
static int perf_acquire_observe(int handle,int duration,int*resources,int count){
    int before=errno;bool observe=perf_observe_event();int values[32]{};
    bool read=observe&&count>=0&&count<=32&&tuning_read(reinterpret_cast<uintptr_t>(resources),values,static_cast<size_t>(count)*4);
    errno=before;int result=perf_original_acquire.load()(handle,duration,resources,count);int after=errno;
    if(observe){
        char text[400]{};unsigned used=0;
        if(read)for(int i=0;i<count&&used<sizeof(text)-12;++i){int n=snprintf(text+used,sizeof(text)-used,"%08x,",static_cast<unsigned>(values[i]));if(n>0)used+=static_cast<unsigned>(n);}
        __android_log_print(ANDROID_LOG_INFO,"OcPerf","ACQUIRE handle=%d duration=%d count=%d read_ok=%d args=%s result=%d original_once=1 no_vote_added=1",handle,duration,count,read,text,result);
        perf_orms_snapshot();
    }
    errno=after;return result;
}
static int perf_release_observe(int handle){
    int before=errno;bool observe=perf_observe_event();errno=before;
    int result=perf_original_release.load()(handle);int after=errno;
    if(observe)__android_log_print(ANDROID_LOG_INFO,"OcPerf","RELEASE handle=%d result=%d original_once=1 no_vote_added=1",handle,result);
    errno=after;return result;
}
static int perf_hint_observe(int hint,const char*user,int duration,int type){
    int before=errno;bool observe=perf_observe_event();errno=before;
    int result=perf_original_hint.load()(hint,user,duration,type);int after=errno;
    if(observe){
        __android_log_print(ANDROID_LOG_INFO,"OcPerf","HINT id=%x user=%p duration=%d type=%d result=%d original_once=1 no_vote_added=1",hint,user,duration,type,result);
        perf_orms_snapshot();
    }
    errno=after;return result;
}
static void*perf_audit_bind(void*result,const char*symbol,const void*caller,F14LoaderDlsym loader){
    if(!result||!symbol||!caller)return result;
    uintptr_t expected=0,site=0;void*replacement=nullptr;
    if(!strcmp(symbol,"perf_lock_acq")){expected=0x1080;site=0xeade4;replacement=reinterpret_cast<void*>(perf_acquire_observe);}
    else if(!strcmp(symbol,"perf_lock_rel")){expected=0x10a0;site=0xeadf8;replacement=reinterpret_cast<void*>(perf_release_observe);}
    else if(!strcmp(symbol,"perf_hint")){expected=0x1060;site=0xeae0c;replacement=reinterpret_cast<void*>(perf_hint_observe);}
    else return result;
    Dl_info from{},target{};
    if(!dladdr(caller,&from)||!from.dli_fname||strcmp(from.dli_fname,"/vendor/lib64/hw/com.qti.chi.override.so")
        ||reinterpret_cast<uintptr_t>(caller)-reinterpret_cast<uintptr_t>(from.dli_fbase)!=site
        ||!dladdr(result,&target)||!target.dli_fname||strcmp(target.dli_fname,"/vendor/lib64/libqti-perfd-client.so")
        ||reinterpret_cast<uintptr_t>(result)-reinterpret_cast<uintptr_t>(target.dli_fbase)!=expected)return result;
    if(expected==0x1080)perf_original_acquire.store(reinterpret_cast<PerfAcquire>(result));
    if(expected==0x10a0)perf_original_release.store(reinterpret_cast<PerfRelease>(result));
    if(expected==0x1060)perf_original_hint.store(reinterpret_cast<PerfHint>(result));
    if(!perf_orms_base.load()&&loader){
        const char*name="_ZN7android13OrmsHalClient11getInstanceERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE";
        void*method=loader(RTLD_DEFAULT,name,caller);Dl_info info{};
        if(method&&dladdr(method,&info)&&info.dli_fname&&!strcmp(info.dli_fname,"/odm/lib64/libormshalclient.so")
            &&reinterpret_cast<uintptr_t>(method)-reinterpret_cast<uintptr_t>(info.dli_fbase)==0x363c)
            perf_orms_base.store(reinterpret_cast<uintptr_t>(info.dli_fbase));
    }
    if(perf_bindings.fetch_add(1)<3)__android_log_print(ANDROID_LOG_INFO,"OcPerf","API_BOUND symbol=%s chi_site=%lx rom_target=%lx marker_gated=1",symbol,static_cast<unsigned long>(site),static_cast<unsigned long>(expected));
    return replacement;
}
