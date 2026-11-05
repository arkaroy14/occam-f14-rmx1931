package com.arkaroy.occamfresh;

import android.graphics.ImageFormat;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.util.Log;
import android.util.Size;
import java.io.File;
import java.io.FileInputStream;
import java.nio.charset.StandardCharsets;
import org.json.JSONObject;
import java.util.Arrays;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

/** Validate original OEM capability assumptions against unmodified HAL metadata. */
final class CameraCapabilities {
    private static final Set<String> reported = ConcurrentHashMap.newKeySet();
    static boolean originalF14Profile(File root) {
        try (FileInputStream in = new FileInputStream(new File(root, "active_hal_profile.json"))) {
            JSONObject profile = new JSONObject(new String(in.readAllBytes(), StandardCharsets.UTF_8));
            return "847b1f9e19ef20e58cdf7ac42b0d3ed2c026b1e1b343be28471019c5de8e4ac0".equals(profile.optString("camera_sha256"))
                    && "a0836c593f66580a7faef35463abf7bd5a049b0acb91b72a6ef16a6fbd397fce".equals(profile.optString("chi_sha256"));
        } catch (Exception unavailable) { return false; }
    }
    static boolean install(ClassLoader loader, File root) throws Exception {
        // Private OEM SAT/RAW support need not appear in standard Camera2 flags.
        // These workarounds belong ONLY to the measured, incompatible HAL pair,
        // attested at boot by our root module. Never infer failure for stock F14
        // merely from missing standard LOGICAL_MULTI_CAMERA or RAW-input flags.
        File profile = new File(root, "active_hal_profile.json");
        if (!profile.isFile()) {
            once("HAL_WORKAROUNDS_INACTIVE no attested compatibility profile");
            return false;
        }
        JSONObject identity;
        try (FileInputStream in = new FileInputStream(profile)) {
            identity = new JSONObject(new String(in.readAllBytes(), StandardCharsets.UTF_8));
        }
        if (!"660eef76135ef87d2c5c18cd96f2df98b1203c667889ba3460145d833d0c90ef".equals(identity.optString("camera_sha256"))
                || !"d409821294137000aab4d2748ce79d924b1a7fc5c24038f8c04572278c47740d".equals(identity.optString("chi_sha256"))) {
            once("HAL_WORKAROUNDS_INACTIVE different HAL pair");
            return false;
        }
        Class<?> utility = Class.forName("com.oppo.camera.f.a", false, loader);
        XposedHelpers.findAndHookMethod(utility, "f", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) throws Throwable {
                if (p.hasThrowable() || !Boolean.TRUE.equals(p.getResult())) return;
                CameraManager manager = (CameraManager) XposedHelpers.getStaticObjectField(utility, "a");
                Map<?, ?> types = (Map<?, ?>) XposedHelpers.getStaticObjectField(utility, "f");
                if (manager == null || types == null || !types.containsKey(12)) return;
                String id = String.valueOf(types.get(12));
                CameraCharacteristics c = manager.getCameraCharacteristics(id);
                int[] capabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
                boolean logical = capabilities != null && Arrays.stream(capabilities).anyMatch(
                        value -> value == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA);
                Set<String> physical = c.getPhysicalCameraIds();
                if (logical && physical.size() >= 2) return;
                if (!types.containsKey(2) || !types.containsKey(0)) return;
                CameraCharacteristics ultra = manager.getCameraCharacteristics(String.valueOf(types.get(2)));
                CameraCharacteristics main = manager.getCameraCharacteristics(String.valueOf(types.get(0)));
                Size pixels = c.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE);
                if (pixels != null && pixels.equals(main.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE))
                        && manager.getCameraIdList().length == 10) {
                    once("SAT_ORIGINAL_PATH_RESTORED type12Camera=" + id + " main anchor with10 camera IDs; leave OEM SAT check intact");
                    return;
                }
                if (pixels == null || !pixels.equals(ultra.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE))
                        || pixels.equals(main.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE))) return;
                p.setResult(false);
                once("SAT_CAPABILITY_UNAVAILABLE type12Camera=" + id + " logical=" + logical
                        + " physicalIds=" + physical + " anchor matches ultra-wide, not main; verified HAL workaround");
            }
        });
        XposedHelpers.findAndHookMethod("com.oppo.camera.p", loader, "h", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) throws Throwable {
                if (p.hasThrowable() || !Boolean.TRUE.equals(p.getResult())) return;
                if (NightRawReprocess.enabled()) {
                    once("NIGHT_RAW_PROBE_RETAINS_OEM_RESULT real HAL configuration will decide support");
                    return;
                }
                CameraManager manager = (CameraManager) XposedHelpers.getStaticObjectField(utility, "a");
                if (manager == null) return;
                String[] ids = manager.getCameraIdList();
                if (ids.length == 0) return;
                // Only reject a global RAW16-input assumption when NO exposed
                // camera advertises it. Keep original result if any camera does.
                for (String id : ids) {
                    StreamConfigurationMap streams = manager.getCameraCharacteristics(id)
                            .get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                    if (streams == null) return;
                    int[] formats = streams.getInputFormats();
                    once("REPROCESS_INPUT_CAPABILITIES camera=" + id + " formats=" + Arrays.toString(formats));
                    for (int format : formats) if (format == ImageFormat.RAW_SENSOR) return;
                }
                p.setResult(false);
                once("RAW16_REPROCESS_INPUT_UNAVAILABLE cameras=" + Arrays.toString(ids)
                        + "; retain original non-reprocessing path; RAW output and algorithms unchanged");
            }
        });
        XposedHelpers.findAndHookMethod("com.oppo.camera.entry.b", loader, "a",
                String.class, int.class, new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) throws Throwable {
                if (p.hasThrowable() || !"macro".equals(p.args[0]) || (Integer) p.args[1] != 0) return;
                Map<?, ?> types = (Map<?, ?>) XposedHelpers.getStaticObjectField(utility, "f");
                if (types == null || !types.containsKey(2)) return;
                int ultra = ((Number) types.get(2)).intValue();
                // The original r() getter looks for OEM camera type 0x11 (17).
                if (types.containsKey(17) || Integer.valueOf(ultra).equals(p.getResult())) return;
                CameraManager manager = (CameraManager) XposedHelpers.getStaticObjectField(utility, "a");
                CameraCharacteristics camera = manager.getCameraCharacteristics(String.valueOf(ultra));
                if (!Integer.valueOf(CameraCharacteristics.LENS_FACING_BACK).equals(
                        camera.get(CameraCharacteristics.LENS_FACING))) return;
                once("MACRO_CAMERA_COMPAT original=" + p.getResult() + " -> ultrawide=" + ultra
                        + "; dedicated macro ID absent; original macro mode retained");
                p.setResult(ultra);
            }
        });
        DiscreteLenses.install(loader, utility);
        return true;
    }
    private static void once(String message) {
        if (reported.add(message)) Log.i("OcFresh", message);
    }
}
