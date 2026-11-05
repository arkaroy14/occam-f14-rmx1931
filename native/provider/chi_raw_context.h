// Test-only CHI callback context, pinned by the provider controller's HAL hashes.
// Callback slot/signature verified from this blob's chi_hal_override_entry and
// chi_initialize_override_session, not inferred from another CHI release.
#pragma once
#include <elf.h>
#include <link.h>
#include <errno.h>
#include <unistd.h>

static bool night_raw_test_enabled;
static thread_local bool in_night_raw_configuration;
static std::atomic<const void*> night_raw_device{nullptr};
static std::atomic<unsigned> night_identity_reports{0}, night_request_reports{0};
struct RawStreamPrefix { int32_t direction; uint32_t width, height; int32_t format; };
struct RawConfigPrefix { uint32_t count; const RawStreamPrefix* const* streams; uint32_t mode; };
static_assert(offsetof(RawConfigPrefix, streams) == 8 && offsetof(RawConfigPrefix, mode) == 16, "CHI config ABI");
static bool matches_night_raw(uint32_t id, const RawConfigPrefix* config) {
    if (id != 0 || !config || config->count != 4 || (config->mode & 0xffff) != 0x8019 || !config->streams) return false;
    unsigned input = 0, raw = 0, preview = 0, yuv = 0;
    for (unsigned i = 0; i < config->count; ++i) {
        const RawStreamPrefix* s = config->streams[i];
        if (!s) return false;
        if (s->direction == 1 && s->format == 32 && s->width == 4608 && s->height == 3456) ++input;
        else if (s->direction == 0 && s->format == 32 && s->width == 4608 && s->height == 3456) ++raw;
        else if (s->direction == 0 && s->format == 35 && s->width == 4608 && s->height == 3456) ++yuv;
        else if (s->direction == 0 && s->format == 34 && s->width && s->height && s->width <= 4608 && s->height <= 3456) ++preview;
        else return false;
    }
    return input == 1 && raw == 1 && preview == 1 && yuv == 1;
}
using ChiInitialize = uint64_t (*)(uint32_t, const void*, const void*, RawConfigPrefix*, bool*, void**);
using ChiProcess = uint64_t (*)(const void*, void*, void*);
using ChiEntry = void (*)(void*);
static ChiInitialize original_chi_initialize;
static ChiProcess original_chi_process;
static ChiEntry original_chi_entry;
static uint64_t scoped_chi_initialize(uint32_t id, const void* device, const void* ops,
                                      RawConfigPrefix* config, bool* enabled, void** private_data) {
    int entry_errno = errno;
    bool previous = in_night_raw_configuration;
    bool scope = night_raw_test_enabled && matches_night_raw(id, config);
    if (night_raw_device.load() == device) night_raw_device.store(nullptr);
    in_night_raw_configuration = scope;
    if (scope) __android_log_print(ANDROID_LOG_INFO, OCCAM_SENSOR_LOG_TAG,
            "NIGHT_RAW_CONTEXT_BEGIN camera=0 mode=8019; actual RAW input/output tuple, original factory delegated");
    errno = entry_errno;
    uint64_t result = original_chi_initialize(id, device, ops, config, enabled, private_data);
    int saved_errno = errno;
    if (scope && result == 0 && enabled && *enabled) night_raw_device.store(device);
    if (scope) __android_log_print(ANDROID_LOG_INFO, OCCAM_SENSOR_LOG_TAG,
            "NIGHT_RAW_CONTEXT_END result=%llu override=%d; genuine original result",
            static_cast<unsigned long long>(result), enabled ? *enabled : -1);
    in_night_raw_configuration = previous;
    errno = saved_errno;
    return result;
}
static uint64_t scoped_chi_process(const void* device, void* request, void* private_data) {
    int entry_errno = errno;
    bool previous = in_night_raw_configuration;
    bool scope = night_raw_test_enabled && device && night_raw_device.load() == device;
    in_night_raw_configuration = scope;
    bool report = false;
    if (scope && request) {
        // Stable AArch64 camera3_capture_request prefix: frame#0, settings@8,
        // input_buffer@16. Observe presence, never inspect/change its pixels.
        const void* input = nullptr;
        memcpy(&input, static_cast<const char*>(request) + 16, sizeof(input));
        report = input && night_request_reports.fetch_add(1) < 10;
        if (report) __android_log_print(ANDROID_LOG_INFO, OCCAM_SENSOR_LOG_TAG,
                "NIGHT_RAW_REQUEST_BEGIN frame=%u actual_input=1 original request delegated",
                *static_cast<const uint32_t*>(request));
    }
    errno = entry_errno;
    uint64_t result = original_chi_process(device, request, private_data);
    int saved_errno = errno;
    if (report) __android_log_print(ANDROID_LOG_INFO, OCCAM_SENSOR_LOG_TAG,
            "NIGHT_RAW_REQUEST_END result=%llu original return retained", static_cast<unsigned long long>(result));
    in_night_raw_configuration = previous;
    errno = saved_errno;
    return result;
}
static void scoped_chi_entry(void* callbacks) {
    original_chi_entry(callbacks);
    int saved_errno = errno;
    if (callbacks) {
        auto** entries = static_cast<void**>(callbacks);
        Dl_info owner{}, process_owner{};
        if (dladdr(entries[3], &owner) && owner.dli_fname && owner.dli_fbase
                && !strcmp(owner.dli_fname, "/vendor/lib64/hw/com.qti.chi.override.so")
                && reinterpret_cast<uintptr_t>(entries[3]) - reinterpret_cast<uintptr_t>(owner.dli_fbase) == 0x55450
                && dladdr(entries[6], &process_owner) && process_owner.dli_fbase == owner.dli_fbase
                && reinterpret_cast<uintptr_t>(entries[6]) - reinterpret_cast<uintptr_t>(owner.dli_fbase) == 0x57d90) {
            original_chi_initialize = reinterpret_cast<ChiInitialize>(entries[3]);
            original_chi_process = reinterpret_cast<ChiProcess>(entries[6]);
            entries[3] = reinterpret_cast<void*>(scoped_chi_initialize);
            entries[6] = reinterpret_cast<void*>(scoped_chi_process);
            __android_log_print(ANDROID_LOG_INFO, OCCAM_SENSOR_LOG_TAG,
                    "NIGHT_RAW_CALLBACK_BOUND slots=3,6 exact CHI callbacks; accepted Night session context only");
        }
    }
    errno = saved_errno;
}

