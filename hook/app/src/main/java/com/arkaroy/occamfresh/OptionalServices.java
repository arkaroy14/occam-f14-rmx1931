package com.arkaroy.occamfresh;

import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Typeface;
import android.net.Uri;
import android.util.Log;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

final class OptionalServices {
    static void install(ClassLoader loader) throws ClassNotFoundException {
        Class<?> access = Class.forName("com.oplus.app.OPlusAccessControlManager", false, loader);
        XposedBridge.hookAllMethods(access, "isEncryptedPackage", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                // ColorOS's app-lock service is optional on this AOSP ROM. Only
                // handle its proven null-service case; retain the real service
                // and all Android keyguard behavior when available.
                if (XposedHelpers.getObjectField(p.thisObject, "mService") != null) return;
                Log.i("OcFresh", "OPTIONAL_SERVICE_ABSENT ColorOS app-lock package=" + p.args[0]);
                p.setResult(false);
            }
        });

        Class<?> heytapSystem = Class.forName("com.heytap.providers.a$d", false, loader);
        XposedHelpers.findAndHookMethod(heytapSystem, "a", ContentResolver.class,
                String.class, String.class, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                // RegionAdapter writes this OEM recording-state notification on
                // resume/pause. Its original caller ignores the boolean result.
                // With no provider, report a failed write instead of dereferencing
                // a null client. Do not fabricate success or write Android settings.
                if (!"oppo_is_camera_recording".equals(p.args[1])) return;
                Uri uri = (Uri) XposedHelpers.getStaticObjectField(heytapSystem, "a");
                try (ContentProviderClient client = ((ContentResolver) p.args[0])
                        .acquireContentProviderClient(uri)) {
                    if (client != null) return;
                }
                Log.i("OcFresh", "OPTIONAL_PROVIDER_ABSENT " + uri
                        + " key=" + p.args[1] + " write unavailable");
                p.setResult(false);
            }
        });

        Class<?> oplusConfiguration = Class.forName("android.content.res.OplusBaseConfiguration", false, loader);
        XposedHelpers.findAndHookMethod("com.oppo.camera.util.Util", loader, "a",
                Context.class, boolean.class, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                Context context = (Context) p.args[0];
                if (oplusConfiguration.isInstance(context.getResources().getConfiguration())) return;
                // The donor font helper dereferences a ColorOS-only Configuration
                // subclass before its own fallback. AOSP has no OEM font-variation
                // setting; use its normal UI font, keeping the requested style.
                p.setResult(Typeface.create(Typeface.DEFAULT,
                        (Boolean) p.args[1] ? Typeface.BOLD : Typeface.NORMAL));
                Log.i("OcFresh", "UI_FONT_COMPAT AOSP configuration; style=" + p.args[1]);
            }
        });
    }
}
