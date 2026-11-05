// Opt-in observation of the original Arcsoft Night -> OpenCL boundary.
// Included after LOG/readable in native_paths.cpp. Optional rear RAW sparse statistics
// read original buffer values; no images/models are dumped or changed.
// AArch64 C ABI signatures checked against Khronos CL/cl.h (2026-11-01):
// https://github.com/KhronosGroup/OpenCL-Headers/blob/main/CL/cl.h
// ARC_SN_PreProcess(handle, config) verified in donor SHA89178e57... at ELF0x9514.
// No API arguments, output pointers, errors, callbacks or cleanup are replaced.
#pragma once

static bool night_observer_enabled;
// Separate opt-in: scalar SDK boundaries only. No front function hook, OpenCL
// wrapper, INFO-bit change, RAW sampling or dump is enabled by this flag.
static bool night_scalar_enabled;
static bool night_signal_enabled;
static std::atomic<unsigned> night_scalar_pre_reports{0};
static constexpr const char* night_library =
        "/system/priv-app/OppoCamera/lib/arm64/libarcsoft_super_night_raw.so";
static thread_local unsigned night_depth;
static std::atomic<unsigned> night_reports{0};
static std::atomic<unsigned> night_failures{0};
// Original F14 frontNightProcessV1 returns0 on every exit; that value is not an
// SDK success code. Its own category-specific INFO messages expose real PSN
// statuses. Enable only that logging bit during <=6 calls, then restore it.
using FrontNightProcess = uint64_t (*)(void*);
static FrontNightProcess original_front_night_process;
static uint8_t* front_night_info_mask;
static std::atomic<bool> front_night_bound{false};
static std::atomic<unsigned> front_night_reports{0};
static bool front_night_writable(const void* pointer) {
    FILE* maps=fopen("/proc/self/maps","r");if(!maps)return false;
    char line[1024],permissions[5];unsigned long start,end;
    const auto address=reinterpret_cast<uintptr_t>(pointer);bool result=false;
    while(fgets(line,sizeof(line),maps))if(sscanf(line,"%lx-%lx %4s",&start,&end,permissions)==3
        &&address>=start&&address<end){result=permissions[1]=='w';break;}
    fclose(maps);return result;
}
static uint64_t observe_front_night_process(void* data) {
    int entry_errno=errno;bool report=front_night_reports.fetch_add(1)<6;
    uint8_t before=0;
    if(report){
        int32_t count=-1;
        if(data&&readable(data,0x28))memcpy(&count,static_cast<char*>(data)+0x24,sizeof(count));
        before=__atomic_fetch_or(front_night_info_mask,static_cast<uint8_t>(2),__ATOMIC_RELAXED);
        LOG("FRONT_NIGHT_V1_BEGIN inputs=%d info_before=%u; original parameters retained",count,before);
    }
    errno=entry_errno;uint64_t result=original_front_night_process(data);int saved=errno;
    if(report){
        if(!(before&2))__atomic_fetch_and(front_night_info_mask,static_cast<uint8_t>(~2),__ATOMIC_RELAXED);
        LOG("FRONT_NIGHT_V1_END original=%llu info_after=%u; wrapper0 is not SDK success",
            static_cast<unsigned long long>(result),__atomic_load_n(front_night_info_mask,__ATOMIC_RELAXED));
    }
    errno=saved;return result;
}
static void bind_front_night_observer(const Dl_info& source) {
    if(!highres_copy_enabled||!hook_function||!source.dli_fbase||front_night_bound.load())return;
    auto*base=static_cast<char*>(source.dli_fbase);void*target=base+0x265cdc;
    constexpr uint32_t prologue[]={0xa9ba7bfd,0xa9016ffc,0xa90267fa,0xa9035ff8};
    if(!readable(target,sizeof(prologue))||memcmp(target,prologue,sizeof(prologue)))return;
    // Verified GOT entry used by the original function's INFO bit test.
    uint8_t*mask=nullptr;
    if(readable(base+0x870940,sizeof(mask)))memcpy(&mask,base+0x870940,sizeof(mask));
    if(!mask||!readable(mask,0x12)||!front_night_writable(mask+0x11))return;
    if(front_night_bound.exchange(true))return;
    front_night_info_mask=mask+0x11;
    int status=hook_function(target,reinterpret_cast<void*>(observe_front_night_process),
                             reinterpret_cast<void**>(&original_front_night_process));
    LOG("FRONT_NIGHT_V1_OBSERVER_BOUND status=%d exactF14 entry265cdc metadata/logging only",status);
}
struct NightErrnoRestore {
    int saved = errno;
    ~NightErrnoRestore() { errno = saved; }
};
static bool night_caller(const void* caller) {
    Dl_info info{};
    return dladdr(caller, &info) && info.dli_fname && !strcmp(info.dli_fname, night_library);
}
static bool night_report(const void* caller, bool failed = false) {
    if (!night_depth && !night_caller(caller)) return false;
    // Reserve error reports even if many successful kernel builds use the
    // normal budget before the failing operation is reached.
    return failed ? night_failures.fetch_add(1) < 32 : night_reports.fetch_add(1) < 256;
}
// Read output values only after a successful API call, or its documented error
// output after return. Null pointers remain null in the original invocation.
template<typename T> static T night_value(const T* pointer, T fallback = {}) {
    T value = fallback;
    if (pointer && readable(pointer, sizeof(T))) memcpy(&value, pointer, sizeof(T));
    return value;
}
using NightPreprocess = int64_t (*)(int32_t*, const char*);
static NightPreprocess original_night_preprocess;
static int64_t audit_night_preprocess(int32_t* handle, const char* config) {
    int entry_errno = errno;
    ++night_depth;
    bool report = night_scalar_enabled ? night_scalar_pre_reports.fetch_add(1)<2
                                       : night_reports.fetch_add(1)<256;
    int32_t fields[4]{};
    bool valid = readable(handle, sizeof(fields));
    if (valid) memcpy(fields, handle, sizeof(fields));
    if (report) LOG("NIGHT_PREPROCESS_BEGIN handle=%p fields_valid=%d camera_mode=%d camera_state=%d width=%d height=%d config_present=%d",
                    handle, valid, fields[0], fields[1], fields[2], fields[3], config != nullptr);
    errno = entry_errno;
    int64_t result = original_night_preprocess(handle, config);
    NightErrnoRestore preserve_errno;
    if (report) LOG("NIGHT_PREPROCESS_END result=%lld original error and cleanup retained", static_cast<long long>(result));
    --night_depth;
    return result;
}

