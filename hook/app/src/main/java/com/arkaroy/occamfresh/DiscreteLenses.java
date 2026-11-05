package com.arkaroy.occamfresh;

import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.util.Log;
import android.util.Size;
import java.util.Map;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

/** Restore stock non-SAT photo switching on the attested incompatible HAL only. */
final class DiscreteLenses {
    private static volatile boolean verifiedUltraWide;
    private static volatile boolean verifiedTelephoto;
    private static Class<?> cameraUtility;
    private static boolean restoredSat() {
        try {
            if (cameraUtility == null) return false;
            CameraManager manager = (CameraManager) XposedHelpers.getStaticObjectField(cameraUtility, "a");
            Map<?, ?> types = (Map<?, ?>) XposedHelpers.getStaticObjectField(cameraUtility, "f");
            if (manager == null || types == null || manager.getCameraIdList().length != 10
                    || !"7".equals(String.valueOf(types.get(12)))) return false;
            if (!new Size(1616, 1216).equals(manager.getCameraCharacteristics("4").get(
                    CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE))
                    || !new Size(4608, 3456).equals(manager.getCameraCharacteristics("7").get(
                    CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE))) return false;
            // Original OEM support check, left intact by the SAT workaround
            // when type12 anchors main rather than the previously mis-mappedUW.
            return Boolean.TRUE.equals(XposedHelpers.callStaticMethod(cameraUtility, "f"));
        } catch (Throwable unavailable) { return false; }
    }
    private static final java.util.concurrent.atomic.AtomicInteger geometryReports =
            new java.util.concurrent.atomic.AtomicInteger();
    private static boolean activeTelephoto(Object manager) {
        try {
            Object mode = XposedHelpers.getObjectField(manager, "cj");
            Object wrapper = XposedHelpers.getObjectField(manager, "bS");
            if (restoredSat() || !verifiedTelephoto || mode == null || wrapper == null
                    || !"common".equals(XposedHelpers.callMethod(mode, "l"))) return false;
            // The proxy's c() reports a logical role during switching, so verify
            // the actual active characteristics used by the crop calculation.
            CameraCharacteristics camera = (CameraCharacteristics) XposedHelpers.getObjectField(wrapper, "a");
            if (camera == null || !new Size(4224, 3136).equals(camera.get(
                    CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE))) return false;
            float[] focal = camera.get(CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
            return focal != null && focal.length > 0 && Math.abs(focal[0] - 6.25f) < 0.01f;
        } catch (Throwable unavailable) { return false; }
    }
    static void install(ClassLoader loader, Class<?> utility) throws Exception {
        cameraUtility = utility;
        // Original BaseMode.N -> Util.b(mode,front) checks two stock project
        // declarations absent from this SAT-oriented donor. CommonCapMode uses
        // that result for pref_none_sat_ultra_wide_angle_key. ZoomManager then
        // displays 0.6 and writes the preference; the original listener reopens
        // the physical camera through entry.b. Keep all that original behavior.
        // This installer is called only AFTER CameraCapabilities' exact HAL gate.
        XposedHelpers.findAndHookMethod("com.oppo.camera.util.Util", loader, "b",
                String.class, boolean.class, new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) throws Throwable {
                if (restoredSat()) return;
                if (p.hasThrowable() || Boolean.TRUE.equals(p.getResult())
                        || !"common".equals(p.args[0]) || Boolean.TRUE.equals(p.args[1])) return;
                if (!verifiedUltraWide) {
                    Map<?, ?> types = (Map<?, ?>) XposedHelpers.getStaticObjectField(utility, "f");
                    CameraManager manager = (CameraManager) XposedHelpers.getStaticObjectField(utility, "a");
                    if (types == null || manager == null || !types.containsKey(2)) return;
                    String id = String.valueOf(types.get(2));
                    if (!"2".equals(id)) return;
                    CameraCharacteristics camera = manager.getCameraCharacteristics(id);
                    if (!Integer.valueOf(CameraCharacteristics.LENS_FACING_BACK).equals(
                            camera.get(CameraCharacteristics.LENS_FACING))) return;
                    if (!new Size(3264, 2448).equals(camera.get(
                            CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE))) return;
                    verifiedUltraWide = true;
                    Log.i("OcFresh", "DISCRETE_ULTRAWIDE_PHOTO stock non-SAT path enabled; verified camera=2; SAT remains unavailable");
                }
                p.setResult(true);
            }
        });
        // BaseMode.dl() forwards this exact support flag to ZoomManager. Its
        // original 2x threshold writes pref_none_sat_tele_angle_key; the stock
        // listener/entry selector then opens type6 telephoto before back_wide.
        XposedHelpers.findAndHookMethod("com.oppo.camera.e.a", loader, "f",
                String.class, new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) throws Throwable {
                if (p.hasThrowable() || Boolean.TRUE.equals(p.getResult())
                        || !"pref_none_sat_tele_angle_key".equals(p.args[0])
                        || XposedHelpers.getBooleanField(p.thisObject, "t")
                        || !"common".equals(XposedHelpers.callMethod(p.thisObject, "a"))) return;
                if (restoredSat()) return;
                if (!verifiedTelephoto) {
                    Map<?, ?> types = (Map<?, ?>) XposedHelpers.getStaticObjectField(utility, "f");
                    CameraManager manager = (CameraManager) XposedHelpers.getStaticObjectField(utility, "a");
                    // Current attested type6 maps to physical telephoto ID3.
                    if (types == null || manager == null || !"3".equals(String.valueOf(types.get(6)))) return;
                    CameraCharacteristics camera = manager.getCameraCharacteristics("3");
                    if (!Integer.valueOf(CameraCharacteristics.LENS_FACING_BACK).equals(
                            camera.get(CameraCharacteristics.LENS_FACING))) return;
                    if (!new Size(4224, 3136).equals(camera.get(
                            CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE))) return;
                    float[] focal = camera.get(CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
                    if (focal == null || focal.length == 0 || Math.abs(focal[0] - 6.25f) > 0.01f) return;
                    verifiedTelephoto = true;
                    Log.i("OcFresh", "DISCRETE_TELEPHOTO_PHOTO stock non-SAT path enabled; verified camera=3 focal=6.25; original 2x threshold");
                }
                p.setResult(true);
            }
        });
        // Measured startup: physical tele open, original base=1, globalZoom=1.
        // Original body computes global-2+base -> zero, then divides by zero.
        // Keep the original base getter intact; normalize the body input below.
        XposedHelpers.findAndHookMethod("com.oppo.camera.f", loader, "b",
                float.class, boolean.class, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (!activeTelephoto(p.thisObject)) return;
                float base = ((Number) XposedHelpers.callMethod(p.thisObject, "aQ")).floatValue();
                float global = (Float) p.args[0];
                if (!Float.isFinite(global) || !Float.isFinite(base) || base <= 0) return;
                // Preserve the original body's bookkeeping and request submit.
                // It computes (argument-2+base). Also handle startup with the
                // tele preference retained but a fresh default globalZoom=1.
                float physical = Math.max(1f, global / 2f);
                p.args[0] = physical + 2f - base;
                if (geometryReports.getAndIncrement() < 20) Log.i("OcFresh",
                        "TELE_CROP_GEOMETRY global=" + global + " stockBase=" + base
                                + " originalBodyArgument=" + p.args[0] + " physicalZoom=" + physical);
            }
        });
        XposedHelpers.findAndHookMethod("com.oppo.camera.ac", loader, "a",
                float.class, float.class, float.class, android.graphics.Rect.class,
                Class.forName("com.oppo.camera.ab", false, loader), boolean.class,
                new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (restoredSat() || !verifiedTelephoto || (Float) p.args[2] >= 2f) return;
                Object listener = XposedHelpers.getObjectField(p.thisObject, "y");
                Object data = p.args[4];
                if (listener == null || data == null
                        || !Boolean.TRUE.equals(XposedHelpers.callMethod(data, "e"))
                        || !Boolean.TRUE.equals(XposedHelpers.callMethod(listener, "o"))) return;
                Log.i("OcFresh", "TELE_INITIAL_UI_ZOOM " + p.args[2] + " ->2; physical tele preference is active");
                p.args[2] = 2f;
            }
        });
        XposedHelpers.findAndHookMethod("com.oppo.camera.f", loader, "ci", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (!verifiedTelephoto) return;
                Object proxy = XposedHelpers.getObjectField(p.thisObject, "bR");
                Object prefs = XposedHelpers.getObjectField(p.thisObject, "bP");
                Log.i("OcFresh", "TELE_SWITCH_STATE proxyId="
                        + (proxy == null ? null : XposedHelpers.callMethod(proxy, "c"))
                        + " activeTeleCharacteristics=" + activeTelephoto(p.thisObject)
                        + " preference=" + (prefs == null ? null : XposedHelpers.callMethod(
                                prefs, "getString", "pref_none_sat_tele_angle_key", "off")));
            }
        });
    }
}
