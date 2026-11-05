package com.arkaroy.occamfresh;

import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.media.Image;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;
import java.lang.reflect.Array;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;
import org.json.JSONArray;
import org.json.JSONObject;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Opt-in audit foundation. No argument/result/metadata/pixel overrides. */
final class EnvironmentAudit {
    private static final AtomicLong sequence=new AtomicLong(),dropped=new AtomicLong(),decisions=new AtomicLong();
    private static volatile String lastDecision="";
    private static final Set<String> transportTypes=java.util.concurrent.ConcurrentHashMap.newKeySet();
    private static final int LIMIT=3000;
    private static Object scalar(Object value){
        if(value==null)return JSONObject.NULL;
        if(value instanceof Number||value instanceof Boolean)return value;
        if(value instanceof String)return ((String)value).substring(0,Math.min(300,((String)value).length()));
        if(value.getClass().isArray()){
            JSONArray array=new JSONArray();int n=Array.getLength(value);
            for(int i=0;i<Math.min(n,32);i++)array.put(scalar(Array.get(value,i)));
            if(n>32)array.put("TRUNCATED length="+n);
            return array;
        }
        return value.getClass().getName();
    }
    private static void put(JSONObject object,String key,Object value){try{object.put(key,value);}catch(Exception ignored){}}
    private static Object field(Object object,String name){
        try{return XposedHelpers.getObjectField(object,name);}catch(Throwable unavailable){return "UNAVAILABLE:"+name;}
    }
    private static void emit(String kind,JSONObject detail){
        long n=sequence.getAndIncrement();if(n>=LIMIT){dropped.incrementAndGet();if(n==LIMIT)Log.w("OcEnvApp","COVERAGE_LIMIT limit=3000 further_events_unobserved; see HEARTBEAT");return;}
        put(detail,"event",kind);put(detail,"sequence",n);put(detail,"elapsedNs",SystemClock.elapsedRealtimeNanos());
        put(detail,"pid",Process.myPid());put(detail,"tid",Process.myTid());put(detail,"mode",ObserveHook.currentMode());
        put(detail,"lastOpenedCamera",ObserveHook.currentCameraId());
        Log.i("OcEnvApp",detail.toString());
    }
    private static void image(JSONObject object,Object value){
        if(!(value instanceof Image))return;
        Image im=(Image)value;
        try{
            put(object,"imageTimestamp",im.getTimestamp());put(object,"width",im.getWidth());put(object,"height",im.getHeight());put(object,"format",im.getFormat());
            JSONArray planes=new JSONArray();for(Image.Plane p:im.getPlanes()){JSONObject plane=new JSONObject();put(plane,"rowStride",p.getRowStride());put(plane,"pixelStride",p.getPixelStride());planes.put(plane);}put(object,"planes",planes);
        }catch(Throwable unavailable){put(object,"imageUnavailable",unavailable.getClass().getSimpleName());}
    }
    private static void metadata(JSONObject object,Object value){
        if(!(value instanceof TotalCaptureResult))return;
        TotalCaptureResult result=(TotalCaptureResult)value;
        put(object,"camera",result.getCameraId());put(object,"frame",result.getFrameNumber());
        put(object,"sensorTimestamp",result.get(CaptureResult.SENSOR_TIMESTAMP));
        put(object,"iso",result.get(CaptureResult.SENSOR_SENSITIVITY));put(object,"exposureNs",result.get(CaptureResult.SENSOR_EXPOSURE_TIME));
    }
    private static JSONObject captureInput(Object[]arguments){
        JSONObject object=new JSONObject();
        for(Object arg:arguments){
            if(arg==null||!arg.getClass().getName().endsWith("ApsCaptureParam"))continue;
            put(object,"frame",scalar(field(arg,"mFrameNumber")));put(object,"role",scalar(field(arg,"mRole")));
            Object buffer=field(arg,"mImageBuffer");image(object,field(buffer,"mImage"));metadata(object,field(arg,"mLogicMeta"));
            put(object,"inputSize",scalar(field(arg,"mInputSize")));
        }
        return object;
    }
    private static final Set<String> CAPTURE_KEYS=new HashSet<>(Arrays.asList("identity","capture_feature_type","logic_camera_id","camera_id","operation_mode","super_night_scene","output_width","output_height","zoom_ratio","input_ev_list:","pi_enable","ai_scene","capture_algo_list","meta_index","capture_mfnr_num"));
    private static JSONObject parameters(Object[]arguments){
        JSONObject result=new JSONObject();
        for(Object arg:arguments){
            if(arg==null||!arg.getClass().isArray())continue;
            int n=Array.getLength(arg);
            for(int i=0;i+1<n;i++){
                Object key=Array.get(arg,i);if(key instanceof String&&CAPTURE_KEYS.contains(key))put(result,(String)key,scalar(Array.get(arg,++i)));
            }
        }
        return result;
    }
    private static void installMethod(ClassLoader loader,String className,String name,XC_MethodHook hook){
        JSONObject detail=new JSONObject();put(detail,"class",className);put(detail,"method",name);
        try{Class<?> cls=Class.forName(className,false,loader);int count=XposedBridge.hookAllMethods(cls,name,hook).size();put(detail,"overloads",count);put(detail,"installed",count>0);}
        catch(Throwable error){put(detail,"installed",false);put(detail,"error",error.toString());}
        emit("HOOK_COVERAGE",detail);
    }
    static void install(ClassLoader loader){
        try{NativePaths.setEnvironmentAudit(true);}catch(Throwable error){JSONObject detail=new JSONObject();put(detail,"error",error.toString());emit("NATIVE_OBSERVER_UNAVAILABLE",detail);}
        JSONObject coverage=new JSONObject();put(coverage,"nativeSDKExecutionCoverage","Superphoto only; Night/bokeh/HDR require separate verified native probes");put(coverage,"gateCoverage","provider property_get selected keys only; direct system-property reads not covered");put(coverage,"noStockReference",true);put(coverage,"pixelReadback",false);emit("COVERAGE",coverage);
        installMethod(loader,"com.aps.APSClient","previewDecision",new XC_MethodHook(XC_MethodHook.PRIORITY_LOWEST){
            @Override protected void afterHookedMethod(MethodHookParam p){
                decisions.incrementAndGet();if(p.hasThrowable()||p.getResult()==null)return;
                JSONObject object=new JSONObject();Object decision=p.getResult();
                if(!decision.getClass().getName().equals("com.oppo.camera.aps.adapter.ApsAdapterDecision$DecisionResult")){
                    if(transportTypes.add(decision.getClass().getName())){put(object,"resultType",decision.getClass().getName());put(object,"interpretation","transport overload, not parsed APS decision");emit("APS_TRANSPORT_COVERAGE",object);}
                    return;
                }
                for(String name:new String[]{"mApsDecisionFeatureType","mApsDecisionSceneMode","mSuperNightScene","mMultiFrameCount","mRequestFormat","mApsBracketMode","mCaptureEVList","mApsAlgoFlag"})put(object,name,scalar(field(decision,name)));
                String value=ObserveHook.currentMode()+"/"+ObserveHook.currentCameraId()+object;
                if(!value.equals(lastDecision)){lastDecision=value;emit("APS_DECISION",object);}
            }
        });
        for(String name:new String[]{"addFrameBuff","processImages","algoInit"}){
            installMethod(loader,"com.aps.APSClient",name,new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    try{JSONObject object=name.equals("addFrameBuff")?captureInput(p.args):parameters(p.args);put(object,"method",name);emit("APS_INPUT",object);}
                    catch(Throwable error){JSONObject object=new JSONObject();put(object,"error",error.toString());emit("AUDIT_UNAVAILABLE",object);}
                }
                @Override protected void afterHookedMethod(MethodHookParam p){JSONObject object=new JSONObject();put(object,"method",name);put(object,"result",scalar(p.getResult()));put(object,"throwable",p.hasThrowable()?p.getThrowable().toString():JSONObject.NULL);emit("APS_RETURN",object);}
            });
        }
        // JNI postEventFromNative receives a transport object before parsing.
        // The handler receives the populated ApsResult for non-preview events.
        installMethod(loader,"com.aps.APSClient$EventHandler","handleMessage",new XC_MethodHook(){
            @Override protected void beforeHookedMethod(MethodHookParam p){
                if(p.args.length!=1||!(p.args[0] instanceof android.os.Message))return;
                android.os.Message message=(android.os.Message)p.args[0];Object arg=message.obj;
                if(arg==null||!arg.getClass().getName().equals("com.oppo.camera.aps.adapter.ApsResult"))return;
                JSONObject object=new JSONObject();put(object,"handlerMessage",message.what);
                for(String key:new String[]{"mPipelineName","mIdentity","mWidth","mHeight","mMessageType","mBufferType"})put(object,key,scalar(field(arg,key)));
                emit("APS_OUTPUT",object);
            }
        });
        for(String method:new String[]{"createCaptureSessionInternal","createReprocessCaptureRequest"})installMethod(loader,"android.hardware.camera2.impl.CameraDeviceImpl",method,new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam p){JSONObject object=new JSONObject();put(object,"method",method);put(object,"throwable",p.hasThrowable()?p.getThrowable().toString():JSONObject.NULL);for(Object arg:p.args)metadata(object,arg);emit("CAMERA_BOUNDARY",object);}
        });
        HandlerThread thread=new HandlerThread("OcEnvAudit");thread.start();Handler handler=new Handler(thread.getLooper());
        handler.postDelayed(new Runnable(){public void run(){
            JSONObject object=new JSONObject();put(object,"eventsAttempted",sequence.get());put(object,"dropped",dropped.get());put(object,"previewDecisionsSeen",decisions.get());put(object,"limit",LIMIT);put(object,"elapsedNs",SystemClock.elapsedRealtimeNanos());Log.i("OcEnvApp","HEARTBEAT "+object);handler.postDelayed(this,5000);
        }},5000);
    }
}
