package com.reicast.emulator;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.support.v7.app.AppCompatDelegate;
import android.util.Log;
import android.net.Uri;
import android.content.Intent;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.emu.AudioBackend;
import com.reicast.emulator.emu.JNIdc;

public class Emulator extends Application {
    private static Context context;
    private static BaseGLActivity currentActivity;

    // see MapleDeviceType in hw/maple/maple_devs.h
    public static final int MDT_Microphone = 2;
    public static final int MDT_None = 8;

    public static boolean nosound = false;
    public static int vibrationDuration = 20;
    public static int screenOrientation = 2; //Default value is 2

    public static int maple_devices[] = {
            MDT_None,
            MDT_None,
            MDT_None,
            MDT_None
    };
    public static int maple_expansion_devices[][] = {
        { MDT_None, MDT_None },
        { MDT_None, MDT_None },
        { MDT_None, MDT_None },
        { MDT_None, MDT_None },
    };

    /**
     * Load the settings from native code
     *
     */
    public void getConfigurationPrefs() {
        Emulator.nosound = JNIdc.getNosound();
        Emulator.vibrationDuration = JNIdc.getVirtualGamepadVibration();
        Emulator.screenOrientation = JNIdc.getScreenOrientation(); //Loading screenOrientation
        JNIdc.getControllers(maple_devices, maple_expansion_devices);
    }

    /**
     * Fetch current configuration settings from the emulator and save them
     * Called from JNI code
     */
    public void SaveAndroidSettings(String homeDirectory)
    {

        Log.i("reicast", "SaveAndroidSettings: saving preferences");
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        Emulator.nosound = JNIdc.getNosound();
        Emulator.vibrationDuration = JNIdc.getVirtualGamepadVibration();
        Emulator.screenOrientation = JNIdc.getScreenOrientation();
        JNIdc.getControllers(maple_devices, maple_expansion_devices);

        prefs.edit()
                .putString(Config.pref_home, homeDirectory).apply();
        AudioBackend.getInstance().enableSound(!Emulator.nosound);

        FileBrowser.installButtons(prefs);
        if (micPluggedIn() && currentActivity instanceof BaseGLActivity) {
            ((BaseGLActivity)currentActivity).requestRecordAudioPermission();
        }
        currentActivity.setOrientation(); //As the settings are saved, setOrientation sets the new Orientation of the device

    }

    public void LaunchFromUrl(String url) {
        Intent i = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(i);
    }

    public static boolean micPluggedIn() {
        JNIdc.getControllers(maple_devices, maple_expansion_devices);
        for (int i = 0; i < maple_expansion_devices.length; i++)
            if (maple_expansion_devices[i][0] == MDT_Microphone
                    || maple_expansion_devices[i][1] == MDT_Microphone)
                return true;
        return false;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Emulator.context = getApplicationContext();
    }

    public static Context getAppContext() {
        return Emulator.context;
    }

    public static BaseGLActivity getCurrentActivity() {
        return Emulator.currentActivity;
    }

    public static void setCurrentActivity(BaseGLActivity activity) {
        Emulator.currentActivity = activity;
    }

    static {
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true);
    }
}
