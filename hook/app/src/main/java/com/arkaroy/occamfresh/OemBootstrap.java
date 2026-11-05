package com.arkaroy.occamfresh;

import android.util.Log;
import java.io.File;
import java.io.FileInputStream;
import java.security.MessageDigest;
import java.lang.reflect.Array;
import dalvik.system.PathClassLoader;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

/** Optional original-JAR bridge, restricted to the exact raw F14 APK and marker. */
final class OemBootstrap {
    private static final String RAW = "3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5";

    static void install(XC_LoadPackage.LoadPackageParam lp) {
        File root = new File(new File(lp.appInfo.sourceDir).getParentFile(), "fresh_oem_bootstrap");
        if (!new File(root, "enable").isFile()) return;
        try {
            MessageDigest hash = MessageDigest.getInstance("SHA-256");
            try (FileInputStream in = new FileInputStream(lp.appInfo.sourceDir)) {
                byte[] block = new byte[1024 * 1024];
                int n;
                while ((n = in.read(block)) != -1) hash.update(block, 0, n);
            }
            StringBuilder hex = new StringBuilder();
            for (byte value : hash.digest()) hex.append(String.format("%02x", value & 255));
            if (!RAW.equals(hex.toString())) {
                Log.i("OcFresh", "OEM_BOOTSTRAP_REFUSED APK hash=" + hex);
                return;
            }
            installLegacyFeatureCacheAdapter();
            StringBuilder path = new StringBuilder();
            for (String jar : new String[]{"framework.jar", "oplus-framework.jar", "coloros-support-wrapper.jar"}) {
                File file = new File(root, jar);
                if (!file.isFile()) throw new IllegalStateException("missing donor " + file);
                if (path.length() != 0) path.append(File.pathSeparator);
                path.append(file.getAbsolutePath());
            }
            // Existing Android boot classes take priority. No BOOTCLASSPATH changes.
            ClassLoader donor = new PathClassLoader(path.toString(), lp.classLoader.getParent());
            Object appPath = XposedHelpers.getObjectField(lp.classLoader, "pathList");
            Object donorPath = XposedHelpers.getObjectField(donor, "pathList");
            Object[] original = (Object[]) XposedHelpers.getObjectField(appPath, "dexElements");
            Object[] extra = (Object[]) XposedHelpers.getObjectField(donorPath, "dexElements");
            Object merged = Array.newInstance(original.getClass().getComponentType(), original.length + extra.length);
            System.arraycopy(original, 0, merged, 0, original.length);
            System.arraycopy(extra, 0, merged, original.length, extra.length);
            XposedHelpers.setObjectField(appPath, "dexElements", merged);
            DonorFeatures.install(lp.classLoader, root);
            OptionalServices.install(lp.classLoader);
            try { CameraLayoutDensity.install(lp.classLoader); }
            catch (Throwable unavailable) { Log.e("OcFresh", "UI_DENSITY_SETUP_FAILED", unavailable); }
            boolean measuredHal = CameraCapabilities.install(lp.classLoader, root);
            GalleryBridge.install(lp.classLoader);
            RecorderProfiles.install();
            boolean traceBuffers = new File(root, "trace_capture_buffers").isFile();
            boolean exactSizeTest = false;
            boolean traceNight = false;
            boolean nightRawTest = false;
            boolean nightProcessedRawTest = false;
            boolean testDarksight = false;
            boolean testFrontDsp = false;
            boolean traceModes = false;
            boolean environmentAudit = false;
            boolean expertWb = false;
            boolean traceBeauty = false;
            boolean frontPublicSizes = false;
            boolean nightReferenceMetadata = false;
            boolean nightRequestKeys = false;
            boolean nightImagePair = false;
            boolean frontPoolSizes = false;
            boolean frontFullSizes = false;
            boolean frontNightFullTrial = false;
            boolean nightScalarAudit = false;
            boolean nightSignalAudit = false;
            boolean nightDetachTrial = false;
            boolean retainedTimelapse = false;
            // Existing enable file also supports a temporary diagnostic flag;
            // adding new files beneath an active overlay can retain stale misses.
            try (FileInputStream flags = new FileInputStream(new File(root, "enable"))) {
                String content = new String(flags.readNBytes(1024), java.nio.charset.StandardCharsets.UTF_8);
                retainedTimelapse = content.contains("\nENABLE_TIMELAPSE_STOCK_RULE=1\n");
                nightScalarAudit = content.contains("\nTRACE_NIGHT_SCALARS=1\n");
                nightSignalAudit = content.contains("\nTRACE_NIGHT_SIGNAL=1\n");
                nightDetachTrial = content.contains("\nTEST_NIGHT_DETACH_COMPAT=1\n")
                        || content.contains("\nENABLE_NIGHT_DETACH_COMPAT=1\n");
                frontPoolSizes = content.contains("\nTEST_FRONT_NIGHT_POOL=1\n")
                        || content.contains("\nENABLE_FRONT_NIGHT_POOL=1\n");
                frontNightFullTrial = content.contains("\nTEST_FRONT_NIGHT_FULL_YUV=1\n")
                        || content.contains("\nENABLE_FRONT_NIGHT_FULL_YUV=1\n");
                nightImagePair = content.contains("\nTRACE_NIGHT_IMAGE_PAIR=1\n");
                nightRequestKeys = content.contains("\nTRACE_NIGHT_REQUEST_KEYS=1\n");
                nightReferenceMetadata = content.contains("\nTEST_NIGHT_REFERENCE_METADATA=1\n")
                        || content.contains("\nENABLE_NIGHT_REFERENCE_METADATA=1\n");
                traceBuffers |= content.contains("\nTRACE_CAPTURE_BUFFERS=1\n");
                exactSizeTest = content.contains("\nTEST_HIGHRES_EXACT_SIZE=1\n");
                traceNight = content.contains("\nTRACE_NIGHT_OPENCL=1\n");
                nightRawTest = content.contains("\nTEST_NIGHT_RAW_REPROCESS=1\n");
                nightProcessedRawTest = content.contains("\nTEST_NIGHT_PROCESSED_RAW_CONTROL=1\n");
                testDarksight = content.contains("\nTEST_DARKSIGHT_RUNTIME=1\n");
                testFrontDsp = content.contains("\nTEST_FRONT_NIGHT_DSP_SESSION=1\n")
                        || content.contains("\nENABLE_FRONT_NIGHT_DSP=1\n");
                traceModes = content.contains("\nTRACE_MODE_PROCESSING=1\n");
                environmentAudit = content.contains("\nTRACE_ENVIRONMENT=1\n");
                traceBeauty = content.contains("\nTRACE_BEAUTY=1\n");
                frontFullSizes = content.contains("\nTEST_FRONT_FULL_SIZE_ORDER=1\n")
                        || content.contains("\nENABLE_FRONT_FULL_SIZE_ORDER=1\n");
                frontPublicSizes = content.contains("\nTEST_FRONT_PUBLIC_SIZE=1\n")
                        || content.contains("\nENABLE_FRONT_PUBLIC_SIZE=1\n");
                expertWb = content.contains("\nTEST_EXPERT_WB_QCA=1\n")
                        || content.contains("\nENABLE_EXPERT_WB_QCA=1\n");
            }
            if (CameraCapabilities.originalF14Profile(root) || (measuredHal && exactSizeTest))
                HighResolutionStreams.install();
            if (CameraCapabilities.originalF14Profile(root) && expertWb) {
                try { ExpertWhiteBalance.install(lp.classLoader); }
                catch (Throwable unavailable) { Log.e("OcFresh", "EXPERT_WB_SETUP_FAILED", unavailable); }
            }
            if (CameraCapabilities.originalF14Profile(root) && frontPublicSizes) {
                try { FrontPublicSizes.install(lp.classLoader, frontFullSizes, frontPoolSizes, frontNightFullTrial); }
                catch (Throwable unavailable) { Log.e("OcFresh", "FRONT_PUBLIC_SIZE_SETUP_FAILED", unavailable); }
            }
            if (measuredHal && nightRawTest) NightRawReprocess.install(lp.classLoader);
            if (measuredHal && nightRawTest && nightProcessedRawTest)
                NightRawReprocess.testProcessedRawControl(lp.classLoader);
            if (CameraCapabilities.originalF14Profile(root) && nightRequestKeys) {
                try { NightRequestTrace.install(lp.classLoader); }
                catch (Throwable unavailable) { Log.e("OcFresh", "NIGHT_REQUEST_KEYS_SETUP_FAILED", unavailable); }
            }
            if (CameraCapabilities.originalF14Profile(root) && nightReferenceMetadata) {
                try { NightReferenceMetadata.install(lp.classLoader, nightImagePair); }
                catch (Throwable unavailable) { Log.e("OcFresh", "NIGHT_REFERENCE_SETUP_FAILED", unavailable); }
            }
            if (traceNight) {
                try { ReprocessTrace.install(lp.classLoader); }
                catch (Throwable unavailable) { Log.e("OcFresh", "REPROCESS_TRACE_SETUP_FAILED", unavailable); }
            }
            if (traceBuffers) {
                BufferTrace.install(lp.classLoader);
                Log.i("OcFresh", "CAPTURE_LAYOUT_TRACE_ACTIVE metadata only; preview decisions excluded");
            }
            if (traceModes) {
                try { ModeProcessingTrace.install(lp.classLoader); }
                catch (Throwable unavailable) { Log.e("OcFresh", "MODE_PROCESSING_TRACE_SETUP_FAILED", unavailable); }
            }
            if (traceBeauty) {
                try {
                    traceBeauty = CameraCapabilities.originalF14Profile(root)
                            && matchesDigest(new File(lp.appInfo.nativeLibraryDir, "libAlgoInterface.so"),
                                "aeea5953699e06773de4c68f5f51b312fb3d7f4d14c6755299d777ccab1bdecc")
                            && matchesDigest(new File(lp.appInfo.nativeLibraryDir, "libFaceBeautyCap.so"),
                                "2e537cdf30e138e7d97787c5b26ef458b00398b88e82646c8b5f4735d36e70a5");
                } catch (Exception unavailable) {
                    traceBeauty = false;
                    Log.e("OcFresh", "BEAUTY_OBSERVER_ATTESTATION_FAILED", unavailable);
                }
                Log.i("OcFresh", "BEAUTY_OBSERVER_ATTESTED=" + traceBeauty);
            }
            NativePaths.install(lp.appInfo.nativeLibraryDir, lp.appInfo.dataDir, measuredHal,
                    CameraCapabilities.originalF14Profile(root), traceNight, testDarksight, testFrontDsp, traceBeauty, retainedTimelapse);
            if(nightScalarAudit && CameraCapabilities.originalF14Profile(root)) {
                NightScalarTrace.install(lp.classLoader);
                NativePaths.setNightScalarAudit(true);
                if(nightSignalAudit){
                    NativePaths.setNightSignalAudit(true);
                    NightBufferTrace.install(lp.classLoader);
                }
            }
            if(nightDetachTrial && CameraCapabilities.originalF14Profile(root))
                NightDetachCompat.install(lp.classLoader);
            if(environmentAudit && CameraCapabilities.originalF14Profile(root)) EnvironmentAudit.install(lp.classLoader);
            Log.i("OcFresh", "OEM_BOOTSTRAP_ACTIVE original APK hash verified; appended original JAR DEX elements="
                    + extra.length + " after app elements=" + original.length + "; boot parent unchanged");
        } catch (Throwable failure) {
            Log.e("OcFresh", "OEM_BOOTSTRAP_SETUP_FAILED", failure);
        }
    }

