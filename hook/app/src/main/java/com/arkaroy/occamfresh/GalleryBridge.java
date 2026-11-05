package com.arkaroy.occamfresh;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.util.Log;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

/** Use Android's photo viewer only when the original ColorOS gallery is absent. */
final class GalleryBridge {
    static void install(ClassLoader loader) throws ClassNotFoundException {
        Class<?> thumbnail = Class.forName("com.oppo.camera.ui.control.e", false, loader);
        XposedHelpers.findAndHookMethod("com.oppo.camera.f", loader, "a", thumbnail,
                new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                Activity activity = (Activity) XposedHelpers.getObjectField(p.thisObject, "bO");
                try {
                    activity.getPackageManager().getPackageInfo("com.coloros.gallery3d", 0);
                    return;
                } catch (PackageManager.NameNotFoundException absent) {
                    // The original handler returns without opening anything here.
                }
                p.setResult(null);
                if (p.args[0] == null) return;
                Uri uri = (Uri) XposedHelpers.callMethod(p.args[0], "d");
                String mime = (String) XposedHelpers.callMethod(p.args[0], "i");
                if (uri == null || !"content".equals(uri.getScheme())) {
                    Log.w("OcFresh", "GALLERY_VIEW_UNAVAILABLE no content URI");
                    return;
                }
                try {
                    XposedHelpers.callMethod(p.thisObject, "cL");
                    Intent intent = new Intent(Intent.ACTION_VIEW).setDataAndType(uri, mime)
                            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    intent.setClipData(ClipData.newRawUri("Camera photo", uri));
                    activity.startActivity(intent);
                    XposedHelpers.setBooleanField(p.thisObject, "bl", true);
                    Object callback = XposedHelpers.getObjectField(p.thisObject, "du");
                    if (callback != null) XposedHelpers.callMethod(callback, "a", true);
                    Log.i("OcFresh", "GALLERY_VIEW_STARTED uri=" + uri + " mime=" + mime);
                } catch (Throwable failure) {
                    XposedHelpers.setBooleanField(p.thisObject, "bl", false);
                    Log.e("OcFresh", "GALLERY_VIEW_FAILED", failure);
                }
            }
        });
    }
}
