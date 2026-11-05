package com.arkaroy.occamfresh;
import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;
public final class StatusActivity extends Activity {
 @Override public void onCreate(Bundle state) {
  super.onCreate(state);
  TextView text = new TextView(this);
  text.setPadding(36,48,36,36);
  text.setText("OcCam Fresh " + BuildConfig.VERSION_NAME + "\n\nScope: com.oppo.camera\n\nOriginal F.14 APK compatibility and diagnostics. Uses original OEM classes and source-backed F.14 feature declarations, with narrow handling for absent optional ColorOS services. Requires the original APK hash and bootstrap marker.\n\nCamera functions are still under investigation.\n\nLogcat: OcFresh\nCamera cache: oc_fresh.log");
  setContentView(text);
 }
}
