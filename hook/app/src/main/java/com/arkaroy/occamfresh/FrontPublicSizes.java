package com.arkaroy.occamfresh;

import android.graphics.ImageFormat;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.ImageReader;
import android.util.Log;
import android.util.Size;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

/** F14 front Photo QCFA output and scoped original Night YUV requests. */
final class FrontPublicSizes {
    static void install(ClassLoader loader, boolean fullSizeOutput, boolean poolSizes,
                        boolean nightFullTrial) throws ClassNotFoundException {
        Class<?> cameraUtility = Class.forName("com.oppo.camera.f.a", false, loader);
        if(poolSizes) {
            Class<?> stateMachine=Class.forName("com.oppo.camera.f.i",false,loader);
            XposedHelpers.findAndHookMethod(stateMachine,"a",String.class,int.class,int.class,
                    int.class,int.class,long.class,new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    if(!"night".equals(ObserveHook.currentMode()) || !"1".equals(ObserveHook.currentCameraId())
                            || !"type_still_capture_yuv_main".equals(p.args[0])
                            || (Integer)p.args[3]!=ImageFormat.YUV_420_888
                            || (Integer)p.args[4]!=20 || (Long)p.args[5]!=3L)return;
                    int width=(Integer)p.args[1],height=(Integer)p.args[2];
                    if(nightFullTrial && originalFullSize(width,height)){
                        Log.i("OcFresh","FRONT_NIGHT_FULL_POOL keep requested"+width+"x"+height+"; original lookup/reuse/replacement owns reader");
                        return;
                    }
                    try {
                        Size target=advertisedTarget(cameraUtility,width,height);if(target==null)return;
                        // Original pool now compares against the real supported geometry.
                        // A Photo reader mismatch invokes its own replacement branch;
                        // no manual pool clearing or RAW/preview reader closure.
                        p.args[1]=target.getWidth();p.args[2]=target.getHeight();
                        Log.i("OcFresh","FRONT_NIGHT_POOL_SIZE requested="+width+"x"+height
                                +" native="+target+" original pool lookup/replacement delegated");
                    }catch(Throwable unavailable){p.args[1]=width;p.args[2]=height;
                        Log.e("OcFresh","FRONT_NIGHT_POOL_UNAVAILABLE original dimensions retained",unavailable);}
                }
            });
            Log.i("OcFresh","FRONT_NIGHT_POOL_READY exact camera1/Night/mainYUV20/usage3; original replacement owns closure");
        }
        XposedHelpers.findAndHookMethod(ImageReader.class, "newInstance",
                int.class, int.class, int.class, int.class, new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                String mode = ObserveHook.currentMode();
                if (!"1".equals(ObserveHook.currentCameraId())
                        || !("common".equals(mode) || "night".equals(mode))
                        || (Integer) p.args[2] != ImageFormat.YUV_420_888) return;
                int width = (Integer) p.args[0], height = (Integer) p.args[1];
                if (!((width == 4608 && (height == 3456 || height == 2592 || height == 2080))
                        || (width == 3456 && height == 3456))) return;
                boolean fullNight = nightFullTrial && "night".equals(mode)
                        && originalFullSize(width,height) && (Integer)p.args[3]==20;
                if ((fullSizeOutput && "common".equals(mode)) || fullNight) {
                    ImageReader reader = ImageReader.newInstance(width, height, ImageFormat.YUV_420_888,
                            (Integer) p.args[3], android.hardware.HardwareBuffer.USAGE_CPU_READ_OFTEN | 0x00100000L);
                    p.setResult(reader);
                    Log.i("OcFresh", (fullNight?"FRONT_NIGHT_FULL_YUV":"FRONT_FULL_SIZE")
                            + " original" + width + "x" + height + " preserved; no pixel resizing");
                    return;
                }
                Size target = new Size(width / 2, height / 2);
                try {
                    CameraManager manager = (CameraManager)
                            XposedHelpers.getStaticObjectField(cameraUtility, "a");
                    if (manager == null) return;
                    CameraCharacteristics c = manager.getCameraCharacteristics("1");
                    if (!Integer.valueOf(CameraCharacteristics.LENS_FACING_FRONT).equals(
                            c.get(CameraCharacteristics.LENS_FACING))
                            || !new Size(2304, 1728).equals(
                                    c.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE))) return;
                    StreamConfigurationMap map = c.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                    if (map == null) return;
                    Size[] sizes = map.getOutputSizes(ImageFormat.YUV_420_888);
                    boolean supported = false;
                    if (sizes != null) for (Size size : sizes)
                        if (target.equals(size)) { supported = true; break; }
                    if (!supported) return;
                    // Call the same original four-argument API with its default
                    // usage/maxImages. No synthetic metadata or pixel resizing.
                    p.args[0] = target.getWidth();
                    p.args[1] = target.getHeight();
                    Log.i("OcFresh", "FRONT_PUBLIC_SIZE mode=" + mode + " requested="
                            + width + "x" + height + " native=" + target
                            + "; advertised front YUV output, no upscaling");
                } catch (Throwable unavailable) {
                    p.args[0] = width;
                    p.args[1] = height;
                    Log.e("OcFresh", "FRONT_PUBLIC_SIZE_UNAVAILABLE original request retained", unavailable);
                }
            }
        });
        Log.i("OcFresh", "FRONT_PUBLIC_SIZE_READY opt-in front1 Photo/Night only; original RAW unchanged");
        if(nightFullTrial)Log.i("OcFresh","FRONT_NIGHT_FULL_YUV_READY front1 Night original four YUV ratios/maxImages20 only; RAW unchanged");
    }
    private static boolean originalFullSize(int width,int height){
        return (width==4608&&(height==3456||height==2592||height==2080))
                ||(width==3456&&height==3456);
    }
    private static Size advertisedTarget(Class<?> cameraUtility,int width,int height) throws Throwable {
        if(!((width==4608&&(height==3456||height==2592||height==2080))
                ||(width==3456&&height==3456)))return null;
        CameraManager manager=(CameraManager)XposedHelpers.getStaticObjectField(cameraUtility,"a");
        if(manager==null)return null;
        CameraCharacteristics c=manager.getCameraCharacteristics("1");
        if(!Integer.valueOf(CameraCharacteristics.LENS_FACING_FRONT).equals(c.get(CameraCharacteristics.LENS_FACING))
                ||!new Size(2304,1728).equals(c.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE)))return null;
        StreamConfigurationMap map=c.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        if(map==null)return null;Size target=new Size(width/2,height/2);
        Size[] sizes=map.getOutputSizes(ImageFormat.YUV_420_888);
        if(sizes!=null)for(Size size:sizes)if(target.equals(size))return target;
        return null;
    }

}
