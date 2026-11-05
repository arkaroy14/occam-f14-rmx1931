package com.arkaroy.occamfresh;

import android.app.Application;
import android.content.Context;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.SessionConfiguration;
import android.media.CamcorderProfile;
import android.os.Process;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Array;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

/** Diagnostics plus an explicitly staged, raw-APK-only OEM classloader bridge. */
public final class ObserveHook implements IXposedHookLoadPackage {
    private static final String TAG = "OcFresh";
    private static final Set<String> seen = ConcurrentHashMap.newKeySet();
    private static final ConcurrentHashMap<String, String> previous = new ConcurrentHashMap<>();
    private static final AtomicInteger events = new AtomicInteger();
    private static volatile File logFile;
    private static volatile String mode = "unknown";
    static String currentMode() { return mode; }
    private static volatile String cameraId = "unknown";
    static String currentCameraId() { return cameraId; }
    private static long lastTimestamp;
    private static int frames;
    private static long firstTimestamp;
    private static long firstFrameNumber;
    private static int frameSession;
    private static int lateFrames;

    @Override public void handleLoadPackage(XC_LoadPackage.LoadPackageParam lp) {
        if (!"com.oppo.camera".equals(lp.packageName)) return;
        log("LOAD version=" + BuildConfig.VERSION_NAME + " pid=" + Process.myPid() + " process=" + lp.processName
                + " apk=" + lp.appInfo.sourceDir + " nativeDir=" + lp.appInfo.nativeLibraryDir);
        OemBootstrap.install(lp);
        hook(Application.class, "attach", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                Context context = (Context) p.args[0];
                logFile = new File(context.getCacheDir(), "oc_fresh.log");
                log("ATTACH package=" + context.getPackageName() + " process=" + lp.processName);
            }
        });
        hookFailures();
        hookNativeLoads();
        hookCamera(lp.classLoader);
        hookRecorder();
        hookAps(lp.classLoader);
    }

    private void hookFailures() {
        hook(dalvik.system.BaseDexClassLoader.class, "findClass", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (!p.hasThrowable() || p.args.length == 0) return;
                String name = String.valueOf(p.args[0]);
                if (name.startsWith("android.") || name.startsWith("com.oplus.")
                        || name.startsWith("com.color.") || name.startsWith("com.heytap.")
                        || name.startsWith("com.oppo.")) {
                    if (seen.size() < 500 && seen.add("class:" + name))
                        log("CLASS_LOOKUP_MISS name=" + name + " error=" + p.getThrowable());
                }
            }
        });
        hook(Thread.class, "dispatchUncaughtException", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (p.args.length > 0 && p.args[0] instanceof Throwable)
                    log("UNCAUGHT " + Log.getStackTraceString((Throwable) p.args[0]));
            }
        });
    }

    private void hookNativeLoads() {
        XC_MethodHook load = new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                log("NATIVE_LOAD " + p.method.getName() + " " + render(p.args));
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.hasThrowable()) log("NATIVE_LOAD_FAIL " + p.method.getName() + " "
                        + render(p.args) + " " + p.getThrowable());
            }
        };
        hook(Runtime.class, "loadLibrary0", load);
        hook(Runtime.class, "load0", load);
    }

    private void hookCamera(ClassLoader loader) {
        Class<?> manager = find("android.hardware.camera2.CameraManager", loader);
        if (manager != null) {
            hook(manager, "openCamera", trace("OPEN_CAMERA"));
            hook(manager, "getCameraCharacteristics", new XC_MethodHook() {
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (p.hasThrowable() || !(p.getResult() instanceof CameraCharacteristics)) return;
                    String id = String.valueOf(p.args[0]);
                    if (!seen.add("characteristics:" + id)) return;
                    CameraCharacteristics c = (CameraCharacteristics) p.getResult();
                    log("CHARACTERISTICS id=" + id
                            + " capabilities=" + render(c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES))
                            + " aeRanges=" + render(c.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES))
                            + " activeArray=" + c.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE)
                            + " physicalIds=" + c.getPhysicalCameraIds());
                }
            });
        }
        Class<?> device = find("android.hardware.camera2.impl.CameraDeviceImpl", loader);
        if (device != null) {
            for (String method : new String[]{"createCaptureSession", "createCaptureSessionInternal",
                    "createCustomCaptureSession", "createConstrainedHighSpeedCaptureSession",
                    "createReprocessableCaptureSession", "createCaptureSessionByOutputConfigurations"}) {
                hook(device, method, new XC_MethodHook() {
                    @Override protected void beforeHookedMethod(MethodHookParam p) {
                        log("SESSION " + p.method.getName() + " args=" + render(p.args) + " mode=" + mode);
                        for (Object argument : p.args) {
                            if (argument instanceof SessionConfiguration) {
                                SessionConfiguration s = (SessionConfiguration) argument;
                                log("SESSION_CONFIG type=" + s.getSessionType() + " outputs="
                                        + s.getOutputConfigurations() + " parameters=" + s.getSessionParameters());
                                CaptureRequest parameters = s.getSessionParameters();
                                if (parameters != null) {
                                    StringBuilder values = new StringBuilder("SESSION_PARAMETERS");
                                    for (CaptureRequest.Key<?> key : parameters.getKeys()) {
                                        if (key.getName().contains("Fps") || key.getName().contains("fps")
                                                || key.getName().contains("sensorMode") || key.getName().contains("operationMode")
                                                || key.getName().contains("captureIntent") || key.getName().contains("feature"))
                                            values.append(' ').append(key.getName()).append('=').append(render(parameters.get(key)));
                                    }
                                    log(values.toString());
                                }
                            }
                        }
                    }
                    @Override protected void afterHookedMethod(MethodHookParam p) {
                        if (p.hasThrowable()) log("SESSION_THROW " + p.getThrowable());
                    }
                });
            }
        }
        hook(CaptureRequest.Builder.class, "set", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (p.args.length != 2 || !(p.args[0] instanceof CaptureRequest.Key)) return;
                String key = ((CaptureRequest.Key<?>) p.args[0]).getName();
                if (!(key.contains("aeTargetFpsRange") || key.contains("cropRegion")
                        || key.contains("captureIntent") || key.contains("sensor.frameDuration")
                        || key.contains("aps") || key.contains("featureType")
                        || key.contains("manualWB") || key.endsWith(".AWBCCT")
                        || key.equals("android.control.awbMode")
                        || key.contains("operationMode") || key.contains("sensorMode"))) return;
                String value = render(p.args[1]);
                String old = previous.put(mode + ":" + key, value);
                if (!value.equals(old)) log("REQUEST mode=" + mode + " " + key + "=" + value);
            }
        });
        hook(android.media.ImageReader.class, "newInstance", trace("IMAGE_READER"));
        Class<?> callback = find("com.oppo.camera.f.g$18", loader);
        if (callback != null) hook(callback, "onCaptureCompleted", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                for (Object argument : p.args) {
                    if (!(argument instanceof TotalCaptureResult)) continue;
                    TotalCaptureResult result = (TotalCaptureResult) argument;
                    Long timestamp = result.get(CaptureResult.SENSOR_TIMESTAMP);
                    if (timestamp == null) return;
                    synchronized (ObserveHook.class) {
                        long delta = timestamp - lastTimestamp;
                        int session = System.identityHashCode(p.args[0]);
                        if (delta <= 0 || delta > 1_000_000_000L || frameSession != session) {
                            frames = 0;
                            firstTimestamp = timestamp;
                            firstFrameNumber = result.getFrameNumber();
                            lateFrames = 0;
                            frameSession = session;
                        }
                        lastTimestamp = timestamp;
                        if (frames > 0 && delta > 25_000_000L) ++lateFrames;
                        if (++frames % 120 == 0)
                            log("FRAME mode=" + mode + " session=" + p.args[0].getClass().getName()
                                    + " number=" + result.getFrameNumber() + " deltaNs=" + delta
                                    + " durationNs=" + result.get(CaptureResult.SENSOR_FRAME_DURATION)
                                    + " ae=" + result.get(CaptureResult.CONTROL_AE_TARGET_FPS_RANGE)
                                    + " deliveredFps=" + ((frames - 1) * 1_000_000_000.0 / (timestamp - firstTimestamp))
                                    + " callbacks=" + frames + " frameNumberSpan=" + (result.getFrameNumber() - firstFrameNumber)
                                    + " intervalsOver25ms=" + lateFrames);
                    }
                }
            }
        });
    }

    private void hookRecorder() {
        for (String method : new String[]{"setProfile", "setVideoSize", "setVideoFrameRate",
                "setCaptureRate", "prepare", "start", "stop"}) {
            hook(android.media.MediaRecorder.class, method, trace("RECORDER"));
        }
    }

    private void hookAps(ClassLoader loader) {
        Class<?> aps = find("com.aps.APSClient", loader);
        // Lowest priority: after callback runs before normal-priority after hooks.
        // This observes the native decision even if a different module later rewrites it.
        if (aps != null) hook(aps, "previewDecision", new XC_MethodHook(XC_MethodHook.PRIORITY_LOWEST) {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.hasThrowable()) { log("APS_THROW " + p.getThrowable()); return; }
                Object decision = p.getResult();
                if (decision == null) return;
                // previewDecision also has a native String-returning overload.
                // Observe the decoded decision object once, not both layers per frame.
                if (!decision.getClass().getName().endsWith("$DecisionResult")) return;
                StringBuilder text = new StringBuilder("APS_DECISION mode=" + mode);
                for (String field : new String[]{"mApsAlgoFlag", "mApsDecisionFeatureType",
                        "mApsDecisionSceneMode", "mSuperNightScene", "mMultiFrameCount",
                        "mRequestFormat", "mApsBracketMode", "mCaptureEVList"}) {
                    try { text.append(' ').append(field).append('=').append(render(XposedHelpers.getObjectField(decision, field))); }
                    catch (Throwable ignored) { text.append(' ').append(field).append("=<unavailable>"); }
                }
                String value = text.toString();
                if (!value.equals(previous.put("decision", value))) log(value);
            }
        });
        Class<?> modes = find("com.oppo.camera.e.a.a", loader);
        if (modes != null) hook(modes, "c", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (p.args.length == 1 && p.args[0] instanceof String) {
                    mode = (String) p.args[0];
                    log("MODE " + mode);
                }
            }
        });
    }

    private static XC_MethodHook trace(String label) {
        return new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if ("OPEN_CAMERA".equals(label) && p.args.length > 0 && p.args[0] instanceof String)
                    cameraId = (String) p.args[0];
                log(label + " " + p.method.getName() + " args=" + render(p.args) + " mode=" + mode);
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.hasThrowable()) log(label + "_THROW " + p.getThrowable());
            }
        };
    }

    private static Class<?> find(String name, ClassLoader loader) {
        try { return Class.forName(name, false, loader); }
        catch (Throwable t) { log("OPTIONAL_TARGET_UNAVAILABLE " + name + " " + t); return null; }
    }

    private static void hook(Class<?> owner, String method, XC_MethodHook callback) {
        try {
            int count = XposedBridge.hookAllMethods(owner, method, callback).size();
            log("HOOK " + owner.getName() + "." + method + " overloads=" + count);
        } catch (Throwable t) { log("HOOK_UNAVAILABLE " + owner.getName() + "." + method + " " + t); }
    }

    private static String render(Object value) {
        if (value == null) return "null";
        if (value instanceof byte[]) return "byte[" + ((byte[]) value).length + "]";
        if (value instanceof CamcorderProfile) {
            CamcorderProfile p = (CamcorderProfile) value;
            return p.videoFrameWidth + "x" + p.videoFrameHeight + " fps=" + p.videoFrameRate
                    + " quality=" + p.quality + " audioCodec=" + p.audioCodec;
        }
        if (value.getClass().isArray()) {
            int length = Array.getLength(value);
            StringBuilder s = new StringBuilder("[");
            for (int i = 0; i < Math.min(length, 12); i++) {
                if (i != 0) s.append(',');
                s.append(render(Array.get(value, i)));
            }
            if (length > 12) s.append(",...");
            return s.append(']').toString();
        }
        String text = String.valueOf(value);
        return text.length() > 800 ? text.substring(0, 800) + "..." : text;
    }

    private static synchronized void log(String message) {
        if (events.incrementAndGet() > 5000) return;
        String text = System.currentTimeMillis() + " pid=" + Process.myPid() + " " + message;
        Log.i(TAG, text);
        File file = logFile;
        if (file == null || file.length() > 2_000_000L) return;
        try (FileOutputStream stream = new FileOutputStream(file, true)) {
            stream.write((text + "\n").getBytes(java.nio.charset.StandardCharsets.UTF_8));
        } catch (Throwable ignored) { /* Never interfere with the camera for logging. */ }
    }
}
