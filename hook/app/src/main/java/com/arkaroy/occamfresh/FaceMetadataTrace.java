package com.arkaroy.occamfresh;

import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.Face;
import android.util.Log;
import java.util.concurrent.atomic.AtomicInteger;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;

/** Opt-in face metadata observation, with no rectangles or face counts fabricated. */
final class FaceMetadataTrace {
    private static final AtomicInteger frames = new AtomicInteger();
    static void install(ClassLoader loader) throws Exception {
        Class<?> callback = Class.forName("com.oppo.camera.f.g$18", false, loader);
        XposedBridge.hookAllMethods(callback, "onCaptureCompleted", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.hasThrowable() || !"portrait".equals(ObserveHook.currentMode())) return;
                int frame = frames.getAndIncrement();
                if (frame % 90 != 0 || frame >= 90 * 40) return;
                try {
                    TotalCaptureResult result = null;
                    CaptureRequest request = null;
                    String id = "unknown";
                    for (Object a : p.args) {
                        if (a instanceof TotalCaptureResult) result = (TotalCaptureResult) a;
                        else if (a instanceof CaptureRequest) request = (CaptureRequest) a;
                        else if (a instanceof CameraCaptureSession) id = ((CameraCaptureSession) a).getDevice().getId();
                    }
                    if (result == null) return;
                    Face[] faces = result.get(CaptureResult.STATISTICS_FACES);
                    byte[] vendor = null;
                    for (CaptureResult.Key<?> key : result.getKeys()) {
                        if ("org.quic.camera2.oemfdresults.OEMFDResults".equals(key.getName())) {
                            Object value = result.get(key);
                            if (value instanceof byte[]) vendor = (byte[]) value;
                            break;
                        }
                    }
                    StringBuilder words = new StringBuilder();
                    if (vendor != null) for (int offset = 0; offset + 4 <= Math.min(32, vendor.length); offset += 4) {
                        long v = 0;
                        for (int i = 0; i < 4; ++i) v |= (long) (vendor[offset+i] & 255) << (8*i);
                        words.append(offset == 0 ? "" : ",").append(v);
                    }
                    StringBuilder scores = new StringBuilder();
                    if (faces != null) for (int i = 0; i < Math.min(5, faces.length); ++i) {
                        if (i != 0) scores.append(',');
                        scores.append(faces[i].getScore()).append('@').append(faces[i].getBounds());
                    }
                    Log.i("OcFreshFace", "PORTRAIT_FACE_RESULT camera=" + id
                            + " requestFD=" + (request == null ? null : request.get(CaptureRequest.STATISTICS_FACE_DETECT_MODE))
                            + " resultFD=" + result.get(CaptureResult.STATISTICS_FACE_DETECT_MODE)
                            + " androidFaces=" + (faces == null ? -1 : faces.length)
                            + " OEMbytes=" + (vendor == null ? -1 : vendor.length)
                            + " OEMfirstWords=[" + words + "]"
                            + " scoresAndBounds=[" + scores + "]"
                            + " afState=" + result.get(CaptureResult.CONTROL_AF_STATE)
                            + " afMode=" + result.get(CaptureResult.CONTROL_AF_MODE)
                            + " focusDiopters=" + result.get(CaptureResult.LENS_FOCUS_DISTANCE)
                            + " focalMm=" + result.get(CaptureResult.LENS_FOCAL_LENGTH)
                            + " aperture=" + result.get(CaptureResult.LENS_APERTURE)
                            + " crop=" + result.get(CaptureResult.SCALER_CROP_REGION)
                            + " physicalResultIds=" + result.getPhysicalCameraResults().keySet());
                } catch (Throwable unavailable) {
                    Log.i("OcFreshFace", "PORTRAIT_FACE_OBSERVE_ERROR " + unavailable);
                }
            }
        });
        Log.i("OcFreshFace", "PORTRAIT_FACE_OBSERVER_ACTIVE bounded40; original metadata unchanged");
    }
}
