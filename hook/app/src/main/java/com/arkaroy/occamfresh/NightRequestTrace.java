package com.arkaroy.occamfresh;

import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.ColorSpaceTransform;
import android.hardware.camera2.params.RggbChannelVector;
import android.util.Log;
import android.util.Rational;
import java.lang.reflect.Array;
import java.util.Collections;
import java.util.Map;
import java.util.TreeMap;
import java.util.WeakHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Opt-in two-capture scalar/request audit. No supplier is evaluated twice. */
final class NightRequestTrace {
    private static volatile boolean enabled;
    private static final AtomicInteger budget=new AtomicInteger();
    private static final Map<CaptureRequest.Builder, Audit> builders=
            Collections.synchronizedMap(new WeakHashMap<>());
    private static final Map<CaptureRequest, Audit> requests=
            Collections.synchronizedMap(new WeakHashMap<>());
    private static final ThreadLocal<String> writer=new ThreadLocal<>();
    private static final class Audit {
        final int id;
        final Map<String,String> reference=new TreeMap<>();
        final Map<String,CaptureResult.Key<?>> resultKeys=new TreeMap<>();
        TotalCaptureResult result;
        final Map<String,Boolean> written=new TreeMap<>();
        Audit(int id){this.id=id;}
    }
    private static boolean selected(String name){
        return name.startsWith("android.colorCorrection.") || name.startsWith("android.blackLevel.")
                || name.equals("android.sensor.dynamicBlackLevel") || name.equals("android.sensor.dynamicWhiteLevel")
                || name.equals("android.sensor.neutralColorPoint") || name.equals("android.sensor.sensitivity")
                || name.equals("android.sensor.exposureTime") || name.equals("android.sensor.frameDuration")
                || name.equals("android.scaler.cropRegion") || name.startsWith("android.control.awb")
                || name.equals("android.control.postRawSensitivityBoost") || name.contains("supernight")
                || name.contains("ainight") || name.contains("rawhdr") || name.contains("MetadataOwner")
                || name.contains("snapshot.master") || name.endsWith("sensor_mode_index");
    }
    private static String render(Object value){
        if(value==null)return "null";
        if(value instanceof Rational){Rational r=(Rational)value;return "Rational("+r.getNumerator()+"/"+r.getDenominator()+")";}
        if(value instanceof ColorSpaceTransform){
            ColorSpaceTransform c=(ColorSpaceTransform)value;StringBuilder b=new StringBuilder("ColorSpaceTransform[");
            for(int row=0;row<3;row++)for(int col=0;col<3;col++)b.append(row==0&&col==0?"":",").append(render(c.getElement(col,row)));
            return b.append(']').toString();
        }
        if(value instanceof RggbChannelVector){RggbChannelVector g=(RggbChannelVector)value;
            return "Rggb["+g.getRed()+","+g.getGreenEven()+","+g.getGreenOdd()+","+g.getBlue()+"]";}
        if(value.getClass().isArray()){
            int n=Array.getLength(value);StringBuilder b=new StringBuilder(value.getClass().getComponentType().getSimpleName()+"["+n+"]{");
            for(int i=0;i<Math.min(n,128);i++)b.append(i==0?"":",").append(render(Array.get(value,i)));
            if(n>128)b.append(",TRUNCATED");return b.append('}').toString();
        }
        String text=String.valueOf(value);if(text.length()>900)text=text.substring(0,900)+"...TRUNCATED";
        return value.getClass().getSimpleName()+"("+text+")";
    }
    @SuppressWarnings({"rawtypes","unchecked"})
    private static Map<String,String> requestValues(CaptureRequest request){
        Map<String,String> values=new TreeMap<>();
        for(CaptureRequest.Key key:request.getKeys()){
            String name=key.getName();try{values.put(name,render(request.get(key)));}
            catch(Throwable unavailable){values.put(name,"UNAVAILABLE:"+unavailable.getClass().getSimpleName());}
        }
        return values;
    }
    private static void snapshot(Audit a,String stage,CaptureRequest request){
        if(a==null)return;
        try{
            Map<String,String> values=requestValues(request);Map<String,Boolean> names=new TreeMap<>();
            for(String name:values.keySet())if(selected(name))names.put(name,true);
            synchronized(a){names.putAll(a.written);for(String name:a.reference.keySet())names.put(name,true);}
            Log.i("OcNightKeys","BOUNDARY capture="+a.id+" stage="+stage+" requestKeys="+values.size());
            for(String name:names.keySet())Log.i("OcNightKeys","KEY capture="+a.id+" stage="+stage+" name="+name
                    +" value="+values.getOrDefault(name,"UNAVAILABLE:not_in_request_getKeys")
                    +" ev0="+a.reference.getOrDefault(name,"UNAVAILABLE:not_in_result_selection"));
        }catch(Throwable unavailable){Log.i("OcNightKeys","SNAPSHOT_UNAVAILABLE stage="+stage+" error="+unavailable);}
    }
    @SuppressWarnings({"rawtypes","unchecked"})
    static void created(TotalCaptureResult reference,CaptureRequest.Builder builder){
        if(!enabled)return;
        int index=budget.getAndIncrement();if(index>=2)return;
        try{
            Audit a=new Audit(index);a.result=reference;
            for(CaptureResult.Key<?> key:reference.getKeys())a.resultKeys.put(key.getName(),key);
            for(CaptureResult.Key key:reference.getKeys())if(selected(key.getName())){
                try{a.reference.put(key.getName(),render(reference.get(key)));}
                catch(Throwable unavailable){a.reference.put(key.getName(),"UNAVAILABLE:"+unavailable.getClass().getSimpleName());}
            }
            builders.put(builder,a);
            Log.i("OcNightKeys","REFERENCE capture="+index+" camera="+reference.getCameraId()
                    +" frame="+reference.getFrameNumber()+" timestamp="+reference.get(CaptureResult.SENSOR_TIMESTAMP));
            snapshot(a,"created",builder.build());
        }catch(Throwable unavailable){Log.i("OcNightKeys","CREATED_UNAVAILABLE "+unavailable);}
    }
    static void install(ClassLoader loader) throws ClassNotFoundException {
        Class<?> camera=Class.forName("com.oppo.camera.f.g",false,loader);
        Class<?> tag=Class.forName("com.oppo.camera.f.d",false,loader);
        XposedHelpers.findAndHookMethod(camera,"a",CaptureRequest.Builder.class,new XC_MethodHook(){
            @Override protected void beforeHookedMethod(MethodHookParam p){
                Audit a=builders.get(p.args[0]);if(a==null)return;
                p.setObjectExtra("nk_previous",writer.get());p.setObjectExtra("nk_active",true);writer.set("cache");
                try{
                    Object map=XposedHelpers.getObjectField(p.thisObject,"I");
                    if(map instanceof Map){int count=0;for(Object key:((Map<?,?>)map).keySet()){
                        if(count++>=128)break;
                        Log.i("OcNightKeys","CACHE_NAME capture="+a.id+" name="+render(key));
                    }}
                    snapshot(a,"beforeCache",((CaptureRequest.Builder)p.args[0]).build());
                }catch(Throwable unavailable){Log.i("OcNightKeys","CACHE_UNAVAILABLE "+unavailable);}
            }
            @Override protected void afterHookedMethod(MethodHookParam p){
                if(!Boolean.TRUE.equals(p.getObjectExtra("nk_active")))return;
                writer.set((String)p.getObjectExtra("nk_previous"));
                CaptureRequest.Builder builder=(CaptureRequest.Builder)p.args[0];
                try{snapshot(builders.get(builder),"afterCache",builder.build());}
                catch(Throwable unavailable){Log.i("OcNightKeys","CACHE_AFTER_UNAVAILABLE "+unavailable);}
            }
        });
        XposedHelpers.findAndHookMethod(CaptureRequest.Builder.class,"set",CaptureRequest.Key.class,Object.class,new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam p){
                Audit a=builders.get(p.thisObject);if(a==null)return;
                try{
                    String name=((CaptureRequest.Key<?>)p.args[0]).getName();
                    synchronized(a){
                        if(a.written.size()>=128&&!a.written.containsKey(name))return;a.written.put(name,true);
                        if(!a.reference.containsKey(name)&&a.resultKeys.containsKey(name))
                            a.reference.put(name,render(a.result.get(a.resultKeys.get(name))));
                    }
                    Log.i("OcNightKeys","WRITE capture="+a.id+" writer="+(writer.get()==null?"postCacheOrCrop":writer.get())
                            +" name="+name+" value="+render(p.args[1])+" error="+p.getThrowable());
                }catch(Throwable unavailable){Log.i("OcNightKeys","WRITE_UNAVAILABLE "+unavailable);}
            }
        });
        XposedHelpers.findAndHookMethod(camera,"a",TotalCaptureResult.class,tag,new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam p){
                if(p.hasThrowable()||!(p.getResult() instanceof CaptureRequest.Builder))return;
                CaptureRequest.Builder builder=(CaptureRequest.Builder)p.getResult();
                if(builders.containsKey(builder))try{snapshot(builders.get(builder),"afterPrivateBuilder",builder.build());}
                catch(Throwable unavailable){Log.i("OcNightKeys","PRIVATE_AFTER_UNAVAILABLE "+unavailable);}
            }
        });
        XposedBridge.hookAllMethods(CaptureRequest.Builder.class,"build",new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam p){
                Audit a=builders.get(p.thisObject);if(a!=null&&!p.hasThrowable()&&p.getResult() instanceof CaptureRequest)
                    requests.put((CaptureRequest)p.getResult(),a);
            }
        });
        Class<?> session=Class.forName("android.hardware.camera2.impl.CameraCaptureSessionImpl",false,loader);
        for(String method:new String[]{"capture","captureSingleRequest","captureBurst","captureBurstRequests"})
            XposedBridge.hookAllMethods(session,method,new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    try{
                        if(p.args.length==0)return;Object request=p.args[0];
                        if(request instanceof java.util.List&&!((java.util.List<?>)request).isEmpty())request=((java.util.List<?>)request).get(0);
                        Audit a=requests.remove(request);if(a==null||!(request instanceof CaptureRequest))return;
                        snapshot(a,"submitted",(CaptureRequest)request);
                        synchronized(builders){builders.entrySet().removeIf(entry->entry.getValue()==a);}
                    }catch(Throwable unavailable){Log.i("OcNightKeys","SUBMIT_UNAVAILABLE "+unavailable);}
                }
            });
        enabled=true;
        Log.i("OcNightKeys","READY max2 applied standardNight captures; scalar metadata only; suppliers called only by original code");
    }
}