// Resolve the exported loader directly, without recursively calling dlsym.
// The same ELF walk is already used by the tested app-native loader adapter.
struct RawLoaderQuery { void* result; };
static int raw_loader_symbol(dl_phdr_info* info, size_t, void* argument) {
    if (!info->dlpi_name || (!strstr(info->dlpi_name, "/linker64") && !strstr(info->dlpi_name, "ld-android.so"))) return 0;
    const ElfW(Dyn)* dynamic = nullptr;
    for (size_t i = 0; i < info->dlpi_phnum; ++i)
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC)
            dynamic = reinterpret_cast<const ElfW(Dyn)*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    if (!dynamic) return 0;
    const ElfW(Sym)* symbols = nullptr; const char* strings = nullptr;
    const uint32_t* gnu = nullptr; size_t count = 0, string_size = 0;
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
        const uint32_t* buckets = reinterpret_cast<const uint32_t*>(reinterpret_cast<const ElfW(Addr)*>(gnu + 4) + gnu[2]);
        const uint32_t* chains = buckets + gnu[0]; uint32_t largest = 0;
        for (uint32_t i = 0; i < gnu[0]; ++i) if (buckets[i] > largest) largest = buckets[i];
        if (largest >= gnu[1] && largest < 65536) {
            while (largest < 65536 && !(chains[largest - gnu[1]] & 1)) ++largest;
            count = largest + 1;
        }
    }
    if (count > 65536) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (symbols[i].st_shndx == SHN_UNDEF || symbols[i].st_name >= string_size) continue;
        if (!strcmp(strings + symbols[i].st_name, "__loader_dlsym")) {
            static_cast<RawLoaderQuery*>(argument)->result = reinterpret_cast<void*>(info->dlpi_addr + symbols[i].st_value);
            return 1;
        }
    }
    return 0;
}
using RawLoaderDlsym = void* (*)(void*, const char*, const void*);
static std::atomic<RawLoaderDlsym> raw_loader_dlsym{nullptr};
extern "C" __attribute__((visibility("default")))
void* dlsym(void* handle, const char* symbol) {
    RawLoaderDlsym loader = raw_loader_dlsym.load();
    if (!loader) {
        RawLoaderQuery query{};
        dl_iterate_phdr(raw_loader_symbol, &query);
        loader = reinterpret_cast<RawLoaderDlsym>(query.result);
        if (!loader) return nullptr;
        raw_loader_dlsym.store(loader);
    }
    const void* caller = __builtin_return_address(0);
    void* result = loader(handle, symbol, caller);
    int saved_errno = errno;
    if (night_raw_test_enabled && result && !strcmp(symbol, "chi_hal_override_entry")) {
        Dl_info source{}, target{};
        bool source_known = dladdr(caller, &source) && source.dli_fname;
        bool target_known = dladdr(result, &target) && target.dli_fname;
        __android_log_print(ANDROID_LOG_INFO, OCCAM_SENSOR_LOG_TAG,
                "NIGHT_RAW_CHI_ENTRY_LOOKUP source=%s target=%s",
                source_known ? source.dli_fname : "unknown", target_known ? target.dli_fname : "unknown");
        if (source_known
                && !strcmp(source.dli_fname, "/vendor/lib64/hw/camera.qcom.so")
                && target_known
                && !strcmp(target.dli_fname, "/vendor/lib64/hw/com.qti.chi.override.so")) {
            original_chi_entry = reinterpret_cast<ChiEntry>(result);
            result = reinterpret_cast<void*>(scoped_chi_entry);
        }
    }
    errno = saved_errno;
    return result;
}
