package com.arkaroy.occamfresh;

import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.util.Log;
import java.lang.reflect.Array;
import java.util.Collections;
import java.util.Map;
import java.util.WeakHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;

/** Two applied rear Night reference/submitted snapshots, without extra build calls. */
final class NightScalarTrace {
    private static volatile boolean enabled;
    private static final AtomicInteger budget=new AtomicInteger();
    private static final AtomicInteger aecReports=new AtomicInteger();
    private static final AtomicLong aecFrame=new AtomicLong(-1);
    private static final Map<CaptureRequest.Builder, Audit> builders=
            Collections.synchronizedMap(new WeakHashMap<>());
    private static final Map<CaptureRequest, Audit> requests=
            Collections.synchronizedMap(new WeakHashMap<>());
    private static final class Audit {
        final int id; final TotalCaptureResult first,reference;
        Audit(int id,TotalCaptureResult first,TotalCaptureResult reference){this.id=id;this.first=first;this.reference=reference;}
    }
    private static boolean selected(String name){
        return name.startsWith("android.colorCorrection.")
            || name.equals("android.sensor.dynamicBlackLevel")
            || name.equals("android.sensor.dynamicWhiteLevel")
            || name.equals("android.sensor.neutralColorPoint")
            || name.equals("android.sensor.sensitivity")
            || name.equals("android.sensor.exposureTime")
            || name.equals("android.scaler.cropRegion")
            || name.equals("android.control.awbMode")
            || name.equals("android.control.awbLock")
            || name.equals("android.control.postRawSensitivityBoost")
            || name.equals("android.blackLevel.lock")
            || name.startsWith("com.oplus.rawhdr.")
            || name.equals("com.oplus.supernight.mode")
            || name.equals("com.oplus.underExpRatio")
            || name.equals("com.oplus.ChromatixName")
            || name.equals("com.oplus.SensorName")
            || name.equals("com.oplus.supernight.reprocess.stats")
            || name.equals("com.qti.stats_control.drc_gain")
            || name.equals("com.qti.stats_control.dark_boost_gain");
    }
    private static String render(Object value){
        if(value==null)return "null";
        if(value.getClass().isArray()){
            int count=Array.getLength(value);StringBuilder out=new StringBuilder("[");
            for(int i=0;i<Math.min(count,12);i++)out.append(i==0?"":",").append(Array.get(value,i));
            return out.append(count>12?",TRUNCATED]":"]").toString();
        }
        String out=String.valueOf(value);return out.length()>600?out.substring(0,600)+"...":out;
    }
    private static String identity(TotalCaptureResult result,String key){
        try{
            byte[] value=result.get(new CaptureResult.Key<byte[]>(key,byte[].class));
            if(value==null)return "null";
            int length=0;while(length<Math.min(value.length,64)&&value[length]!=0)length++;
            StringBuilder out=new StringBuilder();
            for(int i=0;i<length;i++){
                int c=value[i]&255;out.append(c>=32&&c<=126?(char)c:'?');
            }
            return out+"(bytes="+value.length+")";
        }catch(Throwable unavailable){return "unavailable:"+unavailable.getClass().getSimpleName();}
    }
    private static void identityLog(TotalCaptureResult result,String stage){
        Log.i("OcNightScalar","AEC_IDENTITY stage="+stage+" frame="+result.getFrameNumber()
                +" camera="+result.getCameraId()
                +" chromatix="+identity(result,"com.oplus.ChromatixName")
                +" sensor="+identity(result,"com.oplus.SensorName"));
    }
    @SuppressWarnings({"rawtypes","unchecked"})
    private static void reference(Audit audit){
        identityLog(audit.reference,"reference_capture_"+audit.id);
        int scanned=0,logged=0;
        for(CaptureResult.Key key:audit.reference.getKeys()){
            if(scanned++>=512)break;if(!selected(key.getName()))continue;
            if(logged++>=48)break;
            try{Log.i("OcNightScalar","VALUE capture="+audit.id+" stage=reference name="+key.getName()
                    +" value="+render(audit.reference.get(key)));}
            catch(Throwable error){Log.i("OcNightScalar","UNAVAILABLE capture="+audit.id+" name="+key.getName());}
        }
        Log.i("OcNightScalar","COUNTS capture="+audit.id+" stage=reference scanned="+scanned+" logged="+logged);
    }
    static void created(TotalCaptureResult first,TotalCaptureResult reference,CaptureRequest.Builder builder){
        if(!enabled)return;int id=budget.getAndIncrement();if(id>=2)return;
        Audit audit=new Audit(id,first,reference);builders.put(builder,audit);
        // Defer value enumeration until submit: observe the original builder only.
        Log.i("OcNightScalar","REFERENCE capture="+id+" camera="+reference.getCameraId()
                +" frame="+reference.getFrameNumber()+" timestamp="+reference.get(CaptureResult.SENSOR_TIMESTAMP));
    }
    @SuppressWarnings({"rawtypes","unchecked"})
    private static void submitted(CaptureRequest request,Audit audit){
        Log.i("OcNightScalar","SUBMITTED capture="+audit.id+" reprocess="+request.isReprocess()
                +" refFrame="+audit.reference.getFrameNumber());
        reference(audit);
        privateBlob(audit.id,"first",audit.first,null);
        privateBlob(audit.id,"reference",audit.reference,null);
        privateBlob(audit.id,"submitted",null,request);
        int scanned=0,logged=0;
        for(CaptureRequest.Key key:request.getKeys()){
            if(scanned++>=512)break;if(!selected(key.getName()))continue;
            if(logged++>=48)break;
            try{Log.i("OcNightScalar","VALUE capture="+audit.id+" stage=submitted name="+key.getName()
                    +" value="+render(request.get(key)));}
            catch(Throwable error){Log.i("OcNightScalar","UNAVAILABLE capture="+audit.id+" name="+key.getName());}
        }
        Log.i("OcNightScalar","COUNTS capture="+audit.id+" stage=submitted scanned="+scanned+" logged="+logged);
    }
    private static void privateBlob(int id,String stage,TotalCaptureResult result,CaptureRequest request){
        final String key="org.codeaurora.qcamera3.internal_private.private_property";
        try{
            byte[] bytes=result!=null?result.get(new CaptureResult.Key<byte[]>(key,byte[].class))
                    :request.get(new CaptureRequest.Key<byte[]>(key,byte[].class));
            PrivatePropertyBlob.Result parsed=PrivatePropertyBlob.decode(bytes);
            Log.i("OcNightScalar","PRIVATE capture="+id+" stage="+stage+" length="+(bytes==null?-1:bytes.length)
                    +" state="+parsed.state+" declared="+parsed.declared+" walked="+parsed.walked+" consumed="+parsed.consumed);
            for(Map.Entry<Long,String> value:parsed.selected.entrySet())
                Log.i("OcNightScalar","PRIVATE_TAG capture="+id+" stage="+stage+" tag="+Long.toHexString(value.getKey())+" "+value.getValue());
        }catch(Throwable error){Log.i("OcNightScalar","PRIVATE_UNAVAILABLE capture="+id+" stage="+stage+" error="+error.getClass().getSimpleName());}
    }
    static void install(ClassLoader loader) throws ClassNotFoundException {
        // Preview-only check of the stock AEC's published scale, no capture or
        // metadata override. Constructor argument/result remains untouched.
        XposedBridge.hookAllConstructors(TotalCaptureResult.class,new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam p){
                if(!enabled||aecReports.get()>=4||!"night".equals(ObserveHook.currentMode())
                        ||p.hasThrowable()||!(p.thisObject instanceof TotalCaptureResult))return;
                try{
                    TotalCaptureResult result=(TotalCaptureResult)p.thisObject;
                    if(!"7".equals(result.getCameraId()))return;
                    float[] lux=result.get(new CaptureResult.Key<float[]>("com.oplus.rawhdr.isp.luxindex",float[].class));
                    if(lux==null||lux.length!=1||!Float.isFinite(lux[0])||lux[0]<525)return;
                    long frame=result.getFrameNumber();if(aecFrame.getAndSet(frame)==frame)return;
                    float[] scale=result.get(new CaptureResult.Key<float[]>("com.oplus.underExpRatio",float[].class));
                    if(aecReports.getAndIncrement()>=4)return;
                    identityLog(result,"preview");
                    Log.i("OcNightScalar","AEC_SCALE frame="+frame+" camera=7 lux="+lux[0]
                            +" underExpRatio="+render(scale)+" iso="+result.get(CaptureResult.SENSOR_SENSITIVITY)
                            +" exposureNs="+result.get(CaptureResult.SENSOR_EXPOSURE_TIME)
                            +" read_only_current_original_result");
                }catch(Throwable unavailable){
                    if(aecReports.getAndIncrement()<4)Log.i("OcNightScalar","AEC_SCALE_UNAVAILABLE "+unavailable.getClass().getSimpleName());
                }
            }
        });
        XposedBridge.hookAllMethods(CaptureRequest.Builder.class,"build",new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam p){
                Audit audit=builders.get(p.thisObject);
                if(audit!=null&&!p.hasThrowable()&&p.getResult() instanceof CaptureRequest)
                    requests.put((CaptureRequest)p.getResult(),audit);
            }
        });
        Class<?> session=Class.forName("android.hardware.camera2.impl.CameraCaptureSessionImpl",false,loader);
        for(String name:new String[]{"capture","captureSingleRequest","captureBurst","captureBurstRequests"})
            XposedBridge.hookAllMethods(session,name,new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    try{
                        if(p.args.length==0)return;Object first=p.args[0];
                        if(first instanceof java.util.List&&!((java.util.List<?>)first).isEmpty())
                            first=((java.util.List<?>)first).get(0);
                        Audit audit=requests.remove(first);
                        if(audit==null||!(first instanceof CaptureRequest))return;
                        synchronized(builders){builders.entrySet().removeIf(entry->entry.getValue()==audit);}
                        synchronized(requests){requests.entrySet().removeIf(entry->entry.getValue()==audit);}
                        submitted((CaptureRequest)first,audit);
                    }catch(Throwable error){Log.i("OcNightScalar","UNAVAILABLE submit="+error);}
                }
            });
        enabled=true;
        Log.i("OcNightScalar","READY max2 applied rearNight captures; original builds only; no writes");
    }
}
