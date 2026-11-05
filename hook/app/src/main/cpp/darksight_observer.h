// Opt-in diagnostics for the unchanged ROM darksight C API. Signatures and
// table layout verified from osight_get_api/process_raw and original F14 caller.
// Observers retain all arguments/results. A separate one-capture opt-in at the
// bottom tests the measured SDK(+64) -> F14 ISP(zero-pedestal) format boundary.
#pragma once
using DarksightApi = uint64_t (*)(void**, uint8_t);
using DarksightProcess = uint64_t (*)(void*, uint64_t, uint64_t, uint64_t);
static DarksightApi original_darksight_api;
static DarksightProcess original_darksight_process;
static uint32_t (*darksight_error_code)();
static const char* (*darksight_error_string)();
static std::atomic<unsigned> darksight_error_reports{0};
using DarksightClBuild = int32_t (*)(void*, uint32_t, void* const*, const char*, void (*)(void*, void*), void*);
static DarksightClBuild original_darksight_cl_build;
static int32_t (*darksight_program_info)(void*, uint32_t, size_t, void*, size_t*);
static int32_t (*darksight_build_info)(void*, void*, uint32_t, size_t, void*, size_t*);
static std::atomic<unsigned> darksight_build_reports{0};
// Observe the actual inference boolean, rather than process_raw's unconditional
// zero. AArch64 C++ instance ABI: this, ITensor const*, TensorMap&; bool return.
using DarksightExecute=bool(*)(void*,const void*,void*);
static DarksightExecute original_darksight_execute;
static std::atomic<bool> darksight_execute_bound{false};
static std::atomic<unsigned> darksight_execute_reports{0};
static thread_local bool darksight_inference_succeeded;
static bool darksight_zero_pedestal_probe;
static std::atomic<unsigned> darksight_zero_pedestal_count{0};
static bool observe_darksight_execute(void* self,const void* input,void* output) {
    int entry_errno=errno;
    bool report=darksight_gpu_depth&&darksight_execute_reports.fetch_add(1)<16;
    if(report)LOG("DARKSIGHT_INFERENCE_BEGIN tensor_present=%d output_map_present=%d",input!=nullptr,output!=nullptr);
    errno=entry_errno;bool result=original_darksight_execute(self,input,output);int saved=errno;
    if(darksight_gpu_depth&&result)darksight_inference_succeeded=true;
    if(report){
        uint32_t code=darksight_error_code?darksight_error_code():0xffffffff;
        const char*message=darksight_error_string?darksight_error_string():nullptr;
        LOG("DARKSIGHT_INFERENCE_END actual_bool=%d snpe_code=%u message=%.768s original_result_retained",
            result,code,message&&readable(message,1)?message:"unavailable");
    }
    errno=saved;return result;
}
static void bind_darksight_execute(void* handle,const void* caller) {
    if(!night_observer_enabled||!hook_function||darksight_execute_bound.load())return;
    void* entry=original_symbol(handle,"_ZN3zdl4SNPE4SNPE7executeEPKNS_8DlSystem7ITensorERNS2_9TensorMapE",caller);
    Dl_info owner{};
    constexpr uint32_t prologue[]={0xf81e0ff3,0xa9017bfd,0x910043fd,0xf9400000};
    if(!entry||!dladdr(entry,&owner)||!owner.dli_fname||!owner.dli_fbase
       ||strcmp(owner.dli_fname,"/system/priv-app/OppoCamera/fresh_native/darksight_rom/lib64/libSNPE.so")
       ||reinterpret_cast<uintptr_t>(entry)-reinterpret_cast<uintptr_t>(owner.dli_fbase)!=0xfd58c
       ||!readable(entry,sizeof(prologue))||memcmp(entry,prologue,sizeof(prologue)))return;
    if(darksight_execute_bound.exchange(true))return;
    int status=hook_function(entry,reinterpret_cast<void*>(observe_darksight_execute),
                             reinterpret_cast<void**>(&original_darksight_execute));
    LOG("DARKSIGHT_INFERENCE_OBSERVER_BOUND status=%d exact private SNPEfd58c; no tensor edits",status);
}
// C image descriptor verified from original darkSightProcess construction:
// width,height,data,pixel-count. Sample adjacent2x2 sites so Bayer parity is
// visible in aggregate. No raw image is written and no sample is modified.
struct DarksightImage { uint32_t width, height; const uint16_t* data; uint64_t pixels; };
static_assert(sizeof(DarksightImage)==24, "verified AArch64 darksight descriptor");
static std::atomic<unsigned> darksight_signal_reports{0};
static void observe_darksight_signal(const char*phase,uint64_t address,unsigned call) {
    const auto*desc=reinterpret_cast<const DarksightImage*>(address);
    if(!desc||!readable(desc,sizeof(*desc)))return;
    const DarksightImage image=*desc;
    if(image.width!=4608||image.height!=3456||image.pixels!=15925248
            ||!image.data||!readable(image.data,31850496))return;
    nightsignal::Result result;
    if(!nightsignal::sample(reinterpret_cast<const uint8_t*>(image.data),31850496,4608,3456,9216,result))return;
    LOG("DARK_SIGNAL_BUFFER call=%u phase=%s pixels=%p descriptor=%p width=%u height=%u",call,phase,image.data,desc,image.width,image.height);
    for(unsigned i=0;i<4;++i){const auto&s=result.sites[i];
        LOG("DARK_SIGNAL_SITE call=%u phase=%s site=%u samples=256 sum=%llu min=%u max=%u le63=%u ge1023=%u gt1023=%u hash=%llx read_only_no_dump",
            call,phase,i,(unsigned long long)s.sum,s.low,s.high,s.below_black,s.at_white,s.above_white,(unsigned long long)s.hash);
    }
}
// Iterator layout is read directly by original mergeGBRG: +10 dense flag,
// +18 float cursor, +20 auxiliary/end word. Non-dense iterators are not called
// through guessed virtual interfaces. Extent comes from that merge's exact
// width*height loop, not from an assumed ITensor implementation.
using DarksightMerge=void(*)(void*,const void*,const void*,DarksightImage*,float,bool);
static DarksightMerge original_darksight_merge;
static std::atomic<bool> darksight_merge_bound{false};
static std::atomic<unsigned> darksight_tensor_reports{0};
static void darksight_tensor_stats(const char*label,const void*iterator,uint64_t values) {
    if(!readable(iterator,40))return;
    auto*p=static_cast<const uint8_t*>(iterator);uint8_t dense=p[16];const float*data=nullptr;uint64_t tail=0;
    memcpy(&data,p+24,8);memcpy(&tail,p+32,8);
    if(dense!=1||values!=uint64_t(4608)*3456||!data||reinterpret_cast<uintptr_t>(data)%4
            ||!readable(data,values*sizeof(float))){
        LOG("DARKSIGHT_TENSOR_STATS label=%s unsupported_dense_or_extent dense=%u cursor=%p tail=%llx",label,dense,data,static_cast<unsigned long long>(tail));return;
    }
    double sum[4]{},absolute[4]{},squares[4]{};float low[4]={INFINITY,INFINITY,INFINITY,INFINITY},high[4]={-INFINITY,-INFINITY,-INFINITY,-INFINITY};
    unsigned zeros[4]{},nonfinite[4]{},finite[4]{};
    for(uint64_t k=0;k<4096;++k){uint64_t base=(k*(values-4)/4095)&~uint64_t(3);
        for(unsigned c=0;c<4;++c){float v=data[base+c];if(!__builtin_isfinite(v)){++nonfinite[c];continue;}
            ++finite[c];if(v==0)++zeros[c];sum[c]+=v;absolute[c]+=v<0?-v:v;squares[c]+=double(v)*v;
            if(v<low[c])low[c]=v;if(v>high[c])high[c]=v;
        }
    }
    for(unsigned c=0;c<4;++c){double n=finite[c]?finite[c]:1,mean=sum[c]/n;
        LOG("DARKSIGHT_TENSOR_STATS label=%s channel=%u samples=%u zero=%u nonfinite=%u min=%.9g max=%.9g mean=%.9g meanAbs=%.9g variance=%.9g merge_extent=%llu dense=%u tail=%llx read_only_no_dump",
            label,c,finite[c],zeros[c],nonfinite[c],low[c],high[c],mean,absolute[c]/n,squares[c]/n-mean*mean,
            static_cast<unsigned long long>(values),dense,static_cast<unsigned long long>(tail));}
}
static void observe_darksight_merge(void*self,const void*output,const void*input,DarksightImage*raw,float blend,bool addition){
    int entry_errno=errno;
    if(night_observer_enabled&&darksight_gpu_depth&&darksight_inference_succeeded
            &&readable(raw,sizeof(*raw))&&raw->width==4608&&raw->height==3456
            &&darksight_tensor_reports.fetch_add(1)<3){
        LOG("DARKSIGHT_MERGE_INPUTS addition=%d blend=%.6g original_tensors_and_pixels_retained",addition,blend);
        darksight_tensor_stats("network_output",output,uint64_t(raw->width)*raw->height);
        darksight_tensor_stats("network_input",input,uint64_t(raw->width)*raw->height);
    }
    errno=entry_errno;original_darksight_merge(self,output,input,raw,blend,addition);
}
static void bind_darksight_merge(void*handle,const void*caller){
    if(!night_observer_enabled||!hook_function||darksight_merge_bound.load())return;
    void*entry=original_symbol(handle,"_ZN3oua8RGGBUtilIN3zdl8DlSystem10ITensorItrILb0EEEE24mergeAndConver2BayerGBRGES4_S4_RNS_11OSightImageEfb",caller);
    Dl_info owner{};constexpr uint32_t prologue[]={0xd10743ff,0x6d152beb,0x6d1623e9,0xa9176ffc};
    if(!entry||!dladdr(entry,&owner)||!owner.dli_fname||!owner.dli_fbase
            ||strcmp(owner.dli_fname,"/system/priv-app/OppoCamera/fresh_native/darksight_rom/lib64/libapsdarksight.so")
            ||reinterpret_cast<uintptr_t>(entry)-reinterpret_cast<uintptr_t>(owner.dli_fbase)!=0x14190
            ||!readable(entry,sizeof(prologue))||memcmp(entry,prologue,sizeof(prologue)))return;
    if(darksight_merge_bound.exchange(true))return;
    int status=hook_function(entry,reinterpret_cast<void*>(observe_darksight_merge),reinterpret_cast<void**>(&original_darksight_merge));
    LOG("DARKSIGHT_TENSOR_OBSERVER_BOUND status=%d exactROMwrapper14190 originalmergeABI; finite_stats_only",status);
}
static void test_darksight_zero_pedestal(uint64_t input_address,uint64_t output_address,uint64_t result) {
    if(!darksight_zero_pedestal_probe||!darksight_inference_succeeded||result!=0
            ||darksight_zero_pedestal_count.load())return;
    auto*in=reinterpret_cast<const DarksightImage*>(input_address);
    auto*out=reinterpret_cast<const DarksightImage*>(output_address);
    if(!readable(in,sizeof(*in))||!readable(out,sizeof(*out)))return;
    DarksightImage image=*out;
    if(image.width!=4608||image.height!=3456||image.pixels!=uint64_t(4608)*3456
            ||image.data==in->data||!readable(image.data,image.pixels*2))return;
    // Validate writable output ownership before the one temporary conversion.
    FILE*maps=fopen("/proc/self/maps","r");if(!maps)return;
    char line[1024],permissions[5];unsigned long begin,end;bool writable=false;
    const auto address=reinterpret_cast<uintptr_t>(image.data);
    while(fgets(line,sizeof(line),maps))if(sscanf(line,"%lx-%lx %4s",&begin,&end,permissions)==3
            &&address>=begin&&address<end){
        writable=permissions[1]=='w'&&image.pixels*2<=end-address;break;
    }
    fclose(maps);if(!writable||darksight_zero_pedestal_count.exchange(1))return;
    // Original wrapper mergeAndConver2BayerGBRG explicitly encodes +64.
    // The observed F14 reprocess path disables linearization and consumes
    // ordinary Night's zero-based RAW. This is a scoped format-contract test,
    // not a change to the model, its input, inference, dimensions or success.
    auto*pixels=const_cast<uint16_t*>(image.data);uint64_t clamped=0;
    for(uint64_t i=0;i<image.pixels;++i){uint16_t v=pixels[i];if(v<=64){pixels[i]=0;++clamped;}else pixels[i]=v-64;}
    LOG("DARKSIGHT_ZERO_PEDESTAL_TEST original_sdk_offset=64 pixels=%llu clamped=%llu one_capture_budget_exhausted; original_inference_true original_result_retained",
        static_cast<unsigned long long>(image.pixels),static_cast<unsigned long long>(clamped));
}
static char darksight_probe_directory[PATH_MAX];
static std::atomic<unsigned> darksight_probe_count{0};
static void save_darksight_probe(const char* label,uint64_t address) {
    auto*desc=reinterpret_cast<const DarksightImage*>(address);
    if(!darksight_probe_directory[0]||!readable(desc,sizeof(*desc)))return;
    DarksightImage image=*desc;
    if(image.width!=4608||image.height!=3456||image.pixels!=uint64_t(4608)*3456
       ||!readable(image.data,image.pixels*2))return;
    char path[PATH_MAX];snprintf(path,sizeof(path),"%s/%s_%d_4608x3456.raw16",darksight_probe_directory,label,getpid());
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return;
    const char*data=reinterpret_cast<const char*>(image.data);size_t bytes=image.pixels*2,done=0;
    while(done<bytes){ssize_t n=write(fd,data+done,bytes-done);if(n<0&&errno==EINTR)continue;if(n<=0)break;done+=n;}
    close(fd);LOG("DARKSIGHT_FINITE_RAW_COPY label=%s bytes=%zu expected=%zu; source unchanged",label,done,bytes);
}
static void darksight_image_stats(const char* label, uint64_t address) {
    const auto* descriptor = reinterpret_cast<const DarksightImage*>(address);
    if (!descriptor || !readable(descriptor, sizeof(*descriptor))) { LOG("DARKSIGHT_RAW_STATS label=%s descriptor_unreadable", label); return; }
    const DarksightImage image = *descriptor;
    uint64_t expected=static_cast<uint64_t>(image.width)*image.height;
    if (image.width<2 || image.height<2 || image.width>10000 || image.height>10000
            || image.pixels!=expected || expected>70000000 || !readable(image.data, expected*2)) {
        LOG("DARKSIGHT_RAW_STATS label=%s invalid_layout width=%u height=%u pixels=%llu", label,image.width,image.height,static_cast<unsigned long long>(image.pixels));return;
    }
    uint64_t sums[4]{}, counts[4]{},signature=0xcbf29ce484222325ULL;uint16_t minimum[4]={65535,65535,65535,65535}, maximum[4]{};
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x){
        uint32_t row=((static_cast<uint64_t>(y)*(image.height-2)/31)&~1U);
        uint32_t col=((static_cast<uint64_t>(x)*(image.width-2)/31)&~1U);
        for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){
            unsigned site=2*dy+dx;uint16_t value=image.data[static_cast<size_t>(row+dy)*image.width+col+dx];
            sums[site]+=value;++counts[site];if(value<minimum[site])minimum[site]=value;if(value>maximum[site])maximum[site]=value;
            signature=(signature^(value&255))*0x100000001b3ULL;
            signature=(signature^(value>>8))*0x100000001b3ULL;
        }
    }
    LOG("DARKSIGHT_RAW_STATS label=%s width=%u height=%u data=%p mean=[%.2f,%.2f,%.2f,%.2f] min=[%u,%u,%u,%u] max=[%u,%u,%u,%u] samplesPerSite=%llu",
        label,image.width,image.height,image.data,
        static_cast<double>(sums[0])/counts[0],static_cast<double>(sums[1])/counts[1],static_cast<double>(sums[2])/counts[2],static_cast<double>(sums[3])/counts[3],
        minimum[0],minimum[1],minimum[2],minimum[3],maximum[0],maximum[1],maximum[2],maximum[3],static_cast<unsigned long long>(counts[0]));
    LOG("DARKSIGHT_RAW_SITES label=%s sums=[%llu,%llu,%llu,%llu] sampledFNV=%llx; read-only4096samples",
        label,static_cast<unsigned long long>(sums[0]),static_cast<unsigned long long>(sums[1]),
        static_cast<unsigned long long>(sums[2]),static_cast<unsigned long long>(sums[3]),static_cast<unsigned long long>(signature));
}
static int32_t observe_darksight_cl_build(void* program, uint32_t count, void* const* devices,
                                        const char* options, void (*callback)(void*, void*), void* user) {
    int32_t result = original_darksight_cl_build(program, count, devices, options, callback, user);
    const int saved_errno = errno;
    if (result == -11 && darksight_build_reports.fetch_add(1) < 8) {
        void* device = nullptr;
        if (count && devices && readable(devices, sizeof(void*))) device = devices[0];
        else if (darksight_program_info) {
            // CL_PROGRAM_DEVICES. Query the count first to supply the documented
            // full output size even when a program belongs to multiple devices.
            size_t bytes = 0; void* candidates[8]{};
            if (darksight_program_info(program, 0x1163, 0, nullptr, &bytes) == 0
                    && bytes && bytes <= sizeof(candidates)
                    && darksight_program_info(program, 0x1163, bytes, candidates, nullptr) == 0)
                device = candidates[0];
        }
        char log[4096]{}; size_t bytes = 0; int32_t status = -1;
        if (device && darksight_build_info) {
            // CL_PROGRAM_BUILD_LOG. Preserve original build status, options and
            // callback; read diagnostics only after the failed original call.
            status = darksight_build_info(program, device, 0x1183, 0, nullptr, &bytes);
            if (status == 0 && bytes > 0 && bytes <= sizeof(log))
                status = darksight_build_info(program, device, 0x1183, sizeof(log), log, nullptr);
        }
        LOG("DARKSIGHT_CL_BUILD_FAILURE original=%d diagnostic_status=%d log_bytes=%zu options=%.384s log=%.4000s",
            result, status, bytes, options ? options : "NULL", bytes <= sizeof(log) ? log : "exceeds4096bytebudget");
    }
    errno = saved_errno;
    return result;
}
static uint64_t observe_darksight_process(void* handle, uint64_t first, uint64_t second, uint64_t third) {
    int entry_errno = errno;
    unsigned signal_call=night_scalar_enabled&&night_signal_enabled?darksight_signal_reports.fetch_add(1):~0U;
    bool signal=signal_call<2;
    if(signal){
        uint32_t words[5]{};
        if(second&&readable(reinterpret_cast<const void*>(second),sizeof(words)))memcpy(words,reinterpret_cast<const void*>(second),sizeof(words));
        LOG("DARK_SIGNAL_BEGIN call=%u parameters=%u,%u,%u,%u,%u no_SNPE_hook_no_error_getter",signal_call,words[0],words[1],words[2],words[3],words[4]);
        observe_darksight_signal("before_input",first,signal_call);
        observe_darksight_signal("before_output",third,signal_call);
    }
    bool copy=night_observer_enabled&&darksight_probe_directory[0]&&darksight_probe_count.fetch_add(1)==0;
    if(copy)save_darksight_probe("input",first);
    bool report = night_observer_enabled && darksight_error_reports.load()<8;
    if (report) {
        uint32_t parameters[5]{};
        bool valid=second && readable(reinterpret_cast<void*>(second),sizeof(parameters));
        if(valid)memcpy(parameters,reinterpret_cast<void*>(second),sizeof(parameters));
        LOG("DARKSIGHT_RAW_PARAMETERS valid=%d words=[%u,%u,%u,%u,%u]",valid,parameters[0],parameters[1],parameters[2],parameters[3],parameters[4]);
        darksight_image_stats("before_input",first);darksight_image_stats("before_output",third);
    }
    ++darksight_gpu_depth;
    darksight_inference_succeeded=false;
    errno = entry_errno;
    uint64_t result = original_darksight_process(handle, first, second, third);
    int saved_errno = errno;
    --darksight_gpu_depth;
    if(signal){
        observe_darksight_signal("after_input",first,signal_call);
        observe_darksight_signal("after_output",third,signal_call);
        LOG("DARK_SIGNAL_END call=%u original_x0=%llx status_not_quality_proof",signal_call,(unsigned long long)result);
    }
    uint32_t parameters[5]{};
    if(second&&readable(reinterpret_cast<void*>(second),sizeof(parameters))){
        memcpy(parameters,reinterpret_cast<void*>(second),sizeof(parameters));
        if(parameters[0]==0&&parameters[1]==0&&parameters[2]==3){
            if(darksight_zero_pedestal_probe&&!darksight_zero_pedestal_count.load())
                darksight_image_stats("sdk_output_before_conversion",third);
            test_darksight_zero_pedestal(first,third,result);
        }
    }
    if(copy)save_darksight_probe("output",third);
    if(report)darksight_image_stats("after_output",third);
    if (night_observer_enabled && darksight_error_reports.fetch_add(1) < 8) {
        // These getters read the SNPE instance's thread-local error. The same
        // unchanged result is returned to the original AlgoInterface caller.
        uint32_t code = darksight_error_code ? darksight_error_code() : 0xffffffff;
        const char* message = darksight_error_string ? darksight_error_string() : nullptr;
        bool valid = message && readable(message, 1);
        LOG("DARKSIGHT_PROCESS_RESULT original=%llu snpe_code=%u snpe_message=%.768s",
            static_cast<unsigned long long>(result), code, valid ? message : "unavailable");
    }
    errno = saved_errno;
    return result;
}
static uint64_t observe_darksight_api(void** table, uint8_t debug) {
    uint64_t result = original_darksight_api(table, debug);
    int saved_errno = errno;
    if (result == 0 && table && readable(table, 5*sizeof(void*))) {
        Dl_info owner{};
        if (dladdr(table[2], &owner) && owner.dli_fname && owner.dli_fbase
                && !strcmp(owner.dli_fname,
                    "/system/priv-app/OppoCamera/fresh_native/darksight_rom/lib64/libapsdarksight.so")
                && reinterpret_cast<uintptr_t>(table[2])-reinterpret_cast<uintptr_t>(owner.dli_fbase) == 0xaefc) {
            original_darksight_process = reinterpret_cast<DarksightProcess>(table[2]);
            table[2] = reinterpret_cast<void*>(observe_darksight_process);
            LOG("DARKSIGHT_PROCESS_OBSERVER_BOUND exact original API slot2; original arguments/result retained");
        }
    }
    errno = saved_errno;
    return result;
}
static void* audit_darksight_symbol(void* result, void* handle, const char* symbol, const void* caller) {
    if (!darksight_namespace || !result || !symbol) return result;
    Dl_info source{}, target{};
    if (night_observer_enabled && !strcmp(symbol, "clBuildProgram") && dladdr(caller, &source) && source.dli_fname
            && !strcmp(source.dli_fname,
                "/system/priv-app/OppoCamera/fresh_native/darksight_rom/lib64/libSNPE.so")
            && dladdr(result, &target) && target.dli_fname
            && (!strcmp(target.dli_fname, "/vendor/lib64/libOpenCL.so")
                || !strcmp(target.dli_fname,
                    "/system/priv-app/OppoCamera/fresh_native/darksight_f14_gpu/lib64/libOpenCL.so"))) {
        original_darksight_cl_build = reinterpret_cast<DarksightClBuild>(result);
        darksight_program_info = reinterpret_cast<decltype(darksight_program_info)>(original_symbol(handle, "clGetProgramInfo", caller));
        darksight_build_info = reinterpret_cast<decltype(darksight_build_info)>(original_symbol(handle, "clGetProgramBuildInfo", caller));
        LOG("DARKSIGHT_CL_BUILD_OBSERVER_BOUND build_info=%p program_info=%p", darksight_build_info, darksight_program_info);
        return reinterpret_cast<void*>(observe_darksight_cl_build);
    }
    if (strcmp(symbol, "osight_get_api")) return result;
    if (!dladdr(caller, &source) || !source.dli_fname || strcmp(source.dli_fname,
            "/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so")
            || !dladdr(result, &target) || !target.dli_fname || strcmp(target.dli_fname,
            "/system/priv-app/OppoCamera/fresh_native/darksight_rom/lib64/libapsdarksight.so")) return result;
    original_darksight_api = reinterpret_cast<DarksightApi>(result);
    darksight_error_code = reinterpret_cast<uint32_t (*)()>(original_symbol(handle,
            "_ZN3zdl8DlSystem16getLastErrorCodeEv", caller));
    darksight_error_string = reinterpret_cast<const char* (*)()>(original_symbol(handle,
            "_ZN3zdl8DlSystem18getLastErrorStringEv", caller));
    bind_darksight_execute(handle,caller);
    bind_darksight_merge(handle,caller);
    LOG("DARKSIGHT_ERROR_GETTERS code=%p string=%p original dependency instance", darksight_error_code, darksight_error_string);
    return reinterpret_cast<void*>(observe_darksight_api);
}
