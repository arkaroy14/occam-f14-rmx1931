package com.arkaroy.occamfresh;

import android.media.CamcorderProfile;
import android.media.MediaRecorder;
import android.util.Log;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

/** Restore the original F14 framework's negative-audio-codec profile handling. */
final class RecorderProfiles {
    static void install() {
        XposedHelpers.findAndHookMethod(MediaRecorder.class, "setProfile", CamcorderProfile.class,
                new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                CamcorderProfile profile = (CamcorderProfile) p.args[0];
                if (profile == null || profile.audioCodec >= 0) return;
                // Verified in original F14 framework.jar MediaRecorder.setProfile:
                // configure these five video fields, then skip all audio setters
                // when audioCodec<0. AOSP16 lacks that OEM conditional. Profile
                // fields and captureRate remain unchanged; real errors propagate.
                MediaRecorder recorder = (MediaRecorder) p.thisObject;
                try {
                    recorder.setOutputFormat(profile.fileFormat);
                    recorder.setVideoFrameRate(profile.videoFrameRate);
                    recorder.setVideoSize(profile.videoFrameWidth, profile.videoFrameHeight);
                    recorder.setVideoEncodingBitRate(profile.videoBitRate);
                    recorder.setVideoEncoder(profile.videoCodec);
                    Log.i("OcFresh", "RECORDER_F14_SILENT_PROFILE quality=" + profile.quality
                            + " size=" + profile.videoFrameWidth + "x" + profile.videoFrameHeight
                            + " fps=" + profile.videoFrameRate + " codec=" + profile.videoCodec
                            + " audioCodec=" + profile.audioCodec + "; original F14 video-only semantics");
                    p.setResult(null);
                } catch (Throwable failure) {
                    p.setThrowable(failure);
                }
            }
        });
    }
}
