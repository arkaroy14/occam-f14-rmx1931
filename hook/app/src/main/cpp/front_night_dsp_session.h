// Opt-in camera-process FastRPC session compatibility experiment.
// Qualcomm remote.h: DSPRPC_CONTROL_UNSIGNED_MODULE=2, {int domain,int enable}.
// Exact original F14 libcdsprpc@1c790 independently verified, including size8
// and unopened-session checks. No SDK result, DSP ELF or firmware is replaced.
#pragma once
static bool front_dsp_session_test;
static std::atomic<bool> front_dsp_session_attempted{false};
static FILE*(*front_dsp_original_fopen)(const char*,const char*);
static char front_dsp_private_skeleton[PATH_MAX];
static std::atomic<unsigned> front_dsp_file_reports{0};
static FILE* front_dsp_fopen(const char* path,const char* mode) {
    int entry_errno=errno;
    if(front_dsp_session_test&&path&&mode&&(!strcmp(mode,"r")||!strcmp(mode,"rb"))
       &&(!strcmp(path,"/odm/lib/rfsa/adsp/libarcsoft_psn_hvx_5221_skel.so")
          ||!strcmp(path,"/vendor/lib/rfsa/adsp/libarcsoft_psn_hvx_5221_skel.so")
          ||!strcmp(path,"/system/vendor/lib/rfsa/adsp/libarcsoft_psn_hvx_5221_skel.so"))){
        Dl_info caller{};
        if(dladdr(__builtin_return_address(0),&caller)&&caller.dli_fname&&!strcmp(caller.dli_fname,
            "/system/priv-app/OppoCamera/lib/arm64/libcdsprpc.so")){
            errno=entry_errno;FILE*result=front_dsp_original_fopen(front_dsp_private_skeleton,mode);int saved=errno;
            if(front_dsp_file_reports.fetch_add(1)<12)LOG("FRONT_DSP_ORIGINAL_FILE_OPEN success=%d; exact RPC read uses byte-identical private skeleton, original result retained",result!=nullptr);
            errno=saved;return result;
        }
    }
    errno=entry_errno;return front_dsp_original_fopen(path,mode);
}
static void configure_front_dsp_access() {
    constexpr const char*file="/system/priv-app/OppoCamera/fresh_native/front_night_dsp/libarcsoft_psn_hvx_5221_skel.so";
    if(access(file,R_OK))return;
    snprintf(front_dsp_private_skeleton,sizeof(front_dsp_private_skeleton),"%s",file);
    // This legacy RPC runtime does not use the supplied environment path for
    // the measured read. Redirect only that original caller's exact file open.
    // Other files, writers, libraries and callers retain original behavior.
    void*entry=dlsym(RTLD_DEFAULT,"fopen");
    Dl_info owner{};
    if(!entry||!dladdr(entry,&owner)||!owner.dli_fname||strcmp(owner.dli_fname,
        "/apex/com.android.runtime/lib64/bionic/libc.so"))return;
    int status=hook_function(entry,reinterpret_cast<void*>(front_dsp_fopen),
                             reinterpret_cast<void**>(&front_dsp_original_fopen));
    if(status!=0){LOG("FRONT_DSP_FILE_ADAPTER_REFUSED status=%d",status);return;}
    front_dsp_session_test=true;
    LOG("FRONT_DSP_FILE_ADAPTER_READY exact originalRPC skeleton reads only; no shared mount or policy change");
}
static void prepare_front_dsp_session(void* algorithm_handle,const void* caller) {
    if(!front_dsp_session_test||front_dsp_session_attempted.load())return;
    Dl_info source{};
    if(!dladdr(caller,&source)||!source.dli_fname||strcmp(source.dli_fname,
        "/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so"))return;
    if(front_dsp_session_attempted.exchange(true))return;
    int saved=errno;
    constexpr const char* library="/system/priv-app/OppoCamera/lib/arm64/libcdsprpc.so";
    // Resolve through the original algorithm's DT_NEEDED dependency graph so
    // the control changes the same RPC instance used by its remote calls.
    void* entry=algorithm_handle?original_symbol(algorithm_handle,"remote_session_control",caller):nullptr;
    Dl_info target{};
    if(entry&&dladdr(entry,&target)&&target.dli_fname&&target.dli_fbase
       &&!strcmp(target.dli_fname,library)
       &&reinterpret_cast<uintptr_t>(entry)-reinterpret_cast<uintptr_t>(target.dli_fbase)==0x1c790){
        using SessionControl=int32_t(*)(uint32_t,void*,uint32_t);
        struct UnsignedModule{int32_t domain,enable;}request{3,1};
        static_assert(sizeof(request)==8,"verified F14 FastRPC control ABI");
        int32_t result=reinterpret_cast<SessionControl>(entry)(2,&request,sizeof(request));
        LOG("FRONT_DSP_SESSION_CONTROL domain=3 request=2 enable=1 original_result=%d; app process only, SDK still decides capture",result);
    }else LOG("FRONT_DSP_SESSION_REFUSED exact original RPC entry unavailable");
    errno=saved;
}
