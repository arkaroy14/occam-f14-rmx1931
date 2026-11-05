// Retains init's UID, groups, capabilities, task profile and SELinux domain.
// Executes an identical copy of the original provider with our temporary shim.
#include <android/log.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#ifndef OCCAM_PROVIDER_PAYLOAD_DIR
#define OCCAM_PROVIDER_PAYLOAD_DIR "/vendor/bin/hw/occam_probe"
#endif

int main(int, char** arguments) {
    const char* shim = OCCAM_PROVIDER_PAYLOAD_DIR "/liboc_sensor_compat.so";
    const char* original = OCCAM_PROVIDER_PAYLOAD_DIR "/provider.real";
    if (setenv("OCCAM_SENSOR_ONLY", "1", 1) || setenv("LD_PRELOAD", shim, 1)) return 126;
    __android_log_print(ANDROID_LOG_INFO, "OcSensorProbe", "WRAPPER exec identical original provider");
    execv(original, arguments);
    __android_log_print(ANDROID_LOG_ERROR, "OcSensorProbe", "WRAPPER exec failed errno=%d", errno);
    return 127;
}