// Read-only standard rearNight boundary. ARC_SN_Process is the original
// eight scalar/pointer-register call in superNightProcessV3@207b18. Its entire
// 12692-byte caller body matches the saved REA target byte for byte (6af251b7).
// Forward every x0..x7 value and original x0 return without interpreting its ABI
// as success. The signed low32 result is the original SDK status for reporting.
using NightProcessRegisters = uintptr_t (*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,
                                            uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static NightProcessRegisters original_night_process;
static std::atomic<unsigned> night_process_reports{0};
#include "night_signal_stats.h"
static void observe_night_signal(uintptr_t pointer,const char*phase,unsigned capture){
 if(!night_signal_enabled)return;
 const char*image=reinterpret_cast<const char*>(pointer);
 if(!image||!readable(image,80))return;
 uint32_t width=0,height=0,pitch=0,index=0,count=0,format=0;const uint8_t*pixels=nullptr;
 memcpy(&index,image+4,4);memcpy(&count,image+8,4);memcpy(&format,image+16,4);
 memcpy(&width,image+20,4);memcpy(&height,image+24,4);memcpy(&pixels,image+32,8);memcpy(&pitch,image+64,4);
 if(width!=4608||height!=3456||pitch!=9216||!pixels||!readable(pixels,31850496)){
  LOG("NIGHT_SIGNAL_UNAVAILABLE capture=%u phase=%s width=%u height=%u pitch=%u",capture,phase,width,height,pitch);return;
 }
 nightsignal::Result result;
 if(!nightsignal::sample(pixels,31850496,width,height,pitch,result))return;
 LOG("NIGHT_SIGNAL_BUFFER capture=%u phase=%s index=%u count=%u pixels=%p image=%p",capture,phase,index,count,pixels,image);
 for(unsigned i=0;i<4;++i){const auto&s=result.sites[i];
  LOG("NIGHT_SIGNAL capture=%u phase=%s index=%u count=%u format=%x site=%u samples=256 sum=%llu min=%u max=%u le63=%u ge1023=%u gt1023=%u hash=%llx hist=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u central80pct read_only_no_dump",
   capture,phase,index,count,format,i,(unsigned long long)s.sum,s.low,s.high,s.below_black,s.at_white,s.above_white,(unsigned long long)s.hash,
   s.hist[0],s.hist[1],s.hist[2],s.hist[3],s.hist[4],s.hist[5],s.hist[6],s.hist[7],s.hist[8],s.hist[9],s.hist[10],s.hist[11],s.hist[12],s.hist[13],s.hist[14],s.hist[15]);
 }
}
static void observe_standard_raw(uintptr_t output,const char* phase,unsigned capture) {
    const auto*image=reinterpret_cast<const char*>(output);
    if(!image||!readable(image,80)){LOG("NIGHT_V3_RAW capture=%u phase=%s header_unreadable",capture,phase);return;}
    // Assignments in the matching original caller: width/height at20/24,
    // buffer at32 and pitch at64. Observe only the verified RAW4608 layout.
    uint32_t width=0,height=0,pitch=0,fields[4]={};const unsigned char*pixels=nullptr;
    memcpy(fields,image,16);memcpy(&width,image+20,4);memcpy(&height,image+24,4);
    memcpy(&pixels,image+32,sizeof(pixels));memcpy(&pitch,image+64,4);
    if(width!=4608||height!=3456||pitch!=9216||!pixels||!readable(pixels,31850496)){
        LOG("NIGHT_V3_RAW capture=%u phase=%s width=%u height=%u pitch=%u fields=%x,%x,%x,%x verified_layout=0",
            capture,phase,width,height,pitch,fields[0],fields[1],fields[2],fields[3]);return;
    }
    uint64_t sums[4]={};uint32_t low[4]={65535,65535,65535,65535},high[4]={};
    for(unsigned gy=0;gy<32;++gy)for(unsigned gx=0;gx<32;++gx){
        unsigned row=(gy*(height-2)/31)&~1U,col=(gx*(width-2)/31)&~1U;
        for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){
            unsigned site=2*dy+dx;uint16_t value=0;
            memcpy(&value,pixels+(row+dy)*pitch+(col+dx)*2,2);
            sums[site]+=value;if(value<low[site])low[site]=value;if(value>high[site])high[site]=value;
        }
    }
    LOG("NIGHT_V3_RAW capture=%u phase=%s width=%u height=%u pitch=%u fields=%x,%x,%x,%x samplesPerSite=1024 sums=%llu,%llu,%llu,%llu min=%u,%u,%u,%u max=%u,%u,%u,%u read_only_no_dump",
        capture,phase,width,height,pitch,fields[0],fields[1],fields[2],fields[3],
        (unsigned long long)sums[0],(unsigned long long)sums[1],(unsigned long long)sums[2],(unsigned long long)sums[3],
        low[0],low[1],low[2],low[3],high[0],high[1],high[2],high[3]);
}
// Original V3 local prepareInputInfo@20adb8 calls SDK81c8 with three
// pointers. Field layout verified in REA ev_e88d9c36, not V1's exported helper.
using NightAddRegisters=uintptr_t(*)(uintptr_t,uintptr_t,uintptr_t);
static NightAddRegisters original_night_add;
static std::atomic<unsigned> night_add_reports{0};
static void observe_night_raw_info(uintptr_t info,uintptr_t image,unsigned call) {
    const char*raw=reinterpret_cast<const char*>(info);const char*frame=reinterpret_cast<const char*>(image);
    if(!raw||!frame||!readable(raw,0xb8)||!readable(frame,80))return;
    uint32_t index=0,count=0,width=0,height=0,pitch=0,format=0;int32_t ev=0;
    memcpy(&index,frame+4,4);memcpy(&count,frame+8,4);memcpy(&width,frame+20,4);
    memcpy(&height,frame+24,4);memcpy(&pitch,frame+64,4);memcpy(&format,raw,4);
    if(width!=4608||height!=3456||pitch!=9216||!count||count>20||index>=count)return;
    memcpy(&ev,raw+4+index*4,4);uint32_t white[4],black[4],lsc_words[6];float wb[4];
    memcpy(white,raw+0x54,16);memcpy(black,raw+0x64,16);memcpy(wb,raw+0x74,16);memcpy(lsc_words,raw+0xa0,24);
    float shutter=0,sensor=0,isp=0,adrc=0;uint32_t lux=0,exposure_index=0;
    memcpy(&shutter,raw+0x84,4);memcpy(&sensor,raw+0x88,4);memcpy(&isp,raw+0x8c,4);
    memcpy(&lux,raw+0x94,4);memcpy(&exposure_index,raw+0x98,4);memcpy(&adrc,raw+0x9c,4);
    LOG("NIGHT_V3_RAW_INFO call=%u capture=%u index=%u count=%u ev=%d format=%x white=%u,%u,%u,%u black=%u,%u,%u,%u wb=%f,%f,%f,%f shutterMs=%f sensorGain=%f ispGain=%f lux=%u exposureIndex=%u adrc=%f lscWords=%x,%x,%x,%x,%x,%x original_values",
        call,night_process_reports.load(),index,count,ev,format,white[0],white[1],white[2],white[3],black[0],black[1],black[2],black[3],
        wb[0],wb[1],wb[2],wb[3],shutter,sensor,isp,lux,exposure_index,adrc,
        lsc_words[0],lsc_words[1],lsc_words[2],lsc_words[3],lsc_words[4],lsc_words[5]);
}
static uintptr_t audit_standard_night_add(uintptr_t handle,uintptr_t info,uintptr_t image) {
    int entry_errno=errno;unsigned call=night_add_reports.fetch_add(1);bool report=call<(night_scalar_enabled?20U:60U);
    if(report)observe_night_raw_info(info,image,call);
    if(report&&night_scalar_enabled)observe_night_signal(image,"input",night_process_reports.load());
    errno=entry_errno;uintptr_t result=original_night_add(handle,info,image);int saved=errno;
    if(report)LOG("NIGHT_V3_ADD_END call=%u original_status=%d",call,static_cast<int32_t>(result));
    errno=saved;return result;
}
static void observe_night_crop(uintptr_t pointer,const char*phase,unsigned capture) {
    const auto*crop=reinterpret_cast<const int32_t*>(pointer);int32_t values[4]={};
    if(crop&&readable(crop,sizeof(values))){memcpy(values,crop,sizeof(values));
        LOG("NIGHT_V3_CROP capture=%u phase=%s values=%d,%d,%d,%d original_values",capture,phase,values[0],values[1],values[2],values[3]);}
}
static uintptr_t audit_standard_night_process(uintptr_t a0,uintptr_t a1,uintptr_t a2,uintptr_t a3,
                                             uintptr_t a4,uintptr_t a5,uintptr_t a6,uintptr_t a7) {
    int entry_errno=errno;unsigned capture=night_process_reports.fetch_add(1);bool report=capture<(night_scalar_enabled?2U:3U);
    if(report){LOG("NIGHT_V3_PROCESS_BEGIN capture=%u x3=%llx x4=%llx callbacks_present=%d,%d original_args_retained",
        capture,(unsigned long long)a3,(unsigned long long)a4,a6!=0,a7!=0);
        if(!night_scalar_enabled)observe_standard_raw(a2,"before",capture);
        if(night_scalar_enabled)observe_night_signal(a2,"before",capture);
        observe_night_crop(a5,"before",capture);}
    ++night_depth;errno=entry_errno;
    uintptr_t result=original_night_process(a0,a1,a2,a3,a4,a5,a6,a7);int saved=errno;
    --night_depth;
    if(report){if(!night_scalar_enabled)observe_standard_raw(a2,"after",capture);
        if(night_scalar_enabled)observe_night_signal(a2,"after",capture);
        observe_night_crop(a5,"after",capture);
        LOG("NIGHT_V3_PROCESS_END capture=%u original_status=%d original_x0=%llx",capture,
            static_cast<int32_t>(result),(unsigned long long)result);}
    errno=saved;return result;
}

