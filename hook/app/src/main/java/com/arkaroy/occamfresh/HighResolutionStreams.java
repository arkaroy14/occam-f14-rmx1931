package com.arkaroy.occamfresh;

import android.graphics.ImageFormat;
import android.hardware.HardwareBuffer;
import android.media.ImageReader;
import android.util.Log;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

/** Retain the original F14 highDefinition stream dimensions on newer Android. */
final class HighResolutionStreams {
    static void install() {
        // Caller is raw-APK gated and the measured HAL pair is root-attested.
        // Actual vendor QCFA dimensions and sensor-mode table contain9280x6944;
        // neither9280x6944 nor4640x3472 is in the public stream map. AOSP rounds
        // flexible consumers to <=1920 when an exact size is absent. Its existing
        // non-flexible-consumer rule recognizes GRALLOC_USAGE_RENDERSCRIPT.
        // This preserves YUV420 and CPU read usage, and asks HAL for the actual
        // original dimensions. It does not invent a stream-map entry or resize.
        final long GRALLOC_USAGE_RENDERSCRIPT = 0x00100000L;
        XposedHelpers.findAndHookMethod(ImageReader.class, "newInstance",
                int.class, int.class, int.class, int.class, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (!"highDefinition".equals(ObserveHook.currentMode())) return;
                int width = (Integer) p.args[0], height = (Integer) p.args[1];
                if ((Integer) p.args[2] != ImageFormat.YUV_420_888
                        || !((width == 9280 && height == 6944) || (width == 4640 && height == 3472))) return;
                long usage = HardwareBuffer.USAGE_CPU_READ_OFTEN | GRALLOC_USAGE_RENDERSCRIPT;
                ImageReader reader = ImageReader.newInstance(width, height, ImageFormat.YUV_420_888,
                                                             (Integer) p.args[3], usage);
                Log.i("OcFresh", "HIGHRES_ORIGINAL_SIZE reader=" + reader + " requested="
                        + width + "x" + height + " usage=" + Long.toHexString(usage)
                        + "; preserve original stream dimensions, no scaling");
                p.setResult(reader);
            }
        });
    }
}
