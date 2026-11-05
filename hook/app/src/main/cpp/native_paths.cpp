// LSPosed Native Hook API: https://github.com/LSPosed/LSPosed/wiki/Native-Hook
// Bionic __loader_* entrypoints retain the ORIGINAL caller address/namespace.
// No original ELF or APK bytes are changed on disk. Only this scoped process
// redirects allowlisted absolute OEM library paths to verified donor copies.
#include <android/dlext.h>
#include <android/log.h>
#include <jni.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <atomic>
#include "native_allowlist.h"

using HookFunction = int (*)(void*, void*, void**);
using UnhookFunction = int (*)(void*);
using OnLoaded = void (*)(const char*, void*);
struct NativeAPIEntries { uint32_t version; HookFunction hook_func; UnhookFunction unhook_func; };
static HookFunction hook_function;
static char library_directory[PATH_MAX];
static constexpr const char* application_directory = "/system/priv-app/OppoCamera";
static std::atomic<bool> active{false};
static void* (*original_dlopen)(const char*, int, const void*);
static void* (*original_ext)(const char*, int, const android_dlextinfo*, const void*);
static void* (*original_symbol)(void*, const char*, const void*);
static std::atomic<unsigned> missing_symbols{0};
static android_namespace_t* snpe_namespace;
static android_namespace_t* snpe_platform_namespace;
static constexpr const char* snpe_directory =
        "/system/priv-app/OppoCamera/fresh_native/anc_snpe/lib64";
// Optional, separately pinned current-ROM engine: absent from the recovered
// F14 ODM image. Never merge this dependency group's C++ runtime with the APK.
static android_namespace_t* darksight_namespace;
static android_namespace_t* darksight_platform_namespace;
static android_namespace_t* darksight_gpu_namespace;
static thread_local unsigned darksight_gpu_depth;
static constexpr const char* darksight_gpu_directory =
        "/system/priv-app/OppoCamera/fresh_native/darksight_f14_gpu/lib64";
static constexpr const char* darksight_directory =
        "/system/priv-app/OppoCamera/fresh_native/darksight_rom/lib64";
static std::atomic<unsigned> darksight_loader_reports{0};
// One-process comparison only. The default remains the current working route.
static bool darksight_unavailable_trial;
static bool shared_snpe_group;
static constexpr const char* darksight_missing_engine =
        "/system/priv-app/OppoCamera/fresh_native/darksight_rom/lib64/ocfresh_unavailable_libapsdarksight.so";
using SuperInit = uintptr_t (*)(const void*, uintptr_t, void*, void*);
using SuperFrame = uintptr_t (*)(void*, void*, uintptr_t, void*);
using SuperOutput = uintptr_t (*)(void*, void*, void*);
using SuperProcess = uintptr_t (*)(const void*, void*);
static SuperInit original_super_init;
static SuperFrame original_super_frame;
static SuperOutput original_super_output;
static SuperProcess original_super_process;
static std::atomic<bool> init_hooked{false}, frame_hooked{false}, output_hooked{false};
static std::atomic<bool> process_hooked{false};
static std::atomic<unsigned> super_init_reports{0}, super_frame_reports{0}, super_output_reports{0};
static char capture_directory[PATH_MAX];
static bool capture_enabled;
static bool capture_highres_enabled;
static bool copy_frames_enabled;
static bool highres_copy_enabled;
static std::atomic<unsigned> capture_count{0};
struct CaptureAudit {
    uint32_t descriptor[19];
    void* engine;
    void* output_y;
    void* output_uv;
    unsigned number;
    unsigned frame;
};
static thread_local CaptureAudit capture_audit{};

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "OcFreshNative", __VA_ARGS__)
#include "timelapse_compat.h"
#include "environment_native.h"

// Optional, finite image observation for one diagnostic capture only. This is
// off unless our marker exists in this app's private files before configuration.
// Copy visible NV21 rows to our files; never change the original image buffers.
// Input dimensions/padding and output buffer stride are confirmed by the
// original API's initialization code. Reject any uncovered mapped read range.
static bool readable(const void* pointer, size_t length) {
    uintptr_t cursor = reinterpret_cast<uintptr_t>(pointer);
    if (!cursor || !length || cursor + length < cursor) return false;
    const uintptr_t end = cursor + length;
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) return false;
    char line[1024], permissions[5]; unsigned long begin, finish;
    while (cursor < end && fgets(line, sizeof(line), maps)) {
        if (sscanf(line, "%lx-%lx %4s", &begin, &finish, permissions) != 3) continue;
        if (cursor >= begin && cursor < finish && permissions[0] == 'r') cursor = finish;
    }
    fclose(maps);
    return cursor >= end;
}
#include "night_observer.h"
#include "darksight_observer.h"
#include "front_night_dsp_session.h"
#include "beauty_observer.h"

static void save_capture_planes(const char* label, const void* y, const void* uv,
                                uint32_t width, uint32_t height, uint32_t stride) {
    if (!capture_audit.number || !y || !uv || width > stride || stride > 8192
            || width < 2 || height < 2 || height > 8192 || (width & 1) || (height & 1)) return;
    if (!readable(y, static_cast<size_t>(stride) * (height - 1) + width)
            || !readable(uv, static_cast<size_t>(stride) * (height / 2 - 1) + width)) {
        LOG("SUPERPHOTO_DUMP_REFUSED unmapped planes capture=%u label=%s", capture_audit.number, label);
        return;
    }
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/capture_%u_%s_%ux%u.nv21", capture_directory,
                     capture_audit.number, label, width, height);
    if (n < 0 || static_cast<size_t>(n) >= sizeof(path)) return;
    FILE* output = fopen(path, "wb");
    if (!output) { LOG("SUPERPHOTO_DUMP_OPEN_FAILED capture=%u label=%s errno=%d", capture_audit.number, label, errno); return; }
    bool good = true;
    for (uint32_t row = 0; row < height && good; ++row)
        good = fwrite(static_cast<const uint8_t*>(y) + static_cast<size_t>(row) * stride, 1, width, output) == width;
    for (uint32_t row = 0; row < height / 2 && good; ++row)
        good = fwrite(static_cast<const uint8_t*>(uv) + static_cast<size_t>(row) * stride, 1, width, output) == width;
    fclose(output);
    LOG("SUPERPHOTO_DUMP capture=%u label=%s width=%u height=%u stride=%u complete=%d",
        capture_audit.number, label, width, height, stride, good);
}