using ClInt = int32_t;
using ClUint = uint32_t;
using ClBits = uint64_t;
static_assert(sizeof(void*) == 8 && sizeof(size_t) == 8, "Night observer requires the verified 64-bit ABI");
using ClContextNotify = void (*)(const char*, const void*, size_t, void*);
using ClProgramNotify = void (*)(void*, void*);
static ClInt (*original_cl_platforms)(ClUint, void**, ClUint*);
static ClInt (*original_cl_devices)(void*, ClBits, ClUint, void**, ClUint*);
static ClInt (*original_cl_device_info)(void*, ClUint, size_t, void*, size_t*);
static void* (*original_cl_context)(const intptr_t*, ClUint, void* const*, ClContextNotify, void*, ClInt*);
static void* (*original_cl_queue)(void*, void*, ClBits, ClInt*);
static void* (*original_cl_queue_props)(void*, void*, const ClBits*, ClInt*);
static void* (*original_cl_binary)(void*, ClUint, void* const*, const size_t*, const unsigned char**, ClInt*, ClInt*);
static void* (*original_cl_source)(void*, ClUint, const char**, const size_t*, ClInt*);
static ClInt (*original_cl_build)(void*, ClUint, void* const*, const char*, ClProgramNotify, void*);
static void* (*original_cl_kernel)(void*, const char*, ClInt*);
static void* (*original_cl_buffer)(void*, ClBits, size_t, void*, ClInt*);

