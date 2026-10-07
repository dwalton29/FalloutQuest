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
    private MediaPlayer music, ambience, dialogue;
    private int dialogueToken;
    private float dialogueGain=1;
    public native void audioDialogueDone(int token,boolean success);
    public native void audioDialoguePosition(int token,int milliseconds);
    private void publishDialoguePosition(MediaPlayer player,int token) {
        if(dialogue!=player||dialogueToken!=token)return;
        try {if(player.isPlaying())audioDialoguePosition(token,player.getCurrentPosition());}
        catch(IllegalStateException ignored) {}
        audio.postDelayed(() -> publishDialoguePosition(player,token),33);
    }
    public void audioDialogue(String path,int token) {
        audio.post(() -> {
            releaseDialogue(false);dialogueToken=token;
            if(destroyed || !enabled() || path.isEmpty()) {
                Log.w("FalloutQuest", "DIALOGUE rejected token="+token+" path="+path+" resumed="+resumed+" focused="+focused+" destroyed="+destroyed);
                if(token!=0 && !path.isEmpty())audioDialogueDone(token,false);return;
            }
            Log.i("FalloutQuest", "DIALOGUE preparing token="+token+" path="+path);
            MediaPlayer player=new MediaPlayer();dialogue=player;
            prepare(player,path,dialogueGain,false,() -> {
                if(dialogue==player) {
                    // Some Android decoders have already left PlaybackCompleted
                    // when delivering this callback. Diagnostics must never abort
                    // the UI thread before the native choice transition.
                    try {
                        Log.i("FalloutQuest", "DIALOGUE completed token="+token+" positionMs="+player.getCurrentPosition()+" durationMs="+player.getDuration());
                    } catch (IllegalStateException e) {
                        Log.w("FalloutQuest", "DIALOGUE completed token="+token+" timing unavailable",e);
                    }
                    // Leave MediaPlayer's completion callback before crossing
                    // into JNI. This removes callback/release/native re-entrancy
                    // from the exact headset failure boundary.
                    dialogue=null;
                    try { player.release(); } catch (RuntimeException e) {
                        Log.w("FalloutQuest", "DIALOGUE release token="+token+" failed",e);
                    }
                    duckSpeech();
                    audio.post(() -> {
                        Log.i("FalloutQuest", "DIALOGUE native-dispatch token="+token);
                        audioDialogueDone(token,true);
                        Log.i("FalloutQuest", "DIALOGUE native-return token="+token);
                    });
                }
            },() -> dialogue==player);
            audio.postDelayed(() -> publishDialoguePosition(player,token),33);
            duckSpeech();
        });
    }
    public void audioDialogueGain(float gain) {
        audio.post(() -> { dialogueGain=Math.max(0,Math.min(1,gain));if(dialogue!=null)try{dialogue.setVolume(dialogueGain,dialogueGain);}catch(IllegalStateException ignored){} });
    }
    private void duckSpeech() {
        for(Broadcast b:broadcasts)if(b.player!=null)try{float gain=dialogue==null?1:.25f;b.player.setVolume(gain,gain);}catch(IllegalStateException ignored){}
    }
    private void releaseDialogue(boolean failed) {
        if(dialogue!=null) {dialogue.release();dialogue=null;if(failed)audioDialogueDone(dialogueToken,false);}
        duckSpeech();
    }
    private final Broadcast[] broadcasts = {new Broadcast(),new Broadcast()};
    private static final class Broadcast {
        final List<String> paths = new ArrayList<>();
        int index, generation; MediaPlayer player;
        void release() { if (player != null) { player.release(); player = null; } }
    }
    public native void audioBroadcastDone(int channel,int generation);
    public void audioBroadcast(String playlist, int channel,int generation) {
        audio.post(() -> {
            if (destroyed || channel < 0 || channel > 1) return;
            Broadcast b = broadcasts[channel]; b.release(); b.paths.clear(); b.index = 0; b.generation = generation;
            for (String path : playlist.split("\n")) if (!path.isEmpty()) b.paths.add(path);
            // Vanilla radio displaces exploration music; ambience stays live.
            if (channel == 0 && !b.paths.isEmpty()) releaseMusic();
            refresh();
        });
    }
    private void refreshBroadcast(int channel) {
        Broadcast b = broadcasts[channel];
        if (b.player != null || b.index >= b.paths.size()) return;
        MediaPlayer player = new MediaPlayer(); b.player = player;
        prepare(player,b.paths.get(b.index),dialogue==null?1:.25f,false,() -> {
            b.release(); ++b.index;
            if (b.index >= b.paths.size()) { b.paths.clear(); b.index = 0; audioBroadcastDone(channel,b.generation); }
            else refreshBroadcast(channel);
        },() -> b.player == player);
    }
    private boolean failBroadcast(MediaPlayer player) {
        if(player==dialogue){releaseDialogue(true);return true;}
        for (int channel = 0; channel < broadcasts.length; ++channel) {
            Broadcast b = broadcasts[channel];
            if (b.player == player) { b.release(); b.paths.clear(); b.index = 0;
                if (channel == 1) audioBroadcastDone(1,b.generation);
                refresh(); return true; }
        }
        return false;
    }
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
        for (int channel = 0; channel < 2; ++channel) refreshBroadcast(channel);
        if (music == null && !tracks.isEmpty() && broadcasts[0].paths.isEmpty()) {
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
                    if(p==dialogue)Log.i("FalloutQuest", "DIALOGUE started token="+dialogueToken+" durationMs="+p.getDuration());
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
                    else if (!failBroadcast(p)) { effects.remove(p); p.release(); }
                }
                return true;
            });
            player.prepareAsync();
        } catch (Exception e) {
            Log.w("FalloutQuest", "AUDIO open failed: " + path, e);
            if (current.check()) {
                if (player == music) { tracks.clear(); releaseMusic(); }
                else if (player == ambience) { ambientPath = ""; releaseAmbience(); }
                else if (!failBroadcast(player)) { effects.remove(player); player.release(); }
            }
        }
    }
    private void releaseMusic() { if (music != null) { music.release(); music = null; } }
    private void releaseAmbience() { if (ambience != null) { ambience.release(); ambience = null; } }
    private void stopAll() {
        releaseDialogue(true);releaseMusic(); releaseAmbience();
        for (Broadcast b : broadcasts) b.release();
        for (MediaPlayer player : effects) player.release();
        effects.clear();
    }
}