// Read-only native engine boundary audit. Actual V2 planar entry points are
// verified from the original libAlgoInterface function table and original API.
// The non-planar functions are also resolved by the wrapper but unused by V2.
// No pointer dereference is needed for the input/output plane audit.
// Read-only native engine boundary audit. The original F14 wrapper's disassembly
// confirms four integer/pointer arguments and a76-byte initialization descriptor.
// Return the original x0 value unchanged. Pixel reads are separately opt-in,
// finite and never modify camera buffers.
static uintptr_t audit_super_init(const void* parameters, uintptr_t cores, void* output, void* engine) {
    if (parameters && super_init_reports.fetch_add(1) < 12) {
        uint32_t words[19];
        memcpy(words, parameters, sizeof(words));
        char text[768]; size_t used = 0;
        for (unsigned i = 0; i < 19 && used < sizeof(text); ++i) {
            int n = snprintf(text + used, sizeof(text) - used, "%s%u:%u", i ? "," : "", i, words[i]);
            if (n < 0 || static_cast<size_t>(n) >= sizeof(text) - used) break;
            used += static_cast<size_t>(n);
        }
        LOG("SUPERPHOTO_INIT_DESCRIPTOR cores=%lu words=[%s]", static_cast<unsigned long>(cores), text);
    }
    uintptr_t result = original_super_init(parameters, cores, output, engine);
    environment_event("SDK_RETURN","Superphoto.Init",__builtin_return_address(0),result,engine);
    capture_audit = {};
    if (capture_enabled && parameters && engine) {
        uint32_t words[19]; memcpy(words, parameters, sizeof(words));
        // Pixel observation requires an explicit private marker. Keep the
        // existing tele probe and the separately requested64M tuple distinct.
        bool tele_tuple = words[1] == 8 && words[2] == 4160 && words[3] == 3120
                && words[4] == 16 && words[5] == 0 && words[7] == 4160 && words[8] == 3120
                && words[15] == 4160 && words[16] == 3120;
        bool highres_tuple = capture_highres_enabled && words[1] == 8
                && words[2] == 4640 && words[3] == 3472 && words[4] == 48 && words[5] == 32
                && words[7] == 4640 && words[8] == 3472 && words[15] == 4672 && words[16] == 3472;
        if ((!capture_highres_enabled && tele_tuple) || highres_tuple) {
            unsigned number = capture_count.fetch_add(1) + 1;
            if (number <= 1 && *static_cast<void**>(engine)) {
                memcpy(capture_audit.descriptor, words, sizeof(words));
                capture_audit.engine = *static_cast<void**>(engine);
                capture_audit.number = number;
            }
        }
    }
    return result;
}
static uintptr_t audit_super_frame(void* y, void* uv, uintptr_t kind, void* engine) {
    if (super_frame_reports.fetch_add(1) < 32) LOG(
            "SUPERPHOTO_INPUT_PLANES y=%p uv=%p uvMinusY=%ld kind=%lu engine=%p", y, uv,
            static_cast<long>(reinterpret_cast<uintptr_t>(uv) - reinterpret_cast<uintptr_t>(y)),
            static_cast<unsigned long>(kind), engine);
    bool audit = capture_audit.number && capture_audit.engine == engine;
    unsigned frame = 999;
    if (audit) {
        frame = capture_audit.frame++;
        if (readable(engine, 0x708)) {
            const uint8_t* state = static_cast<const uint8_t*>(engine);
            uint32_t geometry[4]; memcpy(geometry, state + 0x4f8, sizeof(geometry));
            uint32_t counter, format, original_format, legacy, crop[2];
            memcpy(&counter, state + 0x55c, 4);
            memcpy(&format, state + 0x6b0, 4);
            memcpy(&original_format, state + 0x6b4, 4);
            memcpy(&legacy, state + 0x6ec, 4);
            memcpy(crop, state + 0x6c8, sizeof(crop));
            LOG("SUPERPHOTO_ENGINE_STATE frame=%u width=%u height=%u padRows=%u padCols=%u counter=%u format=%u originalFormat=%u legacy=%u cropWidth=%u cropHeight=%u",
                frame, geometry[0], geometry[1], geometry[2], geometry[3], counter, format, original_format, legacy, crop[0], crop[1]);
        }
        if (frame == 0 || frame == 6 || frame == 7) save_capture_planes(
                frame == 0 ? "input0_before" : (frame == 6 ? "input6_before" : "input7_before"),
                y, uv, capture_audit.descriptor[2], capture_audit.descriptor[3],
                capture_audit.descriptor[2] + capture_audit.descriptor[5]);
    }
    // The original legacy entry supports allocate=1, copying into engine-owned
    // storage. allocate=0 crops/compacts Y and converts UV in the borrowed input.
    // APS repeats an earlier plane pointer as its final frame, so the borrowed
    // path processes that buffer twice. Use the original copying mode only for
    // the separately attested tele and F14 highres layouts below.
    uintptr_t delegated_kind = kind;
    if ((copy_frames_enabled || highres_copy_enabled) && kind == 0 && readable(engine, 0x708)) {
        const uint8_t* state = static_cast<const uint8_t*>(engine);
        uint32_t geometry[4], legacy, format;
        memcpy(geometry, state + 0x4f8, sizeof(geometry));
        memcpy(&legacy, state + 0x6ec, 4);
        memcpy(&format, state + 0x6b4, 4);
        bool tele = copy_frames_enabled && legacy == 1 && format == 10 && geometry[0] == 4160 && geometry[1] == 3120
                && geometry[2] == 16 && geometry[3] == 0
                && reinterpret_cast<uintptr_t>(uv) - reinterpret_cast<uintptr_t>(y) == 13045760;
        // F14 QCFA/superphoto layout: boundary observation proved frame6/7
        // reuse frame0/1 after allocate0 modified their borrowed pixel bytes.
        bool highres = highres_copy_enabled && legacy == 1 && format == 10
                && geometry[0] == 4640 && geometry[1] == 3472 && geometry[2] == 48 && geometry[3] == 32
                && reinterpret_cast<uintptr_t>(uv) - reinterpret_cast<uintptr_t>(y) == 16445440;
        if (tele || highres) {
            delegated_kind = 1;
            if (super_frame_reports.load() <= 32)
                LOG("SUPERPHOTO_OWNED_FRAME originalAllocate=0 delegatedAllocate=1 legacy=1 width=%u height=%u scope=%s",
                        geometry[0], geometry[1], highres ? "originalF14_highres" : "OP4A89_tele");
        }
    }
    uintptr_t result = original_super_frame(y, uv, delegated_kind, engine);
    environment_event("SDK_RETURN","Superphoto.AddFramePlanar",__builtin_return_address(0),result,engine);
    if (audit && frame == 0) save_capture_planes("input0_after_add", y, uv,
            capture_audit.descriptor[2], capture_audit.descriptor[3],
            capture_audit.descriptor[2] + capture_audit.descriptor[5]);
    return result;
}
static uintptr_t audit_super_output(void* y, void* uv, void* engine) {
    if (super_output_reports.fetch_add(1) < 12) LOG(
            "SUPERPHOTO_OUTPUT_PLANES y=%p uv=%p uvMinusY=%ld engine=%p", y, uv,
            static_cast<long>(reinterpret_cast<uintptr_t>(uv) - reinterpret_cast<uintptr_t>(y)), engine);
    if (capture_audit.number && capture_audit.engine == engine) {
        capture_audit.output_y = y;
        capture_audit.output_uv = uv;
    }
    uintptr_t result=original_super_output(y, uv, engine);
    environment_event("SDK_RETURN","Superphoto.AddOutput",__builtin_return_address(0),result,engine);
    return result;
}
static uintptr_t audit_super_process(const void* parameters, void* engine) {
    uintptr_t result = original_super_process(parameters, engine);
    environment_event("SDK_RETURN","Superphoto.Process",__builtin_return_address(0),result,engine);
    if (capture_audit.number && capture_audit.engine == engine) {
        save_capture_planes("output_after", capture_audit.output_y, capture_audit.output_uv,
                            capture_audit.descriptor[7], capture_audit.descriptor[8],
                            capture_audit.descriptor[15]);
        capture_audit = {};
    }
    return result;
}

