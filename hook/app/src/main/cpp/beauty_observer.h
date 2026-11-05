#pragma once
#include <time.h>

// Original F14 AlgoInterface capture wrapper calls Slender3D_process with four
// pointer arguments and an int result. Its local op_param_data_t starts with
// country-region and face count (two int32s). Observe only this attested caller;
// do not substitute faces, parameters, image data, results, or errno.
static bool beauty_observer_enabled;
static std::atomic<bool> beauty_process_hooked{false};
static std::atomic<unsigned> beauty_process_reports{0};
using BeautyProcess = int (*)(void*, void*, void*, void*);
static BeautyProcess original_beauty_process;

static int observe_beauty_process(void* engine, void* image, void* output, void* parameters) {
    int entry_errno = errno;
    Dl_info caller{};
    bool scoped = beauty_observer_enabled
            && dladdr(__builtin_return_address(0), &caller) && caller.dli_fname
            && !strcmp(caller.dli_fname, "/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so");
    uintptr_t offset = scoped ? reinterpret_cast<uintptr_t>(__builtin_return_address(0))
            - reinterpret_cast<uintptr_t>(caller.dli_fbase) : 0;
    // APSBeautyWithoutDeblemishDepouchEyeEnhance::process, pinned F14 ELF RVAs.
    scoped = scoped && offset >= 0x23876c && offset < 0x239198;
    unsigned report = scoped ? beauty_process_reports.fetch_add(1) + 1 : 0;
    bool observed = report && report <= 8;
    int32_t prefix[2] = {-1, -1};
    bool valid = observed && readable(parameters, sizeof(prefix));
    if (valid) memcpy(prefix, parameters, sizeof(prefix));
    timespec start{}, finish{};
    if (observed) clock_gettime(CLOCK_MONOTONIC, &start);
    errno = entry_errno;
    int result = original_beauty_process(engine, image, output, parameters);
    int result_errno = errno;
    if (observed) {
        clock_gettime(CLOCK_MONOTONIC, &finish);
        int64_t micros = (finish.tv_sec - start.tv_sec) * 1000000LL
                + (finish.tv_nsec - start.tv_nsec) / 1000;
        LOG("BEAUTY_SDK_BOUNDARY call=%u callerRva=0x%lx readable=%d region=%d faces=%d result=%d durationUs=%lld; original call retained",
            report, static_cast<unsigned long>(offset), valid, prefix[0], prefix[1],
            result, static_cast<long long>(micros));
    }
    errno = result_errno;
    return result;
}

static void audit_beauty_symbol(void* result, const char* symbol, const void* caller) {
    if (!beauty_observer_enabled || !active.load() || !hook_function || !result
            || !symbol || strcmp(symbol, "Slender3D_process")) return;
    Dl_info source{}, target{};
    if (!dladdr(caller, &source) || !source.dli_fname || strcmp(source.dli_fname,
            "/system/priv-app/OppoCamera/lib/arm64/libAlgoInterface.so")) return;
    if (!dladdr(result, &target) || !target.dli_fname || strcmp(target.dli_fname,
            "/system/priv-app/OppoCamera/lib/arm64/libFaceBeautyCap.so")) return;
    if (beauty_process_hooked.exchange(true)) return;
    int status = hook_function(result, reinterpret_cast<void*>(observe_beauty_process),
                              reinterpret_cast<void**>(&original_beauty_process));
    LOG("BEAUTY_SDK_PROBE_READY status=%d; read-only, max8 capture calls", status);
}
