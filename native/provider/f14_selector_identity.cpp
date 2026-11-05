// F14-only sensor-selection compatibility. Original HAL/CHI bytes unchanged.
// Controller hashes both HALs before mounting. Do not use with OP4A89 CHI.
#include <android/log.h>
#include <dlfcn.h>
#include <sys/system_properties.h>
#include <atomic>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

using PropertyGet = int (*)(const char*, char*, const char*);
static PropertyGet original_get;
static std::atomic<unsigned> enabled_reports{0}, skipped_reports{0};
#include "f14_highres_context.h"
#ifdef OCCAM_F14_ISP_LOG_SCOPE
#include "f14_isp_log_scope.h"
#endif
__attribute__((constructor)) static void initialize() {
    original_get = reinterpret_cast<PropertyGet>(dlsym(RTLD_NEXT, "property_get"));
    __android_log_print(ANDROID_LOG_INFO, "OcF14Sensor", "F14 HFR/EIS and exact highres-session adapter loaded delegate=%p", original_get);
}
static int delegate(const char* key, char* value, const char* fallback) {
    if (original_get) return original_get(key, value, fallback);
    int length = __system_property_get(key, value);
    if (!length && fallback) {
        size_t n = strnlen(fallback, PROP_VALUE_MAX-1);
        memcpy(value, fallback, n); value[n] = 0; return static_cast<int>(n);
    }
    return length;
}
extern "C" __attribute__((visibility("hidden"))) int oc_f14_property_get(const char* key, char* value, const char* fallback, const F14PropertyEntry* entry) {
    const int entry_errno = errno;
    const void* audit_caller=reinterpret_cast<const void*>(entry->x19_x30[11]);
    ud_oem_prepare(entry);
    tuning_observe(entry);
    errno=entry_errno;
    if (key && value && !strcmp(key, "oppo.camera.packname")) {
        Dl_info owner{}; const auto caller = reinterpret_cast<uintptr_t>(audit_caller);
        if (dladdr(reinterpret_cast<void*>(caller), &owner) && owner.dli_fname && owner.dli_fbase
                && !strcmp(owner.dli_fname, "/vendor/lib64/camera/components/com.oppo.node.preview.so")) {
            const auto offset=caller-reinterpret_cast<uintptr_t>(owner.dli_fbase);
            // Original SHA39fd... Create's final OEM identity query. Earlier
            // queries in the same function belong to unrelated app handling.
            if(offset==0x11fdc){
                static std::atomic<unsigned> reports{0};
                if(reports.fetch_add(1)<16)__android_log_print(ANDROID_LOG_INFO,"OcF14Preview",
                    "PREVIEW_OEM_QUERY caller=%lx scoped=%d",static_cast<unsigned long>(offset),f14_preview_context);
                if(f14_preview_context){
                    static constexpr char identity[]="com.oppo.camera";
                    memcpy(value,identity,sizeof(identity));errno=entry_errno;return env_property_result(key,value,sizeof(identity)-1,audit_caller,true);
                }
            }
        }
        if (dladdr(reinterpret_cast<void*>(caller), &owner) && owner.dli_fname && owner.dli_fbase
                && !strcmp(owner.dli_fname, "/vendor/lib64/hw/com.qti.chi.override.so")) {
            const auto offset = caller - reinterpret_cast<uintptr_t>(owner.dli_fbase);
            if(ud_oem_query(entry,offset)){
                static constexpr char identity[]="com.oppo.camera";
                memcpy(value,identity,sizeof(identity));
                __android_log_print(ANDROID_LOG_INFO,"OcUltraDarkOEM",
                    "APPLY query=e9984 caller=ea970 offline=c9fa4 usecase=13 sensor=2 feature=20 original_selector_preserved=1");
                errno=entry_errno;
                return env_property_result(key,value,sizeof(identity)-1,audit_caller,true);
            }

            if ((f14_highres_context || f14_front_context) && ((offset>=0xde660 && offset<0xde7a8)
                    || (offset>=0x3d018 && offset<0x3d128)
                    || (offset>=0xede90 && offset<0xedfa0))) {
                static constexpr char identity[] = "com.oppo.camera";
                memcpy(value,identity,sizeof(identity));errno=entry_errno;
                return env_property_result(key,value,sizeof(identity)-1,audit_caller,true);
            }
            // Exact F14 mini-debug symbol boundaries (272 bytes). Canonical
            // string is com.oppo.camera, not OP4A89's com.oplus.camera.
            if (offset >= 0xede90 && offset < 0xedfa0) {
                char mode[PROP_VALUE_MAX], camera[PROP_VALUE_MAX];
                delegate("vendor.oppo.camera.capmode", mode, "0");
                delegate("vendor.oppo.camera.id", camera, "-1");
                const auto operation = strtoul(mode, nullptr, 0);
                uint32_t group = 0;
                // Verified F14 FindBestSensorMode ADRP+ADD+LDR and live readback.
                memcpy(&group, static_cast<const char*>(owner.dli_fbase) + 0x21a2e4, sizeof(group));
                // Original F14 group9781 switch selects sensor index3 for
                // both 0x8021 and UltraSteady 0x8028. Keep the original EIS
                // session/pipeline; expose identity only to this selector.
                bool enabled = !strcmp(camera, "0") && group == 0x9781
                        && (operation == 0x800d || operation == 0x8021
                            || operation == 0x8028);
                bool report = enabled ? enabled_reports.fetch_add(1) < 64 : skipped_reports.fetch_add(1) < 8;
                if (report) __android_log_print(ANDROID_LOG_INFO, "OcF14Sensor",
                    "SELECTOR_PROPERTY caller=%lx operation=%lx camera=%s group=%x canonical=%d",
                    static_cast<unsigned long>(offset), operation, camera, group, enabled);
                if (enabled) {
                    static constexpr char identity[] = "com.oppo.camera";
                    memcpy(value, identity, sizeof(identity)); errno = entry_errno;
                    return env_property_result(key,value,sizeof(identity)-1,audit_caller,true);
                }
            }
        }
    }
    errno = entry_errno;
    int result=delegate(key, value, fallback);
#ifdef OCCAM_F14_ISP_LOG_SCOPE
    int saved=errno;
    bool log_override=isp_log_override(key,value,audit_caller);
    if(log_override)result=4;
    errno=saved;
    return env_property_result(key,value,result,audit_caller,log_override);
#else
    return env_property_result(key,value,result,audit_caller,false);
#endif
}
extern "C" __attribute__((naked)) int property_get(const char*, char*, const char*) {
    __asm__ volatile(OCFRESH_PROPERTY_VENEER);
}
