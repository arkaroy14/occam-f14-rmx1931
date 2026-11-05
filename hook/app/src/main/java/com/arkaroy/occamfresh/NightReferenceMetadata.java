package com.arkaroy.occamfresh;

import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.media.ImageWriter;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.TotalCaptureResult;
import android.media.Image;
import android.os.SystemClock;
import android.util.Log;
import java.util.ArrayList;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Opt-in original V3 EV0 RAW/reference-result pairing; no synthetic metadata. */
final class NightReferenceMetadata {
    private static final Object LOCK = new Object();
    private static final TreeMap<Long, RawFrame> RAW = new TreeMap<>();
    private static Plan pending;
    private static final Pattern INTEGER = Pattern.compile("-?\\d+");
    private static final class RawFrame {
        final long timestamp;
        final TotalCaptureResult result;
        RawFrame(long timestamp, TotalCaptureResult result) { this.timestamp=timestamp; this.result=result; }
    }
    private static final class Plan {
        final RawFrame first, reference;
        final long lastTimestamp, created;
        final int index;
        Plan(RawFrame first, RawFrame reference, long lastTimestamp, int index) {
            this.first=first; this.reference=reference; this.lastTimestamp=lastTimestamp; this.index=index;
            this.created=SystemClock.elapsedRealtime();
        }
    }
    private static boolean rearNight() {
        return "night".equals(ObserveHook.currentMode()) && "7".equals(ObserveHook.currentCameraId());
    }
    private static String value(String[] pairs,String key) {
        for(int i=0;i+1<pairs.length;i+=2) if(key.equals(pairs[i]))return pairs[i+1];
        return null;
    }
    private static void cache(Object capture) throws Throwable {
        if(!rearNight() || capture==null || !capture.getClass().getName().equals(
                "com.oppo.camera.aps.adapter.ApsCaptureParam"))return;
        Object buffer=XposedHelpers.getObjectField(capture,"mImageBuffer");
        Image image=(Image)XposedHelpers.getObjectField(buffer,"mImage");
        Object metadata=XposedHelpers.getObjectField(capture,"mLogicMeta");
        if(image==null || image.getFormat()!=ImageFormat.RAW_SENSOR || image.getWidth()!=4608
                || image.getHeight()!=3456 || !(metadata instanceof TotalCaptureResult))return;
        TotalCaptureResult result=(TotalCaptureResult)metadata;
        Long timestamp=result.get(CaptureResult.SENSOR_TIMESTAMP);
        if(!"7".equals(result.getCameraId()) || timestamp==null || timestamp!=image.getTimestamp())return;
        synchronized(LOCK) {
            RAW.put(timestamp,new RawFrame(timestamp,result));
            while(RAW.size()>48)RAW.pollFirstEntry();
        }
    }
    private static void prepare(Object[] arguments) {
        if(!rearNight())return;
        for(Object argument:arguments) {
            if(!(argument instanceof String[]))continue;
            String[] pairs=(String[])argument;
            if(!"10".equals(value(pairs,"capture_feature_type"))
                    || !"2".equals(value(pairs,"super_night_scene"))
                    || !"7".equals(value(pairs,"logic_camera_id"))
                    || !"0".equals(value(pairs,"camera_id"))
                    || !"32793".equals(value(pairs,"operation_mode")))continue;
            String encoded=value(pairs,"input_ev_list:");
            if(encoded==null)encoded=value(pairs,"input_ev_list");
            String identity=value(pairs,"identity");
            if(encoded==null||identity==null)continue;
            ArrayList<Integer> ev=new ArrayList<>();Matcher matcher=INTEGER.matcher(encoded);
            while(matcher.find()&&ev.size()<20)ev.add(Integer.parseInt(matcher.group()));
            if(ev.size()<10 || ev.get(0)>=0)continue;
            int reference=-1;
            for(int i=0;i<10;i++)if(ev.get(i)==0){reference=i;break;}
            if(reference<1)continue;
            long root=Long.parseLong(identity);
            synchronized(LOCK) {
                ArrayList<RawFrame> batch=new ArrayList<>(RAW.tailMap(root,true).values());
                if(batch.size()!=10 || batch.get(0).timestamp!=root)continue;
                long firstFrame=batch.get(0).result.getFrameNumber();boolean contiguous=true;
                for(int i=0;i<10;i++)if(batch.get(i).result.getFrameNumber()!=firstFrame+i)contiguous=false;
                if(!contiguous)continue;
                pending=new Plan(batch.get(0),batch.get(reference),batch.get(9).timestamp,reference);
                Log.i("OcFresh","NIGHT_REFERENCE_PLAN root="+root+" count=10 refIndex="+reference
                        +" firstFrame="+firstFrame+" refFrame="+pending.reference.result.getFrameNumber()
                        +" originalEV="+ev.subList(0,10));
            }
        }
    }
    private static void installImagePairTrace(ClassLoader loader) throws ClassNotFoundException {
        final ThreadLocal<String> queueContext=new ThreadLocal<>();
        final java.util.concurrent.atomic.AtomicInteger budget=new java.util.concurrent.atomic.AtomicInteger();
        Class<?> camera=Class.forName("com.oppo.camera.f.g",false,loader);
        Class<?> tag=Class.forName("com.oppo.camera.f.d",false,loader);
        XposedHelpers.findAndHookMethod(camera,"a",Image.class,TotalCaptureResult.class,Rect.class,tag,new XC_MethodHook(){
            @Override protected void beforeHookedMethod(MethodHookParam p){
                try{
                    if(!"night".equals(ObserveHook.currentMode()) || !(p.args[0] instanceof Image)
                            || !(p.args[1] instanceof TotalCaptureResult))return;
                    Image image=(Image)p.args[0];TotalCaptureResult source=(TotalCaptureResult)p.args[1];
                    if(image.getFormat()!=32||image.getWidth()!=4608||image.getHeight()!=3456
                            ||!"7".equals(source.getCameraId())||budget.getAndIncrement()>=2)return;
                    Plan plan;synchronized(LOCK){plan=pending;}
                    String context="sourceFrame="+source.getFrameNumber()+" sourceTimestamp="+source.get(CaptureResult.SENSOR_TIMESTAMP)
                            +(plan==null?" plan=missing":" firstFrame="+plan.first.result.getFrameNumber()
                            +" firstTimestamp="+plan.first.timestamp+" refIndex="+plan.index
                            +" refFrame="+plan.reference.result.getFrameNumber()+" refTimestamp="+plan.reference.timestamp);
                    p.setObjectExtra("night_pair_active",true);p.setObjectExtra("night_pair_previous",queueContext.get());
                    queueContext.set(context);
                    Log.i("OcNightPair","ENTRY imageTimestamp="+image.getTimestamp()+" width="+image.getWidth()
                            +" height="+image.getHeight()+" format="+image.getFormat()+" "+context);
                }catch(Throwable unavailable){Log.i("OcNightPair","ENTRY_UNAVAILABLE "+unavailable);}
            }
            @Override protected void afterHookedMethod(MethodHookParam p){
                if(Boolean.TRUE.equals(p.getObjectExtra("night_pair_active")))
                    queueContext.set((String)p.getObjectExtra("night_pair_previous"));
            }
        });
        XposedHelpers.findAndHookMethod(ImageWriter.class,"queueInputImage",Image.class,new XC_MethodHook(){
            @Override protected void beforeHookedMethod(MethodHookParam p){
                String context=queueContext.get();if(context==null)return;
                try{Image image=(Image)p.args[0];Log.i("OcNightPair","QUEUE imageTimestamp="+image.getTimestamp()
                        +" width="+image.getWidth()+" height="+image.getHeight()+" format="+image.getFormat()+" "+context);}
                catch(Throwable unavailable){Log.i("OcNightPair","QUEUE_UNAVAILABLE "+unavailable);}
            }
        });
        Log.i("OcNightPair","READY max2 standardNight scalar timestamp/frame observations; pixels and args untouched");
    }
    static void install(ClassLoader loader, boolean imagePairTrace) throws ClassNotFoundException {
        if(imagePairTrace)installImagePairTrace(loader);
        Class<?> client=Class.forName("com.aps.APSClient",false,loader);
        XposedBridge.hookAllMethods(client,"addFrameBuff",new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                try { for(Object argument:p.args)cache(argument); }
                catch(Throwable unavailable){Log.i("OcFresh","NIGHT_REFERENCE_CACHE_UNAVAILABLE "+unavailable);}
            }
        });
        XposedBridge.hookAllMethods(client,"processImages",new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                try { prepare(p.args); }
                catch(Throwable unavailable){Log.i("OcFresh","NIGHT_REFERENCE_PLAN_UNAVAILABLE "+unavailable);}
            }
        });
        // Pair at the stable Camera2 consumer boundary. Plan preparation is
        // already camera7/RAW/geometry/mode/burst gated; the actual source
        // camera and snapshot identity are stronger than a last-opened UI id.
        Class<?> device=Class.forName("android.hardware.camera2.impl.CameraDeviceImpl",false,loader);
        XposedBridge.hookAllMethods(device,"createReprocessCaptureRequest",new XC_MethodHook(100) {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if(p.args.length!=1 || !(p.args[0] instanceof TotalCaptureResult))return;
                Object original=p.args[0];
                try {
                    TotalCaptureResult source=(TotalCaptureResult)original;Plan plan;
                    synchronized(LOCK) {
                        plan=pending;if(plan==null)return;
                        Log.i("OcFresh","NIGHT_REFERENCE_CONSUMER mode="+ObserveHook.currentMode()
                                +" lastOpened="+ObserveHook.currentCameraId()+" sourceCamera="+source.getCameraId()
                                +" sourceFrame="+source.getFrameNumber()+" plannedFirst="+plan.first.result.getFrameNumber());
                        if(!"night".equals(ObserveHook.currentMode()) || !"7".equals(source.getCameraId())
                                || SystemClock.elapsedRealtime()-plan.created>30000
                                || source.getFrameNumber()!=plan.first.result.getFrameNumber())return;
                        Long timestamp=source.get(CaptureResult.SENSOR_TIMESTAMP);
                        if(timestamp==null || timestamp!=plan.first.timestamp)return;
                        pending=null;RAW.headMap(plan.lastTimestamp,true).clear();
                    }
                    TotalCaptureResult reference=plan.reference.result;
                    Log.i("OcFresh","NIGHT_REFERENCE_APPLY firstFrame="+source.getFrameNumber()
                            +" firstISO="+source.get(CaptureResult.SENSOR_SENSITIVITY)
                            +" firstExposure="+source.get(CaptureResult.SENSOR_EXPOSURE_TIME)
                            +" refIndex="+plan.index+" refFrame="+reference.getFrameNumber()
                            +" refISO="+reference.get(CaptureResult.SENSOR_SENSITIVITY)
                            +" refExposure="+reference.get(CaptureResult.SENSOR_EXPOSURE_TIME)
                            +" actualHALresult at Camera2 reprocess builder; original RAW/crop untouched");
                    p.args[0]=reference;
                    p.setObjectExtra("night_reference_first",source);
                    p.setObjectExtra("night_reference_applied",Boolean.TRUE);
                }catch(Throwable unavailable){p.args[0]=original;
                    Log.e("OcFresh","NIGHT_REFERENCE_UNAVAILABLE original retained",unavailable);}
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if(!Boolean.TRUE.equals(p.getObjectExtra("night_reference_applied")))return;
                if(p.hasThrowable()){Log.e("OcFresh","NIGHT_REFERENCE_BUILDER_ERROR",p.getThrowable());return;}
                if(p.getResult() instanceof CaptureRequest.Builder){
                    CaptureRequest.Builder builder=(CaptureRequest.Builder)p.getResult();
                    NightRequestTrace.created((TotalCaptureResult)p.args[0],builder);
                    NightScalarTrace.created((TotalCaptureResult)p.getObjectExtra("night_reference_first"),
                            (TotalCaptureResult)p.args[0],builder);
                    Log.i("OcFresh","NIGHT_REFERENCE_BUILDER ISO="+builder.get(CaptureRequest.SENSOR_SENSITIVITY)
                            +" exposure="+builder.get(CaptureRequest.SENSOR_EXPOSURE_TIME));
                }
            }
        });
        Class<?> session=Class.forName("android.hardware.camera2.impl.CameraCaptureSessionImpl",false,loader);
        for(String method:new String[]{"capture","captureSingleRequest","captureBurst","captureBurstRequests"})
            XposedBridge.hookAllMethods(session,method,new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    try{
                        if(!"night".equals(ObserveHook.currentMode())||p.args.length==0)return;
                        Object first=p.args[0];
                        if(first instanceof java.util.List&&!((java.util.List<?>)first).isEmpty())first=((java.util.List<?>)first).get(0);
                        if(!(first instanceof CaptureRequest))return;CaptureRequest request=(CaptureRequest)first;
                        if(!Boolean.TRUE.equals(XposedHelpers.callMethod(request,"isReprocess")))return;
                        Log.i("OcFresh","NIGHT_REFERENCE_FINAL_REQUEST ISO="+request.get(CaptureRequest.SENSOR_SENSITIVITY)
                                +" exposure="+request.get(CaptureRequest.SENSOR_EXPOSURE_TIME)
                                +" crop="+request.get(CaptureRequest.SCALER_CROP_REGION));
                    }catch(Throwable ignored){/* Read-only submission observation. */}
                }
            });
        // Cache holds only metadata (48 entries), with an exact snapshot
        // identity and30s plan expiry. Other camera closes must not erase a
        // pending original asynchronous RAW operation.
        Log.i("OcFresh","NIGHT_REFERENCE_READY opt-in rearSAT7 standardNight10RAW scene2; real firstEV0 result only");
    }
}
