package com.arkaroy.occamfresh;

import android.util.Log;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Opt-in original AI/bokeh/EIS decisions and capture parameters; no overrides. */
final class ModeProcessingTrace {
    private static final Map<String, String> previous = new ConcurrentHashMap<>();
    private static final AtomicInteger budget = new AtomicInteger();
    private static void changed(String key, Object value) {
        String text = String.valueOf(value);
        if (!text.equals(previous.put(key, text)) && budget.getAndIncrement() < 300)
            Log.i("OcFreshMode", "mode=" + ObserveHook.currentMode() + " " + key + "=" + text);
    }
    private static void observeVideoBokeh(ClassLoader loader) throws Exception {
        Class<?> api = Class.forName("com.anc.humansdk.HumanEffectBokehApi", false, loader);
        XposedBridge.hookAllMethods(api, "initByConfig", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                try {
                    changed("videoBokeh.initResult", p.hasThrowable() ? p.getThrowable() : p.getResult());
                    changed("videoBokeh.handle", XposedHelpers.getObjectField(p.thisObject, "handle"));
                    if (p.args.length == 1 && p.args[0] != null) {
                        for (String field : new String[]{"modelPath", "nativeLibPath", "runtime", "isRealTime"})
                            changed("videoBokeh.config." + field, XposedHelpers.getObjectField(p.args[0], field));
                    }
                } catch (Throwable ignored) { /* Original initialization result is untouched. */ }
            }
        });
        for (String name : new String[]{"detectTextureIn", "detectTextureInSeg", "process"}) {
            XposedBridge.hookAllMethods(api, name, new XC_MethodHook() {
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    try {
                        String method = p.method.getName() + "." + p.args.length;
                        changed("videoBokeh." + method + ".result", p.hasThrowable() ? p.getThrowable() : p.getResult());
                        if ("process".equals(p.method.getName()) && p.args.length == 5)
                            changed("videoBokeh.render.blur", p.args[2]);
                        else if ("process".equals(p.method.getName()) && p.args.length >= 12) {
                            changed("videoBokeh.request.blur", p.args[5]);
                            changed("videoBokeh.request.facingFlag", p.args[7]);
                            changed("videoBokeh.request.geometry", p.args[10] + "x" + p.args[11]);
                        }
                        else if (p.method.getName().startsWith("detectTexture") && p.args.length >= 4)
                            changed("videoBokeh.detect.geometry", p.args[1] + "x" + p.args[2] + " rotation=" + p.args[3]);
                    } catch (Throwable ignored) { /* No frame, argument or result override. */ }
                }
            });
        }
        Log.i("OcFreshMode", "VIDEO_BOKEH_OBSERVER_ACTIVE original SDK codes:0=OK,1=argument,2=handle,3=failure,4=no_person,5=too_far,6=multiple,7=GL_compiling");
    }
    static void install(ClassLoader loader) throws Exception {
        Class<?> total = Class.forName("com.oppo.camera.aps.adapter.ApsTotalResult", false, loader);
        String[] names = {"APS_AI_SCENE", "APS_AI_TUNING_SCENE", "APS_BOKEN_STATE", "APS_VIDEO_EIS"};
        Object[] keys = new Object[names.length];
        for (int i=0; i<names.length; ++i) keys[i] = XposedHelpers.getStaticObjectField(total, names[i]);
        XposedBridge.hookAllConstructors(total, new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.hasThrowable()) return;
                try {
                    Map<?, ?> values = (Map<?, ?>) XposedHelpers.getObjectField(p.thisObject, "mResults");
                    if (values == null) return;
                    for (int i=0; i<keys.length; ++i) changed("nativeResult." + names[i], values.get(keys[i]));
                } catch (Throwable ignored) { /* Observation cannot alter the original constructor. */ }
            }
        });
        Class<?> model = Class.forName("com.oppo.camera.aps.adapter.ApsParameters$ParameterModel", false, loader);
        XposedBridge.hookAllMethods(model, "setParameter", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.hasThrowable() || p.args.length != 2 || p.args[0] == null) return;
                try {
                    String name = (String) XposedHelpers.callMethod(p.args[0], "getName");
                    if ("ai_scene".equals(name) || "pi_enable".equals(name)
                            || "blur_level".equals(name) || "video_blur_level".equals(name)
                            || "bokeh_blur_level".equals(name) || "video_eis".equals(name))
                        changed("capture." + p.thisObject.getClass().getSimpleName() + "." + name, p.args[1]);
                } catch (Throwable ignored) { /* Original metadata is untouched. */ }
            }
        });
        Class<?> preferences = Class.forName("android.app.SharedPreferencesImpl");
        XposedHelpers.findAndHookMethod(preferences, "getString", String.class, String.class, new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (!p.hasThrowable() && ("pref_ai_scene_key".equals(p.args[0])
                        || "pref_camera_pi_mode_key".equals(p.args[0]))) changed("preference." + p.args[0], p.getResult());
            }
        });
        try { observeVideoBokeh(loader); }
        catch (Throwable unavailable) { Log.i("OcFreshMode", "VIDEO_BOKEH_OBSERVER_UNAVAILABLE " + unavailable); }
        try { ReadinessTrace.install(loader); }
        catch (Throwable unavailable) { Log.i("OcFreshMode", "READINESS_OBSERVER_UNAVAILABLE " + unavailable); }
        try { FaceMetadataTrace.install(loader); }
        catch (Throwable unavailable) { Log.i("OcFreshMode", "FACE_OBSERVER_UNAVAILABLE " + unavailable); }
        Log.i("OcFreshMode", "MODE_PROCESSING_TRACE_ACTIVE budget300; native results and capture parameters unchanged");
    }
}