static void audit_super_symbol(void* result, const char* symbol, const void* caller) {
    if (!result || !symbol || !active.load() || !hook_function) return;
    if (strcmp(symbol, "VDInitializeSuperPhotoWithCores")
            && strcmp(symbol, "VDSuperPhoto_AddFramePlanar") && strcmp(symbol, "VDSuperPhoto_AddOutputBufferPlanes")
            && strcmp(symbol, "VDSuperPhoto_Process")) return;
    Dl_info source{}, target{};
    if (!dladdr(caller, &source) || !source.dli_fname || strcmp(source.dli_fname,
            "/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so")) return;
    if (!dladdr(result, &target) || !target.dli_fname || strcmp(target.dli_fname,
            "/system/priv-app/OppoCamera/lib/arm64/libVDSuperPhotoAPI.so")) return;
    int status = -1;
    if (!strcmp(symbol, "VDInitializeSuperPhotoWithCores") && !init_hooked.exchange(true))
        status = hook_function(result, reinterpret_cast<void*>(audit_super_init),
                               reinterpret_cast<void**>(&original_super_init));
    else if (!strcmp(symbol, "VDSuperPhoto_AddFramePlanar") && !frame_hooked.exchange(true))
        status = hook_function(result, reinterpret_cast<void*>(audit_super_frame),
                               reinterpret_cast<void**>(&original_super_frame));
    else if (!strcmp(symbol, "VDSuperPhoto_AddOutputBufferPlanes") && !output_hooked.exchange(true))
        status = hook_function(result, reinterpret_cast<void*>(audit_super_output),
                               reinterpret_cast<void**>(&original_super_output));
    else if (!strcmp(symbol, "VDSuperPhoto_Process") && !process_hooked.exchange(true))
        status = hook_function(result, reinterpret_cast<void*>(audit_super_process),
                               reinterpret_cast<void**>(&original_super_process));
    else return;
    LOG("SUPERPHOTO_METADATA_AUDIT symbol=%s status=%d original function delegated", symbol, status);
}

