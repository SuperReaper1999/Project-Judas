package org.projectjudas.runtime;

import org.libsdl.app.SDLActivity;
import org.json.*;
import java.io.*;
import java.security.MessageDigest;

/** Native SDL Activity; gameplay, physics, rendering and JS run in libmain. */
public final class JudasActivity extends SDLActivity {
    private android.media.AudioFocusRequest audioFocus;
    private static native void nativeAudioFocus(boolean focused);
    @Override protected void onResume() {
        super.onResume();
        android.media.AudioManager manager=(android.media.AudioManager)getSystemService(AUDIO_SERVICE);
        if(manager==null){nativeAudioFocus(false);return;}
        if(audioFocus==null)audioFocus=new android.media.AudioFocusRequest.Builder(android.media.AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(new android.media.AudioAttributes.Builder().setUsage(android.media.AudioAttributes.USAGE_GAME)
                .setContentType(android.media.AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setWillPauseWhenDucked(true)
            .setOnAudioFocusChangeListener(change -> nativeAudioFocus(change==android.media.AudioManager.AUDIOFOCUS_GAIN)).build();
        nativeAudioFocus(manager.requestAudioFocus(audioFocus)==android.media.AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
    }
    @Override protected void onPause() {
        nativeAudioFocus(false);
        android.media.AudioManager manager=(android.media.AudioManager)getSystemService(AUDIO_SERVICE);
        if(manager!=null&&audioFocus!=null)manager.abandonAudioFocusRequest(audioFocus);
        super.onPause();
    }
    @Override protected void onCreate(android.os.Bundle state) {
        super.onCreate(state);
        if(android.os.Build.VERSION.SDK_INT>=33)BackApi33.install(this);
    }
    private static final class BackApi33 {
        static void install(final JudasActivity activity) {
            activity.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,
                new android.window.OnBackInvokedCallback(){@Override public void onBackInvoked(){activity.onBackPressed();}});
        }
    }
    @Override public void onBackPressed() {
        // Back is ordinary logical input. It never silently terminates a game
        // while its project-owned pause menu should receive the action.
        SDLActivity.onNativeKeyDown(android.view.KeyEvent.KEYCODE_ESCAPE);
        SDLActivity.onNativeKeyUp(android.view.KeyEvent.KEYCODE_ESCAPE);
    }
    @Override protected String[] getLibraries() { return new String[]{"c++_shared", "SDL2", "main"}; }

    @Override protected String[] getArguments() {
        try {
            // SDL asks on its native application thread, not the Android UI thread.
            // Publish a complete immutable content generation before any engine IO.
            byte[] bytes=readAsset("judas-content.json", 8*1024*1024);
            JSONObject manifest=new JSONObject(new String(bytes, "UTF-8"));
            String generation=hex(MessageDigest.getInstance("SHA-256").digest(bytes));
            File parent=new File(getFilesDir(), "judas-content");
            File root=new File(parent,generation), ready=new File(root,".ready");
            if(!ready.isFile()) {
                File staging=new File(parent,generation+".staging");
                erase(staging); if(!staging.mkdirs())throw new IOException("Cannot create content staging");
                JSONArray files=manifest.getJSONArray("files");
                if(files.length()>50000)throw new IOException("Content file limit exceeded");
                long total=0;
                for(int i=0;i<files.length();++i) {
                    JSONObject entry=files.getJSONObject(i);String path=entry.getString("path");
                    File out=new File(staging,path);
                    if(path.startsWith("/")||path.contains("\\")||!out.getCanonicalPath().startsWith(staging.getCanonicalPath()+File.separator))
                        throw new IOException("Unsafe content path");
                    long expected=entry.getLong("bytes");total+=expected;
                    if(expected<0||total>1024L*1024*1024)throw new IOException("Content size limit exceeded");
                    File directory=out.getParentFile();if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Cannot create content directory");
                    MessageDigest hash=MessageDigest.getInstance("SHA-256");long size=0;
                    try(InputStream in=getAssets().open("content/"+path); FileOutputStream stream=new FileOutputStream(out)) {
                        byte[] buffer=new byte[65536];int n;
                        while((n=in.read(buffer))!=-1){size+=n;if(size>expected)throw new IOException("Content length mismatch");hash.update(buffer,0,n);stream.write(buffer,0,n);}
                        stream.getFD().sync();
                    }
                    if(size!=expected||!hex(hash.digest()).equals(entry.getString("sha256")))throw new IOException("Content hash mismatch: "+path);
                }
                try(FileOutputStream marker=new FileOutputStream(new File(staging,".ready"))){marker.write(generation.getBytes("UTF-8"));marker.getFD().sync();}
                erase(root);if(!staging.renameTo(root))throw new IOException("Cannot publish content generation");
                // Only engine-owned immutable APK content is retired; saves are
                // elsewhere in private storage and never touched by this adapter.
                File[] old=parent.listFiles();if(old!=null)for(File f:old)if(!f.equals(root))erase(f);
            }
            android.os.Bundle settings=getPackageManager().getApplicationInfo(getPackageName(),android.content.pm.PackageManager.GET_META_DATA).metaData;
            boolean shadows=settings!=null&&settings.getBoolean("judas.shadows",false);
            if((getApplicationInfo().flags&android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE)!=0&&getIntent().hasExtra("judas.shadows"))
                shadows=getIntent().getBooleanExtra("judas.shadows",shadows);
            SDLActivity.nativeSetenv("JUDAS_SHADOWS",shadows?"1":"0");
            if(getIntent().getBooleanExtra("judas.profile",false)) {
                SDLActivity.nativeSetenv("JUDAS_PROFILE","1");
                SDLActivity.nativeSetenv("JUDAS_PROFILE_OUTPUT",new File(getFilesDir(),"judas-profile.json").getAbsolutePath());
            }
            return new String[]{"--android-content",root.getAbsolutePath()};
        } catch(Exception e) {
            android.util.Log.e("Judas", "Native content startup failed",e);
            // No fallback to a partially extracted game or development tree.
            throw new IllegalStateException("Cannot prepare Judas content",e);
        }
    }
    private byte[] readAsset(String name,int limit)throws IOException {
        try(InputStream in=getAssets().open(name);ByteArrayOutputStream out=new ByteArrayOutputStream()){
            byte[] buffer=new byte[8192];int n;while((n=in.read(buffer))!=-1){if(out.size()+n>limit)throw new IOException("Manifest too large");out.write(buffer,0,n);}return out.toByteArray();
        }
    }
    private static String hex(byte[] bytes){StringBuilder s=new StringBuilder();for(byte b:bytes)s.append(String.format(java.util.Locale.ROOT,"%02x",b&255));return s.toString();}
    private static void erase(File file)throws IOException {
        if(file.isDirectory()){File[] children=file.listFiles();if(children==null)throw new IOException("Cannot list content");for(File child:children)erase(child);}
        if(file.exists()&&!file.delete())throw new IOException("Cannot retire content");
    }
}
