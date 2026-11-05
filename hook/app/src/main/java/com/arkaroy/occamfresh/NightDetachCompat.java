package com.arkaroy.occamfresh;

import android.media.Image;
import android.media.ImageReader;
import android.util.Log;
import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicInteger;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

/** Original F14 oplusDetachImage is only an alias for detachImage.
 * Restore that operation at its original call site, in rear Night RAW only. */
final class NightDetachCompat {
    private static final AtomicInteger reports = new AtomicInteger();
    static void install(ClassLoader loader) throws Exception {
        try {
            ImageReader.class.getDeclaredMethod("oplusDetachImage", Image.class);
            Log.i("OcFresh", "NIGHT_DETACH_COMPAT_REFUSED OEM_method_already_present");
            return;
        } catch (NoSuchMethodException expected) { /* The missing OEM alias. */ }
        Method detach = ImageReader.class.getDeclaredMethod("detachImage", Image.class);
        detach.setAccessible(true);
        Class<?> properties = Class.forName("android.os.SystemProperties");
        Class<?> utils = Class.forName("com.oppo.camera.aps.adapter.ApsUtils", false, loader);
        XposedHelpers.findAndHookMethod(utils, "detachImage", ImageReader.class, Image.class,
                new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (p.args.length != 2 || !(p.args[0] instanceof ImageReader)
                        || !(p.args[1] instanceof Image)
                        || !"night".equals(ObserveHook.currentMode())
                        || !"7".equals(ObserveHook.currentCameraId())) return;
                try {
                    Image image = (Image) p.args[1];
                    if (image.getFormat() != 32 || image.getWidth() != 4608
                            || image.getHeight() != 3456) return;
                    if (Boolean.TRUE.equals(XposedHelpers.callStaticMethod(properties,
                            "getBoolean", "persist.sys.cam.skip_detach_image", false))) {
                        if (reports.getAndIncrement() < 24)
                            Log.i("OcFresh", "NIGHT_DETACH_COMPAT_REFUSED ROM_skip_detach_property");
                        return;
                    }
                    long timestamp = image.getTimestamp();
                    // Framework performs original ownership/validity checks,
                    // native detach, plane cleanup and detached-state update.
                    detach.invoke(p.args[0], image);
                    p.setResult(null); // The original OEM alias returns void.
                    if (reports.getAndIncrement() < 24)
                        Log.i("OcFresh", "NIGHT_DETACH_COMPAT_APPLIED timestamp=" + timestamp
                                + " original_framework_detach no_copy_no_refcount_override");
                } catch (Throwable error) {
                    // No second detach attempt. Original APK's missing-alias
                    // branch remains responsible for logging/fallback.
                    if (reports.getAndIncrement() < 24)
                        Log.i("OcFresh", "NIGHT_DETACH_COMPAT_UNAVAILABLE " + error);
                }
            }
        });
        Log.i("OcFresh", "NIGHT_DETACH_COMPAT_READY scoped_rear7_RAW4608_only original_framework_method");
    }
}
