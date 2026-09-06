package com.falloutquest.app;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainActivity extends Activity {
    static {
        System.loadLibrary("falloutquest");
    }

    private static native String nativeBootMessage();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        );

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(64, 64, 64, 64);
        root.setBackgroundColor(Color.rgb(12, 14, 12));

        TextView title = new TextView(this);
        title.setText("FALLOUTQUEST");
        title.setTextColor(Color.rgb(216, 165, 42));
        title.setTextSize(36);
        title.setGravity(Gravity.CENTER);

        TextView status = new TextView(this);
        status.setText(nativeBootMessage() + "\n\nQ2 PIPELINE PROOF\nGitHub Actions -> APK -> Quest 3");
        status.setTextColor(Color.rgb(210, 220, 205));
        status.setTextSize(20);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, 32, 0, 0);

        root.addView(title);
        root.addView(status);
        setContentView(root);
    }
}