// The module has its own classloader namespace, where dlsym(RTLD_DEFAULT)
// does not expose the linker's loader entrypoints. Resolve their exported ELF
// names from the already mapped linker; no hardcoded offsets or file changes.
struct SymbolQuery { const char* name; void* result; };
static int linker_symbol(dl_phdr_info* info, size_t, void* argument) {
    if (!info->dlpi_name || (!strstr(info->dlpi_name, "/linker64")
            && !strstr(info->dlpi_name, "ld-android.so"))) return 0;
    auto* query = static_cast<SymbolQuery*>(argument);
    const ElfW(Dyn)* dynamic = nullptr;
    for (size_t i = 0; i < info->dlpi_phnum; ++i)
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC)
            dynamic = reinterpret_cast<const ElfW(Dyn)*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    if (!dynamic) return 0;
    const ElfW(Sym)* symbols = nullptr;
    const char* strings = nullptr;
    const uint32_t* gnu = nullptr;
    size_t string_size = 0, count = 0;
    for (const ElfW(Dyn)* entry = dynamic; entry->d_tag != DT_NULL; ++entry) {
        uintptr_t pointer = info->dlpi_addr + entry->d_un.d_ptr;
        switch (entry->d_tag) {
            case DT_SYMTAB: symbols = reinterpret_cast<const ElfW(Sym)*>(pointer); break;
            case DT_STRTAB: strings = reinterpret_cast<const char*>(pointer); break;
            case DT_STRSZ: string_size = entry->d_un.d_val; break;
            case DT_HASH: count = reinterpret_cast<const uint32_t*>(pointer)[1]; break;
            case DT_GNU_HASH: gnu = reinterpret_cast<const uint32_t*>(pointer); break;
        }
    }
    if (!symbols || !strings) return 0;
    if (!count && gnu && gnu[0] < 65536 && gnu[2] < 65536) {
        const uint32_t* buckets = reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const ElfW(Addr)*>(gnu + 4) + gnu[2]);
        const uint32_t* chains = buckets + gnu[0];
        uint32_t largest = 0;
        for (uint32_t i = 0; i < gnu[0]; ++i) if (buckets[i] > largest) largest = buckets[i];
        if (largest >= gnu[1] && largest < 65536) {
            while (largest < 65536 && !(chains[largest - gnu[1]] & 1)) ++largest;
            count = largest + 1;
        }
    }
    if (count > 65536) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (symbols[i].st_shndx == SHN_UNDEF || symbols[i].st_name >= string_size) continue;
        if (!strcmp(strings + symbols[i].st_name, query->name)) {
            query->result = reinterpret_cast<void*>(info->dlpi_addr + symbols[i].st_value);
            return 1;
        }
    }
    return 0;
}
static void* resolve_loader(const char* name) {
    void* result = dlsym(RTLD_DEFAULT, name);
    if (result) return result;
    SymbolQuery query{name, nullptr};
    dl_iterate_phdr(linker_symbol, &query);
    return query.result;
}

static bool mapped_path(const char* path, const void* caller, char* mapped) {
    if (!active.load(std::memory_order_acquire) || !path) return false;
    const char* name = nullptr;
    bool vendor = false;
    for (const char* prefix : {"/odm/lib64/", "/vendor/lib64/", "/system/vendor/lib64/"}) {
        if (!strncmp(path, prefix, strlen(prefix))) {
            name = path + strlen(prefix);
            vendor = strcmp(prefix, "/odm/lib64/") != 0;
            break;
        }
    }
    if (!name || strchr(name, '/')) return false;
    bool allowed = false;
    for (const char* candidate : kOriginalLibraries) {
        if (!strcmp(name, candidate)) { allowed = true; break; }
    }
    if (!allowed) return false;
    // Do not redirect Android framework/other modules' library loads.
    Dl_info information{};
    if (!dladdr(caller, &information) || !information.dli_fname
            || strncmp(information.dli_fname, application_directory, strlen(application_directory))) return false;
    // Explicit vendor paths retain vendor donor provenance even when ODM has
    // a different library with the same basename. Do not merge those variants.
    int length = vendor
            ? snprintf(mapped, PATH_MAX, "%s/fresh_native/vendor/lib64/%s", application_directory, name)
            : snprintf(mapped, PATH_MAX, "%s/%s", library_directory, name);
    return length > 0 && length < PATH_MAX && access(mapped, R_OK) == 0;
}

// F14's app and ODM ship DIFFERENT libc++_shared builds. Keep the C-only SNPE
// loader boundary separate: its original Symphony dependency otherwise binds
// app-local C++/unwind symbols and deadlocks in a constructor on this ROM.
// Namespace APIs/signatures were checked against Android16 bionic dlfcn.cpp.
// No algorithm, exception result or constructor is skipped.
static bool snpe_request(const char* path, const void* caller) {
    if (!active.load(std::memory_order_acquire) || !snpe_namespace || !path) return false;
    const char* name = strrchr(path, '/');
    name = name ? name + 1 : path;
    if (strcmp(name, "libsnpe_loader.so")) return false;
    Dl_info information{};
    if (!dladdr(caller, &information) || !information.dli_fname) return false;
    return !strcmp(information.dli_fname,
                   "/system/priv-app/OppoCamera/lib/arm64/libAncHumBokeh.so")
            || !strcmp(information.dli_fname,
                       "/system/priv-app/OppoCamera/lib/arm64/libAncHumBokehPost.so");
}

static void* open_snpe(const char* path, int flags, const void* caller) {
    char target[PATH_MAX];
    snprintf(target, sizeof(target), "%s/libsnpe_loader.so", snpe_directory);
    android_dlextinfo extra{};
    extra.flags = ANDROID_DLEXT_USE_NAMESPACE;
    extra.library_namespace = snpe_namespace;
    void* result = original_ext(target, flags, &extra, caller);
    LOG("SNPE_NAMESPACE_LOAD %s -> %s result=%p", path, target, result);
    return result;
}

static bool snpe_opencl_request(const char* path, const void* caller) {
    if (!snpe_namespace || !snpe_platform_namespace || !path
            || (strcmp(path, "/vendor/lib64/libOpenCL.so")
                && strcmp(path, "/system/vendor/lib64/libOpenCL.so"))) return false;
    Dl_info source{};
    return dladdr(caller, &source) && source.dli_fname
            && !strcmp(source.dli_fname,
                "/system/priv-app/OppoCamera/fresh_native/anc_snpe/lib64/libSNPE.so");
}
static void* open_snpe_opencl(int flags, const void* caller) {
    android_dlextinfo extra{};
    extra.flags = ANDROID_DLEXT_USE_NAMESPACE;
    extra.library_namespace = snpe_platform_namespace;
    void* result = original_ext("/vendor/lib64/libOpenCL.so", flags, &extra, caller);
    const int saved = errno;
    LOG("ANC_SNPE_OPENCL_VENDOR_NAMESPACE result=%p; unchanged ROM dispatcher/backend", result);
    errno = saved;
    return result;
}

