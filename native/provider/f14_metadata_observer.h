// Original F14 MultiCamera reprocess copy observation; no donor code mutation.
// camera_metadata ABI verified against AOSP system/media camera_metadata.h.
#pragma once
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

struct F14MetaRational { int32_t numerator,denominator; };
struct F14MetaEntry {
    size_t index; uint32_t tag; uint8_t type; size_t count;
    union { const uint8_t*u8; const int32_t*i32; const float*f;
        const int64_t*i64; const double*d; const F14MetaRational*r; } data;
};
static_assert(sizeof(F14MetaEntry)==32 && offsetof(F14MetaEntry,tag)==8
    && offsetof(F14MetaEntry,type)==12 && offsetof(F14MetaEntry,count)==16
    && offsetof(F14MetaEntry,data)==24,"AOSP64 camera_metadata_ro_entry ABI");
using F14MetaCopy=void*(*)(const void*,size_t);
using F14MetaFind=int(*)(const void*,uint32_t,F14MetaEntry*);
static std::atomic<F14MetaCopy> f14_original_meta_copy{nullptr};
static std::atomic<F14MetaFind> f14_original_meta_find{nullptr};
static std::atomic<const void*> f14_rear_night_observer_device{nullptr};
static std::atomic<unsigned> f14_meta_capture_budget{0};
struct F14MetaContext { bool active; unsigned capture; uint32_t frame; const void*settings; };
static thread_local F14MetaContext f14_meta_context{};
static constexpr const char* f14_meta_marker="/data/vendor/camera/ocfresh_reprocess_metadata_trace";
static void f14_metadata_resolve() {
    int saved=errno;
    if(!f14_original_meta_copy.load())f14_original_meta_copy.store(
        reinterpret_cast<F14MetaCopy>(dlsym(RTLD_NEXT,"allocate_copy_camera_metadata_checked")));
    if(!f14_original_meta_find.load())f14_original_meta_find.store(
        reinterpret_cast<F14MetaFind>(dlsym(RTLD_NEXT,"find_camera_metadata_ro_entry")));
    errno=saved;
}
static bool f14_metadata_armed() { int saved=errno;bool result=access(f14_meta_marker,R_OK)==0;errno=saved;return result; }
static void f14_metadata_values(const void* metadata,const char*stage,unsigned capture,uint32_t frame){
    auto find=f14_original_meta_find.load();if(!find||!metadata)return;
    struct Tag {uint32_t number;const char*name;};
    static constexpr Tag tags[]={
        {0xe0010,"sensor.timestamp"},{0xe0000,"sensor.exposureTime"},
        {0xe0001,"sensor.frameDuration"},{0xe0002,"sensor.sensitivity"},
        {0x2,"colorCorrection.gains"},{0x1,"colorCorrection.transform"},
        {0xe001c,"sensor.dynamicBlackLevel"},{0xe001d,"sensor.dynamicWhiteLevel"},
        {0xe0012,"sensor.neutralColorPoint"},{0x1000b,"control.awbMode"},
        {0x10028,"control.postRawSensitivityBoost"},{0xd0000,"scaler.cropRegion"}
    };
    for(const auto&tag:tags){
        F14MetaEntry entry{};int status=find(metadata,tag.number,&entry);
        if(status!=0){__android_log_print(ANDROID_LOG_INFO,"OcF14Meta",
            "TAG capture=%u frame=%u stage=%s name=%s status=%d absent_or_unavailable",capture,frame,stage,tag.name,status);continue;}
        char values[640];size_t used=0;values[0]=0;
        for(size_t i=0;i<entry.count&&i<16&&used+64<sizeof(values);++i){
            int wrote=0;const char*sep=i?",":"";
            switch(entry.type){
                case 0:wrote=snprintf(values+used,sizeof(values)-used,"%s%u",sep,entry.data.u8[i]);break;
                case 1:wrote=snprintf(values+used,sizeof(values)-used,"%s%d",sep,entry.data.i32[i]);break;
                case 2:wrote=snprintf(values+used,sizeof(values)-used,"%s%.9g",sep,static_cast<double>(entry.data.f[i]));break;
                case 3:wrote=snprintf(values+used,sizeof(values)-used,"%s%lld",sep,static_cast<long long>(entry.data.i64[i]));break;
                case 4:wrote=snprintf(values+used,sizeof(values)-used,"%s%.12g",sep,entry.data.d[i]);break;
                case 5:wrote=snprintf(values+used,sizeof(values)-used,"%s%d/%d",sep,entry.data.r[i].numerator,entry.data.r[i].denominator);break;
                default:wrote=snprintf(values+used,sizeof(values)-used,"unsupported_type");break;
            }
            if(wrote<0||static_cast<size_t>(wrote)>=sizeof(values)-used)break;used+=static_cast<size_t>(wrote);
        }
        __android_log_print(ANDROID_LOG_INFO,"OcF14Meta",
            "TAG capture=%u frame=%u stage=%s name=%s type=%u count=%zu values=[%s]%s",capture,frame,stage,
            tag.name,entry.type,entry.count,values,entry.count>16?" TRUNCATED":"");
    }
}
extern "C" void* allocate_copy_camera_metadata_checked(const void*source,size_t size){
    int entry_errno=errno;auto original=f14_original_meta_copy.load();
    if(!original){f14_metadata_resolve();original=f14_original_meta_copy.load();}
    // Delegation is mandatory; controller must verify COPY_OBSERVER_READY
    // original/find non-null before arming or asking for any capture.
    if(!original){errno=entry_errno;return nullptr;}
    const auto context=f14_meta_context;bool observe=false;
    uintptr_t offset=0;Dl_info caller{};
    if(context.active&&dladdr(__builtin_return_address(0),&caller)&&caller.dli_fname&&caller.dli_fbase
            &&!strcmp(caller.dli_fname,"/vendor/lib64/hw/com.qti.chi.override.so")){
        offset=reinterpret_cast<uintptr_t>(__builtin_return_address(0))-reinterpret_cast<uintptr_t>(caller.dli_fbase);
        // Exact return after BLb73b4, original reprocess copy -> this+1247958.
        observe=offset==0xb73b8;
    }
    errno=entry_errno;void*result=original(source,size);int saved=errno;
    if(observe){
        __android_log_print(ANDROID_LOG_INFO,"OcF14Meta",
            "REPROCESS_COPY capture=%u frame=%u caller=%lx callbackSettings=%p source=%p copy=%p size=%zu input_buffer=1 original_result_retained",
            context.capture,context.frame,static_cast<unsigned long>(offset),context.settings,source,result,size);
        f14_metadata_values(source,"multiSource",context.capture,context.frame);
        f14_metadata_values(result,"multiCopy",context.capture,context.frame);
    }
    errno=saved;return result;
}
