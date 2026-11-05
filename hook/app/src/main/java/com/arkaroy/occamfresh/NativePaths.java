package com.arkaroy.occamfresh;

import android.util.Log;

/** Native path compatibility, called only after OemBootstrap's exact APK gate. */
final class NativePaths {
    static void install(String nativeDirectory, String dataDirectory, boolean measuredHal, boolean originalF14, boolean traceNight, boolean testDarksight, boolean testFrontDsp, boolean traceBeauty, boolean retainedTimelapse) {
        try {
            System.loadLibrary("ocfresh");
            configure(nativeDirectory, dataDirectory, measuredHal, originalF14, traceNight, testDarksight, testFrontDsp, traceBeauty, retainedTimelapse);
        } catch (Throwable failure) {
            Log.e("OcFresh", "NATIVE_PATH_SETUP_FAILED", failure);
        }
    }
    static native void setEnvironmentAudit(boolean enabled);
    static native void setNightScalarAudit(boolean enabled);
    static native void setNightSignalAudit(boolean enabled);
    static native void observeNightApsBuffer(java.nio.ByteBuffer pixels, int call, long frame, long timestamp);
    static native void observeNightQueuedBuffer(java.nio.ByteBuffer pixels, int call, long timestamp);
    private static native void configure(String nativeDirectory, String dataDirectory, boolean measuredHal, boolean originalF14, boolean traceNight, boolean testDarksight, boolean testFrontDsp, boolean traceBeauty, boolean retainedTimelapse);
}
