// Temporary F14 HAL probe: reuse ROM compatibility libraries without editing
// the original HAL's DT_NEEDED or executable code. Never used for boot startup.
#include <android/log.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main(int, char** argv) {
    const char* original = "/vendor/bin/hw/occam_f14_stack/provider.real";
#ifdef OCCAM_F14_PRODUCTION
    if (setenv("LD_PRELOAD", "/vendor/bin/hw/occam_f14_stack/liboc_f14_selector.so:/vendor/lib64/libcamera_metadata_shim.so", 1)) return 126;
#elif defined(OCCAM_F14_HFR_TEST)
    if (setenv("LD_PRELOAD", "/vendor/bin/hw/occam_f14_stack/liboc_f14_selector.so:/vendor/bin/hw/occam_f14_stack/liboc_f14_loader_observer.so:/vendor/lib64/libcamera_metadata_shim.so", 1)) return 126;
#else
    if (setenv("LD_PRELOAD", "/vendor/bin/hw/occam_f14_stack/liboc_f14_loader_observer.so:/vendor/lib64/libcamera_metadata_shim.so", 1)) return 126;
#endif
    unsetenv("OCCAM_SENSOR_ONLY");
    __android_log_print(ANDROID_LOG_INFO, "OcF14Stack", "Temporary original-F14 stack test; original provider exec");
    execv(original, argv);
    __android_log_print(ANDROID_LOG_ERROR, "OcF14Stack", "Original provider exec failed errno=%d", errno);
    return 127;
}
