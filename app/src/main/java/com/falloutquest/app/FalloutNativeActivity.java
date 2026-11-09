package com.falloutquest.app;

import android.app.NativeActivity;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.media.AudioManager;
import android.media.audiofx.Equalizer;
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
    private MediaPlayer menuLoop, loadingEffect;
    private final List<MediaPlayer> effects = new ArrayList<>();
    private final List<String> tracks = new ArrayList<>();
    private int track;
    private MediaPlayer music, ambience;
    private static final class DialogueVoice {
        MediaPlayer player;
        Equalizer filter;
        int token;
    }
    private final DialogueVoice[] voices = {new DialogueVoice(), new DialogueVoice()};
    private float dialogueGain=1;
    public native void audioDialogueDone(int token,boolean success);
    public native void audioDialoguePosition(int token,int milliseconds);

    private void publishDialoguePosition(DialogueVoice voice,MediaPlayer player,int token) {
        if(voice.player!=player||voice.token!=token)return;
        try {if(player.isPlaying())audioDialoguePosition(token,player.getCurrentPosition());}
        catch(IllegalStateException ignored) {}
        audio.postDelayed(() -> publishDialoguePosition(voice,player,token),33);
    }

    private Equalizer intercomFilter(MediaPlayer player) {
        Equalizer filter=null;
        try {
            filter=new Equalizer(0,player.getAudioSessionId());
            short[] limits=filter.getBandLevelRange();
            for(short band=0;band<filter.getNumberOfBands();++band) {
                int frequency=filter.getCenterFreq(band);
                int gain=frequency<350000||frequency>3400000?-1400:0;
                filter.setBandLevel(band,(short)Math.max(limits[0],Math.min(limits[1],gain)));
            }
            filter.setEnabled(true);
            return filter;
        } catch(RuntimeException unavailable) {
            if(filter!=null)filter.release();
            Log.w("FalloutQuest","Intercom filter unavailable",unavailable);
            return null;
        }
    }

    public void audioDialogue(String path,int token,int lane,boolean intercom) {
        audio.post(() -> {
            if(lane<0||lane>=voices.length)return;
            DialogueVoice voice=voices[lane];
            releaseDialogue(voice,false);
            voice.token=token;
            if(destroyed||!enabled()||path.isEmpty()) {
                if(token!=0&&!path.isEmpty())audioDialogueDone(token,false);
                return;
            }
            MediaPlayer player=new MediaPlayer();
            voice.player=player;
            if(intercom)voice.filter=intercomFilter(player);
            Log.i("FalloutQuest","DIALOGUE preparing token="+token+" lane="+lane+" path="+path);
            prepare(player,path,dialogueGain,false,() -> {
                if(voice.player!=player||voice.token!=token)return;
                releaseDialogue(voice,false);
                // Deliver completion outside the platform callback.
                audio.post(() -> audioDialogueDone(token,true));
            },() -> voice.player==player&&voice.token==token);
            audio.postDelayed(() -> publishDialoguePosition(voice,player,token),33);
            duckSpeech();
        });
    }
    public void audioDialogueGain(float gain) {
        audio.post(() -> {
            dialogueGain=Math.max(0,Math.min(1,gain));
            for(DialogueVoice voice:voices)if(voice.player!=null)
                try{volume(voice.player,dialogueGain);}catch(IllegalStateException ignored){}
        });
    }
    private boolean speechActive() {return voices[0].player!=null||voices[1].player!=null;}
    private boolean dialoguePlayer(MediaPlayer player) {
        return player!=null&&(voices[0].player==player||voices[1].player==player);
    }
    private void duckSpeech() {
        for(Broadcast b:broadcasts)if(b.player!=null)
            try{volume(b.player,speechActive()?.25f:1f);}catch(IllegalStateException ignored){}
    }
    private void releaseDialogue(DialogueVoice voice,boolean failed) {
        if(voice.filter!=null){voice.filter.release();voice.filter=null;}
        if(voice.player!=null) {
            voice.player.release();
            voice.player=null;
            if(failed)audioDialogueDone(voice.token,false);
        }
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
        prepare(player,b.paths.get(b.index),speechActive()?.25f:1f,false,() -> {
            b.release(); ++b.index;
            if (b.index >= b.paths.size()) { b.paths.clear(); b.index = 0; audioBroadcastDone(channel,b.generation); }
            else refreshBroadcast(channel);
        },() -> b.player == player);
    }
    private boolean failBroadcast(MediaPlayer player) {
        for(DialogueVoice voice:voices)if(player==voice.player){releaseDialogue(voice,true);return true;}
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
    private boolean movieMuteWorld=false, moviePauseMusic=false;
    private float masterVolume=1, musicVolume=.3f, effectsVolume=1, voiceVolume=1, musicMultiplier=1;
    private final java.util.Map<MediaPlayer,Float> baseGains=new java.util.WeakHashMap<>();
    private final java.util.Map<MediaPlayer,Float> effectPans=new java.util.WeakHashMap<>();
    private float scaled(MediaPlayer player,float gain) {
        if(player!=music&&player!=loadingEffect&&movieMuteWorld)return 0;
        float channel=player==music?musicVolume:(dialoguePlayer(player)||player==broadcasts[0].player||player==broadcasts[1].player)?voiceVolume:effectsVolume;
        return Math.max(0,Math.min(1,gain*masterVolume*channel));
    }
    private void volume(MediaPlayer player,float gain) {
        baseGains.put(player,gain);applyVolume(player,gain);
    }
    private void applyVolume(MediaPlayer player,float gain) {
        float v=scaled(player,gain),pan=effectPans.getOrDefault(player,0f);
        player.setVolume(v*(1-Math.max(0,pan)),v*(1+Math.min(0,pan)));
    }
    public void audioMovie(boolean muteWorld, boolean pauseMusic) {
        audio.post(() -> {
            movieMuteWorld=muteWorld;moviePauseMusic=pauseMusic;
            for(java.util.Map.Entry<MediaPlayer,Float> e:baseGains.entrySet())try{applyVolume(e.getKey(),e.getValue());}catch(IllegalStateException ignored){}
            if(music!=null)try{if(pauseMusic&&music.isPlaying())music.pause();else if(!pauseMusic&&enabled())music.start();}catch(IllegalStateException ignored){}
            if(!pauseMusic)refresh();
        });
    }
    public void audioVolumes(float master,float music,float effects,float voice) {
        audio.post(() -> {masterVolume=master;musicVolume=music;effectsVolume=effects;voiceVolume=voice;
            for(java.util.Map.Entry<MediaPlayer,Float> e:baseGains.entrySet())try{applyVolume(e.getKey(),e.getValue());}catch(IllegalStateException ignored){}
        });
    }

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
    public void audioMusic(String playlist, float multiplier) {
        audio.post(() -> {
            if (destroyed) return;
            releaseMusic(); tracks.clear(); track = 0; musicMultiplier=multiplier;
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
    public void audioEffect(String path, float gain, float pan) {
        audio.post(() -> {
            if (!enabled() || path.isEmpty()) return;
            if (effects.size() >= 6) { MediaPlayer old = effects.remove(0); old.release(); }
            MediaPlayer player = new MediaPlayer(); effects.add(player);effectPans.put(player,pan);
            prepare(player, path, gain, false, () -> { effects.remove(player); player.release(); },
                    () -> effects.contains(player));
        });
    }
    private native boolean audioLoadingCurrent(long generation);
    public void audioLoadingEffect(String path, float gain, long generation) {
        audio.post(() -> {
            if (!path.isEmpty() && !audioLoadingCurrent(generation)) return;
            releaseLoadingEffect();
            if (!enabled() || path.isEmpty()) return;
            MediaPlayer player = new MediaPlayer(); loadingEffect = player;
            prepare(player, path, gain, false, this::releaseLoadingEffect,
                    () -> loadingEffect == player && audioLoadingCurrent(generation));
            watchLoadingEffect(player, generation);
        });
    }
    private void watchLoadingEffect(MediaPlayer player, long generation) {
        if (loadingEffect != player) return;
        if (!audioLoadingCurrent(generation)) releaseLoadingEffect();
        else audio.postDelayed(() -> watchLoadingEffect(player, generation), 25);
    }
    private void releaseLoadingEffect() {
        if (loadingEffect != null) { loadingEffect.release(); loadingEffect = null; }
    }
    public void audioMenuLoop(String path, float gain) {
        audio.post(() -> {
            if (menuLoop != null) { menuLoop.release(); menuLoop = null; }
            if (!enabled() || path.isEmpty()) return;
            MediaPlayer player = new MediaPlayer(); menuLoop = player;
            prepare(player, path, gain, true, () -> {}, () -> menuLoop == player);
        });
    }
    private boolean enabled() { return resumed && focused && !destroyed; }
    private void refresh() {
        if (!enabled()) { stopAll(); return; }
        for (int channel = 0; channel < 2; ++channel) refreshBroadcast(channel);
        if (!moviePauseMusic && music == null && !tracks.isEmpty() && broadcasts[0].paths.isEmpty()) {
            MediaPlayer player = new MediaPlayer(); music = player;
            prepare(player, tracks.get(track), musicMultiplier, false, () -> {
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
                    volume(p,gain); p.setLooping(loop); if(p!=music||!moviePauseMusic)p.start();
                    if(dialoguePlayer(p))Log.i("FalloutQuest","DIALOGUE started durationMs="+p.getDuration());
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
                    else if (p == menuLoop) { menuLoop = null; p.release(); }
                    else if (p == loadingEffect) releaseLoadingEffect();
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
                else if (player == menuLoop) { menuLoop = null; player.release(); }
                else if (player == loadingEffect) releaseLoadingEffect();
                else if (!failBroadcast(player)) { effects.remove(player); player.release(); }
            }
        }
    }
    private void releaseMusic() { if (music != null) { music.release(); music = null; } }
    private void releaseAmbience() { if (ambience != null) { ambience.release(); ambience = null; } }
    private void stopAll() {
        releaseLoadingEffect();
        if (menuLoop != null) { menuLoop.release(); menuLoop = null; }
        for(DialogueVoice voice:voices)releaseDialogue(voice,true);
        releaseMusic(); releaseAmbience();
        for (Broadcast b : broadcasts) b.release();
        for (MediaPlayer player : effects) player.release();
        effects.clear();
    }
}
