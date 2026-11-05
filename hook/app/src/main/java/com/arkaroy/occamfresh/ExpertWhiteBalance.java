package com.arkaroy.occamfresh;

import android.util.Log;
import java.util.Collections;
import java.util.Set;
import java.util.WeakHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

/** Opt-in F14 Expert WB request-branch compatibility; original Kelvin/SDK kept. */
final class ExpertWhiteBalance {
    private static final ThreadLocal<Boolean> scope = new ThreadLocal<>();
    private static final Set<Object> bridged = Collections.synchronizedSet(
            Collections.newSetFromMap(new WeakHashMap<Object, Boolean>()));
    private static final AtomicInteger reports = new AtomicInteger();
    private static final String EXTRA = "ocfreshExpertWbPreviousScope";

    static void install(ClassLoader loader) throws ClassNotFoundException {
        Class<?> camera = Class.forName("com.oppo.camera.f.g", false, loader);
        Class<?> utility = Class.forName("com.oppo.camera.util.Util", false, loader);
        Class<?> config = Class.forName("com.oppo.camera.aps.config.CameraConfig", false, loader);
        XposedHelpers.findAndHookMethod(config, "getConfigBooleanValue", String.class,
                new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.hasThrowable() || !Boolean.TRUE.equals(scope.get())
                        || !"com.oplus.mmcamera.professional.support".equals(p.args[0])) return;
                if (reports.getAndIncrement() < 24)
                    Log.i("OcFresh", "EXPERT_WB_ORIGINAL_QCA_BRANCH previous=" + p.getResult()
                            + "; original setter sends partial_mwb_mode and colour temperature");
                p.setResult(true);
            }
        });
        XposedHelpers.findAndHookMethod(camera, "k", int.class, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                int kelvin = (Integer) p.args[0];
                String id = ObserveHook.currentCameraId();
                if (!"professional".equals(ObserveHook.currentMode())
                        || !("0".equals(id) || "2".equals(id) || "3".equals(id))
                        || kelvin < 2000 || kelvin > 8000
                        || !Boolean.TRUE.equals(XposedHelpers.callStaticMethod(utility, "p"))) return;
                p.setObjectExtra(EXTRA, new Boolean[]{scope.get()});
                scope.set(true);
                Log.i("OcFresh", "EXPERT_WB_SET kelvin=" + kelvin
                        + " camera=" + id + "; original arguments/results retained");
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                Boolean[] previous = (Boolean[]) p.getObjectExtra(EXTRA);
                if (previous == null) return;
                scope.set(previous[0]);
                if (!p.hasThrowable()) bridged.add(p.thisObject);
            }
        });
        XposedHelpers.findAndHookMethod(camera, "r", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (!bridged.contains(p.thisObject)) return;
                p.setObjectExtra(EXTRA, new Boolean[]{scope.get()});
                scope.set(true);
                Log.i("OcFresh", "EXPERT_WB_AUTO original reset also clears partial_mwb_mode");
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                Boolean[] previous = (Boolean[]) p.getObjectExtra(EXTRA);
                if (previous == null) return;
                scope.set(previous[0]);
                if (!p.hasThrowable()) bridged.remove(p.thisObject);
            }
        });
        Log.i("OcFresh", "EXPERT_WB_COMPAT_READY exactF14 opt-in Expert cameras0/2/3; original request branch only");
    }
}
