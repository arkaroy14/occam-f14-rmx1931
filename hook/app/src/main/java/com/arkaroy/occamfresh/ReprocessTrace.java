package com.arkaroy.occamfresh;

import android.graphics.Rect;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.CaptureResult;
import android.media.Image;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.Collection;
import java.util.List;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Opt-in observation of the original processed-RAW -> camera reprocess path. */
final class ReprocessTrace {
    private static final AtomicInteger rawReports = new AtomicInteger();
    private static void observeRawSites(Image image, Image.Plane plane) {
        // Match the native observer's exact4096 sample positions. The duplicate
        // is read-only; the caller's buffer position/limit and pixels survive.
        if (image.getFormat()!=32 || image.getWidth()!=4608 || image.getHeight()!=3456
                || plane.getPixelStride()!=2 || plane.getRowStride()!=9216
                || rawReports.getAndIncrement()>=3) return;
        ByteBuffer original=plane.getBuffer();
        ByteBuffer samples=original.asReadOnlyBuffer().order(ByteOrder.LITTLE_ENDIAN);
        samples.clear();
        if(samples.capacity()!=4608*3456*2)return;
        long[] sums=new long[4]; int[] minimum={65535,65535,65535,65535},maximum=new int[4];
        long signature=0xcbf29ce484222325L;
        for(int y=0;y<32;++y)for(int x=0;x<32;++x){
            int row=((y*(3456-2)/31)&~1),col=((x*(4608-2)/31)&~1);
            for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
                int site=2*dy+dx,value=samples.getShort(((row+dy)*4608+col+dx)*2)&65535;
                sums[site]+=value;minimum[site]=Math.min(minimum[site],value);maximum[site]=Math.max(maximum[site],value);
                signature=(signature^(value&255))*0x100000001b3L;
                signature=(signature^(value>>>8))*0x100000001b3L;
            }
        }
        Log.i("OcFresh","REPROCESS_RAW_SITES sums="+java.util.Arrays.toString(sums)
                +" min="+java.util.Arrays.toString(minimum)+" max="+java.util.Arrays.toString(maximum)
                +" sampledFNV="+Long.toUnsignedString(signature,16)+" samplesPerSite=1024"
                +" originalPosition="+original.position()+" originalLimit="+original.limit());
        // Finite diagnostic on at most3 actual reprocess images. A full-range
        // count distinguishes10-bit from an expanded SDK output without
        // presuming that the dimensions/RAW_SENSOR enum establish its scale.
        int fullMin=65535,fullMax=0;long over10=0,over12=0,over14=0;
        for(int offset=0;offset<samples.capacity();offset+=2){
            int value=samples.getShort(offset)&65535;
            fullMin=Math.min(fullMin,value);fullMax=Math.max(fullMax,value);
            if(value>1023)++over10;if(value>4095)++over12;if(value>16383)++over14;
        }
        Log.i("OcFresh","REPROCESS_RAW_FULL_RANGE min="+fullMin+" max="+fullMax
                +" over10="+over10+" over12="+over12+" over14="+over14
                +" pixels="+(samples.capacity()/2)+" read_only_no_dump");
    }
    static void install(ClassLoader loader) throws ClassNotFoundException {
        Class<?> camera = Class.forName("com.oppo.camera.f.g", false, loader);
        Class<?> tag = Class.forName("com.oppo.camera.f.d", false, loader);
        XposedHelpers.findAndHookMethod(camera, "a", Image.class, TotalCaptureResult.class,
                Rect.class, tag, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                try {
                    Image image = (Image) p.args[0];
                    CameraCaptureSession session = (CameraCaptureSession) XposedHelpers.getObjectField(p.thisObject, "A");
                    Log.i("OcFresh", "REPROCESS_IMAGE_INPUT role=" + XposedHelpers.getIntField(p.thisObject, "t")
                            + " state=" + XposedHelpers.getIntField(p.thisObject, "r")
                            + " sessionReprocessable=" + (session != null && session.isReprocessable())
                            + " image=" + (image == null ? "null" : image.getWidth() + "x" + image.getHeight()
                            + " format=" + image.getFormat()) + " crop=" + p.args[2]);
                    if(image!=null&&image.getPlanes().length>0){
                        Image.Plane plane=image.getPlanes()[0];
                        Log.i("OcFresh","REPROCESS_RAW_LAYOUT rowStride="+plane.getRowStride()
                                +" pixelStride="+plane.getPixelStride()+" capacity="+plane.getBuffer().capacity());
                        observeRawSites(image,plane);
                    }
                    TotalCaptureResult source = (TotalCaptureResult) p.args[1];
                    if (source != null) {
                        Log.i("OcFresh", "REPROCESS_SOURCE_SENSOR camera=" + source.getCameraId()
                                + " frame=" + source.getFrameNumber()
                                + " exposureNs=" + source.get(CaptureResult.SENSOR_EXPOSURE_TIME)
                                + " ISO=" + source.get(CaptureResult.SENSOR_SENSITIVITY)
                                + " black=" + java.util.Arrays.toString(source.get(CaptureResult.SENSOR_DYNAMIC_BLACK_LEVEL))
                                + " white=" + source.get(CaptureResult.SENSOR_DYNAMIC_WHITE_LEVEL)
                                + " gains=" + source.get(CaptureResult.COLOR_CORRECTION_GAINS)
                                + " transform=" + source.get(CaptureResult.COLOR_CORRECTION_TRANSFORM)
                                + " neutral=" + java.util.Arrays.toString(source.get(CaptureResult.SENSOR_NEUTRAL_COLOR_POINT))
                                + " awbMode=" + source.get(CaptureResult.CONTROL_AWB_MODE)
                                + " awbState=" + source.get(CaptureResult.CONTROL_AWB_STATE)
                                + " postRawBoost=" + source.get(CaptureResult.CONTROL_POST_RAW_SENSITIVITY_BOOST));
                    }
                } catch (Throwable unavailable) {
                    Log.i("OcFresh", "REPROCESS_TRACE_UNAVAILABLE " + unavailable);
                }
            }
        });
        Class<?> device = Class.forName("android.hardware.camera2.impl.CameraDeviceImpl", false, loader);
        XposedBridge.hookAllMethods(device, "createReprocessCaptureRequest", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                Log.i("OcFresh", "REPROCESS_REQUEST_CREATE error=" + p.getThrowable());
            }
        });
        Class<?> session = Class.forName("android.hardware.camera2.impl.CameraCaptureSessionImpl", false, loader);
        for (String method : new String[]{"capture", "captureSingleRequest", "captureBurst", "captureBurstRequests"}) {
            XposedBridge.hookAllMethods(session, method, new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) {
                    try {
                        Object argument = p.args.length == 0 ? null : p.args[0];
                        if (argument instanceof List && !((List<?>) argument).isEmpty()) argument = ((List<?>) argument).get(0);
                        if (!(argument instanceof CaptureRequest)) return;
                        CaptureRequest request = (CaptureRequest) argument;
                        boolean reprocess = (Boolean) XposedHelpers.callMethod(request, "isReprocess");
                        if (reprocess) {
                            Collection<?> targets = (Collection<?>) XposedHelpers.callMethod(request, "getTargets");
                            Log.i("OcFresh", "REPROCESS_SUBMIT sessionReprocessable="
                                    + ((CameraCaptureSession) p.thisObject).isReprocessable()
                                    + " requestReprocess=true targets=" + targets.size()
                                    + " exposureNs=" + request.get(CaptureRequest.SENSOR_EXPOSURE_TIME)
                                    + " durationNs=" + request.get(CaptureRequest.SENSOR_FRAME_DURATION)
                                    + " sensitivity=" + request.get(CaptureRequest.SENSOR_SENSITIVITY)
                                    + " aeMode=" + request.get(CaptureRequest.CONTROL_AE_MODE)
                                    + " fps=" + request.get(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE)
                                    + " crop=" + request.get(CaptureRequest.SCALER_CROP_REGION));
                            for (CaptureRequest.Key<?> key : request.getKeys()) {
                                String name = key.getName();
                                if (name.contains("supernight") || name.contains("reprocess")
                                        || name.contains("ainight") || name.contains("rawhdr")
                                        || name.equals("android.control.postRawSensitivityBoost")
                                        || name.contains("snapshot") || name.contains("crop")
                                        || name.startsWith("android.colorCorrection.")
                                        || name.startsWith("android.blackLevel.")
                                        || name.equals("android.sensor.dynamicBlackLevel")
                                        || name.equals("android.sensor.dynamicWhiteLevel")
                                        || name.equals("android.sensor.neutralColorPoint")
                                        || name.startsWith("android.control.awb")) {
                                    Object value = request.get(key);
                                    String rendered = value instanceof int[] ? java.util.Arrays.toString((int[]) value)
                                            : value instanceof float[] ? java.util.Arrays.toString((float[]) value)
                                            : value instanceof byte[] ? java.util.Arrays.toString((byte[]) value)
                                            : String.valueOf(value);
                                    Log.i("OcFresh", "REPROCESS_METADATA " + name + "=" + rendered);
                                }
                            }
                        }
                    } catch (Throwable unavailable) { /* Observation cannot alter submission. */ }
                }
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (p.hasThrowable()) Log.e("OcFresh", "REPROCESS_SUBMIT_THROW " + p.method.getName(), p.getThrowable());
                }
            });
        }
        Log.i("OcFresh", "REPROCESS_TRACE_ACTIVE original images, requests, errors and session flags retained");
    }
}