static bool darksight_request(const char* path, const void* caller) {
    if (!active.load(std::memory_order_acquire) || !darksight_namespace || !path
            || (strcmp(path, "/odm/lib64/libapsdarksight.so")
                && strcmp(path, "/odm/lib64/libaps_frame_registration.so"))) return false;
    Dl_info information{};
    return dladdr(caller, &information) && information.dli_fname &&
            !strcmp(information.dli_fname,
                "/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so");
}
static void* open_darksight(const char* path, int flags, const void* caller) {
    char target[PATH_MAX];
    const bool unavailable = darksight_unavailable_trial
            && !strcmp(path, "/odm/lib64/libapsdarksight.so");
    if (unavailable) snprintf(target, sizeof(target), "%s", darksight_missing_engine);
    else snprintf(target, sizeof(target), "%s/%s", darksight_directory, strrchr(path, '/')+1);
    android_dlextinfo extra{};
    extra.flags = ANDROID_DLEXT_USE_NAMESPACE;
    extra.library_namespace = darksight_namespace;
    void* result = original_ext(target, flags, &extra, caller);
    const int saved_errno = errno;
    LOG("DARKSIGHT_NAMESPACE_LOAD %s -> %s result=%p unchanged current-ROM dependency", path, target, result);
    if (unavailable)
        LOG("DARKSIGHT_AVAILABILITY_TEST engine_unavailable=1 result=%p errno=%d original_namespace_loader_called=1", result, saved_errno);
    errno = saved_errno;
    return result;
}
static void observe_darksight_load(const char* path, void* result, const void* caller) {
    const int saved_errno = errno;
    Dl_info source{};
    if (darksight_namespace && dladdr(caller, &source) && source.dli_fname
            && !strncmp(source.dli_fname, darksight_directory, strlen(darksight_directory))
            && darksight_loader_reports.fetch_add(1) < 60) {
        LOG("DARKSIGHT_DEPENDENCY_LOAD path=%s result=%p caller=%s", path ? path : "NULL", result, source.dli_fname);
    }
    errno = saved_errno;
}
static bool darksight_opencl_request(const char* path, const void* caller) {
    if (!darksight_namespace || !darksight_platform_namespace || !path
            || strcmp(path, "/vendor/lib64/libOpenCL.so")) return false;
    Dl_info source{};
    return dladdr(caller, &source) && source.dli_fname
            && !strcmp(source.dli_fname,
                "/system/priv-app/OppoCamera/fresh_native/darksight_rom/lib64/libSNPE.so");
}
static void* open_darksight_opencl(const char* path, int flags, const void* caller) {
    android_dlextinfo extra{};
    extra.flags = ANDROID_DLEXT_USE_NAMESPACE;
    extra.library_namespace = darksight_gpu_namespace ? darksight_gpu_namespace : darksight_platform_namespace;
    char target[PATH_MAX];
    snprintf(target, sizeof(target), "%s/libOpenCL.so", darksight_gpu_directory);
    const char* selected = darksight_gpu_namespace ? target : path;
    void* result = original_ext(selected, flags, &extra, caller);
    int saved_errno = errno;
    LOG("DARKSIGHT_OPENCL_VENDOR_NAMESPACE path=%s selected=%s result=%p F14isolated=%d", path, selected, result, darksight_gpu_namespace != nullptr);
    errno = saved_errno;
    return result;
}
static bool darksight_gpu_request(const char* path, const void* caller) {
    if (!darksight_gpu_namespace || !darksight_gpu_depth || !path) return false;
    const char* name = strrchr(path, '/'); name = name ? name+1 : path;
    if (strcmp(name, "libCB.so") && strcmp(name, "libllvm-qcom.so")) return false;
    Dl_info source{};
    if (!dladdr(caller, &source) || !source.dli_fname) return false;
    // Original F14 OpenCL/CB use android_load_sphal_library, which normally
    // escapes to the system's vendor namespace. Keep only these two loads
    // inside their matched private group during the real darksight C call.
    return !strncmp(source.dli_fname, darksight_gpu_directory, strlen(darksight_gpu_directory))
            || !strcmp(source.dli_fname, "/system/lib64/libvndksupport.so");
}
static void* open_darksight_gpu(const char* path, int flags, const void* caller) {
    const char* name = strrchr(path, '/'); name = name ? name+1 : path;
    char target[PATH_MAX]; snprintf(target, sizeof(target), "%s/%s", darksight_gpu_directory, name);
    android_dlextinfo extra{}; extra.flags = ANDROID_DLEXT_USE_NAMESPACE;
    extra.library_namespace = darksight_gpu_namespace;
    void* result = original_ext(target, flags, &extra, caller);
    int saved_errno = errno;
    LOG("DARKSIGHT_F14_GPU_LOAD path=%s result=%p original matching driver file", target, result);
    errno = saved_errno; return result;
}

static void* route_dlopen(const char* path, int flags, const void* caller) {
    if (snpe_opencl_request(path, caller)) return open_snpe_opencl(flags, caller);
    if (darksight_gpu_request(path, caller)) return open_darksight_gpu(path, flags, caller);
    if (darksight_opencl_request(path, caller)) return open_darksight_opencl(path, flags, caller);
    if (darksight_request(path, caller)) return open_darksight(path, flags, caller);
    if (snpe_request(path, caller)) return open_snpe(path, flags, caller);
    char mapped[PATH_MAX];
    if (!mapped_path(path, caller, mapped)) {
        void* result = original_dlopen(path, flags, caller);
        observe_darksight_load(path, result, caller);
        return result;
    }
    void* result = original_dlopen(mapped, flags, caller);
    if(result&&!strcmp(path,"/odm/lib64/libarcsoft_portrait_super_night_raw.so"))prepare_front_dsp_session(result,caller);
    LOG("DLOPEN_REDIRECT %s -> %s result=%p", path, mapped, result);
    return result;
}

