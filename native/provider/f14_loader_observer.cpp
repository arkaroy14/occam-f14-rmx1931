// Diagnostic only: preserve the loader's caller namespace, arguments, results,
// errno and dlerror state. No node callbacks or donor code are changed.
#include <android/log.h>
#include <android/dlext.h>
#include <dlfcn.h>
#include <link.h>
#include <elf.h>
#include <atomic>
#include <errno.h>
#include <stdint.h>
#include <string.h>

struct Query { const char* name; void* address; };
static int lookup(dl_phdr_info* info, size_t, void* argument) {
    if (!info->dlpi_name || (!strstr(info->dlpi_name, "/linker64") && !strstr(info->dlpi_name, "ld-android.so"))) return 0;
    const ElfW(Dyn)* dynamic = nullptr;
    for (size_t i = 0; i < info->dlpi_phnum; ++i)
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC)
            dynamic = reinterpret_cast<const ElfW(Dyn)*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    if (!dynamic) return 0;
    const ElfW(Sym)* symbols = nullptr; const char* strings = nullptr;
    const uint32_t* gnu = nullptr; size_t count = 0, string_size = 0;
    for (auto* entry = dynamic; entry->d_tag != DT_NULL; ++entry) {
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
    auto* query = static_cast<Query*>(argument);
    for (size_t i = 0; i < count; ++i) {
        if (symbols[i].st_shndx == SHN_UNDEF || symbols[i].st_name >= string_size) continue;
        if (!strcmp(strings + symbols[i].st_name, query->name)) {
            query->address = reinterpret_cast<void*>(info->dlpi_addr + symbols[i].st_value);
            return 1;
        }
    }
    return 0;
}
template <class T> static T resolve(const char* name) {
    Query query{name, nullptr}; dl_iterate_phdr(lookup, &query);
    return reinterpret_cast<T>(query.address);
}
static std::atomic<unsigned> reports{0};
static void report(const char* op, const char* name, void* result, const void* caller) {
    if (reports.fetch_add(1) >= 700) return;
    Dl_info from{}, to{}; dladdr(caller, &from);
    if (result && !strcmp(op, "dlsym")) dladdr(result, &to);
    __android_log_print(ANDROID_LOG_INFO, "OcF14Load", "%s name=%s result=%p from=%s+%lx target=%s",
        op, name ? name : "NULL", result, from.dli_fname ? from.dli_fname : "?",
        from.dli_fbase ? reinterpret_cast<uintptr_t>(caller)-reinterpret_cast<uintptr_t>(from.dli_fbase) : 0,
        to.dli_fname ? to.dli_fname : "?");
}
extern "C" void* dlopen(const char* name, int flags) {
    using Fn = void* (*)(const char*, int, const void*);
    static Fn fn = resolve<Fn>("__loader_dlopen");
    int entry_errno = errno; const void* caller = __builtin_return_address(0);
    if (!fn) return nullptr;
    errno = entry_errno; void* result = fn(name, flags, caller); int saved = errno;
    report("dlopen", name, result, caller); errno = saved; return result;
}
extern "C" void* android_dlopen_ext(const char* name, int flags, const android_dlextinfo* ext) {
    using Fn = void* (*)(const char*, int, const android_dlextinfo*, const void*);
    static Fn fn = resolve<Fn>("__loader_android_dlopen_ext");
    int entry_errno = errno; const void* caller = __builtin_return_address(0);
    if (!fn) return nullptr;
    errno = entry_errno; void* result = fn(name, flags, ext, caller); int saved = errno;
    report("android_dlopen_ext", name, result, caller); errno = saved; return result;
}
extern "C" void* dlsym(void* handle, const char* name) {
    using Fn = void* (*)(void*, const char*, const void*);
    static Fn fn = resolve<Fn>("__loader_dlsym");
    int entry_errno = errno; const void* caller = __builtin_return_address(0);
    if (!fn) return nullptr;
    errno = entry_errno; void* result = fn(handle, name, caller); int saved = errno;
    report("dlsym", name, result, caller); errno = saved; return result;
}
extern "C" char* dlerror() {
    using Fn = char* (*)();
    static Fn fn = resolve<Fn>("__loader_dlerror");
    int entry_errno = errno;
    if (!fn) return nullptr;
    errno = entry_errno; char* result = fn(); int saved = errno;
    if (result && reports.fetch_add(1) < 700)
        __android_log_print(ANDROID_LOG_INFO, "OcF14Load", "dlerror original=%s", result);
    errno = saved; return result;
}
