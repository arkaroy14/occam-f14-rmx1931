package com.arkaroy.occamfresh;

import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.TotalCaptureResult;
import android.media.Image;
import android.media.ImageWriter;
import android.util.Log;
import android.os.SystemClock;
import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Signal-trial only. Observe each original Image before APS takes it.
 * No Image/ByteBuffer/pointer retained and no close/detach/refcount changes. */
final class NightBufferTrace {
    private static final AtomicInteger calls = new AtomicInteger();
    private static final AtomicInteger queues = new AtomicInteger();
    private static volatile long reprocessTimestamp = -1, darkProcessAt = -1;
    private static final AtomicLong seenReprocess = new AtomicLong(-1);
    private static String value(String[] pairs,String key){
        for(int i=0;i+1<pairs.length;i+=2)if(key.equals(pairs[i]))return pairs[i+1];
        return null;
    }
    static void install(ClassLoader loader) throws ClassNotFoundException {
        Class<?> client = Class.forName("com.aps.APSClient", false, loader);
        XposedBridge.hookAllMethods(client, "addFrameBuff", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (p.args.length != 1 || calls.get() >= 20
                        || !"night".equals(ObserveHook.currentMode())) return;
                try {
                    Object param = p.args[0];
                    if (param == null || !param.getClass().getName().equals(
                            "com.oppo.camera.aps.adapter.ApsCaptureParam")) return;
                    Object buffer = XposedHelpers.getObjectField(param, "mImageBuffer");
                    Object metadata = XposedHelpers.getObjectField(param, "mLogicMeta");
                    if (!(metadata instanceof TotalCaptureResult)) return;
                    TotalCaptureResult result = (TotalCaptureResult) metadata;
                    if (!"7".equals(result.getCameraId())) return;
                    Image image = (Image) XposedHelpers.getObjectField(buffer, "mImage");
                    if (image == null || image.getFormat() != 32 || image.getWidth() != 4608
                            || image.getHeight() != 3456) return;
                    Long timestamp = result.get(CaptureResult.SENSOR_TIMESTAMP);
                    if (timestamp == null || timestamp.longValue() != image.getTimestamp()) return;
                    Image.Plane[] planes = image.getPlanes();
                    if (planes.length != 1 || planes[0].getRowStride() != 9216
                            || planes[0].getPixelStride() != 2) return;
                    ByteBuffer view = planes[0].getBuffer().asReadOnlyBuffer();
                    if (!view.isDirect() || view.capacity() != 31850496) return;
                    int call = calls.getAndIncrement();
                    if (call >= 20) return;
                    if(XposedHelpers.getBooleanField(param,"mbReprocessFrame"))reprocessTimestamp=timestamp;
                    view.clear(); // This is an independent view, never the original buffer.
                    p.setObjectExtra("night_buffer_call", call);
                    Log.i("OcFresh", "NIGHT_APS_ENTRY call=" + call
                            + " frame=" + result.getFrameNumber() + " timestamp=" + timestamp
                            + " iso=" + result.get(CaptureResult.SENSOR_SENSITIVITY)
                            + " exposureNs=" + result.get(CaptureResult.SENSOR_EXPOSURE_TIME)
                            + " ev=" + result.get(CaptureResult.CONTROL_AE_EXPOSURE_COMPENSATION)
                            + " black=" + java.util.Arrays.toString(result.get(CaptureResult.SENSOR_DYNAMIC_BLACK_LEVEL))
                            + " reprocess=" + XposedHelpers.getBooleanField(param, "mbReprocessFrame")
                            + " maxHold=" + XposedHelpers.getIntField(param, "mMaxHoldImages")
                            + " prefer=" + XposedHelpers.getIntField(param, "mPreferType"));
                    NativePaths.observeNightApsBuffer(view, call, result.getFrameNumber(), timestamp);
                } catch (Throwable error) {
                    Log.i("OcFresh", "NIGHT_APS_UNAVAILABLE " + error.getClass().getSimpleName());
                }
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                Object call = p.getObjectExtra("night_buffer_call");
                if (call != null) Log.i("OcFresh", "NIGHT_APS_RETURN call=" + call
                        + " original=" + p.getResult() + " throwable=" + p.hasThrowable());
            }
        });
        XposedBridge.hookAllMethods(client,"processImages",new XC_MethodHook(){
            @Override protected void beforeHookedMethod(MethodHookParam p){
                for(Object arg:p.args)if(arg instanceof String[]){
                    String[] pairs=(String[])arg;String feature=value(pairs,"capture_feature_type");
                    if(feature==null)continue;
                    darkProcessAt="13".equals(feature)&&"5".equals(value(pairs,"super_night_scene"))
                            &&"7".equals(value(pairs,"logic_camera_id"))
                            ?SystemClock.elapsedRealtime():-1;
                }
            }
        });
        Class<?> device=Class.forName("android.hardware.camera2.impl.CameraDeviceImpl",false,loader);
        XposedBridge.hookAllMethods(device,"createReprocessCaptureRequest",new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam p){
                if(darkProcessAt<0||SystemClock.elapsedRealtime()-darkProcessAt>30000
                        ||!"night".equals(ObserveHook.currentMode())||p.hasThrowable()
                        ||p.args.length!=1||!(p.args[0] instanceof TotalCaptureResult)
                        ||!(p.getResult() instanceof CaptureRequest.Builder))return;
                TotalCaptureResult source=(TotalCaptureResult)p.args[0];
                Long timestamp=source.get(CaptureResult.SENSOR_TIMESTAMP);
                if(!"7".equals(source.getCameraId())||timestamp==null
                        ||timestamp.longValue()!=reprocessTimestamp
                        ||seenReprocess.getAndSet(timestamp)==timestamp.longValue())return;
                Log.i("OcFresh","DARK_REPROCESS_SOURCE frame="+source.getFrameNumber()+" timestamp="+timestamp
                        +" original_result original_builder no_substitution");
                NightScalarTrace.created(source,source,(CaptureRequest.Builder)p.getResult());
            }
        });
        XposedHelpers.findAndHookMethod(ImageWriter.class,"queueInputImage",Image.class,new XC_MethodHook(){
            @Override protected void beforeHookedMethod(MethodHookParam p){
                if(queues.get()>=2 || !"night".equals(ObserveHook.currentMode())
                        || !"7".equals(ObserveHook.currentCameraId()) || !(p.args[0] instanceof Image))return;
                try{
                    Image image=(Image)p.args[0];
                    if(image.getFormat()!=32||image.getWidth()!=4608||image.getHeight()!=3456)return;
                    Image.Plane[] planes=image.getPlanes();
                    if(planes.length!=1||planes[0].getRowStride()!=9216||planes[0].getPixelStride()!=2)return;
                    ByteBuffer view=planes[0].getBuffer().asReadOnlyBuffer();
                    if(!view.isDirect()||view.capacity()!=31850496)return;
                    int call=queues.getAndIncrement();if(call>=2)return;
                    view.clear();
                    NativePaths.observeNightQueuedBuffer(view,call,image.getTimestamp());
                }catch(Throwable error){Log.i("OcFresh","NIGHT_QUEUE_UNAVAILABLE "+error.getClass().getSimpleName());}
            }
        });
        Log.i("OcFresh", "NIGHT_APS_TRACE_READY max20 original_Image_lifetime no_retention_no_copy");
    }
}