static void* route_ext(const char* path, int flags,
                          const android_dlextinfo* extra, const void* caller) {
    if (!extra && snpe_opencl_request(path, caller)) return open_snpe_opencl(flags, caller);
    if ((!extra || extra->flags == ANDROID_DLEXT_USE_NAMESPACE)
            && darksight_gpu_request(path, caller)) return open_darksight_gpu(path, flags, caller);
    if (!extra && darksight_opencl_request(path, caller)) return open_darksight_opencl(path, flags, caller);
    if (!extra && darksight_request(path, caller)) return open_darksight(path, flags, caller);
    if (!extra && snpe_request(path, caller)) return open_snpe(path, flags, caller);
    char mapped[PATH_MAX];
    // File descriptors and explicit namespaces have semantics we must retain.
    if (extra || !mapped_path(path, caller, mapped)) return original_ext(path, flags, extra, caller);
    void* result = original_ext(mapped, flags, extra, caller);
    if(result&&!strcmp(path,"/odm/lib64/libarcsoft_portrait_super_night_raw.so"))prepare_front_dsp_session(result,caller);
    LOG("DLOPEN_EXT_REDIRECT %s -> %s result=%p", path, mapped, result);
    return result;
}

static void* redirect_dlopen(const char*path,int flags,const void*caller){
    void*result=route_dlopen(path,flags,caller);int saved=errno;
    if(environment_camera_caller(caller))environment_event("DLOPEN",path,caller,reinterpret_cast<uintptr_t>(result));
    errno=saved;return result;
}
static void* redirect_ext(const char*path,int flags,const android_dlextinfo*extra,const void*caller){
    void*result=route_ext(path,flags,extra,caller);int saved=errno;
    if(environment_camera_caller(caller))environment_event("DLOPEN_EXT",path,caller,reinterpret_cast<uintptr_t>(result));
    errno=saved;return result;
}
static void* observe_symbol(void* handle, const char* symbol, const void* caller) {
    void* result = original_symbol(handle, symbol, caller);
    int result_errno = errno;
    if(environment_camera_caller(caller))environment_event(result?"SYMBOL_RESOLVED":"SYMBOL_MISSING",symbol,caller,reinterpret_cast<uintptr_t>(result));
    audit_super_symbol(result, symbol, caller);
    audit_beauty_symbol(result, symbol, caller);
    void* observed = audit_night_symbol(result, symbol, caller);
    observed = audit_darksight_symbol(observed, handle, symbol, caller);
    if (!result && active.load() && symbol) {
        Dl_info information{};
        if (dladdr(caller, &information) && information.dli_fname
                && !strncmp(information.dli_fname, application_directory, strlen(application_directory))
                && missing_symbols.fetch_add(1) < 100) {
            LOG("CAMERA_SYMBOL_MISSING caller=%s symbol=%s handle=%p",
                information.dli_fname, symbol, handle);
        }
    }
    errno = result_errno;
    return observed;
}

extern "C" __attribute__((visibility("default"), used))
OnLoaded native_init(const NativeAPIEntries* entries) {
    hook_function = entries->hook_func;
    LOG("NATIVE_API_READY version=%u; inactive until exact APK gate", entries->version);
    return timelapse_on_loaded;
}

extern "C" JNIEXPORT void JNICALL
Java_com_arkaroy_occamfresh_NativePaths_observeNightQueuedBuffer(JNIEnv*env,jclass,jobject buffer,jint call,jlong timestamp) {
    NightErrnoRestore preserve_errno;
    if(!highres_copy_enabled||!night_scalar_enabled||!night_signal_enabled||!buffer||call<0||call>=2)return;
    const auto*pixels=static_cast<const uint8_t*>(env->GetDirectBufferAddress(buffer));
    if(!pixels||env->GetDirectBufferCapacity(buffer)!=31850496||!readable(pixels,31850496))return;
    nightsignal::Result result;
    if(!nightsignal::sample(pixels,31850496,4608,3456,9216,result))return;
    LOG("NIGHT_QUEUE_BUFFER call=%d timestamp=%lld pixels=%p before_original_queue",call,(long long)timestamp,pixels);
    for(unsigned i=0;i<4;++i){const auto&s=result.sites[i];
        LOG("NIGHT_QUEUE_SITE call=%d site=%u samples=256 sum=%llu min=%u max=%u hash=%llx read_only_no_retention",
            call,i,(unsigned long long)s.sum,s.low,s.high,(unsigned long long)s.hash);
    }
}
extern "C" JNIEXPORT void JNICALL
Java_com_arkaroy_occamfresh_NativePaths_observeNightApsBuffer(JNIEnv*env,jclass,jobject buffer,jint call,jlong frame,jlong timestamp) {
    NightErrnoRestore preserve_errno;
    if(!highres_copy_enabled||!night_scalar_enabled||!night_signal_enabled||!buffer||call<0||call>=20)return;
    const auto*pixels=static_cast<const uint8_t*>(env->GetDirectBufferAddress(buffer));
    if(!pixels||env->GetDirectBufferCapacity(buffer)!=31850496||!readable(pixels,31850496))return;
    nightsignal::Result result;
    if(!nightsignal::sample(pixels,31850496,4608,3456,9216,result))return;
    LOG("NIGHT_APS_BUFFER call=%d frame=%lld timestamp=%lld pixels=%p original_image_before_add",call,(long long)frame,(long long)timestamp,pixels);
    for(unsigned i=0;i<4;++i){const auto&s=result.sites[i];
        LOG("NIGHT_APS_SITE call=%d site=%u samples=256 sum=%llu min=%u max=%u hash=%llx read_only_no_retention",
            call,i,(unsigned long long)s.sum,s.low,s.high,(unsigned long long)s.hash);
    }
}
extern "C" JNIEXPORT void JNICALL
Java_com_arkaroy_occamfresh_NativePaths_setNightSignalAudit(JNIEnv*,jclass,jboolean enabled) {
    night_signal_enabled=enabled==JNI_TRUE;
    LOG("NIGHT_SIGNAL_SETUP enabled=%d central256samples_per_Bayer_site max2_process_and20_inputs no_pixel_changes_no_dump",night_signal_enabled);
}
extern "C" JNIEXPORT void JNICALL
Java_com_arkaroy_occamfresh_NativePaths_setNightScalarAudit(JNIEnv*,jclass,jboolean enabled) {
    night_scalar_enabled=enabled==JNI_TRUE;
    LOG("NIGHT_SCALAR_SETUP enabled=%d max_process=2 max_frames=20 no_pixels_no_OpenCL_no_front_hook",night_scalar_enabled);
}