    private static boolean matchesDigest(File file, String expected) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (FileInputStream input = new FileInputStream(file)) {
            byte[] block = new byte[1024 * 1024];
            int count;
            while ((count = input.read(block)) != -1) digest.update(block, 0, count);
        }
        StringBuilder hex = new StringBuilder();
        for (byte value : digest.digest()) hex.append(String.format("%02x", value & 255));
        return expected.equals(hex.toString());
    }

    private static void installLegacyFeatureCacheAdapter() throws ClassNotFoundException {
        Class<?> cache = Class.forName("android.app.PropertyInvalidatedCache");
        XposedBridge.hookAllConstructors(cache, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                for (int i = 0; i < p.args.length; i++) {
                    if (!(p.args[i] instanceof String)) continue;
                    String name = (String) p.args[i];
                    if (!name.startsWith("sys.cache.has_feature_")) continue;
                    p.setObjectExtra("ocfreshLegacyFeatureCache", name);
                    // Android 16 rejects the old OEM cache property namespace.
                    // A local cache identity avoids writing any system property.
                    p.args[i] = "cache_key.test.ocfresh_" + name.substring("sys.cache.".length());
                }
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                Object name = p.getObjectExtra("ocfreshLegacyFeatureCache");
                if (name == null || p.hasThrowable()) return;
                // No ROM server invalidates this OEM cache on AOSP. Bypass its
                // local storage so original recompute/feature queries remain live.
                XposedHelpers.callMethod(p.thisObject, "disableLocal");
                Log.i("OcFresh", "OEM_FEATURE_CACHE_COMPAT " + name
                        + " local cache disabled; original feature recompute unchanged");
            }
        });
    }
}
