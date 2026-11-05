package com.arkaroy.occamfresh;

import android.content.Context;
import android.util.Log;
import android.view.Display;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

/** Restore the original F14 display-table choice inside the raw camera only. */
final class CameraLayoutDensity {
    private static final AtomicInteger reports = new AtomicInteger();

    private static void selectOriginalDensity(Class<?> utility) {
        try {
            Object value = XposedHelpers.getStaticObjectField(utility, "L");
            Object table = XposedHelpers.getStaticObjectField(utility, "M");
            if (!(value instanceof Display) || !(table instanceof Map)) return;
            Display display = (Display) value;
            Display.Mode mode = display.getMode();
            if (display.getDisplayId() != Display.DEFAULT_DISPLAY
                    || mode.getPhysicalWidth() != 1080 || mode.getPhysicalHeight() != 2400) return;
            Object density = ((Map<?, ?>) table).get(mode.getPhysicalWidth());
            if (!(density instanceof Integer) || (Integer) density != 480) return;
            int previous = XposedHelpers.getStaticIntField(utility, "a");
            // Only the measured ROM-default fallback is replaced. Preserve
            // another display configuration or an already-correct OEM choice.
            if (previous != 400) return;
            XposedHelpers.setStaticIntField(utility, "a", (Integer) density);
            if (reports.getAndIncrement() < 8)
                Log.i("OcFresh", "UI_DENSITY_APP_ONLY previous=" + previous + " table=" + density
                        + " display=1080x2400; original Util.m applies its own configuration");
        } catch (Throwable unavailable) {
            if (reports.getAndIncrement() < 8)
                Log.i("OcFresh", "UI_DENSITY_COMPAT_UNAVAILABLE " + unavailable);
        }
    }

    static void install(ClassLoader loader) throws ClassNotFoundException {
        Class<?> utility = Class.forName("com.oppo.camera.util.Util", false, loader);
        XposedHelpers.findAndHookMethod(utility, "d", Context.class, new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (!p.hasThrowable()) selectOriginalDensity(utility);
            }
        });
        XposedHelpers.findAndHookMethod(utility, "m", Context.class, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                selectOriginalDensity(utility);
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.hasThrowable() || reports.get() >= 8 || !(p.getResult() instanceof Context)) return;
                Context context = (Context) p.getResult();
                Log.i("OcFresh", "UI_DENSITY_CONFIGURATION densityDpi="
                        + context.getResources().getConfiguration().densityDpi
                        + " density=" + context.getResources().getDisplayMetrics().density);
                reports.incrementAndGet();
            }
        });
    }
}