extern "C" JNIEXPORT void JNICALL
Java_com_arkaroy_occamfresh_NativePaths_configure(JNIEnv* env, jclass, jstring directory, jstring data_directory, jboolean measured_hal, jboolean original_f14, jboolean trace_night, jboolean test_darksight, jboolean test_front_dsp, jboolean trace_beauty, jboolean retained_timelapse) {
    if (!hook_function || active.load()) return;
    const char* value = env->GetStringUTFChars(directory, nullptr);
    if (!value) return;
    // Restrict this prototype to the verified fresh privileged-app deployment.
    bool valid = !strcmp(value, "/system/priv-app/OppoCamera/lib/arm64");
    if (valid) snprintf(library_directory, sizeof(library_directory), "%s", value);
    env->ReleaseStringUTFChars(directory, value);
    if (!valid) { LOG("NATIVE_PATH_REFUSED unexpected directory"); return; }
    if (original_f14 == JNI_TRUE && test_darksight == JNI_TRUE) {
        char content[1025]{};
        FILE* enable = fopen("/system/priv-app/OppoCamera/fresh_oem_bootstrap/enable", "re");
        if (enable) {
            const size_t count = fread(content, 1, sizeof(content)-1, enable);
            const bool complete = count < sizeof(content)-1 && feof(enable) && !ferror(enable);
            fclose(enable);
            const bool requested = complete && strlen(content) == count
                    && strstr(content, "\nTEST_DARKSIGHT_UNAVAILABLE=1\n");
            if (requested) {
                errno = 0;
                darksight_unavailable_trial = access(darksight_missing_engine, F_OK) != 0
                        && errno == ENOENT;
                LOG("DARKSIGHT_AVAILABILITY_SETUP requested=1 armed=%d exact_engine_only; donor_files_unchanged",
                        darksight_unavailable_trial);
            }
        }
    }
    shared_snpe_group=original_f14==JNI_TRUE&&test_darksight==JNI_TRUE;
    highres_copy_enabled = original_f14 == JNI_TRUE;
    beauty_observer_enabled = original_f14 == JNI_TRUE && trace_beauty == JNI_TRUE;
    LOG("BEAUTY_OBSERVER_SETUP enabled=%d; original SDK calls/results retained", beauty_observer_enabled);
    const char* data = data_directory ? env->GetStringUTFChars(data_directory, nullptr) : nullptr;
    if (data && (!strcmp(data, "/data/user/0/com.oppo.camera") || !strcmp(data, "/data/data/com.oppo.camera"))) {
        timelapse_configure(data,original_f14==JNI_TRUE,retained_timelapse==JNI_TRUE);
        if(original_f14==JNI_TRUE&&test_front_dsp==JNI_TRUE)configure_front_dsp_access();
        char marker[PATH_MAX];
        snprintf(marker, sizeof(marker), "%s/files/ocfresh_superphoto_enable", data);
        bool tele_probe = access(marker, R_OK) == 0;
        snprintf(marker, sizeof(marker), "%s/files/ocfresh_darksight_raw_probe", data);
        if(trace_night==JNI_TRUE&&access(marker,R_OK)==0){
            snprintf(darksight_probe_directory,sizeof(darksight_probe_directory),"%s/files/ocfresh_darksight_raw",data);
            if(mkdir(darksight_probe_directory,0700)!=0&&access(darksight_probe_directory,W_OK)!=0)darksight_probe_directory[0]=0;
        }
        snprintf(marker,sizeof(marker),"%s/files/ocfresh_darksight_zero_pedestal_probe",data);
        darksight_zero_pedestal_probe=original_f14==JNI_TRUE&&trace_night==JNI_TRUE
                &&test_darksight==JNI_TRUE&&access(marker,R_OK)==0;
        if(darksight_zero_pedestal_probe)LOG("DARKSIGHT_ZERO_PEDESTAL_ARMED one_capture_only exactF14ISP/ROMwrapper; no model or input edits");
        snprintf(marker, sizeof(marker), "%s/files/ocfresh_highres_boundary_enable", data);
        capture_highres_enabled = access(marker, R_OK) == 0;
        if (tele_probe || capture_highres_enabled) {
            snprintf(capture_directory, sizeof(capture_directory), "%s/files/ocfresh_superphoto", data);
            capture_enabled = mkdir(capture_directory, 0700) == 0 || access(capture_directory, W_OK) == 0;
        }
    }
    if (data) env->ReleaseStringUTFChars(data_directory, data);
    copy_frames_enabled = measured_hal == JNI_TRUE;
    night_observer_enabled = trace_night == JNI_TRUE;
    LOG("NIGHT_OBSERVER_SETUP enabled=%d budget=256+32errors; original calls and returns retained", night_observer_enabled);
    LOG("SUPERPHOTO_PIXEL_AUDIT enabled=%d max_captures=1; original buffers read only", capture_enabled);
    LOG("HIGHRES_BOUNDARY_AUDIT enabled=%d; exact4640x3472 tuple, no pixel or argument overrides", capture_highres_enabled);
    LOG("SUPERPHOTO_COPY_MODE enabled=%d; original legacy API allocate flag, measured HAL and tele layout only", copy_frames_enabled);
    LOG("HIGHRES_COPY_MODE enabled=%d; originalF14 attested, exact4640x3472/pad48x32/legacy1/format10 only", highres_copy_enabled);
    void* open_entry = resolve_loader("__loader_dlopen");
    void* ext_entry = resolve_loader("__loader_android_dlopen_ext");
    int first = open_entry ? hook_function(open_entry, reinterpret_cast<void*>(redirect_dlopen),
                                           reinterpret_cast<void**>(&original_dlopen)) : -1;
    int second = ext_entry ? hook_function(ext_entry, reinterpret_cast<void*>(redirect_ext),
                                           reinterpret_cast<void**>(&original_ext)) : -1;
    void* symbol_entry = resolve_loader("__loader_dlsym");
    int third = symbol_entry ? hook_function(symbol_entry, reinterpret_cast<void*>(observe_symbol),
                                             reinterpret_cast<void**>(&original_symbol)) : -1;
    active.store(first == 0 || second == 0, std::memory_order_release);
    timelapse_try_install();
    if (original_ext && access(
            "/system/priv-app/OppoCamera/fresh_oem_bootstrap/isolate_anc_snpe", R_OK) == 0) {
        using CreateNamespace = android_namespace_t* (*)(const char*, const char*, const char*,
                                                         uint64_t, const char*,
                                                         android_namespace_t*, const void*);
        using LinkNamespaces = bool (*)(android_namespace_t*, android_namespace_t*, const char*);
        auto create = reinterpret_cast<CreateNamespace>(resolve_loader("__loader_android_create_namespace"));
        auto link = reinterpret_cast<LinkNamespaces>(resolve_loader("__loader_android_link_namespaces"));
        if (create && link) {
            // ISOLATED=1; do not inherit all libraries from the camera classloader.
            char permitted[PATH_MAX];snprintf(permitted,sizeof(permitted),"%s:%s",snpe_directory,darksight_directory);
            const char* search=shared_snpe_group?darksight_directory:snpe_directory;
            auto* ns = create("occam_f14_anc_snpe", search, search, 1,
                              shared_snpe_group?permitted:snpe_directory, nullptr, reinterpret_cast<void*>(create));
            if (ns && link(ns, nullptr, "libc.so:libm.so:libdl.so:liblog.so")) {
                snpe_namespace = ns;
                using GetNamespace = android_namespace_t* (*)(const char*);
                auto get_namespace = reinterpret_cast<GetNamespace>(resolve_loader("__loader_android_get_exported_namespace"));
                snpe_platform_namespace = get_namespace ? get_namespace("sphal") : nullptr;
            }
        }
        LOG("SNPE_NAMESPACE_SETUP isolated=%d original ODM runtime only", snpe_namespace != nullptr);
        LOG("ANC_SNPE_PLATFORM_OPENCL available=%d exact private SNPE caller only", snpe_platform_namespace != nullptr);
    }
    if (original_ext && test_darksight == JNI_TRUE) {
        using CreateNamespace = android_namespace_t* (*)(const char*, const char*, const char*,
                                                         uint64_t, const char*,
                                                         android_namespace_t*, const void*);
        using LinkNamespaces = bool (*)(android_namespace_t*, android_namespace_t*, const char*);
        auto create = reinterpret_cast<CreateNamespace>(resolve_loader("__loader_android_create_namespace"));
        auto link = reinterpret_cast<LinkNamespaces>(resolve_loader("__loader_android_link_namespaces"));
        if (create && link) {
            auto* ns = shared_snpe_group ? snpe_namespace : create("occam_darksight_rom", darksight_directory, darksight_directory, 1,
                              darksight_directory, nullptr, reinterpret_cast<void*>(create));
            LOG("SHARED_SNPE_GROUP requested=%d shared_namespace=%d environment_hooks=0",shared_snpe_group,ns&&ns==snpe_namespace);
            // Share the already working ROM OpenCL dispatcher/backend; keep
            // SNPE's original C++/Symphony group isolated from the APK.
            if (ns && link(ns, nullptr, "libc.so:libm.so:libdl.so:liblog.so:libOpenCL.so")) {
                using GetNamespace = android_namespace_t* (*)(const char*);
                auto get_namespace = reinterpret_cast<GetNamespace>(resolve_loader("__loader_android_get_exported_namespace"));
                auto* sphal = get_namespace ? get_namespace("sphal") : nullptr;
                // Frame-registration's FastRPC interface stays in the ROM's
                // own vendor namespace with its matching platform dependencies.
                bool fast_rpc = sphal && link(ns, sphal, "libcdsprpc.so");
                if (fast_rpc) { darksight_namespace = ns; darksight_platform_namespace = sphal; }
                auto* gpu = create("occam_darksight_f14_gpu", darksight_gpu_directory,
                                   darksight_gpu_directory, 1, darksight_gpu_directory,
                                   nullptr, reinterpret_cast<void*>(create));
                const char* vendor_imports = "libcutils.so:liblog.so:libutils.so:libvndksupport.so:libgralloc.qti.so:libgralloctypes.so:vendor.qti.hardware.display.mapper@4.0.so:android.hardware.graphics.mapper@4.0.so:vendor.qti.hardware.display.mapperextensions@1.0.so:vendor.qti.hardware.display.mapperextensions@1.1.so:libhidlbase.so:libc++.so:libc.so:libm.so:libdl.so:libz.so:libsync.so:libion.so";
                const char* system_imports = "libcutils.so:libvndksupport.so:liblog.so:libc++.so:libutils.so:libc.so:libm.so:libdl.so:libz.so:libsync.so:libion.so:libhidlbase.so";
                if (gpu && sphal && link(gpu, sphal, vendor_imports) && link(gpu, nullptr, system_imports))
                    darksight_gpu_namespace = gpu;
                LOG("DARKSIGHT_F14_GPU_NAMESPACE isolated=%d original4filegroup; system display driver unchanged", darksight_gpu_namespace != nullptr);
                LOG("DARKSIGHT_PLATFORM_LINK OpenCL=1 vendorFastRPC=%d", fast_rpc);
            }
        }
        LOG("DARKSIGHT_NAMESPACE_SETUP isolated=%d current-ROM original dependency group; output unqualified",
                darksight_namespace != nullptr);
    }
    LOG("NATIVE_PATH_SETUP active=%d dlopen=%p:%d ext=%p:%d dlsym=%p:%d allowlist=%zu caller namespace preserved",
        active.load(), open_entry, first, ext_entry, second, symbol_entry, third,
        sizeof(kOriginalLibraries) / sizeof(kOriginalLibraries[0]));
}