static ClInt audit_cl_platforms(ClUint capacity, void** platforms, ClUint* count) {
    ClInt result = original_cl_platforms(capacity, platforms, count);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), result != 0))
        LOG("NIGHT_CL clGetPlatformIDs result=%d capacity=%u count_ptr=%d count=%u first=%p", result, capacity,
            count != nullptr, result == 0 ? night_value(count) : 0,
            result == 0 && capacity ? night_value(platforms) : nullptr);
    return result;
}
static ClInt audit_cl_devices(void* platform, ClBits type, ClUint capacity, void** devices, ClUint* count) {
    ClInt result = original_cl_devices(platform, type, capacity, devices, count);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), result != 0))
        LOG("NIGHT_CL clGetDeviceIDs result=%d platform=%p type=%llu capacity=%u count_ptr=%d count=%u first=%p",
            result, platform, static_cast<unsigned long long>(type), capacity, count != nullptr,
            result == 0 ? night_value(count) : 0, result == 0 && capacity ? night_value(devices) : nullptr);
    return result;
}
static ClInt audit_cl_device_info(void* device, ClUint name, size_t capacity, void* value, size_t* size) {
    ClInt result = original_cl_device_info(device, name, capacity, value, size);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), result != 0))
        LOG("NIGHT_CL clGetDeviceInfo result=%d device=%p name=0x%x capacity=%zu size=%zu scalar32=%u", result,
            device, name, capacity, result == 0 ? night_value(size) : 0,
            result == 0 && capacity == sizeof(ClUint) ? night_value(static_cast<ClUint*>(value)) : 0);
    return result;
}
static void* audit_cl_context(const intptr_t* properties, ClUint count, void* const* devices,
                              ClContextNotify callback, void* user, ClInt* error) {
    int entry_errno = errno;
    bool report = night_report(__builtin_return_address(0));
    if (report && properties) {
        for (unsigned i = 0; i < 8; i += 2) {
            if (!readable(properties + i, sizeof(intptr_t))) break;
            intptr_t key = properties[i];
            if (!key || !readable(properties + i + 1, sizeof(intptr_t))) break;
            LOG("NIGHT_CL_CONTEXT_PROPERTY key=0x%llx value=0x%llx", static_cast<unsigned long long>(key),
                static_cast<unsigned long long>(properties[i + 1]));
        }
    }
    errno = entry_errno;
    void* result = original_cl_context(properties, count, devices, callback, user, error);
    NightErrnoRestore preserve_errno;
    if (report || (!result && night_report(__builtin_return_address(0), true)))
        LOG("NIGHT_CL clCreateContext result=%p count=%u error_ptr=%d error=%d", result, count,
                    error != nullptr, night_value(error));
    return result;
}
static void* audit_cl_queue(void* context, void* device, ClBits properties, ClInt* error) {
    void* result = original_cl_queue(context, device, properties, error);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), !result))
        LOG("NIGHT_CL clCreateCommandQueue result=%p flags=0x%llx error_ptr=%d error=%d", result,
            static_cast<unsigned long long>(properties), error != nullptr, night_value(error));
    return result;
}
static void* audit_cl_queue_props(void* context, void* device, const ClBits* properties, ClInt* error) {
    void* result = original_cl_queue_props(context, device, properties, error);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), !result))
        LOG("NIGHT_CL clCreateCommandQueueWithProperties result=%p error_ptr=%d error=%d", result,
            error != nullptr, night_value(error));
    return result;
}
static void* audit_cl_binary(void* context, ClUint count, void* const* devices, const size_t* lengths,
                             const unsigned char** binaries, ClInt* status, ClInt* error) {
    void* result = original_cl_binary(context, count, devices, lengths, binaries, status, error);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), !result))
        LOG("NIGHT_CL clCreateProgramWithBinary result=%p count=%u first_length=%zu binary_status_ptr=%d first_status=%d error_ptr=%d error=%d",
            result, count, count ? night_value(lengths) : 0, status != nullptr,
            count ? night_value(status) : 0, error != nullptr, night_value(error));
    return result;
}
static void* audit_cl_source(void* context, ClUint count, const char** strings, const size_t* lengths, ClInt* error) {
    void* result = original_cl_source(context, count, strings, lengths, error);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), !result))
        LOG("NIGHT_CL clCreateProgramWithSource result=%p count=%u error_ptr=%d error=%d", result, count,
            error != nullptr, night_value(error));
    return result;
}
static ClInt audit_cl_build(void* program, ClUint count, void* const* devices, const char* options,
                            ClProgramNotify callback, void* user) {
    ClInt result = original_cl_build(program, count, devices, options, callback, user);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), result != 0))
        LOG("NIGHT_CL clBuildProgram result=%d program=%p device_count=%u callback_present=%d", result, program,
            count, callback != nullptr);
    return result;
}
static void* audit_cl_kernel(void* program, const char* name, ClInt* error) {
    void* result = original_cl_kernel(program, name, error);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), !result))
        LOG("NIGHT_CL clCreateKernel result=%p program=%p error_ptr=%d error=%d", result, program,
            error != nullptr, night_value(error));
    return result;
}
static void* audit_cl_buffer(void* context, ClBits flags, size_t size, void* host, ClInt* error) {
    void* result = original_cl_buffer(context, flags, size, host, error);
    NightErrnoRestore preserve_errno;
    if (night_report(__builtin_return_address(0), !result))
        LOG("NIGHT_CL clCreateBuffer result=%p size=%zu flags=0x%llx error_ptr=%d error=%d", result, size,
            static_cast<unsigned long long>(flags), error != nullptr, night_value(error));
    return result;
}

