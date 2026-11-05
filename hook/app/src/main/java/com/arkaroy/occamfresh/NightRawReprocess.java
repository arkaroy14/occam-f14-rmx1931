package com.arkaroy.occamfresh;

import android.graphics.ImageFormat;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.InputConfiguration;
import android.util.Log;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Explicit opt-in experiment: let the measured HAL decide a private RAW session.
 * Does not fake metadata, session flags, requests, pixels or successful results.
 */
final class NightRawReprocess {
    private static boolean armed;
    private static volatile String cameraId = "unknown";
    static boolean enabled() {
        return armed && "0".equals(cameraId) && "night".equals(ObserveHook.currentMode());
    }
    static void install(ClassLoader loader) throws ClassNotFoundException {
        Class<?> device = Class.forName("android.hardware.camera2.impl.CameraDeviceImpl", false, loader);
        XposedBridge.hookAllConstructors(device, new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (!p.hasThrowable()) cameraId = ((CameraDevice) p.thisObject).getId();
            }
        });
        XposedHelpers.findAndHookMethod(device, "checkInputConfiguration", InputConfiguration.class,
                new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                InputConfiguration input = (InputConfiguration) p.args[0];
                if (!enabled() || input == null || input.getFormat() != ImageFormat.RAW_SENSOR
                        || input.getWidth() != 4608 || input.getHeight() != 3456
                        || !"0".equals(((CameraDevice) p.thisObject).getId())) return;
                // AOSP16's standard-map validation rejects unadvertised private
                // RAW input before camera3/HAL can inspect it. Skip only that
                // Java check for this opt-in measured tuple. Preserve the real
                // InputConfiguration and all native configuration/queue errors.
                Log.i("OcFresh", "NIGHT_RAW_INPUT_PROBE camera=0 size=4608x3456 format=32; public-map check skipped, actual HAL acceptance required");
                p.setResult(null);
            }
        });
        armed = true;
        Log.i("OcFresh", "NIGHT_RAW_PROBE_ARMED measured HAL only; rear Night only; genuine RAW input session required");
    }
    static void testProcessedRawControl(ClassLoader loader) throws ClassNotFoundException {
        XposedHelpers.findAndHookMethod(CaptureRequest.Builder.class, "build",
                new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (!enabled()) return;
                CaptureRequest.Builder builder = (CaptureRequest.Builder) p.thisObject;
                CaptureRequest request = (CaptureRequest) XposedHelpers.getObjectField(builder, "mRequest");
                if (!Boolean.TRUE.equals(XposedHelpers.callMethod(request, "isReprocess"))) return;
                for (CaptureRequest.Key<?> key : request.getKeys()) {
                    if (!"com.oplus.supernight.mode".equals(key.getName())) continue;
                    Object value = request.get(key);
                    if (!(value instanceof int[]) || ((int[]) value).length != 1 || ((int[]) value)[0] != 2) return;
                    // The original Arcsoft Night engine has already produced
                    // this RAW. In this HAL, copying its capture-time mode2
                    // causes FeatureZSL to insert a new real-time AE bracket
                    // and ainight/isp.gain metadata on the reprocess request,
                    // before any offline processing. Test the original neutral
                    // mode only on that second-stage request. Keep the original
                    // processed RAW input, exposure, crop and all output paths.
                    @SuppressWarnings("unchecked") CaptureRequest.Key<int[]> typed = (CaptureRequest.Key<int[]>) key;
                    builder.set(typed, new int[]{0});
                    Log.i("OcFresh", "NIGHT_PROCESSED_RAW_CONTROL_TEST final build supernight.mode=2->0; original Night capture/processing and RAW input retained");
                    return;
                }
            }
        });
    }
}
