package com.falloutquest.app;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.File;

/** Missing installs and permission requests stay outside the native renderer. */
public final class MainActivity extends Activity {
    private TextView status;
    private boolean launching;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(32, 24, 32, 24);
        root.setBackgroundColor(Color.rgb(12, 14, 12));
        status = new TextView(this);
        status.setTextColor(Color.rgb(199, 255, 165));
        status.setTextSize(18);
        root.addView(status);
        Button access = new Button(this);
        access.setText("Allow game folder access");
        access.setOnClickListener(v -> requestStorage());
        root.addView(access);
        Button play = new Button(this);
        play.setText("Check files and play Fallout 3");
        play.setOnClickListener(v -> check());
        root.addView(play);
        setContentView(root);
    }
    @Override public void onResume() { super.onResume(); check(); }
    @Override public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(code, permissions, results);
        check();
    }
    private boolean storageAllowed() {
        return Build.VERSION.SDK_INT >= 30 ? Environment.isExternalStorageManager() :
            checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }
    private void requestStorage() {
        if (storageAllowed()) { check(); return; }
        if (Build.VERSION.SDK_INT < 30) {
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1);
            return;
        }
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                Uri.parse("package:" + getPackageName())));
        } catch (android.content.ActivityNotFoundException e) {
            try { startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)); }
            catch (android.content.ActivityNotFoundException unavailable) {
                status.setText("This headset does not expose storage permission settings.\n" +
                    "For this sideloaded build, enable access from your PC:\n" +
                    "adb shell appops set --uid com.falloutquest.app MANAGE_EXTERNAL_STORAGE allow\n" +
                    "Then press Check files and play Fallout 3.");
            }
        }
    }
    private void check() {
        if (launching) return;
        File root = new File(Environment.getExternalStorageDirectory(), "FalloutQuest");
        String instructions = "FALLOUTQUEST\n\nCopy your Steam install into:\n" +
            "Internal shared storage/FalloutQuest/Fallout3/\n" +
            "Internal shared storage/FalloutQuest/FalloutNV/\n\n";
        if (!storageAllowed()) {
            android.util.Log.i("FalloutQuest", "SETUP waiting for shared storage permission");
            status.setText(instructions + "Allow game folder access, then check files.");
            return;
        }
        for (GameInstall.Game game : GameInstall.Game.values()) new File(root, game.folder).mkdirs();
        File data = GameInstall.findData(new File(root, "Fallout3"), GameInstall.Game.FALLOUT3);
        String missing = GameInstall.missing(data, GameInstall.Game.FALLOUT3);
        android.util.Log.i("FalloutQuest", "SETUP data=" + data + " validation=" + missing);
        File nv = GameInstall.findData(new File(root, "FalloutNV"), GameInstall.Game.NEW_VEGAS);
        status.setText(instructions + (missing.isEmpty() ? "Fallout 3 ready: " + data : missing) +
            "\n\nNew Vegas: " + (nv == null ? "folder reserved" : "install detected") +
            "; runtime support coming later.");
        if (missing.isEmpty()) {
            launching = true;
            Intent intent = new Intent(this, FalloutNativeActivity.class);
            intent.setAction(Intent.ACTION_MAIN);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.putExtra("dataRoot", data.getAbsolutePath());
            android.util.Log.i("FalloutQuest", "SETUP launching immersive activity");
            startActivity(intent);
            finishAndRemoveTask();
        }
    }
}