// Return wrappers only to these exact original callers' dlsym lookups. This
// avoids patching a shared OpenCL entrypoint or a driver's aliased stub address.
static void* audit_night_symbol(void* result, const char* symbol, const void* caller) {
    if ((!night_observer_enabled&&!night_scalar_enabled) || !active.load() || !result || !symbol) return result;
    Dl_info source{}, target{};
    if (!dladdr(caller, &source) || !source.dli_fname || !dladdr(result, &target) || !target.dli_fname) return result;
    if(night_observer_enabled&&!strcmp(symbol,"ARC_PSN_GetVersion")&&!strcmp(source.dli_fname,
        "/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so")&&!strcmp(target.dli_fname,
        "/system/priv-app/OppoCamera/lib/arm64/libarcsoft_portrait_super_night_raw.so")){
        bind_front_night_observer(source);
        return result;
    }
    if(!strcmp(symbol,"ARC_SN_AddOneInputInfo")) {
        if(strcmp(source.dli_fname,"/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so")
                ||strcmp(target.dli_fname,night_library)||!target.dli_fbase
                ||reinterpret_cast<uintptr_t>(result)-reinterpret_cast<uintptr_t>(target.dli_fbase)!=0x81c8)return result;
        original_night_add=reinterpret_cast<NightAddRegisters>(result);
        LOG("NIGHT_V3_RAW_INFO_OBSERVER_BOUND originalSDK81c8, V3 scalar-layout guard, max60; no mutation");
        return reinterpret_cast<void*>(audit_standard_night_add);
    }
    if (!strcmp(symbol,"ARC_SN_Process")) {
        if(strcmp(source.dli_fname,"/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so")
                ||strcmp(target.dli_fname,night_library)||!target.dli_fbase
                ||reinterpret_cast<uintptr_t>(result)-reinterpret_cast<uintptr_t>(target.dli_fbase)!=0x8514)return result;
        original_night_process=reinterpret_cast<NightProcessRegisters>(result);
        LOG("NIGHT_V3_PROCESS_OBSERVER_BOUND exact originalSDK8514; max3; no mutation");
        return reinterpret_cast<void*>(audit_standard_night_process);
    }
    if (!strcmp(symbol, "ARC_SN_PreProcess")) {
        if (strcmp(source.dli_fname, "/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so")
                || strcmp(target.dli_fname, night_library)) return result;
        if(night_scalar_enabled&&(!target.dli_fbase||reinterpret_cast<uintptr_t>(result)
                -reinterpret_cast<uintptr_t>(target.dli_fbase)!=0x9514))return result;
        original_night_preprocess = reinterpret_cast<NightPreprocess>(result);
        LOG("NIGHT_OBSERVER_BIND symbol=%s source=%s target=%s", symbol, source.dli_fname, target.dli_fname);
        return reinterpret_cast<void*>(audit_night_preprocess);
    }
    if(!night_observer_enabled)return result;
    if (strcmp(source.dli_fname, night_library)) return result;
    // The current trace loads the vendor OpenCL dispatcher through vndksupport.
    // Accept only explicit driver locations, never similarly named libraries.
    if (strcmp(target.dli_fname, "/vendor/lib64/libOpenCL.so")
            && strcmp(target.dli_fname, "/vendor/lib64/libOpenCL_adreno.so")
            && strcmp(target.dli_fname, "/system/priv-app/OppoCamera/lib/arm64/libOpenCL.so")) return result;
#define INSTALL_NIGHT_CL(symbol_name, wrapper, original) \
    if (!strcmp(symbol, symbol_name)) { \
        original = reinterpret_cast<decltype(original)>(result); \
        LOG("NIGHT_OBSERVER_BIND symbol=%s target=%s", symbol, target.dli_fname); \
        return reinterpret_cast<void*>(wrapper); \
    }
    INSTALL_NIGHT_CL("clGetPlatformIDs", audit_cl_platforms, original_cl_platforms)
    INSTALL_NIGHT_CL("clGetDeviceIDs", audit_cl_devices, original_cl_devices)
    INSTALL_NIGHT_CL("clGetDeviceInfo", audit_cl_device_info, original_cl_device_info)
    INSTALL_NIGHT_CL("clCreateContext", audit_cl_context, original_cl_context)
    INSTALL_NIGHT_CL("clCreateCommandQueue", audit_cl_queue, original_cl_queue)
    INSTALL_NIGHT_CL("clCreateCommandQueueWithProperties", audit_cl_queue_props, original_cl_queue_props)
    INSTALL_NIGHT_CL("clCreateProgramWithBinary", audit_cl_binary, original_cl_binary)
    INSTALL_NIGHT_CL("clCreateProgramWithSource", audit_cl_source, original_cl_source)
    INSTALL_NIGHT_CL("clBuildProgram", audit_cl_build, original_cl_build)
    INSTALL_NIGHT_CL("clCreateKernel", audit_cl_kernel, original_cl_kernel)
    INSTALL_NIGHT_CL("clCreateBuffer", audit_cl_buffer, original_cl_buffer)
#undef INSTALL_NIGHT_CL
    return result;
}
