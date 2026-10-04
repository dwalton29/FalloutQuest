package com.falloutquest.app;

import android.app.NativeActivity;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Platform decoding on an audio thread; original assets remain in the user's install. */
public final class FalloutNativeActivity extends NativeActivity {
    private HandlerThread thread;
    private Handler audio;
    private boolean resumed, focused, destroyed;
    private final List<MediaPlayer> effects = new ArrayList<>();
    private final List<String> tracks = new ArrayList<>();
    private int track;
    private MediaPlayer music, ambience;
    private String ambientPath = "";
    private float ambientGain = 1;
    private static final float MUSIC_VOLUME = .3f; // Original supplied FALLOUT.INI.

    @Override public void onCreate(Bundle state) {
        // Configure every native reader before NativeActivity loads the library.
        String dataRoot = getIntent().getStringExtra("dataRoot");
        if (dataRoot == null || !GameInstall.missing(new java.io.File(dataRoot),
                GameInstall.Game.FALLOUT3).isEmpty())
            throw new IllegalStateException("Launch through FalloutQuest game setup");
        try {
            android.system.Os.setenv("FALLOUTQUEST_DATA_ROOT", dataRoot, true);
        } catch (android.system.ErrnoException e) {
            throw new IllegalStateException("Unable to configure game folder", e);
        }
        // Start before NativeActivity launches android_main.
        thread = new HandlerThread("FalloutAudio");
        thread.start();
        audio = new Handler(thread.getLooper());
        super.onCreate(state);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        Log.i("FalloutQuest", "AUDIO platform thread ready");
    }
    @Override public void onResume() {
        super.onResume();
        audio.post(() -> { resumed = true; Log.i("FalloutQuest", "AUDIO activity resumed"); refresh(); });
    }
    @Override public void onPause() {
        audio.post(() -> { resumed = false; stopAll(); });
        super.onPause();
    }
    @Override public void onDestroy() {
        super.onDestroy();
        audio.post(() -> { destroyed = true; stopAll(); thread.quitSafely(); });
    }
    // Called from the native worker, never directly operate MediaPlayer there.
    public void audioActive(boolean value) {
        audio.post(() -> { if (focused != value) { focused = value; Log.i("FalloutQuest", "AUDIO XR focus=" + value); refresh(); } });
    }
    public void audioMusic(String playlist) {
        audio.post(() -> {
            if (destroyed) return;
            releaseMusic(); tracks.clear(); track = 0;
            for (String path : playlist.split("\n")) if (!path.isEmpty()) tracks.add(path);
            Collections.shuffle(tracks);
            refresh();
        });
    }
    public void audioAmbient(String path, float gain) {
        audio.post(() -> {
            if (destroyed) return;
            releaseAmbience(); ambientPath = path; ambientGain = gain;
            refresh();
        });
    }
    public void audioEffect(String path, float gain) {
        audio.post(() -> {
            if (!enabled() || path.isEmpty()) return;
            if (effects.size() >= 6) { MediaPlayer old = effects.remove(0); old.release(); }
            MediaPlayer player = new MediaPlayer(); effects.add(player);
            prepare(player, path, gain, false, () -> { effects.remove(player); player.release(); },
                    () -> effects.contains(player));
        });
    }
    private boolean enabled() { return resumed && focused && !destroyed; }
    private void refresh() {
        if (!enabled()) { stopAll(); return; }
        if (music == null && !tracks.isEmpty()) {
            MediaPlayer player = new MediaPlayer(); music = player;
            prepare(player, tracks.get(track), MUSIC_VOLUME, false, () -> {
                if (music != player) return;
                releaseMusic(); track = (track + 1) % tracks.size(); refresh();
            }, () -> music == player);
        }
        if (ambience == null && !ambientPath.isEmpty()) {
            MediaPlayer player = new MediaPlayer(); ambience = player;
            prepare(player, ambientPath, ambientGain, true, this::releaseAmbience,
                    () -> ambience == player);
        }
    }
    private interface Current { boolean check(); }
    private void prepare(MediaPlayer player, String path, float gain, boolean loop,
                         Runnable complete, Current current) {
        try {
            player.setAudioAttributes(new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build());
            player.setDataSource(path);
            player.setOnPreparedListener(p -> {
                if (enabled() && current.check()) {
                    p.setVolume(gain, gain); p.setLooping(loop); p.start();
                    Log.i("FalloutQuest", "AUDIO playing: " + path + " gain=" + gain + " loop=" + loop);
                }
            });
            player.setOnCompletionListener(p -> { if (current.check()) complete.run(); });
            player.setOnErrorListener((p, what, extra) -> {
                Log.w("FalloutQuest", "AUDIO decode failed: " + path + " " + what + "/" + extra);
                // Do not retry corrupt tracks in a tight completion loop.
                if (current.check()) {
                    if (p == music) { tracks.clear(); releaseMusic(); }
                    else if (p == ambience) { ambientPath = ""; releaseAmbience(); }
                    else { effects.remove(p); p.release(); }
                }
                return true;
            });
            player.prepareAsync();
        } catch (Exception e) {
            Log.w("FalloutQuest", "AUDIO open failed: " + path, e);
            if (current.check()) {
                if (player == music) { tracks.clear(); releaseMusic(); }
                else if (player == ambience) { ambientPath = ""; releaseAmbience(); }
                else { effects.remove(player); player.release(); }
            }
        }
    }
    private void releaseMusic() { if (music != null) { music.release(); music = null; } }
    private void releaseAmbience() { if (ambience != null) { ambience.release(); ambience = null; } }
    private void stopAll() {
        releaseMusic(); releaseAmbience();
        for (MediaPlayer player : effects) player.release();
        effects.clear();
    }
}
