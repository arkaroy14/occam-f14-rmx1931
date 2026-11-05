// Temporary provider-side sensor-selection probe. Original HAL files unchanged.
// The wrapper/controller pins the exact audited HAL SHA256 before loading this.
// Only ChxSensorModeSelect::isOPPOCamera receives the canonical identity, only
// for rear main opmode0x8021 and the measured0x9781 slow-motion0x800d branch.
// General isOPPOCamera / stream initialization sees
// the real property unchanged. No result/FPS/buffer values are fabricated.
#include <android/log.h>
#include <dlfcn.h>
#include <sys/system_properties.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <stddef.h>

#ifndef OCCAM_SENSOR_LOG_TAG
#define OCCAM_SENSOR_LOG_TAG "OcSensorProbe"
#endif

using PropertyGet = int (*)(const char*, char*, const char*);
static PropertyGet real_get;
static std::atomic<unsigned> reports{0};
#include "chi_raw_context.h"

static int delegate_get(const char* key, char* value, const char* fallback) {
    if (real_get) return real_get(key, value, fallback);
    int length = __system_property_get(key, value);
    if (!length && fallback) {
        size_t size = strnlen(fallback, PROP_VALUE_MAX - 1);
        memcpy(value, fallback, size); value[size] = 0;
        return static_cast<int>(size);
    }
    return length;
}

__attribute__((constructor)) static void initialize_probe() {
    night_raw_test_enabled = access("/vendor/bin/hw/occam_runtime/enable_night_raw_identity", R_OK) == 0;
    real_get = reinterpret_cast<PropertyGet>(dlsym(RTLD_NEXT, "property_get"));
    __android_log_print(ANDROID_LOG_INFO, OCCAM_SENSOR_LOG_TAG, "LOADED delegate=%p pid-local sensor selector night_raw_test=%d loader=%p", real_get, night_raw_test_enabled, raw_loader_dlsym.load());
}

extern "C" __attribute__((visibility("default")))
int property_get(const char* key, char* value, const char* fallback) {
    if (key && !strcmp(key, "oplus.camera.packname") && getenv("OCCAM_SENSOR_ONLY")) {
        Dl_info owner{};
        uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
        if (dladdr(reinterpret_cast<void*>(caller), &owner) && owner.dli_fname && owner.dli_fbase
                && strstr(owner.dli_fname, "/hw/com.qti.chi.override.so")) {
            if (in_night_raw_configuration) {
                int saved_errno = errno;
                static const char identity[] = "com.oplus.camera";
                memcpy(value, identity, sizeof(identity));
                if (night_identity_reports.fetch_add(1) < 32)
                    __android_log_print(ANDROID_LOG_INFO, OCCAM_SENSOR_LOG_TAG,
                            "NIGHT_RAW_CONTEXT_PROPERTY caller_offset=%lx original OEM accepted-session path",
                            static_cast<unsigned long>(caller - reinterpret_cast<uintptr_t>(owner.dli_fbase)));
                errno = saved_errno;
                return sizeof(identity) - 1;
            }
            uintptr_t offset = caller - reinterpret_cast<uintptr_t>(owner.dli_fbase);
            // Audited LOCAL helper boundaries from this exact blob's mini-debug
            // symbols. Hash guard lives in the root controller; never reuse on a
            // different HAL or redirect the broader global isOPPOCamera helper.
            if (offset >= 0xeab00 && offset < 0xeac18) {
                char mode[PROP_VALUE_MAX], camera[PROP_VALUE_MAX];
                delegate_get("vendor.oppo.camera.capmode", mode, "0");
                delegate_get("vendor.oppo.camera.id", camera, "-1");
                unsigned long operation = strtoul(mode, nullptr, 0);
                // Exact current-CHI local variable audited from original code;
                // root readback confirms0x9781 on this device. This pins the HFR
                // branch whose real sensor table has index7=240,index8=480.
                uint32_t selector_group = 0;
                memcpy(&selector_group, static_cast<const char*>(owner.dli_fbase) + 0x213104, 4);
                bool hfr = operation == 0x800d && selector_group == 0x9781;
                bool enabled = (operation == 0x8021 || hfr) && !strcmp(camera, "0");
                if (reports.fetch_add(1) < 24) __android_log_print(ANDROID_LOG_INFO,
                    OCCAM_SENSOR_LOG_TAG, "SELECTOR_PROPERTY caller=%s+%lx mode=%s camera=%s group=%x canonical=%d",
                    owner.dli_fname, static_cast<unsigned long>(offset), mode, camera, selector_group, enabled);
                if (enabled) {
                    static const char identity[] = "com.oplus.camera";
                    memcpy(value, identity, sizeof(identity));
                    return sizeof(identity) - 1;
                }
            }
        }
    }
    return delegate_get(key, value, fallback);
}
