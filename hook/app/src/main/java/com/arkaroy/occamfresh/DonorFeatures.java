package com.arkaroy.occamfresh;

import android.util.Log;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Collection;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import org.json.JSONObject;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Default donor product configuration, not a camera-capability override. */
final class DonorFeatures {
    static void install(ClassLoader loader, File root) throws Exception {
        File file = new File(root, "feature_config.json");
        if (file.length() > 1_000_000) throw new IllegalArgumentException("oversized feature profile");
        JSONObject profile = new JSONObject(new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8));
        if (profile.getInt("schema") != 1) throw new IllegalArgumentException("unknown feature schema");
        JSONObject declarations = profile.getJSONObject("features");
        JSONObject legacy = profile.getJSONObject("legacy_features");
        Set<String> logged = ConcurrentHashMap.newKeySet();
        Class<?> manager = Class.forName("com.oplus.content.OplusFeatureConfigManager", false, loader);
        XposedBridge.hookAllMethods(manager, "hasFeatureIPC", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) throws Throwable {
                if (p.args.length != 1 || !(p.args[0] instanceof String)) return;
                String name = (String) p.args[0];
                // Original service reads native feature properties directly.
                Class<?> cache = Class.forName("com.oplus.content.OplusFeatureCache", false, loader);
                for (String field : new String[]{"READONLYNATIVEFEATURES", "PERSISTNATIVEFEATURES", "MEMORYNATIVEFEATURES"}) {
                    Object list;
                    try { list = XposedHelpers.getStaticObjectField(cache, field); }
                    catch (NoSuchFieldError missing) { continue; }
                    if (list instanceof Collection && ((Collection<?>) list).contains(name)) {
                        boolean value = (Boolean) XposedHelpers.callStaticMethod(
                                Class.forName("android.os.SystemProperties"), "getBoolean", name, false);
                        p.setResult(value);
                        if (logged.add(name)) Log.i("OcFresh", "FEATURE_PROPERTY " + name + "=" + value);
                        return;
                    }
                }
                JSONObject declaration = declarations.optJSONObject(name);
                boolean value = declaration != null && declaration.getBoolean("enabled");
                p.setResult(value);
                if (logged.add(name)) Log.i("OcFresh", "FEATURE_F14_DEFAULT " + name + "=" + value
                        + " source=" + (declaration == null ? "absent from default F14 configuration" : declaration.getString("source")));
            }
        });
        Class<?> pm = Class.forName("android.app.ApplicationPackageManager");
        XposedBridge.hookAllMethods(pm, "hasSystemFeature", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) throws Throwable {
                if (p.args.length < 1 || !(p.args[0] instanceof String)) return;
                String name = (String) p.args[0];
                JSONObject declaration = legacy.optJSONObject(name);
                if (declaration == null) return;
                boolean value = declaration.getBoolean("enabled");
                p.setResult(value);
                if (logged.add("legacy:" + name)) Log.i("OcFresh", "FEATURE_F14_LEGACY " + name + "=" + value
                        + " source=" + declaration.getString("source"));
            }
        });
        Log.i("OcFresh", "FEATURE_PROVIDER_ACTIVE build=" + profile.getString("build")
                + " declarations=" + declarations.length() + " legacy=" + legacy.length()
                + "; product configuration only, no camera2 metadata changes");
    }
}
