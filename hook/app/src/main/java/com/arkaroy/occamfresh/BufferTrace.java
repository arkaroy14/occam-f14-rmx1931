package com.arkaroy.occamfresh;

import android.media.Image;
import android.util.Log;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;

/** Opt-in capture diagnostics. Night samples are read only; no pixels changed. */
final class BufferTrace {
    private static final AtomicInteger count = new AtomicInteger();
    private static final AtomicInteger nightSamples = new AtomicInteger();
    private static final AtomicLong lastNightTimestamp = new AtomicLong(-1);
    private static final AtomicLong lastRawTimestamp = new AtomicLong(-1);
    private static final AtomicInteger rawSamples = new AtomicInteger();
    private static void observeNightSensorRaw(Image image) {
        if(image.getFormat()!=32||image.getWidth()!=4608||image.getHeight()!=3456
                ||!"night".equals(ObserveHook.currentMode())||rawSamples.get()>=32)return;
        long timestamp=image.getTimestamp();if(lastRawTimestamp.getAndSet(timestamp)==timestamp)return;
        Image.Plane[] planes=image.getPlanes();
        if(planes.length!=1||planes[0].getPixelStride()!=2||planes[0].getRowStride()!=9216)return;
        ByteBuffer samples=planes[0].getBuffer().asReadOnlyBuffer().order(ByteOrder.LITTLE_ENDIAN);samples.clear();
        if(samples.capacity()!=31850496||rawSamples.getAndIncrement()>=32)return;
        long[] sums=new long[4];int[] low={65535,65535,65535,65535},high=new int[4];
        long signature=0xcbf29ce484222325L;
        for(int y=0;y<32;++y)for(int x=0;x<32;++x){
            int row=(y*(3456-2)/31)&~1,col=(x*(4608-2)/31)&~1;
            for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
                int site=2*dy+dx,value=samples.getShort(((row+dy)*4608+col+dx)*2)&65535;
                sums[site]+=value;low[site]=Math.min(low[site],value);high[site]=Math.max(high[site],value);
                signature=(signature^(value&255))*0x100000001b3L;
                signature=(signature^(value>>>8))*0x100000001b3L;
            }
        }
        Log.i("OcFreshBuffer","NIGHT_SENSOR_RAW_SITES timestamp="+timestamp+" sums="+java.util.Arrays.toString(sums)
                +" min="+java.util.Arrays.toString(low)+" max="+java.util.Arrays.toString(high)
                +" sampledFNV="+Long.toUnsignedString(signature,16)+" samplesPerSite=1024 read_only_no_dump");
    }
    private static void observeNightYuv(Image image) {
        if(image.getFormat()!=35 || image.getWidth()!=4608 || image.getHeight()!=3456
                || !"night".equals(ObserveHook.currentMode()) || nightSamples.get()>=3)return;
        long timestamp=image.getTimestamp();
        if(lastNightTimestamp.getAndSet(timestamp)==timestamp)return;
        if(nightSamples.getAndIncrement()>=3)return;
        Image.Plane[] planes=image.getPlanes();if(planes.length!=3)return;
        ByteBuffer[] buffers=new ByteBuffer[3];
        for(int i=0;i<3;++i){buffers[i]=planes[i].getBuffer().asReadOnlyBuffer();buffers[i].clear();}
        long[] sums=new long[3],rgb=new long[3];int[] low={255,255,255},high=new int[3];
        int samples=0;
        for(int gy=0;gy<64;++gy)for(int gx=0;gx<64;++gx){
            int y=gy*(3456-2)/63,x=gx*(4608-2)/63;
            int[] values=new int[3];
            for(int i=0;i<3;++i){
                int py=i==0?y:y/2,px=i==0?x:x/2;
                int offset=py*planes[i].getRowStride()+px*planes[i].getPixelStride();
                values[i]=buffers[i].get(offset)&255;
                sums[i]+=values[i];low[i]=Math.min(low[i],values[i]);high[i]=Math.max(high[i],values[i]);
            }
            int c=Math.max(0,values[0]-16),d=values[1]-128,e=values[2]-128;
            rgb[0]+=Math.max(0,Math.min(255,(298*c+409*e+128)>>8));
            rgb[1]+=Math.max(0,Math.min(255,(298*c-100*d-208*e+128)>>8));
            rgb[2]+=Math.max(0,Math.min(255,(298*c+516*d+128)>>8));
            ++samples;
        }
        Log.i("OcFreshBuffer","NIGHT_REPROCESS_YUV_STATS timestamp="+timestamp
                +" samples="+samples+" sumsYUV="+java.util.Arrays.toString(sums)
                +" minYUV="+java.util.Arrays.toString(low)+" maxYUV="+java.util.Arrays.toString(high)
                +" approximateBT601RGB="+java.util.Arrays.toString(new long[]{rgb[0]/samples,rgb[1]/samples,rgb[2]/samples})
                +" originalPlaneOrder=Y,U,V; read-only samples, no saved pixels");
    }
    static void install(ClassLoader loader) throws ClassNotFoundException {
        Class<?> client = Class.forName("com.aps.APSClient", false, loader);
        for (String method : new String[]{"addFrameBuff", "processImages", "postEventFromNative", "algoInit"}) {
            XposedBridge.hookAllMethods(client, method, new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) {
                    // Decision callbacks arrive continuously during preview.
                    // Preserve the diagnostic budget for actual image buffers.
                    boolean capture = !p.method.getName().equals("postEventFromNative");
                    if (!capture) for (Object argument : p.args) {
                        if (argument == null || !argument.getClass().getName().equals(
                                "com.oppo.camera.aps.adapter.ApsResult")) continue;
                        try {
                            Field field = argument.getClass().getDeclaredField("mPipelineName");
                            field.setAccessible(true);
                            Object pipeline = field.get(argument);
                            capture = pipeline != null && pipeline.toString().contains("capture");
                        } catch (Throwable unavailable) { /* Other callback kinds are not images. */ }
                    }
                    if (!capture || count.incrementAndGet() > 500) return;
                    p.setObjectExtra("oc_capture_trace", Boolean.TRUE);
                    StringBuilder text = new StringBuilder("CAPTURE_BUFFER ").append(p.method.getName());
                    for (Object argument : p.args) text.append(" | ").append(describe(argument, 0));
                    Log.i("OcFreshBuffer", text.toString());
                }
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (!Boolean.TRUE.equals(p.getObjectExtra("oc_capture_trace"))) return;
                    Log.i("OcFreshBuffer", "CAPTURE_RETURN " + p.method.getName() + " result="
                            + p.getResult() + " error=" + p.getThrowable());
                }
            });
        }
    }
    private static String describe(Object value, int depth) {
        if (value == null) return "null";
        if (value instanceof Image) {
            try {
                Image image = (Image) value;
                observeNightSensorRaw(image);
                observeNightYuv(image);
                StringBuilder text = new StringBuilder("Image[").append(image.getWidth()).append('x')
                        .append(image.getHeight()).append(" format=").append(image.getFormat())
                        .append(" crop=").append(image.getCropRect()).append(" timestamp=").append(image.getTimestamp());
                for (Image.Plane plane : image.getPlanes()) text.append(" plane(row=")
                        .append(plane.getRowStride()).append(" pixel=").append(plane.getPixelStride())
                        .append(" capacity=").append(plane.getBuffer().capacity()).append(')');
                return text.append(']').toString();
            } catch (Throwable closed) { return "Image[unavailable:" + closed.getClass().getSimpleName() + ']'; }
        }
        if (value instanceof Buffer) {
            Buffer buffer = (Buffer) value;
            return value.getClass().getSimpleName() + "[capacity=" + buffer.capacity()
                    + " position=" + buffer.position() + " limit=" + buffer.limit() + ']';
        }
        Class<?> type = value.getClass();
        if (type.isArray()) {
            int size = Array.getLength(value);
            if (type.getComponentType() == byte.class) return "byte[length=" + size + ']';
            StringBuilder text = new StringBuilder("[");
            for (int i = 0; i < Math.min(size, 140); ++i) text.append(i == 0 ? "" : ",").append(describe(Array.get(value, i), depth + 1));
            return text.append(size > 140 ? ",..." : "").append(']').toString();
        }
        if (depth < 3 && type.getName().startsWith("com.oppo.camera.aps.adapter.")) {
            StringBuilder text = new StringBuilder(type.getSimpleName()).append('{');
            for (Field field : type.getDeclaredFields()) {
                if (java.lang.reflect.Modifier.isStatic(field.getModifiers())) continue;
                try { field.setAccessible(true); text.append(field.getName()).append('=')
                        .append(describe(field.get(value), depth + 1)).append(';'); }
                catch (Throwable inaccessible) { text.append(field.getName()).append("=<inaccessible>;"); }
            }
            return text.append('}').toString();
        }
        return String.valueOf(value);
    }
}
