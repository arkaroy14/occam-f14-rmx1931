package com.arkaroy.occamfresh;

import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import java.util.concurrent.atomic.AtomicInteger;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Bounded, opt-in startup/tap timing. Never enables a control or bypasses readiness. */
final class ReadinessTrace {
    private static final AtomicInteger budget = new AtomicInteger();
    private static final long origin = SystemClock.elapsedRealtime();
    private static void log(String text) {
        if (budget.getAndIncrement() < 140)
            Log.i("OcFreshReady", "elapsedMs=" + (SystemClock.elapsedRealtime() - origin)
                    + " mode=" + ObserveHook.currentMode() + " " + text);
    }
    private static String state(Object object) {
        View view = (View) object;
        return "enabled=" + view.isEnabled() + " clickable=" + view.isClickable()
                + " visibility=" + view.getVisibility() + " attached=" + view.isAttachedToWindow();
    }
    private static XC_MethodHook timing(String label) {
        return new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                p.setObjectExtra("oc_ready_start", SystemClock.elapsedRealtime());
                log(label + " begin");
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                try {
                    long start = (Long) p.getObjectExtra("oc_ready_start");
                    log(label + " end durationMs=" + (SystemClock.elapsedRealtime() - start)
                            + " result=" + p.getResult() + " error=" + p.getThrowable());
                } catch (Throwable ignored) { /* Original return/exception remains unchanged. */ }
            }
        };
    }
    static void install(ClassLoader loader) throws Exception {
        Class<?> shutter = Class.forName("com.oppo.camera.ui.control.ShutterButton", false, loader);
        XposedHelpers.findAndHookMethod(shutter, "onTouchEvent", MotionEvent.class, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                try {
                    int action = ((MotionEvent) p.args[0]).getActionMasked();
                    if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_UP)
                        log("SHUTTER_TOUCH action=" + action + " " + state(p.thisObject));
                } catch (Throwable ignored) { }
            }
        });
        XposedHelpers.findAndHookMethod(shutter, "performClick", timing("SHUTTER_CLICK"));
        for (String method : new String[]{"setEnabled", "setClickable"}) {
            XposedHelpers.findAndHookMethod(View.class, method, boolean.class, new XC_MethodHook() {
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (!shutter.isInstance(p.thisObject)) return;
                    try {
                        StringBuilder callers = new StringBuilder();
                        for (StackTraceElement frame : Thread.currentThread().getStackTrace()) {
                            if (frame.getClassName().startsWith("com.oppo.camera.")) {
                                callers.append(frame.getClassName()).append('.').append(frame.getMethodName()).append(';');
                                if (callers.length() > 220) break;
                            }
                        }
                        log("SHUTTER_STATE " + p.method.getName() + "=" + p.args[0] + " "
                                + state(p.thisObject) + " callers=" + callers);
                    } catch (Throwable ignored) { }
                }
            });
        }
        Class<?> base = Class.forName("com.oppo.camera.e.a", false, loader);
        XposedHelpers.findAndHookMethod(base, "c", Class.forName("com.oppo.camera.f.d", false, loader),
                timing("BEFORE_SNAPPING"));
        Class<?> aps = Class.forName("com.aps.APSClient", false, loader);
        for (String method : new String[]{"algoInit", "beforeCapture", "connect"})
            XposedBridge.hookAllMethods(aps, method, timing("APS_" + method));
        log("READINESS_TRACE_ACTIVE bounded140; original taps, states and capture checks unchanged");
    }
}
